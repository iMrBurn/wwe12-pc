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

namespace newmoves13 {

using Bytes = std::vector<uint8_t>;

struct Move {
  int number = 0;     // as PRO and moveset_port hold it
  std::string name;
  Bytes waze;         // its 160-byte record, made WWE '12's
  Bytes low, high;    // its motion's light and full records
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
  Bytes m;
  svrfmt::Archive a;
  if (!d.Read("pac/misc.pac", m) || !svrfmt::ArchiveOpen(m, a, nullptr))
    return out;
  for (auto &k : a.root.kids) {
    if (Trim(k.group) != "MOVS" || Trim(k.name) != "WAZE")
      continue;
    const Bytes t = svrfmt::ArchiveData(k);
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
        out[t[o + 0x90] << 8 | t[o + 0x91]] = Bytes(t.begin() + long(o), t.begin() + long(o) + 160);
  }
  return out;
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
    for (size_t i = 0x98; i < 0xA0; ++i)
      m.waze[i] = 0xFF;
    m.name = std::string(reinterpret_cast<const char *>(&m.waze[0x10]), strnlen(reinterpret_cast<const char *>(&m.waze[0x10]), 0x40));
    found[MotionOf(n)] = std::move(m);
  }
  for (const char *file : {"pac/mot00.pac", "pac/mot01.pac"}) {
    Bytes head;
    const uint64_t size = w13.SizeOf(file);
    std::vector<svrfmt::EpacRow> rows;
    if (!size || !w13.ReadPart(file, 0, 0x8000, head) || !svrfmt::EpacTable(head, size, rows))
      continue;
    for (const auto &row : rows) {
      if (detail::Trim(row.group) != "ZMOT" || row.name.rfind("CA", 0) != 0)
        continue;
      Bytes raw;
      std::vector<svrfmt::PachEntry> tops;
      if (!w13.ReadPart(file, row.offset, row.size, raw) || !svrfmt::PachRead(svrfmt::Unpack(raw), tops))
        continue;
      for (const auto &top : tops) {
        std::vector<svrfmt::PachEntry> kids;
        if (!svrfmt::PachRead(svrfmt::Unpack(top.data), kids))
          continue;
        for (size_t i = 0; i + 1 < kids.size(); i += 2) {
          std::vector<svrfmt::PachEntry> lo, hi;
          if (!svrfmt::PachRead(svrfmt::Unpack(kids[i].data), lo) || !svrfmt::PachRead(svrfmt::Unpack(kids[i + 1].data), hi))
            continue;
          for (const auto &l : lo) {
            const auto f = found.find(int(l.id));
            if (f == found.end() || !f->second.low.empty())
              continue;
            for (const auto &h : hi)
              if (h.id == l.id) {
                f->second.low = svrfmt::Unpack(l.data);
                f->second.high = svrfmt::Unpack(h.data);
                break;
              }
          }
        }
      }
    }
  }
  for (auto &[motion, m] : found)
    if (!m.low.empty() && !m.high.empty())
      out.push_back(std::move(m));
  return out;
}

}  // namespace newmoves13
