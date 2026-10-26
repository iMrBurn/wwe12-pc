// An arena of WWE '13 (the player's own copy of the game) made into a WWE '12 arena (docs/mod_maker.md, "Portage
// d'arènes de WWE '13"; found in game with Capitol Punishment, variants A to I):
//  - the file is XMemCompress'd (svrfmt/xcompress.cpp) and, once unpacked, is WWE '12's format but for two header
//    bytes: "EPK8" instead of "EPAC", and a 0x0010 flag in the high half of the STG group word at 0x804;
//  - its texture detail list (entry 50010: XML, the mip level each texture loads at) asks for 129 textures at full
//    size: WWE '12 never finds the memory and the load waits forever. Every WWE '12 arena has the same list: the
//    base arena's is used;
//  - its ring (951-980) stands too high for WWE '12: the base arena's ring parts are used (they find the WWE '13
//    apron and mat pictures by name), WWE '13's extra post and cover (981, 982) are left out;
//  - the base's ring post (951) looks for pictures the WWE '13 arena may not have (Royal Rumble's "rin_pos00": Vengeance
//    has "rin_pos00_slv"; the game then draws another picture, the titantron's): they are copied from the base;
//  - the collision (995, 996, 997, 999) comes from the base: the entrance walk follows the base's stage (with WWE
//    '13's, wrestlers floated over a Vengeance tile based on Royal Rumble);
//  - the ramps must stand at the same height: most WWE '12 arenas have a 1.80 m ramp, Bragging Rights and Royal
//    Rumble none, like the WWE '13 arenas seen (Vengeance, Capitol Punishment); else the wrestlers float during the
//    entrances (RampHeight, the Mod Maker advises a base);
//  - entry 61000 (a 256 x 128 picture every WWE '12 arena has) comes from the base;
//  - the breakable barricade corners (390-397, ar_fence01/02_c/f/g/h: modelled at the centre, placed by WWE '13's
//    code for its "OMG moments") are moved into the corners in the model itself.
// WWE 2K14, 2K15 and 2K16 arenas are the same kind (EPAC, WWE '12's models; 2K16 xcompressed with 20-byte entries)
// and go through the same recipe; WWE 2K17's are an HSPC whose arena list is an SHDC: its entries are put in a copy of
// the base's EPAC first (its crowd packs and missing data files: the base's). (Checked offline with each game's bg00.)
// Lights, crowd, placement and visibility files of WWE '13 work as they are. Besides the arena file: its banner, its
// VS screen (MatchHD.pac), its entrance plates (evtgdhd.pac), its pre-match screen (LoadHD.pac) and its transition
// video (evtmov.pac), served by the game for the added arena.
#pragma once

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "svrfmt/archive.h"
#include "svrfmt/jboy.h"
#include "svrfmt/texture.h"
#include "svrfmt/xcompress.h"

