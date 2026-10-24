// WWE12-PC: taken from Kaiko's WWE SmackDown vs. Raw 2011 PC port (with his permission, credit given),
// https://github.com/KaikoClanworth1/wwe-svr2011-pc, port/modmaker/svrfmt/jboy.cpp at b2521fe.
// Reading and writing also take WWE '12 models: their mesh records are 0xB8 bytes, a word inserted at +0x74 (arenas: a
// second pointer to the strip table; superstars: to one tangent per vertex), every later field 4 bytes further
// (docs/limit_breaking.md, "Comparaison JBOY"); and all the model's parts (superstars have several).
#include "jboy.h"

#include <algorithm>

namespace svrfmt {

namespace {

constexpr size_t B = 8;  // pointers are offsets from byte 8
constexpr size_t kMesh = 0xB4, kNode = 80;

std::vector<uint32_t> PofDecode(const uint8_t* p, size_t n) {
  std::vector<uint32_t> offs;
  uint32_t cur = 0;
  for (size_t i = 0; i < n;) {
    const int t = p[i] >> 6;
    uint32_t v;
    if (t == 0) break;
    if (t == 1) { v = p[i] & 0x3F; i += 1; }
    else if (t == 2) { v = (p[i] & 0x3F) << 8 | p[i + 1]; i += 2; }
    else { v = uint32_t(p[i] & 0x3F) << 24 | p[i + 1] << 16 | p[i + 2] << 8 | p[i + 3]; i += 4; }
    cur += v * 4;
    offs.push_back(cur);
  }
  return offs;
}

Bytes PofEncode(std::vector<uint32_t> offs) {
  std::sort(offs.begin(), offs.end());
  Bytes out;
  uint32_t cur = 0;
  for (uint32_t o : offs) {
    const uint32_t v = (o - cur) / 4;
    cur = o;
    if (v < 0x40) out.push_back(uint8_t(0x40 | v));
    else if (v < 0x4000) { out.push_back(uint8_t(0x80 | v >> 8)); out.push_back(uint8_t(v)); }
    else { out.push_back(uint8_t(0xC0 | v >> 24)); out.push_back(uint8_t(v >> 16)); out.push_back(uint8_t(v >> 8)); out.push_back(uint8_t(v)); }
  }
  Pad(out, 4);  // zero padding doubles as the terminator
  return out;
}

}  // namespace

bool JboyRead(const Bytes& d, Model& m, std::string* error) {
  auto fail = [&](const char* why) { if (error) *error = why; return false; };
  if (!IsJboy(d)) return fail("not JBOY");
  const size_t len = Be32(&d[4]);
  if (8 + len + 8 > d.size() || std::memcmp(&d[8 + len], "POF0", 4)) return fail("no POF0");
  auto in = [&](size_t o, size_t n) { return o + n <= 8 + len; };
  m = {};
  m.header.assign(d.begin() + 8, d.begin() + 0x48);
  const uint32_t nmesh = Be32(&d[0x18]), mptr = Be32(&d[0x1C]), nnode = Be32(&d[0x20]), ntex = Be32(&d[0x24]);
  const uint32_t nptr = Be32(&d[0x28]), tptr = Be32(&d[0x2C]), optr = Be32(&d[0x30]);
  // WWE '12: 0xB8-byte mesh records (the shader name at +0x7C instead of +0x78; SvR 2011 has a pointer there).
  const bool w12 = nmesh && in(B + mptr, 0x80) && d[B + mptr + 0x7C] == 'y' && d[B + mptr + 0x78] != 'y';
  const size_t stride = w12 ? kMesh + 4 : kMesh;
  auto f = [w12](size_t off) { return w12 && off >= 0x74 ? off + 4 : off; };  // a field's place in the record
  if (!in(B + optr, 32) || !in(B + tptr, 16 * ntex) || !in(B + nptr, kNode * nnode) || !in(B + mptr, stride * nmesh))
    return fail("table out of range");
  m.w12 = w12;
  m.name = Name16(&d[B + optr]);
  m.group.assign(d.begin() + B + optr + 16, d.begin() + B + optr + 32);
  const uint32_t nparts = Be32(&d[0x34]);
  for (uint32_t i = 0; i < nparts && in(B + optr + 32 * size_t(i), 32); ++i) {
    const uint8_t* p = &d[B + optr + 32 * size_t(i)];
    Part part;
    part.name = Name16(p);
    part.info.assign(p + 16, p + 32);
    part.meshes = Be32(p + 24);
    m.parts.push_back(std::move(part));
  }
  for (uint32_t i = 0; i < ntex; ++i) m.textures.push_back(Name16(&d[B + tptr + 16 * i]));
  for (uint32_t i = 0; i < nnode; ++i) {
    const uint8_t* o = &d[B + nptr + kNode * i];
    Node n;
    n.name = Name16(o);
    for (int k = 0; k < 3; ++k) { n.t[k] = BeF(o + 16 + 4 * k); n.r[k] = BeF(o + 32 + 4 * k); }
    n.pad28 = Be32(o + 28);
    n.pad44 = Be32(o + 44);
    n.parent = int32_t(Be32(o + 48));
    for (int k = 0; k < 3; ++k) n.u52[k] = Be32(o + 52 + 4 * k);
    for (int k = 0; k < 4; ++k) n.sphere[k] = BeF(o + 64 + 4 * k);
    m.nodes.push_back(n);
  }
  for (uint32_t i = 0; i < nmesh; ++i) {
    const size_t o = B + mptr + stride * i;
    Mesh s;
    s.raw.assign(d.begin() + o, d.begin() + o + stride);
    const uint32_t vc = Be32(&d[o]), nstrip = Be32(&d[o + 4]), npal = Be32(&d[o + 8]);
    for (uint32_t k = 0; k < npal && k < 20; ++k) s.palette.push_back(int32_t(Be32(&d[o + 0x0C + 4 * k])));
    const uint32_t nw = std::max<uint32_t>(1, Be32(&d[o + 0x5C]));
    const uint32_t vblk = Be32(&d[o + 0x68]), wptr = Be32(&d[o + 0x6C]), uvptr = Be32(&d[o + 0x70]);
    s.material = Be32(&d[o + f(0x74)]);
    s.shader = Name16(&d[o + f(0x78)]);
    s.vfmt = Be32(&d[o + f(0x8C)]);
    const uint32_t npar = Be32(&d[o + f(0x90)]), parptr = Be32(&d[o + f(0x94)]), iptr = Be32(&d[o + f(0x98)]);
    for (int k = 0; k < 4; ++k) s.sphere[k] = BeF(&d[o + f(0xA4) + 4 * k]);
    if (!in(B + vblk, 4)) return fail("vertex header out of range");
    const size_t vd = B + Be32(&d[B + vblk]);
    if (!in(vd, 28 * size_t(vc)) || !in(B + wptr, 8 * size_t(vc) * nw) || !in(B + uvptr, 8 * size_t(vc)))
      return fail("vertex data out of range");
    s.verts.resize(vc);
    s.uvs.resize(vc);
    for (uint32_t k = 0; k < vc; ++k) {
      const uint8_t* v = &d[vd + 28 * k];
      for (int c = 0; c < 3; ++c) { s.verts[k].pos[c] = BeF(v + 4 * c); s.verts[k].normal[c] = BeF(v + 12 + 4 * c); }
      s.verts[k].color = Be32(v + 24);
      s.uvs[k] = {BeF(&d[B + uvptr + 8 * k]), BeF(&d[B + uvptr + 8 * k + 4])};
    }
    // WWE '12: the word at +0x74 points to the strips (arenas), or to one tangent per vertex (superstars).
    if (w12 && Be32(&d[o + 0x74]) != iptr) {
      const uint32_t tptr2 = Be32(&d[o + 0x74]);
      if (!in(B + tptr2, 12 * size_t(vc))) return fail("tangents out of range");
      s.tangents.resize(vc);
      for (uint32_t k = 0; k < vc; ++k)
        for (int c = 0; c < 3; ++c) s.tangents[k][c] = BeF(&d[B + tptr2 + 12 * k + 4 * c]);
    }
    for (uint32_t j = 0; j < nw; ++j) {
      std::vector<Weight> blk(vc);
      for (uint32_t k = 0; k < vc; ++k) {
        const uint8_t* w = &d[B + wptr + 8 * (size_t(nw) * k + j)];
        std::memcpy(blk[k].bones, w, 4);
        blk[k].weight = BeF(w + 4);
      }
      s.weights.push_back(std::move(blk));
    }
    if (!in(B + parptr, 4 * size_t(npar))) return fail("params out of range");
    for (uint32_t k = 0; k < npar; ++k) {
      const size_t po = B + Be32(&d[B + parptr + 4 * k]);
      if (!in(po, 20)) return fail("param out of range");
      Param p;
      p.name = Name16(&d[po]);
      p.type = Be16(&d[po + 16]);
      const uint16_t size = Be16(&d[po + 18]);
      if (size < 20 || !in(po, size)) return fail("param size");
      p.value.assign(d.begin() + po + 20, d.begin() + po + size);
      s.params.push_back(std::move(p));
    }
    if (!in(B + iptr, 12 * size_t(nstrip))) return fail("strips out of range");
    for (uint32_t k = 0; k < nstrip; ++k) {
      const uint8_t* h = &d[B + iptr + 12 * k];
      Strip st;
      st.prim = Be32(h);
      const uint32_t cnt = Be32(h + 4), at = Be32(h + 8);
      if (!in(B + at, 2 * size_t(cnt))) return fail("indices out of range");
      for (uint32_t q = 0; q < cnt; ++q) st.indices.push_back(Be16(&d[B + at + 2 * q]));
      s.strips.push_back(std::move(st));
    }
    m.meshes.push_back(std::move(s));
  }
  return true;
}

Bytes JboyWrite(const Model& m) {
  const size_t rec = m.w12 ? kMesh + 4 : kMesh;
  auto f = [&m](size_t off) { return m.w12 && off >= 0x74 ? off + 4 : off; };  // a field's place in the record
  Bytes out(0x48, 0);
  std::vector<uint32_t> ptrs;  // file offsets holding pointers
  auto put_ptr = [&](size_t at, size_t target) {
    PutBe32(&out[at], uint32_t(target - B));
    ptrs.push_back(uint32_t(at - B));
  };
  const size_t mesh_at = out.size();
  out.resize(out.size() + rec * m.meshes.size(), 0);
  for (size_t i = 0; i < m.meshes.size(); ++i) {
    const Mesh& s = m.meshes[i];
    const size_t vc = s.verts.size(), md = mesh_at + rec * i;
    const size_t par_list = out.size();
    out.resize(out.size() + 4 * s.params.size(), 0);
    std::vector<size_t> par_at;
    for (const auto& p : s.params) {
      par_at.push_back(out.size());
      AppName(out, p.name);
      AppBe16(out, p.type);
      AppBe16(out, uint16_t(20 + p.value.size()));
      App(out, p.value);
    }
    const size_t vh = out.size();
    out.resize(out.size() + 4, 0);
    const size_t vd = out.size();
    for (const auto& v : s.verts) {
      for (float f : v.pos) AppBeF(out, f);
      for (float f : v.normal) AppBeF(out, f);
      AppBe32(out, v.color);
    }
    const size_t wd = out.size();
    for (size_t k = 0; k < vc; ++k)
      for (const auto& blk : s.weights) {
        const Weight w = k < blk.size() ? blk[k] : Weight{{255, 0, 0, 0}, 0.f};
        App(out, w.bones, 4);
        AppBeF(out, w.weight);
      }
    if (s.weights.empty())  // every mesh has at least one block (bone 0, 1.0)
      for (size_t k = 0; k < vc; ++k) { AppBe32(out, 0); AppBeF(out, 1.f); }
    const size_t ud = out.size();
    for (const auto& uv : s.uvs) { AppBeF(out, uv[0]); AppBeF(out, uv[1]); }
    const size_t td = out.size();
    for (size_t k = 0; m.w12 && !s.tangents.empty() && k < vc; ++k)
      for (int c = 0; c < 3; ++c) AppBeF(out, k < s.tangents.size() ? s.tangents[k][c] : (c == 0 ? 1.f : 0.f));
    const size_t ih = out.size();
    out.resize(out.size() + 12 * s.strips.size(), 0);
    std::vector<size_t> id_at;
    for (const auto& st : s.strips) {
      id_at.push_back(out.size());
      for (uint16_t x : st.indices) AppBe16(out, x);
    }
    Pad(out, 4);
    // descriptor (unknown fields from the original)
    Bytes desc = s.raw;
    desc.resize(rec, 0);
    PutBe32(&desc[0], uint32_t(vc));
    PutBe32(&desc[4], uint32_t(s.strips.size()));
    PutBe32(&desc[8], uint32_t(s.palette.size()));
    for (int k = 0; k < 20; ++k) PutBe32(&desc[0x0C + 4 * k], k < int(s.palette.size()) ? uint32_t(s.palette[k]) : 0xFFFFFFFFu);
    PutBe32(&desc[0x5C], uint32_t(std::max<size_t>(1, s.weights.size())));
    PutBe32(&desc[f(0x74)], s.material);
    std::memset(&desc[f(0x78)], 0, 16);
    std::memcpy(&desc[f(0x78)], s.shader.data(), std::min<size_t>(16, s.shader.size()));
    PutBe32(&desc[f(0x8C)], s.vfmt);
    PutBe32(&desc[f(0x90)], uint32_t(s.params.size()));
    PutBe32(&desc[f(0x9C)], uint32_t(vc));
    for (int k = 0; k < 4; ++k) PutBeF(&desc[f(0xA4) + 4 * k], s.sphere[k]);
    std::memcpy(&out[md], desc.data(), rec);
    put_ptr(md + 0x68, vh);
    put_ptr(md + 0x6C, wd);
    put_ptr(md + 0x70, ud);
    if (m.w12) put_ptr(md + 0x74, s.tangents.empty() ? ih : td);
    put_ptr(md + f(0x94), par_list);
    put_ptr(md + f(0x98), ih);
    put_ptr(vh, vd);
    for (size_t k = 0; k < par_at.size(); ++k) put_ptr(par_list + 4 * k, par_at[k]);
    for (size_t k = 0; k < s.strips.size(); ++k) {
      PutBe32(&out[ih + 12 * k], s.strips[k].prim);
      PutBe32(&out[ih + 12 * k + 4], uint32_t(s.strips[k].indices.size()));
      put_ptr(ih + 12 * k + 8, id_at[k]);
    }
  }
  Pad(out, 16, B);
  const size_t node_at = out.size();
  for (const auto& n : m.nodes) {
    AppName(out, n.name);
    for (float f : n.t) AppBeF(out, f);
    AppBe32(out, n.pad28);
    for (float f : n.r) AppBeF(out, f);
    AppBe32(out, n.pad44);
    AppBe32(out, uint32_t(n.parent));
    for (uint32_t u : n.u52) AppBe32(out, u);
    for (float f : n.sphere) AppBeF(out, f);
  }
  const size_t tex_at = out.size();
  for (const auto& t : m.textures) AppName(out, t);
  const size_t grp_at = out.size();
  if (m.parts.size() > 1) {
    for (const auto& p : m.parts) {
      AppName(out, p.name);
      Bytes info = p.info;
      info.resize(16, 0);
      PutBe32(&info[8], p.meshes);
      App(out, info);
    }
  } else {
    AppName(out, m.name);
    Bytes group = m.group;
    group.resize(16, 0);
    PutBe32(&group[8], uint32_t(m.meshes.size()));
    App(out, group);
  }
  // header
  std::memcpy(&out[8], m.header.data(), std::min<size_t>(0x40, m.header.size()));
  const uint32_t len = uint32_t(out.size() - 8);
  std::memcpy(&out[0], "JBOY", 4);
  PutBe32(&out[4], len);
  if (!m.w12) PutBe32(&out[8], 0);  // (WWE '12 superstars have 0x10 there: kept)
  PutBe32(&out[0x0C], len);
  PutBe32(&out[0x18], uint32_t(m.meshes.size()));
  put_ptr(0x1C, mesh_at);
  PutBe32(&out[0x20], uint32_t(m.nodes.size()));
  PutBe32(&out[0x24], uint32_t(m.textures.size()));
  put_ptr(0x28, node_at);
  put_ptr(0x2C, tex_at);
  put_ptr(0x30, grp_at);
  if (m.parts.size() > 1) PutBe32(&out[0x34], uint32_t(m.parts.size()));
  const Bytes pof = PofEncode(ptrs);
  App(out, reinterpret_cast<const uint8_t*>("POF0"), 4);
  AppBe32(out, uint32_t(pof.size()));
  App(out, pof);
  return out;
}

std::vector<std::array<uint16_t, 3>> StripToTriangles(const std::vector<uint16_t>& s) {
  std::vector<std::array<uint16_t, 3>> tris;
  for (size_t k = 0; k + 2 < s.size(); ++k) {
    const uint16_t a = s[k], b = s[k + 1], c = s[k + 2];
    if (a == b || b == c || a == c) continue;
    if (k % 2 == 0) tris.push_back({a, b, c});
    else tris.push_back({b, a, c});
  }
  return tris;
}

std::vector<uint16_t> TrianglesToStrip(const std::vector<std::array<uint16_t, 3>>& tris) {
  // Each triangle as its own 3-index run joined by degenerates; every
  // triangle starts at an even position so its winding is kept.
  std::vector<uint16_t> s;
  for (const auto& t : tris) {
    if (!s.empty()) {
      s.push_back(s.back());
      s.push_back(t[0]);
    }
    if (s.size() % 2 == 1) s.push_back(t[0]);
    s.push_back(t[0]);
    s.push_back(t[1]);
    s.push_back(t[2]);
  }
  return s;
}

}  // namespace svrfmt
