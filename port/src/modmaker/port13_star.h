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

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "port_svr.h"
#include "svrfmt/archive.h"
#include "svrfmt/jboy.h"
#include "svrfmt/texture.h"

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

// The nodes of the spring chains that hang from a leg: kami_* nodes whose nearest other ancestor is a thigh, calf or
// foot.
inline std::vector<bool> LegChains(const svrfmt::Model &m) {
  std::vector<bool> out(m.nodes.size(), false);
  for (size_t i = 0; i < m.nodes.size(); ++i) {
    if (m.nodes[i].name.rfind("kami", 0) != 0)
      continue;
    int p = m.nodes[i].parent;
    while (p >= 0 && m.nodes[size_t(p)].name.rfind("kami", 0) == 0)
      p = m.nodes[size_t(p)].parent;
    if (p < 0)
      continue;
    const std::string &n = m.nodes[size_t(p)].name;
    out[i] = n.find("Calf") != std::string::npos || n.find("Thigh") != std::string::npos || n.find("Foot") != std::string::npos;
  }
  return out;
}

// The leg chains' vertices (their first influence on a chain node) turned about the chain's root (its first node)
// from the chain's direction (root -> its farthest node) to the leg's (the leg bone -> its child bone), in the bind
// pose (R = Rz Ry Rx, as the nodes' Euler angles).
inline void StraightenLegChains(svrfmt::Model &m, const std::vector<bool> &on_leg) {
  using M = std::array<float, 12>;  // 3x4, row-major
  auto local = [](const svrfmt::Node &n) {
    const float cx = std::cos(n.r[0]), sx = std::sin(n.r[0]), cy = std::cos(n.r[1]), sy = std::sin(n.r[1]),
                cz = std::cos(n.r[2]), sz = std::sin(n.r[2]);
    return M{cz * cy, cz * sy * sx - sz * cx, cz * sy * cx + sz * sx, n.t[0],
             sz * cy, sz * sy * sx + cz * cx, sz * sy * cx - cz * sx, n.t[1],
             -sy,     cy * sx,                cy * cx,                n.t[2]};
  };
  auto mul = [](const M &a, const M &b) {
    M o{};
    for (int r = 0; r < 3; ++r) {
      for (int c = 0; c < 3; ++c)
        o[size_t(r * 4 + c)] = a[size_t(r * 4)] * b[size_t(c)] + a[size_t(r * 4 + 1)] * b[size_t(4 + c)] +
                               a[size_t(r * 4 + 2)] * b[size_t(8 + c)];
      o[size_t(r * 4 + 3)] = a[size_t(r * 4)] * b[3] + a[size_t(r * 4 + 1)] * b[7] + a[size_t(r * 4 + 2)] * b[11] +
                             a[size_t(r * 4 + 3)];
    }
    return o;
  };
  std::vector<M> w(m.nodes.size());
  for (size_t i = 0; i < m.nodes.size(); ++i) {
    const int p = m.nodes[i].parent;
    w[i] = p >= 0 && size_t(p) < i ? mul(w[size_t(p)], local(m.nodes[i])) : local(m.nodes[i]);
  }
  auto pos = [&](size_t i) { return std::array<float, 3>{w[i][3], w[i][7], w[i][11]}; };
  auto unit = [](std::array<float, 3> v) {
    const float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    return l > 0 ? std::array<float, 3>{v[0] / l, v[1] / l, v[2] / l} : v;
  };
  for (size_t r = 0; r < m.nodes.size(); ++r) {
    const int leg = m.nodes[r].parent;
    if (!on_leg[r] || leg < 0 || on_leg[size_t(leg)])
      continue;  // (a chain's root: on the leg bone itself)
    // its nodes, its farthest one, the leg's child bone
    std::vector<bool> in(m.nodes.size(), false);
    in[r] = true;
    size_t tip = r, below = r;
    float tip_d = 0;
    for (size_t i = r + 1; i < m.nodes.size(); ++i)
      if (m.nodes[i].parent >= 0 && in[size_t(m.nodes[i].parent)]) {
        in[i] = true;
        const auto a = pos(i), b = pos(r);
        const float d = (a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]);
        if (d > tip_d)
          tip_d = d, tip = i;
      }
    for (size_t i = 0; i < m.nodes.size(); ++i)
      if (m.nodes[i].parent == leg && m.nodes[i].name.rfind("kami", 0) != 0)
        below = i;
    if (tip == r || below == r)
      continue;
    const auto root = pos(r), lp = pos(size_t(leg)), bp = pos(below), fp = pos(tip);
    const auto a = unit({fp[0] - root[0], fp[1] - root[1], fp[2] - root[2]});
    const auto b = unit({bp[0] - lp[0], bp[1] - lp[1], bp[2] - lp[2]});
    // the rotation taking a onto b (Rodrigues)
    const std::array<float, 3> v = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    const float c = a[0] * b[0] + a[1] * b[1] + a[2] * b[2], k = c > -0.999f ? 1.0f / (1.0f + c) : 0.0f;
    const float R[9] = {1 - k * (v[1] * v[1] + v[2] * v[2]), k * v[0] * v[1] - v[2], k * v[0] * v[2] + v[1],
                        k * v[0] * v[1] + v[2], 1 - k * (v[0] * v[0] + v[2] * v[2]), k * v[1] * v[2] - v[0],
                        k * v[0] * v[2] - v[1], k * v[1] * v[2] + v[0], 1 - k * (v[0] * v[0] + v[1] * v[1])};
    auto chain_vert = [&](const svrfmt::Mesh &me, size_t vi) {
      const uint8_t s = me.weights[0][vi].bones[0];
      return s != 255 && s < me.palette.size() && me.palette[s] >= 1 && size_t(me.palette[s]) <= in.size() &&
             in[size_t(me.palette[s]) - 1];
    };
    auto turned = [&](const float *p) {
      const float d[3] = {p[0] - root[0], p[1] - root[1], p[2] - root[2]};
      std::array<float, 3> o;
      for (int j = 0; j < 3; ++j)
        o[size_t(j)] = root[size_t(j)] + R[j * 3] * d[0] + R[j * 3 + 1] * d[1] + R[j * 3 + 2] * d[2];
      return o;
    };
    // then moved so that its vertices' middle is on the leg's line (at rest it hangs ~17 cm out: Warrior's skirts,
    // which 2K14's gravity and calf collision bring around the boot)
    double sum[3] = {0, 0, 0};
    size_t count = 0;
    for (const auto &me : m.meshes)
      for (size_t vi = 0; !me.weights.empty() && vi < me.verts.size() && vi < me.weights[0].size(); ++vi)
        if (chain_vert(me, vi)) {
          const auto t = turned(me.verts[vi].pos);
          for (int j = 0; j < 3; ++j)
            sum[j] += t[size_t(j)];
          ++count;
        }
    if (!count)
      continue;
    const float mid[3] = {float(sum[0] / count), float(sum[1] / count), float(sum[2] / count)};
    const float along = (mid[0] - lp[0]) * b[0] + (mid[1] - lp[1]) * b[1] + (mid[2] - lp[2]) * b[2];
    const float shift[3] = {lp[0] + b[0] * along - mid[0], lp[1] + b[1] * along - mid[1], lp[2] + b[2] * along - mid[2]};
    for (auto &me : m.meshes)
      for (size_t vi = 0; !me.weights.empty() && vi < me.verts.size() && vi < me.weights[0].size(); ++vi)
        if (chain_vert(me, vi)) {
          float *p = me.verts[vi].pos, *n = me.verts[vi].normal;
          const auto t = turned(p);
          const float nn[3] = {n[0], n[1], n[2]};
          for (int j = 0; j < 3; ++j) {
            p[j] = t[size_t(j)] + shift[j];
            n[j] = R[j * 3] * nn[0] + R[j * 3 + 1] * nn[1] + R[j * 3 + 2] * nn[2];
          }
        }
  }
}

