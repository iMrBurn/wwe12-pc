// WWE '12: "turbo" loading screens, the way the WWE 2K14 recomp's author did it (same Yuke's
// engine), and a probe that measures the loads.
//
// The game's threads move in step with the frames: each one does its share, then waits at the
// frame barrier (sub_827DEA20: signals its event, waits for the go event) for the next frame. The
// loader too (sub_82819258 hands the device to the main thread for one frame, wwe12_loading.cpp).
// So a load that takes a few hundred frames takes seconds at 60 fps although the PC has almost
// nothing to do: measured 2026-10-16, every game thread waits, the render thread mostly sleeps in
// our frame pacing.
//
// The loader alone does not mark a loading screen: it also loads in the menus (wrestler select),
// and on the loading screen it works only ~1 s of ~4 (the rest is frame-paced waiting). The
// loading screens are 14 classes (one per mode: /LOAD/IG%02d, GN, CV, CA, UN... built by their
// vtable slot 13) sharing base methods: sub_826AF0F0 (slot 14), sub_826A83B0 (16), sub_826A6B90
// (22), sub_826B3060 (23), each in exactly those 14 vtables. They are not called every frame
// (measured 2026-10-16): slot 14 runs ~10 frames when the screen opens, slots 16/22/23 when it
// closes, right as the entrance starts. So the screen is "up" from the first slot-14 call to the
// first call of the others (or kScreenMaxMs at most), and in that window wwe12_load_turbo:
//   - turns the SDK's vsync cvar off, so the guest vblank ticks at 1 kHz instead of 60 Hz;
//   - shows one frame in wwe12_load_turbo_frames: the others are rendered but not presented
//     (Video::Present asks Wwe12LoadTurboSkipPresent), so neither the swap chain's vsync nor our
//     frame pacing hold the game back.
// The loading screen's animation then runs faster, which is the visible cost.
//
// wwe12_load_probe logs once a second (and at each turbo change) the loader calls, the frames the
// game's threads went through (barrier calls of the busiest thread), the presents and skips.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <fmt/format.h>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

// 1.3: on by default (the WWE 2K14 recomp does the same for everyone). The switch has a NEW key:
// every player's wwe12.toml still holds "wwe12_load_turbo = false" (1.2's shipped default, kept by
// the launcher's updates), so the old key is read but no longer used.
REXCVAR_DEFINE_BOOL(wwe12_fast_loading, true, "WWE12",
                    "Loading screens run the game as fast as possible; false = the original pace.");
REXCVAR_DEFINE_BOOL(wwe12_load_turbo, false, "WWE12",
                    "1.2's key for the turbo loading screens: no longer used (wwe12_fast_loading).");
REXCVAR_DEFINE_INT32(wwe12_load_turbo_frames, 16, "WWE12",
                     "During a turbo loading screen, game frames per frame shown.")
    .range(1, 32);
// 1.3: the hidden turbo frames used to be drawn in full and only their present skipped, so the
// render thread (draws, constants, geometry) held the game's frame rate back. Now the frame is
// known hidden when it starts (decided at the previous D3D Swap, hooks/device.cpp): its draws and
// clears are skipped like svr_test_half_frames does; the game's world, its D3D library and the
// resolves still run (the next shown frame draws everything again). false = the 1.2 behaviour.
REXCVAR_DEFINE_BOOL(wwe12_load_turbo_skip_draws, true, "WWE12",
                    "Turbo loading: frames that are not shown are not drawn either.");
REXCVAR_DEFINE_BOOL(wwe12_load_turbo_vblank, true, "WWE12",
                    "Dev: the turbo also turns the SDK's vsync off (guest vblank at 1 kHz).");
REXCVAR_DEFINE_BOOL(wwe12_load_turbo_scene_end, true, "WWE12",
                    "Turbo loading: end the turbo when the entrance script is asked for (no fast-forwarded start).");
REXCVAR_DEFINE_BOOL(wwe12_load_probe, false, "WWE12",
                    "Dev: log loader calls, game frames and presents each second.");
