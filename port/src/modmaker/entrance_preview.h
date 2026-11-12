// Mod Maker: the full preview of an entrance (a window, as the "Full preview" of Kaiko's SvR 2011 Mod Maker,
// port/modmaker/full_preview.cpp, MIT, with credit): a ported entrance (WWE '12's format, entrance13.h) played in 3D
// on a WWE '12 superstar's model, its parts one after another, with Pause, Restart, Skip to the ring and a time bar.
//
// An entrance n: its script EVP n (objects, 32 bytes each from +0x48 + 8: +8 / +12 the type, the entrant 01000000 0,
// the camera 00010000 0) and its tracks EVZ n (a PACH, a YANM record per object, the leaf's id = the object's place in
// the script). A record has a block per part of the entrance and per take, named "PPPPYYYY" (PPPP the part: 0000,
// 0005, 0010...; YYYY 0000 the body, a camera has two takes 0000 / 0001, the game plays one of them at random). Part 0
// is the hidden intro (the titantron plays, the superstar comes out after it). Tracks are 60 frames a second, positions
// in cm (y down) in the arena's space but with the ring mat at y 0 (moved 1.2 m up into the arena: cam_y_ / body_y_).
// The camera: eye #5edfdf5f, target #70d146a5 (both positions), field of view #70d304a4 (its third value, vertical).
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <fstream>
#include <future>

#include "arena_page.h"
#include "arena_view.h"
#include "audio_out.h"
#include "entrance_screens.h"
#include "media.h"
#include "theme_convert.h"
#include "theme_source.h"
#include "video/bink2_decode.h"
#include "entrance13.h"
#include "game_disc.h"
#include "imgui.h"
#include "svrfmt/archive.h"
#include "svrfmt/pac.h"
#include "wic_image.h"
#include "yanm.h"

namespace entrance_preview {

using Bytes = std::vector<uint8_t>;

struct Star {
  int id = 0;
  std::string name;
};

// A titantron movie: Bink 1 (WWE '12, SvR: bink_decode) or Bink 2 (WWE '13, the 2K games: our bink2_decode).
class Video {
 public:
  Video() = default;
  Video(const Video &) = delete;
  Video &operator=(const Video &) = delete;
  ~Video() { Close(); }
  bool Open(const Bytes &data) {
    Close();
    WCHAR err[256];
    if (data.size() > 4 && !memcmp(data.data(), "KB2", 3)) {
      b2_ = bink2_open_memory(data.data(), data.size(), err, 256);
    } else if (data.size() > 4 && !memcmp(data.data(), "BIK", 3)) {  // (Bink 1 reads a file)
      const std::filesystem::path p = media::TempDir() / L"entrance_preview.bik";
      std::ofstream(p, std::ios::binary).write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));
      b1_ = bink_open(p.c_str(), err, 256);
    }
    return On();
  }
  void Close() {
    if (b1_)
      bink_close(b1_), b1_ = nullptr;
    if (b2_)
      bink2_close(b2_), b2_ = nullptr;
  }
  bool On() const { return b1_ || b2_; }
  int W() const { return b1_ ? bink_width(b1_) : b2_ ? bink2_width(b2_) : 0; }
  int H() const { return b1_ ? bink_height(b1_) : b2_ ? bink2_height(b2_) : 0; }
  int Frames() const { return b1_ ? bink_frames(b1_) : b2_ ? bink2_frames(b2_) : 0; }
  long long FrameTime() const { return b1_ ? bink_frame_time(b1_) : b2_ ? bink2_frame_time(b2_) : 0; }
  void Next() { b1_ ? void(bink_next(b1_)) : b2_ ? void(bink2_next(b2_)) : void(); }
  void Rewind() { b1_ ? bink_rewind(b1_) : b2_ ? bink2_rewind(b2_) : void(); }
  void Bgra(uint8_t *out) const { b1_ ? bink_bgra(b1_, out) : b2_ ? bink2_bgra(b2_, out) : void(); }

 private:
  BinkReader *b1_ = nullptr;
  Bink2Reader *b2_ = nullptr;
};

// The titantron's part of a movie laid out like WWE '12's (320 x 320: rows 0-218, v 0-0.684), in 1/256 of its height.
constexpr int kTitanPart = 175;

// A movie for a screen: its bytes (Bink 1 or 2), the rows of it used (in 1/256 of its height: 2K14's walls hold the
// ribbon below), what it is (shown on the window).
struct Clip {
  Bytes movie;
  int y0 = 0, y1 = 256;
  std::string name;
};
// A ported superstar's screens from his game (FromGame): his titantron (titan_2k: one 16:9 picture in the square, the
// 2K games'), the minitron put under it, his wall and his ribbon. Empty ones: the shown WWE '12 superstar's.
struct Sources {
  Clip titan, mini, wall, ribbon;
  bool titan_2k = false;
};

// A screen's picture: a movie (rows y0..y1) stretched to W x H, or laid along it (a ribbon); or, with a strip, the
// movie in the titantron's part (rows 0..218 of 320, v 0-0.684, WWE '12's layout) and the strip's movie under it.
class Screen {
 public:
  Screen() = default;
  Screen(const Screen &) = delete;
  Screen &operator=(const Screen &) = delete;
  bool Open(ID3D11Device *dev, const Clip &main, int W, int H, bool tile, const Clip *strip) {
    Close();
    if (!a_.v.Open(main.movie))
      return false;
    a_.y0 = main.y0, a_.y1 = main.y1;
    if (strip && !strip->movie.empty() && b_.v.Open(strip->movie))
      b_.y0 = strip->y0, b_.y1 = strip->y1;
    W_ = W, H_ = H, tile_ = tile, compose_ = strip != nullptr;
    name = main.name + (b_.v.On() ? " + " + strip->name : "");
    D3D11_TEXTURE2D_DESC td{};
    td.Width = UINT(W), td.Height = UINT(H), td.MipLevels = 1, td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &tex_)) || FAILED(dev->CreateShaderResourceView(tex_.Get(), nullptr, &srv_))) {
      Close();
      return false;
    }
    out_.assign(size_t(W) * H * 4, 0);
    return true;
  }
  void Close() {
    for (Src *s : {&a_, &b_})
      s->v.Close(), s->at = -1, s->y0 = 0, s->y1 = 256, s->px.clear();
    tex_.Reset(), srv_.Reset();
    name.clear();
  }
  bool On() const { return a_.v.On() && srv_; }
  ID3D11ShaderResourceView *Srv() const { return srv_.Get(); }
  int Frames() const { return a_.v.Frames(); }
  // Its picture at `seconds` into its movie (looping).
  void At(double seconds, ID3D11DeviceContext *ctx) {
    if (!On())
      return;
    const bool a = a_.Step(seconds), b = b_.v.On() && b_.Step(seconds);
    if (!a && !b)
      return;
    const int split = compose_ ? kTitanRows : H_;
    Put(a_, out_.data(), split, compose_ ? false : tile_);
    if (compose_) {
      if (b_.v.On())
        Put(b_, out_.data() + size_t(split) * W_ * 4, H_ - split, false);
      else
        std::fill(out_.begin() + ptrdiff_t(size_t(split) * W_ * 4), out_.end(), uint8_t(0));
    }
    ctx->UpdateSubresource(tex_.Get(), 0, nullptr, out_.data(), UINT(W_ * 4), 0);
  }
  std::string name;

 private:
  static constexpr int kTitanRows = 219;  // (320 x 0.684)
  struct Src {
    Video v;
    int y0 = 0, y1 = 256, at = -1;
    std::vector<uint8_t> px;
    // Decoded up to the frame at `seconds`; false if it is the one already there.
    bool Step(double seconds) {
      const double per = v.FrameTime() > 0 ? double(v.FrameTime()) / 1e7 : 1.0 / 30;
      const int frames = std::max(1, v.Frames()), want = int(seconds / per) % frames;
      if (want == at)
        return false;
      if (want < at || at < 0)
        v.Rewind(), at = -1;
      for (int k = 0; at < want && k < 120; ++k)  // (a long jump catches up over a few frames)
        v.Next(), ++at;
      px.resize(size_t(v.W()) * v.H() * 4);
      v.Bgra(px.data());
      return true;
    }
  };
  void Put(const Src &s, uint8_t *dst, int rows, bool tile) const {
    const int sw = s.v.W(), sh = s.v.H();
    if (sw <= 0 || sh <= 0 || s.px.empty() || rows <= 0)
      return;
    const int r0 = sh * s.y0 / 256, r1 = std::max(r0 + 1, std::min(sh, sh * s.y1 / 256));
    entrance_screens::detail::Place(s.px.data() + size_t(r0) * sw * 4, sw, r1 - r0, dst, W_, rows, tile);
  }
  Src a_, b_;
  int W_ = 0, H_ = 0;
  bool tile_ = false, compose_ = false;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> tex_;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv_;
  std::vector<uint8_t> out_;
};

