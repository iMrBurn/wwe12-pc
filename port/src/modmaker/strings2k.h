// Mod Maker: the WWE 2K games' texts, for the names of their superstars and arenas (read from the player's own
// discs, My game discs). Strings: pac/string.pac (2K17: string_360.pac, HSPC), tables of u32 0, u32 count, count x
// {u32 offset, u32 length, u32 id}; English = groups "SDB /XX00" (2K17's names are hashed: every table, the first
// id seen kept). Arenas: pac/wuniData.pac, "udt" tables (u32 "udt\2", u32 count, u32 52, ...; records from +32)
// of {name[40], u32 string id, u32 arena number, u32 index} (2K15 to 2K17; 2K14 has none).
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include "game_disc.h"
#include "svrfmt/archive.h"

namespace strings2k {

using Bytes = std::vector<uint8_t>;

// Every string of the game's English tables with an id of at least `min_id` ("" if the game has none).
inline std::map<uint32_t, std::string> Strings(const disc::GameDisc &g, uint32_t min_id = 0) {
  std::map<uint32_t, std::string> strings;
  Bytes str;
  svrfmt::Archive s;
  if ((!g.Read("pac/string.pac", str) && !g.Read("pac/string_360.pac", str)) || !svrfmt::ArchiveOpen(str, s, nullptr))
    return strings;
  const auto walk = [&](auto &self, svrfmt::ArcNode &n, int depth) -> void {
    if (depth == 1 && n.group == "SDB " && n.name.size() == 4 && n.name.compare(2, 2, "00") != 0)
      return;  // (another language)
    const Bytes d = depth ? svrfmt::ArchiveData(n) : Bytes();
    uint32_t zero = 1, count = 0;
    if (d.size() >= 8) {
      memcpy(&zero, d.data(), 4);
      memcpy(&count, d.data() + 4, 4);
    }
    if (depth && zero == 0 && count && 8 + size_t(count) * 12 <= d.size()) {
      for (uint32_t i = 0; i < count; ++i) {
        uint32_t e[3];
        memcpy(e, d.data() + 8 + i * 12, 12);
        if (e[0] < d.size() && e[1] <= d.size() - e[0] && e[2] >= min_id) {
          std::string t(reinterpret_cast<const char *>(d.data() + e[0]),
                        strnlen(reinterpret_cast<const char *>(d.data() + e[0]), e[1]));
          for (const char *q : {"\xE2\x80\x9C", "\xE2\x80\x9D"})  // (curly quotes: plain ones for the font)
            for (size_t at; (at = t.find(q)) != std::string::npos;)
              t.replace(at, 3, "\"");
          strings.emplace(e[2], std::move(t));
        }
      }
      return;
    }
    if (depth < 3 && svrfmt::ArchiveExpand(n))
      for (auto &c : n.kids)
        self(self, c, depth + 1);
  };
  walk(walk, s.root, 0);
  return strings;
}

// The arenas' names by arena number (bgNN), for the 2K games that have them.
inline std::map<int, std::string> ArenaNames(const disc::GameDisc &g) {
  std::map<int, uint32_t> ids;
  Bytes u;
  svrfmt::Archive a;
  if (!g.Read("pac/wuniData.pac", u) || !svrfmt::ArchiveOpen(u, a, nullptr))
    return {};
  const auto walk = [&](auto &self, svrfmt::ArcNode &n, int depth) -> void {
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
    for (uint32_t i = 0; i < count; ++i) {
      uint32_t sid, bg;
      memcpy(&sid, b.data() + 32 + i * 52 + 40, 4);
      memcpy(&bg, b.data() + 32 + i * 52 + 44, 4);
      if (sid >= 1000 && bg < 1000)  // (smaller ids: the created arenas' slots)
        ids.emplace(int(bg), sid);
    }
  };
  walk(walk, a.root, 0);
  std::map<int, std::string> names;
  if (ids.empty())
    return names;
  const auto strings = Strings(g, 1000);
  for (const auto &[bg, sid] : ids)
    if (const auto it = strings.find(sid); it != strings.end() && !it->second.empty())
      names[bg] = it->second;
  return names;
}

}  // namespace strings2k
