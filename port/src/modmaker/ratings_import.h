// Mod Maker: a ported superstar's ratings, size, gender and abilities read from its own game's record (NEW SUPERSTAR,
// "the mods should come with their size and ratings", the user 2026-11-05; in 1.1 as an experiment, 2026-11-06).
//
// Its record: pac\ch\chEtc.pac CHAR / DAT (2K16's by index, +4 its number), else pac\misc.pac (SvR 2007 / 2008).
//  - WWE '13, 2K14, 2K15, 2K16: 14 attributes, WWE '12's 12 with two more at 5 and 11 (seen on Cena, Orton, Rey, Big
//    Show, Kane, Undertaker, Mark Henry), at +4 (WWE '13), +36 (2K14, 2K15), +40 (2K16); the size scale (4096 = 1.0,
//    Cena 3988 in all) at +0x34, +22, +26.
//  - SvR 2009, 2010, 2011: each WWE '12 value a + b x one byte of the record, fitted on the superstars both games have by
//    name (the study tool "fit", 2026-11-05: SvR 2011 185 of them, mean error 1.1-1.8 points; SvR 2010 161, 1.7-2.6;
//    SvR 2009 94, 2.2-3.3; sizes 2.6-3 / 31 for 2009). SvR 2007 / 2008 rate otherwise (errors 10-12: the base's); 2007's
//    size fits (u16 +22), 2008's doesn't.
//  - Gender: the bit that matched every shared superstar (SvR 2009-2011 +212, 2008 +206, WWE '13 +256, 2K14 / 2K15
//    +27, 2K16 +31; SvR 2007: its weight class +147 is 0 for a diva, Kaiko's superstar_port.py).
//  - Abilities: WWE '13's +274..+278 (the same set as WWE '12's for 23 of 42); the other games number theirs otherwise.
//  - 2K17's records aren't read.
// Guards: ratings only if all 12 are within 20-100 and not all alike, a size only within 3200-5000 (0.78-1.22 x Cena),
// else the base's; the player can change them all on the page.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "game_disc.h"
#include "svrfmt/archive.h"
#include "svrfmt/pac.h"
#include "theme_source.h"

