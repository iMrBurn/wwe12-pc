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
// - "60_entrances": 60 everywhere (the mode is kept at 60 when the game asks for 30);
// - "30": 30 everywhere (1.2, small PCs, issue #57): the game's own 30 fps mode, the one it uses
//   for entrances, also in menus and matches. The logic steps twice as far per frame and the game
//   waits 2 vblanks a frame, so speeds stay right while the CPU and GPU do half the frames.
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
}  // namespace

// wwe12_title_fix.cpp: the console only showed the even animation frames of these scenes.
bool Wwe12EntranceAt60() { return g_entrance_at60.load(std::memory_order_relaxed); }

// An entrance or a cutscene is playing (wwe12_remote_pad.cpp: the test scripts' "scene" state).
bool Wwe12SceneMode() { return g_scene_mode.load(std::memory_order_relaxed); }

// The whole game runs at 30 (wwe12_frame_mode = "30"): wwe12_benchmark.cpp's target.
bool Wwe12FrameMode30() { return Mode() == FrameMode::k30; }

// SetFrameRate(fps).
extern "C" REX_FUNC(sub_82827918) {
  const u32 requested = ctx.r3.u32;
  if (requested == 50 || requested == 25)
    g_pal.store(true, std::memory_order_relaxed);
  g_scene_mode.store(requested == 30 || requested == 25, std::memory_order_relaxed);
  const FrameMode mode = Mode();
  if (mode == FrameMode::k60Entrances && (requested == 30 || requested == 25)) {
    ctx.r3.u64 = requested == 30 ? 60 : 50;
    g_entrance_at60.store(true, std::memory_order_relaxed);
    BD_INFO("[fps] game set frame rate {}; using {}", requested, ctx.r3.u32);
  } else if (mode == FrameMode::k30 && (requested == 60 || requested == 50)) {
    // Not SetFrameRate(30): it leaves the step at +36 at its 60 fps value (only "drop to 30",
    // sub_82827AB8, sets it), and matches ran in slow motion (test 2026-11-09). The game's own 60
    // first, then its "drop to 30", exactly as for an entrance.
    g_entrance_at60.store(false, std::memory_order_relaxed);
    __imp__sub_82827918(ctx, base);
    __imp__sub_82827AB8(ctx, base);
    BD_INFO("[fps] game set frame rate {}; dropped to {} (30 fps mode)", requested,
            requested == 60 ? 30 : 25);
    return;
  } else {
    g_entrance_at60.store(false, std::memory_order_relaxed);
    BD_DEBUG("[fps] game set frame rate {}", requested);
  }
  __imp__sub_82827918(ctx, base);
}

// "Drop to 30 (25) fps": skipped while the 60 fps mode is kept.
extern "C" REX_FUNC(sub_82827AB8) {
  g_scene_mode.store(true, std::memory_order_relaxed);
  if (Mode() == FrameMode::k60Entrances) {
    g_entrance_at60.store(true, std::memory_order_relaxed);
#if defined(WWE12_DEV_PROBES)
    wwe12_dev_entrance_started();
#endif
    static std::atomic<u32> s_calls{0};
    const u32 n = s_calls.fetch_add(1, std::memory_order_relaxed);
    if (n < 10 || n % 100 == 0)
      BD_INFO("[fps] game switched to 30 fps mode; kept at 60");
    return;
  }
  __imp__sub_82827AB8(ctx, base);
}

// "Back to 60 (50) fps": the entrance or cutscene is over. In the 30 fps mode the game's own
// function still runs, then its "drop to 30" (the entrance mode, every step value set) at once.
extern "C" REX_FUNC(sub_82827B58) {
  g_entrance_at60.store(false, std::memory_order_relaxed);
  g_scene_mode.store(false, std::memory_order_relaxed);
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