// A ported superstar's screens from his game `title` (his number there `n`): titantron (SourceMovie), wall and ribbon
// (entrance_screens::Versions, as the mod gets them), and for a 2K titantron its minitron (2K14 evtmov MVSB, 2K15-2K17
// ss_mini).
inline Bytes SourceMovie(const disc::GameDisc &g, uint32_t title, int n, std::string *name);
inline Sources FromGame(const disc::GameDisc &g, uint32_t title, int n) {
  Sources s;
  if (n < 0)
    return s;
  s.titan.movie = SourceMovie(g, title, n, &s.titan.name);
  s.titan_2k = title == 0x545408B2 || title == 0x545408B6 || title == 0x315A07D2 || title == 0x545408BA;
  if (title == 0x545408B2)  // (2K14's is laid out like WWE '12's: its video on top, strips below - its part alone)
    s.titan.y1 = kTitanPart;
  const auto first = [](std::vector<entrance_screens::Version> v, Clip &c) {
    if (!v.empty())
      c.movie = std::move(v[0].movie), c.y0 = v[0].y0, c.y1 = v[0].y1, c.name = v[0].label;
  };
  first(entrance_screens::Versions(g, title, false, n), s.wall);
  first(entrance_screens::Versions(g, title, true, n), s.ribbon);
  if (s.titan_2k)
    first(entrance_screens::Versions(g, title, true, n, true), s.mini);
  return s;
}

// An arena's walkable heights outside the ring (its stage, its ramp): per 20 cm cell, the highest upward-facing
// surface between the floor and 2.2 m up (y 0 .. -220, y down), from its models as the 3D view draws them for the
// entrance (arena_view: flags 50001, "r" mirrored across x = 0, animated / match-only / haze left out). 0: the floor.
class Ground {
 public:
  static constexpr float kX0 = -2400, kZ0 = -2400, kCell = 20;
  static constexpr int kNx = 240, kNz = 480;  // x -24..24 m, z -24..72 m (the stages are at +z)
  bool Empty() const { return h_.empty(); }
  float At(float x, float z) const {
    if (h_.empty() || (std::fabs(x) < 450 && std::fabs(z) < 450))  // (the ring and its steps: the tracks' own)
      return 0;
    const int i = int((x - kX0) / kCell), j = int((z - kZ0) / kCell);
    return i >= 0 && j >= 0 && i < kNx && j < kNz ? h_[size_t(j) * kNx + size_t(i)] : 0.0f;
  }
  std::string screens;  // (test aid: its screen models, Build)
  void Build(const Bytes &file) {
    h_.clear();
    screens.clear();
    svrfmt::Archive a;
    if (!svrfmt::ArchiveOpen(file, a, nullptr) || a.root.kids.empty() || !svrfmt::ArchiveExpand(a.root.kids[0]))
      return;
    h_.assign(size_t(kNx) * kNz, 0.0f);
    std::map<uint32_t, arena_view::Flags> flags;
    for (auto &k : a.root.kids[0].kids)
      if (k.id == 50001)
        flags = arena_view::ReadFlags(svrfmt::ArchiveData(k));
    for (auto &k : a.root.kids[0].kids) {
      const Bytes data = svrfmt::ArchiveData(k);
      svrfmt::Model m;
      if (!svrfmt::IsJboy(data) || !svrfmt::JboyRead(data, m, nullptr))
        continue;
      const arena_view::Flags f = flags.count(k.id) ? flags[k.id] : arena_view::Flags{};
      if ((k.id >= 800 && k.id <= 899) || (k.id >= 950 && k.id <= 999)) {  // (test aid: screens, ring pieces)
        for (const auto &me : m.meshes) {  // (the ring's: their meshes, first vertices and how they're bound)
          if (k.id < 950)
            break;
          char b[256];
          snprintf(b, sizeof(b), "  mesh of %u: %zu verts, palette %zu, weights %zu", k.id, me.verts.size(),
                   me.palette.size(), me.weights.size());
          screens += b;
          for (size_t v = 0; v < me.verts.size() && v < 4; ++v) {
            snprintf(b, sizeof(b), " (%.1f %.1f %.1f", me.verts[v].pos[0], me.verts[v].pos[1], me.verts[v].pos[2]);
            screens += b;
            for (size_t j = 0; j < me.weights.size() && j < 2; ++j)
              if (v < me.weights[j].size()) {
                snprintf(b, sizeof(b), " b%d w%.2f", me.weights[j][v].bones[0], me.weights[j][v].weight);
                screens += b;
              }
            screens += ")";
          }
          if (!me.palette.empty()) {
            snprintf(b, sizeof(b), " pal0 %d", me.palette[0]);
            screens += b;
          }
          screens += "\n";
        }
        float lo[5] = {1e9f, 1e9f, 1e9f, 1e9f, 1e9f}, hi[5] = {-1e9f, -1e9f, -1e9f, -1e9f, -1e9f};
        for (const auto &me : m.meshes)
          for (size_t v = 0; v < me.verts.size(); ++v)
            for (int c = 0; c < 5; ++c) {
              const float x = c < 3 ? me.verts[v].pos[c] : v < me.uvs.size() ? me.uvs[v][size_t(c - 3)] : 0.0f;
              lo[c] = std::min(lo[c], x), hi[c] = std::max(hi[c], x);
            }
        char b[256];
        snprintf(b, sizeof(b), "screen %u%s %s: x %.0f..%.0f y %.0f..%.0f z %.0f..%.0f u %.3f..%.3f v %.3f..%.3f\n", k.id,
                 f.mirrored ? " (r)" : "", m.textures.empty() ? "-" : m.textures[0].c_str(), lo[0], hi[0], lo[1], hi[1],
                 lo[2], hi[2], lo[3], hi[3], lo[4], hi[4]);
        screens += b;
      }
      if (f.animated || f.match_only || f.haze || f.additive)
        continue;
      for (const auto &me : m.meshes)
        for (const auto &st : me.strips) {
          std::vector<std::array<uint16_t, 3>> tris;
          if (st.IsStrip())
            for (const auto &t : svrfmt::StripToTriangles(st.indices))
              tris.push_back({uint16_t(t[0]), uint16_t(t[1]), uint16_t(t[2])});
          else
            for (size_t q = 0; q + 2 < st.indices.size(); q += 3)
              tris.push_back({st.indices[q], st.indices[q + 1], st.indices[q + 2]});
          for (const auto &t : tris) {
            if (t[0] >= me.verts.size() || t[1] >= me.verts.size() || t[2] >= me.verts.size())
              continue;
            for (int mirror = 0; mirror < (f.mirrored ? 2 : 1); ++mirror) {
              float p[3][3];
              for (int v = 0; v < 3; ++v)
                for (int c = 0; c < 3; ++c)
                  p[v][c] = me.verts[t[size_t(v)]].pos[c] * (c == 0 && mirror ? -1.0f : 1.0f);
              Add(p);
            }
          }
        }
    }
  }

