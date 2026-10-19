// WWE '12: entrances and cutscenes at 60 fps, the way nefariousjosiah's SvR ports do it.
//
// Yuke's engine keeps a frame-rate mode in a global struct (WWE '12: 0x8324CE28): +8 target fps
// (60/50/30/25), +12 mode lock, per-frame time-step floats (+28/+32/+36/+60/+64) and +40 ms per
// frame, all set together by SetFrameRate (sub_82827918, r3 = fps), "drop to 30" (sub_82827AB8)
// and "back to 60" (sub_82827B58). The game drops to 30 for entrances and cutscenes. Raising only
// the present rate would draw at 60 but still step the logic as at 30 (double-speed entrances);
// keeping the mode at 60 changes both together, so everything plays at the right speed.
//
// wwe12_60fps = false in wwe12.toml restores the original 30 fps.

#include <atomic>

#include <rex/cvar.h>

#include "core/logging.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_60fps, false, "WWE12",
                    "Keep the game's 60 fps mode (50 PAL) in entrances and cutscenes instead of "
                    "dropping to 30 (25); game timing follows the mode.");

#if defined(WWE12_DEV_PROBES)
void wwe12_dev_entrance_started();  // wwe12_autocap.cpp
#endif

REX_EXTERN(__imp__sub_82827918);
REX_EXTERN(__imp__sub_82827AB8);
REX_EXTERN(__imp__sub_82827B58);

namespace {
// True while the game wants 30 (an entrance or a cutscene) and runs at 60 instead.
std::atomic<bool> g_entrance_at60{false};
}  // namespace

// wwe12_title_fix.cpp: the console only showed the even animation frames of these scenes.
bool Wwe12EntranceAt60() { return g_entrance_at60.load(std::memory_order_relaxed); }

// SetFrameRate(fps).
extern "C" REX_FUNC(sub_82827918) {
  const u32 requested = ctx.r3.u32;
  if (REXCVAR_GET(wwe12_60fps) && (requested == 30 || requested == 25)) {
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
  if (REXCVAR_GET(wwe12_60fps)) {
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

// "Back to 60 (50) fps": the entrance or cutscene is over.
extern "C" REX_FUNC(sub_82827B58) {
  g_entrance_at60.store(false, std::memory_order_relaxed);
  __imp__sub_82827B58(ctx, base);
}
