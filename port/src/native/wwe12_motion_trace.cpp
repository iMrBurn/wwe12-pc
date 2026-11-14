// Dev trace (1.4, session Gameplay, for the Mod Maker's WWE '13 I Quit motions): how the game finds a motion played
// by number (sub_8247AD18 -> sub_82478AE8). wwe12_motion_trace (off) logs, for 203 and 13100-13199 only:
// - the load queue sub_821D7480(queue, number, r5): its answer (3 = already queued, 4 = refused, else queued);
// - the lookup sub_821A55B8(number, *[actor+684], *[actor+688]): the motion handed to the actor's player (0 = none);
//   inside it, sub_821A5350 (the 16 loaded motions at manager+8, matched on number + the two bytes) and, when not
//   there, sub_821A5230 (a binary search of the number in each bank the manager knows: manager[0] = banks,
//   manager[4] = their count; each bank +4 -> its table (+4 count), +8 -> 16-byte entries sorted by number).
// Read 2026-11-14 in the generated code; lines "[motion]".

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(wwe12_motion_trace, false, "WWE12",
                    "Dev: log how motions 203 and 13100-13199 are queued and looked up (Mod Maker's I Quit motions).");

#include <atomic>

#include "svr_hook.h"
#include "core/logging.h"
#include "core/memory_helpers.h"

REX_EXTERN(__imp__sub_821D7480);
REX_EXTERN(__imp__sub_821A55B8);
REX_EXTERN(__imp__sub_821A5350);
REX_EXTERN(__imp__sub_821A5230);

namespace {
bool Watched(u32 n) {
  n &= 0xFFFF;
  return REXCVAR_GET(wwe12_motion_trace) && (n == 203 || (n >= 13100 && n < 13200));
}
std::atomic<int> g_lines{0};
bool More() { return g_lines.fetch_add(1) < 400; }

// the banks the manager knows: their count and, per bank, its entries' count and number range
void Banks(u32 mgr) {
  const u32 banks = bd::mem::try_load<u32>(mgr), n = bd::mem::try_load<u32>(mgr + 4);
  BD_INFO("[motion]   gestionnaire {:08X} : {} banques", mgr, n);
  for (u32 i = 0; i < n && i < 64; ++i) {
    const u32 b = banks + i * 16;  // (stride guessed from the loop: r31 += 16? logged raw below)
    const u32 tab = bd::mem::try_load<u32>(b + 4), ent = bd::mem::try_load<u32>(b + 8);
    const u32 cnt = tab ? bd::mem::try_load<u32>(tab + 4) : 0;
    const u16 lo = ent && cnt ? bd::mem::try_load<u16>(ent) : 0;
    const u16 hi = ent && cnt ? bd::mem::try_load<u16>(ent + (cnt - 1) * 16) : 0;
    BD_INFO("[motion]   banque {} @{:08X} : table {:08X} entrees {:08X} nombre {} ({}-{})", i, b, tab, ent, cnt, lo, hi);
  }
}
}  // namespace

extern "C" REX_FUNC(sub_821D7480) {
  const u32 n = ctx.r4.u32, r5 = ctx.r5.u32;
  __imp__sub_821D7480(ctx, base);
  if (Watched(n) && More())
    BD_INFO("[motion] file : numero {} r5 {} -> {}", n & 0xFFFF, r5 & 0xFF, ctx.r3.u32);
}

extern "C" REX_FUNC(sub_821A5350) {
  const u32 mgr = ctx.r3.u32, n = ctx.r4.u32, a = ctx.r5.u32, b = ctx.r6.u32;
  __imp__sub_821A5350(ctx, base);
  if (Watched(n) && More())
    BD_INFO("[motion]   deja chargees ({:08X}) : numero {} ({}, {}) -> {:08X}", mgr, n & 0xFFFF, a, b, ctx.r3.u32);
}

extern "C" REX_FUNC(sub_821A5230) {
  const u32 mgr = ctx.r3.u32, n = ctx.r4.u32;
  __imp__sub_821A5230(ctx, base);
  if (Watched(n) && More()) {
    BD_INFO("[motion]   recherche dans les banques : numero {} -> {:08X}", n & 0xFFFF, ctx.r3.u32);
    static std::atomic<bool> s_banks{false};
    if (!s_banks.exchange(true))
      Banks(mgr);
  }
}

extern "C" REX_FUNC(sub_821A55B8) {
  const u32 n = ctx.r3.u32, a = ctx.r4.u32, b = ctx.r5.u32;
  __imp__sub_821A55B8(ctx, base);
  if (Watched(n) && More())
    BD_INFO("[motion] recherche : numero {} ({}, {}) -> motion {:08X}", n & 0xFFFF, a & 0xFF, b & 0xFF, ctx.r3.u32);
}
