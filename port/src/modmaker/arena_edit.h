// An arena file (pac/bg/bgNN.pac) opened for its textures: EPAC -> "STG " entry -> PACH whose texture packages
// (ids 400-402, 32010...: {count, ..., records {name[16], "dds", size, offset, 0}}, Kaiko's BundleRead / BundleWrite)
// hold the arena's pictures as PC DDS. Pictures are replaced in place; the file is rebuilt with Kaiko's archive
// writer, the changed packages packed again keeping Yuke's own blocks (bpe_like.h).
// The game loads an arena into fixed memory sized for the arena it replaces: the file must never be bigger than
// the shipped bgNN.pac of its slot (docs/limit_breaking.md, "Arènes": 10 KB over already crashed). Fit() makes room
// by dropping the top level of the biggest textures nobody changed (their smaller levels are kept as they are:
// nothing is encoded again, the picture is only half as sharp).
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <map>
#include <set>
#include <cmath>
#include <string>
#include <vector>

#include "bpe_like.h"
#include "svrfmt/archive.h"
#include "svrfmt/jboy.h"
#include "svrfmt/texture.h"

namespace arena_edit {

using svrfmt::Bytes;

// A DDS without its top mip level (half the size, the same smaller levels); false if it has only one level.
inline bool DropTopMip(Bytes &dds) {
  svrfmt::DdsInfo info;
  if (!svrfmt::DdsInfoOf(dds, info) || info.mips < 2 || dds.size() < 128)
    return false;
  const size_t bw = std::max(1, (info.w + 3) / 4), bh = std::max(1, (info.h + 3) / 4);
  size_t top = 0;
  switch (info.format) {
  case svrfmt::DxtFormat::kDxt1: top = bw * bh * 8; break;
  case svrfmt::DxtFormat::kDxt3:
  case svrfmt::DxtFormat::kDxt5: top = bw * bh * 16; break;
  case svrfmt::DxtFormat::kArgb: top = size_t(info.w) * info.h * 4; break;
  case svrfmt::DxtFormat::kArgb4444: top = size_t(info.w) * info.h * 2; break;
  default: return false;
  }
  if (128 + top >= dds.size())
    return false;
  Bytes out(dds.begin(), dds.begin() + 128);
  const uint32_t w = std::max(1, info.w / 2), h = std::max(1, info.h / 2);
  svrfmt::PutLe32(&out[12], h);
  svrfmt::PutLe32(&out[16], w);
  const uint32_t flags = svrfmt::Le32(&out[8]);
  const size_t nbw = std::max<uint32_t>(1, (w + 3) / 4), nbh = std::max<uint32_t>(1, (h + 3) / 4);
  const size_t next = info.format == svrfmt::DxtFormat::kArgb      ? size_t(w) * 4
                      : info.format == svrfmt::DxtFormat::kArgb4444 ? size_t(w) * 2
                      : info.format == svrfmt::DxtFormat::kDxt1    ? nbw * nbh * 8
                                                                   : nbw * nbh * 16;
  svrfmt::PutLe32(&out[20], uint32_t(next));  // pitch (ARGB) or linear size (DXT), as the flags say
  (void)flags;
  svrfmt::PutLe32(&out[28], uint32_t(info.mips - 1));
  out.insert(out.end(), dds.begin() + ptrdiff_t(128 + top), dds.end());
  dds = std::move(out);
  return true;
}

struct Texture {
  size_t package = 0;  // index in Arena::packages
  size_t index = 0;    // in its package
  bool replaced = false, reduced = false;
};

// An object of the arena moved (its vertices, cm), turned about the up axis (degrees) or hidden (sent far
// under the floor: removing it could leave the game looking for it), after the Arena Editor of Kaiko's SvR 2011 Mod
// Maker (editor.cpp; credit in docs/mod_maker.md).
struct Move {
  float d[3] = {0, 0, 0};
  float turn = 0;
  bool hide = false;
  bool None() const { return !d[0] && !d[1] && !d[2] && !turn && !hide; }
};

// A superstar's material (Kaiko's Mod Maker has them): the shine and tint of a group of its body model's meshes (child
// 0 of the attire): "skin" (yCh_skin*, not the teeth) or "gear:<part>" (yCh_gear, by the model's part: body, boots,
// shorts...). Shine multiplies g_fSpecLev and g_fSpecLvBase; tint multiplies g_f4MatDifCol's colour.
struct Material {
  float shine = 1.0f;
  float tint[3] = {1.0f, 1.0f, 1.0f};
  bool Default() const { return shine == 1.0f && tint[0] == 1.0f && tint[1] == 1.0f && tint[2] == 1.0f; }
};

struct Package {
  svrfmt::ArcNode *node = nullptr;
  uint32_t id = 0;  // its child id (node found again when the list's children move: an object added or removed)
  std::vector<svrfmt::BundleTexture> textures, original;
  Bytes shipped;  // the package as stored in the shipped file (the blocks EncodeLike keeps)
  bool changed = false;
};

class Arena {
 public:
  // An arena (its "STG " section), or with `section` >= 0 that entry of any pac whose entries are PACH lists
  // (a superstar's chNNN.pac: one "EMD " entry per attire).
  bool Load(Bytes file, std::string *error, int section = -1) {
    packages_.clear();
    textures_.clear();
    originals_.clear();
    moves_.clear();
    copies_.clear();
    added_pictures_.clear();
    flags_text_.clear();
    moves_changed_ = false;
    materials_.clear();
    materials_changed_ = false;
    body_.clear();
    archive_ = svrfmt::Archive();
    if (!svrfmt::ArchiveOpen(file, archive_, error))
      return false;
    section_ = section < 0 ? 0 : size_t(section);
    if (archive_.root.kind != svrfmt::ArcKind::Epac || archive_.root.kids.size() <= section_ ||
        (section < 0 && archive_.root.kids[0].group != "STG ")) {
      if (error)
        *error = section < 0 ? "not an arena (no STG section)" : "no such entry";
      return false;
    }
    svrfmt::ArcNode &stg = archive_.root.kids[section_];
    if (!svrfmt::ArchiveExpand(stg) || stg.kind != svrfmt::ArcKind::Pach) {
      if (error)
        *error = "the arena's list can't be read";
      return false;
    }
    for (auto &k : stg.kids) {
      if (svrfmt::ArchiveKindOf(k) != "textures")
        continue;
      Package p;
      p.node = &k;
      p.id = k.id;
      if (!svrfmt::BundleRead(svrfmt::ArchiveData(k), p.textures))
        continue;
      p.original = p.textures;
      p.shipped = k.stored;
      packages_.push_back(std::move(p));
    }
    for (size_t i = 0; i < packages_.size(); ++i)
      for (size_t j = 0; j < packages_[i].textures.size(); ++j)
        textures_.push_back({i, j});
    return true;
  }

