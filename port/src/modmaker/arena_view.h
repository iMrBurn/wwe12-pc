// Mod Maker: an arena in 3D (Direct3D 11, drawn into a picture the page shows). Its models (JBOY entries of the
// arena's PACH, read with Kaiko's svrfmt/jboy) are drawn where they are stored: WWE '12 arena models hold their
// vertices in arena space, in cm, y pointing down. The flag table (entry 50001, one line per model id, see the
// Japanese help at its end) says how the game draws each: "r" also mirrored across x = 0, "a" additive, "m(n)"
// placed by an animation (light rigs, beams: left out, they sit at the centre until the game moves them), "e"
// entrance only, "g" match only. Pictures come from the arena's texture packages (sampDiffusePrm = the slot in the
// model's texture list); light maps and shader effects are not drawn: it's a preview, not the game's look.
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
#include <vector>

#include "svrfmt/archive.h"
#include "svrfmt/jboy.h"
#include "svrfmt/texture.h"

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

class View {
 public:
  bool show_entrance = false;  // entrance-only models (stage, ramp lights...) instead of match-only ones

  // Reads every model of an arena file and uploads it. False (and why) if nothing could be read.
  bool Load(ID3D11Device *dev, const Bytes &file, std::string *error) {
    device_ = dev;
    dev->GetImmediateContext(&ctx_);
    draws_.clear();
    textures_.clear();
    if (!CreatePipeline(error))
      return false;
    svrfmt::Archive a;
    if (!svrfmt::ArchiveOpen(file, a, error) || a.root.kids.empty())
      return false;
    svrfmt::ArcNode &stg = a.root.kids[0];
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
            dds[x.name] = std::move(x.data);
      }
    }
    lo_[0] = lo_[1] = lo_[2] = 1e30f;
    hi_[0] = hi_[1] = hi_[2] = -1e30f;
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
      for (const auto &s : m.meshes)
        AddMesh(k.id, m, s, f, dds);
    }
    if (draws_.empty()) {
      if (error)
        *error = "no models";
      return false;
    }
    ResetCamera();
    return true;
  }

  void ResetCamera() {
    // Looking at the ring from the hard camera's side, high up.
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
    const float clear[4] = {0.08f, 0.08f, 0.09f, 1.0f};
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
        if (show_entrance ? d.flags.match_only : d.flags.entrance_only)
          continue;
        for (int mirror = 0; mirror < (d.flags.mirrored ? 2 : 1); ++mirror) {
          Constants c{};
          memcpy(c.view_proj, view_proj, sizeof(view_proj));
          c.mirror = mirror ? -1.0f : 1.0f;
          c.additive = d.flags.additive ? 1.0f : 0.0f;
          c.textured = d.tex ? 1.0f : 0.0f;
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

  size_t Models() const { return models_; }
  size_t Triangles() const { return triangles_; }

 private:
  struct Constants {
    float view_proj[16];
    float mirror, additive, textured, pad;
  };
  struct Draw {
    ComPtr<ID3D11Buffer> vb, ib;
    UINT count = 0;
    ID3D11ShaderResourceView *tex = nullptr;  // owned by textures_
    Flags flags;
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
  static float Dot(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
  static void Normalize(float a[3]) {
    const float l = std::sqrt(Dot(a, a));
    if (l > 0)
      a[0] /= l, a[1] /= l, a[2] /= l;
  }

  ID3D11ShaderResourceView *Texture(const std::string &name, const std::map<std::string, Bytes> &dds) {
    auto it = textures_.find(name);
    if (it != textures_.end())
      return it->second.Get();
    ComPtr<ID3D11ShaderResourceView> srv;
    svrfmt::Image img;
    const auto d = dds.find(name);
    if (d != dds.end() && svrfmt::DdsDecode(d->second, img) && img.w > 0) {
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
    textures_[name] = srv;
    return srv.Get();
  }

  void AddMesh(uint32_t id, const svrfmt::Model &m, const svrfmt::Mesh &s, const Flags &f,
               const std::map<std::string, Bytes> &dds) {
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
      if (st.prim == 6) {
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
    std::string tex;
    for (const auto &p : s.params)
      if ((p.name == "sampDiffusePrm" || p.name == "texDiffuse") && p.value.size() >= 4) {
        const int32_t slot = int32_t(svrfmt::Be32(p.value.data()));
        if (slot >= 0 && size_t(slot) < m.textures.size())
          tex = m.textures[size_t(slot)];
      }
    Draw d;
    d.flags = f;
    d.count = UINT(idx.size());
    d.tex = tex.empty() ? nullptr : Texture(tex, dds);
    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.ByteWidth = UINT(v.size() * sizeof(Vtx));
    D3D11_SUBRESOURCE_DATA init{v.data(), 0, 0};
    if (FAILED(device_->CreateBuffer(&bd, &init, &d.vb)))
      return;
    bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
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
cbuffer C : register(b0) { row_major float4x4 view_proj; float mirror; float additive; float textured; float pad; };
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
  float4 c = textured > 0.5 ? tex.Sample(samp, i.uv) : float4(0.6, 0.6, 0.62, 1);
  if (additive > 0.5) return float4(c.rgb * c.a, 1);
  clip(c.a - 0.4);
  float3 l = normalize(float3(0.3, -1.0, 0.45));  // light from above (y is down)
  float shade = 0.55 + 0.45 * saturate(abs(dot(normalize(i.n), l)));
  return float4(c.rgb * shade, 1);
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
