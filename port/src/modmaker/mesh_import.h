// 3D objects for the arenas' 3D editor, after the arena objects of Kaiko's SvR 2011 Mod Maker
// (svrfmt/model_objects.cpp: add an object from an OBJ / FBX file, export one as OBJ + MTL + PNG; credit in
// docs/mod_maker.md). A file's triangles as game space (cm, y down: a y-up file turned half a turn about x, which
// keeps its winding), with its first diffuse picture; and an arena model written back as OBJ.
#pragma once

#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "svrfmt/jboy.h"
#include "third_party/ufbx/ufbx.h"

namespace mesh_import {

namespace fs = std::filesystem;

struct TriMesh {
  std::vector<std::array<float, 3>> pos, normal;
  std::vector<std::array<float, 2>> uv;
  std::vector<uint32_t> tris;  // 3 per triangle
  fs::path texture;            // its first diffuse picture, if the file names one that exists
};

// y-up (metres x `cm_per_unit`) to game space.
inline std::array<float, 3> ToGame(const float *p, float k) { return {p[0] * k, -p[1] * k, -p[2] * k}; }

inline bool LoadObj(const fs::path &file, TriMesh &out, std::string &why) {
  std::ifstream f(file);
  if (!f)
    return why = "can't open", false;
  std::vector<std::array<float, 3>> v, vn;
  std::vector<std::array<float, 2>> vt;
  std::map<std::tuple<int, int, int>, uint32_t> seen;
  fs::path mtl;
  std::string line;
  auto index = [](const std::string &s, size_t n) {
    const int i = s.empty() ? 0 : atoi(s.c_str());
    return i < 0 ? int(n) + i : i - 1;
  };
  while (std::getline(f, line)) {
    std::istringstream ss(line);
    std::string tag;
    ss >> tag;
    if (tag == "v") {
      std::array<float, 3> p{};
      ss >> p[0] >> p[1] >> p[2];
      v.push_back(p);
    } else if (tag == "vt") {
      std::array<float, 2> t{};
      ss >> t[0] >> t[1];
      vt.push_back({t[0], 1.0f - t[1]});
    } else if (tag == "vn") {
      std::array<float, 3> n{};
      ss >> n[0] >> n[1] >> n[2];
      vn.push_back(n);
    } else if (tag == "mtllib" && mtl.empty()) {
      std::string rest;
      std::getline(ss >> std::ws, rest);
      mtl = file.parent_path() / fs::u8path(rest);
    } else if (tag == "f") {
      std::vector<uint32_t> face;
      std::string c;
      while (ss >> c) {
        std::string a, b, n;
        const size_t s1 = c.find('/'), s2 = s1 == std::string::npos ? s1 : c.find('/', s1 + 1);
        a = c.substr(0, s1);
        if (s1 != std::string::npos)
          b = c.substr(s1 + 1, s2 == std::string::npos ? std::string::npos : s2 - s1 - 1);
        if (s2 != std::string::npos)
          n = c.substr(s2 + 1);
        const auto key = std::make_tuple(index(a, v.size()), b.empty() ? -1 : index(b, vt.size()),
                                         n.empty() ? -1 : index(n, vn.size()));
        auto it = seen.find(key);
        if (it == seen.end()) {
          const auto [pi, ti, ni] = key;
          if (pi < 0 || size_t(pi) >= v.size())
            return why = "a face uses a vertex the file hasn't", false;
          out.pos.push_back(ToGame(v[size_t(pi)].data(), 100.0f));
          out.uv.push_back(ti >= 0 && size_t(ti) < vt.size() ? vt[size_t(ti)] : std::array<float, 2>{0, 0});
          out.normal.push_back(ni >= 0 && size_t(ni) < vn.size() ? ToGame(vn[size_t(ni)].data(), 1.0f)
                                                                 : std::array<float, 3>{0, -1, 0});
          it = seen.emplace(key, uint32_t(out.pos.size() - 1)).first;
        }
        face.push_back(it->second);
      }
      for (size_t i = 2; i < face.size(); ++i)
        out.tris.insert(out.tris.end(), {face[0], face[i - 1], face[i]});
    }
  }
  // The first map_Kd of its MTL file.
  std::ifstream m(mtl);
  while (m && std::getline(m, line)) {
    std::istringstream ss(line);
    std::string tag;
    ss >> tag;
    if (tag == "map_Kd") {
      std::string rest;
      std::getline(ss >> std::ws, rest);
      const size_t sp = rest.rfind(' ');  // (options before the name: the name is the last word when it isn't found)
      fs::path p = file.parent_path() / fs::u8path(rest);
      if (!fs::exists(p) && sp != std::string::npos)
        p = file.parent_path() / fs::u8path(rest.substr(sp + 1));
      if (fs::exists(p))
        out.texture = p;
      break;
    }
  }
  if (out.tris.empty())
    return why = "no faces", false;
  return true;
}

inline bool LoadFbx(const fs::path &file, TriMesh &out, std::string &why) {
  ufbx_load_opts lo = {};
  lo.target_axes = ufbx_axes_right_handed_y_up;
  lo.target_unit_meters = 0.01f;  // (cm)
  lo.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
  ufbx_error ue;
  const auto u8 = file.u8string();
  ufbx_scene *scene = ufbx_load_file(std::string(u8.begin(), u8.end()).c_str(), &lo, &ue);
  if (!scene)
    return why = std::string("the FBX can't be read (") + ue.description.data + ")", false;
  std::vector<uint32_t> tri;
  for (size_t ni = 0; ni < scene->nodes.count; ++ni) {
    const ufbx_node *node = scene->nodes.data[ni];
    const ufbx_mesh *mesh = node->mesh;
    if (!mesh)
      continue;
    tri.resize(mesh->max_face_triangles * 3);
    for (size_t fi = 0; fi < mesh->faces.count; ++fi) {
      const uint32_t n = ufbx_triangulate_face(tri.data(), tri.size(), mesh, mesh->faces.data[fi]);
      for (uint32_t t = 0; t < n * 3; ++t) {
        const uint32_t ix = tri[t];
        const ufbx_vec3 p = ufbx_transform_position(&node->geometry_to_world, ufbx_get_vertex_vec3(&mesh->vertex_position, ix));
        ufbx_vec3 nm = mesh->vertex_normal.exists ? ufbx_get_vertex_vec3(&mesh->vertex_normal, ix) : ufbx_vec3{0, 1, 0};
        nm = ufbx_transform_direction(&node->geometry_to_world, nm);
        const float pf[3] = {float(p.x), float(p.y), float(p.z)};
        const double len = std::sqrt(nm.x * nm.x + nm.y * nm.y + nm.z * nm.z);
        const float nf[3] = {float(nm.x / (len > 0 ? len : 1)), float(nm.y / (len > 0 ? len : 1)), float(nm.z / (len > 0 ? len : 1))};
        out.pos.push_back(ToGame(pf, 1.0f));
        out.normal.push_back(ToGame(nf, 1.0f));
        if (mesh->vertex_uv.exists) {
          const ufbx_vec2 uv = ufbx_get_vertex_vec2(&mesh->vertex_uv, ix);
          out.uv.push_back({float(uv.x), 1.0f - float(uv.y)});
        } else {
          out.uv.push_back({0, 0});
        }
        out.tris.push_back(uint32_t(out.pos.size() - 1));
      }
    }
    if (out.texture.empty())
      for (size_t mi = 0; mi < mesh->materials.count && out.texture.empty(); ++mi) {
        const ufbx_material *mat = mesh->materials.data[mi];
        const ufbx_texture *tex = mat->pbr.base_color.texture ? mat->pbr.base_color.texture : mat->fbx.diffuse_color.texture;
        if (!tex)
          continue;
        for (const ufbx_string &s : {tex->filename, tex->absolute_filename, tex->relative_filename}) {
          if (!s.length)
            continue;
          fs::path p = fs::u8path(std::string(s.data, s.length));
          if (p.is_relative())
            p = file.parent_path() / p;
          if (!fs::exists(p))
            p = file.parent_path() / p.filename();
          if (fs::exists(p)) {
            out.texture = p;
            break;
          }
        }
      }
  }
  ufbx_free_scene(scene);
  if (out.tris.empty())
    return why = "no triangles", false;
  return true;
}

inline bool Load(const fs::path &file, TriMesh &out, std::string &why) {
  std::string ext = file.extension().string();
  for (auto &c : ext)
    c = char(std::tolower(static_cast<unsigned char>(c)));
  return ext == ".fbx" ? LoadFbx(file, out, why) : LoadObj(file, out, why);
}

// The triangles of a strip list (strips: every other triangle turned; repeated indices are the joins, skipped).
inline std::vector<uint32_t> Triangles(const svrfmt::Mesh &m) {
  std::vector<uint32_t> out;
  for (const auto &s : m.strips) {
    const auto &ix = s.indices;
    if (!s.IsStrip()) {
      for (size_t i = 0; i + 2 < ix.size(); i += 3)
        out.insert(out.end(), {ix[i], ix[i + 1], ix[i + 2]});
      continue;
    }
    for (size_t i = 0; i + 2 < ix.size(); ++i) {
      const uint32_t a = ix[i], b = ix[i + 1], c = ix[i + 2];
      if (a == b || b == c || a == c)
        continue;
      if (i % 2)
        out.insert(out.end(), {b, a, c});
      else
        out.insert(out.end(), {a, b, c});
    }
  }
  return out;
}

// A model written as OBJ (metres, y up) with its MTL: `picture(mesh)` names each mesh's picture file ("" = none).
template <class Picture>
inline bool SaveObj(const fs::path &file, const svrfmt::Model &m, Picture picture) {
  std::ofstream o(file), mtl(fs::path(file).replace_extension(".mtl"));
  if (!o || !mtl)
    return false;
  o << "# " << m.name << ", exported by the WWE '12 Mod Maker\nmtllib "
    << fs::path(file).replace_extension(".mtl").filename().string() << "\n";
  size_t base = 1;
  for (size_t i = 0; i < m.meshes.size(); ++i) {
    const auto &me = m.meshes[i];
    for (const auto &v : me.verts)
      o << "v " << v.pos[0] / 100 << ' ' << -v.pos[1] / 100 << ' ' << -v.pos[2] / 100 << "\n";
    for (size_t k = 0; k < me.verts.size(); ++k) {
      const auto uv = k < me.uvs.size() ? me.uvs[k] : std::array<float, 2>{0, 0};
      o << "vt " << uv[0] << ' ' << 1.0f - uv[1] << "\n";
    }
    for (const auto &v : me.verts)
      o << "vn " << v.normal[0] << ' ' << -v.normal[1] << ' ' << -v.normal[2] << "\n";
    const std::string pic = picture(me);
    o << "g mesh" << i << "\nusemtl m" << i << "\n";
    mtl << "newmtl m" << i << "\nKd 1 1 1\n";
    if (!pic.empty())
      mtl << "map_Kd " << pic << "\n";
    const auto t = Triangles(me);
    for (size_t k = 0; k + 2 < t.size(); k += 3) {
      o << 'f';
      for (int c = 0; c < 3; ++c) {
        const size_t n = base + t[k + size_t(c)];
        o << ' ' << n << '/' << n << '/' << n;
      }
      o << "\n";
    }
    base += me.verts.size();
  }
  return bool(o);
}

}  // namespace mesh_import
