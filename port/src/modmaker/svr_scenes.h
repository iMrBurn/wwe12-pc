// A SvR 2011 superstar's own entrance and victory scene, made WWE '12's (svr11_entrance.h: after LA Max's
// SVR11_to_WWE12_Tool and Kaiko's tools, with credit), as entrance13::Part's for the superstar mod:
//  - SvR 2011: pac\evt\Nyujyo0-9,A.pac (EPK8), groups EVP1..K (the script, "0FOP", WWE '12's 212-byte slots in model
//    units) and EVT1..K (a YMKs bank: the tracks); victories in pac\evt\Winning0-3,A.pac, EVPA / EVTA, named 6000 + the
//    superstar's number, as WWE '12's;
//  - its game event script (pac\gm.pac GMEV/NNNN) and effects (pac\prtcls.pac PCE6/NNNN) are WWE '12's format already
//    (GMEV: 175 of the 223 both games have are the same bytes), taken as they are.
// The conversion needs one of WWE '12's own entrances for what 2011 data hasn't (record headers, the tracks of the
// bones 2011 doesn't animate): the Rock's, 100, from the player's WWE '12.
// SvR 2007-2010's scenes are made SvR 2011's first (svr_old_entrance.h, after Kaiko's tools), then go the same way.
// Theirs beside the scenes (compared with WWE '12's, 2026-11-02):
//  - the game event script (gm.pac GMEV): every 2007-2009 one has an empty PACH where all of WWE '12's (and 2011's)
//    7-part ones have a 417-byte YMKs (part 5); SvR 2010's either that or a part 4 WWE '12 never has (a PACH with one
//    entry), and none of 2010's 321 is a WWE '12 one. But the other parts are WWE '12's layout (2010 Jeff Hardy 111
//    against WWE '12's 535: parts 0-3 the same records, 6 the same bytes), and every WWE '12 one has the same parts 4
//    (an empty PACH) and 5: GmevTo12 keeps its own 0-3 and 6 with WWE '12's 4 and 5 (Cena's, 139). It's what fires
//    the pyro: with Cena's whole script none showed, with this one it did (2026-11-02);
//  - the effects (prtcls.pac): 2007-2009 have another group (PCEN, an older format): none (no pyro). SvR 2010's PCE6
//    are 2011's / WWE '12's format (Kaiko's PYRO_GAMES; every PRTC record has WWE '12's version 0x600 and section sizes
//    288 / 336 / 48 / 48 / 144): taken.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cstring>
#include <map>
#include <set>
#include <tuple>
#include <string>
#include <vector>

#include "entrance13.h"
#include "svr11_entrance.h"
#include "svr_old_entrance.h"

