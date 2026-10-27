// A superstar's entrance from WWE '13 (docs/plan_movesets.md, step 4). WWE '13's entrances are WWE '12's format
// (Kaiko's OTHER_GAMES_ENTRANCES.md; compared here: Cena's script 93 % the same bytes, the same track trees):
//  - WWE '13: pac\evt\Entrance00..08.pac (xcompressed EPK8), groups "EVP " (the script, "0FOP") and "EVZ " (a PACH of
//    YANM tracks, one per object), entries named by the entrance number: NNN and its variants 1xNNN (10NNN..14NNN);
//  - WWE '12: pac\evt\Nyujyo0-9,A.pac, the same entries in groups by number range (Nyujyo0 EVP1 / EVZ1 100.., Nyujyo1
//    EVP2 137.., Nyujyo2 EVP3 179.., Nyujyo3 EVP4 272.., Nyujyo4 EVP5 328.., Nyujyo5 EVPE 480.., Nyujyo6 EVPF 512..,
//    Nyujyo7 EVPG 532.., Nyujyo8 EVPH 552.., Nyujyo9 EVPI 600..855, NyujyoA EVPK 50..55); the superstar's CHAR / PRO
//    +0x1D4 names the entrance (16-bit little-endian).
// Entrance(): an entrance's entries as WWE '13 stores them (unpacked), for the game to serve under WWE '12's names.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "game_disc.h"
#include "svrfmt/archive.h"
#include "svrfmt/pac.h"
#include "svrfmt/xcompress.h"

namespace entrance13 {

using Bytes = std::vector<uint8_t>;

struct Part {
  int number = 0;  // NNN or a variant 1xNNN
  bool script = false;  // EVP (else EVZ: the tracks)
  Bytes data;
};

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

// The mod's file of a part (superstar/entrance/EVP_246.bin...): the game serves it as /EVP<group>/<number>.
inline std::string ModPath(const Part &p) {
  return "superstar/entrance/" + std::string(p.script ? "EVP_" : "EVZ_") + std::to_string(p.number) + ".bin";
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
      out.push_back({n, k.group.rfind("EVP", 0) == 0, svrfmt::Unpack(svrfmt::ArchiveData(k))});
    }
  }
  if (out.empty() && why)
    *why = "no such entrance in WWE '13";
  return !out.empty();
}

}  // namespace entrance13