 private:
  void Add(const float p[3][3]) {
    const float ux = p[1][0] - p[0][0], uy = p[1][1] - p[0][1], uz = p[1][2] - p[0][2];
    const float vx = p[2][0] - p[0][0], vy = p[2][1] - p[0][1], vz = p[2][2] - p[0][2];
    const float nx = uy * vz - uz * vy, ny = uz * vx - ux * vz, nz = ux * vy - uy * vx;
    const float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (nl < 1e-6f || std::fabs(ny) / nl < 0.7f)  // (walls)
      return;
    for (int v = 0; v < 3; ++v)
      if (p[v][1] < -220.0f || p[v][1] > 5.0f)
        return;
    const float det = ux * vz - vx * uz;
    if (std::fabs(det) < 1e-6f)
      return;
    const float minx = std::min({p[0][0], p[1][0], p[2][0]}), maxx = std::max({p[0][0], p[1][0], p[2][0]});
    const float minz = std::min({p[0][2], p[1][2], p[2][2]}), maxz = std::max({p[0][2], p[1][2], p[2][2]});
    for (int j = std::max(0, int((minz - kZ0) / kCell)); j <= std::min(kNz - 1, int((maxz - kZ0) / kCell)); ++j)
      for (int i = std::max(0, int((minx - kX0) / kCell)); i <= std::min(kNx - 1, int((maxx - kX0) / kCell)); ++i) {
        const float cx = kX0 + (float(i) + 0.5f) * kCell, cz = kZ0 + (float(j) + 0.5f) * kCell;
        const float b1 = ((cx - p[0][0]) * vz - vx * (cz - p[0][2])) / det;
        const float b2 = (ux * (cz - p[0][2]) - (cx - p[0][0]) * uz) / det;
        if (b1 < 0 || b2 < 0 || b1 + b2 > 1)
          continue;
        float &h = h_[size_t(j) * kNx + size_t(i)];
        h = std::min(h, p[0][1] + b1 * uy + b2 * vy);  // (y down: the highest)
      }
  }
  std::vector<float> h_;
};

// A superstar's titantron movie in his game (number `n`): WWE '13 / 2K14 NNN.bik2, 2K15-2K17 ent_NNNN_*.bk2 (the
// first), SvR NNN.bik / NN.bik. Empty if none; `name` its file.
inline Bytes SourceMovie(const disc::GameDisc &g, uint32_t title, int n, std::string *name) {
  const bool twok = title == 0x545408B6 || title == 0x315A07D2 || title == 0x545408BA;
  char pre[16], pre2[16];
  snprintf(pre, sizeof(pre), twok ? "ent_%04d_" : "%03d.", n);
  snprintf(pre2, sizeof(pre2), "%02d.", n);
  auto files = g.List("movies/titantron");
  std::sort(files.begin(), files.end());
  for (const auto &f : files) {
    const size_t dot = f.rfind('.');
    if (dot == std::string::npos)
      continue;
    const std::string ext = f.substr(dot);
    const bool movie = !_stricmp(ext.c_str(), ".bik") || !_stricmp(ext.c_str(), ".bik2") || !_stricmp(ext.c_str(), ".bk2");
    if (!movie || (_strnicmp(f.c_str(), pre, strlen(pre)) && (twok || _strnicmp(f.c_str(), pre2, strlen(pre2)))))
      continue;
    Bytes b;
    if (g.Read("movies/titantron/" + f, b) && !b.empty()) {
      if (name)
        *name = f;
      return b;
    }
  }
  return {};
}

class Window {
 public:
  // Opens the window on entrance `number` of `parts` (WWE '12's format), shown on WWE '12 superstar `star`; `stars`:
  // the superstars "Show it on someone else" offers. `title`: whose entrance (the window's name).
  void Open(ID3D11Device *dev, const disc::GameDisc &w12, const std::vector<entrance13::Part> &parts, int number,
            const std::string &title, int star, const std::vector<Star> &stars, const theme_source::Theme &theme = {},
            const std::filesystem::path &vgmstream = {}, const Sources &screens = {}) {
    sources_ = screens;  // (his own titantron, wall and ribbon, else the shown superstar's: LoadMovie)
    device_ = dev;
    w12_ = &w12;
    title_ = title;
    stars_ = stars;
    error_.clear();
    open_ = true;
    Parse(parts, number);
    // the theme (decoded on a worker: vgmstream takes seconds), from the frame the script starts the music at
    player_.Unload();
    song_error_.clear();
    music_frame_ = 100;
    for (const auto &p : parts)
      if (p.number == number && p.kind == "EVP")
        music_frame_ = TailFrame(p.data, 1, 100), movie_frame_ = TailFrame(p.data, 8, 0);
    if (!theme.stream.empty() && !vgmstream.empty()) {
      const std::wstring ext(theme.ext.begin(), theme.ext.end());
      decoding_ = std::async(std::launch::async, [vgmstream, stream = theme.stream, ext]() {
        Song s;
        theme_convert::Decode(vgmstream, stream, ext, s.samples, s.channels, s.rate);
        return s;
      });
    } else {
      song_error_ = theme.stream.empty() ? "no theme" : "no vgmstream";
    }
    star_ = -1;
    SetStar(star);
    time_ = 0;
    playing_ = true;
    use_cam_ = true;
    PickTake();
  }
  ~Window() {
  }
  bool IsOpen() const { return open_; }
  // Test aid: paused at `seconds`, the picture saved to `png` (and what was read to <png>.txt), once.
  void TestShot(float seconds, const std::string &png) {
    time_ = seconds * 60.0f;
    playing_ = false;
    shot_ = png;
    shot_wait_ = 3;
    // (WWE12_MM_WATCH_ARENA=<stage number, -1 none>, WWE12_MM_WATCH_TAKE=<0 A, 1 B>, WWE12_MM_WATCH_FREE: no camera)
    if (const char *a = getenv("WWE12_MM_WATCH_ARENA"))
      arena_on_ = atoi(a) >= 0, arena_ = std::max(0, atoi(a)), ShowArena();
    if (const char *t = getenv("WWE12_MM_WATCH_TAKE"))
      take_choice_ = std::clamp(atoi(t), 0, 1), PickTake();
    // (and WWE12_MM_WATCH_CAMY / _BODYY: other heights for the camera / him, in cm, to try)
    if (const char *y = getenv("WWE12_MM_WATCH_CAMY"))
      cam_y_ = float(atof(y));
    if (const char *y = getenv("WWE12_MM_WATCH_BODYY"))
      body_y_ = float(atof(y));
    if (getenv("WWE12_MM_WATCH_FREE"))
      use_cam_ = false, view_.fov = 0.9f;
    posed_ = -1;
  }
  void Close() { open_ = false; }

