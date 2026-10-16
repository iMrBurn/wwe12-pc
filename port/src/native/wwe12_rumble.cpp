// WWE '12: controller vibration in WWE Universe matches (missing there on the Xbox 360 too).
//
// sub_824729A8(wrestler, kind) is the game's "rumble the pad of this wrestler". It goes on only if
// the global at 0x8306AF00 is set (1 in every match seen), the wrestler is controlled from a pad
// (vtable slot 23 returns 0), and wrestler+3452 is non-zero: the player's vibration flag, which
// match setup copies from byte 0 of the player's slot (sub_8267CD30). Exhibition matches set it
// to 1; Universe matches build their wrestlers through sub_82761630, which copies a 0 instead, so
// no rumble ever starts. Seen with the wwe12_game_log probes (2026-10-16): same values in both
// modes except wrestler+3452 for the player's wrestler (pad 0).
//
// The hook sets that flag to 1 for the call when the wrestler has a real pad (0-3, +1760) and
// puts the game's value back after. wwe12_universe_rumble = false keeps the console behaviour.

#include <rex/cvar.h>

#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_universe_rumble, true, "WWE12",
                    "Controller vibration in WWE Universe matches too (the Xbox 360 game has none).");

REX_EXTERN(__imp__sub_824729A8);
extern "C" REX_FUNC(sub_824729A8) {
  const u32 wrestler = ctx.r3.u32;
  if (!REXCVAR_GET(wwe12_universe_rumble) || !wrestler) {
    __imp__sub_824729A8(ctx, base);
    return;
  }
  const u32 flag_va = wrestler + 3452;
  const u32 flag = bd::mem::load<u32>(flag_va);
  const u32 pad = bd::mem::load<u32>(wrestler + 1760);
  const bool lend = flag == 0 && pad < 4;
  if (lend)
    bd::mem::store<u32>(flag_va, 1);
  __imp__sub_824729A8(ctx, base);
  if (lend)
    bd::mem::store<u32>(flag_va, flag);
}