namespace port13 {

using svrfmt::Bytes;

// The arenas of WWE '13 by file number (pac/bg/bgNN.pac), found by their models' prefixes and their banners: its name and
// its banner in menu/MatchHD.pac (MLYE/MUAS, PACH entry 7001). The banners follow the arena select screen's order
// ("arenaNN": SmackDown, RAW, NXT, Superstars, SummerSlam, Survivor Series, Royal Rumble, WrestleMania XXVIII, Over
// the Limit, Capitol Punishment, Money in the Bank, Night of Champions, Hell in a Cell, Vengeance, TLC, Elimination
// Chamber, Extreme Rules), not the files'; the Attitude Era arenas (bg28-49) have "AEM01".."AEM22" in file order.
// Files not listed are not arenas (match structures, the moveset editor's ring, backstage rooms, menus' stages).
struct Arena13 {
  int file;
  const char *name, *banner;
};
inline constexpr Arena13 kArenas13[] = {
    {0, "SmackDown", "arena00"},          {1, "RAW", "arena01"},
    {2, "NXT", "arena02"},                {3, "Superstars", "arena03"},
    {4, "Over the Limit", "arena08"},     {5, "Capitol Punishment", "arena09"},
    {6, "Money in the Bank", "arena10"},  {7, "SummerSlam", "arena04"},
    {8, "Night of Champions", "arena11"}, {9, "Hell in a Cell", "arena12"},
    {10, "Vengeance", "arena13"},         {11, "Survivor Series", "arena05"},
    {12, "TLC", "arena14"},               {13, "Royal Rumble", "arena06"},
    {14, "Elimination Chamber", "arena15"}, {15, "WrestleMania XXVIII", "arena07"},
    {16, "Extreme Rules", "arena16"},
    {28, "RAW is WAR (1)", "AEM01"},      {29, "SmackDown 1999", "AEM02"},
    {30, "RAW is WAR (2)", "AEM03"},      {31, "Badd Blood", "AEM04"},
    {32, "Survivor Series 1997", "AEM05"}, {33, "One Night Only", "AEM06"},
    {34, "SummerSlam 1997", "AEM07"},     {35, "Royal Rumble 1998", "AEM08"},
    {36, "WrestleMania XIV", "AEM09"},    {37, "Over the Edge", "AEM10"},
    {38, "King of the Ring", "AEM11"},    {39, "SummerSlam 1998", "AEM12"},
    {40, "Judgment Day", "AEM13"},        {41, "Survivor Series 1998", "AEM14"},
    {42, "Unforgiven", "AEM15"},          {43, "Breakdown", "AEM16"},
    {44, "Rock Bottom", "AEM17"},         {45, "Royal Rumble 1999", "AEM18"},
    {46, "St. Valentine's Day Massacre", "AEM19"}, {47, "WrestleMania XV", "AEM20"},
    {48, "Halftime Heat", "AEM21"},       {49, "Sunday Night Heat", "AEM22"},
};

inline const Arena13 *Find13(int file) {
  for (const auto &a : kArenas13)
    if (a.file == file)
      return &a;
  return nullptr;
}
inline const char *ArenaName(int file) {
  const Arena13 *a = Find13(file);
  return a ? a->name : "";
}

// Moves a model (WWE '12 / '13 JBOY, 0xB8-byte meshes): its vertices, its meshes' and nodes' bounding spheres.
inline void MoveModel(Bytes &d, float dx, float dz) {
  constexpr size_t B = 8;
  if (d.size() < 0x48)
    return;
  const uint32_t nmesh = svrfmt::Be32(&d[0x18]), mptr = svrfmt::Be32(&d[0x1C]), nnode = svrfmt::Be32(&d[0x20]),
                 nptr = svrfmt::Be32(&d[0x28]);
  auto add = [&](size_t at, float v) {
    if (at + 4 > d.size())
      return;
    const float f = svrfmt::BeF(&d[at]) + v;
    uint32_t u;
    memcpy(&u, &f, 4);
    svrfmt::PutBe32(&d[at], u);
  };
  for (uint32_t i = 0; i < nmesh; ++i) {
    const size_t o = B + mptr + 0xB8 * size_t(i);
    if (o + 0xB8 > d.size())
      return;
    const uint32_t vc = svrfmt::Be32(&d[o]);
    const size_t vblk = B + svrfmt::Be32(&d[o + 0x68]);
    if (vblk + 4 > d.size())
      return;
    const size_t vd = B + svrfmt::Be32(&d[vblk]);
    for (uint32_t k = 0; k < vc; ++k) {
      add(vd + 28 * size_t(k), dx);
      add(vd + 28 * size_t(k) + 8, dz);
    }
    add(o + 0xA8, dx);
    add(o + 0xA8 + 8, dz);
  }
  for (uint32_t i = 0; i < nnode; ++i) {
    const size_t n = B + nptr + 80 * size_t(i);
    add(n + 64, dx);
    add(n + 72, dz);
  }
}

// The height of the entrance ramp in cm (0 = at floor level; WWE '12's usual ramp: 180): its models are "*slo*"
// (200-299), y pointing down.
inline float RampHeight(const svrfmt::ArcNode &group) {
  float top = 0;
  for (const auto &k : group.kids) {
    if (k.id < 200 || k.id > 299)
      continue;
    svrfmt::Model m;
    if (!svrfmt::JboyRead(svrfmt::ArchiveData(k), m, nullptr) || m.name.find("slo") == std::string::npos)
      continue;
    for (const auto &me : m.meshes)
      for (const auto &v : me.verts)
        top = std::max(top, -v.pos[1]);
  }
  return top;
}

// Arenas of WWE '12 without a raised ramp (the bases to choose for a WWE '13 arena at floor level).
inline constexpr int kFlatBases12[] = {13, 10};  // Royal Rumble, Bragging Rights

struct PortInfo {
  float ramp13 = 0, ramp12 = 0;  // the ramps' heights (cm)
  int pictures_added = 0;        // ring pictures copied from the base
  bool RampsMatch() const { return (ramp13 < 60) == (ramp12 < 60); }
};

// The WWE '13 file (as on the disc) and the base arena's WWE '12 file -> the WWE '12 arena.
inline bool PortTree(svrfmt::Archive &a, svrfmt::Archive &b, Bytes &out, std::string *why, PortInfo *info);

inline bool Port(const Bytes &w13, const Bytes &base12, Bytes &out, std::string *why, PortInfo *info = nullptr) {
  auto fail = [&](const std::string &w) {
    if (why)
      *why = w;
    return false;
  };
  // WWE 2K17: an HSPC (names hashed) whose arena list is an SHDC, PACH's place with the same children (ids): its
  // entries are put in a copy of the base's EPAC in place of the base's own, its nested crowd packs (SHDC too) left
  // for the base's; then the same recipe.
  {
    svrfmt::Archive h, b;
    std::string e;
    if (svrfmt::ArchiveOpen(w13, h, &e) && h.root.kind == svrfmt::ArcKind::Hspc) {
      if (h.root.kids.empty() || !svrfmt::ArchiveExpand(h.root.kids[0]) || !svrfmt::ArchiveOpen(base12, b, &e) ||
          b.root.kids.empty() || !svrfmt::ArchiveExpand(b.root.kids[0]))
        return fail("unreadable WWE 2K17 arena");
      svrfmt::Archive a;
      svrfmt::ArchiveOpen(base12, a, &e);
      svrfmt::ArcNode &ga = a.root.kids[0];
      svrfmt::ArchiveExpand(ga);
      std::vector<svrfmt::ArcNode> kids;
      for (auto &k : h.root.kids[0].kids) {
        if (svrfmt::ArchiveKindOf(k) == "SHDC" || svrfmt::ArchiveKindOf(k) == "PACH") {
          for (const auto &bk : b.root.kids[0].kids)
            if (bk.id == k.id)
              kids.push_back(bk);
          continue;
        }
        svrfmt::ArcNode n;
        n.id = k.id;
        n.wrap = k.wrap;
        n.stored = k.stored;
        kids.push_back(n);
      }
      // (data files WWE 2K17 hasn't, the light zones 50003 seen: the base's)
      for (const auto &bk : b.root.kids[0].kids)
        if (bk.id >= 30000 && std::none_of(kids.begin(), kids.end(), [&](const svrfmt::ArcNode &k) { return k.id == bk.id; }))
          kids.push_back(bk);
      ga.kids = kids;
      return PortTree(a, b, out, why, info);
    }
  }
  Bytes plain;
  if (svrfmt::IsXcompress(w13)) {
    std::string e;
    if (!svrfmt::XcompressDecode(w13, plain, &e))
      return fail("can't unpack the WWE '13 file: " + e);
  } else {
    plain = w13;
  }
  if (plain.size() < 0x4000 || (memcmp(plain.data(), "EPK8", 4) != 0 && memcmp(plain.data(), "EPAC", 4) != 0) ||
      memcmp(&plain[0x800], "STG ", 4) != 0)
    return fail("not a WWE '13 arena file");
  memcpy(plain.data(), "EPAC", 4);
  plain[0x806] = plain[0x807] = 0;

  svrfmt::Archive a, b;
  std::string e;
  if (!svrfmt::ArchiveOpen(plain, a, &e) || !svrfmt::ArchiveOpen(base12, b, &e))
    return fail("unreadable arena: " + e);
  return PortTree(a, b, out, why, info);
}

// The recipe on the opened arena (a) and base (b).
inline bool PortTree(svrfmt::Archive &a, svrfmt::Archive &b, Bytes &out, std::string *why, PortInfo *info) {
  auto fail = [&](const std::string &w) {
    if (why)
      *why = w;
    return false;
  };
  if (a.root.kids.empty() || b.root.kids.empty())
    return fail("no arena data");
  svrfmt::ArcNode &ga = a.root.kids[0], &gb = b.root.kids[0];
  if (!svrfmt::ArchiveExpand(ga) || !svrfmt::ArchiveExpand(gb))
    return fail("unreadable arena list");
  PortInfo pi;
  pi.ramp13 = RampHeight(ga);
  pi.ramp12 = RampHeight(gb);
  const std::set<uint32_t> ring = {951, 952, 953, 956, 960, 964, 973, 974, 980};
  std::set<uint32_t> from_base = {50010, 61000, 995, 996, 997, 999};
  from_base.insert(ring.begin(), ring.end());
  const std::set<uint32_t> drop = {981, 982};
  ga.kids.erase(std::remove_if(ga.kids.begin(), ga.kids.end(),
                               [&](const svrfmt::ArcNode &k) { return from_base.count(k.id) || drop.count(k.id); }),
                ga.kids.end());
  for (const auto &k : gb.kids)
    if (from_base.count(k.id))
      ga.kids.push_back(k);
  // The ring's pictures the arena hasn't: from the base's bundle (400).
  auto bundle = [](svrfmt::ArcNode &g) -> svrfmt::ArcNode * {
    for (auto &k : g.kids)
      if (k.id == 400 && svrfmt::ArchiveKindOf(k) == "textures")
        return &k;
    return nullptr;
  };
  svrfmt::ArcNode *ta = bundle(ga), *tb = bundle(gb);
  if (ta && tb) {
    std::vector<svrfmt::BundleTexture> have, theirs;
    svrfmt::BundleRead(svrfmt::ArchiveData(*ta), have);
    svrfmt::BundleRead(svrfmt::ArchiveData(*tb), theirs);
    std::set<std::string> names;
    for (const auto &t : have)
      names.insert(t.name);
    for (const auto &k : gb.kids) {
      svrfmt::Model m;
      if (!ring.count(k.id) || !svrfmt::JboyRead(svrfmt::ArchiveData(k), m, nullptr))
        continue;
      for (const auto &n : m.textures) {
        if (names.count(n))
          continue;
        for (const auto &t : theirs)
          if (t.name == n) {
            have.push_back(t);
            names.insert(n);
            ++pi.pictures_added;
            break;
          }
      }
    }
    if (pi.pictures_added)
      svrfmt::ArchiveReplace(*ta, svrfmt::BundleWrite(have));
  }
  if (info)
    *info = pi;
  std::sort(ga.kids.begin(), ga.kids.end(),
            [](const svrfmt::ArcNode &x, const svrfmt::ArcNode &y) { return x.id < y.id; });
  // The barricade corners into the corners (x, z in cm; as WWE '12 places its own).
  const std::map<uint32_t, std::pair<float, float>> corners = {
      {390, {561, -559}}, {391, {-561, -559}}, {392, {533, 560}}, {393, {-533, 560}},
      {394, {561, -559}}, {395, {-561, -559}}, {396, {533, 560}}, {397, {-533, 560}}};
  for (auto &k : ga.kids) {
    const auto it = corners.find(k.id);
    if (it == corners.end())
      continue;
    Bytes d = svrfmt::ArchiveData(k);
    if (!svrfmt::IsJboy(d))
      continue;
    MoveModel(d, it->second.first, it->second.second);
    svrfmt::ArchiveReplace(k, d);
  }
  // The 2K games keep entries "ZLIB"-wrapped (2K16's arena: 207 of 227) where WWE '12 has BPE, and WWE '12 crashes
  // on them (2026-10-25: WrestleMania 31 of 2K16, right after the arena loaded): unwrapped and packed again as BPE,
  // in the nested packs too.
  const auto to_bpe = [](auto &self, svrfmt::ArcNode &n) -> void {
    if (n.wrap == svrfmt::ArcWrap::Zlib) {
      const Bytes d = svrfmt::ArchiveData(n);
      n.wrap = svrfmt::ArcWrap::Bpe;
      svrfmt::ArchiveReplace(n, d);
    }
    if (svrfmt::ArchiveExpand(n) && !n.kids.empty()) {
      for (auto &c : n.kids)
        self(self, c);
      svrfmt::ArchiveTouch(n);
    }
  };
  for (auto &k : ga.kids)
    to_bpe(to_bpe, k);
  svrfmt::ArchiveTouch(ga);
  svrfmt::ArchiveTouch(a.root);
  out = svrfmt::ArchiveSave(a);
  return true;
}

// The arena's own banner in WWE '13's menu/MatchHD.pac (MLYE/MUAS, PACH entry 7001; 256 x 128 or 128 x 64).
inline bool Banner(const Bytes &matchhd13, int file, svrfmt::Image &out) {
  const Arena13 *arena = Find13(file);
  svrfmt::Archive a;
  if (!arena || !svrfmt::ArchiveOpen(matchhd13, a, nullptr))
    return false;
  for (auto &k : a.root.kids) {
    if (k.group != "MLYE" || k.name != "MUAS" || !svrfmt::ArchiveExpand(k))
      continue;
    for (auto &e : k.kids) {
      if (e.id != 7001)
        continue;
      std::vector<svrfmt::BundleTexture> texs;
      if (!svrfmt::BundleRead(svrfmt::ArchiveData(e), texs))
        continue;
      for (const auto &t : texs)
        if (t.name == arena->banner && svrfmt::DdsDecode(t.data, out))
          return true;
    }
  }
  return false;
}

// The arena's entrance graphics (location plate, name plates) in WWE '13's pac/evt/evtgdhd.pac: its ETEX/00NN group,
// the same entries as WWE '12's (pictures 0-40, 2D animations 10000-10040, fonts 20000-20011), each "ZLIB"-wrapped
// where WWE '12 has BPE: unwrapped and packed again as BPE. The game serves it for the added arena.
inline bool Etex(const Bytes &evtgdhd13, int file, Bytes &out) {
  out.clear();
  svrfmt::Archive a;
  if (!svrfmt::ArchiveOpen(evtgdhd13, a, nullptr))
    return false;
  char name[8];
  snprintf(name, sizeof(name), "%04d", file);
  for (auto &k : a.root.kids) {
    if (k.group != "ETEX" || k.name != name || !svrfmt::ArchiveExpand(k))
      continue;
    std::vector<svrfmt::PachEntry> entries;
    for (const auto &c : k.kids) {
      const Bytes raw = svrfmt::ArchiveData(c);
      if (raw.empty())
        return false;  // (no zlib in this build, or a broken entry)
      entries.push_back({c.id, svrfmt::BpeEncode(raw)});
    }
    out = svrfmt::PachWrite(entries);
    return !entries.empty();
  }
  return false;
}

// The arena's pre-match screen in WWE '13's menu/LoadHD.pac: LOAD/90NN (the VS screen and its animation before the
// match, YXMLBIN VER0003 as WWE '12's; "ZLIB" entries, which WWE '12's LoadHD.pac has too), as stored.
inline bool PreMatch(const Bytes &loadhd13, int file, Bytes &out) {
  out.clear();
  svrfmt::Archive a;
  if (!svrfmt::ArchiveOpen(loadhd13, a, nullptr))
    return false;
  char name[8];
  snprintf(name, sizeof(name), "90%02d", file);
  for (const auto &k : a.root.kids)
    if (k.group == "LOAD" && k.name == name)
      out = k.stored;
  return out.size() >= 8 && memcmp(out.data(), "PACH", 4) == 0;
}

// The arena's transition video in WWE '13's pac/evt/evtmov.pac: MOVT/M0NN, Bink 1 at 640 x 640 as WWE '12's (WWE '13's
// titantron movies are Bink 2, not these), as stored. SvR 2011's evtmov.pac has the same MOVT group.
inline bool TransitionVideo(const Bytes &evtmov13, int file, Bytes &out) {
  out.clear();
  svrfmt::Archive a;
  if (!svrfmt::ArchiveOpen(evtmov13, a, nullptr))
    return false;
  char name[8];
  snprintf(name, sizeof(name), "M%03d", file);
  for (const auto &k : a.root.kids)
    if (k.group == "MOVT" && k.name == name)
      out = k.stored;
  return out.size() >= 8 && memcmp(out.data(), "BIKi", 4) == 0;
}

// The arena's VS screen ("match card") in WWE '13's menu/MatchHD.pac, as stored: MLYE/MBnn (its layout, YXMLBIN, and
// pictures, as WWE '12's) and MENU/MnnF (its font, "font_venge" for Vengeance: the layout names it), nn = the arena's
// file number. The game serves them for the added arena (wwe12_arena_pages.cpp).
inline bool Card(const Bytes &matchhd13, int file, Bytes &mb, Bytes &mf) {
  svrfmt::Archive a;
  mb.clear();
  mf.clear();
  if (!svrfmt::ArchiveOpen(matchhd13, a, nullptr))
    return false;
  char nb[8], nf[8];
  snprintf(nb, sizeof(nb), "MB%02d", file);
  snprintf(nf, sizeof(nf), "M%02dF", file);
  for (const auto &k : a.root.kids) {
    if (k.group == "MLYE" && k.name == nb)
      mb = k.stored;
    else if (k.group == "MENU" && k.name == nf)
      mf = k.stored;
  }
  if (mb.empty() || mf.empty() || memcmp(mb.data(), "PACH", 4) != 0 || memcmp(mf.data(), "PACH", 4) != 0) {
    mb.clear();
    mf.clear();
    return false;
  }
  return true;
}

}  // namespace port13
