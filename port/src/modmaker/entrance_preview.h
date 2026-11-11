// Mod Maker: the full preview of an entrance (a window, as the "Full preview" of Kaiko's SvR 2011 Mod Maker,
// port/modmaker/full_preview.cpp, MIT, with credit): a ported entrance (WWE '12's format, entrance13.h) played in 3D
// on a WWE '12 superstar's model, its parts one after another, with Pause, Restart, Skip to the ring and a time bar.
//
// An entrance n: its script EVP n (objects, 32 bytes each from +0x48 + 8: +8 / +12 the type, the entrant 01000000 0,
// the camera 00010000 0) and its tracks EVZ n (a PACH, a YANM record per object, the leaf's id = the object's place in
// the script). A record has a block per part of the entrance and per take, named "PPPPYYYY" (PPPP the part: 0000,
// 0005, 0010...; YYYY 0000 the body, a camera has two takes 0000 / 0001, the game plays one of them at random). Part 0
// is the hidden intro (the titantron plays, the superstar comes out after it). Tracks are 60 frames a second, positions
// in cm in the arena's space (y down).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "arena_view.h"
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
            const std::string &title, int star, const std::vector<Star> &stars) {
    device_ = dev;
    w12_ = &w12;
    title_ = title;
    stars_ = stars;
    error_.clear();
    open_ = true;
    Parse(parts, number);
    star_ = -1;
    SetStar(star);
    time_ = 0;
    playing_ = true;
    posed_ = -1;
  }
  bool IsOpen() const { return open_; }
  // Test aid: paused at `seconds`, the picture saved to `png` (and what was read to <png>.txt), once.
  void TestShot(float seconds, const std::string &png) {
    time_ = seconds * 60.0f;
    playing_ = false;
    shot_ = png;
    shot_wait_ = 3;
  }
  void Close() { open_ = false; }

  // Every frame (draws the window while it is open).
  void Draw(bool fr) {
    if (!open_)
      return;
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
    view_.Light();
    loaded_ = view_.Load(device_, f, &model_error_, section);
    if (loaded_)
      view_.SetAngles(3.14159265f, 0.15f, 520);
    posed_ = -1;
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
        pose[t.name] = bp;
      }
    return pose;
  }

  // What was read (test aid).
  void Report(const std::string &file) const {
    FILE *f = fopen(file.c_str(), "w");
    if (!f)
      return;
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
    float local = 0;
    const int part = PartAt(time_, local);
    const Part &p = parts_[size_t(part)];
    // the picture
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const ImVec2 size(avail.x, std::max(200.0f, avail.y - 90));
    if (!loaded_) {
      ImGui::Dummy(size);
      if (!shot_.empty())
        Report(shot_ + ".txt"), shot_.clear();
    } else {
      const float key = float(part) * 100000.0f + local;
      if (key != posed_) {
        posed_ = key;
        view_.SetPose(Pose(p.body, local));
        if (follow_)
          view_.SetTarget(view_.Hips());
      }
      const ImVec2 at = ImGui::GetCursorScreenPos();
      if (ID3D11ShaderResourceView *pic = view_.Render(int(size.x), int(size.y))) {
        ImGui::Image(ImTextureRef(ImTextureID(reinterpret_cast<intptr_t>(pic))), size);
        if (!shot_.empty() && --shot_wait_ <= 0) {
          svrfmt::Image img;
          if (view_.ReadBack(img))
            wic_image::SavePng(std::filesystem::u8path(shot_), img);
          Report(shot_ + ".txt");
          shot_.clear();
        }
        if (ImGui::IsItemHovered()) {
          const ImGuiIO &io = ImGui::GetIO();
          if (io.MouseWheel != 0)
            view_.Zoom(io.MouseWheel);
          if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0))
            view_.Orbit(io.MouseDelta.x, io.MouseDelta.y);
          if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0))
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
      time_ = 0, playing_ = true;
    ImGui::SameLine();
    if (ImGui::Button(fr ? "Aller au ring" : "Skip to the ring", ImVec2(130, 0)))
      time_ = float(parts_.back().start), playing_ = true;
    ImGui::SameLine();
    const std::string clock = Clock(time_) + " / " + Clock(float(total_));
    ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x - 120));
    float t = time_;
    if (ImGui::SliderFloat("##time", &t, 0, float(total_ - 1), clock.c_str()))
      time_ = t;
    ImGui::SameLine();
    ImGui::Text(fr ? "partie %d sur %d" : "part %d of %d", part + 1, int(parts_.size()));
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
    if (ImGui::Checkbox(fr ? "Suivre le catcheur" : "Follow the superstar", &follow_))
      posed_ = -1;
    ImGui::SameLine();
    if (!model_error_.empty())
      ImGui::TextColored(red, "%s", fr ? "(pas de modèle pour lui : choisissez-en un autre)" : "(no model for him: pick another)");
    else
      ImGui::TextDisabled("%s", fr ? "Glisser : tourner, clic droit : déplacer, molette : zoom."
                                   : "Drag: turn, right drag: move, wheel: zoom.");
  }

  ID3D11Device *device_ = nullptr;
  const disc::GameDisc *w12_ = nullptr;
  bool open_ = false;
  std::string title_, error_, model_error_;
  std::vector<Star> stars_;
  yanm::Record body_, cam_;
  std::vector<Part> parts_;
  int total_ = 1;
  arena_view::View view_;
  int star_ = -1;
  bool loaded_ = false, playing_ = true, follow_ = true;
  float time_ = 0, posed_ = -1;
  std::string shot_;
  int shot_wait_ = 0;
};

}  // namespace entrance_preview
