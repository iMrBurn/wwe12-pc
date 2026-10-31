// Mod Maker: an arena in 3D (Direct3D 11, drawn into a picture the page shows). Its models (JBOY entries of the
// arena's PACH, read with Kaiko's svrfmt/jboy) are drawn where they are stored: WWE '12 arena models hold their
// vertices in arena space, in cm, y pointing down. The flag table (entry 50001, one line per model id, see the
// Japanese help at its end) says how the game draws each: "r" also mirrored across x = 0, "a" additive, "m(n)"
// placed by an animation (light rigs, beams: left out, they sit at the centre until the game moves them), "e"
// entrance only, "g" match only. Pictures come from the arena's texture packages (sampDiffusePrm = the slot in the
// model's texture list); light maps and shader effects are not drawn: it's a preview, not the game's look.
// Superstars can be posed (keep_skin before Load, then SetPose: the game's motions, yanm.h): each model's node table
// is its skeleton (local = T(t) Rz Ry Rx, y down), its vertices sit in the bind pose, skinned on the CPU.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "svrfmt/archive.h"
#include "svrfmt/jboy.h"
#include "svrfmt/texture.h"
#include "yanm.h"

namespace arena_view {

using Microsoft::WRL::ComPtr;
using svrfmt::Bytes;

struct Flags {
  bool mirrored = false, additive = false, animated = false, entrance_only = false, match_only = false;
};

// Entry 50001: "<id> <flags>" lines.
inline std::map<uint32_t, Flags> ReadFlags(const Bytes &text) {
  std::map<uint32_t, Flags> out;
  size_t p = 0;
  while (p < text.size()) {
    size_t e = p;
    while (e < text.size() && text[e] != '\n')
      ++e;
    const std::string line(text.begin() + ptrdiff_t(p), text.begin() + ptrdiff_t(e));
    p = e + 1;
    if (line.empty() || !isdigit(static_cast<unsigned char>(line[0])))
      continue;
    const uint32_t id = uint32_t(strtoul(line.c_str(), nullptr, 10));
    Flags f;
    size_t i = line.find(' ');
    while (i != std::string::npos && i < line.size()) {
      while (i < line.size() && line[i] == ' ')
        ++i;
      size_t j = i;
      while (j < line.size() && line[j] != ' ' && line[j] != '\r')
        ++j;
      if (j == i)
        break;  // (the end of the line, or a carriage return)
      const std::string w = line.substr(i, j - i);
      if (w == "r")
        f.mirrored = true;
      else if (w == "a")
        f.additive = true;
      else if (w == "e")
        f.entrance_only = true;
      else if (w == "g")
        f.match_only = true;
      else if (w.rfind("m(", 0) == 0)
        f.animated = true;
      i = j < line.size() ? j : std::string::npos;
    }
    out[id] = f;
  }
  return out;
}

struct Vtx {
  float pos[3], normal[3], uv[2];
  uint32_t color;  // RGBA8
};

// A bone's pose from a motion: its local rotation (replacing the bind one) and an offset added to its translation.
struct BonePose {
  bool has_rot = false, has_pos = false;
  yanm::Q rot{0, 0, 0, 1};
  yanm::V pos{0, 0, 0};
};

class View {
 public:
  bool show_entrance = false;  // entrance-only models (stage, ramp lights...) instead of match-only ones
  bool keep_skin = false;      // (before Load) keep what SetPose needs: the skeletons, the bind-pose vertices
  bool show_others = true;     // the superstars added with Add drawn

  // Reads every model of an arena file (or of `section`, an entry of another pac: a superstar's attire) and
  // uploads it. False (and why) if nothing could be read.
  bool Load(ID3D11Device *dev, const Bytes &file, std::string *error, size_t section = 0) {
    device_ = dev;
    dev->GetImmediateContext(&ctx_);
    draws_.clear();
    textures_.clear();
    skels_.clear();
    if (!CreatePipeline(error))
      return false;
    lo_[0] = lo_[1] = lo_[2] = 1e30f;
    hi_[0] = hi_[1] = hi_[2] = -1e30f;
    if (!Add(file, error, section, 0))
      return false;
    ResetCamera();
    return true;
  }

