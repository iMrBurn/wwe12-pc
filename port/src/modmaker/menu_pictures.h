// Menu pictures (type=menupics, pictures/<name>.dds): the Universe's show logos, brand logos, match-up screen
// pictures and arena photos replaced by the player's own (2026-11-06). The game names them in its code and finds
// them in pac\menu\MnLayHD.pac / MnLaySD.pac, in many layouts at once (MLYE/USEI SHOW EDIT, UMSI, WSDI, CHRM... and
// the other languages' MLYI/F/G/S): every picture of that name, in its own size and format, is replaced.
//   - show logos (WWE ORIGINAL): 40 pairs {PPV_x, PPV_x2} at 0x82022CB8 (Add_n / Add2_n: No Mercy, nWo...), 256 x 128;
//   - brand logos: 9 at 0x82022E74 (the show logos PPV_raw, sd, NXT, legend, WCW, WWE, ECW, FCW, AWA), and the
//     smaller brandlogo_* (256 x 64) of the Universe screens;
//   - match-up screens: 31 PPVbg_* (512 x 128) at 0x82022DF8 (the VS screens themselves are MatchHD.pac's MBnn);
//   - arena photos: Uni_arena_* (256 x 64), a table of 21 stages in sub_82586428 (CAR: a created arena).
// Adding new ones (the tables are full) is for 1.2.
// The game looks each layout up in the file directory by a name it builds ("/MLYE/%s", 0x8205BE48), as MatchHD.pac's
// banner pack (wwe12_arena_pages.cpp). A dressed layout can't keep its place (its pictures are ZLIB-packed, and Yuke's
// zlib packs 0.5 % better than miniz: nothing would fit), so it is appended to a copy of the file and the directory
// answers its name with that place and size (src/native/wwe12_superstar_mods.cpp builds it at each start).
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "svrfmt/archive.h"
#include "svrfmt/texture.h"

