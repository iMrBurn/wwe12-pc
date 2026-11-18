// A title belt of another WWE game in place of one of WWE '12's (2026-11-18, a player on Discord: "I wanna use the 2013
// WWE title from 2K14"; the user: from any WWE game, and study adding one).
//
// WWE '12's belts are pac\evt\evtobj.pac's SOBJ/NNNN (SOBJ/LIST names them: 64-byte records {u16 LE number, name}),
// several shapes a title: 2NNN / 3NNN in the hand and on the shoulder ("Stretched": a strap rig of bones bone01-15 and
// AW_01-20), 4NNN (17 bones), 20N1-2030 "Folded" / "Rolled" (rigid: root, bone01), 5MMM at the waist ("Folded2": bones
// L_bone01-12 / R_bone01-12). Each is a PACH {0 the JBOY model, 10 its pictures, 30, 100, 200, 1000, 1010}.
//  - WWE '13, 2K14, 2K15: the same, as evtobj.pac's BOBJ/NNNN (2K14: 154 of them; 2004 is the 2013 WWE title, its model
//    "2933_New_WWE_01", 5004 its waist one "5066_New_WWE_04"); a child 4 (a small second model) WWE '12 hasn't: left out.
//    No "Folded" / "Rolled" ones: WWE '12's own stay for those.
//  - 2K16, 2K17: one pac a belt, pac\prop\NNNN.pac (HSPC of one SHDC, children "ZLIB"-packed, as entrance13::Props2K).
//  - SvR 2007-2011: SOBJ in evtobj.pac / _EvtObj.pac, SvR's model format (star_port::SvrPropsTo12 makes it WWE '12's).
// A belt's shapes are found by its model's own name (a node "2933_New_WWE_01": the family "New_WWE") or else by its LIST
// name without " - Stretched" / " - Folded2"...; each of the WWE '12 title's shapes gets the source shape whose bones
// are the same (names, then order, then the same thousand), none if none is close enough (the game's own stays).
// The mod: type=belt, belt\SOBJ_<number>.bin, served in place of the game's (wwe12_arena_pages.cpp, "Title belt mods").
//
// Adding a 24th title isn't possible this way: the game has 23 titles in fixed tables of its save (docs/univers.md
// §3, champions 23 x 2 places) and its menus; a new belt goes in place of a title the player doesn't use (Hardcore,
// Cruiserweight, European...), which can then be given to anyone (EDIT UNIVERSE, Edit Titles).
#pragma once

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "game_disc.h"
#ifndef BELT_PORT_NO_SVR  // (a test build without the Mod Maker around it)
#include "star_port.h"
#endif
#include "svrfmt/archive.h"
#include "svrfmt/pac.h"
#include "svrfmt/texture.h"
#include "svrfmt/xcompress.h"

