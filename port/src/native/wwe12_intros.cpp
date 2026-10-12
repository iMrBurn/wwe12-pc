// WWE '12 PC: skip the start-up videos (publisher/developer logos, then the opening), as the
// SvR 2011 PC port does, to reach the title screen faster.
//
// sub_821A9AC0(buffer, movie) writes the path of movie number `movie` into the buffer from the
// game's movie table (0x82001468: 0 and 1 = GAME:\movies\logo.bik, 2 = GAME:\movies\wweopen.bik,
// 3 = rtwmop.bik, ...) and returns 1. Its callers hand the buffer to the movie player without
// checking anything; for the first request of each start-up movie the hook leaves the path empty.
// The player then shows nothing and waits for Start, the button that skips a video: the hook on
// the game's XInputGetState (sub_82A8C070) presses Start for player 1 once, 0.15 s long, 0.2 s
// later. One press per skipped video, so nothing reaches the title screen or the menus. Later
// requests (the opening shown again on the title screen when idle) play as usual.
//
// wwe12_skip_intros = false in wwe12.toml brings the videos back.

#include <atomic>
#include <chrono>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_skip_intros, true, "WWE12",
                    "Skip the start-up videos (logos and opening) to reach the title screen faster.");

REX_EXTERN(__imp__sub_821A9AC0);
REX_EXTERN(__imp__sub_82A8C070);

namespace {
// Window (steady clock, ms) during which player 1's Start is held down.
std::atomic<long long> g_press_from{0}, g_press_until{0};

long long NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
}  // namespace

extern "C" REX_FUNC(sub_821A9AC0) {
  static std::atomic<u32> s_skipped{0};  // bit per start-up movie already skipped once
  const u32 movie = ctx.r4.u32;
  if (REXCVAR_GET(wwe12_skip_intros) && movie <= 2) {
    const u32 bit = 1u << movie;
    if (!(s_skipped.fetch_or(bit) & bit)) {
      bd::mem::store<u8>(ctx.r3.u32, 0);
      ctx.r3.u64 = 0;
      const long long now = NowMs();
      g_press_from = now + 200;
      g_press_until = now + 350;
      BD_INFO("[intros] start-up video {} skipped", movie);
      return;
    }
  }
  __imp__sub_821A9AC0(ctx, base);
}

// XInputGetState(user, state): X_INPUT_STATE is {u32 packet, u16 buttons, ...}, big-endian.
extern "C" REX_FUNC(sub_82A8C070) {
  const u32 user = ctx.r3.u32, state = ctx.r4.u32;
  __imp__sub_82A8C070(ctx, base);
  if (user != 0 || !state)
    return;
  const long long now = NowMs();
  if (now < g_press_from.load() || now >= g_press_until.load())
    return;
  constexpr u16 kStart = 0x0010;
  if (ctx.r3.u32 != 0) {  // no controller on player 1: report one, idle
    for (u32 i = 0; i < 16; i += 4)
      bd::mem::store<u32>(state + i, 0);
    ctx.r3.u64 = 0;
  }
  bd::mem::store<u32>(state, bd::mem::load<u32>(state) + 1);
  bd::mem::store<u16>(state + 4, u16(bd::mem::load<u16>(state + 4) | kStart));
}