// The SDK's vsync cvar (guest vblank at 60 Hz when on, 1 kHz when off) lives in its DLL: by name.

namespace {

using Clock = std::chrono::steady_clock;
constexpr int64_t kScreenMaxMs = 20000;
constexpr u32 kReplayPhaseVa = 0x8317B7BC;  // 1 menus/load/match, 5 entrances (logged only)

int64_t NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch())
      .count();
}

std::atomic<bool> g_screen_up{false};
std::atomic<bool> g_scene_started{false};  // the window ended early: the screen's open calls don't reopen it
std::atomic<int64_t> g_screen_event_ms{0};  // last open or close of a loading screen
std::atomic<int64_t> g_screen_since_ms{0};
std::atomic<u32> g_loader_calls{0}, g_presents{0}, g_skips{0};
std::atomic<u32> g_screen_calls[4];  // the four loading-screen base methods

// Probe timings (microseconds) of the game's file reads and zlib.
std::atomic<u64> g_read_us{0}, g_read_wait_us{0}, g_inflate_us{0}, g_read_bytes{0};
std::atomic<u32> g_reads{0}, g_inflates{0};
thread_local bool t_in_read = false;

u64 NowUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(Clock::now().time_since_epoch())
      .count();
}

std::mutex g_barrier_mu;
std::unordered_map<u32, u32> g_barrier_calls;  // guest thread (r13) -> barrier calls

struct TurboState {
  bool active = false;
  bool saved_vsync = true;
  bool vblank = false;  // this turbo turned the SDK's vsync off
  u32 frame = 0;
  u32 shown = 0;  // frames shown during this turbo window (timing line)
  int64_t since_ms = 0;
  int64_t probe_ms = 0;
};
// Updated by the present thread (wwe12_load_turbo_skip_draws off) or by the thread that calls the
// game's D3D Swap (on): the mutex keeps a mid-game cvar change safe.
TurboState g_turbo;
std::mutex g_turbo_mu;

// Timing of the last loading screen (the "[loadtime]" line when it closes).
std::atomic<int64_t> g_turbo_on_ms{0};   // turbo time inside the current screen
std::atomic<u32> g_turbo_frames{0}, g_turbo_shown{0};
std::atomic<int64_t> g_closed_screen_ms{-1};  // last screen's time up, waiting to be logged

// The loading-screen window closes (its close methods or the entrance script): its time is kept
// and logged by the next frame, once the turbo has added its own numbers ("[loadtime]").
void ScreenClosed() {
  if (g_screen_up.exchange(false, std::memory_order_relaxed))
    g_closed_screen_ms.store(NowMs() - g_screen_since_ms.load(std::memory_order_relaxed),
                             std::memory_order_relaxed);
}

u32 BusiestThreadFrames() {
  std::lock_guard lock(g_barrier_mu);
  u32 most = 0;
  for (const auto &[tid, n] : g_barrier_calls)
    most = std::max(most, n);
  g_barrier_calls.clear();
  return most;
}

void ProbeLine(const char *why, int64_t now) {
  const u32 loader = g_loader_calls.exchange(0), presents = g_presents.exchange(0),
            skips = g_skips.exchange(0), frames = BusiestThreadFrames();
  u32 screen[4];
  for (u32 i = 0; i < 4; ++i)
    screen[i] = g_screen_calls[i].exchange(0);
  BD_INFO("[loadprobe] {}: loader {} | game frames {} | presented {} skipped {} | phase {} | "
          "load screen {}/{}/{}/{} | turbo {} | reads {} ({} KB) {} ms, waiting {} ms | "
          "inflate {} calls {} ms",
          why, loader, frames, presents, skips, bd::mem::load<u32>(kReplayPhaseVa), screen[0],
          screen[1], screen[2], screen[3], g_turbo.active ? "on" : "off", g_reads.exchange(0),
          g_read_bytes.exchange(0) >> 10, g_read_us.exchange(0) / 1000,
          g_read_wait_us.exchange(0) / 1000, g_inflates.exchange(0),
          g_inflate_us.exchange(0) / 1000);
  g_turbo.probe_ms = now;
}

}  // namespace