namespace belt_port {

using Bytes = svrfmt::Bytes;

struct Obj {
  int number = 0;
  std::string label;               // its game's SOBJ/LIST name ("WWE Title - Stretched"), if any
  std::vector<std::string> nodes;  // its model's node names
  std::string model;               // its model's own name ("2933_New_WWE_01"), if it has one
  Bytes pach;                      // its PACH, children as stored (BPE)
  bool bobj = false;               // a BOBJ entry (WWE '13 and the 2K games' belts)
};

struct Family {
  std::string key, label;
  std::vector<size_t> members;  // (indexes in Belts::objs, by number)
  size_t show = 0;              // the member whose picture is shown (a 2NNN one if any)
};

struct Belts {
  std::vector<Obj> objs;
  std::vector<Family> families;
};

namespace detail {

inline std::string Trim(std::string s) {
  while (!s.empty() && (s.back() == ' ' || s.back() == '\0'))
    s.pop_back();
  return s;
}
inline uint32_t Be32(const Bytes &d, size_t o) {
  return o + 4 <= d.size() ? uint32_t(d[o]) << 24 | uint32_t(d[o + 1]) << 16 | uint32_t(d[o + 2]) << 8 | d[o + 3] : 0u;
}
inline bool Digits(const std::string &s) {
  return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}
inline std::string Lower(std::string s) {
  for (char &c : s)
    c = char(std::tolower(uint8_t(c)));
  return s;
}

// A PACH's child `id`, unpacked; empty if none.
inline Bytes Child(const Bytes &pach, uint32_t id) {
  std::vector<svrfmt::PachEntry> kids;
  if (!svrfmt::PachRead(pach, kids))
    return {};
  for (const auto &k : kids)
    if (k.id == id)
      return svrfmt::IsBpe(k.data) ? svrfmt::Unpack(k.data) : k.data;
  return {};
}

// The node names of a JBOY (header: count at +0x20, table at 8 + +0x28, 80-byte nodes, name first; the same in SvR's
// and WWE '12's layouts).
inline std::vector<std::string> Nodes(const Bytes &m) {
  std::vector<std::string> out;
  if (m.size() < 0x48 || memcmp(m.data(), "JBOY", 4))
    return out;
  const uint32_t n = Be32(m, 0x20);
  const size_t t = 8 + size_t(Be32(m, 0x28));
  for (uint32_t i = 0; i < n && i < 512 && t + 80 * size_t(i + 1) <= m.size(); ++i) {
    std::string s;
    for (size_t k = 0; k < 16 && m[t + 80 * i + k]; ++k)
      s += char(m[t + 80 * i + k]);
    out.push_back(s);
  }
  return out;
}

// "2933_New_WWE_01" -> "New_WWE"; "" if the name isn't so.
inline std::string ModelKey(const std::string &model) {
  size_t a = 0;
  while (a < model.size() && std::isdigit(uint8_t(model[a])))
    ++a;
  if (a == 0 || a >= model.size() || model[a] != '_')
    return "";
  std::string k = model.substr(a + 1);
  if (const size_t u = k.rfind('_'); u != std::string::npos && u + 1 < k.size() && Digits(k.substr(u + 1)))
    k = k.substr(0, u);
  return k;
}

// "WWE Title - Stretched" -> "WWE Title"
inline std::string LabelKey(std::string label) {
  for (const char *s : {" - Stretched", " - Folded2", " - Folded", " - Rolled"})
    if (const size_t at = label.find(s); at != std::string::npos)
      label = label.substr(0, at);
  label = Trim(label);
  while (!label.empty() && label.back() == ' ')
    label.pop_back();
  return label;
}

// Whether a model is a belt's strap (in the hand or at the waist): its bones say so (a hand one's AW_01-20, a waist
// one's L_bone01-12 and R_bone01-12; SvR 2007-2010's hand ones only bone01-12; 2K14's ATV has an AW_01 too).
inline bool Strap(const std::vector<std::string> &nodes, const std::string &label = "", bool bobj = false) {
  int aw = 0, l = 0, r = 0, bone = 0;
  for (const auto &n : nodes)
    aw += n.rfind("AW_", 0) == 0, l += n.rfind("L_bone", 0) == 0, r += n.rfind("R_bone", 0) == 0,
        bone += n.size() == 6 && n.rfind("bone", 0) == 0 && Digits(n.substr(4));
  // (bone01-12 alone: a title's only, by its name or its BOBJ group - 2K16 / 2K17's, bone01-14; ropes and other
  // props have such bones too)
  const std::string low = Lower(label);
  const bool title = low.find("title") != std::string::npos || low.find("champion") != std::string::npos ||
                     low.find("belt") != std::string::npos;
  return aw >= 10 || (l >= 6 && r >= 6) || (bone >= 10 && (title || bobj));
}

// A PACH's children as WWE '12 reads them: 2K16 / 2K17's "ZLIB" ones inflated and BPE-packed (WWE '12 reads no ZLIB).
inline Bytes Wwe12Children(const Bytes &pach) {
  std::vector<svrfmt::PachEntry> kids;
  if (!svrfmt::PachRead(pach, kids))
    return pach;
  bool zlib = false;
  for (auto &k : kids)
    if (k.data.size() >= 16 && !memcmp(k.data.data(), "ZLIB", 4)) {
      k.data = svrfmt::BpeEncodeStored(svrfmt::Unpack(k.data));
      zlib = true;
    }
  return zlib ? svrfmt::PachWrite(kids) : pach;
}

// How close source model `s` is to WWE '12's `t` for its place (0 = not at all): the share of t's bones s has (names
// that are numbers left out: WWE '12's models have a node named by their number), a little more when they are in the
// same order and when both are in the same thousand (2NNN / 3NNN have the same bones in other orders). A 4NNN gets a
// 2NNN / 3NNN (2K14 has none): its bones are theirs but bone15, the strap's AW_ bones stay as modelled.
inline double Score(const Obj &t, const Obj &s) {
  std::vector<std::string> a, b;
  for (const auto &n : t.nodes)
    if (!Digits(n) && n != t.model)
      a.push_back(n);
  for (const auto &n : s.nodes)
    if (!Digits(n) && n != s.model)
      b.push_back(n);
  if (a.empty() || b.empty())
    return 0;
  const std::set<std::string> bs(b.begin(), b.end());
  size_t common = 0;
  for (const auto &n : a)
    common += bs.count(n);
  // (most bones of the smaller one in the other, and not much smaller: 4NNN's 16 bones are 2K14's 2NNN's but 15 of 35,
  // kept; SvR 2010's hand one's 14 are 13 of WWE '12's 36, kept; a rigid folded one's 2 of a strap's 35, not)
  const double share = double(common) / double(std::min(a.size(), b.size()));
  if (share < 0.85 || double(common) / double(std::max(a.size(), b.size())) < 0.3)
    return 0;
  return share + (a == b ? 0.25 : 0.0) + (t.number / 1000 == s.number / 1000 ? 0.5 : 0.0) + (t.number == s.number ? 0.1 : 0.0);
}

inline bool OpenPac(const disc::GameDisc &g, const std::string &f, Bytes &b, svrfmt::Archive &a) {
  if (!g.Read(f, b))
    return false;
  if (svrfmt::IsXcompress(b)) {
    Bytes o;
    if (!svrfmt::XcompressDecode(b, o))
      return false;
    b.swap(o);
  }
  return svrfmt::ArchiveOpen(b, a, nullptr);
}

}  // namespace detail

// The belts of a game (its disc `g`; `svr`: an SvR game, its models made WWE '12's). Families sorted by label. `ref`:
// WWE '12's, whose names a belt without one of its own borrows by number (2K14 names its 2NNN only; its 5001-5031 are
// WWE '12's numbers: 5004 the WWE title at the waist).
inline Belts Read(const disc::GameDisc &g, bool svr, const Belts *ref = nullptr) {
  using namespace detail;
  Belts out;
  std::map<int, std::string> labels;
  std::map<int, Bytes> found;  // number -> PACH (the first file that has it wins: the disc's, then its DLC's)
  std::set<int> bobj;          // (those of a BOBJ group: belts)
  // (SvR 2008-2010: _EvtObj.pac; SvR 2007: evt0.pac's SOBJ)
  std::vector<std::string> files = {"pac/evt/evtobj.pac", "pac/evt/_EvtObj.pac", "pac/evt/evt0.pac"};
  for (const auto &f : g.List("pac/evt"))
    if (Lower(f).rfind("evtobj_dlc", 0) == 0)
      files.push_back("pac/evt/" + f);
  // 2K16 / 2K17: one pac a prop, pac\prop\NNNN.pac (a BOBJ / SOBJ entry, its children "ZLIB"-packed; or an HSPC of
  // one SHDC, as entrance13::Props2K reads)
  std::vector<std::string> props;
  for (const auto &f : g.List("pac/prop"))
    if (const int number = atoi(f.c_str()); number >= 2000 && number <= 5999)
      props.push_back("pac/prop/" + f);
  files.insert(files.end(), props.begin(), props.end());
  for (const auto &f : files) {
    Bytes b;
    svrfmt::Archive a;
    if (!OpenPac(g, f, b, a))
      continue;
    if (a.root.kind == svrfmt::ArcKind::Hspc) {
      const int number = atoi(f.c_str() + 9);
      if (a.root.kids.size() != 1 || found.count(number) || !svrfmt::ArchiveExpand(a.root.kids[0]))
        continue;
      std::vector<svrfmt::PachEntry> leaves;
      for (const auto &k : a.root.kids[0].kids) {
        Bytes d = svrfmt::ArchiveData(k);
        if (!d.empty())
          leaves.push_back({k.id, svrfmt::BpeEncodeStored(d)});
      }
      if (!leaves.empty())
        found[number] = svrfmt::PachWrite(leaves);
      continue;
    }
    for (auto &k : a.root.kids) {
      const std::string group = Trim(k.group), name = Trim(k.name);
      if (group != "SOBJ" && group != "BOBJ")
        continue;
      if (name == "LIST") {
        const Bytes list = svrfmt::Unpack(svrfmt::ArchiveData(k));
        for (size_t at = 0; at + 64 <= list.size(); at += 64) {
          std::string n(reinterpret_cast<const char *>(&list[at + 2]), 62);
          n = Trim(n.substr(0, n.find('\0')));
          if (!n.empty())
            labels.emplace(list[at] | list[at + 1] << 8, n);
        }
        continue;
      }
      const int number = atoi(name.c_str());
      if (number < 2000 || number > 5999 || found.count(number))
        continue;
      if (group == "BOBJ")
        bobj.insert(number);
      found[number] = Wwe12Children(svrfmt::Unpack(svrfmt::ArchiveData(k)));
    }
  }
  for (auto &[number, pach] : found) {
    Obj o;
    o.number = number;
    o.bobj = bobj.count(number) != 0;
    if (const auto l = labels.find(number); l != labels.end())
      o.label = l->second;
    else if (const auto l2 = labels.find(2000 + number % 1000); number / 1000 != 5 && l2 != labels.end())
      o.label = l2->second;  // (3NNN / 4NNN: its 2NNN's)
    else if (ref)
      for (const auto &r : ref->objs)
        if (r.number == number)
          o.label = r.label;
    o.nodes = Nodes(Child(pach, 0));
    for (const auto &n : o.nodes)
      if (!ModelKey(n).empty())
        o.model = n;
    if (!Strap(o.nodes, o.label, o.bobj) && Lower(o.label).find("title") == std::string::npos &&
        Lower(o.label).find("champion") == std::string::npos)
      continue;
    // (its pictures there: 2K14's 2926-2930 / 5059-5063 have an empty child 10, theirs are elsewhere)
    if (Child(pach, 10).size() < 1024)
      continue;
    o.pach = std::move(pach);
    out.objs.push_back(std::move(o));
  }
  std::map<std::string, size_t> by_key;
  for (size_t i = 0; i < out.objs.size(); ++i) {
    const Obj &o = out.objs[i];
    std::string key = ModelKey(o.model);
    if (key.empty())
      key = LabelKey(o.label);
    if (key.empty())  // (2NNN, 3NNN, 4NNN of one belt; a 5MMM alone without a name)
      key = "#" + std::to_string(o.number / 1000 == 5 ? o.number : o.number % 1000);
    const std::string k = Lower(key);
    auto at = by_key.find(k);
    if (at == by_key.end()) {
      Family f;
      f.key = k;
      at = by_key.emplace(k, out.families.size()).first;
      out.families.push_back(f);
    }
    out.families[at->second].members.push_back(i);
  }
  for (auto &f : out.families) {
    // its name: its model's ("New WWE") with its LIST name when there is one ("WWE Title")
    std::string model, list;
    for (size_t m : f.members) {
      const Obj &o = out.objs[m];
      if (model.empty() && !ModelKey(o.model).empty())
        model = ModelKey(o.model);
      if (list.empty() && !o.label.empty())
        list = LabelKey(o.label);
    }
    std::replace(model.begin(), model.end(), '_', ' ');
    f.show = f.members.front();
    for (size_t m : f.members)
      if (out.objs[m].number / 1000 == 2 && Strap(out.objs[m].nodes, out.objs[m].label, out.objs[m].bobj)) {
        f.show = m;
        break;
      }
    f.label = !list.empty() ? (model.empty() || Lower(model) == Lower(list) ? list : list + " (" + model + ")")
              : !model.empty() ? model
                               : "Belt " + std::to_string(out.objs[f.show].number);
  }
  // (the families with a strap in the hand: a belt one holds; the rest - a lone folded one, a waist one alone (2K14's
  // 5032-5052, whose hand ones aren't known) - left out)
  out.families.erase(std::remove_if(out.families.begin(), out.families.end(),
                                    [&](const Family &f) {
                                      return std::none_of(f.members.begin(), f.members.end(), [&](size_t m) {
                                        return out.objs[m].number / 1000 != 5 && Strap(out.objs[m].nodes, out.objs[m].label, out.objs[m].bobj);
                                      });
                                    }),
                     out.families.end());
  std::sort(out.families.begin(), out.families.end(),
            [](const Family &a, const Family &b) { return detail::Lower(a.label) < detail::Lower(b.label); });
  (void)svr;
  return out;
}

// A family's picture (the first of its shown member's pictures, a DDS), empty if none.
inline Bytes Picture(const Belts &b, const Family &f) {
  if (f.show >= b.objs.size())
    return {};
  std::vector<svrfmt::BundleTexture> t;
  const Bytes pics = detail::Child(b.objs[f.show].pach, 10);
  if (pics.empty() || !svrfmt::BundleRead(pics, t) || t.empty())
    return {};
  return t.front().data;
}

struct Swap {
  int to = 0, from = 0;  // WWE '12's number, the source's
};

// Source family `sf` of `src` in place of WWE '12's family `tf` of `w12`: each of tf's shapes given the closest of sf's
// (none when none is close: the game's own stays). `svr`: the source is an SvR game. Returns the files (SOBJ_<n>.bin)
// and what went where.
inline std::vector<std::pair<int, Bytes>> Make(const Belts &w12, const Family &tf, const Belts &src, const Family &sf,
                                               bool svr, std::vector<Swap> &swaps) {
  std::vector<std::pair<int, Bytes>> out;
  swaps.clear();
  for (size_t t : tf.members) {
    const Obj &to = w12.objs[t];
    double best = 0;
    const Obj *from = nullptr;
    for (size_t s : sf.members)
      if (const double sc = detail::Score(to, src.objs[s]); sc > best)
        best = sc, from = &src.objs[s];
    if (!from)
      continue;
    // WWE '12's children only (2K14's child 4, a second model, left out)
    std::vector<svrfmt::PachEntry> kids, keep;
    if (!svrfmt::PachRead(from->pach, kids))
      continue;
    for (auto &k : kids)
      if (k.id == 0 || k.id == 10 || k.id == 20 || k.id == 30 || k.id == 100 || k.id == 200 || k.id == 1000 ||
          k.id == 1010)
        keep.push_back(std::move(k));
    Bytes pach = svrfmt::PachWrite(keep);
#ifndef BELT_PORT_NO_SVR
    if (svr) {  // (SvR's model made WWE '12's, as its entrances' props)
      std::vector<entrance13::Part> one = {{to.number, "SOBJ", pach}};
      star_port::SvrPropsTo12(one);
      if (one.empty())
        continue;
      pach = std::move(one[0].data);
    }
#endif
    out.push_back({to.number, std::move(pach)});
    swaps.push_back({to.number, from->number});
  }
  return out;
}

}  // namespace belt_port
