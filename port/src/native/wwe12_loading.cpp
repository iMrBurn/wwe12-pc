// WWE '12: faster loading, the way nefariousjosiah's SvR 2009/2010 ports do it (same Yuke's engine).
//
// The engine's graphics object guards the D3D device with a lock the loader thread holds while it
// loads. The loader often calls sub_82819258: once the time since the last loading-screen frame
// (timebase ticks from +200, divided into milliseconds) reaches the budget at +196, it hands the
// device to the main thread for one frame and waits for it. Match loads set a 1 ms budget, so the
// loader worked about 1 ms per 16.7 ms frame: on the Xbox 360 the DVD drive was the bottleneck
// anyway, on PC the data is there at once and loading mostly waited for frames.
//
// The hook raises that budget for the call (never lowers it) and puts the game's value back after,
// so the loading screen still gets a frame every 1/60 s and the loader gets most of each frame.

#include <algorithm>
#include <atomic>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_INT32(wwe12_load_slice_ms, 12, "WWE12",
                     "Milliseconds the loader may work per loading-screen frame (the game uses 1 for "
                     "match loads; 0 keeps the game's value).")
    .range(0, 15);

REX_EXTERN(__imp__sub_82819258);

void Wwe12LoaderTick();  // wwe12_load_turbo.cpp

extern "C" REX_FUNC(sub_82819258) {
  Wwe12LoaderTick();
  const int slice = REXCVAR_GET(wwe12_load_slice_ms);
  const u32 budget_va = ctx.r3.u32 + 196;
  const float budget = bd::mem::load<float>(budget_va);
  if (slice <= 0 || budget >= float(slice)) {
    __imp__sub_82819258(ctx, base);
    return;
  }
  static std::atomic<u32> s_logged{0};
  if (s_logged.fetch_add(1, std::memory_order_relaxed) < 3)
    BD_INFO("[load] loader budget {} ms raised to {} ms", budget, slice);
  bd::mem::store<float>(budget_va, float(slice));
  __imp__sub_82819258(ctx, base);
  // Put the game's budget back, unless the game set a new one meanwhile.
  if (bd::mem::load<float>(budget_va) == float(slice))
    bd::mem::store<float>(budget_va, budget);
}
