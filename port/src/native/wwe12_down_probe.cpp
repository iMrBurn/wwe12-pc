// Probe (wwe12_down_probe, off by default): how a move ends, face up or face down.
//
// Question from the Mods session (2026-11-09): an added move (SvR 2011 Batista Bomb on DLC
// Batista, move 57284, anim 50399) ends with the opponent on his back, then the game rolls him
// onto his stomach; the DLC's own Batista Bomb (anim 50429) does not. Which state does the game
// choose at the end of the move, and from what?
//
// A host thread polls the match actors (0x8317B770, 0-5 = superstars) every 5 ms and logs each
// change of their state fields ("[au_sol]"):
//   st   = u16 at [actor+680]  (state / animation in progress)
//   move = u16 at actor+700    (move number)
//   e1   = u8 at [actor+684], e2 = u8 at [actor+688] (step bytes)
//   a/b/c = u32 at actor+1076, +1096, +1188 (read by move events, sub_824A5458)
//   opp  = actor+3032 (opponent's index)
// Comparing both moves' lines shows which state differs at the end.

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(wwe12_down_probe, false, "WWE12",
                    "Probe: log the superstars' state changes in a match (face up / face down at the end of a move).");

#include <chrono>
#include <string>
#include <thread>

#include <fmt/format.h>

#include "core/logging.h"
#include "core/memory_helpers.h"

void Wwe12DownProbeStart();

namespace {

std::string Line(u32 actor) {
  using bd::mem::try_field;
  const u32 st = try_field<u32>(actor, 680);
  const u32 p1 = try_field<u32>(actor, 684);
  const u32 p2 = try_field<u32>(actor, 688);
  return fmt::format("st {} move {} e1 {} e2 {} a {} b {} c {} opp {}",
                     st ? try_field<u16>(st, 0) : 0xFFFF, try_field<u16>(actor, 700),
                     p1 ? try_field<u8>(p1, 0) : 0xFF, p2 ? try_field<u8>(p2, 0) : 0xFF,
                     try_field<u32>(actor, 1076), try_field<u32>(actor, 1096),
                     try_field<u32>(actor, 1188), try_field<u32>(actor, 3032));
}

void Poll() {
  constexpr u32 kActors = 0x8317B770;
  std::string last[6];
  const auto t0 = std::chrono::steady_clock::now();
  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - t0)
                             .count();
    for (u32 i = 0; i < 6; ++i) {
      const u32 actor = bd::mem::try_load<u32>(kActors + i * 4);
      std::string line = actor ? Line(actor) : std::string("-");
      if (line != last[i]) {
        BD_INFO("[au_sol] {} acteur {} : {}", ms, i, line);
        last[i] = std::move(line);
      }
    }
  }
}

}  // namespace

void Wwe12DownProbeStart() {
  if (!REXCVAR_GET(wwe12_down_probe))
    return;
  std::thread(Poll).detach();
  BD_INFO("[au_sol] sonde active");
}
