// A superstar's model from an FBX file (docs/mod_maker.md, "Import FBX"), made into a WWE '12 attire over a base
// attire (its skeleton, materials and other entries), as port_svr.h does for SvR 2011's.
//
// Taken from Kaiko's WWE SmackDown vs. Raw 2011 PC port (with his permission, credit given),
// https://github.com/KaikoClanworth1/wwe-svr2011-pc, port/modmaker/svrfmt/char_fbx.cpp at b2521fe: the FBX reading
// (ufbx), the pose (each mapped bone turned to the template bone's direction, the hands rolled to its knuckle line;
// the mesh follows by its own skin), the scale (the soles as far below the hips as the template's), the detail
// reduction (Decimate) and the texture sheet. His builder made a SvR 2011 model from that; here:
//  - the template is the base attire's WWE '12 skeleton (rig_map.cpp names SvR 2011's bones, port_svr::MapBone gives
//    WWE '12's), in its own units (cm from the floor, y down: the same axes as SvR 2011's);
//  - each vertex then moves by its bones' joints, from the model's to the base's (the game keeps its skeleton);
//  - meshes take a base mesh's material: skin (a plain yCh_skin one: no muscle or wrinkle maps, they fit the base's
//    pictures only), clothes (yCh_gear), eyes (yCh_Reflect); at most 20 bones a mesh (the game's palette);
//  - the pictures: one colour sheet the size of the base's (in its format), the normal map flat where the FBX has
//    none, Superstar Threads masks blank; the base's hair (10000) and eyelashes (20000) left out (bald superstars
//    have none either).
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "port_svr.h"
#include "rig_map.h"
#include "svrfmt/archive.h"
#include "svrfmt/jboy.h"
#include "svrfmt/texture.h"
#include "third_party/ufbx/ufbx.h"
#include "wic_image.h"

