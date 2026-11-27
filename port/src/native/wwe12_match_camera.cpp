// MATCH CAMERA (2026-11-27, Rendu; docs/etude_camera_libre.md): the player picks the match's camera: NORMAL (the
// game's) or RAMP (toward the stage, as in a Royal Rumble); the free camera comes next. Cvar wwe12_match_camera
// (saved in wwe12.toml), F5 in a match for the next one (a short text shows which). The benchmark keeps NORMAL.
//
// Found with the camera probe (wwe12_camera_probe, v5): a camera object, updated each frame by sub_82824370(camera)
// (already hooked by wwe12_widescreen.cpp, which knows its field of view +512, near +500 and far +504):
//   +32  its local matrix, +96 its world matrix (camera -> world: rows right, up, forward, then the eye),
//   +176 the projection, +240 the view (world -> camera), +368 view x projection;
//   sub_82824370 rebuilds +96 = inverse(+240) (sub_821DB980), then +368 = +240 x +176 (sub_821DB8C0).
// So the camera's own code writes the VIEW at +240 before the update, like SvR 2011's view builder in Kaiko's port
// (sub_82227150(view, eye, direction); his MATCH CAMERA moves the eye just before it; method only, with his
// permission, THIRD_PARTY.md). Here the view at +240 is replaced before sub_82824370 and kept (it is read again for
// the draws), only for the cameras updated from 8247F64C during a match (after sub_822B51A0, outside scenes and
// loading screens). Checked in game by iMrBurn on 2026-11-27 with a test view (moved back): the match camera moved.

#include <rex/cvar.h>

#include <algorithm>
#include <chrono>
#include <atomic>
#include <cmath>
#include <cstring>

#include <rex/filesystem.h>

#include "../launcher/toml_file.h"
#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

#if defined(_WIN32) && !defined(__ANDROID__)
#include <windows.h>
#define WWE12_FREECAM_KEYBOARD 1
#endif

REXCVAR_DEFINE_INT32(wwe12_match_camera, 0, "WWE12",
                     "Match camera: 0 NORMAL (the game's), 1 RAMP (toward the stage, as in a Royal Rumble); F5 in a "
                     "match: the next one.")
    .range(0, 1);
// RAMP's shape (2026-11-27: iMrBurn wants it lower and a little farther, "comme sur 2K26"; tried from the command
// line, the best one becomes the default). Defaults = try C2 (pitch 4, 8 m, 48 degrees, aim 2.2 m, follow 0.4),
// "pas trop mal" for iMrBurn (2026-11-27): near the top rope, almost level, wide, on the ring's axis like 2K26.
REXCVAR_DEFINE_DOUBLE(wwe12_ramp_pitch, 4.0, "WWE12", "RAMP camera: degrees it looks down (0 = level).")
    .range(-10.0, 60.0);
// v2 (iMrBurn's screenshot, 2026-11-27: "au début elle est bien, puis après elle zoome trop pendant le match"):
// the first version took the game's TV camera's distance and zoom, and that camera zooms in more and more during a
// match. RAMP now has its own fixed distance and field of view, aims at chest height, and follows what the game's
// camera looks at smoothly (no jump at the TV camera's cuts).
REXCVAR_DEFINE_DOUBLE(wwe12_ramp_distance, 8.0, "WWE12",
                      "RAMP camera: metres back from what it looks at.")
    .range(3.0, 60.0);
REXCVAR_DEFINE_DOUBLE(wwe12_ramp_fov, 48.0, "WWE12", "RAMP camera: vertical field of view, degrees.")
    .range(5.0, 90.0);
REXCVAR_DEFINE_DOUBLE(wwe12_ramp_aim_height, 2.2, "WWE12",
                      "RAMP camera: height it aims at above the arena floor, metres (the mat is ~1.2 m up: 2.3 = the "
                      "wrestlers' chests in the ring).")
    .range(0.0, 5.0);
