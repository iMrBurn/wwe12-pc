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

// When RAMP last replaced a view (ms): the depth of field is off while it is on (draw.cpp).
std::atomic<long long> g_ramp_ms{0};

u32 g_fov_cam = 0;   // the camera whose field of view (+512) is changed for this update (put back after it)
float g_fov_saved = 0;

// Before sub_82824370(camera): the match camera's view at camera + 240 replaced (RAMP).
void Wwe12CameraTestBefore(u8 *base, u32 cam, u32 lr) {
  g_fov_cam = 0;
  const int preset = Wwe12BenchmarkActive() ? 0 : REXCVAR_GET(wwe12_match_camera);
  if (preset != 1 || !cam || !bd::mem::try_at<u8>(cam + 520))
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
  // The camera's axes for that direction, built as the game's are: up = +y on screen rows (world y down), right =
  // up x forward (the game's: forward (1,0,0) gives right (0,0,-1), up (0,1,0)).
  V3 r{1.0f * g.z - 0.0f * g.y, 0.0f * g.x - 0.0f * g.z, 0.0f * g.y - 1.0f * g.x};  // (0,1,0) x g
  const float rl = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z);
  if (rl < 1e-4f)
    return;
  r = {r.x / rl, r.y / rl, r.z / rl};
  const V3 u{g.y * r.z - g.z * r.y, g.z * r.x - g.x * r.z, g.x * r.y - g.y * r.x};  // g x r
  const float A[3][3] = {{r.x, r.y, r.z}, {u.x, u.y, u.z}, {g.x, g.y, g.z}};
  for (int j = 0; j < 3; ++j) {
    for (int k = 0; k < 3; ++k)
      St(base, cam + 240 + u32(16 * k + 4 * j), A[j][k]);  // V[k][j] = A[j][k]
    St(base, cam + 240 + 48 + u32(4 * j), -(n.x * A[j][0] + n.y * A[j][1] + n.z * A[j][2]));
  }
  std::memcpy(slot->ours, base + cam + 240, 64);
  g_ramp_ms.store(now_ms, std::memory_order_relaxed);
  // The field of view for this update (the projection is built from it inside sub_82824370), put back after.
  const float fov = Ld(base, cam + 512);
  if (fov > 0.01f && fov < 3.0f) {
    g_fov_cam = cam;
    g_fov_saved = fov;
    St(base, cam + 512, float(REXCVAR_GET(wwe12_ramp_fov)) * 3.14159265f / 180.0f);
  }
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
