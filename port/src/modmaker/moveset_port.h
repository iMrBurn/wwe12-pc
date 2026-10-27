// A ported superstar's own moveset (docs/plan_movesets.md, step 1): its SvR 2011 moves put into WWE '12's slots by
// their names. The method is Kaiko's (SvR 2011 PC port, KaikoClanworth1: moveset_port.py, the other way round):
//  - the moveset: pac\ch\chEtc.pac CHAR / PRO, the child of the superstar's number, 16-bit move numbers big-endian
//    (WWE '12 1072 bytes, moves from +0x00A to +0x1C2; SvR 2011 1056 bytes, the same slots 4 bytes earlier);
//  - the move names: pac\misc.pac MOVS / WAZE, 160-byte records from 0x430, name at +0x10, number u16 at +0x90;
//  - a slot takes the WWE '12 move of the same name (else of the same name without its variant number,
//    "Attitude Adjustment 2" -> "Attitude Adjustment 4") that WWE '12's roster puts in that slot, else in that
//    section (a strike doesn't go to a grapple slot; such a move has its animations); else the base's move stays.
//    Slots the whole roster has alike (ring in / out...) stay the base's.
// Checked offline on the 223 superstars both games have: 78 % of the slots get their move.
#pragma once

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

namespace moveset_port {

using Bytes = std::vector<uint8_t>;

struct Row {
  int off = 0;           // the WWE '12 PRO byte offset
  std::string from, to;  // the SvR move's name; the WWE '12 move's ("" = the base's kept)
  bool variant = false;  // found without its variant number (another version of the move)
};

struct Result {
  std::vector<std::pair<int, int>> moves;  // (PRO offset, WWE '12 move number)
  std::vector<Row> rows;                   // every slot the SvR superstar has a move in
  int exact = 0, variant = 0, kept = 0;
};

namespace detail {

inline svrfmt::ArcNode *Find(svrfmt::ArcNode &n, const char *g, const char *nm) {
  for (auto &k : n.kids)
    if (k.group == g && k.name == nm)
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

inline bool Load(const disc::GameDisc &d, Game &g) {
  Bytes m, c;
  svrfmt::Archive M, C;
  if (!d.Read("pac/misc.pac", m) || !d.Read("pac/ch/chEtc.pac", c) || !svrfmt::ArchiveOpen(m, M, nullptr) ||
      !svrfmt::ArchiveOpen(c, C, nullptr))
    return false;
  svrfmt::ArcNode *w = Find(M.root, "MOVS", "WAZE"), *pro = Find(C.root, "CHAR", "PRO ");
  if (!w || !pro || !svrfmt::ArchiveExpand(*pro))
    return false;
  const Bytes d0 = svrfmt::ArchiveData(*w);
  for (size_t o = 0x430; o + 160 <= d0.size(); o += 160)
    g.names[d0[o + 0x90] << 8 | d0[o + 0x91]] =
        std::string(reinterpret_cast<const char *>(&d0[o + 0x10]), strnlen(reinterpret_cast<const char *>(&d0[o + 0x10]), 0x40));
  for (auto &k : pro->kids)
    g.pro[int(k.id)] = svrfmt::ArchiveData(k);
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
constexpr int kFirst = 0x00A, kEnd = 0x1C2, kSvrShift = 4;

inline int SectionOf(int off) {
  for (int i = 0; i < int(sizeof(kSections) / sizeof(kSections[0])); ++i)
    if (off >= kSections[i][0] && off < kSections[i][1])
      return i;
  return -1;
}

}  // namespace detail

// The SvR 2011 superstar `number`'s moveset (svr: its disc) in WWE '12's terms (w12: the player's WWE '12).
inline bool FromSvr(const disc::GameDisc &w12, const disc::GameDisc &svr, int number, Result &out, std::string *why) {
  using namespace detail;
  out = {};
  Game w, s;
  if (!Load(w12, w) || !Load(svr, s)) {
    if (why)
      *why = "the moves can't be read (misc.pac, chEtc.pac)";
    return false;
  }
  const auto sp = s.pro.find(number);
  if (sp == s.pro.end() || sp->second.size() < size_t(kEnd - kSvrShift)) {
    if (why)
      *why = "no moveset for this superstar";
    return false;
  }
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
  std::map<std::string, std::vector<int>> exact, base;
  for (const auto &[id, n] : w.names)
    if (!n.empty()) {
      exact[Norm(n, false)].push_back(id);
      base[Norm(n, true)].push_back(id);
    }
  for (int o = kFirst; o < kEnd; o += 2) {
    if (in_slot[o].size() <= 1)
      continue;  // (the same move for everyone)
    const int sv = U16(sp->second, size_t(o - kSvrShift));
    if (sv <= 0)
      continue;
    const auto sn = s.names.find(sv);
    Row r;
    r.off = o;
    r.from = sn != s.names.end() ? sn->second : "";
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
    if (got >= 0) {
      r.to = w.names[got];
      out.moves.push_back({o, got});
      ++(r.variant ? out.variant : out.exact);
    } else {
      ++out.kept;
    }
    out.rows.push_back(std::move(r));
  }
  return true;
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
