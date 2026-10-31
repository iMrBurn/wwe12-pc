// WWE '12 PC: CPU time the render thread wastes, found with SVR_SAMPLE_STACKS (2026-10-17).
//
// sub_82AAAAF8 is the D3D library's wait for the GPU to read the ring buffer past a point (it loops
// while the read pointer the GPU writes back, device+11024, is behind). Each turn of the loop is
// sub_82AA7040: 32 x db16cyc of busy spinning, then a timeout check that may kick the GPU. Our GPU
// is the SDK's command processor thread, so the game's render thread burnt a core spinning for
// it: ~9% of its samples on a 4-thread run, and on 2-core laptops the spinner competes with the
// very thread it waits for. The hooks spin the first kSpinTurns turns of each wait as before, then
// yield the CPU once per turn. Most of these waits are the GPU waiting for the vertical blank
// (WAIT_REG_MEM, the command processor sleeps), so yielding at once costs no frame time (measured
// at 16.7 ms either way); with 16 spin turns the spinning was still a third of the render thread's
// busy time. The guest logic is unchanged.
//
// Yielding still spins when no other thread wants the core (the 32 x db16cyc of each turn plus the
// yield): the render thread kept ~17% of its active time in this loop. After kYieldTurns turns a
// wait now sleeps wwe12_gpu_wait_sleep_us per turn on a high-resolution timer, as Jarrod's WWE 2K14
// recomp stopped its game spinning while waiting for the GPU (about two cores less in a match).

#include <chrono>
#include <thread>
#if defined(_WIN32)
#include <windows.h>
#endif

#include <rex/cvar.h>

#include "svr_hook.h"

#if defined(WWE12_DEV_PROBES)
void Wwe12SyncProbeGpuWait(u64 ns);  // wwe12_syncprobe.cpp
void Wwe12SyncProbeWaitTurn();
#endif

REXCVAR_DEFINE_BOOL(wwe12_gpu_wait_yield, true, "WWE12",
                    "While the game waits for the GPU to read its commands, give the CPU to other "
                    "threads instead of spinning (helps 2-4 thread processors).");
REXCVAR_DEFINE_INT32(wwe12_gpu_wait_sleep_us, 200, "WWE12",
                     "Once a wait for the GPU lasts, sleep this many microseconds per check instead "
                     "of spinning (0 = only yield, the 0.9 behaviour).")
    .range(0, 2000);

namespace {
constexpr unsigned kSpinTurns = 2;
constexpr unsigned kYieldTurns = 8;
thread_local unsigned t_wait_turns = 0;

// Sleeps us microseconds. Windows' Sleep() rounds up to the 15.6 ms clock tick; a high-resolution
// waitable timer (Windows 10 1803+) doesn't. Without one, a yield.
void SleepMicro(int us) {
#if defined(_WIN32)
  thread_local HANDLE timer =
      CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                             TIMER_ALL_ACCESS);
  LARGE_INTEGER due;
  due.QuadPart = -10ll * us;  // relative, in 100 ns
  if (timer && SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
    WaitForSingleObject(timer, INFINITE);
    return;
  }
  std::this_thread::yield();
#else
  std::this_thread::sleep_for(std::chrono::microseconds(us));
#endif
}
}  // namespace

REX_EXTERN(__imp__sub_82AAAAF8);
extern "C" REX_FUNC(sub_82AAAAF8) {
  t_wait_turns = 0;
#if defined(WWE12_DEV_PROBES)
  const auto t0 = std::chrono::steady_clock::now();
  __imp__sub_82AAAAF8(ctx, base);
  Wwe12SyncProbeGpuWait(std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - t0).count());
#else
  __imp__sub_82AAAAF8(ctx, base);
#endif
}

REX_EXTERN(__imp__sub_82AA7040);
extern "C" REX_FUNC(sub_82AA7040) {
  if (REXCVAR_GET(wwe12_gpu_wait_yield) && ++t_wait_turns > kSpinTurns) {
    const int sleep_us = REXCVAR_GET(wwe12_gpu_wait_sleep_us);
    if (sleep_us > 0 && t_wait_turns > kYieldTurns)
      SleepMicro(sleep_us);
    else
      std::this_thread::yield();
  }
#if defined(WWE12_DEV_PROBES)
  Wwe12SyncProbeWaitTurn();
#endif
  __imp__sub_82AA7040(ctx, base);
}
