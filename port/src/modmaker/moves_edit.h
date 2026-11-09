// Mod Maker, MOVES & ENTRANCES page: a WWE '12 superstar's moveset changed by hand, slot by slot, with the game's own
// moves (2026-11-08; after Kaiko's SvR 2011 Mod Maker, its Moves / Taunts / Entrance tabs and their "Play", with
// credit). The slots are WWE '12's CHAR / PRO sections (moveset_port.h kSections); a slot offers the moves the game's
// roster puts in it first, then the other moves of its section (the same kind); slots the whole roster has alike are
// left out (ring in / out...). A move's motion, for its preview: m.pac ZMOT/CAnn, a PACH tree whose pairs of children
// hold the light and full records, the motion number its move number with its bytes swapped (newmoves13.h).
#pragma once

#include <algorithm>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "game_disc.h"
#include "moveset_port.h"
#include "newmoves13.h"
#include "svrfmt/pac.h"

namespace moves_edit {

using Bytes = std::vector<uint8_t>;

// A section's name (moveset_port::detail::kSections, in order), and whether the page shows it.
struct Section {
  const char *en, *fr;
  bool shown;
};
inline const Section &SectionInfo(int i) {
  static const Section s[] = {
      {"Ring in and out", "Entrer et sortir du ring", false},
      {"Taunts", "Provocations", true},
      {"Taunts (more) and comeback", "Provocations (suite) et retour", true},
      {"Signature moves", "Prises de signature", true},
      {"Finishers", "Finishers", true},
      {"Royal Rumble and specials", "Royal Rumble et spéciales", true},
      {"Ladder", "Échelle", true},
      {"Ropes", "Cordes", true},
      {"Strikes", "Coups", true},
      {"Fixed", "Fixes", false},
      {"Front grapples", "Prises de face", true},
      {"Rear grapples", "Prises de dos", true},
      {"Ground", "Au sol", true},
      {"Ropes (ground)", "Cordes (au sol)", true},
      {"Corner", "Dans le coin", true},
      {"Diving", "Plongeons", true},
      {"Apron", "Tablier", true},
      {"Running", "En courant", true},
      {"Irish whip", "Projection dans les cordes", true},
      {"Fixed (2)", "Fixes (2)", false},
      {"Tag team", "En équipe", true},
      {"Fixed (3)", "Fixes (3)", false},
      {"Outside dives", "Plongeons à l'extérieur", true},
      {"Reversals", "Contres", true},
      {"Submissions", "Soumissions", true},
  };
  static const Section none = {"Other", "Autres", true};
  return i >= 0 && i < int(std::size(s)) ? s[i] : none;
}
inline int SectionCount() { return int(std::size(moveset_port::detail::kSections)); }

// WWE '12's moves and how its roster fills each slot.
struct Catalog {
  bool loaded = false;
  std::map<int, std::string> names;            // move number -> name
  std::map<int, Bytes> pro;                    // superstar number -> his CHAR / PRO record
  std::map<int, std::vector<int>> slot_moves;  // slot (PRO offset) -> the moves the roster puts there, most used first
  std::map<int, std::vector<int>> section_moves;  // section -> its moves (any slot of it), by name
  std::set<int> fixed;                            // slots the whole roster has alike

  static int At(const Bytes &pro, int off) { return size_t(off) + 1 < pro.size() ? pro[size_t(off)] << 8 | pro[size_t(off) + 1] : 0; }

  bool Load(const disc::GameDisc &w12) {
    if (loaded)
      return true;
    moveset_port::detail::Game g;
    if (!moveset_port::detail::Load(w12, g))
      return false;
    names = std::move(g.names);
    pro = std::move(g.pro);
    std::map<int, std::map<int, int>> counts;  // slot -> move -> how many
    for (const auto &[who, p] : pro)
      for (int off = moveset_port::detail::kFirst; off < moveset_port::detail::kEnd; off += 2)
        if (const int id = At(p, off); id && names.count(id))
          ++counts[off][id];
    std::map<int, std::set<int>> sec;
    for (const auto &[off, c] : counts) {
      if (c.size() <= 1)
        fixed.insert(off);
      std::vector<std::pair<int, int>> v(c.begin(), c.end());
      std::sort(v.begin(), v.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
      for (const auto &[id, n] : v) {
        slot_moves[off].push_back(id);
        sec[moveset_port::detail::SectionOf(off)].insert(id);
      }
    }
    for (auto &[s, ids] : sec) {
      std::vector<int> v(ids.begin(), ids.end());
      std::sort(v.begin(), v.end(), [&](int a, int b) { return names[a] < names[b]; });
      section_moves[s] = std::move(v);
    }
    loaded = true;
    return true;
  }
  std::string Name(int id) const {
    const auto it = names.find(id);
    return it != names.end() ? it->second : (id ? "#" + std::to_string(id) : std::string("-"));
  }
};

// Where each match move's motion is in m.pac (ZMOT/CAnn), to read it again when it is played.
class MotionIndex {
 public:
  bool built = false;

