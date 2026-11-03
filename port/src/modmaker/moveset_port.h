// A ported superstar's own moveset (docs/plan_movesets.md, step 1): its moves in its game (SvR 2011, WWE '13...) put
// into WWE '12's slots by their names. The method is Kaiko's (SvR 2011 PC port, KaikoClanworth1: moveset_port.py and
// moveset_maps.py, the other way round):
//  - the moveset: pac\ch\chEtc.pac CHAR / PRO, the child of the superstar's number, 16-bit move numbers big-endian
//    (WWE '12 1072 bytes, moves from +0x00A to +0x1C2; SvR 2011 1056 bytes; WWE '13 1180 bytes);
//  - the move names: pac\misc.pac MOVS / WAZE, 160-byte records, name at +0x10, number u16 at +0x90 (where they start
//    differs: WWE '12 and SvR 2011 0x430, WWE '13 elsewhere: found from the records that look like moves);
//  - the slots: which slot of the other game is which of WWE '12's, from the superstars both games have (the same
//    numbers): the slot pairs whose moves have the same names most often, in order (SvR 2011: WWE '12's 4 bytes on);
//  - a slot takes the WWE '12 move of the same name (else of the same name without its variant number,
//    "Attitude Adjustment 2" -> "Attitude Adjustment 4") that WWE '12's roster puts in that slot, else in that
//    section (a strike doesn't go to a grapple slot; such a move has its animations); else the base's move stays.
//    Slots the whole roster has alike (ring in / out...) stay the base's.
// Checked offline on the 223 superstars both games have: 78 % of the slots get their move.
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "game_disc.h"
#include "svrfmt/archive.h"
#include "svrfmt/pac.h"
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
};

struct Result {
  std::vector<std::pair<int, int>> moves;  // (PRO offset, WWE '12 move number)
  std::vector<Row> rows;                   // every slot the SvR superstar has a move in
  int exact = 0, variant = 0, kept = 0;
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
};

// (rec / num: the move records' size and where their number is: 160 / 0x90, SvR 2007-2009 144 / 0x88. A number is read
// big-endian, as the PRO's moves: SvR 2007-2011 store both little-endian, the same swap on both keeps them matching.)
inline bool Load(const disc::GameDisc &d, Game &g, size_t rec = 160, size_t num = 0x90) {
  Bytes m[2], c;
  svrfmt::Archive M[2], C;
  const bool etc = d.Read("pac/ch/chEtc.pac", c) && svrfmt::ArchiveOpen(c, C, nullptr);
  // (the 2K games keep their move names in createmode.pac, Kaiko's moves_port.py Game14)
  svrfmt::ArcNode *w = nullptr;
  const char *files[2] = {"pac/misc.pac", "pac/createmode.pac"};
  for (int i = 0; i < 2 && !w; ++i)
    if (d.Read(files[i], m[i]) && svrfmt::ArchiveOpen(m[i], M[i], nullptr))
      w = Find(M[i].root, "MOVS", "WAZE");
  // (SvR 2007 / 2008 keep their profiles in misc.pac, Kaiko's superstar_port.py Game08 / Game07: no chEtc.pac)
  svrfmt::ArcNode *pro = etc ? Find(C.root, "CHAR", "PRO ") : Find(M[0].root, "CHAR", "PRO ");
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
    if (looks(o))
      g.names.emplace(d0[o + num] << 8 | d0[o + num + 1], std::string(reinterpret_cast<const char *>(&d0[o + 0x10])));
  if (svrfmt::ArchiveExpand(*pro) && !pro->kids.empty()) {
    for (auto &k : pro->kids)
      g.pro[int(k.id)] = svrfmt::ArchiveData(k);
  } else {  // (the 2K games: a PACH the archive doesn't open)
    std::vector<svrfmt::PachEntry> kids;
    if (svrfmt::PachRead(svrfmt::Unpack(svrfmt::ArchiveData(*pro)), kids))
      for (auto &k : kids)
        g.pro[int(k.id)] = svrfmt::Unpack(k.data);
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

// Which slot (PRO byte offset) of the other game each WWE '12 slot is, -1 for none: for the superstars both games
// have, how often the two slots' moves have the same names (without variant numbers); the pairs that agree for at
// least 15 % of them (Kaiko's threshold) and the most in all, in order.
inline std::map<int, int> SlotMap(const detail::Game &w, const detail::Game &s) {
  using namespace detail;
  std::vector<int> shared;
  for (const auto &[id, p] : w.pro)
    if (id >= 100 && s.pro.count(id) && p.size() >= size_t(kEnd))
      shared.push_back(id);
  std::map<int, std::string> wn, sn;  // normalised names
  for (const auto &[id, n] : w.names)
    wn[id] = Norm(n, true);
  for (const auto &[id, n] : s.names)
    sn[id] = Norm(n, true);
  size_t send = 0;
  for (int id : shared)
    send = std::max(send, s.pro.at(id).size());
  const int ns = int(send / 2), nw = (kEnd - kFirst) / 2;
  std::vector<std::vector<int>> agree(size_t(ns), std::vector<int>(size_t(nw), 0));
  for (int id : shared) {
    const Bytes &a = s.pro.at(id), &b = w.pro.at(id);
    std::vector<const std::string *> bw(size_t(nw), nullptr);
    for (int j = 0; j < nw; ++j)
      if (const auto it = wn.find(U16(b, size_t(kFirst + 2 * j))); it != wn.end() && !it->second.empty())
        bw[size_t(j)] = &it->second;
    for (int i = 0; i < ns && size_t(2 * i + 1) < a.size(); ++i) {
      const auto it = sn.find(U16(a, size_t(2 * i)));
      if (it == sn.end() || it->second.empty())
        continue;
      for (int j = 0; j < nw; ++j)
        if (bw[size_t(j)] && *bw[size_t(j)] == it->second)
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
  return out;
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
  std::map<std::string, std::vector<int>> exact, base;
  for (const auto &[id, n] : w.names)
    if (!n.empty()) {
      exact[Norm(n, false)].push_back(id);
      base[Norm(n, true)].push_back(id);
    }
  for (int o = kFirst; o < kEnd; o += 2) {
    if (in_slot[o].size() <= 1)
      continue;  // (the same move for everyone)
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
        const int sc = a != in_slot[o].end() ? 1000 + a->second : b != in_section[SectionOf(o)].end() ? b->second : 0;
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
  std::map<int, int> slots;
  for (const auto &[o12, t11] : svr_old_slots::kWwe12)
    if (const auto it = to11.find(t11); it != to11.end())
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
