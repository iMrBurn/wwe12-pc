// Moves WWE '13 has and WWE '12 hasn't, for a ported superstar (docs/plan_movesets.md, step 3): WWE '13 kept WWE '12's
// move numbering and its formats, so such a move is its WAZE record and its motions, added as WWE '12's own DLC packs
// add moves (Booker T's Scissors kick, pack 4):
//  - the record: misc.pac MOVS/WAZE, 160 bytes, the same layout (1781 shared moves: +0x70..+0x97 equal at 97-100 %);
//    after the number (+0x98..) WWE '12 has FF;
//  - the motions: the match's move banks m.pac ZMOT/CAnn (WWE '13: pac\mot00.pac, mot01.pac ZMOT/CA01..CA08), a PACH
//    tree: a top id, then children in pairs (an even one with the light records, the odd one with the full records
//    of the same motions), each a PACH of motions by number; a move's motion number is its number as WAZE +0x90
//    holds it read little-endian (moveset_port reads it big-endian: the bytes swapped).
// The game adds them to the added superstars' pack (superstar_pack.h: a misc.pac copy with the records, m_dlcNN.pac
// with ZMOT/CA(NN + 6) as the game's packs 3 and 4 have CA09 and CA10).
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "game_disc.h"
#include "svrfmt/archive.h"
#include "svrfmt/pac.h"
#include "svrfmt/xcompress.h"
#include "m3_params.h"
#include "yanm.h"
#include "yanm_ik.h"

