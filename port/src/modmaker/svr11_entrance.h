// Mod Maker: an entrance of SvR 2011 (pac\evt\NyujyoN.pac: its script EVPx/NNN, "0FOP", and its tracks EVTx/NNN, a
// YMKs motion bank) made into a WWE '12 entrance (the script, and an EVZ: a PACH of YANM records, one per object).
// Ported from LA Max's (Monday Night Max) SVR11_to_WWE12_Tool (svr11_to_wwe12*.py, evp_svr11_to_wwe12.py,
// yanm_write.py; used with his permission, credit given), which stands on Kaiko's Mod Maker tools (ymks.py,
// w13_retarget.py, w13_skeletons.py, finger_retarget.py, svr08_entrance.py, yanm_decode.py: the YMKs and YANM formats,
// the two skeletons and the hands, reverse-engineered from the games' code; credit in docs/mod_maker.md). Two additions
// of ours on top: the global object's lights rebuilt from SvR 2011's own light tracks, and every block's size written
// in its record's table.
//
// SvR 2011 (YMKs, little-endian): a bank of motions by (id, x, y), id = 25000 + the script's object number, x = the
// segment, y = the track (0 the body, 2 / 3 the right / left hand); each motion a stream of 30 Hz frames, one record
// per frame with a key: [0x80 | n][n bytes of events][the pose]; a key at frame f with count c is reached at f + c + 1.
// WWE '12 (YANM, big-endian, yanm.h): per object a record of blocks (one per segment, named "XXXXYYYY"), each a YANM
// chunk (tracks by bone name hash, keys in 60 Hz frames), its POF0 and its events (YAT2 commands).
// Per object (the script's object n = track id 25000 + n, record n):
//  - the entrant (a character with a body track): the 21 bones turned into WWE '12's skeleton (rotations: R13 = R11 x
//    bind11^T x bind13, the whole root on 'Bip'), the pelvis position, the 30 finger joints, the events; the other
//    tracks of WWE '12's body (face, helpers) keep the base entrance's first pose;
//  - the camera (eye, target, field of view; the target distance, not in SvR 2011, guessed from the entrant's pelvis);
//  - ropes, turnbuckles: the rope track's keys, the other tracks the base's, re-timed;
//  - props: the base's tracks; the global object: its lights (SvR 2011's 25-byte light keys); else the base's tracks;
//  - anything else: the base's 96-byte placeholder.
// The base (WWE '12's own entrance 100, the Rock's: Convert's base_evz12) gives the record headers, the static tracks
// and the body's track list. The script: positions x 10 (cm), spaces of the name as '_', header word +0x44.
// Byte for byte what the Python tools write (numpy's 3x3 products are BLAS chains of fused multiply-adds: Mul3 / Mul4
// do the same; Python's round() is round-half-even: Round; Python 3.12's sum() is compensated: PySum).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "svrfmt/pac.h"
#include "yanm.h"
#include "yanm_ik.h"

namespace svr11_entrance {

using svrfmt::Bytes;

namespace detail {

// ---------------------------------------------------------------------------------------------------- errors, numbers
// (the first failure is kept; the Python tools stop there with an exception)
struct Ctx {
  bool ok = true;
  std::string why;
  void Fail(const std::string &w) {
    if (ok)
      ok = false, why = w;
  }
};

inline long long Round(double v) { return (long long)std::nearbyint(v); }  // Python's round(): half to even
inline long long PyMod(long long a, long long m) { return ((a % m) + m) % m; }
inline int Wrap16(double v) { return int(PyMod(Round(v) + 32768, 65536) - 32768); }  // an angle of +pi is -pi
inline int Clamp16(double v) { return int(std::max(-32768LL, std::min(32767LL, Round(v)))); }

const double kPi = 3.141592653589793;
const double kPi16 = kPi / 32768;                // s16 angle unit (rot16, YMKs bones)
const double kAng = 2 * kPi / 65536.0;           // YMKs angle unit
const double kStep = 2 * kPi / 256.0;            // SvR 2011 finger angle unit (s8)
const double kRootUnit = 0.01;                   // YMKs root position unit (model units, 10 cm)

// A read-only view; Sub() is Python's slice (clamped).
struct Span {
  const uint8_t *p = nullptr;
  size_t n = 0;
  uint8_t operator[](size_t i) const { return p[i]; }
  Span Sub(size_t a, size_t b = SIZE_MAX) const {
    a = std::min(a, n), b = std::max(a, std::min(b, n));
    return {p + a, b - a};
  }
  bool Empty() const { return n == 0; }
};
inline Span SpanOf(const Bytes &b) { return {b.data(), b.size()}; }
inline Bytes BytesOf(Span s) { return Bytes(s.p, s.p + s.n); }

inline uint32_t Le32(Span s, size_t o) { return svrfmt::Le32(s.p + o); }
inline int16_t Le16s(Span s, size_t o) { return int16_t(svrfmt::Le16(s.p + o)); }
inline float LeF(Span s, size_t o) { return svrfmt::LeF(s.p + o); }

// ---------------------------------------------------------------------------------------------------- 3x3 / 4x4 math
using V3 = std::array<double, 3>;
using M3 = std::array<double, 9>;   // row-major
using M4 = std::array<double, 16>;
using Q = std::array<double, 4>;    // x, y, z, w

inline M3 Eye3() { return {1, 0, 0, 0, 1, 0, 0, 0, 1}; }
// numpy's a @ b (BLAS dgemm): each element a chain of fused multiply-adds from 0, in k order
inline M3 Mul3(const M3 &a, const M3 &b) {
  M3 c;
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) {
      double s = 0.0;
      for (int k = 0; k < 3; ++k)
        s = std::fma(a[3 * i + k], b[3 * k + j], s);
      c[3 * i + j] = s;
    }
  return c;
}
inline M4 Mul4(const M4 &a, const M4 &b) {
  M4 c;
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) {
      double s = 0.0;
      for (int k = 0; k < 4; ++k)
        s = std::fma(a[4 * i + k], b[4 * k + j], s);
      c[4 * i + j] = s;
    }
  return c;
}
inline M3 Tr(const M3 &a) { return {a[0], a[3], a[6], a[1], a[4], a[7], a[2], a[5], a[8]}; }
inline M3 Rot3(const M4 &a) { return {a[0], a[1], a[2], a[4], a[5], a[6], a[8], a[9], a[10]}; }
inline M4 Trs(const M3 &r, const V3 &t) {
  return {r[0], r[1], r[2], t[0], r[3], r[4], r[5], t[1], r[6], r[7], r[8], t[2], 0, 0, 0, 1};
}
// R = Rz Ry Rx (the node tables' Euler order)
inline M3 Rotm(double x, double y, double z) {
  const double cx = std::cos(x), sx = std::sin(x), cy = std::cos(y), sy = std::sin(y), cz = std::cos(z),
               sz = std::sin(z);
  return {cy * cz, sx * sy * cz - cx * sz, cx * sy * cz + sx * sz,
          cy * sz, sx * sy * sz + cx * cz, cx * sy * sz - sx * cz,
          -sy,     sx * cy,                cx * cy};
}
inline M3 Rotm(const V3 &r) { return Rotm(r[0], r[1], r[2]); }
inline V3 EulerZyx(const M3 &R) {
  const double sy = std::max(-1.0, std::min(1.0, -R[6])), y = std::asin(sy);
  if (std::fabs(sy) < 0.99999)
    return {std::atan2(R[7], R[8]), y, std::atan2(R[3], R[0])};
  return {0.0, y, std::atan2(-R[1], R[4])};
}

