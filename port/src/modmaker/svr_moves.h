// Mod Maker: the moves of a SvR 2011 superstar WWE '12 hasn't, made WWE '12 moves (2026-11-08, the "Moveset update").
//
// The move itself is SvR's: its attacker and victim (every segment: x 0, 1, 2, 80...) converted by the entrance body
// conversion (svr11_entrance.h: SvR 2011's 21 bones turned into WWE '12's skeleton, the root on 'Bip'), its facing (the
// "vector" track, recomputed from the converted body: WWE '12 keeps it within 20 degrees of the Bip's heading in 84 % of
// its moves' frames), its own events (misc.pac WAZA/DATA EVT = WWE '12's m3.pac 0/3 + 0/4) and EXH fields (+10 = 0/1 +6,
// +18 = 0/2 +4).
// What SvR has no equivalent for comes from a "model": a WWE '12 move SvR 2011 has too (the same name, animations in
// both), of the same SvR category (WAZE +0x91, then +0x73), the same segments, the closest length, whose SvR animation
// lands on WWE '12's with a half turn or none and a shift (under 15 cm off): it gives the placement of each person (its
// measured turn and shift: pairfit showed the layout depends on the kind of move), the record's header and in-motion
// commands (YAT2, retimed to the new length), the counter branches' stubs (x 90-92), its other m3.pac rows (retimed),
// its WAZE record (name and number changed), its moveset-editor preview script and its Create a Moveset records. As
// Kaiko's moves_port.py does the other way (a 2011 template's record, events and banks).
// Checked offline (tools_local\moves_study\makemove3, VALIDATE: a move both games have converted with another model,
// against WWE '12's own): Pedigree 1, Bip 2 / 1.4 degrees and 6 / 4 cm off, its facing 7 degrees off (the model's: 27).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "game_disc.h"
#include "m3_params.h"
#include "moves_edit.h"
#include "moveset_port.h"
#include "newmoves13.h"
#include "svr11_entrance.h"
#include "svrfmt/archive.h"
#include "svrfmt/pac.h"
#include "yanm.h"

