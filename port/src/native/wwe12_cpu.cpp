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

#include <thread>

#include <rex/cvar.h>

#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_gpu_wait_yield, true, "WWE12",
                    "While the game waits for the GPU to read its commands, give the CPU to other "
                    "threads instead of spinning (helps 2-4 thread processors).");

namespace {
constexpr unsigned kSpinTurns = 2;
thread_local unsigned t_wait_turns = 0;
}  // namespace

REX_EXTERN(__imp__sub_82AAAAF8);
extern "C" REX_FUNC(sub_82AAAAF8) {
  t_wait_turns = 0;
  __imp__sub_82AAAAF8(ctx, base);
}

REX_EXTERN(__imp__sub_82AA7040);
extern "C" REX_FUNC(sub_82AA7040) {
  if (REXCVAR_GET(wwe12_gpu_wait_yield) && ++t_wait_turns > kSpinTurns)
    std::this_thread::yield();
  __imp__sub_82AA7040(ctx, base);
}
