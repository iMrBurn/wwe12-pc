// A superstar's attire of SvR 2011 (the player's own file) made into a WWE '12 attire (docs/mod_maker.md, "Portage de
// catcheurs de SvR 2011"), over a WWE '12 attire of the same build (its skeleton, materials and other entries):
//  - same container (EPK8, one "EMD " PACH per attire); the model (entry 0) is rebuilt in WWE '12's layout
//    (svrfmt/jboy.h: 0xB8-byte mesh records, one tangent per vertex, the model's parts);
//  - skeleton: SvR's 102 bones (Japanese names) are mapped by name to WWE '12's 107 ("Biped" names; the face's are
//    the same in both) and the vertices are moved into WWE '12's bind pose bone by bone (SvR: arms lower, legs apart;
//    units 10 cm from the hips, WWE '12: cm from the floor, y down in both);
//  - materials: SvR's shaders (yBumpMapSS, yReflect) aren't WWE '12's (yCh_skin, yCh_skin_flex, yCh_gear,
//    yCh_Reflect): each mesh takes the material of the base's mesh of the same part nearest to it;
//  - pictures: SvR's colour (12), colour masks (13, 14, 15) and normal map; the base's others (noise, cube,
//    muscles...).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "svrfmt/archive.h"
#include "svrfmt/jboy.h"
#include "svrfmt/texture.h"

namespace port_svr {

using svrfmt::Bytes;

// SvR 2011 -> WWE '12 bone names. "" = no counterpart: the bone follows its parent.
inline std::string MapBone(const std::string &s) {
  static const std::map<std::string, std::string> trunk = {
      {"root", "Bip"},           {"koshi", "Bip Spine"},   {"mune", "Bip Spine1"},     {"onaka", ""},
      {"kubi", "Bip Neck"},      {"kubi_x", "Neck_x"},     {"atama", "Bip Head"},      {"l_mune_dummy", "L_Chest_Dummy"},
      {"r_mune_dummy", "R_Chest_Dummy"}, {"l_sakotsu_m", "L_Clavicle_m"}, {"r_sakotsu_m", "R_Clavicle_m"},
  };
  if (auto it = trunk.find(s); it != trunk.end())
    return it->second;
  if (s.size() > 2 && (s[0] == 'l' || s[0] == 'r') && s[1] == '_') {
    const std::string side = s[0] == 'l' ? "L" : "R", b = s.substr(2);
    static const std::map<std::string, std::string> limb = {
        {"momo", "Bip # Thigh"},      {"momo_x", "#_Thigh_x"},          {"sune", "Bip # Calf"},
        {"ashi", "Bip # Foot"},       {"tsumasaki", "Bip # Toe0"},      {"sakotsu", "Bip # Clavicle"},
        {"kata_m", "#_Shoulder_m"},   {"ninoude", "Bip # UpperArm"},    {"ninoude_x", "#_UpperArm_x"},
        {"ninoude_m", "#_UpperArm_m"}, {"kote", "Bip # Forearm"},       {"kote_m", "#_Elbow_m"},
        {"kote_x", "#_Wrist_x"},      {"kote_x2", "#_Wrist_x2"},        {"te", "Bip # Hand"}};
    if (auto it = limb.find(b); it != limb.end()) {
      std::string r = it->second;
      r.replace(r.find('#'), 1, side);
      return r;
    }
    if (b.rfind("yubi", 0) == 0 && b.size() == 6) {  // yubiNK: finger N (0 = thumb), joint K
      std::string r = "Bip " + side + " Finger" + b.substr(4, 1);
      if (b[5] != '0')
        r += b.substr(5, 1);
      return r;
    }
  }
  return s;  // the face's bones
}

// SvR part names -> WWE '12's.
inline std::string MapPart(const std::string &s) {
  if (s == "eyes")
    return "EYE";
  if (s == "teeth")
    return "tooth";
  return s;
}

// 4 x 4 matrices (row-major, column vectors).
struct M4 {
  double m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};
inline M4 Mul(const M4 &a, const M4 &b) {
  M4 r;
  for (int i = 0; i < 4; ++i)
    for (int j = 0; j < 4; ++j) {
      double s = 0;
      for (int k = 0; k < 4; ++k)
        s += a.m[i * 4 + k] * b.m[k * 4 + j];
      r.m[i * 4 + j] = s;
    }
  return r;
}
// A node's local matrix: T(t) Rz Ry Rx.
inline M4 Local(const svrfmt::Node &n) {
  const double cx = cos(n.r[0]), sx = sin(n.r[0]), cy = cos(n.r[1]), sy = sin(n.r[1]), cz = cos(n.r[2]),
               sz = sin(n.r[2]);
  const M4 rx{{1, 0, 0, 0, 0, cx, -sx, 0, 0, sx, cx, 0, 0, 0, 0, 1}};
  const M4 ry{{cy, 0, sy, 0, 0, 1, 0, 0, -sy, 0, cy, 0, 0, 0, 0, 1}};
  const M4 rz{{cz, -sz, 0, 0, sz, cz, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
  const M4 t{{1, 0, 0, n.t[0], 0, 1, 0, n.t[1], 0, 0, 1, n.t[2], 0, 0, 0, 1}};
  return Mul(t, Mul(rz, Mul(ry, rx)));
}
inline std::vector<M4> World(const svrfmt::Model &m) {
  std::vector<M4> w(m.nodes.size());
  for (size_t i = 0; i < m.nodes.size(); ++i) {
    const int p = m.nodes[i].parent;
    w[i] = p >= 0 && size_t(p) < i ? Mul(w[size_t(p)], Local(m.nodes[i])) : Local(m.nodes[i]);
  }
  return w;
}
// Inverse of a rotation + translation.
inline M4 InvRigid(const M4 &a) {
  M4 r;
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      r.m[i * 4 + j] = a.m[j * 4 + i];
  for (int i = 0; i < 3; ++i)
    r.m[i * 4 + 3] = -(r.m[i * 4 + 0] * a.m[3] + r.m[i * 4 + 1] * a.m[7] + r.m[i * 4 + 2] * a.m[11]);
  return r;
}

struct Report {
  int meshes = 0, unmapped_bones = 0;
  std::vector<std::string> notes;
  std::vector<std::string> textures;  // SvR pictures the model now names besides the base's (copied by PortAttire)
};

inline bool SameName(const std::string &a, const std::string &b) {
  return a.size() == b.size() &&
         std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) { return std::tolower((unsigned char)x) == std::tolower((unsigned char)y); });
}

namespace detail {

inline std::array<double, 3> Centroid(const svrfmt::Mesh &s) {
  std::array<double, 3> c{};
  for (const auto &v : s.verts)
    for (int i = 0; i < 3; ++i)
      c[i] += v.pos[i];
  for (auto &x : c)
    x /= std::max<size_t>(1, s.verts.size());
  return c;
}

inline std::vector<std::array<uint16_t, 3>> Triangles(const svrfmt::Strip &st) {
  if (st.IsStrip())
    return svrfmt::StripToTriangles(st.indices);
  std::vector<std::array<uint16_t, 3>> t;
  for (size_t q = 0; q + 2 < st.indices.size(); q += 3)
    t.push_back({st.indices[q], st.indices[q + 1], st.indices[q + 2]});
  return t;
}

// One tangent per vertex: the U direction of the texture, square to the normal (as WWE '12's).
inline void Tangents(svrfmt::Mesh &s) {
  const size_t n = s.verts.size();
  std::vector<std::array<double, 3>> t(n, {0, 0, 0});
  for (const auto &st : s.strips)
    for (const auto &tr : Triangles(st)) {
      if (tr[0] >= n || tr[1] >= n || tr[2] >= n)
        continue;
      const auto &p0 = s.verts[tr[0]].pos, &p1 = s.verts[tr[1]].pos, &p2 = s.verts[tr[2]].pos;
      const auto &u0 = s.uvs[tr[0]], &u1 = s.uvs[tr[1]], &u2 = s.uvs[tr[2]];
      const double du1 = u1[0] - u0[0], dv1 = u1[1] - u0[1], du2 = u2[0] - u0[0], dv2 = u2[1] - u0[1];
      const double det = du1 * dv2 - du2 * dv1;
      if (std::abs(det) < 1e-12)
        continue;
      for (int v = 0; v < 3; ++v)
        for (int q = 0; q < 3; ++q)
          t[tr[v]][q] += ((p1[q] - p0[q]) * dv2 - (p2[q] - p0[q]) * dv1) / det;
    }
  s.tangents.assign(n, {1, 0, 0});
  for (size_t i = 0; i < n; ++i) {
    const auto &nn = s.verts[i].normal;
    auto v = t[i];
    const double d = v[0] * nn[0] + v[1] * nn[1] + v[2] * nn[2];
    for (int q = 0; q < 3; ++q)
      v[q] -= d * nn[q];
    const double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 1e-9)
      s.tangents[i] = {float(v[0] / l), float(v[1] / l), float(v[2] / l)};
  }
}

}  // namespace detail

