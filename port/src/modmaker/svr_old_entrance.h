// An entrance or victory scene of SvR 2007, 2008, 2009 or 2010 (Xbox 360) made a SvR 2011 one (its script, "0FOP" with
// 2011's 32-byte objects, and a YMKs track bank), byte for byte what Kaiko's tools write (entrance_port.py's
// svr07_convert / svr08_convert / svr09_convert / svr10_convert and --kind victory, svr08_entrance.py, svr10_entrance.py,
// ymks.py; ported with his permission, credit given), without the Create-An-Entrance parts, pyro or titantron. The
// result goes on through svr11_entrance.h (svr_scenes.h does both) like a SvR 2011 entrance.
//
// Where the scenes are (pac\evt, EPAC; SvR 2010 EPK8), entries named by number, script group EVP?, tracks EVT?:
//  - SvR 2007: evt1.pac / evt2.pac, entrances 4000-4362 (evt0 the editor and props, evt3 parts, evt4-8 season / win
//    scenes: no victory converted, as in Kaiko's tools);
//  - SvR 2008 / 2009: Nyujyo0-3.pac, entrances 4000-4424; victories Winning0-3.pac 6000-9300 (TNyujyo / TWinning: tests);
//  - SvR 2010: Nyujyo0-3.pac, entrances 100-840; victories Winning0-3.pac 6100-9400.
// Formats (Kaiko's svr08_entrance.py notes):
//  - the script (big-endian, pointers from offset 8, a POF0 list at the end): 'EV' header, number u16 +0x20, name 30
//    bytes, objects u32 +0x40, table +0x48; an object {idx, model, type[8], -1, 0, u16 slots, u16, block} - 2007-2009
//    idx / model u16 (28 bytes), 2010 / 2011 u32 (32); 212-byte slots {u16 slot, u8 on, u8, u32 track id, ...,
//    u16 parent}; a tail {u16 number, u16 n, records {u16 type, u16, u32, u32 m, items {u8 kind, u24, value}}};
//  - the tracks: 2011's YMKs bank (16-byte header, a 256-byte event size table, a directory {u8 y, u8 x, u16 id, u32
//    offset, u32 frames, u32 rt}, then the motions: streams of records [0x80 | n][n bytes of events][pose]); 2007-2009
//    have it without its 16-byte header, and their event size table differs (op 151 2 bytes, 2011 3...).
// What changes (Yuke's own 2008 -> 2011 / 2010 -> 2011 pairs, Kaiko's rules): track ids + 25000 (slots and bank),
// 2011's size table, every rt 55438460, op 193 '3x' events off the body tracks, x 10..14 tracks of the global effect
// objects copied to x + 50; the entrant's model 16102 (the preview body, the game puts the superstar in), extras
// 3902 -> 13902, title belt 2029 -> 5009, the tail's music record two more u16; 2007: other characters generic, its
// type-9 tail record (a music cue) and op 79 on the face / hand tracks dropped; 2009 / 2010: the stock SvR 2011
// entrances decide the entrant's model (kept when 2011 uses it) and fill object 6's empty positions (templates from
// 2011's 263 and another) - without a SvR 2011 disc, as Kaiko's tools do without the 2011 pac folder: the entrant is
// always 16102 and object 6's positions stay empty. Victory scenes: every character the generic 13902 (the game puts the
// winner, the loser and the referee in). Kaiko's last check: every event block walks 2011's size table (SvR 2009's 4200,
// Cena champion, doesn't: op 208 is 6 bytes there, 2 in 2011 - not converted, as in his tools).
//
// A superstar's own scenes (EntranceOf / VictoryOf, from the games' profiles - CHAR / PRO in pac\ch\chEtc.pac - and
// Kaiko's superstar_port.py): SvR 2007 / 2008 entrance 4000 + id (their profiles have no entrance), victory 6000 + id;
// SvR 2009 the profile's entrance (PRO +0xE4: music, movie, entrance, alternative - 4000 + id - 100 for the most), victory
// 6000 + id - 100; SvR 2010 PRO +0x178 (the same block: the id for the most), victory 6000 + id.
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "game_disc.h"
#include "svrfmt/archive.h"
#include "svrfmt/pac.h"