namespace fbx_star {

using svrfmt::Bytes;
namespace fs = std::filesystem;

struct Options {
  int max_vertices = 12000;                 // fewer vertices past this (0: as they are); stock bodies ~9,000
  bool keep_face = true;                    // ... the head's kept as long as possible
  std::map<std::string, bool> include;      // by mesh name ("node / material"): off leaves it out
  std::map<std::string, std::string> kind;  // by mesh name: "skin", "gear", "eye", "hair" (default: by its names)
};

struct Report {
  std::string rig;
  int bones = 0, mapped = 0, meshes = 0, vertices = 0, triangles = 0, textures = 0;
  int reduced_from = 0;  // vertices before the reduction (0: none)
  double scale = 1;
  std::vector<std::string> warnings, lines;
};

namespace detail {

using V3 = std::array<double, 3>;
struct M {  // 3x3 + translation, column vectors: p' = r p + t
  double r[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
  double t[3] = {0, 0, 0};
};
inline M Mul(const M &a, const M &b) {
  M o;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j)
      o.r[i][j] = a.r[i][0] * b.r[0][j] + a.r[i][1] * b.r[1][j] + a.r[i][2] * b.r[2][j];
    o.t[i] = a.r[i][0] * b.t[0] + a.r[i][1] * b.t[1] + a.r[i][2] * b.t[2] + a.t[i];
  }
  return o;
}
inline M InvRigid(const M &a) {
  M o;
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j)
      o.r[i][j] = a.r[j][i];
  for (int i = 0; i < 3; ++i)
    o.t[i] = -(o.r[i][0] * a.t[0] + o.r[i][1] * a.t[1] + o.r[i][2] * a.t[2]);
  return o;
}
inline M InvAffine(const M &a) {  // (bind matrices can carry scale)
  const double(*r)[3] = a.r;
  const double det = r[0][0] * (r[1][1] * r[2][2] - r[1][2] * r[2][1]) - r[0][1] * (r[1][0] * r[2][2] - r[1][2] * r[2][0]) +
                     r[0][2] * (r[1][0] * r[2][1] - r[1][1] * r[2][0]);
  M o;
  if (std::fabs(det) < 1e-12)
    return o;
  const double k = 1.0 / det;
  o.r[0][0] = (r[1][1] * r[2][2] - r[1][2] * r[2][1]) * k, o.r[0][1] = (r[0][2] * r[2][1] - r[0][1] * r[2][2]) * k;
  o.r[0][2] = (r[0][1] * r[1][2] - r[0][2] * r[1][1]) * k, o.r[1][0] = (r[1][2] * r[2][0] - r[1][0] * r[2][2]) * k;
  o.r[1][1] = (r[0][0] * r[2][2] - r[0][2] * r[2][0]) * k, o.r[1][2] = (r[0][2] * r[1][0] - r[0][0] * r[1][2]) * k;
  o.r[2][0] = (r[1][0] * r[2][1] - r[1][1] * r[2][0]) * k, o.r[2][1] = (r[0][1] * r[2][0] - r[0][0] * r[2][1]) * k;
  o.r[2][2] = (r[0][0] * r[1][1] - r[0][1] * r[1][0]) * k;
  for (int i = 0; i < 3; ++i)
    o.t[i] = -(o.r[i][0] * a.t[0] + o.r[i][1] * a.t[1] + o.r[i][2] * a.t[2]);
  return o;
}
inline M FromUfbx(const ufbx_matrix &u) {
  M m;
  m.r[0][0] = u.m00, m.r[0][1] = u.m01, m.r[0][2] = u.m02, m.t[0] = u.m03;
  m.r[1][0] = u.m10, m.r[1][1] = u.m11, m.r[1][2] = u.m12, m.t[1] = u.m13;
  m.r[2][0] = u.m20, m.r[2][1] = u.m21, m.r[2][2] = u.m22, m.t[2] = u.m23;
  return m;
}
inline V3 Apply(const M &m, const V3 &p) {
  return {m.r[0][0] * p[0] + m.r[0][1] * p[1] + m.r[0][2] * p[2] + m.t[0],
          m.r[1][0] * p[0] + m.r[1][1] * p[1] + m.r[1][2] * p[2] + m.t[1],
          m.r[2][0] * p[0] + m.r[2][1] * p[1] + m.r[2][2] * p[2] + m.t[2]};
}
inline V3 ApplyDir(const M &m, const V3 &p) {
  return {m.r[0][0] * p[0] + m.r[0][1] * p[1] + m.r[0][2] * p[2], m.r[1][0] * p[0] + m.r[1][1] * p[1] + m.r[1][2] * p[2],
          m.r[2][0] * p[0] + m.r[2][1] * p[1] + m.r[2][2] * p[2]};
}
inline V3 Sub(const V3 &a, const V3 &b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
inline double Dot(const V3 &a, const V3 &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
inline V3 Cross(const V3 &a, const V3 &b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
inline double Len(const V3 &a) { return std::sqrt(Dot(a, a)); }
inline V3 Norm(const V3 &a) {
  const double l = Len(a);
  return l > 1e-12 ? V3{a[0] / l, a[1] / l, a[2] / l} : V3{0, 0, 0};
}
inline M AxisAngle(const V3 &k, double a) {  // (Rodrigues, k a unit axis)
  M m;
  const double c = std::cos(a), s = std::sin(a), t = 1 - c;
  m.r[0][0] = t * k[0] * k[0] + c, m.r[0][1] = t * k[0] * k[1] - s * k[2], m.r[0][2] = t * k[0] * k[2] + s * k[1];
  m.r[1][0] = t * k[0] * k[1] + s * k[2], m.r[1][1] = t * k[1] * k[1] + c, m.r[1][2] = t * k[1] * k[2] - s * k[0];
  m.r[2][0] = t * k[0] * k[2] - s * k[1], m.r[2][1] = t * k[1] * k[2] + s * k[0], m.r[2][2] = t * k[2] * k[2] + c;
  return m;
}
inline M Swing(const V3 &a0, const V3 &b0) {  // the shortest rotation taking direction a onto b
  const V3 a = Norm(a0), b = Norm(b0);
  V3 axis = Cross(a, b);
  const double s = Len(axis), c = Dot(a, b);
  if (s < 1e-9) {
    if (c > 0)
      return M();
    axis = Cross(a, std::fabs(a[0]) < 0.9 ? V3{1, 0, 0} : V3{0, 1, 0});
  }
  return AxisAngle(Norm(axis), std::atan2(s, c));
}
inline std::string Lower(std::string s) {
  for (auto &c : s)
    c = char(std::tolower(uint8_t(c)));
  return s;
}
inline std::string Str(const ufbx_string &s) { return std::string(s.data, s.length); }

// The SvR 2011 bones whose direction the model takes (bone -> the joint it points at), top down.
inline const char *const kAimPairs[][2] = {
    {"koshi", "mune"},          {"mune", "kubi"},         {"kubi", "atama"},
    {"l_sakotsu", "l_ninoude"}, {"l_ninoude", "l_kote"},  {"l_kote", "l_te"},     {"l_te", "l_yubi20"},
    {"r_sakotsu", "r_ninoude"}, {"r_ninoude", "r_kote"},  {"r_kote", "r_te"},     {"r_te", "r_yubi20"},
    {"l_yubi00", "l_yubi01"},   {"l_yubi01", "l_yubi02"}, {"l_yubi10", "l_yubi11"}, {"l_yubi11", "l_yubi12"},
    {"l_yubi20", "l_yubi21"},   {"l_yubi21", "l_yubi22"}, {"l_yubi30", "l_yubi31"}, {"l_yubi31", "l_yubi32"},
    {"l_yubi40", "l_yubi41"},   {"l_yubi41", "l_yubi42"}, {"r_yubi00", "r_yubi01"}, {"r_yubi01", "r_yubi02"},
    {"r_yubi10", "r_yubi11"},   {"r_yubi11", "r_yubi12"}, {"r_yubi20", "r_yubi21"}, {"r_yubi21", "r_yubi22"},
    {"r_yubi30", "r_yubi31"},   {"r_yubi31", "r_yubi32"}, {"r_yubi40", "r_yubi41"}, {"r_yubi41", "r_yubi42"},
    {"l_momo", "l_sune"},       {"l_sune", "l_ashi"},     {"l_ashi", "l_tsumasaki"}, {"r_momo", "r_sune"},
    {"r_sune", "r_ashi"},       {"r_ashi", "r_tsumasaki"},
};

// A SvR 2011 bone's parent (for a bone the base hasn't: its weights go up the chain).
inline std::string Parent2011(const std::string &n) {
  if (n.size() > 2 && n[1] == '_' && (n[0] == 'l' || n[0] == 'r')) {
    const std::string s = n.substr(0, 2), b = n.substr(2);
    if (b.rfind("yubi", 0) == 0 && b.size() == 6)
      return b[5] > '0' ? s + b.substr(0, 5) + char(b[5] - 1) : s + "te";
    static const std::map<std::string, std::string> up = {
        {"te", "kote"},     {"kote_x", "kote"}, {"kote_x2", "kote"}, {"kote", "ninoude"}, {"ninoude_x", "ninoude"},
        {"ninoude", "sakotsu"}, {"tsumasaki", "ashi"}, {"ashi", "sune"}, {"sune", "momo"}, {"momo_x", "momo"},
        {"eye", ""}};
    if (auto it = up.find(b); it != up.end())
      return it->second.empty() ? "atama" : s + it->second;
    if (b == "sakotsu")
      return "mune";
    if (b == "momo")
      return "root";
  }
  static const std::map<std::string, std::string> trunk = {
      {"d_kuchi", "atama"}, {"atama", "kubi"}, {"kubi", "mune"}, {"mune", "koshi"}, {"koshi", "root"}};
  if (auto it = trunk.find(n); it != trunk.end())
    return it->second;
  return "";
}

struct OutMesh {
  std::string name, kind, texture_key;
  std::vector<V3> pos, nrm;
  std::vector<std::array<double, 2>> uv;
  std::vector<std::vector<std::pair<int, double>>> w;  // (output bone, weight)
  std::vector<std::array<int, 3>> tris;
};

// Fewer vertices (Kaiko's): quadric error half-edge collapses - a vertex merges into a neighbour, so the kept ones
// keep their exact UVs, normals and weights; seam vertices and open edges stay put; a collapse that flips or squashes
// a triangle is refused. keep: the vertices to keep at most; lock: vertices dear to remove (the face).
struct Quadric {
  double a[10] = {};
  void AddPlane(const V3 &n, double d, double w) {
    const double p[4] = {n[0], n[1], n[2], d};
    int k = 0;
    for (int i = 0; i < 4; ++i)
      for (int j = i; j < 4; ++j)
        a[k++] += w * p[i] * p[j];
  }
  void Add(const Quadric &q) {
    for (int k = 0; k < 10; ++k)
      a[k] += q.a[k];
  }
  double Eval(const V3 &v) const {
    const double x = v[0], y = v[1], z = v[2];
    return a[0] * x * x + 2 * a[1] * x * y + 2 * a[2] * x * z + 2 * a[3] * x + a[4] * y * y + 2 * a[5] * y * z +
           2 * a[6] * y + a[7] * z * z + 2 * a[8] * z + a[9];
  }
};

inline int Decimate(OutMesh &m, int keep, std::vector<bool> *lock = nullptr) {
  const int nv = int(m.pos.size());
  if (keep >= nv || m.tris.empty())
    return nv;
  std::map<std::array<double, 3>, int> weld;
  std::vector<int> pid(static_cast<size_t>(nv));
  std::vector<std::vector<int>> members;
  for (int i = 0; i < nv; ++i) {
    auto [it, fresh] = weld.emplace(m.pos[size_t(i)], int(members.size()));
    if (fresh)
      members.emplace_back();
    pid[size_t(i)] = it->second;
    members[size_t(it->second)].push_back(i);
  }
  const int np = int(members.size());
  std::vector<bool> plock(static_cast<size_t>(np));
  if (lock)
    for (int i = 0; i < nv && i < int(lock->size()); ++i)
      if ((*lock)[size_t(i)])
        plock[size_t(pid[size_t(i)])] = true;
  std::vector<std::vector<int>> vt(static_cast<size_t>(nv));
  for (int t = 0; t < int(m.tris.size()); ++t)
    for (int c = 0; c < 3; ++c)
      vt[size_t(m.tris[size_t(t)][size_t(c)])].push_back(t);
  auto key = [](int a, int b) { return a < b ? std::make_pair(a, b) : std::make_pair(b, a); };
  std::map<std::pair<int, int>, std::set<std::pair<int, int>>> variants;
  std::map<std::pair<int, int>, int> uses;
  for (const auto &t : m.tris)
    for (int c = 0; c < 3; ++c) {
      const int a = t[size_t(c)], b = t[size_t((c + 1) % 3)];
      const auto k = key(pid[size_t(a)], pid[size_t(b)]);
      ++uses[k];
      variants[k].insert(pid[size_t(a)] < pid[size_t(b)] ? std::make_pair(a, b) : std::make_pair(b, a));
    }
  std::vector<std::set<int>> fadj(static_cast<size_t>(np));
  for (const auto &[k, n] : uses)
    if (n == 1 || variants[k].size() > 1)
      fadj[size_t(k.first)].insert(k.second), fadj[size_t(k.second)].insert(k.first);
  auto is_feature = [&](int a, int b) { return fadj[size_t(a)].count(b) > 0; };
  std::vector<Quadric> q(static_cast<size_t>(np));
  for (const auto &t : m.tris) {
    const V3 &p0 = m.pos[size_t(t[0])], &p1 = m.pos[size_t(t[1])], &p2 = m.pos[size_t(t[2])];
    const V3 c = Cross(Sub(p1, p0), Sub(p2, p0));
    const double area = Len(c);
    if (area < 1e-12)
      continue;
    const V3 n = Norm(c);
    for (int k = 0; k < 3; ++k)
      q[size_t(pid[size_t(t[size_t(k)])])].AddPlane(n, -Dot(n, p0), area);
    for (int k = 0; k < 3; ++k) {
      const int a = t[size_t(k)], b = t[size_t((k + 1) % 3)];
      if (!is_feature(pid[size_t(a)], pid[size_t(b)]))
        continue;
      const V3 e = Sub(m.pos[size_t(b)], m.pos[size_t(a)]);
      const V3 en = Norm(Cross(e, n));
      const double w = Dot(e, e) * 8;
      q[size_t(pid[size_t(a)])].AddPlane(en, -Dot(en, m.pos[size_t(a)]), w);
      q[size_t(pid[size_t(b)])].AddPlane(en, -Dot(en, m.pos[size_t(a)]), w);
    }
  }
  std::vector<bool> tri_dead(m.tris.size()), dead(static_cast<size_t>(nv)), pdead(static_cast<size_t>(np));
  std::vector<int> stamp(static_cast<size_t>(np));
  auto movable = [&](int pu) { return fadj[size_t(pu)].empty() || fadj[size_t(pu)].size() == 2; };
  struct Cand {
    double cost;
    int pu, pv, su;
  };
  auto worse = [](const Cand &a, const Cand &b) { return a.cost > b.cost; };
  std::vector<Cand> heap;
  auto push_from = [&](int pu) {
    if (pdead[size_t(pu)] || !movable(pu))
      return;
    std::set<int> seen;
    for (int u : members[size_t(pu)])
      for (int t : vt[size_t(u)]) {
        if (tri_dead[size_t(t)])
          continue;
        for (int x : m.tris[size_t(t)]) {
          const int pv = pid[size_t(x)];
          if (pv == pu || !seen.insert(pv).second)
            continue;
          if (!fadj[size_t(pu)].empty() && !is_feature(pu, pv))
            continue;
          Quadric s = q[size_t(pu)];
          s.Add(q[size_t(pv)]);
          const double cost = s.Eval(m.pos[size_t(x)]);
          heap.push_back({plock[size_t(pu)] ? cost * 30 + 2e-4 : cost, pu, pv, stamp[size_t(pu)]});
          std::push_heap(heap.begin(), heap.end(), worse);
        }
      }
  };
  for (int p = 0; p < np; ++p)
    push_from(p);
  int live = nv;
  while (live > keep && !heap.empty()) {
    std::pop_heap(heap.begin(), heap.end(), worse);
    const Cand c = heap.back();
    heap.pop_back();
    if (pdead[size_t(c.pu)] || pdead[size_t(c.pv)] || stamp[size_t(c.pu)] != c.su)
      continue;
    std::vector<std::pair<int, int>> pairs;
    bool ok = true;
    for (int u : members[size_t(c.pu)]) {
      int to = -1;
      for (int t : vt[size_t(u)]) {
        if (tri_dead[size_t(t)])
          continue;
        for (int x : m.tris[size_t(t)])
          if (pid[size_t(x)] == c.pv)
            to = x;
      }
      if (to < 0) {
        ok = false;
        break;
      }
      pairs.push_back({u, to});
    }
    if (!ok || pairs.empty())
      continue;
    for (const auto &[u, v] : pairs) {
      for (int t : vt[size_t(u)]) {
        if (tri_dead[size_t(t)])
          continue;
        auto tr = m.tris[size_t(t)];
        if (pid[size_t(tr[0])] == c.pv || pid[size_t(tr[1])] == c.pv || pid[size_t(tr[2])] == c.pv)
          continue;
        const V3 before =
            Cross(Sub(m.pos[size_t(tr[1])], m.pos[size_t(tr[0])]), Sub(m.pos[size_t(tr[2])], m.pos[size_t(tr[0])]));
        for (auto &x : tr)
          if (x == u)
            x = v;
        const V3 after =
            Cross(Sub(m.pos[size_t(tr[1])], m.pos[size_t(tr[0])]), Sub(m.pos[size_t(tr[2])], m.pos[size_t(tr[0])]));
        if (Len(after) < 1e-10 || Dot(Norm(before), Norm(after)) < 0.3)
          ok = false;
      }
      if (!ok)
        break;
    }
    if (!ok)
      continue;
    for (const auto &[u, v] : pairs) {
      for (int t : vt[size_t(u)]) {
        if (tri_dead[size_t(t)])
          continue;
        auto &tr = m.tris[size_t(t)];
        if (pid[size_t(tr[0])] == c.pv || pid[size_t(tr[1])] == c.pv || pid[size_t(tr[2])] == c.pv) {
          tri_dead[size_t(t)] = true;
          continue;
        }
        for (auto &x : tr)
          if (x == u)
            x = v;
        vt[size_t(v)].push_back(t);
      }
      dead[size_t(u)] = true;
      --live;
    }
    for (int other : fadj[size_t(c.pu)]) {
      fadj[size_t(other)].erase(c.pu);
      if (other != c.pv)
        fadj[size_t(other)].insert(c.pv), fadj[size_t(c.pv)].insert(other);
    }
    fadj[size_t(c.pu)].clear();
    fadj[size_t(c.pv)].erase(c.pu);
    pdead[size_t(c.pu)] = true;
    q[size_t(c.pv)].Add(q[size_t(c.pu)]);
    std::set<int> around;
    for (int v : members[size_t(c.pv)])
      for (int t : vt[size_t(v)])
        if (!tri_dead[size_t(t)])
          for (int x : m.tris[size_t(t)])
            around.insert(pid[size_t(x)]);
    for (int p : around)
      ++stamp[size_t(p)], push_from(p);
    if (heap.size() > size_t(nv) * 64) {
      heap.clear();
      for (int p = 0; p < np; ++p)
        push_from(p);
    }
  }
  std::vector<bool> used(static_cast<size_t>(nv));
  for (size_t t = 0; t < m.tris.size(); ++t)
    if (!tri_dead[t])
      for (int x : m.tris[t])
        used[size_t(x)] = true;
  OutMesh f;
  f.name = m.name, f.kind = m.kind, f.texture_key = m.texture_key;
  std::vector<int> remap(size_t(nv), -1);
  std::vector<bool> kept_lock;
  for (int i = 0; i < nv; ++i)
    if (used[size_t(i)] && !dead[size_t(i)]) {
      remap[size_t(i)] = int(f.pos.size());
      f.pos.push_back(m.pos[size_t(i)]);
      f.nrm.push_back(m.nrm[size_t(i)]);
      f.uv.push_back(m.uv[size_t(i)]);
      f.w.push_back(m.w[size_t(i)]);
      if (lock)
        kept_lock.push_back(i < int(lock->size()) && (*lock)[size_t(i)]);
    }
  if (lock)
    *lock = std::move(kept_lock);
  for (size_t t = 0; t < m.tris.size(); ++t) {
    if (tri_dead[t])
      continue;
    const auto &tr = m.tris[t];
    if (remap[size_t(tr[0])] < 0 || remap[size_t(tr[1])] < 0 || remap[size_t(tr[2])] < 0)
      continue;
    f.tris.push_back({remap[size_t(tr[0])], remap[size_t(tr[1])], remap[size_t(tr[2])]});
  }
  m = std::move(f);
  return int(m.pos.size());
}

// A TGA picture (uncompressed or RLE, 24 / 32 bits, or 8-bit grey): WIC doesn't read them; game rips often are.
inline bool LoadTga(const Bytes &b, svrfmt::Image &out) {
  if (b.size() < 18)
    return false;
  const int idlen = b[0], cmap = b[1], type = b[2], w = b[12] | b[13] << 8, h = b[14] | b[15] << 8, bpp = b[16],
            desc = b[17];
  if (cmap || (type != 2 && type != 10 && type != 3 && type != 11) || w <= 0 || h <= 0 || w > 8192 || h > 8192 ||
      (bpp != 24 && bpp != 32 && bpp != 8))
    return false;
  const int px = bpp / 8;
  size_t at = 18 + size_t(idlen);
  std::vector<uint8_t> raw(size_t(w) * h * px);
  if (type == 2 || type == 3) {
    if (at + raw.size() > b.size())
      return false;
    std::memcpy(raw.data(), &b[at], raw.size());
  } else {
    size_t o = 0;
    while (o < raw.size() && at < b.size()) {
      const uint8_t c = b[at++];
      const size_t n = size_t(c & 0x7F) + 1;
      if (c & 0x80) {
        if (at + px > b.size())
          return false;
        for (size_t k = 0; k < n && o < raw.size(); ++k, o += px)
          std::memcpy(&raw[o], &b[at], px);
        at += px;
      } else {
        const size_t len = std::min(n * px, raw.size() - o);
        if (at + len > b.size())
          return false;
        std::memcpy(&raw[o], &b[at], len);
        o += len, at += len;
      }
    }
  }
  out.w = w, out.h = h;
  out.rgba.assign(size_t(w) * h * 4, 255);
  const bool top = desc & 0x20;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const uint8_t *s = &raw[(size_t(top ? y : h - 1 - y) * w + x) * px];
      uint8_t *d = &out.rgba[(size_t(y) * w + x) * 4];
      if (px == 1)
        d[0] = d[1] = d[2] = s[0];
      else
        d[0] = s[2], d[1] = s[1], d[2] = s[0], d[3] = px == 4 ? s[3] : 255;
    }
  return true;
}

// A picture from its bytes (PNG, JPEG, BMP... through WIC; DDS; TGA).
inline bool LoadPicture(const Bytes &b, svrfmt::Image &out) {
  if (b.size() > 4 && !std::memcmp(b.data(), "DDS ", 4))
    return svrfmt::DdsDecode(b, out);
  if (LoadTga(b, out))
    return true;
  auto wic = wic_image::Factory();
  wic_image::ComPtr<IWICStream> stream;
  wic_image::ComPtr<IWICBitmapDecoder> dec;
  wic_image::ComPtr<IWICBitmapFrameDecode> frame;
  wic_image::ComPtr<IWICFormatConverter> conv;
  if (!wic || FAILED(wic->CreateStream(&stream)) ||
      FAILED(stream->InitializeFromMemory(const_cast<BYTE *>(b.data()), DWORD(b.size()))) ||
      FAILED(wic->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &dec)) ||
      FAILED(dec->GetFrame(0, &frame)) || FAILED(wic->CreateFormatConverter(&conv)) ||
      FAILED(conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0,
                              WICBitmapPaletteTypeCustom)))
    return false;
  UINT w = 0, h = 0;
  conv->GetSize(&w, &h);
  if (!w || !h || w > 8192 || h > 8192)
    return false;
  out.w = int(w), out.h = int(h);
  out.rgba.resize(size_t(w) * h * 4);
  return SUCCEEDED(conv->CopyPixels(nullptr, w * 4, UINT(out.rgba.size()), out.rgba.data()));
}