// SvR's model (entry 0) on the WWE '12 base model (same superstar or build) -> the WWE '12 model.
// A rotation matrix (R = Rz Ry Rx, as Local) back to its angles.
inline void EulerOf(const M4 &w, float r[3]) {
  const double sy = std::clamp(-w.m[8], -1.0, 1.0);
  r[1] = float(std::asin(sy));
  if (std::abs(sy) < 0.99999) {
    r[0] = float(std::atan2(w.m[9], w.m[10]));
    r[2] = float(std::atan2(w.m[4], w.m[0]));
  } else {
    r[0] = float(std::atan2(-w.m[6], w.m[5]));
    r[2] = 0;
  }
}

// See ConvertModel's keep_chains. ws: SvR's bind (its units), move: each SvR node's move into WWE '12's bind (x10),
// target: each SvR node's WWE '12 node (updated: the kept chain bones).
inline void KeepChains(const svrfmt::Model &svr, const std::vector<M4> &ws, const std::vector<M4> &move,
                       svrfmt::Model &out, std::vector<int> &target, Report *rep) {
  auto kami = [](const std::string &n) { return n.rfind("kami", 0) == 0; };
  bool any = false;
  for (const auto &n : svr.nodes)
    any = any || kami(n.name);
  if (!any)
    return;
  // the base's own chains out (their indices: the later nodes move down)
  std::vector<int> remap(out.nodes.size(), -1);
  std::vector<svrfmt::Node> kept;
  for (size_t i = 0; i < out.nodes.size(); ++i)
    if (!kami(out.nodes[i].name)) {
      remap[i] = int(kept.size());
      kept.push_back(out.nodes[i]);
    }
  for (auto &n : kept)
    n.parent = n.parent >= 0 && size_t(n.parent) < remap.size() ? remap[size_t(n.parent)] : -1;
  out.nodes = std::move(kept);
  for (auto &t : target)
    t = t >= 0 && size_t(t) < remap.size() ? remap[size_t(t)] : -1;
  int head = -1;
  for (size_t i = 0; i < out.nodes.size(); ++i)
    if (out.nodes[i].name == "Bip Head")
      head = int(i);
  if (head < 0)
    return;
  auto wo = World(out);
  std::vector<int> now(svr.nodes.size(), -1);  // SvR chain bone -> its new node
  int added = 0;
  for (size_t i = 0; i < svr.nodes.size(); ++i) {
    if (!kami(svr.nodes[i].name))
      continue;
    const int p = svr.nodes[i].parent;
    if (p < 0 || size_t(p) >= svr.nodes.size())
      continue;
    const int parent = kami(svr.nodes[size_t(p)].name) ? now[size_t(p)] : svr.nodes[size_t(p)].name == "atama" ? head : -1;
    if (parent < 0)
      continue;
    // its new world: SvR's moved as its strands' vertices are (the rotation without the x10)
    const M4 &d = move[i], &w = ws[i];
    M4 nw;
    for (int r = 0; r < 3; ++r) {
      for (int c = 0; c < 3; ++c) {
        double x = 0;
        for (int k = 0; k < 3; ++k)
          x += d.m[r * 4 + k] / 10 * w.m[k * 4 + c];
        nw.m[r * 4 + c] = x;
      }
      nw.m[r * 4 + 3] = d.m[r * 4] * w.m[3] + d.m[r * 4 + 1] * w.m[7] + d.m[r * 4 + 2] * w.m[11] + d.m[r * 4 + 3];
    }
    const M4 local = Mul(InvRigid(wo[size_t(parent)]), nw);
    svrfmt::Node n;
    n.name = svr.nodes[i].name;
    n.parent = parent;
    for (int c = 0; c < 3; ++c)
      n.t[c] = float(local.m[c * 4 + 3]);
    EulerOf(local, n.r);
    if (kami(svr.nodes[size_t(p)].name)) {  // (as WWE '12's chain bones: a bounding sphere at the joint)
      n.sphere[0] = float(nw.m[3]);
      n.sphere[1] = float(nw.m[7]);
      n.sphere[2] = float(nw.m[11]);
      n.sphere[3] = 6;
    }
    now[i] = int(out.nodes.size());
    target[i] = now[i];
    out.nodes.push_back(n);
    wo.push_back(Mul(wo[size_t(parent)], Local(n)));
    ++added;
  }
  if (rep)
    rep->notes.push_back("hair: " + std::to_string(added) + " chain bones kept");
}

