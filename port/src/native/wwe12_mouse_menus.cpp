// The mouse in the game's menus, stage 1a (2.0, 2026-11-28, Rendu; docs/etude_souris_menus.md): left click = A,
// right click = B, wheel = D-pad up / down, everywhere but in a match being played. The game is never written:
// the mouse presses player 1's buttons through the XInputGetState hook (wwe12_intros.cpp), as Jarrod's 2K14 and
// Kaiko's SvR 2011 ports do (method only).
//
// Stage 1b, hover = select (iMrBurn: "quand le curseur passe sur quelque chose ça se met direct dessus"), in the
// main menu and its sub-menus (OPTIONS, PLAY...): their layout screen S (vtbl 82021BA0, sub_824D3458 every frame)
// is the menu object M (vtbl 82021C84) + 0x30, and M+360 is the active list's row (probe of 2026-11-28, followed
// step by step). The shots of that probe: the active list's rows are always at the same height, row i centred at
// y = 432 + 25.8 i of the 1280x720 picture, the active list right of x = 540 (the parent list, when a sub-menu is
// open, is left of it). A row the mouse moves onto becomes the target: the D-pad is pressed one step at a time until
// M+360 is there (never more: a press that doesn't bring it closer - the list's end, a greyed row - stops it); a left
// click on a row waits for the cursor to be there, then A.
//
// The cursor (iMrBurn, 2026-11-28): in the menus it hides after a few seconds without moving and comes back as soon
// as the mouse moves; in a match it stays hidden. Quiet when the SDK's mnk_mouse (the mouse as the right stick) is
// on, when an overlay took the click, and when the window is not in front.

#include <rex/cvar.h>
#include <rex/ui/window.h>
#include <rex/ui/window_listener.h>
#include <rex/ui/windowed_app_context.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <deque>
#include <mutex>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_mouse_menus, true, "WWE12",
                    "The mouse in the menus: left click = A, right click = B, wheel = up / down (never in a match).");
REXCVAR_DECLARE(bool, mnk_mouse);

bool Wwe12LoadingScreenUp();  // wwe12_load_turbo.cpp
bool Wwe12SceneMode();        // wwe12_fps.cpp
bool Wwe12MatchPlaying();     // wwe12_match_camera.cpp: the match camera updated in the last 300 ms
bool Wwe12BenchmarkActive();  // wwe12_benchmark.cpp

namespace {

long long NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

constexpr u16 kUp = 0x0001, kDown = 0x0002, kA = 0x1000, kB = 0x2000;
constexpr long long kHoldMs = 50, kGapMs = 50;  // a press held ~3 frames, then released ~3 (no menu repeat)

rex::ui::Window *g_window = nullptr;
std::mutex g_mu;
std::deque<u16> g_queue;     // presses to make, in order
u16 g_held = 0;              // the press being held
long long g_held_until = 0;  // ... until then, then a gap until g_free_at
long long g_free_at = 0;
int g_wheel = 0;  // scroll left over (a detent is 120)
std::atomic<int> g_cursor{-1};  // the cursor state asked last: 0 hidden (match), 1 auto-hidden (menus)

// Hover (stage 1b): the picture in the window (present.cpp), the mouse, the main menu's screen.
std::atomic<int> g_pic_x{0}, g_pic_y{0}, g_pic_w{0}, g_pic_h{0};
std::atomic<int> g_mouse_x{-1}, g_mouse_y{-1};
std::atomic<u32> g_menu_screen{0};
std::atomic<long long> g_menu_screen_ms{0};
constexpr u32 kMenuScreenVtbl = 0x82021BA0, kMenuVtbl = 0x82021C84;
constexpr float kRow0 = 432.0f, kRowStep = 25.8f, kListLeft = 540.0f, kListRight = 900.0f;
int g_target = -1;          // the row to reach (-1: none)
int g_last_hover = -1;      // the row under the mouse last time it moved
bool g_click_on_target = false;
int g_before_press = -1;    // the cursor before our last press (to see it moved)

// The main menu's object, when its screen ticked in the last 150 ms.
u32 MenuObject() {
  const u32 s = g_menu_screen.load();
  if (!s || NowMs() - g_menu_screen_ms.load() > 150)
    return 0;
  const u32 m = s - 0x30;
  return bd::mem::try_load<u32>(m) == kMenuVtbl ? m : 0;
}

// The row of the active list under the mouse (-1: none).
int HoveredRow() {
  const int w = g_pic_w.load(), h = g_pic_h.load(), mx = g_mouse_x.load(), my = g_mouse_y.load();
  if (w <= 0 || h <= 0 || mx < 0)
    return -1;
  const float gx = float(mx - g_pic_x.load()) * 1280.0f / float(w);
  const float gy = float(my - g_pic_y.load()) * 720.0f / float(h);
  if (gx < kListLeft || gx > kListRight)
    return -1;
  const float r = (gy - kRow0) / kRowStep + 0.5f;
  return r >= 0.0f && r < 12.0f ? int(r) : -1;
}

bool InMenus() {
  return REXCVAR_GET(wwe12_mouse_menus) && !REXCVAR_GET(mnk_mouse) && !Wwe12LoadingScreenUp() && !Wwe12SceneMode() &&
         !Wwe12MatchPlaying() && !Wwe12BenchmarkActive();
}

void Queue(u16 b) {
  if (!InMenus())
    return;
  std::lock_guard lock(g_mu);
  if (g_queue.size() < 8)
    g_queue.push_back(b);
}

class MouseListener final : public rex::ui::WindowInputListener {
 public:
  void OnMouseMove(rex::ui::MouseEvent &e) override {
    g_mouse_x = e.x();
    g_mouse_y = e.y();
  }
  void OnMouseDown(rex::ui::MouseEvent &e) override {
    if (e.is_handled())
      return;
    g_mouse_x = e.x();
    g_mouse_y = e.y();
    if (e.button() == rex::ui::MouseEvent::Button::kLeft) {
      // On a row of the main menu's list: A once the cursor is there (Hover, on the game thread).
      if (MenuObject() && HoveredRow() >= 0) {
        std::lock_guard lock(g_mu);
        g_click_on_target = true;
        g_last_hover = -2;  // (re-aim at the row clicked)
        return;
      }
      Queue(kA);
    }
    else if (e.button() == rex::ui::MouseEvent::Button::kRight)
      Queue(kB);
  }
  void OnMouseWheel(rex::ui::MouseEvent &e) override {
    if (e.is_handled() || !InMenus())
      return;
    int steps = 0;
    {
      std::lock_guard lock(g_mu);
      g_wheel += e.scroll_y();
      steps = g_wheel / int(rex::ui::MouseEvent::kScrollPerDetent);
      g_wheel -= steps * int(rex::ui::MouseEvent::kScrollPerDetent);
    }
    for (int i = 0; i < std::abs(steps) && i < 4; ++i)
      Queue(steps > 0 ? kUp : kDown);  // (positive: away from the user = up the list)
  }
};
MouseListener g_listener;

// The cursor for where we are: auto-hidden in the menus, hidden in a match (asked on the UI thread on a change).
void UpdateCursor(bool menus) {
  const int want = menus ? 1 : 0;
  if (!g_window || g_cursor.exchange(want) == want || REXCVAR_GET(mnk_mouse))
    return;
  rex::ui::Window *w = g_window;
  w->app_context().CallInUIThread([w, want] {
    w->SetCursorAutoHideDelayMs(3000);
    w->SetCursorVisibility(want ? rex::ui::Window::CursorVisibility::kAutoHidden
                                : rex::ui::Window::CursorVisibility::kHidden);
  });
}

}  // namespace

