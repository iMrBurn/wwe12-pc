// A superstar attire of WWE '13 (the player's own copy of the game, pac/ch/chNNN.pac) over a WWE '12 superstar's
// attire (docs/mod_maker.md). WWE '13 keeps WWE '12's formats (the file xcompressed, "EPK8"; the models already in
// WWE '12's layout, same shaders, same units), so little changes:
//  - its skeleton has two more bones (L_Thigh_u, R_Thigh_u, children of the pelvis) and a slightly different order:
//    the nodes are put in the base's order by name (the game's animations know WWE '12's), the two extra ones left
//    out and their weights given to the thigh (Bip L / R Thigh); every mesh palette follows;
//  - the eyelashes are entry 20002 (WWE '12: 20000), the same way; the hair (10000, on the head's bones) replaces the
//    base's (whose spring bones, kami_*, stay in its node list unused); without hair of its own, the base's goes with
//    its springs (60, 61): else it wears WWE '13's pictures (blocks of hair, seen on Jericho over Ziggler);
//  - entry 110 (animations WWE '12 doesn't have) is left out; the base keeps its own engine data (40, 50, 80, 100,
//    40000-40040): the model, its pictures (10) and colour and masks (12-14) are WWE '13's.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "port_svr.h"
#include "svrfmt/archive.h"
#include "svrfmt/jboy.h"

