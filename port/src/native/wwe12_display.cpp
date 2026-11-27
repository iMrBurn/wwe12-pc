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
#include <cfloat>
#include <cmath>
#include <initializer_list>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <imgui.h>

#include <string>

#include <rex/cvar.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>
#include <rex/ui/keybinds.h>

#include "core/logging.h"

#if REX_PLATFORM_ANDROID
int Wwe12PhoneTempTenths();  // android/wwe12_temp.cpp
#endif

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

// FPS counter fonts (1.5): baked once at start-up for a 1080p screen (kNumPx / kTextPx pixels), before ImGui
// uploads its font atlas (wwe12_app.h calls Wwe12DisplayInit before the first overlay frame). Null = ImGui's
// own font.
constexpr float kNumPx = 27.0f, kTextPx = 17.0f;
ImFont *g_font_num = nullptr, *g_font_text = nullptr;

bool FileExists(const char *path) {
  if (FILE *f = fopen(path, "rb")) {
    fclose(f);
    return true;
  }
  return false;
}

void LoadOverlayFonts(rex::ui::ImGuiDrawer *drawer) {
  ImGuiIO &io = drawer->GetIO();
  // The SDK builds the atlas pixels at start-up but uploads them only at the first overlay frame
  // (ImGuiDrawer::SetupFontTexture, detached mode): fonts can still be added until that upload, then the
  // pixels are built again (Build, below) so the upload takes the new glyphs.
  if (io.Fonts->TexRef._TexID != ImTextureID_Invalid) {
    BD_WARN("[display] font atlas already uploaded: FPS counter keeps ImGui's font");
    return;
  }
  static const ImWchar ranges[] = {0x0020, 0x00FF, 0};  // Latin (the temperature's degree sign)
  auto load = [&](std::initializer_list<const char *> paths, float px) -> ImFont * {
    for (const char *p : paths) {
      if (!FileExists(p))
        continue;
      ImFontConfig cfg;
      cfg.OversampleH = 2;
      cfg.OversampleV = 1;
      cfg.PixelSnapH = true;
      if (ImFont *f = io.Fonts->AddFontFromFileTTF(p, px, &cfg, ranges)) {
        BD_INFO("[display] FPS counter font {} ({} px)", p, px);
        return f;
      }
    }
    return nullptr;
  };
#if REX_PLATFORM_ANDROID
  g_font_num = load({"/system/fonts/Roboto-Medium.ttf", "/system/fonts/Roboto-Regular.ttf"}, kNumPx);
  g_font_text = load({"/system/fonts/Roboto-Regular.ttf"}, kTextPx);
#else
  g_font_num = load({"C:\\Windows\\Fonts\\seguisb.ttf", "C:\\Windows\\Fonts\\segoeui.ttf"}, kNumPx);
  g_font_text = load({"C:\\Windows\\Fonts\\segoeui.ttf"}, kTextPx);
#endif
  if ((g_font_num || g_font_text) && !io.Fonts->Build()) {
    BD_WARN("[display] font atlas rebuild failed: FPS counter keeps ImGui's font");
    g_font_num = g_font_text = nullptr;
  }
}

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
    // 1.5 look (2026-11-20): a small rounded pill centred at the top: a coloured dot with a soft halo (green
    // >= 55, yellow >= 30, red below), the FPS in white semibold, "FPS" small and grey, a thin separator, the
    // frame time (and the phone's temperature on Android). Real fonts (Segoe UI / Roboto, loaded once at
    // their size by Wwe12DisplayInit; ImGui's own font if missing), fixed widths so the pill never changes
    // size, the shown FPS only moves when it is off by more than half a frame (no 59 / 60 flicker), and a
    // 150 ms fade-in when F2 shows it.
    if (fps_ > 0.5 && (shown_fps_ < 0 || std::abs(fps_ - shown_fps_) > 0.6))
      shown_fps_ = std::round(fps_);
    const bool have = fps_ > 0.5 && shown_fps_ >= 0;
    const double fps = have ? shown_fps_ : 0.0;
    if (shown_at_ < 0)
      shown_at_ = now;
    const float fade = float(std::min(1.0, (now - shown_at_) / 0.15));
    auto col = [&](int r, int g, int b, int a) { return IM_COL32(r, g, b, int(a * fade)); };

    const float k = std::max(0.6f, io.DisplaySize.y / 1080.0f);
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    ImFont *num_font = g_font_num ? g_font_num : ImGui::GetFont();
    ImFont *txt_font = g_font_text ? g_font_text : ImGui::GetFont();
    // the fonts are baked for g_font_k; at that scale they are drawn at their own pixel size (crisp)
    const float fps_sz = kNumPx * k, text_sz = kTextPx * k;
    const float pad_x = 14.0f * k, pad_y = 4.0f * k, gap = 9.0f * k, dot_r = 4.0f * k;
    auto width = [&](ImFont *f, float size, const char *t) { return f->CalcTextSizeA(size, FLT_MAX, 0.0f, t).x; };

    char num[16], ms[24];
    if (have) {
      snprintf(num, sizeof(num), "%.0f", fps);
      snprintf(ms, sizeof(ms), "%.1f ms", 1000.0 / fps_);
    } else {
      snprintf(num, sizeof(num), "--");
      snprintf(ms, sizeof(ms), "-- ms");
    }
    char extra[24] = "";
