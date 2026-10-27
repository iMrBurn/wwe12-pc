// WWE '12 PC: Match Creator restrictions (docs/match_creator.md).
//
// wwe12_mc_unlock (on by default, the user's choice, 2026-10-27): a few rules a match type keeps
// fixed become the player's choice, only where they make sense (the user's choices):
//   - RULES > TIME LIMIT in every match type;
//   - WIN CONDITION > K.O. and LAST MAN STANDING when the ring is a STEEL CAGE or HELL IN A CELL;
//   - WIN CONDITION > OVER THE TOP ROPE and RULES > ELIMINATION with 3 superstars or more.
// Never opened: pinfalls in ladder or table matches and the like, ENTRANCE / POST MATCH REPLAY (the
// Elimination Chamber can break), RING TYPE, FIRST BLOOD (follows the BLOOD option), FLAMING TABLE,
// the cage exits, the Extreme Rules weapons.
// A row is greyed when its "allowed" field (panel + 232 + row * 24, with a copy at +236, set by the
// panel's open method from the [+12] table) is below 50 and not 8-11 (sub_82591B20 draws it grey,
// the panel's virtual +20, e.g. sub_82591448, refuses it); 50 = the player chooses. The rows' current
// values are at panel + 224 + row * 24; the ring type is ENVIRONMENT's row 3 (0 standard, 1 steel
// cage, 2 Hell in a Cell, 3 Elimination Chamber, 4 Inferno). The rules are applied after a panel
// opens (WIN CONDITION sub_825910F8, RULES sub_82591ED0) and before WIN CONDITION draws its rows
// (sub_82591B20), so a ring changed on ENVIRONMENT counts.
// Superstars per match type: the type's name is text 40687 + type (0 SINGLE, 3 TAG TEAM, 13 TRIPLE
// THREAT...); only types 0-26, whose names and counts were checked, are known (MatchSize).
//
// The Match Creator screen has three option panels (ENVIRONMENT, WIN CONDITION, RULES: screen +316,
// +932, +1684), each built by sub_825935C8(panel, screen, tables..., match type) with four table
// pointers at panel +8, +12, +16, +20 and opened by sub_825910F8(panel), which reads for each row
// (+176 rows) the byte at [+8] + group * rows + row (start value) and [+12] (same index), with
// group = the match type's group (sub_825936B0: table 0x82027E28, 67 groups).
//
// wwe12_mc_probe: each time a panel opens, the panel's rows and its four tables (67 groups) are
// logged once ("[mc] ..."), and for the current match type the row values it got. Every panel also
// goes through sub_82593BD0(panel) (greys its rows: widgets [+4] layout, indices [+180] + row and
// [+184] + row, flag at widget +180): there the rows' widgets (name at +0x60, text at [+200]) and
// their flags are logged once per panel and match type, which names each row.

#include <set>
#include <string>

#include <fmt/format.h>
#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_mc_probe, false, "WWE12",
                    "Probe: log the Match Creator's option tables (restrictions by match type).");
REXCVAR_DEFINE_BOOL(wwe12_mc_unlock, true, "WWE12",
                    "Match Creator: win conditions and rules a match type keeps fixed become the player's choice.");

REX_EXTERN(__imp__sub_825910F8);
REX_EXTERN(__imp__sub_82591ED0);
REX_EXTERN(__imp__sub_82593BD0);