namespace svr_moves {

using svrfmt::Bytes;

namespace detail {

namespace se = svr11_entrance::detail;
constexpr double kPi = 3.14159265358979323846;

inline uint32_t Be32At(const Bytes &d, size_t o) { return uint32_t(d[o]) << 24 | d[o + 1] << 16 | d[o + 2] << 8 | d[o + 3]; }
inline uint16_t Be16At(const Bytes &d, size_t o) { return uint16_t(d[o] << 8 | d[o + 1]); }
inline void PutBe32(Bytes &d, size_t o, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    d[o + size_t(i)] = uint8_t(v >> (24 - 8 * i));
}
inline void PutBe16(Bytes &d, size_t o, uint16_t v) { d[o] = uint8_t(v >> 8), d[o + 1] = uint8_t(v); }
inline int BankId(int number) { return ((number & 0xFF) << 8) | (number >> 8); }  // (WAZE number <-> bank id)

// SvR -> WWE '12 placement of one person: a turn (0 or pi, about the vertical) and a shift (cm), fitted on the roots
struct Fit {
  double ang = 0, tx = 0, tz = 0, err = 1e9;
};
inline Fit Solve(const std::vector<std::array<double, 4>> &pts) {  // (svr x, z) -> (wwe x, z)
  Fit f;
  if (pts.size() < 3)
    return f;
  double msx = 0, msz = 0, mwx = 0, mwz = 0;
  for (auto &p : pts)
    msx += p[0], msz += p[1], mwx += p[2], mwz += p[3];
  const double n = double(pts.size());
  msx /= n, msz /= n, mwx /= n, mwz /= n;
  double best = 1e18;
  for (double snap : {0.0, kPi}) {
    const double c = std::cos(snap), s = std::sin(snap);
    const double tx = mwx - (c * msx - s * msz), tz = mwz - (s * msx + c * msz);
    double e = 0;
    for (auto &p : pts)
      e += std::hypot(c * p[0] - s * p[1] + tx - p[2], s * p[0] + c * p[1] + tz - p[3]);
    if (e < best)
      best = e, f.ang = snap, f.tx = tx, f.tz = tz;
  }
  f.err = best / n;
  return f;
}

// a WWE '12 block's tracks as the body conversion's template (each track's first pose; Bip with a position)
inline void TplFrom(const yanm::Block &b, std::vector<se::TemplateTrack> &out) {
  out.clear();
  for (auto &t : b.tracks) {
    se::TemplateTrack tt;
    tt.hash = t.hash;
    bool hr, hp;
    yanm::Q q{};
    yanm::V p{};
    yanm::Sample(t, 0, hr, q, hp, p);
    if (hr) {
      const double x = q[0], y = q[1], z = q[2], w = q[3];
      const se::M3 M = {1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w),
                        2 * (x * y + z * w),     1 - 2 * (x * x + z * z), 2 * (y * z - x * w),
                        2 * (x * z - y * w),     2 * (y * z + x * w),     1 - 2 * (x * x + y * y)};
      const se::V3 e = se::EulerZyx(M);
      for (int a = 0; a < 3; ++a)
        tt.rot[size_t(a)] = int(se::Round(e[size_t(a)] / se::kPi16));
    }
    tt.has_pos = t.name == "Bip";
    out.push_back(std::move(tt));
  }
}

// a person's root placed as the model's: turned by `turn` (0 or pi), shifted by (tx, tz) cm
inline void Place(se::Raw &raw, double turn, double tx, double tz) {
  const uint32_t hbip = yanm::NameHash("Bip");
  const bool flip = std::fabs(turn) > 1;
  for (auto &t : raw.tracks) {
    if (t.hash != hbip)
      continue;
    if (flip)
      for (auto &k : t.c[se::kRot16].keys) {
        const se::V3 e = {k.v[0] * se::kPi16, k.v[1] * se::kPi16, k.v[2] * se::kPi16};
        const se::M3 R = se::Rotm(e), Y = {-1, 0, 0, 0, 1, 0, 0, 0, -1};
        const se::V3 n = se::EulerZyx(se::Mul3(Y, R));
        for (int a = 0; a < 3; ++a)
          k.v[a] = int(se::Round(n[size_t(a)] / se::kPi16));
      }
    auto &pk = t.c[se::kPos16];
    if (pk.on) {
      if (flip) {
        pk.bias = {-pk.bias[0], pk.bias[1], -pk.bias[2]};
        for (auto &k : pk.keys)
          k.v[0] = -k.v[0], k.v[2] = -k.v[2];
      }
      pk.bias[0] += tx, pk.bias[2] += tz;
    }
  }
}

// The "vector" track: the Bip's z axis heading - 90 degrees (after the placement's half turn), smoothed over +-`win`
// frames (60 Hz), as rotation keys
inline std::vector<se::RotKey> VectorKeys(const std::vector<se::BodyKey> &keys, bool turned, int last, int win) {
  const auto &S = se::Skeletons::Get();
  std::vector<std::pair<int, double>> h;  // (t, heading, unwrapped)
  std::vector<se::M3> loc;
  se::V3 pos;
  for (auto &k : keys) {
    if (!k.has_pose)
      continue;
    S.Solve(k.root, k.bones, loc, pos);
    const se::M3 &M = loc[size_t(S.ib)];  // (the Bip's world rotation: its parent "null" is the identity)
    double a = std::atan2(M[2], M[8]) + (turned ? kPi : 0.0) - kPi / 2;
    const int t = std::min(last, 2 * (k.frame + k.count));
    if (!h.empty()) {
      while (a - h.back().second > kPi)
        a -= 2 * kPi;
      while (a - h.back().second < -kPi)
        a += 2 * kPi;
      if (t <= h.back().first) {
        h.back().second = a;
        continue;
      }
    }
    h.push_back({t, a});
  }
  std::vector<se::RotKey> out;
  if (h.empty() || last < 0)
    return out;
  std::vector<double> per(size_t(last + 1));
  size_t j = 0;
  for (int t = 0; t <= last; ++t) {
    while (j + 1 < h.size() && h[j + 1].first <= t)
      ++j;
    if (t <= h[0].first)
      per[size_t(t)] = h[0].second;
    else if (j + 1 >= h.size())
      per[size_t(t)] = h.back().second;
    else
      per[size_t(t)] =
          h[j].second + (h[j + 1].second - h[j].second) * (t - h[j].first) / double(h[j + 1].first - h[j].first);
  }
  for (int t = 0; t <= last; ++t) {
    double s = 0;
    int n = 0;
    for (int u = std::max(0, t - win); u <= std::min(last, t + win); ++u)
      s += per[size_t(u)], ++n;
    out.push_back({t, se::Eul16(se::Rotm(se::V3{0.0, s / n, 0.0}))});
  }
  return se::ReduceRot(out, 0.5);
}
inline void SetVector(se::Raw &raw, const std::vector<se::RotKey> &ks) {
  if (ks.empty())
    return;
  for (auto &t : raw.tracks)
    if (t.hash == yanm::NameHash("vector")) {
      for (auto &c : t.c)
        c.on = false, c.keys.clear();
      t.flags = 0x2000;
      t.c[se::kRot16].on = true, t.c[se::kRot16].keys = se::RotKeys16(ks);
    }
}

// a block's kept tail (YAT2, POF0...): the first frame of each of its commands (by id)
inline std::map<int, int> CmdFrames(const Bytes &tail) {
  std::map<int, int> out;
  for (size_t o = 0; o + 8 <= tail.size(); o += 4) {
    if (memcmp(&tail[o], "YAT2", 4))
      continue;
    const size_t b = o + 8;
    if (b + 12 > tail.size())
      return out;
    const uint16_t n = Be16At(tail, b + 2);
    const uint32_t eoff = Be32At(tail, b + 4), poff = Be32At(tail, b + 8);
    for (uint16_t i = 0; i < n && b + eoff + 8 * size_t(i) + 6 <= tail.size(); ++i) {
      const size_t e = b + eoff + 8 * size_t(i);
      size_t p = b + poff + Be16At(tail, e + 2);
      for (int c = 0; c < Be16At(tail, e + 4) && p + 4 <= tail.size(); ++c) {
        out.emplace(Be16At(tail, p), Be16At(tail, e));
        p += std::max<size_t>(4, tail[p + 2]);
      }
    }
    return out;
  }
  return out;
}

// a block's kept tail: its commands' frames moved by a piecewise-linear map through `anchors` (old frame -> new frame,
// increasing; the first (0, 0), the last (old end, new end))
inline void Retime(Bytes &tail, const std::vector<std::pair<int, int>> &anchors) {
  auto map = [&](int f) {
    for (size_t i = 1; i < anchors.size(); ++i)
      if (f <= anchors[i].first || i + 1 == anchors.size()) {
        const auto &a = anchors[i - 1], &z = anchors[i];
        return z.first == a.first ? z.second
                                  : int(std::lround(a.second + double(f - a.first) * (z.second - a.second) / (z.first - a.first)));
      }
    return f;
  };
  for (size_t o = 0; o + 8 <= tail.size(); o += 4) {
    if (memcmp(&tail[o], "YAT2", 4))
      continue;
    const size_t b = o + 8;
    if (b + 12 > tail.size())
      return;
    const uint16_t n = Be16At(tail, b + 2);
    const uint32_t eoff = Be32At(tail, b + 4);
    int last = -1;
    for (uint16_t i = 0; i < n && b + eoff + 8 * size_t(i) + 2 <= tail.size(); ++i) {
      const size_t e = b + eoff + 8 * size_t(i);
      int f = std::max(map(Be16At(tail, e)), last);  // (kept in order)
      last = f;
      PutBe16(tail, e, uint16_t(std::clamp(f, 0, 65535)));
    }
    return;
  }
}

// the model's record with blocks' YANM + POF0 replaced (by index; new frame count), their commands retimed from the
// model's frame counts - through the move's own moments where it has them (`own`: block -> command id -> its frame);
// the table's u16 at +2 of each entry = the block's size in 32-byte units
inline Bytes Rebuild(const Bytes &tpl, const std::map<size_t, std::pair<Bytes, int>> &repl,
                     const std::map<size_t, int> &tpl_frames, int motion,
                     const std::map<size_t, std::map<int, int>> &own = {}) {
  const uint32_t first = Be32At(tpl, 12), n = Be32At(tpl, 0x48);
  std::vector<size_t> at, start;
  for (uint32_t i = 0; i < n; ++i)
    at.push_back(first + Be32At(tpl, 0x70 + 0x28 * i + 0x24));
  Bytes out(tpl.begin(), tpl.begin() + long(first));
  for (uint32_t i = 0; i < n; ++i) {
    const size_t s = at[i], e = i + 1 < n ? at[i + 1] : tpl.size();
    while (out.size() % 0x20)
      out.push_back(0);
    start.push_back(out.size());
    PutBe32(out, 0x70 + 0x28 * i + 0x24, uint32_t(out.size() - first));
    const auto r = repl.find(i);
    if (r == repl.end()) {
      out.insert(out.end(), tpl.begin() + long(s), tpl.begin() + long(e));
      continue;
    }
    size_t o = s;
    for (const char *tag : {"YANM", "POF0"})
      if (o + 8 <= e && !memcmp(&tpl[o], tag, 4))
        o += 8 + Be32At(tpl, o + 4);
    Bytes tail(tpl.begin() + long(o), tpl.begin() + long(e));
    const auto old = tpl_frames.find(i);
    if (old != tpl_frames.end() && old->second > 0) {
      std::vector<std::pair<int, int>> anchors = {{0, 0}}, mid;
      if (const auto ow = own.find(i); ow != own.end()) {
        const auto cf = CmdFrames(tail);
        for (const auto &[id, f] : ow->second)
          if (const auto c = cf.find(id); c != cf.end() && c->second > 0 && c->second < old->second && f > 0 &&
                                          f < r->second.second)
            mid.push_back({c->second, f});
        std::sort(mid.begin(), mid.end());
      }
      for (const auto &m : mid)  // (both frames increasing: one out of order is left out)
        if (m.first > anchors.back().first && m.second > anchors.back().second)
          anchors.push_back(m);
      anchors.push_back({old->second, r->second.second});
      Retime(tail, anchors);
    }
    out.insert(out.end(), r->second.first.begin(), r->second.first.end());
    out.insert(out.end(), tail.begin(), tail.end());
  }
  while (out.size() % 0x20)
    out.push_back(0);
  for (uint32_t i = 0; i < n; ++i) {
    const size_t end = i + 1 < n ? start[i + 1] : out.size(), units = (end - start[i]) / 32;
    PutBe16(out, 0x70 + 0x28 * i + 2, uint16_t(std::min<size_t>(units, 65535)));
  }
  PutBe32(out, 4, uint32_t(out.size() - first));
  char num[16] = {};
  snprintf(num, sizeof(num), "%d", motion);
  memcpy(&out[0x4C], num, 12);
  return out;
}

// SvR 2011, read once: its moves' names and records, every move's x 0 attacker / victim (body keys, for the models'
// fit), the wanted moves' motions (all segments, copied), its WAZA/DATA (events, EXH)
struct Svr {
  struct Mo {
    int x = 0, y = 0;
    uint32_t frames = 0;
    Bytes data, T;
  };
  std::map<int, std::string> names;            // by bank id
  std::map<int, Bytes> recs;                   // WAZE records by number
  std::map<int, std::vector<se::BodyKey>> keys[2];  // x 0, y 0 / 1, by bank id (events not kept)
  std::map<int, int> frames[2];
  std::map<int, std::set<int>> segments;       // bank id -> its x (< 90, more than one frame)
  std::map<int, std::vector<Mo>> wanted;       // bank id -> its motions
  Bytes waza;
  void Load(const disc::GameDisc &s, const std::set<int> &want_ids) {
    moveset_port::detail::Game g;
    moveset_port::detail::Load(s, g);
    for (auto &[sid, n] : g.names)
      names[BankId(sid)] = n;
    recs = newmoves13::detail::Records(s);
    Bytes m;
    svrfmt::Epac e;
    if (s.Read("pac/m.pac", m) && svrfmt::EpacRead(m, e)) {
      std::function<void(const Bytes &, int)> walk = [&](const Bytes &b, int depth) {
        if (b.size() >= 4 && !memcmp(b.data(), "YMKs", 4)) {
          std::vector<se::Motion> ms;
          se::Span T;
          if (!se::ReadBank(b, ms, T))
            return;
          std::set<int> here;
          for (auto &mo : ms) {
            if (want_ids.count(mo.id) && !wanted.count(mo.id))
              here.insert(mo.id);
            if (mo.y == 0 && mo.x < 90 && mo.frames > 1)
              segments[mo.id].insert(mo.x);
            if (mo.x != 0 || mo.y > 1 || keys[mo.y].count(mo.id))
              continue;
            std::vector<se::BodyKey> k;
            se::Ctx c;
            if (se::BodyKeys(mo.data, T, k, c) && !k.empty()) {
              for (auto &x : k)
                x.events.clear();  // (spans into the bank, gone after it)
              keys[mo.y][mo.id] = std::move(k), frames[mo.y][mo.id] = int(mo.frames);
            }
          }
          for (int id : here)
            for (auto &mo : ms)
              if (mo.id == id)
                wanted[id].push_back({mo.x, mo.y, mo.frames, se::BytesOf(mo.data), se::BytesOf(T)});
          return;
        }
        std::vector<svrfmt::PachEntry> kids;
        if (depth < 4 && svrfmt::IsPach(b) && svrfmt::PachRead(b, kids))
          for (auto &k : kids)
            walk(svrfmt::Unpack(k.data), depth + 1);
      };
      for (auto &gr : e.groups)
        for (auto &en : gr.entries)
          if (gr.type == "MOT " && en.name.rfind("BM", 0) == 0)
            walk(svrfmt::Unpack(en.data), 0);
    }
    Bytes misc;
    svrfmt::Epac me;
    if (s.Read("pac/misc.pac", misc) && svrfmt::EpacRead(misc, me))
      for (auto &gr : me.groups)
        for (auto &en : gr.entries)
          if (gr.type == "WAZA" && en.name.rfind("DATA", 0) == 0)
            waza = svrfmt::Unpack(en.data);
  }
  // its own events by part (EVT rows {move, part, ..., +8 first event, +10 count}, 4-byte events {kind, frame}) and
  // its EXH row (36 bytes)
  void Own(int id, std::map<int, std::vector<Bytes>> &evs, Bytes &exh) const {
    auto L32 = [](const Bytes &d, size_t o) { return uint32_t(d[o] | d[o + 1] << 8 | d[o + 2] << 16 | uint32_t(d[o + 3]) << 24); };
    auto U16 = [](const Bytes &d, size_t o) { return int(d[o] | d[o + 1] << 8); };
    std::vector<svrfmt::PachEntry> kids;
    if (!svrfmt::PachRead(waza, kids))
      return;
    for (auto &k : kids) {
      std::vector<svrfmt::PachEntry> groups;
      svrfmt::PachRead(svrfmt::Unpack(k.data), groups);
      for (auto &g : groups) {
        const Bytes u = svrfmt::Unpack(g.data);
        if (k.id == 0 && u.size() >= 8 && !memcmp(u.data(), "EXH", 3)) {
          for (uint32_t i = 0; i < L32(u, 4) && 8 + 36 * size_t(i + 1) <= u.size(); ++i)
            if (U16(u, 8 + 36 * size_t(i)) == id && U16(u, 8 + 36 * size_t(i) + 2) == 0)  // (its own row: part 0; the
              // counter branch's (part 0x5A) came last and was taken, 2026-11-09)
              exh.assign(u.begin() + long(8 + 36 * i), u.begin() + long(8 + 36 * (i + 1)));
        } else if (k.id == 1 && u.size() >= 8 && L32(u, 0) == 16) {
          const uint32_t n = L32(u, 4);
          const size_t ev = 8 + 16 * size_t(n);
          for (uint32_t i = 0; i < n && 8 + 16 * size_t(i + 1) <= u.size(); ++i) {
            const size_t o = 8 + 16 * size_t(i);
            if (U16(u, o) != id || evs.count(U16(u, o + 2)))
              continue;
            auto &v = evs[U16(u, o + 2)];
            for (int j = 0; j < u[o + 10]; ++j) {
              const size_t p = ev + 4 * size_t(U16(u, o + 8) + j);
              if (p + 4 <= u.size())
                v.push_back(Bytes(u.begin() + long(p), u.begin() + long(p + 4)));
            }
          }
        }
      }
    }
  }
};

// WWE '12's records of a motion: light / full in m.pac, its preview script (mvMot.pac), its RGRP/MAGG rows (DLC.pac)
inline Bytes Mvmot(const disc::GameDisc &w12, int motion) {
  Bytes mv;
  svrfmt::Archive ma;
  std::vector<svrfmt::PachEntry> scripts;
  if (w12.Read("pac/menu/mvMot.pac", mv) && svrfmt::ArchiveOpen(mv, ma, nullptr) && !ma.root.kids.empty() &&
      svrfmt::PachRead(svrfmt::Unpack(svrfmt::ArchiveData(ma.root.kids[0])), scripts))
    for (auto &e : scripts)
      if (int(e.id) == motion) {
        std::vector<svrfmt::PachEntry> kids;
        if (svrfmt::PachRead(svrfmt::Unpack(e.data), kids) && !kids.empty())
          return svrfmt::Unpack(kids[0].data);
      }
  return {};
}
inline Bytes Magg(const disc::GameDisc &w12, int motion) {
  Bytes dlc, out[2];
  svrfmt::Archive a;
  if (w12.Read("pac/DLC.pac", dlc) && svrfmt::ArchiveOpen(dlc, a, nullptr))
    for (auto &k : a.root.kids) {
      std::vector<svrfmt::PachEntry> kids;
      if (newmoves13::detail::Trim(k.group) != "RGRP" || !svrfmt::PachRead(svrfmt::Unpack(svrfmt::ArchiveData(k)), kids))
        continue;
      for (const auto &kid : kids)
        if (kid.id == 10 || kid.id == 11) {
          const Bytes d = svrfmt::Unpack(kid.data);
          for (size_t o = 0; o + 104 <= d.size(); o += 104)
            if ((d[o] | d[o + 1] << 8) == motion && out[kid.id == 11].empty())
              out[kid.id == 11].assign(d.begin() + long(o), d.begin() + long(o) + 104);
        }
    }
  if (out[0].empty() || out[1].empty())
    return {};
  Bytes r = out[0];
  r.insert(r.end(), out[1].begin(), out[1].end());
  return r;
}

}  // namespace detail

