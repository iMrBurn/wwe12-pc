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
#include <cctype>
#include <cstring>
#include <functional>
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

// The video screens (models 800-899). Same numbers in WWE '12 and the 2K games (800 small screen, 801/802 and
// 803/804 big ones, 806/807 the wide ribbons, 808/809 strips), but the 2K games draw them from one "layout_*" picture
// of their own: its top quarter (v 0-0.25) for the ribbons, the rest (v 0.25-1) the titantron. WWE '12 puts its
// movie there (320 x 320: the titantron's top 0-0.684, the two side panels' strip 0.691-0.998 under it; arena and
// minitron movies whole): drawn as they were, the 2K screens showed the wrestler cut and stretched across the
// ribbons (WWE 2K16's WrestleMania 31, 2026-10-26). Their UVs moved from the 2K places to WWE '12's: the titantron
// part to the titantron's, the ribbons' part to the side panels', the whole picture to the whole; their textures
// named as WWE '12's screen of that number (titantron_movie / arena_movie / minitron_movie). Changed in the model's
// bytes (JboyWrite doesn't give these models back byte for byte).
inline void ScreensTo12(svrfmt::ArcNode &ga) {
  constexpr size_t B = 8;
  // WWE '12 gives each screen its movie by number (RenderDoc, WrestleMania 31, 2026-10-26: renamed, screen 805 still
  // showed the live camera): the titantron on 800/803/806/808, the arena's movie on 801/807/809, the live camera on
  // 804 and 805; 802 none of WWE '12's. The 2K games' screens by role, renumbered to WWE '12's: their 801 (the big
  // wall) -> 808, 808 (the strip above the stage) -> 800, 805 (an LED band) -> 803, 806 (the ribbons) stays, 800
  // (the high screens) -> 807 (the arena's movie) when 808 takes 800; their repeats with another layout (a show's
  // look: 802 = 801, 803/804 = 800, 807 = 806, 809 = 808) left out. Their "screen off" covers (titan_off, hidden by
  // the 2K games during the entrances) left out too: WrestleMania 31's hid its central circle.
  {
    const auto layout = [](const svrfmt::ArcNode &k) {
      const Bytes d = svrfmt::ArchiveData(k);
      return svrfmt::IsJboy(d) && std::search(d.begin(), d.end(), "layout_", "layout_" + 7) != d.end();
    };
    const auto has = [&](uint32_t id) {
      for (const auto &k : ga.kids)
        if (k.id == id && layout(k))
          return true;
      return false;
    };
    bool twok = false;  // (a 2K game's arena: screens drawn from layout_ pictures)
    for (const auto &k : ga.kids)
      twok = twok || (k.id >= 800 && k.id <= 899 && layout(k));
    const bool strip = has(808);
    std::vector<svrfmt::ArcNode> kept;
    for (auto &k : ga.kids) {
      if (!twok) {
        kept.push_back(std::move(k));
        continue;
      }
      if (layout(k)) {
        if (k.id == 802 || k.id == 803 || k.id == 804 || k.id == 807 || k.id == 809)
          continue;
        k.id = k.id == 801 ? 808 : k.id == 808 ? 800 : k.id == 805 ? 803 : k.id == 800 && strip ? 807 : k.id;
      }
      const Bytes d = svrfmt::ArchiveData(k);
      svrfmt::Model m;
      if (svrfmt::IsJboy(d) && svrfmt::JboyRead(d, m, nullptr) && !m.textures.empty() &&
          std::all_of(m.textures.begin(), m.textures.end(), [](std::string t) {
            std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            return t == "titan_off";
          }))
        continue;
      kept.push_back(std::move(k));
    }
    ga.kids = std::move(kept);
    std::sort(ga.kids.begin(), ga.kids.end(),
              [](const svrfmt::ArcNode &x, const svrfmt::ArcNode &y) { return x.id < y.id; });
  }
  for (auto &k : ga.kids) {
    if (k.id < 800 || k.id > 899)
      continue;
    Bytes d = svrfmt::ArchiveData(k);
    svrfmt::Model m;
    if (!svrfmt::IsJboy(d) || !svrfmt::JboyRead(d, m, nullptr))
      continue;
    const uint32_t tptr = svrfmt::Be32(&d[0x2C]);
    bool changed = false;
    for (auto &me : m.meshes) {
      int slot = -1;
      for (const auto &p : me.params)
        if (p.type == 0x0f && p.value.size() >= 4 && svrfmt::Be32(p.value.data()) < m.textures.size() &&
            m.textures[svrfmt::Be32(p.value.data())].rfind("layout_", 0) == 0)
          slot = int(svrfmt::Be32(p.value.data()));
      if (slot < 0 || me.uvs.empty())
        continue;
      // (never minitron_movie: WWE '12's minitron is its live camera, the 2K screens show movies: WrestleMania 31's
      // LED band under the letters, layout_mini_tro, came out as a mirror of the arena)
      const bool mini = m.textures[size_t(slot)].find("mini") != std::string::npos;
      const char *name = !mini && (k.id == 801 || k.id == 807 || k.id == 809) ? "arena_movie" : "titantron_movie";
      float v0 = 1e9f, v1 = -1e9f;
      for (const auto &uv : me.uvs) {
        v0 = std::min(v0, uv[1]);
        v1 = std::max(v1, uv[1]);
      }
      // from (the 2K picture's part this mesh shows) to (WWE '12's part for it)
      float f0 = 0, f1 = 1, t0 = 0, t1 = 1;
      const bool whole = std::strcmp(name, "titantron_movie") != 0;
      if (mini) {
        t1 = 0.684f;  // (its own movie, whole: the titantron's part)
      } else if (v1 <= 0.26f) {
        f1 = 0.25f;
        if (!whole)
          t0 = 0.691f, t1 = 0.998f;
      } else if (v0 >= 0.24f) {
        f0 = 0.25f;
        if (!whole)
          t1 = 0.684f;
      }
      const size_t uvs = B + svrfmt::Be32(&me.raw[0x70]);
      if (uvs + 8 * me.uvs.size() > d.size() || B + tptr + 16 * (size_t(slot) + 1) > d.size())
        continue;
      for (size_t q = 0; q < me.uvs.size(); ++q)
        svrfmt::PutBeF(&d[uvs + 8 * q + 4], t0 + (std::clamp(me.uvs[q][1], f0, f1) - f0) / (f1 - f0) * (t1 - t0));
      uint8_t *tn = &d[B + tptr + 16 * size_t(slot)];
      std::memset(tn, 0, 16);
      std::memcpy(tn, name, std::strlen(name));
      changed = true;
    }
    if (changed)
      svrfmt::ArchiveReplace(k, d);
  }
}