REXCVAR_DEFINE_DOUBLE(wwe12_ramp_follow, 0.4, "WWE12",
                      "RAMP camera: how much it follows the action sideways and toward the stage (0 = stays on the "
                      "ring's centre line, as WWE 2K26's ramp view, 1 = follows fully).")
    .range(0.0, 1.0);
REXCVAR_DEFINE_INT32(wwe12_camera_test, 0, "WWE12",
                     "Dev (old, ignored since 2026-11-27: the FAR / CLOSE / HIGH / LOW tests are gone).")
    .range(0, 4);

bool Wwe12MatchSeen();        // wwe12_fps.cpp: a match started since launch
bool Wwe12SceneMode();        // wwe12_fps.cpp: an entrance / victory / story scene
bool Wwe12LoadingScreenUp();  // wwe12_load_turbo.cpp
bool Wwe12BenchmarkActive();  // wwe12_benchmark.cpp
void Wwe12DisplayToast(const char *text);  // wwe12_display.cpp: a short text at the top of the screen
void Wwe12PressStart();                    // wwe12_intros.cpp: Start pressed once for player 1 (the game's pause)

namespace {

// The match's cameras: the 1st v6 run saw the menu camera come from 8254296C and two cameras of the entrances and
// the match from 8247F64C (the benchmark then never reached its match: moving every camera upset its route).
constexpr u32 kMatchCameraCaller = 0x8247F64C;

struct V3 {
  float x, y, z;
};

float Ld(u8 *base, u32 va) {
  u32 v;
  std::memcpy(&v, base + va, 4);
  v = __builtin_bswap32(v);
  float f;
  std::memcpy(&f, &v, 4);
  return f;
}
void St(u8 *base, u32 va, float f) {
  u32 v;
  std::memcpy(&v, &f, 4);
  v = __builtin_bswap32(v);
  std::memcpy(base + va, &v, 4);
}

// v6c: the game's view is NOT put back after sub_82824370 any more. In the v6b test (iMrBurn, FAR) nothing moved:
// +240 is read again later for the draws, so putting it back undid the change. Each camera keeps the game's last
// view and ours: when +240 still holds ours at the next update (the game did not write a new one), the move starts
// again from the game's own view, so it never adds up.
struct Moved {
  u32 cam = 0;
  u8 game[64];  // the game's view at the last update
  u8 ours[64];  // what we wrote
  long long next_log_ms = 0;
};
Moved g_moved[8];

Moved *Slot(u32 cam) {
  for (Moved &m : g_moved)
    if (m.cam == cam)
      return &m;
  for (Moved &m : g_moved)
    if (!m.cam) {
      m.cam = cam;
      return &m;
    }
  return nullptr;
}

long long NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// Every caller of sub_82824370 seen during the match, once (which cameras the match updates).
struct Caller {
  u32 lr, cam;
};
Caller g_callers[48];
int g_callers_n = 0;

}  // namespace

// ---- FREE CAMERA (palier 1, 2026-11-27; docs/etude_camera_libre.md; keys as in Kaiko's SvR 2011 port, method only) ----
// F6 in a match: the view is ours, flown with the keyboard / mouse / pad; F5 or F6: back to the match camera. F7: the
// HUD hidden (the 2D sprites, hooks/draw.cpp). F8: the game's pause (its Start), and the HUD hidden while paused.
// While it is on, the pads do not reach the game (the players' wrestlers stand; the computer's still move - F8 stops
// them). Never in the benchmark, never outside a match: the loading screen after the match turns it off (so the
// menus always get their pads and their HUD back).
std::atomic<bool> g_free{false};
std::atomic<bool> g_free_init{false};   // take the game's eye at the next update
std::atomic<bool> g_free_hud_off{false};
std::atomic<bool> g_free_paused{false};
std::atomic<int> g_free_updates{0};      // camera updates seen while paused (does the game update it in its pause?)
std::atomic<bool> g_wwe12_hide_hud{false};  // hooks/draw.cpp: HUD sprites not drawn
// Player 1's pad, as the game would have read it (XInputGetState hook): sticks, triggers, buttons.
std::atomic<u64> g_free_sticks{0};
std::atomic<u32> g_free_buttons{0};

