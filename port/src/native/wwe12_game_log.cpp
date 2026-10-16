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