inline Q QMul(const Q &a, const Q &b) {
  return {a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
          a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
}
inline Q QAxis(int axis, double ang) {
  Q q{0.0, 0.0, 0.0, std::cos(ang * 0.5)};
  q[size_t(axis)] = std::sin(ang * 0.5);
  return q;
}
inline Q QEuler(double x, double y, double z) { return QMul(QMul(QAxis(2, z), QAxis(1, y)), QAxis(0, x)); }
// Python 3.12's sum() of floats: Neumaier's compensated sum (the Python tools' dot products and norms)
inline double PySum(const double *v, size_t n) {
  double s = 0.0 + v[0], c = 0.0;
  for (size_t i = 1; i < n; ++i) {
    const double t = s + v[i];
    c += std::fabs(s) >= std::fabs(v[i]) ? (s - t) + v[i] : (v[i] - t) + s;
    s = t;
  }
  if (c != 0.0 && std::isfinite(c))
    s += c;
  return s;
}
inline double QDot(const Q &a, const Q &b) {
  const double p[4] = {a[0] * b[0], a[1] * b[1], a[2] * b[2], a[3] * b[3]};
  return PySum(p, 4);
}
inline Q QNorm(Q q) {
  const double p[4] = {q[0] * q[0], q[1] * q[1], q[2] * q[2], q[3] * q[3]};
  double n = std::sqrt(PySum(p, 4));
  if (n == 0.0)
    n = 1.0;
  for (auto &c : q)
    c = c / n;
  return q;
}
inline Q QSlerp(const Q &a, Q b, double t) {
  double d = QDot(a, b);
  if (d < 0) {
    for (auto &c : b)
      c = -c;
    d = -d;
  }
  Q o;
  if (d > 0.9995) {
    for (int i = 0; i < 4; ++i)
      o[size_t(i)] = a[size_t(i)] + (b[size_t(i)] - a[size_t(i)]) * t;
    return QNorm(o);
  }
  const double th = std::acos(std::min(1.0, d)), s = std::sin(th);
  const double wa = std::sin((1 - t) * th) / s, wb = std::sin(t * th) / s;
  for (int i = 0; i < 4; ++i)
    o[size_t(i)] = wa * a[size_t(i)] + wb * b[size_t(i)];
  return o;
}
inline M3 QToMat(const Q &q) {
  const double x = q[0], y = q[1], z = q[2], w = q[3];
  return {1 - 2 * (y * y + z * z), 2 * (x * y - z * w),     2 * (x * z + y * w),
          2 * (x * y + z * w),     1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
          2 * (x * z - y * w),     2 * (y * z + x * w),     1 - 2 * (x * x + y * y)};
}
inline Q MatToQ(const M3 &m) {
  const double tr = m[0] + m[4] + m[8];
  if (tr > 0) {
    const double s = std::sqrt(tr + 1.0) * 2;
    return QNorm({(m[7] - m[5]) / s, (m[2] - m[6]) / s, (m[3] - m[1]) / s, 0.25 * s});
  }
  if (m[0] > m[4] && m[0] > m[8]) {
    const double s = std::sqrt(1.0 + m[0] - m[4] - m[8]) * 2;
    return QNorm({0.25 * s, (m[1] + m[3]) / s, (m[2] + m[6]) / s, (m[7] - m[5]) / s});
  }
  if (m[4] > m[8]) {
    const double s = std::sqrt(1.0 + m[4] - m[0] - m[8]) * 2;
    return QNorm({(m[1] + m[3]) / s, 0.25 * s, (m[5] + m[7]) / s, (m[2] - m[6]) / s});
  }
  const double s = std::sqrt(1.0 + m[8] - m[0] - m[4]) * 2;
  return QNorm({(m[2] + m[6]) / s, (m[5] + m[7]) / s, 0.25 * s, (m[3] - m[1]) / s});
}
// yanm_decode.q_to_euler (its gimbal case keeps z = 0, EulerZyx's x = 0)
inline V3 QToEuler(const Q &q) {
  const M3 m = QToMat(q);
  const double sy = std::max(-1.0, std::min(1.0, -m[6])), y = std::asin(sy);
  if (std::fabs(sy) < 0.99999)
    return {std::atan2(m[7], m[8]), y, std::atan2(m[3], m[0])};
  return {std::atan2(-m[5], m[4]), y, 0.0};
}

// ---------------------------------------------------------------------------------------------------- skeletons
// Kaiko's reference skeletons (w13_skeletons.py; node tables of two models, pruned to the motion chains):
// WWE '13 ch246 (Mankind, the same Biped rig as WWE '12) in cm, SvR 2011 ch104 in model units; -Y up, floor origin.
// (name, t, r = Euler zyx radians, parent)
struct Node {
  const char *name;
  V3 t, r;
  int parent;
};
inline const std::vector<Node> &Nodes13() {
  static const std::vector<Node> n = {
      {"vector", {0.0, 0.0, 0.0}, {1.5e-06, -1e-07, -1e-07}, -1},
      {"null", {0.0, 0.0, 0.0}, {0.0, -0.0, 0.0}, -1},
      {"Bip", {0.0, -96.5507507, 5e-07}, {-0.0, 1.5711416, 0.0}, 1},
      {"Bip Pelvis", {-0.0, 0.0, 0.0}, {1.5707958, 3.1415925, 1.5707963}, 2},
      {"Bip L Thigh", {0.0002441, -11.5000143, -8.9e-06}, {-1.2e-06, -2.2e-06, -2.8797929}, 3},
      {"L_Thigh_x", {21.6596985, -1.14e-05, 0.0013944}, {3e-07, -1.7e-06, -0.0}, 4},
      {"Bip L Calf", {21.8202972, 1.14e-05, -0.0014149}, {2e-07, 7e-07, 0.0}, 5},
      {"Bip L Foot", {43.4799881, -7.6e-06, 0.001296}, {1e-07, 3.68e-05, -1e-07}, 6},
      {"Bip L Toe0", {8.3083582, -0.0191727, 11.8992615}, {-4e-07, -1.5711416, 0.0}, 7},
      {"Bip R Thigh", {0.0002365, 11.4999838, 6.9e-06}, {-3e-07, 4e-07, 2.8797929}, 3},
      {"R_Thigh_x", {21.6535416, 3.81e-05, 0.0008295}, {-5e-07, 0.0, 1e-07}, 9},
      {"Bip R Calf", {21.8264275, 3.8e-06, -0.001462}, {-4e-07, -4.02e-05, -1e-07}, 10},
      {"Bip R Foot", {43.4799805, 3.8e-06, -0.0013588}, {1e-07, 7.82e-05, 0.0}, 11},
      {"Bip R Toe0", {8.3091793, 0.0196915, 11.8983259}, {-4e-07, -1.5711416, 0.0}, 12},
      {"Bip Spine", {-3e-07, -18.6909637, 7.8e-06}, {1.5707958, -3.1415927, 1.5707953}, 2},
      {"Bip Spine1", {22.3148727, 0.0, 1.86e-05}, {7e-07, 8.17e-05, 0.0}, 14},
      {"Bip Neck", {29.5010986, 0.0, 0.0024509}, {8e-07, -8.28e-05, 0.0}, 15},
      {"Neck_x", {3.5233917, -0.0039673, -0.0038572}, {0.0, -3e-07, 0.0}, 16},
      {"Bip Head", {5.3445892, 0.006012, 2.7349157}, {-1e-06, 7e-07, 0.0}, 17},
      {"Bip L Clavicle", {17.6370239, -3.05e-05, 0.0014681}, {-3.1415396, -8e-07, -1.5707964}, 15},
      {"Bip L UpperArm", {20.0014915, -0.0015564, 3.03e-05}, {2.16e-05, 1e-07, 0.261799}, 19},
      {"L_UpperArm_x", {14.0794487, -0.0011292, 5.6e-06}, {1e-07, 1e-07, -1e-07}, 20},
      {"Bip L Forearm", {14.0696106, -0.0011292, 0.0}, {-0.0, 0.3490654, -3e-07}, 21},
      {"L_Wrist_x", {9.7812347, -0.0008087, -3.8e-06}, {8.7e-06, 5e-07, 1.1e-06}, 22},
      {"L_Wrist_x2", {9.077755, -0.0006256, 6.7e-06}, {1.3e-06, 2e-06, -1.3e-06}, 23},
      {"Bip L Hand", {9.2910805, -0.0007324, 2.9e-06}, {-1.5707968, -1.7e-06, 0.0}, 24},
      {"Bip R Clavicle", {17.6370239, -0.0015259, 1.31e-05}, {3.1415114, 6e-07, 1.5707964}, 15},
      {"Bip R UpperArm", {19.9998322, -0.0015259, -0.0014551}, {-9.8e-06, -3e-07, -0.2618007}, 26},
      {"R_UpperArm_x", {14.0794506, 0.0044861, -2e-06}, {0.0, 2e-07, 0.0}, 27},
      {"Bip R Forearm", {14.1205711, 0.0011444, -3.24e-05}, {6e-07, 0.3490664, 1.2e-06}, 28},
      {"R_Wrist_x", {9.7302628, 0.0009003, 5.34e-05}, {8.8e-06, -1.3e-06, -5e-07}, 29},
      {"R_Wrist_x2", {9.0777931, 0.0007629, -6.7e-06}, {1.1e-06, 2e-07, 1e-07}, 30},
      {"Bip R Hand", {9.2922173, 0.0007324, -4.8e-06}, {1.5708044, 7e-07, 9e-07}, 31},
  };
  return n;
}
inline const std::vector<Node> &Nodes11() {
  static const std::vector<Node> n = {
      {"root", {0.0, 0.0, 0.0}, {6e-07, 1e-07, 1.5707998}, -1},
      {"l_momo", {-3.9e-06, -1.1419998, -0.0001423}, {-0.0276719, 0.0009473, -0.2603565}, 0},
      {"l_momo_x", {2.1659713, 2e-07, 0.0}, {-1e-07, -0.0, -0.0}, 1},
      {"l_sune", {2.1820302, 1e-07, -9e-07}, {-0.0, 0.0, -0.0}, 2},
      {"l_ashi", {4.3480034, -0.0003042, 1.74e-05}, {-0.0, 0.0, -0.0}, 3},
      {"l_tsumasaki", {0.8307934, -2.22e-05, -1.1899576}, {1e-07, -0.0, 2e-07}, 4},
      {"r_momo", {3.9e-06, 1.141799, -0.0001437}, {0.0264962, -0.0080374, 0.2602296}, 0},
      {"r_momo_x", {2.1653554, -8.36e-05, 1e-07}, {-0.0, -0.0, 0.0}, 6},
      {"r_sune", {2.1826453, 0.0001166, -1.1e-06}, {-0.0, -0.0, 0.0}, 7},
      {"r_ashi", {4.3480034, 0.0001674, 1.74e-05}, {0.0, -0.0, 1e-07}, 8},
      {"r_tsumasaki", {0.8307924, -5.89e-05, -1.1899575}, {-5e-07, -0.0, -7.73e-05}, 9},
      {"koshi", {-1.8690751, 6.5e-06, -1e-07}, {7e-07, 5e-07, 3.1415899}, 0},
      {"mune", {2.2314901, -1.7e-06, 9e-07}, {2e-07, -4e-07, 8.11e-05}, 11},
      {"r_sakotsu", {1.7637029, -0.0001443, -0.0}, {-5e-07, -5e-07, -1.5708783}, 12},
      {"r_ninoude", {1.9999816, 5e-07, -8e-07}, {0.0, -0.0, -0.2617993}, 13},
      {"r_ninoude_x", {1.4079437, 0.0, -5e-07}, {-1e-07, 1e-07, -0.0}, 14},
      {"r_kote", {1.4069604, 0.0, -7e-07}, {-8e-07, 0.3490658, 5e-07}, 15},
      {"r_kote_x", {0.9781222, -1e-06, 6e-07}, {1e-07, -1e-07, -0.0}, 16},
      {"r_kote_x2", {0.9077768, 5e-07, 1e-07}, {-0.0, 0.0, -0.0}, 17},
      {"r_te", {0.9292183, 5e-07, -5e-07}, {1e-07, -1e-07, -1e-07}, 18},
      {"l_sakotsu", {1.7637029, -0.0001443, -0.0}, {0.0, -5e-07, 1.5706182}, 12},
      {"l_ninoude", {2.0000024, 0.0001922, -1.2e-06}, {-1e-06, 0.0, 0.2618958}, 20},
      {"l_ninoude_x", {1.4079449, 0.0001364, -8e-07}, {0.0, 5e-07, 9.69e-05}, 21},
      {"l_kote", {1.4069612, -5e-07, -1e-07}, {3e-07, 0.3490663, -5e-07}, 22},
      {"l_kote_x", {0.9781246, -5e-07, -5e-07}, {-0.0, 5e-07, 3e-07}, 23},
      {"l_kote_x2", {0.9077759, 0.0, 2e-07}, {-0.0, -0.0, 0.0}, 24},
      {"l_te", {0.9291086, 0.0, 4e-07}, {-5e-07, -0.0, 0.0}, 25},
      {"kubi", {2.9482093, -0.0002408, 0.0005655}, {0.0, -0.0, -8.15e-05}, 12},
      {"kubi_x", {0.4442077, -0.0001418, 0.0002757}, {-1e-06, 7e-07, 0.0006962}, 27},
      {"atama", {0.4452028, -0.0001687, -0.2739244}, {5e-07, -0.0006199, -0.0002513}, 28},
  };
  return n;
}
// The 21 YMKs body bones in their motion order; the rotations SvR 2011's own motions are made against (HCTP's rest
// pose: REST11); the WWE '12 bone each one drives (MAP).
struct Bone11 {
  const char *name;
  V3 rest;
  const char *w12;
};
inline const std::array<Bone11, 21> &Bones11() {
  static const std::array<Bone11, 21> b = {{
      {"root", {6e-07, 1e-07, 1.5707998}, "Bip Pelvis"},
      {"koshi", {7e-07, 5e-07, 3.1415899}, "Bip Spine"},
      {"mune", {2e-07, -4e-07, 8.11e-05}, "Bip Spine1"},
      {"kubi", {0.0, -0.0, -8.15e-05}, "Bip Neck"},
      {"atama", {0.0, 0.0, 0.0}, "Bip Head"},
      {"l_sakotsu", {0.0, -5e-07, 1.5706182}, "Bip L Clavicle"},
      {"l_ninoude", {0.0, 0.0, 0.0}, "Bip L UpperArm"},
      {"l_kote", {0.0, 0.0, 0.0}, "Bip L Forearm"},
      {"l_te", {-5e-07, -0.0, 0.0}, "Bip L Hand"},
      {"r_sakotsu", {-5e-07, -5e-07, -1.5708783}, "Bip R Clavicle"},
      {"r_ninoude", {0.0, 0.0, 0.0}, "Bip R UpperArm"},
      {"r_kote", {0.0, 0.0, 0.0}, "Bip R Forearm"},
      {"r_te", {1e-07, -1e-07, -1e-07}, "Bip R Hand"},
      {"l_momo", {0.0, 0.0, 0.0}, "Bip L Thigh"},
      {"l_sune", {-0.0, 0.0, -0.0}, "Bip L Calf"},
      {"l_ashi", {-0.0, 0.0, -0.0}, "Bip L Foot"},
      {"l_tsumasaki", {1e-07, -0.0, 2e-07}, "Bip L Toe0"},
      {"r_momo", {0.0, 0.0, 0.0}, "Bip R Thigh"},
      {"r_sune", {-0.0, -0.0, 0.0}, "Bip R Calf"},
      {"r_ashi", {0.0, -0.0, 1e-07}, "Bip R Foot"},
      {"r_tsumasaki", {-5e-07, -0.0, -7.73e-05}, "Bip R Toe0"},
  }};
  return b;
}
// YMKs bone types in m.pac / entrance banks (ymks.TYPES_M1): bits 0-2 = the axes x y z stored (s16 each)
inline const std::array<int, 25> &TypesM1() {
  static const std::array<int, 25> t = {7, 7, 7, 6, 0x17, 6, 7, 3, 6, 6, 7, 3, 6, 7, 2, 0x17, 2, 7, 2, 0x17, 2, 7, 7, 7, 7};
  return t;
}

// The fingers (finger_retarget.py): the bind rotations of the 15 finger joints per hand, SvR 2011 ch104 (l_yubiFJ)
// and WWE '13 ch246 (Bip L FingerF / FingerFJ); [side][finger][joint], side 0 = left, 1 = right.
inline const V3 &Finger11R(int side, int f, int j) {
  static const V3 r[2][5][3] = {
      {{{0.2532623, 0.7518788, 0.362218}, {-1.228575e-07, -0.4363322, 2.677201e-07}, {-5.016821e-15, -2.79396e-09, -2.235175e-07}},
       {{4.117128e-07, 0.1039879, 4.45531e-08}, {3.725291e-09, -3.72529e-09, -6.938895e-18}, {-3.725289e-09, -1.862645e-08, -1.490116e-08}},
       {{-4.060567e-07, -8.381892e-09, 2.980232e-08}, {7.45058e-09, 2.700835e-08, -1.490116e-08}, {7.450581e-09, -2.775559e-17, 7.450583e-09}},
       {{6.492649e-09, -0.1208366, -7.50531e-09}, {5.587938e-09, 1.071021e-08, 5.215406e-08}, {-9.48086e-07, 2.7474e-08, -1.490115e-08}},
       {{-7.459605e-09, -0.2084018, 4.095495e-08}, {1.303852e-08, 2.374873e-08, 2.235174e-08}, {-5.709007e-06, 1.070961e-08, -5.960475e-08}}},
      {{{-0.2532607, 0.7518799, -0.3622178}, {4.137486e-10, -0.4363322, 7.565289e-08}, {-1.862645e-08, 6.053594e-08, -1.490116e-08}},
       {{1.909743e-06, 0.1039887, 7.742133e-08}, {-9.685746e-08, 4.507602e-07, 5.811454e-07}, {-7.823109e-08, -1.322478e-07, 7.450578e-08}},
       {{-3.725752e-09, 8.382134e-09, 6.181747e-05}, {-4.768372e-07, 1.303748e-08, -4.284084e-06}, {7.450579e-09, -1.024455e-08, -4.991889e-07}},
       {{1.631445e-06, -0.1208358, 2.24123e-06}, {-1.907349e-06, 5.587462e-09, -4.842877e-07}, {3.20375e-07, 3.697349e-07, 3.501773e-07}},
       {{1.528601e-06, -0.2084009, 1.940669e-06}, {-9.499519e-08, 3.948806e-07, -1.586973e-06}, {2.384185e-07, -2.761371e-07, 1.490112e-08}}},
  };
  return r[side][f][j];
}
inline const V3 &Finger13R(int side, int f, int j) {
  static const V3 r[2][5][3] = {
      {{{1.570796, -0.2617981, 0.7851787}, {-0.0001325559, -0.4354581, 5.673192e-05}, {6.426125e-07, -2.65427e-07, 2.682208e-07}},
       {{-8.660321e-07, -7.766268e-07, 0.1039888}, {1.128763e-06, 1.862648e-07, -4.759057e-07}, {1.676381e-07, 1.639128e-07, 1.611188e-07}},
       {{-1.262873e-06, -9.685737e-08, 2.104789e-07}, {1.713634e-07, 4.693865e-07, 3.371388e-07}, {1.788139e-07, 1.639128e-07, 1.406297e-07}},
       {{-1.280868e-06, 1.613873e-07, -0.1208363}, {1.788139e-07, 1.862645e-07, 2.970919e-07}, {1.750887e-07, 1.788139e-07, 8.335338e-08}},
       {{-8.896463e-07, -2.965419e-07, -0.2084012}, {1.806766e-07, 1.713633e-07, 2.630987e-07}, {1.713634e-07, 6.556511e-07, 2.118759e-07}}},
      {{{-1.570798, -0.26169, -0.7851497}, {1.822909e-06, -0.4354595, 1.510663e-06}, {8.009375e-08, 1.294538e-07, 1.639128e-07}},
       {{-9.930907e-06, -1.559816e-06, -0.1039891}, {1.676381e-07, 7.450616e-09, -4.656613e-07}, {1.676381e-07, 2.235172e-08, 2.747402e-07}},
       {{-8.884816e-06, -3.054734e-07, 9.313357e-08}, {1.788139e-07, -3.054738e-07, 3.324821e-07}, {-3.017473e-07, 1.579523e-06, 1.571141e-06}},
       {{-6.902413e-06, 6.56749e-07, 0.120837}, {1.788139e-07, -3.054738e-07, -4.190978e-09}, {-2.998848e-07, 1.601875e-06, 1.387205e-06}},
       {{9.593376e-05, 6.502534e-05, 0.2084028}, {-0.0001056828, -6.274129e-05, 1.115314e-06}, {1.750886e-07, 1.713634e-07, 2.840536e-08}}},
  };
  return r[side][f][j];
}
// A: the rotation taking WWE '13 hand coordinates to SvR 2011's (finger_retarget's Kabsch fit of the 30 bind joint
// positions of the two tables, x 0.1; computed once with numpy, kept to the last bit)
inline const M3 &HandA(int side) {
  static const M3 a[2] = {
      {0.9999999921385362, -9.811625463268325e-05, -7.80777156631862e-05, 7.808445772807442e-05, 6.871311412906676e-05,
       0.999999994590663, -9.81108891389796e-05, -0.9999999928258546, 6.872077494347021e-05},
      {0.9999999926322016, 9.481227843303722e-05, -7.580388165329852e-05, -7.580877755770318e-05, 5.163534514277185e-05,
       -0.9999999957934104, -9.480836387461687e-05, 0.9999999941722109, 5.164253236547709e-05},
  };
  return a[side];
}

// ---------------------------------------------------------------------------------------------------- YMKs
// Bank: 'YMKs', u32 0x100, 8 bytes, the event size table T[256] (0x10..0x110), u32 n, n x {u8 y, u8 x, u16 id,
// u32 offset, u32 frames, u32 rt}, the motions (an entry's data runs to the next offset).
struct Motion {
  int y = 0, x = 0, id = 0;
  uint32_t frames = 0;
  Span data;
};
inline bool ReadBank(const Bytes &raw, std::vector<Motion> &out, Span &T) {
  if (raw.size() < 0x114 || (memcmp(raw.data(), "YMKs", 4) && memcmp(raw.data(), "YMBs", 4)))
    return false;
  const Span r = SpanOf(raw);
  T = r.Sub(0x10, 0x110);
  const uint32_t n = Le32(r, 0x110);
  const size_t base = 0x114 + 16 * size_t(n);
  if (base > raw.size())
    return false;
  const size_t dl = raw.size() - base;
  std::set<size_t> offs;
  for (uint32_t i = 0; i < n; ++i)
    if (Le32(r, 0x114 + 16 * i + 4) < dl)
      offs.insert(Le32(r, 0x114 + 16 * i + 4));
  out.clear();
  for (uint32_t i = 0; i < n; ++i) {
    const size_t q = 0x114 + 16 * size_t(i), off = Le32(r, q + 4);
    Motion m;
    m.y = r[q], m.x = r[q + 1], m.id = svrfmt::Le16(r.p + q + 2), m.frames = Le32(r, q + 8);
    if (off < dl) {
      auto it = offs.upper_bound(off);
      m.data = r.Sub(base + off, base + (it == offs.end() ? dl : *it));
    }
    out.push_back(m);
  }
  return true;
}

// A motion's frame stream: b < 0x60 skips b frames; 0x60 + u16 or 0x60 + size: a record, one frame.
inline bool Records(Span d, std::vector<std::pair<int, Span>> &out, Ctx &ctx) {
  out.clear();
  size_t p = 0;
  int f = 0;
  while (p < d.n) {
    const uint8_t b = d[p];
    if (b < 0x60) {
      f += b, ++p;
      continue;
    }
    size_t sz;
    if (b == 0x60) {
      if (p + 2 >= d.n)
        return ctx.Fail("YMKs: record size out of the motion"), false;
      sz = d[p + 1] | size_t(d[p + 2]) << 8, p += 3;
    } else
      sz = b - 0x60u, p += 1;
    if (p + sz > d.n)
      return ctx.Fail("YMKs: record overruns motion data"), false;
    out.push_back({f++, d.Sub(p, p + sz)});
    p += sz;
  }
  return true;
}
// [0x80 | n][n bytes of events][pose]
inline void SplitRecord(Span r, Span &ev, Span &pose) {
  ev = {}, pose = r;
  if (!r.Empty() && (r[0] & 0x80)) {
    const size_t n = r[0] & 0x7f;
    ev = r.Sub(1, 1 + n), pose = r.Sub(1 + n);
  }
}
// events: each T[op] bytes long; op -1: the rest, unreadable (a size of 0, or past the end)
struct Event {
  int op;
  Span b;
};
inline std::vector<Event> ParseEvents(Span ev, Span T) {
  std::vector<Event> out;
  size_t p = 0;
  while (p < ev.n) {
    const int op = ev[p];
    const size_t n = T[size_t(op)];
    if (n == 0 || p + n > ev.n) {
      out.push_back({-1, ev.Sub(p)});
      break;
    }
    out.push_back({op, ev.Sub(p, p + n)});
    p += n;
  }
  return out;
}

// A body pose (m.pac's unpacked format, ymks.decode_body): sub-records by their first byte:
//   0x7d / 0x7c / 0x60-0x6f  count, root (f32 / s16 / s16), the 21 bones;   0x7a / 0x75 / 0x50-0x5f  the same + a mask
//   0x7e / 0x7f  count + root;  0x7b ref, count, s16 root (not the root);  0x76 ref, count, 4 bytes;  0x74 ...
//   0x00-0x18  one bone: [k][count][values]
// A bone = the axes of its type, s16 each (angle = v x 2 pi / 65536, R = Rz Ry Rx on the motion bind).
struct BodySub {
  bool has_count = false, has_root = false;
  int op = 0, count = 0;
  V3 root{};
  std::vector<std::pair<int, std::array<int, 3>>> bones;
};
struct BodyReader {
  Span s;
  bool bad = false;
  uint8_t U8(size_t o) {
    if (o >= s.n)
      return bad = true, 0;
    return s[o];
  }
  bool Need(size_t o, size_t n) { return o + n <= s.n ? true : (bad = true, false); }
  V3 RootF(size_t o) { return Need(o, 12) ? V3{LeF(s, o), LeF(s, o + 4), LeF(s, o + 8)} : V3{}; }
  V3 RootS(size_t o) {
    return Need(o, 6) ? V3{double(Le16s(s, o)), double(Le16s(s, o + 2)), double(Le16s(s, o + 4))} : V3{};
  }
  // one bone: its axes (x, y, z order), s16 each
  size_t Bone(size_t o, int t, std::array<int, 3> &v) {
    v = {0, 0, 0};
    const char *ax = t == 2 ? "y" : t == 3 ? "xy" : t == 4 ? "z" : t == 6 ? "yz" : "xyz";
    const size_t n = strlen(ax);
    if (!Need(o, 2 * n))
      return 2 * n;
    for (size_t i = 0; i < n; ++i)
      v[size_t(ax[i] - 'x')] = Le16s(s, o + 2 * i);
    return 2 * n;
  }
};
// -> false (ctx failed) on an unknown sub-record or a first one past the end
inline bool DecodeBody(Span pose, std::vector<BodySub> &subs, Ctx &ctx) {
  subs.clear();
  const auto &types = TypesM1();
  size_t p = 0;
  const size_t n = pose.n;
  while (n && p < n - 1) {
    BodyReader r{pose};
    BodySub s;
    const int op = pose[p];
    s.op = op;
    size_t q;
    auto all = [&](size_t at, uint32_t mask) {
      for (int k = 0; k < 21; ++k)
        if (mask >> k & 1) {
          std::array<int, 3> v;
          at += r.Bone(at, types[size_t(k)], v);
          s.bones.push_back({k, v});
        }
      return at;
    };
    if (op == 0x7e || op == 0x7f) {
      s.has_count = true, s.count = r.U8(p + 1), s.has_root = true;
      s.root = op == 0x7e ? r.RootS(p + 2) : r.RootF(p + 2);
      q = p + (op == 0x7e ? 8 : 14);
    } else if (op == 0x7b || op == 0x76) {
      r.U8(p + 1), s.has_count = true, s.count = r.U8(p + 2);
      if (op == 0x7b)
        s.has_root = true, s.root = r.RootS(p + 3), q = p + 9;
      else
        q = p + 7;
    } else if (op == 0x74) {
      q = p + 1;
      if (r.U8(q++))
        q += 2;
      if (r.U8(q++))
        q += 4;
    } else if (op == 0x7d || op == 0x7c || (op >= 0x60 && op <= 0x6f)) {
      s.has_count = true, s.has_root = true;
      if (op == 0x7d)
        s.count = r.U8(p + 1), s.root = r.RootF(p + 2), q = p + 14;
      else if (op == 0x7c)
        s.count = r.U8(p + 1), s.root = r.RootS(p + 2), q = p + 8;
      else
        s.count = op - 0x60, s.root = r.RootS(p + 1), q = p + 7;
      q = all(q, 0x1FFFFF);
    } else if (op == 0x7a || op == 0x75 || (op >= 0x50 && op <= 0x5f)) {
      s.has_count = true, s.has_root = true;
      if (op == 0x7a)
        s.count = r.U8(p + 1), s.root = r.RootF(p + 2), q = p + 14;
      else if (op == 0x75)
        s.count = r.U8(p + 1), s.root = r.RootS(p + 2), q = p + 8;
      else
        s.count = op - 0x50, s.root = r.RootF(p + 1), q = p + 13;
      const uint32_t m = r.U8(q) | uint32_t(r.U8(q + 1)) << 8 | uint32_t(r.U8(q + 2)) << 16;
      q = all(q + 3, m);
    } else if (op < 25) {
      s.has_count = true, s.count = r.U8(p + 1);
      std::array<int, 3> v;
      q = p + 2 + r.Bone(p + 2, types[size_t(op)], v);
      s.bones.push_back({op, v});
    } else {
      char b[64];
      snprintf(b, sizeof(b), "unknown body sub-record op 0x%02x", op);
      return ctx.Fail(b), false;
    }
    if (r.bad || q > n) {
      if (subs.empty())
        return ctx.Fail("body sub-record overruns the record"), false;
      break;  // (stock quirk: leftover bytes the game reads past)
    }
    subs.push_back(std::move(s));
    p = q;
  }
  return true;
}

// A character motion as absolute keys (ymks.body_keys: the state carried across partial sub-records)
struct BodyKey {
  int frame = 0, count = 0;
  V3 root{};
  std::array<std::array<int, 3>, 21> bones{};
  std::vector<Span> events;
  bool has_pose = false;
};
inline bool BodyKeys(Span data, Span T, std::vector<BodyKey> &out, Ctx &ctx) {
  std::vector<std::pair<int, Span>> recs;
  if (!Records(data, recs, ctx))
    return false;
  out.clear();
  V3 root{0.0, 0.0, 0.0};
  std::array<std::array<int, 3>, 21> bones{};
  std::vector<BodySub> subs;
  for (auto &[f, r] : recs) {
    Span ev, pose;
    SplitRecord(r, ev, pose);
    BodyKey k;
    k.frame = f;
    if (!ev.Empty())
      for (auto &e : ParseEvents(ev, T))
        k.events.push_back(e.b);
    bool has_cnt = false;
    if (!pose.Empty()) {
      if (!DecodeBody(pose, subs, ctx))
        return false;
      for (auto &s : subs) {
        if (s.has_root && s.op != 0x7b)
          root = s.root;
        for (auto &[b, v] : s.bones)
          if (b < 21)
            bones[size_t(b)] = v;
        if (s.has_count)
          k.count = s.count, has_cnt = true;
      }
    }
    if (!has_cnt)
      k.count = 0;
    k.root = root, k.bones = bones, k.has_pose = !pose.Empty();
    out.push_back(std::move(k));
  }
  return true;
}

// ---------------------------------------------------------------------------------------------------- YANM writer
// A block's raw tracks (yanm_write.py). Components in descriptor order (descending flag bit), each a key list
// (3 values + the time step; scale keys hold an absolute frame; hinges 1 value); pos: scale + bias (f32).
enum Kind { kPos8, kRot8, kRot16, kRot8f, kHinge, kHingeF, kScale, kPos16, kRot8ff, kHingeFf, kKinds };
const uint16_t kBits[kKinds] = {0x8000, 0x4000, 0x2000, 0x1000, 0x0800, 0x0400, 0x0200, 0x0100, 0x0010, 0x0008};
inline bool IsHinge(int k) { return k == kHinge || k == kHingeF || k == kHingeFf; }
inline bool IsPos(int k) { return k == kPos8 || k == kPos16; }
inline bool Is8(int k) { return k == kPos8 || k == kRot8 || k == kRot8f || k == kRot8ff || IsHinge(k); }
inline int BodySize(int k) { return IsPos(k) ? 24 : 8; }
struct Key {
  int v[3] = {0, 0, 0};
  int dt = 0;
};
struct Comp {
  bool on = false;
  std::vector<Key> keys;
  double scale = 0;
  V3 bias{};
  int axis = 0;
};
struct RawTrack {
  uint32_t hash = 0;
  uint16_t flags = 0;
  std::array<Comp, kKinds> c;
};
struct Raw {
  int ver = 4;
  std::vector<RawTrack> tracks;
};
struct Cmd {
  int id, type;
  Bytes data;
};
struct Ev {
  int frame;
  std::vector<Cmd> cmds;
};

// A YANM chunk at y -> raw tracks (the component kinds above only)
inline bool ParseRaw(const Bytes &d, size_t y, Raw &out, Ctx &ctx) {
  const size_t base = y + 8;
  auto need = [&](size_t o, size_t n) { return o + n <= d.size() ? true : (ctx.Fail("YANM block out of its record"), false); };
  if (!need(base, 8))
    return false;
  if (d[base + 1] != 0)
    return ctx.Fail("templated YANM block in the base entrance"), false;
  out.ver = d[base];
  const uint16_t cnt = svrfmt::Be16(&d[base + 2]);
  size_t o = base + svrfmt::Be32(&d[base + 4]);
  out.tracks.clear();
  for (uint16_t i = 0; i < cnt; ++i) {
    if (!need(o, 8))
      return false;
    RawTrack t;
    t.flags = svrfmt::Be16(&d[o]), t.hash = svrfmt::Be32(&d[o + 4]);
    const uint16_t sz = svrfmt::Be16(&d[o + 2]);
    size_t b = o + 8;
    for (int k = 0; k < kKinds; ++k) {
      if (!(t.flags & kBits[k]))
        continue;
      Comp &c = t.c[size_t(k)];
      c.on = true;
      if (!need(b, size_t(BodySize(k))))
        return false;
      const uint32_t so = svrfmt::Be32(&d[b]);
      const uint32_t n = IsHinge(k) ? svrfmt::Be16(&d[b + 4]) : svrfmt::Be32(&d[b + 4]);
      if (IsHinge(k))
        c.axis = svrfmt::Be16(&d[b + 6]);
      if (IsPos(k)) {
        c.scale = svrfmt::BeF(&d[b + 8]);
        for (int a = 0; a < 3; ++a)
          c.bias[size_t(a)] = svrfmt::BeF(&d[b + 12 + 4 * size_t(a)]);
      }
      const size_t ksz = IsHinge(k) ? 2 : Is8(k) ? 4 : 8;
      if (!need(base + so, ksz * n))
        return false;
      for (uint32_t j = 0; j < n; ++j) {
        const uint8_t *p = &d[base + so + ksz * j];
        Key key;
        if (IsHinge(k))
          key.v[0] = int8_t(p[0]), key.dt = p[1];
        else if (ksz == 4)
          key.v[0] = int8_t(p[0]), key.v[1] = int8_t(p[1]), key.v[2] = int8_t(p[2]), key.dt = p[3];
        else
          for (int a = 0; a < 3; ++a)
            key.v[a] = int16_t(svrfmt::Be16(p + 2 * a)), key.dt = svrfmt::Be16(p + 6);
        c.keys.push_back(key);
      }
      b += size_t(BodySize(k));
    }
    out.tracks.push_back(std::move(t));
    o += sz;
  }
  return true;
}

// A key list stretched / shrunk so its last key is at `last` (yanm_write.retime): a static one rebuilt with as many
// equal keys as its time step allows, a moving one with its steps scaled.
inline std::vector<Key> Retime(int kind, const std::vector<Key> &ks, int last) {
  const int maxdt = Is8(kind) ? 255 : 65535, nv = IsHinge(kind) ? 1 : 3;
  if (ks.size() < 2)
    return ks;
  auto same = [&](const Key &a, const Key &b) { return std::equal(a.v, a.v + nv, b.v); };
  bool flat = true;
  for (auto &k : ks)
    flat = flat && same(k, ks[0]);
  std::vector<Key> out;
  if (flat) {
    Key k = ks[0];
    k.dt = 0, out.push_back(k);
    for (int t = 0; t < last;) {
      k.dt = std::min(maxdt, last - t), out.push_back(k), t += k.dt;
    }
    return out;
  }
  long long tot = 0;
  for (auto &k : ks)
    tot += k.dt;
  const double f = tot ? last / double(tot) : 1.0;
  double want = 0.0;
  long long acc = 0;
  for (auto k : ks) {
    want += k.dt * f;
    const long long d = Round(want) - acc;
    acc += d;
    k.dt = int(std::max(0LL, std::min<long long>(maxdt, d)));
    out.push_back(k);
  }
  return out;
}
inline void Stretch(Raw &raw, int nframes60) {
  for (auto &t : raw.tracks)
    for (int k = 0; k < kKinds; ++k)
      if (t.c[size_t(k)].on)
        t.c[size_t(k)].keys = Retime(k, t.c[size_t(k)].keys, nframes60 - 1);
}

inline void Put16(Bytes &b, int v) { svrfmt::AppBe16(b, uint16_t(v)); }
inline void Put32(Bytes &b, uint32_t v) { svrfmt::AppBe32(b, v); }
inline void Chunk(Bytes &out, const char *tag, const Bytes &body) {
  out.insert(out.end(), tag, tag + 4);
  Put32(out, uint32_t(body.size()));
  svrfmt::App(out, body);
}
// -> YANM chunk + its POF0 (the stream offset fields' positions from the YANM's base)
inline void Serialize(const Raw &raw, Bytes &out, Ctx &ctx) {
  Bytes desc, streams;
  std::vector<uint32_t> ptrs = {4};
  size_t dsize = 8;
  for (auto &t : raw.tracks) {
    dsize += 8;
    for (int k = 0; k < kKinds; ++k)
      if (t.flags & kBits[k])
        dsize += size_t(BodySize(k));
  }
  auto fits = [&](long long v, long long lo, long long hi) {
    if (v < lo || v > hi)
      ctx.Fail("a key value out of its field");
    return v;
  };
  for (auto &t : raw.tracks) {
    Bytes body;
    for (int k = 0; k < kKinds; ++k) {
      if (!(t.flags & kBits[k]))
        continue;
      const Comp &c = t.c[size_t(k)];
      if (!c.on)
        return ctx.Fail("a track without the component its flags name");
      const uint32_t so = uint32_t(dsize + streams.size());
      ptrs.push_back(uint32_t(8 + desc.size() + 8 + body.size()));
      Put32(body, so);
      if (IsHinge(k))
        Put16(body, int(c.keys.size())), Put16(body, c.axis);
      else
        Put32(body, uint32_t(c.keys.size()));
      if (IsPos(k)) {
        svrfmt::AppBeF(body, float(c.scale));
        for (double v : c.bias)
          svrfmt::AppBeF(body, float(v));
      }
      for (auto &key : c.keys) {
        if (IsHinge(k))
          streams.push_back(uint8_t(fits(key.v[0], -128, 127))), streams.push_back(uint8_t(fits(key.dt, 0, 255)));
        else if (Is8(k)) {
          for (int a = 0; a < 3; ++a)
            streams.push_back(uint8_t(fits(key.v[a], -128, 127)));
          streams.push_back(uint8_t(fits(key.dt, 0, 255)));
        } else {
          for (int a = 0; a < 3; ++a)
            Put16(streams, int(fits(key.v[a], -32768, 32767)));
          Put16(streams, int(fits(key.dt, 0, 65535)));
        }
      }
    }
    Put16(desc, t.flags), Put16(desc, int(8 + body.size())), Put32(desc, t.hash);
    svrfmt::App(desc, body);
  }
  Bytes payload = {uint8_t(raw.ver), 0};
  Put16(payload, int(raw.tracks.size())), Put32(payload, 8);
  svrfmt::App(payload, desc), svrfmt::App(payload, streams);
  Chunk(out, "YANM", payload);
  Chunk(out, "POF0", yanm::ik::detail::PofWrite(ptrs));
}
// YAT2: u16 1, u16 n, u32 12, u32 12 + 8 n, n x {u16 frame, u16 offset, u16 count, u16 0}, commands {u16 id,
// u8 size (4 + data), u8 type, data}; then an empty POF0
inline void Yat2(const std::vector<Ev> &events, Bytes &out, Ctx &ctx) {
  Bytes ent, pay;
  for (auto &e : events) {
    if (e.frame < 0 || e.frame > 65535 || pay.size() > 65535)
      ctx.Fail("an event out of its field");
    Put16(ent, e.frame), Put16(ent, int(pay.size())), Put16(ent, int(e.cmds.size())), Put16(ent, 0);
    for (auto &c : e.cmds) {
      Put16(pay, c.id), pay.push_back(uint8_t(4 + c.data.size())), pay.push_back(uint8_t(c.type));
      svrfmt::App(pay, c.data);
    }
  }
  Bytes body;
  Put16(body, 1), Put16(body, int(events.size())), Put32(body, 12), Put32(body, uint32_t(12 + 8 * events.size()));
  svrfmt::App(body, ent), svrfmt::App(body, pay);
  svrfmt::Pad(body, 4);
  Chunk(out, "YAT2", body);
  Chunk(out, "POF0", {});
}
// A record: the header of a real one of the same kind (0x70 bytes: +0 header size 0x50 + 0x28 n, +4 the blocks'
// size, +0x0C the first block, +0x48 n), the table (0x28 bytes per block: u8 0xFF, u8 0, u16 the block's size in
// 32-byte units - WWE '12's own meaning, ours; the Python tool copied the base's -, 8-character name, +0x24 its
// offset from the first block), the blocks (YANM, POF0, YAT2, POF0; each on a 0x20 boundary).
struct BlockOut {
  std::string name;
  Raw raw;
  std::vector<Ev> events;
};
inline Bytes BuildRecord(const Bytes &head0, const std::vector<BlockOut> &blocks, Ctx &ctx) {
  const size_t n = blocks.size(), first = (0x70 + 0x28 * n + 0x1f) & ~size_t(0x1f);
  Bytes head(head0.begin(), head0.begin() + 0x70), ents, body;
  svrfmt::PutBe32(&head[0], uint32_t(0x50 + 0x28 * n));
  svrfmt::PutBe32(&head[0x48], uint32_t(n));
  svrfmt::PutBe32(&head[0x0c], uint32_t(first));
  for (auto &b : blocks) {
    const size_t rel = body.size();
    Serialize(b.raw, body, ctx);
    Yat2(b.events, body, ctx);
    svrfmt::Pad(body, 0x20);
    Bytes e(0x28, 0);
    e[0] = 0xff;
    const size_t units = (body.size() - rel) / 32;
    if (units > 65535)
      ctx.Fail("a block too big");
    svrfmt::PutBe16(&e[2], uint16_t(units));
    memcpy(&e[4], b.name.data(), std::min<size_t>(8, b.name.size()));
    svrfmt::PutBe32(&e[0x24], uint32_t(rel));
    svrfmt::App(ents, e);
  }
  svrfmt::PutBe32(&head[4], uint32_t(body.size()));
  Bytes out = head;
  svrfmt::App(out, ents);
  out.resize(first, 0);
  svrfmt::App(out, body);
  return out;
}

// A record's header (yanm_decode.parse_header): kind at +0x20, its blocks (name, YANM offset)
struct BlockRef {
  std::string name;
  size_t offset;
};
inline bool Header(const Bytes &d, int &kind, std::vector<BlockRef> &blocks) {
  if (d.size() < 0x70)
    return false;
  kind = svrfmt::Be16(&d[0x20]);
  const uint32_t first = svrfmt::Be32(&d[0x0c]), n = svrfmt::Be32(&d[0x48]);
  if (0x70 + 0x28 * size_t(n) > d.size())
    return false;
  blocks.clear();
  for (uint32_t i = 0; i < n; ++i) {
    const size_t e = 0x70 + 0x28 * size_t(i);
    blocks.push_back({std::string((const char *)&d[e + 4], 8), first + size_t(svrfmt::Be32(&d[e + 0x24]))});
  }
  return true;
}

// ---------------------------------------------------------------------------------------------------- key reduction
// Douglas-Peucker on the slerp path (degrees) / the straight line (per axis), as the Python tools
struct RotKey {
  int t;
  std::array<int, 3> e;
};
struct PosKey {
  int t;
  V3 v;
};
// one key per time (the last given), sorted: Python's sorted(dict(keys).items())
template <class K>
inline std::vector<K> ByTime(const std::vector<K> &ks) {
  std::map<int, K> m;
  for (auto &k : ks)
    m[k.t] = k;
  std::vector<K> out;
  for (auto &[t, k] : m)
    out.push_back(k);
  return out;
}
inline std::vector<RotKey> ReduceRot(const std::vector<RotKey> &in, double tol) {
  const std::vector<RotKey> ks = ByTime(in);
  if (ks.size() < 3)
    return ks;
  std::vector<Q> q;
  for (auto &k : ks)
    q.push_back(QEuler(k.e[0] * kPi16, k.e[1] * kPi16, k.e[2] * kPi16));
  std::set<size_t> keep = {0, ks.size() - 1};
  std::vector<std::pair<size_t, size_t>> stack = {{0, ks.size() - 1}};
  while (!stack.empty()) {
    const auto [i, j] = stack.back();
    stack.pop_back();
    double worst = 0.0;
    size_t wk = 0;
    for (size_t k = i + 1; k < j; ++k) {
      const double a = (ks[k].t - ks[i].t) / double(ks[j].t - ks[i].t);
      const double d = std::fabs(QDot(q[k], QSlerp(q[i], q[j], a)));
      const double err = 2 * std::acos(std::min(1.0, d)) * (180.0 / kPi);
      if (err > worst)
        worst = err, wk = k;
    }
    if (worst > tol) {
      keep.insert(wk);
      stack.push_back({i, wk}), stack.push_back({wk, j});
    }
  }
  std::vector<RotKey> out;
  for (size_t k : keep)
    out.push_back(ks[k]);
  return out;
}
inline std::vector<PosKey> ReducePos(const std::vector<PosKey> &pts, double tol, Ctx &ctx) {
  if (pts.size() < 3)
    return pts;
  std::set<size_t> keep = {0, pts.size() - 1};
  std::vector<std::pair<size_t, size_t>> stack = {{0, pts.size() - 1}};
  while (!stack.empty()) {
    const auto [i, j] = stack.back();
    stack.pop_back();
    double worst = 0.0;
    size_t wk = 0;
    if (j > i + 1 && pts[j].t == pts[i].t)
      return ctx.Fail("two keys at the same time"), pts;
    for (size_t k = i + 1; k < j; ++k) {
      const double a = (pts[k].t - pts[i].t) / double(pts[j].t - pts[i].t);
      double err = 0;
      for (int c = 0; c < 3; ++c)
        err = std::max(err, std::fabs(pts[k].v[size_t(c)] -
                                      (pts[i].v[size_t(c)] + (pts[j].v[size_t(c)] - pts[i].v[size_t(c)]) * a)));
      if (err > worst)
        worst = err, wk = k;
    }
    if (worst > tol) {
      keep.insert(wk);
      stack.push_back({i, wk}), stack.push_back({wk, j});
    }
  }
  std::vector<PosKey> out;
  for (size_t k : keep)
    out.push_back(pts[k]);
  return out;
}
inline std::vector<Key> RotKeys16(const std::vector<RotKey> &ks) {
  std::vector<Key> out;
  int prev = 0;
  for (auto &k : ks) {
    Key key;
    for (int a = 0; a < 3; ++a)
      key.v[a] = k.e[size_t(a)];
    key.dt = k.t - prev, prev = k.t;
    out.push_back(key);
  }
  return out;
}
// pos16 keys from points already in units (deltas of the rounded values)
inline std::vector<Key> PosKeys16(const std::vector<PosKey> &pts, bool clamp) {
  std::vector<Key> out;
  long long acc[3] = {0, 0, 0};
  int prev = 0;
  for (auto &p : pts) {
    Key k;
    for (int a = 0; a < 3; ++a) {
      const long long q = clamp ? Clamp16(p.v[size_t(a)]) : Round(p.v[size_t(a)]);
      k.v[a] = int(q - acc[a]), acc[a] = q;
    }
    k.dt = p.t - prev, prev = p.t;
    out.push_back(k);
  }
  return out;
}

// ---------------------------------------------------------------------------------------------------- events
// SvR 2011 event (op + operands) -> WWE '12 commands (svr11_to_wwe12_events.inverse; types / sizes as the Rock's WWE '12
// record). Not converted: op 64 (face layers: WWE '12's commands carry asset ids), 43-45, 62, 81, 96-99, 108...
enum ObjKind { kChar, kCamera, kProp, kRope, kTurnbuckle, kGlobal };
inline Bytes U32s(std::initializer_list<uint32_t> v) {
  Bytes b;
  for (uint32_t x : v)
    svrfmt::AppLe32(b, x);
  return b;
}
inline bool Inverse(Span ev, ObjKind kind, std::vector<Cmd> &out) {
  const int op = ev[0];
  const Span a = ev.Sub(1);
  auto has = [&](size_t n) { return a.n >= n; };
  auto add = [&](int id, int type, Bytes d) { return out.push_back({id, type, std::move(d)}), true; };
  if (kind == kChar || kind == kCamera || kind == kProp || kind == kRope || kind == kTurnbuckle) {
    if (op >= 100 && op <= 103)
      return add(1, 1, U32s({uint32_t(op - 100)}));
    if (op == 193 && has(2))
      return add(4, 1, {a[0], a[1], 0, 0});
    if (op == 63 && has(4))
      return add(6, 3, U32s({a[0], a[1], uint32_t(a[2] | a[3] << 8)}));
    if (op == 80 && has(1))
      return add(149, 1, U32s({a[0]}));
    if (op == 162 && has(1))
      return add(212, 1, U32s({a[0]}));
    if (op == 137 && has(4))
      return add(208, 4, {a[0], a[1], a[2], a[3]});
    if (op == 171 && has(3))
      return add(215, 3, {a[0], a[1], a[2], 0});
  }
  if (kind == kCamera) {
    if (op == 128 && has(1))
      return add(196, 1, U32s({a[0]}));
    if (op == 58 && has(2))
      return add(200, 2, U32s({a[0], a[1]}));
    if (op == 174 && has(1))
      return add(205, 1, U32s({a[0]}));
    if (op == 113 && has(1))
      return add(192, 2, {a[0], 0, 0, 0});
    if (op == 116 && has(2))
      return add(192, 2, {a[0], a[1], 0, 0});
    if (op == 130 && has(1))
      return add(194, 2, {a[0], 0, 0, 0});
    if (op == 131 && has(2))
      return add(194, 2, {a[0], a[1], 0, 0});
    if (op == 163 && has(1))
      return add(209, 1, U32s({a[0]}));
    if (op == 71 && has(1))
      return add(203, 1, U32s({a[0]}));
    if (op == 127 && has(2))
      return add(204, 2, {a[0], a[1], 0, 0});
    if (op == 165 && has(1))
      return add(201, 1, U32s({a[0]}));
    if (op == 164 && has(1))
      return add(202, 1, U32s({a[0]}));
  }
  if (kind == kProp) {
    if (op == 182)
      return add(165, 0, {});
    if (op == 180 && has(1))
      return add(179, 1, U32s({a[0]}));
  }
  if (kind == kRope || kind == kTurnbuckle) {
    if (op == 160)
      return add(114, 0, {});
    if (op == 161)
      return add(115, 0, {});
  }
  return false;
}
// {2011 frame: [events]} -> [(60 Hz frame 2 f, commands)]
inline std::vector<Ev> ConvertEvents(const std::map<int, std::vector<Span>> &fe, ObjKind kind) {
  std::vector<Ev> out;
  for (auto &[f, lst] : fe) {
    Ev e{2 * f, {}};
    for (auto &ev : lst)
      if (!ev.Empty())
        Inverse(ev, kind, e.cmds);
    if (!e.cmds.empty())
      out.push_back(std::move(e));
  }
  return out;
}
// a record's readable events (op known), by frame
inline void FrameEvents(int f, Span ev, Span T, std::map<int, std::vector<Span>> &fe) {
  if (ev.Empty())
    return;
  auto &lst = fe[f];
  for (auto &e : ParseEvents(ev, T))
    if (e.op >= 0)
      lst.push_back(e.b);
}
// the global object's events: marks (op 1) -> 87 at frame 0 + 256 at each; op 137 -> 208 (as on the character)
inline std::vector<Ev> GlobalEvents(const std::map<int, std::vector<Span>> &fe, bool with208) {
  std::vector<int> marks;
  for (auto &[f, lst] : fe)
    for (auto &e : lst)
      if (e[0] == 1) {
        marks.push_back(f);
        break;
      }
  std::vector<Ev> ev;
  if (!marks.empty())
    ev.push_back({0, {{87, 0, {}}}});
  for (int f : marks)
    ev.push_back({2 * f, {{256, 0, {}}}});
  if (with208) {
    for (auto &[f, lst] : fe)
      for (auto &e : lst)
        if (e[0] == 137 && e.n >= 5)
          ev.push_back({2 * f, {{208, 4, {e[1], e[2], e[3], e[4]}}}});
    std::stable_sort(ev.begin(), ev.end(), [](const Ev &a, const Ev &b) { return a.frame < b.frame; });
  }
  return ev;
}

// ---------------------------------------------------------------------------------------------------- the body
// SvR 2011 pose -> WWE '12 bone rotations (svr11_to_wwe12_pose.Inverse, the inverse of Kaiko's w13_retarget):
// per mapped bone R13_world = R11_world x Wbind11^T x Wbind13; the whole root rotation on 'Bip' (pelvis at its
// bind); unmapped nodes at their bind; local = parent_world^T x world. Pelvis position (cm) = 10 x the 2011 root.
struct Skeletons {
  // SvR 2011 (motion bind = REST11)
  std::vector<M4> local11, wbind11;
  std::vector<int> mot;  // node -> YMKs bone (-1)
  std::vector<M3> b11;   // rotm(REST11) per node with a bone
  // WWE '13
  std::vector<M4> wbind13;
  std::vector<int> map13;  // node -> its SvR 2011 node (-1)
  int ip = 0, ib = 0;
  // the hands: per side, B (2011 bind) and C (bind frame difference) per joint
  M3 hb[2][5][3], hc[2][5][3];
  Skeletons() {
    const auto &n11 = Nodes11();
    const auto &n13 = Nodes13();
    auto idx = [](const std::vector<Node> &n, const char *name) {
      for (size_t i = 0; i < n.size(); ++i)
        if (!strcmp(n[i].name, name))
          return int(i);
      return -1;
    };
    mot.assign(n11.size(), -1), b11.assign(n11.size(), Eye3());
    for (size_t k = 0; k < Bones11().size(); ++k) {
      const int i = idx(n11, Bones11()[k].name);
      mot[size_t(i)] = int(k), b11[size_t(i)] = Rotm(Bones11()[k].rest);
    }
    for (size_t i = 0; i < n11.size(); ++i) {
      local11.push_back(Trs(Rotm(n11[i].r), n11[i].t));
      wbind11.push_back(n11[i].parent < 0 ? local11[i] : Mul4(wbind11[size_t(n11[i].parent)], local11[i]));
    }
    for (size_t i = 0; i < n13.size(); ++i) {
      const M4 m = Trs(Rotm(n13[i].r), n13[i].t);
      wbind13.push_back(n13[i].parent < 0 ? m : Mul4(wbind13[size_t(n13[i].parent)], m));
    }
    map13.assign(n13.size(), -1);
    for (size_t j = 0; j < n11.size(); ++j)
      for (auto &b : Bones11())
        if (!strcmp(b.name, n11[j].name))
          map13[size_t(idx(n13, b.w12))] = int(j);
    ip = idx(n13, "Bip Pelvis"), ib = idx(n13, "Bip");
    for (int s = 0; s < 2; ++s) {
      const M3 &A = HandA(s);
      for (int f = 0; f < 5; ++f) {
        M3 r13 = Eye3(), r11 = Eye3();
        for (int j = 0; j < 3; ++j) {
          r13 = Mul3(r13, Rotm(Finger13R(s, f, j))), r11 = Mul3(r11, Rotm(Finger11R(s, f, j)));
          hc[s][f][j] = Mul3(Tr(Mul3(A, r13)), r11);
          hb[s][f][j] = Rotm(Finger11R(s, f, j));
        }
      }
    }
  }
  static const Skeletons &Get() {
    static const Skeletons s;
    return s;
  }
  // root raw (0.01 units), the 21 bones' s16 angles -> local rotations of WWE '13 nodes (all), the Bip position (cm)
  void Solve(const V3 &root, const std::array<std::array<int, 3>, 21> &ang, std::vector<M3> &loc, V3 &pos) const {
    const auto &n11 = Nodes11();
    const auto &n13 = Nodes13();
    std::vector<M4> W(n11.size());
    for (size_t i = 0; i < n11.size(); ++i) {
      M4 L;
      const int k = mot[i];
      if (k < 0)
        L = local11[i];
      else {
        const auto &a = ang[size_t(k)];
        const M3 R = Mul3(b11[i], Rotm(a[0] * kAng, a[1] * kAng, a[2] * kAng));
        V3 t = n11[i].t;
        if (k == 0)
          for (int c = 0; c < 3; ++c)
            t[size_t(c)] = root[size_t(c)] * kRootUnit;
        L = Trs(R, t);
      }
      W[i] = n11[i].parent < 0 ? L : Mul4(W[size_t(n11[i].parent)], L);
    }
    auto target = [&](size_t i13) {
      const size_t j = size_t(map13[i13]);
      return Mul3(Mul3(Rot3(W[j]), Tr(Rot3(wbind11[j]))), Rot3(wbind13[i13]));
    };
    std::vector<M3> Rw(n13.size());
    loc.assign(n13.size(), Eye3());
    const M3 Rl_p = Rotm(n13[size_t(ip)].r), tgt_p = target(size_t(ip));
    for (size_t i = 0; i < n13.size(); ++i) {
      const M3 Rp = n13[i].parent < 0 ? Eye3() : Rw[size_t(n13[i].parent)];
      if (int(i) == ib) {
        Rw[i] = Mul3(tgt_p, Tr(Rl_p)), loc[i] = Mul3(Tr(Rp), Mul3(tgt_p, Tr(Rl_p)));
      } else if (int(i) == ip) {
        Rw[i] = Mul3(Rp, Rl_p), loc[i] = Rl_p;
      } else if (map13[i] >= 0) {
        const M3 tgt = target(i);
        Rw[i] = tgt, loc[i] = Mul3(Tr(Rp), tgt);
      } else {
        const M3 Rl = Rotm(n13[i].r);
        Rw[i] = Mul3(Rp, Rl), loc[i] = Rl;
      }
    }
    for (int c = 0; c < 3; ++c)
      pos[size_t(c)] = 10.0 * (root[size_t(c)] * kRootUnit) - n13[size_t(ib)].t[size_t(c)];
  }
};
inline std::array<int, 3> Eul16(const M3 &M) {
  const V3 e = EulerZyx(M);
  return {Wrap16(e[0] / kPi16), Wrap16(e[1] / kPi16), Wrap16(e[2] / kPi16)};
}

// The hands (svr11_to_wwe12_fingers, the inverse of finger_retarget): SvR 2011 y2 (right) / y3 (left) records of
// 5-byte sub-records [k][count][s8 x][s8 y][s8 z] (k 0-14 left, 15-29 right; a record lists only the joints that
// change; angle = s8 x 1.40625 degrees on the joint's bind). Per finger: P11_j = P11_(j-1) B_j R_j,
// P13_j = A^T P11_j C_j^T, local13_j = P13_(j-1)^T P13_j.
inline std::string FingerName(int side, int f, int j) {
  std::string s = std::string("Bip ") + (side ? "R" : "L") + " Finger" + char('0' + f);
  if (j)
    s += char('0' + j);
  return s;
}
// -> {WWE track hash: reduced keys}
inline bool FingerKeys(Span data, int y, std::map<uint32_t, std::vector<RotKey>> &out, Ctx &ctx) {
  const Skeletons &S = Skeletons::Get();
  const int side = y == 2 ? 1 : 0, kb = y == 2 ? 15 : 0;
  std::vector<std::pair<int, Span>> recs;
  if (!Records(data, recs, ctx))
    return false;
  std::map<int, std::array<int, 3>> cur;
  std::map<std::string, std::vector<RotKey>> raw;
  std::vector<std::string> order;
  for (auto &[f, r] : recs) {
    Span ev, pose;
    SplitRecord(r, ev, pose);
    if (pose.Empty())
      continue;
    int cnt = 0;
    for (size_t i = 0; i < pose.n / 5; ++i) {
      cur[pose[5 * i]] = {int8_t(pose[5 * i + 2]), int8_t(pose[5 * i + 3]), int8_t(pose[5 * i + 4])};
      cnt = std::max(cnt, int(pose[5 * i + 1]));
    }
    const int t_hold = std::max(0, 2 * (f - 1)), t_tgt = 2 * (f + cnt);  // (the body keys' one-frame lag)
    for (int F = 0; F < 5; ++F) {
      M3 P11 = Eye3(), P13p = Eye3();
      for (int J = 0; J < 3; ++J) {
        auto it = cur.find(kb + 3 * F + J);
        const std::array<int, 3> a = it == cur.end() ? std::array<int, 3>{0, 0, 0} : it->second;
        P11 = Mul3(P11, Mul3(S.hb[side][F][J], Rotm(a[0] * kStep, a[1] * kStep, a[2] * kStep)));
        const M3 P13 = Mul3(Mul3(Tr(HandA(side)), P11), Tr(S.hc[side][F][J]));
        const auto e = Eul16(Mul3(Tr(P13p), P13));
        P13p = P13;
        const std::string nm = FingerName(side, F, J);
        auto &lst = raw[nm];
        if (lst.empty() && std::find(order.begin(), order.end(), nm) == order.end())
          order.push_back(nm);
        if (cnt > 0 && !lst.empty() && lst.back().t < t_hold)
          lst.push_back({t_hold, lst.back().e});
        lst.push_back({t_tgt, e});
      }
    }
  }
  for (auto &nm : order)
    out[yanm::NameHash(nm)] = ReduceRot(ByTime(raw[nm]), 0.8);
  return true;
}

// The body template: WWE '12's body track list, from the base entrance's entrant (record 0, its first block): per track
// its hash, its first pose as rot16 (the rotation components' first keys combined), its scale keys, its pos16
// (scale, bias, first key). (LA Max's wwe12_body_template.json, made from the Rock's entrance 100.)
struct TemplateTrack {
  uint32_t hash = 0;
  std::array<int, 3> rot{};
  std::vector<Key> scale;
  bool has_pos = false;
  double pscale = 0;
  V3 bias{};
  std::array<int, 3> first{};
};
inline bool BodyTemplate(const Bytes &rec0, std::vector<TemplateTrack> &out, Ctx &ctx) {
  int kind;
  std::vector<BlockRef> blocks;
  if (!Header(rec0, kind, blocks) || blocks.empty())
    return ctx.Fail("the base entrance has no entrant record"), false;
  Raw raw;
  if (!ParseRaw(rec0, blocks[0].offset, raw, ctx))
    return false;
  out.clear();
  for (auto &t : raw.tracks) {
    TemplateTrack tt;
    tt.hash = t.hash;
    // rot16, rot8 x rot8f x rot8ff, or the hinges: the first keys' rotations multiplied in the game's order
    M3 M = Eye3();
    const struct {
      int kind;
      double unit;
    } parts[] = {{kRot16, kPi / 32768}, {kRot8, kPi / 128}, {kRot8f, kPi / 4608}, {kRot8ff, kPi / 11520},
                 {kHinge, kPi / 128},   {kHingeF, kPi / 4608}, {kHingeFf, kPi / 11520}};
    for (auto &p : parts) {
      const Comp &c = t.c[size_t(p.kind)];
      if (!c.on || c.keys.empty())
        continue;
      V3 a{};
      if (IsHinge(p.kind))
        a[size_t(std::min(2, c.axis))] = c.keys[0].v[0] * p.unit;
      else
        a = {c.keys[0].v[0] * p.unit, c.keys[0].v[1] * p.unit, c.keys[0].v[2] * p.unit};
      M = Mul3(M, Rotm(a));
    }
    const V3 e = EulerZyx(M);
    for (int a = 0; a < 3; ++a)
      tt.rot[size_t(a)] = int(Round(e[size_t(a)] / kPi16));
    if (t.c[kScale].on)
      tt.scale = t.c[kScale].keys;
    if (t.c[kPos16].on) {
      const Comp &c = t.c[kPos16];
      tt.has_pos = true, tt.pscale = c.scale, tt.bias = c.bias;
      if (!c.keys.empty())
        tt.first = {c.keys[0].v[0], c.keys[0].v[1], c.keys[0].v[2]};
    }
    out.push_back(std::move(tt));
  }
  return true;
}

// One body block (svr11_to_wwe12_body.build_tracks): the 21 bones + Bip as rot16 keys (reduced to 0.35 degrees),
// Bip's position as pos16 keys, the fingers' keys; every other track the template's first pose held.
// Keys: a 2011 key at frame f (count c) starts blending at f and is reached at f + c + 1: the game shows it one frame
// (2 WWE frames) after the WWE pose it was sampled from, so WWE time 2 (f + c), the old pose held from 2 (f - 1).
inline bool BodyBlock(const std::vector<TemplateTrack> &tpl, const std::vector<BodyKey> &keys, int nframes60,
                      const std::map<uint32_t, std::vector<RotKey>> &xr, Raw &out, Ctx &ctx) {
  const Skeletons &S = Skeletons::Get();
  const auto &n13 = Nodes13();
  struct PK {
    int t;
    const BodyKey *k;
  };
  std::vector<PK> pk;
  int prev_t = -1;
  bool have_prev = false;
  for (auto &k : keys) {
    if (!k.has_pose)
      continue;
    const int f = k.frame, c = k.count;
    const int t_hold = std::max(0, 2 * (f - 1)), t_tgt = 2 * (f + c);
    if (have_prev && t_hold > prev_t && c > 0)
      pk.push_back({t_hold, pk.back().k});  // the old pose held until the blend starts
    pk.push_back({t_tgt, &k});
    prev_t = t_tgt, have_prev = true;
  }
  if (pk.empty())
    return ctx.Fail("an entrant body track without a pose"), false;
  if (pk[0].t != 0)
    pk.insert(pk.begin(), {0, pk[0].k});
  const int last = nframes60 - 1;
  if (pk.back().t < last)
    pk.push_back({last, pk.back().k});
  std::map<uint32_t, std::vector<RotKey>> rot;
  std::vector<PosKey> bip;
  std::vector<M3> loc;
  V3 pos;
  for (auto &p : pk) {
    S.Solve(p.k->root, p.k->bones, loc, pos);
    for (size_t i = 0; i < n13.size(); ++i)
      if (S.map13[i] >= 0 || int(i) == S.ib)
        rot[yanm::NameHash(n13[i].name)].push_back({p.t, Eul16(loc[i])});
    bip.push_back({p.t, pos});
  }
  const uint32_t hbip = yanm::NameHash("Bip");
  out.ver = 4, out.tracks.clear();
  for (auto &tt : tpl) {
    RawTrack t;
    t.hash = tt.hash;
    t.flags = tt.scale.empty() ? 0x2000 : 0x2200;
    std::vector<RotKey> ks;
    if (auto it = xr.find(tt.hash); it != xr.end() && !it->second.empty()) {
      ks = it->second;
      if (ks[0].t > 0)
        ks.insert(ks.begin(), {0, ks[0].e});
      if (ks.back().t < last)
        ks.push_back({last, ks.back().e});
    } else if (auto ir = rot.find(tt.hash); ir != rot.end()) {
      ks = ReduceRot(ir->second, 0.35);
    } else {
      const std::array<int, 3> e = {Wrap16(tt.rot[0]), Wrap16(tt.rot[1]), Wrap16(tt.rot[2])};
      ks = {{0, e}, {last, e}};
    }
    t.c[kRot16].on = true, t.c[kRot16].keys = RotKeys16(ks);
    if (!tt.scale.empty()) {
      t.c[kScale].on = true, t.c[kScale].keys = tt.scale;
      t.c[kScale].keys.back().dt = last;  // (scale keys hold absolute frames)
    }
    if (tt.has_pos) {
      t.flags |= 0x0100;
      Comp &c = t.c[kPos16];
      c.on = true;
      std::vector<PosKey> pts;
      if (tt.hash == hbip) {
        // bias = the first position; the scale keeps every delta inside an s16
        c.bias = bip[0].v;
        double span = 0;
        bool firstv = true;
        for (auto &b : bip)
          for (int a = 0; a < 3; ++a) {
            const double d = std::fabs(b.v[size_t(a)] - c.bias[size_t(a)]);
            if (firstv || d > span)
              span = d, firstv = false;
          }
        c.scale = std::max(0.05, span * 2.0 / 30000.0);
        for (auto &b : ReducePos(bip, 0.2, ctx)) {
          PosKey q{b.t, {}};
          for (int a = 0; a < 3; ++a)
            q.v[size_t(a)] = (b.v[size_t(a)] - c.bias[size_t(a)]) / c.scale;
          pts.push_back(q);
        }
      } else {
        c.scale = tt.pscale, c.bias = tt.bias;
        const V3 f{double(tt.first[0]), double(tt.first[1]), double(tt.first[2])};
        pts = {{0, f}, {last, f}};
      }
      c.keys = PosKeys16(pts, true);
    }
    out.tracks.push_back(std::move(t));
  }
  return ctx.ok;
}

// ---------------------------------------------------------------------------------------------------- the camera
// SvR 2011 camera keys (29 bytes: count, f32 position x3 (10 cm units x 0.01... raw), f32 Euler x3, f32 fov) -> a
// WWE '12 camera block (svr11_to_wwe12_camera, the inverse of Kaiko's wwe13_entrance.camera_track), three tracks:
//   eye    #5edfdf5f  rot16 (R13 = R11 x CAM_M^T) + scale + pos16 (the eye, cm)
//   target #70d146a5  rot16 (0) + scale + pos16 (eye + R13 column 1 x distance)
//   fov    #70d304a4  pos16 (v, 2 atan(tan(v / 2) 4 / 3), 2 atan(tan(v / 2) 0.8))
// The distance, not in SvR 2011: 0.92 x the one to the entrant's pelvis (WWE '12's cameras aim at the wrestler).
// A cut (60 cm, 20 degrees or 0.15 rad of fov in one 2011 frame) splits the rotation key reduction.
struct CamKey {
  int f, c;
  V3 eye;
  M3 R13;
  double fov;
};
inline const M3 &CamMT() {
  static const M3 m = {0, -1, 0, 0, 0, -1, 1, 0, 0};  // CAM_M^T
  return m;
}
inline bool CamKeys(Span data, Span T, std::vector<CamKey> &keys, std::map<int, std::vector<Span>> &fe, Ctx &ctx) {
  std::vector<std::pair<int, Span>> recs;
  if (!Records(data, recs, ctx))
    return false;
  for (auto &[f, r] : recs) {
    Span ev, pose;
    SplitRecord(r, ev, pose);
    FrameEvents(f, ev, T, fe);
    if (pose.n != 29)
      continue;
    CamKey k;
    k.f = f, k.c = pose[0];
    for (int a = 0; a < 3; ++a)
      k.eye[size_t(a)] = double(LeF(pose, 1 + 4 * size_t(a))) / 10.0;
    k.R13 = Mul3(Rotm(LeF(pose, 13), LeF(pose, 17), LeF(pose, 21)), CamMT());
    k.fov = LeF(pose, 25);
    keys.push_back(k);
  }
  return true;
}
inline double Norm3(const V3 &v) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
// [(t, vec)] -> pos16 (one key per time, reduced, scale so every delta fits an s16)
inline Comp Quant(const std::vector<PosKey> &in, double tol, double min_scale, bool with_bias, Ctx &ctx) {
  const std::vector<PosKey> pts = ReducePos(ByTime(in), tol, ctx);
  Comp c;
  c.on = true;
  if (with_bias)
    c.bias = pts[0].v;
  double dmax = 0;
  bool firstv = true;
  for (size_t i = 0; i + 1 < pts.size(); ++i)
    for (int a = 0; a < 3; ++a) {
      const double d = std::fabs(pts[i + 1].v[size_t(a)] - pts[i].v[size_t(a)]);
      if (firstv || d > dmax)
        dmax = d, firstv = false;
    }
  if (firstv || 1e-9 > dmax)
    dmax = 1e-9;
  c.scale = std::max(dmax / 30000.0, min_scale);
  std::vector<PosKey> q;
  for (auto &p : pts) {
    PosKey k{p.t, {}};
    for (int a = 0; a < 3; ++a)
      k.v[size_t(a)] = (p.v[size_t(a)] - c.bias[size_t(a)]) / c.scale;
    q.push_back(k);
  }
  c.keys = PosKeys16(q, false);
  return c;
}
inline bool CameraBlock(const std::vector<CamKey> &keys, int nframes60, const std::map<int, V3> &pelvis, Raw &out,
                        Ctx &ctx) {
  const int last = nframes60 - 1;
  std::vector<PosKey> eye_p, tg_p, fov_p;
  std::vector<RotKey> rot_k;
  std::set<int> cuts;
  struct Held {
    int t;
    V3 eye, tg, fv;
    std::array<int, 3> e16;
    M3 R13;
    double v;
  };
  bool have = false;
  Held held{};
  int prev_t = 0;
  for (auto &k : keys) {
    const int t_hold = std::max(0, 2 * (k.f - 1)), t_tgt = 2 * (k.f + k.c);
    double dist = 300.0;
    if (!pelvis.empty()) {
      auto it = pelvis.upper_bound(k.f);  // the pelvis at the key's frame, or the last one before
      if (it == pelvis.begin())
        return ctx.Fail("a camera key before the entrant's first"), false;
      const V3 &pl = std::prev(it)->second;
      dist = std::max(30.0, 0.92 * Norm3({pl[0] - k.eye[0], pl[1] - k.eye[1], pl[2] - k.eye[2]}));
    }
    V3 tg;
    for (int a = 0; a < 3; ++a)
      tg[size_t(a)] = k.eye[size_t(a)] + k.R13[3 * size_t(a) + 1] * dist;
    const auto e16 = Eul16(k.R13);
    const double v = k.fov, h = std::tan(v / 2);
    const V3 fv{v, 2 * std::atan(h * 4 / 3), 2 * std::atan(h * 0.8)};
    if (have && k.c > 0 && t_hold > prev_t) {
      eye_p.push_back({t_hold, held.eye}), tg_p.push_back({t_hold, held.tg}), fov_p.push_back({t_hold, held.fv});
      rot_k.push_back({t_hold, held.e16});
    }
    if (have) {
      // a camera cut: the real data crosses it in 2 WWE frames; kept apart in the reduction
      const double d = Norm3({k.eye[0] - held.eye[0], k.eye[1] - held.eye[1], k.eye[2] - held.eye[2]});
      const M3 P = Mul3(Tr(held.R13), k.R13);
      const double cs = (P[0] + P[4] + P[8] - 1) / 2;
      const double ang = std::acos(std::max(-1.0, std::min(1.0, cs))) * (180.0 / kPi);
      if (d > 60.0 || ang > 20.0 || std::fabs(v - held.v) > 0.15)
        cuts.insert(t_tgt);
    }
    eye_p.push_back({t_tgt, k.eye}), tg_p.push_back({t_tgt, tg}), fov_p.push_back({t_tgt, fv});
    rot_k.push_back({t_tgt, e16});
    prev_t = t_tgt;
    held = {t_tgt, k.eye, tg, fv, e16, k.R13, v}, have = true;
  }
  if (rot_k.empty() || rot_k[0].t > 0)
    return ctx.Fail("a camera track without a key at frame 0"), false;
  for (auto *l : {&eye_p, &tg_p, &fov_p})
    if (l->back().t < last)
      l->push_back({last, l->back().v});
  if (rot_k.back().t < last)
    rot_k.push_back({last, rot_k.back().e});
  // rotation keys reduced between cuts
  std::vector<RotKey> rk, seg;
  for (auto &k : ByTime(rot_k)) {
    if (cuts.count(k.t) && !seg.empty()) {
      for (auto &r : ReduceRot(seg, 0.1))
        rk.push_back(r);
      seg.clear();
    }
    seg.push_back(k);
  }
  for (auto &r : ReduceRot(seg, 0.1))
    rk.push_back(r);
  Comp scale;
  scale.on = true, scale.keys = {{{256, 256, 256}, 0}, {{256, 256, 256}, last}};
  Comp zero;
  zero.on = true, zero.keys = {{{0, 0, 0}, 0}, {{0, 0, 0}, last}};
  out.ver = 4, out.tracks.clear();
  RawTrack eye, tgt, fov;
  eye.hash = 0x5edfdf5f, eye.flags = 0x2300;
  eye.c[kRot16].on = true, eye.c[kRot16].keys = RotKeys16(rk), eye.c[kScale] = scale;
  eye.c[kPos16] = Quant(eye_p, 0.15, 1e-7, true, ctx);
  tgt.hash = 0x70d146a5, tgt.flags = 0x2300;
  tgt.c[kRot16] = zero, tgt.c[kScale] = scale, tgt.c[kPos16] = Quant(tg_p, 0.3, 1e-7, true, ctx);
  fov.hash = 0x70d304a4, fov.flags = 0x0100;
  fov.c[kPos16] = Quant(fov_p, 0.0004, 1e-7, true, ctx);
  out.tracks = {eye, tgt, fov};
  return ctx.ok;
}

// ---------------------------------------------------------------------------------------------------- ropes
// SvR 2011 rope / turnbuckle keys (5 bytes: count, s16 y, s16 z; 10 x cm) -> the base block's rope track (#0e5bf865)
// pos16 (x 0, y, z in cm); its other tracks the base's, re-timed.
inline bool RopeBlock(const Raw &base, Span data, Span T, int nframes60, Raw &out,
                      std::map<int, std::vector<Span>> &fe, Ctx &ctx) {
  std::vector<std::pair<int, Span>> recs;
  if (!Records(data, recs, ctx))
    return false;
  std::vector<PosKey> pts;
  for (auto &[f, r] : recs) {
    Span ev, pose;
    SplitRecord(r, ev, pose);
    FrameEvents(f, ev, T, fe);
    if (pose.n == 5)
      pts.push_back({2 * (f + pose[0]), {0.0, Le16s(pose, 1) / 10.0, Le16s(pose, 3) / 10.0}});
  }
  const int last = nframes60 - 1;
  out = base;
  for (auto &t : out.tracks)
    for (int k = 0; k < kKinds; ++k) {
      Comp &c = t.c[size_t(k)];
      if (!c.on)
        continue;
      if (!(t.hash == 0x0e5bf865 && k == kPos16)) {
        c.keys = Retime(k, c.keys, last);
        continue;
      }
      std::vector<PosKey> p = pts;
      if (p.empty() || p[0].t > 0)
        p.insert(p.begin(), {0, {0.0, 0.0, 0.0}});
      if (p.back().t < last)
        p.push_back({last, p.back().v});
      c = Quant(p, 0.05, 1e-6, false, ctx);
    }
  return ctx.ok;
}

// ---------------------------------------------------------------------------------------------------- lights
// (ours) The global object's tracks are SvR 2011's lights, 25-byte keys: count, f32 position x3, s16 Euler x3, u16
// colour x3 (/ 4096). Each becomes a WWE '12 light block: the light track (rot8 + scale + pos16) and its colour track
// (pos16). Kind by the track: x % 50 == 10 spot, y >= 50 far, y < 10 omni, else spot (positions / 100, far / 10).
// The rotation: R11 x B_M as rot8, of the two Euler solutions the one with z nearest 0 (WWE '12's way).
inline std::array<int, 3> LightRot8(int rx, int ry, int rz) {
  static const M3 bm = {0, -1, 0, 0, 0, -1, 1, 0, 0};
  const M3 M = Mul3(Rotm(rx * kPi / 32768, ry * kPi / 32768, rz * kPi / 32768), bm);
  const V3 e = QToEuler(MatToQ(M));
  auto w = [](double v) { return int(PyMod(Round(v * 128 / kPi) + 128, 256) - 128); };
  const std::array<int, 3> a = {w(e[0]), w(e[1]), w(e[2])}, b = {w(e[0] + kPi), w(kPi - e[1]), w(e[2] + kPi)};
  auto key = [](const std::array<int, 3> &x) { return std::make_pair(std::abs(x[2]), std::abs(x[0]) + std::abs(x[1]) + std::abs(x[2])); };
  return key(b) < key(a) ? b : a;
}
inline Comp Q16(const std::vector<PosKey> &in, int last, Ctx &ctx) {
  std::vector<PosKey> pts = ByTime(in);
  if (pts[0].t > 0)
    pts.insert(pts.begin(), {0, pts[0].v});
  if (pts.back().t < last)
    pts.push_back({last, pts.back().v});
  pts = ReducePos(pts, 0.002, ctx);
  Comp c;
  c.on = true, c.bias = pts[0].v;
  double dmax = 0.0;
  for (size_t i = 0; i + 1 < pts.size(); ++i)
    for (int a = 0; a < 3; ++a)
      dmax = std::max(dmax, std::fabs(pts[i + 1].v[size_t(a)] - pts[i].v[size_t(a)]));
  c.scale = dmax > 0 ? dmax / 30000.0 : 0.0;
  std::vector<PosKey> q;
  for (auto &p : pts) {
    PosKey k{p.t, {}};
    for (int a = 0; a < 3; ++a)
      k.v[size_t(a)] = c.scale != 0.0 ? (p.v[size_t(a)] - c.bias[size_t(a)]) / c.scale : 0.0;
    q.push_back(k);
  }
  c.keys = PosKeys16(q, false);
  return c;
}
// false: no light key in the track
inline bool LightBlock(Span data, Span T, int x, int y, int nframes60, BlockOut &out, Ctx &ctx) {
  const int last = nframes60 - 1;
  uint32_t hl = 0x5cf4bfa7, hc = 0x72f8c451;  // spot
  double div = 100.0;
  if (x % 50 != 10 && y >= 50)
    hl = 0x0dfd1aae, hc = 0xe225a29f, div = 10.0;  // far
  else if (x % 50 != 10 && y < 10)
    hl = 0x3d7e5834, hc = 0x5ea1e944;  // omni
  std::vector<std::pair<int, Span>> recs;
  if (!Records(data, recs, ctx))
    return false;
  std::map<int, std::vector<Span>> fe;
  std::vector<PosKey> pos, col;
  std::vector<RotKey> rot;
  for (auto &[f, r] : recs) {
    Span ev, pose;
    SplitRecord(r, ev, pose);
    FrameEvents(f, ev, T, fe);
    if (pose.n != 25)
      continue;
    const int t = f ? 2 * (f + pose[0]) : 0;
    PosKey p{t, {}}, c{t, {}};
    for (int a = 0; a < 3; ++a) {
      p.v[size_t(a)] = double(LeF(pose, 1 + 4 * size_t(a))) / div;
      c.v[size_t(a)] = svrfmt::Le16(pose.p + 19 + 2 * a) / 4096.0;
    }
    pos.push_back(p), col.push_back(c);
    rot.push_back({t, LightRot8(Le16s(pose, 13), Le16s(pose, 15), Le16s(pose, 17))});
  }
  if (pos.empty())
    return false;
  std::vector<RotKey> rk = ByTime(rot);
  if (rk[0].t > 0)
    rk.insert(rk.begin(), {0, rk[0].e});
  if (rk.back().t < last)
    rk.push_back({last, rk.back().e});
  // constant runs dropped, time steps <= 255
  std::vector<Key> r8;
  int prev = 0;
  for (size_t i = 0; i < rk.size(); ++i) {
    if (i > 0 && i + 1 < rk.size() && rk[i].e == rk[i - 1].e && rk[i].e == rk[i + 1].e)
      continue;
    while (rk[i].t - prev > 255) {
      Key k = r8.back();
      k.dt = 255, r8.push_back(k), prev += 255;
    }
    r8.push_back({{rk[i].e[0], rk[i].e[1], rk[i].e[2]}, rk[i].t - prev}), prev = rk[i].t;
  }
  Comp sc;
  sc.on = true, sc.keys = {{{256, 256, 256}, 0}};
  for (int t = 0; t < last;) {
    const int d = std::min(65535, last - t);
    sc.keys.push_back({{256, 256, 256}, d}), t += d;
  }
  RawTrack L, C;
  L.hash = hl, L.flags = 0x4300;
  L.c[kRot8].on = true, L.c[kRot8].keys = r8, L.c[kScale] = sc, L.c[kPos16] = Q16(pos, last, ctx);
  C.hash = hc, C.flags = 0x0100, C.c[kPos16] = Q16(col, last, ctx);
  out.raw.ver = 4, out.raw.tracks = {L, C};
  // (with its effects' op 137 -> 208: WWE '12's Jeff Hardy 535 fires its pyro so, from the light track 50; left out
  // at first, SvR 2010 Jeff Hardy had no pyro, 2026-11-02)
  out.events = GlobalEvents(fe, true);
  return true;
}

// ---------------------------------------------------------------------------------------------------- the script
// EVP ("0FOP", big-endian, SvR 2011 layout = WWE '12's): +0x20 u16 number, +0x22 the name (30 bytes), +0x40 u16
// objects, +0x48 the object table (32 bytes: u32 index, u32 model, u32 type x2, u32 x2, u16 slots, u16, u32 the slots'
// offset), the slots (212 bytes each), an optional tail (+0x1C), a POF0 (offsets from +8).
struct EvpObj {
  uint32_t idx, model, t0, t1, w4, w5, nslots, lo;
  std::vector<Bytes> slots;
};
struct EvpItem {
  uint32_t k, low, val;
};
struct EvpRec {
  uint32_t typ, z, v, m;
  std::vector<EvpItem> items;
};
struct Evp {
  uint32_t num = 0, f10 = 0, f18 = 0, f44 = 0, n_hi = 0;
  Bytes name;
  std::vector<EvpObj> objs;
  bool has_tail = false;
  uint32_t tail_num = 0;
  std::vector<EvpRec> recs;
};
inline bool ParseEvp(const Bytes &d, Evp &e) {
  auto u32 = [&](size_t o) { return o + 4 <= d.size() ? svrfmt::Be32(&d[o]) : 0u; };
  auto u16 = [&](size_t o) { return o + 2 <= d.size() ? uint32_t(svrfmt::Be16(&d[o])) : 0u; };
  if (d.size() < 0x4C || memcmp(d.data(), "0FOP", 4) || d[8] != 'E' || d[9] != 'V')
    return false;
  e.num = u16(0x20), e.name.assign(d.begin() + 0x22, d.begin() + 0x40);
  e.f10 = u32(0x10), e.f18 = u32(0x18), e.f44 = u32(0x44);
  const uint32_t n = u32(0x40) & 0xffff;
  e.n_hi = u32(0x40) >> 16;
  const size_t t = size_t(u32(0x48)) + 8;
  for (uint32_t i = 0; i < n; ++i) {
    const size_t q = t + 32 * size_t(i);
    if (q + 32 > d.size())
      return false;
    EvpObj o{u32(q), u32(q + 4), u32(q + 8), u32(q + 12), u32(q + 16), u32(q + 20), u32(q + 24) >> 16,
             u32(q + 24) & 0xffff, {}};
    const size_t bp = size_t(u32(q + 28)) + 8;
    for (uint32_t k = 0; k < o.nslots; ++k) {
      if (bp + 212 * (k + size_t(1)) > d.size())
        return false;
      o.slots.emplace_back(d.begin() + long(bp + 212 * k), d.begin() + long(bp + 212 * (k + 1)));
    }
    e.objs.push_back(std::move(o));
  }
  if (const uint32_t tp0 = u32(0x1C)) {
    const size_t tp = size_t(tp0) + 8;
    e.has_tail = true, e.tail_num = u16(tp);
    const uint32_t cnt = u16(tp + 2);
    const size_t lp = size_t(u32(tp + 4)) + 8;
    for (uint32_t i = 0; i < cnt; ++i) {
      const size_t r = lp + 16 * size_t(i);
      if (r + 16 > d.size())
        return false;
      EvpRec rec{u16(r), u16(r + 2), u32(r + 4), u32(r + 8), {}};
      const size_t ip = size_t(u32(r + 12)) + 8;
      for (uint32_t j = 0; j < (rec.typ == 9 ? 1u : rec.m); ++j) {  // (type 9: one item; m a parameter, kept)
        if (ip + 8 * (j + size_t(1)) > d.size())
          return false;
        const uint32_t kind = u32(ip + 8 * j), vp = u32(ip + 8 * j + 4) + 8;
        const uint32_t k = kind >> 24;
        if (vp + (k == 2 ? 4 : 2) > d.size())
          return false;
        rec.items.push_back({k, kind & 0xffffff, k == 2 ? u32(vp) : u16(vp)});
      }
      e.recs.push_back(std::move(rec));
    }
  }
  return true;
}
inline Bytes BuildEvp(const Evp &e) {
  const size_t n = e.objs.size();
  std::vector<uint32_t> ptrs = {0x14, 0x1C, 0x48};
  Bytes body(0x4C + 32 * n, 0), blocks;
  const size_t pos = body.size();
  for (size_t i = 0; i < n; ++i) {
    const EvpObj &o = e.objs[i];
    uint8_t *q = &body[0x4C + 32 * i];
    const uint32_t w[8] = {o.idx, o.model, o.t0, o.t1, o.w4, o.w5, o.nslots << 16 | o.lo, uint32_t(pos + blocks.size() - 8)};
    for (int k = 0; k < 8; ++k)
      svrfmt::PutBe32(q + 4 * k, w[k]);
    ptrs.push_back(uint32_t(0x4C + 32 * i + 28));
    for (auto &s : o.slots)
      svrfmt::App(blocks, s);
  }
  svrfmt::App(body, blocks);
  if (e.has_tail) {
    const size_t tp = body.size(), rec_at = tp + 8, list_at = rec_at + 16 * e.recs.size();
    Bytes tb;
    Put16(tb, int(e.tail_num)), Put16(tb, int(e.recs.size())), Put32(tb, uint32_t(tp));
    ptrs.push_back(uint32_t(tp + 4));
    std::vector<size_t> lists;
    size_t la = list_at;
    for (auto &r : e.recs)
      lists.push_back(la), la += 8 * r.items.size();
    // the values: SvR 2011 writes kind 1 (u16) values before kind 2 (u32)
    Bytes vals;
    const size_t val_base = la;
    std::vector<std::vector<size_t>> item_ptr(e.recs.size());
    for (size_t ri = 0; ri < e.recs.size(); ++ri) {
      const auto &r = e.recs[ri];
      std::vector<size_t> order(r.items.size());
      for (size_t j = 0; j < order.size(); ++j)
        order[j] = j;
      std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return r.items[a].k < r.items[b].k; });
      item_ptr[ri].resize(r.items.size());
      for (size_t j : order) {
        const auto &it = r.items[j];
        const size_t al = it.k == 2 ? 4 : 2;
        while ((val_base + vals.size()) % al)
          vals.push_back(0);
        item_ptr[ri][j] = val_base + vals.size();
        if (it.k == 2)
          Put32(vals, it.val);
        else
          Put16(vals, int(it.val));
      }
    }
    for (size_t ri = 0; ri < e.recs.size(); ++ri) {
      const auto &r = e.recs[ri];
      Put16(tb, int(r.typ)), Put16(tb, int(r.z)), Put32(tb, r.v);
      Put32(tb, r.typ == 9 ? r.m : uint32_t(r.items.size())), Put32(tb, uint32_t(lists[ri] - 8));
      ptrs.push_back(uint32_t(rec_at + 16 * ri + 12));
    }
    for (size_t ri = 0; ri < e.recs.size(); ++ri)
      for (size_t j = 0; j < e.recs[ri].items.size(); ++j) {
        const auto &it = e.recs[ri].items[j];
        Put32(tb, it.k << 24 | it.low), Put32(tb, uint32_t(item_ptr[ri][j] - 8));
        ptrs.push_back(uint32_t(lists[ri] + 8 * j + 4));
      }
    svrfmt::App(tb, vals);
    svrfmt::App(body, tb);
    svrfmt::PutBe32(&body[0x1C], uint32_t(tp - 8));
  }
  while ((body.size() - 8) % 16)
    body.push_back(0);
  const uint32_t size = uint32_t(body.size() - 8);
  memcpy(&body[0], "0FOP", 4);
  svrfmt::PutBe32(&body[4], size);
  memcpy(&body[8], "EV\0\0", 4);
  svrfmt::PutBe32(&body[0xC], size);
  svrfmt::PutBe32(&body[0x10], e.f10), svrfmt::PutBe32(&body[0x14], 0x18), svrfmt::PutBe32(&body[0x18], e.f18);
  svrfmt::PutBe16(&body[0x20], uint16_t(e.num));
  memcpy(&body[0x22], e.name.data(), std::min<size_t>(30, e.name.size()));
  svrfmt::PutBe32(&body[0x40], uint32_t(n) | e.n_hi << 16);
  svrfmt::PutBe32(&body[0x44], e.f44), svrfmt::PutBe32(&body[0x48], 0x44);
  std::sort(ptrs.begin(), ptrs.end());
  for (auto &p : ptrs)
    p -= 8;
  Chunk(body, "POF0", yanm::ik::detail::PofWrite(ptrs));
  return body;
}

