// Android's Performance Hint API (ADPF, Android 12+): the render thread tells the system its
// frame target (60 fps) and how long each frame's work really took. Without it the scheduler
// judges the game by average load and often leaves the render thread on a little core or at a
// low clock, then catches up too late (stutters). Google's advice for games.
// libandroid.so has the functions from API 33 on (the app supports API 28): looked up at run time.
// Called from present.cpp (FrameStatsPresented), on the render thread.

#include <dlfcn.h>
#include <unistd.h>

#include <cstdint>

#include "core/logging.h"

bool Wwe12FrameMode30();  // native/wwe12_fps.cpp: wwe12_frame_mode = "30" (1.2)

namespace {

using GetManager = void *(*)();
using CreateSession = void *(*)(void *manager, const int32_t *tids, size_t count, int64_t target_ns);
using ReportActual = int (*)(void *session, int64_t actual_ns);

using UpdateTarget = int (*)(void *session, int64_t target_ns);

constexpr int64_t kTargetNs = 16'666'667;    // 60 fps
constexpr int64_t kTarget30Ns = 33'333'333;  // 30 fps mode (wwe12_frame_mode = "30"): a frame may take twice as long

ReportActual g_report = nullptr;
UpdateTarget g_update = nullptr;
int64_t g_target = kTargetNs;
uint32_t g_calls = 0;
void *g_session = nullptr;
bool g_tried = false;

void Start() {
  g_tried = true;
  void *lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
  auto get_manager = lib ? reinterpret_cast<GetManager>(dlsym(lib, "APerformanceHint_getManager")) : nullptr;
  auto create = lib ? reinterpret_cast<CreateSession>(dlsym(lib, "APerformanceHint_createSession")) : nullptr;
  g_report = lib ? reinterpret_cast<ReportActual>(dlsym(lib, "APerformanceHint_reportActualWorkDuration")) : nullptr;
  g_update = lib ? reinterpret_cast<UpdateTarget>(dlsym(lib, "APerformanceHint_updateTargetWorkDuration")) : nullptr;
  void *manager = get_manager ? get_manager() : nullptr;
  const int32_t tid = int32_t(gettid());
  g_target = Wwe12FrameMode30() ? kTarget30Ns : kTargetNs;
  g_session = manager && create && g_report ? create(manager, &tid, 1, g_target) : nullptr;
  BD_INFO("Android performance hint: {}", g_session ? "on (render thread, 60 fps target)"
                                                    : "not available on this phone");
}

}  // namespace

void Wwe12PerfHintReport(double work_ms) {
  if (!g_tried)
    Start();
  // The 30 fps mode can be switched in the game's menu: look again about once a second.
  if (g_session && g_update && ++g_calls % 60 == 0) {
    const int64_t want = Wwe12FrameMode30() ? kTarget30Ns : kTargetNs;
    if (want != g_target && g_update(g_session, want) == 0)
      g_target = want;
  }
  if (g_session && work_ms > 0)
    g_report(g_session, int64_t(work_ms * 1e6));
}
