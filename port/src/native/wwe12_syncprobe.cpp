// Dev-only probe (compiled only with -DWWE12_DEV_PROBES; releases carry no hooks).
//
// Step 1 of decoupling the game's D3D library from the null GPU (docs/couches.md): measure what the
// game actually waits for. It changes nothing, it only times:
//   - sub_82AA83D8  D3DDevice_SynchronizeToPresentationInterval (WAIT_REG_MEM, once per frame),
//   - sub_82AAAAF8  the wait for the GPU to read the ring buffer past a point (hooked in
//                   wwe12_cpu.cpp, which reports here), and its turns (sub_82AA7040).
// Every 2 s, one line "[syncprobe]" with calls, total and worst time inside each wait, and the ring
// buffer read pointer the GPU writes back (device + 11024; device 0xADB97A80, docs/d3d_functions.md).
//
// Not compiled or run yet: written without the SDK or the generated code at hand. Check that it
// builds, and that the numbers look plausible (presync ~30-60 calls/s), before relying on it.

#if defined(WWE12_DEV_PROBES)

#include <algorithm>
#include <atomic>
#include <chrono>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_syncprobe, false, "WWE12",
                    "Debug: log how long the game waits for the (null) GPU every 2 s.");

namespace {

using Clock = std::chrono::steady_clock;

struct Stat {
  std::atomic<u64> calls{0};
  std::atomic<u64> total_ns{0};
  std::atomic<u64> worst_ns{0};

  void Add(u64 ns) {
    calls.fetch_add(1, std::memory_order_relaxed);
    total_ns.fetch_add(ns, std::memory_order_relaxed);
    u64 w = worst_ns.load(std::memory_order_relaxed);
    while (ns > w && !worst_ns.compare_exchange_weak(w, ns, std::memory_order_relaxed)) {
    }
  }
};

Stat g_presync;   // sub_82AA83D8
Stat g_gpuwait;   // sub_82AAAAF8, whole wait
std::atomic<u64> g_wait_turns{0};  // sub_82AA7040 calls

constexpr u32 kDevice = 0xADB97A80;       // docs/d3d_functions.md
constexpr u32 kRingReadPtrOffset = 11024;  // wwe12_cpu.cpp

u64 NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch())
      .count();
}

void MaybeReport() {
  static std::atomic<u64> s_last{0};
  const u64 now = NowMs();
  u64 last = s_last.load(std::memory_order_relaxed);
  if (now - last < 2000 || !s_last.compare_exchange_strong(last, now))
    return;
  if (last == 0)
    return;  // first call only starts the clock
  auto line = [](const char *name, Stat &s) {
    const u64 c = s.calls.exchange(0, std::memory_order_relaxed);
    const u64 t = s.total_ns.exchange(0, std::memory_order_relaxed);
    const u64 w = s.worst_ns.exchange(0, std::memory_order_relaxed);
    BD_INFO("[syncprobe] {}: {} calls, {:.2f} ms total, {:.3f} ms worst", name, c, t / 1e6,
            w / 1e6);
  };
  line("presync(82AA83D8)", g_presync);
  line("gpuwait(82AAAAF8)", g_gpuwait);
  const u32 rptr = bd::mem::try_load<u32>(kDevice + kRingReadPtrOffset, 0);
  BD_INFO("[syncprobe] wait turns (82AA7040): {}; ring read ptr = 0x{:08X}",
          g_wait_turns.exchange(0, std::memory_order_relaxed), rptr);
}

}  // namespace

// Called from the hooks in wwe12_cpu.cpp.
void Wwe12SyncProbeGpuWait(u64 ns) {
  if (!REXCVAR_GET(wwe12_syncprobe))
    return;
  g_gpuwait.Add(ns);
  MaybeReport();
}

void Wwe12SyncProbeWaitTurn() {
  if (!REXCVAR_GET(wwe12_syncprobe))
    return;
  g_wait_turns.fetch_add(1, std::memory_order_relaxed);
}

REX_EXTERN(__imp__sub_82AA83D8);
extern "C" REX_FUNC(sub_82AA83D8) {
  if (!REXCVAR_GET(wwe12_syncprobe)) {
    __imp__sub_82AA83D8(ctx, base);
    return;
  }
  const auto t0 = Clock::now();
  __imp__sub_82AA83D8(ctx, base);
  g_presync.Add(
      std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t0).count());
  MaybeReport();
}

#endif  // WWE12_DEV_PROBES