namespace {
bool InMatchNow() {
  return Wwe12MatchSeen() && !Wwe12SceneMode() && !Wwe12LoadingScreenUp() && !Wwe12BenchmarkActive();
}
bool FreeActive() { return g_free.load() && InMatchNow(); }
void UpdateHideHud() { g_wwe12_hide_hud.store(g_free.load() && (g_free_hud_off.load() || g_free_paused.load())); }
void FreeOff() {
  if (!g_free.exchange(false))
    return;
  if (g_free_paused.exchange(false))
    Wwe12PressStart();  // (the game's pause closed again)
  g_free_hud_off = false;
  UpdateHideHud();
}
}  // namespace

// From the XInputGetState hook (wwe12_intros.cpp), each pad read that succeeded: true = its buttons, triggers and
// sticks were zeroed (the free camera has them).
bool Wwe12FreeCamPad(u32 user, u32 state) {
  if (!g_free.load())
    return false;
  if (Wwe12LoadingScreenUp()) {  // the match is over: everything back
    FreeOff();
    return false;
  }
  if (!InMatchNow())
    return false;
  if (user == 0) {
    u64 sticks = 0;
    for (u32 i = 0; i < 4; ++i)
      sticks |= u64(bd::mem::load<u16>(state + 8 + 2 * i)) << (16 * i);
    g_free_sticks.store(sticks);
    g_free_buttons.store(u32(bd::mem::load<u16>(state + 4)) | (u32(bd::mem::load<u8>(state + 6)) << 16) |
                         (u32(bd::mem::load<u8>(state + 7)) << 24));
  }
  for (u32 i = 4; i < 16; i += 4)
    bd::mem::store<u32>(state + i, 0);
  return true;
}

// When RAMP last replaced a view (ms): the depth of field is off while it is on (draw.cpp).
std::atomic<long long> g_ramp_ms{0};

u32 g_fov_cam = 0;   // the camera whose field of view (+512) is changed for this update (put back after it)
float g_fov_saved = 0;

namespace {
// Our view at camera + 240: the eye n looking along g (unit), level (no roll), and the field of view (degrees) for
// this update (the projection is built from +512 inside sub_82824370; put back after it).
bool WriteView(u8 *base, u32 cam, Moved *slot, V3 n, V3 g, float fov_deg) {
  // The camera's axes for that direction, built as the game's are: up = +y on screen rows (world y down), right =
  // up x forward (the game's: forward (1,0,0) gives right (0,0,-1), up (0,1,0)).
  V3 r{1.0f * g.z - 0.0f * g.y, 0.0f * g.x - 0.0f * g.z, 0.0f * g.y - 1.0f * g.x};  // (0,1,0) x g
  const float rl = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z);
  if (rl < 1e-4f)
    return false;
  r = {r.x / rl, r.y / rl, r.z / rl};
  const V3 u{g.y * r.z - g.z * r.y, g.z * r.x - g.x * r.z, g.x * r.y - g.y * r.x};  // g x r
  const float A[3][3] = {{r.x, r.y, r.z}, {u.x, u.y, u.z}, {g.x, g.y, g.z}};
  for (int j = 0; j < 3; ++j) {
    for (int k = 0; k < 3; ++k)
      St(base, cam + 240 + u32(16 * k + 4 * j), A[j][k]);  // V[k][j] = A[j][k]
    St(base, cam + 240 + 48 + u32(4 * j), -(n.x * A[j][0] + n.y * A[j][1] + n.z * A[j][2]));
  }
  std::memcpy(slot->ours, base + cam + 240, 64);
  g_ramp_ms.store(NowMs(), std::memory_order_relaxed);
  const float fov = Ld(base, cam + 512);
  if (fov > 0.01f && fov < 3.0f) {
    g_fov_cam = cam;
    g_fov_saved = fov;
    St(base, cam + 512, fov_deg * 3.14159265f / 180.0f);
  }
  return true;
}

