// Mod Maker: the moves of WWE '13 and the 2K games that WWE '12 hasn't, rebuilt on a WWE '12 "model" of their kind
// (1.3, 2026-11-10), as SvR's are (svr_moves.h) and as Kaiko's moves_port.py does: the move keeps its own animations
// (each person, each segment), and everything the game needs to play it comes from a WWE '12 move of the same kind -
// its record (WAZE), its in-motion commands (YAT2: grabs, impacts, the end of the move; retimed to the new length,
// through the move's own impact and finisher moments), its motion parameters (m3.pac rows, retimed), its preview script
// and Create a Moveset rows; its motion number goes in the model's hundred (newmoves13::Renumber).
//
// Why: a 2K14 Hulk Hogan's added moves kept 2K14's own commands and parameters (converted); his finisher (Running Leg
// Drop) and some others froze both wrestlers through the move in game, then they moved again (2026-11-10). Testing every
// move of every superstar can't be asked of anyone: a move with no sure model is not added (its slot keeps the base's).
//
// A model: a WWE '12 move with the same kind (WAZE +0x72-0x73, e.g. 0003 running at a downed opponent, 0010 a running
// strike, 000A the corner and top rope), the same people (y 0 / 1 / 2) and segments (x < 90), a length within half and
// twice the move's, then preferably one WWE '12's rosters put in the same moveset slot, then the closest length.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "game_disc.h"
#include "m3_params.h"
#include "moves_edit.h"
#include "moveset_port.h"
#include "newmoves13.h"
#include "svr_moves.h"
#include "yanm.h"

