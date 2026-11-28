// WWE '12: option to skip the instant replay after matches (2.0, docs/etude_replay_option_2_0.md).
//
// At the end of a match sub_823F4910 (object in r3, state at +8) runs once per frame. In state 1 it
// decides at +28 whether the replay plays: in exhibition / Universe (mode G+12460 = 0, G = 0x8317C850)
// when the match changed a title (+24) or kept at least one moment (sub_821EDEE0); in Road to
// WrestleMania (2) on a title change only; in Create a Story (3 / 4) never, because sub_821D49A0
// ("mode 3 or 4?") returns 1 there. +28 = 0 goes straight to state 7 (done, 0x823F5144) and the
// victory scene, the menus and the Universe save follow as usual.
//
// With wwe12_skip_replay = true, sub_821D49A0 answers 1 while sub_823F4910 runs its state 1 (one
// call there, at 0x823F4AA4), so every mode takes Create a Story's own "no replay" path. Its other
// callers and the later states are untouched. The moments are still recorded: MATCH HIGHLIGHTS
// keeps working (unlike Android's wwe12_instant_replay, which turns the recording off).

#include <atomic>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_skip_replay, false, "WWE12",
                    "Skip the instant replay after matches (MATCH HIGHLIGHTS still works).");

namespace {
thread_local bool t_deciding = false;
constexpr u32 kMatchInfo = 0x8317C850;  // sub_8267CF38
}  // namespace

REX_EXTERN(__imp__sub_823F4910);
extern "C" REX_FUNC(sub_823F4910) {
  const u32 self = ctx.r3.u32;
  const u32 before = bd::mem::load<u32>(self + 8);
  if (before != 1 || !REXCVAR_GET(wwe12_skip_replay)) {
    __imp__sub_823F4910(ctx, base);
    return;
  }
  t_deciding = true;
  __imp__sub_823F4910(ctx, base);
  t_deciding = false;
  const u32 after = bd::mem::load<u32>(self + 8);
  if (after != before) {
    static std::atomic<int> s_logged{0};
    if (s_logged.fetch_add(1) < 32)
      BD_INFO("[replay] skip (wwe12_skip_replay): mode {} title change {} state 1 -> {}",
              bd::mem::load<u32>(kMatchInfo + 12460), bd::mem::load<u32>(self + 24), after);
  }
}

REX_EXTERN(__imp__sub_821D49A0);
extern "C" REX_FUNC(sub_821D49A0) {
  if (t_deciding) {
    t_deciding = false;  // the first call only: the one at 0x823F4AA4, not one deeper in a callee
    ctx.r3.u64 = 1;
    return;
  }
  __imp__sub_821D49A0(ctx, base);
}