#if defined(WWE12_FREECAM_KEYBOARD)
bool OurWindowInFront() {
  DWORD pid = 0;
  const HWND w = GetForegroundWindow();
  return w && GetWindowThreadProcessId(w, &pid) && pid == GetCurrentProcessId();
}
// A key by its place on the keyboard (scan code): W A S D on QWERTY = Z Q S D on AZERTY, Q / E = A / E.
bool KeyAt(UINT scan) { return (GetAsyncKeyState(int(MapVirtualKeyW(scan, MAPVK_VSC_TO_VK))) & 0x8000) != 0; }
bool Key(int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; }
#endif

float Stick(u64 sticks, int i) {
  const float v = float(int16_t(u16(sticks >> (16 * i)))) / 32767.0f;
  constexpr float kDead = 0.24f;
  return std::fabs(v) < kDead ? 0.0f : (v - std::copysign(kDead, v)) / (1.0f - kDead);
}

// The free camera's pose: eye (game units, cm; y down), yaw / pitch (radians; a positive pitch looks down), field of
// view (degrees).
struct FreePose {
  V3 eye{};
  float yaw = 0, pitch = 0, fov = 45;
  long long ms = 0;
#if defined(WWE12_FREECAM_KEYBOARD)
  POINT cursor{};
  bool rmb = false;
#endif
};
FreePose g_pose;

void FreeView(u8 *base, u32 cam, Moved *slot, V3 e, V3 f) {
  FreePose &p = g_pose;
  const long long now = NowMs();
  if (g_free_init.exchange(false)) {  // from the game's view of the moment
    p.eye = e;
    p.yaw = std::atan2(f.x, f.z);
    p.pitch = std::asin(std::clamp(f.y, -1.0f, 1.0f));
    const float fov = Ld(base, cam + 512);
    p.fov = (fov > 0.01f && fov < 3.0f) ? fov * 180.0f / 3.14159265f : 45.0f;
    p.ms = now;
    BD_INFO("[camera] FREE: from the game's eye ({:.0f}, {:.0f}, {:.0f}), yaw {:.2f} pitch {:.2f}, field of view "
            "{:.1f}", e.x, e.y, e.z, p.yaw, p.pitch, p.fov);
  }
  if (g_free_paused.load())
    g_free_updates.fetch_add(1);
  const float dt = std::clamp(float(now - p.ms) / 1000.0f, 0.0f, 0.1f);
  p.ms = now;
  float fwd = 0, side = 0, vert = 0, turn = 0, look = 0, zoom = 0, speed = 1;
  float mouse_x = 0, mouse_y = 0;  // pixels
  // The pad (player 1): left stick moves, right stick looks, RT / LT up / down, RB / LB zoom in / out, A faster.
  const u64 sticks = g_free_sticks.load();
  const u32 pad = g_free_buttons.load();
  side += Stick(sticks, 0);
  fwd += Stick(sticks, 1);
  turn += Stick(sticks, 2);
  look -= Stick(sticks, 3);
  vert += float(pad >> 24) / 255.0f - float((pad >> 16) & 0xFF) / 255.0f;
  if (pad & 0x0200)
    zoom -= 1;  // RB: narrower
  if (pad & 0x0100)
    zoom += 1;  // LB
  if (pad & 0x1000)
    speed = 3;
#if defined(WWE12_FREECAM_KEYBOARD)
  // The keyboard and mouse: WASD / ZQSD moves, E / Space up, Q (A on AZERTY) down, arrows or the right mouse button
  // held look, Page Up / Page Down (or + / -) zoom, Shift faster, Ctrl slower.
  if (OurWindowInFront()) {
    fwd += float(KeyAt(0x11)) - float(KeyAt(0x1F));
    side += float(KeyAt(0x20)) - float(KeyAt(0x1E));
    vert += float(KeyAt(0x12) || Key(VK_SPACE)) - float(KeyAt(0x10));
    turn += float(Key(VK_RIGHT)) - float(Key(VK_LEFT));
    look += float(Key(VK_DOWN)) - float(Key(VK_UP));
    zoom += float(Key(VK_NEXT) || Key(VK_SUBTRACT) || Key(VK_OEM_MINUS)) -
            float(Key(VK_PRIOR) || Key(VK_ADD) || Key(VK_OEM_PLUS));
    if (Key(VK_SHIFT))
      speed = 3;
    if (Key(VK_CONTROL))
      speed = 0.3f;
    POINT c{};
    const bool rmb = Key(VK_RBUTTON) && GetCursorPos(&c);
    if (rmb && p.rmb) {
      mouse_x = float(c.x - p.cursor.x);
      mouse_y = float(c.y - p.cursor.y);
    }
    p.rmb = rmb;
    p.cursor = c;
  } else {
    p.rmb = false;
  }
#endif
  constexpr float kDeg = 3.14159265f / 180.0f;
  const float zoom_k = p.fov / 45.0f;  // (looks slower when zoomed in)
  p.yaw += (turn * 100.0f * dt + mouse_x * 0.15f) * kDeg * zoom_k;
  p.pitch = std::clamp(p.pitch + (look * 80.0f * dt + mouse_y * 0.15f) * kDeg * zoom_k, -85.0f * kDeg, 85.0f * kDeg);
  p.fov = std::clamp(p.fov + zoom * 40.0f * dt, 10.0f, 90.0f);
  const V3 g{std::cos(p.pitch) * std::sin(p.yaw), std::sin(p.pitch), std::cos(p.pitch) * std::cos(p.yaw)};
  const V3 r{std::cos(p.yaw), 0.0f, -std::sin(p.yaw)};  // (0,1,0) x g, normalized
  const float step = 400.0f * speed * dt;  // 4 m/s
  p.eye = {p.eye.x + (g.x * fwd + r.x * side) * step, p.eye.y + g.y * fwd * step - vert * step,
           p.eye.z + (g.z * fwd + r.z * side) * step};
  p.eye.y = std::min(p.eye.y, -10.0f);  // (not under the arena floor, y = 0; y is down)
  WriteView(base, cam, slot, p.eye, g, p.fov);
}
}  // namespace

