// A ported superstar's own moveset (docs/plan_movesets.md, step 1): its moves in its game (SvR 2011, WWE '13...) put
// into WWE '12's slots by their names. The method is Kaiko's (SvR 2011 PC port, KaikoClanworth1: moveset_port.py and
// moveset_maps.py, the other way round):
//  - the moveset: pac\ch\chEtc.pac CHAR / PRO, the child of the superstar's number, 16-bit move numbers big-endian
//    (WWE '12 1072 bytes, moves from +0x00A to +0x1C2; SvR 2011 1056 bytes; WWE '13 1180 bytes);
//  - the move names: pac\misc.pac MOVS / WAZE, 160-byte records, name at +0x10, number u16 at +0x90 (where they start
//    differs: WWE '12 and SvR 2011 0x430, WWE '13 elsewhere: found from the records that look like moves);
//  - the slots: which slot of the other game is which of WWE '12's, from the superstars both games have (the same
//    numbers, and since 2026-11-05 the same names: SharedStars): the slot pairs whose moves have the same names (the
//    same numbers too for the games that kept WWE '12's) most often, in order (SvR 2011: WWE '12's 4 bytes on), then
//    the ones left out of order that are each other's best match (the 2K games moved whole sections);
//  - a slot takes the WWE '12 move of the same name (else of the same name without its variant number,
//    "Attitude Adjustment 2" -> "Attitude Adjustment 4"; else the same words in another order or spelling, "Reversal
//    Clothesline" -> "Clothesline Reversal 1"; else a longer / shorter name of a move the roster has in that slot)
//    that WWE '12's roster puts in that slot, else in that section, else whose category bits fit the slot (a strike
//    doesn't go to a grapple slot; such a move has its animations); else the base's move stays (Row::kept_why).
//    Slots the whole roster has alike (ring in / out...) stay the base's.
// Measured offline on every superstar of each game (2026-11-05, the study tool "cover"; share of WWE '12's 210 slots
// that differ in its roster filled, before -> after): SvR 2007 9 -> 15 %, 2008 12 -> 17 %, 2009 20 -> 24 %, 2010 40 ->
// 46 %, 2011 73 -> 75 %, WWE '13 73 -> 74 %, 2K14 48 -> 73 %, 2K15 47 -> 72 %, 2K16 29 -> 52 % (the old games have fewer
// slots and other names). Of the moves a superstar has in a slot found: SvR 2007 48 -> 57 %, 2011 93 -> 95 %, the 2K
// games 91-97 %.
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "game_disc.h"
#include "svrfmt/archive.h"
#include "svrfmt/pac.h"
#include "svrfmt/xcompress.h"
#include "strings2k.h"
#include "svr_old_slots.h"

