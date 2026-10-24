// WWE12-PC: taken from Kaiko's WWE SmackDown vs. Raw 2011 PC port (with his permission, credit given),
// https://github.com/KaikoClanworth1/wwe-svr2011-pc, port/modmaker/svrfmt/jboy.h at b2521fe.
// Reading also takes WWE '12 models: their mesh records are 0xB8 bytes, a word inserted at +0x74 (a second pointer
// to the strip table), every later field 4 bytes further (docs/limit_breaking.md, "Comparaison JBOY").
// JBOY arena models (Yuke's YOBJ, big-endian). Mirrors tools/jboy.py, which
// reads and rewrites all 8633 models in the 35 arena files (layout notes in
// that file and docs/ARENA_MOD_MAKER_PLAN.md).
#pragma once

#include <array>
#include <string>
#include <vector>

#include "bytes.h"

namespace svrfmt {

struct Vertex {
  float pos[3];
  float normal[3];
  uint32_t color;  // ARGB
};

struct Weight {
  uint8_t bones[4];  // palette slots, 255 = none
  float weight;
};

struct Param {
  std::string name;
  uint16_t type = 0;  // 0x0d f32x4, 0x0a f32, 0x05 int, 0x0f texture slot, 0x10 bool
  Bytes value;        // big-endian, as stored
};

struct Strip {
  uint32_t prim = 6;  // 6 = triangle strip
  std::vector<uint16_t> indices;
};

struct Mesh {
  Bytes raw;  // the 0xB4-byte descriptor as read (0xB8 in WWE '12; unknown fields kept)
  std::vector<int32_t> palette;
  uint32_t material = 0;  // +0x74
  std::string shader;
  uint32_t vfmt = 0;
  std::array<float, 4> sphere{};
  std::vector<Vertex> verts;
  // influence j of vertex k is weights[j][k] (stored interleaved: vertex by
  // vertex, all its influences); bones[0] is a palette slot, 255 = none
  std::vector<std::vector<Weight>> weights;
  std::vector<std::array<float, 2>> uvs;
  std::vector<std::array<float, 3>> tangents;  // WWE '12 superstars: one per vertex (the record's word at +0x74)
  std::vector<Param> params;
  std::vector<Strip> strips;
};

// 80 bytes: name, t at +16, a pad, r at +32 (Euler radians, R = Rz Ry Rx), a
// pad, parent at +48 (-1: root), 3 unknown, the bounding sphere at +64. A
// node's local matrix is T(t) R(r). Mesh palettes hold node index + 1.
struct Node {
  std::string name;
  float t[3] = {}, r[3] = {};
  uint32_t pad28 = 0, pad44 = 0;
  int32_t parent = -1;
  uint32_t u52[3] = {};
  float sphere[4] = {};
};

// The model's parts (header +0x34 = their number), 32 bytes each: name, then {1, 0, meshes, 0}; the meshes follow the
// parts' order (superstars: boots, teeth, wrist, body, kneepad, eyes, shorts...).
struct Part {
  std::string name;
  uint32_t meshes = 0;
  Bytes info;  // the 16 bytes after the name (meshes at +8)
};

struct Model {
  bool w12 = false;  // WWE '12 layout (0xB8-byte mesh records, tangents)
  Bytes header;  // file bytes 8..0x48 (counts and pointers rewritten on write)
  std::string name;  // = parts[0]
  Bytes group;   // 16 bytes after the name (parts[0].info)
  std::vector<Part> parts;  // all of them (written when more than one; their mesh counts must add up)
  std::vector<std::string> textures;
  std::vector<Node> nodes;
  std::vector<Mesh> meshes;
};

inline bool IsJboy(const Bytes& b) { return b.size() >= 0x48 && !std::memcmp(b.data(), "JBOY", 4); }
bool JboyRead(const Bytes& in, Model& out, std::string* error = nullptr);
// In the model's own layout (m.w12).
Bytes JboyWrite(const Model& m);

// Triangle list from a degenerate-joined strip (winding alternates).
std::vector<std::array<uint16_t, 3>> StripToTriangles(const std::vector<uint16_t>& strip);
// Triangle list -> one strip (each triangle joined with degenerates).
std::vector<uint16_t> TrianglesToStrip(const std::vector<std::array<uint16_t, 3>>& tris);

}  // namespace svrfmt