// keep_chains (the hair, 10000): SvR's hair bones (kami_bNN chains under the head, moved by the game's hair physics in
// both games, same names and layout) are kept, placed under WWE '12's head as the head moved, the strands weighted to
// them as in SvR (the base's own chains, if any, left out: they'd hold none of SvR's strands).
// turns_in / turns_out: the bones' turns by name, taken from / given to another model's conversion (the hair and the
// eyes' pieces turn as the body does: each model's own children gave the head another turn).
inline bool ConvertModel(const svrfmt::Model &svr, const svrfmt::Model &base, svrfmt::Model &out, Report *rep,
                         bool keep_chains = false, const std::map<std::string, M4> *turns_in = nullptr,
                         std::map<std::string, M4> *turns_out = nullptr) {
  if (!base.w12 || base.meshes.empty() || svr.meshes.empty())
    return false;
  const auto ws = World(svr), wb = World(base);
  // Each SvR bone: its WWE '12 bone and the move from SvR's bind pose to WWE '12's. The two games don't orient their
  // bones' frames alike (WWE '12's head and feet are turned half round), so the move comes from the joints'
  // places: D = T(WWE '12 joint) R x10 T(-SvR joint), R turning the SvR bone (joint -> its farthest child) onto the
  // WWE '12 one (the arms and legs; identity where the pose is the same); bones without a child turn as their parent.
  std::vector<int> target(svr.nodes.size(), -1);
  for (size_t i = 0; i < svr.nodes.size(); ++i) {
    const std::string name = MapBone(svr.nodes[i].name);
    for (size_t j = 0; j < base.nodes.size() && !name.empty(); ++j)
      if (base.nodes[j].name == name)
        target[i] = int(j);
  }
  auto pos = [](const M4 &w, double scale) {
    return std::array<double, 3>{w.m[3] * scale, w.m[7] * scale, w.m[11] * scale};
  };
  auto sub = [](std::array<double, 3> a, const std::array<double, 3> &b) {
    for (int q = 0; q < 3; ++q)
      a[q] -= b[q];
    return a;
  };
  auto len = [](const std::array<double, 3> &a) { return std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]); };
  // The rotation turning direction a onto b (Rodrigues).
  auto turn = [&](std::array<double, 3> a, std::array<double, 3> b) {
    M4 r;
    const double la = len(a), lb = len(b);
    if (la < 1e-9 || lb < 1e-9)
      return r;
    for (int q = 0; q < 3; ++q) {
      a[q] /= la;
      b[q] /= lb;
    }
    const double c = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
    std::array<double, 3> k{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    const double s = len(k);
    if (s < 0.05 || c < -0.99)  // (under ~3 degrees: the same pose)
      return r;
    for (auto &x : k)
      x /= s;
    const double v = 1 - c;
    const double m[9] = {c + k[0] * k[0] * v,        k[0] * k[1] * v - k[2] * s, k[0] * k[2] * v + k[1] * s,
                         k[1] * k[0] * v + k[2] * s, c + k[1] * k[1] * v,        k[1] * k[2] * v - k[0] * s,
                         k[2] * k[0] * v - k[1] * s, k[2] * k[1] * v + k[0] * s, c + k[2] * k[2] * v};
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j)
        r.m[i * 4 + j] = m[i * 3 + j];
    return r;
  };
  std::vector<M4> rot(svr.nodes.size()), move(svr.nodes.size());
  for (size_t i = 0; i < svr.nodes.size(); ++i) {
    const int p = svr.nodes[i].parent;
    if (target[i] < 0) {  // no counterpart: follows its parent
      if (p >= 0 && size_t(p) < i) {
        target[i] = target[size_t(p)];
        rot[i] = rot[size_t(p)];
        move[i] = move[size_t(p)];
      }
      if (rep && !MapBone(svr.nodes[i].name).empty())
        ++rep->unmapped_bones;
      continue;
    }
    const auto si = pos(ws[i], 10), wi = pos(wb[size_t(target[i])], 1);
    int farthest = -1;
    double farthest_len = 5;  // (cm: a child at the joint gives no direction)
    // (not a hair chain: the base's chain of the same name is another hairstyle's - Drew McIntyre of SvR on WWE '12's
    // Drew, both with a kami_b00: the hair model's head turned onto the other ponytail, the strands tucked under his
    // scalp in SvR stood out of his forehead, 2026-10-27)
    for (size_t c = i + 1; c < svr.nodes.size(); ++c)
      if (svr.nodes[c].parent == int(i) && !MapBone(svr.nodes[c].name).empty() &&
          svr.nodes[c].name.rfind("kami", 0) != 0) {
        int tc = -1;
        for (size_t j = 0; j < base.nodes.size(); ++j)
          if (base.nodes[j].name == MapBone(svr.nodes[c].name))
            tc = int(j);
        const double l = len(sub(pos(ws[c], 10), si));
        if (tc >= 0 && l > farthest_len) {
          farthest = int(c);
          farthest_len = l;
        }
      }
    if (farthest >= 0) {
      int tf = -1;
      for (size_t j = 0; j < base.nodes.size(); ++j)
        if (base.nodes[j].name == MapBone(svr.nodes[size_t(farthest)].name))
          tf = int(j);
      rot[i] = turn(sub(pos(ws[size_t(farthest)], 10), si), sub(pos(wb[size_t(tf)], 1), wi));
    } else if (p >= 0 && size_t(p) < i) {
      rot[i] = rot[size_t(p)];
    }
    // (the hair model's head has no face below it, so no turn of its own: the body's, else SvR 2009 William Regal's
    // and Maria's hair turned ~27 degrees off the head and hung over their eyes, 2026-10-31)
    if (turns_in)
      if (const auto it = turns_in->find(svr.nodes[i].name); it != turns_in->end())
        rot[i] = it->second;
    // The face's bones move with the head, keeping SvR's face as it is: placed by their own joints (the two games'
    // face joints aren't in the same places), the eyelids, teeth and lips moved off the face (CM Punk of SvR: eyes
    // shut, teeth out of the mouth, 2026-10-26).
    bool face = false;  // (a bone under the head: jaw, lips, cheeks, eyelids, brows, eyes)
    for (int q = p; q >= 0 && size_t(q) < svr.nodes.size(); q = svr.nodes[size_t(q)].parent)
      if (svr.nodes[size_t(q)].name == "atama") {
        face = true;
        break;
      }
    if (face && p >= 0 && size_t(p) < i) {
      rot[i] = rot[size_t(p)];
      move[i] = move[size_t(p)];
      continue;
    }
    if (turns_out)
      (*turns_out)[svr.nodes[i].name] = rot[i];
    M4 to, from, x10;
    x10.m[0] = x10.m[5] = x10.m[10] = 10;
    for (int q = 0; q < 3; ++q) {
      to.m[q * 4 + 3] = wi[q];
      from.m[q * 4 + 3] = -si[q] / 10;
    }
    move[i] = Mul(to, Mul(rot[i], Mul(x10, from)));
  }
  out = base;
  out.meshes.clear();
  out.parts.clear();
  if (keep_chains)
    KeepChains(svr, ws, move, out, target, rep);
  // Parts: SvR's (their meshes come in that order), named as WWE '12's.
  std::vector<int> part_of;
  for (const auto &p : svr.parts) {
    svrfmt::Part q;
    q.name = MapPart(p.name);
    q.meshes = p.meshes;
    q.info = base.parts.empty() ? p.info : base.parts[0].info;
    for (const auto &bp : base.parts)
      if (bp.name == q.name)
        q.info = bp.info;
    for (uint32_t k = 0; k < p.meshes; ++k)
      part_of.push_back(int(out.parts.size()));
    out.parts.push_back(std::move(q));
  }
  part_of.resize(svr.meshes.size(), 0);
  std::vector<int> svr_part_of;  // (SvR's part of each mesh)
  for (size_t pi = 0; pi < svr.parts.size(); ++pi)
    for (uint32_t k = 0; k < svr.parts[pi].meshes; ++k)
      svr_part_of.push_back(int(pi));
  svr_part_of.resize(svr.meshes.size(), 0);
  // The base's meshes by part, and their centres.
  std::vector<int> base_part;
  for (size_t i = 0; i < base.parts.size(); ++i)
    for (uint32_t k = 0; k < base.parts[i].meshes; ++k)
      base_part.push_back(int(i));
  base_part.resize(base.meshes.size(), -1);

  for (size_t mi = 0; mi < svr.meshes.size(); ++mi) {
    const svrfmt::Mesh &src = svr.meshes[mi];
    // SvR's blood layer on the hands (a part named "blood_hand": yDefault meshes over the hands, painted plain red,
    // shown in SvR only when the superstar bleeds; WWE '12 has its own blood): left out, else red hands with pointed
    // fingers (CM Punk of SvR, 2026-10-26). (Other superstars' real hands use BloodHand pictures too: Orton's.)
    if (size_t(svr_part_of[mi]) < svr.parts.size() && svr.parts[size_t(svr_part_of[mi])].name == "blood_hand") {
      if (size_t(part_of[mi]) < out.parts.size() && out.parts[size_t(part_of[mi])].meshes)
        --out.parts[size_t(part_of[mi])].meshes;
      continue;
    }
    svrfmt::Mesh dst = src;
    // Vertices into WWE '12's pose: the blend of their bones' moves.
    for (size_t k = 0; k < src.verts.size(); ++k) {
      double p[3] = {0, 0, 0}, nn[3] = {0, 0, 0}, total = 0;
      for (const auto &blk : src.weights) {
        if (k >= blk.size() || blk[k].bones[0] == 255 || blk[k].bones[0] >= src.palette.size())
          continue;
        const int node = src.palette[blk[k].bones[0]] - 1;
        const double w = src.weights.size() == 1 ? 1.0 : blk[k].weight;
        if (node < 0 || size_t(node) >= move.size() || w <= 0)
          continue;
        const M4 &d = move[size_t(node)];
        const auto &v = src.verts[k];
        for (int r = 0; r < 3; ++r) {
          p[r] += w * (d.m[r * 4] * v.pos[0] + d.m[r * 4 + 1] * v.pos[1] + d.m[r * 4 + 2] * v.pos[2] + d.m[r * 4 + 3]);
          nn[r] += w * (d.m[r * 4] * v.normal[0] + d.m[r * 4 + 1] * v.normal[1] + d.m[r * 4 + 2] * v.normal[2]) / 10;
        }
        total += w;
      }
      if (total <= 0) {  // no bone: as the root
        const M4 d = move.empty() ? M4() : move[0];
        const auto &v = src.verts[k];
        for (int r = 0; r < 3; ++r) {
          p[r] = d.m[r * 4] * v.pos[0] + d.m[r * 4 + 1] * v.pos[1] + d.m[r * 4 + 2] * v.pos[2] + d.m[r * 4 + 3];
          nn[r] = v.normal[r];
        }
        total = 1;
      }
      const double nl = std::sqrt(nn[0] * nn[0] + nn[1] * nn[1] + nn[2] * nn[2]);
      for (int r = 0; r < 3; ++r) {
        dst.verts[k].pos[r] = float(p[r] / total);
        dst.verts[k].normal[r] = nl > 1e-9 ? float(nn[r] / nl) : src.verts[k].normal[r];
      }
    }
    // Palette: WWE '12's bones.
    for (auto &slot : dst.palette)
      if (slot > 0 && size_t(slot - 1) < target.size() && target[size_t(slot - 1)] >= 0)
        slot = target[size_t(slot - 1)] + 1;
    // Material: the nearest base mesh of the same part (eyes: a reflecting one; the body: skin).
    const bool reflect = src.shader == "yReflect";
    const std::string part = out.parts.empty() ? "" : out.parts[size_t(part_of[mi])].name;
    const auto c = detail::Centroid(dst);
    int best = -1;
    double best_d = 1e30;
    for (int pass = 0; pass < 2 && best < 0; ++pass)
      for (size_t bi = 0; bi < base.meshes.size(); ++bi) {
        const svrfmt::Mesh &b = base.meshes[bi];
        if ((b.shader == "yCh_Reflect") != reflect || (part == "body" && b.shader == "yCh_gear"))
          continue;  // (the body is skin: the base's gear there is tape, pads...)
        if (pass == 0 && (base_part[bi] < 0 || base.parts[size_t(base_part[bi])].name != part))
          continue;
        const auto bc = detail::Centroid(b);
        const double d = (bc[0] - c[0]) * (bc[0] - c[0]) + (bc[1] - c[1]) * (bc[1] - c[1]) + (bc[2] - c[2]) * (bc[2] - c[2]);
        if (d < best_d) {
          best_d = d;
          best = int(bi);
        }
      }
    if (best < 0)
      best = 0;
    const svrfmt::Mesh &tm = base.meshes[size_t(best)];
    dst.raw = tm.raw;
    dst.shader = tm.shader;
    dst.vfmt = tm.vfmt;
    dst.material = tm.material;
    dst.params = tm.params;
    // Pictures of its own (SvR's hands: BloodHand0_L/R and their normal maps, not the body's colour picture): the
    // WWE '12 material is pointed at them (the game finds pictures by name, any case).
    auto svr_tex = [&](const char *param) -> std::string {
      for (const auto &p : src.params)
        if (p.name == param && p.type == 15 && p.value.size() >= 4) {
          const uint32_t v = svrfmt::Be32(p.value.data());
          return v < svr.textures.size() ? svr.textures[v] : "";
        }
      return "";
    };
    for (const auto &[from, to] : {std::pair<const char *, const char *>{"texDiffuse", "g_mDiffusePrm"},
                                   std::pair<const char *, const char *>{"texNormal", "g_mNormalFxPrm"}}) {
      const std::string name = svr_tex(from);
      if (name.empty() || SameName(name, "color") || SameName(name, "normal"))
        continue;
      size_t idx = out.textures.size();
      for (size_t t = 0; t < out.textures.size(); ++t)
        if (SameName(out.textures[t], name))
          idx = t;
      if (idx == out.textures.size()) {
        out.textures.push_back(name);
        if (rep)
          rep->textures.push_back(name);
      }
      for (auto &p : dst.params)
        if (p.name == to && p.value.size() >= 4)
          svrfmt::PutBe32(p.value.data(), uint32_t(idx));
    }
    if (dst.raw.size() >= 0x64)
      svrfmt::PutBe32(&dst.raw[0x60], uint32_t(part_of[mi]));  // its part
    // Bounding sphere.
    double lo[3] = {1e30, 1e30, 1e30}, hi[3] = {-1e30, -1e30, -1e30};
    for (const auto &v : dst.verts)
      for (int r = 0; r < 3; ++r) {
        lo[r] = std::min(lo[r], double(v.pos[r]));
        hi[r] = std::max(hi[r], double(v.pos[r]));
      }
    double rad = 0;
    for (const auto &v : dst.verts) {
      double d = 0;
      for (int r = 0; r < 3; ++r)
        d += (v.pos[r] - (lo[r] + hi[r]) / 2) * (v.pos[r] - (lo[r] + hi[r]) / 2);
      rad = std::max(rad, d);
    }
    for (int r = 0; r < 3; ++r)
      dst.sphere[r] = float((lo[r] + hi[r]) / 2);
    dst.sphere[3] = float(std::sqrt(rad));
    detail::Tangents(dst);
    if (rep)
      rep->notes.push_back(src.shader + " (" + std::to_string(src.verts.size()) + " vertices, " + part + ") -> " +
                           tm.shader + " of base mesh " + std::to_string(best));
    out.meshes.push_back(std::move(dst));
  }
  // (SvR's eyes are two meshes: the eyeballs, and the irises over them, a flat mesh on the eye picture's middle: both
  // kept. Left out once as "a glossy layer" while the face's bones were still misplaced: white eyes, 2026-10-26.)
  // Parts left without a mesh (the blood layers'): removed, the later parts' meshes told their new number.
  for (size_t i = out.parts.size(); i-- > 0;)
    if (!out.parts[i].meshes) {
      out.parts.erase(out.parts.begin() + long(i));
      for (auto &m : out.meshes)
        if (m.raw.size() >= 0x64 && svrfmt::Be32(&m.raw[0x60]) > i)
          svrfmt::PutBe32(&m.raw[0x60], svrfmt::Be32(&m.raw[0x60]) - 1);
    }
  out.w12 = true;
  if (rep)
    rep->meshes = int(out.meshes.size());
  return true;
}

