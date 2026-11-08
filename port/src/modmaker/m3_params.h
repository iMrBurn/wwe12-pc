// A motion's parameters in pac\m3.pac (MDAT/PRM), for the moves the mods add (2026-11-08). The game plays a move's
// motion only with its rows there: an exact copy of a game move, added with its own motion number, froze both
// wrestlers in game; the game's packs 3 and 4 ship a whole m3.pac with their moves' rows (docs/prises_svr11.md).
//
// MDAT/PRM = a PACH: child 0 a PACH of tables (BPE), children 1-5 other tables (5 plain). The tables a motion has rows
// in, sorted by motion number (u16 little-endian at the start of each row), no header unless said:
//   0/1  28-byte rows, the motion again at +24 (one row);
//   0/3  8-byte rows {motion, part (0, 1, ...), start, count}: its events in 0/4 - start counts from the first event of
//        the motion's hundred (the game sums the counts of the rows before, by hundred: no base stored anywhere);
//   0/4  4-byte events {u16 kind, u16 frame (60 Hz)}, in 0/3's order;
//   0/6  36-byte rows (a part, a counter branch 0x5A-0x5C...), the motion again at +20 in some;
//   0/7  20-byte rows (one a part);
//   5    "SMB" (sounds), a 28-byte header (u16 +6 a count) then 20-byte rows {motion, 0, u16 from, u16 to (frames),
//        ..., u16 +18 = its row number from 1};
//   0/2  20-byte rows (Suplex 3: one, naming its counter branch 0x5A; the packs' moves: none). A copy of Suplex 3
//        without it stopped half-way through in game (2026-11-08: suspected, not proved); 0/8: not keyed by motion (not understood), left out.
// A motion's rows travel as a "row set" (Extract, a mod's superstar\moves\<number>.m3) and go into a copy of the
// latest m3.pac under the added move's motion (Insert). A motion is best put last in its hundred (57098 after the
// pack's 57007): then no other motion's 0/3 start moves.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "svrfmt/archive.h"
#include "svrfmt/pac.h"

