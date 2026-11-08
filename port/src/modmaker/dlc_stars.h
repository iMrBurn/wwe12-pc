// Mod Maker: the official DLC superstars the game installed for the player (Documents\wwe12\0000000000000000\
// 54510890\00000002\<package>, not our pack WWE12PCMODS): their chNN_dlcMM.pac, named from the package's own chEtc.pac.
// The SUPERSTARS page listed them already; NEW SUPERSTAR (as a base) and MEDIA too since Jay's report n°2 (2026-11-04:
// Shawn Michaels missing from their lists).
#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include "game_disc.h"

namespace dlc_stars {

namespace fs = std::filesystem;

struct Star {
  int id = 0;
  std::string name;  // its name in its package ("SHAWN MICHAELS"), else "Superstar NN"
  fs::path file;     // its pac\ch\chNN_dlcMM.pac
  fs::path pack;     // its package's folder (its chEtc.pac: its record)
};

// A superstar file of the disc: chNNN.pac, or chNN.pac (Shawn Michaels' ch53.pac, his pre-order DLC on the disc: no
// page listed him, players' reports 2026-11-08)
inline bool DiscStarFile(const std::string &f) {
  auto dig = [&](size_t i) { return i < f.size() && isdigit(static_cast<unsigned char>(f[i])); };
  return (f.size() == 8 || f.size() == 9) && !_strnicmp(f.c_str(), "ch", 2) && dig(2) && dig(3) && (f.size() == 8 || dig(4)) &&
         !_stricmp(f.c_str() + f.size() - 4, ".pac");
}

// Those not already in `have` (the disc's), in package order.
inline std::vector<Star> List(const fs::path &user_root, const std::set<int> &have) {
  std::vector<Star> out;
  if (user_root.empty())
    return out;
  std::error_code ec;
  std::set<int> seen = have;
  const fs::path content = user_root / "0000000000000000" / "54510890" / "00000002";
  std::vector<fs::path> packs;
  for (fs::directory_iterator it(content, ec), end; !ec && it != end; it.increment(ec))
    if (it->is_directory(ec) && it->path().filename() != "WWE12PCMODS" && fs::is_directory(it->path() / "pac" / "ch", ec))
      packs.push_back(it->path());
  std::sort(packs.begin(), packs.end());
  for (const auto &pk : packs) {
    disc::GameDisc d;
    if (!d.OpenPath(pk))
      continue;
    const auto names = disc::SuperstarNames(d);
    for (const auto &f : d.List("pac/ch")) {
      if (f.size() < 12 || _strnicmp(f.c_str(), "ch", 2) || !isdigit(static_cast<unsigned char>(f[2])) ||
          f.find("_dlc") == std::string::npos || _stricmp(f.c_str() + f.size() - 4, ".pac"))
        continue;
      Star s;
      s.id = atoi(f.c_str() + 2);
      if (!seen.insert(s.id).second)
        continue;
      s.file = pk / "pac" / "ch" / f;
      s.pack = pk;
      const auto it = names.find(s.id);
      s.name = it != names.end() ? it->second : "Superstar " + std::to_string(s.id);
      out.push_back(s);
    }
  }
  return out;
}

}  // namespace dlc_stars
