// WWE '12: the game's own debug messages, written to our log (dev aid).
//
// Yuke's left its debug prints in the retail build, but they format into a stack buffer that is
// then thrown away. Every one of them goes through the game's vsprintf (sub_82A79228), called
// only by six print helpers:
//   sub_827D8B48 (454 call sites), sub_826D4228 (46, the WWE Universe mode: cards of each show
//   "1: JOHN CENA: 901[ 2], ...", rankings, "Draft Event Success" / "Cancel"), sub_827E57F0 (36),
//   sub_827DCFB0 (33), sub_8270EEB0 (31), sub_82BA9CF8 (1).
// With wwe12_game_log = true (tools/dev/lancer_journal.bat) the hook copies the text the game
// formatted into our log as "[game XXXXXXXX] ...", XXXXXXXX being the helper that printed it.
//
// The WWE Draft (Universe, after WrestleMania) works: DRAFT EVENT at the end of the draft match
// plays the lottery and the drafted Superstar's entrance (checked 2026-10-16).

#include <algorithm>
#include <array>
#include <mutex>
#include <string>

#include <fmt/format.h>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_game_log, false, "WWE12",
                    "Dev: write the game's own debug messages (Universe mode, replays...) to the log.");

namespace {

// The print helpers that call vsprintf; the return address of the call is inside one of them.
constexpr u32 kHelpers[] = {0x826D4228, 0x8270EEB0, 0x827D8B48, 0x827DCFB0, 0x827E57F0, 0x82BA9CF8};

u32 HelperFor(u32 lr) {
  for (const u32 start : kHelpers)
    if (lr >= start && lr < start + 0x200)
      return start;
  return lr;
}

}  // namespace

REX_EXTERN(__imp__sub_82A79228);
extern "C" REX_FUNC(sub_82A79228) {
  const u32 buffer = ctx.r3.u32, caller = u32(ctx.lr);
  __imp__sub_82A79228(ctx, base);
  if (!REXCVAR_GET(wwe12_game_log) || !buffer)
    return;
  std::string line = bd::mem::str(buffer);
  if (line.size() > 300)
    line.resize(300);
  while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
    line.pop_back();
  if (line.empty())
    return;
  // Messages printed every frame would flood the log: each one is written again only once 32
  // other messages have gone by.
  static std::mutex s_mu;
  static std::array<std::string, 32> s_recent;
  static u32 s_next = 0, s_count = 0;
  std::lock_guard lock(s_mu);
  for (const std::string &seen : s_recent)
    if (seen == line)
      return;
  if (s_count >= 50000)
    return;
  ++s_count;
  s_recent[s_next++ % s_recent.size()] = line;
  BD_INFO("[game {:08X}] {}", HelperFor(caller), line);
}

// Post-match replay probe (issue #9). GameReplayRecordThread (entry sub_829597D8, from the
// thread table at 0x82001900) runs sub_8295A1C0(recorder) once per frame, recorder being the
// object at 0x8324E1B0 (made by sub_823DDE48 / sub_82959FB8). It records only while the global
// at 0x8317B7BC is 1, through the recorder's current mode object at +20 (vtable slot 1).
// Logged on each change.
REX_EXTERN(__imp__sub_8295A1C0);
extern "C" REX_FUNC(sub_8295A1C0) {
  if (REXCVAR_GET(wwe12_game_log)) {
    const u32 self = ctx.r3.u32;
    const u32 flag = bd::mem::load<u32>(0x8317B7BC);
    const u32 mode = bd::mem::load<u32>(self + 20), f24 = bd::mem::load<u32>(self + 24),
              f32 = bd::mem::load<u32>(self + 32);
    static u32 s_last[5] = {~0u, ~0u, ~0u, ~0u, ~0u};
    const u32 now[5] = {self, flag, mode, f24, f32};
    if (!std::equal(now, now + 5, s_last)) {
      std::copy(now, now + 5, s_last);
      // The mode (vtable 0x82065A54, slot 1 = sub_8295E600) hands each frame to up to four
      // parts at mode+8..+20, through their vtable slot 3.
      std::string parts;
      for (u32 i = 0; mode && i < 4; ++i) {
        const u32 part = bd::mem::load<u32>(mode + 8 + i * 4);
        const u32 vt = part ? bd::mem::load<u32>(part) : 0;
        parts += fmt::format(" [0x{:08X} vtbl 0x{:08X} rec 0x{:08X}]", part, vt,
                             vt ? bd::mem::load<u32>(vt + 12) : 0u);
      }
      BD_INFO("[replay] record step: recorder 0x{:08X} flag(0x8317B7BC)={} mode=0x{:08X} "
              "vtbl=0x{:08X} +24=0x{:08X} +32={} parts{}",
              self, flag, mode, mode ? bd::mem::load<u32>(mode) : 0u, f24, f32, parts);
    }
  }
  __imp__sub_8295A1C0(ctx, base);
}

