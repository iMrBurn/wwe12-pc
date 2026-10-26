// An arena of SvR 2011 (the player's own file, pac/bg/bgNN.pac) made into a WWE '12 arena (docs/limit_breaking.md,
// "Arènes", versions 1 to 22 of WrestleMania 26; the steps of tools/dev/arena_convert.py, in the way of port13.h):
//  - its models (JBOY) have 180-byte mesh records where WWE '12's have 184: a word is inserted at +0x74 (WWE '12
//    keeps there a second pointer to the mesh's strips, which its background shaders need), everything behind moves
//    and the pointer list (POF0) follows;
//  - SvR 2011 counts in 10 cm, WWE '12 in cm: models, light zones (50003), visibility boxes (50020) and the light
//    banks' positions (30000 / 31010, channels 10-13) are scaled 10 times;
//  - the match lights (light sets 10-14 of the banks) are the base arena's: SvR 2011's flood the ring;
//  - its beam cones (objects 230-269 and 300-305) get WWE '12's background shaders, a third of their colour and the
//    "entrances only" flag (50001): else a white glare covers the ring;
//  - the ring (951-974 and 980, WWE '12's ropes) and its pictures, the light maps WWE '12's code looks up by name
//    (fade_lightmap00 / 01: else a dark crowd) and entry 30002 are the base arena's; SvR's extra ring pieces (957-967)
//    are left out;
//  - its own crowd (20000-20003, 20010): the people (models of the crowd packs 20000 / 20010) converted but NOT
//    scaled (their bodies have the same size in both games: scaled, giants stood in front of the camera, v18), their
//    place files (seats and areas, texts of the same packs) scaled 10 times; the base's crowd if a model can't be;
//  - a big entry changed in a few places (the texture package) keeps Yuke's own packing (bpe_like::EncodeLike):
//    an arena must not outgrow the shipped file of the arena it plays as.
//  - the collision (995, 996, 997, 999) is the base's, as for WWE '13's arenas: SvR 2011's, in 10 cm, let the wrestlers
//    walk through the barricades.
// Its placement and visibility files work as they are (scaled).
#pragma once

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "bpe_like.h"
#include "port13.h"
#include "svrfmt/archive.h"
#include "svrfmt/jboy.h"
#include "svrfmt/texture.h"

