// Mod Maker, MOVES & ENTRANCES page: a move played in 3D on the superstar (person 0) with another one as his opponent
// (person 1), as the LOOK page's Animations (look_page.h) and Kaiko's "Play" (his SvR 2011 Mod Maker, with credit).
// A move's full motion record (moves_edit::MotionIndex) holds a block per person ("00000000" him, "00000001" the
// other); each frame poses both.
#pragma once

#include <algorithm>
#include <cmath>
#include <map>
#include <string>

#include "arena_view.h"
#include "game_disc.h"
#include "imgui.h"
#include "yanm.h"

namespace move_preview {

using Bytes = std::vector<uint8_t>;

class Preview {
 public:
  // The superstar shown (`number`'s pac\ch\chNNN.pac, his first attire) and his opponent; loaded again when they change.
  void Set(ID3D11Device *dev, const disc::GameDisc &w12, int number, int opponent) {
    if (!dev || (number == number_ && opponent == opponent_))
      return;
    number_ = number;
    opponent_ = opponent;
    error_.clear();
    Bytes f;
    size_t section = 0;
    if (number <= 0 || !Read(w12, number, f, section)) {
      error_ = "no model";
      loaded_ = false;
      return;
    }
    view_.keep_skin = true;
    loaded_ = view_.Load(dev, f, &error_, section);
    if (loaded_) {
      view_.SetAngles(3.14159265f, 0.12f, 420);
      Bytes o;
      size_t os = 0;
      std::string why;
      if (opponent > 0 && Read(w12, opponent, o, os))
        view_.Add(o, &why, os, 1);
    }
    posed_ = -2;
  }
  // A move's motion to play (its full record), from its first frame; empty: back to standing.
  void Play(const Bytes &record, const std::string &label) {
    record_ = {};
    label_ = label;
    if (!record.empty())
      yanm::Read(record, record_);
    frame_ = 0;
    playing_ = true;
    standing_ = false;
    posed_ = -2;
  }
  bool HasMotion() const { return !record_.blocks.empty(); }
  // Standing: a motion's first frame, still (his signature move's: the bind pose has his arms out), shown with no label.
  void Stand(const Bytes &record) {
    Play(record, "");
    playing_ = false;
    standing_ = true;
  }

  void Draw(bool fr, const ImVec2 &size) {
    ImGui::PushTextWrapPos(0);
    struct Pop {
      ~Pop() { ImGui::PopTextWrapPos(); }
    } pop;
    if (!loaded_) {
      ImGui::TextDisabled("%s", fr ? "(choisissez un catcheur pour le voir ici)" : "(choose a superstar to see him here)");
      return;
    }
    // his block and the other person's (the same part, person 0001)
    const yanm::Block *me = nullptr, *other = nullptr;
    for (const auto &b : record_.blocks)
      if (b.name.size() == 8 && b.name.compare(4, 4, "0000") == 0) {
        me = &b;
        break;
      }
    if (!me && !record_.blocks.empty())
      me = &record_.blocks[0];
    if (me && me->name.size() == 8)
      for (const auto &b : record_.blocks)
        if (b.name == me->name.substr(0, 4) + "0001")
          other = &b;
    const int frames = me ? std::max(1, me->frames) : 1;
    if (me && playing_) {
      frame_ += ImGui::GetIO().DeltaTime * 60.0f;
      if (frame_ > float(frames - 1))
        frame_ = std::fmod(frame_, float(frames));
    }
    const float shown = me ? frame_ : -1.0f;
    if (shown != posed_) {
      posed_ = shown;
      view_.SetPose(Pose(me));
      view_.show_others = other != nullptr && !standing_;  // (standing: alone, not his opponent lying there)
      if (other)
        view_.SetPose(Pose(other), 1);
      if (me)
        view_.SetTarget(view_.Hips());
    }
    const ImVec2 at = ImGui::GetCursorScreenPos();
    if (ID3D11ShaderResourceView *pic = view_.Render(int(size.x), int(size.y))) {
      ImGui::Image(ImTextureRef(ImTextureID(reinterpret_cast<intptr_t>(pic))), size);
      if (ImGui::IsItemHovered()) {
        const ImGuiIO &io = ImGui::GetIO();
        if (io.MouseWheel != 0)
          view_.Zoom(io.MouseWheel);
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0))
          view_.Orbit(io.MouseDelta.x, io.MouseDelta.y);
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0))
          view_.Pan(io.MouseDelta.x, io.MouseDelta.y);
      }
    }
    (void)at;
    if (me && !standing_) {
      ImGui::TextDisabled("%s", label_.c_str());
      if (ImGui::SmallButton(playing_ ? "Pause" : (fr ? "Lecture" : "Play")))
        playing_ = !playing_;
      ImGui::SameLine();
      if (ImGui::SmallButton(fr ? "Arrêter" : "Stop"))
        Play({}, "");
    } else {
      ImGui::TextDisabled("%s", fr ? "Jouer : la prise sur lui (adversaire : un autre catcheur du jeu). Glisser : tourner, "
                                     "molette : zoom."
                                   : "Play: the move on him (opponent: another superstar of the game). Drag: turn, wheel: "
                                     "zoom.");
    }
    if (!error_.empty())
      ImGui::TextDisabled("(%s)", error_.c_str());
  }

 private:
  static bool Read(const disc::GameDisc &w12, int number, Bytes &file, size_t &section) {
    char rel[32];
    snprintf(rel, sizeof(rel), "pac/ch/ch%d.pac", number);
    section = 0;
    if (!w12.Read(rel, file))
      return false;
    svrfmt::Archive a;
    if (svrfmt::ArchiveOpen(file, a, nullptr))
      for (size_t k = 0; k < a.root.kids.size(); ++k)
        if (a.root.kids[k].name.size() == 8 && a.root.kids[k].name[7] == '2') {
          section = k;
          break;
        }
    return true;
  }
  std::map<std::string, arena_view::BonePose> Pose(const yanm::Block *b) const {
    std::map<std::string, arena_view::BonePose> pose;
    if (b)
      for (const auto &t : b->tracks) {
        if (t.name.empty() || t.name[0] == '#')
          continue;
        arena_view::BonePose bp;
        yanm::Sample(t, frame_, bp.has_rot, bp.rot, bp.has_pos, bp.pos);
        pose[t.name] = bp;
      }
    return pose;
  }

  arena_view::View view_;
  bool loaded_ = false;
  int number_ = -1, opponent_ = -1;
  yanm::Record record_;
  std::string label_, error_;
  float frame_ = 0, posed_ = -2;
  bool playing_ = true, standing_ = false;
};

}  // namespace move_preview
