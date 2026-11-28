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

#include <algorithm>
#include <atomic>
#include <chrono>

#include <rex/cvar.h>
#include <rex/memory.h>
#include <rex/system/kernel_state.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_skip_intros, true, "WWE12",
                    "Skip the start-up videos (logos and opening) to reach the title screen faster.");

REX_EXTERN(__imp__sub_821A9AC0);
REX_EXTERN(__imp__sub_82A8C070);

u16 Wwe12RemotePadButtons();  // wwe12_remote_pad.cpp (dev)
u16 Wwe12MouseMenuButtons();  // wwe12_mouse_menus.cpp: the mouse in the menus (A, B, up / down)
bool Wwe12FreeCamPad(u32 user, u32 state);  // wwe12_match_camera.cpp: the free camera has the pads (zeroed)
void Wwe12TurnStick(u32 state);             // wwe12_match_camera.cpp: the left stick turned for our view
void Wwe12MouseProbePad(u16 buttons, int16_t lx, int16_t ly);  // wwe12_mouse_probe.cpp (dev)
u16 Wwe12MatchPad(u32 user, u16 buttons, u8 lt, u8 rt);  // wwe12_match_modes.cpp: buttons to hide from the game
int Wwe12JukeboxPad(uint32_t user, uint16_t buttons, int16_t lx, int16_t ly);  // wwe12_jukebox.cpp: -1 = all hidden
void Wwe12BootMovieAsked(u32 movie);  // wwe12_load_turbo.cpp (fast boot)
void Wwe12BenchmarkRecordPad(u16 buttons, int16_t stick_x, int16_t stick_y, bool loading);  // wwe12_benchmark.cpp
bool Wwe12LoadingScreenUp();  // wwe12_load_turbo.cpp

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
      Wwe12BootMovieAsked(movie);  // wwe12_load_turbo.cpp: the fast boot ends after movie 2
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

// 2.0 (chantier 3, 2026-11-28): every pad plugged in plays player 1 (a pad Windows put in another slot, a
// PlayStation or generic pad the SDK mapped to slot 2...). Player 1's read gets all the connected pads merged (buttons
// together, the strongest stick and trigger), the keyboard included (it is slot 1); the other slots read "not
// connected", so no pad also plays player 2. Off by default (local multiplayer needs the slots).
REXCVAR_DEFINE_BOOL(wwe12_all_pads_p1, false, "WWE12",
                    "Every controller plugged in plays player 1 (no local multiplayer while on).");

namespace {
constexpr u32 kNotConnected = 0x48F;  // ERROR_DEVICE_NOT_CONNECTED

// Player 1's read with every slot merged into it (the hook's original call already filled slot 1's).
void MergeAllPads(PPCContext &ctx, const PPCContext &in, u8 *base, u32 state) {
  static u32 s_scratch = 0;
  if (!s_scratch)
    s_scratch = REX_KERNEL_MEMORY()->SystemHeapAlloc(16, 0x10);
  if (!s_scratch)
    return;
  bool any = ctx.r3.u32 == 0;
  if (!any)
    for (u32 i = 0; i < 16; i += 4)
      bd::mem::store<u32>(state + i, 0);
  u32 packet = bd::mem::load<u32>(state);
  for (u32 u = 1; u < 4; ++u) {
    PPCContext x = in;
    x.r3.u64 = u;
    x.r4.u64 = s_scratch;
    __imp__sub_82A8C070(x, base);
    if (x.r3.u32 != 0)
      continue;
    any = true;
    packet += bd::mem::load<u32>(s_scratch);
    bd::mem::store<u16>(state + 4, u16(bd::mem::load<u16>(state + 4) | bd::mem::load<u16>(s_scratch + 4)));
    for (u32 t = 6; t < 8; ++t)
      bd::mem::store<u8>(state + t, std::max(bd::mem::load<u8>(state + t), bd::mem::load<u8>(s_scratch + t)));
    for (u32 stick = 8; stick < 16; stick += 4) {  // a stick (x, y) whole: the one pushed further
      const int ax = bd::mem::load<int16_t>(state + stick), ay = bd::mem::load<int16_t>(state + stick + 2);
      const int bx = bd::mem::load<int16_t>(s_scratch + stick), by = bd::mem::load<int16_t>(s_scratch + stick + 2);
      if (bx * bx + by * by > ax * ax + ay * ay) {
        bd::mem::store<int16_t>(state + stick, int16_t(bx));
        bd::mem::store<int16_t>(state + stick + 2, int16_t(by));
      }
    }
  }
  if (any) {
    bd::mem::store<u32>(state, packet);
    ctx.r3.u64 = 0;
  }
}
}  // namespace

