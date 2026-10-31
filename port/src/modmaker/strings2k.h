// Mod Maker: the WWE 2K games' texts, for the names of their superstars and arenas (read from the player's own
// discs, My game discs). Strings: pac/string.pac (2K17: string_360.pac, HSPC), tables of u32 0, u32 count, count x
// {u32 offset, u32 length, u32 id}; English = groups "SDB /XX00" (2K17's names are hashed: every table, the first
// id seen kept). Arenas: pac/wuniData.pac, "udt" tables (u32 "udt\2", u32 count, u32 52, ...; records from +32)
// of {name[40], u32 string id, u32 arena number, u32 index} (2K15 to 2K17). WWE 2K14 has no such table (its arena
// select screen shows logos): its arenas were named from their aprons (rin_ep00), kArenas2K14.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "game_disc.h"
#include "svrfmt/archive.h"

namespace strings2k {

using Bytes = std::vector<uint8_t>;

// The copies of a game's file and of its DLC's own ones: "pac/ch/chEtc.pac" -> every chEtc.pac (the disc's, a DLC's
// newer one: SvR 2009 / 2011) and every pac/ch/chEtc_dlcNN.pac (WWE '13, the 2K games), the disc's first.
inline std::vector<Bytes> WithDlc(const disc::GameDisc &g, const std::string &path, bool copies = true) {
  auto all = copies ? g.ReadAll(path) : std::vector<Bytes>();
  const size_t slash = path.rfind('/'), dot = path.rfind('.');
  const std::string dir = path.substr(0, slash), stem = path.substr(slash + 1, dot - slash - 1) + "_dlc";
  auto files = g.List(dir);
  std::sort(files.begin(), files.end());
  for (const auto &f : files)
    if (f.size() > stem.size() && _strnicmp(f.c_str(), stem.c_str(), stem.size()) == 0 &&
        _stricmp(f.c_str() + f.size() - 4, ".pac") == 0)
      for (auto &b : g.ReadAll(dir + "/" + f))
        all.push_back(std::move(b));
  return all;
}

// Every string of the game's English tables with an id of at least `min_id` ("" if the game has none), its DLC's too
// (string_dlcNN.pac, 2K17 string_dlcNN_360.pac; a DLC's newer string.pac over the disc's: SvR 2011's DLC arenas).
inline std::map<uint32_t, std::string> Strings(const disc::GameDisc &g, uint32_t min_id = 0) {
  std::map<uint32_t, std::string> strings;
  auto all = g.ReadAll("pac/string.pac");
  for (auto *more : {"pac/string_360.pac", ""})
    for (auto &b : *more ? g.ReadAll(more) : WithDlc(g, "pac/string.pac", false))
      all.push_back(std::move(b));
  for (auto &str : all) {
    svrfmt::Archive s;
    if (!svrfmt::ArchiveOpen(str, s, nullptr))
      continue;
    std::map<uint32_t, std::string> one;  // (this file's: the first id seen kept; the later files' over the others)
    const auto walk = [&](auto &self, svrfmt::ArcNode &n, int depth) -> void {
      if (depth == 1 && n.group == "SDB " && n.name.size() == 4 && n.name.compare(2, 2, "00") != 0)
        return;  // (another language)
      const Bytes d = depth ? svrfmt::ArchiveData(n) : Bytes();
      uint32_t zero = 1, count = 0;
      if (d.size() >= 8) {
        memcpy(&zero, d.data(), 4);
        memcpy(&count, d.data() + 4, 4);
      }
      // (records of 12 bytes {offset, length, id}, or of 16 with a 0 after: SvR 2011, WWE '12 / '13; the size whose
      // every record fits, 16 tried first, as Kaiko's arena_names.py)
      size_t rec = 0;
      for (size_t r : {size_t(16), size_t(12)})
        if (!rec && depth && zero == 0 && count && 8 + size_t(count) * r <= d.size()) {
          bool fits = true;
          for (uint32_t i = 0; i < count && fits; ++i) {
            uint32_t e[2];
            memcpy(e, d.data() + 8 + i * r, 8);
            fits = e[0] <= d.size() && e[1] <= d.size() - e[0];
          }
          rec = fits ? r : 0;
        }
      if (rec) {
        for (uint32_t i = 0; i < count; ++i) {
          uint32_t e[3];
          memcpy(e, d.data() + 8 + i * rec, 12);
          if (e[0] < d.size() && e[1] <= d.size() - e[0] && e[2] >= min_id) {
            std::string t(reinterpret_cast<const char *>(d.data() + e[0]),
                          strnlen(reinterpret_cast<const char *>(d.data() + e[0]), e[1]));
            for (const char *q : {"\xE2\x80\x9C", "\xE2\x80\x9D"})  // (curly quotes: plain ones for the font)
              for (size_t at; (at = t.find(q)) != std::string::npos;)
                t.replace(at, 3, "\"");
            one.emplace(e[2], std::move(t));
          }
        }
        return;
      }
      if (depth < 3 && svrfmt::ArchiveExpand(n))
        for (auto &c : n.kids)
          self(self, c, depth + 1);
    };
    walk(walk, s.root, 0);
    for (auto &[id, t] : one)
      if (!t.empty() || !strings.count(id))
        strings[id] = std::move(t);
  }
  return strings;
}

struct NamedArena {
  int file;
  const char *name;
};
inline constexpr NamedArena kArenas2K14[] = {
    {0, "SmackDown"},         {1, "RAW"},                 {2, "NXT"},                {3, "Superstars"},
    {4, "Over the Limit"},    {5, "No Way Out"},          {6, "Money in the Bank"},  {7, "SummerSlam"},
    {8, "Night of Champions"}, {9, "Hell in a Cell"},     {11, "Survivor Series"},   {12, "TLC"},
    {13, "Royal Rumble"},     {14, "Elimination Chamber"}, {15, "WrestleMania 29"},  {16, "Extreme Rules"},
    {17, "Main Event"},       {21, "Elimination Chamber (2)"}, {22, "Royal Rumble (2)"}, {23, "RAW (2)"},
    {28, "WrestleMania"},     {29, "WrestleMania 2"},     {30, "WrestleMania III"},  {31, "WrestleMania IV"},
    {32, "WrestleMania V"},   {33, "WrestleMania VI"},    {34, "WrestleMania VII"},  {35, "WrestleMania VIII"},
    {36, "WrestleMania IX"},  {37, "WrestleMania X"},     {38, "WrestleMania XI"},   {39, "WrestleMania XII"},
    {40, "WrestleMania 13"},  {41, "WrestleMania XIV"},   {42, "WrestleMania XV"},   {43, "WrestleMania 2000"},
    {44, "WrestleMania X-Seven"}, {45, "WrestleMania X8"}, {46, "WrestleMania XIX"}, {47, "WrestleMania XX"},
    {48, "WrestleMania 21"},  {49, "WrestleMania 22"},    {50, "WrestleMania 23"},   {51, "WrestleMania XXIV"},
    {52, "WrestleMania 25"},  {53, "WrestleMania XXVI"},  {54, "WrestleMania XXVII"}, {55, "WrestleMania XXVIII"},
    {56, "WrestleMania XXX"},
};

// The arenas' names by arena number (bgNN), for the 2K games that have them (their DLC's too: WUniData_dlc40.pac).
inline std::map<int, std::string> ArenaNames(const disc::GameDisc &g) {
  std::map<int, uint32_t> ids;
  for (auto &u : WithDlc(g, "pac/wuniData.pac")) {
    svrfmt::Archive a;
    if (!svrfmt::ArchiveOpen(u, a, nullptr))
      continue;
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
  }
  std::map<int, std::string> names;
  if (ids.empty())
    return names;
  const auto strings = Strings(g, 1000);
  for (const auto &[bg, sid] : ids)
    if (const auto it = strings.find(sid); it != strings.end() && !it->second.empty())
      names[bg] = it->second;
  return names;
}

// The 2K games' names: their CHAR / DAT records (208, 216 or 220 bytes, number at +4) hold string ids made of the
// number plus a constant: the full name is number + 64000 in WWE 2K14, number + 6000 in 2K15 to 2K17 (+1000 / +3000
// are shorter names, +9000 the Twitter name), in the game's strings (Strings).
inline void Names2K(const disc::GameDisc &g, const std::map<int, std::vector<int>> &ids,
                    std::map<int, std::string> &names) {
  const auto strings = Strings(g, 6000);
  for (const auto &[number, ks] : ids)
    for (int k : {64000, 6000})
      if (std::find(ks.begin(), ks.end(), k) != ks.end())
        if (const auto it = strings.find(uint32_t(number + k)); it != strings.end() && !it->second.empty()) {
          names[number] = it->second;
          break;
        }
}

// A game's superstar names by number, from its pac/ch/chEtc.pac: the CHAR / DAT records (SvR 2011 264 bytes, id at
// +36 and name at +38; WWE '12 290 bytes, +44 / +46; WWE '13 324 bytes, +56 / +58), the 2K games' in their strings.
inline std::map<int, std::string> SuperstarNames(const disc::GameDisc &g) {
  std::map<int, std::string> names;
  std::map<int, std::vector<int>> ids;  // the 2K games: number -> its "number + k" string ids' k
  // (SvR 2008 keeps its records in pac/misc.pac: 232 bytes, id at +30; SvR 2009 in chEtc.pac, 252 bytes, id at +36;
  // after Kaiko's superstar_port.py. A DLC's: a newer chEtc.pac, or its chEtc_dlcNN.pac)
  auto all = WithDlc(g, "pac/ch/chEtc.pac");
  if (all.empty())
    all = g.ReadAll("pac/misc.pac");
  for (auto &etc : all) {
    svrfmt::Archive a;
    if (!svrfmt::ArchiveOpen(etc, a, nullptr))
      continue;
    const auto walk = [&](auto &self, svrfmt::ArcNode &n, int depth) -> void {
      const Bytes d = depth ? svrfmt::ArchiveData(n) : Bytes();
      const size_t id_at = d.size() == 264 || d.size() == 252 ? 36 : d.size() == 290 ? 44 : d.size() == 324 ? 56
                           : d.size() == 232                   ? 30
                                                               : 0;
      if (d.size() == 208 || d.size() == 216 || d.size() == 220) {  // (the 2K games: names in their strings)
        const int number = d[4] | d[5] << 8;
        for (size_t k = 6; k + 1 < 32; k += 2)
          if (const int v = d[k] | d[k + 1] << 8; number > 0 && v > number)
            ids[number].push_back(v - number);
        return;
      }
      if (id_at) {
        std::string name;
        for (size_t k = id_at + 2; k < id_at + 34 && k < d.size() && d[k]; ++k)
          name += d[k] < 0x80 ? std::string(1, char(d[k]))
                              : std::string{char(0xC0 | (d[k] >> 6)), char(0x80 | (d[k] & 0x3F))};
        names[d[id_at] | d[id_at + 1] << 8] = name;
        return;
      }
      if (depth < 3 && svrfmt::ArchiveExpand(n))
        for (auto &c : n.kids)
          self(self, c, depth + 1);
    };
    walk(walk, a.root, 0);
  }
  if (names.empty() && !ids.empty())  // (SvR 2011, WWE '12, WWE '13: names in the records)
    Names2K(g, ids, names);
  return names;
}

}  // namespace strings2k