  // Every ZMOT/CA row read once (a few seconds): motion number -> (row, top, pair).
  void Build(const disc::GameDisc &w12) {
    {
      std::lock_guard<std::mutex> lock(cache_mutex_);
      cache_.clear();
    }
    built = true;
    const std::string file = "pac/m.pac";
    Bytes head;
    const uint64_t size = w12.SizeOf(file);
    if (!size || !w12.ReadPart(file, 0, 0x8000, head) || !svrfmt::EpacTable(head, size, rows_))
      return;
    for (size_t r = 0; r < rows_.size(); ++r) {
      if (newmoves13::detail::Trim(rows_[r].group) != "ZMOT" || rows_[r].name.rfind("CA", 0) != 0)
        continue;
      Bytes raw;
      std::vector<svrfmt::PachEntry> tops;
      if (!w12.ReadPart(file, rows_[r].offset, rows_[r].size, raw) || !svrfmt::PachRead(svrfmt::Unpack(raw), tops))
        continue;
      for (size_t t = 0; t < tops.size(); ++t) {
        std::vector<svrfmt::PachEntry> kids;
        if (!svrfmt::PachRead(svrfmt::Unpack(tops[t].data), kids))
          continue;
        for (size_t i = 0; i + 1 < kids.size(); i += 2) {
          std::vector<svrfmt::PachEntry> hi;
          if (!svrfmt::PachRead(svrfmt::Unpack(kids[i + 1].data), hi))
            continue;
          for (const auto &h : hi)
            where_.emplace(int(h.id), Place{r, t, i + 1});
        }
      }
    }
  }
  bool Has(int move) const { return where_.count(newmoves13::MotionOf(move)) != 0; }
  // The full record of move `move`'s motion (yanm), empty if none.
  Bytes Full(const disc::GameDisc &w12, int move) const {
    const auto it = where_.find(newmoves13::MotionOf(move));
    if (it == where_.end())
      return {};
    // (the motions' group - an entry of m.pac, unpacked three times over - kept for the next ones: a SvR move's
    // model choice reads ~1 000 motions, 6 ms each from the disc, 2026-11-09)
    const Place &pl = it->second;
    std::lock_guard<std::mutex> lock(cache_mutex_);
    std::vector<svrfmt::PachEntry> *hi = nullptr;
    for (auto &c : cache_)
      if (c.row == pl.row && c.top == pl.top && c.kid == pl.kid)
        hi = &c.hi;
    if (!hi) {
      const auto &row = rows_[pl.row];
      Bytes raw;
      std::vector<svrfmt::PachEntry> tops, kids, got;
      if (!w12.ReadPart("pac/m.pac", row.offset, row.size, raw) || !svrfmt::PachRead(svrfmt::Unpack(raw), tops) ||
          pl.top >= tops.size() || !svrfmt::PachRead(svrfmt::Unpack(tops[pl.top].data), kids) ||
          pl.kid >= kids.size() || !svrfmt::PachRead(svrfmt::Unpack(kids[pl.kid].data), got))
        return {};
      if (cache_.size() >= 24)
        cache_.erase(cache_.begin());
      cache_.push_back({pl.row, pl.top, pl.kid, std::move(got)});
      hi = &cache_.back().hi;
    }
    for (const auto &h : *hi)
      if (int(h.id) == it->first)
        return svrfmt::Unpack(h.data);
    return {};
  }
  size_t Count() const { return where_.size(); }

 private:
  struct Place {
    size_t row, top, kid;
  };
  std::vector<svrfmt::EpacRow> rows_;
  std::map<int, Place> where_;
  struct Group {
    size_t row, top, kid;
    std::vector<svrfmt::PachEntry> hi;
  };
  mutable std::mutex cache_mutex_;
  mutable std::vector<Group> cache_;  // the last motion groups read (Full)
};

}  // namespace moves_edit
