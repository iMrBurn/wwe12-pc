// Added moves in the game's situation sets of motions (m.pac ZMVM/STAT, GRND, CORN, SPE1, RUN...; 2026-11-10). The
// method is LA Max's WWE 2K14 -> WWE '12 move porter (wwe12_port.py, built on Kaiko's SvR 2011 tools): a ported motion
// is added beside its WWE '12 template move in every situation set that holds the template, at the same place (its
// group, its light / full case), as an addition - nothing of the game's is replaced.
//
// What they are: m.pac's 23 ZMVM entries are each a PACH of whole motion groups copied from ZMOT/CA01-CA08 (same ids,
// the same bytes: STAT has groups 0, 1, 10, 21, 22, 40, 41 and 47; group 47 = ZMOT/CA05's, Gut Kick 1 and 2's, cases
// 0 / 1 = light / full records of 400xx, 2 / 3 of 401xx), by motion number. The game asks for them by name
// ("/ZMVM/STAT", a table at 0x820166D1) and takes the first archive that has the name (sub_827F29F0): ours are served
// in the pack's m_dlcNN.pac under lowercase names ("/ZMVM/stat") and the game's lookup tries those first
// (wwe12_superstar_mods.cpp, sub_827D98D8), whatever order the archives are in.
//
// Why: a 2009 Maria's moves mod broke the Gut Kick of every superstar (players, 2026-11-10); her Woman's Toe Kick goes
// in Gut Kick 2's kind (401xx) with no place in the sets that hold it. Not proved to be the cause: THQ's packs 3 and 4
// add only a ZMOT bank, and their moves play.
//
// The sets are large (128 MB in all; Gut Kick's group alone is in 36 MB of them): made at each start with added moves
// (0.6 s for a 2009 Maria's 13 moves, 11 sets; a cache of them read as slowly, so none).
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "svrfmt/pac.h"

namespace zmvm_add {

using svrfmt::Bytes;

// An added move: its motion, its WWE '12 template's motion, its full record (yanm, unpacked).
struct Add {
  int motion = 0, model = 0;
  const Bytes *high = nullptr;
};

// A part of m.pac: (offset, size, out).
using Reader = std::function<bool(uint64_t, uint32_t, Bytes &)>;

namespace detail {

inline std::string Lower(std::string s) {
  for (auto &c : s)
    c = char(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

}  // namespace detail

// The situation sets that hold a template of `adds`, with the added motions put beside it: EPAC entries named in
// lowercase (group ZMVM). Empty when nothing is to be added.
inline std::vector<svrfmt::EpacEntry> Make(const Reader &read, uint64_t m_size, const std::vector<Add> &adds,
                                           std::string *log = nullptr) {
  using namespace detail;
  std::vector<svrfmt::EpacEntry> out;
  std::map<int, std::vector<const Add *>> by_model;  // template motion -> the moves put beside it
  std::map<int, const Add *> by_motion;              // every added move by its motion
  for (const auto &a : adds)
    if (a.motion > 0 && a.high && !a.high->empty()) {
      by_motion[a.motion] = &a;
      if (a.model > 0)
        by_model[a.model].push_back(&a);
    }
  if (by_motion.empty() || !read)
    return out;
  // made: every ZMVM set of m.pac read; the groups holding a template rebuilt once (the sets share them)
  Bytes head;
  std::vector<svrfmt::EpacRow> rows;
  if (!read(0, uint32_t(std::min<uint64_t>(m_size, 0x8000)), head) || !svrfmt::EpacTable(head, m_size, rows))
    return out;
  std::map<uint32_t, Bytes> changed;  // group id -> the group with the moves added (empty: unchanged)
  std::set<uint32_t> seen;
  int moves_placed = 0;
  auto group_of = [&](uint32_t id, const Bytes &data) -> const Bytes & {
    if (seen.insert(id).second) {
      std::vector<svrfmt::PachEntry> cases;
      if (svrfmt::PachRead(svrfmt::Unpack(data), cases)) {
        bool any = false;
        for (auto &c : cases) {
          std::vector<svrfmt::PachEntry> ids;
          if (!svrfmt::PachRead(svrfmt::Unpack(c.data), ids))
            continue;
          bool here = false;
          for (const auto &[model, list] : by_model) {
            const auto t = std::find_if(ids.begin(), ids.end(), [&](const auto &e) { return int(e.id) == model; });
            if (t == ids.end())
              continue;
            // (light or full as the template's: the light records are BPE, the full ones ZLIB on the disc; ours as
            // the pack's ZMOT bank has them, the light one the full record BPE-compressed)
            const bool light = svrfmt::IsBpe(t->data);
            for (const Add *a : list) {
              if (std::any_of(ids.begin(), ids.end(), [&](const auto &e) { return int(e.id) == a->motion; }))
                continue;  // (never over one of the game's)
              ids.push_back({uint32_t(a->motion), light ? svrfmt::BpeEncode(*a->high) : *a->high});
              here = true;
              ++moves_placed;
            }
          }
          // (an added move kept a number the game holds a place for: a 128-byte stub, no motion - SvR 2009's Judo Flip
          // Suplex 50292, taunts 37098 / 37134, whole hundreds 502xx, 370xx, 371xx of them: its motion goes there,
          // light or full as the stub)
          for (auto &e : ids) {
            const auto a = by_motion.find(int(e.id));
            if (a == by_motion.end() || svrfmt::Unpack(e.data).size() > 128)
              continue;
            e.data = svrfmt::IsBpe(e.data) ? svrfmt::BpeEncode(*a->second->high) : *a->second->high;
            here = true;
            ++moves_placed;
          }
          if (!here)
            continue;
          std::sort(ids.begin(), ids.end(), [](const auto &x, const auto &y) { return x.id < y.id; });
          const Bytes pach = svrfmt::PachWrite(ids);
          c.data = svrfmt::IsBpe(c.data) ? svrfmt::BpeEncode(pach) : pach;
          any = true;
        }
        if (any) {
          const Bytes pach = svrfmt::PachWrite(cases);
          changed[id] = svrfmt::IsBpe(data) ? svrfmt::BpeEncode(pach) : pach;
        }
      }
    }
    static const Bytes none;
    const auto it = changed.find(id);
    return it == changed.end() ? none : it->second;
  };
  for (const auto &r : rows) {
    if (r.group.rfind("ZMVM", 0) != 0)
      continue;
    Bytes raw;
    std::vector<svrfmt::PachEntry> groups;
    if (!read(r.offset, r.size, raw) || !svrfmt::PachRead(svrfmt::Unpack(raw), groups))
      continue;
    bool any = false;
    for (auto &g : groups)
      if (const Bytes &now = group_of(g.id, g.data); !now.empty())
        g.data = now, any = true;
    if (!any)
      continue;
    svrfmt::EpacEntry e;
    e.name = Lower(r.name).substr(0, 4);
    e.name.resize(4, ' ');
    const Bytes pach = svrfmt::PachWrite(groups);
    e.data = svrfmt::IsBpe(raw) ? svrfmt::BpeEncode(pach) : pach;
    out.push_back(std::move(e));
  }
  if (log)
    *log = "situation sets made: " + std::to_string(out.size()) + " (" + std::to_string(changed.size()) +
           " motion group(s), " + std::to_string(moves_placed) + " place(s))";
  return out;
}

}  // namespace zmvm_add