// Milliseconds since a loading screen last opened or closed (wwe12_quit.cpp scans the string
// tables again then: the game reloads them around loading screens).
int64_t Wwe12MsSinceLoadScreen() {
  return NowMs() - g_screen_event_ms.load(std::memory_order_relaxed);
}

// A loading screen is up (wwe12_remote_pad.cpp: the test scripts' "loading" state).
bool Wwe12LoadingScreenUp() { return g_screen_up.load(std::memory_order_relaxed); }

// The loading screen closes 0.1-0.4 s after the entrance behind it starts, so the turbo ran the
// entrance's first frames fast-forwarded (a visible jump). Its last step is the entrance script:
// sub_825B85B8(kind, number, out) builds a resource's name, kind 0 = "/GMEV/%04d" (the script; kind
// 11 the same). Asked on a loading screen, it ends the turbo window there (measured 2026-11-03: the
// entrance's first sound comes 0.1-0.4 s later, the screen closes ~0.15 s after that).
REX_EXTERN(__imp__sub_825B85B8);
extern "C" REX_FUNC(sub_825B85B8) {
  const u32 kind = ctx.r3.u32;
  if ((kind == 0 || kind == 11) && REXCVAR_GET(wwe12_load_turbo_scene_end) &&
      g_screen_up.load(std::memory_order_relaxed)) {
    g_scene_started.store(true, std::memory_order_relaxed);
    ScreenClosed();
    BD_INFO("[turbo] entrance script {} asked: turbo window closed", ctx.r4.u32);
  }
  __imp__sub_825B85B8(ctx, base);
}

// Called by the loader hook (wwe12_loading.cpp) each time the game's loader yields.
void Wwe12LoaderTick() {
  g_loader_calls.fetch_add(1, std::memory_order_relaxed);
}

namespace {
// One frame of the turbo: turns it on / off with the loading-screen window and says whether the
// frame it counts is hidden. Called once per game frame, from one place (see the two callers).
bool TurboFrame() {
  const int64_t now = NowMs();
  const bool probe = REXCVAR_GET(wwe12_load_probe);
  const bool want = REXCVAR_GET(wwe12_fast_loading) &&
                    g_screen_up.load(std::memory_order_relaxed) &&
                    now - g_screen_since_ms.load(std::memory_order_relaxed) < kScreenMaxMs;
  std::lock_guard lock(g_turbo_mu);
  TurboState &t = g_turbo;
  if (want != t.active) {
    if (want) {
      t.vblank = REXCVAR_GET(wwe12_load_turbo_vblank);
      if (t.vblank) {
        t.saved_vsync = rex::cvar::Query<bool>("vsync");
        rex::cvar::SetFlagByName("vsync", "false");
      }
      t.since_ms = now;
      t.frame = 0;
      t.shown = 0;
    } else {
      if (t.vblank)
        rex::cvar::SetFlagByName("vsync", t.saved_vsync ? "true" : "false");
      g_turbo_on_ms.fetch_add(now - t.since_ms, std::memory_order_relaxed);
      g_turbo_frames.fetch_add(t.frame, std::memory_order_relaxed);
      g_turbo_shown.fetch_add(t.shown, std::memory_order_relaxed);
    }
    t.active = want;
    if (want)
      BD_INFO("[turbo] on");
    else
      BD_INFO("[turbo] off after {} ms: {} game frames, {} shown ({} game frames/s)",
              now - t.since_ms, t.frame, t.shown,
              now > t.since_ms ? int64_t(t.frame) * 1000 / (now - t.since_ms) : 0);
    if (probe)
      ProbeLine(want ? "turbo on" : "turbo off", now);
  }
  if (!t.active) {
    const int64_t screen_ms = g_closed_screen_ms.exchange(-1, std::memory_order_relaxed);
    if (screen_ms >= 0)
      BD_INFO("[loadtime] loading screen {} ms (turbo {} ms, {} game frames, {} shown)", screen_ms,
              g_turbo_on_ms.exchange(0, std::memory_order_relaxed),
              g_turbo_frames.exchange(0, std::memory_order_relaxed),
              g_turbo_shown.exchange(0, std::memory_order_relaxed));
  }
  if (probe && now - t.probe_ms >= 1000)
    ProbeLine("1 s", now);
  const bool skip =
      t.active && (t.frame++ % u32(std::max(1, REXCVAR_GET(wwe12_load_turbo_frames)))) != 0;
  if (t.active && !skip)
    ++t.shown;
  (skip ? g_skips : g_presents).fetch_add(1, std::memory_order_relaxed);
  return skip;
}
}  // namespace

