// An arena of SvR 2011 (the player's own file, pac/bg/bgNN.pac) made into a WWE '12 arena (docs/limit_breaking.md,
// "Arènes", versions 1 to 22 of WrestleMania 26; the steps of tools/dev/arena_convert.py, in the way of port13.h):
//  - its models (JBOY) have 180-byte mesh records where WWE '12's have 184: a word is inserted at +0x74 (WWE '12
//    keeps there a second pointer to the mesh's strips, which its background shaders need), everything behind moves
//    and the pointer list (POF0) follows;
//  - SvR 2011 counts in 10 cm, WWE '12 in cm: models, light zones (50003) and the light banks' positions (30000 /
//    31010, channels 10-13) are scaled 10 times; its visibility boxes (50020) count in metres: 100 times (at 10 times,
//    stands, stage, ramp and floor came and went with the camera in the entrances, 2026-10-29);
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
#include <array>
#include <cstdio>
#include <cstring>
#include <map>
#include <tuple>
#include <set>
#include <string>
#include <vector>

#include "bpe_like.h"
#include "port13.h"
#include "strings2k.h"
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

// SvR 2011's reflecting surfaces (yReflect: a diffuse picture plus a cube map at g_fReflectAlpha 0.5) drawn as plain
// ones (yDefault: its parameters are among yReflect's): in WWE '12 Breaking Point's ramp floor came out see-through
// during the entrances (2026-10-29).
inline void NoReflect(Bytes &d) {
  for (uint32_t i = 0; i < U32(d, 0x18); ++i) {
    const size_t at = kPofBase + kMeshTable + i * kW12Mesh + 0x7C;
    if (at + 16 > d.size())
      return;
    if (svrfmt::Name16(&d[at]) == "yReflect") {
      memset(&d[at], 0, 16);
      memcpy(&d[at], "yDefault", 8);
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

// SvR 2010's (Kaiko's arena_port.py SVR2010, with his permission: names, banners in MENU/MASI). Its arena files are
// SvR 2011's kind (he plays them as they are in his SvR 2011 port): Port takes them (all 17 checked, 2026-10-31).
inline constexpr ArenaSvr kArenasSvr10[] = {
    {0, "SmackDown", "arena_SD"},         {1, "RAW", "arena_raw"},          {2, "ECW", "arena_ecw"},
    {4, "SummerSlam", "arena_SS"},        {5, "Unforgiven", "arena_UG"},    {6, "No Mercy", "arena_no"},
    {7, "Survivor Series", "arena_sur"},  {8, "Armageddon", "arena_arm"},   {9, "Royal Rumble", "arena_RR"},
    {10, "No Way Out", "arena_out"},      {11, "WrestleMania 25", "arena_WM25"}, {12, "Backlash", "arena_back"},
    {13, "Judgment Day", "arena_judg"},   {14, "Night of Champions", "arena_NoC"},
    {15, "The Great American Bash", "arena_GAB"}, {18, "Cyber Sunday", "arena_CS"}, {24, "Extreme Rules", ""},
};

// SvR 2009's and 2008's (Kaiko's arena_port.py SVR2009 / SVR2008, with his permission): made SvR 2011's by
// OldSvrArena. Their banners aren't named: the biggest picture of their VS screen's (Banner).
inline constexpr ArenaSvr kArenasSvr09[] = {
    {0, "SmackDown", ""},          {1, "RAW", ""},          {2, "ECW", ""},
    {3, "Tribute to the Troops", ""}, {4, "SummerSlam", ""}, {5, "Unforgiven", ""},
    {6, "No Mercy", ""},           {7, "Survivor Series", ""}, {8, "Armageddon", ""},
    {9, "Royal Rumble", ""},       {10, "No Way Out", ""},  {11, "WrestleMania XXIV", ""},
    {12, "Backlash", ""},          {13, "Judgment Day", ""}, {14, "Night of Champions", ""},
    {15, "The Great American Bash", ""}, {16, "One Night Stand", ""}, {17, "Cyber Sunday", ""},
    {18, "Vengeance", ""},
};
inline constexpr ArenaSvr kArenasSvr08[] = {
    {0, "SmackDown", ""},          {1, "RAW", ""},          {2, "ECW", ""},
    {3, "Sunday Night Heat", ""},  {4, "SummerSlam", ""},   {5, "Unforgiven", ""},
    {6, "No Mercy", ""},           {7, "Survivor Series", ""}, {8, "Armageddon", ""},
    {9, "Royal Rumble", ""},       {10, "No Way Out", ""},  {11, "WrestleMania 23", ""},
    {12, "Backlash", ""},          {13, "Judgment Day", ""}, {14, "Vengeance", ""},
    {15, "The Great American Bash", ""}, {16, "New Year's Revolution", ""}, {17, "One Night Stand", ""},
    {18, "December to Dismember", ""}, {19, "Cyber Sunday", ""},
};

// `year`: 2011, 2010, 2009 or 2008.
inline const ArenaSvr *FindSvr(int file, int year = 2011) {
  auto in = [&](const auto &table) -> const ArenaSvr * {
    for (const auto &a : table)
      if (a.file == file)
        return &a;
    return nullptr;
  };
  if (year == 2010)
    return in(kArenasSvr10);
  if (year == 2009)
    return in(kArenasSvr09);
  if (year == 2008)
    return in(kArenasSvr08);
  for (const auto &a : kArenasSvr)
    if (a.file == file)
      return &a;
  return nullptr;
}

// SvR 2011's DLC arenas (bg50 NXT Arena, bg51 WCW Nitro Arena) by file number, from 50 on: their names in its string
// table's arena run (id 40002 + number, as Kaiko's arena_names.py; a DLC's newer string.pac names them, the disc's
// says "ARENA 50").
inline std::map<int, std::string> DlcArenaNames(const disc::GameDisc &g) {
  std::map<int, std::string> names;
  const auto strings = strings2k::Strings(g, 40052);
  for (int n = 50; n < 70; ++n)
    if (const auto it = strings.find(uint32_t(40002 + n)); it != strings.end() && !it->second.empty() &&
                                                           it->second.rfind("ARENA ", 0) != 0) {
      std::string t;  // ("WCW NITRO ARENA" -> "WCW Nitro Arena": words of 3 letters or less kept)
      size_t w = 0;
      const std::string &u = it->second;
      for (size_t i = 0; i <= u.size(); ++i)
        if (i == u.size() || u[i] == ' ') {
          for (size_t k = w; k < i; ++k)
            t += i - w <= 3 || k == w ? u[k] : char(tolower(static_cast<unsigned char>(u[k])));
          if (i < u.size())
            t += ' ';
          w = i + 1;
        }
      names[n] = t;
    }
  return names;
}

// The arena's banner in SvR 2011's menu/MatchHD.pac.
namespace detail {
inline svrfmt::ArcNode *Pictures(svrfmt::ArcNode &n);
}
inline bool Banner(const Bytes &matchhd, int file, svrfmt::Image &out, int year = 2011) {
  const ArenaSvr *arena = FindSvr(file, year);
  svrfmt::Archive a;
  if (!arena || !svrfmt::ArchiveOpen(matchhd, a, nullptr))
    return false;
  // SvR 2008 / 2009: the biggest picture at least 256 wide of its VS screen (MENU/MnnI), as Kaiko's svr2008_convert.
  if (year < 2010) {
    char ni[8];
    snprintf(ni, sizeof(ni), "M%02dI", file);
    int best = 0;
    for (auto &k : a.root.kids)
      if (k.group == "MENU" && k.name == ni)
        if (auto *p = detail::Pictures(k)) {
          std::vector<svrfmt::BundleTexture> texs;
          svrfmt::BundleRead(svrfmt::ArchiveData(*p), texs);
          for (const auto &t : texs) {
            svrfmt::Image img;
            if (svrfmt::DdsDecode(t.data, img) && img.w >= 256 && img.w * img.h > best) {
              best = img.w * img.h;
              out = std::move(img);
            }
          }
        }
    return best > 0;
  }
  if (!*arena->banner)
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

// The pictures of a VS screen (its entry 7001, at any depth; WWE 2K17's in SHDC lists).
inline svrfmt::ArcNode *Pictures(svrfmt::ArcNode &n) {
  if (n.id == 7001 && svrfmt::ArchiveKindOf(n) == "textures")
    return &n;
  const std::string kind = svrfmt::ArchiveKindOf(n);
  if ((kind == "PACH" || kind == "SHDC") && svrfmt::ArchiveExpand(n))
    for (auto &c : n.kids)
      if (auto *p = Pictures(c))
        return p;
  return nullptr;
}

}  // namespace detail

// A WWE 2K game's own VS screen made WWE '12's (2026-10-31): its layout (7000, YXMLBIN VER0003) has WWE '12's frame
// (36 nodes in groups 1, 8, 9, 5, 6, 1, 6) but 14 resources against 15: one background picture for both sides (3000)
// and "dummy" 3001 where WWE '12 has 3000 / 3001 the two sides and "dummy" 3002, and WWE '12 takes them by rank (its
// names went missing and a black box showed, WrestleMania 31 of 2K16, 2026-10-26). 3000 is given twice (3000, 3001),
// "dummy" becomes 3002, its nodes renumbered (the second background node 3001); its font (font_sd2015m..., a version
// WWE '12 can't read) renamed font000, WWE '12 RAW's (MENU/M01F, `mf`). Its pictures (7001) and animations (7002,
// ATB0 version 3 where WWE '12's are 2) as they are, all packed again as BPE (the 2K games' ZLIB crashed WWE '12, and
// 2K17's are SHDC lists). `card`: the VS screen's node (a PACH or SHDC holding 7000-7002).
inline bool Card2KOwn(svrfmt::ArcNode &card, const Bytes &matchhd12, Bytes &mb, Bytes &mf) {
  using namespace detail;
  mb.clear();
  mf.clear();
  const auto Rd32 = [](const Bytes &b, size_t at) {
    return at + 4 <= b.size() ? uint32_t(b[at] | b[at + 1] << 8 | b[at + 2] << 16 | uint32_t(b[at + 3]) << 24) : 0u;
  };
  const auto find_id = [](auto &self, svrfmt::ArcNode &n, uint32_t id, int depth) -> svrfmt::ArcNode * {
    if (n.id == id && depth)
      return &n;
    if (depth < 4 && svrfmt::ArchiveExpand(n))
      for (auto &c : n.kids)
        if (auto *f = self(self, c, id, depth + 1))
          return f;
    return nullptr;
  };
  svrfmt::ArcNode *lay = find_id(find_id, card, 7000, 0), *pics = find_id(find_id, card, 7001, 0),
                  *anim = find_id(find_id, card, 7002, 0);
  if (!lay || !pics || !anim)
    return false;
  // (their animations: ATB0 version 3 in 2K14-2K16, 8 in 2K17, WWE '12's 2: 2K17's left to the wearing VS screen
  // until a version 3 one is seen in the game)
  {
    const Bytes a = svrfmt::ArchiveData(*anim);
    const auto at = std::search(a.begin(), a.end(), "ATB0", "ATB0" + 4);
    if (at == a.end() || size_t(at - a.begin()) + 12 > a.size() || Rd32(a, size_t(at - a.begin()) + 8) > 3)
      return false;
  }
  Bytes y = svrfmt::ArchiveData(*lay);
  if (y.size() < 0x20 || memcmp(y.data(), "YXMLBIN VER0003", 15) != 0)
    return false;
  const uint32_t nres = Rd32(y, 16), nnodes = Rd32(y, 20), ngroups = Rd32(y, 24);
  if (0x20 + size_t(nres) * 0x4C > y.size())
    return false;
  // a resource's name (+12) by its id
  const auto res_name = [&](const Bytes &b, uint32_t id) {
    for (uint32_t i = 0; i < Rd32(b, 16); ++i)
      if (0x20 + 0x4C * size_t(i + 1) <= b.size() && Rd32(b, 0x20 + 0x4C * i) == id)
        return std::string(reinterpret_cast<const char *>(&b[0x20 + 0x4C * i + 12]),
                           strnlen(reinterpret_cast<const char *>(&b[0x20 + 0x4C * i + 12]), 0x40));
    return std::string();
  };
  // every `from` (whole, NUL-ended) made `to`, where there's room (the bytes after it zero)
  const auto rename = [](Bytes &b, const std::string &from, const std::string &to) {
    for (size_t i = 0; i + from.size() < b.size(); ++i)
      if ((i == 0 || b[i - 1] == 0) && memcmp(&b[i], from.data(), from.size()) == 0 && b[i + from.size()] == 0) {
        size_t room = from.size();
        while (i + room < b.size() && b[i + room] == 0 && room < to.size() + 1)
          ++room;
        if (room < to.size() + 1)
          continue;
        memset(&b[i], 0, from.size());
        memcpy(&b[i], to.data(), to.size());
      }
  };
  // WWE 2K17's: 8 resources, 8 nodes (its frame, background and pictures, no text: it writes the names otherwise).
  // WWE '12 RAW's layout kept, wearing its frame and background (their animations, 5000 / 5001 here, 5001 / 5000
  // there) and its background picture's name, as SvR 2011's (SvrCard).
  Bytes out;
  if (nres != 14 || nnodes != 36 || ngroups != 7) {
    std::string fr, bg;
    for (uint32_t i = 0; i < nres; ++i) {
      const uint32_t id = Rd32(y, 0x20 + 0x4C * i);
      const std::string n = res_name(y, id);
      if ((id == 5000 || id == 5001) && n.size() > 3 && n.compare(n.size() - 3, 3, "_fr") == 0)
        fr = n;
      if ((id == 5000 || id == 5001) && n.size() > 3 && n.compare(n.size() - 3, 3, "_bg") == 0)
        bg = n;
    }
    const std::string back = res_name(y, 3000);
    svrfmt::Archive w12;
    svrfmt::ArcNode *raw12 = nullptr;
    if (fr.empty() || bg.empty() || back.empty() || !svrfmt::ArchiveOpen(matchhd12, w12, nullptr))
      return false;
    for (auto &k : w12.root.kids)
      if (k.group == "MLYE" && k.name == "MB01")
        raw12 = &k;
    svrfmt::ArcNode *lay12 = raw12 ? find_id(find_id, *raw12, 7000, 0) : nullptr;
    if (!lay12)
      return false;
    out = svrfmt::ArchiveData(*lay12);
    rename(out, "RAW_fr", fr);
    rename(out, "RAW_bg", bg);
    rename(out, "matchup_BG01", back);
    rename(out, "matchup_BG02", back);
  }
  if (out.empty() && (nres != 14 || nnodes != 36 || ngroups != 7))
    return false;
  if (out.empty()) {
  // its resources, 3000 twice and "dummy" 3002
  Bytes res;
  std::string font;
  for (uint32_t i = 0; i < nres; ++i) {
    Bytes r(y.begin() + long(0x20 + 0x4C * i), y.begin() + long(0x20 + 0x4C * (i + 1)));
    const uint32_t id = Rd32(r, 0);
    if (id == 1000)
      font.assign(reinterpret_cast<const char *>(&r[12]), strnlen(reinterpret_cast<const char *>(&r[12]), 0x40));
    if (id == 3001) {
      r[0] = 0xBA;  // (3002)
    }
    res.insert(res.end(), r.begin(), r.end());
    if (id == 3000) {
      r[0] = 0xB9;  // (3001: the same picture)
      res.insert(res.end(), r.begin(), r.end());
    }
  }
  if (res.size() != 15 * 0x4C || font.empty())
    return false;
  out.assign(y.begin(), y.begin() + 0x20);
  out[16] = 15;
  out.insert(out.end(), res.begin(), res.end());
  // its nodes (0x84 each, the resource's id first): "dummy" 3001 -> 3002, the second 3000 -> 3001
  size_t p = 0x20 + size_t(nres) * 0x4C;
  int backgrounds = 0;
  for (uint32_t g = 0; g < ngroups; ++g) {
    if (p + 4 > y.size())
      return false;
    const uint32_t n = Rd32(y, p);
    out.insert(out.end(), y.begin() + long(p), y.begin() + long(p + 4));
    p += 4;
    for (uint32_t i = 0; i < n; ++i, p += 0x84) {
      if (p + 0x84 > y.size())
        return false;
      Bytes node(y.begin() + long(p), y.begin() + long(p + 0x84));
      const uint32_t id = Rd32(node, 0);
      if (id == 3001)
        node[0] = 0xBA, node[1] = 0x0B;
      else if (id == 3000 && backgrounds++ == 1)
        node[0] = 0xB9, node[1] = 0x0B;
      out.insert(out.end(), node.begin(), node.end());
    }
  }
  out.insert(out.end(), y.begin() + long(p), y.end());
  // its font's name -> font000 (in place, the rest zeroed)
  for (size_t i = 0; i + font.size() <= out.size(); ++i)
    if (memcmp(&out[i], font.data(), font.size()) == 0 && (i + font.size() == out.size() || out[i + font.size()] == 0)) {
      memset(&out[i], 0, font.size());
      memcpy(&out[i], "font000", std::min<size_t>(7, font.size()));
    }
  }
  // WWE '12 RAW's VS screen (MLYE/MB01) wearing them, its packing and alignment kept; its font
  svrfmt::Archive w;
  if (!svrfmt::ArchiveOpen(matchhd12, w, nullptr))
    return false;
  svrfmt::ArcNode *raw = nullptr;
  for (auto &k : w.root.kids)
    if (k.group == "MENU" && k.name == "M01F")
      mf = k.stored;
    else if (k.group == "MLYE" && k.name == "MB01")
      raw = &k;
  if (mf.empty() || !raw || !svrfmt::ArchiveExpand(*raw))
    return false;
  int replaced = 0;
  for (auto &c : raw->kids)
    if (c.id == 7000 || c.id == 7001 || c.id == 7002) {
      svrfmt::ArchiveReplace(c, c.id == 7000 ? out : svrfmt::ArchiveData(c.id == 7001 ? *pics : *anim));
      ++replaced;
    }
  if (replaced != 3)
    return false;
  svrfmt::ArchiveTouch(*raw);
  svrfmt::Archive one;
  one.root = *raw;
  mb = svrfmt::ArchiveSave(one);
  return mb.size() >= 4 && memcmp(mb.data(), "PACH", 4) == 0;
}

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

// A WWE 2K entrance plate cut from a picture of its movie (bink2::Still): the plate on black, the arena's lights around
// it. Its rows: the ones where something lit runs across a fifth of the picture without a break (the plate's edges; the
// lights are dots), a third of their height more above and below; its columns: the lit ones in those rows. Black made
// clear (the Bink 2 movies' alpha is lost). A picture with its alpha (WWE 2K14's Bink 1 plates) is only cut to its
// opaque part. Empty if no plate is found. `rect`: where it was in the picture (x, y, w, h).
inline svrfmt::Image PlateCut(const svrfmt::Image &f, int *rect = nullptr) {
  svrfmt::Image out;
  const auto keep = [&](int x0, int y0) {
    if (rect) {
      rect[0] = x0;
      rect[1] = y0;
      rect[2] = out.w;
      rect[3] = out.h;
    }
  };
  if (f.w <= 0 || f.h <= 0 || f.rgba.size() < size_t(f.w) * f.h * 4)
    return out;
  bool alpha = false;
  for (size_t i = 3; i < f.rgba.size() && !alpha; i += 4)
    alpha = f.rgba[i] < 255;
  if (alpha) {
    int x0 = f.w, y0 = f.h, x1 = -1, y1 = -1;
    for (int y = 0; y < f.h; ++y)
      for (int x = 0; x < f.w; ++x)
        if (f.rgba[(size_t(y) * f.w + x) * 4 + 3] > 16) {
          x0 = std::min(x0, x);
          x1 = std::max(x1, x);
          y0 = std::min(y0, y);
          y1 = std::max(y1, y);
        }
    if (x1 < x0)
      return out;
    out.w = x1 - x0 + 1;
    out.h = y1 - y0 + 1;
    out.rgba.resize(size_t(out.w) * out.h * 4);
    for (int y = 0; y < out.h; ++y)
      memcpy(&out.rgba[size_t(y) * out.w * 4], &f.rgba[(size_t(y0 + y) * f.w + x0) * 4], size_t(out.w) * 4);
    // (their faint haze over the whole picture showed as a dark box in the game: cleared)
    for (size_t i = 3; i < out.rgba.size(); i += 4)
      out.rgba[i] = out.rgba[i] < 24 ? 0 : uint8_t((out.rgba[i] - 24) * 255 / 231);
    keep(x0, y0);
    return out;
  }
  const auto lum = [&](int x, int y) {
    const uint8_t *p = &f.rgba[(size_t(y) * f.w + x) * 4];
    return std::max({p[0], p[1], p[2]});
  };
  int y0 = -1, y1 = -1;
  for (int y = 0; y < f.h; ++y) {
    int run = 0, best = 0, gap = 0;
    for (int x = 0; x < f.w; ++x) {
      if (lum(x, y) > 60) {
        run += gap + 1;
        gap = 0;
      } else if (run && ++gap > 2) {
        run = gap = 0;
      }
      best = std::max(best, run);
    }
    if (best >= f.w / 5) {
      y0 = y0 < 0 ? y : y0;
      y1 = y;
    }
  }
  if (y0 < 0)
    return out;
  const int m = (y1 - y0) / 3 + 4;
  y0 = std::max(0, y0 - m);
  y1 = std::min(f.h - 1, y1 + m);
  int x0 = f.w, x1 = -1;
  for (int y = y0; y <= y1; ++y)
    for (int x = 0; x < f.w; ++x)
      if (lum(x, y) > 60) {
        x0 = std::min(x0, x);
        x1 = std::max(x1, x);
      }
  if (x1 < x0)
    return out;
  x0 = std::max(0, x0 - 4);
  x1 = std::min(f.w - 1, x1 + 4);
  out.w = x1 - x0 + 1;
  out.h = y1 - y0 + 1;
  out.rgba.resize(size_t(out.w) * out.h * 4);
  for (int y = 0; y < out.h; ++y)
    for (int x = 0; x < out.w; ++x) {
      const uint8_t *s = &f.rgba[(size_t(y0 + y) * f.w + x0 + x) * 4];
      uint8_t *d = &out.rgba[(size_t(y) * out.w + x) * 4];
      d[0] = s[0];
      d[1] = s[1];
      d[2] = s[2];
      d[3] = uint8_t(std::clamp((int(std::max({s[0], s[1], s[2]})) - 8) * 5, 0, 255));
    }
  keep(x0, y0);
  return out;
}

// A WWE 2K plate and where it goes on a 1920 x 1080 screen. The WWE 2K games show their plate movies 1.5 times as
// big, the location's at the top of the screen, the name's at the bottom (seen in WWE 2K14, 2026-10-30); WWE '12 writes
// its text at fixed places of its own, which fall about where theirs are: the name in the plate's box as it stands,
// the city further left (both moved on WWE '12's text by PlateOnScreen).
struct Plate2K {
  svrfmt::Image img;
  float x = 0, y = 0, w = 0, h = 0;
  std::vector<svrfmt::Image> intro;  // the plate coming in (its movie's pictures before `img`, cut as `img`)
};

namespace detail {

// A WWE '12 2D animation (AG2D, big-endian: +0x18 layers (count, offset; 0x20 each: w, h, picture, track, name[16]),
// +0x20 tracks (count, offset; 0x18 each: what its keys hold (bit 0: x, y), keys << 16 | fields, frames << 16 | parent
// track, its keys (keys x fields u32: frame, previous, next, then x, y...), ?, its rest (+4 x, +8 y, +16 / +20 scale))
// with its layer `name` moved and sized so it covers screen rectangle (x, y, w, h) of 1920 x 1080 at the end. Screen =
// 3 x the animation's place, 29 lower (measured in the game: WrestleMania XXVII's plates, 2026-10-30). Its keys are moved
// with it (they slide it in).
inline bool Ag2dPlaceLayer(Bytes &d, uint32_t layer, float x, float y, float w, float h);
inline Bytes Ag2dPlace(const Bytes &in, const char *name, float x, float y, float w, float h) {
  Bytes d = in;
  if (d.size() < 0x30 || memcmp(d.data(), "AG2D", 4) != 0)
    return d;
  const uint32_t nl = U32(d, 0x18), lo = U32(d, 0x1C);
  for (uint32_t i = 0; i < nl; ++i) {
    const size_t r = lo + 0x20 * size_t(i);
    if (r + 0x20 <= d.size() && strncmp(reinterpret_cast<const char *>(&d[r + 16]), name, 16) == 0)
      return Ag2dPlaceLayer(d, i, x, y, w, h) ? d : in;
  }
  return d;
}

// The layers of its logo's flip book (WrestleMania XXVII's plates: 21 layers showing pictures "logo_01" to "logo_21"
// 2 frames each, the last one staying): (layer, picture) in the order they show.
inline std::vector<std::pair<uint32_t, uint32_t>> Ag2dFlipBook(const Bytes &d) {
  std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> found;  // (first frame shown, layer, picture)
  if (d.size() < 0x30 || memcmp(d.data(), "AG2D", 4) != 0)
    return {};
  const uint32_t nn = U32(d, 0x10), no = U32(d, 0x14), nl = U32(d, 0x18), lo = U32(d, 0x1C), nt = U32(d, 0x20),
                 to = U32(d, 0x24);
  for (uint32_t i = 0; i < nl; ++i) {
    const size_t r = lo + 0x20 * size_t(i);
    const uint32_t pic = U32(d, r + 8), t = U32(d, r + 12);
    if (r + 0x20 > d.size() || pic >= nn || t >= nt || no + 16 * size_t(pic + 1) > d.size() ||
        strncmp(reinterpret_cast<const char *>(&d[no + 16 * size_t(pic)]), "logo_", 5) != 0)
      continue;
    const size_t tr = to + 0x18 * size_t(t);
    const uint32_t keys = U32(d, tr + 4) & 0xFFFF, table = U32(d, tr + 12);
    found.emplace_back(keys > 1 ? U32(d, table + 4 * (U32(d, tr + 4) >> 16)) : 0, i, pic);
  }
  std::sort(found.begin(), found.end());
  std::vector<std::pair<uint32_t, uint32_t>> out;
  for (const auto &[f, l, p] : found)
    out.emplace_back(l, p);
  return out;
}

// Layer `layer` (its index) moved and sized as Ag2dPlace's.
inline bool Ag2dPlaceLayer(Bytes &d, uint32_t layer, float x, float y, float w, float h) {
  if (d.size() < 0x30 || memcmp(d.data(), "AG2D", 4) != 0)
    return false;
  const auto f = [&](size_t o) {
    const uint32_t u = U32(d, o);
    float v;
    memcpy(&v, &u, 4);
    return v;
  };
  const auto putf = [&](size_t o, float v) {
    uint32_t u;
    memcpy(&u, &v, 4);
    Put32(d, o, u);
  };
  const uint32_t nl = U32(d, 0x18), lo = U32(d, 0x1C), nt = U32(d, 0x20), to = U32(d, 0x24);
  for (uint32_t i = layer; i < nl && i == layer; ++i) {
    const size_t r = lo + 0x20 * size_t(i);
    if (r + 0x20 > d.size())
      return false;
    const uint32_t t = U32(d, r + 12);
    if (t >= nt)
      return false;
    // the parents' place and scale (their rest: they don't move)
    float px = 0, py = 0, ps = 1;
    std::vector<uint32_t> chain;
    for (uint32_t p = U32(d, to + 0x18 * size_t(t) + 8) & 0xFFFF; p < nt && chain.size() < 16;
         p = U32(d, to + 0x18 * size_t(p) + 8) & 0xFFFF)
      chain.push_back(p);
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
      const size_t rest = U32(d, to + 0x18 * size_t(*it) + 20);
      px += ps * f(rest + 4);
      py += ps * f(rest + 8);
      ps *= f(rest + 16);
    }
    if (ps <= 0)
      return false;
    const size_t tr = to + 0x18 * size_t(t), rest = U32(d, tr + 20), table = U32(d, tr + 12);
    const float nx = ((x + w / 2) / 3 - px) / ps, ny = ((y + h / 2 - 29) / 3 - py) / ps;
    const float dx = nx - f(rest + 4), dy = ny - f(rest + 8);
    putf(rest + 4, nx);
    putf(rest + 8, ny);
    if (U32(d, tr) & 1) {
      const uint32_t keys = U32(d, tr + 4) & 0xFFFF, fields = U32(d, tr + 4) >> 16;
      for (uint32_t k = 0; k < keys; ++k) {
        const size_t o = table + 4 * (size_t(k) * fields);
        putf(o + 12, f(o + 12) + dx);
        putf(o + 16, f(o + 16) + dy);
      }
    }
    Put32(d, r, uint32_t(int32_t(w / 3 / ps + 0.5f)));
    Put32(d, r + 4, uint32_t(int32_t(h / 3 / ps + 0.5f)));
    return true;
  }
  return false;
}

}  // namespace detail

// Where WWE '12 writes the plates' text, on 1920 x 1080: it depends on the arena whose number the added one plays on
// (its base; not in the plates' files: their fonts' data are the same in every arena), measured in the game
// (2026-10-31): the name centred, the city from x = 440 (taken as centred 170 further) or centred. Others: base 2's.
struct PlateText {
  float name_x, name_y, city_x, city_y;
};
inline PlateText PlateTextFor(int base) {
  switch (base) {
    case 3:  // Superstars (SmackDown of 2K15: Brock Lesnar, Pensacola)
      return {1105, 802, 957, 162};
    case 5:  // Fatal 4-Way (RAW of 2K14: Randy Orton, The Miz, Providence)
      return {1115, 925, 610, 195};
    default:  // 2, NXT (SmackDown of 2K17: Vladimir Kozlov, Rio Rancho, Indianapolis)
      return {918, 857, 610, 215};
  }
}

// `intro`: the movie's pictures before `still` at half size (bink2::Still), cut as the plate is. `text_x/y`: where the
// 2K game writes its text on the plate as it shows it (WWE 2K17's layouts: port13::TextCentre2K17), else theirs on
// average (2K17's SmackDown, RAW, WWF Live 91...); the plate is moved so that place falls on WWE '12's text for the
// added arena's `base` (PlateTextFor; before, the text was on the plates' edges).
inline Plate2K PlateOnScreen(const svrfmt::Image &still, bool name, const std::vector<svrfmt::Image> &intro = {},
                             float text_x = -1, float text_y = -1, int base = 2) {
  Plate2K p;
  int r[4] = {};
  p.img = PlateCut(still, r);
  if (p.img.rgba.empty())
    return p;
  p.w = 1.5f * r[2];
  p.h = 1.5f * r[3];
  p.x = 1.5f * r[0];
  p.y = name ? 1080.f - 1.5f * still.h + 1.5f * r[1] : 1.5f * r[1];
  // (each kept if likely: a few 2K17 layouts give x 0 or -480, their text sliding in from the edge)
  if (text_x < 200 || text_x > 1720)
    text_x = 960;
  if (name ? text_y < 700 || text_y > 1080 : text_y < 0 || text_y > 400)
    text_y = name ? 920.f : 150.f;
  const PlateText t = PlateTextFor(base);
  p.x += (name ? t.name_x : t.city_x) - text_x;
  p.y += (name ? t.name_y : t.city_y) - text_y;
  bool alpha = false;
  for (size_t i = 3; i < still.rgba.size() && !alpha; i += 4)
    alpha = still.rgba[i] < 255;
  for (const auto &f : intro) {
    svrfmt::Image c;
    c.w = std::max(1, r[2] / 2);
    c.h = std::max(1, r[3] / 2);
    c.rgba.assign(size_t(c.w) * c.h * 4, 0);
    for (int y = 0; y < c.h; ++y)
      for (int x = 0; x < c.w; ++x) {
        const int sx = r[0] / 2 + x, sy = r[1] / 2 + y;
        if (sx >= f.w || sy >= f.h)
          continue;
        const uint8_t *s = &f.rgba[(size_t(sy) * f.w + sx) * 4];
        uint8_t *d = &c.rgba[(size_t(y) * c.w + x) * 4];
        d[0] = s[0];
        d[1] = s[1];
        d[2] = s[2];
        // (as PlateCut: the real alpha with its haze cleared, else black made clear)
        d[3] = alpha ? (s[3] < 24 ? 0 : uint8_t((s[3] - 24) * 255 / 231))
                     : uint8_t(std::clamp((int(std::max({s[0], s[1], s[2]})) - 8) * 5, 0, 255));
      }
    p.intro.push_back(std::move(c));
  }
  return p;
}

// The entrance graphics (location plate, name plates: pac/evt/evtgdhd.pac ETEX/00NN) of a game whose own lack them:
// the WWE 2K games' ETEX hold only the copyright plate (20) and fonts WWE '12 can't read (their plates are movies,
// pac/mov/bg_name, bg_locator), so WrestleMania 31 of 2K16 had none (2026-10-26). WWE '12's WrestleMania XXVII's
// (ETEX/0015: blue lights, chevrons and its logo, the logo's pictures 0 and 6-20 of entries 0 and 10, the steps of
// its animation) wears the arena's logo (made `logo_wide` times as wide as high, fitted whole) and its copyright
// plate (`plate`, entry 20). Its animations and fonts are WWE '12's.
// With the arena's own plates (`location`, `name`: PlateOnScreen of its movies), entry 10 (location) or 0 (name) shows
// only that plate, in its bar (picture 21, layer "base" of animations 10010 / 10000, moved and sized to the plate's
// place): its other pictures (logo, chevrons, light lines) are made clear, the plate having its logo.
// With their `intro` too (2026-10-31: "make them play, not old 2D pictures"), the plates come in as in their movies:
// the logo's flip book (21 layers, 2 frames each: Ag2dFlipBook) shows the intro's pictures (512 wide) and, on its
// last layer, which stays, the plate (1024 wide); all moved to the plate's place, the bar made clear.
inline bool EtexWearing(const Bytes &evtgd12, const svrfmt::Image &logo, double logo_wide, const svrfmt::Image &plate,
                        Bytes &out, const Plate2K &location = {}, const Plate2K &name = {}) {
  using namespace detail;
  out.clear();
  svrfmt::Archive w;
  // (no logo needed with both of its own plates: WWE 2K15's SmackDown had none, and its plates were left out)
  const bool both = !location.img.rgba.empty() && !name.img.rgba.empty();
  if ((logo.rgba.empty() && !both) || !svrfmt::ArchiveOpen(evtgd12, w, nullptr))
    return false;
  svrfmt::ArcNode *tpl = nullptr;
  for (auto &k : w.root.kids)
    if (k.group == "ETEX" && k.name == "0015")
      tpl = &k;
  if (!tpl || !svrfmt::ArchiveExpand(*tpl))
    return false;
  const svrfmt::Image wide =
      logo.rgba.empty() ? svrfmt::Image() : svrfmt::Resize(logo, std::max(1, int(logo.h * logo_wide)), logo.h);
  const auto wear = [](svrfmt::ArcNode &pic, const svrfmt::Image &img) {
    const Bytes d = svrfmt::ArchiveData(pic);
    if (d.size() > 128 && memcmp(d.data(), "DDS ", 4) == 0)
      svrfmt::ArchiveReplace(pic, LikePicture(d, img, false));
  };
  // the flip books' pictures in the order they show (entry 10's from animation 10010, entry 0's from 10000)
  std::map<uint32_t, std::vector<uint32_t>> flips;
  for (auto &e : tpl->kids)
    if ((e.id == 10000 || e.id == 10010) && svrfmt::ArchiveExpand(e) && !e.kids.empty())
      for (const auto &[layer, pic] : Ag2dFlipBook(svrfmt::ArchiveData(e.kids[0])))
        flips[e.id == 10010 ? 10 : 0].push_back(pic);
  // (heights a power of two, the one under the picture's: the 2K14 name plate's intro at 512 x 128 made 2 MB of plates
  // against WrestleMania XXVII's 1.1)
  const auto sized = [](const svrfmt::Image &img, int w, const svrfmt::DdsInfo &info) {
    int h = 16;
    while (h < 256 && h * 2 <= w * img.h / std::max(1, img.w))
      h *= 2;
    return svrfmt::DdsEncode(svrfmt::Resize(img, w, h), info.format, info.mips > 1);
  };
  for (auto &e : tpl->kids) {
    if (e.id != 0 && e.id != 10 && e.id != 20 && e.id != 10000 && e.id != 10010)
      continue;
    // (seen in the game, 2026-10-30: entry 10's bar goes with the city, entry 0's with the superstar's name)
    const Plate2K &own = e.id == 10 || e.id == 10010 ? location : name;
    if (e.id >= 10000) {  // its animation: the bar where the plate goes
      if (own.img.rgba.empty() || !svrfmt::ArchiveExpand(e))
        continue;
      for (auto &a : e.kids) {
        Bytes d = Ag2dPlace(svrfmt::ArchiveData(a), "base", own.x, own.y, own.w, own.h);
        if (!own.intro.empty())
          for (const auto &[layer, pic] : Ag2dFlipBook(d))
            Ag2dPlaceLayer(d, layer, own.x, own.y, own.w, own.h);
        svrfmt::ArchiveReplace(a, d);
      }
      svrfmt::ArchiveTouch(e);
      continue;
    }
    const std::vector<uint32_t> &flip = flips[e.id];
    if (!svrfmt::ArchiveExpand(e))
      continue;
    for (auto &pic : e.kids) {
      if (e.id == 20 && !plate.rgba.empty()) {
        wear(pic, plate);
      } else if (e.id != 20 && !own.img.rgba.empty()) {
        const Bytes d = svrfmt::ArchiveData(pic);
        svrfmt::DdsInfo info;
        if (d.size() <= 128 || memcmp(d.data(), "DDS ", 4) != 0 || !svrfmt::DdsInfoOf(d, info))
          continue;
        const auto at = std::find(flip.begin(), flip.end(), pic.id);
        if (!own.intro.empty() && flip.size() > 1 && at != flip.end()) {  // (the plate coming in, then the plate)
          const size_t k = size_t(at - flip.begin());
          svrfmt::ArchiveReplace(pic, k + 1 == flip.size()
                                          ? sized(own.img, 1024, info)
                                          : sized(own.intro[k * own.intro.size() / (flip.size() - 1)], 512, info));
        } else {
          svrfmt::ArchiveReplace(pic, pic.id == 21 && own.intro.empty()
                                          ? svrfmt::DdsEncode(svrfmt::Resize(own.img, info.w, info.h), info.format,
                                                              info.mips > 1)
                                          : Clear(d));
        }
      } else if (e.id != 20 && !wide.rgba.empty() && (pic.id == 0 || (pic.id >= 6 && pic.id <= 20))) {
        wear(pic, wide);
      }
    }
    svrfmt::ArchiveTouch(e);
  }
  svrfmt::ArchiveTouch(*tpl);
  svrfmt::Archive one;
  one.root = *tpl;
  out = svrfmt::ArchiveSave(one);
  return out.size() >= 4 && memcmp(out.data(), "PACH", 4) == 0;
}

// ---- SvR 2008 / 2009 entrance graphics. Their plates are 2D animations WWE '12 can't play (ATB0 version 1, as WWE
// 2K17's layouts, little-endian: +0x10 layers (count, offset; 0x20 each: w, h, picture, track, name[16]), +0x18 names,
// +0x20 tracks (count) at +0x28, +0x2C the length in frames (60 a second); a track 0x10 {channels, parent, offset of
// its channels}, a channel 0x10 {first key, end, id, keys}, a key 16 bytes {x, y, frame, 0}: channel 0 the place (of
// the layer's centre, on 640 x 448, its parents' added), 1 the scale, 2 the colour (R, G, B, A bytes: the layers
// flash on and off by it), 5 and 6 the corners of the picture shown (u, v: flipped ones 1 to 0), 8 the screen
// (640, 448)). So they are drawn here, frame by frame, from their pictures:
// SvR 2009's pac/evt/_EvtGdHD.pac ETEX/00NN (pictures entry 0, the name plate's animation 10000; 10010 the WWE
// logo crossed by streaks that opens its entrances, the same in every arena), SvR 2008's ET2D/00NN (0 and 10000).
// The name plate comes in by its flip book (as the 2K games' plates, EtexWearing); the logo animation is made the
// arena's transition movie. Seen on SvR 2009's RAW (2026-10-31): its plate slides in from the left at the bottom,
// the logo's streaks sweep the screen in 60 frames.
namespace detail {

struct Atb1Key {
  float x = 0, y = 0;
  uint8_t c[4] = {255, 255, 255, 255};
  uint32_t frame = 0;
};
struct Atb1 {
  struct Layer {
    int w, h, pic, track;
  };
  struct Track {
    int parent = -1;
    std::map<uint32_t, std::vector<Atb1Key>> ch;
  };
  std::vector<Layer> layers;
  std::vector<Track> tracks;
  int length = 0;
};

inline bool Atb1Read(const Bytes &d, Atb1 &a) {
  const auto u = [&](size_t o) { return o + 4 <= d.size() ? svrfmt::Le32(&d[o]) : 0u; };
  if (d.size() < 0x30 || memcmp(d.data(), "ATB0", 4) != 0)
    return false;
  const uint32_t nl = u(0x10), lo = u(0x14), nt = u(0x20), to = u(0x28);
  a = {};
  a.length = int(u(0x2C));
  if (nl > 512 || nt > 512 || lo + 0x20 * size_t(nl) > d.size() || to + 0x10 * size_t(nt) > d.size())
    return false;
  for (uint32_t i = 0; i < nl; ++i)
    a.layers.push_back({int(u(lo + 0x20 * i)), int(u(lo + 0x20 * i + 4)), int(u(lo + 0x20 * i + 8)),
                        int(u(lo + 0x20 * i + 12))});
  for (uint32_t t = 0; t < nt; ++t) {
    Atb1::Track tr;
    const uint32_t nc = u(to + 16 * t), off = u(to + 16 * t + 8);
    tr.parent = int(u(to + 16 * t + 4));
    for (uint32_t c = 0; c < nc && c < 32; ++c) {
      const uint32_t s = u(off + 16 * c), e = u(off + 16 * c + 4), id = u(off + 16 * c + 8), keys = u(off + 16 * c + 12);
      if (!keys || e <= s || (e - s) / keys != 16 || e > d.size())
        continue;
      auto &out = tr.ch[id];
      for (uint32_t k = 0; k < keys; ++k) {
        const size_t at = s + 16 * size_t(k);
        Atb1Key key;
        key.x = svrfmt::LeF(&d[at]);
        key.y = svrfmt::LeF(&d[at + 4]);
        memcpy(key.c, &d[at], 4);
        key.frame = u(at + 8);
        out.push_back(key);
      }
    }
    a.tracks.push_back(std::move(tr));
  }
  return !a.layers.empty();
}

// A channel at frame t: its keys in between, straight (x, y and the colour's bytes).
inline Atb1Key Atb1At(const std::vector<Atb1Key> &k, float t) {
  if (k.empty())
    return {};
  if (t <= float(k[0].frame))
    return k[0];
  for (size_t i = 1; i < k.size(); ++i)
    if (t <= float(k[i].frame)) {
      const float f = (t - float(k[i - 1].frame)) / std::max(1.0f, float(k[i].frame) - float(k[i - 1].frame));
      Atb1Key r;
      r.x = k[i - 1].x + (k[i].x - k[i - 1].x) * f;
      r.y = k[i - 1].y + (k[i].y - k[i - 1].y) * f;
      for (int c = 0; c < 4; ++c)
        r.c[c] = uint8_t(k[i - 1].c[c] + (int(k[i].c[c]) - int(k[i - 1].c[c])) * f + 0.5f);
      return r;
    }
  return k.back();
}

// One layer's place, size (640 x 448 units), colour and picture corners (uv: u0, v0, u1, v1) at frame t; false when it
// doesn't show.
inline bool Atb1Place(const Atb1 &a, const Atb1::Layer &l, float t, float &x, float &y, float &w, float &h, float c[4],
                      float *uv = nullptr) {
  std::vector<int> chain;
  for (int k = l.track; k >= 0 && size_t(k) < a.tracks.size() && chain.size() < 16; k = a.tracks[size_t(k)].parent)
    chain.push_back(k);
  x = y = 0;
  float sx = 1, sy = 1;
  c[0] = c[1] = c[2] = c[3] = 1;
  for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
    const auto &ch = a.tracks[size_t(*it)].ch;
    if (const auto p = ch.find(0); p != ch.end()) {
      const Atb1Key k = Atb1At(p->second, t);
      x += sx * k.x;
      y += sy * k.y;
    }
    if (const auto p = ch.find(1); p != ch.end()) {
      const Atb1Key k = Atb1At(p->second, t);
      sx *= k.x;
      sy *= k.y;
    }
    if (const auto p = ch.find(2); p != ch.end()) {
      const Atb1Key k = Atb1At(p->second, t);
      for (int q = 0; q < 4; ++q)
        c[q] *= k.c[q] / 255.0f;
    }
  }
  w = l.w * sx;
  h = l.h * sy;
  if (uv && l.track >= 0 && size_t(l.track) < a.tracks.size()) {
    const auto &ch = a.tracks[size_t(l.track)].ch;
    uv[0] = uv[1] = 0;
    uv[2] = uv[3] = 1;
    if (const auto p = ch.find(5); p != ch.end()) {
      const Atb1Key k = Atb1At(p->second, t);
      uv[0] = k.x;
      uv[1] = k.y;
    }
    if (const auto p = ch.find(6); p != ch.end()) {
      const Atb1Key k = Atb1At(p->second, t);
      uv[2] = k.x;
      uv[3] = k.y;
    }
  }
  return c[3] > 0.004f && std::isfinite(x) && std::isfinite(y) && std::abs(w) >= 0.5f && std::abs(h) >= 0.5f;
}

// The animation at frame t drawn over `canvas` (straight alpha), 640 x 448 units times sx, sy, moved by ox, oy.
inline void Atb1Draw(const Atb1 &a, const std::map<uint32_t, svrfmt::Image> &pics, float t, svrfmt::Image &canvas,
                     float sx, float sy, float ox, float oy) {
  for (const auto &l : a.layers) {
    const auto pic = pics.find(uint32_t(l.pic));
    float x, y, w, h, c[4], uv[4] = {0, 0, 1, 1};
    if (l.pic < 0 || pic == pics.end() || pic->second.w <= 0 || !Atb1Place(a, l, t, x, y, w, h, c, uv))
      continue;
    const svrfmt::Image &p = pic->second;
    const float dw = w * sx, dh = h * sy, x0 = ox + x * sx - dw / 2, y0 = oy + y * sy - dh / 2;
    const int ax = std::max(0, int(std::floor(std::min(x0, x0 + dw)))),
              bx = std::min(canvas.w, int(std::ceil(std::max(x0, x0 + dw))));
    const int ay = std::max(0, int(std::floor(std::min(y0, y0 + dh)))),
              by = std::min(canvas.h, int(std::ceil(std::max(y0, y0 + dh))));
    for (int py = ay; py < by; ++py)
      for (int px = ax; px < bx; ++px) {
        // (bilinear in the picture)
        const float su = ((px + 0.5f) - x0) / dw, sv = ((py + 0.5f) - y0) / dh;
        const float fu = (uv[0] + (uv[2] - uv[0]) * su) * p.w - 0.5f, fv = (uv[1] + (uv[3] - uv[1]) * sv) * p.h - 0.5f;
        if (fu < -0.5f || fv < -0.5f || fu > p.w - 0.5f || fv > p.h - 0.5f)
          continue;
        const int u0 = std::clamp(int(std::floor(fu)), 0, p.w - 1), v0 = std::clamp(int(std::floor(fv)), 0, p.h - 1);
        const int u1 = std::min(u0 + 1, p.w - 1), v1 = std::min(v0 + 1, p.h - 1);
        const float du = std::clamp(fu - u0, 0.0f, 1.0f), dv = std::clamp(fv - v0, 0.0f, 1.0f);
        float s[4];
        for (int q = 0; q < 4; ++q) {
          const auto at = [&](int uu, int vv) { return float(p.rgba[(size_t(vv) * p.w + uu) * 4 + q]); };
          s[q] = (at(u0, v0) * (1 - du) + at(u1, v0) * du) * (1 - dv) + (at(u0, v1) * (1 - du) + at(u1, v1) * du) * dv;
        }
        const float sa = s[3] / 255 * c[3];
        if (sa <= 0)
          continue;
        uint8_t *d = &canvas.rgba[(size_t(py) * canvas.w + px) * 4];
        const float da = d[3] / 255.0f, oa = sa + da * (1 - sa);
        for (int q = 0; q < 3; ++q)
          d[q] = uint8_t(std::clamp((s[q] * c[q] * sa + d[q] * da * (1 - sa)) / oa, 0.0f, 255.0f));
        d[3] = uint8_t(std::clamp(oa * 255, 0.0f, 255.0f));
      }
  }
}

inline svrfmt::Image Blank(int w, int h) {
  svrfmt::Image i;
  i.w = w;
  i.h = h;
  i.rgba.assign(size_t(w) * h * 4, 0);
  return i;
}

inline svrfmt::Image Crop(const svrfmt::Image &f, int x, int y, int w, int h) {
  svrfmt::Image c = Blank(std::max(1, w), std::max(1, h));
  for (int r = 0; r < c.h; ++r)
    for (int q = 0; q < c.w; ++q)
      if (x + q >= 0 && y + r >= 0 && x + q < f.w && y + r < f.h)
        memcpy(&c.rgba[(size_t(r) * c.w + q) * 4], &f.rgba[(size_t(y + r) * f.w + x + q) * 4], 4);
  return c;
}

}  // namespace detail

// SvR 2008 / 2009's entrance graphics for arena `file` (`year` 2008 or 2009) from their _EvtGdHD.pac: `name` its name
// plate as the 2K ones (Plate2K: its picture held, its flip book coming in, placed so its middle bar, where the game
// writes the name, falls on WWE '12's text for `base`: PlateTextFor), `transition` (2009) the logo animation's
// frames, 30 a second, at 320 x 320 (WWE '12's transition movies are square, shown over the whole screen).
inline bool OldSvrPlates(const Bytes &evtgd, int file, int year, int base, Plate2K &name,
                         std::vector<svrfmt::Image> *transition = nullptr) {
  using namespace detail;
  name = {};
  svrfmt::Archive a;
  if (!svrfmt::ArchiveOpen(evtgd, a, nullptr))
    return false;
  char want[8];
  snprintf(want, sizeof(want), "%04d", file);
  svrfmt::ArcNode *e = nullptr;
  for (auto &k : a.root.kids)
    if (k.group == (year == 2008 ? "ET2D" : "ETEX") && k.name == want)
      e = &k;
  if (!e || !svrfmt::ArchiveExpand(*e))
    return false;
  std::map<uint32_t, std::map<uint32_t, svrfmt::Image>> pics;  // entry -> picture number -> picture
  std::map<uint32_t, Atb1> anims;
  for (auto &k : e->kids) {
    if ((k.id != 0 && k.id != 10 && k.id != 10000 && k.id != 10010) || !svrfmt::ArchiveExpand(k))
      continue;
    for (const auto &p : k.kids) {
      const Bytes d = svrfmt::ArchiveData(p);
      svrfmt::Image img;
      if (k.id < 10000 && svrfmt::DdsDecode(d, img))
        pics[k.id][p.id] = std::move(img);
      else if (k.id >= 10000 && p.id == 0)
        Atb1Read(d, anims[k.id]);
    }
  }
  // The name plate, on 1280 x 720 (640 x 448 stretched to it, as the game showed it) with room round it: its frames'
  // light (the layers' alpha times their area) for where it comes in and where it holds.
  if (const auto an = anims.find(10000); an != anims.end() && !an->second.layers.empty() && an->second.length > 4) {
    const Atb1 &anim = an->second;
    const float sx = 2.0f, sy = 720.0f / 448;
    const int W = 1920, H = 1080, ox = 320, oy = 180;
    const auto light = [&](float t) {
      double sum = 0;
      for (const auto &l : anim.layers) {
        float x, y, w, h, c[4];
        if (l.pic >= 0 && pics[0].count(uint32_t(l.pic)) && Atb1Place(anim, l, t, x, y, w, h, c))
          sum += double(c[3]) * std::abs(w * h);
      }
      return sum;
    };
    int start = 0;
    while (start < anim.length && light(float(start)) <= 0)
      ++start;
    // where it holds: of the sets of layers showing that last (a tenth of the time at least, not counting the coming
    // in and going out: the first and last fifths), the one showing the most (its middle frame); some plates spend
    // most of their time half shown (SvR 2009's Vengeance: a post alone)
    std::map<std::vector<bool>, std::vector<int>> states;
    for (int t = start + anim.length / 5; t < anim.length - anim.length / 5; ++t) {
      std::vector<bool> on;
      for (const auto &l : anim.layers) {
        float x, y, w, h, c[4];
        on.push_back(l.pic >= 0 && Atb1Place(anim, l, float(t), x, y, w, h, c) && c[3] > 0.5f);
      }
      states[on].push_back(t);
    }
    int hold = anim.length / 2;
    size_t total = 0;
    for (const auto &[on, ts] : states)
      total += ts.size();
    double best = 0;
    for (const auto &[on, ts] : states)
      if (ts.size() >= std::max<size_t>(6, total / 10) && light(float(ts[ts.size() / 2])) > best) {
        best = light(float(ts[ts.size() / 2]));
        hold = ts[ts.size() / 2];
      }
    if (light(float(hold)) <= 0)
      for (int t = start; t < anim.length; ++t)
        if (light(float(t)) > light(float(hold)))
          hold = t;
    svrfmt::Image still = Blank(W, H);
    Atb1Draw(anim, pics[0], float(hold), still, sx, sy, float(ox), float(oy));
    int x0 = W, y0 = H, x1 = -1, y1 = -1;
    for (int y = 0; y < H; ++y)
      for (int x = 0; x < W; ++x)
        if (still.rgba[(size_t(y) * W + x) * 4 + 3] > 16) {
          x0 = std::min(x0, x);
          x1 = std::max(x1, x);
          y0 = std::min(y0, y);
          y1 = std::max(y1, y);
        }
    if (x1 > x0 && y1 > y0) {
      x0 &= ~1;
      y0 &= ~1;
      const int w = (x1 - x0 + 2) & ~1, h = (y1 - y0 + 2) & ~1;
      name.img = Crop(still, x0, y0, w, h);
      name.w = 1.5f * w;
      name.h = 1.5f * h;
      // (the name on the bar: the plate's middle, a little right of it past the show's logo on its left)
      const PlateText t = PlateTextFor(base);
      name.x = t.name_x - 1.5f * 0.6f * w;
      name.y = t.name_y - 1.5f * 0.5f * h;
      // its coming in: 20 pictures from the first frame it shows, every other frame (the flip book shows each 2
      // frames at 60 a second), at half size, cut as the plate
      for (int k = 0; k < 20 && start + 2 * k < hold; ++k) {
        svrfmt::Image half = Blank(W / 2, H / 2);
        Atb1Draw(anim, pics[0], float(start + 2 * k), half, sx / 2, sy / 2, ox / 2.0f, oy / 2.0f);
        name.intro.push_back(Crop(half, x0 / 2, y0 / 2, w / 2, h / 2));
      }
    }
  }
  // The logo animation (SvR 2009): 640 x 448 on the whole screen (its centre at 256, 224: drawn as on a 512-wide
  // screen), then squeezed square.
  if (transition)
    if (const auto an = anims.find(10010); an != anims.end() && an->second.length > 4 && pics.count(10)) {
      transition->clear();
      for (int t = 0; t < an->second.length; t += 2) {
        svrfmt::Image f = Blank(1280, 720);
        Atb1Draw(an->second, pics[10], float(t), f, 2.5f, 720.0f / 448, 0, 0);
        transition->push_back(svrfmt::Resize(f, 320, 320));
      }
    }
  return !name.img.rgba.empty() || (transition && !transition->empty());
}

// The arena's own SvR 2011 entrance graphics (pac/evt/evtgdhd.pac ETEX/00NN: the same entries as WWE '12's, pictures
// 0-90, 2D animations 10000-10040, fonts 20000-20011). Their 2D animations (AG2D: {"AG2D", size, 0x10001, frames,
// names (count, offset), layers (0x20: w, h, parent, track, name), tracks (0x18: ?, rows << 16 | columns, frames << 16
// | parent, offset in the table, flags, offset of its keys), table (rows x columns u32 per track)}, then the keys)
// are SvR's but for their keys: a WWE '12 key is 108 bytes and every track has the 0x1000000 flag; SvR 2011's are 96
// bytes with that flag {frame, x, y, angle, scale x, y, colour, 3 x ?, u0, v0, u1, v1, pivot x, y, 8 x 0}, else 44
// {frame, x, y, angle, scale x, y, colour, u0, v0, u1, v1}. WWE '12's has three more words before u0 (0, 0, 1.0;
// some of its own have 0x64 and flags there). Seen in the game as they were (2026-10-29, Breaking Point): the plates'
// pictures out of place, upside down and too big, their text right.
namespace detail {

inline Bytes Ag2dToW12(const Bytes &d) {
  if (d.size() < 0x30 || memcmp(d.data(), "AG2D", 4) != 0)
    return d;
  const uint32_t nt = U32(d, 0x20), to = U32(d, 0x24), nk = U32(d, 0x28), ko = U32(d, 0x2C);
  const size_t keys = size_t(ko) + 4 * size_t(nk);
  if (keys > d.size() || to + 0x18 * size_t(nt) > keys)
    return d;
  std::vector<uint32_t> offs;
  for (uint32_t i = 0; i < nt; ++i)
    offs.push_back(U32(d, to + 0x18 * i + 20));
  Bytes out(d.begin(), d.begin() + keys);
  for (uint32_t i = 0; i < nt; ++i) {
    const size_t rec = to + 0x18 * i;
    const bool ext = U32(d, rec + 16) & 0x1000000;
    const size_t size = ext ? 96 : 44;
    size_t end = d.size();
    for (uint32_t o : offs)
      if (o > offs[i] && o < end)
        end = o;
    if (offs[i] < keys || end > d.size())
      return d;
    Put32(out, rec + 16, U32(d, rec + 16) | 0x1000000);
    Put32(out, rec + 20, uint32_t(out.size()));
    for (size_t k = offs[i]; k + size <= end; k += size) {
      uint32_t w[27] = {};
      const auto in = [&](int j) { return U32(d, k + 4 * j); };
      for (int j = 0; j < 7; ++j)
        w[j] = in(j);
      w[12] = 0x3F800000;  // 1.0
      if (ext) {
        for (int j = 7; j < 10; ++j)
          w[j] = in(j);
        for (int j = 10; j < 24; ++j)
          w[j + 3] = in(j);
      } else {
        for (int j = 7; j < 11; ++j)
          w[j + 6] = in(j);
        w[17] = w[18] = 0x3F000000;  // pivot 0.5
      }
      const size_t at = out.size();
      out.resize(at + sizeof(w));
      for (int j = 0; j < 27; ++j)
        Put32(out, at + 4 * j, w[j]);
    }
  }
  Put32(out, 4, uint32_t(out.size()));
  return out;
}

}  // namespace detail

inline bool SvrEtex(const Bytes &evtgdhd_svr, int file, Bytes &out) {
  using namespace detail;
  out.clear();
  svrfmt::Archive a;
  if (!svrfmt::ArchiveOpen(evtgdhd_svr, a, nullptr))
    return false;
  char name[8];
  snprintf(name, sizeof(name), "%04d", file);
  for (auto &k : a.root.kids) {
    if (k.group != "ETEX" || k.name != name || !svrfmt::ArchiveExpand(k))
      continue;
    std::vector<svrfmt::PachEntry> entries;
    for (const auto &c : k.kids) {
      Bytes raw = svrfmt::ArchiveData(c);
      std::vector<svrfmt::PachEntry> kids;
      if (c.id >= 10000 && c.id < 20000 && svrfmt::PachRead(raw, kids)) {
        for (auto &kid : kids) {
          const Bytes was = svrfmt::Unpack(kid.data);
          if (was.size() >= 4 && memcmp(was.data(), "AG2D", 4) == 0)
            kid.data = svrfmt::BpeEncode(Ag2dToW12(was));
        }
        raw = svrfmt::PachWrite(kids);
      }
      if (raw.empty())
        return false;
      entries.push_back({c.id, svrfmt::BpeEncode(raw)});
    }
    out = svrfmt::PachWrite(entries);
    return !entries.empty();
  }
  return false;
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

// The arena's own SvR 2011 VS screen, after Kaiko's notes (VS_STYLES.md and vs_port.py, sent to the user 2026-10-28;
// his SvR 2011 port takes SvR 2008-2010's themes the same way). The same system as WWE '12's, one version older:
// SvR 2011's theme is MENU/MnnL (PACH: the layout, YXMLBIN VER0002), MENU/MnnI (7001 pictures, 7002 animations, ATB0
// version 1) and MENU/MnnF (font); WWE '12's MLYE/MBnn holds the layout (VER0003), pictures and animations, and
// MENU/MnnF its font. Every WWE '12 layout has the same frame - 15 resources, 36 nodes in groups 1, 8, 9, 5, 6, 1, 6 -
// and its VS screen takes node 30 (the background) by number (sub_824C5458 then sub_82571BA8, which faulted on SvR
// 2011's 22 nodes); every SvR 2011 one has 12 resources, 22 nodes, a frame "<x>_fr" (resource 5000: the four text
// groups' first node), a background "<x>_bg" (5001) and beta, each an animation of its pictures. So WWE '12's RAW
// layout (MB01) is kept as it is (its text lines, its "dummy" and "matchup_BG" pictures, its node numbers) and wears
// SvR's frame, background and beta: their resource records and nodes' places, its pictures (WWE '12's dummy and
// matchup_BG added), animations and font. Royal Rumble's second frame (rr_fr02) has no place in it: left out.
// Seen in the game (2026-10-29, Breaking Point on Fatal 4-Way's place): background, frame, logo and their animations
// (WWE '12 plays SvR's ATB0 version 1 as they are) and SvR's font. The base arena must be unlocked in the player's
// save: a locked one's VS screen is looked up as "MB-1" (none) and SmackDown's is shown.
namespace detail {

inline uint32_t Rd32(const Bytes &b, size_t at) {
  return at + 4 <= b.size() ? uint32_t(b[at] | b[at + 1] << 8 | b[at + 2] << 16 | uint32_t(b[at + 3]) << 24) : 0;
}

struct Yxml {
  Bytes head;                       // the header (0x20) and the resources (0x4C each)
  std::vector<std::vector<Bytes>> groups;
  Bytes tail;
  size_t rec = 0;                   // a node's size: 0x90 (VER0002), 0x84 (VER0003)
};

inline bool ReadYxml(const Bytes &y, Yxml &out) {
  if (y.size() < 0x20 || memcmp(y.data(), "YXMLBIN VER000", 14) != 0)
    return false;
  out.rec = y[14] == '3' ? 0x84 : 0x90;
  const uint32_t nres = Rd32(y, 16), nnodes = Rd32(y, 20), ngroups = Rd32(y, 24);
  size_t p = 0x20 + size_t(nres) * 0x4C;
  if (p > y.size() || ngroups > 64)
    return false;
  out.head.assign(y.begin(), y.begin() + long(p));
  out.groups.clear();
  uint32_t seen = 0;
  for (uint32_t g = 0; g < ngroups; ++g) {
    if (p + 4 > y.size())
      return false;
    const uint32_t n = Rd32(y, p);
    p += 4;
    if (p + n * out.rec > y.size())
      return false;
    std::vector<Bytes> nodes;
    for (uint32_t i = 0; i < n; ++i, p += out.rec)
      nodes.emplace_back(y.begin() + long(p), y.begin() + long(p + out.rec));
    out.groups.push_back(std::move(nodes));
    seen += n;
  }
  if (seen != nnodes)
    return false;
  out.tail.assign(y.begin() + long(p), y.end());
  return true;
}

inline Bytes WriteYxml(const Yxml &y) {
  Bytes out = y.head;
  for (const auto &g : y.groups) {
    const uint32_t n = uint32_t(g.size());
    out.insert(out.end(), {uint8_t(n), uint8_t(n >> 8), uint8_t(n >> 16), uint8_t(n >> 24)});
    for (const auto &r : g)
      out.insert(out.end(), r.begin(), r.end());
  }
  out.insert(out.end(), y.tail.begin(), y.tail.end());
  return out;
}

inline std::string ResName(const Bytes &head, size_t i) {
  const char *s = reinterpret_cast<const char *>(&head[0x20 + i * 0x4C + 12]);
  return std::string(s, strnlen(s, 0x40));
}

// The resource of that type whose name ends so ("_fr", "_bg"), else -1.
inline int ResourceEnding(const Bytes &head, uint32_t type, const char *end) {
  const uint32_t n = Rd32(head, 16);
  for (uint32_t i = 0; i < n; ++i) {
    const std::string name = ResName(head, i);
    if (Rd32(head, 0x20 + i * 0x4C + 4) == type && name.size() >= strlen(end) &&
        name.compare(name.size() - strlen(end), std::string::npos, end) == 0)
      return int(i);
  }
  return -1;
}

inline svrfmt::ArcNode *Kid(svrfmt::ArcNode &n, uint32_t id) {
  if (!svrfmt::ArchiveExpand(n))
    return nullptr;
  for (auto &c : n.kids)
    if (c.id == id)
      return &c;
  return nullptr;
}

}  // namespace detail

inline bool SvrCard(const Bytes &svr_matchhd, const Bytes &matchhd12, int file, Bytes &mb, Bytes &mf,
                    std::string *why = nullptr) {
  using namespace detail;
  auto fail = [&](const std::string &w) {
    if (why)
      *why = w;
    mb.clear();
    mf.clear();
    return false;
  };
  svrfmt::Archive s, w;
  if (!svrfmt::ArchiveOpen(svr_matchhd, s, nullptr) || !svrfmt::ArchiveOpen(matchhd12, w, nullptr))
    return fail("unreadable MatchHD.pac");
  char nl[8], ni[8], nf[8];
  snprintf(nl, sizeof(nl), "M%02dL", file);
  snprintf(ni, sizeof(ni), "M%02dI", file);
  snprintf(nf, sizeof(nf), "M%02dF", file);
  svrfmt::ArcNode *sl = nullptr, *si = nullptr, *card = nullptr;
  mf.clear();
  for (auto &k : s.root.kids)
    if (k.group == "MENU" && k.name == nl)
      sl = &k;
    else if (k.group == "MENU" && k.name == ni)
      si = &k;
    else if (k.group == "MENU" && k.name == nf)
      mf = k.stored;
  for (auto &k : w.root.kids)
    if (k.group == "MLYE" && k.name == "MB01")
      card = &k;
  if (!sl || !si || mf.empty())
    return fail("SvR 2011 has no VS theme " + std::to_string(file));
  svrfmt::ArcNode *l2 = Kid(*sl, 0), *p2 = Kid(*si, 7001), *a2 = Kid(*si, 7002);
  svrfmt::ArcNode *l3 = card ? Kid(*card, 7000) : nullptr, *p3 = card ? Kid(*card, 7001) : nullptr,
                  *a3 = card ? Kid(*card, 7002) : nullptr;
  if (!l2 || !p2 || !a2 || !l3 || !p3 || !a3)
    return fail("a VS theme of an unknown shape");
  Yxml y2, y3;
  if (!ReadYxml(svrfmt::ArchiveData(*l2), y2) || !ReadYxml(svrfmt::ArchiveData(*l3), y3) || y2.rec != 0x90 ||
      y3.rec != 0x84 || y2.groups.size() != 7 || y3.groups.size() != 7)
    return fail("a VS layout of an unknown shape");
  // SvR's frame, background and beta: their resources' records, and their nodes' places and animations (the node's
  // +0x04..+0x3F; +0x40, the resource's number, stays WWE '12's), its resource's name at +0x4C.
  struct Part {
    uint32_t type;
    const char *end;
  };
  for (const Part part : {Part{5, "_fr"}, Part{5, "_bg"}, Part{2, "beta"}}) {
    const int r2 = ResourceEnding(y2.head, part.type, part.end), r3 = ResourceEnding(y3.head, part.type, part.end);
    if (r2 < 0 || r3 < 0)
      return fail(std::string("no ") + part.end + " in one of the layouts");
    memcpy(&y3.head[0x20 + size_t(r3) * 0x4C], &y2.head[0x20 + size_t(r2) * 0x4C], 0x4C);
    const Bytes name2(&y2.head[0x20 + size_t(r2) * 0x4C + 12], &y2.head[0x20 + size_t(r2) * 0x4C + 28]);
    const uint32_t id2 = Rd32(y2.head, 0x20 + size_t(r2) * 0x4C);
    std::vector<const Bytes *> from;  // (SvR's nodes of that resource, in order)
    for (const auto &g : y2.groups)
      for (const auto &n : g)
        if (Rd32(n, 0) == id2)
          from.push_back(&n);
    size_t k = 0;
    for (auto &g : y3.groups)
      for (auto &n : g)
        if (Rd32(n, 0x40) == uint32_t(r3) && !from.empty()) {
          const Bytes &src = *from[std::min(k++, from.size() - 1)];
          memcpy(&n[0], &src[0], 0x40);
          memcpy(&n[0x4C], name2.data(), name2.size());
        }
  }
  // Its pictures, with WWE '12's own the layout keeps.
  std::vector<svrfmt::BundleTexture> pics, ours;
  if (!svrfmt::BundleRead(svrfmt::ArchiveData(*p2), pics) || !svrfmt::BundleRead(svrfmt::ArchiveData(*p3), ours))
    return fail("unreadable VS pictures");
  for (const auto &t : ours)
    if ((t.name == "dummy" || t.name == "matchup_BG01" || t.name == "matchup_BG02") && !Named(pics, t.name))
      pics.push_back(t);
  ReplaceLike(*l3, WriteYxml(y3));
  ReplaceLike(*p3, svrfmt::BundleWrite(pics));
  ReplaceLike(*a3, svrfmt::ArchiveData(*a2));
  svrfmt::ArchiveTouch(*card);
  svrfmt::Archive one;
  one.root = *card;
  mb = svrfmt::ArchiveSave(one);
  if (mb.size() < 4 || memcmp(mb.data(), "PACH", 4) != 0)
    return fail("the VS screen couldn't be written");
  return true;
}

// SvR 2008 / 2009 arenas as SvR 2011 has them (after Kaiko's arena_port.py svr2008_convert, with his permission and
// credit). Their pictures are 200 / 201 (2011: 400 / 401); their crowd (20000-20010, 2009's) and their light motions
// (31010) are an older format: left out (the base's crowd comes in); their light bank (30000) has no "YMKs" header;
// they have no visibility tree (50020: the game then culls every arena model, a black floor and no barricades in
// SvR 2011): one is made, a node per model boxed by its vertices; the crowd's pictures (402) and 61000 are the
// base's. The light sets the game looks up itself that the bank lacks come from the base's bank, and the models' flags
// naming a set the bank hasn't are moved to the nearest one of the same hundred. False: not such an arena.
inline bool OldSvrArena(svrfmt::ArcNode &ga, svrfmt::ArcNode &gb, std::vector<std::string> *notes = nullptr) {
  using namespace detail;
  auto kid = [](svrfmt::ArcNode &n, uint32_t id) -> svrfmt::ArcNode * {
    for (auto &k : n.kids)
      if (k.id == id)
        return &k;
    return nullptr;
  };
  if (kid(ga, 400) || !kid(ga, 200))
    return false;
  std::string e;
  for (auto &k : ga.kids)
    if (k.id == 200 || k.id == 201)
      k.id += 200;
  const std::set<uint32_t> old = {20000, 20001, 20002, 20003, 20010, 31010};
  ga.kids.erase(std::remove_if(ga.kids.begin(), ga.kids.end(), [&](const svrfmt::ArcNode &k) { return old.count(k.id) != 0; }),
                ga.kids.end());
  for (uint32_t id : {402u, 61000u})
    if (!kid(ga, id))
      if (svrfmt::ArcNode *b = kid(gb, id))
        ga.kids.push_back(*b);
  // The light bank, with its header, and the sets the game looks up itself.
  std::set<int> have;
  if (svrfmt::ArcNode *l = kid(ga, 30000)) {
    Bytes d = svrfmt::ArchiveData(*l);
    if (d.size() >= 4 && memcmp(d.data(), "YMKs", 4) != 0) {
      Bytes h = {'Y', 'M', 'K', 's', 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
      d.insert(d.begin(), h.begin(), h.end());
    }
    std::vector<Track> ours, theirs;
    if (ReadTracks(d, ours)) {
      for (const auto &t : ours)
        have.insert(t.id);
      std::vector<int> added;
      if (svrfmt::ArcNode *bl = kid(gb, 30000); bl && ReadTracks(svrfmt::ArchiveData(*bl), theirs))
        for (int id : {10, 11, 12, 13, 14, 20, 30, 31, 100, 110, 115, 500}) {
          if (have.count(id))
            continue;
          bool any = false;
          for (const auto &t : theirs)
            if (t.id == id) {
              ours.push_back(t);
              any = true;
            }
          if (any) {
            have.insert(id);
            added.push_back(id);
          }
        }
      d = WriteTracks(d, ours);
      if (notes && !added.empty())
        notes->push_back(std::to_string(added.size()) + " light sets the game needs, from the base");
    }
    ReplaceLike(*l, d);
  }
  // The flags' light sets the bank hasn't: l(n), le(n), lg(n).
  if (svrfmt::ArcNode *f = kid(ga, 50001); f && !have.empty()) {
    auto lines = Lines(svrfmt::ArchiveData(*f));
    bool changed = false;
    for (auto &line : lines)
      for (size_t at = 0; (at = line.find("(", at)) != std::string::npos; ++at) {
        size_t s = at;
        while (s > 0 && std::isalpha((unsigned char)line[s - 1]))
          --s;
        const std::string tag = line.substr(s, at - s);
        if ((tag != "l" && tag != "le" && tag != "lg") || (s > 0 && std::isalnum((unsigned char)line[s - 1])))
          continue;
        const size_t close = line.find(')', at);
        if (close == std::string::npos || close == at + 1 ||
            !std::all_of(line.begin() + long(at + 1), line.begin() + long(close), ::isdigit))
          continue;
        const int n = atoi(line.c_str() + at + 1);
        if (have.count(n))
          continue;
        int to = -1;
        for (int pass = 0; pass < 2 && to < 0; ++pass)
          for (int h : have)
            if ((pass || h / 100 == n / 100) && (to < 0 || std::abs(h - n) < std::abs(to - n)))
              to = h;
        line.replace(at + 1, close - at - 1, std::to_string(to));
        changed = true;
      }
    if (changed)
      ReplaceLike(*f, Join(lines));
  }
  // The visibility tree: the base's head (comments), the root boxing all, a node per model (3ds Max axes: x, z, -y,
  // a tenth of the model's units, as SvR 2011's own trees).
  if (!kid(ga, 50020)) {
    std::vector<std::pair<std::string, std::array<double, 6>>> boxes;
    for (auto &k : ga.kids) {
      svrfmt::Model m;
      if (k.id >= 400 || !svrfmt::JboyRead(svrfmt::ArchiveData(k), m, nullptr))
        continue;
      std::array<double, 6> b = {1e30, 1e30, 1e30, -1e30, -1e30, -1e30};
      for (const auto &s : m.meshes)
        for (const auto &v : s.verts) {
          const double p[3] = {v.pos[0] * 0.1, v.pos[2] * 0.1, -v.pos[1] * 0.1};
          for (int c = 0; c < 3; ++c) {
            b[size_t(c)] = std::min(b[size_t(c)], p[c]);
            b[size_t(c) + 3] = std::max(b[size_t(c) + 3], p[c]);
          }
        }
      if (b[0] <= b[3])
        boxes.push_back({m.name, b});
    }
    if (!boxes.empty()) {
      std::vector<std::string> head, out;
      std::vector<VisNode> unused;
      if (svrfmt::ArcNode *bv = kid(gb, 50020))
        ReadVis(svrfmt::ArchiveData(*bv), head, unused);
      std::array<double, 6> root = boxes[0].second;
      for (const auto &[n, b] : boxes)
        for (int c = 0; c < 3; ++c) {
          root[size_t(c)] = std::min(root[size_t(c)], b[size_t(c)]);
          root[size_t(c) + 3] = std::max(root[size_t(c) + 3], b[size_t(c) + 3]);
        }
      auto box = [](const std::array<double, 6> &b) {
        std::string s = "\t";
        for (int c = 0; c < 6; ++c)
          s += (c ? " " : "") + Num(b[size_t(c)]);
        return s;
      };
      out = head;
      for (const auto &l : std::vector<std::string>{"NODE MY_NAME = VT_ROOT", "{", box(root), "}",
                                                    std::to_string(boxes.size()), ""})
        out.push_back(l);
      for (const auto &[n, b] : boxes)
        for (const auto &l : std::vector<std::string>{"NODE MY_NAME = " + n + " PARENT_NAME = VT_ROOT", "{", box(b), "}", "0", ""})
          out.push_back(l);
      out.push_back("");
      if (svrfmt::ArcNode *bv = kid(gb, 50020)) {  // (wrapped as the base's)
        svrfmt::ArcNode n = *bv;
        ReplaceLike(n, Join(out));
        ga.kids.push_back(n);
      }
      if (notes)
        notes->push_back("visibility tree made (" + std::to_string(boxes.size()) + " models)");
    }
  }
  std::sort(ga.kids.begin(), ga.kids.end(), [](const svrfmt::ArcNode &a, const svrfmt::ArcNode &b) { return a.id < b.id; });
  svrfmt::ArchiveTouch(ga);
  return true;
}

struct PortInfo {
  int models = 0, models_left = 0;  // models converted, models not (unknown layout)
  int pictures_added = 0;           // ring and crowd pictures copied from the base
  bool own_crowd = false;           // its own crowd (else the base's)
  int pictures_missing = 0;         // ... that found no free record in the package
  int fences = 0;                   // barricade pieces taken from the base
  float ramp_svr = 0, ramp12 = 0;   // the ramps' heights (cm)
  std::vector<std::string> notes;   // what else was done (SvR 2008 / 2009 arenas)
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
  if (OldSvrArena(ga, gb, &pi.notes))
    pi.notes.insert(pi.notes.begin(), "SvR 2008 / 2009 arena made as SvR 2011's");
  const std::set<uint32_t> ring = {951, 952, 953, 956, 960, 964, 973, 974, 980};
  std::set<uint32_t> from_base = {30002, 995, 996, 997, 999};
  from_base.insert(ring.begin(), ring.end());
  // Its own crowd: the people converted (not scaled), their places scaled; else the base's.
  const std::set<uint32_t> crowd = {20000, 20001, 20002, 20003, 20010};
  pi.own_crowd = std::any_of(ga.kids.begin(), ga.kids.end(), [](const svrfmt::ArcNode &k) { return k.id == 20000; });
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
      NoReflect(m);
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
      ReplaceLike(k, ScaleText(k.id, d, k.id == 50020 ? 100 : 10));
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
  // The moving lights (objects moved by the light bank's motions, "m(N)" in their flag line: Breaking Point's 230-269,
  // the lights and their 10 m beams): in WWE '12 the motions put them on the ring (dark lights on the mat and the
  // apron, the beams' blue ends on the mat, 2026-10-29). Left out with their flag lines.
  {
    std::set<uint32_t> beams;
    for (const auto &k : ga.kids)
      if (k.id == 50001)
        for (const auto &line : Lines(svrfmt::ArchiveData(k))) {
          const auto w = Words(line);
          if (!w.empty() && std::all_of(w[0].begin(), w[0].end(), ::isdigit) &&
              std::any_of(w.begin() + 1, w.end(), [](const std::string &x) { return x.rfind("m(", 0) == 0; }))
            beams.insert(uint32_t(atoi(w[0].c_str())));
        }
    if (!beams.empty()) {
      ga.kids.erase(std::remove_if(ga.kids.begin(), ga.kids.end(),
                                   [&](const svrfmt::ArcNode &k) { return beams.count(k.id) != 0; }),
                    ga.kids.end());
      for (auto &k : ga.kids)
        if (k.id == 50001) {
          Bytes d = svrfmt::ArchiveData(k);
          SwapFlagLines(d, beams, {});
          ReplaceLike(k, d);
        }
    }
  }
  // Pieces standing on the ground or the stage shown in the match only ("g": Breaking Point's stage deck sd_tile_core /
  // sd_tile0 and its stage set bp_obj0): during its entrances SvR put its own there, WWE '12 doesn't: holes in the ramp
  // and at the back of the stage (2026-10-29). Shown at all times, unless the arena has an entrance twin of it (a model
  // named the same plus more, shown in the entrances only, "e", not additive: bp_p_black / bp_p_black01).
  {
    std::map<int, std::string> names;
    std::set<int> standing;
    for (const auto &k : ga.kids) {
      svrfmt::Model m;
      if (k.id >= 50000 || !svrfmt::JboyRead(svrfmt::ArchiveData(k), m, nullptr))
        continue;
      float hi = -1e9f;
      for (const auto &s : m.meshes)
        for (const auto &v : s.verts)
          hi = std::max(hi, v.pos[1]);
      names[int(k.id)] = m.name;
      if (hi >= -200 && hi <= 1)  // (its lowest point on the ground or the stage: y is down)
        standing.insert(int(k.id));
    }
    std::set<std::string> entrance_names;
    for (const auto &k : ga.kids)
      if (k.id == 50001)
        for (const auto &line : Lines(svrfmt::ArchiveData(k))) {
          const auto w = Words(line);
          if (!w.empty() && std::all_of(w[0].begin(), w[0].end(), ::isdigit) &&
              std::find(w.begin() + 1, w.end(), "e") != w.end() &&
              std::find(w.begin() + 1, w.end(), "a") == w.end() &&  // (an additive one is a glow over it: sd_tile0e)
              names.count(atoi(w[0].c_str())))
            entrance_names.insert(names[atoi(w[0].c_str())]);
        }
    std::set<int> floor;
    for (const int id : standing) {
      const std::string &n = names[id];
      if (n.empty() || std::none_of(entrance_names.begin(), entrance_names.end(), [&](const std::string &e) {
            return e.size() > n.size() && e.compare(0, n.size(), n) == 0;
          }))
        floor.insert(id);
    }
    for (auto &k : ga.kids)
      if (k.id == 50001) {
        auto lines = Lines(svrfmt::ArchiveData(k));
        bool changed = false;
        for (auto &line : lines) {
          auto w = Words(line);
          if (w.empty() || !std::all_of(w[0].begin(), w[0].end(), ::isdigit) || !floor.count(atoi(w[0].c_str())))
            continue;
          const auto g = std::find(w.begin() + 1, w.end(), "g");
          if (g == w.end())
            continue;
          w.erase(g);
          line.clear();
          for (const auto &x : w)
            line += (line.empty() ? "" : " ") + x;
          changed = true;
        }
        if (changed)
          ReplaceLike(k, Join(lines));
      }
  }
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
