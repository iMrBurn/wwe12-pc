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
#include <string>

#include <fmt/format.h>

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
  return REXCVAR_GET(wwe12_motion_trace) && (n == 203 || n == 20152 || (n >= 13100 && n < 13200));
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

// The queue's two lists (queue +96 A, +112 B: +0 entries of 16 bytes {u16 number, u8, u8 counter, ..., +8 state},
// +8 count; sub_821D7480 answers 4 when the number is in A, 3 in B, 2 already queued, 1 queued now; read 2026-11-14)
std::string InLists(u32 q, u32 n) {
  std::string out;
  for (u32 l : {96u, 112u}) {
    const u32 ent = bd::mem::try_load<u32>(q + l), cnt = bd::mem::try_load<u32>(q + l + 8);
    for (u32 i = 0; i < cnt && i < 256 && ent; ++i)
      if (bd::mem::try_load<u16>(ent + i * 16) == u16(n))
        out += fmt::format(" liste {} : etat {} octets {} {} mots {:08X} {:08X}", l == 96 ? 'A' : 'B',
                           bd::mem::try_load<u32>(ent + i * 16 + 8), bd::mem::try_load<u8>(ent + i * 16 + 2),
                           bd::mem::try_load<u8>(ent + i * 16 + 3), bd::mem::try_load<u32>(ent + i * 16 + 4),
                           bd::mem::try_load<u32>(ent + i * 16 + 12));
  }
  return out;
}

// The motion directory [0x8317B870] (read 2026-11-14/15: slots of 16 bytes {+0 entries of 12 bytes sorted by number
// {u16 number, u16 variants, u32 their table}, +4 the full records' pointers, +8 the light ones', +12 count}; the
// queue's reader, sub_821D7C80 -> sub_8246EB78, drops a number whose entry has no variant in slots 1 and 11-22):
// where a number stands in it, and its full record's first bytes (the loader, sub_821D7E78, wants "ZLIB").
void Directory(u32 n) {
  static std::atomic<int> s_lines{0};
  if (s_lines.fetch_add(1) >= 24)
    return;
  const u32 dir = bd::mem::try_load<u32>(0x8317B870);
  std::string out;
  for (u32 slot : {0u, 1u, 2u, 11u, 12u, 13u, 14u, 15u, 16u, 17u, 18u, 19u, 20u, 21u, 22u}) {
    const u32 s = dir + slot * 16, tab = dir ? bd::mem::try_load<u32>(s) : 0, cnt = tab ? bd::mem::try_load<u32>(s + 12) : 0;
    if (!cnt)
      continue;
    out += fmt::format(" | case {} ({} numeros)", slot, cnt);
    for (u32 i = 0; i < cnt && i < 65536; ++i) {
      const u32 e = tab + i * 12;
      if (bd::mem::try_load<u16>(e) != u16(n))
        continue;
      const u32 fulls = bd::mem::try_load<u32>(s + 4), lights = bd::mem::try_load<u32>(s + 8);
      const u32 full = fulls ? bd::mem::try_load<u32>(fulls + i * 4) : 0, light = lights ? bd::mem::try_load<u32>(lights + i * 4) : 0;
      out += fmt::format(" : n {} variantes {} table {:08X} complete {:08X} ({:08X}) legere {:08X} ({:08X})", i,
                         bd::mem::try_load<u16>(e + 2), bd::mem::try_load<u32>(e + 4), full,
                         full ? bd::mem::try_load<u32>(full) : 0, light, light ? bd::mem::try_load<u32>(light) : 0);
      break;
    }
  }
  BD_INFO("[motion] repertoire {:08X}, numero {}{}", dir, n, out);
}

// The loader of one list entry (queue, list, flag, index): the entry's state after it (1 asked, 2 being read,
// 3 read, 5 given up).
REX_EXTERN(__imp__sub_821D7E78);
extern "C" REX_FUNC(sub_821D7E78) {
  const u32 list = ctx.r4.u32, idx = ctx.r6.u32;
  const u32 e = bd::mem::try_load<u32>(list) + idx * 16;
  const u32 n = bd::mem::try_load<u16>(e);
  __imp__sub_821D7E78(ctx, base);
  if (Watched(n) && n >= 13100 && n < 13200 && More())
    BD_INFO("[motion] chargement : numero {} (liste {:08X}, place {}) -> etat {} compteur {}", n, list, idx,
            bd::mem::try_load<u32>(e + 8), bd::mem::try_load<u16>(e + 4));
}

extern "C" REX_FUNC(sub_821D7480) {
  const u32 q = ctx.r3.u32, n = ctx.r4.u32, r5 = ctx.r5.u32;
  __imp__sub_821D7480(ctx, base);
  if (Watched(n) && More())
    BD_INFO("[motion] file : numero {} r5 {} -> {}{}", n & 0xFFFF, r5 & 0xFF, ctx.r3.u32, InLists(q, n & 0xFFFF));
  if (Watched(n) && (n & 0xFFFF) >= 13100 && (n & 0xFFFF) < 13200)
    Directory(n & 0xFFFF);
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