inline bool IsType(const EvpObj &o, uint32_t t0, uint32_t t1) { return o.t0 == t0 && o.t1 == t1; }

}  // namespace detail

// The script (evp_svr11_to_wwe12): the same objects and 212-byte slots; positions x 10 (cm) for characters (words 4-51
// of each slot, 8 x 3 floats at a stride of 6), ropes (the same) and turnbuckles (words 4-6); the name's inner spaces
// as '_'; header word +0x44 = 0x00010000.
inline bool ConvertEvp(const Bytes &evp11, Bytes &evp12, std::string *why = nullptr) {
  using namespace detail;
  Evp e;
  if (!ParseEvp(evp11, e)) {
    if (why)
      *why = "not a SvR 2011 entrance script";
    return false;
  }
  for (auto &o : e.objs) {
    std::vector<int> words;
    if (IsType(o, 0x01000000, 0) || IsType(o, 1, 0)) {
      for (int j = 0; j < 8; ++j)
        for (int d = 0; d < 3; ++d)
          words.push_back(4 + j * 6 + d);
    } else if (IsType(o, 0, 0x01000000))
      words = {4, 5, 6};
    for (auto &s : o.slots)
      for (int w : words) {
        const float v = svrfmt::BeF(&s[4 * size_t(w)]);
        if (v != 0.0f && std::isfinite(v))
          svrfmt::PutBeF(&s[4 * size_t(w)], float(double(v) * 10.0));
      }
  }
  size_t len = 0;
  while (len < e.name.size() && e.name[len])
    ++len;
  size_t core = len;
  while (core && e.name[core - 1] == ' ')
    --core;
  for (size_t i = 0; i < core; ++i)
    if (e.name[i] == ' ')
      e.name[i] = '_';
  std::fill(e.name.begin() + long(len), e.name.end(), 0);
  e.f44 = 0x10000;
  evp12 = BuildEvp(e);
  return true;
}