// Before sub_82824370(camera): the match camera's view at camera + 240 replaced (RAMP, FREE CAMERA).
void Wwe12CameraTestBefore(u8 *base, u32 cam, u32 lr) {
  g_fov_cam = 0;
  const int preset = Wwe12BenchmarkActive() ? 0 : REXCVAR_GET(wwe12_match_camera);
  const bool fly = g_free.load() && !Wwe12BenchmarkActive();
  if ((preset != 1 && !fly) || !cam || !bd::mem::try_at<u8>(cam + 520))
    return;
  // The match only (after the game's match start, no scene, no loading screen), its cameras' caller only.
  if (lr != kMatchCameraCaller || !Wwe12MatchSeen() || Wwe12SceneMode() || Wwe12LoadingScreenUp())
    return;
  Moved *slot = Slot(cam);
  if (!slot)
    return;
  // The game's view: what is there now, unless it is still ours from the last update.
  if (std::memcmp(base + cam + 240, slot->ours, 64) == 0)
    std::memcpy(base + cam + 240, slot->game, 64);
  else
    std::memcpy(slot->game, base + cam + 240, 64);
  if (bd::mem::try_load<u32>(cam + 480, 1) != 0)
    return;  // not a perspective projection
  float v[4][4];
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j)
      v[i][j] = Ld(base, cam + 240 + u32(16 * i + 4 * j));
  for (int i = 0; i < 3; ++i) {
    const float n = v[i][0] * v[i][0] + v[i][1] * v[i][1] + v[i][2] * v[i][2];
    if (std::fabs(n - 1.0f) > 1e-2f || std::fabs(v[i][3]) > 1e-4f)
      return;
  }
  if (std::fabs(v[3][3] - 1.0f) > 1e-4f)
    return;
  // Row vectors (p_cam = p_world * V): V = [R^T ; -e R^T], R = the camera's axes (right, up, forward) as rows.
  float R[3][3];
  for (int j = 0; j < 3; ++j)
    for (int k = 0; k < 3; ++k)
      R[j][k] = v[k][j];
  const V3 f{R[2][0], R[2][1], R[2][2]};
  V3 e{};
  float *ep = &e.x;
  for (int k = 0; k < 3; ++k)
    ep[k] = -(v[3][0] * R[0][k] + v[3][1] * R[1][k] + v[3][2] * R[2][k]);
  if (std::max({std::fabs(e.x), std::fabs(e.y), std::fabs(e.z)}) < 50.0f)
    return;  // a camera at the origin (2D / menu)
  if (fly) {
    FreeView(base, cam, slot, e, f);
    return;
  }
  // What the game's camera looks at (it follows the action): on the plane through the ring's centre.
  const float dist = std::max(300.0f, -(e.x * f.x + e.y * f.y + e.z * f.z));
  const V3 t{e.x + f.x * dist, e.y + f.y * dist, e.z + f.z * dist};
  // RAMP (iMrBurn, 2026-11-27: "vue vers la rampe comme pendant les royal rumble"): from the side opposite the
  // stage, looking toward it. Every arena's stage is at +z (the Mod Maker's entrance ground: z up to 72 m), on the
  // left of the game's match camera (checked by iMrBurn). A fixed direction, a little down (y is down), like
  // Kaiko's RAMP for SvR 2011 (0, 0.3, 0.95); the eye follows what the game's camera looks at, 9 to 16 m back, and
  // the field of view is widened for that distance so the wrestlers keep the size the game gives them.
  const float pitch = float(REXCVAR_GET(wwe12_ramp_pitch)) * 3.14159265f / 180.0f;
  const V3 g{0.0f, std::sin(pitch), std::cos(pitch)};  // (y down: a positive pitch looks down)
  const float back = float(REXCVAR_GET(wwe12_ramp_distance)) * 100.0f;
  // The point followed: what the game's camera looks at, at chest height (y down, the arena floor at 0), smoothed.
  static V3 s_aim{0, 0, 0};
  static long long s_aim_ms = 0;
  const float follow = float(REXCVAR_GET(wwe12_ramp_follow));
  const V3 want{t.x * follow, -float(REXCVAR_GET(wwe12_ramp_aim_height)) * 100.0f, t.z * follow};
  const long long now_ms = NowMs();
  if (now_ms - s_aim_ms > 500) {
    s_aim = want;  // (first frame, or back after a scene / a pause)
  } else {
    const float k = std::clamp(float(now_ms - s_aim_ms) / 400.0f, 0.0f, 1.0f);  // ~0.4 s to catch up
    s_aim = {s_aim.x + (want.x - s_aim.x) * k, want.y, s_aim.z + (want.z - s_aim.z) * k};
  }
  s_aim_ms = now_ms;
  const V3 n{s_aim.x - g.x * back, s_aim.y - g.y * back, s_aim.z - g.z * back};
  const float fov = Ld(base, cam + 512);
  if (!WriteView(base, cam, slot, n, g, float(REXCVAR_GET(wwe12_ramp_fov))))
    return;
  static int s_lines = 0;
  const long long now = NowMs();
  if (now >= slot->next_log_ms && s_lines < 6) {
    slot->next_log_ms = now + 3000;
    ++s_lines;
    BD_INFO("[camera] RAMP: camera {:08X}: the game's eye ({:.0f}, {:.0f}, {:.0f}) looking at ({:.0f}, {:.0f}, "
            "{:.0f}), field of view {:.3f} -> eye ({:.0f}, {:.0f}, {:.0f}), {:.0f} back",
            cam, e.x, e.y, e.z, t.x, t.y, t.z, fov, n.x, n.y, n.z, back);
  }
}

