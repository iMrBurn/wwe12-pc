// Dev test (wwe12_camera_test, 0 = off by default): move the game's camera, to check that we found where (2026-11-27,
// Rendu; docs/etude_camera_libre.md, step A -> palier 0).
//
// Found with the camera probe (wwe12_camera_probe, v5): a camera object, updated each frame by sub_82824370(camera)
// (already hooked by wwe12_widescreen.cpp, which knows its field of view +512, near +500 and far +504):
//   +32  its local matrix, +96 its world matrix (camera -> world: rows right, up, forward, then the eye),
//   +176 the projection, +240 the view (world -> camera), +368 view x projection;
//   sub_82824370 rebuilds +96 = inverse(+240) (sub_821DB980), then +368 = +240 x +176 (sub_821DB8C0).
// So the camera's own code writes the VIEW at +240 before the update, like SvR 2011's view builder in Kaiko's port
// (sub_82227150(view, eye, direction); his MATCH CAMERA moves the eye just before it and puts the game's back just
// after, so the game's smoothing goes on from its own values; method only, with his permission, THIRD_PARTY.md).
//
// Here, the same idea on +240: before sub_82824370, the view's eye is moved (FAR / CLOSE along the camera's
// forward axis, around the point it looks at on the plane through the ring's centre; HIGH / LOW on the world's
// vertical axis, -y is up), the view rebuilt with the same rotation, and the game's view put back after the update
// (the world matrix, the view x projection and everything drawn from them keep ours for this frame).
// Test only: v6b moves the cameras updated from 8247F64C during a match (after sub_822B51A0, outside scenes and
// loading screens); "[camtest]" lines name each camera object moved (address, caller, eye, field of view).
// Values: 1 FAR, 2 CLOSE, 3 HIGH, 4 LOW (Kaiko's presets).

#include <rex/cvar.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_INT32(wwe12_camera_test, 0, "WWE12",
                     "Dev: move the game's camera to test the camera object found (1 far, 2 close, 3 high, 4 low; "
                     "0 = off).")
    .range(0, 4);

bool Wwe12MatchSeen();        // wwe12_fps.cpp: a match started since launch
bool Wwe12SceneMode();        // wwe12_fps.cpp: an entrance / victory / story scene
bool Wwe12LoadingScreenUp();  // wwe12_load_turbo.cpp

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