// WWE '12 reads how much the skin shines in the normal map's alpha (Orton's chest: 16 in WWE '12, 45 in SvR 2011's:
// the ported Orton shone too much). SvR's normal map with its alpha scaled to 0.8 of the base's mean, in the base's
// format: SvR's is more contrasted, so at the base's mean its bright patches still shone more (Drew McIntyre of SvR,
// skin: p90 38 for WWE '12's 31, "a bit too shiny", 2026-10-27); 0.8 brings them to the base's.
inline Bytes Shine(const Bytes &svr_normal, const Bytes &base_normal) {
  svrfmt::Image s, b;
  svrfmt::DdsInfo si, bi;
  if (!svrfmt::DdsDecode(svr_normal, s) || !svrfmt::DdsDecode(base_normal, b) || !svrfmt::DdsInfoOf(svr_normal, si) ||
      !svrfmt::DdsInfoOf(base_normal, bi) || bi.format == svrfmt::DxtFormat::kUnknown)
    return svr_normal;
  auto mean = [](const svrfmt::Image &im) {
    double t = 0;
    for (size_t q = 3; q < im.rgba.size(); q += 4)
      t += im.rgba[q];
    return t / std::max<size_t>(1, im.rgba.size() / 4);
  };
  const double ms = mean(s), mb = mean(b) * 0.8;
  if (ms < 1 || std::abs(ms - mb) < 2)
    return svr_normal;
  const double k = mb / ms;
  for (size_t q = 3; q < s.rgba.size(); q += 4)
    s.rgba[q] = uint8_t(std::min(255.0, s.rgba[q] * k + 0.5));
  return svrfmt::DdsEncode(s, bi.format, si.mips > 1);
}