// draw.cpp: the RAMP view is on (a view replaced in the last 250 ms). Its depth of field is then cut: the game
// sets the focus for its own camera (distance, zoom), so with ours the referee, the apron or the background came out
// blurred at times (iMrBurn's screenshots, 2026-11-27).
bool Wwe12RampCameraActive() {
  const long long t = g_ramp_ms.load(std::memory_order_relaxed);
  return t && NowMs() - t < 250;
}

// After sub_82824370: the game's field of view back (the view at +240 stays ours: it is read again for the draws).
void Wwe12CameraTestAfter(u8 *base) {
  if (!g_fov_cam)
    return;
  St(base, g_fov_cam + 512, g_fov_saved);
  g_fov_cam = 0;
}

// F5 (wwe12_display.cpp's bind): the next match camera, saved in wwe12.toml, and shown for a moment.
// iMrBurn (2026-11-27): only the game's camera, the ramp view and (next) the free camera; no FAR / CLOSE / HIGH / LOW.
constexpr int kMatchCameras = 2;  // 0 NORMAL, 1 RAMP (2 FREE CAMERA: palier 1)

const char *Wwe12MatchCameraName(int preset) {
  static const char *const kNames[] = {"NORMAL", "RAMP"};
  return kNames[std::clamp(preset, 0, kMatchCameras - 1)];
}