namespace svr_old_entrance {

using Bytes = std::vector<uint8_t>;

enum : uint32_t {
  kSvr07 = 0x545107E0,
  kSvr08 = 0x5451080B,
  kSvr09 = 0x54510826,
  kSvr10 = 0x54510844,
  kSvr11 = 0x5451085D,
};

namespace detail {

constexpr uint32_t kIdShift = 25000;     // track ids: 2007-2010 0.. -> 2011 25000..
constexpr uint32_t kRt2011 = 55438460;   // the directory's rt (overwritten at run time; 2011's 506's value)
constexpr size_t kSlot = 212;
constexpr uint16_t kNone16 = 9999;       // a slot's parent: none
constexpr uint32_t kGeneric08 = 3902, kGeneric11 = 13902, kPreview11 = 16102;
constexpr size_t kPos0 = 12 + 25 * 4, kPos1 = 12 + 31 * 4, kLast = 12 + 24 * 4;  // object 6: positions, last index
constexpr int kSegs[] = {0, 5, 10, 15, 20};

// SvR 2011's event size table (the variant of 506 and 480 other banks)
inline const uint8_t *T11() {
  static const uint8_t t[256] = {
      1, 2, 3, 5, 7, 0, 2, 1, 0, 0, 3, 0, 0, 0, 0, 0, 4, 4, 6, 0, 8, 6, 3, 8, 0, 2, 0, 4, 0, 0, 0, 0,
      1, 1, 1, 1, 1, 12, 1, 1, 2, 1, 5, 5, 10, 3, 11, 0, 0, 3, 4, 4, 1, 2, 4, 6, 1, 1, 3, 2, 3, 3, 2, 5,
      6, 3, 2, 10, 3, 1, 1, 2, 7, 12, 11, 11, 4, 4, 3, 9, 2, 2, 3, 0, 2, 5, 1, 2, 1, 1, 2, 0, 2, 6, 1, 2,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 4, 4, 1, 3, 2, 3, 2, 2, 1, 3, 4, 2, 1, 2, 2, 3, 1, 3, 2, 2, 3,
      2, 3, 2, 3, 3, 2, 1, 3, 6, 5, 3, 4, 2, 12, 2, 2, 2, 2, 2, 2, 2, 4, 4, 3, 4, 2, 1, 2, 1, 2, 3, 5,
      1, 1, 2, 2, 2, 2, 3, 2, 2, 2, 2, 4, 5, 2, 2, 2, 2, 0, 0, 4, 2, 1, 1, 1, 2, 2, 1, 1, 1, 1, 1, 1,
      2, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 2, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      2, 3, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  return t;
}

inline uint32_t Be32(const Bytes &b, size_t o) { return svrfmt::Be32(&b[o]); }
inline uint16_t Be16(const Bytes &b, size_t o) { return svrfmt::Be16(&b[o]); }

// ---- the script ("0FOP")

struct Obj {
  uint32_t idx = 0, model = 0, t0 = 0, t1 = 0, w4 = 0, w5 = 0;
  uint16_t lo = 0;
  std::vector<Bytes> slots;  // 212 bytes each
  bool Is(uint32_t a, uint32_t b) const { return t0 == a && t1 == b; }
  bool Char() const { return Is(0x01000000, 0); }
  bool Prop() const { return Is(0x00000100, 0); }
  bool Global() const { return Is(0, 0x00010000); }
};
struct Item {
  uint8_t kind = 0;
  uint32_t low = 0, value = 0;  // kind 2: u32, else u16
};
struct Rec {
  uint16_t type = 0, z = 0;
  uint32_t v = 0;
  std::vector<Item> items;
  bool m_kept = false;  // (type 9: one item, its count m kept as read)
  uint32_t m = 0;
};
struct Evp {
  uint16_t num = 0;
  uint8_t name[30] = {};
  uint32_t f10 = 0, f18 = 0, f44 = 0, n_hi = 0;
  std::vector<Obj> objs;
  bool tail = false;
  uint16_t tail_num = 0;
  std::vector<Rec> recs;
};

// svr08_entrance.parse_evp: 2007-2009 (old) with 28-byte objects, 2010 / 2011 with 32-byte ones.
inline bool ParseEvp(const Bytes &d, bool old, Evp &e) {
  e = Evp{};
  if (d.size() < 0x4C || memcmp(d.data(), "0FOP", 4) || d[8] != 'E' || d[9] != 'V')
    return false;
  e.num = Be16(d, 0x20);
  memcpy(e.name, &d[0x22], 30);
  e.f10 = Be32(d, 0x10);
  e.f18 = Be32(d, 0x18);
  e.f44 = Be32(d, 0x44);
  const uint32_t n = Be32(d, 0x40) & 0xFFFF;  // (a 30-character name without its NUL runs into the high half)
  e.n_hi = Be32(d, 0x40) >> 16;
  const size_t t = size_t(Be32(d, 0x48)) + 8, es = old ? 28 : 32;
  for (uint32_t i = 0; i < n; ++i) {
    size_t q = t + es * i;
    if (q + es > d.size())
      return false;
    Obj o;
    if (old) {
      o.idx = Be16(d, q);
      o.model = Be16(d, q + 2);
      q += 4;
    } else {
      o.idx = Be32(d, q);
      o.model = Be32(d, q + 4);
      q += 8;
    }
    o.t0 = Be32(d, q);
    o.t1 = Be32(d, q + 4);
    o.w4 = Be32(d, q + 8);
    o.w5 = Be32(d, q + 12);
    const uint32_t ns = Be32(d, q + 16);
    const size_t bp = size_t(Be32(d, q + 20)) + 8;
    o.lo = uint16_t(ns & 0xFFFF);
    for (uint32_t k = 0; k < ns >> 16; ++k) {
      if (bp + kSlot * (k + 1) > d.size())
        return false;
      o.slots.emplace_back(d.begin() + std::ptrdiff_t(bp + kSlot * k), d.begin() + std::ptrdiff_t(bp + kSlot * (k + 1)));
    }
    e.objs.push_back(std::move(o));
  }
  if (const uint32_t tp0 = Be32(d, 0x1C)) {
    const size_t tp = size_t(tp0) + 8;
    if (tp + 8 > d.size())
      return false;
    e.tail = true;
    e.tail_num = Be16(d, tp);
    const uint32_t cnt = Be16(d, tp + 2);
    const size_t lp = size_t(Be32(d, tp + 4)) + 8;
    for (uint32_t i = 0; i < cnt; ++i) {
      const size_t r = lp + 16 * i;
      if (r + 16 > d.size())
        return false;
      Rec rec;
      rec.type = Be16(d, r);
      rec.z = Be16(d, r + 2);
      rec.v = Be32(d, r + 4);
      const uint32_t m = Be32(d, r + 8);
      const size_t ip = size_t(Be32(d, r + 12)) + 8;
      if (rec.type == 9) {  // (type 9: one item; m is a parameter, kept)
        rec.m_kept = true;
        rec.m = m;
      }
      for (uint32_t j = 0; j < (rec.type == 9 ? 1u : m); ++j) {
        if (ip + 8 * j + 8 > d.size())
          return false;
        const uint32_t kind = Be32(d, ip + 8 * j);
        const size_t vp = size_t(Be32(d, ip + 8 * j + 4)) + 8;
        Item it;
        it.kind = uint8_t(kind >> 24);
        it.low = kind & 0xFFFFFF;
        if (vp + (it.kind == 2 ? 4 : 2) > d.size())
          return false;
        it.value = it.kind == 2 ? Be32(d, vp) : Be16(d, vp);
        rec.items.push_back(it);
      }
      e.recs.push_back(std::move(rec));
    }
  }
  return true;
}

// POF0: the pointers' offsets, as 4-byte steps from offset 8 (1, 2 or 4 bytes each).
inline Bytes Pof0(std::vector<uint32_t> ptrs) {
  std::sort(ptrs.begin(), ptrs.end());
  Bytes out;
  uint32_t prev = 8;
  for (uint32_t o : ptrs) {
    const uint32_t v = (o - prev) / 4;
    if (v < 0x40) {
      out.push_back(uint8_t(0x40 | v));
    } else if (v < 0x4000) {
      out.push_back(uint8_t(0x80 | v >> 8));
      out.push_back(uint8_t(v));
    } else {
      out.push_back(uint8_t(0xC0 | v >> 24));
      out.push_back(uint8_t(v >> 16));
      out.push_back(uint8_t(v >> 8));
      out.push_back(uint8_t(v));
    }
    prev = o;
  }
  while (out.size() % 4)
    out.push_back(0);
  Bytes r = {'P', 'O', 'F', '0'};
  svrfmt::AppBe32(r, uint32_t(out.size()));
  svrfmt::App(r, out);
  return r;
}

// svr08_entrance.build_evp, 2011's layout (32-byte objects; the tail's u16 values before its u32 ones).
inline Bytes BuildEvp(const Evp &e) {
  using svrfmt::PutBe16;
  using svrfmt::PutBe32;
  const size_t n = e.objs.size();
  std::vector<uint32_t> ptrs = {0x14, 0x1C, 0x48};
  Bytes body(0x4C + 32 * n, 0), blocks;
  const size_t pos = body.size();
  for (size_t i = 0; i < n; ++i) {
    const Obj &o = e.objs[i];
    const size_t q = 0x4C + 32 * i;
    PutBe32(&body[q], o.idx);
    PutBe32(&body[q + 4], o.model);
    PutBe32(&body[q + 8], o.t0);
    PutBe32(&body[q + 12], o.t1);
    PutBe32(&body[q + 16], o.w4);
    PutBe32(&body[q + 20], o.w5);
    PutBe32(&body[q + 24], uint32_t(o.slots.size()) << 16 | o.lo);
    PutBe32(&body[q + 28], uint32_t(pos + blocks.size() - 8));
    ptrs.push_back(uint32_t(q + 28));
    for (const auto &s : o.slots)
      svrfmt::App(blocks, s);
  }
  svrfmt::App(body, blocks);
  if (e.tail) {
    const size_t tp = body.size();
    const size_t rec_at = tp + 8, list_at = rec_at + 16 * e.recs.size();
    std::vector<size_t> lists;
    size_t la = list_at;
    for (const auto &r : e.recs) {
      lists.push_back(la);
      la += 8 * r.items.size();
    }
    const size_t val_base = la;
    Bytes vals;
    std::vector<std::vector<size_t>> at(e.recs.size());
    for (size_t ri = 0; ri < e.recs.size(); ++ri) {
      const auto &items = e.recs[ri].items;
      at[ri].resize(items.size());
      std::vector<size_t> order(items.size());
      for (size_t j = 0; j < order.size(); ++j)
        order[j] = j;
      std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return items[a].kind < items[b].kind; });
      for (size_t j : order) {
        const Item &it = items[j];
        if (it.kind == 2) {
          while ((val_base + vals.size()) % 4)
            vals.push_back(0);
          at[ri][j] = val_base + vals.size();
          svrfmt::AppBe32(vals, it.value);
        } else {
          while ((val_base + vals.size()) % 2)
            vals.push_back(0);
          at[ri][j] = val_base + vals.size();
          svrfmt::AppBe16(vals, uint16_t(it.value));
        }
      }
    }
    Bytes tb;
    svrfmt::AppBe16(tb, e.tail_num);
    svrfmt::AppBe16(tb, uint16_t(e.recs.size()));
    svrfmt::AppBe32(tb, uint32_t(tp));
    ptrs.push_back(uint32_t(tp + 4));
    for (size_t ri = 0; ri < e.recs.size(); ++ri) {
      const Rec &r = e.recs[ri];
      svrfmt::AppBe16(tb, r.type);
      svrfmt::AppBe16(tb, r.z);
      svrfmt::AppBe32(tb, r.v);
      svrfmt::AppBe32(tb, r.m_kept ? r.m : uint32_t(r.items.size()));
      svrfmt::AppBe32(tb, uint32_t(lists[ri] - 8));
      ptrs.push_back(uint32_t(rec_at + 16 * ri + 12));
    }
    for (size_t ri = 0; ri < e.recs.size(); ++ri)
      for (size_t j = 0; j < e.recs[ri].items.size(); ++j) {
        const Item &it = e.recs[ri].items[j];
        svrfmt::AppBe32(tb, uint32_t(it.kind) << 24 | it.low);
        svrfmt::AppBe32(tb, uint32_t(at[ri][j] - 8));
        ptrs.push_back(uint32_t(lists[ri] + 8 * j + 4));
      }
    svrfmt::App(tb, vals);
    svrfmt::App(body, tb);
    PutBe32(&body[0x1C], uint32_t(tp - 8));
  }
  while ((body.size() - 8) % 16)
    body.push_back(0);
  const uint32_t size = uint32_t(body.size() - 8);
  memcpy(&body[0], "0FOP", 4);
  PutBe32(&body[4], size);
  memcpy(&body[8], "EV\0\0", 4);
  PutBe32(&body[0xC], size);
  PutBe32(&body[0x10], e.f10);
  PutBe32(&body[0x14], 0x18);
  PutBe32(&body[0x18], e.f18);
  PutBe16(&body[0x20], e.num);
  memcpy(&body[0x22], e.name, 30);
  PutBe32(&body[0x40], uint32_t(n) | e.n_hi << 16);
  PutBe32(&body[0x44], e.f44);
  PutBe32(&body[0x48], 0x44);
  svrfmt::App(body, Pof0(ptrs));
  return body;
}

// The 2011 tail's music record (type 1 with one u32) has two more u16 parameters (all of 2011's 216 tails).
inline void TailTo2011(Evp &e, int number) {
  for (auto &r : e.recs)
    if (r.type == 1 && r.items.size() == 1 && r.items[0].kind == 2) {
      r.items.push_back({1, 0, 0});
      r.items.push_back({1, 0, 0});
    }
  e.tail_num = uint16_t(number);
}

// Every slot's track id + 25000 (false past 32 bits: Kaiko's writer stops there too).
inline bool ShiftSlots(Obj &o) {
  for (auto &s : o.slots) {
    const uint64_t v = uint64_t(Be32(s, 4)) + kIdShift;
    if (v > 0xFFFFFFFFu)
      return false;
    svrfmt::PutBe32(&s[4], uint32_t(v));
  }
  return true;
}

// svr08_entrance.convert_evp (SvR 2007 / 2008): 2008 script -> 2011's (the entrant the preview body).
inline bool ConvertEvp08(Evp &e, int number, std::vector<std::string> *notes) {
  e.num = uint16_t(number);
  e.f44 = 0;  // (52 / 52 of Yuke's pairs: 2008 1 / 2 / 0 -> 2011 0)
  e.f18 = e.tail ? 0x10000 : 0;
  for (size_t i = 0; i < e.objs.size(); ++i) {
    Obj &o = e.objs[i];
    if (o.Char()) {
      uint32_t m;
      if (i == 0)
        m = kPreview11;
      else if (o.model == kGeneric08 && i <= 6)
        m = kPreview11;  // (Yuke's: extras 1..6 follow the preview body)
      else if (o.model == kGeneric08)
        m = kGeneric11;
      else {
        m = o.model < 10000 ? o.model + 10000 : o.model;
        if (notes)
          notes->push_back("object " + std::to_string(i) + ": character model " + std::to_string(o.model) + " -> " +
                           std::to_string(m) + " (check it exists in 2011)");
      }
      o.model = m;
    } else if (o.Prop() && o.model == 2029) {
      o.model = 5009;  // (World Heavyweight Title rolled -> folded, Yuke's 56 / 67 pairs)
    }
    if (!ShiftSlots(o))
      return false;
  }
  if (e.tail)
    TailTo2011(e, number);
  return true;
}

// ---- the stock SvR 2011 entrances (SvR 2009 / 2010's rules)

struct Stock11 {
  bool found = false;
  std::set<uint32_t> obj0_models;                 // the models 2011 gives its entrants (object 0)
  std::map<float, std::vector<Bytes>> templates;  // object 6's positions by its last index (6.0 / 7.0), per segment
};

inline float LastIndex(const Bytes &s) { return svrfmt::BeF(&s[kLast]); }

// svr10_entrance.convert_evp (SvR 2009 / 2010; 2009's script read with 2008's objects).
inline bool ConvertEvp10(Evp &e, int number, const Stock11 &st, std::vector<std::string> *notes) {
  e.num = uint16_t(number);
  e.f44 = 0;
  for (size_t i = 0; i < e.objs.size(); ++i) {
    Obj &o = e.objs[i];
    if (i == 0 && o.Char() && !st.obj0_models.count(o.model)) {
      if (notes)
        notes->push_back("object 0: model " + std::to_string(o.model) + " -> 16102 (preview body)");
      o.model = kPreview11;
    }
    if (o.Prop() && o.model == 2029)
      o.model = 5009;
    if (!ShiftSlots(o))
      return false;
    for (int k : kSegs) {
      if (size_t(k) >= o.slots.size())
        continue;
      Bytes &s = o.slots[size_t(k)];
      if (o.Global() && Be16(s, kSlot - 4) == kNone16)
        svrfmt::PutBe16(&s[kSlot - 4], 0);  // (the global effect object: parent none -> 0 on the segment slots)
      if (!st.templates.empty() && i == 6 && o.Char()) {
        bool empty = true;
        for (size_t p = kPos0; p < kPos1; ++p)
          empty = empty && !s[p];
        const auto t = st.templates.find(LastIndex(s));
        if (empty && t != st.templates.end())
          memcpy(&s[kPos0], t->second[size_t(k) / 5].data(), kPos1 - kPos0);
      }
    }
  }
  if (e.tail)
    TailTo2011(e, number);
  return true;
}

// svr10_entrance.obj6_templates: from 2011's 263 (last index 6.0), then the first other (under 800) with 7.0.
inline void Templates(const std::map<int, Bytes> &stock, Stock11 &st) {
  std::vector<int> order = {263};
  for (const auto &[n, b] : stock)
    if (n != 263 && n < 800)
      order.push_back(n);
  for (int n : order) {
    const auto it = stock.find(n);
    Evp e;
    if (it == stock.end() || !ParseEvp(it->second, false, e) || e.objs.size() < 7)
      continue;
    const Obj &o = e.objs[6];
    if (o.slots.size() != 21 || !o.Char())
      continue;
    const float v = LastIndex(o.slots[0]);
    bool any = false;
    for (size_t p = kPos0; p < kPos1; ++p)
      any = any || o.slots[0][p];
    if ((v == 6.0f || v == 7.0f) && !st.templates.count(v) && any) {
      auto &t = st.templates[v];
      for (int k : kSegs)
        t.emplace_back(o.slots[size_t(k)].begin() + kPos0, o.slots[size_t(k)].begin() + kPos1);
    }
    if (st.templates.size() == 2)
      break;
  }
}

// ---- the track bank (ymks.py)

struct Entry {
  uint8_t y = 0, x = 0;
  uint32_t id = 0, frames = 0, rt = 0;
  Bytes data;
  bool share = false;     // the previous entry's data (the same offset)
  bool tail = false;      // an offset at / past the end of the data (a stock quirk): tail_off past it
  uint32_t tail_off = 0;
};
struct Bank {
  Bytes head;  // 0x110 bytes: 'YMKs', 0x100, 8 zero bytes, the event size table
  std::vector<Entry> entries;
  const uint8_t *T() const { return &head[0x10]; }
};

// ymks.read_bank (the bytes before the first entry's offset aren't kept: Kaiko's reader drops them too).
inline bool ReadBank(const Bytes &raw, Bank &b) {
  if (raw.size() < 0x114 || (memcmp(raw.data(), "YMKs", 4) && memcmp(raw.data(), "YMBs", 4)))
    return false;
  const uint32_t n = svrfmt::Le32(&raw[0x110]);
  const size_t base = 0x114 + 16 * size_t(n);
  if (base > raw.size())
    return false;
  const size_t dl = raw.size() - base;
  std::set<uint32_t> offs;
  for (uint32_t i = 0; i < n; ++i)
    if (const uint32_t o = svrfmt::Le32(&raw[0x114 + 16 * i + 4]); o < dl)
      offs.insert(o);
  offs.insert(0);
  std::map<uint32_t, size_t> next;
  for (auto it = offs.begin(); it != offs.end(); ++it) {
    auto nx = std::next(it);
    next[*it] = nx == offs.end() ? dl : *nx;
  }
  b.head.assign(raw.begin(), raw.begin() + 0x110);
  b.entries.clear();
  int64_t prev = -1;
  for (uint32_t i = 0; i < n; ++i) {
    const uint8_t *p = &raw[0x114 + 16 * i];
    Entry e;
    e.y = p[0];
    e.x = p[1];
    e.id = svrfmt::Le16(p + 2);
    const uint32_t off = svrfmt::Le32(p + 4);
    e.frames = svrfmt::Le32(p + 8);
    e.rt = svrfmt::Le32(p + 12);
    if (off >= dl) {
      e.tail = true;
      e.tail_off = uint32_t(off - dl);
    } else if (int64_t(off) == prev) {
      e.data = b.entries.back().data;
      e.share = true;
    } else {
      e.data.assign(raw.begin() + std::ptrdiff_t(base + off), raw.begin() + std::ptrdiff_t(base + next[off]));
    }
    prev = off;
    b.entries.push_back(std::move(e));
  }
  return true;
}

// ymks.write_bank: data in directory order, a shared entry the previous offset.
inline bool WriteBank(const Bank &b, Bytes &out) {
  Bytes body;
  std::vector<int64_t> offs;
  std::vector<size_t> pend;
  for (size_t i = 0; i < b.entries.size(); ++i) {
    const Entry &e = b.entries[i];
    if (e.tail) {
      pend.push_back(offs.size());
      offs.push_back(-1);
    } else if (e.share && !offs.empty()) {
      if (offs.back() < 0)
        return false;  // (shares a tail entry's offset: Kaiko's writer fails there too)
      offs.push_back(offs.back());
    } else {
      offs.push_back(int64_t(body.size()));
      svrfmt::App(body, e.data);
    }
  }
  for (size_t i : pend)
    offs[i] = int64_t(body.size()) + b.entries[i].tail_off;
  out.assign(b.head.begin(), b.head.begin() + 0x110);
  svrfmt::AppLe32(out, uint32_t(b.entries.size()));
  for (size_t i = 0; i < b.entries.size(); ++i) {
    const Entry &e = b.entries[i];
    if (e.id > 0xFFFF || offs[i] > 0xFFFFFFFFll)
      return false;
    out.push_back(e.y);
    out.push_back(e.x);
    out.push_back(uint8_t(e.id));
    out.push_back(uint8_t(e.id >> 8));
    svrfmt::AppLe32(out, uint32_t(offs[i]));
    svrfmt::AppLe32(out, e.frames);
    svrfmt::AppLe32(out, e.rt);
  }
  svrfmt::App(out, body);
  return true;
}

// A motion's frame stream: a byte < 0x60 skips that many frames; 0x60 + u16 or 0x60 + n: a record of that size.
struct Item2 {
  bool rec = false;
  uint8_t skip = 0;
  Bytes data;
  bool lng = false;  // (its size written 0x60 + u16)
};
inline bool ParseStream(const Bytes &d, std::vector<Item2> &items) {
  items.clear();
  size_t p = 0;
  while (p < d.size()) {
    const uint8_t b = d[p];
    if (b < 0x60) {
      items.push_back({false, b, {}, false});
      ++p;
      continue;
    }
    size_t sz;
    bool lng;
    if (b == 0x60) {
      if (p + 3 > d.size())
        return false;
      sz = d[p + 1] | d[p + 2] << 8;
      p += 3;
      lng = true;
    } else {
      sz = b - 0x60u;
      ++p;
      lng = false;
    }
    if (p + sz > d.size())
      return false;  // (a record overruns the motion)
    items.push_back({true, 0, Bytes(d.begin() + std::ptrdiff_t(p), d.begin() + std::ptrdiff_t(p + sz)), lng});
    p += sz;
  }
  return true;
}
inline Bytes BuildStream(const std::vector<Item2> &items) {
  Bytes out;
  for (const auto &it : items) {
    if (!it.rec) {
      out.push_back(it.skip);
      continue;
    }
    if (it.lng || it.data.empty()) {
      out.push_back(0x60);
      out.push_back(uint8_t(it.data.size()));
      out.push_back(uint8_t(it.data.size() >> 8));
    } else {
      out.push_back(uint8_t(0x60 + it.data.size()));
    }
    svrfmt::App(out, it.data);
  }
  return out;
}
// A record: [0x80 | n][n bytes of events][the pose], or the pose alone.
inline void SplitRecord(const Bytes &r, Bytes &ev, Bytes &pose) {
  ev.clear();
  pose.clear();
  if (r.empty())
    return;
  if (r[0] & 0x80) {
    const size_t n = r[0] & 0x7F, e = std::min(r.size(), 1 + n);
    ev.assign(r.begin() + 1, r.begin() + std::ptrdiff_t(e));
    pose.assign(r.begin() + std::ptrdiff_t(e), r.end());
  } else {
    pose = r;
  }
}
inline Bytes JoinRecord(const Bytes &ev, const Bytes &pose) {
  if (ev.empty())
    return pose;
  Bytes r;
  r.push_back(uint8_t(0x80 | ev.size()));
  svrfmt::App(r, ev);
  svrfmt::App(r, pose);
  return r;
}
// The events of a block, each T[op] bytes long; false when one doesn't walk the table (the rest kept as one).
struct Event {
  int op;  // -1: the bytes that didn't walk
  Bytes b;
};
inline bool ParseEvents(const Bytes &ev, const uint8_t *T, std::vector<Event> &out) {
  out.clear();
  size_t p = 0;
  while (p < ev.size()) {
    const uint8_t op = ev[p], n = T[op];
    if (!n || p + n > ev.size()) {
      out.push_back({-1, Bytes(ev.begin() + std::ptrdiff_t(p), ev.end())});
      return false;
    }
    out.push_back({op, Bytes(ev.begin() + std::ptrdiff_t(p), ev.begin() + std::ptrdiff_t(p + n))});
    p += n;
  }
  return true;
}

// ymks.insert_motion: replaced if the bank has that key, else added where it sorts (id << 16 | x << 8 | y).
inline void InsertMotion(Bank &b, uint32_t id, uint8_t x, uint8_t y, const Bytes &data, uint32_t frames, uint32_t rt) {
  for (auto &e : b.entries)
    if (e.id == id && e.x == x && e.y == y) {
      e.data = data;
      e.frames = frames;
      e.share = false;
      e.tail = false;
      e.tail_off = 0;
      return;
    }
  Entry ne;
  ne.y = y;
  ne.x = x;
  ne.id = id;
  ne.frames = frames;
  ne.rt = rt;
  ne.data = data;
  const uint64_t k = uint64_t(id) << 16 | uint32_t(x) << 8 | y;
  size_t i = 0;
  while (i < b.entries.size() &&
         (uint64_t(b.entries[i].id) << 16 | uint32_t(b.entries[i].x) << 8 | b.entries[i].y) < k)
    ++i;
  if (i < b.entries.size() && b.entries[i].share)
    b.entries[i].share = false;  // (an inserted entry must not lend its data to the next)
  b.entries.insert(b.entries.begin() + std::ptrdiff_t(i), std::move(ne));
}

// Shared entries take their (maybe rebuilt) predecessor's data again.
inline void Relink(Bank &b) {
  for (size_t i = 1; i < b.entries.size(); ++i)
    if (b.entries[i].share)
      b.entries[i].data = b.entries[i - 1].data;
}

// Op `op` events out of the tracks whose y is in ys (entrance_port.drop_ops).
inline bool DropOps(Bytes &evt11, uint8_t op, std::initializer_list<int> ys) {
  Bank b;
  if (!ReadBank(evt11, b))
    return false;
  for (auto &e : b.entries) {
    if (e.data.empty() || e.share || std::find(ys.begin(), ys.end(), int(e.y)) == ys.end())
      continue;
    std::vector<Item2> items;
    if (!ParseStream(e.data, items))
      return false;
    bool changed = false;
    for (auto &it : items) {
      if (!it.rec)
        continue;
      Bytes ev, pose;
      SplitRecord(it.data, ev, pose);
      std::vector<Event> lst;
      if (ev.empty() || !ParseEvents(ev, b.T(), lst))
        continue;
      bool has = false;
      for (const auto &x : lst)
        has = has || x.op == op;
      if (!has)
        continue;
      Bytes keep;
      for (const auto &x : lst)
        if (x.op != op)
          svrfmt::App(keep, x.b);
      it.data = JoinRecord(keep, pose);
      it.lng = it.data.size() > 0x9F;
      changed = true;
    }
    if (changed)
      e.data = BuildStream(items);
  }
  Relink(b);
  return WriteBank(b, evt11);
}

// svr08_entrance.convert_evt: a 2007-2009 bank (no 16-byte header; SvR 2010's has it, taken off) -> 2011's.
inline bool ConvertEvt(Bytes raw, Bytes &out, std::string *why) {
  if (raw.size() >= 4 && !memcmp(raw.data(), "YMKs", 4))
    raw.erase(raw.begin(), raw.begin() + 16);
  if (raw.size() < 4 || raw[0] != 1 || raw[1] != 2 || raw[2] != 3 || raw[3] != 5) {
    if (why)
      *why = "not an entrance track bank";
    return false;
  }
  Bytes full = {'Y', 'M', 'K', 's', 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  svrfmt::App(full, raw);
  Bank b;
  if (!ReadBank(full, b)) {
    if (why)
      *why = "the track bank can't be read";
    return false;
  }
  const Bytes t8(b.head.begin() + 0x10, b.head.begin() + 0x110);
  for (auto &e : b.entries) {
    e.id += kIdShift;
    e.rt = kRt2011;
    if (e.data.empty() || e.share)
      continue;
    std::vector<Item2> items;
    if (!ParseStream(e.data, items)) {
      if (why)
        *why = "a track's record overruns it";
      return false;
    }
    bool changed = false;
    for (auto &it : items) {
      if (!it.rec)
        continue;
      Bytes ev, pose;
      SplitRecord(it.data, ev, pose);
      std::vector<Event> evl;
      if (ev.empty() || !ParseEvents(ev, t8.data(), evl) || e.y != 0)
        continue;
      // (2011 keeps op 193 '3x' events on the y 1..3 tracks only)
      auto lo193 = [](const Bytes &x) { return x[0] == 193 && x.size() > 1 && x[1] >= 0x30 && x[1] <= 0x3F; };
      bool any = false;
      for (const auto &x : evl)
        any = any || lo193(x.b);
      if (!any)
        continue;
      Bytes keep;
      for (const auto &x : evl)
        if (!lo193(x.b))
          svrfmt::App(keep, x.b);
      it.data = JoinRecord(keep, pose);
      it.lng = it.data.size() > 0x9F;
      changed = true;
    }
    if (changed)
      e.data = BuildStream(items);
  }
  Relink(b);
  memcpy(&b.head[0x10], T11(), 256);
  // the global effect objects' (those with x 11..14 tracks) x 10..14 tracks copied to x + 50
  std::set<uint32_t> ids;
  for (const auto &e : b.entries)
    ids.insert(e.id);
  for (uint32_t id : ids) {
    struct Mine {
      uint8_t x, y;
      Bytes data;
      uint32_t frames;
    };
    std::vector<Mine> mine;
    std::set<std::pair<int, int>> have;
    bool fx = false;
    for (const auto &e : b.entries)
      if (e.id == id) {
        mine.push_back({e.x, e.y, e.data, e.frames});
        have.insert({e.x, e.y});
        fx = fx || (e.x >= 11 && e.x <= 14);
      }
    if (!fx)
      continue;
    for (const auto &m : mine)
      if (m.x >= 10 && m.x <= 14 && m.y < 50 && !have.count({m.x + 50, m.y}))
        InsertMotion(b, id, uint8_t(m.x + 50), m.y, m.data, m.frames, kRt2011);
  }
  if (!WriteBank(b, out)) {
    if (why)
      *why = "the track bank can't be written";
    return false;
  }
  return true;
}

// Kaiko's last check (cmd_convert): the script reads as 2011's and every event block walks 2011's size table.
inline bool Check(const Bytes &evp11, const Bytes &evt11, std::string *why) {
  Evp e;
  Bank b;
  if (!ParseEvp(evp11, false, e) || !ReadBank(evt11, b)) {
    if (why)
      *why = "the converted scene doesn't read back";
    return false;
  }
  for (const auto &x : b.entries) {
    if (x.data.empty())
      continue;
    std::vector<Item2> items;
    if (!ParseStream(x.data, items)) {
      if (why)
        *why = "a track's record overruns it";
      return false;
    }
    for (const auto &it : items) {
      if (!it.rec)
        continue;
      Bytes ev, pose;
      SplitRecord(it.data, ev, pose);
      std::vector<Event> evl;
      if (!ev.empty() && !ParseEvents(ev, b.T(), evl)) {
        if (why)
          *why = "an event block doesn't walk 2011's size table in (" + std::to_string(x.id) + "," +
                 std::to_string(x.x) + "," + std::to_string(x.y) + ")";
        return false;
      }
    }
  }
  return true;
}

// ---- the discs

// A name of digits (spaces / NULs around it allowed), else -1.
inline int Number(const std::string &s) {
  size_t a = 0, b = s.size();
  while (a < b && (s[a] == ' ' || s[a] == '\0'))
    ++a;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\0'))
    --b;
  if (a == b || b - a > 6)
    return -1;
  int n = 0;
  for (size_t i = a; i < b; ++i) {
    if (s[i] < '0' || s[i] > '9')
      return -1;
    n = n * 10 + (s[i] - '0');
  }
  return n;
}

// Every entry of a pac (its table only read; then each wanted entry's bytes), unpacked: want(group, number).
template <class F>
inline void Entries(const disc::GameDisc &g, const std::string &file, F &&take) {
  Bytes head;
  const uint64_t size = g.SizeOf(file);
  std::vector<svrfmt::EpacRow> rows;
  if (size < 0x4000 || !g.ReadPart(file, 0, 0x4000, head) || !svrfmt::EpacTable(head, size, rows))
    return;
  for (const auto &r : rows) {
    const int n = Number(r.name);
    if (n < 0 || !take(r.group, n, false))
      continue;
    Bytes raw;
    const uint32_t len = uint32_t(std::min<uint64_t>(r.size, size - std::min<uint64_t>(size, r.offset)));
    if (g.ReadPart(file, r.offset, len, raw))
      take(r.group, n, true, svrfmt::Unpack(raw));
  }
}

// The scene `number`'s script and tracks in the game's scene pacs.
inline bool Scene(const disc::GameDisc &g, uint32_t title, bool victory, int number, Bytes &evp, Bytes &evt) {
  evp.clear();
  evt.clear();
  for (const auto &f : g.List("pac/evt")) {
    std::string l = f;
    for (auto &c : l)
      c = char(tolower(uint8_t(c)));
    // (SvR 2007: evt1 / evt2.pac; else Nyujyo0-3 / Winning0-3.pac, not the tests TNyujyo / TWinning)
    const std::string stem = title == kSvr07 ? "evt" : victory ? "winning" : "nyujyo";
    if (l.size() != stem.size() + 5 || l.compare(0, stem.size(), stem) || l.compare(stem.size() + 1, 4, ".pac") ||
        !isdigit(uint8_t(l[stem.size()])) || (title == kSvr07 && l[3] != '1' && l[3] != '2'))
      continue;
    Entries(g, "pac/evt/" + f, [&](const std::string &grp, int n, bool got, const Bytes &data = {}) {
      if (n != number || n >= 9999 || (grp.rfind("EVP", 0) && grp.rfind("EVT", 0)))
        return false;
      if (got)
        (grp[2] == 'P' ? evp : evt) = data;
      return true;
    });
  }
  return !evp.empty() && !evt.empty();
}

}  // namespace detail

using detail::Stock11;

// The stock SvR 2011 entrances' rules for 2009 / 2010 (pac\evt\Nyujyo0-9,A.pac EVP, numbers under 10000).
inline Stock11 ReadStock11(const disc::GameDisc &svr11) {
  Stock11 st;
  std::map<int, Bytes> stock;
  for (const char *n : {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A"})
    detail::Entries(svr11, std::string("pac/evt/Nyujyo") + n + ".pac",
                    [&](const std::string &grp, int num, bool got, const Bytes &data = {}) {
                      if (grp.rfind("EVP", 0) || num >= 10000)
                        return false;
                      if (got)
                        stock[num] = data;
                      return true;
                    });
  st.found = !stock.empty();
  for (const auto &[n, b] : stock) {
    detail::Evp e;
    if (detail::ParseEvp(b, false, e) && !e.objs.empty())
      st.obj0_models.insert(e.objs[0].model);
  }
  detail::Templates(stock, st);
  return st;
}

// Scene `scene` (an entrance, or a victory scene: SvR 2008-2010) of the game `src` (title id) as SvR 2011's, numbered
// `number` (an entrance 100..999, a victory 6000..7999): its script (evp11) and track bank (evt11). stock11: the stock
// SvR 2011 entrances (ReadStock11), for 2009 / 2010; null without a SvR 2011 disc (said in notes). False (and why) when
// the game hasn't it or Kaiko's tools wouldn't convert it.
inline bool Convert(const disc::GameDisc &src, uint32_t title, int scene, bool victory, int number,
                    const Stock11 *stock11, Bytes &evp11, Bytes &evt11, std::string *why,
                    std::vector<std::string> *notes = nullptr) {
  using namespace detail;
  auto fail = [why](const char *w) {
    if (why && why->empty())
      *why = w;
    return false;
  };
  if (why)
    why->clear();
  if (title != kSvr07 && title != kSvr08 && title != kSvr09 && title != kSvr10)
    return fail("not SvR 2007-2010");
  if (victory && title == kSvr07)
    return fail("SvR 2007's victory scenes aren't converted");
  Bytes evp, evt;
  if (!Scene(src, title, victory, scene, evp, evt))
    return fail("no such scene in its game");
  const bool old = title != kSvr10;  // (2007-2009: 28-byte objects, the bank without its header)
  Evp e;
  if (!ParseEvp(evp, old, e))
    return fail("its script can't be read");
  static const Stock11 none;
  const Stock11 &st = stock11 ? *stock11 : none;
  if ((title == kSvr09 || title == kSvr10) && !st.found && notes)
    notes->push_back("no SvR 2011 disc: object 6 position templates and object 0 model rule skipped");
  if (title == kSvr07) {
    for (size_t i = 1; i < e.objs.size(); ++i)  // (Yuke's own 2007 -> 2011 pair 4252 -> 772: extras are generic)
      if (e.objs[i].Char() && e.objs[i].model != kGeneric08)
        e.objs[i].model = kGeneric08;
    if (!ConvertEvp08(e, number, notes))
      return fail("its script can't be converted");
    // (the 2007-only type-9 tail record, a music cue, dropped)
    std::vector<Rec> keep;
    for (auto &r : e.recs)
      if (r.type != 9)
        keep.push_back(std::move(r));
    e.recs.swap(keep);
  } else if (title == kSvr08) {
    if (!ConvertEvp08(e, number, notes))
      return fail("its script can't be converted");
  } else {
    if (!ConvertEvp10(e, number, st, notes))
      return fail("its script can't be converted");
    if (title == kSvr09 && !e.objs.empty() && e.objs[0].model == kPreview11)
      for (size_t i = 1; i < 7 && i < e.objs.size(); ++i)  // (Yuke's: extras follow the preview body)
        if (e.objs[i].Char() && e.objs[i].model == kGeneric11)
          e.objs[i].model = kPreview11;
  }
  if (!ConvertEvt(evt, evt11, why))
    return false;
  if (title == kSvr07 && !DropOps(evt11, 79, {1, 2, 3}))  // (2007's loop marker on the face / hand tracks)
    return fail("its tracks can't be converted");
  // (victory scenes: every character the generic 13902, as 2011's: the game puts the winner, loser and referee in)
  if (victory)
    for (auto &o : e.objs)
      if (o.Char())
        o.model = kGeneric11;
  // (ours, not Kaiko's) victory scenes laid out as 2011's and WWE '12's: theirs all have 7 characters, 3 props, the
  // camera at 10 and the global object at 11; every SvR 2008-2010 one 6 characters, 2 props, the camera at 8. WWE '12
  // looks for them in their places: SvR 2010 Jeff Hardy's showed the crowd and no superstar (2026-11-02). An unused
  // character (6, as 5) and prop (9, model 0, as WWE '12's) put in, the objects after them and their tracks moved.
  if (victory && e.objs.size() >= 10) {
    bool six = true;
    for (size_t i = 0; i < 10; ++i)
      six = six && (i < 6 ? e.objs[i].Char() : i < 8 ? e.objs[i].Prop() : i == 8 ? e.objs[i].Is(0x10000, 0)
                                                                                : e.objs[i].Global());
    Bank b;
    if (six && ReadBank(evt11, b)) {
      auto to = [](uint32_t n) { return n <= 5 ? n : n <= 7 ? n + 1 : n + 2; };
      for (auto &o : e.objs)
        for (auto &s : o.slots)
          if (s.size() >= 212) {
            if (const uint32_t id = Be32(s, 4); id >= 25000 && id < 25000 + e.objs.size())
              svrfmt::PutBe32(&s[4], 25000 + to(id - 25000));
            if (const uint16_t p = Be16(s, 208); p != 9999 && p < e.objs.size())
              svrfmt::PutBe16(&s[208], uint16_t(to(p)));
          }
      auto unused = [](Obj o, uint32_t n) {  // (its slots off, no tracks)
        for (auto &s : o.slots)
          if (s.size() >= 212) {
            s[2] = 0;
            svrfmt::PutBe32(&s[4], 25000 + n);
          }
        return o;
      };
      Obj extra = unused(e.objs[5], 6), prop = unused(e.objs[7], 9);
      prop.model = 0;
      e.objs.insert(e.objs.begin() + 8, prop);
      e.objs.insert(e.objs.begin() + 6, extra);
      for (size_t i = 0; i < e.objs.size(); ++i)
        e.objs[i].idx = uint32_t(i);
      for (auto &en : b.entries)
        if (en.id >= 25000 && en.id < 25000 + 1000)
          en.id = 25000 + to(en.id - 25000);
      if (!WriteBank(b, evt11))
        return fail("its tracks can't be written");
    }
  }
  evp11 = BuildEvp(e);
  if (!Check(evp11, evt11, why))
    return false;
  return true;
}

// The game's profile of superstar `id` (pac\ch\chEtc.pac, CHAR / PRO: a PACH of records by id), empty if none.
inline Bytes Profile(const disc::GameDisc &g, int id) {
  Bytes c;
  svrfmt::Archive a;
  if (!g.Read("pac/ch/chEtc.pac", c) || !svrfmt::ArchiveOpen(c, a, nullptr))
    return {};
  for (auto &k : a.root.kids) {
    std::string n4 = k.name;
    while (!n4.empty() && (n4.back() == ' ' || n4.back() == '\0'))
      n4.pop_back();
    if (k.group != "CHAR" || n4 != "PRO")
      continue;
    std::vector<svrfmt::PachEntry> kids;
    if (svrfmt::PachRead(svrfmt::Unpack(svrfmt::ArchiveData(k)), kids))
      for (auto &e : kids)
        if (e.id == uint32_t(id))
          return svrfmt::Unpack(e.data);
  }
  return {};
}

// Superstar `id`'s own entrance in its game (title id): SvR 2007 / 2008 4000 + id; SvR 2009 / 2010 its profile's
// (attire 1: PRO +0xE4 / +0x178, the third u16 of {music, movie, entrance, alternative}; without a profile 4000 + id -
// 100 / id); SvR 2011 its own number (svr_scenes.h). 0 if unknown.
inline int EntranceOf(const disc::GameDisc &g, uint32_t title, int id) {
  switch (title) {
    case kSvr07:
    case kSvr08:
      return 4000 + id;
    case kSvr09:
    case kSvr10: {
      const size_t at = title == kSvr09 ? 0xE4 : 0x178;
      const Bytes p = Profile(g, id);
      if (p.size() >= at + 6)
        if (const int e = svrfmt::Le16(&p[at + 4]); e > 0 && e != 4000)  // (4000: the edit slots' "none")
          return e;
      return title == kSvr09 ? 4000 + id - 100 : id;
    }
    case kSvr11:
      return id;
    default:
      return 0;
  }
}

// Its victory scene (Winning*.pac; many superstars have none: the game then plays a generic one): SvR 2008 6000 + id,
// SvR 2009 6000 + id - 100, SvR 2010 / 2011 6000 + id (2011: % 1000, svr_scenes.h); SvR 2007's aren't converted (0).
inline int VictoryOf(uint32_t title, int id) {
  switch (title) {
    case kSvr08:
      return 6000 + id;
    case kSvr09:
      return 6000 + id - 100;
    case kSvr10:
      return 6000 + id;
    case kSvr11:
      return 6000 + id % 1000;
    default:
      return 0;
  }
}

}  // namespace svr_old_entrance