// The WWE '13 model `m` on the base model's skeleton: base's node order, WWE '13's bind pose.
// skeleton: the base's body nodes, for a model other than the body (the hair): a bone of the body its own skeleton
// hasn't is added to it (2K14 Ultimate Warrior's boot fringes are in his hair model, on the calves and feet; the base's
// hair skeleton is the head's 7 nodes: they went to "Bip", following the hips, never the legs - 2026-11-01).
inline bool Reskeleton(svrfmt::Model &m, const svrfmt::Model &base, Report *rep, std::string *why,
                       const std::vector<svrfmt::Node> *skeleton = nullptr) {
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
  // Its spring chains on the legs (2K14 Ultimate Warrior's knee strings, kami_b10 / b11 on the calves): WWE '12's
  // springs neither follow a leg (they hung straight, beside his bent legs) nor does the game move a node it added
  // without a spring (left at its rest place). They go to the leg bone itself, their vertices first turned about the
  // chain's root so that the chain lies along the leg (at rest it leans out, hanging from a spread leg).
  const std::vector<bool> on_leg = LegChains(m);
  StraightenLegChains(m, on_leg);
  // WWE '13 node index -> base index (its extra bones: their stand-in's; else the nearest ancestor found). Its other
  // spring bones (kami_*: hair, the arms' streamers b08 / b09; WWE '12's ch283 has springs on the upper arms) the base
  // hasn't are added after the base's nodes, under their parent's place: the springs (entries 60, 61, taken with the
  // attire) swing them (2026-11-01).
  std::vector<int> map(m.nodes.size(), -1);
  for (size_t i = 0; i < m.nodes.size(); ++i) {
    const auto extra = ExtraBones().find(m.nodes[i].name);
    const std::string &want = extra != ExtraBones().end() ? extra->second : m.nodes[i].name;
    int k = find(nodes, want);
    const int parent = m.nodes[i].parent;
    if (k < 0 && skeleton && find(*skeleton, want) >= 0 && parent >= 0 && size_t(parent) < i && map[size_t(parent)] >= 0) {
      svrfmt::Node n = m.nodes[i];
      n.name = want;
      n.parent = map[size_t(parent)];
      nodes.push_back(n);
      k = int(nodes.size()) - 1;
    }
    if (k < 0 && !on_leg[i] && m.nodes[i].name.rfind("kami", 0) == 0 && parent >= 0 && size_t(parent) < i &&
        map[size_t(parent)] >= 0) {
      svrfmt::Node n = m.nodes[i];
      n.parent = map[size_t(parent)];
      nodes.push_back(n);
      k = int(nodes.size()) - 1;
    }
    for (int p = m.nodes[i].parent; k < 0 && p >= 0; p = m.nodes[size_t(p)].parent)
      k = find(base.nodes, m.nodes[size_t(p)].name);
    // (a model's root, named after the model: WWE 2K17 Sasha Banks's "transB_eyeshado" -> the base's
    // "transB_eyelash")
    for (size_t j = 0; k < 0 && m.nodes[i].parent < 0 && j < base.nodes.size(); ++j)
      if (base.nodes[j].parent < 0)
        k = int(j);
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

// A SpringHairSetting text without the lines of `chains` (and their ReferenceHair lines).
inline Bytes DropChains(const Bytes &text, const std::set<std::string> &chains) {
  std::string in(text.begin(), text.end()), out;
  size_t at = 0;
  while (at < in.size()) {
    size_t end = in.find('\n', at);
    end = end == std::string::npos ? in.size() : end + 1;
    std::string line = in.substr(at, end - at);
    at = end;
    std::vector<std::string> tok;
    for (size_t p = 0; p < line.size();) {
      const size_t q = line.find_first_not_of(" \t\r\n", p);
      if (q == std::string::npos)
        break;
      const size_t r = line.find_first_of(" \t\r\n", q);
      tok.push_back(line.substr(q, (r == std::string::npos ? line.size() : r) - q));
      p = r == std::string::npos ? line.size() : r;
    }
    if (!tok.empty() && chains.count(tok[0]))
      continue;
    out += line;
  }
  return Bytes(out.begin(), out.end());
}

// A texture bundle's pictures with a short mip chain given the rest of it, down to 1 x 1 (the levels it has kept as they
// are; the missing ones made from its top picture). WWE 2K16's colour and normal pictures have 4 levels (3 once
// halved); the game's have them all (Ted DiBiase's colour 2048 x 1024: 12): seen from afar, a ported superstar's face
// was pixelated and not his own (Jay's report, Seth Rollins from 2K16, 2026-11-04). Empty when nothing changed.
inline Bytes FullMipChains(const Bytes &bundle) {
  std::vector<svrfmt::BundleTexture> tex;
  if (!svrfmt::BundleRead(bundle, tex))
    return {};
  bool changed = false;
  for (auto &x : tex) {
    svrfmt::DdsInfo info;
    if (!svrfmt::DdsInfoOf(x.data, info) || info.mips <= 1 || info.w < 8 || info.h < 8 ||
        (info.format != svrfmt::DxtFormat::kDxt1 && info.format != svrfmt::DxtFormat::kDxt3 &&
         info.format != svrfmt::DxtFormat::kDxt5))
      continue;
    int full = 1;
    for (int m = std::max(info.w, info.h); m > 1; m >>= 1)
      ++full;
    if (info.mips >= full)
      continue;
    const size_t block = info.format == svrfmt::DxtFormat::kDxt1 ? 8 : 16;
    size_t at = 128;
    for (int l = 0; l < info.mips; ++l)
      at += size_t(std::max(1, ((info.w >> l) + 3) / 4)) * size_t(std::max(1, ((info.h >> l) + 3) / 4)) * block;
    svrfmt::Image top;
    if (at > x.data.size() || !svrfmt::DdsDecode(x.data, top))
      continue;
    const int nw = std::max(1, info.w >> info.mips), nh = std::max(1, info.h >> info.mips);
    const Bytes rest = svrfmt::DdsEncode(svrfmt::Resize(top, std::max(4, nw), std::max(4, nh)), info.format, true);
    svrfmt::DdsInfo ri;
    if (rest.size() <= 128 || !svrfmt::DdsInfoOf(rest, ri))
      continue;
    Bytes out(x.data.begin(), x.data.begin() + ptrdiff_t(at));
    out.insert(out.end(), rest.begin() + 128, rest.end());
    svrfmt::PutLe32(&out[28], uint32_t(info.mips + ri.mips));
    x.data = std::move(out);
    changed = true;
  }
  return changed ? svrfmt::BundleWrite(tex) : Bytes();
}

// The eyes (the mesh drawn with yCh_Reflect) given the base's eye parameters, their texture slots found by name in the
// ported model's list (added when missing). 2K14's eye has a reflection colour g_f4Ref_COL (0.26, 0.26, 0.42) and no
// cube light (g_mCubeLPrm) where all 72 eyes of WWE '12 read have 0 and the cube light: 2K14 Hogan's and Cesaro's eyes
// came out glowing white in the game (2026-11-10). "cube_light" is none of the superstar's pictures: the game's own,
// found by its name like "cubemap_eyes".
inline void EyeAsBase(svrfmt::Model &ms, const svrfmt::Model &mb) {
  const svrfmt::Mesh *base_eye = nullptr;
  for (const auto &m : mb.meshes)
    if (m.shader == "yCh_Reflect")
      base_eye = &m;
  if (!base_eye)
    return;
  auto lower = [](std::string s) {
    for (auto &c : s)
      c = char(std::tolower(uint8_t(c)));
    return s;
  };
  for (auto &m : ms.meshes) {
    if (m.shader != "yCh_Reflect")
      continue;
    std::vector<svrfmt::Param> params = base_eye->params;
    bool ok = true;
    for (auto &p : params) {
      if (p.type != 0x0F)
        continue;
      if (p.value.size() < 4) {
        ok = false;
        break;
      }
      const uint32_t slot = uint32_t(p.value[0]) << 24 | uint32_t(p.value[1]) << 16 | uint32_t(p.value[2]) << 8 | p.value[3];
      if (slot >= mb.textures.size()) {
        ok = false;
        break;
      }
      const std::string name = lower(mb.textures[slot]);
      size_t at = 0;
      while (at < ms.textures.size() && lower(ms.textures[at]) != name)
        ++at;
      if (at == ms.textures.size())
        ms.textures.push_back(mb.textures[slot]);
      for (int i = 0; i < 4; ++i)
        p.value[size_t(i)] = uint8_t(at >> (24 - 8 * i));
    }
    if (ok)
      m.params = std::move(params);
  }
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
  std::set<std::string> leg_chains;  // its spring chains hanging from a leg (kami_b10 on Bip L Calf...)
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
    const std::vector<bool> on_leg = LegChains(ms);
    for (size_t i = 0; i < ms.nodes.size(); ++i)
      if (on_leg[i])
        leg_chains.insert(ms.nodes[i].name);
    svrfmt::Model body;  // (the base's body skeleton, for the hair's bones on the body: Warrior's boot fringes)
    if (from != 0 && child(base, 0))
      svrfmt::JboyRead(svrfmt::ArchiveData(*child(base, 0)), body, &e);
    if (!Reskeleton(ms, mb, from == 0 ? rep : nullptr, why, from != 0 && !body.nodes.empty() ? &body.nodes : nullptr))
      return false;
    if (from == 0 && rep)
      rep->meshes = int(ms.meshes.size());
    if (from == 0)
      EyeAsBase(ms, mb);
    svrfmt::ArchiveReplace(*b, svrfmt::JboyWrite(ms));
  }
  // Its extra models (JBOY children 0x10-0xff: WWE '13 Mankind's Mr. Socko "sockko" 0x1c, his Mandible Claw's; WWE '12
  // has such too, ch100's elbow pads "elbow_l" / "elbow_r" 0x1a / 0x1b, shown or hidden by the moves), on the body's
  // bones as the hair (after Kaiko's w13_char.py extra_models). In place of the base's of that number, else added.
  if (svrfmt::ArcNode *body12 = child(base, 0)) {
    svrfmt::Model mb;
    std::string e;
    if (svrfmt::JboyRead(svrfmt::ArchiveData(*body12), mb, &e))
      for (auto &k : w13.kids) {
        if (k.id < 0x10 || k.id > 0xFF)
          continue;
        const Bytes data = svrfmt::Unpack(svrfmt::ArchiveData(k));
        svrfmt::Model ms;
        if (!svrfmt::IsJboy(data) || !svrfmt::JboyRead(data, ms, &e) || !ms.w12)
          continue;
        std::string w;
        if (!Reskeleton(ms, mb, nullptr, &w, &mb.nodes))
          continue;
        const Bytes out = svrfmt::JboyWrite(ms);
        if (svrfmt::ArcNode *b = child(base, k.id))
          svrfmt::ArchiveReplace(*b, out);
        else
          svrfmt::ArchiveAdd(base, "", "", k.id, out, &e);
      }
  }
  // Its springs (60 the settings, 61 the collisions: text, the same "Version2" in WWE '12, '13 and 2K14), for its spring
  // bones kept above, in place of the base's. A setting line: chain, radius, viscosity, gravity, spring, damping, A,
  // length correction, bend, move correction (Kaiko's w13_char.py). The chains on the legs are not kept (Reskeleton): their
  // lines go.
  for (uint32_t id : {60u, 61u}) {
    svrfmt::ArcNode *s = child(w13, id), *b = child(base, id);
    if (!s)
      continue;
    Bytes text = svrfmt::Unpack(svrfmt::ArchiveData(*s));
    if (id == 60 && !leg_chains.empty())
      text = DropChains(text, leg_chains);
    std::string e;
    if (b)
      svrfmt::ArchiveReplace(*b, text);
    else if (!svrfmt::ArchiveAdd(base, "", "", id, text, &e))
      return fail("can't add its springs: " + e);
  }
  // Its pictures, colour and masks (all three: the base's Mask3 fits the base's colour picture).
  for (uint32_t id : {10u, 12u, 13u, 14u, 15u}) {
    svrfmt::ArcNode *s = child(w13, id), *b = child(base, id);
    if (!s || !b)
      continue;
    const Bytes pics = svrfmt::Unpack(svrfmt::ArchiveData(*s)), full = FullMipChains(pics);
    svrfmt::ArchiveReplace(*b, full.empty() ? svrfmt::ArchiveData(*s) : full);
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

// One more WWE '13 attire (entry `from` of its file) ported over a copy of the base's attire (base_file's entry
// `base_attire`) and added to `w` as `to` ("000NNNA2" another attire, "000NNNA4" an entrance outfit). An entry of `w`
// already named `to` is kept. False (and why) when it can't be ported.
inline bool AddPortedAttire(svrfmt::ArcNode &from, const std::vector<uint8_t> &base_file, size_t base_attire,
                            svrfmt::Archive &w, const std::string &to, std::string *why) {
  for (const auto &k : w.root.kids)
    if (k.name == to)
      return true;
  svrfmt::Archive copy;
  std::string e;
  if (!svrfmt::ArchiveOpen(base_file, copy, &e) || base_attire >= copy.root.kids.size())
    return why && (*why = "base unreadable: " + e, false);
  if (!PortAttire(from, copy.root.kids[base_attire], nullptr, why))
    return false;
  svrfmt::ArchiveTouch(copy.root);
  svrfmt::Archive again;
  if (!svrfmt::ArchiveOpen(svrfmt::ArchiveSave(copy), again, &e))
    return why && (*why = "ported attire unreadable: " + e, false);
  if (!svrfmt::ArchiveAdd(w.root, "EMD ", to, 0, svrfmt::ArchiveData(again.root.kids[base_attire]), &e))
    return why && (*why = "can't add it: " + e, false);
  return true;
}

// Its entrance outfit (jacket, hat... worn only during the entrance): the WWE '13 file's entry named like the attire
// with kind 4 ("00013902" -> "00013904", as in WWE '12: Cena, Undertaker...), added to `w` under the base attire's name
// with kind 4 (AddPortedAttire). *found tells whether the attire has one.
inline bool AddEntranceOutfit(svrfmt::Archive &w13, size_t attire, const std::vector<uint8_t> &base_file,
                              size_t base_attire, svrfmt::Archive &w, bool *found, std::string *why,
                              const std::string &base_name = "") {
  *found = false;
  if (attire >= w13.root.kids.size() || base_attire >= w.root.kids.size())
    return true;
  std::string name = w13.root.kids[attire].name;
  if (name.size() != 8 || name[7] != '2')
    return true;
  name[7] = '4';
  for (auto &k : w13.root.kids)
    if (k.name == name) {
      *found = true;
      std::string to = base_name.empty() ? w.root.kids[base_attire].name : base_name;
      to[7] = '4';
      return AddPortedAttire(k, base_file, base_attire, w, to, why);
    }
  return true;
}

}  // namespace port13_star