namespace newmoves13 {

using Bytes = std::vector<uint8_t>;

struct Move {
  int number = 0;     // as PRO and moveset_port hold it
  std::string name;
  Bytes waze;         // its 160-byte record, made WWE '12's
  Bytes low, high;    // its motion's light and full records
  Bytes magg;         // its DLC.pac RGRP/MAGG records (children 10 and 11, 104 bytes each), else empty
  Bytes m3;           // its own motion parameters from its game's m3.pac, made WWE '12's (m3_params row set)
  Bytes mvmot;        // its moveset editor preview script (its game's pac/menu/mvMot.pac), else empty
};

inline int MotionOf(int number) { return ((number & 0xFF) << 8) | (number >> 8); }

namespace detail {

inline std::string Trim(std::string s) {
  while (!s.empty() && (s.back() == ' ' || s.back() == '\0'))
    s.pop_back();
  return s;
}

// WWE '13's WAZE records by number (moveset_port's reading), found where its grid starts.
inline std::map<int, Bytes> Records(const disc::GameDisc &d) {
  std::map<int, Bytes> out;
  // (WWE '13: misc.pac; the 2K games keep them in createmode.pac, as moveset_port reads them)
  for (const char *file : {"pac/misc.pac", "pac/createmode.pac"}) {
  Bytes m;
  svrfmt::Archive a;
  if (!d.Read(file, m))
    continue;
  if (svrfmt::IsXcompress(m)) {  // (2K17: compressed whole)
    Bytes o;
    if (!svrfmt::XcompressDecode(m, o))
      continue;
    m.swap(o);
  }
  if (!svrfmt::ArchiveOpen(m, a, nullptr))
    continue;
  // (2K17: an HSPC, names hashed: the entry whose records look most like moves, as moveset_port::detail::Load)
  svrfmt::ArcNode *hashed = nullptr;
  if (a.root.kind == svrfmt::ArcKind::Hspc) {
    size_t most = 500;
    for (auto &k : a.root.kids) {
      const Bytes t = svrfmt::Unpack(svrfmt::ArchiveData(k));
      size_t sc = 0;
      for (size_t ph = 0; ph < 160; ph += 2) {  // (wherever its grid starts)
        size_t n = 0;
        for (size_t o = ph; o + 160 <= t.size(); o += 160)
          n += isalnum(t[o + 0x10]) && isalnum(t[o + 0x11]) && (t[o + 0x90] | t[o + 0x91]);
        sc = std::max(sc, n);
      }
      if (sc > most) {
        most = sc;
        hashed = &k;
      }
    }
  }
  for (auto &k : a.root.kids) {
    if (&k != hashed && (Trim(k.group) != "MOVS" || Trim(k.name) != "WAZE"))
      continue;
    const Bytes t = svrfmt::Unpack(svrfmt::ArchiveData(k));
    auto looks = [&](size_t o) { return o + 160 <= t.size() && isalnum(t[o + 0x10]) && (t[o + 0x90] | t[o + 0x91]); };
    size_t phase = 0;
    int best = -1;
    for (size_t ph = 0; ph < 160; ph += 2) {
      int sc = 0;
      for (size_t o = ph; o + 160 <= t.size(); o += 160)
        sc += looks(o);
      if (sc > best) {
        best = sc;
        phase = ph;
      }
    }
    for (size_t o = phase; o + 160 <= t.size(); o += 160)
      if (looks(o))
        out.emplace(t[o + 0x90] << 8 | t[o + 0x91], Bytes(t.begin() + long(o), t.begin() + long(o) + 160));
  }
  }
  return out;
}

// Every track of a motion record uses kinds of keys WWE '12's own motions use (bits of the track flags: 0x0001, 0x0002,
// 0x0020-0x0080 never; 2026-11-08 over m.pac's 50 503 records).
// A preview script made WWE '12's: 2K14's "0,0,motd,<motion>,0,0,0," (both people's motion in one line) is two lines
// in WWE '12's, "0,0,motn,<motion>,0,0,0," and "0,1,motn,<motion>,0,1,0," (WWE '12 doesn't know motd: Cesaro 2K14's
// previews showed both people standing in each other, 2026-11-08)
inline Bytes PreviewScript(const Bytes &in) {
  std::string s(in.begin(), in.end()), out;
  for (size_t at = 0; at < s.size();) {
    size_t end = s.find('\n', at);
    end = end == std::string::npos ? s.size() : end + 1;
    std::string line = s.substr(at, end - at);
    at = end;
    const size_t p = line.find(",motd,");
    if (p == std::string::npos) {
      out += line;
      continue;
    }
    const std::string eol = line.find('\r') != std::string::npos ? "\r\n" : "\n";
    const size_t q = line.find(',', p + 6);
    const std::string motion = line.substr(p + 6, q == std::string::npos ? std::string::npos : q - p - 6);
    out += line.substr(0, p) + ",motn," + motion + ",0,0,0," + eol;
    out += "0,1,motn," + motion + ",0,1,0," + eol;
  }
  return Bytes(out.begin(), out.end());
}

inline bool KnownTracks(const Bytes &d) {
  yanm::Record r;
  if (!yanm::Read(d, r))
    return false;
  for (const auto &b : r.blocks)
    for (const auto &t : b.tracks)
      if (t.flags & 0x00E3)
        return false;
  return true;
}

}  // namespace detail

// The moves `numbers` (moveset_port's numbers, WWE '12 hasn't them) from the player's WWE '13: their records and
// motions; those without both are left out.
inline std::vector<Move> Collect(const disc::GameDisc &w13, const std::set<int> &numbers) {
  std::vector<Move> out;
  if (numbers.empty())
    return out;
  const auto records = detail::Records(w13);
  std::map<int, Move> found;  // by motion number
  for (int n : numbers) {
    const auto r = records.find(n);
    if (r == records.end())
      continue;
    Move m;
    m.number = n;
    m.waze = r->second;
    // (after its number WWE '12 has FF: from +0x92. WWE '13 had them from +0x98 already; 2K15 keeps other values in
    // +0x92..+0x9F, its records otherwise WWE '12's, 82 % of the bytes equal on the moves both have, 2026-11-08)
    for (size_t i = 0x92; i < 0xA0; ++i)
      m.waze[i] = 0xFF;
    m.name = std::string(reinterpret_cast<const char *>(&m.waze[0x10]), strnlen(reinterpret_cast<const char *>(&m.waze[0x10]), 0x40));
    found[MotionOf(n)] = std::move(m);
  }
  // A tree of motions: a PACH (or SHDC) whose leaves are motion records, numbered by motion. Each motion is there twice,
  // its light and its full record: the smaller, the larger.
  struct W {
    static void Leaf(const Bytes &d, int id, std::map<int, Move> &found) {
      const auto f = found.find(id);
      if (f == found.end() || !yanm::IsRecord(d) || !f->second.high.empty())
        return;
      if (f->second.low.empty())
        f->second.low = d;
      else if (d.size() != f->second.low.size()) {
        f->second.high = d;
        if (f->second.high.size() < f->second.low.size())
          std::swap(f->second.high, f->second.low);
      }
    }
    static void Walk(const Bytes &d, int id, std::map<int, Move> &found, int depth) {
      if (svrfmt::IsPach(d)) {
        std::vector<svrfmt::PachEntry> kids;
        if (depth < 6 && svrfmt::PachRead(d, kids))
          for (const auto &k : kids)
            Walk(svrfmt::Unpack(k.data), int(k.id), found, depth + 1);
        return;
      }
      Leaf(d, id, found);
    }
    static void Node(svrfmt::ArcNode &n, std::map<int, Move> &found, int depth) {  // (an HSPC's: 2K17)
      for (auto &k : n.kids)
        if (depth < 6 && svrfmt::ArchiveExpand(k) && !k.kids.empty())
          Node(k, found, depth + 1);
        else
          Walk(svrfmt::Unpack(svrfmt::ArchiveData(k)), int(k.id), found, depth + 1);
    }
  };
  // (WWE '13: pac\mot00.pac, mot01.pac; 2K14-2K16: mot00-mot09.pac; 2K17: pac\ingame\motion\all_mot*.pac; WWE '12's own:
  // m.pac)
  std::vector<std::string> files;
  for (const auto &f : w13.List("pac"))
    if (f.size() == 9 && _strnicmp(f.c_str(), "mot", 3) == 0 && _stricmp(f.c_str() + 5, ".pac") == 0)
      files.push_back("pac/" + f);
  for (const auto &f : w13.List("pac/ingame/motion"))
    if (f.size() > 4 && _stricmp(f.c_str() + f.size() - 4, ".pac") == 0)
      files.push_back("pac/ingame/motion/" + f);
  std::sort(files.begin(), files.end());
  files.push_back("pac/m.pac");
  bool one_record = false;  // (2K17: each motion once, its full record only)
  for (const auto &file : files) {
    Bytes head, whole;  // (whole: a file compressed as a whole, 2K16's mot*.pac and 2K17's (Xcompress), unpacked once)
    uint64_t size = w13.SizeOf(file);
    std::vector<svrfmt::EpacRow> rows;
    if (!size || !w13.ReadPart(file, 0, std::min<uint64_t>(size, 0x8000), head))
      continue;
    if (svrfmt::IsXcompress(head)) {
      Bytes packed;
      if (!w13.Read(file, packed) || !svrfmt::XcompressDecode(packed, whole))
        continue;
      size = whole.size();
      head.assign(whole.begin(), whole.begin() + long(std::min<size_t>(whole.size(), 0x8000)));
    }
    if (!svrfmt::EpacTable(head, size, rows)) {
      // (2K17: an HSPC of SHDCs, names hashed: every motion in it walked)
      if (head.size() < 4 || memcmp(head.data(), "HSPC", 4) != 0)
        continue;
      if (whole.empty() && !w13.Read(file, whole))
        continue;
      svrfmt::Archive a;
      if (svrfmt::ArchiveOpen(whole, a, nullptr))
        W::Node(a.root, found, 0);
      one_record = true;
      continue;
    }
    // (every motion group: WWE '13's ZMOT/CAnn, 2K14's ZMVM/Mnnn, Ennn and ZMBS too - its moves' motions are there,
    // numbered as WWE '12's: a player's 2K14 Antonio Cesaro, 11 of his 12 moves WWE '12 hasn't were outside ZMOT,
    // 2026-11-08)
    for (const auto &row : rows) {
      if (row.group.empty() || row.group[0] != 'Z')
        continue;
      Bytes raw;
      if (!whole.empty()) {
        if (row.offset + row.size > whole.size())
          continue;
        raw.assign(whole.begin() + long(row.offset), whole.begin() + long(row.offset + row.size));
      } else if (!w13.ReadPart(file, row.offset, row.size, raw)) {
        continue;
      }
      W::Walk(svrfmt::Unpack(raw), 0, found, 0);
    }
  }
  // their RGRP/MAGG records (WWE '13's DLC.pac: children 10 and 11, 104 bytes by motion number; 2K17's DLC.pac is
  // Xcompress, an HSPC whose first SHDC has the same children 10 and 11, 1418 records)
  {
    Bytes dlc, plain;
    svrfmt::Archive a;
    std::map<int, Bytes> rec[2];
    auto take = [&](uint32_t id, const Bytes &d) {
      if ((id == 10 || id == 11) && d.size() >= 104)
        for (size_t o = 0; o + 104 <= d.size(); o += 104)
        {
          Bytes r(d.begin() + long(o), d.begin() + long(o) + 104);
          // (2K14's layout: {motion, kind, flag, flag, 0, 0, 0}; WWE '12's: {motion, kind, 0, flag, 0, flag, 0} - copied as
          // it was, the game read no valid kind and listed a 2K14 Cesaro's moves nowhere in Create a Moveset; learned
          // on the 793 rows both games have, kinds the same at 99 %, 2026-11-08)
          if (r[6] == 0 && r[7] == 0 && r[5] == 0 && (r[3] || r[4])) {
            const uint8_t f3 = r[3], f4 = r[4];
            r[3] = 0, r[4] = f3, r[5] = 0, r[6] = f4, r[7] = 0;
          }
          rec[id == 11][d[o] | d[o + 1] << 8] = std::move(r);
        }
    };
    if (w13.Read("pac/DLC.pac", dlc) && svrfmt::IsXcompress(dlc) && svrfmt::XcompressDecode(dlc, plain))
      dlc.swap(plain);
    if (!dlc.empty() && svrfmt::ArchiveOpen(dlc, a, nullptr))
      for (auto &k : a.root.kids) {
        std::vector<svrfmt::PachEntry> kids;
        if (detail::Trim(k.group) == "RGRP" && svrfmt::PachRead(svrfmt::Unpack(svrfmt::ArchiveData(k)), kids)) {
          for (const auto &kid : kids)
            take(kid.id, svrfmt::Unpack(kid.data));
        } else if (k.group.empty() && rec[0].empty() && svrfmt::ArchiveExpand(k)) {
          for (auto &kid : k.kids)
            take(kid.id, svrfmt::Unpack(svrfmt::ArchiveData(kid)));
        }
      }
    for (auto &[motion, m] : found)
      if (rec[0].count(motion) && rec[1].count(motion)) {
        m.magg = rec[0][motion];
        m.magg.insert(m.magg.end(), rec[1][motion].begin(), rec[1][motion].end());
      }
  }
  // 2K17 keeps one record by motion (its full one: Super German Suplex, 517 frames, 24 tracks a person, as 2K16's full
  // record; 2K16's light one has 20 tracks and few keys): it serves as both. Its tracks may use a kind WWE '12's 50 503
  // motions never use (flag 0x0001, 280 tracks of 2K17's 7258 motions): those motions are left out (2026-11-08).
  if (one_record)
    for (auto &[motion, m] : found)
      if (!m.low.empty() && m.high.empty() && detail::KnownTracks(m.low))
        m.high = m.low;
  // their own data: the motion's rows of the game's m3.pac (WWE '13 / 2K14: m3_params::FromOther; without them the
  // game doesn't play an added move) and its preview script in pac/menu/mvMot.pac (2026-11-08)
  {
    Bytes m3, plain, mv;
    if (w13.Read("pac/m3.pac", m3) && svrfmt::IsXcompress(m3) && svrfmt::XcompressDecode(m3, plain))
      m3.swap(plain);
    std::vector<svrfmt::PachEntry> scripts;
    svrfmt::Archive ma;
    if (w13.Read("pac/menu/mvMot.pac", mv) && svrfmt::IsXcompress(mv) && svrfmt::XcompressDecode(mv, plain))
      mv.swap(plain);
    if (!mv.empty() && svrfmt::ArchiveOpen(mv, ma, nullptr) && !ma.root.kids.empty())
      svrfmt::PachRead(svrfmt::Unpack(svrfmt::ArchiveData(ma.root.kids[0])), scripts);
    for (auto &[motion, m] : found) {
      if (m.low.empty() || m.high.empty())
        continue;
      // (legs driven by IK from WWE '13 on - the calves' ankle targets - made plain rotations WWE '12 plays: a 2K14
      // Suplex 3 played with still feet, 2026-11-08; yanm_ik.h, as the WWE '13 entrances)
      yanm::ik::BakeLegIk(m.low);
      yanm::ik::BakeLegIk(m.high);
      const auto rows = m3.empty() ? m3_params::Rows{} : m3_params::FromOther(m3, motion);
      if (!rows.empty())
        m.m3 = m3_params::Write(rows);
      for (const auto &e : scripts)
        if (int(e.id) == motion) {
          std::vector<svrfmt::PachEntry> kids;
          if (svrfmt::PachRead(svrfmt::Unpack(e.data), kids) && !kids.empty())
            m.mvmot = detail::PreviewScript(svrfmt::Unpack(kids[0].data));
        }
    }
  }
  for (auto &[motion, m] : found)
    if (!m.low.empty() && !m.high.empty())
      out.push_back(std::move(m));
  return out;
}

// The added moves' category bits (WAZE bytes 0x00-0x0F: which moveset slots and lists a move may go in) made WWE '12's:
// each game lays them out its own way (2K14: byte 0 differs on 42 % of the moves both games have, 0x0F on 39 %), and a
// 2K14 Cesaro's added moves showed as "None" in his moveset (2026-11-08). As Kaiko's ports do (svr10_moves.py, "WAZE
// category bits: from 2011 donor moves whose 2010 bits are equal"): the bits WWE '12 gives the moves both games have
// with the same bits in the source game (the commonest), else those of the closest such move (fewest bits apart).
inline void FitCategories(const disc::GameDisc &w12, const disc::GameDisc &src, std::vector<Move> &moves) {
  if (moves.empty())
    return;
  const auto a = detail::Records(w12), b = detail::Records(src);
  for (auto &m : moves) {
    if (m.waze.size() < 16)
      continue;
    std::map<Bytes, int> votes;
    int best_d = 1 << 30;
    Bytes nearest;
    for (const auto &[num, rb] : b) {
      const auto ra = a.find(num);
      if (ra == a.end() || num == m.number)
        continue;
      int d = 0;
      for (int i = 0; i < 16; ++i)
        d += __builtin_popcount(unsigned(rb[size_t(i)] ^ m.waze[size_t(i)]));
      const Bytes bits(ra->second.begin(), ra->second.begin() + 16);
      if (d == 0)
        votes[bits]++;
      else if (d < best_d)
        best_d = d, nearest = bits;
    }
    const Bytes *pick = nullptr;
    int n = 0;
    for (const auto &[bits, c] : votes)
      if (c > n)
        n = c, pick = &bits;
    if (!pick && !nearest.empty())
      pick = &nearest;
    if (pick)
      std::copy(pick->begin(), pick->end(), m.waze.begin());
  }
}

// Added moves renumbered into a kind of move WWE '12 knows. The hundred of a motion number is its kind of move (570xx
// the apron moves, 500-504xx the front grapples...); a motion in a hundred WWE '12 doesn't have froze both wrestlers -
// every added move of a 2K14 Cesaro (505xx...), while the same moves renumbered into 504xx played (2026-11-08). A move
// whose hundred WWE '12's m3.pac has keeps its number; else it takes a free motion of the hundred of the WWE '12 move
// with the same category bits (FitCategories' donors; else the closest bits), from the top of that hundred down (the
// packs fill theirs from the bottom: pack 4's 50420-50436). Its number follows (the motion's bytes swapped) and so do
// its records, preview script and RGRP/MAGG records. Returns old number -> new number (the moveset's slots follow).
// prefer: for a move number, the hundred WWE '12's rosters use most in the moveset slot it goes in (star_port: its slot
// tells its kind best - the bits alone put a 2K14 Neutralizer among the taunts, 371xx).
inline std::map<int, int> Renumber(const disc::GameDisc &w12, std::vector<Move> &moves,
                                   const std::map<int, std::map<int, int>> &prefer = {}) {
  std::map<int, int> out;
  if (moves.empty())
    return out;
  Bytes m3;
  std::set<int> used, hundreds;  // motions WWE '12 has rows for, and their hundreds
  m3_params::detail::Prm prm;
  if (w12.Read("pac/m3.pac", m3) && prm.Open(m3)) {
    const Bytes t = prm.Get("0/3");
    for (size_t o = 0; o + 8 <= t.size(); o += 8) {
      const int mo = t[o] | t[o + 1] << 8;
      used.insert(mo), hundreds.insert(mo / 100);
    }
  }
  if (hundreds.empty())
    return out;
  const auto recs = detail::Records(w12);
  for (const auto &[num, r] : recs)
    used.insert(MotionOf(num));
  for (auto &m : moves) {
    const int motion = MotionOf(m.number);
    used.insert(motion);
  }
  for (auto &m : moves) {
    const int motion = MotionOf(m.number);
    // (its slot's hundreds: those WWE '12's rosters use in the slot it goes in, with their counts - a 2K14 Cesaro's
    // taunts, 374xx, froze: WWE '12 has 374xx, but not as taunts, its taunts are 371xx)
    static const std::map<int, int> none;
    const auto pf = prefer.find(m.number);
    const std::map<int, int> &slot = pf != prefer.end() ? pf->second : none;
    const bool fits = slot.empty() || slot.count(motion / 100);
    if ((hundreds.count(motion / 100) && fits) || m.waze.size() < 16)
      continue;
    // the closest hundred WWE '12 has in the same thousand (2K14 added hundreds next to WWE '12's: its 505xx front
    // grapples after 500-504xx, its 511xx after 510xx - a 2K14 Neutralizer, 505xx, played in 504xx), else its slot's
    // commonest hundred, else the donor: a WWE '12 move with these category bits (else the fewest bits apart) whose
    // motion's hundred is known
    int best = -1, best_d = 1 << 30;
    const int h0 = motion / 100;
    for (int k = 0; k < 10 && best < 0; ++k)
      for (int h : {h0 - k, h0 + k})
        if (best < 0 && h / 10 == h0 / 10 && hundreds.count(h) && (slot.empty() || slot.count(h)))
          best = h, best_d = -1;
    for (int k = 1; k < 10 && best < 0 && slot.empty(); ++k)
      for (int h : {h0 - k, h0 + k})
        if (best < 0 && h / 10 == h0 / 10 && hundreds.count(h))
          best = h, best_d = -1;
    if (best < 0) {  // (the slot's commonest)
      int n = 0;
      for (const auto &[h, c] : slot)
        if (c > n && hundreds.count(h))
          n = c, best = h, best_d = -1;
    }
    for (const auto &[num, r] : recs) {
      if (best_d < 0)
        break;
      const int h = MotionOf(num) / 100;
      if (!hundreds.count(h))
        continue;
      int d = 0;
      for (int i = 0; i < 16; ++i)
        d += __builtin_popcount(unsigned(r[size_t(i)] ^ m.waze[size_t(i)]));
      if (d < best_d)
        best_d = d, best = h;
    }
    if (best < 0)
      continue;
    int fresh = -1;
    for (int mo = best * 100 + 99; mo >= best * 100; --mo)
      if (!used.count(mo) && !recs.count(MotionOf(mo))) {
        fresh = mo;
        break;
      }
    if (fresh < 0)
      continue;
    used.insert(fresh);
    const int number = MotionOf(fresh);  // (the swap is its own inverse)
    out[m.number] = number;
    m.number = number;
    m.waze[0x90] = uint8_t(number >> 8), m.waze[0x91] = uint8_t(number);
    char text[16] = {};
    snprintf(text, sizeof(text), "%d", fresh);
    for (Bytes *y : {&m.low, &m.high})
      if (y->size() > 0x58)
        memcpy(&(*y)[0x4C], text, 12);
    std::string s(m.mvmot.begin(), m.mvmot.end());
    const std::string from = "," + std::to_string(motion) + ",", to = "," + std::to_string(fresh) + ",";
    for (size_t p; (p = s.find(from)) != std::string::npos;)
      s.replace(p, from.size(), to);
    m.mvmot.assign(s.begin(), s.end());
    for (size_t o : {size_t(0), size_t(104)})
      if (m.magg.size() >= o + 2 && (m.magg[o] | m.magg[o + 1] << 8) == motion)
        m.magg[o] = uint8_t(fresh), m.magg[o + 1] = uint8_t(fresh >> 8);
    // (its m3 rows: m3_params::Insert writes the new motion over the first u16 and over the old one where a row names
    // itself - nothing to change here)
  }
  // (their Create a Moveset kind, RGRP/MAGG +2: one WWE '12's lists don't have - 2K14 added 0x16 - becomes the commonest
  // kind of WWE '12's rows in the motion's hundred)
  {
    Bytes dlc;
    svrfmt::Archive a;
    std::set<int> kinds;
    std::map<int, std::map<int, int>> by_hundred;
    if (w12.Read("pac/DLC.pac", dlc) && svrfmt::ArchiveOpen(dlc, a, nullptr))
      for (auto &k : a.root.kids) {
        std::vector<svrfmt::PachEntry> kids;
        if (detail::Trim(k.group) != "RGRP" || !svrfmt::PachRead(svrfmt::Unpack(svrfmt::ArchiveData(k)), kids))
          continue;
        for (const auto &kid : kids)
          if (kid.id == 10) {
            const Bytes d = svrfmt::Unpack(kid.data);
            for (size_t o = 0; o + 104 <= d.size(); o += 104) {
              kinds.insert(d[o + 2]);
              by_hundred[(d[o] | d[o + 1] << 8) / 100][d[o + 2]]++;
            }
          }
      }
    for (auto &m : moves)
      for (size_t o : {size_t(0), size_t(104)})
        if (m.magg.size() >= o + 104 && !kinds.empty() && !kinds.count(m.magg[o + 2])) {
          int best = 0, n = 0;
          for (const auto &[k, c] : by_hundred[MotionOf(m.number) / 100])
            if (c > n)
              n = c, best = k;
          m.magg[o + 2] = uint8_t(best);
        }
  }
  return out;
}

}  // namespace newmoves13