// The 2K games' animated LED panels: yBG_UVScroll with 2 to 5 pictures (WWE '12's has one); WWE '12 fills the other
// slots with whatever it has bound (WrestleMania 31's central circle and ramp sides showed pieces of the screen):
// made yBG_Default with their first picture (still, as WWE 2K16 shows the circle). Changed in place.
inline void LedPanelsTo12(svrfmt::ArcNode &ga) {
  constexpr size_t B = 8;
  for (auto &k : ga.kids) {
    Bytes d = svrfmt::ArchiveData(k);
    svrfmt::Model m;
    if (!svrfmt::IsJboy(d) || !svrfmt::JboyRead(d, m, nullptr) || !m.w12)
      continue;
    const uint32_t mptr = svrfmt::Be32(&d[0x1C]);
    bool changed = false;
    for (size_t i = 0; i < m.meshes.size(); ++i) {
      int pictures = 0;
      for (const auto &p : m.meshes[i].params)
        pictures += p.type == 0x0f;
      const size_t rec = B + mptr + 0xB8 * i;
      if (m.meshes[i].shader != "yBG_UVScroll" || pictures < 2 || rec + 0x8C > d.size())
        continue;
      std::memset(&d[rec + 0x7C], 0, 16);
      std::memcpy(&d[rec + 0x7C], "yBG_Default", 11);
      changed = true;
    }
    if (changed)
      svrfmt::ArchiveReplace(k, d);
  }
}

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
  ScreensTo12(ga);
  LedPanelsTo12(ga);
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

// The arena's banner in a WWE 2K game's menus (its select screen shows none we can use: these are the Universe's):
//  - 2K16, 2K17: "Pbg_NN_<short name>" (512 x 256, NN = the arena file), in menu/MnLayHD.pac (2K16, MLYE/UMTX) or
//    menu/MnLayHD_WWEUnivese.pac (2K17);
//  - 2K15: "arena_<name>" (256 x 128, MnLayHD.pac MLYE/USCH), named after the arena's "PPV_<name>" record in
//    pac/wuniData.pac's 52-byte udt tables {name[40], string id, arena number, index}: PPV_raw'02 -> arena_raw02, but
//    shortened: PPV_main -> arena_maineve, PPV_armage'02 -> arena_arm02, PPV_survivor'02 -> arena_surviv02 (one of
//    the names starts with the other, same number), PPV_ER -> arena_ex;
//  - 2K14: the same pictures, no such records: by the arena's number (kBanners2K14, from its names kArenas2K14).
// (2026-10-26: WWE 2K16's WrestleMania 31 came with the base's banner.)
struct Banner2K14 {
  int file;
  const char *name;
};
inline constexpr Banner2K14 kBanners2K14[] = {
    {0, "smack"},  {1, "raw"},       {2, "NXT"},    {3, "super"},   {4, "over"},     {5, "out"},     {6, "money"},
    {7, "summer"}, {8, "NofC"},      {9, "hell"},   {11, "survivor"}, {12, "TLC"},   {13, "royal"},  {14, "chamber"},
    {15, "WM29"},  {16, "ex"},       {17, "maineve"}, {21, "chamber"}, {22, "royal"}, {23, "raw"},   {56, "WM30"}};

