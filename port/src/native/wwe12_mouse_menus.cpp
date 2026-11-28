// The mouse in the game's menus, stage 1a (2.0, 2026-11-28, Rendu; docs/etude_souris_menus.md): left click = A,
// right click = B, wheel = D-pad up / down, everywhere but in a match being played. The game is never written:
// the mouse presses player 1's buttons through the XInputGetState hook (wwe12_intros.cpp), as Jarrod's 2K14 and
// Kaiko's SvR 2011 ports do (method only). Hover = select comes next, menu family by menu family.
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
  void OnMouseDown(rex::ui::MouseEvent &e) override {
    if (e.is_handled())
      return;
    if (e.button() == rex::ui::MouseEvent::Button::kLeft)
      Queue(kA);
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
    return 0;
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