// SvR 2011 entrance -> WWE '12 entrance. ymks: the tracks (EVT entry, a YMKs bank), evp11: the script (EVP entry),
// base_evz12: a WWE '12 entrance's tracks (EVZ entry, PACH; WWE '12's entrance 100, the Rock's: the Python tool's
// template and placeholders come from it). -> evz12 (PACH, one record per script object) and evp12.
// prop_donors (ours, 2026-11-12): by prop model, a WWE '12 entrance's record of that prop (unpacked) and the name of
// one of its blocks where it is held - the prop's pose in the hand, used for every block of that prop instead of the
// base's (whose pose put Sandman's kendo stick and beer out of sight); Kaiko's wwe13_entrance.py prop_donor does the
// same the other way (WWE '13 -> SvR 2011), with credit.
using PropDonors = std::map<uint32_t, std::pair<Bytes, std::string>>;
inline bool Convert(const Bytes &ymks, const Bytes &evp11, const Bytes &base_evz12, Bytes &evz12, Bytes &evp12,
                    std::string *why = nullptr, const PropDonors *prop_donors = nullptr) {
  using namespace detail;
  Ctx ctx;
  auto fail = [&](const std::string &w) {
    if (why)
      *why = ctx.ok ? w : ctx.why;
    return false;
  };
  std::vector<Motion> bank;
  Span T;
  if (!ReadBank(ymks, bank, T))
    return fail("not a SvR 2011 track bank (YMKs)");
  Evp e11;
  if (!ParseEvp(evp11, e11))
    return fail("not a SvR 2011 entrance script");
  std::vector<svrfmt::PachEntry> pach;
  if (!svrfmt::PachRead(base_evz12, pach))
    return fail("the base WWE '12 entrance tracks are not a PACH");
  std::map<uint32_t, Bytes> base;
  std::vector<uint32_t> base_order;
  for (auto &p : pach) {
    if (!base.count(p.id))
      base_order.push_back(p.id);
    base[p.id] = svrfmt::Unpack(p.data);
  }
  std::vector<TemplateTrack> tpl;
  if (!base.count(0) || !BodyTemplate(base[0], tpl, ctx))
    return fail("the base WWE '12 entrance has no entrant record");
  // the motions by object (track id 25000 + n) and (x, y); headers by record kind; the placeholder
  std::map<int, std::map<std::pair<int, int>, const Motion *>> by;
  for (auto &m : bank)
    if (!m.data.Empty())
      by[m.id - 25000][{m.x, m.y}] = &m;
  std::map<int, Bytes> head_of;
  Bytes stub(96, 0);
  bool have_stub = false;
  for (uint32_t id : base_order) {
    const Bytes &lf = base[id];
    if (lf.size() >= 0x70) {
      const int kind = svrfmt::Be16(&lf[0x20]);
      if (!head_of.count(kind))
        head_of[kind] = Bytes(lf.begin(), lf.begin() + 0x70);
    } else if (!have_stub)
      stub = lf, have_stub = true;
  }
  int first_char = 0;
  for (size_t i = 0; i < e11.objs.size(); ++i)
    if (IsType(e11.objs[i], 0x01000000, 0)) {
      first_char = int(i);
      break;
    }
  std::vector<svrfmt::PachEntry> out;
  for (size_t n = 0; n < e11.objs.size() && ctx.ok; ++n) {
    const EvpObj &o = e11.objs[n];
    out.push_back({uint32_t(n), stub});
    auto itr = by.find(int(n));
    if (itr == by.end())
      continue;
    const auto &tr = itr->second;
    const Bytes *bl = base.count(uint32_t(n)) ? &base[uint32_t(n)] : nullptr;
    int bkind = -1;
    std::vector<BlockRef> bblocks;
    if (bl && !Header(*bl, bkind, bblocks))
      bl = nullptr;
    // (ours) the base record of the same index is an entrance's object: a victory scene's objects are laid out
    // otherwise (SvR 2010 Jeff Hardy's ropes 10 / 11 got the base's camera and global records, and the scene showed
    // the crowd, 2026-11-02). One of the kind the object needs instead: ropes / turnbuckles 119, props 130 (not the
    // entrant's 0), the global object 1 (not the camera's 10); the base entrance's own ones first (12, 7, 11).
    {
      int want = -1;
      uint32_t first = 0, avoid = 0xFFFFFFFF;
      if (IsType(o, 1, 0) || IsType(o, 0, 0x01000000))
        want = 119, first = 12;
      else if (IsType(o, 0x100, 0))
        want = 130, first = 7, avoid = 0;
      else if (IsType(o, 0, 0x10000))
        want = 1, first = 11, avoid = 10;
      // (a prop on another kind's record kept as Max's tool does: on a rope record it gets its own 2011 motion, Cena's
      // 17; only one without a record gets a prop's)
      if (want >= 0 && (!bl || (bkind != want && want != 130))) {
        auto fits = [&](uint32_t id) {
          int k = -1;
          std::vector<BlockRef> bb;
          if (id == avoid || !base.count(id) || !Header(base[id], k, bb) || k != want)
            return false;
          bl = &base[id], bkind = k, bblocks = std::move(bb);
          return true;
        };
        if (!fits(first))
          for (uint32_t id : base_order)
            if (fits(id))
              break;
      }
    }
    // (ours) a prop a WWE '12 entrance holds too: its record there, its held block for every block
    std::string donor_block;
    if (prop_donors && IsType(o, 0x100, 0))
      if (const auto d = prop_donors->find(o.model); d != prop_donors->end()) {
        int k = -1;
        std::vector<BlockRef> bb;
        if (Header(d->second.first, k, bb)) {
          bl = &d->second.first, bkind = k, bblocks = std::move(bb);
          donor_block = d->second.second;
        }
      }
    auto base_block = [&](const std::string &nm) -> size_t {
      for (auto &b : bblocks)
        if (b.name == (donor_block.empty() ? nm : donor_block))
          return b.offset;
      return bblocks.empty() ? 0 : bblocks[0].offset;
    };
    auto name_of = [](int x, int y) {
      char b[16];
      snprintf(b, sizeof(b), "%04d%04d", x, y);
      return std::string(b);
    };
    std::vector<BlockOut> blocks;
    Bytes hdr;
    if (IsType(o, 0x01000000, 0)) {
      // the entrant: body + fingers + events per segment (y 0)
      bool has_body = false;
      for (auto &[xy, m] : tr)
        has_body = has_body || xy.second == 0;
      if (!has_body)
        continue;
      if (!head_of.count(130)) {
        ctx.Fail("the base entrance has no character record");
        break;
      }
      hdr = head_of[130];
      for (auto &[xy, m] : tr) {
        if (xy.second != 0)
          continue;
        std::vector<BodyKey> keys;
        if (!BodyKeys(m->data, T, keys, ctx))
          break;
        std::map<uint32_t, std::vector<RotKey>> xr;
        for (int yy : {2, 3})
          if (auto f = tr.find({xy.first, yy}); f != tr.end())
            if (!FingerKeys(f->second->data, yy, xr, ctx))
              break;
        std::map<int, std::vector<Span>> fe;
        for (auto &k : keys)
          if (!k.events.empty())
            for (auto &ev : k.events)
              fe[k.frame].push_back(ev);
        BlockOut b;
        b.name = name_of(xy.first, xy.second);
        if (!BodyBlock(tpl, keys, 2 * int(m->frames) + 1, xr, b.raw, ctx))
          break;
        b.events = ConvertEvents(fe, kChar);
        blocks.push_back(std::move(b));
      }
    } else if (IsType(o, 0x10000, 0)) {
      // the camera; the target distance from the first character's pelvis
      if (!head_of.count(1)) {
        ctx.Fail("the base entrance has no camera record");
        break;
      }
      hdr = head_of[1];
      const auto ib = by.find(first_char);
      for (auto &[xy, m] : tr) {
        std::map<int, V3> pel;
        if (ib != by.end())
          if (auto b0 = ib->second.find({xy.first, 0}); b0 != ib->second.end()) {
            std::vector<BodyKey> bk;
            if (!BodyKeys(b0->second->data, T, bk, ctx))
              break;
            for (auto &k : bk)
              pel[k.frame] = {10 * k.root[0] * kRootUnit, 10 * k.root[1] * kRootUnit, 10 * k.root[2] * kRootUnit};
          }
        std::vector<CamKey> keys;
        std::map<int, std::vector<Span>> fe;
        if (!CamKeys(m->data, T, keys, fe, ctx))
          break;
        if (keys.empty())
          continue;
        BlockOut b;
        b.name = name_of(xy.first, xy.second);
        if (!CameraBlock(keys, 2 * int(m->frames) + 1, pel, b.raw, ctx))
          break;
        b.events = ConvertEvents(fe, kCamera);
        b.events.push_back({2 * int(m->frames), {{254, 0, {}}}});  // (the end marker every camera block has)
        blocks.push_back(std::move(b));
      }
    } else if (bl && (IsType(o, 1, 0) || IsType(o, 0, 0x01000000) || bkind == 119)) {
      // ropes, turnbuckles
      hdr.assign(bl->begin(), bl->begin() + 0x70);
      for (auto &[xy, m] : tr) {
        BlockOut b;
        b.name = name_of(xy.first, xy.second);
        Raw braw;
        std::map<int, std::vector<Span>> fe;
        if (!ParseRaw(*bl, base_block(b.name), braw, ctx) ||
            !RopeBlock(braw, m->data, T, 2 * int(m->frames) + 1, b.raw, fe, ctx))
          break;
        b.events = ConvertEvents(fe, kTurnbuckle);
        blocks.push_back(std::move(b));
      }
    } else if (bl) {
      // props, the global object: the base's tracks (a global block re-timed to 2 x frames - 1, a prop block kept,
      // a fixed 201-frame hold) + their events
      hdr.assign(bl->begin(), bl->begin() + 0x70);
      const bool prop = IsType(o, 0x100, 0);
      for (auto &[xy, m] : tr) {
        BlockOut b;
        b.name = name_of(xy.first, xy.second);
        std::vector<std::pair<int, Span>> recs;
        if (!Records(m->data, recs, ctx))
          break;
        std::map<int, std::vector<Span>> fe;
        for (auto &[f, r] : recs) {
          Span ev, pose;
          SplitRecord(r, ev, pose);
          FrameEvents(f, ev, T, fe);
        }
        b.events = prop ? ConvertEvents(fe, kProp) : GlobalEvents(fe, true);
        if (!ParseRaw(*bl, base_block(b.name), b.raw, ctx))
          break;
        if (!prop)
          Stretch(b.raw, 2 * int(m->frames) - 1);
        blocks.push_back(std::move(b));
      }
    } else
      continue;
    if (!ctx.ok)
      break;
    // (ours) the global object: SvR 2011's own lights, one block per track (in the bank's order by x, y), if every
    // track has light keys
    if (IsType(o, 0, 0x10000) && !blocks.empty()) {
      std::vector<const Motion *> ms;
      for (auto &m : bank)
        if (m.id - 25000 == int(n) && !m.data.Empty())
          ms.push_back(&m);
      std::stable_sort(ms.begin(), ms.end(), [](const Motion *a, const Motion *b) {
        return std::make_pair(a->x, a->y) < std::make_pair(b->x, b->y);
      });
      std::vector<BlockOut> lights;
      bool all = true;
      for (auto *m : ms) {
        BlockOut b;
        b.name = name_of(m->x, m->y);
        if (!LightBlock(m->data, T, m->x, m->y, 2 * int(m->frames) - 1, b, ctx)) {
          all = false;
          break;
        }
        lights.push_back(std::move(b));
      }
      if (!ctx.ok)
        break;
      if (all && !lights.empty())
        blocks = std::move(lights);
    }
    if (!blocks.empty())
      out.back().data = BuildRecord(hdr, blocks, ctx);
  }
  if (!ctx.ok)
    return fail(ctx.why);
  evz12 = svrfmt::PachWrite(out);
  return ConvertEvp(evp11, evp12, why);
}

}  // namespace svr11_entrance