namespace {

constexpr u32 kGroups = 0x82027E28u;  // 67 x {u32 list, u32 count, u32 flag}
constexpr int kGroupCount = 67;

int GroupOf(u32 type) {
  for (int g = 0; g < kGroupCount; ++g) {
    const u32 list = bd::mem::try_load<u32>(kGroups + g * 12), count = bd::mem::try_load<u32>(kGroups + g * 12 + 4);
    for (u32 i = 0; i < count && i < 64; ++i)
      if (bd::mem::try_load<u32>(list + i * 4) == type)
        return g;
  }
  return -1;
}

std::set<u32> g_dumped;                      // panels whose tables were logged
std::set<std::pair<u32, u32>> g_seen_types;  // (panel, match type) logged
std::set<std::pair<u32, u32>> g_seen_rows;   // (panel, match type) rows logged

std::string GuestString(u32 at) {
  std::string s;
  for (u32 i = 0; i < 48; ++i) {
    const u8 c = bd::mem::try_load<u8>(at + i);
    if (!c)
      break;
    s += (c >= 32 && c < 127) ? char(c) : '?';
  }
  return s;
}

// A layout widget (sub_824C5458: [layout+8] + index * 240, index < [layout+24]): name and text.
std::string Widget(u32 layout, u32 index) {
  const u32 items = bd::mem::try_field<u32>(layout, 8), count = bd::mem::try_field<u32>(layout, 24);
  if (!items || index >= count)
    return "-";
  const u32 w = items + index * 240;
  const u32 text = bd::mem::try_field<u32>(w, 200);
  return fmt::format("{}='{}' flag {}", GuestString(w + 0x60), text ? GuestString(text) : std::string(),
                     bd::mem::try_field<u32>(w, 180));
}

void DumpTables(u32 panel, u32 rows) {
  if (!g_dumped.insert(panel).second)
    return;
  std::string words;
  for (u32 o = 28; o < 164; o += 4)
    words += fmt::format(" {}", bd::mem::try_field<u32>(panel, o));
  BD_INFO("[mc] panel {:08X}: vtable {:08X}, {} rows, texts{}", panel, bd::mem::try_field<u32>(panel, 0), rows,
          words);
  for (u32 t = 8; t <= 20; t += 4) {
    const u32 table = bd::mem::try_field<u32>(panel, t);
    if (!table)
      continue;
    for (int g = 0; g < kGroupCount; ++g) {
      std::string line;
      for (u32 r = 0; r < rows; ++r)
        line += fmt::format(" {:3}", bd::mem::try_load<u8>(table + g * rows + r));
      BD_INFO("[mc] panel {:08X} table +{} ({:08X}) group {:2}:{}", panel, t, table, g, line);
    }
  }
}

// The panels (their vtables) and their place in the screen (ENVIRONMENT +316, WIN CONDITION +932,
// RULES +1684).
constexpr u32 kWinCondition = 0x82027C7Cu, kRules = 0x82027CACu;
constexpr u32 kEnvironmentFromWin = 932 - 316, kEnvironmentFromRules = 1684 - 316;
constexpr u32 kWinOverTheTopRope = 3, kWinKo = 4, kWinLastManStanding = 5;
constexpr u32 kRulesElimination = 3, kRulesTimeLimit = 5;
constexpr u32 kRingTypeRow = 3, kSteelCage = 1, kHellInACell = 2;

// Superstars in a match type, or 0 when not known (types 0-26: text 40687 + type).
u32 MatchSize(u32 type) {
  static constexpr u8 kSizes[27] = {
      2, 2, 2,                 // SINGLE, SINGLE w/MANAGER (x2)
      4, 4, 4, 4, 4, 6,        // TAG TEAM, TORNADO TAG, TAG w/MANAGER (x2), MIXED TAG, 6-MAN TAG
      4, 4, 6, 4,              // ELIMINATION TAG, TORNADO, 6-MAN, TABLE TORNADO
      3, 3, 3, 3,              // TRIPLE THREAT (normal, iron man anywhere / Hell in a Cell / table)
      4,                       // FATAL 4-WAY
      3, 3, 4, 4, 5,           // HANDICAP 1 on 2 (x2), 1 on 3 (x2), 2 on 3
      10, 20, 30, 40,          // ROYAL RUMBLE
  };
  return type < 27 ? kSizes[type] : 0;
}

u32 RowValue(u32 panel, u32 row) { return bd::mem::try_load<u32>(panel + 224 + row * 24); }

bool Open(u32 panel, u32 row) {
  const u32 allowed = panel + 232 + row * 24;
  const u32 v = bd::mem::try_load<u32>(allowed);
  if (v >= 50 || (v >= 8 && v <= 11))
    return false;
  bd::mem::store<u32>(allowed, 50);
  bd::mem::store<u32>(allowed + 4, 50);
  return true;
}

std::set<std::pair<u32, u32>> g_unlock_logged;  // (panel, match type)

// Opens the rows the user chose for this panel and match type; true when one was opened.
bool Unlock(u32 panel) {
  if (!REXCVAR_GET(wwe12_mc_unlock))
    return false;
  const u32 vtable = bd::mem::try_field<u32>(panel, 0);
  const u32 rows = bd::mem::try_field<u32>(panel, 176);
  const u32 type = bd::mem::try_field<u32>(panel, 24);
  const bool many = MatchSize(type) >= 3;
  std::string opened;
  const auto open = [&](u32 row) {
    if (row < rows && Open(panel, row))
      opened += fmt::format(" {}", row);
  };
  if (vtable == kWinCondition) {
    const u32 ring = RowValue(panel - kEnvironmentFromWin, kRingTypeRow);
    if (ring == kSteelCage || ring == kHellInACell) {
      open(kWinKo);
      open(kWinLastManStanding);
    }
    if (many)
      open(kWinOverTheTopRope);
  } else if (vtable == kRules) {
    open(kRulesTimeLimit);
    if (many)
      open(kRulesElimination);
  }
  if (opened.empty())
    return false;
  if (g_unlock_logged.insert({panel, type}).second)
    BD_INFO("[mc] unlock: match type {} ({} superstars), panel {:08X}: rows{} now the player's choice", type,
            MatchSize(type), vtable, opened);
  return true;
}

void UnlockAndRedraw(PPCContext &ctx, u8 *base, u32 panel) {
  if (!Unlock(panel))
    return;
  PPCContext c = ctx;
  c.r3.u64 = panel;
  __imp__sub_82593BD0(c, base);  // the rows' grey state again
}

}  // namespace