namespace m3_params {

using svrfmt::Bytes;

namespace detail {

inline int U16(const Bytes &d, size_t o) { return o + 2 <= d.size() ? d[o] | d[o + 1] << 8 : 0; }
inline void Put16(Bytes &d, size_t o, int v) { d[o] = uint8_t(v), d[o + 1] = uint8_t(v >> 8); }

struct Table {
  const char *name;  // "0/1" ... "5"
  size_t stride, head, self;  // row size, header size, where else the row holds its motion (0: nowhere)
};
inline const std::vector<Table> &Keyed() {
  static const std::vector<Table> t = {{"0/1", 28, 0, 24}, {"0/2", 20, 0, 0}, {"0/3", 8, 0, 0}, {"0/6", 36, 0, 20}, {"0/7", 20, 0, 0},
                                       {"5", 20, 28, 0}};
  return t;
}

// The PRM tables by name, and where each sits (child 0's child id, or the PRM child id).
struct Prm {
  svrfmt::Archive arc;
  std::vector<svrfmt::PachEntry> top, inner;  // PRM's children, child 0's children
  bool Open(const Bytes &m3) {
    if (!svrfmt::ArchiveOpen(m3, arc, nullptr) || arc.root.kids.empty())
      return false;
    if (!svrfmt::PachRead(svrfmt::Unpack(svrfmt::ArchiveData(arc.root.kids[0])), top))
      return false;
    for (const auto &e : top)
      if (e.id == 0)
        return svrfmt::PachRead(svrfmt::Unpack(e.data), inner);
    return false;
  }
  svrfmt::PachEntry *Entry(const std::string &name) {
    auto &list = name.rfind("0/", 0) == 0 ? inner : top;
    const uint32_t id = uint32_t(std::stoul(name.rfind("0/", 0) == 0 ? name.substr(2) : name));
    for (auto &e : list)
      if (e.id == id)
        return &e;
    return nullptr;
  }
  Bytes Get(const std::string &name) {
    const auto *e = Entry(name);
    return e ? svrfmt::Unpack(e->data) : Bytes{};
  }
  void Set(const std::string &name, const Bytes &plain) {
    if (auto *e = Entry(name))
      e->data = svrfmt::IsBpe(e->data) ? svrfmt::BpeEncode(plain) : plain;
  }
  Bytes Save() {
    for (auto &e : top)
      if (e.id == 0)
        e.data = svrfmt::PachWrite(inner);
    svrfmt::ArchiveReplace(arc.root.kids[0], svrfmt::PachWrite(top));
    svrfmt::ArchiveTouch(arc.root);
    return svrfmt::ArchiveSave(arc);
  }
};

}  // namespace detail

// A motion's rows: per table its rows (0/3's starts made relative to the set's own events), and its 0/4 events.
struct Rows {
  std::map<std::string, std::vector<Bytes>> rows;
  std::vector<Bytes> events;  // 0/4, 4 bytes each
  bool empty() const { return rows.empty(); }
};

// The rows of `motion` in m3 (frames scaled by `k`: a motion made longer or shorter than the one they come from).
inline Rows Extract(const Bytes &m3, int motion, double k = 1.0) {
  using namespace detail;
  Rows out;
  Prm p;
  if (!p.Open(m3))
    return out;
  const Bytes t3 = p.Get("0/3"), t4 = p.Get("0/4");
  auto frame = [&](int f) { return std::clamp(int(std::lround(f * k)), 0, 65535); };
  for (const auto &t : Keyed()) {
    const Bytes d = p.Get(t.name);
    for (size_t o = t.head; o + t.stride <= d.size(); o += t.stride) {
      if (U16(d, o) != motion)
        continue;
      Bytes r(d.begin() + long(o), d.begin() + long(o + t.stride));
      if (std::string(t.name) == "0/3") {
        // its events: where the hundred's first event is + start (the counts of the rows before, by hundred)
        long base = 0;
        for (size_t q = 0; q + 8 <= t3.size() && q < o; q += 8)
          base += U16(t3, q + 6);
        // (base: every event before this row; its own start is then base itself - the rows are in order)
        const int count = U16(r, 6);
        Put16(r, 4, int(out.events.size()));
        for (int i = 0; i < count; ++i) {
          const size_t e = size_t(base + i) * 4;
          if (e + 4 > t4.size())
            break;
          Bytes ev(t4.begin() + long(e), t4.begin() + long(e + 4));
          Put16(ev, 2, frame(U16(ev, 2)));
          out.events.push_back(ev);
        }
      } else if (std::string(t.name) == "5") {
        Put16(r, 4, frame(U16(r, 4)));
        Put16(r, 6, frame(U16(r, 6)));
      }
      out.rows[t.name].push_back(std::move(r));
    }
  }
  return out;
}

// A row set as a file (superstar\moves\<number>.m3): "M3RS", then per table {name 4 bytes, u32 rows, u32 stride,
// rows}, then "0/4\0" {u32 events, 4, events}; little-endian.
inline Bytes Write(const Rows &r) {
  Bytes out = {'M', '3', 'R', 'S'};
  auto u32 = [&](uint32_t v) {
    for (int i = 0; i < 4; ++i)
      out.push_back(uint8_t(v >> (8 * i)));
  };
  auto table = [&](const std::string &name, const std::vector<Bytes> &rows) {
    char n[4] = {};
    memcpy(n, name.data(), std::min<size_t>(4, name.size()));
    out.insert(out.end(), n, n + 4);
    u32(uint32_t(rows.size()));
    u32(uint32_t(rows.empty() ? 0 : rows[0].size()));
    for (const auto &x : rows)
      out.insert(out.end(), x.begin(), x.end());
  };
  for (const auto &[name, rows] : r.rows)
    table(name, rows);
  table("0/4", r.events);
  return out;
}
inline bool Read(const Bytes &b, Rows &r) {
  r = {};
  if (b.size() < 4 || memcmp(b.data(), "M3RS", 4))
    return false;
  size_t o = 4;
  auto u32 = [&](size_t at) { return uint32_t(b[at] | b[at + 1] << 8 | b[at + 2] << 16 | uint32_t(b[at + 3]) << 24); };
  while (o + 12 <= b.size()) {
    const std::string name(reinterpret_cast<const char *>(&b[o]), strnlen(reinterpret_cast<const char *>(&b[o]), 4));
    const uint32_t n = u32(o + 4), stride = u32(o + 8);
    o += 12;
    if (o + size_t(n) * stride > b.size())
      return false;
    std::vector<Bytes> rows;
    for (uint32_t i = 0; i < n; ++i, o += stride)
      rows.emplace_back(b.begin() + long(o), b.begin() + long(o + stride));
    if (name == "0/4")
      r.events = std::move(rows);
    else
      r.rows[name] = std::move(rows);
  }
  return true;
}

// m3 with `rows` added under `motion` (its rows in each table, in motion order; 0/3 starts and 0/4 events placed in
// its hundred, the later rows of that hundred moved on; table 5's row numbers and count kept right). Each motion once:
// one already there is left as it is. Empty on failure.
inline Bytes Insert(const Bytes &m3, const std::vector<std::pair<int, Rows>> &adds) {
  using namespace detail;
  Prm p;
  if (!p.Open(m3))
    return {};
  std::map<std::string, Bytes> t;
  for (const auto &tab : Keyed())
    t[tab.name] = p.Get(tab.name);
  t["0/4"] = p.Get("0/4");
  for (const auto &[motion, rs] : adds) {
    bool have = false;
    for (size_t o = 0; o + 8 <= t["0/3"].size(); o += 8)
      have |= U16(t["0/3"], o) == motion;
    if (have || rs.empty())
      continue;
    for (const auto &tab : Keyed()) {
      const auto it = rs.rows.find(tab.name);
      if (it == rs.rows.end() || it->second.empty())
        continue;
      Bytes &d = t[tab.name];
      bool here = false;  // (rows the disc already has for it - the packs' motions: 0/6 placeholders, some 0/1, 0/7)
      for (size_t o = tab.head; o + tab.stride <= d.size() && !here; o += tab.stride)
        here = U16(d, o) == motion;
      if (here)
        continue;
      size_t at = tab.head;  // (after the rows of smaller motions)
      while (at + tab.stride <= d.size() && U16(d, at) <= motion)
        at += tab.stride;
      std::vector<Bytes> rows = it->second;
      for (auto &r : rows) {
        if (r.size() != tab.stride)
          return {};
        Put16(r, 0, motion);
        if (tab.self && U16(r, tab.self) != 0 && U16(r, tab.self) == U16(it->second[0], 0))
          Put16(r, tab.self, motion);
      }
      if (std::string(tab.name) == "0/3") {
        // where its events go: after every event of the rows before (the counts in order); its starts from the
        // hundred's first event; the hundred's later rows moved on by its count
        long before = 0, in_hundred = 0;
        for (size_t q = 0; q < at; q += 8) {
          before += U16(d, q + 6);
          if (U16(d, q) / 100 == motion / 100)
            in_hundred += U16(d, q + 6);
        }
        int added = 0;
        for (auto &r : rows) {
          const int rel = U16(r, 4);
          Put16(r, 4, int(in_hundred + rel));
          added += U16(r, 6);
        }
        for (size_t q = at; q + 8 <= d.size() && U16(d, q) / 100 == motion / 100; q += 8)
          Put16(d, q + 4, U16(d, q + 4) + added);
        Bytes ev;
        for (const auto &e : rs.events)
          ev.insert(ev.end(), e.begin(), e.end());
        Bytes &t4 = t["0/4"];
        const size_t pos = std::min(t4.size(), size_t(before) * 4);
        t4.insert(t4.begin() + long(pos), ev.begin(), ev.end());
      }
      Bytes flat;
      for (const auto &r : rows)
        flat.insert(flat.end(), r.begin(), r.end());
      d.insert(d.begin() + long(at), flat.begin(), flat.end());
      if (std::string(tab.name) == "5") {
        for (size_t o = tab.head, i = 1; o + tab.stride <= d.size(); o += tab.stride, ++i)
          Put16(d, o + 18, int(i));
        Put16(d, 6, U16(d, 6) + int(rows.size()));
      }
    }
  }
  for (auto &[name, d] : t)
    p.Set(name, d);
  return p.Save();
}

}  // namespace m3_params
