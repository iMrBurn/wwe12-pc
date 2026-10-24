// A superstar attire of WWE '13 (the player's own copy of the game, pac/ch/chNNN.pac) over a WWE '12 superstar's
// attire (docs/mod_maker.md). WWE '13 keeps WWE '12's formats (the file xcompressed, "EPK8"; the models already in
// WWE '12's layout, same shaders, same units), so little changes:
//  - its skeleton has two more bones (L_Thigh_u, R_Thigh_u, children of the pelvis) and a slightly different order:
//    the nodes are put in the base's order by name (the game's animations know WWE '12's), the two extra ones left
//    out and their weights given to the thigh (Bip L / R Thigh); every mesh palette follows;
//  - the eyelashes are entry 20002 (WWE '12: 20000), the same way;
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
  // The body (0) and the eyelashes (WWE '13: 20002, WWE '12: 20000).
  for (const auto &[from, to] : std::vector<std::pair<uint32_t, uint32_t>>{{0, 0}, {20002, 20000}}) {
    svrfmt::ArcNode *s = child(w13, from), *b = child(base, to);
    if (!s && from == 20002)
      s = child(w13, 20000);
    if (!s || !b) {
      if (from == 0)
        return fail("no model in the attire");
      continue;
    }
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
  // Its pictures, colour and masks.
  for (uint32_t id : {10u, 12u, 13u, 14u}) {
    svrfmt::ArcNode *s = child(w13, id), *b = child(base, id);
    if (s && b)
      svrfmt::ArchiveReplace(*b, svrfmt::ArchiveData(*s));
  }
  svrfmt::ArchiveTouch(base);
  return true;
}

}  // namespace port13_star
