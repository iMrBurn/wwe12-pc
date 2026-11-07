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
#include "yanm.h"

namespace newmoves13 {

using Bytes = std::vector<uint8_t>;

struct Move {
  int number = 0;     // as PRO and moveset_port hold it
  std::string name;
  Bytes waze;         // its 160-byte record, made WWE '12's
  Bytes low, high;    // its motion's light and full records
  Bytes magg;         // its DLC.pac RGRP/MAGG records (children 10 and 11, 104 bytes each), else empty
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
  if (!d.Read(file, m) || !svrfmt::ArchiveOpen(m, a, nullptr))
    continue;
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
        out.emplace(t[o + 0x90] << 8 | t[o + 0x91], Bytes(t.begin() + long(o), t.begin() + long(o) + 160));
  }
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
    // (after its number WWE '12 has FF: from +0x92. WWE '13 had them from +0x98 already; 2K15 keeps other values in
    // +0x92..+0x9F, its records otherwise WWE '12's, 82 % of the bytes equal on the moves both have, 2026-11-08)
    for (size_t i = 0x92; i < 0xA0; ++i)
      m.waze[i] = 0xFF;
    m.name = std::string(reinterpret_cast<const char *>(&m.waze[0x10]), strnlen(reinterpret_cast<const char *>(&m.waze[0x10]), 0x40));
    found[MotionOf(n)] = std::move(m);
  }
  // (WWE '13: pac\mot00.pac, mot01.pac; 2K14: mot00-mot08.pac; WWE '12's own: m.pac)
  std::vector<std::string> files;
  for (const auto &f : w13.List("pac"))
    if (f.size() == 9 && _strnicmp(f.c_str(), "mot", 3) == 0 && _stricmp(f.c_str() + 5, ".pac") == 0)
      files.push_back("pac/" + f);
  std::sort(files.begin(), files.end());
  files.push_back("pac/m.pac");
  for (const auto &file : files) {
    Bytes head;
    const uint64_t size = w13.SizeOf(file);
    std::vector<svrfmt::EpacRow> rows;
    if (!size || !w13.ReadPart(file, 0, 0x8000, head) || !svrfmt::EpacTable(head, size, rows))
      continue;
    // (every motion group: WWE '13's ZMOT/CAnn, 2K14's ZMVM/Mnnn, Ennn and ZMBS too - its moves' motions are there,
    // numbered as WWE '12's: a player's 2K14 Antonio Cesaro, 11 of his 12 moves WWE '12 hasn't were outside ZMOT,
    // 2026-11-08). Each motion is there twice, its light and its full record: the smaller, the larger.
    for (const auto &row : rows) {
      if (row.group.empty() || row.group[0] != 'Z')
        continue;
      Bytes raw;
      if (!w13.ReadPart(file, row.offset, row.size, raw))
        continue;
      struct W {
        static void Walk(const Bytes &d, int id, std::map<int, Move> &found, int depth) {
          if (svrfmt::IsPach(d)) {
            std::vector<svrfmt::PachEntry> kids;
            if (depth < 6 && svrfmt::PachRead(d, kids))
              for (const auto &k : kids)
                Walk(svrfmt::Unpack(k.data), int(k.id), found, depth + 1);
            return;
          }
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
      };
      W::Walk(svrfmt::Unpack(raw), 0, found, 0);
    }
  }
  // their RGRP/MAGG records (WWE '13's DLC.pac: children 10 and 11, 104 bytes by motion number)
  {
    Bytes dlc;
    svrfmt::Archive a;
    if (w13.Read("pac/DLC.pac", dlc) && svrfmt::ArchiveOpen(dlc, a, nullptr))
      for (auto &k : a.root.kids) {
        std::vector<svrfmt::PachEntry> kids;
        if (detail::Trim(k.group) != "RGRP" || !svrfmt::PachRead(svrfmt::Unpack(svrfmt::ArchiveData(k)), kids))
          continue;
        std::map<int, Bytes> rec[2];
        for (const auto &kid : kids)
          if (kid.id == 10 || kid.id == 11) {
            const Bytes d = svrfmt::Unpack(kid.data);
            for (size_t o = 0; o + 104 <= d.size(); o += 104)
              rec[kid.id == 11][d[o] | d[o + 1] << 8] = Bytes(d.begin() + long(o), d.begin() + long(o) + 104);
          }
        for (auto &[motion, m] : found)
          if (rec[0].count(motion) && rec[1].count(motion)) {
            m.magg = rec[0][motion];
            m.magg.insert(m.magg.end(), rec[1][motion].begin(), rec[1][motion].end());
          }
      }
  }
  for (auto &[motion, m] : found)
    if (!m.low.empty() && !m.high.empty())
      out.push_back(std::move(m));
  return out;
}

}  // namespace newmoves13