namespace port13_star {

using svrfmt::Bytes;

struct Report {
  int meshes = 0, bones_mapped = 0, bones_kept = 0;  // bones_kept: the base's, WWE '13 hasn't them
};

// WWE '13's bones that WWE '12 hasn't, and the one that takes their weights.
inline const std::map<std::string, std::string> &ExtraBones() {
  static const std::map<std::string, std::string> m = {{"L_Thigh_u", "Bip L Thigh"}, {"R_Thigh_u", "Bip R Thigh"}};
  return m;
}

// The WWE '13 model `m` on the base model's skeleton: base's node order, WWE '13's bind pose.
inline bool Reskeleton(svrfmt::Model &m, const svrfmt::Model &base, Report *rep, std::string *why) {
  auto find = [](const std::vector<svrfmt::Node> &nodes, const std::string &name) {
    for (size_t i = 0; i < nodes.size(); ++i)
      if (port_svr::SameName(nodes[i].name, name))
        return int(i);
    return -1;
  };
  std::vector<svrfmt::Node> nodes = base.nodes;
  for (size_t j = 0; j < nodes.size(); ++j) {
    const int i = find(m.nodes, base.nodes[j].name);
    if (i < 0) {
      if (rep)
        ++rep->bones_kept;
      continue;
    }
    svrfmt::Node n = m.nodes[size_t(i)];
    n.name = base.nodes[j].name;
    n.parent = base.nodes[j].parent;  // (the same hierarchy: WWE '13's parents, by name, are WWE '12's)
    nodes[j] = n;
    if (rep)
      ++rep->bones_mapped;
  }
  // WWE '13 node index -> base index (its extra bones: their stand-in's; else the nearest ancestor found).
  std::vector<int> map(m.nodes.size(), -1);
  for (size_t i = 0; i < m.nodes.size(); ++i) {
    const auto extra = ExtraBones().find(m.nodes[i].name);
    int k = find(base.nodes, extra != ExtraBones().end() ? extra->second : m.nodes[i].name);
    for (int p = m.nodes[i].parent; k < 0 && p >= 0; p = m.nodes[size_t(p)].parent)
      k = find(base.nodes, m.nodes[size_t(p)].name);
    map[i] = k;
  }
  for (auto &me : m.meshes)
    for (auto &p : me.palette)
      if (p > 0 && size_t(p) <= map.size()) {
        if (map[size_t(p) - 1] < 0) {
          if (why)
            *why = "bone " + m.nodes[size_t(p) - 1].name + " has no place in WWE '12's skeleton";
          return false;
        }
        p = map[size_t(p) - 1] + 1;
      }
  m.nodes = std::move(nodes);
  return true;
}

// The WWE '13 attire (an "EMD " entry of its chNNN.pac) over the base's (WWE '12): the base attire node changed in
// place. False (and why) when it can't.
inline bool PortAttire(svrfmt::ArcNode &w13, svrfmt::ArcNode &base, Report *rep, std::string *why) {
  auto fail = [&](const std::string &w) {
    if (why)
      *why = w;
    return false;
  };
  if (!svrfmt::ArchiveExpand(w13) || !svrfmt::ArchiveExpand(base))
    return fail("unreadable attire");
  auto child = [](svrfmt::ArcNode &n, uint32_t id) -> svrfmt::ArcNode * {
    for (auto &k : n.kids)
      if (k.id == id)
        return &k;
    return nullptr;
  };
  // The body (0), the hair (10000) and the eyelashes (WWE '13: 20002, WWE '12: 20000).
  for (const auto &[from, to] : std::vector<std::pair<uint32_t, uint32_t>>{{0, 0}, {10000, 10000}, {20002, 20000}}) {
    svrfmt::ArcNode *s = child(w13, from), *b = child(base, to);
    if (!s && from == 20002)
      s = child(w13, 20000);
    if (from == 0 && (!s || !b))
      return fail("no model in the attire");
    if (from == 10000 && !s && b) {
      // No hair of its own: the base's would wear WWE '13's pictures. It goes, with its springs (60, 61).
      for (size_t i = base.kids.size(); i-- > 0;)
        if (base.kids[i].id == 10000 || base.kids[i].id == 60 || base.kids[i].id == 61)
          svrfmt::ArchiveRemove(base, i);
      continue;
    }
    if (from == 10000 && s && !b) {  // (the base is bald: its hair as it is, on the head's bones)
      std::string e;
      if (!svrfmt::ArchiveAdd(base, "", "", 10000, svrfmt::ArchiveData(*s), &e))
        return fail("can't add the hair: " + e);
      continue;
    }
    if (!s || !b)
      continue;
    svrfmt::Model ms, mb;
    std::string e;
    if (!svrfmt::JboyRead(svrfmt::ArchiveData(*s), ms, &e) || !svrfmt::JboyRead(svrfmt::ArchiveData(*b), mb, &e))
      return fail("unreadable model: " + e);
    if (!ms.w12)
      return fail("not a WWE '13 model (old layout)");
    if (!Reskeleton(ms, mb, from == 0 ? rep : nullptr, why))
      return false;
    if (from == 0 && rep)
      rep->meshes = int(ms.meshes.size());
    svrfmt::ArchiveReplace(*b, svrfmt::JboyWrite(ms));
  }
  // Its pictures, colour and masks (all three: the base's Mask3 fits the base's colour picture).
  for (uint32_t id : {10u, 12u, 13u, 14u, 15u}) {
    svrfmt::ArcNode *s = child(w13, id), *b = child(base, id);
    if (s && b)
      svrfmt::ArchiveReplace(*b, svrfmt::ArchiveData(*s));
  }
  // A Mask3 (15) the other game hasn't: the base's would be on the wrong picture: left out (WWE 2K17's AJ Styles has
  // none and looks right).
  if (!child(w13, 15))
    for (size_t i = 0; i < base.kids.size(); ++i)
      if (base.kids[i].id == 15) {
        svrfmt::ArchiveRemove(base, i);
        break;
      }
  svrfmt::ArchiveTouch(base);
  return true;
}

// Its entrance outfit (jacket, hat... worn only during the entrance): the WWE '13 file's entry named like the attire
// with kind 4 ("00013902" -> "00013904", as in WWE '12: Cena, Undertaker...), ported over a copy of the base's attire
// (base_file's entry `base_attire`) and added to `w` under the base attire's name with kind 4. *found tells whether
// the attire has one. False (and why) when it has one that can't be ported.
inline bool AddEntranceOutfit(svrfmt::Archive &w13, size_t attire, const std::vector<uint8_t> &base_file,
                              size_t base_attire, svrfmt::Archive &w, bool *found, std::string *why) {
  *found = false;
  if (attire >= w13.root.kids.size() || base_attire >= w.root.kids.size())
    return true;
  std::string name = w13.root.kids[attire].name;
  if (name.size() != 8 || name[7] != '2')
    return true;
  name[7] = '4';
  svrfmt::ArcNode *outfit = nullptr;
  for (auto &k : w13.root.kids)
    if (k.name == name)
      outfit = &k;
  if (!outfit)
    return true;
  *found = true;
  svrfmt::Archive copy;
  std::string e;
  if (!svrfmt::ArchiveOpen(base_file, copy, &e) || base_attire >= copy.root.kids.size())
    return why && (*why = "base unreadable: " + e, false);
  if (!PortAttire(*outfit, copy.root.kids[base_attire], nullptr, why))
    return false;
  svrfmt::ArchiveTouch(copy.root);
  svrfmt::Archive again;
  if (!svrfmt::ArchiveOpen(svrfmt::ArchiveSave(copy), again, &e))
    return why && (*why = "ported outfit unreadable: " + e, false);
  std::string to = w.root.kids[base_attire].name;
  to[7] = '4';
  for (const auto &k : w.root.kids)
    if (k.name == to)
      return true;  // (the base has its own already: kept)
  if (!svrfmt::ArchiveAdd(w.root, "EMD ", to, 0, svrfmt::ArchiveData(again.root.kids[base_attire]), &e))
    return why && (*why = "can't add the outfit: " + e, false);
  return true;
}

}  // namespace port13_star