namespace svr_scenes {

using Bytes = std::vector<uint8_t>;
using entrance13::Part;

namespace detail {

// The script and track bank named `name` in a scene pac (groups EVP? and EVT? / EVZ?), unpacked.
inline bool Scene(const disc::GameDisc &g, const std::string &file, int name, Bytes &evp, Bytes &tracks,
                  const char *tracks_group) {
  svrfmt::Archive a;
  if (!entrance13::detail::OpenPac(g, file, a))
    return false;
  for (auto &k : a.root.kids) {
    if (atoi(k.name.c_str()) != name)
      continue;
    if (k.group.rfind("EVP", 0) == 0)
      evp = svrfmt::Unpack(svrfmt::ArchiveData(k));
    else if (k.group.rfind(tracks_group, 0) == 0)
      tracks = svrfmt::Unpack(svrfmt::ArchiveData(k));
  }
  return !evp.empty() && !tracks.empty();
}

inline bool Find(const disc::GameDisc &g, const char *prefix, int name, Bytes &evp, Bytes &tracks,
                 const char *tracks_group) {
  for (const char *n : {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A"})
    if (Scene(g, std::string("pac/evt/") + prefix + n + ".pac", name, evp, tracks, tracks_group))
      return true;
  return false;
}

// WWE '12's entrance 100 (the Rock's) track bank: the conversion's base.
inline const Bytes &Base12(const disc::GameDisc &w12) {
  static Bytes base;
  if (base.empty()) {
    Bytes evp;
    Find(w12, "Nyujyo", 100, evp, base, "EVZ");
  }
  return base;
}

// The entry `name` (4 digits) of group `group` in one of the game's pacs, unpacked; empty if none.
inline Bytes Entry(const disc::GameDisc &g, const char *file, const char *group, int name) {
  svrfmt::Archive a;
  if (!entrance13::detail::OpenPac(g, file, a))
    return {};
  for (auto &k : a.root.kids)
    if (k.group.rfind(group, 0) == 0 && atoi(k.name.c_str()) == name)
      return svrfmt::Unpack(svrfmt::ArchiveData(k));
  return {};
}

// An older game's game event script (7 parts, see above) as WWE '12's: its parts 0-3 and 6 with WWE '12's 4 and 5
// (from `w12gm`, one of its own); empty if either isn't one.
inline Bytes GmevTo12(const Bytes &old, const Bytes &w12gm) {
  std::vector<svrfmt::PachEntry> a, b;
  if (!svrfmt::PachRead(old, a) || !svrfmt::PachRead(w12gm, b) || a.size() != 7 || b.size() != 7)
    return {};
  for (size_t i = 0; i < 7; ++i)
    if (a[i].id != i || b[i].id != i)
      return {};
  a[4].data = b[4].data;
  a[5].data = b[5].data;
  return svrfmt::PachWrite(a);
}

// The props a SvR 2011 script uses (objects of type 0x100), by model.
inline std::set<uint32_t> PropsOf(const Bytes &evp) {
  std::set<uint32_t> out;
  if (evp.size() < 0x50 || memcmp(evp.data(), "0FOP", 4))
    return out;
  const uint32_t n = svrfmt::Be32(&evp[0x40]) & 0xFFFF;
  const size_t t = size_t(svrfmt::Be32(&evp[0x48])) + 8;
  for (uint32_t i = 0; i < n && t + 32 * size_t(i + 1) <= evp.size(); ++i) {
    const size_t q = t + 32 * size_t(i);
    if (svrfmt::Be32(&evp[q + 8]) == 0x100 && svrfmt::Be32(&evp[q + 12]) == 0)
      out.insert(svrfmt::Be32(&evp[q + 4]));
  }
  return out;
}

// Prop donors (svr11_entrance::PropDonors) for `models` from WWE '12's own entrances (Nyujyo0-9,A.pac): the first
// entrance whose script holds one (a segment where it is shown with a parent: slot +0 bits 8-15, slot +208 high half
// not 9999; else one where it is shown) and whose tracks have that segment's block. Sandman's SvR 2008 kendo stick 2108 and beer 2115: WWE '12's
// 506 holds both (2026-11-12). Read once per model.
inline const svr11_entrance::PropDonors &PropDonors12(const disc::GameDisc &w12, const std::set<uint32_t> &models) {
  static svr11_entrance::PropDonors cache;
  static std::set<uint32_t> looked;
  std::set<uint32_t> want;
  for (uint32_t m : models)
    if (!looked.count(m))
      want.insert(m);
  if (want.empty())
    return cache;
  for (uint32_t m : want)
    looked.insert(m);
  static const char kFiles[] = "0123456789A";
  for (size_t fi = 0; fi < 11 && !want.empty(); ++fi) {
    const std::string file = std::string("pac/evt/Nyujyo") + kFiles[fi] + ".pac";
    // (model -> entrance number, object, segment) held in this file's scripts
    std::map<uint32_t, std::tuple<int, uint32_t, uint32_t>> found;
    svr_old_entrance::detail::Entries(w12, file, [&](const std::string &g, int num, bool got, const Bytes &d = {}) {
      if (g.rfind("EVP", 0) || num <= 0 || num >= 1000)
        return false;
      if (!got)
        return true;
      if (d.size() < 0x50 || memcmp(d.data(), "0FOP", 4))
        return false;
      const uint32_t n = svrfmt::Be32(&d[0x40]) & 0xFFFF;
      const size_t t = size_t(svrfmt::Be32(&d[0x48])) + 8;
      for (uint32_t i = 0; i < n && t + 32 * size_t(i + 1) <= d.size(); ++i) {
        const size_t q = t + 32 * size_t(i);
        const uint32_t model = svrfmt::Be32(&d[q + 4]);
        if (svrfmt::Be32(&d[q + 8]) != 0x100 || !want.count(model) || found.count(model))
          continue;
        const uint32_t ns = svrfmt::Be32(&d[q + 24]) >> 16;
        const size_t bp = size_t(svrfmt::Be32(&d[q + 28])) + 8;
        // (held, else shown on a track of its own: Sandman's beer 2115 has no parent in WWE '12's 506)
        int held = -1, shown = -1;
        for (uint32_t k = 0; k < ns && bp + 212 * size_t(k + 1) <= d.size(); ++k) {
          const size_t sl = bp + 212 * size_t(k);
          if (!(svrfmt::Be32(&d[sl]) & 0xFF00))
            continue;
          if (held < 0 && (svrfmt::Be32(&d[sl + 208]) >> 16) != 9999)
            held = int(k);
          if (shown < 0)
            shown = int(k);
        }
        if (held >= 0 || shown >= 0)
          found[model] = {num, i, uint32_t(held >= 0 ? held : shown)};
      }
      return false;
    });
    for (const auto &[model, at] : found) {
      const auto [num, obj, seg] = at;
      Bytes evp, evz;
      if (!detail::Scene(w12, file, num, evp, evz, "EVZ"))
        continue;
      std::vector<svrfmt::PachEntry> recs;
      if (!svrfmt::PachRead(evz, recs))
        continue;
      for (const auto &r : recs) {
        if (r.id != obj)
          continue;
        Bytes rec = svrfmt::Unpack(r.data);
        char name[16];
        snprintf(name, sizeof(name), "%04u0000", seg);
        int kind = -1;
        std::vector<svr11_entrance::detail::BlockRef> bb;
        if (svr11_entrance::detail::Header(rec, kind, bb) &&
            std::any_of(bb.begin(), bb.end(), [&](const auto &b) { return b.name == name; })) {
          cache[model] = {std::move(rec), name};
          want.erase(model);
        }
        break;
      }
    }
  }
  return cache;
}

inline bool Convert(const disc::GameDisc &w12, int number, const Bytes &evp11, const Bytes &evt11,
                    std::vector<Part> &out, std::string *why) {
  const Bytes &base = Base12(w12);
  if (base.empty()) {
    if (why)
      *why = "WWE '12's entrance 100 can't be read";
    return false;
  }
  Bytes evz, evp;
  // (its props held as WWE '12's own entrances hold the same ones, PropDonors12)
  if (!svr11_entrance::Convert(evt11, evp11, base, evz, evp, why, &PropDonors12(w12, PropsOf(evp11))))
    return false;
  out.push_back({number, "EVP", std::move(evp)});
  out.push_back({number, "EVZ", std::move(evz)});
  return true;
}

// A free number for an older game's entrance (4000-4424 there): WWE '12's has none of it, 499 down to 100.
inline int Free12(const disc::GameDisc &w12) {
  std::set<int> have;  // (the Nyujyo files' tables only: entrance13::Exists12 reads a whole pac per number)
  bool read = false;
  for (const char *n : {"0", "1", "2", "3", "4"})
    svr_old_entrance::detail::Entries(w12, std::string("pac/evt/Nyujyo") + n + ".pac",
                                      [&](const std::string &g, int num, bool, const Bytes & = {}) {
                                        read = true;
                                        if (!g.rfind("EVP", 0))
                                          have.insert(num);
                                        return false;
                                      });
  for (int n = 499; read && n >= 100; --n)
    if (!entrance13::Group12(n).empty() && !have.count(n))
      return n;
  return 0;
}

// The older game's scene (svr_old_entrance.h) made SvR 2011's, numbered `as`; stock: SvR 2011's stock entrances, if
// the player has that disc.
inline bool Old(const disc::GameDisc &svr, uint32_t title, int scene, bool victory, int as, const disc::GameDisc *svr11,
                Bytes &evp, Bytes &evt, std::string *why) {
  svr_old_entrance::Stock11 stock;
  if (svr11 && (title == svr_old_entrance::kSvr09 || title == svr_old_entrance::kSvr10))
    stock = svr_old_entrance::ReadStock11(*svr11);
  return svr_old_entrance::Convert(svr, title, scene, victory, as, stock.found ? &stock : nullptr, evp, evt, why);
}

}  // namespace detail

// Entrance `number` of the game `svr` (title: SvR 2011 by default, or SvR 2007-2010 - svr11, the player's SvR 2011 if
// any, gives 2009 / 2010 its stock rules) as WWE '12's parts (script, tracks, game event script, effects). Numbered
// `number` (SvR 2011, 2010); SvR 2007-2009's (4000..) as `as`, else a number WWE '12 hasn't (Free12): the parts say it.
inline bool Entrance(const disc::GameDisc &svr, const disc::GameDisc &w12, int number, std::vector<Part> &out,
                     std::string *why, uint32_t title = svr_old_entrance::kSvr11, const disc::GameDisc *svr11 = nullptr,
                     int as = 0) {
  out.clear();
  Bytes evp, evt;
  const bool old = title != svr_old_entrance::kSvr11;
  if (!old) {
    if (!detail::Find(svr, "Nyujyo", number, evp, evt, "EVT")) {
      if (why)
        *why = "no such entrance in its game";
      return false;
    }
    as = number;
  } else {
    if (!as)
      as = number >= 100 && number < 1000 ? number : detail::Free12(w12);
    if (!as) {
      if (why)
        *why = "no free entrance number in WWE '12";
      return false;
    }
    if (!detail::Old(svr, title, number, false, as, svr11, evp, evt, why))
      return false;
  }
  if (!detail::Convert(w12, as, evp, evt, out, why))
    return false;
  entrance13::FillTracks(out);
  // (its game event script; an older game's made WWE '12's; none of its own: Cena's, generic, as for the 2K games)
  Bytes gm = detail::Entry(svr, "pac/gm.pac", "GMEV", number);
  if (old && !gm.empty())
    gm = detail::GmevTo12(gm, detail::Entry(w12, "pac/gm.pac", "GMEV", 139));
  if (gm.empty())
    gm = detail::Entry(old ? w12 : svr, "pac/gm.pac", "GMEV", 139);
  if (!gm.empty())
    out.push_back({as, "GMEV", std::move(gm)});
  // (its effects: PCE6 and LPE6, both asked for by the game, WWE '12's Jeff Hardy 535 has both; with PCE6 alone no
  // pyro showed, SvR 2010 Jeff Hardy, 2026-11-02)
  if (!old || title == svr_old_entrance::kSvr10)
    for (const char *group : {"PCE6", "LPE6"})
      if (Bytes fx = detail::Entry(svr, "pac/prtcls.pac", group, number); !fx.empty())
        out.push_back({as, group, std::move(fx)});
  entrance13::CameraTwins12(out);
  return true;
}

// Its victory scene (6000 + its number), as entrance13::Victory gives a WWE '13 one. `number`: the superstar's in its
// game (title); SvR 2007-2010's scene is svr_old_entrance::VictoryOf's, served as 6000 + number % 1000 too.
inline bool Victory(const disc::GameDisc &svr, const disc::GameDisc &w12, int number, std::vector<Part> &out,
                    uint32_t title = svr_old_entrance::kSvr11, const disc::GameDisc *svr11 = nullptr) {
  out.clear();
  const int want = 6000 + number % 1000;
  Bytes evp, evt;
  if (title == svr_old_entrance::kSvr11) {
    if (!detail::Find(svr, "Winning", want, evp, evt, "EVT"))
      return false;
  } else {
    const int scene = svr_old_entrance::VictoryOf(title, number);
    if (!scene || !detail::Old(svr, title, scene, true, want, svr11, evp, evt, nullptr))
      return false;
  }
  if (!detail::Convert(w12, want, evp, evt, out, nullptr))
    return false;
  entrance13::FillTracks(out);
  entrance13::CameraTwins12(out);
  return out.size() == 2;
}

}  // namespace svr_scenes
