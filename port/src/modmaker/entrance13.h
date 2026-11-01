// A superstar's entrance from WWE '13 (docs/plan_movesets.md, step 4). WWE '13's entrances are WWE '12's format
// (Kaiko's OTHER_GAMES_ENTRANCES.md; compared here: Cena's script 93 % the same bytes, the same track trees):
//  - WWE '13: pac\evt\Entrance00..08.pac (xcompressed EPK8), groups "EVP " (the script, "0FOP") and "EVZ " (a PACH of
//    YANM tracks, one per object), entries named by the entrance number: NNN and its variants 1xNNN (10NNN..14NNN);
//  - WWE '12: pac\evt\Nyujyo0-9,A.pac, the same entries in groups by number range (Nyujyo0 EVP1 / EVZ1 100.., Nyujyo1
//    EVP2 137.., Nyujyo2 EVP3 179.., Nyujyo3 EVP4 272.., Nyujyo4 EVP5 328.., Nyujyo5 EVPE 480.., Nyujyo6 EVPF 512..,
//    Nyujyo7 EVPG 532.., Nyujyo8 EVPH 552.., Nyujyo9 EVPI 600..855, NyujyoA EVPK 50..55); the superstar's CHAR / PRO
//    +0x1D4 names the entrance (16-bit little-endian).
// (Its theme comes with the superstar, any game's: theme_source.h, superstar/theme.wem.)
// Besides its script and tracks, an entrance has a game event script (pac\gm.pac GMEV/NNNN, 1536 bytes, PACH; Cena's
// the same bytes in both games) and its effects (pac\prtcls.pac PCE6/NNNN, PACH): without them the superstar never
// showed and no music played (2026-10-28, the game's lookups logged: /GMEV/0246 and /PCE6/0246 missing).
// WWE 2K14 stores its entrances (Entrance00-08.pac) and victory scenes (Winning00-03.pac) the same way, in xcompressed
// EPAC (Kaiko's wwe13_entrance.py): the same scripts and tracks (Cena 139: the same sizes, the same YANM; its tracks'
// header names "25000139_A00_def" where WWE '12 / '13 have "25000", after a length of 5), so the readers below take it too.
// Entrance(): an entrance's entries as WWE '13 stores them (unpacked), for the game to serve under WWE '12's names; the
// tracks' IK legs baked to FK (yanm_ik.h: WWE '12 ignores IK, the feet walked on their heels).
#pragma once

#include <cstdio>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <tuple>
#include <vector>

#include "game_disc.h"
#include "svrfmt/archive.h"
#include "svrfmt/pac.h"
#include "svrfmt/xcompress.h"
#include "yanm_ik.h"