#if REX_PLATFORM_ANDROID
    // Phone (battery) temperature, when the launcher setting is on (wwe12_temp.cpp).
    if (const int t = Wwe12PhoneTempTenths(); t > -1000)
      snprintf(extra, sizeof(extra), "%d.%d \xC2\xB0" "C", t / 10, std::abs(t % 10));
#endif
    const float num_w = width(num_font, fps_sz, "000"), fps_w = width(txt_font, text_sz, "FPS");
    const float ms_w = width(txt_font, text_sz, "000.0 ms");
    const float extra_w = extra[0] ? width(txt_font, text_sz, "00.0 \xC2\xB0" "C") : 0.0f;
    const float h = std::floor(fps_sz + 2.0f * pad_y);
    float w = pad_x + 2.0f * dot_r + gap + num_w + 5.0f * k + fps_w + gap + 1.0f + gap + ms_w + pad_x;
    if (extra[0])
      w += gap + 1.0f + gap + extra_w;
    w = std::floor(w);
    const float x0 = std::floor((io.DisplaySize.x - w) * 0.5f), y0 = std::floor(12.0f * k);

    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + w, y0 + h), col(12, 12, 14, 158), h * 0.5f);
    dl->AddRect(ImVec2(x0 + 0.5f, y0 + 0.5f), ImVec2(x0 + w - 0.5f, y0 + h - 0.5f), col(255, 255, 255, 34),
                h * 0.5f, 0, 1.0f);

    int r = 150, g = 150, b = 150;
    if (have && fps >= 55.0) { r = 80; g = 220; b = 110; }
    else if (have && fps >= 30.0) { r = 245; g = 200; b = 70; }
    else if (have) { r = 240; g = 85; b = 75; }
    float x = x0 + pad_x;
    const float mid = y0 + h * 0.5f;
    dl->AddCircleFilled(ImVec2(x + dot_r, mid), dot_r * 2.1f, col(r, g, b, 40), 24);  // halo
    dl->AddCircleFilled(ImVec2(x + dot_r, mid), dot_r * 1.45f, col(r, g, b, 60), 24);
    dl->AddCircleFilled(ImVec2(x + dot_r, mid), dot_r, col(r, g, b, 255), 24);
    x += 2.0f * dot_r + gap;
    // the FPS right-aligned in its 3-digit box; the small texts bottom-aligned with it (same baseline)
    const float num_y = std::floor(mid - fps_sz * 0.5f);
    const float text_y = std::floor(num_y + (fps_sz - text_sz) * 0.80f);
    dl->AddText(num_font, fps_sz, ImVec2(std::floor(x + num_w - width(num_font, fps_sz, num)), num_y),
                col(255, 255, 255, 255), num);
    x += num_w + 5.0f * k;
    dl->AddText(txt_font, text_sz, ImVec2(x, text_y), col(190, 192, 198, 235), "FPS");
    x += fps_w + gap;
    auto separator = [&]() {
      const float sx = std::floor(x) + 0.5f;
      dl->AddLine(ImVec2(sx, y0 + h * 0.28f), ImVec2(sx, y0 + h * 0.72f), col(255, 255, 255, 50), 1.0f);
      x += 1.0f + gap;
    };
    separator();
    dl->AddText(txt_font, text_sz, ImVec2(std::floor(x + ms_w - width(txt_font, text_sz, ms)), text_y),
                col(215, 217, 222, 235), ms);
    x += ms_w;
    if (extra[0]) {
      x += gap;
      separator();
      dl->AddText(txt_font, text_sz, ImVec2(std::floor(x + extra_w - width(txt_font, text_sz, extra)), text_y),
                  col(215, 217, 222, 235), extra);
    }
  }

 private:
  double last_time_ = 0.0;
  uint64_t last_frames_ = 0;
  double fps_ = 0.0;
  double shown_fps_ = -1.0;  // the integer shown (changes only past half a frame of difference)
  double shown_at_ = -1.0;   // ImGui time of the first draw (fade-in)
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