  // The pac's entries ("EMD /00013902"...): group, name.
  std::vector<std::pair<std::string, std::string>> Sections() const {
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto &k : archive_.root.kids)
      out.push_back({k.group, k.name});
    return out;
  }
  size_t SectionSize() const { return archive_.root.kids.size() > section_ ? archive_.root.kids[section_].stored.size() : 0; }

  // Sets the stage number (the STG entry's name, "%04d"): an arena works only under its own slot's number.
  bool SetStage(int number) {
    if (archive_.root.kids.empty() || Stage() == number)
      return true;
    char name[8];
    snprintf(name, sizeof(name), "%04d", number);
    std::string why;
    return svrfmt::ArchiveRename(archive_.root, 0, name, &why);
  }

  // The arena's stage number ("0015" -> 15), the STG entry's name.
  int Stage() const { return archive_.root.kids.empty() ? -1 : atoi(archive_.root.kids[0].name.c_str()); }

  std::vector<Texture> &Textures() { return textures_; }
  const svrfmt::BundleTexture &Get(const Texture &t) const { return packages_[t.package].textures[t.index]; }
  const svrfmt::BundleTexture &Original(const Texture &t) const { return packages_[t.package].original[t.index]; }

  void Replace(Texture &t, Bytes dds) {
    packages_[t.package].textures[t.index].data = std::move(dds);
    packages_[t.package].changed = true;
    t.replaced = true;
  }
  void Restore(Texture &t) {
    packages_[t.package].textures[t.index] = packages_[t.package].original[t.index];
    t.replaced = t.reduced = false;
    packages_[t.package].changed = true;  // (Build finds it unchanged again if it is)
  }