  // Every frame (draws the window while it is open).
  void Draw(bool fr) {
    if (!open_) {
      if (player_.Playing())  // (closed: silence)
        player_.Pause();
      return;
    }
    ImGui::SetNextWindowSize(ImVec2(1100, 760), ImGuiCond_FirstUseEver);
    char head[160];
    snprintf(head, sizeof(head), "%s %s###entrance_preview", fr ? "Aperçu complet :" : "Full preview:", title_.c_str());
    if (!ImGui::Begin(head, &open_, ImGuiWindowFlags_NoCollapse)) {
      ImGui::End();
      return;
    }
    DrawInside(fr);
    ImGui::End();
  }

 private:
  struct Part {
    int id = 0;        // PPPP
    int frames = 1;    // its length (60 a second)
    int start = 0;     // where it starts in the whole entrance
    const yanm::Block *body = nullptr;
    const yanm::Block *cam[2] = {nullptr, nullptr};
  };

  static uint32_t Be32(const Bytes &b, size_t o) { return o + 4 <= b.size() ? svrfmt::Be32(&b[o]) : 0; }

  // The script's entrant and camera, their records, the parts.
  void Parse(const std::vector<entrance13::Part> &parts, int number) {
    body_ = cam_ = {};
    objects_.clear();
    lifts_.clear();
    parts_.clear();
    total_ = 1;
    const entrance13::Part *evp = nullptr, *evz = nullptr;
    for (const auto &p : parts)
      if (p.number == number && p.kind == "EVP")
        evp = &p;
      else if (p.number == number && p.kind == "EVZ")
        evz = &p;
    if (!evp || !evz) {
      error_ = "no script or tracks for entrance " + std::to_string(number);
      return;
    }
    int entrant = -1, camera = -1;
    const Bytes &s = evp->data;
    const uint32_t n = Be32(s, 0x40) & 0xFFFF, t = Be32(s, 0x48) + 8;
    for (uint32_t i = 0; i < n && t + 32 * size_t(i + 1) <= s.size(); ++i) {
      const uint32_t t0 = Be32(s, t + 32 * size_t(i) + 8), t1 = Be32(s, t + 32 * size_t(i) + 12);
      if (entrant < 0 && t0 == 0x01000000 && t1 == 0)
        entrant = int(i);
      if (camera < 0 && t0 == 0x10000 && t1 == 0)
        camera = int(i);
      Object o;
      o.index = int(i), o.model = Be32(s, t + 32 * size_t(i) + 4), o.t0 = t0, o.t1 = t1;
      const uint32_t slots = Be32(s, t + 32 * size_t(i) + 24) >> 16, at = Be32(s, t + 32 * size_t(i) + 28) + 8;
      for (uint32_t k = 0; k < slots && at + 212 * size_t(k + 1) <= s.size(); ++k) {
        std::vector<float> w(53);
        for (size_t j = 0; j < w.size(); ++j)
          w[j] = svrfmt::BeF(&s[at + 212 * size_t(k) + 4 * j]);
        o.slots.push_back(std::move(w));
      }
      objects_.push_back(std::move(o));
    }
    Bytes bank = svrfmt::IsBpe(evz->data) ? svrfmt::Unpack(evz->data) : evz->data;
    std::vector<svrfmt::PachEntry> leaves;
    if (entrant < 0 || !svrfmt::PachRead(bank, leaves)) {
      error_ = entrant < 0 ? "no superstar in the script" : "tracks not readable";
      return;
    }
    for (const auto &e : leaves) {
      for (auto &o : objects_)
        if (o.index == int(e.id))
          yanm::Read(svrfmt::Unpack(e.data), o.record);
      if (int(e.id) != entrant && int(e.id) != camera)
        continue;
      yanm::Read(svrfmt::Unpack(e.data), int(e.id) == entrant ? body_ : cam_);
    }
    if (body_.blocks.empty()) {
      error_ = "the superstar's tracks not readable";
      return;
    }
    // the parts: every PPPP of his blocks and the camera's
    std::map<int, Part> by;
    auto id_of = [](const std::string &nm) { return atoi(nm.substr(0, 4).c_str()); };
    for (const auto &b : body_.blocks)
      if (b.name.size() == 8 && b.name.compare(4, 4, "0000") == 0) {
        Part &p = by[id_of(b.name)];
        p.body = &b;
        p.frames = std::max(p.frames, b.frames);
      }
    for (const auto &b : cam_.blocks) {
      if (b.name.size() != 8)
        continue;
      const int take = b.name.compare(4, 4, "0001") == 0 ? 1 : b.name.compare(4, 4, "0000") == 0 ? 0 : -1;
      if (take < 0)
        continue;
      Part &p = by[id_of(b.name)];
      p.cam[take] = &b;
      p.frames = std::max(p.frames, b.frames);
    }
    int at = 0;
    for (auto &[id, p] : by) {
      p.id = id;
      p.start = at;
      parts_.push_back(p);
      at += p.frames;
    }
    total_ = std::max(1, at);
  }

  // The superstar shown (pac\ch\chNNN.pac, his first attire).
  void SetStar(int id) {
    if (id == star_ || !device_)
      return;
    star_ = id;
    loaded_ = false;
    model_error_.clear();
    char rel[32];
    snprintf(rel, sizeof(rel), "pac/ch/ch%d.pac", id);
    Bytes f;
    if (id <= 0 || !w12_->Read(rel, f)) {
      model_error_ = "no model";
      return;
    }
    size_t section = 0;
    svrfmt::Archive a;
    if (svrfmt::ArchiveOpen(f, a, nullptr))
      for (size_t k = 0; k < a.root.kids.size(); ++k)
        if (a.root.kids[k].name.size() == 8 && a.root.kids[k].name[7] == '2') {
          section = k;
          break;
        }
    view_.keep_skin = true;
    loaded_ = view_.Load(device_, f, &model_error_, section);
    if (loaded_)
      view_.SetAngles(3.14159265f, 0.15f, 520);
    view_.Light();
    arena_shown_ = -1;  // (Load took everything away: the arena comes back)
    ShowArena();
    LoadMovie();
    posed_ = -1;
  }

  // The arena drawn around him (WWE '12's pac\bg\bgNN.pac, its entrance objects: stage, ramp), or none: as person 2,
  // without skeletons (not posed).
  void ShowArena() {
    const int want = arena_on_ ? arena_ : -1;
    if (!loaded_ || want == arena_shown_)
      return;
    if (arena_shown_ >= 0) {  // (an arena in the view: all of it loaded again, View::Remove only takes posed models away)
      const int id = star_;
      star_ = -1;
      SetStar(id);
      return;
    }
    arena_shown_ = want;
    arena_error_.clear();
    if (want < 0) {
      view_.Light();
      return;
    }
    if (arena_file_for_ != want) {
      arena_file_.clear();
      arena_file_for_ = want;
      char rel[32];
      snprintf(rel, sizeof(rel), "pac/bg/bg%02d.pac", want);
      if (!w12_->Read(rel, arena_file_))
        arena_error_ = "no arena file";
      ground_.Build(arena_file_);
      lifts_.clear();
    }
    view_.keep_skin = false;
    view_.copies = RingPieces();
    if (arena_error_.empty() && !view_.Add(arena_file_, &arena_error_, 0, 2) && arena_error_.empty())
      arena_error_ = "not readable";
    view_.copies.clear();
    view_.keep_skin = true;
    view_.show_entrance = true;
    for (int c = 0; c < 4; ++c)
      view_.background[c] = c == 3 ? 1.0f : 0.015f;
  }