// A moment is added by sub_821EE520(ctrl = *0x83113ED8, kind): sub_821ED988 picks a slot, then
// the slot is filled only if *0x83174250 != 0, ctrl+88 == 1 and sub_822EAA80(ctrl+156, slot)
// (keeps the replay of the moment) returns non-zero.
REX_EXTERN(__imp__sub_821EE520);
extern "C" REX_FUNC(sub_821EE520) {
  const u32 ctrl = ctx.r3.u32, kind = ctx.r4.u32;
  const u32 gate = bd::mem::load<u32>(0x83174250), state = bd::mem::load<u32>(ctrl + 88);
  __imp__sub_821EE520(ctx, base);
  if (REXCVAR_GET(wwe12_game_log))
    BD_INFO("[replay] add moment: ctrl 0x{:08X} kind {} | *0x83174250={} ctrl+88={} -> slot {}",
            ctrl, kind, gate, state, int(bd::mem::load<u32>(ctrl + 172)));
}

REX_EXTERN(__imp__sub_822EAA80);
extern "C" REX_FUNC(sub_822EAA80) {
  const u32 self = ctx.r3.u32, slot = ctx.r4.u32;
  __imp__sub_822EAA80(ctx, base);
  if (REXCVAR_GET(wwe12_game_log))
    BD_INFO("[replay] keep moment replay sub_822EAA80(0x{:08X}, slot {}) = {}", self, slot,
            ctx.r3.u32);
}

// The same thread then runs sub_82959CC0(part) for each wrestler's 'CMIF' component: it notes
// the match's moments for the post-match replay and MATCH HIGHLIGHTS, after a row of checks.
// The moments land in 20 slots of 20 bytes at (*0x8317C84C)+128+6200 (word 0 != 0 = used, +8 =
// kind, see sub_821ED298; sub_821EDEE0 = "any moment?", which greys MATCH HIGHLIGHTS). Logs
// which checks pass, per wrestler, and the slots, on each change.
REX_EXTERN(__imp__sub_82959CC0);
extern "C" REX_FUNC(sub_82959CC0) {
  if (REXCVAR_GET(wwe12_game_log)) {
    const u32 self = ctx.r3.u32;
    const u32 actor = bd::mem::load<u32>(self + 72);
    constexpr u32 kMatchInfo = 0x8317C850;  // sub_8267CF38
    const u32 ctrl = bd::mem::load<u32>(0x83113ED8);
    // Bit set = that check lets the step go on (sub_82959CC0, in order).
    u32 pass = 0;
    pass |= (actor && bd::mem::load<u32>(actor + 1736) != 0) << 0;
    pass |= (bd::mem::load<u32>(self + 40) != 0) << 1;
    pass |= (bd::mem::load<u32>(0x8317B7BC) == 1) << 2;
    pass |= (bd::mem::load<u32>(kMatchInfo + 336) == 0) << 3;
    pass |= (actor && bd::mem::load<u32>(actor + 1196) == 0) << 4;
    pass |= (actor && bd::mem::load<u8>(actor + 2781) != 1) << 5;
    pass |= (!ctrl || bd::mem::load<u32>(ctrl + 88) != 3) << 6;
    pass |= (bd::mem::load<u8>(kMatchInfo + 6) == 0) << 7;  // else sub_82201250(actor) must be 0
    pass |= (bd::mem::load<u8>(kMatchInfo) != 25) << 8;     // sub_82A2FC18
    pass |= (bd::mem::load<u32>(self + 64) != 0) << 9;
    pass |= (bd::mem::load<u32>(self + 76) != 0) << 10;
    pass |= (bd::mem::load<u32>(self + 80) != 0) << 11;
    std::string slots;
    const u32 data = bd::mem::load<u32>(0x8317C84C);
    for (u32 i = 0; data && i < 20; ++i) {
      const u32 s = data + 128 + 6200 + i * 20;
      if (bd::mem::load<u32>(s))
        slots += fmt::format(" {}:kind {} t {}", i, bd::mem::load<u32>(s + 8),
                             bd::mem::load<u32>(s + 12));
    }
    static std::mutex s_mu;
    static std::string s_last[9];
    static u32 s_self[9] = {};
    const std::string now = fmt::format("checks 0x{:03X} (match kind {}, ctrl 0x{:08X}) moments:{}",
                                        pass, bd::mem::load<u8>(kMatchInfo), ctrl,
                                        slots.empty() ? " none" : slots);
    std::lock_guard lock(s_mu);
    u32 k = 0;
    while (k < 9 && s_self[k] && s_self[k] != self)
      ++k;
    if (k < 9 && s_last[k] != now) {
      s_self[k] = self;
      s_last[k] = now;
      BD_INFO("[replay] wrestler part 0x{:08X} (actor 0x{:08X}): {}", self, actor, now);
    }
  }
  __imp__sub_82959CC0(ctx, base);
}