namespace ratings_import {

using Bytes = std::vector<uint8_t>;

struct Result {
  bool found = false;           // its record was read
  std::vector<int> attributes;  // WWE '12's 12, empty: the base's
  int size = 0;                 // record +0x28 (4096 = 1.0), 0: the base's
  int gender = -1;              // 0 man, 1 woman, -1: unknown (the base's)
  std::vector<int> abilities;   // WWE '12 ability numbers (WWE '13 only), empty: the base's
};

namespace detail {

struct Lin {
  int at;
  double a, b;
};
struct Fitted {
  uint32_t game;
  Lin r[12];                  // at < 0: none
  Lin size;                   // u16 little-endian; at < 0: none
  int gender_at, gender_bit;  // gender_bit < 0: the byte is 0 for a woman
};
inline const Fitted *FittedOf(uint32_t game) {
  static const Fitted kTable[] = {
      {0x5451085D,
       {{4, 0.02, 1.008}, {4, -0.06, 1.002}, {5, -0.01, 1.015}, {8, 0.03, 1.010}, {7, 0.10, 1.016}, {6, 0.16, 0.962},
        {6, 0.18, 0.948}, {6, 0.20, 1.024}, {5, -0.02, 1.083}, {4, -0.04, 0.965}, {9, 0.03, 1.038}, {5, 0.03, 1.086}},
       {32, -17.35, 1.004}, 212, 0},
      {0x54510844,
       {{10, 0.09, 0.934}, {4, 0.22, 0.931}, {10, 0.20, 0.919}, {5, 0.11, 0.979}, {7, 0.05, 0.965}, {6, -0.03, 0.929},
        {6, -0.02, 0.919}, {6, -0.01, 0.991}, {7, 0.08, 0.970}, {10, 0.09, 0.896}, {9, 0.24, 1.003}, {10, 0.29, 0.978}},
       {32, -25.96, 1.006}, 212, 0},
      {0x54510826,
       {{5, 0.08, 0.971}, {4, 0.38, 0.923}, {5, 0.12, 0.941}, {9, 0.08, 0.902}, {7, -0.06, 0.960}, {6, -0.16, 0.899},
        {6, -0.11, 0.881}, {6, -0.23, 0.968}, {10, -0.07, 0.958}, {5, 0.02, 0.931}, {9, -0.06, 1.009}, {10, -0.06, 0.989}},
       {32, 499.11, 0.874}, 212, 0},
      {0x5451080B, {{-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}}, {-1}, 206, 0},
      {0x545107E0, {{-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}, {-1}}, {22, -113.96, 1.025}, 147, -1},
  };
  for (const auto &t : kTable)
    if (t.game == game)
      return &t;
  return nullptr;
}

// The record of superstar `number`: chEtc.pac CHAR / DAT, else misc.pac (SvR 2007 / 2008), found by the number it
// holds (strings2k's places: 264 / 252 bytes +36, 324 +56, 232 +30, 176 +26, the 2K games' +4).
inline Bytes RecordOf(const disc::GameDisc &src, int number) {
  auto all = src.ReadAll("pac/ch/chEtc.pac");
  if (all.empty())
    all = src.ReadAll("pac/misc.pac");
  Bytes found;
  for (auto &etc : all) {
    svrfmt::Archive a;
    if (!svrfmt::ArchiveOpen(etc, a, nullptr))
      continue;
    const auto take = [&](const Bytes &d) {
      const size_t id_at = d.size() == 264 || d.size() == 252 ? 36 : d.size() == 324 ? 56 : d.size() == 232 ? 30
                           : d.size() == 176 ? 26 : d.size() >= 200 && d.size() <= 240 ? 4 : 0;
      if (id_at && d.size() > id_at + 1 && (d[id_at] | d[id_at + 1] << 8) == number)
        found = d;
    };
    const auto walk = [&](auto &self, svrfmt::ArcNode &n, int depth, bool dat) -> void {
      if (depth && dat) {
        take(svrfmt::Unpack(svrfmt::ArchiveData(n)));
        return;
      }
      if (depth < 3 && svrfmt::ArchiveExpand(n))
        for (auto &c : n.kids) {
          std::string nm = c.name;
          while (!nm.empty() && (nm.back() == ' ' || nm.back() == '\0'))
            nm.pop_back();
          self(self, c, depth + 1, dat || (c.group == "CHAR" && nm == "DAT"));
        }
    };
    walk(walk, a.root, 0, false);
    if (found.empty())  // (the 2K games: a PACH the archive doesn't open)
      for (auto &k : a.root.kids) {
        std::string nm = k.name;
        while (!nm.empty() && (nm.back() == ' ' || nm.back() == '\0'))
          nm.pop_back();
        std::vector<svrfmt::PachEntry> kids;
        if (k.group == "CHAR" && nm == "DAT" && svrfmt::PachRead(svrfmt::Unpack(svrfmt::ArchiveData(k)), kids))
          for (auto &e : kids)
            take(svrfmt::Unpack(e.data));
      }
  }
  return found;
}

// The ratings kept only if all are believable (20-100) and not all alike (a placeholder record).
inline std::vector<int> Checked(const std::vector<double> &v) {
  std::vector<int> out;
  for (double x : v) {
    if (!(x >= 19.5 && x <= 100.5))
      return {};
    out.push_back(std::clamp(int(std::lround(x)), 20, 100));
  }
  if (out.size() != 12 || std::all_of(out.begin(), out.end(), [&](int x) { return x == out[0]; }))
    return {};
  return out;
}

}  // namespace detail

// Superstar `number` of `game` (title id; its disc opened as `src`). known_abilities: the WWE '12 ability numbers the
// page offers (others are left out).
inline Result Read(const disc::GameDisc &src, uint32_t game, int number, const std::vector<int> &known_abilities) {
  using namespace detail;
  Result out;
  int at = -1, size_at = -1, gender_at = -1, abilities_at = -1;
  if (game == 0x545108B4)
    at = 4, size_at = 0x34, gender_at = 256, abilities_at = 274;
  else if (game == 0x545408B2 || game == 0x545408B6)
    at = 36, size_at = 22, gender_at = 27;
  else if (game == 0x315A07D2)
    at = 40, size_at = 26, gender_at = 31;
  const Fitted *fitted = FittedOf(game);
  if ((at < 0 && !fitted) || number < 0)
    return out;
  Bytes d;
  if (at >= 0) {
    const auto dat = theme_source::detail::Records(src, "DAT");
    if (game == 0x315A07D2) {
      for (const auto &[i, x] : dat)
        if (x.size() > 6 && (x[4] | x[5] << 8) == number)
          d = x;
    } else if (const auto it = dat.find(uint32_t(number)); it != dat.end()) {
      d = it->second;
    }
  } else {
    d = RecordOf(src, number);
  }
  if (d.empty())
    return out;
  out.found = true;
  int size = 0;
  if (at >= 0 && d.size() >= size_t(std::max(at + 14, size_at + 2))) {
    static const int kFrom14[12] = {0, 1, 2, 3, 4, 6, 7, 8, 9, 10, 12, 13};
    std::vector<double> v;
    for (int i : kFrom14)
      v.push_back(d[size_t(at + i)]);
    out.attributes = Checked(v);
    size = d[size_t(size_at)] | d[size_t(size_at + 1)] << 8;
  } else if (fitted) {
    std::vector<double> v;
    for (const auto &l : fitted->r)
      if (l.at >= 0 && size_t(l.at) < d.size())
        v.push_back(l.a + l.b * d[size_t(l.at)]);
    if (v.size() == 12)
      out.attributes = Checked(v);
    if (fitted->size.at >= 0 && size_t(fitted->size.at + 1) < d.size())
      size = int(std::lround(fitted->size.a +
                             fitted->size.b * (d[size_t(fitted->size.at)] | d[size_t(fitted->size.at) + 1] << 8)));
    gender_at = fitted->gender_at;
  }
  if (size >= 3200 && size <= 5000)
    out.size = size;
  if (gender_at >= 0 && size_t(gender_at) < d.size())
    out.gender = fitted && fitted->gender_bit < 0 ? (d[size_t(gender_at)] == 0 ? 1 : 0) : (d[size_t(gender_at)] & 1);
  if (abilities_at >= 0 && size_t(abilities_at + 5) <= d.size())
    for (int k = 0; k < 5; ++k)
      if (std::find(known_abilities.begin(), known_abilities.end(), int(d[size_t(abilities_at + k)])) !=
          known_abilities.end())
        out.abilities.push_back(d[size_t(abilities_at + k)]);
  std::sort(out.abilities.begin(), out.abilities.end());
  return out;
}

}  // namespace ratings_import
