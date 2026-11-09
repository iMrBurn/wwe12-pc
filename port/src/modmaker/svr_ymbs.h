// SvR's YMBs motion banks (the taunts, the submissions, some finishers: ADPCM channels, the limbs by IK) read as
// YMKs body keys (svr11_entrance's BodyKey: the 21 motion bones' s16 angles on their motion bind, the root), so a
// YMBs move goes on through svr_moves.h like a YMKs one (2026-11-09, the Moveset update).
//
// From Kaiko's ymbs.py (the codec) and ymbs_retarget.py (the game's pose: sub_823A4420 / the IK sub_823A4DD8, checked
// by him bone by bone against the game's own pose dump), ported with his permission, credit given:
//  - bank: 'YMBs', a 256-byte channel definition table T at 0x10, u32 count at 0x110, a directory {u8 y, u8 x, u16 id,
//    u32 offset, u32 frames, u32 rt}, then a database: u32, a 20-byte header per motion {u8 type, u8 segments,
//    u8 interval, u8, 4 f32}, the motions at database + offset (segments: {u16 start, u16 keys, u32 offset} each);
//  - a motion = channel streams in the order of its type's channel list (T + T[type + 1]: count, ids; descriptors
//    T + T[T[0] + 1], 4 bytes {flags, b1, scale, -}): flags 0x80|0x40 an IK target (xyz, 24-bit), 0x80 a limb's pole,
//    0x40 the root translation (xyz, 24-bit, keyed at twice the rate), 0x20 an extra, else rotation axes (bits 0-2);
//  - the pose: rotation channels replace the bone's local rotation (R = Rz Ry Rx), except the IK bones (upper arm,
//    forearm, thigh, calf); clavicles / hands take y and z only, toes y only; the root translation replaces the
//    root's; the arms are solved in the clavicle's frame, the legs in the root's, to root + the IK vector: two equal
//    segments (L = 2 x the shoulder / hip to elbow / knee distance), bend = 2 acos(d / L) (legs negative), upper bone
//    = Align(+X -> target) . RotX(pole, arms - pi) . RotY(-bend / 2), lower bone RotY(bend).
#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "svr11_entrance.h"

