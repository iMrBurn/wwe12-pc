// A ported WWE 2K entrance's title belt in the hand without the one at the waist (2026-11-12: 2K14 The Miz's champion's
// entrance showed the US title in his hands and at his waist).
//
// Why: the superstar's commands (object 0 of the track bank, its blocks' YAT2) of WWE '12's own entrances bracket the
// part where a champion holds his belt with command 29 (type 2, u32 1) and command 30 (the same): The Miz's 218 at
// frames 0 / 714, Cena's 602 at 0 / 164, 82 such in all; entrance 137 has them with 130 (u32 1) and 131 (u32 0) at the
// same frames. WWE 2K14's give 130 / 131 alone: WWE '12 then puts the belt in his hand and keeps it at his waist too.
// What: each 130 with a non-zero value gets a 29 beside it, each 131 with a zero one a 30, in blocks that have none.
// (Two titles held: the game's own commands, as in its own entrances - the user: "two belts if he has two, one if one".)
#pragma once

#include <map>
#include <string>
#include <vector>

#include "entrance13.h"
#include "face12.h"

namespace belt12 {

using Bytes = std::vector<uint8_t>;

namespace detail {

inline uint16_t Be16(const Bytes &d, size_t o) { return o + 2 <= d.size() ? uint16_t(d[o] << 8 | d[o + 1]) : 0; }
inline uint32_t Be32(const Bytes &d, size_t o) { return o + 4 <= d.size() ? svrfmt::Be32(&d[o]) : 0; }
inline void Put16(Bytes &d, uint16_t v) { d.push_back(uint8_t(v >> 8)), d.push_back(uint8_t(v)); }
inline void Put32(Bytes &d, uint32_t v) {
  for (int s = 24; s >= 0; s -= 8)
    d.push_back(uint8_t(v >> s));
}

struct Cmd {
  uint16_t id = 0;
  uint8_t type = 0;
  Bytes data;
};
struct Ev {
  uint16_t frame = 0;
  std::vector<Cmd> cmds;
};

// One block (YANM, POF0, YAT2, ...): its YAT2's events, and where the chunk is (its start and end). False if none.
inline bool ReadYat2(const Bytes &b, std::vector<Ev> &out, size_t &at, size_t &end) {
  out.clear();
  if (b.size() < 16 || memcmp(b.data(), "YANM", 4))
    return false;
  const size_t yend = 8 + size_t(Be32(b, 4));
  if (yend + 8 > b.size() || memcmp(&b[yend], "POF0", 4))
    return false;
  at = yend + 8 + size_t(Be32(b, yend + 4));
  if (at + 8 > b.size() || memcmp(&b[at], "YAT2", 4))
    return false;
  end = at + 8 + size_t(Be32(b, at + 4));
  const size_t body = at + 8;
  if (end > b.size() || body + 12 > end || Be16(b, body) != 1)
    return false;
  const uint16_t n = Be16(b, body + 2);
  const size_t eoff = Be32(b, body + 4), poff = Be32(b, body + 8);
  for (uint16_t i = 0; i < n; ++i) {
    const size_t e = body + eoff + 8 * size_t(i);
    if (e + 8 > end)
      return false;
    Ev ev;
    ev.frame = Be16(b, e);
    size_t p = body + poff + Be16(b, e + 2);
    for (uint16_t c = 0; c < Be16(b, e + 4); ++c) {
      if (p + 4 > end || b[p + 2] < 4 || p + b[p + 2] > end)
        return false;
      Cmd cmd;
      cmd.id = Be16(b, p);
      cmd.type = b[p + 3];
      cmd.data.assign(b.begin() + long(p + 4), b.begin() + long(p + b[p + 2]));
      ev.cmds.push_back(std::move(cmd));
      p += b[p + 2];
    }
    out.push_back(std::move(ev));
  }
  return true;
}

// The YAT2 chunk of `events` (svr11_entrance::Yat2's layout).
inline Bytes WriteYat2(const std::vector<Ev> &events) {
  Bytes ent, pay;
  for (const auto &e : events) {
    Put16(ent, e.frame), Put16(ent, uint16_t(pay.size())), Put16(ent, uint16_t(e.cmds.size())), Put16(ent, 0);
    for (const auto &c : e.cmds) {
      Put16(pay, c.id), pay.push_back(uint8_t(4 + c.data.size())), pay.push_back(c.type);
      pay.insert(pay.end(), c.data.begin(), c.data.end());
    }
  }
  Bytes body;
  Put16(body, 1), Put16(body, uint16_t(events.size())), Put32(body, 12), Put32(body, uint32_t(12 + 8 * events.size()));
  body.insert(body.end(), ent.begin(), ent.end());
  body.insert(body.end(), pay.begin(), pay.end());
  while (body.size() % 4)
    body.push_back(0);
  Bytes out = {'Y', 'A', 'T', '2'};
  Put32(out, uint32_t(body.size()));
  out.insert(out.end(), body.begin(), body.end());
  return out;
}

inline bool Has(const std::vector<Ev> &evs, uint16_t id) {
  for (const auto &e : evs)
    for (const auto &c : e.cmds)
      if (c.id == id)
        return true;
  return false;
}

// A record's blocks given 29 / 30 beside their 130 / 131; how many commands were added.
inline int AddToRecord(Bytes &rec) {
  face12::detail::Rec r;
  if (!face12::detail::Split(rec, r))
    return 0;
  int added = 0;
  for (auto &[name, b] : r.blocks) {
    std::vector<Ev> evs;
    size_t at = 0, end = 0;
    if (!ReadYat2(b, evs, at, end))
      continue;
    const bool has29 = Has(evs, 29), has30 = Has(evs, 30);
    int here = 0;
    for (auto &e : evs) {
      std::vector<Cmd> more;
      for (const auto &c : e.cmds) {
        const uint32_t v = c.data.size() >= 4 ? Be32(c.data, 0) : 0;
        if (c.id == 130 && v != 0 && !has29)
          more.push_back({29, 2, {1, 0, 0, 0}});
        else if (c.id == 131 && v == 0 && !has30)
          more.push_back({30, 2, {1, 0, 0, 0}});
      }
      here += int(more.size());
      e.cmds.insert(e.cmds.end(), more.begin(), more.end());
    }
    if (!here)
      continue;
    Bytes nb(b.begin(), b.begin() + long(at));
    const Bytes y = WriteYat2(evs);
    nb.insert(nb.end(), y.begin(), y.end());
    nb.insert(nb.end(), b.begin() + long(end), b.end());
    b = std::move(nb);
    added += here;
  }
  if (added)
    rec = face12::detail::Join(r);
  return added;
}

}  // namespace detail

// The parts' track banks: the superstar's record (object 0) given 29 / 30 beside 130 / 131 (see above). Returns how
// many commands were added.
inline int BeltInHand12(std::vector<entrance13::Part> &parts) {
  int n = 0;
  for (auto &p : parts) {
    if (p.kind != "EVZ")
      continue;
    std::vector<svrfmt::PachEntry> leaves;
    const bool packed = svrfmt::IsBpe(p.data);
    if (!svrfmt::PachRead(packed ? svrfmt::Unpack(p.data) : p.data, leaves))
      continue;
    int here = 0;
    for (auto &e : leaves) {
      if (e.id != 0)
        continue;
      const bool bpe = svrfmt::IsBpe(e.data);
      Bytes rec = bpe ? svrfmt::Unpack(e.data) : e.data;
      if (const int k = detail::AddToRecord(rec); k > 0) {
        e.data = bpe ? svrfmt::BpeEncode(rec) : rec;
        here += k;
      }
    }
    if (here) {
      const Bytes bank = svrfmt::PachWrite(leaves);
      p.data = packed ? svrfmt::BpeEncode(bank) : bank;
      n += here;
    }
  }
  return n;
}

}  // namespace belt12