namespace model_moves {

using svrfmt::Bytes;

namespace detail {

using svr_moves::detail::Be16At;
using svr_moves::detail::Be32At;

// A record's blocks by name: their table index, the YANM + POF0 bytes and the rest (commands...), and their frames.
struct Blocks {
  struct One {
    size_t index = 0;
    Bytes anim, tail;
    int frames = 0;
  };
  std::map<std::string, One> by_name;
  std::set<int> segments;  // x < 90 of person 0
  int people = 0;          // 1, 2 or 3
  int frames0 = 0;         // the attacker's first segment
};

inline bool ReadBlocks(const Bytes &d, Blocks &out) {
  out = {};
  yanm::Record rec;
  if (d.size() < 0x70 || !yanm::Read(d, rec))
    return false;
  std::map<std::string, int> frames;
  for (const auto &b : rec.blocks)
    frames[b.name] = b.frames;
  const uint32_t first = Be32At(d, 12), n = Be32At(d, 0x48);
  if (n == 0 || n > 64 || 0x70 + 0x28 * size_t(n) > d.size())
    return false;
  std::vector<size_t> at;
  for (uint32_t i = 0; i < n; ++i)
    at.push_back(first + Be32At(d, 0x70 + 0x28 * i + 0x24));
  for (uint32_t i = 0; i < n; ++i) {
    const size_t s = at[i], e = i + 1 < n ? at[i + 1] : d.size();
    if (s > e || e > d.size())
      return false;
    const std::string name(reinterpret_cast<const char *>(&d[0x70 + 0x28 * i + 4]), 8);
    Blocks::One one;
    one.index = i;
    size_t o = s;
    for (const char *tag : {"YANM", "POF0"})
      if (o + 8 <= e && !memcmp(&d[o], tag, 4))
        o += 8 + Be32At(d, o + 4);
    if (o > e)
      return false;
    one.anim.assign(d.begin() + long(s), d.begin() + long(o));
    one.tail.assign(d.begin() + long(o), d.begin() + long(e));
    one.frames = frames.count(name) ? frames[name] : 0;
    const int bx = atoi(name.substr(0, 4).c_str()), by = atoi(name.substr(4).c_str());
    if (by >= 0 && by <= 2 && bx < 90) {
      out.people = std::max(out.people, by + 1);
      if (by == 0)
        out.segments.insert(bx);
    }
    out.by_name[name] = std::move(one);
  }
  if (out.by_name.count("00000000"))
    out.frames0 = out.by_name["00000000"].frames;
  return out.frames0 > 0;
}

}  // namespace detail

// What happened to an added move (for logs and the checker).
struct Note {
  int number = 0;
  std::string name, model, why;  // why: empty when made
  int model_motion = 0;           // its model's motion number
};

// `moves` (newmoves13::Collect's, their own records and motions in WWE '12's formats) rebuilt on WWE '12 models; those
// without one are taken out. `slot_of`: move number -> the moveset slot it goes in (-1: unknown).
inline void OnModels(const disc::GameDisc &w12, std::vector<newmoves13::Move> &moves, const std::map<int, int> &slot_of,
                     std::vector<Note> *notes = nullptr) {
  using namespace detail;
  if (moves.empty())
    return;
  moveset_port::detail::Game g12;
  moveset_port::detail::Load(w12, g12);
  const auto recs12 = newmoves13::detail::Records(w12);
  moves_edit::MotionIndex mi;
  mi.Build(w12);
  Bytes m3;
  w12.Read("pac/m3.pac", m3);
  std::set<int> with_rows;  // (motions WWE '12 has m3 rows for: a model needs them)
  {
    m3_params::detail::Prm prm;
    if (!m3.empty() && prm.Open(m3)) {
      const Bytes t = prm.Get("0/3");
      for (size_t o = 0; o + 8 <= t.size(); o += 8)
        with_rows.insert(t[o] | t[o + 1] << 8);
    }
  }
  std::map<int, std::set<int>> in_slot;  // slot -> WWE '12 move numbers its rosters put there
  for (const auto &[id, p] : g12.pro)
    for (size_t o = 0x0A; o + 2 <= p.size() && o < 0x1C4; o += 2)
      if (const int v = moveset_port::detail::U16(p, o); v > 0)
        in_slot[int(o)].insert(v);
  std::map<int, Blocks> cache;  // model number -> its blocks
  auto blocks_of = [&](int num) -> const Blocks * {
    auto it = cache.find(num);
    if (it == cache.end()) {
      Blocks b;
      const Bytes full = mi.Full(w12, num);
      if (full.empty() || !ReadBlocks(full, b))
        b.frames0 = 0;
      it = cache.emplace(num, std::move(b)).first;
    }
    return it->second.frames0 > 0 ? &it->second : nullptr;
  };
  std::vector<newmoves13::Move> out;
  for (auto &mv : moves) {
    Note note;
    note.number = mv.number;
    note.name = mv.name;
    auto fail = [&](const std::string &why) {
      note.why = why;
      if (notes)
        notes->push_back(note);
    };
    Blocks mine;
    const Bytes high = svrfmt::IsBpe(mv.high) ? svrfmt::Unpack(mv.high) : mv.high;
    if (mv.waze.size() < 160 || !ReadBlocks(high, mine)) {
      fail("its motion couldn't be read");
      continue;
    }
    // the model
    const auto slot = slot_of.find(mv.number);
    const std::set<int> *slot_moves = slot != slot_of.end() && in_slot.count(slot->second) ? &in_slot[slot->second] : nullptr;
    int best = -1;
    long best_score = 0;
    for (const auto &[num, r] : recs12) {
      if (r.size() < 160 || r[0x72] != mv.waze[0x72] || r[0x73] != mv.waze[0x73] || !r[0x10] || !g12.names.count(num))
        continue;
      if (!with_rows.count(newmoves13::MotionOf(num)))
        continue;
      const Blocks *b = blocks_of(num);
      if (!b || b->people != mine.people || b->segments != mine.segments || 2 * b->frames0 < mine.frames0 ||
          b->frames0 > 2 * mine.frames0)
        continue;
      bool all = true;  // (every block of the model has the move's own)
      for (const auto &[name, one] : b->by_name) {
        const int bx = atoi(name.substr(0, 4).c_str()), by = atoi(name.substr(4).c_str());
        if (bx < 90 && by >= 0 && by <= 2 && !mine.by_name.count(name))
          all = false;
      }
      if (!all)
        continue;
      const long score = (slot_moves && slot_moves->count(num) ? 0 : 10000) + std::abs(b->frames0 - mine.frames0);
      if (best < 0 || score < best_score)
        best = num, best_score = score;
    }
    if (best < 0) {
      fail("no WWE '12 model of its kind");
      continue;
    }
    const Blocks &md = *blocks_of(best);
    const int tmotion = newmoves13::MotionOf(best), motion = newmoves13::MotionOf(mv.number);
    note.model = g12.names.at(best);
    note.model_motion = tmotion;
    // its animations in the model's record, the model's commands retimed (through its own impact / finisher moments)
    std::map<size_t, std::pair<Bytes, int>> repl;
    std::map<size_t, int> tf;
    std::map<size_t, std::map<int, int>> own;
    for (const auto &[name, one] : md.by_name) {
      tf[one.index] = one.frames;
      const int bx = atoi(name.substr(0, 4).c_str()), by = atoi(name.substr(4).c_str());
      if (bx >= 90 || by < 0 || by > 2)
        continue;  // (the counter branches' stubs stay the model's)
      const auto &m = mine.by_name.at(name);
      repl[one.index] = {m.anim, m.frames};
      for (const auto &[id, f] : svr_moves::detail::CmdFrames(m.tail))
        if (id == 73 || id == 172 || id == 173)
          own[one.index][id] = f;
    }
    const Bytes full = mi.Full(w12, best);
    newmoves13::Move made = mv;
    made.high = svr_moves::detail::Rebuild(full, repl, tf, motion, own);
    made.low = made.high;
    // its record: the model's, its own name and number
    made.waze = recs12.at(best);
    memcpy(&made.waze[0x10], &mv.waze[0x10], 0x40);
    made.waze[0x90] = mv.waze[0x90], made.waze[0x91] = mv.waze[0x91];
    // its parameters: the model's, retimed
    const double k = double(mine.frames0) / double(md.frames0);
    auto rows = m3_params::Extract(m3, tmotion, k);
    if (rows.empty()) {
      fail("its model has no motion parameters");
      continue;
    }
    svr_moves::SetImpact(made.high, rows);
    svr_moves::SetDown(made.high, rows);
    made.m3 = m3_params::Write(rows);
    // the model's preview script and Create a Moveset rows (its motion number, its name, its quit frame scaled)
    const Bytes script = svr_moves::detail::Mvmot(w12, tmotion);
    std::string s(script.begin(), script.end()), o;
    for (size_t at = 0; at < s.size();) {
      size_t end = s.find('\n', at);
      end = end == std::string::npos ? s.size() : end + 1;
      std::string line = s.substr(at, end - at);
      at = end;
      const std::string from = "motn," + std::to_string(tmotion) + ",";
      for (size_t p = line.find(from); p != std::string::npos; p = line.find(from))
        line.replace(p, from.size(), "motn," + std::to_string(motion) + ",");
      if (const size_t q = line.find(",0,quit,"); q != std::string::npos)
        line = std::to_string(int(std::lround(atoi(line.c_str()) * k))) + line.substr(q);
      o += line;
    }
    made.mvmot.assign(o.begin(), o.end());
    made.magg = svr_moves::detail::Magg(w12, tmotion);
    for (size_t at : {size_t(0), size_t(104)})
      if (made.magg.size() >= at + 104) {
        made.magg[at] = uint8_t(motion), made.magg[at + 1] = uint8_t(motion >> 8);
        memset(&made.magg[at + 8], 0, 104 - 8);
        memcpy(&made.magg[at + 8], made.name.c_str(), std::min<size_t>(made.name.size(), 104 - 9));
      }
    out.push_back(std::move(made));
    if (notes)
      notes->push_back(note);
  }
  moves.swap(out);
}

// The hundreds the moves' models are in (motion / 100), by move number: newmoves13::Renumber puts each move there.
inline std::map<int, std::map<int, int>> ModelHundreds(const std::vector<Note> &notes) {
  std::map<int, std::map<int, int>> out;
  for (const auto &n : notes)
    if (n.why.empty() && n.model_motion)
      out[n.number][n.model_motion / 100] = 1;
  return out;
}

}  // namespace model_moves