namespace menu_pictures {

using svrfmt::Bytes;

// A picture the player can change: the game's names that show it (the first one names it in a mod and in the page).
struct Slot {
  std::string label;
  std::vector<std::string> names;
  bool fill;  // cut to its proportions (photos, match-up screens); else whole, centred, clear around (logos)
};

struct Group {
  const char *en, *fr;
  std::vector<Slot> slots;
};

inline const std::vector<Group> &Groups() {
  static const std::vector<Group> groups = [] {
    static const std::map<std::string, std::string> label = {
        {"raw", "RAW"}, {"sd", "SmackDown"}, {"super", "Superstars"}, {"NXT", "NXT"}, {"RR", "Royal Rumble"},
        {"EC", "Elimination Chamber"}, {"WM27", "WrestleMania"}, {"EXT", "Extreme Rules"}, {"over", "Over the Limit"},
        {"4way", "Fatal 4-Way"}, {"MinB", "Money in the Bank"}, {"Slam", "SummerSlam"}, {"NoC", "Night of Champions"},
        {"HinC", "Hell in a Cell"}, {"BR", "Bragging Rights"}, {"SS", "Survivor Series"}, {"TLC", "TLC"},
        {"legend", "Legends"}, {"WWE", "WWE"}};
    const auto named = [](const std::string &key) {
      const auto it = label.find(key);
      return it == label.end() ? key : it->second;
    };
    // a brand's smaller logo (the Universe screens) goes with its logo
    static const std::map<std::string, std::string> smaller = {{"raw", "RAW"}, {"sd", "SD"},   {"NXT", "NXT"},
                                                               {"ECW", "ECW"}, {"AWA", "AWA"}, {"WCW", "WCW"},
                                                               {"FCW", "FCW"}, {"legend", "LGD"}};
    const auto logo = [&](const std::string &key) {
      Slot s{named(key), {"PPV_" + key, "PPV_" + key + "2"}, false};
      if (key == "raw")
        s.names.push_back("PPV_rawHD2");
      const auto it = smaller.find(key);
      if (it != smaller.end())
        s.names.push_back("brandlogo_" + it->second);
      return s;
    };
    std::vector<Group> g;
    // Show logos, in the game's table order.
    Group shows{"Show logos", "Logos de show", {}};
    for (const char *k : {"raw", "sd", "super", "NXT", "RR", "EC", "WM27", "EXT", "over", "4way", "MinB", "Slam", "NoC",
                          "HinC", "BR", "SS", "TLC", "nitro", "crash", "cade", "ttt", "ECW", "King", "AWA"})
      shows.slots.push_back(logo(k));
    for (int i = 1; i <= 15; ++i) {
      char a[16], b[16];
      snprintf(a, sizeof(a), "Add_%02d", i);
      snprintf(b, sizeof(b), "Add2_%02d", i);
      shows.slots.push_back({a, {a, b}, false});
    }
    shows.slots.push_back(logo("WWE"));
    g.push_back(std::move(shows));
    // Brand logos the shows haven't.
    Group brands{"Brand logos", "Logos de marque", {}};
    for (const char *k : {"legend", "WCW", "FCW"})
      brands.slots.push_back(logo(k));
    brands.slots.push_back({"FA", {"brandlogo_FA"}, false});
    brands.slots.push_back({"Inju", {"brandlogo_Inju"}, false});
    g.push_back(std::move(brands));
    // Match-up screens' pictures (SHOW EDIT > MATCH-UP SCREEN).
    Group matchups{"Match-up screens", "Matchup screens", {}};
    for (const char *k : {"raw", "sd", "super", "NXT", "RR", "EC", "WM27", "EXT", "over", "4way", "MinB", "Slam", "NoC",
                          "HinC", "BR", "SS", "TLC", "WCW", "crash", "cade", "ttt", "origin01", "origin02", "origin03",
                          "origin04", "origin05", "origin06", "origin07", "origin08", "origin09", "origin10"})
      matchups.slots.push_back({named(k), {std::string("PPVbg_") + k}, true});
    g.push_back(std::move(matchups));
    // Arena photos (SHOW EDIT > ARENA).
    Group arenas{"Arena photos", "Photos d'arène", {}};
    for (const char *k : {"raw", "sd", "super", "NXT", "RR", "EC", "WM27", "EXT", "over", "4way", "MinB", "Slam", "NoC",
                          "HinC", "BR", "SS", "TLC", "WCW", "WWE", "cade", "ttt", "clash", "CAR"})
      arenas.slots.push_back({named(k), {std::string("Uni_arena_") + k}, true});
    g.push_back(std::move(arenas));
    return g;
  }();
  return groups;
}

// img into the picture `like` (its size, format and mip levels): cut to its proportions (fill) or whole.
inline Bytes Like(const Bytes &like, const svrfmt::Image &img, bool fill) {
  svrfmt::DdsInfo info;
  if (!svrfmt::DdsInfoOf(like, info) || img.w <= 0 || img.h <= 0)
    return {};
  svrfmt::Image fit;
  if (fill) {
    int cw = img.w, ch = int(int64_t(img.w) * info.h / info.w);
    if (ch > img.h)
      ch = img.h, cw = int(int64_t(img.h) * info.w / info.h);
    svrfmt::Image crop;
    crop.w = cw, crop.h = ch;
    crop.rgba.resize(size_t(cw) * ch * 4);
    for (int y = 0; y < ch; ++y)
      memcpy(&crop.rgba[size_t(y) * cw * 4], &img.rgba[(size_t((img.h - ch) / 2 + y) * img.w + (img.w - cw) / 2) * 4],
             size_t(cw) * 4);
    fit = svrfmt::Resize(crop, info.w, info.h);
  } else {
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

// The game's picture of that name (the first found in MnLayHD.pac's MLYE layouts), for the page.
inline Bytes Find(const Bytes &mnlay, const std::string &name) {
  svrfmt::Archive m;
  if (!svrfmt::ArchiveOpen(mnlay, m, nullptr))
    return {};
  for (auto &k : m.root.kids) {
    if (k.group != "MLYE" || !svrfmt::ArchiveExpand(k))
      continue;
    for (auto &c : k.kids) {
      std::vector<svrfmt::BundleTexture> t;
      if (c.id == 7001 && svrfmt::BundleRead(svrfmt::ArchiveData(c), t))
        for (auto &x : t)
          if (x.name == name)
            return x.data;
    }
  }
  return {};
}

// The game's pictures of these names (the first of each found in MnLayHD.pac's MLYE layouts), in one read.
inline std::map<std::string, Bytes> FindAll(const Bytes &mnlay, const std::vector<std::string> &names) {
  std::map<std::string, Bytes> out;
  svrfmt::Archive m;
  if (!svrfmt::ArchiveOpen(mnlay, m, nullptr))
    return out;
  for (auto &k : m.root.kids) {
    if (k.group != "MLYE" || !svrfmt::ArchiveExpand(k))
      continue;
    for (auto &c : k.kids) {
      std::vector<svrfmt::BundleTexture> t;
      if (c.id == 7001 && svrfmt::BundleRead(svrfmt::ArchiveData(c), t))
        for (auto &x : t)
          if (!out.count(x.name) && std::find(names.begin(), names.end(), x.name) != names.end())
            out[x.name] = x.data;
    }
  }
  return out;
}

namespace detail {

// A zlib stream (RFC 1950) of `raw`: LZ77 (hash chains, greedy) with deflate's fixed Huffman codes, one block. Not as
// small as zlib's, but any inflater reads it; the dressed layouts are served at their own size.
inline Bytes Zlib(const Bytes &raw) {
  Bytes out = {0x78, 0x01};
  uint32_t bits = 0;
  int nbits = 0;
  const auto put = [&](uint32_t v, int n) {  // (deflate packs from the low bit)
    bits |= v << nbits;
    nbits += n;
    while (nbits >= 8) {
      out.push_back(uint8_t(bits));
      bits >>= 8;
      nbits -= 8;
    }
  };
  const auto rev = [](uint32_t v, int n) {
    uint32_t r = 0;
    for (int i = 0; i < n; ++i)
      r = r << 1 | (v >> i & 1);
    return r;
  };
  const auto code = [&](int c) {  // a literal / length code, fixed Huffman (its bits highest first)
    if (c < 144)
      put(rev(uint32_t(0x30 + c), 8), 8);
    else if (c < 256)
      put(rev(uint32_t(0x190 + c - 144), 9), 9);
    else if (c < 280)
      put(rev(uint32_t(c - 256), 7), 7);
    else
      put(rev(uint32_t(0xC0 + c - 280), 8), 8);
  };
  static const int lbase[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
  static const int lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
  static const int dbase[30] = {1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
                                193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
  static const int dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
  constexpr size_t kWindow = 32768;
  constexpr uint32_t kHash = 1 << 15;
  constexpr int kChain = 48;
  std::vector<int32_t> head(kHash, -1), prev(raw.size(), -1);
  const auto hash = [&](size_t i) {
    return (uint32_t(raw[i]) << 10 ^ uint32_t(raw[i + 1]) << 5 ^ uint32_t(raw[i + 2])) & (kHash - 1);
  };
  const auto insert = [&](size_t i) {
    if (i + 2 < raw.size()) {
      const uint32_t h = hash(i);
      prev[i] = head[h];
      head[h] = int32_t(i);
    }
  };
  put(1, 1);  // the last block
  put(1, 2);  // fixed codes
  size_t i = 0;
  while (i < raw.size()) {
    int best = 0;
    size_t dist = 0;
    if (i + 2 < raw.size()) {
      const int limit = int(std::min<size_t>(258, raw.size() - i));
      int32_t c = head[hash(i)];
      for (int n = 0; c >= 0 && n < kChain && i - size_t(c) <= kWindow; ++n, c = prev[size_t(c)]) {
        int l = 0;
        while (l < limit && raw[size_t(c) + size_t(l)] == raw[i + size_t(l)])
          ++l;
        if (l > best) {
          best = l, dist = i - size_t(c);
          if (l == limit)
            break;
        }
      }
    }
    if (best >= 3) {
      int li = 28;
      while (lbase[li] > best)
        --li;
      code(257 + li);
      put(uint32_t(best - lbase[li]), lext[li]);
      int di = 29;
      while (size_t(dbase[di]) > dist)
        --di;
      put(rev(uint32_t(di), 5), 5);
      put(uint32_t(dist - size_t(dbase[di])), dext[di]);
      for (int k = 0; k < best; ++k)
        insert(i + size_t(k));
      i += size_t(best);
    } else {
      code(raw[i]);
      insert(i);
      ++i;
    }
  }
  code(256);
  if (nbits)
    put(0, 8 - nbits);
  uint32_t a = 1, b = 0;
  for (const uint8_t c : raw)
    a = (a + c) % 65521, b = (b + a) % 65521;
  const uint32_t adler = b << 16 | a;
  for (int k = 3; k >= 0; --k)
    out.push_back(uint8_t(adler >> (8 * k)));
  return out;
}

// A "ZLIB" entry as the game's: "ZLIB", 0x1230, packed size, size, then the zlib stream.
inline Bytes ZlibEntry(const Bytes &raw) {
  const Bytes z = Zlib(raw);
  Bytes out = {'Z', 'L', 'I', 'B'};
  for (const uint32_t v : {0x1230u, uint32_t(z.size()), uint32_t(raw.size())})
    for (int k = 0; k < 4; ++k)
      out.push_back(uint8_t(v >> (8 * k)));
  out.insert(out.end(), z.begin(), z.end());
  return out;
}

}  // namespace detail

// A layout of MnLayHD.pac / MnLaySD.pac wearing the player's pictures: its directory name and its new bytes (the
// whole EPAC entry, as stored).
struct Dressed {
  std::string name;  // "/MLYE/USEI"
  Bytes data;
};

// The layouts of `mnlay` that show these pictures, dressed. `pics`: a slot's first name (or any picture's name) ->
// any DDS, fitted to each place's size and format (`fill` as its slot says); a slot's picture goes to all its names.
// `count`: the pictures replaced.
inline std::vector<Dressed> Dress(const Bytes &mnlay, const std::map<std::string, Bytes> &pics, int *count = nullptr) {
  std::map<std::string, bool> fill;
  std::map<std::string, std::string> from;  // a picture's name -> the key of `pics` giving it
  for (const auto &g : Groups())
    for (const auto &s : g.slots)
      for (const auto &n : s.names) {
        fill[n] = s.fill;
        if (pics.count(s.names[0]))
          from[n] = s.names[0];
      }
  for (const auto &p : pics)
    from[p.first] = p.first;
  std::map<std::string, svrfmt::Image> imgs;
  for (const auto &[name, dds] : pics) {
    svrfmt::Image img;
    if (svrfmt::DdsDecode(dds, img) && img.w > 0 && img.h > 0)
      imgs[name] = std::move(img);
  }
  std::vector<Dressed> out;
  int n = 0;
  if (count)
    *count = 0;
  svrfmt::Archive m;
  if (imgs.empty() || !svrfmt::ArchiveOpen(mnlay, m, nullptr))
    return out;
  std::vector<std::pair<std::string, std::string>> touched;  // (group, name)
  for (auto &k : m.root.kids) {
    if (k.group.rfind("MLY", 0) != 0 || !svrfmt::ArchiveExpand(k))
      continue;
    bool changed = false;
    for (auto &c : k.kids) {
      std::vector<svrfmt::BundleTexture> t;
      if (c.id != 7001 || !svrfmt::BundleRead(svrfmt::ArchiveData(c), t))
        continue;
      bool any = false;
      for (auto &x : t) {
        const auto f = from.find(x.name);
        if (f == from.end() || !imgs.count(f->second))
          continue;
        const Bytes d = Like(x.data, imgs[f->second], fill.count(x.name) ? fill[x.name] : false);
        if (d.empty() || d.size() != x.data.size())
          continue;
        x.data = d;
        any = true;
        ++n;
      }
      if (any) {
        const Bytes bundle = svrfmt::BundleWrite(t);
        const bool zlib = c.wrap == svrfmt::ArcWrap::Zlib;
        svrfmt::ArchiveReplace(c, bundle);
        if (zlib)
          c.stored = detail::ZlibEntry(bundle);  // (ArchiveReplace stores it unpacked: 3 x bigger)
        changed = true;
      }
    }
    if (changed) {
      svrfmt::ArchiveTouch(k);
      touched.push_back({k.group, k.name});
    }
  }
  if (touched.empty())
    return out;
  svrfmt::ArchiveTouch(m.root);
  const Bytes saved = svrfmt::ArchiveSave(m);
  std::vector<svrfmt::EpacRow> rows;
  if (!svrfmt::EpacTable(saved, saved.size(), rows))
    return out;
  for (const auto &r : rows)
    for (const auto &[group, name] : touched)
      if (r.group == group && r.name == name)
        out.push_back({"/" + group + "/" + name,
                       Bytes(saved.begin() + long(r.offset), saved.begin() + long(r.offset + r.size))});
  if (count)
    *count = n;
  return out;
}

}  // namespace menu_pictures
