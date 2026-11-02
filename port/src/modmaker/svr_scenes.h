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
//    entry), and none of 2010's 321 is a WWE '12 one: not WWE '12's format. WWE '12's own Cena's (139, the same bytes
//    as 2011's 139, svr_scenes' fallback) is taken instead, under the entrance's number;
//  - the effects (prtcls.pac): 2007-2009 have another group (PCEN, an older format): none (no pyro). SvR 2010's PCE6
//    are 2011's / WWE '12's format (Kaiko's PYRO_GAMES; every PRTC record has WWE '12's version 0x600 and section sizes
//    288 / 336 / 48 / 48 / 144): taken.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <set>
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

inline bool Convert(const disc::GameDisc &w12, int number, const Bytes &evp11, const Bytes &evt11,
                    std::vector<Part> &out, std::string *why) {
  const Bytes &base = Base12(w12);
  if (base.empty()) {
    if (why)
      *why = "WWE '12's entrance 100 can't be read";
    return false;
  }
  Bytes evz, evp;
  if (!svr11_entrance::Convert(evt11, evp11, base, evz, evp, why))
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
  // (its game event script; none of its own: Cena's, generic, as for the 2K games - an older game's: WWE '12's Cena's)
  Bytes gm = old ? Bytes{} : detail::Entry(svr, "pac/gm.pac", "GMEV", number);
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
// game (title); SvR 2008-2010's scene is svr_old_entrance::VictoryOf's, served as 6000 + number % 1000 too.
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