  // The ring's pieces the arena holds once (bg01 RAW, 2026-11-11: their vertices dumped), placed around the ring as
  // the game does: the rope 960 (5.74 m along x at the centre, 25 bones; 956 its shadow and 980 its far version left
  // out) on the four sides (288 cm out) at its three heights (the entrances' rope objects hold the middle and top ones
  // 76 and 118 cm above the mat; the bottom one guessed 34), the turnbuckle pad 964 (one corner) and its fitting 952 at
  // every corner and height, the apron 973 / 974 (one side, from the mat down to the floor) on the four sides. The mat
  // is at y -120 (953), the posts (951) whole.
  static std::map<uint32_t, std::vector<arena_view::View::Place>> RingPieces() {
    using P = arena_view::View::Place;
    const float pi = 3.14159265f, mat = -120, side = 288, apron = 318;
    const float heights[3] = {mat - 34, mat - 76, mat - 118};
    std::map<uint32_t, std::vector<P>> out;
    out[956], out[980];  // (not drawn)
    // the game's effects, not scenery (bg01: 31000 "flash" the photographers' flashes, 32000 "blood", 40200 "rop_mask";
    // drawn where stored they were long white bars across the ring, 2026-11-11)
    out[31000], out[32000], out[40200];
    if (const char *hide = getenv("WWE12_MM_WATCH_HIDE"))  // (test aid: these models not drawn, "204,205,300-399")
      for (const char *p = hide; *p;) {
        char *e = nullptr;
        const uint32_t a = uint32_t(strtoul(p, &e, 10));
        const uint32_t b = *e == '-' ? uint32_t(strtoul(e + 1, &e, 10)) : a;
        for (uint32_t id = a; id <= b && id - a < 100000; ++id)
          out[id];
        p = e;
        while (*p && *p != ',')
          ++p;
        if (*p)
          ++p;
      }
    for (float h : heights) {
      out[960].push_back({0, {0, h, -side}});
      out[960].push_back({pi, {0, h, side}});
      out[960].push_back({pi / 2, {-side, h, 0}});
      out[960].push_back({-pi / 2, {side, h, 0}});
      for (int k = 0; k < 4; ++k)
        for (uint32_t id : {964u, 952u})
          out[id].push_back({float(k) * pi / 2, {0, h, 0}});
    }
    out[973].push_back({0, {0, 0, -apron}});
    out[973].push_back({pi, {0, 0, apron}});
    out[974].push_back({pi / 2, {-apron, 0, 0}});
    out[974].push_back({-pi / 2, {apron, 0, 0}});
    return out;
  }

  // The arena's stage / ramp height under him at frame f of part `part` (0 on the floor, in the ring and without an
  // arena): the entrance's tracks walk a floor-level stage; the game puts him on its own (Kaiko's Full preview does
  // the same: "the arena's stage / ramp height under the entrant"). Eased over +-16 frames: stepping up doesn't jump.
  float Lift(int part, float f) {
    if (arena_shown_ < 0 || ground_.Empty() || part < 0 || size_t(part) >= parts_.size())
      return 0;
    auto &v = lifts_[part];
    const Part &p = parts_[size_t(part)];
    if (v.empty()) {
      const yanm::Track *bip = nullptr;
      if (p.body)
        for (const auto &t : p.body->tracks)
          if (t.name == "Bip")
            bip = &t;
      std::vector<float> raw(size_t(std::max(1, p.frames)), 0.0f);
      for (size_t k = 0; bip && k < raw.size(); ++k) {
        bool hr = false, hp = false;
        yanm::Q q{0, 0, 0, 1};
        yanm::V pos{0, 0, 0};
        yanm::Sample(*bip, float(k), hr, q, hp, pos);
        if (hp)
          raw[k] = ground_.At(pos[0], pos[2]);
      }
      const int ease = 16, n = int(raw.size());
      v.resize(raw.size());
      for (int k = 0; k < n; ++k) {
        float sum = 0;
        int c = 0;
        for (int j = std::max(0, k - ease); j <= std::min(n - 1, k + ease); ++j)
          sum += raw[size_t(j)], ++c;
        v[size_t(k)] = sum / float(std::max(1, c));
      }
    }
    return v[std::min(v.size() - 1, size_t(std::max(0.0f, f)))];
  }

  // The part playing at `time` (its index) and the frame in it.
  int PartAt(float time, float &local) const {
    for (int i = int(parts_.size()) - 1; i >= 0; --i)
      if (time >= float(parts_[size_t(i)].start)) {
        local = std::min(time - float(parts_[size_t(i)].start), float(parts_[size_t(i)].frames - 1));
        return i;
      }
    local = 0;
    return 0;
  }

  std::map<std::string, arena_view::BonePose> Pose(const yanm::Block *b, float f) const {
    std::map<std::string, arena_view::BonePose> pose;
    if (b)
      for (const auto &t : b->tracks) {
        if (t.name.empty())
          continue;  // ("#<hash>" names kept: the fingers and the face, matched by hash in arena_view::Fill)
        arena_view::BonePose bp;
        yanm::Sample(t, f, bp.has_rot, bp.rot, bp.has_pos, bp.pos);
        if (t.name == "Bip" && bp.has_pos)
          bp.pos[1] += body_y_ + lift_now_;
        pose[t.name] = bp;
      }
    return pose;
  }

  // What was read (test aid).
  void Report(const std::string &file) const {
    FILE *f = fopen(file.c_str(), "w");
    if (!f)
      return;
    fprintf(f, "at %.1f: eye %.1f %.1f %.1f target %.1f %.1f %.1f fov %.3f %.3f %.3f hips %.1f %.1f %.1f lift %.1f\n", time_,
            last_eye_[0], last_eye_[1], last_eye_[2], last_tgt_[0], last_tgt_[1], last_tgt_[2], last_fov_[0],
            last_fov_[1], last_fov_[2], view_.Hips()[0], view_.Hips()[1], view_.Hips()[2], lift_now_);
    fprintf(f, "song %d (%.1f s) %s, music from %u; movies from %u: titantron [%s] %d frames, wall [%s], ribbon [%s]\n",
            int(player_.Ready()), player_.Length(), song_error_.c_str(), music_frame_, movie_frame_,
            titan_.name.c_str(), titan_.Frames(), wall_.name.c_str(), ribbon_.name.c_str());
    fputs(ground_.screens.c_str(), f);
    for (const auto &o : objects_) {  // (the script's objects: slot 0's words, the first block's tracks at frame 0)
      fprintf(f, "object %d model %u type %08x %08x, %zu slots, %zu blocks\n", o.index, o.model, o.t0, o.t1,
              o.slots.size(), o.record.blocks.size());
      if (!o.slots.empty()) {
        fprintf(f, "  slot0:");
        for (float v : o.slots[0])
          fprintf(f, " %g", v);
        fprintf(f, "\n");
      }
      if (!o.record.blocks.empty()) {
        const auto &b = o.record.blocks[0];
        fprintf(f, "  block %s frames %d:", b.name.c_str(), b.frames);
        for (const auto &t : b.tracks) {
          bool hr = false, hp = false;
          yanm::Q q{0, 0, 0, 1};
          yanm::V v{0, 0, 0};
          yanm::Sample(t, 0, hr, q, hp, v);
          fprintf(f, " [%s%s%s %.1f %.1f %.1f]", t.name.c_str(), hr ? " r" : "", hp ? " p" : "", v[0], v[1], v[2]);
        }
        fprintf(f, "\n");
      }
    }
    fprintf(f, "star %d loaded %d %s\ntotal %d frames, %zu parts\n", star_, int(loaded_), model_error_.c_str(), total_,
            parts_.size());
    for (const auto &p : parts_) {
      fprintf(f, "part %04d start %d frames %d body %s cam %s %s\n", p.id, p.start, p.frames,
              p.body ? p.body->name.c_str() : "-", p.cam[0] ? p.cam[0]->name.c_str() : "-",
              p.cam[1] ? p.cam[1]->name.c_str() : "-");
      for (const yanm::Block *b : {p.body, p.cam[0], p.cam[1]}) {
        if (!b)
          continue;
        for (const auto &t : b->tracks) {
          bool hr, hp;
          yanm::Q q;
          yanm::V v{0, 0, 0};
          yanm::Sample(t, 0, hr, q, hp, v);
          fprintf(f, "  %s %s flags %04x rot %zu pos %zu  pos0 %.1f %.1f %.1f\n", b->name.c_str(), t.name.c_str(),
                  t.flags, t.rot16.size() + t.rot8.size(), t.pos.size(), v[0], v[1], v[2]);
        }
      }
    }
    fclose(f);
  }

