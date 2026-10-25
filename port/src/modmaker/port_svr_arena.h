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
//  - the ring (951-974 and 980, WWE '12's ropes) and its pictures, entry 30002 and the crowd (20000-20003, 20010)
//    are the base arena's; SvR's extra ring pieces (957-967) are left out;
//  - a big entry changed in a few places (the texture package) keeps Yuke's own packing (bpe_like::EncodeLike):
//    an arena must not outgrow the shipped file of the arena it plays as.
// Its collision, placement and visibility files work as they are.
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

struct PortInfo {
  int models = 0, models_left = 0;  // models converted, models not (unknown layout)
  int pictures_added = 0;           // ring pictures copied from the base
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
  std::set<uint32_t> from_base = {30002, 20000, 20001, 20002, 20003, 20010};
  from_base.insert(ring.begin(), ring.end());
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
    for (const auto &k : gb.kids) {
      svrfmt::Model m;
      if (!ring.count(k.id) || !svrfmt::JboyRead(svrfmt::ArchiveData(k), m, nullptr))
        continue;
      for (const auto &n : m.textures)
        if (!names.count(n))
          for (const auto &t : theirs)
            if (t.name == n) {
              add.push_back(t);
              names.insert(n);
              break;
            }
    }
    pi.pictures_added = AddPictures(pkg, add, models);
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
