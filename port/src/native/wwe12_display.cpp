// WWE '12 PC: display options that live in the game process.
//
// - FPS counter: F2 shows or hides it (wwe12_show_fps starts it shown). It counts the frames the game
//   presents (not the monitor's refresh), drawn with ImGui over the picture.
// - Frame limit (wwe12_fps_limit): the game is paced at 60 frames per second by its own clock, so a
//   limit above 60 changes nothing; below 60 each presented frame waits, which also slows the game
//   down in proportion (its logic advances once per frame). 0 = no limit.
// - CPU priority (wwe12_cpu_priority): 0 normal, 1 above normal, 2 high.

#include "wwe12_display.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <imgui.h>

#include <rex/cvar.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/keybinds.h>

#include "core/logging.h"

REXCVAR_DEFINE_BOOL(wwe12_show_fps, false, "WWE12", "Show the FPS counter at start (F2 shows or hides it).");
REXCVAR_DEFINE_INT32(wwe12_fps_limit, 0, "WWE12",
                     "Frame limit: 0 = none. The game is paced at 60 by its own clock, so values above 60 "
                     "change nothing and values below 60 slow the game down.")
    .range(0, 240);
REXCVAR_DEFINE_INT32(wwe12_cpu_priority, 0, "WWE12",
                     "CPU priority of the game: 0 normal, 1 above normal, 2 high.")
    .range(0, 2);

namespace {

std::atomic<uint64_t> g_frames{0};
std::atomic<bool> g_show{false};
rex::ui::ImGuiDrawer *g_drawer = nullptr;

class FpsOverlay;
FpsOverlay *g_overlay = nullptr;  // UI thread only

class FpsOverlay final : public rex::ui::ImGuiDialog {
 public:
  explicit FpsOverlay(rex::ui::ImGuiDrawer *drawer) : ImGuiDialog(drawer) {}

 protected:
  void OnClose() override { g_overlay = nullptr; }

  void OnDraw(ImGuiIO &io) override {
    if (!g_show.load(std::memory_order_relaxed)) {
      Close();  // deleted at the end of this Draw; OnClose forgets it
      return;
    }
    // Frames per second over the last half second.
    const double now = ImGui::GetTime();
    if (last_time_ == 0.0) {
      last_time_ = now;
      last_frames_ = g_frames.load(std::memory_order_relaxed);
    } else if (now - last_time_ >= 0.5) {
      const uint64_t frames = g_frames.load(std::memory_order_relaxed);
      fps_ = double(frames - last_frames_) / (now - last_time_);
      last_time_ = now;
      last_frames_ = frames;
    }
    const float k = std::max(0.6f, io.DisplaySize.y / 1080.0f);
    char text[64];
    if (fps_ > 0.5)
      snprintf(text, sizeof(text), "%.0f FPS  %.1f ms", fps_, 1000.0 / fps_);
    else
      snprintf(text, sizeof(text), "-- FPS");
    const ImU32 color = fps_ >= 55.0 ? IM_COL32(120, 255, 120, 255)
                        : fps_ >= 30.0 ? IM_COL32(255, 220, 90, 255)
                                       : IM_COL32(255, 110, 100, 255);
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    ImFont *font = ImGui::GetFont();
    const float size = 26.0f * k;
    const ImVec2 pos(16.0f * k, 12.0f * k);
    dl->AddText(font, size, ImVec2(pos.x + 2.0f * k, pos.y + 2.0f * k), IM_COL32(0, 0, 0, 220), text);
    dl->AddText(font, size, pos, color, text);
  }

 private:
  double last_time_ = 0.0;
  uint64_t last_frames_ = 0;
  double fps_ = 0.0;
};

void ShowOverlayIfNeeded() {
  if (g_show.load(std::memory_order_relaxed) && !g_overlay && g_drawer)
    g_overlay = new FpsOverlay(g_drawer);  // deletes itself when closed
}

// Waits until `next`: most of it blocked on a high-resolution timer, the last bit spinning.
void WaitUntil(std::chrono::steady_clock::time_point next) {
  using clock = std::chrono::steady_clock;
  static HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                               TIMER_ALL_ACCESS);
  const auto remaining = next - clock::now();
  if (remaining > std::chrono::microseconds(1500)) {
    const auto sleep_for = remaining - std::chrono::microseconds(1000);
    if (timer) {
      LARGE_INTEGER due;
      due.QuadPart = -(std::chrono::duration_cast<std::chrono::nanoseconds>(sleep_for).count() / 100);
      if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
        WaitForSingleObject(timer, INFINITE);
    } else {
      std::this_thread::sleep_for(sleep_for);
    }
  }
  while (clock::now() < next)
    YieldProcessor();
}

}  // namespace

void Wwe12DisplayInit(rex::ui::ImGuiDrawer *drawer) {
  g_drawer = drawer;
  switch (REXCVAR_GET(wwe12_cpu_priority)) {
  case 1:
    SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
    BD_INFO("[display] CPU priority: above normal");
    break;
  case 2:
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
    BD_INFO("[display] CPU priority: high");
    break;
  default:
    break;
  }
  g_show = REXCVAR_GET(wwe12_show_fps);
  rex::ui::RegisterBind("bind_fps_counter", "F2", "Show or hide the FPS counter", [] {
    g_show = !g_show.load();
    ShowOverlayIfNeeded();
  });
  ShowOverlayIfNeeded();
}

void Wwe12DisplayOnFrame() {
  using clock = std::chrono::steady_clock;
  g_frames.fetch_add(1, std::memory_order_relaxed);

  const int limit = REXCVAR_GET(wwe12_fps_limit);
  static clock::time_point next{};
  if (limit <= 0) {
    next = {};
    return;
  }
  const auto now = clock::now();
  if (next == clock::time_point{} || now > next + std::chrono::milliseconds(100))
    next = now;  // first limited frame, or after a stall (loading screen): do not rush to catch up
  next += std::chrono::nanoseconds(1'000'000'000LL / limit);
  WaitUntil(next);
}