  // Another superstar in the same view (after Load, keep_skin): `person` 1 = the other person of a two-person
  // motion, posed apart (SetPose's person). Remove takes him away.
  bool Add(const Bytes &file, std::string *error, size_t section, int person) {
    if (!ctx_)
      return false;
    Remove(person);
    tex_prefix_ = std::to_string(person) + ":";
    const size_t before = draws_.size();
    svrfmt::Archive a;
    if (!svrfmt::ArchiveOpen(file, a, error) || a.root.kids.size() <= section)
      return false;
    svrfmt::ArcNode &stg = a.root.kids[section];
    if (!svrfmt::ArchiveExpand(stg)) {
      if (error)
        *error = "no model list";
      return false;
    }
    std::map<uint32_t, Flags> flags;
    std::map<std::string, Bytes> dds;  // texture name -> DDS
    for (auto &k : stg.kids) {
      const std::string kind = svrfmt::ArchiveKindOf(k);
      if (k.id == 50001)
        flags = ReadFlags(svrfmt::ArchiveData(k));
      if (kind == "textures") {
        std::vector<svrfmt::BundleTexture> t;
        if (svrfmt::BundleRead(svrfmt::ArchiveData(k), t))
          for (auto &x : t)
            dds[Lower(x.name)] = std::move(x.data);  // (models name them in another case: "mf01" for "MF01")
      }
    }
    for (auto &k : stg.kids) {
      const Bytes data = svrfmt::ArchiveData(k);
      if (!svrfmt::IsJboy(data))
        continue;
      svrfmt::Model m;
      if (!svrfmt::JboyRead(data, m, nullptr))
        continue;
      const Flags f = flags.count(k.id) ? flags[k.id] : Flags{};
      if (f.animated)
        continue;
      int skel = -1;
      if (keep_skin && !m.nodes.empty()) {
        Skel sk;
        sk.person = person;
        for (const auto &n : m.nodes) {
          sk.names.push_back(n.name);
          sk.parent.push_back(n.parent);
          sk.t.push_back({n.t[0], n.t[1], n.t[2]});
          sk.r.push_back(yanm::Euler(n.r[0], n.r[1], n.r[2]));
        }
        const auto bind = sk.World({});
        for (const auto &b : bind)
          sk.inv_bind.push_back(Inverse(b));
        skel = int(skels_.size());
        skels_.push_back(std::move(sk));
      }
      for (const auto &s : m.meshes)
        AddMesh(k.id, m, s, f, dds, skel);
    }
    if (draws_.size() == before) {
      if (error)
        *error = "no models";
      return false;
    }
    return true;
  }
  void Remove(int person) {
    std::vector<int> remap(skels_.size(), -1);
    std::vector<Skel> keep;
    for (size_t i = 0; i < skels_.size(); ++i)
      if (skels_[i].person != person)
        remap[i] = int(keep.size()), keep.push_back(std::move(skels_[i]));
    skels_ = std::move(keep);
    std::vector<Draw> left;
    for (auto &d : draws_) {
      if (d.skel >= 0 && remap[size_t(d.skel)] < 0)
        continue;
      if (d.skel >= 0)
        d.skel = remap[size_t(d.skel)];
      left.push_back(std::move(d));
    }
    draws_ = std::move(left);
    for (auto it = textures_.begin(); it != textures_.end();)
      it = it->first.rfind(std::to_string(person) + ":", 0) == 0 && person ? textures_.erase(it) : std::next(it);
  }

  void ResetCamera() {
    const float extent = std::max({hi_[0] - lo_[0], hi_[1] - lo_[1], hi_[2] - lo_[2]});
    if (extent < 1000) {  // a superstar: in front of him, all of him in view
      for (int k = 0; k < 3; ++k)
        target_[k] = (lo_[k] + hi_[k]) * 0.5f;
      yaw_ = 3.14159265f;
      pitch_ = 0.1f;
      dist_ = std::max(120.0f, extent * 2.0f);
      return;
    }
    // An arena: looking at the ring from the hard camera's side, high up.
    target_[0] = 0;
    target_[1] = -100;
    target_[2] = 0;
    yaw_ = 0.0f;
    pitch_ = 0.35f;
    dist_ = std::max(1500.0f, std::min(6000.0f, (hi_[0] - lo_[0]) * 0.45f));
  }

  // Mouse: left drag turns around, right drag moves, wheel zooms.
  void Orbit(float dx, float dy) {
    yaw_ += dx * 0.008f;
    pitch_ = std::clamp(pitch_ + dy * 0.008f, -1.45f, 1.45f);
  }
  void Zoom(float steps) { dist_ = std::clamp(dist_ * std::pow(0.88f, steps), 50.0f, 40000.0f); }
  void Pan(float dx, float dy) {
    const float s = dist_ * 0.0015f, c = std::cos(yaw_), n = std::sin(yaw_);
    target_[0] -= (c * dx) * s;
    target_[2] -= (-n * dx) * s;
    target_[1] += dy * s;
  }
  void SetAngles(float yaw, float pitch, float dist) {
    yaw_ = yaw;
    pitch_ = pitch;
    dist_ = dist;
  }

