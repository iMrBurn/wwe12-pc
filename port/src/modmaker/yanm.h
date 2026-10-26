// Mod Maker: the game's motions (pac\m.pac), Yuke's YANM skeletal animation, big-endian. Ported from Kaiko's
// yanm_decode.py (WWE '13, reverse-engineered from its executable: his SvR 2011 port, port/tools; credit in
// docs/mod_maker.md). WWE '12 writes the same records.
//
// A record (a PACH leaf of m.pac, e.g. ZMVM/STAT/0/0/20): header {u32 header size, u32 total, u32 0x20, u32 first
// block, +0x20 u16 kind, +0x48 u32 blocks, +0x4C the motion's number as text}, block table at 0x70 (0x28 bytes:
// +4 its 8-character name, +0x24 its offset from the first block). A block = one YANM chunk: tracks named by a hash
// of the bone name ("Bip L Thigh"...), each with streams of keys (Euler angles, one-axis hinges, positions...).
// Rotations replace the bind rotation, positions are added to the bind translation (cm, y down), time in 60 Hz
// frames. Templated chunks (header mode != 0) have no track table: the template (by id) gives the tracks.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "svrfmt/bytes.h"

namespace yanm {

using svrfmt::Bytes;
using Q = std::array<float, 4>;  // x, y, z, w
using V = std::array<float, 3>;

inline uint32_t NameHash(const std::string &s) {  // h = rotl(h, 7) ^ c
  uint32_t h = 0;
  for (unsigned char c : s)
    h = ((h << 7) | (h >> 25)) ^ c;
  return h;
}

inline const std::vector<std::string> &BoneNames() {
  static const std::vector<std::string> n = {
      "Bip",           "Bip Pelvis",     "Bip L Thigh",    "Bip L Calf",     "Bip L Foot",    "Bip L Toe0",
      "Bip R Thigh",   "Bip R Calf",     "Bip R Foot",     "Bip R Toe0",     "Bip Spine",     "Bip Spine1",
      "Bip Neck",      "Bip Head",       "Bip L Clavicle", "Bip L UpperArm", "Bip L Forearm", "Bip L Hand",
      "Bip R Clavicle", "Bip R UpperArm", "Bip R Forearm", "Bip R Hand",    "vector",        "root"};
  return n;
}
inline std::string NameOf(uint32_t h) {
  for (const auto &n : BoneNames())
    if (NameHash(n) == h)
      return n;
  char b[16];
  snprintf(b, sizeof(b), "#%08x", h);
  return b;
}

// ---- quaternions (q_euler: q = qz * qy * qx, R = Rz Ry Rx as the models' node tables)
inline Q Mul(const Q &a, const Q &b) {
  return {a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1], a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0],
          a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3], a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2]};
}
inline Q Axis(int axis, float a) {
  Q q{0, 0, 0, std::cos(a * 0.5f)};
  q[size_t(axis)] = std::sin(a * 0.5f);
  return q;
}
inline Q Euler(float x, float y, float z) { return Mul(Mul(Axis(2, z), Axis(1, y)), Axis(0, x)); }
inline Q Normalize(Q q) {
  const float n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
  if (n > 0)
    for (auto &c : q)
      c /= n;
  return q;
}
inline Q Slerp(const Q &a, Q b, float t) {
  float d = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
  if (d < 0) {
    for (auto &c : b)
      c = -c;
    d = -d;
  }
  Q o;
  if (d > 0.9995f) {
    for (int i = 0; i < 4; ++i)
      o[size_t(i)] = a[size_t(i)] + (b[size_t(i)] - a[size_t(i)]) * t;
    return Normalize(o);
  }
  const float th = std::acos(std::min(1.0f, d)), s = std::sin(th);
  const float wa = std::sin((1 - t) * th) / s, wb = std::sin(t * th) / s;
  for (int i = 0; i < 4; ++i)
    o[size_t(i)] = wa * a[size_t(i)] + wb * b[size_t(i)];
  return o;
}