  // A record of the script's tail ({u16 type, u16 0, u32 frame...}: type 1 the music, 8 the titantron movie,
  // entrance13::AddTail12): its frame, `none` if it has none.
  static uint32_t TailFrame(const Bytes &evp, uint32_t type, uint32_t none) {
    if (evp.size() < 0x50 || !Be32(evp, 0x1C))
      return none;
    const size_t h = size_t(Be32(evp, 0x1C)) + 8;
    const uint32_t lists = Be32(evp, h) & 0xFFFF, at = Be32(evp, h + 4) + 8;
    for (uint32_t l = 0; l < lists && l < 64 && at + 16 * size_t(l + 1) <= evp.size(); ++l)
      if ((Be32(evp, at + 16 * l) >> 16) == type)
        return Be32(evp, at + 16 * l + 4);
    return none;
  }

  // The theme: playing while the entrance plays, from its music frame (each pause or jump: started again there).
  void Sound() {
    if (decoding_.valid() && decoding_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
      Song s = decoding_.get();
      if (s.samples.empty())
        song_error_ = "the theme couldn't be decoded";
      else
        player_.Set(std::move(s.samples), s.channels, s.rate);
    }
    const bool want = music_on_ && playing_ && time_ >= float(music_frame_) && player_.Ready();
    if (want && !player_.Playing())
      player_.Play((time_ - float(music_frame_)) / 60.0);
    else if (!want && player_.Playing())
      player_.Pause();
  }