// What happened to each wanted move (for the Mod Maker's report)
struct Note {
  int number = 0;
  std::string name, model, why;  // why: empty when made
};

// The moves `numbers` (SvR 2011 numbers, as its PRO holds them) as WWE '12 moves (newmoves13::Move: number = the SvR
// number, motion = newmoves13::MotionOf(number) until Renumber; low = high, the game reads the light record only).
inline std::vector<newmoves13::Move> Collect(const disc::GameDisc &w12, const disc::GameDisc &s11, const std::set<int> &numbers,
                                             std::vector<Note> *notes = nullptr) {
  using namespace detail;
  std::vector<newmoves13::Move> out;
  if (numbers.empty())
    return out;
  std::set<int> ids;
  for (int n : numbers)
    ids.insert(BankId(n));
  Svr sv;
  sv.Load(s11, ids);
  moveset_port::detail::Game gw;
  moveset_port::detail::Load(w12, gw);
  std::map<std::string, int> w12num;
  for (auto &[id, n] : gw.names)
    w12num.emplace(n, id);
  const auto recs12 = newmoves13::detail::Records(w12);
  moves_edit::MotionIndex mi;
  mi.Build(w12);
  Bytes m3;
  w12.Read("pac/m3.pac", m3);
  // the models' fits, once per model
  struct Model {
    bool ok = false;
    int num = 0;
    Fit f[2];
    Bytes full;
    yanm::Record rec;
    std::set<int> segments;
  };
  std::map<int, Model> models;
  auto model = [&](int id) -> const Model & {
    auto it = models.find(id);
    if (it != models.end())
      return it->second;
    Model &md = models[id];
    const auto n = sv.names.find(id);
    if (n == sv.names.end() || !w12num.count(n->second) || !sv.keys[0].count(id) || !sv.keys[1].count(id))
      return md;
    md.num = w12num[n->second];
    md.full = mi.Full(w12, md.num);
    if (md.full.empty() || !yanm::Read(md.full, md.rec) || !recs12.count(md.num))
      return md;
    const yanm::Track *bip[2] = {nullptr, nullptr};
    for (auto &b : md.rec.blocks) {
      for (int who = 0; who < 2; ++who)
        if (b.name == (who ? "00000001" : "00000000"))
          for (auto &t : b.tracks)
            if (t.name == "Bip")
              bip[who] = &t;
      if (b.name.size() == 8 && atoi(b.name.substr(4).c_str()) == 0 && atoi(b.name.substr(0, 4).c_str()) < 90)
        md.segments.insert(atoi(b.name.substr(0, 4).c_str()));
    }
    if (!bip[0] || !bip[1])
      return md;
    for (int who = 0; who < 2; ++who) {
      std::vector<std::array<double, 4>> pts;
      for (auto &k : sv.keys[who][id]) {
        bool hr, hp;
        yanm::Q q;
        yanm::V p;
        yanm::Sample(*bip[who], float(2 * k.frame), hr, q, hp, p);
        if (hp)
          pts.push_back({0.1 * k.root[0], 0.1 * k.root[2], p[0], p[2]});
      }
      md.f[who] = Solve(pts);
    }
    md.ok = md.f[0].err < 15 && md.f[1].err < 15;
    return md;
  };
  for (int number : numbers) {
    const int want = BankId(number);
    Note note;
    note.number = number;
    note.name = sv.names.count(want) ? sv.names[want] : "#" + std::to_string(number);
    auto fail = [&](const std::string &why) {
      note.why = why;
      if (notes)
        notes->push_back(note);
    };
    const auto mine_it = sv.recs.find(number);
    if (mine_it == sv.recs.end() || !sv.wanted.count(want) || !sv.frames[0].count(want) || !sv.frames[1].count(want)) {
      fail("no animation for two people in SvR 2011");
      continue;
    }
    const Bytes &mine = mine_it->second;
    const int my_frames = sv.frames[0][want];
    // the model: the same SvR category (+0x91), then kind (+0x73), segments and length
    int best = -1;
    long best_score = 0;
    for (auto &[id, k] : sv.keys[0]) {
      if (id == want)
        continue;
      const auto r = sv.recs.find(BankId(id));
      if (r == sv.recs.end() || r->second.size() < 0x92 || r->second[0x91] != mine[0x91])
        continue;
      const Model &md = model(id);
      if (!md.ok)
        continue;
      const long score = (r->second[0x73] == mine[0x73] ? 0 : 100000) +
                         (md.segments == sv.segments[want] ? 0 : 50000) + std::abs(sv.frames[0][id] - my_frames);
      if (best < 0 || score < best_score)
        best = id, best_score = score;
    }
    if (best < 0) {
      fail("no WWE '12 model of its kind");
      continue;
    }
    const Model &md = models[best];
    note.model = sv.names[best];
    // its blocks: every segment of each person (x < 90; the counter branches' stubs stay the model's)
    std::map<size_t, std::pair<Bytes, int>> repl;
    std::map<size_t, int> tf;
    std::map<size_t, std::map<int, int>> own;  // block -> command id -> the move's own frame for it
    int frames0 = 0;
    std::string why;
    for (size_t idx = 0; idx < md.rec.blocks.size(); ++idx) {
      const auto &blk = md.rec.blocks[idx];
      tf[idx] = blk.frames;
      if (blk.name.size() != 8)
        continue;
      const int bx = atoi(blk.name.substr(0, 4).c_str()), by = atoi(blk.name.substr(4).c_str());
      if ((by != 0 && by != 1) || bx >= 90)
        continue;
      const Svr::Mo *mo = nullptr;
      for (auto &x : sv.wanted[want])
        if (x.x == bx && x.y == by && x.frames > 1)
          mo = &x;
      if (!mo)
        continue;
      se::Ctx ctx;
      std::vector<se::BodyKey> bk;
      std::vector<se::TemplateTrack> tpl;
      se::Raw raw;
      // (both people as long as the longer one, the shorter one's last pose held: WWE '12's moves have their two blocks
      // alike - Gory Bomb 2 534 / 534 -, SvR's don't - Batista Bomb 272 / 187 frames; a victim whose motion ended first
      // went into the game's default down state, on his front: he turned over at the end, in game 2026-11-08)
      uint32_t longest = mo->frames;
      for (auto &x : sv.wanted[want])
        if (x.x == bx && (x.y == 0 || x.y == 1))
          longest = std::max(longest, x.frames);
      const int n60 = 2 * int(longest);
      TplFrom(blk, tpl);
      if (!se::BodyKeys(se::SpanOf(mo->data), se::SpanOf(mo->T), bk, ctx) || !se::BodyBlock(tpl, bk, n60, {}, raw, ctx)) {
        why = "its animation couldn't be converted (" + ctx.why + ")";
        break;
      }
      Place(raw, md.f[by].ang, md.f[by].tx, md.f[by].tz);
      SetVector(raw, VectorKeys(bk, std::fabs(md.f[by].ang) > 1, n60 - 1, 8));
      Bytes chunk;
      se::Serialize(raw, chunk, ctx);
      if (!ctx.ok) {
        why = "its animation couldn't be written (" + ctx.why + ")";
        break;
      }
      repl[idx] = {chunk, n60};
      // (its own moments, where the model's commands for them go - SvR in-motion events 38 / 39 / 86 are WWE '12's
      // commands 172 / 173 / 73 (finishers' own commands, 28 each, and an impact; within 3 frames on the moves both
      // games have, evros: 28 % / 33 % - WWE '12 timed them anew -, 87 %): the finisher's effect
      // came a bit late, stretched with the length only, in game 2026-11-08)
      for (auto &k : bk)
        for (auto &e : k.events)
          if (!e.Empty()) {
            const int id = e[0] == 38 ? 172 : e[0] == 39 ? 173 : e[0] == 86 ? 73 : 0;
            if (id && !own[idx].count(id))
              own[idx][id] = 2 * k.frame;
          }
      if (bx == 0 && by == 0)
        frames0 = n60;
    }
    if (!why.empty() || !frames0) {
      fail(why.empty() ? "its attacker wasn't found" : why);
      continue;
    }
    const int motion = newmoves13::MotionOf(number), tmotion = newmoves13::MotionOf(md.num);
    newmoves13::Move mv;
    mv.number = number;
    mv.name = note.name;
    mv.high = Rebuild(md.full, repl, tf, motion, own);
    mv.low = mv.high;
    // its WAZE record: the model's (category bits; the slot's are set later), its name and number
    mv.waze = recs12.at(md.num);
    memset(&mv.waze[0x10], 0, 0x40);
    memcpy(&mv.waze[0x10], mv.name.c_str(), std::min<size_t>(mv.name.size(), 0x3F));
    mv.waze[0x90] = uint8_t(number >> 8), mv.waze[0x91] = uint8_t(number);
    // its m3 rows: the model's (retimed), its own events and EXH fields in place
    int tframes = 0;
    for (auto &b : md.rec.blocks)
      if (b.name == "00000000")
        tframes = b.frames;
    const double k = tframes ? double(frames0) / double(tframes) : 1.0;
    auto rows = m3.empty() ? m3_params::Rows{} : m3_params::Extract(m3, tmotion, k);
    std::map<int, std::vector<Bytes>> evs;
    Bytes exh;
    sv.Own(want, evs, exh);
    if (!evs.empty()) {
      rows.rows["0/3"].clear();
      rows.events.clear();
      // (in WWE '12's order: by segment, then person - 0000 0100 005A 015A, as its moves have them -, not by the part's
      // value - 0000 005A 0100 015A: the victim's list came after the counter branch's; suspected cause of the game
      // choosing his front-down
      // state (animation 1) over his back-down one (0) at the end, Gameplay's probe wwe12_down_probe, 2026-11-09)
      std::vector<int> order;
      for (const auto &[part, list] : evs)
        order.push_back(part);
      std::sort(order.begin(), order.end(), [](int a, int b) { return std::make_pair(a & 0xFF, a >> 8) < std::make_pair(b & 0xFF, b >> 8); });
      for (const int part : order) {
        const auto &list = evs.at(part);
        Bytes r(8, 0);
        // (the part: u16 = segment (0, 90-92 a counter branch) | person << 8 (1 the victim), in SvR and WWE '12 alike;
        // its high byte dropped, the victim's events went to the attacker - among them the one saying how he ends
        // (kinds 0x0325-0x0328, 0x0358-0x0359 of the victim go with how he lies, downev): a Batista Bomb's victim
        // turned over at the end, 2026-11-08)
        r[0] = uint8_t(tmotion), r[1] = uint8_t(tmotion >> 8), r[2] = uint8_t(part), r[3] = uint8_t(part >> 8);
        r[4] = uint8_t(rows.events.size()), r[5] = uint8_t(rows.events.size() >> 8), r[6] = uint8_t(list.size());
        rows.rows["0/3"].push_back(r);
        // (its kinds as WWE '12 has them: SvR's variants of a few effect kinds are one kind in WWE '12 - learned on the
        // 1 491 moves both games name alike, kindmap 2026-11-09: 0BB9 -> 0BB8 73 %, 0BBA -> 0BB8 53 %, 0BBC -> 0BBB
        // 79 %, 0BBD -> 0BBB 78 %, 0BBF -> 0BBE 81 %, 0BC0 -> 0BBE 75 %, 0BC2 / 0BC3 -> 0BC1 85 / 100 %, 0BC5 -> 0BC4
        // 71 %, 0BC6 -> 0BC4 60 %; the others kept, 70-90 % alike. WWE '12's own Batista Bomb has 0BBB where SvR's has
        // 0BBC, its victim's first event)
        static const std::map<int, int> kinds = {{0x0BB9, 0x0BB8}, {0x0BBA, 0x0BB8}, {0x0BBC, 0x0BBB}, {0x0BBD, 0x0BBB},
                                                 {0x0BBF, 0x0BBE}, {0x0BC0, 0x0BBE}, {0x0BC2, 0x0BC1}, {0x0BC3, 0x0BC1},
                                                 {0x0BC5, 0x0BC4}, {0x0BC6, 0x0BC4}};
        for (auto e : list) {
          if (const auto k = kinds.find(e[0] | e[1] << 8); k != kinds.end())
            e[0] = uint8_t(k->second), e[1] = uint8_t(k->second >> 8);
          rows.events.push_back(e);
        }
      }
    }
    if (exh.size() >= 20) {
      for (auto &r : rows.rows["0/1"])
        if (r.size() >= 8 && r[2] == 0 && r[3] == 0)
          r[6] = exh[10], r[7] = exh[11];
      for (auto &r : rows.rows["0/2"])
        if (r.size() >= 6 && r[2] == 0 && r[3] == 0)
          r[4] = exh[18], r[5] = exh[19];
    }
    if (rows.empty()) {
      fail("its model has no motion parameters");
      continue;
    }
    // the frame of its impact: 0/6's attacker row +34 (u16) is its attacker block's command 73 frame (959 of WWE '12's
    // 1 005 moves with one, 980 within 4 frames; FFFF in 808 moves without; f34, 2026-11-09). The model's stayed (Gory
    // Bomb 2's 436 for a Batista Bomb hitting at 308): the finisher's flashes came late, the victim turned over.
    {
      int impact = -1;
      const uint32_t first = Be32At(mv.high, 12), cnt = Be32At(mv.high, 0x48);
      for (uint32_t i = 0; i < cnt && impact < 0; ++i) {
        const size_t e = 0x70 + 0x28 * size_t(i);
        if (e + 0x28 > mv.high.size() || memcmp(&mv.high[e + 4], "00000000", 8) != 0)
          continue;
        size_t s = first + Be32At(mv.high, e + 0x24);
        const size_t end = i + 1 < cnt ? first + Be32At(mv.high, e + 0x28 + 0x24) : mv.high.size();
        for (const char *tag : {"YANM", "POF0"})
          if (s + 8 <= end && !memcmp(&mv.high[s], tag, 4))
            s += 8 + Be32At(mv.high, s + 4);
        if (s < end) {
          const auto cf = CmdFrames(Bytes(mv.high.begin() + long(s), mv.high.begin() + long(end)));
          if (const auto it = cf.find(73); it != cf.end())
            impact = it->second;
        }
      }
      for (auto &r : rows.rows["0/6"])
        if (impact >= 0 && r.size() >= 36 && r[2] == 0 && r[3] == 0 && !(r[34] == 0xFF && r[35] == 0xFF))
          r[34] = uint8_t(impact), r[35] = uint8_t(impact >> 8);
    }
    mv.m3 = m3_params::Write(rows);
    // the model's preview script (its motion number replaced, its "quit" frame scaled) and Create a Moveset rows
    const Bytes script = Mvmot(w12, tmotion);
    std::string s(script.begin(), script.end()), o;
    for (size_t at = 0; at < s.size();) {
      size_t end = s.find('\n', at);
      end = end == std::string::npos ? s.size() : end + 1;
      std::string line = s.substr(at, end - at);
      at = end;
      const std::string from = "motn," + std::to_string(tmotion) + ",";
      if (const size_t p = line.find(from); p != std::string::npos)
        line.replace(p, from.size(), "motn," + std::to_string(motion) + ",");
      if (const size_t q = line.find(",0,quit,"); q != std::string::npos)
        line = std::to_string(int(std::lround(atoi(line.c_str()) * k))) + line.substr(q);
      o += line;
    }
    mv.mvmot.assign(o.begin(), o.end());
    mv.magg = Magg(w12, tmotion);
    for (size_t at : {size_t(0), size_t(104)})
      if (mv.magg.size() >= at + 104) {
        mv.magg[at] = uint8_t(motion), mv.magg[at + 1] = uint8_t(motion >> 8);
        memset(&mv.magg[at + 8], 0, 104 - 8);
        memcpy(&mv.magg[at + 8], mv.name.c_str(), std::min<size_t>(mv.name.size(), 104 - 9));
      }
    out.push_back(std::move(mv));
    if (notes)
      notes->push_back(note);
  }
  return out;
}

}  // namespace svr_moves