namespace moveset_port {

using Bytes = std::vector<uint8_t>;

struct Row {
  int off = 0;           // the WWE '12 PRO byte offset
  std::string from, to;  // the SvR move's name; the WWE '12 move's ("" = the base's kept)
  bool variant = false;  // found without its variant number (another version of the move)
  int from_id = 0;       // the other game's move number (as PRO holds it)
  bool new_move = false;  // WWE '12 hasn't that number (WWE '13, the 2K games): a move to add (newmoves13.h)
  bool added = false;     // ... and it is added (its record and motions go with the mod)
  // why the base's move stays: 0 it doesn't (a move found), 1 its move has no name, 2 WWE '12 has no move of that
  // name, 3 WWE '12's move of that name can't go in that slot (another kind of move)
  int kept_why = 0;
};

struct Result {
  std::vector<std::pair<int, int>> moves;  // (PRO offset, WWE '12 move number)
  std::vector<Row> rows;                   // every slot the SvR superstar has a move in
  int exact = 0, variant = 0, kept = 0;
  int slots = 0;  // WWE '12's slots its moveset could fill (those the roster doesn't all have alike)
};

namespace detail {

// (names compared without their padding: WWE '12 "PRO ", the 2K games "PRO     ")
inline std::string Trim(std::string s) {
  while (!s.empty() && (s.back() == ' ' || s.back() == '\0'))
    s.pop_back();
  return s;
}

inline svrfmt::ArcNode *Find(svrfmt::ArcNode &n, const char *g, const char *nm) {
  for (auto &k : n.kids)
    if (Trim(k.group) == Trim(g) && Trim(k.name) == Trim(nm))
      return &k;
  for (auto &k : n.kids)
    if (auto *f = Find(k, g, nm))
      return f;
  return nullptr;
}

struct Game {
  std::map<int, std::string> names;  // move number -> name
  std::map<int, Bytes> pro;          // superstar number -> PRO
  std::map<int, Bytes> cats;         // move number -> its record's first 16 bytes (the slots it may go in, Kaiko)
  std::map<int, std::string> stars;  // superstar number -> name (strings2k), to pair superstars of two games
};

// (rec / num: the move records' size and where their number is: 160 / 0x90, SvR 2007-2009 144 / 0x88. A number is read
// big-endian, as the PRO's moves: SvR 2007-2011 store both little-endian, the same swap on both keeps them matching.)
// A pac of the disc, unpacked when it is compressed whole (2K17's createmode.pac and chEtc.pac: Xcompress).
inline bool ReadPac(const disc::GameDisc &d, const char *path, Bytes &out) {
  if (!d.Read(path, out))
    return false;
  if (svrfmt::IsXcompress(out)) {
    Bytes o;
    if (!svrfmt::XcompressDecode(out, o))
      return false;
    out.swap(o);
  }
  return true;
}

inline bool Load(const disc::GameDisc &d, Game &g, size_t rec = 160, size_t num = 0x90) {
  Bytes m[2], c;
  svrfmt::Archive M[2], C;
  const bool etc = ReadPac(d, "pac/ch/chEtc.pac", c) && svrfmt::ArchiveOpen(c, C, nullptr);
  // (the 2K games keep their move names in createmode.pac, Kaiko's moves_port.py Game14)
  svrfmt::ArcNode *w = nullptr;
  const char *files[2] = {"pac/misc.pac", "pac/createmode.pac"};
  for (int i = 0; i < 2 && !w; ++i)
    if (ReadPac(d, files[i], m[i]) && svrfmt::ArchiveOpen(m[i], M[i], nullptr))
      w = Find(M[i].root, "MOVS", "WAZE");
  // (SvR 2007 / 2008 keep their profiles in misc.pac, Kaiko's superstar_port.py Game08 / Game07: no chEtc.pac)
  svrfmt::ArcNode *pro = etc ? Find(C.root, "CHAR", "PRO ") : Find(M[0].root, "CHAR", "PRO ");
  svrfmt::ArcNode *dat = etc ? Find(C.root, "CHAR", "DAT ") : nullptr;
  // 2K17: its pacs are HSPCs, their names hashed (2026-11-08): the move records found by what they hold (the entry
  // with the most 160-byte records named like moves), the profiles by their shape (an SHDC of 1348-byte records,
  // PRO, and one of as many 220-byte ones, DAT).
  auto score = [&](const Bytes &t) {
    int best_score = 0;
    for (size_t ph = 0; ph < rec; ph += 2) {
      int sc = 0;
      for (size_t o = ph; o + rec <= t.size(); o += rec)
        sc += o + 0x12 <= t.size() && isalnum(t[o + 0x10]) && isalnum(t[o + 0x11]) && (t[o + num] | t[o + num + 1]);
      best_score = std::max(best_score, sc);
    }
    return best_score;
  };
  for (int i = 0; i < 2 && !w; ++i) {
    if (M[i].root.kind != svrfmt::ArcKind::Hspc)
      continue;
    int best_score = 500;
    for (auto &k : M[i].root.kids)
      if (const int sc = score(svrfmt::Unpack(svrfmt::ArchiveData(k))); sc > best_score) {
        best_score = sc;
        w = &k;
      }
  }
  if (etc && !pro && C.root.kind == svrfmt::ArcKind::Hspc) {
    auto shape = [](svrfmt::ArcNode &k, size_t &count) -> size_t {  // (its children's size when they are all alike)
      count = 0;
      if (!svrfmt::ArchiveExpand(k) || k.kids.empty())
        return 0;
      const size_t first = svrfmt::ArchiveData(k.kids[0]).size();
      for (auto &c : k.kids)
        if (svrfmt::ArchiveData(c).size() != first)
          return 0;
      count = k.kids.size();
      return first;
    };
    size_t most = 0;
    for (auto &k : C.root.kids) {
      size_t n = 0;
      if (shape(k, n) == 1348 && n > most) {
        most = n;
        pro = &k;
      }
    }
    for (auto &k : C.root.kids) {
      size_t n = 0;
      const size_t sz = shape(k, n);
      if (pro && n == most && sz >= 200 && sz <= 240)
        dat = &k;
    }
  }
  if (!w || !pro)
    return false;
  const Bytes d0 = svrfmt::Unpack(svrfmt::ArchiveData(*w));  // (SvR 2007 / 2008: BPE-packed)
  // a record that looks like a move: a name of letters, digits and punctuation, a number
  auto looks = [&](size_t o) {
    if (o + rec > d0.size() || !isalnum(d0[o + 0x10]) || !(d0[o + num] | d0[o + num + 1]))
      return false;
    size_t n = 0;
    while (n < 0x40 && d0[o + 0x10 + n] >= 0x20 && d0[o + 0x10 + n] < 0x7F)
      ++n;
    return n >= 2 && n < 0x40 && d0[o + 0x10 + n] == 0;
  };
  size_t phase = 0;
  int best = -1;
  for (size_t ph = 0; ph < rec; ph += 2) {
    int score = 0;
    for (size_t o = ph; o + rec <= d0.size(); o += rec)
      score += looks(o);
    if (score > best) {
      best = score;
      phase = ph;
    }
  }
  for (size_t o = phase; o + rec <= d0.size(); o += rec)
    if (looks(o)) {
      const int id = d0[o + num] << 8 | d0[o + num + 1];
      g.names.emplace(id, std::string(reinterpret_cast<const char *>(&d0[o + 0x10])));
      g.cats.emplace(id, Bytes(d0.begin() + long(o), d0.begin() + long(o + 16)));
    }
  g.stars = strings2k::SuperstarNames(d);
  if (svrfmt::ArchiveExpand(*pro) && !pro->kids.empty()) {
    for (auto &k : pro->kids)
      g.pro[int(k.id)] = svrfmt::ArchiveData(k);
  } else {  // (the 2K games: a PACH the archive doesn't open)
    std::vector<svrfmt::PachEntry> kids;
    if (svrfmt::PachRead(svrfmt::Unpack(svrfmt::ArchiveData(*pro)), kids))
      for (auto &k : kids)
        g.pro[int(k.id)] = svrfmt::Unpack(k.data);
  }
  // 2K16 / 2K17: PRO by index, the superstar's number at the DAT record of the same index (+4, u16 little-endian:
  // theme_source.h does the same; 2K16 Cesaro's moveset wasn't found by his number, 2026-11-08)
  if (dat) {
    std::map<int, Bytes> recs;
    if (svrfmt::ArchiveExpand(*dat) && !dat->kids.empty()) {
      for (auto &k : dat->kids)
        recs[int(k.id)] = svrfmt::ArchiveData(k);
    } else {
      std::vector<svrfmt::PachEntry> kids;
      if (svrfmt::PachRead(svrfmt::Unpack(svrfmt::ArchiveData(*dat)), kids))
        for (auto &k : kids)
          recs[int(k.id)] = svrfmt::Unpack(k.data);
    }
    // (2K16's DAT records: 216 bytes; 2K17's: 220)
    bool by_index = !recs.empty() && (recs.begin()->second.size() == 216 || recs.begin()->second.size() == 220);
    if (by_index) {
      std::map<int, Bytes> pro;
      for (const auto &[i, r] : recs)
        if (const auto it = g.pro.find(i); it != g.pro.end() && r.size() >= 6)
          pro[r[4] | r[5] << 8] = it->second;
      if (!pro.empty())
        g.pro.swap(pro);
    }
  }
  return !g.names.empty() && !g.pro.empty();
}

// Lower case letters and digits only; base: without the trailing variant number.
inline std::string Norm(std::string s, bool base) {
  if (base)
    while (!s.empty() && (isdigit(static_cast<unsigned char>(s.back())) || s.back() == ' '))
      s.pop_back();
  std::string o;
  for (char c : s)
    if (isalnum(static_cast<unsigned char>(c)))
      o += char(tolower(static_cast<unsigned char>(c)));
  return o;
}

inline int U16(const Bytes &b, size_t o) { return o + 1 < b.size() ? b[o] << 8 | b[o + 1] : -1; }

// A move name's words (2026-11-05): lower case, without numbers (the variant), "-handed" as "hand", "&" / "and" /
// "to" / "the" / "of" / "on" / "a" left out ("Reversal Clothesline" = "Clothesline Reversal 1", "One Hand Bulldog 2" =
// "One-handed Bulldog 1"), sorted.
inline std::vector<std::string> NameWords(const std::string &name) {
  std::vector<std::string> out;
  std::string cur;
  auto flush = [&] {
    if (cur.size() > 4 && cur.compare(cur.size() - 2, 2, "ed") == 0)
      cur.resize(cur.size() - 2);
    static const std::set<std::string> skip = {"and", "to", "the", "of", "on", "a", "in"};
    if (!cur.empty() && !std::all_of(cur.begin(), cur.end(), ::isdigit) && !skip.count(cur))
      out.push_back(cur);
    cur.clear();
  };
  for (char c : name)
    if (isalnum(static_cast<unsigned char>(c)))
      cur += char(tolower(static_cast<unsigned char>(c)));
    else
      flush();
  flush();
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}
inline std::string WordsKey(const std::string &name) {
  std::string k;
  for (const auto &w : NameWords(name))
    k += w + " ";
  return k;
}

// WWE '12's PRO sections (byte offsets, end excluded): ring moves, taunts, -, signatures, finishers, Royal Rumble,
// ladder, rope, strikes, -, front grapples, rear grapples, ground, rope, corner, diving, apron, running, Irish whip,
// -, tag team, -, outside dives, reversals, submissions (as Kaiko's pro_slots.json, 4 bytes on).
constexpr int kSections[][2] = {{0x00A, 0x016}, {0x016, 0x01E}, {0x01E, 0x022}, {0x022, 0x026}, {0x026, 0x02A},
                                {0x02A, 0x042}, {0x042, 0x04E}, {0x04E, 0x052}, {0x052, 0x06E}, {0x06E, 0x07E},
                                {0x07E, 0x0C0}, {0x0C0, 0x0C8}, {0x0C8, 0x102}, {0x102, 0x104}, {0x104, 0x154},
                                {0x154, 0x15E}, {0x15E, 0x176}, {0x176, 0x182}, {0x182, 0x18C}, {0x18C, 0x192},
                                {0x192, 0x1A2}, {0x1A2, 0x1A6}, {0x1A6, 0x1AE}, {0x1AE, 0x1B6}, {0x1B6, 0x1C2}};
constexpr int kFirst = 0x00A, kEnd = 0x1C2;

inline int SectionOf(int off) {
  for (int i = 0; i < int(sizeof(kSections) / sizeof(kSections[0])); ++i)
    if (off >= kSections[i][0] && off < kSections[i][1])
      return i;
  return -1;
}

inline void Fill(const Game &w, const Game &s, const Bytes &pro, const std::map<int, int> &slots, bool numbers,
                 moveset_port::Result &out);

}  // namespace detail

// A stand-in for a move that couldn't be added (star_port.h): among `slot` (WWE '12 move number -> how many of its
// rosters put it in that slot), the one whose name ends with the same word as `name` ("Running Leg Drop" -> a "... Drop"),
// then has the most words alike, then is the most used; 0 if none ends so. Numbers and case don't count.
inline int StandIn(const std::string &name, const std::map<int, int> &slot, const std::map<int, std::string> &names) {
  auto words = [](const std::string &s) {
    std::vector<std::string> out;
    std::string w;
    for (char c : s + " ") {
      if (std::isalpha(static_cast<unsigned char>(c))) {
        w += char(std::tolower(static_cast<unsigned char>(c)));
        continue;
      }
      if (w.size() >= 2)
        out.push_back(w);
      w.clear();
    }
    return out;
  };
  const auto mine = words(name);
  if (mine.empty())
    return 0;
  // (and done from the same place: "Running Leg Drop" found "Diving Fist Drop 4", from the top rope, 2026-11-10)
  auto where = [](const std::vector<std::string> &ws) {
    static const std::set<std::string> places = {"running", "diving", "springboard", "apron", "corner", "top", "rope",
                                                 "ropes", "turnbuckle", "slingshot", "ladder", "table", "cage"};
    std::set<std::string> out;
    for (const auto &w : ws)
      if (places.count(w))
        out.insert(w);
    return out;
  };
  const auto my_place = where(mine);
  int best = 0;
  std::pair<int, int> best_score{-1, -1};
  for (const auto &[id, count] : slot) {
    const auto it = names.find(id);
    if (it == names.end())
      continue;
    const auto theirs = words(it->second);
    if (theirs.empty() || theirs.back() != mine.back() || where(theirs) != my_place)
      continue;
    int alike = 0;
    for (const auto &w : mine)
      alike += std::count(theirs.begin(), theirs.end(), w) > 0;
    const std::pair<int, int> score{alike, count};
    if (score > best_score)
      best_score = score, best = id;
  }
  return best;
}

namespace detail {

inline bool g_in_order_only = false;  // (study tools: the slot map without its out-of-order pass)

// The superstars both games have: (WWE '12's number, the other game's): the same number (the games after SvR 2011 kept
// WWE '12's), and since 2026-11-05 the same name too (SvR 2007-2010 number theirs otherwise; the 2K games renumbered
// some). A name both games give one superstar only.
inline std::vector<std::pair<int, int>> SharedStars(const Game &w, const Game &s, bool by_number) {
  std::vector<std::pair<int, int>> out;
  std::set<int> wtaken, staken;
  if (by_number)
    for (const auto &[id, p] : w.pro)
      if (id >= 100 && s.pro.count(id) && p.size() >= size_t(kEnd)) {
        out.push_back({id, id});
        wtaken.insert(id);
        staken.insert(id);
      }
  std::map<std::string, std::vector<int>> wn, sn;
  for (const auto &[id, n] : w.stars)
    if (id >= 100 && w.pro.count(id) && w.pro.at(id).size() >= size_t(kEnd) && !Norm(n, false).empty())
      wn[Norm(n, false)].push_back(id);
  for (const auto &[id, n] : s.stars)
    if (s.pro.count(id) && s.pro.at(id).size() >= 64 && !Norm(n, false).empty())
      sn[Norm(n, false)].push_back(id);
  for (const auto &[n, ws] : wn)
    if (const auto it = sn.find(n); it != sn.end() && ws.size() == 1 && it->second.size() == 1 && !wtaken.count(ws[0]) &&
                                    !staken.count(it->second[0]))
      out.push_back({ws[0], it->second[0]});
  return out;
}

}  // namespace detail

// Which slot (PRO byte offset) of the other game each WWE '12 slot is, -1 for none: for the superstars both games
// have (pairs: WWE '12's number, the other's), how often the two slots' moves have the same names (without variant
// numbers); the pairs that agree for at least 15 % of them (Kaiko's threshold) and the most in all, in order.
inline std::map<int, int> SlotMap(const detail::Game &w, const detail::Game &s,
                                  const std::vector<std::pair<int, int>> &pairs) {
  using namespace detail;
  std::vector<std::pair<int, int>> shared;
  for (const auto &pr : pairs)
    if (w.pro.count(pr.first) && s.pro.count(pr.second) && w.pro.at(pr.first).size() >= size_t(kEnd))
      shared.push_back(pr);
  std::map<int, std::string> wn, sn;  // normalised names
  for (const auto &[id, n] : w.names)
    wn[id] = Norm(n, true);
  for (const auto &[id, n] : s.names)
    sn[id] = Norm(n, true);
  // (2026-11-05: a game that kept WWE '12's move numbers - WWE '13, the 2K games: most numbers both have name the same
  // move - also counts the same number as agreeing, its renamed moves too: 2K14-2K16 had 64-109 of 210 slots found)
  bool by_number = false;
  {
    int both = 0, same = 0;
    for (const auto &[id, n] : wn)
      if (const auto it = sn.find(id); it != sn.end() && !n.empty()) {
        ++both;
        same += it->second == n;
      }
    by_number = both > 200 && same * 2 > both;
  }
  size_t send = 0;
  for (const auto &pr : shared)
    send = std::max(send, s.pro.at(pr.second).size());
  const int ns = int(send / 2), nw = (kEnd - kFirst) / 2;
  std::vector<std::vector<int>> agree(size_t(ns), std::vector<int>(size_t(nw), 0));
  for (const auto &pr : shared) {
    const Bytes &a = s.pro.at(pr.second), &b = w.pro.at(pr.first);
    std::vector<const std::string *> bw(size_t(nw), nullptr);
    for (int j = 0; j < nw; ++j)
      if (const auto it = wn.find(U16(b, size_t(kFirst + 2 * j))); it != wn.end() && !it->second.empty())
        bw[size_t(j)] = &it->second;
    for (int i = 0; i < ns && size_t(2 * i + 1) < a.size(); ++i) {
      const int sv = U16(a, size_t(2 * i));
      const auto it = sn.find(sv);
      if (it == sn.end() || it->second.empty())
        continue;
      for (int j = 0; j < nw; ++j)
        if ((bw[size_t(j)] && *bw[size_t(j)] == it->second) ||
            (by_number && sv > 0 && U16(b, size_t(kFirst + 2 * j)) == sv))
          ++agree[size_t(i)][size_t(j)];
    }
  }
  const int least = std::max(3, int(shared.size()) * 15 / 100);
  std::vector<std::vector<int>> best(size_t(ns + 1), std::vector<int>(size_t(nw + 1), 0));
  for (int i = 1; i <= ns; ++i)
    for (int j = 1; j <= nw; ++j) {
      int v = std::max(best[size_t(i - 1)][size_t(j)], best[size_t(i)][size_t(j - 1)]);
      const int g = agree[size_t(i - 1)][size_t(j - 1)];
      if (g >= least)
        v = std::max(v, best[size_t(i - 1)][size_t(j - 1)] + g);
      best[size_t(i)][size_t(j)] = v;
    }
  std::map<int, int> out;
  for (int i = ns, j = nw; i > 0 && j > 0;) {
    const int g = agree[size_t(i - 1)][size_t(j - 1)];
    if (g >= least && best[size_t(i)][size_t(j)] == best[size_t(i - 1)][size_t(j - 1)] + g) {
      out[kFirst + 2 * (j - 1)] = 2 * (i - 1);
      --i;
      --j;
    } else if (best[size_t(i)][size_t(j)] == best[size_t(i - 1)][size_t(j)]) {
      --i;
    } else {
      --j;
    }
  }
  // (2026-11-05) Then the slots left out of order (the 2K games moved whole sections): a WWE '12 slot and an unused
  // slot of the other game that are each other's best match, agreeing for at least `least` superstars.
  std::set<int> used_s;
  for (const auto &[wo, so] : out)
    used_s.insert(so / 2);
  for (bool more = !g_in_order_only; more;) {
    more = false;
    for (int j = 0; j < nw; ++j) {
      if (out.count(kFirst + 2 * j))
        continue;
      int bi = -1, bg = 0;
      for (int i = 0; i < ns; ++i)
        if (!used_s.count(i) && agree[size_t(i)][size_t(j)] > bg) {
          bg = agree[size_t(i)][size_t(j)];
          bi = i;
        }
      if (bi < 0 || bg < least)
        continue;
      bool best_for_it = true;  // (no other free WWE '12 slot agrees more with that one)
      for (int k = 0; k < nw && best_for_it; ++k)
        best_for_it = k == j || out.count(kFirst + 2 * k) || agree[size_t(bi)][size_t(k)] <= bg;
      if (!best_for_it)
        continue;
      out[kFirst + 2 * j] = 2 * bi;
      used_s.insert(bi);
      more = true;
    }
  }
  return out;
}
inline std::map<int, int> SlotMap(const detail::Game &w, const detail::Game &s) {
  return SlotMap(w, s, detail::SharedStars(w, s, true));
}

// The superstar `number` of another game (src: its disc - SvR 2011, WWE '13...)'s moveset in WWE '12's terms (w12: the
// player's WWE '12).
inline bool FromGame(const disc::GameDisc &w12, const disc::GameDisc &src, int number, Result &out, std::string *why) {
  using namespace detail;
  out = {};
  Game w, s;
  if (!Load(w12, w) || !Load(src, s)) {
    if (why)
      *why = "the moves can't be read (misc.pac, chEtc.pac)";
    return false;
  }
  const auto sp = s.pro.find(number);
  if (sp == s.pro.end()) {
    if (why)
      *why = "no moveset for this superstar";
    return false;
  }
  const std::map<int, int> slots = SlotMap(w, s);
  if (slots.size() < 50) {
    if (why)
      *why = "the two games' moves don't match well enough";
    return false;
  }
  detail::Fill(w, s, sp->second, slots, true, out);
  return true;
}

namespace detail {

// The rows of a moveset `pro` of game s put into WWE '12's slots (slots: WWE '12 offset -> s's), by the moves' names
// (numbers: whether s may share WWE '12's move numbers, WWE '13 and the 2K games).
inline void Fill(const Game &w, const Game &s, const Bytes &pro, const std::map<int, int> &slots, bool numbers,
                 moveset_port::Result &out) {
  // WWE '12's roster: how many superstars put each move in each slot and section; the slots all have alike
  std::map<int, std::map<int, int>> in_slot, in_section;
  for (const auto &[id, p] : w.pro) {
    if (id < 100 || p.size() < size_t(kEnd))
      continue;
    for (int o = kFirst; o < kEnd; o += 2)
      if (const int v = U16(p, size_t(o)); v > 0) {
        ++in_slot[o][v];
        ++in_section[SectionOf(o)][v];
      }
  }
  // WWE '13 and the 2K games kept WWE '12's move numbers (a move's number is its motion's), some renamed: WWE '13's
  // "White Noise 1" is WWE '12's "Celtic Cross 1", the same motion. Such a game: the same number in the same slot
  // for many of the superstars both have (SvR 2011 numbers its moves otherwise).
  bool same_numbers = false;
  {
    long same = 0, all = 0;
    for (const auto &[id, p] : w.pro) {
      const auto q = s.pro.find(id);
      if (id < 100 || q == s.pro.end())
        continue;
      for (const auto &[wo, so] : slots)
        if (const int v = U16(p, size_t(wo)); v > 0) {
          ++all;
          same += U16(q->second, size_t(so)) == v;
        }
    }
    same_numbers = numbers && all > 0 && same * 3 > all;
  }
  std::map<std::string, std::vector<int>> exact, base, words;
  std::set<std::string> any;
  for (const auto &[id, n] : w.names)
    if (!n.empty()) {
      exact[Norm(n, false)].push_back(id);
      base[Norm(n, true)].push_back(id);
      words[WordsKey(n)].push_back(id);
      any.insert(Norm(n, true));
    }
  // A slot's kind (2026-11-05): the category bits every move the roster puts in it has (a move's record's first 16
  // bytes decide which slots it may go in, Kaiko's notes). A WWE '12 move no superstar has in that slot or section
  // may still go there when it has them all (before, only the roster's own choices were taken).
  std::map<int, Bytes> need;
  for (const auto &[o, moves] : in_slot) {
    Bytes b(16, 0xFF);
    bool got = false;
    for (const auto &[v, n] : moves)
      if (const auto c = w.cats.find(v); c != w.cats.end() && c->second.size() == 16) {
        for (size_t i = 0; i < 16; ++i)
          b[i] &= c->second[i];
        got = true;
      }
    if (got && std::any_of(b.begin(), b.end(), [](uint8_t x) { return x != 0; }))
      need[o] = b;
  }
  const auto fits = [&](int o, int v) {
    const auto n = need.find(o);
    const auto c = w.cats.find(v);
    if (n == need.end() || c == w.cats.end() || c->second.size() != 16)
      return false;
    for (size_t i = 0; i < 16; ++i)
      if ((c->second[i] & n->second[i]) != n->second[i])
        return false;
    return true;
  };
  for (int o = kFirst; o < kEnd; o += 2) {
    if (in_slot[o].size() <= 1)
      continue;  // (the same move for everyone)
    ++out.slots;
    const auto at = slots.find(o);
    if (at == slots.end())
      continue;  // (no slot of the other game for it: the base's)
    const int sv = U16(pro, size_t(at->second));
    if (sv <= 0)
      continue;
    const auto sn = s.names.find(sv);
    Row r;
    r.off = o;
    r.from = sn != s.names.end() ? sn->second : "";
    r.from_id = sv;
    auto pick = [&](const std::vector<int> &cand) {  // the one the roster uses most in this slot, else this section
      int best = -1, score = 0;
      for (int c : cand) {
        const auto a = in_slot[o].find(c);
        const auto b = in_section[SectionOf(o)].find(c);
        const int sc = a != in_slot[o].end() ? 1000 + a->second : b != in_section[SectionOf(o)].end() ? 1 + b->second
                       : fits(o, c)                                ? 1
                                                                   : 0;
        if (sc > score) {
          score = sc;
          best = c;
        }
      }
      return best;
    };
    int got = -1;
    if (!r.from.empty()) {
      if (const auto e = exact.find(Norm(r.from, false)); e != exact.end())
        got = pick(e->second);
      if (got < 0)
        if (const auto b = base.find(Norm(r.from, true)); b != base.end()) {
          got = pick(b->second);
          r.variant = got >= 0;
        }
      if (got < 0)  // (the same words in another order or spelling)
        if (const auto b = words.find(WordsKey(r.from)); b != words.end() && !b->first.empty()) {
          got = pick(b->second);
          r.variant = got >= 0;
        }
      if (got < 0) {  // (a longer or shorter name of a move the roster has in that slot: "Elbow to Back of Head")
        const auto mine = NameWords(r.from);
        int best = -1;
        double score = 0;
        for (const auto &[v, n] : in_slot[o]) {
          const auto it = w.names.find(v);
          if (it == w.names.end())
            continue;
          const auto theirs = NameWords(it->second);
          std::vector<std::string> common;
          std::set_intersection(mine.begin(), mine.end(), theirs.begin(), theirs.end(), std::back_inserter(common));
          const size_t fewer = std::min(mine.size(), theirs.size()), more_words = std::max(mine.size(), theirs.size());
          if (common.size() < 2 || common.size() != fewer || double(common.size()) / double(more_words) < 0.6)
            continue;  // (all the shorter one's words in the longer one, and most of the longer one's)
          const double sc = double(common.size()) / double(more_words) + n * 1e-6;
          if (sc > score) {
            score = sc;
            best = v;
          }
        }
        if (best >= 0) {
          got = best;
          r.variant = true;
        }
      }
      // (SvR 2010's "Extreme ..." moves are WWE '12's of the rest of the name: Extreme Twist of Fate = Twist of Fate 1,
      // 29 of their records' 32 bytes +0x70..+0x8F alike; Extreme Alley Oop = Alley Oop 2, 26 - Jeff Hardy's finisher
      // 2 and signature 2; as an added move his Twist of Fate, on a model that isn't a finisher, didn't go off in
      // game, 2026-11-09)
      if (got < 0 && r.from.rfind("Extreme ", 0) == 0)
        if (const auto b = base.find(Norm(r.from.substr(8), true)); b != base.end()) {
          got = pick(b->second);
          r.variant = got >= 0;
        }
    }
    if (got < 0 && same_numbers && w.names.count(sv)) {  // (the same move under another name)
      got = pick({sv});
      r.variant = got >= 0;
    }
    r.new_move = got < 0 && same_numbers && !w.names.count(sv) && !r.from.empty();
    if (got >= 0) {
      r.to = w.names.at(got);
      out.moves.push_back({o, got});
      ++(r.variant ? out.variant : out.exact);
    } else {
      ++out.kept;
      r.kept_why = r.from.empty() ? 1 : any.count(Norm(r.from, true)) || words.count(WordsKey(r.from)) ? 3 : 2;
    }
    out.rows.push_back(std::move(r));
  }
}

}  // namespace detail

// SvR 2007-2010's (title: its disc's title id) superstar `number`'s moveset in WWE '12's terms: their slots aren't
// found from shared superstars (their numbers aren't WWE '12's), but from Kaiko's maps through SvR 2011's
// (svr_old_slots.h).
inline bool FromOld(const disc::GameDisc &w12, const disc::GameDisc &src, uint32_t title, int number, Result &out,
                    std::string *why) {
  using namespace detail;
  out = {};
  const bool svr10 = title == 0x54510844;
  Game w, s;
  if (!Load(w12, w) || !Load(src, s, svr10 ? 160 : 144, svr10 ? 0x90 : 0x88)) {
    if (why)
      *why = "the moves can't be read (misc.pac, chEtc.pac)";
    return false;
  }
  const auto sp = s.pro.find(number);
  if (sp == s.pro.end()) {
    if (why)
      *why = "no moveset for this superstar";
    return false;
  }
  std::map<int, int> to11;  // the old game's offset by SvR 2011's
  auto put = [&](const auto &table) {
    for (const auto &[from, t11] : table)
      to11.emplace(t11, from);
  };
  switch (title) {
    case 0x545107E0: put(svr_old_slots::kSvr2007); break;
    case 0x5451080B: put(svr_old_slots::kSvr2008); break;
    case 0x54510826: put(svr_old_slots::kSvr2009); break;
    case 0x54510844: put(svr_old_slots::kSvr2010); break;
    default:
      if (why)
        *why = "not SvR 2007-2010";
      return false;
  }
  // (2026-11-05: first the slots learnt from the superstars both games have by name, as FromGame does by number:
  // Kaiko's maps through SvR 2011's cover 39-157 of WWE '12's 210 varying slots; then his for the slots left)
  std::map<int, int> slots = SlotMap(w, s, SharedStars(w, s, false));
  std::set<int> used;
  for (const auto &[o12, so] : slots)
    used.insert(so);
  for (const auto &[o12, t11] : svr_old_slots::kWwe12)
    if (const auto it = to11.find(t11); it != to11.end() && !slots.count(o12) && !used.count(it->second))
      slots[o12] = it->second;
  Fill(w, s, sp->second, slots, false, out);
  return true;
}

inline bool FromSvr(const disc::GameDisc &w12, const disc::GameDisc &svr, int number, Result &out, std::string *why) {
  return FromGame(w12, svr, number, out, why);
}

// The manifest's moves= value: "OFF:NUMBER" (offset in hex), comma separated.
inline std::string ManifestValue(const std::vector<std::pair<int, int>> &moves) {
  std::string v;
  char b[24];
  for (const auto &[o, id] : moves) {
    snprintf(b, sizeof(b), "%s%X:%d", v.empty() ? "" : ",", o, id);
    v += b;
  }
  return v;
}

}  // namespace moveset_port
