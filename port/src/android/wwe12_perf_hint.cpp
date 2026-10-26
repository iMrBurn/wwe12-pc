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

namespace {

using GetManager = void *(*)();
using CreateSession = void *(*)(void *manager, const int32_t *tids, size_t count, int64_t target_ns);
using ReportActual = int (*)(void *session, int64_t actual_ns);

constexpr int64_t kTargetNs = 16'666'667;  // 60 fps

ReportActual g_report = nullptr;
void *g_session = nullptr;
bool g_tried = false;

void Start() {
  g_tried = true;
  void *lib = dlopen("libandroid.so", RTLD_NOW | RTLD_LOCAL);
  auto get_manager = lib ? reinterpret_cast<GetManager>(dlsym(lib, "APerformanceHint_getManager")) : nullptr;
  auto create = lib ? reinterpret_cast<CreateSession>(dlsym(lib, "APerformanceHint_createSession")) : nullptr;
  g_report = lib ? reinterpret_cast<ReportActual>(dlsym(lib, "APerformanceHint_reportActualWorkDuration")) : nullptr;
  void *manager = get_manager ? get_manager() : nullptr;
  const int32_t tid = int32_t(gettid());
  g_session = manager && create && g_report ? create(manager, &tid, 1, kTargetNs) : nullptr;
  BD_INFO("Android performance hint: {}", g_session ? "on (render thread, 60 fps target)"
                                                    : "not available on this phone");
}

}  // namespace

void Wwe12PerfHintReport(double work_ms) {
  if (!g_tried)
    Start();
  if (g_session && work_ms > 0)
    g_report(g_session, int64_t(work_ms * 1e6));
}