void Wwe12MouseMenusInit(rex::ui::Window *window) {
  g_window = window;
  if (window)
    window->AddInputListener(&g_listener, 1000);
}

// From the XInputGetState hook (wwe12_intros.cpp), player 1: the buttons the mouse holds now.
u16 Wwe12MouseMenuButtons() {
  const bool menus = InMenus();
  UpdateCursor(menus);
  std::lock_guard lock(g_mu);
  const long long now = NowMs();
  if (!menus) {
    g_queue.clear();
    g_held = 0;
    g_target = -1;
    g_click_on_target = false;
    return 0;
  }
  // Hover: a new row under the mouse becomes the target; one D-pad step at a time towards it.
  if (const u32 m = MenuObject(); m && g_queue.empty() && !g_held && now >= g_free_at) {
    const int row = HoveredRow();
    if (row != g_last_hover) {
      g_last_hover = row;
      g_target = row;
      g_before_press = -1;
    }
    const int cursor = int(bd::mem::load<u32>(m + 360));
    if (g_target >= 0 && cursor >= 0 && cursor < 64) {
      if (g_before_press >= 0 && std::abs(g_target - cursor) >= std::abs(g_target - g_before_press))
        g_target = -1;  // the last press brought it no closer (the list's end, a greyed row): stop
      else if (cursor != g_target) {
        g_before_press = cursor;
        g_queue.push_back(g_target > cursor ? kDown : kUp);
      } else {
        g_target = -1;
        g_before_press = -1;
      }
    }
    if (g_target < 0 && g_click_on_target) {
      g_click_on_target = false;
      if (row >= 0 && cursor == row)
        g_queue.push_back(kA);
    }
  } else if (!MenuObject()) {
    g_target = -1;
    g_last_hover = -1;
    g_click_on_target = false;
  }
  if (g_held && now < g_held_until)
    return g_held;
  g_held = 0;
  if (now < g_free_at || g_queue.empty())
    return 0;
  g_held = g_queue.front();
  g_queue.pop_front();
  g_held_until = now + kHoldMs;
  g_free_at = g_held_until + kGapMs;
  return g_held;
}

// wwe12_mouse_probe.cpp's relay of sub_824D3458 (every layout screen, every frame): the main menu's screen.
void Wwe12MouseMenusScreen(u32 screen) {
  if (bd::mem::try_load<u32>(screen) != kMenuScreenVtbl)
    return;
  g_menu_screen = screen;
  g_menu_screen_ms = NowMs();
}

// present.cpp: where the picture is in the window (physical pixels).
void Wwe12MouseMenusPicture(int x, int y, int w, int h) {
  g_pic_x = x, g_pic_y = y, g_pic_w = w, g_pic_h = h;
}
