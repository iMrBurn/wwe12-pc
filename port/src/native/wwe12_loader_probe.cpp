// Probe (read only, off by default: wwe12_loader_probe) for the start-up freeze on "NOW LOADING" with 2 threads
// (2026-11-20, Rendu; docs/savoir_acquis.md). In the frozen runs the first load reads ~2.8 MB in 4 calls, then the
// loader thread (sub_826A4960, F8000038) never gets work again: it polls the flag at [loader + 12] every 3 ms,
// loader = *(0x822314E4).
//
// Read from the generated code: the request is posted by sub_826800E8 (called through a vtable, no direct
// caller). It first TRIES the loader's lock (loader vtable +16 = sub_826A49E0 = RtlTryEnterCriticalSection on
// 0x822314E8, the lock the loader thread holds while it works). If the try fails, it posts nothing and returns;
// otherwise it walks its 4 slots, sets [loader + 12] = 1 and unlocks. The loader thread takes the same lock
// (blocking), works (sub_826A3F08, sub_826A34E8), clears the flag and unlocks. Hypothesis to test: a request
// posted once while the loader holds the lock is dropped, and nothing posts it again.
//
// Logged, with the game-frame-independent time since start:
//   - every call of the poster sub_826800E8: caller (lr), whether its try-lock succeeded, the flag after it;
//   - every failed try-lock sub_826A49E0 (with its caller);
//   - every loader work pass (sub_826A3F08): start and duration.
// The first 300 lines, then only failed try-locks (all of them).

#include <atomic>
#include <chrono>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_loader_probe, false, "WWE12",
                    "Debug: log the game's loader requests, try-locks and work passes ('[loaderprobe]'), for the "
                    "start-up freeze on 2 threads.");

REX_EXTERN(__imp__sub_826800E8);
REX_EXTERN(__imp__sub_826A49E0);
REX_EXTERN(__imp__sub_826A3F08);

namespace {

constexpr u32 kLoaderPtrVa = 0x822314E4;  // lis r11,-31965 / lwz rX,5348(r11)
std::atomic<u32> g_lines{0};
thread_local int t_try_result = -1;  // last try-lock result on this thread (-1 = none)

double MsSinceStart() {
  static const auto start = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

bool LogAllowed(bool important) {
  const u32 n = g_lines.fetch_add(1, std::memory_order_relaxed);
  return important || n < 300;
}

u32 LoaderFlag() {
  const u32 loader = bd::mem::try_load<u32>(kLoaderPtrVa, 0);
  return loader ? bd::mem::try_load<u32>(loader + 12, 0xFFFFFFFF) : 0xFFFFFFFF;
}

}  // namespace

// The loader's try-lock (loader vtable +16).
extern "C" REX_FUNC(sub_826A49E0) {
  const u32 lr = u32(ctx.lr);
  __imp__sub_826A49E0(ctx, base);
  if (!REXCVAR_GET(wwe12_loader_probe))
    return;
  t_try_result = ctx.r3.u32 != 0 ? 1 : 0;
  if (ctx.r3.u32 == 0 && LogAllowed(true))
    BD_INFO("[loaderprobe] {:.1f} ms: try-lock FAILED (caller lr {:08X}, loader flag {})", MsSinceStart(), lr,
            LoaderFlag());
}

// The request poster.
extern "C" REX_FUNC(sub_826800E8) {
  if (!REXCVAR_GET(wwe12_loader_probe)) {
    __imp__sub_826800E8(ctx, base);
    return;
  }
  const u32 lr = u32(ctx.lr), owner = ctx.r3.u32;
  const u32 flag_before = LoaderFlag();
  t_try_result = -1;
  __imp__sub_826800E8(ctx, base);
  const int tried = t_try_result;
  if (LogAllowed(tried == 0))
    BD_INFO("[loaderprobe] {:.1f} ms: post (owner {:08X}, caller lr {:08X}): try-lock {}, flag {} -> {}",
            MsSinceStart(), owner, lr, tried == 1 ? "ok" : tried == 0 ? "FAILED, nothing posted" : "not seen",
            flag_before, LoaderFlag());
}

// One work pass of the loader thread (inside its lock).
extern "C" REX_FUNC(sub_826A3F08) {
  if (!REXCVAR_GET(wwe12_loader_probe)) {
    __imp__sub_826A3F08(ctx, base);
    return;
  }
  const double t0 = MsSinceStart();
  __imp__sub_826A3F08(ctx, base);
  if (LogAllowed(false))
    BD_INFO("[loaderprobe] {:.1f} ms: loader work pass, {:.2f} ms", t0, MsSinceStart() - t0);
}
