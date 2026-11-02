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
#include <set>
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

// WWE '12's victory scripts end with a shorter tail: one record, type 8, the movie (u16 999), 3 items of kind 3 (u16
// 999, 0, 0), 0x3C bytes (Cena's 6139). WWE '13's and 2K14's have none: given this one. Returns whether it was added.
inline bool AddVictoryTail12(Bytes &evp, int number) {
  using yanm::ik::detail::Put16;
  using yanm::ik::detail::Put32;
  auto b32 = [&](size_t o) { return uint32_t(evp[o]) << 24 | uint32_t(evp[o + 1]) << 16 | uint32_t(evp[o + 2]) << 8 | evp[o + 3]; };
  if (evp.size() < 0x50 || memcmp(evp.data(), "0FOP", 4) || b32(0x1C) != 0)
    return false;
  const uint32_t t = b32(4);
  if (t % 4 || 8 + size_t(t) + 8 > evp.size() || memcmp(&evp[8 + t], "POF0", 4))
    return false;
  std::vector<uint32_t> pof = yanm::ik::detail::PofRead(evp, 8 + t + 8, b32(8 + t + 4));
  const Bytes rest(evp.begin() + ptrdiff_t(8 + t + 8 + b32(8 + t + 4)), evp.end());
  evp.resize(8 + t);
  Bytes tail(0x3C, 0);
  Put16(tail, 0, uint16_t(number)), Put16(tail, 2, 1), Put32(tail, 4, t + 8);
  Put16(tail, 8, 8), Put32(tail, 0xC, 100), Put32(tail, 0x10, 3), Put32(tail, 0x14, t + 0x18);
  for (uint32_t i = 0; i < 3; ++i)
    Put32(tail, 0x18 + 8 * i, 0x03000000), Put32(tail, 0x1C + 8 * i, t + 0x30 + 2 * i);
  Put16(tail, 0x30, 999);
  evp.insert(evp.end(), tail.begin(), tail.end());
  Put32(evp, 0x18, 0x10000), Put32(evp, 0x1C, t);
  for (uint32_t o : {0x1Cu - 8, t + 4, t + 0x14, t + 0x1C, t + 0x24, t + 0x2C})
    pof.push_back(o);
  std::sort(pof.begin(), pof.end());
  pof.erase(std::unique(pof.begin(), pof.end()), pof.end());
  Put32(evp, 4, t + 0x3C), Put32(evp, 0xC, t + 0x3C);
  const Bytes pb = yanm::ik::detail::PofWrite(pof);
  evp.insert(evp.end(), {'P', 'O', 'F', '0'});
  for (int i = 0; i < 4; ++i)
    evp.push_back(uint8_t(uint32_t(pb.size()) >> (24 - 8 * i)));
  evp.insert(evp.end(), pb.begin(), pb.end());
  evp.insert(evp.end(), rest.begin(), rest.end());
  return true;
}

// ---- WWE 2K15 / 2K16 / 2K17 (after Kaiko's wwe13_entrance.py, docs/OTHER_GAMES_ENTRANCES.md: 2K15
// pac\evt\Entrance00-10.pac, groups EVP2 / EVZ; 2K16 / 2K17 one pac per scene, pac\evt\NNNNN.pac (victories 06NNN,
// variants 1xNNN), EPAC with 4-character names or 2K17's HSPC with the tracks in an SHDC; their game event scripts in
// pac\gimmick\gmevNNNN.pac, effects in pac\particle\pce6NNNN.pac, props in pac\prop\NNNN.pac.)
namespace detail {

inline uint32_t Be32(const Bytes &b, size_t o) {
  return o + 4 <= b.size() ? uint32_t(b[o]) << 24 | uint32_t(b[o + 1]) << 16 | uint32_t(b[o + 2]) << 8 | b[o + 3] : 0u;
}

inline bool OpenPac(const disc::GameDisc &g, const std::string &f, svrfmt::Archive &a) {
  Bytes b;
  if (!g.Read(f, b))
    return false;
  if (svrfmt::IsXcompress(b)) {
    Bytes o;
    if (!svrfmt::XcompressDecode(b, o))
      return false;
    b.swap(o);
  }
  return svrfmt::ArchiveOpen(b, a, nullptr);
}

// An entry's data, as a track bank: a PACH as is, an SHDC (2K17) made a PACH of the same leaves.
inline Bytes AsPach(svrfmt::ArcNode &k) {
  Bytes u = svrfmt::Unpack(svrfmt::ArchiveData(k));
  if (svrfmt::IsPach(u))
    return u;
  if (u.size() < 4 || memcmp(u.data(), "SHDC", 4) || !svrfmt::ArchiveExpand(k))
    return {};
  std::vector<svrfmt::PachEntry> leaves;
  for (auto &c : k.kids)
    leaves.push_back({c.id, svrfmt::Unpack(svrfmt::ArchiveData(c))});
  return svrfmt::PachWrite(leaves);
}

}  // namespace detail