// ---- Fast boot (1.3): the first picture shown is the title screen ("Press START").
//
// From the first frame, the game runs as on a turbo loading screen (vsync off, guest vblank at
// 1 kHz, screen draws skipped) and NOTHING is presented: the warning / legal screens (~9 s of
// frame-timed still pictures before the start-up videos, measured 2026-11-10) and the start-up
// videos (skipped by wwe12_intros.cpp) are never seen; the window stays as it was. The sound is
// muted during the turbo (SDK cvar audio_mute), so the screens' sounds played fast are not heard.
// The title screen comes right after the opening video (start-up movie 2, which wwe12_intros.cpp
// skips): once it is asked for, the turbo stops (the game's own pace again) and the frames stay
// hidden wwe12_fast_boot_settle_ms longer (the title's fade-in); then everything is shown and
// heard as usual. Safety: whatever happens, after kBootMaxMs from the first frame the boot ends
// (never an endless black window). Needs wwe12_skip_intros (with the videos played, movie 2
// would be asked before its own showing). wwe12_fast_boot = false: the 1.2 start-up.
// "[boot]" lines: times from the process start (first frame, movies, end and why).
REXCVAR_DEFINE_BOOL(wwe12_fast_boot, true, "WWE12",
                    "Start-up: nothing is shown until the title screen (no warning screens).");
REXCVAR_DEFINE_INT32(wwe12_fast_boot_settle_ms, 800, "WWE12",
                     "Fast boot: hidden time at normal pace after the opening video, before the "
                     "title screen is shown.")
    .range(0, 5000);

namespace {
constexpr int64_t kBootMaxMs = 20000;
const int64_t g_process_start_ms = NowMs();  // (static init: before the runtime starts)

enum class Boot { kNotStarted, kTurbo, kSettle, kDone };
std::atomic<Boot> g_boot{Boot::kNotStarted};
std::atomic<int64_t> g_boot_first_ms{0}, g_boot_settle_ms{0};
bool g_boot_saved_vsync = true, g_boot_saved_mute = false;  // (frame thread only)
u32 g_boot_frames = 0;

bool BootWanted() {
  return REXCVAR_GET(wwe12_fast_boot) && rex::cvar::Query<bool>("wwe12_skip_intros");
}

void BootEnd(int64_t now, const char *why) {
  if (g_boot.exchange(Boot::kDone) == Boot::kDone)
    return;
  rex::cvar::SetFlagByName("vsync", g_boot_saved_vsync ? "true" : "false");
  rex::cvar::SetFlagByName("audio_mute", g_boot_saved_mute ? "true" : "false");
  BD_INFO("[boot] title screen shown at {} ms ({}): {} game frames hidden since the first frame "
          "at {} ms",
          now - g_process_start_ms, why, g_boot_frames,
          g_boot_first_ms.load() - g_process_start_ms);
}

// Once per game frame while the boot is not done: true = this frame is hidden.
bool BootFrame() {
  const int64_t now = NowMs();
  Boot b = g_boot.load();
  if (b == Boot::kNotStarted) {
    if (!BootWanted()) {
      g_boot = Boot::kDone;
      return false;
    }
    g_boot_first_ms = now;
    g_boot_saved_vsync = rex::cvar::Query<bool>("vsync");
    g_boot_saved_mute = rex::cvar::Query<bool>("audio_mute");
    rex::cvar::SetFlagByName("vsync", "false");
    rex::cvar::SetFlagByName("audio_mute", "true");
    g_boot = b = Boot::kTurbo;
    BD_INFO("[boot] first frame at {} ms: fast boot, nothing shown until the title screen",
            now - g_process_start_ms);
  }
  ++g_boot_frames;
  if (now - g_boot_first_ms.load() >= kBootMaxMs) {
    BootEnd(now, "safety limit, the opening video never came");
    return false;
  }
  if (b == Boot::kSettle) {
    if (now - g_boot_settle_ms.load() >= REXCVAR_GET(wwe12_fast_boot_settle_ms)) {
      BootEnd(now, "opening video skipped, settle time over");
      return false;
    }
  }
  return true;
}
}  // namespace