  // The section's models: (child id, name).
  std::vector<std::pair<uint32_t, std::string>> Models() {
    std::vector<std::pair<uint32_t, std::string>> out;
    if (archive_.root.kids.size() <= section_)
      return out;
    for (auto &k : archive_.root.kids[section_].kids) {
      const Bytes d = originals_.count(k.id) ? originals_[k.id] : svrfmt::ArchiveData(k);
      if (!svrfmt::IsJboy(d))
        continue;
      svrfmt::Model m;
      out.push_back({k.id, svrfmt::JboyRead(d, m, nullptr) && !m.name.empty() ? m.name : std::to_string(k.id)});
    }
    return out;
  }
  const Move &MoveOf(uint32_t id) {
    static const Move none;
    const auto it = moves_.find(id);
    return it == moves_.end() ? none : it->second;
  }
  void SetMove(uint32_t id, const Move &m) {
    if (archive_.root.kids.size() <= section_)
      return;
    for (auto &k : archive_.root.kids[section_].kids)
      if (k.id == id && !originals_.count(id))
        originals_[id] = svrfmt::ArchiveData(k);
    moves_[id] = m;
    moves_changed_ = true;
  }
  size_t Moved() const {
    size_t n = copies_.size();
    for (const auto &[id, m] : moves_)
      n += !m.None() && !copies_.count(id);
    return n;
  }

  // A copy of an object (the arena's own model, under a free child id, with the source's line of the flag table
  // 50001), placed with SetMove like the others (it starts 3 m along x, beside its source). Its id, 0 if none is free.
  uint32_t AddCopy(uint32_t source) {
    if (archive_.root.kids.size() <= section_)
      return 0;
    auto &kids = archive_.root.kids[section_].kids;
    Bytes data;
    for (auto &k : kids)
      if (k.id == source)
        data = originals_.count(source) ? originals_[source] : svrfmt::ArchiveData(k);
    if (!svrfmt::IsJboy(data))
      return 0;
    auto used = [&](uint32_t id) {
      for (auto &k : kids)
        if (k.id == id)
          return true;
      return copies_.count(id) > 0;
    };
    uint32_t id = 0;  // (39-99 and 300-799 are free in the shipped arenas; 950-999 are the ring's)
    for (uint32_t c = 39; c < 800 && !id; c = c == 99 ? 300 : c + 1)
      if (!used(c))
        id = c;
    if (!id)
      return 0;
    copies_[id] = source;
    originals_[id] = data;
    Move m;
    m.d[0] = 300;
    moves_[id] = m;
    moves_changed_ = true;
    return id;
  }
  void RemoveCopy(uint32_t id) {
    if (!copies_.erase(id) || archive_.root.kids.size() <= section_)
      return;
    originals_.erase(id);
    moves_.erase(id);
    auto &list = archive_.root.kids[section_];
    for (size_t i = 0; i < list.kids.size(); ++i)
      if (list.kids[i].id == id) {
        svrfmt::ArchiveRemove(list, i);
        break;
      }
    Repoint();
    moves_changed_ = true;
  }
  bool IsCopy(uint32_t id) const { return copies_.count(id) > 0; }

