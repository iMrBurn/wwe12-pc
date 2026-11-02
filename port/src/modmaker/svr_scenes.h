// A SvR 2011 superstar's own entrance and victory scene, made WWE '12's (svr11_entrance.h: after LA Max's
// SVR11_to_WWE12_Tool and Kaiko's tools, with credit), as entrance13::Part's for the superstar mod:
//  - SvR 2011: pac\evt\Nyujyo0-9,A.pac (EPK8), groups EVP1..K (the script, "0FOP", WWE '12's 212-byte slots in model
//    units) and EVT1..K (a YMKs bank: the tracks); victories in pac\evt\Winning0-3,A.pac, EVPA / EVTA, named 6000 + the
//    superstar's number, as WWE '12's;
//  - its game event script (pac\gm.pac GMEV/NNNN) and effects (pac\prtcls.pac PCE6/NNNN) are WWE '12's format already
//    (GMEV: 175 of the 223 both games have are the same bytes), taken as they are.
// The conversion needs one of WWE '12's own entrances for what 2011 data hasn't (record headers, the tracks of the
// bones 2011 doesn't animate): the Rock's, 100, from the player's WWE '12.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "entrance13.h"
#include "svr11_entrance.h"

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

}  // namespace detail

// SvR 2011 entrance `number`'s parts (script, tracks, game event script, effects), numbered `number`.
inline bool Entrance(const disc::GameDisc &svr, const disc::GameDisc &w12, int number, std::vector<Part> &out,
                     std::string *why) {
  out.clear();
  Bytes evp, evt;
  if (!detail::Find(svr, "Nyujyo", number, evp, evt, "EVT")) {
    if (why)
      *why = "no such entrance in its game";
    return false;
  }
  if (!detail::Convert(w12, number, evp, evt, out, why))
    return false;
  entrance13::FillTracks(out);
  // (its game event script; none of its own: Cena's, generic, as for the 2K games)
  Bytes gm = detail::Entry(svr, "pac/gm.pac", "GMEV", number);
  if (gm.empty())
    gm = detail::Entry(svr, "pac/gm.pac", "GMEV", 139);
  if (!gm.empty())
    out.push_back({number, "GMEV", std::move(gm)});
  if (Bytes fx = detail::Entry(svr, "pac/prtcls.pac", "PCE6", number); !fx.empty())
    out.push_back({number, "PCE6", std::move(fx)});
  entrance13::CameraTwins12(out);
  return true;
}

// Its victory scene (6000 + its number), as entrance13::Victory gives a WWE '13 one.
inline bool Victory(const disc::GameDisc &svr, const disc::GameDisc &w12, int number, std::vector<Part> &out) {
  out.clear();
  const int want = 6000 + number % 1000;
  Bytes evp, evt;
  if (!detail::Find(svr, "Winning", want, evp, evt, "EVT") || !detail::Convert(w12, want, evp, evt, out, nullptr))
    return false;
  entrance13::FillTracks(out);
  entrance13::CameraTwins12(out);
  return out.size() == 2;
}

}  // namespace svr_scenes