namespace svr_ymbs {

using svrfmt::Bytes;
namespace se = svr11_entrance::detail;

namespace detail {

inline uint32_t L32(const Bytes &d, size_t o) { return o + 4 <= d.size() ? uint32_t(d[o] | d[o + 1] << 8 | d[o + 2] << 16 | uint32_t(d[o + 3]) << 24) : 0; }
inline int S16(const Bytes &d, size_t o) { return o + 2 <= d.size() ? int(int16_t(uint16_t(d[o] | d[o + 1] << 8))) : 0; }

// Yamaha-style ADPCM: init, then codes (nibbles or 6-bit), the step adapting (ymbs.adpcm)
inline std::vector<int> Adpcm(int v, int step, const std::vector<int> &codes, bool bits4) {
  static const int kStep[8] = {57, 57, 57, 57, 77, 102, 128, 153};
  std::vector<int> out{v};
  for (int c : codes) {
    int mag, neg, d, idx;
    if (bits4)
      mag = c & 7, neg = c & 8, d = ((mag * 2 + 1) * step) >> 3, idx = mag;
    else
      mag = c & 0x1F, neg = c & 0x20, d = ((mag * 2 + 1) * step) >> 5, idx = std::min(mag >> 2, 7);
    v = neg ? v - d : v + d;
    step = (step * kStep[idx]) >> 6;
    step = step < 127 ? 127 : step > 24576 ? 24576 : step;
    out.push_back(v);
  }
  return out;
}
inline std::vector<int> Nibbles(const Bytes &d, size_t p, size_t n) {
  std::vector<int> out;
  for (size_t i = 0; i < n; ++i) {
    const int x = p + (i >> 1) < d.size() ? d[p + (i >> 1)] : 0;
    out.push_back(i % 2 == 0 ? x >> 4 : x & 15);
  }
  return out;
}
inline int Wrap(int x) { return ((x + 0x8000) & 0xFFFF) - 0x8000; }
// translation channel: s24 init, u8 step, 2K nibbles (K + 4 bytes)
inline std::vector<int> Ch32(const Bytes &d, size_t &p, size_t K) {
  int init = p + 3 <= d.size() ? int(d[p] | d[p + 1] << 8 | d[p + 2] << 16) : 0;
  if (init & 0x800000)
    init -= 0x1000000;
  const int step = (p + 3 < d.size() ? d[p + 3] : 0) * 127;
  auto v = Adpcm(init, step, Nibbles(d, p + 4, 2 * K), true);
  p += K + 4;
  return v;
}
// rotation channel: 'n' nibbles ((K + 1) / 2 + 3 bytes), '6' 6-bit (K + 3), raw s16 (2K)
inline std::vector<int> Ch16(const Bytes &d, size_t &p, size_t K, char mode) {
  std::vector<int> v;
  if (mode == 'r') {
    for (size_t i = 0; i < K; ++i)
      v.push_back(S16(d, p + 2 * i));
    p += 2 * K;
    return v;
  }
  const int init = S16(d, p), step = (p + 2 < d.size() ? d[p + 2] : 0) * 127;
  if (mode == '6') {
    std::vector<int> codes;
    for (size_t i = 0; i < K; ++i)
      codes.push_back(p + 3 + i < d.size() ? d[p + 3 + i] : 0);
    v = Adpcm(init, step, codes, false);
    p += K + 3;
  } else {
    const size_t n = (K + 1) / 2;
    v = Adpcm(init, step, Nibbles(d, p + 3, K), true);
    p += n + 3;
  }
  for (auto &x : v)
    x = Wrap(x);
  return v;
}

// A decoded segment: its first frame, keys, and the channels (values in their units, scale applied later)
struct Seg {
  int start = 0, keys = 0;
  bool has_root = false;
  double root_scale = 0;
  std::array<std::vector<int>, 3> root;
  std::vector<std::pair<double, std::array<std::vector<int>, 3>>> ik;   // (scale, xyz), limb order
  std::vector<std::pair<double, std::vector<int>>> twist;               // (scale, values), limb order
  struct Rot {
    double scale = 0;
    std::array<std::vector<int>, 3> axes;
    std::array<bool, 3> has{};
  };
  std::vector<Rot> rot;  // the motion bones' order
};

}  // namespace detail

struct Bank {
  Bytes raw;
  struct Entry {
    int y = 0, x = 0, id = 0;
    uint32_t off = 0, frames = 0;
  };
  std::vector<Entry> dir;
  size_t base = 0;
  bool Open(const Bytes &b) {
    if (b.size() < 0x114 || memcmp(b.data(), "YMBs", 4))
      return false;
    raw = b;
    const uint32_t n = detail::L32(raw, 0x110);
    base = 0x114 + 16 * size_t(n);
    if (base > raw.size())
      return false;
    dir.clear();
    for (uint32_t i = 0; i < n; ++i) {
      const size_t q = 0x114 + 16 * size_t(i);
      dir.push_back({raw[q], raw[q + 1], raw[q + 2] | raw[q + 3] << 8, detail::L32(raw, q + 4), detail::L32(raw, q + 8)});
    }
    return true;
  }
  // the channels of a motion type: (id, descriptor {flags, b1, scale, -})
  std::vector<std::pair<int, std::array<uint8_t, 4>>> Channels(int type) const {
    std::vector<std::pair<int, std::array<uint8_t, 4>>> out;
    auto T = [&](size_t i) { return i < 256 ? raw[0x10 + i] : uint8_t(0); };
    const size_t lst = T(size_t(type) + 1), cnt = T(lst), desc0 = T(size_t(T(0)) + 1);
    for (size_t j = 0; j < cnt; ++j) {
      const int b = T(lst + 1 + j);
      if (b < 1)
        continue;
      std::array<uint8_t, 4> d{};
      for (size_t c = 0; c < 4; ++c)
        d[c] = T(desc0 + 4 * size_t(b - 1) + c);
      out.push_back({b, d});
    }
    return out;
  }
  bool Decode(size_t k, std::vector<detail::Seg> &out, int &interval) const {
    using namespace detail;
    out.clear();
    const size_t h = base + 4 + 20 * k;
    if (k >= dir.size() || h + 20 > raw.size())
      return false;
    const int type = raw[h], nseg = raw[h + 1];
    interval = raw[h + 2] ? raw[h + 2] : 1;
    const size_t p = base + dir[k].off;
    std::vector<std::array<int, 3>> segs;  // start, keys, offset
    if (nseg)
      for (int s = 0; s < nseg; ++s)
        segs.push_back({raw[p + 8 * s] | raw[p + 8 * s + 1] << 8, raw[p + 8 * s + 2] | raw[p + 8 * s + 3] << 8, int(L32(raw, p + 8 * s + 4))});
    else
      segs.push_back({0, int((dir[k].frames + uint32_t(interval) - 1) / uint32_t(interval)), 0});
    const auto chans = Channels(type);
    for (const auto &[st, K, off] : segs) {
      size_t q = p + size_t(off);
      if (q >= raw.size())
        return false;
      Seg s;
      s.start = st, s.keys = K;
      for (const auto &[bid, d] : chans) {
        const uint8_t f = d[0], b1 = d[1];
        const double scale = d[2];
        if (f & 0x80) {
          if (f & 0x40) {
            std::array<std::vector<int>, 3> xyz;
            for (auto &a : xyz)
              a = Ch32(raw, q, size_t(K));
            s.ik.push_back({scale, xyz});
          } else
            s.twist.push_back({scale, Ch16(raw, q, size_t(K), 'n')});
        } else if (f & 0x40) {
          for (auto &a : s.root)
            a = Ch32(raw, q, size_t(K));
          s.has_root = true, s.root_scale = scale;
        } else if (f & 0x20) {
          Ch16(raw, q, size_t(K), (b1 & 15) == 2 ? '6' : 'n');
        } else {
          Seg::Rot r;
          r.scale = scale;
          for (int ax = 0; ax < 3; ++ax)
            if (f & (1 << ax)) {
              const char mode = (b1 & 0xF0) == 0x40 ? 'r' : (b1 & 15) == 2 ? '6' : 'n';
              r.axes[size_t(ax)] = Ch16(raw, q, size_t(K), mode), r.has[size_t(ax)] = true;
            }
          s.rot.push_back(std::move(r));
        }
        if (q > raw.size())
          return false;
      }
      out.push_back(std::move(s));
    }
    return true;
  }
};

namespace detail {

using se::M3;
using se::M4;
using se::V3;
inline V3 MulV(const M3 &m, const V3 &v) {
  return {m[0] * v[0] + m[1] * v[1] + m[2] * v[2], m[3] * v[0] + m[4] * v[1] + m[5] * v[2], m[6] * v[0] + m[7] * v[1] + m[8] * v[2]};
}
inline V3 Pos(const M4 &m) { return {m[3], m[7], m[11]}; }
inline double Norm(const V3 &v) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
inline V3 Sub(const V3 &a, const V3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
// the shortest arc +X -> n (ymbs_retarget._align)
inline M3 Align(const V3 &n) {
  const V3 axis{0.0, -n[2], n[1]};
  const double al = Norm(axis), ang = std::acos(std::max(-1.0, std::min(1.0, n[0])));
  if (al < 1e-6)
    return n[0] >= 0 ? se::Eye3() : M3{-1, 0, 0, 0, -1, 0, 0, 0, 1};
  const V3 k{axis[0] / al, axis[1] / al, axis[2] / al};
  const double c = std::cos(ang), s = std::sin(ang), t = 1 - c;
  return {c + k[0] * k[0] * t,        k[0] * k[1] * t - k[2] * s, k[0] * k[2] * t + k[1] * s,
          k[1] * k[0] * t + k[2] * s, c + k[1] * k[1] * t,        k[1] * k[2] * t - k[0] * s,
          k[2] * k[0] * t - k[1] * s, k[2] * k[1] * t + k[0] * s, c + k[2] * k[2] * t};
}

// One key's values (ymbs_retarget.frame_values): radians / model units
struct Vals {
  V3 root_t{};
  std::array<V3, 4> ik{};
  std::array<double, 4> twist{};
  std::array<V3, 21> rot{};
  std::array<bool, 21> has{};
};
inline Vals ValuesAt(const Seg &s, int f) {
  auto g = [](const std::vector<int> &v, int i) { return v.empty() ? 0 : v[size_t(std::max(0, std::min(i, int(v.size()) - 1)))]; };
  Vals o;
  const double ang = 2 * se::kPi / 65536;
  if (s.has_root)
    for (int c = 0; c < 3; ++c)
      o.root_t[size_t(c)] = g(s.root[size_t(c)], 2 * f) * s.root_scale * 1e-4;
  for (size_t l = 0; l < s.ik.size() && l < 4; ++l)
    for (int c = 0; c < 3; ++c)
      o.ik[l][size_t(c)] = g(s.ik[l].second[size_t(c)], 2 * f) * s.ik[l].first * 1e-4;
  for (size_t l = 0; l < s.twist.size() && l < 4; ++l)
    o.twist[l] = g(s.twist[l].second, f) * s.twist[l].first * ang;
  for (size_t c = 0; c < s.rot.size() && c < 21; ++c) {
    for (int a = 0; a < 3; ++a)
      if (s.rot[c].has[size_t(a)])
        o.rot[c][size_t(a)] = g(s.rot[c].axes[size_t(a)], f) * s.rot[c].scale * ang;
    o.has[c] = s.rot[c].has[0] || s.rot[c].has[1] || s.rot[c].has[2];
  }
  return o;
}

// The game's pose from the values (ymbs_retarget.pose): the local matrices of SvR 2011's body nodes
inline std::vector<M4> Pose(const Vals &v) {
  const auto &n11 = se::Nodes11();
  auto idx = [&](const char *name) {
    for (size_t i = 0; i < n11.size(); ++i)
      if (!strcmp(n11[i].name, name))
        return int(i);
    return -1;
  };
  std::vector<M4> local(n11.size());
  for (size_t i = 0; i < n11.size(); ++i)
    local[i] = se::Trs(se::Rotm(n11[i].r), n11[i].t);
  static const bool kIk[21] = {0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0};
  for (int c = 0; c < 21; ++c) {
    const int i = idx(se::Bones11()[size_t(c)].name);
    if (i < 0 || kIk[c] || !v.has[size_t(c)])
      continue;
    V3 e = v.rot[size_t(c)];
    if (c == 5 || c == 8 || c == 9 || c == 12)  // (clavicles, hands: y and z only)
      e[0] = 0;
    if (c == 16 || c == 20)  // (toes: y only)
      e[0] = e[2] = 0;
    local[size_t(i)] = se::Trs(se::Rotm(e), n11[size_t(i)].t);
  }
  {
    const int r = idx("root");
    local[size_t(r)][3] = v.root_t[0], local[size_t(r)][7] = v.root_t[1], local[size_t(r)][11] = v.root_t[2];
  }
  for (const char *side : {"l", "r"}) {  // (the forearm's roll on its twist bones, half each when there are two)
    const double roll = v.rot[size_t(side[0] == 'l' ? 7 : 11)][0];
    const int x1 = idx((std::string(side) + "_kote_x").c_str()), x2 = idx((std::string(side) + "_kote_x2").c_str());
    for (int i : {x1, x2})
      if (i >= 0) {
        const M3 R = se::Rotm(x2 >= 0 ? roll * 0.5 : roll, 0, 0);
        const V3 t = Pos(local[size_t(i)]);
        local[size_t(i)] = se::Trs(R, t);
      }
  }
  auto fk = [&]() {
    std::vector<M4> W(n11.size());
    for (size_t i = 0; i < n11.size(); ++i)
      W[i] = n11[i].parent < 0 ? local[i] : se::Mul4(W[size_t(n11[i].parent)], local[i]);
    return W;
  };
  const std::vector<M4> W = fk();
  const V3 root_p = Pos(W[size_t(idx("root"))]);
  struct Limb {
    const char *up, *lo, *par;
    bool arm;
  };
  static const Limb limbs[4] = {{"l_ninoude", "l_kote", "l_sakotsu", true}, {"r_ninoude", "r_kote", "r_sakotsu", true},
                                {"l_momo", "l_sune", "root", false}, {"r_momo", "r_sune", "root", false}};
  for (int l = 0; l < 4; ++l) {
    const int iu = idx(limbs[l].up), il = idx(limbs[l].lo), ip = idx(limbs[l].par);
    if (iu < 0 || il < 0 || ip < 0)
      continue;
    const int su = n11[size_t(iu)].parent;
    const M4 &P = W[size_t(ip)];
    const M3 Pr = se::Rot3(P), PrT = se::Tr(Pr);
    const V3 S = MulV(PrT, Sub(Pos(W[size_t(iu)]), Pos(P)));
    const V3 tgt{root_p[0] + v.ik[size_t(l)][0], root_p[1] + v.ik[size_t(l)][1], root_p[2] + v.ik[size_t(l)][2]};
    V3 vv = Sub(MulV(PrT, Sub(tgt, Pos(P))), S);
    const double L = 2 * Norm(Sub(Pos(W[size_t(il)]), Pos(W[size_t(iu)])));
    double d = Norm(vv);
    if (L <= 1e-4 || d <= 1e-5)
      continue;
    const double lim = (limbs[l].arm ? 0.99 : 0.99999) * L;
    if (d >= lim)
      d = limbs[l].arm ? lim : lim + (d - lim) * 0.3;
    double bend = d >= L ? 0.0 : 2 * std::acos(std::min(d / L, 0.99999));
    if (!limbs[l].arm)
      bend = -bend;
    const double pole = v.twist[size_t(l)] - (limbs[l].arm ? se::kPi : 0.0);
    const double vn = Norm(vv);
    const M3 R = se::Mul3(se::Mul3(Align({vv[0] / vn, vv[1] / vn, vv[2] / vn}), se::Rotm(pole, 0, 0)), se::Rotm(0, -bend * 0.5, 0));
    const M3 up = se::Mul3(se::Tr(se::Rot3(W[size_t(su)])), se::Mul3(Pr, R));
    local[size_t(iu)] = se::Trs(up, Pos(local[size_t(iu)]));
    local[size_t(il)] = se::Trs(se::Rotm(0, bend, 0), Pos(local[size_t(il)]));
  }
  return local;
}

}  // namespace detail

// Motion (id, x, y) of the bank as YMKs body keys, one per YMBs key (frame = its 30 Hz frame, count 0), the root in
// YMKs units (0.01 model units). False when it isn't there or doesn't decode.
inline bool BodyKeys(const Bank &b, int id, int x, int y, std::vector<se::BodyKey> &out, uint32_t *frames = nullptr) {
  using namespace detail;
  out.clear();
  for (size_t k = 0; k < b.dir.size(); ++k) {
    if (b.dir[k].id != id || b.dir[k].x != x || b.dir[k].y != y)
      continue;
    std::vector<Seg> segs;
    int interval = 1;
    if (!b.Decode(k, segs, interval) || segs.empty())
      return false;
    if (frames)
      *frames = b.dir[k].frames;
    const auto &n11 = se::Nodes11();
    std::vector<int> node(21, -1);
    for (int c = 0; c < 21; ++c)
      for (size_t i = 0; i < n11.size(); ++i)
        if (!strcmp(n11[i].name, se::Bones11()[size_t(c)].name))
          node[size_t(c)] = int(i);
    for (const auto &s : segs)
      for (int f = 0; f < s.keys; ++f) {
        const int frame = s.start + f * interval;
        if (frame >= int(b.dir[k].frames))
          break;
        const Vals v = ValuesAt(s, f);
        const std::vector<M4> local = Pose(v);
        se::BodyKey key;
        key.frame = frame, key.count = 0, key.has_pose = true;
        for (int c = 0; c < 3; ++c)
          key.root[size_t(c)] = v.root_t[size_t(c)] / se::kRootUnit;
        for (int c = 0; c < 21; ++c)
          if (node[size_t(c)] >= 0)
            key.bones[size_t(c)] = se::Eul16(se::Mul3(se::Tr(se::Rotm(se::Bones11()[size_t(c)].rest)), se::Rot3(local[size_t(node[size_t(c)])])));
        out.push_back(std::move(key));
      }
    return !out.empty();
  }
  return false;
}

}  // namespace svr_ymbs