// Before sub_82824370(camera): the view at camera + 240 moved (saved first).
void Wwe12CameraTestBefore(u8 *base, u32 cam, u32 lr) {
  const int preset = REXCVAR_GET(wwe12_camera_test);
  if (!preset || !cam || !bd::mem::try_at<u8>(cam + 520))
    return;
  const bool in_match = Wwe12MatchSeen() && !Wwe12SceneMode() && !Wwe12LoadingScreenUp();
  if (in_match) {
    bool known = false;
    for (int i = 0; i < g_callers_n; ++i)
      known = known || (g_callers[i].lr == lr && g_callers[i].cam == cam);
    if (!known && g_callers_n < 48) {
      g_callers[g_callers_n++] = {lr, cam};
      BD_INFO("[camtest] in the match: sub_82824370(camera {:08X}) from {:08X} (projection type {}, field of view {:.3f})",
              cam, lr, bd::mem::try_load<u32>(cam + 480, 9), Ld(base, cam + 512));
    }
  }
  // v6b: the match only (after the game's match start, no scene, no loading screen), its cameras' caller only.
  if (lr != kMatchCameraCaller || !in_match)
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
  // Rotation part orthonormal (a view), else leave it.
  for (int i = 0; i < 3; ++i) {
    const float n = v[i][0] * v[i][0] + v[i][1] * v[i][1] + v[i][2] * v[i][2];
    if (std::fabs(n - 1.0f) > 1e-2f || std::fabs(v[i][3]) > 1e-4f)
      return;
  }
  if (std::fabs(v[3][3] - 1.0f) > 1e-4f)
    return;
  // Row vectors (p_cam = p_world * V): V = [R^T ; -e R^T] with R the world matrix's rows (camera axes).
  // R[j][k] = V[k][j]; eye e_k = -sum_j V[3][j] * R[j][k]; forward = R[2].
  float R[3][3];
  for (int j = 0; j < 3; ++j)
    for (int k = 0; k < 3; ++k)
      R[j][k] = v[k][j];
  V3 e{}, f{R[2][0], R[2][1], R[2][2]};
  float *ep = &e.x;
  for (int k = 0; k < 3; ++k)
    ep[k] = -(v[3][0] * R[0][k] + v[3][1] * R[1][k] + v[3][2] * R[2][k]);
  if (std::max({std::fabs(e.x), std::fabs(e.y), std::fabs(e.z)}) < 50.0f)
    return;  // a camera at the origin (2D / menu)
  // The point it looks at: on the plane through the ring's centre, at least 3 m ahead.
  const float dist = std::max(300.0f, -(e.x * f.x + e.y * f.y + e.z * f.z));
  const V3 t{e.x + f.x * dist, e.y + f.y * dist, e.z + f.z * dist};
  V3 n = e;
  switch (preset) {
    case 1: n = {t.x - f.x * dist * 1.45f, t.y - f.y * dist * 1.45f, t.z - f.z * dist * 1.45f}; break;  // FAR
    case 2: n = {t.x - f.x * dist * 0.65f, t.y - f.y * dist * 0.65f, t.z - f.z * dist * 0.65f}; break;  // CLOSE
    case 3: n.y = e.y - dist * 0.45f; break;  // HIGH (-y is up)
    case 4: n.y = std::min(e.y + dist * 0.25f, -120.0f); break;  // LOW (kept above the mat)
  }
  // HIGH / LOW: aimed at the same point again. The camera's axes are turned by the smallest rotation that takes
  // the old forward axis onto the new one (Rodrigues), so its roll and handedness stay the game's.
  if (preset == 3 || preset == 4) {
    V3 g{t.x - n.x, t.y - n.y, t.z - n.z};
    const float gl = std::sqrt(g.x * g.x + g.y * g.y + g.z * g.z);
    if (gl > 1.0f) {
      g = {g.x / gl, g.y / gl, g.z / gl};
      const V3 a{f.y * g.z - f.z * g.y, f.z * g.x - f.x * g.z, f.x * g.y - f.y * g.x};  // f x g
      const float sin_t = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z), cos_t = f.x * g.x + f.y * g.y + f.z * g.z;
      if (sin_t > 1e-5f) {
        const V3 k{a.x / sin_t, a.y / sin_t, a.z / sin_t};
        for (int j = 0; j < 3; ++j) {
          const V3 r{R[j][0], R[j][1], R[j][2]};
          const V3 kxr{k.y * r.z - k.z * r.y, k.z * r.x - k.x * r.z, k.x * r.y - k.y * r.x};
          const float kdr = k.x * r.x + k.y * r.y + k.z * r.z;
          R[j][0] = r.x * cos_t + kxr.x * sin_t + k.x * kdr * (1 - cos_t);
          R[j][1] = r.y * cos_t + kxr.y * sin_t + k.y * kdr * (1 - cos_t);
          R[j][2] = r.z * cos_t + kxr.z * sin_t + k.z * kdr * (1 - cos_t);
        }
        // the view's rotation part: V[k][j] = R[j][k]
        for (int j = 0; j < 3; ++j)
          for (int k2 = 0; k2 < 3; ++k2)
            St(base, cam + 240 + u32(16 * k2 + 4 * j), R[j][k2]);
      }
    }
  }
  // The new translation (with the rotation the view now has).
  for (int j = 0; j < 3; ++j)
    St(base, cam + 240 + 48 + u32(4 * j), -(n.x * R[j][0] + n.y * R[j][1] + n.z * R[j][2]));
  std::memcpy(slot->ours, base + cam + 240, 64);
  static int s_lines = 0;
  const long long now = NowMs();
  if (now >= slot->next_log_ms && s_lines < 80) {
    slot->next_log_ms = now + 3000;
    ++s_lines;
    BD_INFO("[camtest] camera {:08X} (from {:08X}): eye ({:.1f}, {:.1f}, {:.1f}) forward ({:.3f}, {:.3f}, {:.3f}), "
            "field of view {:.3f}, {:.0f} from what it looks at -> eye ({:.1f}, {:.1f}, {:.1f})",
            cam, lr, e.x, e.y, e.z, f.x, f.y, f.z, Ld(base, cam + 512), dist, n.x, n.y, n.z);
  }
}

// After sub_82824370: nothing any more (v6c, see Moved above).
void Wwe12CameraTestAfter(u8 *) {}