  // Draws into a w x h picture; its shader view (for ImGui::Image).
  ID3D11ShaderResourceView *Render(int w, int h) {
    if (!ctx_ || w < 16 || h < 16)
      return nullptr;
    if (w != rt_w_ || h != rt_h_)
      CreateTargets(w, h);
    const float clear[4] = {0.015f, 0.015f, 0.02f, 1.0f};  // (an arena in the dark, as in the game)
    ctx_->ClearRenderTargetView(rtv_.Get(), clear);
    ctx_->ClearDepthStencilView(dsv_.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    ID3D11RenderTargetView *rtvs[] = {rtv_.Get()};
    ctx_->OMSetRenderTargets(1, rtvs, dsv_.Get());
    D3D11_VIEWPORT vp{0, 0, float(w), float(h), 0, 1};
    ctx_->RSSetViewports(1, &vp);
    ctx_->IASetInputLayout(layout_.Get());
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx_->VSSetShader(vs_.Get(), nullptr, 0);
    ctx_->PSSetShader(ps_.Get(), nullptr, 0);
    ctx_->RSSetState(raster_.Get());
    ID3D11SamplerState *samp[] = {sampler_.Get()};
    ctx_->PSSetSamplers(0, 1, samp);

    float view_proj[16];
    ViewProj(float(w) / float(h), view_proj);
    // Two passes: solid, then additive (glows) without depth writes.
    for (int pass = 0; pass < 2; ++pass) {
      ctx_->OMSetBlendState(pass ? blend_add_.Get() : blend_alpha_.Get(), nullptr, 0xFFFFFFFF);
      ctx_->OMSetDepthStencilState(pass ? depth_read_.Get() : depth_write_.Get(), 0);
      for (const auto &d : draws_) {
        if (d.flags.additive != (pass == 1))
          continue;
        if (d.skel >= 0 && skels_[size_t(d.skel)].person != 0 && !show_others)
          continue;
        if (show_entrance ? d.flags.match_only : d.flags.entrance_only)
          continue;
        for (int mirror = 0; mirror < (d.flags.mirrored ? 2 : 1); ++mirror) {
          Constants c{};
          memcpy(c.view_proj, view_proj, sizeof(view_proj));
          c.mirror = mirror ? -1.0f : 1.0f;
          c.additive = d.flags.additive ? 1.0f : 0.0f;
          c.textured = d.tex ? 1.0f : 0.0f;
          c.baked = d.baked_light ? 1.0f : 0.0f;
          c.opaque = d.opaque ? 1.0f : 0.0f;
          ctx_->UpdateSubresource(cb_.Get(), 0, nullptr, &c, 0, 0);
          ID3D11Buffer *cbs[] = {cb_.Get()};
          ctx_->VSSetConstantBuffers(0, 1, cbs);
          ctx_->PSSetConstantBuffers(0, 1, cbs);
          ID3D11ShaderResourceView *srv[] = {d.tex};
          ctx_->PSSetShaderResources(0, 1, srv);
          const UINT stride = sizeof(Vtx), offset = 0;
          ID3D11Buffer *vb[] = {d.vb.Get()};
          ctx_->IASetVertexBuffers(0, 1, vb, &stride, &offset);
          ctx_->IASetIndexBuffer(d.ib.Get(), DXGI_FORMAT_R32_UINT, 0);
          ctx_->DrawIndexed(d.count, 0, 0);
        }
      }
    }
    ID3D11ShaderResourceView *none[] = {nullptr};
    ctx_->PSSetShaderResources(0, 1, none);
    ID3D11RenderTargetView *no_rtv[] = {nullptr};
    ctx_->OMSetRenderTargets(1, no_rtv, nullptr);
    return srv_.Get();
  }

  // The last picture as RGBA (tests, "save a picture").
  bool ReadBack(svrfmt::Image &img) {
    if (!color_)
      return false;
    D3D11_TEXTURE2D_DESC td;
    color_->GetDesc(&td);
    td.Usage = D3D11_USAGE_STAGING;
    td.BindFlags = 0;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    td.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device_->CreateTexture2D(&td, nullptr, &staging)))
      return false;
    ctx_->CopyResource(staging.Get(), color_.Get());
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(ctx_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &m)))
      return false;
    img.w = int(td.Width);
    img.h = int(td.Height);
    img.rgba.resize(size_t(img.w) * img.h * 4);
    for (int y = 0; y < img.h; ++y)
      memcpy(&img.rgba[size_t(y) * img.w * 4], static_cast<uint8_t *>(m.pData) + size_t(y) * m.RowPitch,
             size_t(img.w) * 4);
    ctx_->Unmap(staging.Get(), 0);
    return true;
  }

  // The skinned models posed: bones not in `pose` keep their bind pose (an empty pose = the bind pose).
  bool Posable() const { return !skels_.empty(); }
  void SetPose(const std::map<std::string, BonePose> &pose, int person = 0) {
    std::vector<std::vector<M34>> skin(skels_.size());
    bool first = true;
    for (size_t i = 0; i < skels_.size(); ++i) {
      if (skels_[i].person != person)
        continue;
      const auto world = skels_[i].World(pose);
      if (person == 0 && std::exchange(first, false))
        for (size_t b = 0; b < world.size(); ++b)
          if (skels_[i].names[b] == "Bip")
            for (int c = 0; c < 3; ++c)
              hips_[c] = world[b].m[c][3];
      skin[i].resize(world.size());
      for (size_t b = 0; b < world.size(); ++b)
        skin[i][b] = MulM(world[b], skels_[i].inv_bind[b]);
    }
    std::vector<Vtx> v;
    for (auto &d : draws_) {
      if (d.skel < 0 || !d.vb_dynamic || skels_[size_t(d.skel)].person != person)
        continue;
      const auto &S = skin[size_t(d.skel)];
      v = d.base;
      for (size_t k = 0; k < v.size(); ++k) {
        float p[3] = {0, 0, 0}, n[3] = {0, 0, 0}, wsum = 0;
        for (int j = 0; j < d.per; ++j) {
          const int node = d.inode[k * size_t(d.per) + size_t(j)];
          const float w = d.iw[k * size_t(d.per) + size_t(j)];
          if (node < 0 || size_t(node) >= S.size() || w == 0)
            continue;
          const M34 &m = S[size_t(node)];
          const float *a = d.base[k].pos, *b = d.base[k].normal;
          for (int r = 0; r < 3; ++r) {
            p[r] += w * (m.m[r][0] * a[0] + m.m[r][1] * a[1] + m.m[r][2] * a[2] + m.m[r][3]);
            n[r] += w * (m.m[r][0] * b[0] + m.m[r][1] * b[1] + m.m[r][2] * b[2]);
          }
          wsum += w;
        }
        if (wsum > 0)
          for (int r = 0; r < 3; ++r)
            v[k].pos[r] = p[r] / wsum, v[k].normal[r] = n[r] / wsum;
      }
      D3D11_MAPPED_SUBRESOURCE mp;
      if (SUCCEEDED(ctx_->Map(d.vb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mp))) {
        memcpy(mp.pData, v.data(), v.size() * sizeof(Vtx));
        ctx_->Unmap(d.vb.Get(), 0);
      }
    }
  }

  // The hips (bone "Bip") as last posed; the camera looking at a point.
  const float *Hips() const { return hips_; }
  void SetTarget(const float t[3]) {
    for (int c = 0; c < 3; ++c)
      target_[c] = t[c];
  }

  size_t Models() const { return models_; }
  size_t Triangles() const { return triangles_; }

 private:
  struct Constants {
    float view_proj[16];
    float mirror, additive, textured, baked;
    float opaque, pad[3];
  };
  // 3x4 affine matrices, column vectors (p' = m p).
  struct M34 {
    float m[3][4] = {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}};
  };
  static M34 MulM(const M34 &a, const M34 &b) {
    M34 o;
    for (int r = 0; r < 3; ++r)
      for (int c = 0; c < 4; ++c)
        o.m[r][c] = a.m[r][0] * b.m[0][c] + a.m[r][1] * b.m[1][c] + a.m[r][2] * b.m[2][c] + (c == 3 ? a.m[r][3] : 0);
    return o;
  }
  static M34 Inverse(const M34 &a) {  // (rotation and translation: a skeleton has no scale)
    M34 o;
    for (int r = 0; r < 3; ++r)
      for (int c = 0; c < 3; ++c)
        o.m[r][c] = a.m[c][r];
    for (int r = 0; r < 3; ++r)
      o.m[r][3] = -(o.m[r][0] * a.m[0][3] + o.m[r][1] * a.m[1][3] + o.m[r][2] * a.m[2][3]);
    return o;
  }
  static M34 Trs(const yanm::V &t, const yanm::Q &q) {
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    M34 o;
    o.m[0][0] = 1 - 2 * (y * y + z * z), o.m[0][1] = 2 * (x * y - z * w), o.m[0][2] = 2 * (x * z + y * w);
    o.m[1][0] = 2 * (x * y + z * w), o.m[1][1] = 1 - 2 * (x * x + z * z), o.m[1][2] = 2 * (y * z - x * w);
    o.m[2][0] = 2 * (x * z - y * w), o.m[2][1] = 2 * (y * z + x * w), o.m[2][2] = 1 - 2 * (x * x + y * y);
    o.m[0][3] = t[0], o.m[1][3] = t[1], o.m[2][3] = t[2];
    return o;
  }
  struct Skel {
    int person = 0;
    std::vector<std::string> names;
    std::vector<int> parent;
    std::vector<yanm::V> t;
    std::vector<yanm::Q> r;
    std::vector<M34> inv_bind;
    std::vector<M34> World(const std::map<std::string, BonePose> &pose) const {
      std::vector<M34> w(names.size());
      std::vector<char> done(names.size(), 0);
      for (size_t i = 0; i < names.size(); ++i)
        Fill(i, pose, w, done);
      return w;
    }
    void Fill(size_t i, const std::map<std::string, BonePose> &pose, std::vector<M34> &w, std::vector<char> &done) const {
      if (done[i])
        return;
      done[i] = 1;  // (set first: a broken table with a loop stops here)
      yanm::V tt = t[i];
      yanm::Q rr = r[i];
      if (const auto it = pose.find(names[i]); it != pose.end()) {
        if (it->second.has_rot)
          rr = it->second.rot;
        if (it->second.has_pos)
          for (int c = 0; c < 3; ++c)
            tt[size_t(c)] += it->second.pos[size_t(c)];
      }
      const M34 local = Trs(tt, rr);
      const int p = parent[i];
      if (p >= 0 && size_t(p) < names.size()) {
        Fill(size_t(p), pose, w, done);
        w[i] = MulM(w[size_t(p)], local);
      } else {
        w[i] = local;
      }
    }
  };
  std::vector<Skel> skels_;
  float hips_[3] = {0, -100, 0};
  std::string tex_prefix_ = "0:";

  struct Draw {
    int skel = -1;               // its model's skeleton (keep_skin)
    bool vb_dynamic = false;
    std::vector<Vtx> base;       // the bind-pose vertices
    int per = 0;                 // influences a vertex
    std::vector<int> inode;      // per vertex x per: node index (-1 none)
    std::vector<float> iw;       // and its weight
    ComPtr<ID3D11Buffer> vb, ib;
    UINT count = 0;
    ID3D11ShaderResourceView *tex = nullptr;  // owned by textures_
    Flags flags;
    bool baked_light = true;
    bool opaque = false;  // no alpha test (superstars' skin and gear: their pictures' alpha is something else)
  };

  void ViewProj(float aspect, float out[16]) const {
    // Arena space has y down: the camera's "up" is -y.
    const float cp = std::cos(pitch_), sp = std::sin(pitch_), cy = std::cos(yaw_), sy = std::sin(yaw_);
    const float eye[3] = {target_[0] + dist_ * cp * sy, target_[1] - dist_ * sp, target_[2] + dist_ * cp * cy};
    float f[3] = {target_[0] - eye[0], target_[1] - eye[1], target_[2] - eye[2]};
    Normalize(f);
    const float up[3] = {0, -1, 0};
    float r[3] = {f[1] * up[2] - f[2] * up[1], f[2] * up[0] - f[0] * up[2], f[0] * up[1] - f[1] * up[0]};
    Normalize(r);
    const float u[3] = {r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0]};
    // View (row-vector convention, left-handed: z forward).
    const float v[16] = {r[0], u[0], f[0], 0, r[1], u[1], f[1], 0, r[2], u[2], f[2], 0,
                         -Dot(r, eye), -Dot(u, eye), -Dot(f, eye), 1};
    const float zn = 10.0f, zf = 60000.0f, fov = 0.9f, ys = 1.0f / std::tan(fov * 0.5f), xs = ys / aspect;
    const float p[16] = {xs, 0, 0, 0, 0, ys, 0, 0, 0, 0, zf / (zf - zn), 1, 0, 0, -zn * zf / (zf - zn), 0};
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j) {
        float s = 0;
        for (int k = 0; k < 4; ++k)
          s += v[i * 4 + k] * p[k * 4 + j];
        out[i * 4 + j] = s;
      }
  }
  static std::string Lower(std::string s) {
    for (auto &ch : s)
      ch = char(tolower(static_cast<unsigned char>(ch)));
    return s;
  }
  static float Dot(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
  static void Normalize(float a[3]) {
    const float l = std::sqrt(Dot(a, a));
    if (l > 0)
      a[0] /= l, a[1] /= l, a[2] /= l;
  }

  // `cut`: a picture whose alpha multiplies this one's (the superstars' eyelashes and hair: see AddMesh).
  ID3D11ShaderResourceView *Texture(const std::string &name, const std::map<std::string, Bytes> &dds,
                                    const std::string &cut = {}) {
    const std::string key = tex_prefix_ + name + (cut.empty() ? "" : "|" + cut);  // (two superstars: two "color")
    auto it = textures_.find(key);
    if (it != textures_.end())
      return it->second.Get();
    ComPtr<ID3D11ShaderResourceView> srv;
    svrfmt::Image img, ci;
    const auto d = dds.find(Lower(name));
    const auto c = cut.empty() ? dds.end() : dds.find(Lower(cut));
    if (d != dds.end() && svrfmt::DdsDecode(d->second, img) && img.w > 0 && c != dds.end() &&
        svrfmt::DdsDecode(c->second, ci) && ci.w > 0)
      for (int y = 0; y < img.h; ++y)
        for (int x = 0; x < img.w; ++x) {
          uint8_t &a = img.rgba[(size_t(y) * img.w + x) * 4 + 3];
          const uint8_t b = ci.rgba[(size_t(y * ci.h / img.h) * ci.w + size_t(x * ci.w / img.w)) * 4 + 3];
          a = uint8_t(a * b / 255);
        }
    if (d != dds.end() && img.w > 0) {
      D3D11_TEXTURE2D_DESC td{};
      td.Width = UINT(img.w);
      td.Height = UINT(img.h);
      td.MipLevels = 0;  // full chain, made by the GPU
      td.ArraySize = 1;
      td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      td.SampleDesc.Count = 1;
      td.Usage = D3D11_USAGE_DEFAULT;
      td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
      td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
      ComPtr<ID3D11Texture2D> tex;
      if (SUCCEEDED(device_->CreateTexture2D(&td, nullptr, &tex))) {
        ctx_->UpdateSubresource(tex.Get(), 0, nullptr, img.rgba.data(), UINT(img.w * 4), 0);
        if (SUCCEEDED(device_->CreateShaderResourceView(tex.Get(), nullptr, &srv)))
          ctx_->GenerateMips(srv.Get());
      }
    }
    textures_[key] = srv;
    return srv.Get();
  }

  void AddMesh(uint32_t id, const svrfmt::Model &m, const svrfmt::Mesh &s, const Flags &f,
               const std::map<std::string, Bytes> &dds, int skel = -1) {
    if (s.verts.empty())
      return;
    std::vector<Vtx> v(s.verts.size());
    for (size_t i = 0; i < v.size(); ++i) {
      const auto &x = s.verts[i];
      memcpy(v[i].pos, x.pos, 12);
      memcpy(v[i].normal, x.normal, 12);
      v[i].uv[0] = s.uvs[i][0];
      v[i].uv[1] = s.uvs[i][1];
      const uint32_t c = x.color;  // ARGB -> RGBA8 (bytes R, G, B, A)
      v[i].color = ((c >> 16) & 0xFF) | (((c >> 8) & 0xFF) << 8) | ((c & 0xFF) << 16) | ((c >> 24) << 24);
      for (int k = 0; k < 3; ++k) {
        lo_[k] = std::min(lo_[k], x.pos[k]);
        hi_[k] = std::max(hi_[k], x.pos[k]);
      }
    }
    std::vector<uint32_t> idx;
    for (const auto &st : s.strips) {
      if (st.IsStrip()) {
        for (const auto &t : svrfmt::StripToTriangles(st.indices))
          if (t[0] < v.size() && t[1] < v.size() && t[2] < v.size())
            idx.insert(idx.end(), {t[0], t[1], t[2]});
      } else {  // a triangle list
        for (size_t k = 0; k + 2 < st.indices.size(); k += 3)
          idx.insert(idx.end(), {st.indices[k], st.indices[k + 1], st.indices[k + 2]});
      }
    }
    if (idx.empty())
      return;
    std::string tex, cut;
    for (const auto &p : s.params)
      if ((p.name == "sampDiffusePrm" || p.name == "texDiffuse" || p.name == "g_mDiffusePrm") && p.value.size() >= 4) {
        const int32_t slot = int32_t(svrfmt::Be32(p.value.data()));
        if (slot >= 0 && size_t(slot) < m.textures.size())
          tex = m.textures[size_t(slot)];
      }
    // The eyelashes and hair (yCh_trans, yCh_Aniso_trans) are cut out by their normal map's alpha too: WWE '12's
    // colour is opaque there and the strands are in the normal map's alpha (as WWE 2K16's: Paige's eyelashes showed
    // as black patches, 2026-10-31; the game's hair shader, RenderDoc 2026-10-26).
    if (s.shader.find("trans") != std::string::npos)
      for (const auto &p : s.params)
        if (p.name == "g_mNormalFxPrm" && p.value.size() >= 4) {
          const int32_t slot = int32_t(svrfmt::Be32(p.value.data()));
          if (slot >= 0 && size_t(slot) < m.textures.size())
            cut = m.textures[size_t(slot)];
        }
    Draw d;
    d.flags = f;
    // Superstars' shaders (yCh_*) light them in the game: their vertex colours aren't baked light.
    d.baked_light = s.shader.rfind("yCh", 0) != 0;
    // (Only for pictures with a full alpha channel: a DXT1 colour picture's 1-bit alpha may cut real holes.)
    if (s.shader == "yCh_skin" || s.shader == "yCh_skin_flex" || s.shader == "yCh_gear" || s.shader == "yCh_Reflect") {
      const auto it = dds.find(Lower(tex));
      svrfmt::DdsInfo info;
      d.opaque = it != dds.end() && svrfmt::DdsInfoOf(it->second, info) && info.format != svrfmt::DxtFormat::kDxt1;
    }
    d.count = UINT(idx.size());
    d.tex = tex.empty() ? nullptr : Texture(tex, dds, cut);
    if (skel >= 0) {  // the vertices' bones: node = palette[slot] - 1
      d.skel = skel;
      d.vb_dynamic = true;
      d.base = v;
      d.per = std::max(1, int(s.weights.size()));
      d.inode.assign(v.size() * size_t(d.per), -1);
      d.iw.assign(v.size() * size_t(d.per), 0.0f);
      for (size_t k = 0; k < v.size(); ++k) {
        if (s.weights.empty()) {
          d.inode[k] = s.palette.empty() ? -1 : s.palette[0] - 1;
          d.iw[k] = 1;
          continue;
        }
        for (size_t j = 0; j < s.weights.size(); ++j) {
          const auto &w = s.weights[j][k];
          if (w.bones[0] == 255 || w.bones[0] >= s.palette.size())
            continue;
          d.inode[k * size_t(d.per) + j] = s.palette[w.bones[0]] - 1;
          d.iw[k * size_t(d.per) + j] = w.weight;
        }
      }
    }
    D3D11_BUFFER_DESC bd{};
    bd.Usage = d.vb_dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE;
    bd.CPUAccessFlags = d.vb_dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.ByteWidth = UINT(v.size() * sizeof(Vtx));
    D3D11_SUBRESOURCE_DATA init{v.data(), 0, 0};
    if (FAILED(device_->CreateBuffer(&bd, &init, &d.vb)))
      return;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.CPUAccessFlags = 0;
    bd.ByteWidth = UINT(idx.size() * 4);
    init.pSysMem = idx.data();
    if (FAILED(device_->CreateBuffer(&bd, &init, &d.ib)))
      return;
    if (draws_.empty() || last_id_ != id)
      ++models_;
    last_id_ = id;
    triangles_ += idx.size() / 3;
    draws_.push_back(std::move(d));
  }

  bool CreatePipeline(std::string *error) {
    if (vs_)
      return true;
    static const char kShader[] = R"(
cbuffer C : register(b0) { row_major float4x4 view_proj; float mirror; float additive; float textured; float baked;
                           float opaque; float3 pad; };
Texture2D tex : register(t0);
SamplerState samp : register(s0);
struct VI { float3 pos : POSITION; float3 n : NORMAL; float2 uv : TEXCOORD; float4 col : COLOR; };
struct VO { float4 pos : SV_Position; float3 n : NORMAL; float2 uv : TEXCOORD; float4 col : COLOR; };
VO vs(VI i) {
  VO o;
  float3 p = i.pos; p.x *= mirror;
  o.pos = mul(float4(p, 1), view_proj);
  o.n = float3(i.n.x * mirror, i.n.y, i.n.z);
  o.uv = i.uv; o.col = i.col;
  return o;
}
float4 ps(VO i) : SV_Target {
  // No picture: the video screens (the game draws its videos there): a dark screen.
  float4 c = textured > 0.5 ? tex.Sample(samp, i.uv) : float4(0.05, 0.07, 0.12, 1);
  if (additive > 0.5) return float4(c.rgb * c.a * i.col.rgb, 1);
  if (opaque < 0.5) clip(c.a - 0.4);
  // The vertex colours are the arena's baked light (dark floors and stands, white for lit props), as in the game;
  // a little shading from above keeps shapes readable.
  float3 l = normalize(float3(0.3, -1.0, 0.45));  // (y is down)
  float d = abs(dot(normalize(i.n), l));
  if (baked < 0.5) return float4(c.rgb * (0.45 + 0.65 * d), 1);  // (a superstar: a light from above)
  float shade = 0.8 + 0.2 * saturate(d);
  return float4(c.rgb * i.col.rgb * shade, 1);
})";
    ComPtr<ID3DBlob> vsb, psb, err;
    if (FAILED(D3DCompile(kShader, sizeof(kShader) - 1, "arena_view", nullptr, nullptr, "vs", "vs_4_0", 0, 0, &vsb,
                          &err)) ||
        FAILED(D3DCompile(kShader, sizeof(kShader) - 1, "arena_view", nullptr, nullptr, "ps", "ps_4_0", 0, 0, &psb,
                          &err))) {
      if (error)
        *error = err ? std::string(static_cast<const char *>(err->GetBufferPointer()), err->GetBufferSize())
                     : "shader compile failed";
      return false;
    }
    device_->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &vs_);
    device_->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &ps_);
    const D3D11_INPUT_ELEMENT_DESC il[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    device_->CreateInputLayout(il, 4, vsb->GetBufferPointer(), vsb->GetBufferSize(), &layout_);
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = sizeof(Constants);
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    device_->CreateBuffer(&bd, nullptr, &cb_);
    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;  // (mirrored copies turn the winding round)
    rd.DepthClipEnable = TRUE;
    device_->CreateRasterizerState(&rd, &raster_);
    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_ANISOTROPIC;
    sd.MaxAnisotropy = 8;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    device_->CreateSamplerState(&sd, &sampler_);
    D3D11_BLEND_DESC bl{};
    bl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device_->CreateBlendState(&bl, &blend_alpha_);
    bl.RenderTarget[0].BlendEnable = TRUE;
    bl.RenderTarget[0].SrcBlend = bl.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    bl.RenderTarget[0].DestBlend = bl.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    bl.RenderTarget[0].BlendOp = bl.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    device_->CreateBlendState(&bl, &blend_add_);
    D3D11_DEPTH_STENCIL_DESC ds{};
    ds.DepthEnable = TRUE;
    ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    ds.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    device_->CreateDepthStencilState(&ds, &depth_write_);
    ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    device_->CreateDepthStencilState(&ds, &depth_read_);
    return true;
  }

  void CreateTargets(int w, int h) {
    rt_w_ = w;
    rt_h_ = h;
    color_.Reset();
    rtv_.Reset();
    srv_.Reset();
    dsv_.Reset();
    D3D11_TEXTURE2D_DESC td{};
    td.Width = UINT(w);
    td.Height = UINT(h);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    device_->CreateTexture2D(&td, nullptr, &color_);
    device_->CreateRenderTargetView(color_.Get(), nullptr, &rtv_);
    device_->CreateShaderResourceView(color_.Get(), nullptr, &srv_);
    td.Format = DXGI_FORMAT_D32_FLOAT;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    ComPtr<ID3D11Texture2D> depth;
    device_->CreateTexture2D(&td, nullptr, &depth);
    device_->CreateDepthStencilView(depth.Get(), nullptr, &dsv_);
  }

  ID3D11Device *device_ = nullptr;
  ComPtr<ID3D11DeviceContext> ctx_;
  ComPtr<ID3D11VertexShader> vs_;
  ComPtr<ID3D11PixelShader> ps_;
  ComPtr<ID3D11InputLayout> layout_;
  ComPtr<ID3D11Buffer> cb_;
  ComPtr<ID3D11RasterizerState> raster_;
  ComPtr<ID3D11SamplerState> sampler_;
  ComPtr<ID3D11BlendState> blend_alpha_, blend_add_;
  ComPtr<ID3D11DepthStencilState> depth_write_, depth_read_;
  ComPtr<ID3D11Texture2D> color_;
  ComPtr<ID3D11RenderTargetView> rtv_;
  ComPtr<ID3D11ShaderResourceView> srv_;
  ComPtr<ID3D11DepthStencilView> dsv_;
  int rt_w_ = 0, rt_h_ = 0;
  std::vector<Draw> draws_;
  std::map<std::string, ComPtr<ID3D11ShaderResourceView>> textures_;
  float target_[3] = {}, yaw_ = 0, pitch_ = 0.3f, dist_ = 3000;
  float lo_[3] = {}, hi_[3] = {};
  size_t models_ = 0, triangles_ = 0;
  uint32_t last_id_ = 0;
};

}  // namespace arena_view
