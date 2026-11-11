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
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <future>

#include "arena_page.h"
#include "arena_view.h"
#include "audio_out.h"
#include "media.h"
#include "theme_convert.h"
#include "theme_source.h"
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

class Window {
 public:
  // Opens the window on entrance `number` of `parts` (WWE '12's format), shown on WWE '12 superstar `star`; `stars`:
  // the superstars "Show it on someone else" offers. `title`: whose entrance (the window's name).
  void Open(ID3D11Device *dev, const disc::GameDisc &w12, const std::vector<entrance13::Part> &parts, int number,
            const std::string &title, int star, const std::vector<Star> &stars, const theme_source::Theme &theme = {},
            const std::filesystem::path &vgmstream = {}) {
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
    if (bink_)
      bink_close(bink_);
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
    }
    Bytes bank = svrfmt::IsBpe(evz->data) ? svrfmt::Unpack(evz->data) : evz->data;
    std::vector<svrfmt::PachEntry> leaves;
    if (entrant < 0 || !svrfmt::PachRead(bank, leaves)) {
      error_ = entrant < 0 ? "no superstar in the script" : "tracks not readable";
      return;
    }
    for (const auto &e : leaves) {
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
    }
    view_.keep_skin = false;
    if (arena_error_.empty() && !view_.Add(arena_file_, &arena_error_, 0, 2) && arena_error_.empty())
      arena_error_ = "not readable";
    view_.keep_skin = true;
    view_.show_entrance = true;
    for (int c = 0; c < 4; ++c)
      view_.background[c] = c == 3 ? 1.0f : 0.015f;
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
        if (t.name.empty() || t.name[0] == '#')
          continue;
        arena_view::BonePose bp;
        yanm::Sample(t, f, bp.has_rot, bp.rot, bp.has_pos, bp.pos);
        if (t.name == "Bip" && bp.has_pos)
          bp.pos[1] += body_y_;
        pose[t.name] = bp;
      }
    return pose;
  }

  // What was read (test aid).
  void Report(const std::string &file) const {
    FILE *f = fopen(file.c_str(), "w");
    if (!f)
      return;
    fprintf(f, "at %.1f: eye %.1f %.1f %.1f target %.1f %.1f %.1f fov %.3f %.3f %.3f hips %.1f %.1f %.1f\n", time_,
            last_eye_[0], last_eye_[1], last_eye_[2], last_tgt_[0], last_tgt_[1], last_tgt_[2], last_fov_[0],
            last_fov_[1], last_fov_[2], view_.Hips()[0], view_.Hips()[1], view_.Hips()[2]);
    fprintf(f, "song %d (%.1f s) %s, music from %u, titantron %d (%d frames) from %u\n", int(player_.Ready()),
            player_.Length(), song_error_.c_str(), music_frame_, int(bink_ != nullptr), bink_ ? bink_frames(bink_) : 0,
            movie_frame_);
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

  // The superstar's titantron movie (WWE '12's movies\titantron\NNN.bik), on the arena's screens.
  void LoadMovie() {
    if (bink_)
      bink_close(bink_), bink_ = nullptr;
    movie_srv_.Reset();
    movie_tex_.Reset();
    view_.screen = nullptr;
    char file[16];
    snprintf(file, sizeof(file), "%03d.bik", star_);
    const std::filesystem::path p = media::CopyGameMovie(*w12_, file);
    WCHAR err[256];
    if (p.empty() || !(bink_ = bink_open(p.c_str(), err, 256)))
      return;
    D3D11_TEXTURE2D_DESC td{};
    td.Width = UINT(bink_width(bink_));
    td.Height = UINT(bink_height(bink_));
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device_->CreateTexture2D(&td, nullptr, &movie_tex_)) ||
        FAILED(device_->CreateShaderResourceView(movie_tex_.Get(), nullptr, &movie_srv_))) {
      bink_close(bink_), bink_ = nullptr;
      return;
    }
    pixels_.assign(size_t(td.Width) * td.Height * 4, 0);
    movie_at_ = -1;
  }
  // Its picture at the entrance's time (from the script's movie frame, looping), decoded up to there.
  void Movie() {
    view_.screen = movie_on_ && bink_ ? movie_srv_.Get() : nullptr;
    if (!view_.screen)
      return;
    const double per = bink_frame_time(bink_) > 0 ? double(bink_frame_time(bink_)) / 1e7 : 1.0 / 30;
    const int frames = std::max(1, bink_frames(bink_));
    const int want = int(std::max(0.0f, time_ - float(movie_frame_)) / 60.0 / per) % frames;
    if (want == movie_at_)
      return;
    if (want < movie_at_ || movie_at_ < 0)
      bink_rewind(bink_), movie_at_ = -1;
    for (int k = 0; movie_at_ < want && k < 120; ++k)  // (a long jump catches up over a few frames)
      bink_next(bink_), ++movie_at_;
    bink_bgra(bink_, pixels_.data());
    ID3D11DeviceContext *ctx = nullptr;
    device_->GetImmediateContext(&ctx);
    ctx->UpdateSubresource(movie_tex_.Get(), 0, nullptr, pixels_.data(), UINT(bink_width(bink_) * 4), 0);
    ctx->Release();
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
    e[1] += cam_y_, t[1] += cam_y_;
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
    else if (!bink_)
      ImGui::TextDisabled("%s", fr ? "(pas de titantron pour lui)" : "(no titantron for him)");
    else
      ImGui::NewLine();
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
  BinkReader *bink_ = nullptr;
  Microsoft::WRL::ComPtr<ID3D11Texture2D> movie_tex_;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> movie_srv_;
  std::vector<uint8_t> pixels_;
  int movie_at_ = -1;
  std::string title_, error_, model_error_;
  std::vector<Star> stars_;
  yanm::Record body_, cam_;
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
  std::string arena_error_;
  int take_choice_ = -1, take_ = 0;  // the camera take: -1 random (take_ the one drawn), 0 A, 1 B
  float time_ = 0, posed_ = -1;
  std::string shot_;
  int shot_wait_ = 0;
};

}  // namespace entrance_preview
