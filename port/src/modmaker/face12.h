// A ported entrance's (or victory scene's) face made to move: the face tracks of a WWE '12 entrance added to the
// superstar's own (2026-11-11, Obs Kenobs on Discord: "no face animations in imported entrances").
//
// Why: in WWE '12 the superstar's motion record (object 0 of the track bank, EVZ) has 95 tracks per segment - the body
// (Bip ...), the fingers, and 34 more (eyes, brows, mouth, jaw: hashed names, #091970e4 #0d187761 #1639dcce ... #fdfea777)
// -; WWE 2K14's same entrance (Cena's 139) has 61, none of the face's, and its game files hold them nowhere else (its
// pac\evt has the scripts and track banks only): its face moved by other means. Played by WWE '12, the face stayed still.
//
// What: the face tracks a WWE '12 entrance has and the ported one hasn't (the superstar's base's entrance, else John
// Cena's 139), each segment's from the donor's segment of the same name (else its first), copied byte for byte into the
// block; the block's own tracks keep their bytes too (only their streams' offsets and the POF0 are made anew).
#pragma once

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "entrance13.h"
#include "yanm.h"

namespace face12 {

using Bytes = std::vector<uint8_t>;

namespace detail {

inline uint32_t Be32(const Bytes &d, size_t o) { return o + 4 <= d.size() ? svrfmt::Be32(&d[o]) : 0; }
inline void PutBe32(Bytes &d, size_t o, uint32_t v) {
  d[o] = uint8_t(v >> 24), d[o + 1] = uint8_t(v >> 16), d[o + 2] = uint8_t(v >> 8), d[o + 3] = uint8_t(v);
}

// A motion record's blocks: (name, bytes from its YANM to the next block).
struct Rec {
  Bytes head;  // bytes 0..first block
  std::vector<std::pair<std::string, Bytes>> blocks;
  std::map<std::string, int> frames;
};
inline bool Split(const Bytes &d, Rec &out) {
  out = {};
  if (d.size() < 0x70)
    return false;
  const uint32_t first = Be32(d, 0x0C), n = Be32(d, 0x48);
  if (n == 0 || n > 64 || first > d.size() || 0x70 + 0x28 * size_t(n) > first)
    return false;
  out.head.assign(d.begin(), d.begin() + long(first));
  std::vector<size_t> at;
  for (uint32_t i = 0; i < n; ++i)
    at.push_back(first + Be32(d, 0x70 + 0x28 * i + 0x24));
  for (uint32_t i = 0; i < n; ++i) {
    const size_t s = at[i], e = i + 1 < n ? at[i + 1] : d.size();
    if (s > e || e > d.size())
      return false;
    out.blocks.push_back({std::string(reinterpret_cast<const char *>(&d[0x70 + 0x28 * i + 4]), 8),
                          Bytes(d.begin() + long(s), d.begin() + long(e))});
  }
  yanm::Record r;
  if (yanm::Read(d, r))
    for (const auto &b : r.blocks)
      out.frames[b.name] = b.frames;
  return true;
}
inline Bytes Join(Rec &r) {
  Bytes out = r.head;
  size_t total = 0;
  for (size_t i = 0; i < r.blocks.size(); ++i) {
    Bytes &b = r.blocks[i].second;
    b.resize((b.size() + 0x1F) & ~size_t(0x1F), 0);  // (each block on a 0x20 boundary, as BuildRecord)
    const size_t ent = 0x70 + 0x28 * i;
    PutBe32(out, ent + 0x24, uint32_t(total));
    const uint16_t units = uint16_t(std::min<size_t>(0xFFFF, (b.size() + 0x1F) / 0x20));
    out[ent + 2] = uint8_t(units >> 8), out[ent + 3] = uint8_t(units);
    total += b.size();
  }
  for (auto &[name, b] : r.blocks)
    out.insert(out.end(), b.begin(), b.end());
  PutBe32(out, 4, uint32_t(total));  // (+4: the blocks' size, as BuildRecord)
  return out;
}

// A block's YANM as its tracks' own bytes (2026-11-11: decoding and writing the tracks again changed a 2K14 body -
// its keys of kind 0x0004, 20 bytes each, unknown to svr11_entrance's writer): each track's descriptor and, for each
// offset field in it (the block's POF0 says where they are), the bytes of the stream it points at (up to the next
// stream). Written again with the streams one after another (4-byte aligned) and their offsets and POF0 made anew.
struct Track {
  uint32_t hash = 0;
  Bytes desc;                                    // {u16 flags, u16 size, u32 hash, body}
  std::vector<std::pair<size_t, Bytes>> streams;  // (offset field's place in desc, its stream)
};
struct Yanm {
  uint8_t ver = 4;
  std::vector<Track> tracks;
  Bytes rest;  // what follows YANM + POF0 in the block (YAT2, POF0)
};
inline bool ReadYanm(const Bytes &b, Yanm &out) {
  out = {};
  if (b.size() < 16 || memcmp(b.data(), "YANM", 4) != 0)
    return false;
  const size_t len = Be32(b, 4), yend = 8 + len;
  if (yend + 8 > b.size() || memcmp(&b[yend], "POF0", 4) != 0)
    return false;
  const size_t plen = Be32(b, yend + 4);
  if (yend + 8 + plen > b.size())
    return false;
  const Bytes p(b.begin() + 8, b.begin() + long(yend));
  if (p.size() < 8 || p[1] != 0)
    return false;  // (a templated block: none in these games' entrances)
  out.ver = p[0];
  const uint16_t cnt = uint16_t(p[2] << 8 | p[3]);
  const auto pof = yanm::ik::detail::PofRead(b, yend + 8, plen);
  std::set<uint32_t> starts;
  for (uint32_t at : pof)
    if (at != 4 && at + 4 <= p.size())
      starts.insert(Be32(p, at));
  starts.insert(uint32_t(p.size()));
  size_t o = Be32(p, 4);
  for (uint16_t i = 0; i < cnt; ++i) {
    if (o + 8 > p.size())
      return false;
    const size_t sz = size_t(p[o + 2] << 8 | p[o + 3]);
    if (sz < 8 || o + sz > p.size())
      return false;
    Track t;
    t.hash = Be32(p, o + 4);
    t.desc.assign(p.begin() + long(o), p.begin() + long(o + sz));
    for (uint32_t at : pof) {
      if (at <= o || at + 4 > o + sz)
        continue;
      const uint32_t so = Be32(p, at);
      const auto next = starts.upper_bound(so);
      if (so > p.size() || next == starts.end())
        return false;
      t.streams.push_back({at - o, Bytes(p.begin() + long(so), p.begin() + long(*next))});
    }
    out.tracks.push_back(std::move(t));
    o += sz;
  }
  out.rest.assign(b.begin() + long(yend + 8 + plen), b.end());
  return true;
}
inline Bytes WriteYanm(const Yanm &y) {
  size_t descs = 0;
  for (const auto &t : y.tracks)
    descs += t.desc.size();
  Bytes p = {y.ver, 0, uint8_t(y.tracks.size() >> 8), uint8_t(y.tracks.size()), 0, 0, 0, 8};
  Bytes streams;
  std::vector<uint32_t> pof = {4};
  for (const auto &t : y.tracks) {
    const size_t at = p.size();
    p.insert(p.end(), t.desc.begin(), t.desc.end());
    for (const auto &[field, s] : t.streams) {
      while (streams.size() % 4)
        streams.push_back(0);
      PutBe32(p, at + field, uint32_t(8 + descs + streams.size()));
      streams.insert(streams.end(), s.begin(), s.end());
      pof.push_back(uint32_t(at + field));
    }
  }
  p.insert(p.end(), streams.begin(), streams.end());
  std::sort(pof.begin(), pof.end());
  Bytes out = {'Y', 'A', 'N', 'M', 0, 0, 0, 0};
  PutBe32(out, 4, uint32_t(p.size()));
  out.insert(out.end(), p.begin(), p.end());
  const Bytes pw = yanm::ik::detail::PofWrite(pof);
  const size_t at = out.size();
  out.insert(out.end(), {'P', 'O', 'F', '0', 0, 0, 0, 0});
  PutBe32(out, at + 4, uint32_t(pw.size()));
  out.insert(out.end(), pw.begin(), pw.end());
  out.insert(out.end(), y.rest.begin(), y.rest.end());  // (YANM, POF0, YAT2, POF0 one after another, as BuildRecord)
  return out;
}

// A track's keys cut after frame `last`: a donor's face longer than the segment made the segment longer (its length is
// its tracks' last key: 167 frames became 935). Per component (yanm::detail::Layout): Euler 8-bit 4 bytes {x y z, u8
// step}, Euler 16-bit and wide positions 8 bytes {..., u16 step}, hinges 2 bytes {v, u8 step} (count u16), positions
// 4 bytes, 0x0004 20 bytes {s16 step, ...}; others left whole.
inline void Cut(Track &t, int last) {
  if (t.desc.size() < 8)
    return;
  const uint16_t flags = uint16_t(t.desc[0] << 8 | t.desc[1]);
  for (const auto &part : yanm::detail::Layout(flags, nullptr)) {
    const size_t field = 8 + part.off;
    auto s = std::find_if(t.streams.begin(), t.streams.end(), [&](const auto &x) { return x.first == field; });
    if (s == t.streams.end() || field + 8 > t.desc.size())
      continue;
    const bool hinge = part.bit == 0x0800 || part.bit == 0x0400 || part.bit == 0x0008;
    size_t size = 0;
    if (part.bit == 0x4000 || part.bit == 0x1000 || part.bit == 0x0010 || part.bit == 0x8000)
      size = 4;
    else if (part.bit == 0x2000 || part.bit == 0x0100)
      size = 8;
    else if (hinge)
      size = 2;
    else if (part.bit == 0x0004)
      size = 20;
    if (!size)
      continue;
    const uint32_t n = hinge ? uint32_t(t.desc[field + 4] << 8 | t.desc[field + 5]) : Be32(t.desc, field + 4);
    uint32_t keep = 0;
    long time = 0;
    for (uint32_t i = 0; i < n && size * (i + 1) <= s->second.size(); ++i) {
      const uint8_t *k = &s->second[size * i];
      const long step = part.bit == 0x0004 ? int16_t(k[0] << 8 | k[1]) : size == 8 ? (k[6] << 8 | k[7]) : k[size - 1];
      time += step;
      if (i > 0 && time > last)
        break;
      keep = i + 1;
    }
    if (keep >= n)
      continue;
    s->second.resize(size * keep);
    if (hinge)
      t.desc[field + 4] = uint8_t(keep >> 8), t.desc[field + 5] = uint8_t(keep);
    else
      PutBe32(t.desc, field + 4, keep);
  }
}

}  // namespace detail

// The superstar's motion record (`mine`: object 0 of a track bank) given the face tracks of `donor` (a WWE '12 record of
// the same kind) it hasn't, copied as they are, cut to each segment's length (a segment longer than the donor's keeps
// the face's last pose to its end). Returns how many blocks got them (0: unchanged).
inline int AddFace(Bytes &mine, const Bytes &donor) {
  using namespace detail;
  Rec r, d;
  if (!Split(mine, r) || !Split(donor, d))
    return 0;
  std::map<std::string, Yanm> donor_y;
  std::string donor_first;
  for (const auto &[name, b] : d.blocks) {
    Yanm y;
    if (name.size() == 8 && name.compare(4, 4, "0000") == 0 && ReadYanm(b, y)) {
      if (donor_first.empty())
        donor_first = name;
      donor_y[name] = std::move(y);
    }
  }
  if (donor_y.empty())
    return 0;
  const uint32_t vector = yanm::NameHash("vector");
  int done = 0;
  for (auto &[name, b] : r.blocks) {
    if (name.size() != 8 || name.compare(4, 4, "0000") != 0)
      continue;  // (the superstar's own segments only: "xxxx0000")
    Yanm y;
    if (!ReadYanm(b, y))
      continue;
    const auto &dn = donor_y.count(name) ? donor_y.at(name) : donor_y.at(donor_first);
    std::set<uint32_t> have;
    bool sorted = true;
    for (size_t i = 0; i < y.tracks.size(); ++i) {
      have.insert(y.tracks[i].hash);
      sorted = sorted && (i == 0 || y.tracks[i - 1].hash <= y.tracks[i].hash);
    }
    // (the face: the donor's tracks this block hasn't - only when most of the face is missing, 20 or more)
    std::vector<Track> face;
    for (const auto &t : dn.tracks)
      if (!have.count(t.hash) && t.hash != vector)
        face.push_back(t);
    if (face.size() < 20)
      continue;
    const auto f = r.frames.find(name);
    if (f == r.frames.end() || f->second < 2)
      continue;
    for (auto &t : face)
      Cut(t, f->second - 1);
    y.tracks.insert(y.tracks.end(), face.begin(), face.end());
    if (sorted)
      std::stable_sort(y.tracks.begin(), y.tracks.end(), [](const Track &a, const Track &c) { return a.hash < c.hash; });
    b = WriteYanm(y);
    ++done;
  }
  if (done)
    mine = Join(r);
  return done;
}

// WWE '12's track bank object 0 (the superstar's record) of entrance `number`, empty if none.
inline Bytes Record12(const disc::GameDisc &w12, int number) {
  const std::string file = entrance13::File12(entrance13::Group12(number));
  Bytes b;
  svrfmt::Archive a;
  if (file.empty() || !w12.Read(file, b) || !svrfmt::ArchiveOpen(b, a, nullptr))
    return {};
  for (auto &k : a.root.kids) {
    if (k.group.rfind("EVZ", 0) != 0 || atoi(k.name.c_str()) != number)
      continue;
    std::vector<svrfmt::PachEntry> leaves;
    const Bytes bank = svrfmt::ArchiveData(k);
    if (svrfmt::PachRead(svrfmt::IsBpe(bank) ? svrfmt::Unpack(bank) : bank, leaves))
      for (auto &e : leaves)
        if (e.id == 0)
          return svrfmt::Unpack(e.data);
  }
  return {};
}

// A ported entrance's or victory scene's parts: each track bank's superstar record (object 0) given the face of WWE '12's
// entrance `donor` (the base's, else Cena's 139). Returns how many blocks got it.
inline int AddFaces(std::vector<entrance13::Part> &parts, const disc::GameDisc &w12, int donor) {
  Bytes face = donor > 0 ? Record12(w12, donor) : Bytes{};
  if (face.empty())
    face = Record12(w12, 139);
  if (face.empty())
    return 0;
  int n = 0;
  for (auto &p : parts) {
    if (p.kind != "EVZ")
      continue;
    std::vector<svrfmt::PachEntry> leaves;
    const bool packed = svrfmt::IsBpe(p.data);
    if (!svrfmt::PachRead(packed ? svrfmt::Unpack(p.data) : p.data, leaves))
      continue;
    bool changed = false;
    for (auto &e : leaves) {
      if (e.id != 0)
        continue;
      const bool bpe = svrfmt::IsBpe(e.data);
      Bytes rec = bpe ? svrfmt::Unpack(e.data) : e.data;
      if (const int k = AddFace(rec, face); k > 0) {
        e.data = bpe ? svrfmt::BpeEncode(rec) : rec;
        n += k;
        changed = true;
      }
    }
    if (changed) {
      const Bytes bank = svrfmt::PachWrite(leaves);
      p.data = packed ? svrfmt::BpeEncode(bank) : bank;
    }
  }
  return n;
}

}  // namespace face12