// wwe12_intros.cpp: a start-up movie (0, 1 logo, 2 opening) is asked for the first time.
void Wwe12BootMovieAsked(u32 movie) {
  const int64_t now = NowMs();
  BD_INFO("[boot] start-up movie {} asked at {} ms", movie, now - g_process_start_ms);
  if (movie != 2)
    return;
  Boot expected = Boot::kTurbo;
  if (g_boot.compare_exchange_strong(expected, Boot::kSettle)) {
    g_boot_settle_ms = now;
    // The game's own pace again for the title's start (its fade-in), still hidden; its sound
    // back now, so the title music starts as usual.
    rex::cvar::SetFlagByName("vsync", g_boot_saved_vsync ? "true" : "false");
    rex::cvar::SetFlagByName("audio_mute", g_boot_saved_mute ? "true" : "false");
  }
}

// Called by Video::Present before presenting: true = render this frame but do not show it.
// With wwe12_load_turbo_skip_draws (default) the frame was already hidden at its start
// (Wwe12LoadTurboHideNextFrame) and is never presented; the fast boot also hides here the very
// first frame (drawn before any Swap could hide it).
bool Wwe12LoadTurboSkipPresent() {
  if (REXCVAR_GET(wwe12_load_turbo_skip_draws)) {
    const Boot b = g_boot.load();
    return (b == Boot::kNotStarted && BootWanted()) || b == Boot::kTurbo || b == Boot::kSettle;
  }
  if (g_boot.load() != Boot::kDone && BootFrame())
    return true;
  return TurboFrame();
}

// Called by the game's D3D Swap (hooks/device.cpp) at the end of each frame: true = the frame that
// starts now is a hidden turbo frame (no draws, no clears, not presented).
void Wwe12Ag2dProbeTick();  // below (dev probe, off by default)
bool Wwe12LoadTurboHideNextFrame() {
  Wwe12Ag2dProbeTick();
  if (!REXCVAR_GET(wwe12_load_turbo_skip_draws))
    return false;
  if (g_boot.load() != Boot::kDone && BootFrame())
    return true;
  return TurboFrame();
}

// The frame barrier of the game's threads: counted per guest thread for the probe.
REX_EXTERN(__imp__sub_827DEA20);
extern "C" REX_FUNC(sub_827DEA20) {
  if (REXCVAR_GET(wwe12_load_probe)) {
    std::lock_guard lock(g_barrier_mu);
    ++g_barrier_calls[ctx.r13.u32];
  }
  __imp__sub_827DEA20(ctx, base);
}

