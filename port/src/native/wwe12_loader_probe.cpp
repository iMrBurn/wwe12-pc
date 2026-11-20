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
#include <cstring>

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

// ---- 2026-11-20 evening: the task worker (same cvar) ----
// The only thread whose wait differs between a frozen and a healthy start-up at the same moment (+27 s after
// the benchmark starts, nb_ dumps): the game thread sub_821AD068 sits in sub_827D38C8 (frozen) instead of its
// 2 ms sleep (healthy). sub_827D38C8(task) leaves the task's lock (task + 12, an RTL critical section), sets
// [task + 8] = 1 (waiting), waits for the event at [task + 104] for good, then takes the lock again.
// The only signaller is sub_827D5008(task), called by the main thread every frame (sub_827D5380, for the 4 task
// groups at 0x8222AAD8): it TRIES the task's lock first and, if the try fails, does nothing this frame; if it
// succeeds it works through the task's lists, unlocks, clears [task + 40] and sets the event.
// Logged: every wait of sub_827D38C8 longer than 100 ms (task, time), and, while a worker waits, a signaller
// pass whose try-lock fails (task, the lock's count / recursion / owner as the SDK keeps them), the first one
// then once a second while it lasts.

REX_EXTERN(__imp__sub_827D38C8);
REX_EXTERN(__imp__sub_827D5008);

namespace {
struct TaskLock {
  int32_t lock_count;  // host order (the SDK's X_RTL_CRITICAL_SECTION keeps it so)
  int32_t recursion;   // big-endian in guest memory
  u32 owner;           // big-endian
};

TaskLock ReadTaskLock(u8 *base, u32 task) {
  TaskLock l{};
  const u32 cs = task + 12;
  std::memcpy(&l.lock_count, base + cs + 0x10, 4);
  l.recursion = int32_t(bd::mem::try_load<u32>(cs + 0x14, 0));
  l.owner = bd::mem::try_load<u32>(cs + 0x18, 0);
  return l;
}

std::atomic<double> g_last_skip_log{0.0};
}  // namespace

extern "C" REX_FUNC(sub_827D38C8) {
  if (!REXCVAR_GET(wwe12_loader_probe)) {
    __imp__sub_827D38C8(ctx, base);
    return;
  }
  const u32 task = ctx.r3.u32;
  const double t0 = MsSinceStart();
  __imp__sub_827D38C8(ctx, base);
  const double waited = MsSinceStart() - t0;
  if (waited > 100.0)
    BD_INFO("[loaderprobe] {:.1f} ms: task worker {:08X} waited {:.0f} ms for its signal", t0, task, waited);
}

extern "C" REX_FUNC(sub_827D5008) {
  if (!REXCVAR_GET(wwe12_loader_probe)) {
    __imp__sub_827D5008(ctx, base);
    return;
  }
  const u32 task = ctx.r3.u32;
  const u32 waiting = bd::mem::try_load<u32>(task + 8, 0);
  const TaskLock before = ReadTaskLock(base, task);
  t_try_result = -1;
  __imp__sub_827D5008(ctx, base);
  // (the try-lock inside is RtlTryEnterCriticalSection itself, not sub_826A49E0: judge it by the lock state)
  if (waiting == 1 && before.lock_count != -1) {
    const double now = MsSinceStart();
    if (now - g_last_skip_log.load(std::memory_order_relaxed) >= 1000.0) {
      g_last_skip_log.store(now, std::memory_order_relaxed);
      BD_INFO("[loaderprobe] {:.1f} ms: task {:08X} worker waiting but its lock is held (count {}, recursion {}, "
              "owner {:08X}): the signal is skipped this frame",
              now, task, before.lock_count, before.recursion, before.owner);
    }
  }
}