// The texture a material's colour / normal map comes from.
inline const ufbx_texture *MaterialTexture(const ufbx_material *m, bool normal) {
  if (!m)
    return nullptr;
  if (normal) {
    if (m->pbr.normal_map.texture)
      return m->pbr.normal_map.texture;
    return m->fbx.normal_map.texture ? m->fbx.normal_map.texture : m->fbx.bump.texture;
  }
  return m->pbr.base_color.texture ? m->pbr.base_color.texture : m->fbx.diffuse_color.texture;
}

// A texture's picture: embedded, at its path, beside the FBX, or found by name in the folders around it.
inline bool LoadFbxTexture(const ufbx_texture *t, const fs::path &fbx_dir, svrfmt::Image &out) {
  if (!t)
    return false;
  if (t->content.size &&
      LoadPicture(Bytes(static_cast<const uint8_t *>(t->content.data),
                        static_cast<const uint8_t *>(t->content.data) + t->content.size),
                  out))
    return true;
  auto read = [&](const fs::path &p) {
    Bytes b;
    std::error_code ec;
    return fs::is_regular_file(p, ec) && svrfmt::ReadFile(p.string(), b) && LoadPicture(b, out);
  };
  std::vector<std::string> names;
  for (const ufbx_string *s : {&t->absolute_filename, &t->filename, &t->relative_filename})
    if (s->length)
      names.emplace_back(s->data, s->length);
  for (const auto &n : names) {
    const fs::path p = fs::u8path(n);
    for (const fs::path &c : {p, fbx_dir / p, fbx_dir / p.filename(), fbx_dir / "textures" / p.filename()})
      if (read(c))
        return true;
  }
  if (names.empty())
    return false;
  // (by its name in the folders around: Unity / Blender projects keep them in a Textures folder; another extension)
  const fs::path want = fs::u8path(names[0]).filename();
  const std::string stem = Lower(want.stem().string()), full = Lower(want.string());
  for (const fs::path &root : {fbx_dir, fbx_dir.parent_path()}) {
    std::error_code ec;
    int seen = 0;
    for (auto it = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::recursive_directory_iterator() && seen < 20000; it.increment(ec), ++seen) {
      if (!it->is_regular_file(ec))
        continue;
      const std::string f = Lower(it->path().filename().string());
      if ((f == full || Lower(it->path().stem().string()) == stem) && read(it->path()))
        return true;
    }
  }
  return false;
}