  // The arena's video screens during the entrance, as WWE '12 feeds them by model number (bg01 RAW: pictures
  // "titantron_movie" 800 / 803 / 806 / 808, "arena_movie" 801 / 807 / 809, "minitron_movie" 804; port13.h,
  // the screens come in twos on the same faces, seen with each one coloured, 2026-11-11): the titantron movie on 808
  // (v 0-0.684; 803 its twin, the whole movie: not drawn) and its minitron strip (v 0.69-1) on 806 (807 its twin, the
  // arena's movie: not drawn), 800 the small screen; the superstar's wall (evtmov MVSW, a sheet cut up) on 801 / 804
  // (the panel left of the titantron and the ring under the roof); his ribbon (MVSB) on 809 (the band round the top).
  // The ported superstar's own, from his game (sources_), else the shown WWE '12 superstar's.
  void LoadMovie() {
    titan_.Close(), wall_.Close(), ribbon_.Close();
    view_.screens.clear();
    char num[8];
    snprintf(num, sizeof(num), "%03d", star_);
    // the titantron: a 2K one (one 16:9 picture in the square) put above its minitron, as WWE '12's are laid out
    if (!sources_.titan.movie.empty())
      titan_.Open(device_, sources_.titan, 320, 320, false, sources_.titan_2k ? &sources_.mini : nullptr);
    if (!titan_.On()) {
      Clip own;
      own.name = std::string("WWE '12 ") + num + ".bik";
      if (w12_->Read(std::string("movies/titantron/") + num + ".bik", own.movie))
        titan_.Open(device_, own, 320, 320, false, nullptr);
    }
    // the wall and the ribbon: his, else the shown superstar's (WWE '12's pac\evt\evtmov.pac MVSW / MVSB M<number>)
    if (evtmov12_.empty()) {
      Bytes b;
      if (w12_->Read("pac/evt/evtmov.pac", b))
        for (auto &e : entrance_screens::Entries(std::move(b)))
          if (e.group == "MVSW" || e.group == "MVSB")
            evtmov12_[e.group + "/" + e.name] = std::move(e.data);
      evtmov12_["-"] = {};  // (read once)
    }
    for (const bool ribbon : {false, true}) {
      Screen &s = ribbon ? ribbon_ : wall_;
      const Clip &his = ribbon ? sources_.ribbon : sources_.wall;
      if (!his.movie.empty())
        s.Open(device_, his, ribbon ? entrance_screens::kRibbonW : entrance_screens::kWall,
               ribbon ? entrance_screens::kRibbonH : entrance_screens::kWall, ribbon, nullptr);
      if (!s.On()) {
        const auto it = evtmov12_.find(std::string(ribbon ? "MVSB" : "MVSW") + "/M" + num);
        if (it != evtmov12_.end()) {
          Clip own{it->second, 0, 256, std::string("WWE '12 ") + it->first};
          s.Open(device_, own, ribbon ? entrance_screens::kRibbonW : entrance_screens::kWall,
                 ribbon ? entrance_screens::kRibbonH : entrance_screens::kWall, ribbon, nullptr);
        }
      }
    }
  }
  // Their pictures at the entrance's time (from the script's movie frame, looping), decoded up to there.
  void Movie() {
    view_.screens.clear();
    // (test aid WWE12_MM_WATCH_SCREENS: each screen model a colour - 800 red, 801 green, 802 blue, 803 yellow, 804
    // magenta, 805 cyan, 806 orange, 807 purple, 808 white, 809 grey -, its picture's top half bright, bottom half dark,
    // a black band at u < 0.25: which screen is where, and its movie's way up)
    if (getenv("WWE12_MM_WATCH_SCREENS")) {
      static const uint32_t kColours[10] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0xFF00FF,
                                            0x00FFFF, 0xFF8000, 0x8000FF, 0xFFFFFF, 0x808080};
      if (test_colours_.empty())
        for (uint32_t c : kColours) {
          std::vector<uint8_t> px(16 * 16 * 4);
          for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
              const float k = x < 4 ? 0.0f : y < 8 ? 1.0f : 0.35f;
              uint8_t *p = &px[size_t(y * 16 + x) * 4];
              p[0] = uint8_t((c & 0xFF) * k), p[1] = uint8_t((c >> 8 & 0xFF) * k), p[2] = uint8_t((c >> 16) * k), p[3] = 255;
            }
          D3D11_TEXTURE2D_DESC td{};
          td.Width = td.Height = 16, td.MipLevels = 1, td.ArraySize = 1;
          td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
          td.SampleDesc.Count = 1;
          td.Usage = D3D11_USAGE_DEFAULT;
          td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
          D3D11_SUBRESOURCE_DATA sd{px.data(), 16 * 4, 0};
          Microsoft::WRL::ComPtr<ID3D11Texture2D> t;
          Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> v;
          if (SUCCEEDED(device_->CreateTexture2D(&td, &sd, &t)))
            device_->CreateShaderResourceView(t.Get(), nullptr, &v);
          test_colours_.push_back(v);
        }
      for (uint32_t m = 0; m < 10; ++m)
        view_.screens[800 + m] = test_colours_[m].Get();
      return;
    }
    if (!movie_on_)
      return;
    const double at = std::max(0.0, double(time_ - float(movie_frame_)) / 60.0);
    ID3D11DeviceContext *ctx = nullptr;
    device_->GetImmediateContext(&ctx);
    for (Screen *s : {&titan_, &wall_, &ribbon_})
      s->At(at, ctx);
    ctx->Release();
    // (the twins on the same faces not drawn: 803 the titantron's whole movie over 808, minitron strip included; 807
    // the arena's movie over 806)
    view_.screens[803] = view_.screens[807] = nullptr;
    if (titan_.On())
      for (uint32_t m : {800u, 806u, 808u})
        view_.screens[m] = titan_.Srv();
    if (wall_.On())
      for (uint32_t m : {801u, 804u})
        view_.screens[m] = wall_.Srv();
    if (ribbon_.On())
      view_.screens[809] = ribbon_.Srv();
  }

  // The camera take played: the one picked, else one of the two at random (the game's way).
  void PickTake() {
    take_ = take_choice_ >= 0 ? take_choice_ : int(GetTickCount64() & 1);
    posed_ = -1;
  }

  // The entrance's camera at frame f of part p (its take, else the other): eye, target, field of view. False if the
  // part has none. (2K16 / 2K17 name the tracks with another last digit: matched without it.)
  bool Camera(const Part &p, float f) {
    const yanm::Block *b = p.cam[take_] ? p.cam[take_] : p.cam[1 - take_];
    if (!b)
      return false;
    auto find = [&](const char *h) -> const yanm::Track * {
      const yanm::Track *alike = nullptr;
      for (const auto &t : b->tracks)
        if (t.name == h)
          return &t;
        else if (!alike && t.name.size() == 9 && t.name.compare(0, 8, h, 8) == 0)
          alike = &t;
      return alike;
    };
    const yanm::Track *eye = find("#5edfdf5f"), *tgt = find("#70d146a5"), *fov = find("#70d304a4");
    if (!eye)
      return false;
    bool hr = false, hp = false;
    yanm::Q q{0, 0, 0, 1};
    yanm::V e{0, 0, 0}, t{0, 0, 0}, v{0, 0, 0};
    yanm::Sample(*eye, f, hr, q, hp, e);
    if (!hp)
      return false;
    if (tgt) {
      yanm::Sample(*tgt, f, hr, q, hp, t);
      if (!hp)
        tgt = nullptr;
    }
    if (!tgt)
      for (int c = 0; c < 3; ++c)
        t[size_t(c)] = view_.Hips()[c];
    e[1] += cam_y_ + lift_now_, t[1] += cam_y_ + lift_now_;  // (the camera with him: it aims at him as keyed)
    const float d[3] = {e[0] - t[0], e[1] - t[1], e[2] - t[2]};
    const float dist = std::max(1.0f, std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]));
    const float target[3] = {t[0], t[1], t[2]};
    for (int c = 0; c < 3; ++c)
      last_eye_[c] = e[size_t(c)], last_tgt_[c] = t[size_t(c)];
    view_.SetTarget(target);
    view_.SetAngles(std::atan2(d[0], d[2]), std::asin(std::clamp(-d[1] / dist, -1.0f, 1.0f)), dist);
    // the field of view: the track's third value
    view_.fov = 0.6f;
    if (fov) {
      yanm::Sample(*fov, f, hr, q, hp, v);
      if (hp && v[2] > 0.05f && v[2] < 3.0f)
        view_.fov = v[2];
      last_fov_[0] = v[0], last_fov_[1] = v[1], last_fov_[2] = v[2];
    }
    return true;
  }

  static std::string Clock(float frames) {
    const int s = int(frames / 60.0f);
    char b[16];
    snprintf(b, sizeof(b), "%d:%02d", s / 60, s % 60);
    return b;
  }

  void DrawInside(bool fr) {
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f);
    if (!error_.empty()) {
      ImGui::TextColored(red, "%s %s", fr ? "Entrée illisible :" : "Entrance not readable:", error_.c_str());
      return;
    }
    if (playing_) {
      time_ += ImGui::GetIO().DeltaTime * 60.0f;
      if (time_ >= float(total_ - 1))
        time_ = float(total_ - 1), playing_ = false;
    }
    Sound();
    Movie();
    float local = 0;
    const int part = PartAt(time_, local);
    const Part &p = parts_[size_t(part)];
    // the picture
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 size(avail.x, std::max(200.0f, avail.y - 100));
    if (!loaded_) {
      ImGui::Dummy(size);
      if (!shot_.empty())
        Report(shot_ + ".txt"), shot_.clear();
    } else {
      const float key = float(part) * 100000.0f + local;
      if (key != posed_) {
        posed_ = key;
        lift_now_ = Lift(part, local);
        view_.SetPose(Pose(p.body, local));
        if (!(use_cam_ && Camera(p, local)) && follow_)
          view_.SetTarget(view_.Hips());
      }
      const ImVec2 at = ImGui::GetCursorScreenPos();
      if (ID3D11ShaderResourceView *pic = view_.Render(int(size.x), int(size.y))) {
        ImGui::Image(ImTextureRef(ImTextureID(reinterpret_cast<intptr_t>(pic))), size);
        if (!shot_.empty() && (decoding_.valid() ? --shot_wait_ < -900 : --shot_wait_ <= 0)) {  // (the theme read first)
          svrfmt::Image img;
          if (view_.ReadBack(img))
            wic_image::SavePng(std::filesystem::u8path(shot_), img);
          Report(shot_ + ".txt");
          shot_.clear();
        }
        if (ImGui::IsItemHovered()) {
          const ImGuiIO &io = ImGui::GetIO();
          // (the mouse takes the camera: the entrance's let go)
          const bool drag_l = ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0),
                     drag_r = ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0);
          if (use_cam_ && (io.MouseWheel != 0 || drag_l || drag_r))
            use_cam_ = false, view_.fov = 0.9f;
          if (io.MouseWheel != 0)
            view_.Zoom(io.MouseWheel);
          if (drag_l)
            view_.Orbit(io.MouseDelta.x, io.MouseDelta.y);
          if (drag_r)
            view_.Pan(io.MouseDelta.x, io.MouseDelta.y), follow_ = false;
        }
      }
      const char *what = part == 0 && parts_.size() > 1
                             ? (fr ? "L'intro (le catcheur sort après)" : "The intro (the superstar comes out after it)")
                         : part + 1 == int(parts_.size()) ? (fr ? "Au ring" : "At the ring")
                                                          : nullptr;
      if (what)
        ImGui::GetWindowDrawList()->AddText(ImVec2(at.x + 8, at.y + 6), IM_COL32(230, 230, 230, 255), what);
    }
    // the controls
    if (ImGui::Button(playing_ ? "Pause" : (fr ? "Lecture" : "Play"), ImVec2(90, 0))) {
      if (!playing_ && time_ >= float(total_ - 1))
        time_ = 0;
      playing_ = !playing_;
    }
    ImGui::SameLine();
    if (ImGui::Button(fr ? "Recommencer" : "Restart", ImVec2(110, 0)))
      time_ = 0, playing_ = true, PickTake(), player_.Pause();
    ImGui::SameLine();
    if (ImGui::Button(fr ? "Aller au ring" : "Skip to the ring", ImVec2(130, 0)))
      time_ = float(parts_.back().start), playing_ = true, player_.Pause();
    ImGui::SameLine();
    const std::string clock = Clock(time_) + " / " + Clock(float(total_));
    ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x - 120));
    float t = time_;
    if (ImGui::SliderFloat("##time", &t, 0, float(total_ - 1), clock.c_str()))
      time_ = t, player_.Pause();
    ImGui::SameLine();
    ImGui::Text(fr ? "partie %d sur %d" : "part %d of %d", part + 1, int(parts_.size()));
    // the camera
    if (ImGui::Checkbox(fr ? "La caméra de l'entrée" : "The entrance's camera", &use_cam_)) {
      posed_ = -1;
      if (!use_cam_)
        view_.fov = 0.9f, view_.SetAngles(3.14159265f, 0.15f, 520), follow_ = true;
    }
    ImGui::SameLine();
    {
      const char *ab[] = {"A", "B"};
      char now_cam[48];
      snprintf(now_cam, sizeof(now_cam), "%s%s", take_choice_ < 0 ? (fr ? "au hasard : " : "random: ") : "", ab[take_]);
      ImGui::SetNextItemWidth(150);
      if (ImGui::BeginCombo(fr ? "Caméras##ep_cam" : "Cameras##ep_cam", now_cam)) {
        if (ImGui::Selectable(fr ? "Au hasard (comme le jeu)" : "Random (as the game)", take_choice_ < 0))
          take_choice_ = -1, PickTake();
        for (int k = 0; k < 2; ++k)
          if (ImGui::Selectable(ab[k], take_choice_ == k))
            take_choice_ = k, PickTake();
        ImGui::EndCombo();
      }
    }
    // the theme and the titantron
    ImGui::SameLine(0, 24);
    ImGui::Checkbox(fr ? "Musique du thème" : "Theme music", &music_on_);
    ImGui::SameLine();
    ImGui::Checkbox("Titantron", &movie_on_);
    ImGui::SameLine();
    if (decoding_.valid())
      ImGui::TextDisabled("%s", fr ? "(lecture du thème...)" : "(reading the theme...)");
    else if (!song_error_.empty())
      ImGui::TextDisabled(fr ? "(pas de musique : %s)" : "(no music: %s)", song_error_.c_str());
    else if (!titan_.On())
      ImGui::TextDisabled("%s", fr ? "(pas de titantron pour lui)" : "(no titantron for him)");
    else
      ImGui::TextDisabled(fr ? "(titantron : %s ; mur : %s ; ruban : %s)" : "(titantron: %s; wall: %s; ribbon: %s)",
                          titan_.name.c_str(), wall_.On() ? wall_.name.c_str() : "-",
                          ribbon_.On() ? ribbon_.name.c_str() : "-");
    // the arena
    if (ImGui::Checkbox(fr ? "Arène" : "Arena", &arena_on_))
      ShowArena();
    ImGui::SameLine();
    {
      std::string now_arena = "?";
      for (const auto &sl : arena_page::kSlots)
        if (sl.number == arena_)
          now_arena = sl.name;
      ImGui::SetNextItemWidth(200);
      if (ImGui::BeginCombo("##ep_arena", now_arena.c_str(), ImGuiComboFlags_HeightLarge)) {
        for (const auto &sl : arena_page::kSlots)
          if (ImGui::Selectable(arena_page::SlotLabel(sl).c_str(), sl.number == arena_)) {
            arena_ = sl.number, arena_on_ = true;
            ShowArena();
          }
        ImGui::EndCombo();
      }
      if (!arena_error_.empty()) {
        ImGui::SameLine();
        ImGui::TextColored(red, "(%s)", arena_error_.c_str());
      }
    }
    ImGui::SameLine(0, 24);
    // the superstar
    ImGui::TextUnformatted(fr ? "Montrée sur" : "Shown on");
    ImGui::SameLine();
    std::string now = "?";
    for (const auto &s : stars_)
      if (s.id == star_)
        now = s.name;
    ImGui::SetNextItemWidth(260);
    if (ImGui::BeginCombo("##ep_star", now.c_str(), ImGuiComboFlags_HeightLarge)) {
      for (const auto &s : stars_)
        if (ImGui::Selectable((s.name + "##ep" + std::to_string(s.id)).c_str(), s.id == star_))
          SetStar(s.id);
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (!use_cam_) {
      if (ImGui::Checkbox(fr ? "Suivre le catcheur" : "Follow the superstar", &follow_))
        posed_ = -1;
      ImGui::SameLine();
    }
    if (!model_error_.empty())
      ImGui::TextColored(red, "%s", fr ? "(pas de modèle pour lui : choisissez-en un autre)" : "(no model for him: pick another)");
    else
      ImGui::TextDisabled("%s", fr ? "Glisser : tourner, clic droit : déplacer, molette : zoom."
                                   : "Drag: turn, right drag: move, wheel: zoom.");
  }

  struct Song {
    std::vector<int16_t> samples;
    int channels = 0, rate = 0;
  };

  ID3D11Device *device_ = nullptr;
  const disc::GameDisc *w12_ = nullptr;
  bool open_ = false;
  // the theme
  std::future<Song> decoding_;
  audio_out::Player player_;
  std::string song_error_;
  uint32_t music_frame_ = 100, movie_frame_ = 0;  // (the script's: when the music and the titantron start)
  bool music_on_ = true, movie_on_ = true;
  // the titantron
  Sources sources_;                          // the ported superstar's screens (his game's)
  Screen titan_, wall_, ribbon_;             // what the arena's screens show (LoadMovie)
  std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> test_colours_;  // (test aid: Movie)
  std::map<std::string, Bytes> evtmov12_;   // WWE '12's walls and ribbons ("MVSW/M139"...), read once
  std::string title_, error_, model_error_;
  std::vector<Star> stars_;
  yanm::Record body_, cam_;
  // Every object of the script (its place, model, type, slots' words, its record): ropes, turnbuckles, props...
  struct Object {
    int index = 0;
    uint32_t model = 0, t0 = 0, t1 = 0;
    std::vector<std::vector<float>> slots;
    yanm::Record record;
  };
  std::vector<Object> objects_;
  std::vector<Part> parts_;
  int total_ = 1;
  arena_view::View view_;
  int star_ = -1;
  bool loaded_ = false, playing_ = true, follow_ = true;
  bool use_cam_ = true;              // the entrance's camera (else the mouse's)
  // An entrance's tracks (his and the camera's) have the ring mat at y 0; in the arena it is 1.2 m up (y -120: his feet
  // on the mat in the ring, on the ramp and in the ring with it; sunk to the knees without, 2026-11-11). (Test aid
  // WWE12_MM_WATCH_CAMY / _BODYY: other heights.)
  float cam_y_ = -120, body_y_ = -120;
  float last_eye_[3] = {}, last_tgt_[3] = {}, last_fov_[3] = {};  // (test aid: the camera last set)
  bool arena_on_ = true;             // the arena drawn
  int arena_ = 1, arena_shown_ = -1, arena_file_for_ = -1;  // its stage number (RAW), the one in the view, the one read
  Bytes arena_file_;
  Ground ground_;                        // its stage and ramp heights
  std::map<int, std::vector<float>> lifts_;  // per part, per frame: the height under him (Lift)
  float lift_now_ = 0;
  std::string arena_error_;
  int take_choice_ = -1, take_ = 0;  // the camera take: -1 random (take_ the one drawn), 0 A, 1 B
  float time_ = 0, posed_ = -1;
  std::string shot_;
  int shot_wait_ = 0;
};

}  // namespace entrance_preview
