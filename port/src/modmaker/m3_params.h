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
// latest m3.pac under the added move's motion (Insert). The hundred of a motion number is its kind of move: 570xx are
// the apron moves (Hotshot, Apron Suplex...), 500xx-504xx the front grapples (the packs put theirs in 504xx). A copy of
// Suplex 3 (501xx) numbered 57099 most likely played as an apron move (camera low behind the ropes, a stop half-way, in game
// 2026-11-08): an added move's motion goes in its model's hundred, or a free one of the same kind (50399, 50499).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <set>
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

// Another game's PRM tables by name ("0/N", "N"). WWE '13 - 2K16: the PACH above. 2K17: an HSPC whose entry is an
// SHDC laid out as the HSPC itself (header {'SHDC', 0x7DA, 0x40, 0x800, ..., +0x18 index bytes, +0x20 table bytes,
// +0x38 count}; index at 0x800, then 0x800-aligned the table {u32 id, u32 offset, u32 size, u32 0}); its child 21
// holds 2K16's 0/0-0/11 (the same bytes, 2026-11-08), its children 1-14 2K16's 1-14 (child 0: 2K16's 0/21-0/40).
struct OtherPrm {
  std::map<std::string, Bytes> t;
  static uint32_t U32(const Bytes &d, size_t o) {
    return d[o] | d[o + 1] << 8 | d[o + 2] << 16 | uint32_t(d[o + 3]) << 24;
  }
  // an SHDC of the HSPC kind: its children by id (unpacked); false if it isn't one
  static bool Shdc(const Bytes &d, std::map<uint32_t, Bytes> &kids) {
    if (d.size() < 0x40 || memcmp(d.data(), "SHDC", 4) != 0 || U32(d, 0x0C) != 0x800)
      return false;
    const uint32_t ib = U32(d, 0x18), n = U32(d, 0x38);
    const size_t at = 0x800 + ((size_t(ib) + 0x7FF) & ~size_t(0x7FF));
    if (at + size_t(n) * 16 > d.size())
      return false;
    for (uint32_t i = 0; i < n; ++i) {
      const size_t off = U32(d, at + 16 * i + 4), sz = U32(d, at + 16 * i + 8);
      if (off + sz <= d.size())
        kids[U32(d, at + 16 * i)] = svrfmt::Unpack(Bytes(d.begin() + long(off), d.begin() + long(off + sz)));
    }
    return true;
  }
  bool Open(const Bytes &m3) {
    Prm p;
    if (p.Open(m3)) {
      for (const auto &e : p.inner)
        t["0/" + std::to_string(e.id)] = svrfmt::Unpack(e.data);
      for (const auto &e : p.top)
        if (e.id != 0)
          t[std::to_string(e.id)] = svrfmt::Unpack(e.data);
      return true;
    }
    svrfmt::Archive a;
    if (!svrfmt::ArchiveOpen(m3, a, nullptr) || a.root.kind != svrfmt::ArcKind::Hspc)
      return false;
    for (const auto &k : a.root.kids) {
      std::map<uint32_t, Bytes> top, inner;
      if (!Shdc(svrfmt::ArchiveData(k), top) || !top.count(21) || !Shdc(top[21], inner))
        continue;
      for (auto &[id, d] : inner)
        t["0/" + std::to_string(id)] = std::move(d);
      for (auto &[id, d] : top)
        if (id >= 1 && id <= 14)
          t[std::to_string(id)] = std::move(d);
      return true;
    }
    return false;
  }
  Bytes Get(const std::string &name) const {
    const auto it = t.find(name);
    return it == t.end() ? Bytes{} : it->second;
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

// The rows of `motion` in another game's m3.pac, made WWE '12's (the real move's own data, as Kaiko's ports take the
// source game's records; 2026-11-08). WWE '13 and 2K14 (2K16: the console-order set only; 2K17: 2K16's, OtherPrm) keep the same tables
// renumbered and widened: 0/11 (28 bytes) = 0/1, 0/12 (20) = 0/2, 0/14 (12: start at +4, count at +8) = 0/3 with its
// events in 0/15 (4 bytes, as 0/4), 0/10 (40) = 0/6, 0/16 (32) = 0/7, 5 = 5 (0/0-0/7: the same tables in another byte
// order). The byte maps were learned on the motions both games have (tools_local\moves_study\bytemap, 4 213-14 952
// rows a table, 85 % or more alike; WWE '13 and 2K14 the same but one byte): each WWE '12 byte from that source byte,
// or (-1 - v) the constant v.
inline Rows FromOther(const Bytes &m3, int motion, double k = 1.0) {
  using namespace detail;
  Rows out;
  OtherPrm p;
  if (!p.Open(m3))
    return out;
  // The game's tables in WWE '13 / 2K14 order. 2K14 keeps each twice: 0/0-0/7 in the console's byte order, 0/10-0/17 in
  // the other; 2K16 keeps only the first set. The byte order learned on 2K14's two copies (each byte of a 0/1N row from
  // which byte of its 0/N row, 100 % of the rows; tools_local\moves_studyeperm), 2026-11-08.
  std::map<std::string, Bytes> tab;
  {
    // per table: its row size and, for each byte, the byte it comes from (2-byte and 4-byte fields reversed)
    struct Perm {
      const char *be, *le;
      std::vector<int> from;
    };
    static const std::vector<Perm> perms = {
        {"0/0", "0/10", {1, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 12, 15, 14, 17, 16, 19, 18, 21, 20, 22, 23, 25, 24, 26, 27,
                         28, 29, 30, 31, 35, 34, 33, 32, 37, 36, 38, 39}},
        {"0/1", "0/11", {1, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 12, 14, 15, 17, 16, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27}},
        {"0/2", "0/12", {1, 0, 2, 3, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 14, 15, 16, 17, 18, 19}},
        {"0/4", "0/14", {1, 0, 2, 3, 7, 6, 5, 4, 9, 8, 10, 11}},  // (start: 4 bytes; count: 2 bytes, then 2 zeros)
        {"0/5", "0/15", {1, 0, 3, 2}},
        {"0/6", "0/16", {1, 0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
                         28, 29, 30, 31}},
    };
    // (all from one side: 2K16 has tables named 0/10 and 0/11 too, holding something else)
    const bool both = !p.Get("0/14").empty() && !p.Get("0/15").empty();
    for (const auto &pm : perms) {
      Bytes le = both ? p.Get(pm.le) : Bytes{};
      if (!both) {
        const Bytes be = p.Get(pm.be);
        const size_t s = pm.from.size();
        le.resize(be.size() / s * s);
        for (size_t o = 0; o + s <= be.size(); o += s)
          for (size_t i = 0; i < s; ++i)
            le[o + i] = be[o + size_t(pm.from[i])];
      }
      tab[pm.le] = std::move(le);
    }
    tab["5"] = p.Get("5");
  }
  if (tab["0/14"].empty() || tab["0/15"].empty())
    return out;
  struct Map {
    const char *to, *from;
    size_t stride, head, from_stride;
    std::vector<int> bytes;
  };
  static const std::vector<Map> maps = {
      {"0/1", "0/11", 28, 0, 28, {0, 1, 2, 3, -1, 9, 4, 5, 6, 7, 12, -1, 14, 15, 10, -1, -1, 10, -1, -1, 17, 11, -1, -1, 0, 1, 2, 3}},
      {"0/2", "0/12", 20, 0, 20, {0, 1, 2, -1, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1}},
      // (0/6 +20: a motion it refers to - the source's own, 61 % alike, better than none)
      {"0/6", "0/10", 36, 0, 40, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, -1, -1, 27, 28, 29, 30, 31, 33, -1, 36, 38}},
      {"0/7", "0/16", 20, 0, 32, {0, 1, 2, 3, 4, 5, 6, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}},
      {"5", "5", 20, 28, 20, {0, 1, 2, -1, 4, 5, 6, 7, 8, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1}},
  };
  auto frame = [&](int f) { return std::clamp(int(std::lround(f * k)), 0, 65535); };
  for (const auto &m : maps) {
    const Bytes &d = tab[m.from];
    for (size_t o = m.head; o + m.from_stride <= d.size(); o += m.from_stride) {
      if (U16(d, o) != motion)
        continue;
      Bytes r(m.stride, 0);
      for (size_t b = 0; b < m.stride; ++b)
        r[b] = m.bytes[b] >= 0 ? d[o + size_t(m.bytes[b])] : uint8_t(-1 - m.bytes[b]);
      // (one less in WWE '12, 0 -> FF: 0/2 +15 (WWE '13 0C5A = WWE '12 0B5A, 0000 = FF00), 0/6 +26 (0001 -> 0000,
      // 000B -> 000A, 0000 -> 00FF))
      if (std::string(m.to) == "0/2")
        r[15] = uint8_t(d[o + 15] - 1);
      if (std::string(m.to) == "0/6") {
        r[26] = uint8_t(d[o + 26] - 1);
        if (r[27] == 0xFF)
          r[27] = 0;  // (2K16 sets this byte FF where 2K14 and WWE '12 have 0)
        if (U16(r, 20) == 0)  // (WWE '12 names the motion itself there where WWE '13 has 0: rows off 36 % -> 16 %)
          Put16(r, 20, motion);
      }
      if (std::string(m.to) == "5") {
        Put16(r, 4, frame(U16(r, 4)));
        Put16(r, 6, frame(U16(r, 6)));
      }
      out.rows[m.to].push_back(std::move(r));
    }
  }
  // 0/3 and its events: 0/14 {motion, part, start, 0, count, 0}, its events at the running sum of the counts before
  const Bytes &t14 = tab["0/14"], &t15 = tab["0/15"];
  long base = 0;
  for (size_t o = 0; o + 12 <= t14.size(); o += 12) {
    const int count = U16(t14, o + 8);
    if (U16(t14, o) == motion) {
      Bytes r(8, 0);
      Put16(r, 0, motion), Put16(r, 2, U16(t14, o + 2)), Put16(r, 4, int(out.events.size())), Put16(r, 6, count);
      for (int i = 0; i < count && size_t(base + i + 1) * 4 <= t15.size(); ++i) {
        Bytes ev(t15.begin() + long(base + i) * 4, t15.begin() + long(base + i + 1) * 4);
        Put16(ev, 2, frame(U16(ev, 2)));
        out.events.push_back(ev);
      }
      out.rows["0/3"].push_back(std::move(r));
    }
    base += count;
  }
  if (out.rows["0/3"].empty())
    return {};
  return out;
}

// The kinds of event WWE '12's own motions use (its m3.pac's 0/4).
inline std::set<int> KnownEvents(const Bytes &m3) {
  std::set<int> out;
  detail::Prm p;
  if (p.Open(m3)) {
    const Bytes t4 = p.Get("0/4");
    for (size_t o = 0; o + 4 <= t4.size(); o += 4)
      out.insert(detail::U16(t4, o));
  }
  return out;
}
// Events of kinds WWE '12 never uses left out (2K14 added 0x338-0x343, 0x0001...: its Neutralizer's victim has
// 0x338 after the impact, and fell too soon in WWE '12, 2026-11-08). 0/3's starts and counts follow. How many dropped.
inline int DropUnknownEvents(Rows &r, const std::set<int> &known) {
  if (known.empty())
    return 0;
  std::vector<Bytes> kept;
  int dropped = 0;
  for (auto &row : r.rows["0/3"]) {
    const int start = detail::U16(row, 4), count = detail::U16(row, 6);
    const int at = int(kept.size());
    for (int i = start; i < start + count && i < int(r.events.size()); ++i) {
      if (known.count(detail::U16(r.events[size_t(i)], 0)))
        kept.push_back(r.events[size_t(i)]);
      else
        ++dropped;
    }
    detail::Put16(row, 4, at);
    detail::Put16(row, 6, int(kept.size()) - at);
  }
  r.events = std::move(kept);
  return dropped;
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
