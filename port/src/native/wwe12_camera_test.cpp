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
// Test only: every camera that goes through sub_82824370 with a perspective projection is moved (menus and
// entrances too); "[camtest]" lines name each camera object seen (address, caller, eye, field of view).
// Values: 1 FAR, 2 CLOSE, 3 HIGH, 4 LOW (Kaiko's presets).

#include <rex/cvar.h>

#include <algorithm>
#include <cmath>
#include <cstring>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_INT32(wwe12_camera_test, 0, "WWE12",
                     "Dev: move the game's camera to test the camera object found (1 far, 2 close, 3 high, 4 low; "
                     "0 = off).")
    .range(0, 4);

namespace {

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

u32 g_saved_cam = 0;
u8 g_saved_view[64];

u32 g_seen[32];
int g_seen_n = 0;

}  // namespace

// Before sub_82824370(camera): the view at camera + 240 moved (saved first).
void Wwe12CameraTestBefore(u8 *base, u32 cam, u32 lr) {
  g_saved_cam = 0;
  const int preset = REXCVAR_GET(wwe12_camera_test);
  if (!preset || !cam || !bd::mem::try_at<u8>(cam + 520))
    return;
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
  // Same rotation (HIGH / LOW: the camera keeps its direction; a re-aim comes with palier 0), new translation.
  g_saved_cam = cam;
  std::memcpy(g_saved_view, base + cam + 240, 64);
  for (int j = 0; j < 3; ++j)
    St(base, cam + 240 + 48 + u32(4 * j), -(n.x * R[j][0] + n.y * R[j][1] + n.z * R[j][2]));
  bool known = false;
  for (int i = 0; i < g_seen_n; ++i)
    known = known || g_seen[i] == cam;
  if (!known && g_seen_n < 32) {
    g_seen[g_seen_n++] = cam;
    BD_INFO("[camtest] camera {:08X} (from {:08X}): eye ({:.1f}, {:.1f}, {:.1f}) forward ({:.3f}, {:.3f}, {:.3f}), "
            "field of view {:.3f}, {:.0f} from what it looks at -> eye ({:.1f}, {:.1f}, {:.1f})",
            cam, lr, e.x, e.y, e.z, f.x, f.y, f.z, Ld(base, cam + 512), dist, n.x, n.y, n.z);
  }
}

// After sub_82824370: the game's own view back (its next frame starts from it).
void Wwe12CameraTestAfter(u8 *base) {
  if (!g_saved_cam)
    return;
  std::memcpy(base + g_saved_cam + 240, g_saved_view, 64);
  g_saved_cam = 0;
}
