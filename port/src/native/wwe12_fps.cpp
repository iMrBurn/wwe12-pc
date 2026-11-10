// WWE '12: the game's frame rate. Entrances and cutscenes at 60 fps, the way nefariousjosiah's SvR
// ports do it, or (1.2) the whole game at 30 fps for small PCs.
//
// Yuke's engine keeps a frame-rate mode in a global struct (WWE '12: 0x8324CE28): +8 target fps
// (60/50/30/25), +12 mode lock, per-frame time-step floats (+28/+32/+36/+60/+64) and +40 ms per
// frame, all set together by SetFrameRate (sub_82827918, r3 = fps), "drop to 30" (sub_82827AB8)
// and "back to 60" (sub_82827B58). The game drops to 30 for entrances and cutscenes. Raising only
// the present rate would draw at 60 but still step the logic as at 30 (double-speed entrances);
// keeping the mode at 60 changes both together, so everything plays at the right speed.
//
// wwe12_frame_mode:
// - "original": the game's own rates (60 in menus and matches, 30 in entrances and cutscenes);
// - "60_entrances": 60 in entrances and victory scenes (1.3), story scenes (Road to WrestleMania, Create a Story...) at the game's own 30. Before 1.3 every
//   scene was kept at 60: a PC that can't draw a cutscene at 60 fell behind its sound (issue #63);
// - "30" (EXPERIMENTAL, 1.2, small PCs, issue #57): the game's own 30 fps mode, the one it uses
//   for entrances, kept on in matches (menus and loading at 60). The logic steps twice as far per
//   frame and the game waits 2 vblanks a frame, so speeds stay right while the CPU and GPU do half
//   the frames. Some moves look a little off (the matches were made for 60). The switch happens when
//   the entrances end, and (1.3, issue #61) when a match's loading screen closes, entrances or not.
// Empty (older wwe12.toml): wwe12_60fps decides between "original" and "60_entrances".

#include <atomic>
#include <string>

#include <rex/cvar.h>

#include "core/logging.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_60fps, false, "WWE12",
                    "Keep the game's 60 fps mode (50 PAL) in entrances and cutscenes instead of "
                    "dropping to 30 (25); game timing follows the mode. Superseded by "
                    "wwe12_frame_mode when that is set.");
REXCVAR_DEFINE_STRING(wwe12_frame_mode, "", "WWE12",
                      "Frame rate: \"original\" (60, entrances 30), \"60_entrances\" (60 everywhere), "
                      "\"30\" (30 everywhere, for small PCs; the game's own 30 fps mode). Empty = "
                      "wwe12_60fps decides.");

#if defined(WWE12_DEV_PROBES)
void wwe12_dev_entrance_started();  // wwe12_autocap.cpp
#endif

// 1.3, issue #63: which scenes the 60 fps mode keeps at 60. Every scene asks for 30 through the same
// function, but each one first asks for its script, "/GMEV/%04d" (sub_825B85B8, wwe12_load_turbo.cpp).
// Numbers seen in game (tests 2026-11-10): superstar entrance = the superstar's number (123, 139, 161,
// 219), Road to WrestleMania cutscene 1010 / 1810, Create a Story 4000, victory 6000 + the winner
// (6123). Entrances (below 1000) and victories (6000-6999) stay at 60, every other scene at 30.
REXCVAR_DEFINE_BOOL(wwe12_60fps_all_scenes, false, "WWE12",
                    "60 fps entrances: also keep story scenes (Road to WrestleMania, Create a Story) at "
                    "60, as before 1.3.");

REX_EXTERN(__imp__sub_82827918);
REX_EXTERN(__imp__sub_82827AB8);
REX_EXTERN(__imp__sub_82827B58);