extern "C" REX_FUNC(sub_82591ED0) {
  const u32 panel = ctx.r3.u32;
  __imp__sub_82591ED0(ctx, base);
  UnlockAndRedraw(ctx, base, panel);
}

// WIN CONDITION draws its rows (grey or not): the ring may have changed since the page opened.
REX_EXTERN(__imp__sub_82591B20);
extern "C" REX_FUNC(sub_82591B20) {
  Unlock(ctx.r3.u32);
  __imp__sub_82591B20(ctx, base);
}

extern "C" REX_FUNC(sub_825910F8) {
  const u32 panel = ctx.r3.u32;
  __imp__sub_825910F8(ctx, base);
  UnlockAndRedraw(ctx, base, panel);
  if (!REXCVAR_GET(wwe12_mc_probe))
    return;
  const u32 rows = bd::mem::try_field<u32>(panel, 176);
  const u32 type_obj = bd::mem::try_field<u32>(panel, 24);
  if (rows == 0 || rows > 16)
    return;
  DumpTables(panel, rows);
  if (g_seen_types.insert({panel, type_obj}).second) {
    std::string fields;
    for (u32 r = 0; r < rows; ++r) {
      const u32 at = panel + 212 + r * 24;
      fields += fmt::format(" [{}: {} {} {} {} {} {}]", r, bd::mem::try_load<u32>(at), bd::mem::try_load<u32>(at + 4),
                            bd::mem::try_load<u32>(at + 8), bd::mem::try_load<u32>(at + 12),
                            bd::mem::try_load<u32>(at + 16), bd::mem::try_load<u32>(at + 20));
    }
    BD_INFO("[mc] panel {:08X} opened: match type {} (group {}), first row +168 {}:{}", panel, type_obj,
            GroupOf(type_obj), bd::mem::try_field<u32>(panel, 168), fields);
  }
}

// sub_82593BD0(panel): greys the panel's rows. Every panel goes through it.
extern "C" REX_FUNC(sub_82593BD0) {
  const u32 panel = ctx.r3.u32;
  __imp__sub_82593BD0(ctx, base);
  if (!REXCVAR_GET(wwe12_mc_probe))
    return;
  const u32 rows = bd::mem::try_field<u32>(panel, 176);
  const u32 type = bd::mem::try_field<u32>(panel, 24);
  if (rows == 0 || rows > 16 || !g_seen_rows.insert({panel, type}).second)
    return;
  DumpTables(panel, rows);
  const u32 layout = bd::mem::try_field<u32>(panel, 4);
  const u32 first = bd::mem::try_field<u32>(panel, 180), second = bd::mem::try_field<u32>(panel, 184);
  BD_INFO("[mc] panel {:08X} rows (match type {}, group {}, vtable {:08X}, cursor {}):", panel, type, GroupOf(type),
          bd::mem::try_field<u32>(panel, 0), bd::mem::try_field<u32>(panel, 168));
  for (u32 r = 0; r < rows; ++r)
    BD_INFO("[mc]   row {}: {} | {}", r, Widget(layout, first + r), Widget(layout, second + r));
}