// 2K15-2K17 scripts' 788-byte object slots as WWE '12's 212 (words 0-50 and 195-196: 2K14's slot; the rest, longer
// rope / turnbuckle key lists, dropped: Kaiko's evp_788_to_212). The script is written again (header, objects, slots,
// POF0; its tail left out: AddTail12 writes WWE '12's). False when its slots aren't 788 bytes (left as it is).
inline bool Evp788To212(Bytes &evp) {
  using detail::Be32;
  using yanm::ik::detail::Put32;
  if (evp.size() < 0x50 || memcmp(evp.data(), "0FOP", 4))
    return false;
  const uint32_t n = Be32(evp, 0x40) & 0xFFFF, t = Be32(evp, 0x48) + 8, end = Be32(evp, 4) + 8;
  if (!n || t + 32 * size_t(n) > evp.size() || end > evp.size())
    return false;
  // the slot size: from one object's slots to the next one's
  int slot = 0;
  for (uint32_t i = 0; i < n && !slot; ++i) {
    const uint32_t ns = Be32(evp, t + 32 * i + 24) >> 16, bp = Be32(evp, t + 32 * i + 28) + 8;
    uint32_t next = end;
    for (uint32_t j = 0; j < n; ++j) {
      const uint32_t b = Be32(evp, t + 32 * j + 28) + 8;
      if (b > bp && b < next)
        next = b;
    }
    if (ns && next > bp && (next - bp) % ns == 0 && (next - bp) / ns <= 1000)
      slot = int((next - bp) / ns);
  }
  if (slot != 788)
    return false;
  Bytes out(0x4C + 32 * size_t(n), 0);
  std::copy(evp.begin() + 0x10, evp.begin() + 0x4C, out.begin() + 0x10);
  Put32(out, 0x14, 0x18), Put32(out, 0x18, 0), Put32(out, 0x1C, 0), Put32(out, 0x48, 0x44);
  std::vector<uint32_t> pof = {0x14 - 8, 0x1C - 8, 0x48 - 8};
  Bytes slots;
  for (uint32_t i = 0; i < n; ++i) {
    const size_t q = t + 32 * size_t(i), o = 0x4C + 32 * size_t(i);
    std::copy(evp.begin() + long(q), evp.begin() + long(q + 32), out.begin() + long(o));
    const uint32_t ns = Be32(evp, q + 24) >> 16, bp = Be32(evp, q + 28) + 8;
    Put32(out, o + 28, uint32_t(0x4C + 32 * size_t(n) + slots.size() - 8));
    pof.push_back(uint32_t(o + 28 - 8));
    for (uint32_t k = 0; k < ns; ++k) {
      const size_t s = bp + 788 * size_t(k);
      if (s + 788 > evp.size())
        return false;
      slots.insert(slots.end(), evp.begin() + long(s), evp.begin() + long(s + 51 * 4));
      slots.insert(slots.end(), evp.begin() + long(s + 195 * 4), evp.begin() + long(s + 197 * 4));
    }
  }
  out.insert(out.end(), slots.begin(), slots.end());
  out.resize(out.size() + (16 - (out.size() - 8) % 16) % 16, 0);
  const uint32_t size = uint32_t(out.size() - 8);
  memcpy(out.data(), "0FOP", 4);
  Put32(out, 4, size);
  out[8] = 'E', out[9] = 'V', out[10] = 0, out[11] = 0;
  Put32(out, 0xC, size);
  std::sort(pof.begin(), pof.end());
  const Bytes pb = yanm::ik::detail::PofWrite(pof);
  out.insert(out.end(), {'P', 'O', 'F', '0'});
  for (int i = 0; i < 4; ++i)
    out.push_back(uint8_t(uint32_t(pb.size()) >> (24 - 8 * i)));
  out.insert(out.end(), pb.begin(), pb.end());
  // (what follows the source's POF0, if anything)
  if (end + 8 <= evp.size() && !memcmp(&evp[end], "POF0", 4)) {
    const size_t after = end + 8 + Be32(evp, end + 4);
    if (after < evp.size())
      out.insert(out.end(), evp.begin() + long(after), evp.end());
  }
  evp.swap(out);
  return true;
}

// 2K16 / 2K17 track banks (PACH of motion records) for WWE '12: YANM version 5 chunks marked 4 (read the same,
// Kaiko), mode-4 chunks (boned props) made empty (mode 0, no track; the block table code below is for taking them out) (header +0x0C first block, +0x48 block
// count, table at 0x70: 0x28 bytes each, +0x24 the block's offset from the first). Returns how many were changed.
inline int Tracks12(Bytes &evz) {
  using detail::Be32;
  using yanm::ik::detail::Put32;
  std::vector<svrfmt::PachEntry> leaves;
  if (!svrfmt::PachRead(evz, leaves))
    return 0;
  int changed = 0;
  for (auto &e : leaves) {
    Bytes l = svrfmt::Unpack(e.data);
    if (l.size() < 0x70)
      continue;
    const uint32_t first = Be32(l, 0x0C), count = Be32(l, 0x48);
    std::vector<Bytes> keep;
    bool touched = false;
    for (uint32_t i = 0; i < count && 0x70 + 0x28 * size_t(i + 1) <= l.size(); ++i) {
      const size_t ent = 0x70 + 0x28 * size_t(i), y = size_t(first) + Be32(l, ent + 0x24);
      if (y + 10 <= l.size() && !memcmp(&l[y], "YANM", 4)) {
        if (l[y + 8] == 5)
          l[y + 8] = 4, touched = true;
        if (l[y + 9] == 4) {  // (kept, made an empty chunk: mode 0, no track; taken out, a prop left with no block
          l[y + 9] = 0;        //  crashed the game, 2K16 Kevin Owens / 2K17 AJ Styles, 2026-11-02)
          l[y + 10] = l[y + 11] = 0;
          touched = true;
        }
      }
      keep.push_back(Bytes(l.begin() + long(ent), l.begin() + long(ent + 0x28)));
    }
    if (keep.size() != count && count) {
      for (uint32_t i = 0; i < count; ++i) {
        const size_t ent = 0x70 + 0x28 * size_t(i);
        if (i < keep.size())
          std::copy(keep[i].begin(), keep[i].end(), l.begin() + long(ent));
        else
          std::fill(l.begin() + long(ent), l.begin() + long(ent + 0x28), uint8_t(0));
      }
      Put32(l, 0x48, uint32_t(keep.size()));
    }
    if (touched) {
      e.data = std::move(l);
      ++changed;
    }
  }
  if (changed)
    evz = svrfmt::PachWrite(leaves);
  return changed;
}

// A script or track bank of any of these games made WWE '12's: slots, tail (base numbers), track names, IK legs.
inline void To12(Part &p) {
  if (p.kind == "EVP") {
    Evp788To212(p.data);
    if (p.number < 1000)
      AddTail12(p.data, p.number);
    else if (p.number >= 6000 && p.number < 7000)
      AddVictoryTail12(p.data, p.number);
  } else if (p.kind == "EVZ") {
    Tracks12(p.data);
    NameTracks12(p.data);
    yanm::ik::BakeEntranceTracks(p.data);
  }
}