namespace {
// True while the game wants 30 (an entrance or a cutscene) and runs at 60 instead.
std::atomic<bool> g_entrance_at60{false};
// True while the game wants 30 (25), whether or not it is kept at 60.
std::atomic<bool> g_scene_mode{false};
// The game asked for 50 or 25 once (PAL timing): the 30 fps mode then uses 25.
std::atomic<bool> g_pal{false};
// The guest memory base, kept from a hook: "drop to 30" (sub_82827AB8) is a leaf that only stores into
// the frame-rate struct, so it can be run with a scratch context where no hook gives one.
std::atomic<u8 *> g_base{nullptr};
// 30 fps mode: a match is being loaded (its loading screen's closing drops to 30).
std::atomic<bool> g_match_pending{false};
// The last scene script the game thread asked for since it went back to 60 (none: kNoScript). Per
// thread: the loader thread asks for the next scene's script early (seen: 219 while 123 started).
constexpr u32 kNoScript = 0xFFFFFFFFu;
thread_local u32 t_scene_script = kNoScript;

void DropTo30Now() {
  u8 *base = g_base.load(std::memory_order_relaxed);
  if (!base)
    return;
  PPCContext scratch{};
  __imp__sub_82827AB8(scratch, base);
}

enum class FrameMode { kOriginal, k60Entrances, k30 };

FrameMode Mode() {
  const std::string m = REXCVAR_GET(wwe12_frame_mode);
  if (m == "30")
    return FrameMode::k30;
  if (m == "60_entrances" || m == "60")
    return FrameMode::k60Entrances;
  if (m == "original")
    return FrameMode::kOriginal;
  return REXCVAR_GET(wwe12_60fps) ? FrameMode::k60Entrances : FrameMode::kOriginal;
}

// The 60 fps mode keeps this scene at 60: an entrance or a victory. A scene with no script asked
// since the last "back to 60" stays at the game's 30.
bool KeepAt60() {
  if (Mode() != FrameMode::k60Entrances)
    return false;
  if (REXCVAR_GET(wwe12_60fps_all_scenes))
    return true;
  const u32 script = t_scene_script;
  return script < 1000 || (script >= 6000 && script < 7000);
}
}  // namespace

// wwe12_title_fix.cpp: the console only showed the even animation frames of these scenes.
bool Wwe12EntranceAt60() { return g_entrance_at60.load(std::memory_order_relaxed); }

// An entrance or a cutscene is playing (wwe12_remote_pad.cpp: the test scripts' "scene" state).
bool Wwe12SceneMode() { return g_scene_mode.load(std::memory_order_relaxed); }

// The whole game runs at 30 (wwe12_frame_mode = "30"): wwe12_benchmark.cpp's target.
bool Wwe12FrameMode30() { return Mode() == FrameMode::k30; }

// SetFrameRate(fps).
extern "C" REX_FUNC(sub_82827918) {
  g_base.store(base, std::memory_order_relaxed);
  const u32 requested = ctx.r3.u32;
  if (requested == 50 || requested == 25)
    g_pal.store(true, std::memory_order_relaxed);
  g_scene_mode.store(requested == 30 || requested == 25, std::memory_order_relaxed);
  if ((requested == 30 || requested == 25) && KeepAt60()) {
    ctx.r3.u64 = requested == 30 ? 60 : 50;
    g_entrance_at60.store(true, std::memory_order_relaxed);
    BD_INFO("[fps] game set frame rate {}; using {}", requested, ctx.r3.u32);
  } else {
    g_entrance_at60.store(false, std::memory_order_relaxed);
    BD_DEBUG("[fps] game set frame rate {}", requested);
  }
  __imp__sub_82827918(ctx, base);
}

