// Probe: the LOCATION list of Create a Story's scene editor (tabs ARENA / BACKSTAGE / CUSTOM ARENA), to add the
// added arenas to it (docs/mod_maker.md). sub_8278B4C0(editor) builds the list's rows from three tabs at
// editor + 432 + 12 * tab = {?, entries, count}, 16-byte entries {location, ?, ?, u16 at +12}; location < 200 = an
// arena (its name: sub_82789A40 -> text id), 200+ backstage, 300+ custom arena. With wwe12_story_probe = true, each
// build logs the tabs and their entries ("[story]").

#include <cstdint>
#include <cstdio>
#include <string>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_story_probe, false, "WWE12", "Test: log Create a Story's scene LOCATION list.");

REX_EXTERN(__imp__sub_8278B4C0);  // scene editor: build the LOCATION list (editor)
REX_EXTERN(__imp__sub_82789A40);  // scene editor: a location's name (-, entry) -> text id

extern "C" REX_FUNC(sub_8278B4C0) {
  const uint32_t editor = ctx.r3.u32, lr = uint32_t(ctx.lr);
  if (REXCVAR_GET(wwe12_story_probe)) {
    REXLOG_INFO("[story] build list: editor {:08X} (vtable {:08X}), caller {:08X}, +420 {:08X}, +424 {:04X}", editor,
                bd::mem::load<uint32_t>(editor), lr, bd::mem::load<uint32_t>(editor + 420),
                bd::mem::load<uint16_t>(editor + 424));
    for (uint32_t t = 0; t < 3; ++t) {
      const uint32_t tab = editor + 432 + 12 * t, entries = bd::mem::load<uint32_t>(tab + 4),
                     count = bd::mem::load<uint32_t>(tab + 8);
      REXLOG_INFO("[story]  tab {}: {:08X} entries {:08X} count {}", t, bd::mem::load<uint32_t>(tab), entries, count);
      for (uint32_t i = 0; i < count && i < 64 && entries; ++i) {
        const uint32_t e = entries + i * 16;
        REXLOG_INFO("[story]   {:2}: {:08X} {:08X} {:08X} {:08X}", i, bd::mem::load<uint32_t>(e),
                    bd::mem::load<uint32_t>(e + 4), bd::mem::load<uint32_t>(e + 8), bd::mem::load<uint32_t>(e + 12));
      }
    }
  }
  __imp__sub_8278B4C0(ctx, base);
}

REX_EXTERN(__imp__sub_826CBFB8);  // match settings: the stage (u16 at +898)
REX_EXTERN(__imp__sub_82A31780);  // the location table: the record of a location (u8 at +4), 0 if none

// The location table: [0x8324E24C] = 36-byte records, [0x8324E250] = count. Logged once, then the stage reads.
extern "C" REX_FUNC(sub_826CBFB8) {
  const uint32_t lr = uint32_t(ctx.lr);
  __imp__sub_826CBFB8(ctx, base);
  if (!REXCVAR_GET(wwe12_story_probe))
    return;
  static uint32_t last = 0xFFFFFFFF, last_lr = 0;
  if (ctx.r3.u32 != last || lr != last_lr) {
    last = ctx.r3.u32;
    last_lr = lr;
    REXLOG_INFO("[story] match stage {} read by {:08X}", last, lr);
  }
}

extern "C" REX_FUNC(sub_82A31780) {
  const uint32_t location = ctx.r3.u32, lr = uint32_t(ctx.lr);
  __imp__sub_82A31780(ctx, base);
  if (!REXCVAR_GET(wwe12_story_probe))
    return;
  static bool dumped = false;
  if (!dumped) {
    dumped = true;
    const uint32_t table = bd::mem::load<uint32_t>(0x8324E24C), count = bd::mem::load<uint32_t>(0x8324E250);
    REXLOG_INFO("[story] location table {:08X}, {} records", table, count);
    for (uint32_t i = 0; i < count && i < 256 && table; ++i) {
      std::string hex;
      for (uint32_t b = 0; b < 36; ++b) {
        char t[4];
        snprintf(t, sizeof(t), "%02X", bd::mem::load<uint8_t>(table + i * 36 + b));
        hex += t;
        if (b % 4 == 3)
          hex += ' ';
      }
      REXLOG_INFO("[story]  loc {:3}: {}", bd::mem::load<uint8_t>(table + i * 36 + 4), hex);
    }
  }
  static uint32_t last = 0xFFFFFFFF, last_lr = 0;
  if (location != last || lr != last_lr) {
    last = location;
    last_lr = lr;
    REXLOG_INFO("[story] location {} looked up by {:08X}", location, lr);
  }
}

extern "C" REX_FUNC(sub_82789A40) {
  const uint32_t entry = ctx.r4.u32, lr = uint32_t(ctx.lr);
  __imp__sub_82789A40(ctx, base);
  if (REXCVAR_GET(wwe12_story_probe))
    REXLOG_INFO("[story] name of {:08X} (location {}) = text {}, caller {:08X}", entry, bd::mem::load<uint32_t>(entry),
                ctx.r3.u32, lr);
}