// A short text at the top of the screen for 2 s (MATCH CAMERA's F5, wwe12_match_camera.cpp). UI thread.
namespace {
std::string g_toast_text;
double g_toast_until = 0;
class ToastOverlay;
ToastOverlay *g_toast = nullptr;
class ToastOverlay final : public rex::ui::ImGuiDialog {
 public:
  explicit ToastOverlay(rex::ui::ImGuiDrawer *drawer) : ImGuiDialog(drawer) {}

 protected:
  void OnClose() override { g_toast = nullptr; }
  void OnDraw(ImGuiIO &io) override {
    const double now = ImGui::GetTime();
    if (g_toast_until == 0)
      g_toast_until = now + 2.0;
    if (now > g_toast_until) {
      Close();
      return;
    }
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    const ImVec2 size = ImGui::CalcTextSize(g_toast_text.c_str());
    const float x = (io.DisplaySize.x - size.x) * 0.5f, y = io.DisplaySize.y * 0.12f;
    dl->AddRectFilled(ImVec2(x - 16, y - 8), ImVec2(x + size.x + 16, y + size.y + 8), IM_COL32(12, 12, 14, 170),
                      (size.y + 16) * 0.5f);
    dl->AddText(ImVec2(x, y), IM_COL32(255, 255, 255, 255), g_toast_text.c_str());
  }
};
}  // namespace

void Wwe12DisplayToast(const char *text) {
  g_toast_text = text;
  g_toast_until = 0;  // (set at the next draw: 2 s from then)
  if (!g_toast && g_drawer)
    g_toast = new ToastOverlay(g_drawer);  // deletes itself when closed
}

void Wwe12MatchCameraNext();  // wwe12_match_camera.cpp
void Wwe12FreeCameraToggle();
void Wwe12FreeCameraHud();
void Wwe12FreeCameraPause();

void Wwe12DisplayInit(rex::ui::ImGuiDrawer *drawer) {
  g_drawer = drawer;
  if (drawer)
    LoadOverlayFonts(drawer);
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
  rex::ui::RegisterBind("bind_match_camera", "F5", "Next match camera", [] { Wwe12MatchCameraNext(); });
  // The free camera (wwe12_match_camera.cpp): F6 on / off, F7 the HUD, F8 the pause. The SDK's achievements
  // overlay had F7: it moves to F10 (unless the player chose another key).
  if (rex::cvar::GetFlagByName("bind_achievements") == "F7")
    rex::cvar::SetFlagByName("bind_achievements", "F10");
  BD_INFO("[display] achievements overlay key: {}", rex::cvar::GetFlagByName("bind_achievements"));
  rex::ui::RegisterBind("bind_free_camera", "F6", "Free camera on / off (in a match)", [] { Wwe12FreeCameraToggle(); });
  rex::ui::RegisterBind("bind_free_camera_hud", "F7", "Free camera: hide / show the HUD", [] { Wwe12FreeCameraHud(); });
  rex::ui::RegisterBind("bind_free_camera_pause", "F8", "Free camera: pause / play", [] { Wwe12FreeCameraPause(); });
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