// The loading screens' shared base methods: slot 14 opens the turbo window, the others close it.
// The opening also tells the Discord Rich Presence that a match is coming (wwe12_discord.cpp).
void Wwe12DiscordLoadScreen(const PPCContext &ctx, u8 *base, u32 screen);
void Wwe12FpsLoadScreenOpened(PPCContext &ctx, u8 *base);  // wwe12_fps.cpp: 30 fps mode, menus at 60
#define WWE12_LOAD_SCREEN_HOOK(subroutine, index, opens)                         REX_EXTERN(__imp__##subroutine);                                              extern "C" REX_FUNC(subroutine) {                                               g_screen_calls[index].fetch_add(1, std::memory_order_relaxed); g_screen_event_ms.store(NowMs(), std::memory_order_relaxed);                if (opens) {                                                                    if (!g_scene_started.load(std::memory_order_relaxed) &&                           !g_screen_up.exchange(true, std::memory_order_relaxed)) {                   g_screen_since_ms.store(NowMs(), std::memory_order_relaxed);                Wwe12DiscordLoadScreen(ctx, base, ctx.r3.u32); Wwe12FpsLoadScreenOpened(ctx, base);                            }                                                                           } else {                                                                        ScreenClosed();                          g_scene_started.store(false, std::memory_order_relaxed);                    }                                                                             __imp__##subroutine(ctx, base);                                             }
WWE12_LOAD_SCREEN_HOOK(sub_826AF0F0, 0, true)
WWE12_LOAD_SCREEN_HOOK(sub_826A83B0, 1, false)
WWE12_LOAD_SCREEN_HOOK(sub_826A6B90, 2, false)
WWE12_LOAD_SCREEN_HOOK(sub_826B3060, 3, false)

// Probe: the game's file read loop (sub_827EA330: reads 256 KB chunks asynchronously, then waits
// in an alertable sleep, sub_821AEAE0(-1, 1), for each completion), and zlib's inflate
// (sub_827F57A8, the function with zlib's error texts).
// While the u32 at 0x83248B00 is set (around the entrances), sub_827EA330 reads a file larger than one chunk (256 KB,
// 0x83079778) chunk by chunk with a 500 ms sleep after each (sub_821AEB18(*0x8307977C)): on the console, to leave
// the disc to the streams. An added superstar's ribbon or wall (1.7 MB) took 3 s each before his entrance, the
// game's own sometimes 0.5-1 s (2026-11-02). On a PC drive there is nothing to leave: the flag is down for the read
// (put back after, unless the game changed it meanwhile).
REXCVAR_DEFINE_BOOL(wwe12_read_throttle, false, "WWE12",
                    "Keep the console's slowed file reads around the entrances (256 KB, then 500 ms of sleep)");
REX_EXTERN(__imp__sub_827EA330);
namespace {
void ReadUnthrottled(PPCContext &ctx, u8 *base) {
  constexpr u32 kStreaming = 0x83248B00;
  const u32 was = REXCVAR_GET(wwe12_read_throttle) ? 0 : bd::mem::load<u32>(kStreaming);
  if (was)
    bd::mem::store<u32>(kStreaming, 0);
  __imp__sub_827EA330(ctx, base);
  if (was && bd::mem::load<u32>(kStreaming) == 0)
    bd::mem::store<u32>(kStreaming, was);
}
}  // namespace
extern "C" REX_FUNC(sub_827EA330) {
  if (!REXCVAR_GET(wwe12_load_probe) || !ctx.r3.u32) {
    ReadUnthrottled(ctx, base);
    return;
  }
  const u32 bytes = bd::mem::load<u32>(ctx.r3.u32 + 332);
  const u64 start = NowUs();
  t_in_read = true;
  ReadUnthrottled(ctx, base);
  t_in_read = false;
  g_read_us.fetch_add(NowUs() - start, std::memory_order_relaxed);
  g_read_bytes.fetch_add(bytes, std::memory_order_relaxed);
  g_reads.fetch_add(1, std::memory_order_relaxed);
}

REX_EXTERN(__imp__sub_821AEAE0);
extern "C" REX_FUNC(sub_821AEAE0) {
  if (!t_in_read) {
    __imp__sub_821AEAE0(ctx, base);
    return;
  }
  const u64 start = NowUs();
  __imp__sub_821AEAE0(ctx, base);
  g_read_wait_us.fetch_add(NowUs() - start, std::memory_order_relaxed);
}

