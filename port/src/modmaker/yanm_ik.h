// Mod Maker: WWE '13 entrance tracks made playable by WWE '12's legs. After Kaiko's yanm_decode.py (_read_ik,
// _solve_ik) and w13_retarget.py (Pose13), with his permission and credit.
//
// WWE '13 drives a body's calves by IK: the Bip L/R Calf tracks have no rotation, only an IK stream (flag 0x0001: the
// ankle's target in model space, cm, and the knee angle w), and the game (0x82384b78 in WWE '13) solves the thigh and
// the calf from it each frame. WWE '12's bodies are plain FK and ignore that stream: the knees stay straight and the
// thighs keep their FK rotation, so the feet walk on their heels. BakeLegIk solves the legs the way WWE '13 does, frame
// by frame, and writes the thigh's and the calf's local rotations as plain rot16 keys (flag 0x2000), which WWE '12 plays.
//
// A record's block = YANM (the tracks), POF0 (the positions of its offset fields, from the YANM's base), YAT2, POF0
// (empty). The new keys go at the YANM's end; the two tracks' descriptors keep their size (rot8/IK out, rot16 in), and
// POF0 is written again (the calf's offset fields move).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "svrfmt/archive.h"
#include "svrfmt/bytes.h"
#include "yanm.h"

namespace yanm::ik {

using M3 = std::array<float, 9>;  // row-major

inline M3 MatOf(const Q &q) {
  const float x = q[0], y = q[1], z = q[2], w = q[3];
  return {1 - 2 * (y * y + z * z), 2 * (x * y - z * w),     2 * (x * z + y * w),
          2 * (x * y + z * w),     1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
          2 * (x * z - y * w),     2 * (y * z + x * w),     1 - 2 * (x * x + y * y)};
}
inline M3 MulM(const M3 &a, const M3 &b) {
  M3 o{};
  for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c)
      o[size_t(3 * r + c)] = a[size_t(3 * r)] * b[size_t(c)] + a[size_t(3 * r + 1)] * b[size_t(3 + c)] +
                             a[size_t(3 * r + 2)] * b[size_t(6 + c)];
  return o;
}
inline M3 TransM(const M3 &a) { return {a[0], a[3], a[6], a[1], a[4], a[7], a[2], a[5], a[8]}; }
inline V MulV(const M3 &a, const V &v) {
  return {a[0] * v[0] + a[1] * v[1] + a[2] * v[2], a[3] * v[0] + a[4] * v[1] + a[5] * v[2],
          a[6] * v[0] + a[7] * v[1] + a[8] * v[2]};
}
inline V Cross(const V &a, const V &b) { return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]}; }
inline float Dot(const V &a, const V &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
inline float Len(const V &a) { return std::sqrt(Dot(a, a)); }
inline V Unit(const V &a) {
  const float l = Len(a);
  return l > 0 ? V{a[0] / l, a[1] / l, a[2] / l} : a;
}
// R = Rz Ry Rx -> (x, y, z), as the tracks' Euler angles
inline V EulerOf(const M3 &R) {
  const float sy = std::max(-1.0f, std::min(1.0f, -R[6]));
  if (std::fabs(sy) < 0.99999f)
    return {std::atan2(R[7], R[8]), std::asin(sy), std::atan2(R[3], R[0])};
  return {0.0f, std::asin(sy), std::atan2(-R[1], R[4])};
}

// WWE '13's body nodes the legs hang from (Kaiko's w13_skeletons.py NODES13, Mankind ch246): name, t (cm), r (zyx).
struct Node {
  const char *name;
  V t, r;
  int parent;
};
inline const std::vector<Node> &Legs13() {
  static const std::vector<Node> n = {
      {"null", {0, 0, 0}, {0, 0, 0}, -1},
      {"Bip", {0.0f, -96.5507507f, 5e-07f}, {0.0f, 1.5711416f, 0.0f}, 0},
      {"Bip Pelvis", {0, 0, 0}, {1.5707958f, 3.1415925f, 1.5707963f}, 1},
      {"Bip L Thigh", {0.0002441f, -11.5000143f, -8.9e-06f}, {-1.2e-06f, -2.2e-06f, -2.8797929f}, 2},
      {"L_Thigh_x", {21.6596985f, -1.14e-05f, 0.0013944f}, {3e-07f, -1.7e-06f, 0.0f}, 3},
      {"Bip L Calf", {21.8202972f, 1.14e-05f, -0.0014149f}, {2e-07f, 7e-07f, 0.0f}, 4},
      {"Bip L Foot", {43.4799881f, -7.6e-06f, 0.001296f}, {1e-07f, 3.68e-05f, -1e-07f}, 5},
      {"Bip R Thigh", {0.0002365f, 11.4999838f, 6.9e-06f}, {-3e-07f, 4e-07f, 2.8797929f}, 2},
      {"R_Thigh_x", {21.6535416f, 3.81e-05f, 0.0008295f}, {-5e-07f, 0.0f, 1e-07f}, 7},
      {"Bip R Calf", {21.8264275f, 3.8e-06f, -0.001462f}, {-4e-07f, -4.02e-05f, -1e-07f}, 8},
      {"Bip R Foot", {43.4799805f, 3.8e-06f, -0.0013588f}, {1e-07f, 7.82e-05f, 0.0f}, 9}};
  return n;
}

namespace detail {

using yanm::detail::B16;
using yanm::detail::B32;

inline float Half(uint16_t h) {
  const float s = (h & 0x8000) ? -1.0f : 1.0f;
  const int e = (h >> 10) & 0x1f, f = h & 0x3ff;
  if (e == 0)
    return s * float(f) * std::ldexp(1.0f, -24);
  if (e == 31)
    return f == 0 ? s * INFINITY : NAN;
  return s * (1 + float(f) / 1024.0f) * std::ldexp(1.0f, e - 15);
}

struct IkStream {
  int mode = 0;
  std::vector<Key<std::array<float, 4>>> keys;  // ankle target xyz (cm, model space), knee angle w
};

// The 0x0001 stream {u16 sflags, u16 n, u32 offset from the base} (yanm_decode._read_ik).
inline bool ReadIk(const Bytes &d, size_t base, uint16_t sflags, uint16_t n, uint32_t off, IkStream &out) {
  static const int ta[] = {4, 6, 8, 16, 3, 6, 6, 12}, tb[] = {3, 6, 6, 12, 4, 6, 8, 16};
  const size_t p = base + off;
  const int a_code = (sflags >> 3) & 7, b_code = (sflags >> 6) & 7;
  const bool single = (sflags & 7) != 0;
  const size_t hdr = (sflags & 0x38) == 0x18 ? 0 : 16;
  size_t keys_at = p + hdr;
  if (!single && (sflags & 0x1c0) != 0xc0)
    keys_at += 12;
  const size_t size = size_t(ta[a_code] + (single ? 0 : tb[b_code])), stride = size + 1;
  if (a_code > 3 || keys_at + stride * n > d.size())
    return false;
  float off4[4] = {0, 0, 0, 0}, sc4[4] = {0, 0, 0, 0};
  if (hdr)
    for (int i = 0; i < 4; ++i)
      off4[i] = Half(B16(d, p + 2 * size_t(i))), sc4[i] = Half(B16(d, p + 8 + 2 * size_t(i)));
  out.mode = sflags & 7;
  int t = 0;
  for (uint16_t i = 0; i < n; ++i) {
    const size_t k = keys_at + i * stride;
    t += d[k];
    const uint8_t *kd = &d[k + 1];
    std::array<float, 4> v{};
    for (int j = 0; j < 4; ++j) {
      float raw;
      if (a_code == 0)
        raw = kd[j];
      else if (a_code == 1)
        raw = float((kd[j] << 4) | (j & 1 ? kd[4 + j / 2] & 15 : kd[4 + j / 2] >> 4));
      else if (a_code == 2)
        raw = float((kd[j] << 8) | kd[j + 4]);
      else
        raw = svrfmt::BeF(kd + 4 * j);
      v[size_t(j)] = a_code == 3 ? raw : off4[j] + sc4[j] * raw;
    }
    if (v[3] != v[3])
      v[3] = 0;
    out.keys.push_back({t, v});
  }
  return !out.keys.empty();
}

inline std::array<float, 4> SampleIk(const IkStream &s, float f) {
  size_t i;
  float a;
  Find(s.keys, f, i, a);
  auto v = s.keys[i].v;
  if (a != 0)
    for (int c = 0; c < 4; ++c)
      v[size_t(c)] += (s.keys[i + 1].v[size_t(c)] - v[size_t(c)]) * a;
  return v;
}

inline int16_t S16Of(float a) {
  const float v = std::round(a * 32768.0f / 3.14159265f);
  return int16_t(std::max(-32768.0f, std::min(32767.0f, v)));
}

// POF0: each offset field's position from the base, as deltas / 4 in 1, 2 or 4 bytes (0x40, 0x80, 0xC0).
inline std::vector<uint32_t> PofRead(const Bytes &d, size_t at, size_t size) {
  std::vector<uint32_t> out;
  uint32_t p = 0;
  for (size_t i = at; i < at + size && i < d.size();) {
    const uint8_t c = d[i];
    uint32_t v;
    if ((c >> 6) == 0)
      break;
    if ((c >> 6) == 1)
      v = c & 0x3f, i += 1;
    else if ((c >> 6) == 2)
      v = ((c & 0x3fu) << 8) | d[i + 1], i += 2;
    else
      v = ((c & 0x3fu) << 24) | (uint32_t(d[i + 1]) << 16) | (uint32_t(d[i + 2]) << 8) | d[i + 3], i += 4;
    p += v * 4;
    out.push_back(p);
  }
  return out;
}
inline Bytes PofWrite(const std::vector<uint32_t> &pos) {
  Bytes o;
  uint32_t p = 0;
  for (uint32_t x : pos) {
    const uint32_t v = (x - p) / 4;
    p = x;
    if (v < 0x40)
      o.push_back(uint8_t(0x40 | v));
    else if (v < 0x4000)
      o.push_back(uint8_t(0x80 | (v >> 8))), o.push_back(uint8_t(v));
    else
      o.push_back(uint8_t(0xC0 | (v >> 24))), o.push_back(uint8_t(v >> 16)), o.push_back(uint8_t(v >> 8)),
          o.push_back(uint8_t(v));
  }
  while (o.size() % 4)
    o.push_back(0);
  return o;
}

inline void Put16(Bytes &d, size_t o, uint16_t v) { d[o] = uint8_t(v >> 8), d[o + 1] = uint8_t(v); }
inline void Put32(Bytes &d, size_t o, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    d[o + size_t(i)] = uint8_t(v >> (24 - 8 * i));
}
inline void App16(Bytes &d, uint16_t v) { d.push_back(uint8_t(v >> 8)), d.push_back(uint8_t(v)); }
inline void App32(Bytes &d, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    d.push_back(uint8_t(v >> (24 - 8 * i)));
}

// One block (the bytes from its YANM to its last chunk): its legs baked. False (block untouched) if it has none to bake.
inline bool BakeBlock(const Bytes &in, Bytes &out, int *frames_out) {
  if (in.size() < 16 || memcmp(in.data(), "YANM", 4))
    return false;
  const size_t yend = 8 + B32(in, 4), base = 8;
  if (yend + 8 > in.size() || memcmp(&in[yend], "POF0", 4) || in[base + 1] != 0)  // (templated: no IK)
    return false;
  const size_t pof_size = B32(in, yend + 4), rest = yend + 8 + pof_size;
  // descriptors
  struct Desc {
    uint32_t hash;
    uint16_t F;
    size_t at, size;  // descriptor (8 + body) in the block
  };
  std::vector<Desc> ds;
  {
    const uint16_t cnt = B16(in, base + 2);
    size_t o = base + B32(in, base + 4);
    for (uint16_t i = 0; i < cnt && o + 8 <= yend; ++i) {
      const uint16_t sz = B16(in, o + 2);
      ds.push_back({B32(in, o + 4), B16(in, o), o, sz});
      if (!sz)
        break;
      o += sz;
    }
  }
  Block blk;
  if (!yanm::detail::ParseBlock(in, 0, blk))
    return false;
  const auto &nodes = Legs13();
  const size_t nn = nodes.size();
  std::vector<const Track *> tr(nn, nullptr);
  std::vector<int> desc_of(nn, -1);
  for (size_t i = 0; i < nn; ++i) {
    const uint32_t h = NameHash(nodes[i].name);
    for (const auto &t : blk.tracks)
      if (t.hash == h)
        tr[i] = &t;
    for (size_t k = 0; k < ds.size(); ++k)
      if (ds[k].hash == h)
        desc_of[i] = int(k);
  }
  // the calves' IK streams (calf without a rotation of its own)
  struct Leg {
    int thigh, calf, foot;
    IkStream ik;
    bool on = false;
  };
  Leg legs[2] = {{3, 5, 6, {}, false}, {7, 9, 10, {}, false}};
  int frames = blk.frames;
  for (auto &l : legs) {
    const int dc = desc_of[size_t(l.calf)], dt = desc_of[size_t(l.thigh)];
    if (dc < 0 || dt < 0 || !(ds[size_t(dc)].F & 0x0001))
      continue;
    const Track *t = tr[size_t(l.calf)];
    if (t && (!t->rot16.empty() || !t->rot8.empty() || !t->hinges.empty()))
      continue;
    for (const auto &p : yanm::detail::Layout(ds[size_t(dc)].F, nullptr))
      if (p.bit == 0x0001) {
        const size_t b = ds[size_t(dc)].at + 8 + p.off;
        l.on = ReadIk(in, base, B16(in, b), B16(in, b + 2), B32(in, b + 4), l.ik) && (l.ik.mode == 1 || l.ik.mode == 2);
      }
    if (l.on)
      frames = std::max(frames, l.ik.keys.back().t + 1);
  }
  if (!legs[0].on && !legs[1].on)
    return false;
  // bind
  std::vector<Q> bq(nn);
  for (size_t i = 0; i < nn; ++i)
    bq[i] = Euler(nodes[i].r[0], nodes[i].r[1], nodes[i].r[2]);
  auto chain = [&](int a, int b) {
    float tot = 0;
    for (int i = b; i != a && i >= 0; i = nodes[size_t(i)].parent)
      tot += Len(nodes[size_t(i)].t);
    return tot;
  };
  // per frame (every second one: the game runs entrances at 30 fps), the thigh's and the calf's local rotations
  std::vector<int> times;
  for (int f = 0; f < frames; f += 2)
    times.push_back(f);
  if (times.back() != frames - 1 && frames > 1)
    times.push_back(frames - 1);
  std::vector<std::vector<V>> thigh_keys(2), calf_keys(2);
  for (int f : times) {
    std::vector<M3> lr(nn);
    std::vector<V> lt(nn);
    for (size_t i = 0; i < nn; ++i) {
      Q q = bq[i];
      V t = nodes[i].t;
      if (tr[i]) {
        bool hr, hp;
        Q r;
        V p;
        Sample(*tr[i], float(f), hr, r, hp, p);
        if (hr)
          q = r;
        if (hp)
          for (int c = 0; c < 3; ++c)
            t[size_t(c)] += p[size_t(c)];
      }
      lr[i] = MatOf(q), lt[i] = t;
    }
    std::vector<M3> wr(nn);
    std::vector<V> wt(nn);
    auto fk = [&] {
      for (size_t i = 0; i < nn; ++i) {
        const int p = nodes[i].parent;
        if (p < 0) {
          wr[i] = lr[i], wt[i] = lt[i];
        } else {
          wr[i] = MulM(wr[size_t(p)], lr[i]);
          const V o = MulV(wr[size_t(p)], lt[i]);
          wt[i] = {wt[size_t(p)][0] + o[0], wt[size_t(p)][1] + o[1], wt[size_t(p)][2] + o[2]};
        }
      }
    };
    fk();
    for (int s = 0; s < 2; ++s) {
      Leg &l = legs[s];
      if (!l.on)
        continue;
      const size_t th = size_t(l.thigh), ca = size_t(l.calf);
      const auto target = SampleIk(l.ik, float(f));
      const float L1 = chain(l.thigh, l.calf), L2 = chain(l.calf, l.foot), w = target[3];
      const int ax = l.ik.mode == 1 ? 1 : 2;
      const V u = {wr[th][size_t(ax)], wr[th][size_t(3 + ax)], wr[th][size_t(6 + ax)]};
      const V hip = wt[th];
      const V dv = {target[0] - hip[0], target[1] - hip[1], target[2] - hip[2]};
      float dist = Len(dv);
      if (dist <= 0)
        dist = 1e-6f;
      const V d = {dv[0] / dist, dv[1] / dist, dv[2] / dist};
      const V zc = Unit(Cross(d, u)), yc = Unit(Cross(zc, d));
      const float dw = std::sqrt((L1 + L2 * std::cos(w)) * (L1 + L2 * std::cos(w)) + (L2 * std::sin(w)) * (L2 * std::sin(w)));
      const float k = std::max(0.0f, std::min(1.0f, (w - 3.14159265f / 6) * -5.729578f));
      const float de = dist + (dw - dist) * k;
      const float ck = std::max(-1.0f, std::min(1.0f, (de * de - L1 * L1 - L2 * L2) / (2 * L1 * L2)));
      const float knee = std::acos(ck), a = std::atan2(-L2 * std::sin(knee), L1 + L2 * ck);
      M3 F, Rl, calf_R;
      if (l.ik.mode == 1) {
        F = {d[0], yc[0], zc[0], d[1], yc[1], zc[1], d[2], yc[2], zc[2]};
        Rl = MatOf(Axis(1, a)), calf_R = MatOf(Axis(1, knee));
      } else {
        const float ud = Dot(u, d);
        const V z2 = Unit({u[0] - d[0] * ud, u[1] - d[1] * ud, u[2] - d[2] * ud}), y2 = Unit(Cross(z2, d));
        F = {d[0], y2[0], z2[0], d[1], y2[1], z2[1], d[2], y2[2], z2[2]};
        Rl = MatOf(Axis(2, -a)), calf_R = MatOf(Axis(2, -knee));
      }
      lr[th] = MulM(TransM(wr[size_t(nodes[th].parent)]), MulM(F, Rl));
      lr[ca] = calf_R;
      thigh_keys[size_t(s)].push_back(EulerOf(lr[th]));
      calf_keys[size_t(s)].push_back(EulerOf(lr[ca]));
      fk();
    }
  }
  // the new keys, at the YANM's end
  out.assign(in.begin(), in.begin() + ptrdiff_t(yend));
  std::vector<uint32_t> pof = PofRead(in, yend + 8, pof_size);
  auto keys_at = [&](const std::vector<V> &k) {
    const uint32_t at = uint32_t(out.size() - base);
    int prev = 0;
    for (size_t i = 0; i < k.size(); ++i) {
      for (int c = 0; c < 3; ++c)
        App16(out, uint16_t(S16Of(k[i][size_t(c)])));
      App16(out, uint16_t(times[i] - prev));
      prev = times[i];
    }
    return at;
  };
  for (int s = 0; s < 2; ++s) {
    if (!legs[s].on)
      continue;
    for (const auto &[node, keys] : {std::pair<int, const std::vector<V> *>{legs[s].thigh, &thigh_keys[size_t(s)]},
                                     {legs[s].calf, &calf_keys[size_t(s)]}}) {
      const Desc &dsc = ds[size_t(desc_of[size_t(node)])];
      const uint16_t drop = 0x4000 | 0x2000 | 0x1000 | 0x0800 | 0x0400 | 0x0010 | 0x0008 | 0x0004 | 0x0001;
      const uint16_t nf = uint16_t((dsc.F & ~drop) | 0x2000);
      size_t old_total = 0, new_total = 0;
      const auto old_l = yanm::detail::Layout(dsc.F, &old_total), new_l = yanm::detail::Layout(nf, &new_total);
      if (new_total > dsc.size - 8)
        return false;
      const uint32_t at = keys_at(*keys);
      Bytes body(dsc.size - 8, 0);
      std::vector<uint32_t> ptrs;
      for (size_t i = 0; i < new_l.size(); ++i) {
        const size_t sz = (i + 1 < new_l.size() ? new_l[i + 1].off : new_total) - new_l[i].off;
        if (new_l[i].bit == 0x2000) {
          Put32(body, new_l[i].off, at), Put32(body, new_l[i].off + 4, uint32_t(keys->size()));
        } else {
          for (const auto &o : old_l)
            if (o.bit == new_l[i].bit)
              memcpy(&body[new_l[i].off], &in[dsc.at + 8 + o.off], sz);
        }
        ptrs.push_back(uint32_t(dsc.at + 8 + new_l[i].off - base));
      }
      Put16(out, dsc.at, nf);
      memcpy(&out[dsc.at + 8], body.data(), body.size());
      const uint32_t lo = uint32_t(dsc.at + 8 - base), hi = uint32_t(dsc.at + dsc.size - base);
      pof.erase(std::remove_if(pof.begin(), pof.end(), [&](uint32_t p) { return p >= lo && p < hi; }), pof.end());
      pof.insert(pof.end(), ptrs.begin(), ptrs.end());
    }
  }
  std::sort(pof.begin(), pof.end());
  Put32(out, 4, uint32_t(out.size() - 8));
  const Bytes pb = PofWrite(pof);
  out.insert(out.end(), {'P', 'O', 'F', '0'});
  App32(out, uint32_t(pb.size()));
  out.insert(out.end(), pb.begin(), pb.end());
  out.insert(out.end(), in.begin() + ptrdiff_t(rest), in.end());
  if (frames_out)
    *frames_out = frames;
  return true;
}

}  // namespace detail

// A YANM record (an EVZ child): every block's IK legs baked to FK. False if nothing was baked.
inline bool BakeLegIk(Bytes &rec) {
  if (!IsRecord(rec))
    return false;
  using yanm::detail::B32;
  const uint32_t first = B32(rec, 12), n = B32(rec, 0x48);
  if (n == 0 || n > 64 || 0x70 + 0x28 * size_t(n) > first)
    return false;
  std::vector<size_t> at;
  for (uint32_t i = 0; i < n; ++i)
    at.push_back(first + B32(rec, 0x70 + 0x28 * size_t(i) + 0x24));
  Bytes out(rec.begin(), rec.begin() + first);
  bool any = false;
  for (uint32_t i = 0; i < n; ++i) {
    const size_t s = at[i], e = i + 1 < n ? at[i + 1] : rec.size();
    if (s > e || e > rec.size())
      return false;
    // the block's chunks, up to its last (an empty POF0 after the YAT2)
    size_t end = e;
    for (size_t o = s; o + 8 <= e;) {
      const bool last = !memcmp(&rec[o], "POF0", 4) && B32(rec, o + 4) == 0;
      o += 8 + B32(rec, o + 4);
      if (last) {
        end = o;
        break;
      }
    }
    end = std::min(end, e);
    const Bytes in(rec.begin() + ptrdiff_t(s), rec.begin() + ptrdiff_t(end));
    Bytes blk;
    if (detail::BakeBlock(in, blk, nullptr))
      any = true;
    else
      blk = in;
    detail::Put32(out, 0x70 + 0x28 * size_t(i) + 0x24, uint32_t(out.size() - first));
    out.insert(out.end(), blk.begin(), blk.end());
    while ((out.size() - first) % 0x20)
      out.push_back(0);
  }
  if (!any)
    return false;
  detail::Put32(out, 4, uint32_t(out.size() - first));
  rec.swap(out);
  return true;
}

// An entrance's EVZ (a PACH of records, one per object): the records with IK legs (the bodies) baked. Returns how many.
inline int BakeEntranceTracks(Bytes &evz) {
  Bytes b = svrfmt::IsBpe(evz) ? svrfmt::Unpack(evz) : evz;
  svrfmt::Archive a;
  if (!svrfmt::ArchiveOpen(b, a, nullptr))
    return 0;
  int n = 0;
  for (auto &k : a.root.kids) {
    Bytes d = svrfmt::ArchiveData(k);
    if (svrfmt::IsBpe(d))
      d = svrfmt::Unpack(d);
    if (BakeLegIk(d))
      svrfmt::ArchiveReplace(k, d), ++n;
  }
  if (n)
    evz = svrfmt::ArchiveSave(a);
  return n;
}

}  // namespace yanm::ik
