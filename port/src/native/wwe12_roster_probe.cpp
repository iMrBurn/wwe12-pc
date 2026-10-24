// Probe: why a superstar is or isn't in the select screen's DLC tile (docs/mod_maker.md, "Ajouter des catcheurs").
// The tile's test sub_82567068(view, id) wants: the record's DLC flag (sub_82A330F8: byte 283 of sub_82684BF8(id)),
// "unlocked" (sub_82A33FD8: byte 247 of the roster record sub_82684910(id) = [0x8317FA08] + index * 286, index =
// u16 table 0x8307B5E0[id]), owned (sub_82A3FB58: the content list [0x8322E238] + 164, 16-byte entries {kind 1, id,
// 0, state}, owned = state 2) and the page's gender. With wwe12_roster_probe = true, each superstar's answer and the
// bytes behind it are logged once ("[roster]").

#include <cstdint>
#include <set>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_roster_probe, false, "WWE12",
                    "Test: log why each superstar is or isn't in the select screen's DLC tile.");

REX_EXTERN(__imp__sub_82567068);  // select screen: is this superstar in the DLC tile (view, id)

namespace {

std::set<uint32_t> g_logged;

uint32_t OwnedState(uint32_t id) {
  const uint32_t obj = bd::mem::load<uint32_t>(0x8322E238);
  if (!obj)
    return 0xFFFFFFFF;
  const uint32_t list = obj + 164, entries = bd::mem::load<uint32_t>(list + 4),
                 count = bd::mem::load<uint32_t>(list + 12);
  for (uint32_t i = 0; i < count && i < 4096 && entries; ++i) {
    const uint32_t e = entries + i * 16;
    if (bd::mem::load<uint32_t>(e) == 1 && bd::mem::load<uint32_t>(e + 4) == id &&
        bd::mem::load<uint32_t>(e + 8) == 0)
      return bd::mem::load<uint32_t>(e + 12);
  }
  return 0xFFFFFFFE;  // not in the list
}

}  // namespace

extern "C" REX_FUNC(sub_82567068) {
  const uint32_t id = ctx.r4.u32;
  __imp__sub_82567068(ctx, base);
  if (!REXCVAR_GET(wwe12_roster_probe) || id >= 373)
    return;
  const uint32_t result = ctx.r3.u32;
  static bool once = false;
  if (!once) {  // the DLC slots and a few disc ones, whether the tile tests them or not
    once = true;
    const uint32_t roster = bd::mem::load<uint32_t>(0x8317FA08);
    for (uint32_t i = 50; i < 110; ++i) {
      const uint16_t index = bd::mem::load<uint16_t>(0x8307B5E0 + i * 2);
      const uint32_t b247 = index != 512 && roster ? bd::mem::load<uint8_t>(roster + index * 286u + 247) : 0xFF;
      REXLOG_INFO("[roster] slot {}: roster index {}, unlocked byte {}, owned {}", i, index, b247, OwnedState(i));
    }
  }
  if (!g_logged.insert(id * 2 + (result ? 1 : 0)).second)
    return;
  const uint16_t index = bd::mem::load<uint16_t>(0x8307B5E0 + id * 2);
  const uint32_t roster = bd::mem::load<uint32_t>(0x8317FA08);
  uint32_t b247 = 0xFF, b279 = 0xFF, rid = 0xFFFF;
  if (index != 512 && roster) {
    const uint32_t r = roster + index * 286u;
    b247 = bd::mem::load<uint8_t>(r + 247);
    b279 = bd::mem::load<uint8_t>(r + 279);
    rid = bd::mem::load<uint16_t>(r + 0x28);
  }
  REXLOG_INFO("[roster] id {} in DLC tile {}: roster index {} (record id {}), unlocked byte {}, DLC byte {}, owned {}",
              id, result, index, rid, b247, b279, OwnedState(id));
}