// SvR's long hair hangs from hair bones (kami_*, moved by its physics), which WWE '12 hasn't: converted, the strands
// followed the head stiffly and swung behind the back when the head turned up (CM Punk of SvR: hair "slicked back"
// in the game, 2026-10-26; RenderDoc: the strands drawn, opaque, behind the shoulders). The strands' vertices are
// weighted from the head to the upper chest (Bip Spine1, in the hair model's skeleton) as they go down, from the
// head's joint to the chest's: they hang from the shoulders more than from the head.
inline void HangHair(const svrfmt::Model &svr, svrfmt::Model &out) {
  int head = -1, chest = -1;
  for (size_t i = 0; i < out.nodes.size(); ++i) {
    if (out.nodes[i].name == "Bip Head")
      head = int(i);
    if (out.nodes[i].name == "Bip Spine1")
      chest = int(i);
  }
  if (head < 0 || chest < 0)
    return;
  const auto w = World(out);
  const double hy = w[size_t(head)].m[7], cy = w[size_t(chest)].m[7];
  if (std::abs(cy - hy) < 1)
    return;
  for (size_t mi = 0; mi < out.meshes.size() && mi < svr.meshes.size(); ++mi) {
    const svrfmt::Mesh &src = svr.meshes[mi];
    svrfmt::Mesh &m = out.meshes[mi];
    if (m.weights.size() < 2 || src.verts.size() != m.verts.size())
      continue;
    // its palette slots for the head and the chest (added if missing)
    auto slot = [&](int node) {
      for (size_t q = 0; q < m.palette.size(); ++q)
        if (m.palette[q] == node + 1)
          return int(q);
      m.palette.push_back(node + 1);
      return int(m.palette.size() - 1);
    };
    bool any = false;
    for (size_t k = 0; k < src.verts.size() && !any; ++k)
      for (const auto &blk : src.weights)
        if (k < blk.size() && blk[k].bones[0] < src.palette.size()) {
          const int n = src.palette[blk[k].bones[0]] - 1;
          any = any || (n >= 0 && size_t(n) < svr.nodes.size() && svr.nodes[size_t(n)].name.rfind("kami", 0) == 0);
        }
    if (!any)
      continue;  // (not strands: the beard, the scalp)
    const int sh = slot(head), sc = slot(chest);
    if (sh >= 255 || sc >= 255)
      continue;
    for (size_t k = 0; k < m.verts.size(); ++k) {
      bool strand = false;
      for (const auto &blk : src.weights)
        if (k < blk.size() && blk[k].bones[0] < src.palette.size()) {
          const int n = src.palette[blk[k].bones[0]] - 1;
          strand = strand || (n >= 0 && size_t(n) < svr.nodes.size() && svr.nodes[size_t(n)].name.rfind("kami", 0) == 0);
        }
      if (!strand)
        continue;
      const double t = std::clamp((m.verts[k].pos[1] - hy) / (cy - hy), 0.0, 1.0);
      for (size_t j = 0; j < m.weights.size(); ++j) {
        auto &wt = m.weights[j][k];
        wt.bones[0] = j == 0 ? uint8_t(sh) : j == 1 ? uint8_t(sc) : 255;
        wt.bones[1] = wt.bones[2] = wt.bones[3] = 255;
        wt.weight = j == 0 ? float(1 - t) : j == 1 ? float(t) : 0.0f;
      }
    }
  }
}

using UvTri = std::array<std::array<float, 2>, 3>;

// The texels of a w x h picture inside UV triangles (one over half the picture is skipped: wrapped UVs).
inline std::vector<uint8_t> UvMask(const std::vector<UvTri> &tris, int w, int h) {
  std::vector<uint8_t> keep(size_t(w) * h, 0);
  for (const auto &t : tris) {
    float x[3], y[3];
    for (int c = 0; c < 3; ++c) {
      x[c] = t[size_t(c)][0] * w;
      y[c] = t[size_t(c)][1] * h;
    }
    const int x0 = std::max(0, int(std::floor(std::min({x[0], x[1], x[2]})))),
              x1 = std::min(w - 1, int(std::ceil(std::max({x[0], x[1], x[2]})))),
              y0 = std::max(0, int(std::floor(std::min({y[0], y[1], y[2]})))),
              y1 = std::min(h - 1, int(std::ceil(std::max({y[0], y[1], y[2]}))));
    const float area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
    if (std::abs(area) < 1e-6f || x1 - x0 > w / 2 || y1 - y0 > h / 2)
      continue;
    for (int py = y0; py <= y1; ++py)
      for (int px = x0; px <= x1; ++px) {
        const float cx = px + 0.5f, cy = py + 0.5f;
        const float w0 = (x[1] - cx) * (y[2] - cy) - (x[2] - cx) * (y[1] - cy);
        const float w1 = (x[2] - cx) * (y[0] - cy) - (x[0] - cx) * (y[2] - cy);
        const float w2 = (x[0] - cx) * (y[1] - cy) - (x[1] - cx) * (y[0] - cy);
        if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0))
          keep[size_t(py) * w + px] = 1;
      }
  }
  return keep;
}

// WWE '12's hair shader cuts the hair out with the NORMAL map's alpha (the base's: white strands with their tips),
// SvR's with the colour's; SvR's normal alpha (the shine) is near black there, so every strand fragment was thrown
// away (CM Punk of SvR: long hair invisible, only the scalp; RenderDoc 2026-10-26, pixel shader Kill on
// normal.a * vertex alpha). The normal map's alpha under the hair's UVs: SvR's colour alpha (averaged down).
// Its blue there is no normal either: WWE '12's hair has it dark, about the hair colour's brightness (CM Punk: 15 for
// a colour of 13), and full blue (SvR's) made the hair shine blond and blotchy: the colour's brightness.
inline Bytes HairCut(const Bytes &normal, const Bytes &colour, const std::vector<UvTri> &hair) {
  svrfmt::Image n, c;
  svrfmt::DdsInfo ni;
  if (hair.empty() || !svrfmt::DdsInfoOf(normal, ni) || ni.format == svrfmt::DxtFormat::kDxt1 ||
      ni.format == svrfmt::DxtFormat::kUnknown || !svrfmt::DdsDecode(normal, n) || !svrfmt::DdsDecode(colour, c) ||
      n.w <= 0 || n.h <= 0 || c.w <= 0 || c.h <= 0)
    return normal;
  const auto keep = UvMask(hair, n.w, n.h);
  for (int py = 0; py < n.h; ++py)
    for (int px = 0; px < n.w; ++px) {
      if (!keep[size_t(py) * n.w + px])
        continue;
      const int cx0 = px * c.w / n.w, cx1 = std::max(cx0 + 1, (px + 1) * c.w / n.w);
      const int cy0 = py * c.h / n.h, cy1 = std::max(cy0 + 1, (py + 1) * c.h / n.h);
      unsigned sum = 0, cnt = 0;
      double lum = 0;
      for (int y = cy0; y < cy1 && y < c.h; ++y)
        for (int x = cx0; x < cx1 && x < c.w; ++x, ++cnt) {
          const uint8_t *p = &c.rgba[(size_t(y) * c.w + x) * 4];
          sum += p[3];
          lum += 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
        }
      if (cnt) {
        uint8_t *q = &n.rgba[(size_t(py) * n.w + px) * 4];
        q[2] = uint8_t(std::min(255.0, lum / cnt + 0.5));
        q[3] = uint8_t((sum + cnt / 2) / cnt);
      }
    }
  return svrfmt::DdsEncode(n, ni.format, ni.mips > 1);
}

