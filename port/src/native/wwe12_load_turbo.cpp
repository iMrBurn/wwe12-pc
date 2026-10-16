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
#include <unordered_map>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_load_turbo, false, "WWE12",
                    "Loading screens run the game as fast as possible (the loading animation spins "
                    "faster).");
REXCVAR_DEFINE_INT32(wwe12_load_turbo_frames, 6, "WWE12",
                     "During a turbo loading screen, game frames per frame shown.")
    .range(1, 16);
REXCVAR_DEFINE_BOOL(wwe12_load_turbo_vblank, true, "WWE12",
                    "Dev: the turbo also turns the SDK's vsync off (guest vblank at 1 kHz).");
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
std::atomic<int64_t> g_screen_since_ms{0};
std::atomic<u32> g_loader_calls{0}, g_presents{0}, g_skips{0};
std::atomic<u32> g_screen_calls[4];  // the four loading-screen base methods

std::mutex g_barrier_mu;
std::unordered_map<u32, u32> g_barrier_calls;  // guest thread (r13) -> barrier calls

struct TurboState {
  bool active = false;
  bool saved_vsync = true;
  bool vblank = false;  // this turbo turned the SDK's vsync off
  u32 frame = 0;
  int64_t since_ms = 0;
  int64_t probe_ms = 0;
};
TurboState g_turbo;  // the present thread only

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
          "load screen {}/{}/{}/{} | turbo {}",
          why, loader, frames, presents, skips, bd::mem::load<u32>(kReplayPhaseVa), screen[0],
          screen[1], screen[2], screen[3], g_turbo.active ? "on" : "off");
  g_turbo.probe_ms = now;
}

}  // namespace

// Called by the loader hook (wwe12_loading.cpp) each time the game's loader yields.
void Wwe12LoaderTick() {
  g_loader_calls.fetch_add(1, std::memory_order_relaxed);
}

// Called by Video::Present before presenting: true = render this frame but do not show it.
bool Wwe12LoadTurboSkipPresent() {
  const int64_t now = NowMs();
  const bool probe = REXCVAR_GET(wwe12_load_probe);
  const bool want = REXCVAR_GET(wwe12_load_turbo) &&
                    g_screen_up.load(std::memory_order_relaxed) &&
                    now - g_screen_since_ms.load(std::memory_order_relaxed) < kScreenMaxMs;
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
    } else if (t.vblank) {
      rex::cvar::SetFlagByName("vsync", t.saved_vsync ? "true" : "false");
    }
    t.active = want;
    BD_INFO("[turbo] {} (after {} ms)", want ? "on" : "off", now - t.since_ms);
    if (probe)
      ProbeLine(want ? "turbo on" : "turbo off", now);
  }
  if (probe && now - t.probe_ms >= 1000)
    ProbeLine("1 s", now);
  const bool skip =
      t.active && (t.frame++ % u32(std::max(1, REXCVAR_GET(wwe12_load_turbo_frames)))) != 0;
  (skip ? g_skips : g_presents).fetch_add(1, std::memory_order_relaxed);
  return skip;
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
#define WWE12_LOAD_SCREEN_HOOK(subroutine, index, opens)                         REX_EXTERN(__imp__##subroutine);                                              extern "C" REX_FUNC(subroutine) {                                               g_screen_calls[index].fetch_add(1, std::memory_order_relaxed);                if (opens) {                                                                    if (!g_screen_up.exchange(true, std::memory_order_relaxed))                     g_screen_since_ms.store(NowMs(), std::memory_order_relaxed);              } else {                                                                        g_screen_up.store(false, std::memory_order_relaxed);                        }                                                                             __imp__##subroutine(ctx, base);                                             }
WWE12_LOAD_SCREEN_HOOK(sub_826AF0F0, 0, true)
WWE12_LOAD_SCREEN_HOOK(sub_826A83B0, 1, false)
WWE12_LOAD_SCREEN_HOOK(sub_826A6B90, 2, false)
WWE12_LOAD_SCREEN_HOOK(sub_826B3060, 3, false)
