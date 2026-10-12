// Dev-only experiment: compiled only with -DWWE12_DEV_PROBES (off by default, so releases carry no hooks).
#if defined(WWE12_DEV_PROBES)

// Experiment for the belt-at-60-fps bug (Kaiko's tip: the physics during entrances may be what
// breaks). sub_82C7F3D0 is the physics world step: lock, ++counter, virtual call with dt (f1), then
// the result is returned in r3. With wwe12_phys30 on, while the game runs in its 60 fps mode, every
// second call of a given world is skipped and the next one steps with the summed dt, so the physics
// advances in 1/30 s steps while everything else still runs at 60.

#include <unordered_map>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_phys30, false, "WWE12",
                    "Debug: step the physics world every second frame with the summed dt in the 60 fps mode.");

REX_EXTERN(__imp__sub_82C7F3D0);

extern "C" REX_FUNC(sub_82C7F3D0) {
  constexpr u32 kFps = 0x8324CE28;
  if (!REXCVAR_GET(wwe12_phys30) || bd::mem::load<u32>(kFps + 8) != 60) {
    __imp__sub_82C7F3D0(ctx, base);
    return;
  }
  struct State { bool skip_next = true; double dt = 0; u32 last_ret = 0; };
  static std::unordered_map<u32, State> s_worlds;
  static u32 s_logged = 0;
  State &w = s_worlds[ctx.r3.u32];
  if (w.skip_next) {
    w.skip_next = false;
    w.dt = ctx.f1.f64;
    ctx.r3.u64 = w.last_ret;
    return;
  }
  w.skip_next = true;
  ctx.f1.f64 += w.dt;
  if (s_logged++ < 3)
    BD_INFO("[phys30] stepping world 0x{:08X} with dt {:.5f}", ctx.r3.u32, ctx.f1.f64);
  __imp__sub_82C7F3D0(ctx, base);
  w.last_ret = ctx.r3.u32;
}

#endif  // WWE12_DEV_PROBES
