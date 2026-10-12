// Dev-only: compiled only with the WWE12_DEV_PROBES environment variable at configure time.
// When the game enters an entrance (the "drop to 30 fps" call) and runs under RenderDoc, this asks
// RenderDoc for series of consecutive-frame captures at fixed delays, so nobody has to press F12
// at the right millisecond. Parameters: wwe12_autocap_delay_ms, _gap_ms, _series, _frames.
#if defined(WWE12_DEV_PROBES)

#include <windows.h>

#include <atomic>
#include <chrono>
#include <thread>

#include <rex/cvar.h>

#include "core/logging.h"
#include "C:/Users/melfo/Documents/WWE12-PC/renderdoc/RenderDoc_1.46_64/renderdoc_app.h"

REXCVAR_DEFINE_BOOL(wwe12_autocap, false, "WWE12",
                    "Debug: under RenderDoc, capture frame series automatically after an entrance starts.");
REXCVAR_DEFINE_INT32(wwe12_autocap_delay_ms, 1500, "WWE12", "Debug: delay before the first capture series.");
REXCVAR_DEFINE_INT32(wwe12_autocap_gap_ms, 900, "WWE12", "Debug: gap between capture series.");
REXCVAR_DEFINE_INT32(wwe12_autocap_series, 10, "WWE12", "Debug: number of capture series.");
REXCVAR_DEFINE_INT32(wwe12_autocap_frames, 2, "WWE12", "Debug: consecutive frames per series.");

void wwe12_dev_entrance_started() {
  if (!REXCVAR_GET(wwe12_autocap))
    return;
  static std::atomic<long long> s_last_ms{-1000000};
  const long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count();
  long long last = s_last_ms.load();
  if (now - last < 30000 || !s_last_ms.compare_exchange_strong(last, now))
    return;
  HMODULE mod = GetModuleHandleA("renderdoc.dll");
  auto get_api = mod ? reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(mod, "RENDERDOC_GetAPI")) : nullptr;
  RENDERDOC_API_1_4_2 *api = nullptr;
  if (!get_api || !get_api(eRENDERDOC_API_Version_1_4_2, reinterpret_cast<void **>(&api)) || !api) {
    BD_WARN("[autocap] RenderDoc is not loaded: nothing to capture");
    return;
  }
  const int delay = REXCVAR_GET(wwe12_autocap_delay_ms), gap = REXCVAR_GET(wwe12_autocap_gap_ms);
  const int series = REXCVAR_GET(wwe12_autocap_series), frames = REXCVAR_GET(wwe12_autocap_frames);
  BD_INFO("[autocap] entrance started: {} series of {} frames, first in {} ms, every {} ms", series, frames, delay, gap);
  std::thread([=] {
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    for (int i = 0; i < series; ++i) {
      api->TriggerMultiFrameCapture(frames);
      BD_INFO("[autocap] series {} triggered", i + 1);
      std::this_thread::sleep_for(std::chrono::milliseconds(gap));
    }
  }).detach();
}

#endif  // WWE12_DEV_PROBES