inline std::string GuessKind(const std::string &node_name, const std::string &mat_name) {
  const std::string l = Lower(node_name + " " + mat_name);
  auto has = [&](const char *w) { return l.find(w) != std::string::npos; };
  if (has("lash") || has("hair") || has("brow") || has("beard"))
    return "hair";
  if (has("eye") && !has("brow"))
    return "eye?";  // (an eye only when it's small)
  for (const char *w : {"pant", "short", "trunk", "boot", "shoe", "sock", "glove", "shirt", "jacket", "coat", "vest",
                        "pad", "belt", "tight", "cloth", "wrist", "tape", "mask", "hat", "cap", "strap", "jean",
                        "sleeve", "skirt", "top", "bra", "gear", "attire", "costume", "outfit", "elbow", "knee"})
    if (has(w))
      return "gear";
  return "skin";
}

}  // namespace detail

// The meshes of an FBX as the import sees them: one per mesh node and material ("node / material").
struct MeshInfo {
  std::string name, kind;
  int vertices = 0, triangles = 0;
  bool hidden = false;
};
inline bool ListMeshes(const fs::path &fbx, std::vector<MeshInfo> &out, std::string &err) {
  ufbx_load_opts lo = {};
  lo.target_axes = ufbx_axes_right_handed_y_up;
  lo.target_unit_meters = 0.1f;
  ufbx_error ue;
  const auto u8 = fbx.u8string();
  ufbx_scene *scene = ufbx_load_file(std::string(u8.begin(), u8.end()).c_str(), &lo, &ue);
  if (!scene)
    return err = "ufbx: " + detail::Str(ue.description), false;
  for (size_t ni = 0; ni < scene->nodes.count; ++ni) {
    const ufbx_node *node = scene->nodes.data[ni];
    const ufbx_mesh *mesh = node->mesh;
    if (!mesh || !mesh->num_faces)
      continue;
    const size_t parts = std::max<size_t>(1, mesh->material_parts.count);
    for (size_t pi = 0; pi < parts; ++pi) {
      const ufbx_mesh_part *part = mesh->material_parts.count ? &mesh->material_parts.data[pi] : nullptr;
      if (part && !part->num_triangles)
        continue;
      const ufbx_material *mat = part && pi < mesh->materials.count ? mesh->materials.data[pi] : nullptr;
      MeshInfo mi;
      const std::string mat_name = mat ? detail::Str(mat->name) : "";
      mi.name = detail::Str(node->name) + (mat_name.empty() ? "" : " / " + mat_name);
      mi.kind = detail::GuessKind(detail::Str(node->name), mat_name);
      mi.vertices = int(part ? part->num_faces * 3 : mesh->num_indices);
      mi.triangles = int(part ? part->num_triangles : mesh->num_triangles);
      mi.hidden = !node->visible;
      out.push_back(mi);
    }
  }
  ufbx_free_scene(scene);
  return true;
}

