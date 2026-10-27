// WWE '12 PC: Match Creator restrictions, probe (docs/match_creator.md).
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

REX_EXTERN(__imp__sub_825910F8);

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

}  // namespace

extern "C" REX_FUNC(sub_825910F8) {
  const u32 panel = ctx.r3.u32;
  __imp__sub_825910F8(ctx, base);
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
REX_EXTERN(__imp__sub_82593BD0);
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