// ---- tracks
template <class T>
struct Key {
  int t;
  T v;
};
struct Hinge {
  int axis = 0;
  std::vector<Key<float>> keys;
};
struct Track {
  uint32_t hash = 0;
  std::string name;
  uint16_t flags = 0;
  std::vector<Key<V>> rot8, rot8f, rot8ff, rot16;  // Euler angles (radians)
  std::vector<Hinge> hinges;                       // hinge, hinge_f, hinge_ff (multiplied in that order)
  std::vector<Key<V>> pos;                         // offset added to the bind translation
};
struct Block {
  std::string name;  // "00000000": the person's track set ("...0001" the other person in two-person motions)
  int frames = 0;
  std::vector<Track> tracks;
};
struct Record {
  int number = 0, kind = 0;
  std::vector<Block> blocks;
};

namespace detail {

inline uint32_t B32(const Bytes &d, size_t o) { return o + 4 <= d.size() ? svrfmt::Be32(&d[o]) : 0; }
inline uint16_t B16(const Bytes &d, size_t o) { return o + 2 <= d.size() ? svrfmt::Be16(&d[o]) : 0; }
inline float BF(const Bytes &d, size_t o) { return o + 4 <= d.size() ? svrfmt::BeF(&d[o]) : 0.0f; }
inline int8_t S8(const Bytes &d, size_t o) { return o < d.size() ? int8_t(d[o]) : 0; }
inline uint8_t U8(const Bytes &d, size_t o) { return o < d.size() ? d[o] : 0; }
inline int16_t S16(const Bytes &d, size_t o) { return int16_t(B16(d, o)); }

// The track templates (header mode != 0, u16 = template id): (bone, flags), from the executable's table.
inline std::vector<std::pair<uint32_t, uint16_t>> Template(int id) {
  auto h = [](const char *n) { return NameHash(n); };
  using L = std::vector<std::pair<uint32_t, uint16_t>>;
  const L legs_full = {{h("Bip L Thigh"), 0x5000}, {h("Bip L Calf"), 0x0c00}, {h("Bip L Foot"), 0x5000},
                       {h("Bip L Toe0"), 0x0800},  {h("Bip R Thigh"), 0x5000}, {h("Bip R Calf"), 0x0c00},
                       {h("Bip R Foot"), 0x5000},  {h("Bip R Toe0"), 0x0800}};
  const L up_full = {{h("Bip Spine"), 0x4000},      {h("Bip Spine1"), 0x4000},     {h("Bip Neck"), 0x4000},
                     {h("Bip Head"), 0x4000},       {h("Bip L Clavicle"), 0x4000}, {h("Bip L UpperArm"), 0x4000},
                     {h("Bip L Forearm"), 0x0800},  {h("Bip L Hand"), 0x4000},     {h("Bip R Clavicle"), 0x4000},
                     {h("Bip R UpperArm"), 0x4000}, {h("Bip R Forearm"), 0x0800},  {h("Bip R Hand"), 0x4000}};
  const L legs_lite = {{h("Bip L Thigh"), 0x4000}, {h("Bip L Calf"), 0x0800}, {h("Bip L Foot"), 0x4000},
                       {h("Bip R Thigh"), 0x4000}, {h("Bip R Calf"), 0x0800}, {h("Bip R Foot"), 0x4000}};
  const L legs_lite_t = {{h("Bip L Thigh"), 0x4000}, {h("Bip L Calf"), 0x0800}, {h("Bip L Foot"), 0x4000},
                         {h("Bip L Toe0"), 0x0800},  {h("Bip R Thigh"), 0x4000}, {h("Bip R Calf"), 0x0800},
                         {h("Bip R Foot"), 0x4000},  {h("Bip R Toe0"), 0x0800}};
  const L up_lite = {{h("Bip Spine"), 0x4000},      {h("Bip Spine1"), 0x4000},     {h("Bip Neck"), 0x4000},
                     {h("Bip Head"), 0x4000},       {h("Bip L Clavicle"), 0x4000}, {h("Bip L UpperArm"), 0x4000},
                     {h("Bip L Forearm"), 0x0800},  {h("Bip R Clavicle"), 0x4000}, {h("Bip R UpperArm"), 0x4000},
                     {h("Bip R Forearm"), 0x0800}};
  const uint32_t timing = 0x1e796cf7, timing2 = 0x1e796cf4, x1 = 0xcd7ed4b1, x2 = 0x1639dcce;
  L o;
  auto add = [&](const L &l) { o.insert(o.end(), l.begin(), l.end()); };
  switch (id) {
    case 1: o = {{timing, 0x4000}, {h("Bip"), 0x5100}, {h("Bip Pelvis"), 0x5000}}; add(legs_full); add(up_full); break;
    case 2: o = {{timing, 0x4000}, {h("Bip"), 0x4100}, {h("Bip Pelvis"), 0x4000}}; add(legs_lite); add(up_lite); break;
    case 3: o = {{timing, 0x4000}, {h("Bip"), 0x4100}, {h("Bip Pelvis"), 0x4000}}; add(legs_lite_t); add(up_full); break;
    case 4:
      o = {{timing, 0x4000}, {h("Bip"), 0x5100}, {h("Bip Pelvis"), 0x5000}};
      add(up_full), add(legs_full), o.push_back({x1, 0x4000}), o.push_back({x2, 0x4000});
      break;
    case 5: o = {{timing, 0x4000}, {h("Bip"), 0x4100}, {h("Bip Pelvis"), 0x4000}}; add(up_lite); add(legs_lite); break;
    case 6: o = {{timing2, 0x4100}, {h("Bip"), 0x4100}, {h("Bip Pelvis"), 0x4000}}; add(up_full); add(legs_lite_t); break;
    case 7:
      o = {{timing, 0x2000}, {h("Bip"), 0x0104}, {h("Bip Pelvis"), 0x0004}, {h("Bip L Thigh"), 0x0004},
           {h("Bip L Calf"), 0x0004}, {h("Bip L Foot"), 0x0004}, {h("Bip L Toe0"), 0x2000}, {h("Bip R Thigh"), 0x0004},
           {h("Bip R Calf"), 0x0004}, {h("Bip R Foot"), 0x0004}, {h("Bip R Toe0"), 0x2000}};
      for (const auto &u : up_full)
        o.push_back({u.first, 0x2000});
      o.push_back({x1, 0x2000}), o.push_back({x2, 0x2000});
      break;
    default: return {};
  }
  o.push_back({h("vector"), uint16_t(id == 7 ? 0x2100 : 0x4100)});
  return o;
}

// The body of a track: one block per flag bit, in descending bit order; its size and each stream's offset.
struct Part {
  uint16_t bit;
  size_t off;
};
inline std::vector<Part> Layout(uint16_t F, size_t *total) {
  static const uint16_t bits[] = {0x8000, 0x4000, 0x2000, 0x1000, 0x0800, 0x0400, 0x0200,
                                  0x0100, 0x0010, 0x0008, 0x0004, 0x0002, 0x0001};
  const size_t ext = (F & 0x20) ? 1 : 0;
  std::vector<Part> out;
  size_t off = 0;
  for (uint16_t b : bits) {
    if (!(F & b))
      continue;
    size_t sz;
    if (b == 0x8000)
      sz = 24 + 12 * ext;
    else if (b == 0x0100)
      sz = 24 + 8 * ext;
    else if (b == 0x0002 || b == 0x0001)
      sz = 8;
    else
      sz = 8 + 8 * ext;
    out.push_back({b, off});
    off += sz;
  }
  if (total)
    *total = off;
  return out;
}

// Euler keys: n keys of `size` bytes, s8 (or s16) x y z then the frame delta.
inline std::vector<Key<V>> EulerKeys(const Bytes &d, size_t at, uint32_t n, bool wide, float scale) {
  std::vector<Key<V>> out;
  int t = 0;
  for (uint32_t i = 0; i < n && i < 100000; ++i) {
    const size_t k = at + (wide ? 8 : 4) * size_t(i);
    if (k + (wide ? 8 : 4) > d.size())
      break;
    V v;
    for (int c = 0; c < 3; ++c)
      v[size_t(c)] = (wide ? float(S16(d, k + 2 * size_t(c))) : float(S8(d, k + size_t(c)))) * scale;
    t += wide ? B16(d, k + 6) : U8(d, k + 3);
    out.push_back({t, v});
  }
  return out;
}

inline int LastKey(const Track &t) {
  int l = 0;
  for (const auto *v : {&t.rot8, &t.rot8f, &t.rot8ff, &t.rot16, &t.pos})
    if (!v->empty())
      l = std::max(l, v->back().t);
  for (const auto &h : t.hinges)
    if (!h.keys.empty())
      l = std::max(l, h.keys.back().t);
  return l;
}

inline bool ParseBlock(const Bytes &d, size_t y, Block &out) {
  if (y + 16 > d.size() || memcmp(&d[y], "YANM", 4))
    return false;
  const size_t base = y + 8;
  const uint8_t mode = U8(d, base + 1);
  const uint16_t cnt = B16(d, base + 2);
  const uint32_t first = B32(d, base + 4);
  struct E {
    uint32_t h;
    uint16_t F;
    size_t body;
  };
  std::vector<E> entries;
  size_t o = base + first;
  if (mode == 0) {
    for (uint32_t i = 0; i < cnt && o + 8 <= d.size(); ++i) {
      const uint16_t F = B16(d, o), sz = B16(d, o + 2);
      entries.push_back({B32(d, o + 4), F, o + 8});
      if (!sz)
        break;
      o += sz;
    }
  } else {
    auto tpl = Template(cnt);
    if (tpl.empty())
      tpl = Template(mode);
    if (tpl.empty())
      return false;
    for (const auto &[h, F] : tpl) {
      size_t size = 0;
      Layout(F, &size);
      entries.push_back({h, F, o});
      o += size;
    }
  }
  const float pi = 3.14159265f;
  for (const E &e : entries) {
    Track tr;
    tr.hash = e.h;
    tr.name = NameOf(e.h);
    tr.flags = e.F;
    for (const Part &p : Layout(e.F, nullptr)) {
      const size_t b = e.body + p.off;
      const uint32_t so = B32(d, b), n = B32(d, b + 4);
      switch (p.bit) {
        case 0x4000: tr.rot8 = EulerKeys(d, base + so, n, false, pi / 128); break;
        case 0x1000: tr.rot8f = EulerKeys(d, base + so, n, false, pi / 4608); break;
        case 0x0010: tr.rot8ff = EulerKeys(d, base + so, n, false, pi / 11520); break;
        case 0x2000: tr.rot16 = EulerKeys(d, base + so, n, true, pi / 32768); break;
        case 0x0800:
        case 0x0400:
        case 0x0008: {
          const uint16_t hn = B16(d, b + 4), ax = B16(d, b + 6);
          const float sc = p.bit == 0x0800 ? pi / 128 : p.bit == 0x0400 ? pi / 4608 : pi / 11520;
          Hinge h;
          h.axis = std::min<int>(ax, 2);
          int t = 0;
          for (uint32_t i = 0; i < hn && base + so + 2 * size_t(i) + 2 <= d.size(); ++i) {
            const size_t k = base + so + 2 * size_t(i);
            t += U8(d, k + 1);
            h.keys.push_back({t, float(S8(d, k)) * sc});
          }
          tr.hinges.push_back(std::move(h));
          break;
        }
        case 0x8000:
        case 0x0100: {
          const float s = BF(d, b + 8), bias[3] = {BF(d, b + 12), BF(d, b + 16), BF(d, b + 20)};
          const bool wide = p.bit == 0x0100;
          float acc[3] = {0, 0, 0};
          int t = 0;
          for (uint32_t i = 0; i < n && i < 100000; ++i) {
            const size_t k = base + so + (wide ? 8 : 4) * size_t(i);
            if (k + (wide ? 8 : 4) > d.size())
              break;
            V v;
            for (int c = 0; c < 3; ++c) {
              acc[c] += wide ? float(S16(d, k + 2 * size_t(c))) : float(S8(d, k + size_t(c)));
              v[size_t(c)] = acc[c] * s + bias[c];
            }
            t += wide ? B16(d, k + 6) : U8(d, k + 3);
            tr.pos.push_back({t, v});
          }
          break;
        }
        case 0x0004: {  // 20-byte keys: s16 frame delta, s16 x y z (pi / 32768), 12 bytes of tangents
          int t = 0;
          for (uint32_t i = 0; i < n && base + so + 20 * size_t(i) + 20 <= d.size(); ++i) {
            const size_t k = base + so + 20 * size_t(i);
            t += S16(d, k);
            tr.rot16.push_back({t, {S16(d, k + 2) * pi / 32768, S16(d, k + 4) * pi / 32768, S16(d, k + 6) * pi / 32768}});
          }
          break;
        }
        default: break;  // scale, IK effectors: not shown
      }
    }
    out.frames = std::max(out.frames, LastKey(tr) + 1);
    out.tracks.push_back(std::move(tr));
  }
  return true;
}

}  // namespace detail