// XInputGetState(user, state): X_INPUT_STATE is {u32 packet, u16 buttons, ...}, big-endian.
extern "C" REX_FUNC(sub_82A8C070) {
  const u32 user = ctx.r3.u32, state = ctx.r4.u32;
  const PPCContext in = ctx;
  __imp__sub_82A8C070(ctx, base);
  if (REXCVAR_GET(wwe12_all_pads_p1) && state && user < 4) {
    if (user != 0) {
      ctx.r3.u64 = kNotConnected;  // (its pad plays player 1)
      return;
    }
    MergeAllPads(ctx, in, base, state);
  }
  const bool free_cam = state && ctx.r3.u32 == 0 && Wwe12FreeCamPad(user, state);
  if (state && ctx.r3.u32 == 0 && !free_cam)
    Wwe12TurnStick(state);  // RAMP / a kept free camera: the left stick in our view's frame
  if (state && ctx.r3.u32 == 0 && !free_cam) {  // (Univers, 2.0) the jukebox's MY PLAYLIST screen takes the pad
    const int hide = Wwe12JukeboxPad(user, bd::mem::load<u16>(state + 4), bd::mem::load<int16_t>(state + 8),
                                     bd::mem::load<int16_t>(state + 10));
    if (hide < 0) {  // (open: buttons, triggers, sticks all hidden; nothing else of ours on top)
      bd::mem::store<u16>(state + 4, 0);
      bd::mem::store<u16>(state + 6, 0);
      for (u32 i = 8; i < 16; i += 4)
        bd::mem::store<u32>(state + i, 0);
      return;
    }
    if (hide)
      bd::mem::store<u16>(state + 4, u16(bd::mem::load<u16>(state + 4) & ~u32(hide)));
  }
  if (state && ctx.r3.u32 == 0 && !free_cam) {  // match modes (1.4): triggers on the character select, I QUIT's buttons
    const u16 buttons = bd::mem::load<u16>(state + 4);
    if (const u16 hide = Wwe12MatchPad(user, buttons, bd::mem::load<u8>(state + 6), bd::mem::load<u8>(state + 7)))
      bd::mem::store<u16>(state + 4, u16(buttons & ~hide));
  }
  if (user != 0 || !state)
    return;
  if (ctx.r3.u32 == 0)  // (dev: the mouse probe's presses)
    Wwe12MouseProbePad(bd::mem::load<u16>(state + 4), bd::mem::load<int16_t>(state + 8), bd::mem::load<int16_t>(state + 10));
  if (ctx.r3.u32 == 0)  // a pad is there: its own buttons, for the benchmark's route recorder (dev)
    Wwe12BenchmarkRecordPad(bd::mem::load<u16>(state + 4), bd::mem::load<int16_t>(state + 8),
                            bd::mem::load<int16_t>(state + 10), Wwe12LoadingScreenUp());
  const long long now = NowMs();
  u16 press = u16(Wwe12RemotePadButtons() | Wwe12MouseMenuButtons());
  constexpr u16 kStart = 0x0010;
  if (now >= g_press_from.load() && now < g_press_until.load())
    press |= kStart;
  if (!press)
    return;
  if (ctx.r3.u32 != 0) {  // no controller on player 1: report one, idle
    for (u32 i = 0; i < 16; i += 4)
      bd::mem::store<u32>(state + i, 0);
    ctx.r3.u64 = 0;
  }
  bd::mem::store<u32>(state, bd::mem::load<u32>(state) + 1);
  bd::mem::store<u16>(state + 4, u16(bd::mem::load<u16>(state + 4) | press));
}

// The free camera's F8 (wwe12_match_camera.cpp): Start pressed once for player 1, 0.15 s long (the game's pause).
void Wwe12PressStart() {
  const long long now = NowMs();
  g_press_from = now + 30;
  g_press_until = now + 180;
}