  // An object made from a file's triangles (game space, cm; mesh_import.h), after Kaiko's arena objects
  // (model_objects.cpp): a copy of `host` (AddCopy: its flags, its nodes) whose meshes become the file's, with the
  // host's first mesh's material, one static bone, and as its picture `dds` (put into the arena's first texture
  // package as `picture`; empty: the host's). It is placed centred on the host, standing on the host's floor, so the
  // game's culling keeps it; then it moves as any object. Its id, 0 if it can't be made.
  uint32_t AddMeshObject(uint32_t host, const std::vector<std::array<float, 3>> &pos,
                         const std::vector<std::array<float, 3>> &normal, const std::vector<std::array<float, 2>> &uv,
                         const std::vector<uint32_t> &tris, const Bytes &dds, const std::string &picture) {
    svrfmt::Model m;
    if (pos.empty() || tris.size() < 3)
      return 0;
    const uint32_t id = AddCopy(host);
    if (!id || !svrfmt::JboyRead(originals_[id], m, nullptr) || m.meshes.empty()) {
      if (id)
        RemoveCopy(id);
      return 0;
    }
    const svrfmt::Mesh tmpl = m.meshes[0];
    // Where: the host's centre (x, z) and its lowest point (y down: its largest y).
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f}, flo[3] = {1e30f, 1e30f, 1e30f},
          fhi[3] = {-1e30f, -1e30f, -1e30f};
    for (const auto &s : m.meshes)
      for (const auto &v : s.verts)
        for (int q = 0; q < 3; ++q)
          lo[q] = std::min(lo[q], v.pos[q]), hi[q] = std::max(hi[q], v.pos[q]);
    for (const auto &p : pos)
      for (int q = 0; q < 3; ++q)
        flo[q] = std::min(flo[q], p[q]), fhi[q] = std::max(fhi[q], p[q]);
    const float off[3] = {(lo[0] + hi[0]) / 2 - (flo[0] + fhi[0]) / 2, hi[1] - fhi[1], (lo[2] + hi[2]) / 2 - (flo[2] + fhi[2]) / 2};
    // Its picture: one more texture in the first package, the model's list naming it.
    int32_t slot = -1;
    if (!dds.empty() && !packages_.empty()) {
      Package &pk = packages_[0];
      pk.textures.push_back({picture, "dds", dds});
      pk.original.push_back(pk.textures.back());
      pk.changed = true;
      textures_.push_back({0, pk.textures.size() - 1});
      slot = int32_t(m.textures.size());
      m.textures.push_back(picture);
    }
    // The template's average vertex colour (an arena's light is in its vertices).
    uint64_t argb[4] = {};
    for (const auto &v : tmpl.verts)
      for (int c = 0; c < 4; ++c)
        argb[c] += (v.color >> (24 - 8 * c)) & 0xFF;
    uint32_t colour = 0xFFFFFFFFu;
    if (!tmpl.verts.empty()) {
      colour = 0;
      for (int c = 0; c < 4; ++c)
        colour |= uint32_t(argb[c] / tmpl.verts.size()) << (24 - 8 * c);
    }
    m.meshes.clear();
    // One mesh per 65,535 vertices (16-bit indices): the triangles in order, their vertices renumbered.
    float sphere_lo[3] = {1e30f, 1e30f, 1e30f}, sphere_hi[3] = {-1e30f, -1e30f, -1e30f};
    for (size_t t0 = 0; t0 + 2 < tris.size();) {
      svrfmt::Mesh me = tmpl;
      me.verts.clear(), me.uvs.clear(), me.tangents.clear(), me.strips.clear();
      for (auto &w : me.weights)
        w.clear();
      std::map<uint32_t, uint16_t> local;
      svrfmt::Strip strip;
      strip.prim = tmpl.strips.empty() ? 6 : tmpl.strips[0].prim;
      for (; t0 + 2 < tris.size() && local.size() + 3 <= 65535; t0 += 3) {
        uint16_t ix[3];
        for (int c = 0; c < 3; ++c) {
          const uint32_t src = tris[t0 + size_t(c)];
          auto it = local.find(src);
          if (it == local.end()) {
            svrfmt::Vertex v{};
            for (int q = 0; q < 3; ++q) {
              v.pos[q] = pos[src][q] + off[q];
              v.normal[q] = src < normal.size() ? normal[src][q] : 0.0f;
              sphere_lo[q] = std::min(sphere_lo[q], v.pos[q]), sphere_hi[q] = std::max(sphere_hi[q], v.pos[q]);
            }
            v.color = colour;
            me.verts.push_back(v);
            me.uvs.push_back(src < uv.size() ? uv[src] : std::array<float, 2>{0, 0});
            if (!tmpl.tangents.empty()) {  // (any direction across the normal)
              const float *n = v.normal;
              std::array<float, 3> t = std::fabs(n[1]) < 0.9f ? std::array<float, 3>{n[2], 0, -n[0]}
                                                              : std::array<float, 3>{1, 0, 0};
              const float l = std::sqrt(t[0] * t[0] + t[1] * t[1] + t[2] * t[2]);
              me.tangents.push_back({t[0] / l, t[1] / l, t[2] / l});
            }
            for (size_t j = 0; j < me.weights.size(); ++j)
              me.weights[j].push_back(j == 0 ? svrfmt::Weight{{0, 255, 255, 255}, 1.0f}
                                             : svrfmt::Weight{{255, 255, 255, 255}, 0.0f});
            it = local.emplace(src, uint16_t(me.verts.size() - 1)).first;
          }
          ix[c] = it->second;
        }
        if (!strip.IsStrip()) {
          strip.indices.insert(strip.indices.end(), {ix[0], ix[1], ix[2]});
          continue;
        }
        // Joined into one strip: the last index and the next triangle's first repeated (empty triangles), the
        // triangle starting at an even place so it keeps its turn.
        if (!strip.indices.empty()) {
          strip.indices.push_back(strip.indices.back());
          strip.indices.push_back(ix[0]);
          if (strip.indices.size() % 2)
            strip.indices.push_back(ix[0]);
        }
        strip.indices.insert(strip.indices.end(), {ix[0], ix[1], ix[2]});
      }
      me.strips.push_back(std::move(strip));
      if (slot >= 0)
        for (auto &p : me.params)
          if ((p.name == "sampDiffusePrm" || p.name == "texDiffuse" || p.name == "g_mDiffusePrm") && p.value.size() >= 4)
            svrfmt::PutBe32(p.value.data(), uint32_t(slot));
      m.meshes.push_back(std::move(me));
    }
    float centre[3], r = 0;
    for (int q = 0; q < 3; ++q)
      centre[q] = (sphere_lo[q] + sphere_hi[q]) / 2, r = std::max(r, (sphere_hi[q] - sphere_lo[q]) / 2);
    r *= 1.7321f;  // (the box's half diagonal at most)
    for (auto &me : m.meshes)
      me.sphere = {centre[0], centre[1], centre[2], r};
    for (auto &n : m.nodes)
      if (n.sphere[3] > 0)
        n.sphere[0] = centre[0], n.sphere[1] = centre[1], n.sphere[2] = centre[2], n.sphere[3] = r;
    if (m.parts.size() > 1) {  // (one part now: the header's part count +0x34 too)
      m.parts.resize(1);
      if (m.header.size() >= 0x30)
        svrfmt::PutBe32(&m.header[0x2C], 1);
    }
    if (!m.parts.empty())
      m.parts[0].meshes = uint32_t(m.meshes.size());
    originals_[id] = svrfmt::JboyWrite(m);
    moves_[id] = Move();
    moves_changed_ = true;
    if (slot >= 0)
      added_pictures_[id] = textures_.back();
    return id;
  }
  // An object made by AddMeshObject taken away again, its picture too (when it is still the package's last one).
  void RemoveMeshObject(uint32_t id) {
    RemoveCopy(id);
    const auto it = added_pictures_.find(id);
    if (it == added_pictures_.end())
      return;
    const Texture t = it->second;
    added_pictures_.erase(it);
    Package &p = packages_[t.package];
    if (t.index + 1 != p.textures.size() || textures_.empty() || textures_.back().package != t.package ||
        textures_.back().index != t.index)
      return;
    p.textures.pop_back();
    p.original.pop_back();
    textures_.pop_back();
    p.changed = true;
  }

  // An object of the section as the game draws it now (moved, copied...), for an export.
  bool ModelOf(uint32_t id, svrfmt::Model &m) {
    if (archive_.root.kids.size() <= section_)
      return false;
    for (auto &k : archive_.root.kids[section_].kids)
      if (k.id == id)
        return svrfmt::JboyRead(svrfmt::ArchiveData(k), m, nullptr);
    return false;
  }
  // A texture of the arena by name (its packages), for an export.
  const svrfmt::BundleTexture *TextureNamed(const std::string &name) const {
    for (const auto &p : packages_)
      for (const auto &t : p.textures)
        if (t.name == name)
          return &t;
    return nullptr;
  }

  // The material groups of the section's body model (child 0), in the order met: "skin", "gear:body"...
  std::vector<std::string> MaterialGroups() {
    std::vector<std::string> out;
    svrfmt::Model m;
    if (!BodyModel(m))
      return out;
    ForEachMesh(m, [&](svrfmt::Mesh &, const std::string &group) {
      if (!group.empty() && std::find(out.begin(), out.end(), group) == out.end())
        out.push_back(group);
    });
    return out;
  }
  const Material &MaterialOf(const std::string &group) {
    static const Material none;
    const auto it = materials_.find(group);
    return it == materials_.end() ? none : it->second;
  }
  void SetMaterial(const std::string &group, const Material &m) {
    materials_[group] = m;
    materials_changed_ = true;
  }
  bool MaterialsChanged() const {
    for (const auto &[g, m] : materials_)
      if (!m.Default())
        return true;
    return false;
  }

  // The rebuilt file (the original bytes if nothing changed).
  Bytes Build() {
    if (materials_changed_ && archive_.root.kids.size() > section_) {
      materials_changed_ = false;
      svrfmt::Model m;
      if (BodyModel(m)) {
        ForEachMesh(m, [&](svrfmt::Mesh &me, const std::string &group) {
          const auto it = materials_.find(group);
          if (it == materials_.end())
            return;
          for (auto &p : me.params) {
            auto scale = [&](size_t at, float k) {
              if (p.value.size() < at + 4)
                return;
              uint32_t u = uint32_t(p.value[at]) << 24 | p.value[at + 1] << 16 | p.value[at + 2] << 8 | p.value[at + 3];
              float f;
              memcpy(&f, &u, 4);
              f *= k;
              memcpy(&u, &f, 4);
              for (int b = 0; b < 4; ++b)
                p.value[at + size_t(b)] = uint8_t(u >> (24 - 8 * b));
            };
            if (p.type == 0x0a && (p.name == "g_fSpecLev" || p.name == "g_fSpecLvBase"))
              scale(0, it->second.shine);
            else if (p.type == 0x0d && p.name == "g_f4MatDifCol")
              for (int c = 0; c < 3; ++c)
                scale(size_t(c) * 4, it->second.tint[c]);
          }
        });
        for (auto &k : archive_.root.kids[section_].kids)
          if (k.id == 0)
            svrfmt::ArchiveReplace(k, svrfmt::JboyWrite(m));
        svrfmt::ArchiveTouch(archive_.root.kids[section_]);
      }
    }
    if (moves_changed_ && archive_.root.kids.size() > section_) {
      moves_changed_ = false;
      // the copies first: their children (in id order), their lines in the flag table
      auto &list = archive_.root.kids[section_];
      for (const auto &[id, source] : copies_) {
        bool have = false;
        for (auto &k : list.kids)
          have = have || k.id == id;
        std::string why;
        if (!have)
          svrfmt::ArchiveAdd(list, "", "", id, originals_[id], &why);
      }
      for (auto &k : list.kids) {
        if (k.id != 50001)
          continue;
        if (flags_text_.empty())
          flags_text_ = svrfmt::ArchiveData(k);
        std::string t(flags_text_.begin(), flags_text_.end());
        for (const auto &[id, source] : copies_) {  // "<source> flags" again as "<id> flags"
          const std::string key = std::to_string(source);
          size_t at = 0;
          while (at < t.size()) {
            size_t e = t.find('\n', at);
            if (e == std::string::npos)
              e = t.size();
            const std::string line = t.substr(at, e - at);
            if (line.rfind(key, 0) == 0 && (line.size() == key.size() || line[key.size()] == ' ' || line[key.size()] == '\r')) {
              const std::string copy = std::to_string(id) + line.substr(key.size());
              t.insert(e < t.size() ? e + 1 : e, (e < t.size() ? "" : "\n") + copy + (e < t.size() ? "\n" : ""));
              break;
            }
            at = e + 1;
          }
        }
        svrfmt::ArchiveReplace(k, Bytes(t.begin(), t.end()));
      }
      Repoint();
      for (auto &k : archive_.root.kids[section_].kids) {
        const auto o = originals_.find(k.id);
        if (o == originals_.end())
          continue;
        const Move &mv = moves_[k.id];
        svrfmt::Model m;
        if (mv.None() || !svrfmt::JboyRead(o->second, m, nullptr)) {
          svrfmt::ArchiveReplace(k, o->second);
          continue;
        }
        // The game draws an arena model's vertices where they are stored (its root node's place is not used: a
        // barricade moved by its node stayed put, 2026-10-26): the vertices move, turned about the object's centre.
        const float a = mv.turn * 3.14159265f / 180.0f, c = std::cos(a), sn = std::sin(a);
        float lo[2] = {1e30f, 1e30f}, hi[2] = {-1e30f, -1e30f};
        for (const auto &s : m.meshes)
          for (const auto &v : s.verts)
            for (int q = 0; q < 2; ++q)
              lo[q] = std::min(lo[q], v.pos[q * 2]), hi[q] = std::max(hi[q], v.pos[q * 2]);
        const float cx = (lo[0] + hi[0]) / 2, cz = (lo[1] + hi[1]) / 2;
        const float dy = mv.d[1] + (mv.hide ? 100000.0f : 0.0f);  // (y points down: far under the floor)
        auto place = [&](float *p) {
          const float x = p[0] - cx, z = p[2] - cz;
          p[0] = c * x + sn * z + cx + mv.d[0];
          p[2] = -sn * x + c * z + cz + mv.d[2];
          p[1] += dy;
        };
        auto turn = [&](float *v) {
          const float x = v[0], z = v[2];
          v[0] = c * x + sn * z;
          v[2] = -sn * x + c * z;
        };
        for (auto &s : m.meshes) {
          for (auto &v : s.verts)
            place(v.pos), turn(v.normal);
          for (auto &t : s.tangents)
            turn(t.data());
          if (s.sphere[3] > 0)
            place(s.sphere.data());
        }
        for (auto &n : m.nodes)
          if (n.sphere[3] > 0)
            place(n.sphere);
        svrfmt::ArchiveReplace(k, svrfmt::JboyWrite(m));
      }
      svrfmt::ArchiveTouch(archive_.root.kids[section_]);
    }
    for (auto &p : packages_) {
      if (!p.changed)
        continue;
      const Bytes raw = svrfmt::BundleWrite(p.textures);
      p.node->stored = svrfmt::IsBpe(p.shipped) ? bpe_like::EncodeLike(raw, p.shipped) : raw;
      p.node->dirty = true;
    }
    return svrfmt::ArchiveSave(archive_);
  }

  // The body model as shipped (child 0 of the section), kept the first time.
  bool BodyModel(svrfmt::Model &m) {
    if (archive_.root.kids.size() <= section_)
      return false;
    if (body_.empty())
      for (auto &k : archive_.root.kids[section_].kids)
        if (k.id == 0)
          body_ = svrfmt::ArchiveData(k);
    return svrfmt::IsJboy(body_) && svrfmt::JboyRead(body_, m, nullptr);
  }
  template <class F> static void ForEachMesh(svrfmt::Model &m, F f) {
    size_t part = 0, left = m.parts.empty() ? m.meshes.size() : m.parts[0].meshes;
    for (auto &me : m.meshes) {
      while (left == 0 && part + 1 < m.parts.size())
        left = m.parts[++part].meshes;
      if (left)
        --left;
      std::string pname = part < m.parts.size() ? m.parts[part].name : "";
      for (auto &c : pname)
        c = char(std::tolower(static_cast<unsigned char>(c)));
      std::string group;
      if (me.shader.rfind("yCh_skin", 0) == 0 && pname != "tooth" && pname != "mouth")
        group = "skin";
      else if (me.shader.rfind("yCh_gear", 0) == 0)
        group = "gear:" + pname;
      f(me, group);
    }
  }

 public:
  // Makes the file fit `budget` bytes: the biggest textures not replaced lose their top level, one at a time.
  // The textures reduced (names); the file built last is in `out`.
  // section_budget: the budget is for the section (an attire), not the whole file.
  std::vector<std::string> Fit(size_t budget, Bytes &out, bool section_budget = false) {
    std::vector<std::string> reduced;
    out = Build();
    auto size = [&] { return section_budget ? archive_.root.kids[section_].stored.size() : out.size(); };
    std::set<size_t> tried;
    while (size() > budget) {
      size_t best = SIZE_MAX, best_size = 0;
      for (size_t i = 0; i < textures_.size(); ++i) {
        const Texture &t = textures_[i];
        if (t.replaced || tried.count(i) || Risky(Get(t).name))
          continue;
        const size_t s = Get(t).data.size();
        if (s > best_size) {
          best = i;
          best_size = s;
        }
      }
      if (best == SIZE_MAX)
        break;  // nothing left to make smaller
      tried.insert(best);
      Texture &t = textures_[best];
      Bytes dds = Get(t).data;
      if (!DropTopMip(dds))
        continue;
      packages_[t.package].textures[t.index].data = std::move(dds);
      packages_[t.package].changed = true;
      t.reduced = true;
      reduced.push_back(Get(t).name);
      out = Build();
    }
    return reduced;
  }

  // Screens the game may draw videos into, and light maps: never made smaller (their size may be expected).
  static bool Risky(const std::string &name) {
    std::string n = name;
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    for (const char *w : {"titan", "tron", "mov", "light", "lgt"})
      if (n.find(w) != std::string::npos)
        return true;
    return false;
  }

  // Undoes Fit's reductions (before replacing more pictures, so the next Fit starts from the shipped ones).
  void Unreduce() {
    for (auto &t : textures_)
      if (t.reduced && !t.replaced) {
        packages_[t.package].textures[t.index] = packages_[t.package].original[t.index];
        t.reduced = false;
      }
  }

 private:
  // The texture packages' nodes again (an object added or removed moves the list's children).
  void Repoint() {
    if (archive_.root.kids.size() <= section_)
      return;
    for (auto &p : packages_)
      for (auto &k : archive_.root.kids[section_].kids)
        if (k.id == p.id)
          p.node = &k;
  }
  std::map<uint32_t, uint32_t> copies_;  // copy's id -> its source's
  std::map<uint32_t, Texture> added_pictures_;  // an object made from a file -> the picture added for it
  Bytes flags_text_;                     // the flag table (50001) as shipped
  std::map<uint32_t, Bytes> originals_;  // the moved models as shipped
  std::map<uint32_t, Move> moves_;
  bool moves_changed_ = false;
  std::map<std::string, Material> materials_;  // by group (MaterialGroups)
  bool materials_changed_ = false;
  Bytes body_;  // the section's body model as shipped
  svrfmt::Archive archive_;
  size_t section_ = 0;
  std::vector<Package> packages_;
  std::vector<Texture> textures_;
};

}  // namespace arena_edit
