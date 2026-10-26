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
inline bool ConvertModel(const svrfmt::Model &svr, const svrfmt::Model &base, svrfmt::Model &out, Report *rep) {
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
    for (size_t c = i + 1; c < svr.nodes.size(); ++c)
      if (svr.nodes[c].parent == int(i) && !MapBone(svr.nodes[c].name).empty()) {
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
// the ported Orton shone too much). SvR's normal map with its alpha scaled to the base's mean, in the base's format.
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
  const double ms = mean(s), mb = mean(b);
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

// SvR's attire (its EMD PACH node) into the WWE '12 base attire's node (both expanded): the model, the colour and
// masks, the normal map. The base's other entries stay (eyelashes, material data, physics...).
inline bool PortAttire(svrfmt::ArcNode &svr, svrfmt::ArcNode &base, Report *rep, std::string *why) {
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
  if (!ConvertModel(ms, mb, mo, rep))
    return fail("the model can't be converted");
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
  std::vector<std::array<std::array<float, 2>, 3>> see_through;  // UV triangles of the hair and the eyes' pieces
  for (uint32_t id : {10000u, 20000u}) {
    svrfmt::ArcNode *s = child(svr, id), *b = child(base, id);
    svrfmt::Model xs, xb, xo;
    Report extra;
    if (!s || !b || !svrfmt::JboyRead(svrfmt::ArchiveData(*s), xs, &e) ||
        !svrfmt::JboyRead(svrfmt::ArchiveData(*b), xb, &e) || xs.w12 || !ConvertModel(xs, xb, xo, &extra))
      continue;
    HangHair(xs, xo);
    for (const auto &m : xo.meshes)  // (their places in the colour picture: its alpha kept there, below)
      for (const auto &st : m.strips) {
        const auto &ix = st.indices;
        for (size_t q = 0; q + 2 < ix.size(); q += st.IsStrip() ? 1 : 3)
          if (ix[q] < m.uvs.size() && ix[q + 1] < m.uvs.size() && ix[q + 2] < m.uvs.size())
            see_through.push_back({m.uvs[ix[q]], m.uvs[ix[q + 1]], m.uvs[ix[q + 2]]});
      }
    svrfmt::ArchiveReplace(*b, svrfmt::JboyWrite(xo));
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
      std::vector<uint8_t> keep(size_t(img.w) * img.h, 0);
      for (const auto &t : see_through) {
        float x[3], y[3];
        for (int c = 0; c < 3; ++c) {
          x[c] = t[size_t(c)][0] * img.w;
          y[c] = t[size_t(c)][1] * img.h;
        }
        const int x0 = std::max(0, int(std::floor(std::min({x[0], x[1], x[2]})))),
                  x1 = std::min(img.w - 1, int(std::ceil(std::max({x[0], x[1], x[2]})))),
                  y0 = std::max(0, int(std::floor(std::min({y[0], y[1], y[2]})))),
                  y1 = std::min(img.h - 1, int(std::ceil(std::max({y[0], y[1], y[2]}))));
        const float area = (x[1] - x[0]) * (y[2] - y[0]) - (x[2] - x[0]) * (y[1] - y[0]);
        if (std::abs(area) < 1e-6f || x1 - x0 > img.w / 2 || y1 - y0 > img.h / 2)
          continue;
        for (int py = y0; py <= y1; ++py)
          for (int px = x0; px <= x1; ++px) {
            const float cx = px + 0.5f, cy = py + 0.5f;
            const float w0 = (x[1] - cx) * (y[2] - cy) - (x[2] - cx) * (y[1] - cy);
            const float w1 = (x[2] - cx) * (y[0] - cy) - (x[0] - cx) * (y[2] - cy);
            const float w2 = (x[0] - cx) * (y[1] - cy) - (x[1] - cx) * (y[0] - cy);
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0))
              keep[size_t(py) * img.w + px] = 1;
          }
      }
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
  svrfmt::ArcNode *s10 = child(svr, 10), *b10 = child(base, 10);
  if (s10 && b10) {
    std::vector<svrfmt::BundleTexture> st, bt;
    if (svrfmt::BundleRead(svrfmt::ArchiveData(*s10), st) && svrfmt::BundleRead(svrfmt::ArchiveData(*b10), bt)) {
      for (auto &t : bt)
        for (const auto &u : st)
          if (t.name == "normal" && u.name == "normal")
            t.data = Shine(u.data, t.data);
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