inline bool IsRecord(const Bytes &d) {
  if (d.size() < 0x70)
    return false;
  const uint32_t first = detail::B32(d, 12);
  return detail::B32(d, 0) < d.size() && first + 4 <= d.size() && !memcmp(&d[first], "YANM", 4);
}

inline bool Read(const Bytes &d, Record &out) {
  if (!IsRecord(d))
    return false;
  out = {};
  out.kind = detail::B16(d, 0x20);
  out.number = atoi(std::string(reinterpret_cast<const char *>(&d[0x4C]), strnlen(reinterpret_cast<const char *>(&d[0x4C]), 16)).c_str());
  const uint32_t first = detail::B32(d, 12), n = detail::B32(d, 0x48);
  std::vector<std::pair<std::string, size_t>> at;
  for (uint32_t i = 0; i < n && i < 64 && 0x70 + 0x28 * size_t(i) + 0x28 <= d.size(); ++i) {
    const size_t e = 0x70 + 0x28 * size_t(i);
    at.push_back({std::string(reinterpret_cast<const char *>(&d[e + 4]), 8), first + detail::B32(d, e + 0x24)});
  }
  if (at.empty())
    at.push_back({"00000000", first});
  for (auto &[name, o] : at) {
    size_t y = o;
    if (y + 4 > d.size())
      continue;
    if (memcmp(&d[y], "YANM", 4)) {
      const char *m = "YANM";
      const auto it = std::search(d.begin() + ptrdiff_t(y), d.end(), m, m + 4);
      if (it == d.end())
        continue;
      y = size_t(it - d.begin());
    }
    Block b;
    b.name = name;
    if (detail::ParseBlock(d, y, b))
      out.blocks.push_back(std::move(b));
  }
  return !out.blocks.empty();
}