inline bool Banner2K(const std::function<Bytes(const char *)> &read, uint32_t title, int file, svrfmt::Image &out) {
  // Every picture of the menu packs that may hold them, by name.
  std::map<std::string, Bytes> pics;
  const auto walk = [&](auto &self, svrfmt::ArcNode &n, int depth) -> void {
    const Bytes d = svrfmt::ArchiveData(n);
    std::vector<svrfmt::BundleTexture> texs;
    if (svrfmt::BundleRead(d, texs)) {
      for (auto &t : texs)
        if (t.name.rfind("Pbg_", 0) == 0 || t.name.rfind("arena_", 0) == 0)
          pics.emplace(t.name, std::move(t.data));
      return;
    }
    if (depth < 4 && svrfmt::ArchiveExpand(n))
      for (auto &c : n.kids)
        self(self, c, depth + 1);
  };
  for (const char *p : {"pac/menu/MnLayHD.pac", "pac/menu/MnLayHD_WWEUnivese.pac"}) {
    svrfmt::Archive a;
    if (svrfmt::ArchiveOpen(read(p), a, nullptr))
      for (auto &k : a.root.kids)
        walk(walk, k, 0);
  }
  const auto take = [&](const Bytes &dds) { return svrfmt::DdsDecode(dds, out); };
  char pbg[16];
  snprintf(pbg, sizeof(pbg), "Pbg_%02d_", file);
  for (const auto &[name, dds] : pics)
    if (name.rfind(pbg, 0) == 0 && take(dds))
      return true;
  // The arena_ picture's name: 2K14's table, else the PPV_ record.
  std::string stem;
  if (title == 0x545408B2) {
    for (const auto &b : kBanners2K14)
      if (b.file == file)
        stem = b.name;
    if (file >= 28 && file <= 55)
      stem = "WM" + std::to_string(file - 27);
  } else {
    svrfmt::Archive u;
    if (svrfmt::ArchiveOpen(read("pac/wuniData.pac"), u, nullptr)) {
      const auto udt = [&](auto &self, svrfmt::ArcNode &n, int depth) -> void {
        if (depth < 4 && svrfmt::ArchiveExpand(n) && !n.kids.empty()) {
          for (auto &c : n.kids)
            self(self, c, depth + 1);
          return;
        }
        const Bytes b = svrfmt::ArchiveData(n);
        uint32_t count = 0, size = 0;
        if (b.size() < 32 || memcmp(b.data(), "udt", 3) != 0)
          return;
        memcpy(&count, b.data() + 4, 4);
        memcpy(&size, b.data() + 8, 4);
        if (size != 52)
          return;
        count = std::min<uint32_t>(count, uint32_t((b.size() - 32) / 52));
        for (uint32_t i = 0; i < count && stem.empty(); ++i) {
          const uint8_t *r = b.data() + 32 + size_t(i) * 52;
          uint32_t bg;
          memcpy(&bg, r + 44, 4);
          const std::string name(reinterpret_cast<const char *>(r), strnlen(reinterpret_cast<const char *>(r), 40));
          if (bg == uint32_t(file) && name.rfind("PPV_", 0) == 0)
            for (char c : name.substr(4))
              if (c != '\'')
                stem += c;
        }
      };
      for (auto &k : u.root.kids)
        udt(udt, k, 0);
    }
  }
  if (stem.empty())
    return false;
  const auto lower = [](std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
  };
  const auto split = [](const std::string &s, std::string &letters, std::string &digits) {
    size_t k = s.size();
    while (k > 0 && std::isdigit(static_cast<unsigned char>(s[k - 1])))
      --k;
    letters = s.substr(0, k);
    digits = s.substr(k);
  };
  stem = lower(stem);
  if (stem == "er")
    stem = "ex";
  std::string sl, sd;
  split(stem, sl, sd);
  const Bytes *best = nullptr;
  for (const auto &[name, dds] : pics) {
    if (name.rfind("arena_", 0) != 0)
      continue;
    const std::string n = lower(name.substr(6));
    if (n == stem)
      return take(dds);
    std::string nl, nd;
    split(n, nl, nd);
    if (!best && nd == sd && !nl.empty() && !sl.empty() && (nl.rfind(sl, 0) == 0 || sl.rfind(nl, 0) == 0))
      best = &dds;
  }
  return best && take(*best);
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

// The arena's transition movie in WWE 2K15-2K17 (Bink 2, 640 x 640 with alpha; made into WWE '12's Bink 1 by
// bink2::ToBink1): 2K16 / 2K17 pac/mov/transition/0000NN.pac (one movie each), 2K15 pac/evt/evtmov_transition.pac
// MOVT/M0000NN. (WWE 2K14's are WWE '13's kind: evtmov.pac MOVT/M0NN, Bink 1, TransitionVideo.)
inline bool Transition2K(const std::function<Bytes(const char *)> &read, int file, Bytes &bk2) {
  bk2.clear();
  const auto first = [&](auto &self, svrfmt::ArcNode &n, int depth) -> void {
    if (!bk2.empty())
      return;
    if (depth < 3 && svrfmt::ArchiveExpand(n) && !n.kids.empty()) {
      for (auto &c : n.kids)
        self(self, c, depth + 1);
      return;
    }
    const Bytes d = depth ? svrfmt::ArchiveData(n) : Bytes();
    if (d.size() > 44 && memcmp(d.data(), "KB2", 3) == 0)
      bk2 = d;
  };
  char p[48];
  snprintf(p, sizeof(p), "pac/mov/transition/%06d.pac", file);
  svrfmt::Archive a;
  if (svrfmt::ArchiveOpen(read(p), a, nullptr))
    first(first, a.root, 0);
  if (bk2.empty() && svrfmt::ArchiveOpen(read("pac/evt/evtmov_transition.pac"), a, nullptr)) {
    char name[16];
    snprintf(name, sizeof(name), "M%06d", file);
    for (auto &k : a.root.kids)
      if (k.group == "MOVT" && k.name.compare(0, 7, name) == 0)
        first(first, k, 1);
  }
  return !bk2.empty();
}

// A WWE 2K arena's entrance plate movie, `name` the name plate, else the location plate (the WWE 2K games draw the
// text over them with their fonts): 2K16 / 2K17 pac/mov/bg_name|bg_locator/NNNNNN.pac, 2K15
// pac/evt/evtmov_bg_name|evtmov_bg_locate.pac MOVN|MOVL/MNNNNNN (Bink 2, alpha the tools don't give back: the plate on
// black, with the arena's lights), 2K14 pac/evt/evtmov2|evtmov3.pac MOVN|MOVL/MNNN (Bink 1 with an alpha plane).
inline bool PlateMovie2K(const std::function<Bytes(const char *)> &read, int file, bool name, Bytes &bk2) {
  bk2.clear();
  const auto first = [&](auto &self, svrfmt::ArcNode &n, int depth) -> void {
    if (!bk2.empty())
      return;
    if (depth < 3 && svrfmt::ArchiveExpand(n) && !n.kids.empty()) {
      for (auto &c : n.kids)
        self(self, c, depth + 1);
      return;
    }
    const Bytes d = depth ? svrfmt::ArchiveData(n) : Bytes();
    if (d.size() > 44 && (memcmp(d.data(), "KB2", 3) == 0 || memcmp(d.data(), "BIK", 3) == 0))
      bk2 = d;
  };
  char p[48];
  snprintf(p, sizeof(p), "pac/mov/%s/%06d.pac", name ? "bg_name" : "bg_locator", file);
  svrfmt::Archive a;
  if (svrfmt::ArchiveOpen(read(p), a, nullptr))
    first(first, a.root, 0);
  const auto in_pac = [&](const char *pac, const char *group, const char *entry) {
    if (!bk2.empty() || !svrfmt::ArchiveOpen(read(pac), a, nullptr))
      return;
    for (auto &k : a.root.kids)
      if (k.group == group && k.name.compare(0, strlen(entry), entry) == 0 &&
          (k.name.size() == strlen(entry) || k.name[strlen(entry)] == ' ' || k.name[strlen(entry)] == '\0'))
        first(first, k, 1);
  };
  char m6[16], m3[16];
  snprintf(m6, sizeof(m6), "M%06d", file);
  snprintf(m3, sizeof(m3), "M%03d", file);
  in_pac(name ? "pac/evt/evtmov_bg_name.pac" : "pac/evt/evtmov_bg_locate.pac", name ? "MOVN" : "MOVL", m6);
  in_pac(name ? "pac/evt/evtmov2.pac" : "pac/evt/evtmov3.pac", name ? "MOVN" : "MOVL", m3);
  return !bk2.empty();
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