void Wwe12MatchCameraNext() {
  if (g_free.load()) {  // F5 in the free camera: back to the match camera
    FreeOff();
    char text[48];
    std::snprintf(text, sizeof text, "MATCH CAMERA: %s", Wwe12MatchCameraName(REXCVAR_GET(wwe12_match_camera)));
    Wwe12DisplayToast(text);
    BD_INFO("[camera] FREE: off (F5)");
    return;
  }
  const int next = (std::clamp(int(REXCVAR_GET(wwe12_match_camera)), 0, kMatchCameras - 1) + 1) % kMatchCameras;
  rex::cvar::SetFlagByName("wwe12_match_camera", std::to_string(next));
  char text[48];
  std::snprintf(text, sizeof text, "MATCH CAMERA: %s", Wwe12MatchCameraName(next));
  Wwe12DisplayToast(text);
  BD_INFO("[camera] {}", text);
  const auto path = rex::filesystem::GetExecutableFolder() / "wwe12.toml";
  TomlFile f;
  if (f.Load(path)) {
    f.SetInt("wwe12_match_camera", next);
    f.Save();
  }
}

// F6: the free camera on / off (in a match only).
void Wwe12FreeCameraToggle() {
  if (g_free.load()) {
    Wwe12MatchCameraNext();  // (off, as F5)
    return;
  }
  if (!InMatchNow()) {
    Wwe12DisplayToast("FREE CAMERA: IN A MATCH ONLY");
    return;
  }
  g_free_init = true;
  g_free_hud_off = false;
  g_free_paused = false;
  g_free = true;
  UpdateHideHud();
  Wwe12DisplayToast("FREE CAMERA - F5: BACK  F7: HUD  F8: PAUSE");
  BD_INFO("[camera] FREE: on (F6)");
}

// F7: the HUD hidden / shown (free camera only).
void Wwe12FreeCameraHud() {
  if (!FreeActive())
    return;
  g_free_hud_off = !g_free_hud_off.load();
  UpdateHideHud();
  Wwe12DisplayToast(g_free_hud_off.load() ? "HUD: HIDDEN" : "HUD: SHOWN");
}

// F8: the game's pause (its own Start) on / off (free camera only); the HUD (and its pause menu) hidden meanwhile.
void Wwe12FreeCameraPause() {
  if (!FreeActive())
    return;
  const bool paused = !g_free_paused.load();
  if (!paused)
    BD_INFO("[camera] FREE: pause off, {} camera updates during the pause", g_free_updates.load());
  g_free_updates = 0;
  g_free_paused = paused;
  UpdateHideHud();
  Wwe12PressStart();
  Wwe12DisplayToast(paused ? "PAUSE" : "PLAY");
}