// SvR's attire (its EMD PACH node) into the WWE '12 base attire's node (both expanded): the model, the colour and
// masks, the normal map. The base's other entries stay (eyelashes, material data, physics...).
// SvR's hair settings (entry 60: one line a chain, `kami_bNN radius visc grav spring damp A lencorr bend movecorr`,
// SvR's units) as WWE '12's (entry 60 "Version2": the chains' lines in four settings 1, 2, 3, 5 - each headed by two
// numbers, the hair stiffer and lighter from one to the next as the base superstars have them - then "ReferenceHair",
// five links a chain, none here). Radius and gravity x10 (the models' scale). Empty when SvR's has no chain.
inline Bytes HairSettings(const Bytes &svr60) {
  struct Line {
    std::string name;
    double v[9];
  };
  std::vector<Line> lines;
  std::string text(svr60.begin(), svr60.end());
  size_t at = 0;
  while (at < text.size()) {
    size_t e = text.find('\n', at);
    if (e == std::string::npos)
      e = text.size();
    const std::string l = text.substr(at, e - at);
    at = e + 1;
    char name[32] = {};
    Line x;
    if (sscanf(l.c_str(), " %31s %lf %lf %lf %lf %lf %lf %lf %lf %lf", name, &x.v[0], &x.v[1], &x.v[2], &x.v[3], &x.v[4],
               &x.v[5], &x.v[6], &x.v[7], &x.v[8]) == 10 &&
        std::string(name).rfind("kami_b", 0) == 0) {
      x.name = name;
      lines.push_back(x);
    }
  }
  if (lines.empty())
    return {};
  std::string o = "; SpringHairSetting (ported from SvR 2011)\r\nVersion2\r\n";
  const struct {
    int id;
    const char *head;
    double grav;  // (the base superstars' settings: gravity 0.52, 0.45, 0.3, 0.2)
  } sets[] = {{1, "0\t0.2", 1.0}, {2, "4\t0.2", 0.45 / 0.52}, {3, "6\t0", 0.3 / 0.52}, {5, "10\t0", 0.2 / 0.52}};
  char b[256];
  for (const auto &st : sets) {
    o += "Setting" + std::to_string(st.id) + "\r\n" + st.head + "\r\n";
    for (const auto &x : lines) {
      snprintf(b, sizeof(b), "%s\t%g\t%g\t%g\t%g\t%g\t%d\t%g\t%g\t%g\r\n", x.name.c_str(), x.v[0] * 10, x.v[1],
               x.v[2] * 10 * st.grav, x.v[3], x.v[4], int(x.v[5]), x.v[6], x.v[7], x.v[8]);
      o += b;
    }
    o += "*\r\n";
  }
  o += "ReferenceHair\r\n";
  for (const auto &x : lines)
    o += x.name + "\t0\t0\t0\t0\t0\r\n";
  o += "*\r\n";
  return Bytes(o.begin(), o.end());
}

// SvR 2008 / 2009 attires as SvR 2011 has them (after Kaiko's svr08_char.py, with his permission and credit): one
// picture set in entry 10 ("<id>color", "<id>normal", "<id>ao"; 2009 Maria's "12702nomal", 2008 Carlito's
// "1802color_01") where 2011 has the colour alone in 12 and "normal" in 10. The colour goes to a new 12 named "color",
// the normal map is named "normal", and the models (0, 10000, 20000) name them so. False: not such an attire (2010 /
// 2011: has its 12).
inline bool OldSvrPictures(svrfmt::ArcNode &svr) {
  auto child = [&](uint32_t id) -> svrfmt::ArcNode * {
    for (auto &k : svr.kids)
      if (k.id == id)
        return &k;
    return nullptr;
  };
  svrfmt::ArcNode *s10 = child(10);
  std::vector<svrfmt::BundleTexture> st;
  if (child(12) || !s10 || !svrfmt::BundleRead(svrfmt::ArchiveData(*s10), st))
    return false;
  auto lower = [](std::string s) {
    for (auto &c : s)
      c = char(std::tolower((unsigned char)c));
    return s;
  };
  // Its role by name: by the end, else anywhere in it (as Kaiko's pick), never a blood picture.
  auto role = [&](const std::string &name) -> std::string {
    const std::string n = lower(name);
    if (n.find("blood") != std::string::npos)
      return "";
    for (const char *r : {"color", "normal", "nomal"})
      if (n.find(r) != std::string::npos)
        return r[0] == 'c' ? "color" : "normal";
    return "";
  };
  int colour = -1, normal = -1;
  for (size_t i = 0; i < st.size(); ++i) {
    const std::string r = role(st[i].name);
    if (r == "color" && (colour < 0 || lower(st[i].name).ends_with("color")))
      colour = int(i);
    if (r == "normal" && normal < 0)
      normal = int(i);
  }
  // Else named by their ends (2008 / 2009 commentators: Jerry Lawler "5306all" / "5306allN" / "5306_AO", Tazz "5506" /
  // "5506_N" / "5506_ao"): "n" the normal map, "ao" the shading, the biggest other one the colour.
  if (colour < 0)
    for (size_t i = 0; i < st.size(); ++i) {
      const std::string n = lower(st[i].name);
      if (n.find("blood") != std::string::npos || n.ends_with("ao"))
        continue;
      if (n.ends_with("n")) {
        if (normal < 0)
          normal = int(i);
      } else if (colour < 0 || st[i].data.size() > st[size_t(colour)].data.size()) {
        colour = int(i);
      }
    }
  if (colour < 0)
    return false;
  std::vector<svrfmt::BundleTexture> rest;
  for (size_t i = 0; i < st.size(); ++i)
    if (int(i) != colour) {
      rest.push_back(st[i]);
      if (int(i) == normal)
        rest.back().name = "normal";
    }
  const std::string colour_name = st[size_t(colour)].name, normal_name = normal >= 0 ? st[size_t(normal)].name : "";
  svrfmt::BundleTexture c = st[size_t(colour)];
  c.name = "color";
  svrfmt::ArchiveReplace(*s10, svrfmt::BundleWrite(rest));
  std::string e;
  if (!svrfmt::ArchiveAdd(svr, "", "", 12, svrfmt::BundleWrite({c}), &e))
    return false;
  // The models' picture names (16-byte names at the header's +0x2C, after the 8-byte file head), in place. A name the
  // attire hasn't (2009 Jericho's eyebrows name "6102color", another superstar's sheet) is its colour, as Kaiko's
  // converter binds every piece to the attire's own colour.
  std::vector<std::string> have;
  for (const auto &t : rest)
    have.push_back(t.name);
  if (svrfmt::ArcNode *s11 = child(11)) {
    std::vector<svrfmt::BundleTexture> cube;
    if (svrfmt::BundleRead(svrfmt::ArchiveData(*s11), cube))
      for (const auto &t : cube)
        have.push_back(t.name);
  }
  for (uint32_t id : {0u, 10000u, 20000u}) {
    svrfmt::ArcNode *m = child(id);
    if (!m)
      continue;
    Bytes d = svrfmt::ArchiveData(*m);
    if (d.size() < 0x48 || std::memcmp(d.data(), "JBOY", 4))
      continue;
    const uint32_t ntex = svrfmt::Be32(&d[0x24]), tptr = svrfmt::Be32(&d[0x2C]);
    bool changed = false;
    for (uint32_t t = 0; t < ntex && 8 + tptr + 16 * size_t(t + 1) <= d.size(); ++t) {
      char *p = reinterpret_cast<char *>(&d[8 + tptr + 16 * t]);
      const std::string n(p, strnlen(p, 16));
      const char *to = SameName(n, colour_name) ? "color" : !normal_name.empty() && SameName(n, normal_name) ? "normal" : nullptr;
      bool known = n.empty();
      for (const auto &h : have)
        known = known || SameName(n, h);
      if (!to && !known)
        to = "color";
      if (!to)
        continue;
      std::memset(p, 0, 16);
      std::memcpy(p, to, std::strlen(to));
      changed = true;
    }
    if (changed)
      svrfmt::ArchiveReplace(*m, d);
  }
  svrfmt::ArchiveTouch(svr);
  return true;
}