// After a stream ends, the DLC sound tables are put back in order (wwe12_sound_tables.cpp).
void Wwe12FixSoundTableAfterInflate(u32 stream, u32 result);
REX_EXTERN(__imp__sub_827F57A8);
extern "C" REX_FUNC(sub_827F57A8) {
  const u32 stream = ctx.r3.u32;
  if (!REXCVAR_GET(wwe12_load_probe)) {
    __imp__sub_827F57A8(ctx, base);
    Wwe12FixSoundTableAfterInflate(stream, ctx.r3.u32);
    return;
  }
  const u64 start = NowUs();
  __imp__sub_827F57A8(ctx, base);
  g_inflate_us.fetch_add(NowUs() - start, std::memory_order_relaxed);
  g_inflates.fetch_add(1, std::memory_order_relaxed);
  Wwe12FixSoundTableAfterInflate(stream, ctx.r3.u32);
}

// Dev probe (1.3, off by default): finding what moves the loading screens' 2D animations (AG2D),
// to keep them at real-time speed under the turbo. sub_8287BC20(object, data, size, memory) loads
// an AG2D (or another 2D layout) into `object`: the objects loaded while a loading screen is up are
// kept, and once a second the u32 words of their first 0x200 bytes that changed are logged
// ("[ag2dprobe]"): the field that counts the animation's frames shows up, then a write trap on it
// names the function that advances it.
REXCVAR_DEFINE_BOOL(wwe12_ag2d_probe, false, "WWE12",
                    "Dev: log the 2D animation objects of the loading screens and their changing fields.");
namespace {
constexpr u32 kAg2dWords = 0x200 / 4;
struct Ag2dObject {
  u32 va = 0, caller = 0;
  u32 words[kAg2dWords] = {};
};
std::mutex g_ag2d_mu;
std::vector<Ag2dObject> g_ag2d;  // at most 16
int64_t g_ag2d_ms = 0;
}  // namespace

REX_EXTERN(__imp__sub_8287BC20);
extern "C" REX_FUNC(sub_8287BC20) {
  const u32 object = ctx.r3.u32, data = ctx.r4.u32, caller = u32(ctx.lr);
  __imp__sub_8287BC20(ctx, base);
  if (!REXCVAR_GET(wwe12_ag2d_probe) || !object)
    return;
  const u32 magic = data ? bd::mem::load<u32>(data) : 0;
  BD_INFO("[ag2dprobe] load object {:08X} data {:08X} magic {:08X} from {:08X} | screen {}",
          object, data, magic, caller, g_screen_up.load(std::memory_order_relaxed) ? "up" : "no");
  if (!g_screen_up.load(std::memory_order_relaxed))
    return;
  std::lock_guard lock(g_ag2d_mu);
  if (g_ag2d.size() >= 16)
    return;
  Ag2dObject o;
  o.va = object;
  o.caller = caller;
  for (u32 i = 0; i < kAg2dWords; ++i)
    o.words[i] = bd::mem::load<u32>(object + i * 4);
  g_ag2d.push_back(o);
}

// Called once a frame from the game's D3D Swap (Wwe12LoadTurboHideNextFrame).
void Wwe12Ag2dProbeTick() {
  if (!REXCVAR_GET(wwe12_ag2d_probe))
    return;
  const int64_t now = NowMs();
  std::lock_guard lock(g_ag2d_mu);
  if (!g_screen_up.load(std::memory_order_relaxed)) {
    if (!g_ag2d.empty())
      BD_INFO("[ag2dprobe] screen closed: {} objects forgotten", g_ag2d.size());
    g_ag2d.clear();
    return;
  }
  if (now - g_ag2d_ms < 1000)
    return;
  g_ag2d_ms = now;
  for (Ag2dObject &o : g_ag2d) {
    std::string changed;
    u32 count = 0;
    for (u32 i = 0; i < kAg2dWords; ++i) {
      const u32 w = bd::mem::load<u32>(o.va + i * 4);
      if (w != o.words[i]) {
        if (count++ < 12)
          changed += fmt::format(" +{:X}:{:08X}->{:08X}", i * 4, o.words[i], w);
        o.words[i] = w;
      }
    }
    BD_INFO("[ag2dprobe] object {:08X} (from {:08X}): {} words changed{}", o.va, o.caller, count,
            changed);
  }
}