// "Drop to 30 (25) fps": skipped while the 60 fps mode is kept.
extern "C" REX_FUNC(sub_82827AB8) {
  g_base.store(base, std::memory_order_relaxed);
  // Which kind of scene asks for 30 (1.3 study: 60 fps for entrances and victory scenes only). The game
  // has four callers: sub_823DEA10, sub_828DEAB8, sub_828DFDB8, sub_82DCAEE8 (return addresses logged).
  {
    static std::atomic<u32> s_logged{0};
    if (s_logged.fetch_add(1, std::memory_order_relaxed) < 200)
      BD_INFO("[fps] drop to 30 asked from {:08X}, scene script {}", u32(ctx.lr),
              int32_t(t_scene_script));
  }
  g_scene_mode.store(true, std::memory_order_relaxed);
  if (KeepAt60()) {
    g_entrance_at60.store(true, std::memory_order_relaxed);
#if defined(WWE12_DEV_PROBES)
    wwe12_dev_entrance_started();
#endif
    static std::atomic<u32> s_calls{0};
    const u32 n = s_calls.fetch_add(1, std::memory_order_relaxed);
    if (n < 10 || n % 100 == 0)
      BD_INFO("[fps] game switched to 30 fps mode; kept at 60 (entrance or victory)");
    return;
  }
  if (Mode() == FrameMode::k60Entrances)
    BD_INFO("[fps] story scene (script {}): the game's 30",
            int32_t(t_scene_script));
  g_entrance_at60.store(false, std::memory_order_relaxed);
  __imp__sub_82827AB8(ctx, base);
}

// A loading screen opens (wwe12_load_turbo.cpp, game thread): in the 30 fps mode, menus and
// loading run at 60 again (at 30 they felt sluggish, as in Kaiko's SvR 2011 port): the game's own
// "back to 60", which undoes its "drop to 30" (does nothing when already at 60).
void Wwe12FpsLoadScreenOpened(PPCContext &ctx, u8 *base) {
  g_base.store(base, std::memory_order_relaxed);
  if (Mode() != FrameMode::k30)
    return;
  const auto r3 = ctx.r3;
  __imp__sub_82827B58(ctx, base);
  ctx.r3 = r3;
  BD_INFO("[fps] loading screen: back to 60 until the match (30 fps mode)");
}

// "Back to 60 (50) fps": the entrance or cutscene is over. In the 30 fps mode (matches only: from
// the end of the entrances to the next loading screen) the game's own function still runs, then
// its "drop to 30" (the entrance mode, every step value set; SetFrameRate(30) left the +36 step at
// its 60 fps value and matches ran in slow motion, test 2026-11-09).
extern "C" REX_FUNC(sub_82827B58) {
  g_base.store(base, std::memory_order_relaxed);
  g_entrance_at60.store(false, std::memory_order_relaxed);
  g_scene_mode.store(false, std::memory_order_relaxed);
  t_scene_script = kNoScript;
  __imp__sub_82827B58(ctx, base);
  if (Mode() == FrameMode::k30) {
    const auto r3 = ctx.r3;
    __imp__sub_82827AB8(ctx, base);
    ctx.r3 = r3;
    static std::atomic<u32> s_calls{0};
    if (s_calls.fetch_add(1, std::memory_order_relaxed) < 10)
      BD_INFO("[fps] game went back to 60 fps mode; dropped to {} again (30 fps mode)",
              g_pal.load(std::memory_order_relaxed) ? 25 : 30);
  }
}

// 1.3, issue #61: the game's match start (sub_822B51A0, wwe12_benchmark.cpp). In the 30 fps mode the
// match's loading screen closing drops to 30 (a match without entrances never ended any, so it stayed
// at 60); with entrances, the game's own entrance mode and the end of the entrances do the same.
void Wwe12FpsMatchStarting() {
  if (Mode() == FrameMode::k30)
    g_match_pending.store(true, std::memory_order_relaxed);
}

// wwe12_load_turbo.cpp: a loading screen closed (game thread).
void Wwe12FpsLoadScreenClosed() {
  if (Mode() != FrameMode::k30 || !g_match_pending.exchange(false, std::memory_order_relaxed))
    return;
  DropTo30Now();
  BD_INFO("[fps] match loaded: dropped to {} (30 fps mode, entrances or not)",
          g_pal.load(std::memory_order_relaxed) ? 25 : 30);
}

// wwe12_load_turbo.cpp: a scene script "/GMEV/%04d" is asked for (game thread).
void Wwe12FpsSceneScript(u32 number) {
  t_scene_script = number;
  static std::atomic<u32> s_logged{0};
  if (s_logged.fetch_add(1, std::memory_order_relaxed) < 200)
    BD_INFO("[fps] scene script {} asked", number);
}