// collision: the hair's collision capsules (entry 61, WWE '12's) for a base that has none (a long-haired superstar's,
// read from the player's disc); null: none.
inline bool PortAttire(svrfmt::ArcNode &svr, svrfmt::ArcNode &base, Report *rep, std::string *why,
                       const Bytes *collision = nullptr) {
  Report own;
  if (!rep)
    rep = &own;
  auto fail = [&](const std::string &w) {
    if (why)
      *why = w;
    return false;
  };
  if (!svrfmt::ArchiveExpand(svr) || !svrfmt::ArchiveExpand(base))
    return fail("an attire can't be read");
  const bool old_svr = OldSvrPictures(svr);
  if (old_svr)
    rep->notes.push_back("SvR 2008 / 2009 pictures sorted as SvR 2011's");
  auto child = [](svrfmt::ArcNode &n, uint32_t id) -> svrfmt::ArcNode * {
    for (auto &k : n.kids)
      if (k.id == id)
        return &k;
    return nullptr;
  };
  svrfmt::ArcNode *sm = child(svr, 0), *bm = child(base, 0);
  if (!sm || !bm)
    return fail("no model in an attire");
  svrfmt::Model ms, mb, mo;
  std::string e;
  if (!svrfmt::JboyRead(svrfmt::ArchiveData(*sm), ms, &e) || !svrfmt::JboyRead(svrfmt::ArchiveData(*bm), mb, &e))
    return fail("unreadable model: " + e);
  if (ms.w12)
    return fail("the SvR model is already in WWE '12's layout");
  std::map<std::string, M4> turns;  // (the body's, for its other models)
  if (!ConvertModel(ms, mb, mo, rep, false, nullptr, &turns))
    return fail("the model can't be converted");
  // SvR 2008 / 2009's 20000 isn't 2011's eye pieces but any small piece (2008 Rey Mysterio's eyebrows, Torrie
  // Wilson's wristbands, Orton's eyelashes): on WWE '12's 20000 (the eyelashes: the head's bones only) a wristband
  // stayed by the hip. Its pieces off the head are merged into the body (on its bones and skin), as Kaiko merges 2008's
  // costume pieces; those on the head stay in 20000, see-through (merged, 2008 Orton's eyelashes were opaque black rings
  // round his eyes, 2026-10-31). Likewise the hair model's pieces on none of the head's bones (2009 ch127: one on the
  // left hand, hips and thighs, left floating by the hip).
  if (old_svr) {
    auto merge = [&](const svrfmt::Model &xs, const std::string &what) {
      svrfmt::Model xo;
      Report extra;
      if (!ConvertModel(xs, mb, xo, &extra, false, &turns))
        return;
      const uint32_t first_part = uint32_t(mo.parts.size());
      for (auto &m : xo.meshes) {
        for (auto &p : m.params)  // (its own pictures, by name, in the body's list)
          if ((p.name == "g_mDiffusePrm" || p.name == "g_mNormalFxPrm") && p.value.size() >= 4) {
            const uint32_t v = svrfmt::Be32(p.value.data());
            if (v >= xo.textures.size())
              continue;
            size_t idx = mo.textures.size();
            for (size_t t = 0; t < mo.textures.size(); ++t)
              if (SameName(mo.textures[t], xo.textures[v]))
                idx = t;
            if (idx == mo.textures.size()) {
              mo.textures.push_back(xo.textures[v]);
              rep->textures.push_back(xo.textures[v]);
            }
            svrfmt::PutBe32(p.value.data(), uint32_t(idx));
          }
        if (m.raw.size() >= 0x64)
          svrfmt::PutBe32(&m.raw[0x60], svrfmt::Be32(&m.raw[0x60]) + first_part);
        mo.meshes.push_back(std::move(m));
      }
      mo.parts.insert(mo.parts.end(), xo.parts.begin(), xo.parts.end());
      rep->meshes = int(mo.meshes.size());
      rep->notes.push_back("SvR 2008 / 2009 " + what + " merged into the body");
    };
    // A model's meshes on the head (its bones, hair chains, neck) kept in it, the others merged into the body; none
    // on the head: the whole piece merged and removed (whole_off), else left as it is.
    auto split = [&](uint32_t id, const std::string &what, bool whole_off) {
      size_t at = svr.kids.size();
      for (size_t i = 0; i < svr.kids.size(); ++i)
        if (svr.kids[i].id == id)
          at = i;
      svrfmt::Model xs;
      if (at == svr.kids.size() || !svrfmt::JboyRead(svrfmt::ArchiveData(svr.kids[at]), xs, &e) || xs.w12)
        return;
      auto head = [&](int node) {  // (the head, its hair chains, the neck)
        for (int q = node; q >= 0 && size_t(q) < xs.nodes.size(); q = xs.nodes[size_t(q)].parent) {
          const std::string &n = xs.nodes[size_t(q)].name;
          if (n == "atama" || n.rfind("kubi", 0) == 0 || n.rfind("kami", 0) == 0)
            return true;
        }
        return false;
      };
      svrfmt::Model keep = xs, body = xs;
      keep.meshes.clear();
      keep.parts.clear();
      body.meshes.clear();
      body.parts.clear();
      size_t mi = 0;
      for (const auto &part : xs.parts)
        for (uint32_t k = 0; k < part.meshes && mi < xs.meshes.size(); ++k, ++mi) {
          bool on_head = false;
          for (int slot : xs.meshes[mi].palette)
            on_head = on_head || (slot > 0 && head(slot - 1));
          svrfmt::Model &to = on_head ? keep : body;
          if (to.parts.empty() || to.parts.back().name != part.name) {
            to.parts.push_back(part);
            to.parts.back().meshes = 0;
          }
          ++to.parts.back().meshes;
          to.meshes.push_back(xs.meshes[mi]);
        }
      if (body.meshes.empty() || mi != xs.meshes.size())
        return;
      if (keep.meshes.empty()) {
        if (whole_off) {
          merge(xs, what + " (" + xs.name + ")");
          svrfmt::ArchiveRemove(svr, at);
        }
        return;
      }
      merge(body, std::to_string(body.meshes.size()) + " " + what + " piece(s) off the head");
      if (keep.parts.size() == 1 && keep.header.size() >= 0x30) {  // (one part: written as the model's own name)
        keep.name = keep.parts[0].name;
        keep.group = keep.parts[0].info;
        svrfmt::PutBe32(&keep.header[0x2C], 1);  // (the file's +0x34: the parts' count)
      }
      svrfmt::ArchiveReplace(svr.kids[at], svrfmt::JboyWrite(keep));
    };
    split(20000, "piece 20000", true);
    split(10000, "hair-model", false);
  }
  svrfmt::ArchiveReplace(*bm, svrfmt::JboyWrite(mo));
  // The hair (10000) and the pieces round the eyes (20000: SvR's eyelids and the skin round the eyeballs, four
  // meshes; WWE '12's: eyelashes) as SvR has them too: the base's take their colours from the colour picture, which is
  // SvR's now (CM Punk of SvR: hair in the wrong place, dark rings round the eyes), and without SvR's 20000 the eye
  // sockets are holes (the background seen through his eyes, RenderDoc 2026-10-26). Converted on the base's piece of
  // the same number (its materials); the base's 20000 left out when SvR has none to put there.
  {
    svrfmt::ArcNode *s20 = child(svr, 20000);
    if (!s20)
      for (size_t i = 0; i < base.kids.size(); ++i)
        if (base.kids[i].id == 20000) {
          svrfmt::ArchiveRemove(base, i);
          break;
        }
  }
  std::vector<UvTri> see_through, hair_uv;  // UV triangles of the hair and the eyes' pieces; of the hair alone
  Bytes hair_settings;    // the hair's physics (entry 60) when its chains are kept
  bool hair_done = false;
  for (uint32_t id : {10000u, 20000u}) {
    svrfmt::ArcNode *s = child(svr, id), *b = child(base, id);
    svrfmt::Model xs, xb, xo;
    Report extra;
    if (!s || !b || !svrfmt::JboyRead(svrfmt::ArchiveData(*s), xs, &e) ||
        !svrfmt::JboyRead(svrfmt::ArchiveData(*b), xb, &e) || xs.w12)
      continue;
    // The hair keeps its chains when SvR has the settings that move them (entry 60), else hangs from head and chest.
    const Bytes settings = id == 10000 && child(svr, 60) ? HairSettings(svrfmt::ArchiveData(*child(svr, 60))) : Bytes();
    if (!ConvertModel(xs, xb, xo, &extra, !settings.empty(), &turns))
      continue;
    bool chains = false;
    for (const auto &n : xo.nodes)
      chains = chains || n.name.rfind("kami", 0) == 0;
    if (id == 10000) {
      hair_settings = chains ? settings : Bytes();
      hair_done = true;
    }
    if (!chains)
      HangHair(xs, xo);
    for (const auto &m : xo.meshes)  // (their places in the colour picture: its alpha kept there, below)
      for (const auto &st : m.strips) {
        const auto &ix = st.indices;
        for (size_t q = 0; q + 2 < ix.size(); q += st.IsStrip() ? 1 : 3)
          if (ix[q] < m.uvs.size() && ix[q + 1] < m.uvs.size() && ix[q + 2] < m.uvs.size()) {
            see_through.push_back({m.uvs[ix[q]], m.uvs[ix[q + 1]], m.uvs[ix[q + 2]]});
            if (id == 10000)
              hair_uv.push_back(see_through.back());
          }
      }
    svrfmt::ArchiveReplace(*b, svrfmt::JboyWrite(xo));
    b = nullptr;  // (adding or removing entries below moves the others)
    if (id == 10000 && hair_done) {
      if (!hair_settings.empty()) {
        if (svrfmt::ArcNode *b60 = child(base, 60))
          svrfmt::ArchiveReplace(*b60, hair_settings);
        else
          svrfmt::ArchiveAdd(base, "", "", 60, hair_settings, &e);
        if (!child(base, 61) && collision && !collision->empty())
          svrfmt::ArchiveAdd(base, "", "", 61, *collision, &e);
        rep->notes.push_back("hair physics: entry 60 written" + std::string(child(base, 61) ? ", collisions (61)" : ""));
      } else {
        // (a base with chains of its own: its settings would name bones the hair hasn't now)
        for (uint32_t gone : {60u, 61u})
          for (size_t i = 0; i < base.kids.size(); ++i)
            if (base.kids[i].id == gone) {
              svrfmt::ArchiveRemove(base, i);
              break;
            }
      }
    }
    rep->textures.insert(rep->textures.end(), extra.textures.begin(), extra.textures.end());
  }
  // Colour and masks as SvR has them (same bundle format), all three masks: the base's Mask3 (15) fits the base's
  // colour picture, not SvR's (CM Punk of SvR came out with white eyes and a wrong face, 2026-10-26). The colour in
  // the base's DDS format (WWE '12: DXT1, half of SvR's DXT5) when it differs, so it keeps its full size in the
  // attire's room.
  for (uint32_t id : {12u, 13u, 14u, 15u}) {
    svrfmt::ArcNode *s = child(svr, id), *b = child(base, id);
    if (!s || !b)
      continue;
    std::vector<svrfmt::BundleTexture> st, bt;
    const bool read = id == 12 && svrfmt::BundleRead(svrfmt::ArchiveData(*s), st) &&
                      svrfmt::BundleRead(svrfmt::ArchiveData(*b), bt) && st.size() == 1 && !bt.empty();
    b->stored = s->stored;
    b->dirty = true;
    if (!read)
      continue;
    svrfmt::DdsInfo si, bi;
    svrfmt::Image img;
    if (svrfmt::DdsInfoOf(st[0].data, si) && svrfmt::DdsInfoOf(bt[0].data, bi) && si.format != bi.format &&
        bi.format == svrfmt::DxtFormat::kDxt1 && svrfmt::DdsDecode(st[0].data, img)) {
      // Opaque first: SvR's alpha is something else on the body, and DXT1 would make every pixel under half alpha a
      // transparent (black) one (the whole body came out black).
      // Except where the hair and the eyes' pieces (eyelashes, strands) take their colours: there SvR's alpha is
      // their see-through edges (WWE '12 draws them blended): kept (DXT1: on or off), else they came out as solid
      // dark bands (CM Punk: bars under the eyes).
      const auto keep = UvMask(see_through, img.w, img.h);
      for (size_t q = 0; q < keep.size(); ++q)
        if (!keep[q])
          img.rgba[q * 4 + 3] = 255;
      st[0].data = svrfmt::DdsEncode(img, svrfmt::DxtFormat::kDxt1, si.mips > 1);
      svrfmt::ArchiveReplace(*b, svrfmt::BundleWrite(st));
    }
  }
  // A Mask3 (15) the other game hasn't: the base's would be on the wrong picture: left out (WWE 2K17's AJ Styles has
  // none and looks right).
  if (!child(svr, 15))
    for (size_t i = 0; i < base.kids.size(); ++i)
      if (base.kids[i].id == 15) {
        svrfmt::ArchiveRemove(base, i);
        break;
      }
  // The normal map, in the base's bundle 10.
  svrfmt::ArcNode *s10 = child(svr, 10), *b10 = child(base, 10), *s12 = child(svr, 12);
  if (s10 && b10) {
    std::vector<svrfmt::BundleTexture> st, bt, sc;
    if (svrfmt::BundleRead(svrfmt::ArchiveData(*s10), st) && svrfmt::BundleRead(svrfmt::ArchiveData(*b10), bt)) {
      const bool colour = s12 && svrfmt::BundleRead(svrfmt::ArchiveData(*s12), sc) && !sc.empty();
      for (auto &t : bt)
        for (const auto &u : st)
          if (t.name == "normal" && u.name == "normal") {
            t.data = Shine(u.data, t.data);
            if (colour)
              t.data = HairCut(t.data, sc[0].data, hair_uv);
          }
      // The pictures of its own the model now names (the hands').
      for (const auto &name : rep->textures)
        for (const auto &u : st)
          if (SameName(u.name, name)) {
            bool have = false;
            for (const auto &t : bt)
              have = have || SameName(t.name, name);
            if (!have)
              bt.push_back(u);
          }
      svrfmt::ArchiveReplace(*b10, svrfmt::BundleWrite(bt));
    }
  }
  svrfmt::ArchiveTouch(base);
  return true;
}

}  // namespace port_svr