namespace port_svr_arena {

using svrfmt::Bytes;

namespace detail {

constexpr size_t kSvrMesh = 180, kW12Mesh = 184, kMeshTable = 0x40, kInsertAt = 124, kPofBase = 8;

inline uint32_t U32(const Bytes &d, size_t o) { return o + 4 <= d.size() ? svrfmt::Be32(&d[o]) : 0; }
inline void Put32(Bytes &d, size_t o, uint32_t v) {
  if (o + 4 <= d.size())
    svrfmt::PutBe32(&d[o], v);
}
inline void MulF(Bytes &d, size_t o, int n, double f) {  // (in double, as the tool the game took)
  for (int k = 0; k < n; ++k)
    if (o + 4 * size_t(k) + 4 <= d.size())
      svrfmt::PutBeF(&d[o + 4 * size_t(k)], float(svrfmt::BeF(&d[o + 4 * size_t(k)]) * f));
}

inline size_t RFind(const Bytes &d, const char *what) {
  const size_t n = strlen(what);
  for (size_t i = d.size() >= n ? d.size() - n + 1 : 0; i-- > 0;)
    if (memcmp(&d[i], what, n) == 0)
      return i;
  return std::string::npos;
}

// The model's pointer list: POF0's place and the pointers' places (absolute).
inline bool ReadPof0(const Bytes &d, size_t &at, std::vector<size_t> &locs) {
  at = RFind(d, "POF0");
  if (at == std::string::npos || at + 8 > d.size())
    return false;
  const size_t size = U32(d, at + 4), t = at + 8;
  size_t pos = 0;
  locs.clear();
  for (size_t k = 0; k < size && t + k < d.size();) {
    const uint8_t b = d[t + k];
    const int kind = b >> 6;
    size_t step;
    if (kind == 0)
      break;
    if (kind == 1) {
      step = b & 0x3F;
      k += 1;
    } else if (kind == 2) {
      step = size_t(b & 0x3F) << 8 | d[t + k + 1];
      k += 2;
    } else {
      step = size_t(b & 0x3F) << 24 | size_t(d[t + k + 1]) << 16 | size_t(d[t + k + 2]) << 8 | d[t + k + 3];
      k += 4;
    }
    pos += step * 4;
    locs.push_back(pos + kPofBase);
  }
  return true;
}

inline Bytes EncodePof0(std::vector<size_t> locs) {
  std::sort(locs.begin(), locs.end());
  Bytes body;
  size_t prev = 0;
  for (size_t loc : locs) {
    const size_t step = (loc - kPofBase - prev) / 4;
    prev = loc - kPofBase;
    if (step < 0x40) {
      body.push_back(uint8_t(0x40 | step));
    } else if (step < 0x4000) {
      body.push_back(uint8_t(0x80 | step >> 8));
      body.push_back(uint8_t(step));
    } else {
      body.push_back(uint8_t(0xC0 | step >> 24));
      body.push_back(uint8_t(step >> 16));
      body.push_back(uint8_t(step >> 8));
      body.push_back(uint8_t(step));
    }
  }
  while (body.size() % 4)
    body.push_back(0);
  Bytes out = {'P', 'O', 'F', '0', 0, 0, 0, 0};
  svrfmt::PutBe32(&out[4], uint32_t(body.size()));
  out.insert(out.end(), body.begin(), body.end());
  return out;
}

// One SvR 2011 model in the WWE '12 layout (as tools/dev/jboy.py svr_to_w12, which the game took).
inline bool SvrToW12(const Bytes &d, Bytes &out) {
  if (d.size() < 0x48 || memcmp(d.data(), "JBOY", 4) != 0 || U32(d, 0x1C) != kMeshTable)
    return false;
  const uint32_t count = U32(d, 0x18);
  const size_t size = U32(d, 4);
  size_t pof_at;
  std::vector<size_t> locs;
  if (size > d.size() || !ReadPof0(d, pof_at, locs) || pof_at < size)
    return false;
  // Insertion points (old offsets): 4 bytes at +124 of each record, then padding after the table so that what
  // follows keeps its 16-byte alignment.
  const size_t table_end = kMeshTable + count * kSvrMesh;
  std::vector<std::pair<size_t, size_t>> inserts;
  for (uint32_t i = 0; i < count; ++i)
    inserts.push_back({kMeshTable + i * kSvrMesh + kInsertAt, 4});
  const size_t pad = (16 - (4 * size_t(count)) % 16) % 16;
  if (pad)
    inserts.push_back({table_end, pad});
  std::vector<size_t> sums;
  size_t total = 0;
  for (const auto &in : inserts)
    sums.push_back(total += in.second);
  auto moved = [&](size_t off) {
    size_t i = 0;  // (data at an insertion point moves too)
    while (i < inserts.size() && inserts[i].first <= off)
      ++i;
    return off + (i ? sums[i - 1] : 0);
  };
  out.clear();
  size_t prev = 0;
  for (const auto &in : inserts) {
    out.insert(out.end(), d.begin() + ptrdiff_t(prev), d.begin() + ptrdiff_t(in.first));
    out.insert(out.end(), in.second, 0);
    prev = in.first;
  }
  out.insert(out.end(), d.begin() + ptrdiff_t(prev), d.begin() + ptrdiff_t(size));
  const size_t new_size = out.size();
  std::vector<size_t> new_locs;
  for (size_t loc : locs) {
    const size_t nl = moved(loc);
    Put32(out, nl, uint32_t(moved(U32(d, loc))));
    new_locs.push_back(nl);
  }
  // The inserted word: a second pointer to the strips (record +0x9C), as in every WWE '12 model.
  const std::set<size_t> pointers(new_locs.begin(), new_locs.end());
  for (uint32_t i = 0; i < count; ++i) {
    const size_t word = kMeshTable + i * kW12Mesh + kInsertAt, strips = kPofBase + kMeshTable + i * kW12Mesh + 0x9C;
    if (pointers.count(strips)) {
      Put32(out, word, U32(out, strips));
      new_locs.push_back(word);
    }
  }
  Put32(out, 4, uint32_t(new_size));
  Put32(out, 0x0C, uint32_t(new_size));
  out.insert(out.end(), d.begin() + ptrdiff_t(size), d.begin() + ptrdiff_t(pof_at));  // (8-byte trailer)
  const Bytes pof = EncodePof0(new_locs);
  out.insert(out.end(), pof.begin(), pof.end());
  const size_t end = pof_at + 8 + U32(d, pof_at + 4);
  if (end < d.size())
    out.insert(out.end(), d.begin() + ptrdiff_t(end), d.end());  // (whatever follows POF0)
  return true;
}

// A WWE '12-layout model's geometry times f: vertices (28 bytes: position first) of each weight block listed in
// POF0, mesh spheres (+0xA8), nodes' positions (+16) and spheres (+64).
inline bool Scale(Bytes &d, double f) {
  size_t pof_at;
  std::vector<size_t> locs;
  if (!ReadPof0(d, pof_at, locs))
    return false;
  const std::set<size_t> pointers(locs.begin(), locs.end());
  for (uint32_t i = 0; i < U32(d, 0x18); ++i) {
    const size_t rec = kPofBase + kMeshTable + i * kW12Mesh;
    const uint32_t count = U32(d, rec), blocks = U32(d, rec + 0x5C);
    const size_t arrays = kPofBase + U32(d, rec + 0x68);
    for (uint32_t b = 0; b < blocks; ++b) {
      // (skinned crowd models have 2 weight blocks but one vertex array: only POF0's words are addresses)
      if (!pointers.count(arrays + 4 * b))
        break;
      const size_t vtx = kPofBase + U32(d, arrays + 4 * b);
      if (vtx + size_t(count) * 28 > d.size())
        return false;
      for (uint32_t v = 0; v < count; ++v)
        MulF(d, vtx + v * 28, 3, f);
    }
    MulF(d, rec + 0xA8, 4, f);
  }
  const size_t nodes = kPofBase + U32(d, 0x28);
  for (uint32_t j = 0; j < U32(d, 0x20); ++j) {
    MulF(d, nodes + j * 80 + 16, 3, f);
    MulF(d, nodes + j * 80 + 64, 4, f);
  }
  return true;
}

// SvR 2011's shaders -> WWE '12's background ones (yBG_*: every WWE '12 arena uses them; yDefault is for props).
inline void BgShaders(Bytes &d) {
  static const std::map<std::string, std::string> names = {
      {"yDefault", "yBG_Default"}, {"yUVScroll", "yBG_UVScroll"}, {"yBumpMap", "yBG_BumpMap"}};
  for (uint32_t i = 0; i < U32(d, 0x18); ++i) {
    const size_t at = kPofBase + kMeshTable + i * kW12Mesh + 0x7C;
    if (at + 16 > d.size())
      return;
    const auto it = names.find(svrfmt::Name16(&d[at]));
    if (it != names.end()) {
      memset(&d[at], 0, 16);
      memcpy(&d[at], it->second.data(), it->second.size());
    }
  }
}

// Material colours (g_f4MatAmbCol, g_f4MatDifCol: name[16], u16 type 0x0D = float4, u16 size, value) times f.
inline void Dim(Bytes &d, double f) {
  for (const char *name : {"g_f4MatAmbCol", "g_f4MatDifCol"}) {
    const size_t n = strlen(name) + 1;
    for (size_t at = 0; at + n + 32 <= d.size(); ++at)
      if (memcmp(&d[at], name, n) == 0 && svrfmt::Be16(&d[at + 16]) == 0x0D)
        MulF(d, at + 20, 3, f);
  }
}

// ---- light banks (YMKs, little-endian): u32 track count at 0x110, 16-byte entries {y, x, u16 id, offset, frames,
// runtime} at 0x114, then the tracks; a frame is 26 bytes: 0x79, a repeat count, 3 floats, 12 bytes.
struct Track {
  uint8_t y, x;
  uint16_t id;
  uint32_t frames, runtime;
  Bytes data;
};

inline bool ReadTracks(const Bytes &d, std::vector<Track> &out) {
  if (d.size() < 0x114 || memcmp(d.data(), "YMKs", 4) != 0)
    return false;
  const uint32_t count = svrfmt::Le32(&d[0x110]);
  const size_t base = 0x114 + size_t(count) * 16;
  if (base > d.size())
    return false;
  std::vector<uint32_t> starts;
  for (uint32_t i = 0; i < count; ++i)
    starts.push_back(svrfmt::Le32(&d[0x114 + i * 16 + 4]));
  std::vector<uint32_t> sorted = starts;
  std::sort(sorted.begin(), sorted.end());
  out.clear();
  for (uint32_t i = 0; i < count; ++i) {
    const uint8_t *e = &d[0x114 + i * 16];
    const auto next = std::upper_bound(sorted.begin(), sorted.end(), starts[i]);
    const size_t end = next == sorted.end() ? d.size() - base : *next;
    if (base + end > d.size() || starts[i] > end)
      return false;
    out.push_back({e[0], e[1], svrfmt::Le16(e + 2), svrfmt::Le32(e + 8), svrfmt::Le32(e + 12),
                   Bytes(d.begin() + ptrdiff_t(base + starts[i]), d.begin() + ptrdiff_t(base + end))});
  }
  return true;
}

inline Bytes WriteTracks(const Bytes &head, std::vector<Track> tracks) {
  std::sort(tracks.begin(), tracks.end(), [](const Track &a, const Track &b) {
    return (uint32_t(a.id) << 16 | a.x << 8 | a.y) < (uint32_t(b.id) << 16 | b.x << 8 | b.y);
  });
  Bytes out(head.begin(), head.begin() + 0x110), body;
  svrfmt::AppLe32(out, uint32_t(tracks.size()));
  for (const auto &t : tracks) {
    out.push_back(t.y);
    out.push_back(t.x);
    out.push_back(uint8_t(t.id));
    out.push_back(uint8_t(t.id >> 8));
    svrfmt::AppLe32(out, uint32_t(body.size()));
    svrfmt::AppLe32(out, t.frames);
    svrfmt::AppLe32(out, t.runtime);
    body.insert(body.end(), t.data.begin(), t.data.end());
  }
  out.insert(out.end(), body.begin(), body.end());
  return out;
}

// Positions and aimed points (channels y = 10-13) times f; the other channels are not lengths in world units.
inline Bytes ScaleLights(const Bytes &d, float f) {
  std::vector<Track> tracks;
  if (!ReadTracks(d, tracks))
    return d;
  for (auto &t : tracks) {
    if (t.y < 10 || t.y > 13)
      continue;
    for (size_t pos = 0; pos + 26 <= t.data.size();) {
      if (t.data[pos] != 0x79) {
        ++pos;
        continue;
      }
      for (int k = 0; k < 3; ++k) {
        uint8_t *p = &t.data[pos + 2 + 4 * size_t(k)];
        const float v = svrfmt::LeF(p) * f;
        uint32_t u;
        memcpy(&u, &v, 4);
        svrfmt::PutLe32(p, u);
      }
      pos += 26;
    }
  }
  return WriteTracks(d, tracks);
}

// The bank with the base's tracks for the light sets `ids` (theirs replace, ours of those sets go).
inline Bytes MergeLights(const Bytes &ours, const Bytes &base, const std::set<int> &ids) {
  std::vector<Track> a, b;
  if (!ReadTracks(ours, a) || !ReadTracks(base, b))
    return ours;
  a.erase(std::remove_if(a.begin(), a.end(), [&](const Track &t) { return ids.count(t.id) != 0; }), a.end());
  for (const auto &t : b)
    if (ids.count(t.id))
      a.push_back(t);
  return WriteTracks(ours, a);
}

// ---- texts
inline std::vector<std::string> Lines(const Bytes &d) {
  std::vector<std::string> out;
  std::string s(d.begin(), d.end());
  size_t pos = 0;
  for (;;) {
    const size_t e = s.find("\r\n", pos);
    out.push_back(s.substr(pos, e == std::string::npos ? std::string::npos : e - pos));
    if (e == std::string::npos)
      break;
    pos = e + 2;
  }
  return out;
}
inline Bytes Join(const std::vector<std::string> &lines) {
  std::string s;
  for (size_t i = 0; i < lines.size(); ++i)
    s += (i ? "\r\n" : "") + lines[i];
  return Bytes(s.begin(), s.end());
}
inline std::vector<std::string> Words(const std::string &line) {
  std::vector<std::string> w;
  size_t i = 0;
  while (i < line.size()) {
    while (i < line.size() && isspace(static_cast<unsigned char>(line[i])))
      ++i;
    const size_t s = i;
    while (i < line.size() && !isspace(static_cast<unsigned char>(line[i])))
      ++i;
    if (i > s)
      w.push_back(line.substr(s, i - s));
  }
  return w;
}
// Always a decimal point, no exponent: the game's files write "0.0", "960.0" (a bare "0" made its reader lose its
// place, v10).
inline std::string Num(double v) {
  char b[64];
  snprintf(b, sizeof(b), "%.6f", v);
  std::string s = b;
  while (!s.empty() && s.back() == '0')
    s.pop_back();
  if (!s.empty() && s.back() == '.')
    s += '0';
  return s;
}

// 50003 (light zones: the first 4 numbers of each line) and 50020 (visibility boxes: the numbers between braces).
inline Bytes ScaleText(uint32_t id, const Bytes &d, float f) {
  auto lines = Lines(d);
  bool inside = false;
  for (auto &line : lines) {
    if (id == 50020) {
      const auto w = Words(line);
      if (w.size() == 1 && w[0] == "{") {
        inside = true;
      } else if (w.size() == 1 && w[0] == "}") {
        inside = false;
      } else if (inside) {
        std::string out = "\t";
        for (size_t i = 0; i < w.size(); ++i)
          out += (i ? " " : "") + Num(atof(w[i].c_str()) * f);
        line = out;
      }
    } else {
      const auto first = line.find_first_not_of(" \t");
      if (first == std::string::npos || line.compare(first, 2, "//") == 0 || line[first] == ';')
        continue;
      const size_t semi = line.find(';');
      const std::string body = line.substr(0, semi), comment = semi == std::string::npos ? "" : line.substr(semi);
      const auto w = Words(body);
      if (w.size() < 4)
        continue;
      std::string out;
      for (size_t i = 0; i < w.size(); ++i)
        out += (i ? "  " : "") + (i < 4 ? Num(atof(w[i].c_str()) * f) : w[i]);
      line = out + "     " + comment;
    }
  }
  return Join(lines);
}

// The crowd's place files (texts of the crowd packs 20000 / 20010): seats "A (x, z, y, w); count" (the four numbers)
// and areas ("; R" / "; NR": rows of numbers, all of them) -> scaled. (WWE '12's seats are SvR 2011's times 10.)
inline Bytes ScaleCrowdText(const Bytes &d, float f) {
  auto lines = Lines(d);
  const bool seats = std::any_of(lines.begin(), lines.end(), [](const std::string &l) {
    return l.find('(') != std::string::npos && l.find(';') != 0;
  });
  for (auto &line : lines) {
    const auto first = line.find_first_not_of(" \t");
    if (first == std::string::npos || line[first] == ';' || line[first] == '{' || line[first] == '}')
      continue;
    if (seats) {
      const size_t a = line.find('('), b = line.find(')');
      if (a == std::string::npos || b == std::string::npos || b < a)
        continue;
      std::string inner = line.substr(a + 1, b - a - 1), r;
      size_t q = 0;
      for (int i = 0;; ++i) {
        const size_t c = inner.find(',', q);
        r += (i ? ", " : "") + Num(atof(inner.substr(q, c - q).c_str()) * f);
        if (c == std::string::npos)
          break;
        q = c + 1;
      }
      line = line.substr(0, a + 1) + r + line.substr(b);
    } else {
      const auto w = Words(line);
      std::string out;
      for (size_t i = 0; i < w.size(); ++i)
        out += (i ? "  " : "") + Num(atof(w[i].c_str()) * f);
      const size_t end = line.find_last_not_of(" \t\r");
      line = out + line.substr(end + 1);  // (its lines end "    \r\r\n": the end kept)
    }
  }
  return Join(lines);
}

// 50001 (each object's flags): "e" (shown during events only: the entrances) on the given objects.
inline Bytes EntranceOnly(const Bytes &d, const std::set<int> &objects) {
  auto lines = Lines(d);
  for (auto &line : lines) {
    const auto w = Words(line);
    if (w.empty() || !std::all_of(w[0].begin(), w[0].end(), ::isdigit) || !objects.count(atoi(w[0].c_str())) ||
        std::find(w.begin() + 1, w.end(), "e") != w.end())
      continue;
    while (!line.empty() && isspace(static_cast<unsigned char>(line.back())))
      line.pop_back();
    line += " e";
  }
  return Join(lines);
}

// 50001 with the lines of objects `gone` left out and `added` lines put in (before the flags' explanation, "//..."),
// "@light" in them replaced by the first light ("l(N)") the objects gone had (removed if they had none).
inline void SwapFlagLines(Bytes &d, const std::set<uint32_t> &gone, std::vector<std::string> added) {
  const auto lines = Lines(d);
  const auto is_gone = [&](const std::vector<std::string> &w) {
    return !w.empty() && std::all_of(w[0].begin(), w[0].end(), ::isdigit) && gone.count(uint32_t(atoi(w[0].c_str())));
  };
  std::string light;
  for (const auto &line : lines) {
    const auto w = Words(line);
    if (is_gone(w))
      for (size_t i = 1; i < w.size() && light.empty(); ++i)
        if (w[i].rfind("l(", 0) == 0)
          light = w[i];
  }
  for (auto &a : added) {
    const size_t at = a.find("@light");
    if (at != std::string::npos)
      a.replace(at, 6, light);
    while (!a.empty() && a.back() == ' ')
      a.pop_back();
  }
  std::vector<std::string> out;
  bool put = false;
  for (const auto &line : lines) {
    if (is_gone(Words(line)))
      continue;
    if (!put && line.rfind("//", 0) == 0) {
      out.insert(out.end(), added.begin(), added.end());
      out.push_back("");
      put = true;
    }
    out.push_back(line);
  }
  if (!put)
    out.insert(out.end(), added.begin(), added.end());
  d = Join(out);
}

// 50020 (what is drawn from where: "NODE MY_NAME = x PARENT_NAME = y", "{", its box, "}", its number of children; the
// root VT_ROOT has no parent): the arena's nodes named `prefix...` go, the base's come in under the root, the
// children counted again (a wrong count made the game read the file askew). An object keeps the node of its name:
// WM 26's ar_fence02 was a short side piece, and WWE '12's (the logo panels all round) vanished with that box.
struct VisNode {
  std::string name, parent;
  std::vector<std::string> box;  // the lines between the braces, as written
};
inline bool ReadVis(const Bytes &d, std::vector<std::string> &head, std::vector<VisNode> &nodes) {
  const auto lines = Lines(d);
  size_t i = 0;
  for (; i < lines.size() && lines[i].rfind("NODE", 0) != 0; ++i)
    head.push_back(lines[i]);
  while (i < lines.size()) {
    const auto w = Words(lines[i]);
    if (w.empty()) {
      ++i;
      continue;
    }
    if (w[0] != "NODE" || w.size() < 4 || w[1] != "MY_NAME")
      return false;
    VisNode n;
    n.name = w[3];
    if (w.size() >= 7 && w[4] == "PARENT_NAME")
      n.parent = w[6];
    for (++i; i < lines.size() && Words(lines[i]).empty(); ++i) {
    }
    if (i >= lines.size() || Words(lines[i]) != std::vector<std::string>{"{"})
      return false;
    for (++i; i < lines.size() && Words(lines[i]) != std::vector<std::string>{"}"}; ++i)
      if (!Words(lines[i]).empty())
        n.box.push_back(lines[i]);
    for (++i; i < lines.size() && Words(lines[i]).empty(); ++i) {
    }
    ++i;  // (the count: made again on writing)
    nodes.push_back(n);
  }
  return !nodes.empty();
}
inline Bytes SwapVisNodes(const Bytes &arena, const Bytes &base, const std::string &prefix,
                          const std::set<std::string> &from_base) {
  std::vector<std::string> head, base_head;
  std::vector<VisNode> nodes, theirs;
  if (!ReadVis(arena, head, nodes) || !ReadVis(base, base_head, theirs))
    return arena;
  const std::string root = nodes[0].name;
  std::set<std::string> gone;
  for (const auto &n : nodes)
    if (n.name.rfind(prefix, 0) == 0)
      gone.insert(n.name);
  for (auto &n : nodes)  // (a node gone with children: they hang on its parent)
    while (gone.count(n.parent)) {
      for (const auto &p : nodes)
        if (p.name == n.parent) {
          n.parent = p.parent;
          break;
        }
    }
  nodes.erase(std::remove_if(nodes.begin(), nodes.end(), [&](const VisNode &n) { return gone.count(n.name) != 0; }),
              nodes.end());
  for (const auto &t : theirs)
    if (from_base.count(t.name)) {
      VisNode n = t;
      n.parent = root;
      nodes.push_back(n);
    }
  std::vector<std::string> out = head;
  for (const auto &n : nodes) {
    int children = 0;
    for (const auto &c : nodes)
      children += c.parent == n.name;
    out.push_back("NODE MY_NAME = " + n.name + (n.parent.empty() ? "" : " PARENT_NAME = " + n.parent));
    out.push_back("{");
    out.insert(out.end(), n.box.begin(), n.box.end());
    out.push_back("}");
    out.push_back(std::to_string(children));
    out.push_back("");  // (one empty line between nodes, as the game's files)
  }
  out.push_back("");
  return Join(out);
}

// ---- texture package (400): count, 12 bytes, 32-byte records {name[16], "dds", size, offset, 0}, data on 16.
// Pictures added in place: a new name takes the record of a picture no model uses (old ring pictures first; never a
// light map, titantron or movie, which the game may look up itself), its data at the end; the rest keeps its place
// (bpe_like::EncodeLike then keeps Yuke's packing).
inline int AddPictures(Bytes &pkg, const std::vector<svrfmt::BundleTexture> &add, const std::string &models) {
  if (pkg.size() < 16)
    return 0;
  const uint32_t count = svrfmt::Le32(&pkg[0]);
  std::map<std::string, size_t> records;
  std::vector<std::pair<std::string, size_t>> spare;
  for (uint32_t i = 0; i < count && 16 + i * 32 + 32 <= pkg.size(); ++i) {
    const std::string n = svrfmt::Name16(&pkg[16 + i * 32]);
    records[n] = 16 + i * 32;
    if (models.find(n + std::string(1, '\0')) == std::string::npos && n.find("light") == std::string::npos &&
        n.find("titan") == std::string::npos && n.find("mov") == std::string::npos)
      spare.push_back({n, 16 + i * 32});
  }
  std::stable_sort(spare.begin(), spare.end(), [](const auto &a, const auto &b) {
    return (a.first.rfind("rin_", 0) == 0) > (b.first.rfind("rin_", 0) == 0);
  });
  int done = 0;
  for (const auto &t : add) {
    size_t rec;
    if (records.count(t.name)) {
      rec = records[t.name];
    } else {
      if (spare.empty())
        break;
      rec = spare.front().second;
      spare.erase(spare.begin());
    }
    while (pkg.size() % 16)
      pkg.push_back(0);
    memset(&pkg[rec], 0, 32);
    memcpy(&pkg[rec], t.name.data(), std::min<size_t>(t.name.size(), 16));
    memcpy(&pkg[rec + 16], "dds", 4);
    svrfmt::PutLe32(&pkg[rec + 20], uint32_t(t.data.size()));
    svrfmt::PutLe32(&pkg[rec + 24], uint32_t(pkg.size()));
    pkg.insert(pkg.end(), t.data.begin(), t.data.end());
    ++done;
  }
  return done;
}

// New data for a leaf; a BPE one keeps its old blocks where the data hasn't moved.
inline void ReplaceLike(svrfmt::ArcNode &n, const Bytes &raw) {
  const Bytes old = n.stored;
  svrfmt::ArchiveReplace(n, raw);
  if (svrfmt::IsBpe(old))
    n.stored = bpe_like::EncodeLike(raw, old);
}

}  // namespace detail

// SvR 2011's arenas by file number (pac/bg/bgNN.pac = its stage NN; names from its string.pac, in that order) and
// their banners in its menu/MatchHD.pac (MENU/MASI, PACH entry 7001: 256 x 128, the size of WWE '12's tiles).
// Files not listed are match structures, the moveset editor's ring, test stages.
struct ArenaSvr {
  int file;
  const char *name, *banner;
};
inline constexpr ArenaSvr kArenasSvr[] = {
    {0, "SmackDown", "arena_SD"},          {1, "RAW", "arena_RAW"},
    {2, "ECW", "arena_ECW"},               {3, "Tribute to the Troops", "arena_ttt"},
    {4, "SummerSlam", "arena_slam"},       {5, "Breaking Point", "arena_BP"},
    {6, "Hell in a Cell", "arena_HinC"},   {7, "Survivor Series", "arena_Series"},
    {8, "TLC", "arena_TLC"},               {9, "Royal Rumble", "arena_RR"},
    {10, "Elimination Chamber", "arena_EC"}, {11, "WrestleMania XXVI", "arena_WM26"},
    {12, "Backlash", "arena_back"},        {13, "Judgment Day", "arena_Judg"},
    {14, "Night of Champions", "arena_NoC"}, {15, "The Bash", "arena_BASH"},
    {16, "Extreme Rules", "arena_EXT"},    {17, "Superstars", "arena_SS"},
    {18, "Bragging Rights", "arena_BR"},   {19, "Druid Arena", "arena_drui"},
};

inline const ArenaSvr *FindSvr(int file) {
  for (const auto &a : kArenasSvr)
    if (a.file == file)
      return &a;
  return nullptr;
}

// The arena's banner in SvR 2011's menu/MatchHD.pac.
inline bool Banner(const Bytes &matchhd, int file, svrfmt::Image &out) {
  const ArenaSvr *arena = FindSvr(file);
  svrfmt::Archive a;
  if (!arena || !svrfmt::ArchiveOpen(matchhd, a, nullptr))
    return false;
  for (auto &k : a.root.kids) {
    if (k.group != "MENU" || k.name != "MASI" || !svrfmt::ArchiveExpand(k))
      continue;
    for (auto &e : k.kids) {
      std::vector<svrfmt::BundleTexture> texs;
      if (e.id != 7001 || !svrfmt::BundleRead(svrfmt::ArchiveData(e), texs))
        continue;
      for (const auto &t : texs)
        if (t.name == arena->banner && svrfmt::DdsDecode(t.data, out))
          return true;
    }
  }
  return false;
}

// Its VS screen ("match card"). SvR 2011's own layout can't be taken: MENU/MnnL (YXMLBIN VER0002) reads, but WWE '12's
// VS screen code takes its elements by number (sub_82571BA8: element 30, absent) and crashed. So a WWE '12 VS screen
// (MLYE/MBnn: layout 7000, pictures 7001, 7002; MENU/MnnF: its font) wears the arena's pictures (MENU/MnnI, 7001):
//  - WWE '12 kept several of SvR 2011's VS screens (Hell in a Cell, RAW, Night of Champions, Superstars...): the one
//    whose pictures have the same names, with SvR's pictures;
//  - else Hell in a Cell's (MB09: a background, a logo, a frame under the names) with the arena's biggest picture as
//    the background, its logo (else its banner) and its plate (else the frame kept); the fire and the cage hidden.
// Each picture keeps the size and format of the one it replaces. The game serves them for the added arena.
namespace detail {

inline const svrfmt::BundleTexture *Named(const std::vector<svrfmt::BundleTexture> &t, const std::string &n) {
  for (const auto &x : t)
    if (x.name == n)
      return &x;
  return nullptr;
}

// img into the picture `like` (its size, format and mip levels).
inline Bytes LikePicture(const Bytes &like, const svrfmt::Image &img, bool fill) {
  svrfmt::DdsInfo info;
  if (!svrfmt::DdsInfoOf(like, info))
    return like;
  svrfmt::Image fit;
  if (fill) {  // (cut to its proportions, then resized)
    int cw = img.w, ch = img.w * info.h / info.w;
    if (ch > img.h) {
      ch = img.h;
      cw = img.h * info.w / info.h;
    }
    svrfmt::Image crop;
    crop.w = cw;
    crop.h = ch;
    crop.rgba.resize(size_t(cw) * ch * 4);
    for (int y = 0; y < ch; ++y)
      memcpy(&crop.rgba[size_t(y) * cw * 4], &img.rgba[(size_t((img.h - ch) / 2 + y) * img.w + (img.w - cw) / 2) * 4],
             size_t(cw) * 4);
    fit = svrfmt::Resize(crop, info.w, info.h);
  } else {  // (whole, centred on a clear picture)
    const double s = std::min(double(info.w) / img.w, double(info.h) / img.h);
    const int w = std::max(1, int(img.w * s)), h = std::max(1, int(img.h * s));
    const svrfmt::Image shrunk = svrfmt::Resize(img, w, h);
    fit.w = info.w;
    fit.h = info.h;
    fit.rgba.assign(size_t(info.w) * info.h * 4, 0);
    for (int y = 0; y < h; ++y)
      memcpy(&fit.rgba[(size_t((info.h - h) / 2 + y) * info.w + (info.w - w) / 2) * 4], &shrunk.rgba[size_t(y) * w * 4],
             size_t(w) * 4);
  }
  return svrfmt::DdsEncode(fit, info.format, info.mips > 1);
}

// Same name and same proportions.
inline bool SameShape(const svrfmt::BundleTexture *a, const svrfmt::BundleTexture &b) {
  svrfmt::DdsInfo i, j;
  return a && svrfmt::DdsInfoOf(a->data, i) && svrfmt::DdsInfoOf(b.data, j) && i.w * j.h == i.h * j.w;
}

inline Bytes Clear(const Bytes &like) {
  svrfmt::DdsInfo info;
  if (!svrfmt::DdsInfoOf(like, info))
    return like;
  svrfmt::Image img;
  img.w = info.w;
  img.h = info.h;
  img.rgba.assign(size_t(info.w) * info.h * 4, 0);
  return svrfmt::DdsEncode(img, info.format, info.mips > 1);
}

// The pictures of a VS screen (its entry 7001, at any depth).
inline svrfmt::ArcNode *Pictures(svrfmt::ArcNode &n) {
  if (n.id == 7001 && svrfmt::ArchiveKindOf(n) == "textures")
    return &n;
  if (svrfmt::ArchiveKindOf(n) == "PACH" && svrfmt::ArchiveExpand(n))
    for (auto &c : n.kids)
      if (auto *p = Pictures(c))
        return p;
  return nullptr;
}

}  // namespace detail

// A WWE '12 VS screen wearing another game's VS screen pictures (`theirs`): SvR 2011's, and the WWE 2K games' (their
// own layout has a number of elements less than WWE '12's, 14 against 15: WrestleMania 31 of 2K16 showed no names
// and a black box, 2026-10-26).
// stretched_logo: their logo is stored squeezed, drawn wide (the 2K games: WrestleMania 31's, 256 x 256 for a wide
// logo, came out narrow): made 3.4 times as wide as high again, then fitted whole.
inline bool CardWearing(const std::vector<svrfmt::BundleTexture> &theirs, const Bytes &matchhd12,
                        const svrfmt::Image &banner, Bytes &mb, Bytes &mf, std::string *template_name = nullptr,
                        bool stretched_logo = false) {
  using namespace detail;
  mb.clear();
  mf.clear();
  svrfmt::Archive w;
  if (theirs.empty() || !svrfmt::ArchiveOpen(matchhd12, w, nullptr))
    return false;
  // The WWE '12 VS screen with most of the same pictures, else Hell in a Cell's.
  const std::set<std::string> generic = {"dummy", "matchup_BG01", "matchup_BG02"};
  int pick = 9, best = 0;
  for (auto &k : w.root.kids) {
    if (k.group != "MLYE" || k.name.size() != 4 || k.name.compare(0, 2, "MB") != 0)
      continue;
    std::vector<svrfmt::BundleTexture> ours;
    if (auto *p = Pictures(k))
      svrfmt::BundleRead(svrfmt::ArchiveData(*p), ours);
    int own = 0, same = 0;
    for (const auto &t : ours)
      if (!generic.count(t.name)) {
        ++own;
        same += SameShape(Named(theirs, t.name), t);  // (a name alone isn't enough: "bg", "plate01"...)
      }
    if (own && same * 5 >= own * 3 && same > best) {
      best = same;
      pick = atoi(k.name.c_str() + 2);
    }
  }
  char nb[8], nf[8];
  snprintf(nb, sizeof(nb), "MB%02d", pick);
  snprintf(nf, sizeof(nf), "M%02dF", pick);
  svrfmt::ArcNode *card = nullptr;
  for (auto &k : w.root.kids)
    if (k.group == "MLYE" && k.name == nb)
      card = &k;
    else if (k.group == "MENU" && k.name == nf)
      mf = k.stored;
  svrfmt::ArcNode *pics = card ? Pictures(*card) : nullptr;
  std::vector<svrfmt::BundleTexture> ours;
  if (!pics || mf.empty() || !svrfmt::BundleRead(svrfmt::ArchiveData(*pics), ours))
    return false;
  const auto decode = [](const svrfmt::BundleTexture *t, svrfmt::Image &img) {
    return t && svrfmt::DdsDecode(t->data, img);
  };
  svrfmt::Image img;
  if (best) {
    for (auto &t : ours)  // (its own pictures SvR 2011's hasn't: hidden)
      if (!generic.count(t.name))
        t.data = decode(Named(theirs, t.name), img) ? LikePicture(t.data, img, true) : Clear(t.data);
  } else {
    // The background: its biggest picture; its logo: a picture named so (not a background), else its banner; its
    // plate: a picture named so, or a frame.
    const svrfmt::BundleTexture *bg = nullptr, *logo = nullptr, *plate = nullptr;
    const auto area = [](const svrfmt::BundleTexture *t) {
      svrfmt::DdsInfo i;
      return t && svrfmt::DdsInfoOf(t->data, i) ? i.w * i.h : 0;
    };
    const auto has = [](const std::string &n, const char *what) {
      std::string l = n;
      for (auto &c : l)
        c = char(tolower(static_cast<unsigned char>(c)));
      return l.find(what) != std::string::npos;
    };
    for (const auto &t : theirs) {
      if (area(&t) > area(bg))
        bg = &t;
      if (has(t.name, "logo") && !has(t.name, "bg") && area(&t) > area(logo))
        logo = &t;
      if ((has(t.name, "plate") || has(t.name, "_fr") || t.name == "fr") && !plate)
        plate = &t;
    }
    for (auto &t : ours) {
      if (t.name == "hell_bg" && decode(bg, img)) {  // (two pictures one above the other: the background twice)
        svrfmt::DdsInfo info;
        svrfmt::DdsInfoOf(t.data, info);
        svrfmt::Image half;
        half.w = info.w;
        half.h = info.h / 2;
        half.rgba.resize(size_t(half.w) * half.h * 4);
        svrfmt::Image both;
        // (filled when its proportions are near, else whole: Royal Rumble's "background" is its square logo)
        const double ratio = double(img.w) * half.h / (double(img.h) * half.w);
        svrfmt::DdsDecode(
            LikePicture(svrfmt::DdsEncode(half, svrfmt::DxtFormat::kDxt5, false), img, ratio > 0.67 && ratio < 1.5),
            half);
        both.w = info.w;
        both.h = info.h;
        both.rgba = half.rgba;
        both.rgba.insert(both.rgba.end(), half.rgba.begin(), half.rgba.end());
        t.data = svrfmt::DdsEncode(both, info.format, info.mips > 1);
      }
      else if (t.name == "logo" && stretched_logo && decode(logo, img)) {
        // (drawn 3.4 times as wide as high by WWE 2K16's VS screen: back to that, then fitted whole; stretched to the
        // template's 2:1 picture it still looked squeezed)
        t.data = LikePicture(t.data, svrfmt::Resize(img, int(img.h * 3.4), img.h), false);
      } else if (t.name == "logo" && (decode(logo, img) || (img = banner, !banner.rgba.empty())))
        t.data = LikePicture(t.data, img, false);
      else if (t.name == "hell_fr" && decode(plate, img))
        t.data = LikePicture(t.data, img, true);
      else if (t.name == "bg_fire01" || t.name == "bg_fire02" || t.name == "fire_net")
        t.data = Clear(t.data);
    }
  }
  ReplaceLike(*pics, svrfmt::BundleWrite(ours));
  // Saved as stored in its pac: the nodes down to the pictures rebuilt.
  const auto touch = [&](auto &self, svrfmt::ArcNode &n) -> bool {
    if (&n == pics)
      return true;
    for (auto &c : n.kids)
      if (self(self, c)) {
        svrfmt::ArchiveTouch(n);
        return true;
      }
    return false;
  };
  touch(touch, *card);
  svrfmt::Archive one;
  one.root = *card;
  mb = svrfmt::ArchiveSave(one);
  if (template_name)
    *template_name = nb;
  return mb.size() >= 4 && memcmp(mb.data(), "PACH", 4) == 0;
}

// The entrance graphics (location plate, name plates: pac/evt/evtgdhd.pac ETEX/00NN) of a game whose own lack them:
// the WWE 2K games' ETEX hold only the copyright plate (20) and fonts WWE '12 can't read (their plates are movies,
// pac/mov/bg_name, bg_locator), so WrestleMania 31 of 2K16 had none (2026-10-26). WWE '12's WrestleMania XXVII's
// (ETEX/0015: blue lights, chevrons and its logo, the logo's pictures 0 and 6-20 of entries 0 and 10, the steps of
// its animation) wears the arena's logo (made `logo_wide` times as wide as high, fitted whole) and its copyright
// plate (`plate`, entry 20). Its animations and fonts are WWE '12's.
inline bool EtexWearing(const Bytes &evtgd12, const svrfmt::Image &logo, double logo_wide, const svrfmt::Image &plate,
                        Bytes &out) {
  using namespace detail;
  out.clear();
  svrfmt::Archive w;
  if (logo.rgba.empty() || !svrfmt::ArchiveOpen(evtgd12, w, nullptr))
    return false;
  svrfmt::ArcNode *tpl = nullptr;
  for (auto &k : w.root.kids)
    if (k.group == "ETEX" && k.name == "0015")
      tpl = &k;
  if (!tpl || !svrfmt::ArchiveExpand(*tpl))
    return false;
  const svrfmt::Image wide = svrfmt::Resize(logo, std::max(1, int(logo.h * logo_wide)), logo.h);
  const auto wear = [](svrfmt::ArcNode &pic, const svrfmt::Image &img) {
    const Bytes d = svrfmt::ArchiveData(pic);
    if (d.size() > 128 && memcmp(d.data(), "DDS ", 4) == 0)
      svrfmt::ArchiveReplace(pic, LikePicture(d, img, false));
  };
  for (auto &e : tpl->kids) {
    if ((e.id != 0 && e.id != 10 && e.id != 20) || !svrfmt::ArchiveExpand(e))
      continue;
    for (auto &pic : e.kids) {
      if (e.id == 20 && !plate.rgba.empty())
        wear(pic, plate);
      else if (e.id != 20 && (pic.id == 0 || (pic.id >= 6 && pic.id <= 20)))
        wear(pic, wide);
    }
    svrfmt::ArchiveTouch(e);
  }
  svrfmt::ArchiveTouch(*tpl);
  svrfmt::Archive one;
  one.root = *tpl;
  out = svrfmt::ArchiveSave(one);
  return out.size() >= 4 && memcmp(out.data(), "PACH", 4) == 0;
}

inline bool Card(const Bytes &svr_matchhd, const Bytes &matchhd12, int file, const svrfmt::Image &banner, Bytes &mb,
                 Bytes &mf, std::string *template_name = nullptr) {
  using namespace detail;
  svrfmt::Archive s;
  if (!svrfmt::ArchiveOpen(svr_matchhd, s, nullptr))
    return false;
  // The arena's pictures.
  char ni[8];
  snprintf(ni, sizeof(ni), "M%02dI", file);
  std::vector<svrfmt::BundleTexture> theirs;
  for (auto &k : s.root.kids)
    if (k.group == "MENU" && k.name == ni)
      if (auto *p = Pictures(k))
        svrfmt::BundleRead(svrfmt::ArchiveData(*p), theirs);
  return CardWearing(theirs, matchhd12, banner, mb, mf, template_name);
}

struct PortInfo {
  int models = 0, models_left = 0;  // models converted, models not (unknown layout)
  int pictures_added = 0;           // ring and crowd pictures copied from the base
  bool own_crowd = false;           // its own crowd (else the base's)
  int pictures_missing = 0;         // ... that found no free record in the package
  int fences = 0;                   // barricade pieces taken from the base
  float ramp_svr = 0, ramp12 = 0;   // the ramps' heights (cm)
  bool RampsMatch() const { return (ramp_svr < 60) == (ramp12 < 60); }
};

// The SvR 2011 file and the base arena's WWE '12 file -> the WWE '12 arena.
inline bool Port(const Bytes &svr, const Bytes &base12, Bytes &out, std::string *why, PortInfo *info = nullptr) {
  using namespace detail;
  auto fail = [&](const std::string &w) {
    if (why)
      *why = w;
    return false;
  };
  if (svr.size() < 0x4000 || memcmp(svr.data(), "EPAC", 4) != 0 || memcmp(&svr[0x800], "STG ", 4) != 0)
    return fail("not an SvR 2011 arena file");
  svrfmt::Archive a, b;
  std::string e;
  if (!svrfmt::ArchiveOpen(svr, a, &e) || !svrfmt::ArchiveOpen(base12, b, &e))
    return fail("unreadable arena: " + e);
  if (a.root.kids.empty() || b.root.kids.empty())
    return fail("no arena data");
  svrfmt::ArcNode &ga = a.root.kids[0], &gb = b.root.kids[0];
  if (!svrfmt::ArchiveExpand(ga) || !svrfmt::ArchiveExpand(gb))
    return fail("unreadable arena list");
  PortInfo pi;
  const std::set<uint32_t> ring = {951, 952, 953, 956, 960, 964, 973, 974, 980};
  std::set<uint32_t> from_base = {30002, 995, 996, 997, 999};
  from_base.insert(ring.begin(), ring.end());
  // Its own crowd: the people converted (not scaled), their places scaled; else the base's.
  const std::set<uint32_t> crowd = {20000, 20001, 20002, 20003, 20010};
  pi.own_crowd = true;
  for (auto &k : ga.kids) {
    if (k.id != 20000 && k.id != 20010)
      continue;
    if (!svrfmt::ArchiveExpand(k)) {
      pi.own_crowd = false;
      break;
    }
    for (auto &c : k.kids) {
      const Bytes d = svrfmt::ArchiveData(c);
      if (d.size() >= 4 && memcmp(d.data(), "JBOY", 4) == 0) {
        Bytes m;
        if (SvrToW12(d, m))
          ReplaceLike(c, m);
        else if (U32(d, 0x18) != 0) {  // (a model without meshes is the same in both games)
          pi.own_crowd = false;
          break;
        }
      } else if (svrfmt::ArchiveKindOf(c) == "text") {
        ReplaceLike(c, ScaleCrowdText(d, 10));
      }
    }
    svrfmt::ArchiveTouch(k);
  }
  if (!pi.own_crowd)
    from_base.insert(crowd.begin(), crowd.end());
  const std::set<uint32_t> drop = {957, 958, 959, 961, 962, 963, 965, 966, 967};
  std::set<int> cones;
  for (int i = 230; i <= 269; ++i)
    cones.insert(i);
  for (int i = 300; i <= 305; ++i)
    cones.insert(i);
  ga.kids.erase(std::remove_if(ga.kids.begin(), ga.kids.end(),
                               [&](const svrfmt::ArcNode &k) { return from_base.count(k.id) || drop.count(k.id); }),
                ga.kids.end());
  // Models (also inside sub-PACHs), texts and lights.
  const auto convert = [&](auto &self, svrfmt::ArcNode &k, bool cone) -> bool {
    const Bytes d = svrfmt::ArchiveData(k);
    if (d.size() >= 4 && memcmp(d.data(), "JBOY", 4) == 0) {
      Bytes m;
      if (!SvrToW12(d, m) || !Scale(m, 10)) {
        ++pi.models_left;
        return false;
      }
      if (cone) {
        BgShaders(m);
        Dim(m, 0.3);
      }
      ReplaceLike(k, m);
      ++pi.models;
      return true;
    }
    if (d.size() >= 4 && memcmp(d.data(), "PACH", 4) == 0 && svrfmt::ArchiveExpand(k)) {
      bool any = false;
      for (auto &c : k.kids)
        any |= self(self, c, false);
      if (any)
        svrfmt::ArchiveTouch(k);
      return any;
    }
    return false;
  };
  for (auto &k : ga.kids) {
    if (pi.own_crowd && crowd.count(k.id))  // (done above)
      continue;
    if (convert(convert, k, cones.count(int(k.id)) != 0))
      continue;
    const Bytes d = svrfmt::ArchiveData(k);
    if (k.id == 50003 || k.id == 50020)
      ReplaceLike(k, ScaleText(k.id, d, 10));
    else if (k.id == 50001)
      ReplaceLike(k, EntranceOnly(d, cones));
  }
  for (uint32_t id : {30000u, 31010u}) {
    svrfmt::ArcNode *ours = nullptr, *theirs = nullptr;
    for (auto &k : ga.kids)
      if (k.id == id)
        ours = &k;
    for (auto &k : gb.kids)
      if (k.id == id)
        theirs = &k;
    if (!ours)
      continue;
    Bytes d = ScaleLights(svrfmt::ArchiveData(*ours), 10);
    if (theirs)
      d = MergeLights(d, svrfmt::ArchiveData(*theirs), {10, 11, 12, 13, 14});
    ReplaceLike(*ours, d);
  }
  for (const auto &k : gb.kids)
    if (from_base.count(k.id))
      ga.kids.push_back(k);
  // The barricades: WWE '12's code finds them by name (ar_fence01_a to _f: the walls the wrestlers stop at, the
  // pieces that break); SvR 2011's are named otherwise (ar_fence01, ar_fen_bone01...: walked through, seen on WM 26).
  // Its own go, the base's come in (they wear the arena's pictures by name: ar_fence, ar_fen_wwe...), lit as its own
  // were.
  const auto model_name = [](const svrfmt::ArcNode &k) {
    svrfmt::Model m;
    return svrfmt::JboyRead(svrfmt::ArchiveData(k), m, nullptr) ? m.name : std::string();
  };
  std::set<uint32_t> fences_gone, used;
  for (const auto &k : ga.kids)
    if (model_name(k).rfind("ar_fen", 0) == 0)
      fences_gone.insert(k.id);
  ga.kids.erase(std::remove_if(ga.kids.begin(), ga.kids.end(),
                               [&](const svrfmt::ArcNode &k) { return fences_gone.count(k.id) != 0; }),
                ga.kids.end());
  for (const auto &k : ga.kids)
    used.insert(k.id);
  Bytes base_flags;
  for (auto &k : gb.kids)
    if (k.id == 50001)
      base_flags = svrfmt::ArchiveData(k);
  const auto base_lines = Lines(base_flags);
  std::vector<std::string> fence_lines;
  std::vector<uint32_t> fence_ids;
  for (const auto &k : gb.kids) {
    if (model_name(k).rfind("ar_fen", 0) != 0)
      continue;
    uint32_t id = k.id;
    while (used.count(id) && id < 999)  // (its number is another object's in the SvR arena: the next free one)
      ++id;
    used.insert(id);
    svrfmt::ArcNode n = k;
    n.id = id;
    ga.kids.push_back(n);
    fence_ids.push_back(id);
    // Its flags as in the base, its light the arena's own fences'.
    std::string line = std::to_string(id);
    for (const auto &l : base_lines) {
      const auto w = Words(l);
      if (w.empty() || w[0] != std::to_string(k.id))
        continue;
      for (size_t i = 1; i < w.size(); ++i)
        line += " " + (w[i].rfind("l(", 0) == 0 ? std::string("@light") : w[i]);
    }
    fence_lines.push_back(line);
  }
  if (!fence_ids.empty()) {
    std::set<std::string> fence_names;
    Bytes base_vis;
    for (auto &k : gb.kids) {
      if (model_name(k).rfind("ar_fen", 0) == 0)
        fence_names.insert(model_name(k));
      if (k.id == 50020)
        base_vis = svrfmt::ArchiveData(k);
    }
    for (auto &k : ga.kids)
      if (k.id == 50001) {
        Bytes d = svrfmt::ArchiveData(k);
        SwapFlagLines(d, fences_gone, fence_lines);
        ReplaceLike(k, d);
      } else if (k.id == 50020 && !base_vis.empty()) {  // (their visibility boxes too)
        ReplaceLike(k, SwapVisNodes(svrfmt::ArchiveData(k), base_vis, "ar_fen", fence_names));
      }
  }
  pi.fences = int(fence_ids.size());
  // The base ring's pictures the arena hasn't: into its package (400), in place.
  svrfmt::ArcNode *ta = nullptr, *tb = nullptr;
  for (auto &k : ga.kids)
    if (k.id == 400)
      ta = &k;
  for (auto &k : gb.kids)
    if (k.id == 400)
      tb = &k;
  if (ta && tb) {
    Bytes pkg = svrfmt::ArchiveData(*ta);
    std::vector<svrfmt::BundleTexture> have, theirs, add;
    svrfmt::BundleRead(pkg, have);
    svrfmt::BundleRead(svrfmt::ArchiveData(*tb), theirs);
    std::set<std::string> names;
    for (const auto &t : have)
      names.insert(t.name);
    std::string models;  // every model's bytes: which pictures are still used
    const auto gather = [&](auto &self, svrfmt::ArcNode &k) -> void {
      const Bytes d = svrfmt::ArchiveData(k);
      if (d.size() >= 4 && memcmp(d.data(), "JBOY", 4) == 0)
        models.append(d.begin(), d.end()).push_back('\0');
      else if (d.size() >= 4 && memcmp(d.data(), "PACH", 4) == 0 && svrfmt::ArchiveExpand(k))
        for (auto &c : k.kids)
          self(self, c);
    };
    for (auto &k : ga.kids)
      gather(gather, k);
    // (the ring's and the crowd's: WWE '12's crowd looks its lights up by name, fade_lightmap00..., and is drawn
    // black without them)
    const auto wanted = [&](auto &self, svrfmt::ArcNode &k) -> void {
      const Bytes d = svrfmt::ArchiveData(k);
      svrfmt::Model m;
      if (d.size() >= 4 && memcmp(d.data(), "PACH", 4) == 0 && svrfmt::ArchiveExpand(k)) {
        for (auto &c : k.kids)
          self(self, c);
        return;
      }
      if (!svrfmt::JboyRead(d, m, nullptr))
        return;
      for (const auto &n : m.textures) {
        if (names.count(n))
          continue;
        // The apron's second side (WWE '12: rin_ep00 and rin_ep01, SvR 2011: rin_ep00 alone) wears the arena's own
        // apron, not the base's (seen: WM XXVII's apron on one side of WM 26's ring).
        if (n == "rin_ep01") {
          bool own = false;
          for (const auto &t : have)
            if (t.name == "rin_ep00") {
              add.push_back({n, "dds", t.data});
              own = true;
              break;
            }
          if (own) {
            names.insert(n);
            continue;
          }
        }
        for (const auto &t : theirs)
          if (t.name == n) {
            add.push_back(t);
            names.insert(n);
            break;
          }
      }
    };
    for (auto &k : gb.kids)
      if (from_base.count(k.id) || model_name(k).rfind("ar_fen", 0) == 0)  // (the ring, the crowd, the barricades)
        wanted(wanted, k);
    // The light maps WWE '12's code looks up by name in every arena (no model names them; SvR 2011 has
    // aud_lightmap instead): without them the crowd stays dark.
    for (const char *n : {"fade_lightmap00", "fade_lightmap01"})
      if (!names.count(n))
        for (const auto &t : theirs)
          if (t.name == n) {
            add.push_back(t);
            names.insert(n);
            break;
          }
    pi.pictures_added = AddPictures(pkg, add, models);
    pi.pictures_missing = int(add.size()) - pi.pictures_added;
    // No free record left for the others (ECW, Night of Champions, Royal Rumble): the package grows (written again
    // whole: Yuke's packing is lost for it, bigger, and the Mod Maker halves pictures to fit).
    if (pi.pictures_missing > 0) {
      std::vector<svrfmt::BundleTexture> all;
      if (svrfmt::BundleRead(pkg, all)) {
        all.insert(all.end(), add.begin() + pi.pictures_added, add.end());
        pkg = svrfmt::BundleWrite(all);
        pi.pictures_added += pi.pictures_missing;
        pi.pictures_missing = 0;
      }
    }
    if (pi.pictures_added)
      ReplaceLike(*ta, pkg);
  }
  pi.ramp_svr = port13::RampHeight(ga);
  pi.ramp12 = port13::RampHeight(gb);
  std::sort(ga.kids.begin(), ga.kids.end(),
            [](const svrfmt::ArcNode &x, const svrfmt::ArcNode &y) { return x.id < y.id; });
  svrfmt::ArchiveTouch(ga);
  svrfmt::ArchiveTouch(a.root);
  out = svrfmt::ArchiveSave(a);
  if (info)
    *info = pi;
  return true;
}

}  // namespace port_svr_arena