// ---- sampling (linear between keys, as the game's non-cubic path)
template <class T>
inline void Find(const std::vector<Key<T>> &k, float f, size_t &i, float &a) {
  i = 0, a = 0;
  if (k.size() < 2 || f <= float(k[0].t))
    return;
  if (f >= float(k.back().t)) {
    i = k.size() - 1;
    return;
  }
  size_t lo = 0, hi = k.size() - 1;
  while (hi - lo > 1) {
    const size_t mid = (lo + hi) / 2;
    (float(k[mid].t) <= f ? lo : hi) = mid;
  }
  i = lo;
  const float t0 = float(k[lo].t), t1 = float(k[lo + 1].t);
  a = t1 > t0 ? (f - t0) / (t1 - t0) : 0;
}
inline Q SampleEuler(const std::vector<Key<V>> &k, float f) {
  size_t i;
  float a;
  Find(k, f, i, a);
  const Q q1 = Euler(k[i].v[0], k[i].v[1], k[i].v[2]);
  if (a == 0)
    return q1;
  return Slerp(q1, Euler(k[i + 1].v[0], k[i + 1].v[1], k[i + 1].v[2]), a);
}

// The track at frame f: its local rotation (if it has one) and its translation offset (if it has one).
inline void Sample(const Track &t, float f, bool &has_rot, Q &rot, bool &has_pos, V &pos) {
  has_rot = has_pos = false;
  if (!t.rot16.empty()) {
    rot = SampleEuler(t.rot16, f), has_rot = true;
  } else if (!t.rot8.empty()) {
    rot = SampleEuler(t.rot8, f), has_rot = true;
    if (!t.rot8f.empty())
      rot = Mul(rot, SampleEuler(t.rot8f, f));
    if (!t.rot8ff.empty())
      rot = Mul(rot, SampleEuler(t.rot8ff, f));
  } else if (!t.hinges.empty() && !t.hinges[0].keys.empty()) {
    rot = {0, 0, 0, 1}, has_rot = true;
    for (const auto &h : t.hinges) {
      if (h.keys.empty())
        continue;
      size_t i = 0;
      float a = 0;
      Find(h.keys, f, i, a);
      const float v = a == 0 ? h.keys[i].v : h.keys[i].v + (h.keys[i + 1].v - h.keys[i].v) * a;
      rot = Mul(rot, Axis(h.axis, v));
    }
  }
  if (!t.pos.empty()) {
    size_t i;
    float a;
    Find(t.pos, f, i, a);
    pos = t.pos[i].v;
    if (a != 0)
      for (int c = 0; c < 3; ++c)
        pos[size_t(c)] += (t.pos[i + 1].v[size_t(c)] - pos[size_t(c)]) * a;
    has_pos = true;
  }
}

}  // namespace yanm