// The FBX's model into the base attire's EMD node (expanded or not): its model (entry 0), colour (12), normal map
// (10), Superstar Threads masks (13-15); its hair and eyelashes (10000, 20000) left out.
inline bool FbxToAttire(const fs::path &fbx, svrfmt::ArcNode &attire, const Options &opts, Report &rep,
                        std::string *why) {
  using namespace detail;
  auto fail = [&](const std::string &w) {
    if (why)
      *why = w;
    return false;
  };
  if (!svrfmt::ArchiveExpand(attire))
    return fail("the base attire can't be read");
  auto child = [&](uint32_t id) -> svrfmt::ArcNode * {
    for (auto &k : attire.kids)
      if (k.id == id)
        return &k;
    return nullptr;
  };
  svrfmt::ArcNode *model_node = child(0);
  svrfmt::Model base;
  std::string e;
  if (!model_node || !svrfmt::JboyRead(svrfmt::ArchiveData(*model_node), base, &e) || !base.w12 || base.meshes.empty())
    return fail("the base model can't be read " + e);

  // The template: the base's joints by SvR 2011 name, in its units (cm, y down, the hips at Bip).
  const auto wb = port_svr::World(base);
  std::map<std::string, int> base_node;
  for (size_t j = 0; j < base.nodes.size(); ++j)
    base_node.emplace(base.nodes[j].name, int(j));
  auto node_of = [&](const std::string &n2011) {
    for (std::string n = n2011; !n.empty(); n = Parent2011(n))
      if (auto it = base_node.find(port_svr::MapBone(n == "root" ? "root" : n)); it != base_node.end())
        return it->second;
    return 0;
  };
  auto tpl_pos = [&](const std::string &n, V3 &p) {
    const auto it = base_node.find(port_svr::MapBone(n));
    if (it == base_node.end())
      return false;
    const auto &w = wb[size_t(it->second)];
    p = {w.m[3], w.m[7], w.m[11]};
    return true;
  };
  V3 troot;
  if (!tpl_pos("root", troot))
    return fail("the base model has no hips bone (Bip)");
  double tfloor = -1e30;
  for (const auto &m : base.meshes)
    for (const auto &v : m.verts)
      tfloor = std::max(tfloor, double(v.pos[1]));

  ufbx_load_opts lo = {};
  lo.target_axes = ufbx_axes_right_handed_y_up;
  lo.target_unit_meters = 0.1f;
  lo.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
  ufbx_error ue;
  const auto u8 = fbx.u8string();
  ufbx_scene *scene = ufbx_load_file(std::string(u8.begin(), u8.end()).c_str(), &lo, &ue);
  if (!scene)
    return fail("the FBX can't be read (" + Str(ue.description) + ")");
  struct Free {
    ufbx_scene *s;
    ~Free() { ufbx_free_scene(s); }
  } free_scene{scene};

  // The FBX's bones: skeleton nodes and the ones a skin uses; their SvR 2011 names (rig_map.cpp).
  std::set<const ufbx_node *> skinned;
  for (size_t i = 0; i < scene->skin_clusters.count; ++i)
    if (scene->skin_clusters.data[i]->bone_node)
      skinned.insert(scene->skin_clusters.data[i]->bone_node);
  std::vector<const ufbx_node *> nodes;
  std::map<const ufbx_node *, int> index;
  for (size_t i = 0; i < scene->nodes.count; ++i) {
    const ufbx_node *n = scene->nodes.data[i];
    if (!n->bone && !skinned.count(n))
      continue;
    index[n] = int(nodes.size());
    nodes.push_back(n);
  }
  const size_t nb = nodes.size();
  if (!nb)
    return fail("the FBX has no skeleton (no bones, no skin)");
  std::vector<int> parent(nb, -1);
  for (size_t i = 0; i < nb; ++i)
    for (const ufbx_node *p = nodes[i]->parent; p; p = p->parent)
      if (const auto it = index.find(p); it != index.end()) {
        parent[i] = it->second;
        break;
      }
  std::vector<mm::RigBone> rig(nb);
  for (size_t i = 0; i < nb; ++i)
    rig[i] = {Str(nodes[i]->name), parent[i]};
  const std::vector<std::string> targets = mm::MapRig(rig);
  rep.rig = mm::DetectRig(rig);
  rep.bones = int(nb);
  // bind world matrices: a skin cluster's bind, else the parent's bind times the node's local
  std::vector<M> bind(nb);
  std::vector<bool> has_bind(nb);
  for (size_t i = 0; i < scene->skin_clusters.count; ++i) {
    const ufbx_skin_cluster *c = scene->skin_clusters.data[i];
    if (const auto it = index.find(c->bone_node); it != index.end() && !has_bind[size_t(it->second)])
      bind[size_t(it->second)] = FromUfbx(c->bind_to_world), has_bind[size_t(it->second)] = true;
  }
  std::function<void(size_t)> fill = [&](size_t i) {
    if (has_bind[i])
      return;
    has_bind[i] = true;
    if (parent[i] >= 0)
      fill(size_t(parent[i])), bind[i] = Mul(bind[size_t(parent[i])], FromUfbx(nodes[i]->node_to_parent));
    else
      bind[i] = FromUfbx(nodes[i]->node_to_world);
  };
  for (size_t i = 0; i < nb; ++i)
    fill(i);
  auto origin = [](const M &m) { return V3{m.t[0], m.t[1], m.t[2]}; };
  std::vector<int> depth(nb);
  for (size_t i = 0; i < nb; ++i)
    for (int p = parent[i]; p >= 0 && depth[i] <= int(nb); p = parent[size_t(p)])
      ++depth[i];
  std::map<std::string, int> src_of;  // SvR 2011 bone -> its FBX bone (the one with weights, else the topmost)
  for (size_t i = 0; i < nb; ++i)
    if (!targets[i].empty()) {
      auto [it, fresh] = src_of.emplace(targets[i], int(i));
      const bool w_new = skinned.count(nodes[i]) > 0, w_old = skinned.count(nodes[size_t(it->second)]) > 0;
      if (!fresh && (w_new != w_old ? w_new : depth[i] < depth[size_t(it->second)]))
        it->second = int(i);
    }
  rep.mapped = int(src_of.size());
  for (const char *need : {"root", "l_ninoude", "r_ninoude", "atama", "l_ashi", "r_ashi"})
    if (!src_of.count(need))
      return fail(std::string("its skeleton wasn't recognised (no ") + need + " bone; rig: " + rep.rig + ")");

  // Game axes: y up -> y down (180 degrees about X), then a turn about the up axis putting the character's left on +X.
  M axes;
  axes.r[1][1] = axes.r[2][2] = -1;
  {
    const V3 lat = ApplyDir(axes, Sub(origin(bind[size_t(src_of["l_ninoude"])]), origin(bind[size_t(src_of["r_ninoude"])])));
    const double l = std::hypot(lat[0], lat[2]);
    if (l > 1e-6) {
      const double c = lat[0] / l, s = lat[2] / l;
      M yaw;
      yaw.r[0][0] = c, yaw.r[0][2] = s, yaw.r[2][0] = -s, yaw.r[2][2] = c;
      axes = Mul(yaw, axes);
    }
  }
  const M to_src = InvRigid(axes);

  // The pose: each pair's FBX bone turned (with everything under it) to the base bone's direction.
  std::vector<M> posed = bind;
  std::vector<std::vector<int>> kids(nb);
  for (size_t i = 0; i < nb; ++i)
    if (parent[i] >= 0)
      kids[size_t(parent[i])].push_back(int(i));
  std::function<void(int, const M &)> turn = [&](int i, const M &rot) {
    posed[size_t(i)] = Mul(rot, posed[size_t(i)]);
    for (int k : kids[size_t(i)])
      turn(k, rot);
  };
  auto roll = [&](char side) {  // a hand's knuckle line (index -> pinky) as the base's, about the hand's axis
    const std::string s(1, side);
    const auto te = src_of.find(s + "_te"), mid = src_of.find(s + "_yubi20"), idx = src_of.find(s + "_yubi10"),
               pk = src_of.find(s + "_yubi40");
    V3 tidx, tpk;
    if (te == src_of.end() || mid == src_of.end() || idx == src_of.end() || pk == src_of.end() ||
        !tpl_pos(s + "_yubi10", tidx) || !tpl_pos(s + "_yubi40", tpk))
      return;
    const V3 p_te = origin(posed[size_t(te->second)]);
    const V3 axis = Norm(Sub(origin(posed[size_t(mid->second)]), p_te));
    auto flat = [&](V3 v) {
      const double d = Dot(v, axis);
      return Norm(V3{v[0] - d * axis[0], v[1] - d * axis[1], v[2] - d * axis[2]});
    };
    const V3 have = flat(Sub(origin(posed[size_t(pk->second)]), origin(posed[size_t(idx->second)])));
    const V3 want = flat(ApplyDir(to_src, Sub(tpk, tidx)));
    if (Len(have) < 0.5 || Len(want) < 0.5)
      return;
    M rot = AxisAngle(axis, std::atan2(Dot(Cross(have, want), axis), Dot(have, want)));
    const V3 r_p = ApplyDir(rot, p_te);
    for (int k = 0; k < 3; ++k)
      rot.t[k] = p_te[k] - r_p[k];
    turn(te->second, rot);
  };
  for (const auto &pr : kAimPairs) {
    const auto a = src_of.find(pr[0]), b = src_of.find(pr[1]);
    V3 ta, tb;
    if (a == src_of.end() || b == src_of.end() || !tpl_pos(pr[0], ta) || !tpl_pos(pr[1], tb))
      continue;
    const V3 pa = origin(posed[size_t(a->second)]), pb = origin(posed[size_t(b->second)]);
    if (Len(Sub(pb, pa)) < 1e-6)
      continue;
    M rot = Swing(Sub(pb, pa), ApplyDir(to_src, Sub(tb, ta)));
    const V3 r_pa = ApplyDir(rot, pa);
    for (int k = 0; k < 3; ++k)
      rot.t[k] = pa[k] - r_pa[k];
    turn(a->second, rot);
    if (std::string(pr[1]).ends_with("_yubi20"))
      roll(pr[0][0]);
  }
  const V3 hips = origin(posed[size_t(src_of["root"])]);
  auto game = [&](const V3 &p) { return ApplyDir(axes, Sub(p, hips)); };
  if (game(origin(posed[size_t(src_of["l_ashi"])]))[1] < 1 || game(origin(posed[size_t(src_of["r_ashi"])]))[1] < 1)
    rep.warnings.push_back("the ankles aren't below the hips: the model is upside down or the legs are mapped wrong");

  // The bones the weights name: the mapped ones, a folded bone's nearest mapped parent, or root.
  std::vector<std::string> wbone(nb);
  for (size_t i = 0; i < nb; ++i) {
    for (int p = int(i), n = 0; p >= 0 && n <= int(nb); p = parent[size_t(p)], ++n)
      if (!targets[size_t(p)].empty()) {
        wbone[i] = targets[size_t(p)];
        break;
      }
    if (wbone[i].empty())
      wbone[i] = "root";
  }
  std::vector<std::string> out_bones;
  std::map<std::string, int> out_index;
  auto bone_id = [&](const std::string &n) {
    auto [it, fresh] = out_index.emplace(n, int(out_bones.size()));
    if (fresh)
      out_bones.push_back(n);
    return it->second;
  };

  // Meshes: one per mesh node and material, posed by their weights (linear blend from the bind), in game axes.
  std::vector<M> skin(nb);
  for (size_t i = 0; i < nb; ++i)
    skin[i] = Mul(posed[i], InvAffine(bind[i]));
  const fs::path fbx_dir = fbx.parent_path();
  std::vector<OutMesh> meshes;
  std::map<std::string, const ufbx_material *> tex_material;
  std::vector<uint32_t> tri(64);
  for (size_t ni = 0; ni < scene->nodes.count; ++ni) {
    const ufbx_node *node = scene->nodes.data[ni];
    const ufbx_mesh *mesh = node->mesh;
    if (!mesh || !mesh->num_faces)
      continue;
    const ufbx_skin_deformer *sk = mesh->skin_deformers.count ? mesh->skin_deformers.data[0] : nullptr;
    int rigid = -1;  // a mesh without a skin rides on its nearest bone parent (or the hips)
    if (!sk)
      for (const ufbx_node *p = node->parent; p && rigid < 0; p = p->parent)
        if (const auto it = index.find(p); it != index.end())
          rigid = it->second;
    if (!sk && rigid < 0)
      rigid = src_of["root"];
    const M geo_world = FromUfbx(node->geometry_to_world);
    const size_t parts = std::max<size_t>(1, mesh->material_parts.count);
    for (size_t pi = 0; pi < parts; ++pi) {
      const ufbx_mesh_part *part = mesh->material_parts.count ? &mesh->material_parts.data[pi] : nullptr;
      if (part && !part->num_triangles)
        continue;
      const ufbx_material *mat = part && pi < mesh->materials.count ? mesh->materials.data[pi] : nullptr;
      OutMesh om;
      const std::string node_name = Str(node->name), mat_name = mat ? Str(mat->name) : "";
      om.name = node_name + (mat_name.empty() ? "" : " / " + mat_name);
      if (const auto on = opts.include.find(om.name); on != opts.include.end() ? !on->second : !node->visible)
        continue;
      om.kind = GuessKind(node_name, mat_name);
      if (const auto k = opts.kind.find(om.name); k != opts.kind.end() && !k->second.empty())
        om.kind = k->second;
      const ufbx_texture *ct = MaterialTexture(mat, false);
      om.texture_key = !ct ? "" : ct->filename.length ? Lower(Str(ct->filename)) : "embedded|" + std::to_string(ct->element_id);
      if (!om.texture_key.empty())
        tex_material.emplace(om.texture_key, mat);
      std::map<std::array<uint32_t, 6>, int> verts;  // (position index, normal and uv values) -> output vertex
      auto vertex = [&](uint32_t ix) {
        const uint32_t vi = mesh->vertex_indices.data[ix];
        const uint32_t nidx = mesh->vertex_normal.exists ? mesh->vertex_normal.indices.data[ix] : 0;
        const uint32_t uidx = mesh->vertex_uv.exists ? mesh->vertex_uv.indices.data[ix] : 0;
        auto bits = [](double v) {
          const float f = float(v);
          uint32_t u;
          std::memcpy(&u, &f, 4);
          return u;
        };
        std::array<uint32_t, 6> key{vi, 0, 0, 0, 0, 0};
        if (mesh->vertex_normal.exists) {
          const ufbx_vec3 qn = mesh->vertex_normal.values.data[nidx];
          key[1] = bits(qn.x), key[2] = bits(qn.y), key[3] = bits(qn.z);
        }
        if (mesh->vertex_uv.exists) {
          const ufbx_vec2 qu = mesh->vertex_uv.values.data[uidx];
          key[4] = bits(qu.x), key[5] = bits(qu.y);
        }
        if (const auto it = verts.find(key); it != verts.end())
          return it->second;
        const ufbx_vec3 gp = mesh->vertices.data[vi];
        const V3 g{gp.x, gp.y, gp.z};
        V3 n{0, 1, 0};
        if (mesh->vertex_normal.exists) {
          const ufbx_vec3 gn = mesh->vertex_normal.values.data[nidx];
          n = {gn.x, gn.y, gn.z};
        }
        V3 p{0, 0, 0}, nn{0, 0, 0};
        std::map<int, double> wout;
        double sum = 0;
        auto add = [&](int b, double w, const V3 &wp, const V3 &wn) {
          const V3 q = Apply(skin[size_t(b)], wp), qn = ApplyDir(skin[size_t(b)], wn);
          for (int k = 0; k < 3; ++k)
            p[k] += w * q[k], nn[k] += w * qn[k];
          wout[bone_id(wbone[size_t(b)])] += w;
          sum += w;
        };
        if (sk && vi < sk->vertices.count) {
          const ufbx_skin_vertex sv = sk->vertices.data[vi];
          for (uint32_t k = 0; k < sv.num_weights; ++k) {
            const ufbx_skin_weight sw = sk->weights.data[sv.weight_begin + k];
            const ufbx_skin_cluster *c = sk->clusters.data[sw.cluster_index];
            const auto it = index.find(c->bone_node);
            if (it == index.end() || sw.weight <= 0)
              continue;
            const M g2w = Mul(bind[size_t(it->second)], FromUfbx(c->geometry_to_bone));
            add(it->second, sw.weight, Apply(g2w, g), ApplyDir(g2w, n));
          }
        }
        if (sum <= 0)  // (no weights: rides on its node's bone)
          add(rigid >= 0 ? rigid : src_of["root"], 1.0, Apply(geo_world, g), ApplyDir(geo_world, n));
        for (int k = 0; k < 3; ++k)
          p[k] /= sum;
        om.pos.push_back(game(p));
        om.nrm.push_back(Norm(ApplyDir(axes, nn)));
        std::array<double, 2> uv{0, 0};
        if (mesh->vertex_uv.exists) {
          const ufbx_vec2 u = mesh->vertex_uv.values.data[uidx];
          uv = {u.x, 1.0 - u.y};  // (v down)
        }
        om.uv.push_back(uv);
        std::vector<std::pair<int, double>> wl;
        for (const auto &[b, w] : wout)
          wl.push_back({b, w / sum});
        om.w.push_back(std::move(wl));
        const int id = int(om.pos.size()) - 1;
        verts.emplace(key, id);
        return id;
      };
      auto faces = [&](uint32_t f) {
        const ufbx_face face = mesh->faces.data[f];
        if (face.num_indices < 3)
          return;
        if (tri.size() < size_t(face.num_indices) * 3)
          tri.resize(size_t(face.num_indices) * 3);
        const uint32_t n = ufbx_triangulate_face(tri.data(), tri.size(), mesh, face);
        for (uint32_t t = 0; t < n; ++t)
          om.tris.push_back({vertex(tri[t * 3]), vertex(tri[t * 3 + 1]), vertex(tri[t * 3 + 2])});
      };
      if (part)
        for (size_t k = 0; k < part->face_indices.count; ++k)
          faces(part->face_indices.data[k]);
      else
        for (uint32_t f = 0; f < mesh->num_faces; ++f)
          faces(f);
      if (om.tris.empty())
        continue;
      // The game's faces are turned over (its normals and winding point in: Kaiko's finding in SvR 2011, the same
      // engine): turned to it.
      for (auto &nv : om.nrm)
        nv = {-nv[0], -nv[1], -nv[2]};
      for (auto &t : om.tris)
        std::swap(t[1], t[2]);
      if (om.kind == "eye?") {  // (an eye fits in 8 cm; a whole model named "eye" doesn't)
        V3 lo3{1e30, 1e30, 1e30}, hi3{-1e30, -1e30, -1e30};
        for (const auto &p : om.pos)
          for (int k = 0; k < 3; ++k)
            lo3[k] = std::min(lo3[k], p[k]), hi3[k] = std::max(hi3[k], p[k]);
        om.kind = Len(Sub(hi3, lo3)) < 0.8 ? "eye" : "skin";
      }
      meshes.push_back(std::move(om));
    }
  }
  if (meshes.empty())
    return fail("the FBX has no visible meshes");

  // Scale: the soles as far below the hips as the base's; then the base's units and place (cm, the hips at Bip).
  double sole = -1e30;
  for (const auto &m : meshes)
    for (const auto &v : m.pos)
      sole = std::max(sole, v[1]);
  const double scale = sole > 0.5 && tfloor - troot[1] > 1 ? (tfloor - troot[1]) / sole : 10.0;
  rep.scale = scale;
  auto out_pos = [&](const V3 &g) { return V3{g[0] * scale + troot[0], g[1] * scale + troot[1], g[2] * scale + troot[2]}; };
  for (auto &m : meshes)
    for (auto &v : m.pos)
      v = out_pos(v);
  // Each vertex by its bones' joints, from the model's to the base's (its skeleton stays the game's).
  std::vector<V3> shift(out_bones.size(), V3{0, 0, 0});
  for (size_t b = 0; b < out_bones.size(); ++b) {
    // (a bone without its own joint: as the nearest one up its chain that has one)
    for (std::string n = out_bones[b]; !n.empty(); n = Parent2011(n)) {
      V3 tp;
      const auto s = src_of.find(n);
      if (s == src_of.end() || !tpl_pos(n, tp))
        continue;
      const V3 have = out_pos(game(origin(posed[size_t(s->second)])));
      shift[b] = Sub(tp, have);
      break;
    }
  }
  for (auto &m : meshes)
    for (size_t k = 0; k < m.pos.size(); ++k)
      for (const auto &[b, w] : m.w[k])
        for (int c = 0; c < 3; ++c)
          m.pos[k][c] += w * shift[size_t(b)][c];

  // Fewer vertices past the budget (eyes left as they are; the face kept as long as possible).
  {
    std::vector<std::vector<bool>> locks(meshes.size());
    if (opts.keep_face) {
      std::set<std::string> head{"atama", "d_kuchi", "l_eye", "r_eye"};
      for (size_t mi = 0; mi < meshes.size(); ++mi) {
        locks[mi].resize(meshes[mi].pos.size());
        for (size_t k = 0; k < meshes[mi].w.size(); ++k) {
          double hw = 0;
          for (const auto &[b, w] : meshes[mi].w[k])
            if (head.count(out_bones[size_t(b)]))
              hw += w;
          locks[mi][k] = hw >= 0.5;
        }
      }
    }
    int total = 0, eyes_n = 0;
    for (const auto &m : meshes)
      total += int(m.pos.size()), eyes_n += m.kind == "eye" ? int(m.pos.size()) : 0;
    if (opts.max_vertices > 0 && total > opts.max_vertices) {
      rep.reduced_from = total;
      for (int pass = 0; pass < 3 && total > opts.max_vertices; ++pass) {
        const double ratio = std::max(0.05, double(opts.max_vertices - eyes_n) / std::max(1, total - eyes_n));
        total = eyes_n;
        for (size_t mi = 0; mi < meshes.size(); ++mi)
          if (meshes[mi].kind != "eye")
            total += Decimate(meshes[mi], std::max(24, int(double(meshes[mi].pos.size()) * ratio)),
                              locks[mi].empty() ? nullptr : &locks[mi]);
      }
      if (total > opts.max_vertices)
        rep.warnings.push_back("still " + std::to_string(total) + " vertices after reducing the detail");
    }
  }

  // Pictures: every colour (and normal) map in one sheet the size of the base's colour picture, UVs moved into it.
  svrfmt::ArcNode *col_node = child(12), *nrm_node = child(10);
  std::vector<svrfmt::BundleTexture> col_bundle, nrm_bundle;
  if (!col_node || !svrfmt::BundleRead(svrfmt::ArchiveData(*col_node), col_bundle) || col_bundle.empty())
    return fail("the base attire has no colour picture");
  svrfmt::DdsInfo col_info, nrm_info;
  if (!svrfmt::DdsInfoOf(col_bundle[0].data, col_info))
    return fail("the base's colour picture can't be read");
  const int AW = col_info.w, AH = col_info.h;
  struct Sheet {
    svrfmt::Image color, normal;
    bool has_normal = false;
    int x = 0, y = 0, w = 0, h = 0;
  };
  std::map<std::string, Sheet> sheets;
  for (const auto &[key, mat] : tex_material) {
    Sheet s;
    if (!LoadFbxTexture(MaterialTexture(mat, false), fbx_dir, s.color)) {
      rep.warnings.push_back("texture not found: " + key + " (plain grey)");
      continue;
    }
    s.has_normal = LoadFbxTexture(MaterialTexture(mat, true), fbx_dir, s.normal);
    ++rep.textures;
    sheets.emplace(key, std::move(s));
  }
  bool any_plain = false;
  for (const auto &m : meshes)
    any_plain |= !sheets.count(m.texture_key);
  if (any_plain) {
    Sheet s;
    s.color.w = s.color.h = 32;
    s.color.rgba.assign(32 * 32 * 4, 160);
    sheets.emplace("", std::move(s));
    for (auto &m : meshes)
      if (!sheets.count(m.texture_key))
        m.texture_key = "";
  }
  {
    double area = 0;
    for (const auto &[k, s] : sheets)
      area += double(s.color.w) * s.color.h;
    double shrink = std::min(1.0, std::sqrt(double(AW) * AH * 0.85 / std::max(1.0, area)));
    bool fits = false;
    for (int tries = 0; tries < 60 && !fits; ++tries, shrink *= 0.93) {
      std::vector<std::pair<int, Sheet *>> order;
      for (auto &[k, s] : sheets) {
        s.w = std::max(8, int(s.color.w * shrink)), s.h = std::max(8, int(s.color.h * shrink));
        order.push_back({s.h, &s});
      }
      std::sort(order.begin(), order.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
      int x = 0, y = 0, row = 0;
      fits = true;
      for (auto &[h, s] : order) {
        const int pw = s->w + 8, ph = s->h + 8;  // (4 px of edge around each: no bleeding)
        if (x + pw > AW)
          x = 0, y += row, row = 0;
        if (pw > AW || y + ph > AH) {
          fits = false;
          break;
        }
        s->x = x + 4, s->y = y + 4, x += pw, row = std::max(row, ph);
      }
    }
    if (!fits)
      return fail("the textures don't fit the colour picture");
  }
  svrfmt::Image color;
  color.w = AW, color.h = AH;
  color.rgba.assign(size_t(AW) * AH * 4, 255);
  std::vector<svrfmt::BundleTexture>::iterator nrm_tex = nrm_bundle.end();
  if (nrm_node && svrfmt::BundleRead(svrfmt::ArchiveData(*nrm_node), nrm_bundle))
    nrm_tex = std::find_if(nrm_bundle.begin(), nrm_bundle.end(), [](const auto &t) { return t.name == "normal"; });
  svrfmt::Image normal;
  uint8_t shine = 16;  // (the base's: how much the skin shines, in the normal map's alpha)
  if (nrm_tex != nrm_bundle.end() && svrfmt::DdsInfoOf(nrm_tex->data, nrm_info) && svrfmt::DdsDecode(nrm_tex->data, normal)) {
    double t = 0;
    for (size_t q = 3; q < normal.rgba.size(); q += 4)
      t += normal.rgba[q];
    shine = uint8_t(t / std::max<size_t>(1, normal.rgba.size() / 4));
    for (size_t q = 0; q < normal.rgba.size(); q += 4)
      normal.rgba[q] = 128, normal.rgba[q + 1] = 128, normal.rgba[q + 2] = 255, normal.rgba[q + 3] = shine;
  }
  for (auto &[key, s] : sheets) {
    const svrfmt::Image c = svrfmt::Resize(s.color, s.w, s.h);
    for (int y = -4; y < s.h + 4; ++y)
      for (int x = -4; x < s.w + 4; ++x) {
        const int sx = std::clamp(x, 0, s.w - 1), sy = std::clamp(y, 0, s.h - 1);
        const size_t d = (size_t(s.y + y) * AW + size_t(s.x + x)) * 4, src = (size_t(sy) * s.w + size_t(sx)) * 4;
        for (int ch = 0; ch < 3; ++ch)
          color.rgba[d + size_t(ch)] = c.rgba[src + size_t(ch)];
      }
    if (s.has_normal && normal.w > 0) {  // (in the normal map, at its own size)
      const double kx = double(normal.w) / AW, ky = double(normal.h) / AH;
      const int nx = int(s.x * kx), ny = int(s.y * ky), nw = std::max(1, int(s.w * kx)), nh = std::max(1, int(s.h * ky));
      const svrfmt::Image n = svrfmt::Resize(s.normal, nw, nh);
      for (int y = 0; y < nh && ny + y < normal.h; ++y)
        for (int x = 0; x < nw && nx + x < normal.w; ++x) {
          const size_t d = (size_t(ny + y) * normal.w + size_t(nx + x)) * 4, src = (size_t(y) * nw + size_t(x)) * 4;
          for (int ch = 0; ch < 3; ++ch)
            normal.rgba[d + size_t(ch)] = n.rgba[src + size_t(ch)];
        }
    }
  }
  int wrapped = 0;
  for (auto &m : meshes) {
    const Sheet &s = sheets[m.texture_key];
    for (auto &uv : m.uv) {
      double u = uv[0], v = uv[1];
      if (u < -0.001 || u > 1.001 || v < -0.001 || v > 1.001)
        ++wrapped, u -= std::floor(u), v -= std::floor(v);
      uv = {(s.x + u * s.w) / AW, (s.y + v * s.h) / AH};
    }
  }
  if (wrapped)
    rep.warnings.push_back(std::to_string(wrapped) + " vertices had UVs outside the picture (tiled): wrapped");

  // The model: the base's skeleton and parts, the FBX's meshes on base materials.
  int skin_tm = -1, gear_tm = -1, eye_tm = -1;
  std::vector<int> base_part;
  for (size_t i = 0; i < base.parts.size(); ++i)
    for (uint32_t k = 0; k < base.parts[i].meshes; ++k)
      base_part.push_back(int(i));
  base_part.resize(base.meshes.size(), -1);
  auto has_param = [](const svrfmt::Mesh &m, const char *n) {
    return std::any_of(m.params.begin(), m.params.end(), [&](const auto &p) { return p.name == n; });
  };
  for (size_t i = 0; i < base.meshes.size(); ++i) {
    const auto &m = base.meshes[i];
    auto bigger = [&](int cur) { return cur < 0 || m.verts.size() > base.meshes[size_t(cur)].verts.size(); };
    if (m.shader == "yCh_skin" && !has_param(m, "g_mMusclePrm") && !has_param(m, "g_mNormalWklPrm") && bigger(skin_tm))
      skin_tm = int(i);
    if (m.shader == "yCh_gear" && bigger(gear_tm))
      gear_tm = int(i);
    if (m.shader == "yCh_Reflect" && bigger(eye_tm))
      eye_tm = int(i);
  }
  if (skin_tm < 0)  // (any skin)
    for (size_t i = 0; i < base.meshes.size() && skin_tm < 0; ++i)
      if (base.meshes[i].shader.rfind("yCh_skin", 0) == 0)
        skin_tm = int(i);
  if (skin_tm < 0)
    skin_tm = 0;
  if (gear_tm < 0)
    gear_tm = skin_tm;
  if (eye_tm < 0)
    eye_tm = skin_tm;
  auto part_named = [&](int tm, const char *fallback) {
    const int p = base_part[size_t(tm)];
    return p >= 0 ? base.parts[size_t(p)].name : std::string(fallback);
  };
  std::vector<std::string> part_names;
  auto part_index = [&](const std::string &n) {
    for (size_t i = 0; i < part_names.size(); ++i)
      if (part_names[i] == n)
        return int(i);
    part_names.push_back(n);
    return int(part_names.size() - 1);
  };
  // The bones as the base's nodes.
  std::vector<int> bone_node(out_bones.size());
  for (size_t b = 0; b < out_bones.size(); ++b)
    bone_node[b] = node_of(out_bones[b]);

  svrfmt::Model out = base;
  out.meshes.clear();
  std::vector<std::pair<int, svrfmt::Mesh>> built;  // (part, mesh)
  for (const auto &m : meshes) {
    const int tm = m.kind == "eye" ? eye_tm : m.kind == "gear" ? gear_tm : skin_tm;
    const int part = part_index(part_named(tm, "body"));
    // Each vertex: its 4 strongest bones (as base nodes), renormalised.
    std::vector<std::vector<std::pair<int, double>>> vw(m.pos.size());
    for (size_t k = 0; k < m.pos.size(); ++k) {
      std::map<int, double> by_node;
      for (const auto &[b, w] : m.w[k])
        by_node[bone_node[size_t(b)]] += w;
      std::vector<std::pair<int, double>> l(by_node.begin(), by_node.end());
      std::sort(l.begin(), l.end(), [](const auto &a, const auto &b) { return a.second > b.second; });
      if (l.size() > 4)
        l.resize(4);
      double s = 0;
      for (const auto &x : l)
        s += x.second;
      for (auto &x : l)
        x.second = s > 0 ? x.second / s : 1.0 / l.size();
      if (l.empty())
        l.push_back({bone_node[0], 1.0});
      vw[k] = std::move(l);
    }
    // Split: at most 20 bones (the palette) and 32,767 vertices a mesh.
    std::vector<bool> done(m.tris.size());
    for (size_t first = 0; first < m.tris.size();) {
      std::vector<int> palette;
      std::map<int, int> remap;
      svrfmt::Mesh dst;
      const svrfmt::Mesh &t = base.meshes[size_t(tm)];
      dst.raw = t.raw, dst.shader = t.shader, dst.vfmt = t.vfmt, dst.material = t.material, dst.params = t.params;
      std::vector<std::array<uint16_t, 3>> tris;
      for (size_t ti = first; ti < m.tris.size(); ++ti) {
        if (done[ti])
          continue;
        std::set<int> need;
        for (int c : m.tris[ti])
          for (const auto &[n, w] : vw[size_t(c)])
            if (std::find(palette.begin(), palette.end(), n) == palette.end())
              need.insert(n);
        if (palette.size() + need.size() > 20 || remap.size() + 3 > 32767)
          continue;
        palette.insert(palette.end(), need.begin(), need.end());
        std::array<uint16_t, 3> tr{};
        for (int c = 0; c < 3; ++c) {
          auto [it, fresh] = remap.emplace(m.tris[ti][size_t(c)], int(remap.size()));
          tr[size_t(c)] = uint16_t(it->second);
        }
        tris.push_back(tr);
        done[ti] = true;
      }
      while (first < m.tris.size() && done[first])
        ++first;
      if (tris.empty())
        break;
      const size_t nv = remap.size();
      dst.verts.resize(nv);
      dst.uvs.resize(nv);
      std::vector<int> order(nv);
      for (const auto &[src, dsti] : remap)
        order[size_t(dsti)] = src;
      size_t influences = 1;
      for (int src : order)
        influences = std::max(influences, vw[size_t(src)].size());
      dst.weights.assign(influences, std::vector<svrfmt::Weight>(nv));
      for (size_t k = 0; k < nv; ++k) {
        const int src = order[k];
        auto &v = dst.verts[k];
        for (int c = 0; c < 3; ++c)
          v.pos[c] = float(m.pos[size_t(src)][c]), v.normal[c] = float(m.nrm[size_t(src)][c]);
        v.color = 0xFFFFFFFFu;
        dst.uvs[k] = {float(m.uv[size_t(src)][0]), float(m.uv[size_t(src)][1])};
        const auto &l = vw[size_t(src)];
        for (size_t j = 0; j < influences; ++j) {
          // (an unused influence: the first bone with no weight - never a slot past the palette)
          const int n = j < l.size() ? l[j].first : l[0].first;
          const int slot = int(std::find(palette.begin(), palette.end(), n) - palette.begin());
          dst.weights[j][k] = {{uint8_t(slot), 0, 0, 0}, j < l.size() ? float(l[j].second) : 0.0f};
        }
      }
      for (int n : palette)
        dst.palette.push_back(n + 1);
      svrfmt::Strip st;
      st.indices = svrfmt::TrianglesToStrip(tris);
      dst.strips.push_back(std::move(st));
      double lo3[3] = {1e30, 1e30, 1e30}, hi3[3] = {-1e30, -1e30, -1e30};
      for (const auto &v : dst.verts)
        for (int r = 0; r < 3; ++r)
          lo3[r] = std::min(lo3[r], double(v.pos[r])), hi3[r] = std::max(hi3[r], double(v.pos[r]));
      double rad = 0;
      for (const auto &v : dst.verts) {
        double d = 0;
        for (int r = 0; r < 3; ++r)
          d += (v.pos[r] - (lo3[r] + hi3[r]) / 2) * (v.pos[r] - (lo3[r] + hi3[r]) / 2);
        rad = std::max(rad, d);
      }
      for (int r = 0; r < 3; ++r)
        dst.sphere[size_t(r)] = float((lo3[r] + hi3[r]) / 2);
      dst.sphere[3] = float(std::sqrt(rad));
      port_svr::detail::Tangents(dst);
      rep.vertices += int(nv), rep.triangles += int(tris.size());
      built.push_back({part, std::move(dst)});
    }
    rep.lines.push_back(m.name + " (" + m.kind + "): " + std::to_string(m.pos.size()) + " vertices");
  }
  // Parts in order, their meshes after one another.
  out.parts.clear();
  for (size_t p = 0; p < part_names.size(); ++p) {
    svrfmt::Part q;
    q.name = part_names[p];
    q.info = base.parts.empty() ? Bytes(16, 0) : base.parts[0].info;
    for (const auto &bp : base.parts)
      if (bp.name == q.name)
        q.info = bp.info;
    for (auto &[part, mesh] : built)
      if (part == int(p)) {
        if (mesh.raw.size() >= 0x64)
          svrfmt::PutBe32(&mesh.raw[0x60], uint32_t(p));
        out.meshes.push_back(mesh);
        ++q.meshes;
      }
    out.parts.push_back(std::move(q));
  }
  rep.meshes = int(out.meshes.size());
  out.w12 = true;
  svrfmt::ArchiveReplace(*model_node, svrfmt::JboyWrite(out));

  // The pictures.
  col_bundle[0].data = svrfmt::DdsEncode(color, col_info.format, col_info.mips > 1);
  svrfmt::ArchiveReplace(*col_node, svrfmt::BundleWrite(col_bundle));
  if (nrm_tex != nrm_bundle.end() && normal.w > 0) {
    nrm_tex->data = svrfmt::DdsEncode(normal, nrm_info.format, nrm_info.mips > 1);
    svrfmt::ArchiveReplace(*nrm_node, svrfmt::BundleWrite(nrm_bundle));
  }
  for (uint32_t id : {13u, 14u}) {  // Superstar Threads masks: blank (no zone to colour)
    svrfmt::ArcNode *k = child(id);
    std::vector<svrfmt::BundleTexture> b;
    svrfmt::DdsInfo i;
    if (!k || !svrfmt::BundleRead(svrfmt::ArchiveData(*k), b) || b.empty() || !svrfmt::DdsInfoOf(b[0].data, i))
      continue;
    svrfmt::Image blank;
    blank.w = i.w, blank.h = i.h;
    blank.rgba.assign(size_t(i.w) * i.h * 4, 0);
    b[0].data = svrfmt::DdsEncode(blank, i.format, i.mips > 1);
    svrfmt::ArchiveReplace(*k, svrfmt::BundleWrite(b));
  }
  for (uint32_t id : {15u, 10000u, 20000u})
    for (size_t i = 0; i < attire.kids.size(); ++i)
      if (attire.kids[i].id == id) {
        svrfmt::ArchiveRemove(attire, i);
        break;
      }
  svrfmt::ArchiveTouch(attire);
  return true;
}

}  // namespace fbx_star