// Every object of the script gets a motion record in the track bank: WWE '12's (and 2K15's) banks have one per
// object, an empty one (96 bytes, no block) for those that don't move; 2K16's have only the moving ones, and the game
// crashed looking for the others (2K16 Kevin Owens, read of 0x9C in sub_824DE430, 2026-11-02).
inline int FillTracks(std::vector<Part> &parts) {
  static const uint8_t kEmpty[96] = {
      0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x60,
      0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x85, 0x00, 0x00, 'r',  'o',  'o',  't',  0x00, 0x00, 0x00, 0x00, 0x83, 0x47, 0x83, 0x89,
      0x81, 0x5B, 0x00, 0x00, '%',  's',  0x00, 0x00, 'o',  'p',  'e',  'n',  ':',  '%',  's',  '\n',
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  int added = 0;
  for (const auto &evp : parts) {
    if (evp.kind != "EVP" || evp.data.size() < 0x50)
      continue;
    const uint32_t n = detail::Be32(evp.data, 0x40) & 0xFFFF;
    for (auto &evz : parts) {
      std::vector<svrfmt::PachEntry> leaves;
      if (evz.kind != "EVZ" || evz.number != evp.number || !svrfmt::PachRead(evz.data, leaves))
        continue;
      bool changed = false;
      for (uint32_t i = 0; i < n; ++i)
        if (std::none_of(leaves.begin(), leaves.end(), [&](const auto &e) { return e.id == i; })) {
          leaves.push_back({i, Bytes(kEmpty, kEmpty + sizeof(kEmpty))});
          changed = true;
          ++added;
        }
      if (changed) {
        std::sort(leaves.begin(), leaves.end(), [](const auto &x, const auto &y) { return x.id < y.id; });
        evz.data = svrfmt::PachWrite(leaves);
      }
    }
  }
  return added;
}

// A 2K16 / 2K17 scene pac (pac\evt\NNNNN.pac): its script and track bank as parts numbered `number`.
inline void ScenePac(const disc::GameDisc &g, int number, std::vector<Part> &out) {
  char f[32];
  snprintf(f, sizeof(f), "pac/evt/%05d.pac", number);
  svrfmt::Archive a;
  if (!detail::OpenPac(g, f, a))
    return;
  for (auto &k : a.root.kids) {
    const Bytes u = svrfmt::Unpack(svrfmt::ArchiveData(k));
    if (u.size() >= 4 && !memcmp(u.data(), "0FOP", 4)) {
      out.push_back({number, "EVP", u});
    } else {
      Bytes z = detail::AsPach(k);
      if (!z.empty())
        out.push_back({number, "EVZ", std::move(z)});
    }
  }
  // Its lights: the global object's tracks are in pac\evt\light\elNNNNN.pac (EVL, a PACH: 2K16 Kevin Owens' 458 has
  // object 11's there, the EVZ hasn't it; WWE '12 keeps them in the EVZ and crashed without them, 2026-11-02): put in.
  snprintf(f, sizeof(f), "pac/evt/light/el%05d.pac", number);
  svrfmt::Archive l;
  if (!detail::OpenPac(g, f, l))
    return;
  for (auto &p : out) {
    if (p.kind != "EVZ" || p.number != number)
      continue;
    std::vector<svrfmt::PachEntry> leaves, more;
    if (!svrfmt::PachRead(p.data, leaves))
      continue;
    for (auto &k : l.root.kids) {
      const Bytes z = detail::AsPach(k);
      if (!z.empty())
        svrfmt::PachRead(z, more);
    }
    bool added = false;
    for (auto &m : more)
      if (std::none_of(leaves.begin(), leaves.end(), [&](const auto &e) { return e.id == m.id; })) {
        leaves.push_back({m.id, svrfmt::Unpack(m.data)});
        added = true;
      }
    if (added) {
      std::sort(leaves.begin(), leaves.end(), [](const auto &x, const auto &y) { return x.id < y.id; });
      p.data = svrfmt::PachWrite(leaves);
    }
  }
}

// Whether an entrance's base script is a "one-take" (2K15: ~61 put the whole entrance in segment 0, one slot per object;
// WWE '12 played 2K15 Sami Zayn's 478 without the crowd: Kaiko's note, take 2K16's re-cut).
inline bool OneTake(const std::vector<Part> &parts, int number) {
  for (const auto &p : parts)
    if (p.kind == "EVP" && p.number == number) {
      const uint32_t n = detail::Be32(p.data, 0x40) & 0xFFFF, t = detail::Be32(p.data, 0x48) + 8;
      for (uint32_t i = 0; i < n; ++i)
        if ((detail::Be32(p.data, t + 32 * size_t(i) + 24) >> 16) > 1)
          return false;
      return n > 0;
    }
  return false;
}

// 2K16 / 2K17: the entrance (a pac\evt\00NNN.pac) whose script's name (+0x22, "0105_AJ_Styles") is the superstar's
// (letters and digits compared, case ignored; a script named exactly so first, else one starting so), 0 if none.
// 2K17's profiles can't be read here (an HSPC chEtc): its AJ Styles is number 610 but enters with 105 (610 is a
// generic champion's entrance, 2026-11-02).
inline int EntranceByName(const disc::GameDisc &g, const std::string &superstar) {
  auto key = [](const std::string &s) {
    std::string k;
    for (unsigned char c : s)
      if (isalnum(c))
        k += char(tolower(c));
    return k;
  };
  const std::string want = key(superstar);
  if (want.empty())
    return 0;
  int starts = 0;
  for (const auto &f : g.List("pac/evt")) {
    if (f.size() != 9 || f.compare(0, 2, "00") != 0 || _stricmp(f.c_str() + 5, ".pac") != 0)
      continue;
    const int n = atoi(f.c_str());
    svrfmt::Archive a;
    if (n <= 0 || n >= 1000 || !detail::OpenPac(g, "pac/evt/" + f, a))
      continue;
    for (auto &k : a.root.kids) {
      const Bytes u = svrfmt::Unpack(svrfmt::ArchiveData(k));
      if (u.size() < 0x40 || memcmp(u.data(), "0FOP", 4))
        continue;
      std::string name(reinterpret_cast<const char *>(&u[0x22]), 30);
      name = name.c_str();
      if (const size_t us = name.find('_'); us != std::string::npos)
        name = name.substr(us + 1);  // ("0105_")
      const std::string k2 = key(name);
      if (k2 == want)
        return n;
      if (!starts && k2.rfind(want, 0) == 0)
        starts = n;
    }
  }
  return starts;
}

// A game's effects (pac\prtcls.pac PCE6/NNNN, a PACH) of entrance `number`, empty if none.
inline Bytes Pce6Of(const disc::GameDisc &g, int number) {
  svrfmt::Archive a;
  if (!detail::OpenPac(g, "pac/prtcls.pac", a))
    return {};
  for (auto &k : a.root.kids)
    if (k.group.rfind("PCE6", 0) == 0 && atoi(k.name.c_str()) == number)
      return svrfmt::Unpack(svrfmt::ArchiveData(k));
  return {};
}

// An entrance's parts numbered `to` (base number) instead of `from`: when WWE '12 has an entrance of that number already
// (2K16 Kevin Owens 458, 2K17 AJ Styles 610: WWE '12's own), the added one gets a free number. Its variants follow
// (1xNNN), the scripts' own number (+0x20) and their tail's (u16 at the tail).
inline void Renumber(std::vector<Part> &parts, int from, int to) {
  using yanm::ik::detail::Put16;
  for (auto &p : parts) {
    if (p.number % 1000 != from % 1000)
      continue;
    p.number = p.number / 1000 * 1000 + to % 1000;
    if (p.kind == "EVP" && p.data.size() >= 0x50) {
      Put16(p.data, 0x20, uint16_t(p.number));
      if (const uint32_t t = detail::Be32(p.data, 0x1C); t && 8 + size_t(t) + 2 <= p.data.size())
        Put16(p.data, 8 + t, uint16_t(p.number));
    }
  }
}

// The mod's file of a part (superstar/entrance/EVP_246.bin, GMEV_246.bin...): the game serves it as
// /EVP<group>/<number>, /GMEV/<number, 4 digits>...
inline std::string ModPath(const Part &p) {
  if (p.kind == "SOBJ")  // (its objects: served in evtobj.pac with the entrance or the victory scene)
    return "superstar/objects/SOBJ_" + std::to_string(p.number) + ".bin";
  return "superstar/entrance/" + p.kind + "_" + std::to_string(p.number) + ".bin";
}

// The WWE '13 entrance `number`'s parts (its own and its variants'), from the player's WWE '13 (w13).
inline bool Entrance(const disc::GameDisc &w13, int number, std::vector<Part> &out, std::string *why) {
  out.clear();
  // WWE '13, 2K14, 2K15: pac\evt\Entrance00..10.pac (and their DLC's)
  std::vector<std::string> files;
  for (int i = 0; i <= 10; ++i) {
    char f[32];
    snprintf(f, sizeof(f), "pac/evt/Entrance%02d.pac", i);
    files.push_back(f);
  }
  for (const auto &f : w13.List("pac/evt"))
    if (f.size() > 12 && _strnicmp(f.c_str(), "Entrance_dlc", 12) == 0)
      files.push_back("pac/evt/" + f);
  for (const auto &f : files) {
    svrfmt::Archive a;
    if (!detail::OpenPac(w13, f, a))
      continue;
    for (auto &k : a.root.kids) {
      const int n = atoi(k.name.c_str());
      if (n % 1000 != number % 1000 || (n != number && n / 1000 < 10))
        continue;
      if (k.group.rfind("EVP", 0) != 0 && k.group.rfind("EVZ", 0) != 0)
        continue;
      out.push_back({n, k.group.substr(0, 3), svrfmt::Unpack(svrfmt::ArchiveData(k))});
    }
  }
  // 2K16 / 2K17: one pac per scene, and its variants 1xNNN
  if (out.empty())
    for (int v : {number, 10000 + number % 1000, 11000 + number % 1000, 12000 + number % 1000, 13000 + number % 1000,
                  14000 + number % 1000})
      ScenePac(w13, v, out);
  for (auto &p : out)
    To12(p);
  FillTracks(out);
  // its game event script and its effects (2K16 / 2K17: pac\gimmick\gmevNNNN.pac, pac\particle\pce6NNNN.pac)
  if (!out.empty()) {
    char name[8];
    snprintf(name, sizeof(name), "%04d", number % 10000);
    const std::tuple<const char *, const char *, const char *> kinds[] = {{"pac/gm.pac", "pac/gimmick/gmev", "GMEV"},
                                                                          {"pac/prtcls.pac", "pac/particle/pce6", "PCE6"}};
    for (const auto &[file, own, group] : kinds) {
      bool found = false;
      for (const std::string &f : {std::string(file), std::string(own) + name + ".pac"}) {
        svrfmt::Archive a;
        if (found || !detail::OpenPac(w13, f, a))
          continue;
        for (auto &k : a.root.kids) {
          std::string g = k.group, nm = k.name;
          while (!g.empty() && (g.back() == ' ' || g.back() == '\0'))
            g.pop_back();
          while (!nm.empty() && (nm.back() == ' ' || nm.back() == '\0'))
            nm.pop_back();
          if (g == group && nm == name && !found) {
            out.push_back({number, group, svrfmt::Unpack(svrfmt::ArchiveData(k))});
            found = true;
          }
        }
      }
      // (no game event script of its own: WWE '12 then never showed the superstar; Cena's, generic, under its number:
      // 2K15 Sami Zayn's entrance 478 has none)
      if (!found && std::string(group) == "GMEV")
        for (const std::string &f : {std::string(file), std::string(own) + "0139.pac"}) {
          svrfmt::Archive a;
          if (found || !detail::OpenPac(w13, f, a))
            continue;
          for (auto &k : a.root.kids)
            if (!found && k.group.rfind("GMEV", 0) == 0 && atoi(k.name.c_str()) == 139) {
              out.push_back({number, group, svrfmt::Unpack(svrfmt::ArchiveData(k))});
              found = true;
            }
        }
    }
  }
  if (out.empty() && why)
    *why = "no such entrance in its game";
  return !out.empty();
}

// The entrance / victory objects (props: Cena's chain 2718, Kane's mask 2713...) WWE '12 hasn't: the scripts' object
// tables (EVP: u16 count at +0x42, table at +0x48 + 8, 32-byte objects, model at +4) name models; those under 10000
// are pac\evt\evtobj.pac's SOBJ/<model> (PACH {0 the JBOY model, 10 its pictures}; WWE '13's add a 48-byte child
// 200), the same format in WWE '12, '13 and 2K14. Those of `parts` that the player's WWE '12 (w12) has no SOBJ of,
// taken from the game `src`'s evtobj.pac (then its DLC's evtobj_dlcNN.pac), as parts "SOBJ" numbered by model.
inline std::vector<Part> Objects(const disc::GameDisc &src, const disc::GameDisc &w12, const std::vector<Part> &parts) {
  auto be = [](const Bytes &b, size_t o) {
    return o + 4 <= b.size() ? uint32_t(b[o]) << 24 | uint32_t(b[o + 1]) << 16 | uint32_t(b[o + 2]) << 8 | b[o + 3] : 0u;
  };
  std::set<uint32_t> want;
  for (const auto &p : parts)
    if (p.kind == "EVP" && p.data.size() >= 0x50 && !memcmp(p.data.data(), "0FOP", 4)) {
      const uint32_t n = be(p.data, 0x40) & 0xFFFF, t = be(p.data, 0x48) + 8;
      for (uint32_t i = 0; i < n; ++i)
        if (const uint32_t m = be(p.data, t + 32 * size_t(i) + 4); m >= 1000 && m < 10000)
          want.insert(m);
    }
  auto open = [](const disc::GameDisc &g, const std::string &f, svrfmt::Archive &a) {
    Bytes b;
    if (!g.Read(f, b))
      return false;
    if (svrfmt::IsXcompress(b)) {
      Bytes o;
      if (!svrfmt::XcompressDecode(b, o))
        return false;
      b.swap(o);
    }
    return svrfmt::ArchiveOpen(b, a, nullptr);
  };
  auto trimmed = [](std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\0'))
      s.pop_back();
    return s;
  };
  std::vector<Part> out;
  svrfmt::Archive a12;
  if (want.empty() || !open(w12, "pac/evt/evtobj.pac", a12))
    return out;
  for (const auto &k : a12.root.kids)
    if (k.group.rfind("SOBJ", 0) == 0)
      want.erase(uint32_t(atoi(trimmed(k.name).c_str())));
  std::vector<std::string> files = {"pac/evt/evtobj.pac"};
  for (const auto &f : src.List("pac/evt"))
    if (f.rfind("evtobj_dlc", 0) == 0)
      files.push_back("pac/evt/" + f);
  for (uint32_t m : want) {  // (2K16 / 2K17: one pac per prop, pac\prop\NNNN.pac; its SOBJ only, not BOBJ)
    char f[32];
    snprintf(f, sizeof(f), "pac/prop/%04u.pac", m);
    files.push_back(f);
  }
  for (const auto &f : files) {
    svrfmt::Archive a;
    if (want.empty() || !open(src, f, a))
      continue;
    for (auto &k : a.root.kids) {
      const uint32_t m = uint32_t(atoi(trimmed(k.name).c_str()));
      if (k.group.rfind("SOBJ", 0) == 0 && want.count(m)) {
        out.push_back({int(m), "SOBJ", svrfmt::Unpack(svrfmt::ArchiveData(k))});
        want.erase(m);
      }
    }
  }
  return out;
}

// The props of `parts`' scripts that WWE '12 can't load (no SOBJ in its evtobj.pac, none brought by the mod: 2K17
// AJ Styles' 159, a 2K weapon "WOBJ", and the game crashed in his entrance, 2026-11-02): an existing prop (2001) in
// their place, every slot off (slot word 0, bits 8-15). Props: object type +8 = 0x100. Returns how many.
inline int HideMissing(std::vector<Part> &parts, const disc::GameDisc &w12, const std::vector<Part> &brought) {
  using detail::Be32;
  using yanm::ik::detail::Put32;
  std::set<uint32_t> have;
  svrfmt::Archive a;
  if (!detail::OpenPac(w12, "pac/evt/evtobj.pac", a))
    return 0;
  for (const auto &k : a.root.kids)
    if (k.group.rfind("SOBJ", 0) == 0)
      have.insert(uint32_t(atoi(k.name.c_str())));
  for (const auto &b : brought)
    have.insert(uint32_t(b.number));
  int hidden = 0;
  for (auto &p : parts) {
    if (p.kind != "EVP" || p.data.size() < 0x50)
      continue;
    const uint32_t n = Be32(p.data, 0x40) & 0xFFFF, t = Be32(p.data, 0x48) + 8;
    for (uint32_t i = 0; i < n; ++i) {
      const size_t q = t + 32 * size_t(i);
      const uint32_t model = Be32(p.data, q + 4);
      if (Be32(p.data, q + 8) != 0x100 || have.count(model) || model >= 10000)
        continue;
      Put32(p.data, q + 4, 2001);
      const uint32_t ns = Be32(p.data, q + 24) >> 16, bp = Be32(p.data, q + 28) + 8;
      for (uint32_t k = 0; k < ns && bp + 212 * size_t(k) + 4 <= p.data.size(); ++k)
        p.data[bp + 212 * size_t(k) + 2] = 0;
      ++hidden;
    }
  }
  return hidden;
}

// Object 5009 (a prop with a skeleton, type 0x100, track object 25007): WWE '12 and 2K15 only pose it (a 2-track chunk
// per block); 2K16 / 2K17 move every bone of their own skeleton (2K16 Kevin Owens, 2K17 AJ Styles, 2026-11-02). In their
// place, WWE '12's own record of it with the same blocks (from its entrances / victory scenes).
// Returns how many were replaced.
inline int Announcer12(std::vector<Part> &parts, const disc::GameDisc &w12) {
  using detail::Be32;
  constexpr uint32_t kAnnouncer = 5009;
  // a record's blocks (their names), and whether one of its chunks moves more than 2 tracks
  auto blocks = [](const Bytes &l, bool *moves) {
    std::string names;
    if (l.size() < 0x70)
      return names;
    const uint32_t first = Be32(l, 0x0C), count = Be32(l, 0x48);
    for (uint32_t i = 0; i < count && 0x70 + 0x28 * size_t(i + 1) <= l.size(); ++i) {
      const size_t ent = 0x70 + 0x28 * size_t(i), y = size_t(first) + Be32(l, ent + 0x24);
      names.append(reinterpret_cast<const char *>(&l[ent + 4]), 8);
      if (moves && y + 12 <= l.size() && !memcmp(&l[y], "YANM", 4) && ((l[y + 10] << 8) | l[y + 11]) > 2)
        *moves = true;
    }
    return names;
  };
  // the announcer's object index in a script, -1 if none
  auto index = [](const Bytes &evp) {
    if (evp.size() < 0x50)
      return -1;
    const uint32_t n = Be32(evp, 0x40) & 0xFFFF, t = Be32(evp, 0x48) + 8;
    for (uint32_t i = 0; i < n && t + 32 * size_t(i + 1) <= evp.size(); ++i)
      if (Be32(evp, t + 32 * size_t(i) + 4) == kAnnouncer)
        return int(i);
    return -1;
  };
  std::vector<std::pair<std::string, Bytes>> own;  // WWE '12's records of him, by blocks (read once, when needed)
  bool read = false;
  auto load = [&] {
    read = true;
    for (const char *f : {"Nyujyo0", "Nyujyo1", "Winning0", "Winning1"}) {
      svrfmt::Archive a;
      if (!detail::OpenPac(w12, std::string("pac/evt/") + f + ".pac", a))
        continue;
      for (auto &k : a.root.kids) {
        if (k.group.rfind("EVP", 0) != 0)
          continue;
        const int i = index(svrfmt::Unpack(svrfmt::ArchiveData(k)));
        if (i < 0)
          continue;
        for (auto &z : a.root.kids) {
          std::vector<svrfmt::PachEntry> leaves;
          if (z.group.rfind("EVZ", 0) != 0 || z.name != k.name ||
              !svrfmt::PachRead(svrfmt::Unpack(svrfmt::ArchiveData(z)), leaves))
            continue;
          for (auto &e : leaves)
            if (e.id == uint32_t(i)) {
              Bytes l = svrfmt::Unpack(e.data);
              const std::string b = blocks(l, nullptr);
              if (!b.empty() && std::none_of(own.begin(), own.end(), [&](const auto &o) { return o.first == b; }))
                own.push_back({b, std::move(l)});
            }
        }
      }
    }
  };
  int replaced = 0;
  for (const auto &evp : parts) {
    const int i = evp.kind == "EVP" ? index(evp.data) : -1;
    if (i < 0)
      continue;
    for (auto &evz : parts) {
      std::vector<svrfmt::PachEntry> leaves;
      if (evz.kind != "EVZ" || evz.number != evp.number || !svrfmt::PachRead(evz.data, leaves))
        continue;
      bool changed = false;
      for (auto &e : leaves) {
        bool moves = false;
        const std::string b = e.id == uint32_t(i) ? blocks(svrfmt::Unpack(e.data), &moves) : std::string();
        if (!moves)
          continue;
        if (!read)
          load();
        for (const auto &o : own)
          if (o.first == b) {
            e.data = o.second;
            changed = true;
            ++replaced;
            break;
          }
      }
      if (changed)
        evz.data = svrfmt::PachWrite(leaves);
    }
  }
  return replaced;
}

// The superstars waiting in the ring (objects of type 1, models 1 and 2): WWE '12 and 2K15 place them with their root
// track (0c5879e5, its base the spot: -76 / -285); 2K16 / 2K17 move another node (5b391f2a / 5b391f26) at 0, which
// WWE '12 hasn't, and the one waiting stood at the origin under the mat (2K16 Kevin Owens / 2K17 AJ Styles,
// 2026-11-02). In each of their blocks, WWE '12's own chunk of the waiting superstar of the same model and rank (the
// k-th model-1 object takes WWE '12's k-th, from its entrances), and in their shown slots its spot (slot words 3-50:
// the track's base and the slot's spot go together; with 2K16's spot he still stood under the mat).
// The people around the ring (type 0x01000000, 2K16's ring announcer) have no spot in 2K16 / 2K17's slots (their game
// places them) and stood at the origin: not shown, as in WWE '12's entrances. Returns how many objects were changed.
inline int Waiting12(std::vector<Part> &parts, const disc::GameDisc &w12) {
  using detail::Be32;
  using yanm::ik::detail::Put32;
  constexpr uint32_t kRoot = 0x0C5879E5;
  // the waiting objects of a script: (index, model) in order
  auto waiting = [](const Bytes &evp) {
    std::vector<std::pair<uint32_t, uint32_t>> out;
    if (evp.size() < 0x50)
      return out;
    const uint32_t n = Be32(evp, 0x40) & 0xFFFF, t = Be32(evp, 0x48) + 8;
    for (uint32_t i = 0; i < n && t + 32 * size_t(i + 1) <= evp.size(); ++i) {
      const uint32_t model = Be32(evp, t + 32 * size_t(i) + 4);
      if (Be32(evp, t + 32 * size_t(i) + 8) == 1 && (model == 1 || model == 2))
        out.push_back({i, model});
    }
    return out;
  };
  // a record's block: its YANM and what follows it (its event tags, read by sub_8246E1D0: left at 0, the game crashed
  // on them, 2026-11-02), NN x 32 bytes (NN the low 24 bits of the block's entry), empty if none
  auto chunk = [](const Bytes &l, uint32_t block) {
    if (l.size() < 0x70 || block >= Be32(l, 0x48))
      return Bytes();
    const size_t ent = 0x70 + 0x28 * size_t(block), y = size_t(Be32(l, 0x0C)) + Be32(l, ent + 0x24);
    const size_t end = std::min(l.size(), y + 32 * size_t(Be32(l, ent) & 0xFFFFFF));
    if (y + 8 > l.size() || memcmp(&l[y], "YANM", 4) || y + Be32(l, y + 4) > end)
      return Bytes();
    return Bytes(l.begin() + long(y), l.begin() + long(end));
  };
  // the first shown slot of an object (its offset), 0 if none
  auto shown = [](const Bytes &evp, uint32_t i) {
    const size_t q = Be32(evp, 0x48) + 8 + 32 * size_t(i);
    const uint32_t ns = Be32(evp, q + 24) >> 16, bp = Be32(evp, q + 28) + 8;
    for (uint32_t k = 0; k < ns && bp + 212 * size_t(k + 1) <= evp.size(); ++k)
      if (evp[bp + 212 * size_t(k) + 2] & 1)
        return size_t(bp + 212 * size_t(k));
    return size_t(0);
  };
  // WWE '12's waiting chunks and spots (slot words 3-50), by model, in order (read once, when needed)
  struct Own {
    Bytes chunk, spot;
  };
  std::vector<Own> own[3];
  bool read = false;
  auto load = [&] {
    read = true;
    svrfmt::Archive a;
    if (!detail::OpenPac(w12, "pac/evt/Nyujyo0.pac", a))
      return;
    for (auto &k : a.root.kids) {
      if (k.group.rfind("EVP", 0) != 0)
        continue;
      const Bytes script = svrfmt::Unpack(svrfmt::ArchiveData(k));
      const auto objs = waiting(script);
      if (objs.empty())
        continue;
      for (auto &z : a.root.kids) {
        std::vector<svrfmt::PachEntry> leaves;
        if (z.group.rfind("EVZ", 0) != 0 || z.name != k.name ||
            !svrfmt::PachRead(svrfmt::Unpack(svrfmt::ArchiveData(z)), leaves))
          continue;
        for (const auto &[i, model] : objs)
          for (auto &e : leaves)
            if (e.id == i) {
              Bytes y = chunk(svrfmt::Unpack(e.data), 0);
              const size_t s = shown(script, i);
              if (y.size() >= 0x18 && Be32(y, 0x14) == kRoot && s)
                own[model].push_back({std::move(y), Bytes(script.begin() + long(s + 12), script.begin() + long(s + 204))});
            }
      }
      if (!own[1].empty() && !own[2].empty())
        return;
      own[1].clear(), own[2].clear();
    }
  };
  int rebuilt = 0;
  for (auto &evp : parts) {
    if (evp.kind != "EVP" || evp.data.size() < 0x50)
      continue;
    // the people around the ring with no spot in any slot: not shown (object 0, the superstar, has none either: kept)
    const uint32_t n = Be32(evp.data, 0x40) & 0xFFFF, t = Be32(evp.data, 0x48) + 8;
    for (uint32_t i = 1; i < n && t + 32 * size_t(i + 1) <= evp.data.size(); ++i) {
      const size_t q = t + 32 * size_t(i);
      const uint32_t ns = Be32(evp.data, q + 24) >> 16, bp = Be32(evp.data, q + 28) + 8;
      if (Be32(evp.data, q + 8) != 0x01000000 || !shown(evp.data, i))
        continue;
      bool spot = false;
      for (uint32_t k = 0; k < ns && bp + 212 * size_t(k + 1) <= evp.data.size(); ++k)
        for (int w = 3; w <= 7; ++w)
          spot = spot || Be32(evp.data, bp + 212 * size_t(k) + 4 * size_t(w));
      if (spot)
        continue;
      for (uint32_t k = 0; k < ns && bp + 212 * size_t(k + 1) <= evp.data.size(); ++k)
        evp.data[bp + 212 * size_t(k) + 2] = 0;
      ++rebuilt;
    }
    const auto objs = waiting(evp.data);
    if (objs.empty())
      continue;
    for (auto &evz : parts) {
      std::vector<svrfmt::PachEntry> leaves;
      if (evz.kind != "EVZ" || evz.number != evp.number || !svrfmt::PachRead(evz.data, leaves))
        continue;
      bool changed = false;
      size_t rank[3] = {};
      for (const auto &[i, model] : objs) {
        const size_t k = rank[model]++;
        for (auto &e : leaves) {
          if (e.id != i)
            continue;
          Bytes l = svrfmt::Unpack(e.data);
          const Bytes y0 = chunk(l, 0);
          if (y0.size() < 0x18 || Be32(y0, 0x14) == kRoot)
            continue;
          if (!read)
            load();
          if (own[model].empty())
            continue;
          const Bytes &with = own[model][k % own[model].size()].chunk;
          const Bytes &spot = own[model][k % own[model].size()].spot;
          {
            const size_t q = t + 32 * size_t(i);
            const uint32_t ns = Be32(evp.data, q + 24) >> 16, bp = Be32(evp.data, q + 28) + 8;
            for (uint32_t s = 0; s < ns && bp + 212 * size_t(s + 1) <= evp.data.size(); ++s)
              if (evp.data[bp + 212 * size_t(s) + 2] & 1)
                std::copy(spot.begin(), spot.end(), evp.data.begin() + long(bp + 212 * size_t(s) + 12));
          }
          // the record again: its header and block table, then the chunk once per block (each block NN x 32 bytes,
          // NN at the entry's +0 low byte, its offset from the first at +0x24; the size from the first at +4)
          const uint32_t first = Be32(l, 0x0C), count = Be32(l, 0x48);
          const uint32_t nn = uint32_t((with.size() + 31) / 32);
          Bytes out(l.begin(), l.begin() + long(first));
          for (uint32_t b = 0; b < count; ++b) {
            const size_t ent = 0x70 + 0x28 * size_t(b);
            out[ent + 3] = uint8_t(nn);
            Put32(out, ent + 0x24, b * nn * 32);
            Bytes block(size_t(nn) * 32, 0);
            std::copy(with.begin(), with.end(), block.begin());
            out.insert(out.end(), block.begin(), block.end());
          }
          Put32(out, 4, count * nn * 32);
          e.data = std::move(out);
          changed = true;
          ++rebuilt;
        }
      }
      if (changed)
        evz.data = svrfmt::PachWrite(leaves);
    }
  }
  return rebuilt;
}

// WWE '12 and 2K15 scripts have the superstars waiting in the ring (type 1) from object 12 on, models 1, 2, 1, 2...;
// 2K16 / 2K17 have a prop at 12 and them after it (1, 1, 2, 2), and the one waiting and the ring announcer (placed by
// the game) stood under the mat in the middle of the ring (2K16 Kevin Owens, 2K17 AJ Styles; 2K15 Seth Rollins right,
// 2026-11-02). Objects 12 on put in WWE '12's order: in the script (each object's +0 its index, each slot's +4
// 25000 + it) and in the track bank (leaf ids, the record's name "250NN" at +0x4C). Returns how many scripts.
inline int WaitingFirst12(std::vector<Part> &parts) {
  using detail::Be32;
  using yanm::ik::detail::Put32;
  constexpr uint32_t kFrom = 12;
  int changed = 0;
  for (auto &evp : parts) {
    if (evp.kind != "EVP" || evp.data.size() < 0x50)
      continue;
    const uint32_t n = Be32(evp.data, 0x40) & 0xFFFF, t = Be32(evp.data, 0x48) + 8;
    if (n <= kFrom || t + 32 * size_t(n) > evp.data.size())
      continue;
    std::vector<uint32_t> ones, twos, rest;
    for (uint32_t i = kFrom; i < n; ++i) {
      const uint32_t model = Be32(evp.data, t + 32 * size_t(i) + 4), type = Be32(evp.data, t + 32 * size_t(i) + 8);
      (type == 1 && model == 1 ? ones : type == 1 && model == 2 ? twos : rest).push_back(i);
    }
    std::vector<uint32_t> order;  // order[k]: the object that goes to kFrom + k
    for (size_t a = 0, b = 0; a < ones.size() || b < twos.size();) {
      if (a < ones.size())
        order.push_back(ones[a++]);
      if (b < twos.size())
        order.push_back(twos[b++]);
    }
    order.insert(order.end(), rest.begin(), rest.end());
    bool same = true;
    for (size_t k = 0; k < order.size(); ++k)
      same = same && order[k] == kFrom + k;
    if (same)
      continue;
    std::vector<uint32_t> to(n);  // to[old]: its new index
    for (uint32_t i = 0; i < n; ++i)
      to[i] = i;
    for (size_t k = 0; k < order.size(); ++k)
      to[order[k]] = uint32_t(kFrom + k);
    const Bytes old = evp.data;
    for (uint32_t i = kFrom; i < n; ++i) {
      const size_t from = t + 32 * size_t(i), q = t + 32 * size_t(to[i]);
      std::copy(old.begin() + long(from), old.begin() + long(from + 32), evp.data.begin() + long(q));
      Put32(evp.data, q, to[i]);
      const uint32_t ns = Be32(evp.data, q + 24) >> 16, bp = Be32(evp.data, q + 28) + 8;
      for (uint32_t s = 0; s < ns && bp + 212 * size_t(s + 1) <= evp.data.size(); ++s)
        Put32(evp.data, bp + 212 * size_t(s) + 4, 25000 + to[i]);
    }
    for (auto &evz : parts) {
      std::vector<svrfmt::PachEntry> leaves;
      if (evz.kind != "EVZ" || evz.number != evp.number || !svrfmt::PachRead(evz.data, leaves))
        continue;
      for (auto &e : leaves) {
        if (e.id < kFrom || e.id >= n || to[e.id] == e.id)
          continue;
        e.id = to[e.id];
        Bytes l = svrfmt::Unpack(e.data);
        if (l.size() >= 0x51 && l[0x4C] == '2') {
          char name[8];
          snprintf(name, sizeof(name), "%05u", 25000 + e.id);
          memcpy(&l[0x4C], name, 5);
        }
        e.data = std::move(l);
      }
      std::sort(leaves.begin(), leaves.end(), [](const auto &x, const auto &y) { return x.id < y.id; });
      evz.data = svrfmt::PachWrite(leaves);
    }
    ++changed;
  }
  return changed;
}

// A superstar's victory scene in WWE '13 (pac\evt\Winning00-03.pac, xcompressed EPK8, groups "EVP " / "EVZ ", entries
// 6000 + its number: Mankind 6246), its tracks' IK legs baked as an entrance's. WWE '12 looks for 6000 + the superstar's
// own number (sub_82407DC8: participant +16 + 6000, every group letter tried by sub_828CCC10 "/%s%c/%d"), so an added
// superstar's is served under 6000 + its slot (wwe12_arena_pages.cpp). Parts "EVP" and "EVZ", number 6000 + `number`.
inline bool Victory(const disc::GameDisc &w13, int number, std::vector<Part> &out) {
  out.clear();
  const int want = 6000 + number % 1000;
  for (int i = 0; i <= 9; ++i) {
    char f[32];
    snprintf(f, sizeof(f), "pac/evt/Winning%02d.pac", i);
    svrfmt::Archive a;
    if (!detail::OpenPac(w13, f, a))
      continue;
    for (auto &k : a.root.kids)
      if (atoi(k.name.c_str()) == want && (k.group.rfind("EVP", 0) == 0 || k.group.rfind("EVZ", 0) == 0))
        out.push_back({want, k.group.substr(0, 3), svrfmt::Unpack(svrfmt::ArchiveData(k))});
  }
  if (out.empty())  // (2K16 / 2K17: pac\evt\06NNN.pac)
    ScenePac(w13, want, out);
  for (auto &p : out)
    To12(p);
  FillTracks(out);
  return out.size() == 2;
}

}  // namespace entrance13