namespace entrance13 {

using Bytes = std::vector<uint8_t>;

struct Part {
  int number = 0;    // NNN or a variant 1xNNN
  std::string kind;  // "EVP" (the script), "EVZ" (the tracks), "GMEV" (the game event script), "PCE6" (the effects)
  Bytes data;
};

// 2K14's track records name their object "25000254_A00_def" (+0x4C, the object's number 25000 + index, then the
// entrance's and its variant's); WWE '12 / '13 write "25000" alone. As WWE '12's: the text cut after its 5 digits (its
// own 2K14 entrance served as is, the superstar never showed, 2026-11-01). Returns how many were cut.
inline int NameTracks12(Bytes &evz) {
  Bytes b = svrfmt::IsBpe(evz) ? svrfmt::Unpack(evz) : evz;
  svrfmt::Archive a;
  if (!svrfmt::ArchiveOpen(b, a, nullptr))
    return 0;
  int n = 0;
  for (auto &k : a.root.kids) {
    Bytes d = svrfmt::ArchiveData(k);
    if (svrfmt::IsBpe(d))
      d = svrfmt::Unpack(d);
    if (d.size() < 0x70 || d[0x4C] != '2' || d[0x4C + 5] == 0)
      continue;
    std::fill(d.begin() + 0x4C + 5, d.begin() + 0x6C, uint8_t(0));
    svrfmt::ArchiveReplace(k, d), ++n;
  }
  if (n)
    evz = svrfmt::ArchiveSave(a);
  return n;
}

// WWE '12's group letter for an entrance number (its base number: the last three digits), "" if none.
inline std::string Group12(int number) {
  const int n = number % 1000;
  if (n >= 50 && n <= 55)
    return "K";
  struct R {
    int from;
    const char *g;
  };
  static const R r[] = {{600, "I"}, {552, "H"}, {532, "G"}, {512, "F"}, {480, "E"}, {328, "5"},
                        {272, "4"}, {179, "3"}, {137, "2"}, {100, "1"}};
  for (const auto &x : r)
    if (n >= x.from)
      return x.g;
  return "";
}

// WWE '12's Nyujyo file for a group letter.
inline std::string File12(const std::string &group) {
  static const char *g = "12345EFGHIK";
  for (int i = 0; g[i]; ++i)
    if (group.size() == 1 && group[0] == g[i]) {
      char f[32];
      snprintf(f, sizeof(f), "pac/evt/Nyujyo%c.pac", i < 10 ? char('0' + i) : 'A');
      return f;
    }
  return "";
}

// Whether WWE '12 (w12: the player's) has an entrance `number` already (in its Nyujyo file for that group).
inline bool Exists12(const disc::GameDisc &w12, int number) {
  const std::string g = Group12(number), file = File12(g);
  Bytes b;
  svrfmt::Archive a;
  if (file.empty() || !w12.Read(file, b) || !svrfmt::ArchiveOpen(b, a, nullptr))
    return true;  // (unknown: as taken)
  for (const auto &k : a.root.kids)
    if (k.group.rfind("EVP", 0) == 0 && atoi(k.name.c_str()) == number)
      return true;
  return false;
}

// WWE '12's entrance scripts (their base number, not the variants) end with a tail (Kaiko's svr08_entrance.py:
// header +0x18 = 0x10000, +0x1C its offset; {u16 number, u16 2, offset of the records}, records {u16 type, u16 0, u32
// 100, u32 3, offset of 3 items {u8 kind, u24 0, offset of the value}}): type 1, the music (u32 999: the superstar's
// own), and type 8, the movie (u16 999: its titantron). WWE '13's and 2K14's have none; without it the game played the
// entrance with no music, no crowd and no titantron (2K14 Ultimate Warrior, 2026-11-01; WWE '13 Mankind had been given
// this tail by hand). Offsets are from +8, listed in the POF0 after the data. Returns whether a tail was added.
inline bool AddTail12(Bytes &evp, int number) {
  using yanm::ik::detail::Put16;
  using yanm::ik::detail::Put32;
  auto b32 = [&](size_t o) { return uint32_t(evp[o]) << 24 | uint32_t(evp[o + 1]) << 16 | uint32_t(evp[o + 2]) << 8 | evp[o + 3]; };
  if (evp.size() < 0x50 || memcmp(evp.data(), "0FOP", 4) || b32(0x1C) != 0)
    return false;
  const uint32_t t = b32(4);  // the data's end (from +8): the POF0 chunk follows
  if (t % 4 || 8 + size_t(t) + 8 > evp.size() || memcmp(&evp[8 + t], "POF0", 4))
    return false;
  std::vector<uint32_t> pof = yanm::ik::detail::PofRead(evp, 8 + t + 8, b32(8 + t + 4));
  const Bytes rest(evp.begin() + ptrdiff_t(8 + t + 8 + b32(8 + t + 4)), evp.end());
  evp.resize(8 + t);
  Bytes tail(0x70, 0);
  Put16(tail, 0, uint16_t(number)), Put16(tail, 2, 2), Put32(tail, 4, t + 8);
  for (const auto &[at, type, items] : {std::tuple<uint32_t, uint16_t, uint32_t>{8, 1, 0x28}, {0x18, 8, 0x40}})
    Put16(tail, at, type), Put32(tail, at + 4, 100), Put32(tail, at + 8, 3), Put32(tail, at + 12, t + items);
  const uint32_t items[6][2] = {{0x02000000, 0x5C}, {0x01000000, 0x58}, {0x01000000, 0x5A},
                                {0x03000000, 0x60}, {0x03000000, 0x62}, {0x03000000, 0x64}};
  for (uint32_t i = 0; i < 6; ++i)
    Put32(tail, 0x28 + 8 * i, items[i][0]), Put32(tail, 0x2C + 8 * i, t + items[i][1]);
  Put32(tail, 0x5C, 999), Put16(tail, 0x60, 999);
  evp.insert(evp.end(), tail.begin(), tail.end());
  Put32(evp, 0x18, 0x10000), Put32(evp, 0x1C, t);
  for (uint32_t o : {0x1Cu - 8, t + 4, t + 0x14, t + 0x24, t + 0x2C, t + 0x34, t + 0x3C, t + 0x44, t + 0x4C, t + 0x54})
    pof.push_back(o);
  std::sort(pof.begin(), pof.end());
  pof.erase(std::unique(pof.begin(), pof.end()), pof.end());  // (+0x1C is listed already, even when 0)
  Put32(evp, 4, t + 0x70), Put32(evp, 0xC, t + 0x70);
  const Bytes pb = yanm::ik::detail::PofWrite(pof);
  evp.insert(evp.end(), {'P', 'O', 'F', '0'});
  for (int i = 0; i < 4; ++i)
    evp.push_back(uint8_t(uint32_t(pb.size()) >> (24 - 8 * i)));
  evp.insert(evp.end(), pb.begin(), pb.end());
  evp.insert(evp.end(), rest.begin(), rest.end());
  return true;
}

// The mod's file of a part (superstar/entrance/EVP_246.bin, GMEV_246.bin...): the game serves it as
// /EVP<group>/<number>, /GMEV/<number, 4 digits>...
inline std::string ModPath(const Part &p) {
  return "superstar/entrance/" + p.kind + "_" + std::to_string(p.number) + ".bin";
}

// The WWE '13 entrance `number`'s parts (its own and its variants'), from the player's WWE '13 (w13).
inline bool Entrance(const disc::GameDisc &w13, int number, std::vector<Part> &out, std::string *why) {
  out.clear();
  for (int i = 0; i <= 8; ++i) {
    char f[32];
    snprintf(f, sizeof(f), "pac/evt/Entrance%02d.pac", i);
    Bytes b;
    if (!w13.Read(f, b))
      continue;
    if (svrfmt::IsXcompress(b)) {
      Bytes o;
      if (!svrfmt::XcompressDecode(b, o))
        continue;
      b.swap(o);
    }
    svrfmt::Archive a;
    if (!svrfmt::ArchiveOpen(b, a, nullptr))
      continue;
    for (auto &k : a.root.kids) {
      const int n = atoi(k.name.c_str());
      if (n % 1000 != number % 1000 || (n != number && n / 1000 < 10))
        continue;
      if (k.group.rfind("EVP", 0) != 0 && k.group.rfind("EVZ", 0) != 0)
        continue;
      out.push_back({n, k.group.substr(0, 3), svrfmt::Unpack(svrfmt::ArchiveData(k))});
      if (out.back().kind == "EVP" && n < 1000)  // (its music and movie: WWE '12's tail)
        AddTail12(out.back().data, n);
      if (out.back().kind == "EVZ") {  // the body's IK legs as FK, for WWE '12 (yanm_ik.h); 2K14's names as WWE '12's
        NameTracks12(out.back().data);
        yanm::ik::BakeEntranceTracks(out.back().data);
      }
    }
  }
  // its game event script and its effects
  if (!out.empty())
    for (const auto &[file, group] : {std::pair<const char *, const char *>{"pac/gm.pac", "GMEV"}, {"pac/prtcls.pac", "PCE6"}}) {
      Bytes b;
      if (!w13.Read(file, b))
        continue;
      if (svrfmt::IsXcompress(b)) {
        Bytes o;
        if (!svrfmt::XcompressDecode(b, o))
          continue;
        b.swap(o);
      }
      svrfmt::Archive a;
      if (!svrfmt::ArchiveOpen(b, a, nullptr))
        continue;
      char name[8];
      snprintf(name, sizeof(name), "%04d", number % 10000);
      for (auto &k : a.root.kids) {
        std::string g = k.group, nm = k.name;
        while (!g.empty() && (g.back() == ' ' || g.back() == '\0'))
          g.pop_back();
        while (!nm.empty() && (nm.back() == ' ' || nm.back() == '\0'))
          nm.pop_back();
        if (g == group && nm == name)
          out.push_back({number, group, svrfmt::Unpack(svrfmt::ArchiveData(k))});
      }
    }
  if (out.empty() && why)
    *why = "no such entrance in WWE '13";
  return !out.empty();
}

// A superstar's victory scene in WWE '13 (pac\evt\Winning00-03.pac, xcompressed EPK8, groups "EVP " / "EVZ ", entries
// 6000 + its number: Mankind 6246), its tracks' IK legs baked as an entrance's. WWE '12 looks for 6000 + the superstar's
// own number (sub_82407DC8: participant +16 + 6000, every group letter tried by sub_828CCC10 "/%s%c/%d"), so an added
// superstar's is served under 6000 + its slot (wwe12_arena_pages.cpp). Parts "EVP" and "EVZ", number 6000 + `number`.
inline bool Victory(const disc::GameDisc &w13, int number, std::vector<Part> &out) {
  out.clear();
  const int want = 6000 + number % 1000;
  for (int i = 0; i <= 3; ++i) {
    char f[32];
    snprintf(f, sizeof(f), "pac/evt/Winning%02d.pac", i);
    Bytes b;
    if (!w13.Read(f, b))
      continue;
    if (svrfmt::IsXcompress(b)) {
      Bytes o;
      if (!svrfmt::XcompressDecode(b, o))
        continue;
      b.swap(o);
    }
    svrfmt::Archive a;
    if (!svrfmt::ArchiveOpen(b, a, nullptr))
      continue;
    for (auto &k : a.root.kids) {
      if (atoi(k.name.c_str()) != want || (k.group.rfind("EVP", 0) != 0 && k.group.rfind("EVZ", 0) != 0))
        continue;
      out.push_back({want, k.group.substr(0, 3), svrfmt::Unpack(svrfmt::ArchiveData(k))});
      if (out.back().kind == "EVZ") {
        NameTracks12(out.back().data);
        yanm::ik::BakeEntranceTracks(out.back().data);
      }
    }
  }
  return out.size() == 2;
}

}  // namespace entrance13
