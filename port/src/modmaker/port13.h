// An arena of WWE '13 (the player's own copy of the game) made into a WWE '12 arena (docs/mod_maker.md, "Portage
// d'arènes de WWE '13"; found in game with Capitol Punishment, variants A to I):
//  - the file is XMemCompress'd (svrfmt/xcompress.cpp) and, once unpacked, is WWE '12's format but for two header
//    bytes: "EPK8" instead of "EPAC", and a 0x0010 flag in the high half of the STG group word at 0x804;
//  - its texture detail list (entry 50010: XML, the mip level each texture loads at) asks for 129 textures at full
//    size: WWE '12 never finds the memory and the load waits forever. Every WWE '12 arena has the same list: the
//    base arena's is used;
//  - its ring (951-980) stands too high for WWE '12: the base arena's ring parts are used (they find the WWE '13
//    apron and mat pictures by name), WWE '13's extra post and cover (981, 982) are left out;
//  - entry 61000 (a 256 x 128 picture every WWE '12 arena has) comes from the base;
//  - the breakable barricade corners (390-397, ar_fence01/02_c/f/g/h: modelled at the centre, placed by WWE '13's
//    code for its "OMG moments") are moved into the corners in the model itself.
// Lights, crowd, placement and visibility files of WWE '13 work as they are.
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

// The WWE '13 file (as on the disc) and the base arena's WWE '12 file -> the WWE '12 arena.
inline bool Port(const Bytes &w13, const Bytes &base12, Bytes &out, std::string *why) {
  auto fail = [&](const std::string &w) {
    if (why)
      *why = w;
    return false;
  };
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
  if (a.root.kids.empty() || b.root.kids.empty())
    return fail("no arena data");
  svrfmt::ArcNode &ga = a.root.kids[0], &gb = b.root.kids[0];
  if (!svrfmt::ArchiveExpand(ga) || !svrfmt::ArchiveExpand(gb))
    return fail("unreadable arena list");
  const std::set<uint32_t> from_base = {50010, 61000, 951, 952, 953, 956, 960, 964, 973, 974, 980};
  const std::set<uint32_t> drop = {981, 982};
  ga.kids.erase(std::remove_if(ga.kids.begin(), ga.kids.end(),
                               [&](const svrfmt::ArcNode &k) { return from_base.count(k.id) || drop.count(k.id); }),
                ga.kids.end());
  for (const auto &k : gb.kids)
    if (from_base.count(k.id))
      ga.kids.push_back(k);
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

}  // namespace port13
