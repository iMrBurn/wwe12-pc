// LOOK page: viewers, after the LOOK pages of Kaiko's SvR 2011 Mod Maker (icons_page.cpp, installed_page.cpp; credit in
// docs/mod_maker.md). Read-only:
//  - the game's pictures by what they are for: superstars' portraits (menu\menuHD.pac, DLC_HD.pac SSFA / SSFB / SSFC),
//    arena banners (MatchHD.pac MLYE/MUPA), loading screens (LoadHD.pac LOADSCnn), crowd signs (audience.pac
//    AUDE/BORD), the main menu superstar (MnLayHD.pac MLYE/EM00), title belts (MLYE/BELT), shows' logos (MLYE/TIMG);
//    each one can be saved as a PNG (to start a mod from it);
//  - the installed mods in 3D: added superstars, attires and arenas (their models, as the game will draw them).
#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "arena_page.h"
#include "arena_view.h"
#include "game_disc.h"
#include "imgui.h"
#include "media.h"
#include "svrfmt/archive.h"
#include "svrfmt/texture.h"
#include "wic_image.h"

namespace look_page {

namespace fs = std::filesystem;
using svrfmt::Bytes;

class Page {
 public:
  void Draw(bool fr, ID3D11Device *device, const disc::GameDisc &disc, const fs::path &mods_dir) {
    device_ = device;
    if (ImGui::BeginTabBar("look_tabs")) {
      if (ImGui::BeginTabItem(fr ? "Images du jeu" : "Game pictures")) {
        Pictures(fr, disc);
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem(fr ? "Mods installés en 3D" : "Installed mods in 3D")) {
        Installed(fr, mods_dir);
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }
  }

 private:
  struct Kind {
    const char *en, *fr, *pac, *group, *name;  // name: "" = every entry of the group; "*N" = names starting with N
  };
  static constexpr Kind kKinds[] = {
      {"Superstars' portraits (big)", "Portraits des catcheurs (grands)", "pac/menu/menuHD.pac", "SSFA", ""},
      {"Superstars' portraits (DLC)", "Portraits des catcheurs (DLC)", "pac/DLC_HD.pac", "SSFA", ""},
      {"Arena banners", "Bannières des arènes", "pac/menu/MatchHD.pac", "MLYE", "MUPA"},
      {"Loading screens", "Écrans de chargement", "pac/menu/LoadHD.pac", "LOAD", "*SC"},
      {"Crowd signs", "Pancartes du public", "pac/audience.pac", "AUDE", "BORD"},
      {"Main menu superstar", "Catcheur du menu principal", "pac/menu/MnLayHD.pac", "MLYE", "EM00"},
      {"Title belts", "Ceintures", "pac/menu/MnLayHD.pac", "MLYE", "BELT"},
      {"Shows' logos", "Logos des shows", "pac/menu/MnLayHD.pac", "MLYE", "TIMG"},
  };
  struct Pic {
    std::string name;
    Bytes dds;
    arena_page::Thumb thumb;
  };

  // Every DDS under a node (texture bundles too), named by where it is.
  static void Collect(svrfmt::ArcNode &n, const std::string &path, std::vector<Pic> &out, int depth) {
    const Bytes d = svrfmt::ArchiveData(n);
    if (d.size() > 128 && !memcmp(d.data(), "DDS ", 4)) {
      out.push_back({path, d, {}});
      return;
    }
    std::vector<svrfmt::BundleTexture> t;
    if (svrfmt::BundleRead(d, t)) {
      for (auto &x : t)
        out.push_back({path.empty() ? x.name : path + " " + x.name, std::move(x.data), {}});
      return;
    }
    if (depth < 4 && out.size() < 600 && svrfmt::ArchiveExpand(n))
      for (auto &k : n.kids)
        Collect(k, path.empty() ? std::to_string(k.id) : path + "/" + std::to_string(k.id), out, depth + 1);
  }

  void Load(const disc::GameDisc &disc, int kind) {
    pics_.clear();
    selected_ = -1;
    loaded_ = kind;
    const Kind &k = kKinds[kind];
    Bytes pac;
    svrfmt::Archive a;
    if (!disc.Read(k.pac, pac) || !svrfmt::ArchiveOpen(pac, a, nullptr))
      return;
    const std::string want = k.name;
    for (auto &e : a.root.kids) {
      if (e.group != k.group)
        continue;
      if (!want.empty() && (want[0] == '*' ? e.name.rfind(want.substr(1), 0) != 0 : e.name != want))
        continue;
      Collect(e, want.empty() || want[0] == '*' ? e.name : "", pics_, 0);
      if (pics_.size() >= 600)
        break;
    }
  }

  void Pictures(bool fr, const disc::GameDisc &disc) {
    if (!disc.Found()) {
      ImGui::TextDisabled("%s", fr ? "Fichiers du jeu introuvables." : "Game files not found.");
      return;
    }
    ImGui::SetNextItemWidth(320);
    if (ImGui::BeginCombo("##look_kind", fr ? kKinds[kind_].fr : kKinds[kind_].en)) {
      for (int i = 0; i < int(std::size(kKinds)); ++i)
        if (ImGui::Selectable(fr ? kKinds[i].fr : kKinds[i].en, i == kind_))
          kind_ = i;
      ImGui::EndCombo();
    }
    if (loaded_ != kind_)
      Load(disc, kind_);
    ImGui::SameLine();
    ImGui::TextDisabled(fr ? "%zu image(s)" : "%zu picture(s)", pics_.size());
    if (selected_ >= 0 && selected_ < int(pics_.size())) {
      ImGui::SameLine();
      if (ImGui::SmallButton(fr ? "Enregistrer en PNG..." : "Save as PNG..."))
        SavePng(fr, pics_[size_t(selected_)]);
    }
    ImGui::BeginChild("look_grid", ImVec2(0, 0), ImGuiChildFlags_Borders);
    const float box = 110, cell = box + 12;
    const int per_row = std::max(1, int(ImGui::GetContentRegionAvail().x / cell));
    int made = 0;
    for (int i = 0, col = 0; i < int(pics_.size()); ++i) {
      Pic &p = pics_[size_t(i)];
      if (!p.thumb.srv && made < 8) {  // (a few a frame: the page stays responsive)
        p.thumb = arena_page::MakeThumb(device_, p.dds);
        ++made;
      }
      if (col)
        ImGui::SameLine();
      ImGui::BeginGroup();
      ImGui::PushID(i);
      const ImVec2 at = ImGui::GetCursorScreenPos();
      if (ImGui::InvisibleButton("##p", ImVec2(box, box)))
        selected_ = i;
      ImGui::SetCursorScreenPos(at);
      arena_page::DrawPicture(p.thumb, box);
      if (selected_ == i)
        ImGui::GetWindowDrawList()->AddRect(at, ImVec2(at.x + box, at.y + box), IM_COL32(240, 190, 60, 255), 0, 0, 3);
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + box);
      ImGui::TextDisabled("%s", p.name.c_str());
      ImGui::PopTextWrapPos();
      ImGui::PopID();
      ImGui::EndGroup();
      col = (col + 1) % per_row;
    }
    ImGui::EndChild();
  }

  static void SavePng(bool fr, const Pic &p) {
    svrfmt::Image img;
    if (!svrfmt::DdsDecode(p.dds, img))
      return;
    std::wstring name;
    for (unsigned char c : p.name)
      name += std::isalnum(c) ? wchar_t(c) : L'_';
    const fs::path out = media::PickSave(fr, (name.empty() ? L"picture" : name) + L".png");
    if (!out.empty())
      wic_image::SavePng(out, img);
  }

  // ---- installed mods in 3D
  struct Mod {
    std::string label;
    fs::path file;
  };
  void Installed(bool fr, const fs::path &mods_dir) {
    if (ImGui::SmallButton(fr ? "Relire les mods" : "Read the mods again") || !listed_) {
      listed_ = true;
      mods_.clear();
      std::error_code ec;
      const struct {
        const char *kind, *file, *en, *fr;
      } kinds[] = {{"Superstars", "superstar/ch.pac", "Superstar", "Catcheur"},
                   {"Attires", "attire/attire.pac", "Attire", "Tenue"},
                   {"Arenas", "arena/arena.pac", "Arena", "Arène"}};
      for (const auto &k : kinds)
        for (fs::directory_iterator it(mods_dir / ".svrmods" / k.kind, ec), end; !ec && it != end; it.increment(ec)) {
          const fs::path f = it->path() / fs::u8path(k.file);
          if (fs::exists(f, ec))
            mods_.push_back({std::string(fr ? k.fr : k.en) + " : " + it->path().filename().string(), f});
        }
      shown_ = -1;
    }
    ImGui::BeginChild("look_mods", ImVec2(300, 0), ImGuiChildFlags_Borders);
    if (mods_.empty())
      ImGui::TextDisabled("%s", fr ? "Aucun catcheur, tenue ou arène installé." : "No superstar, attire or arena installed.");
    for (int i = 0; i < int(mods_.size()); ++i)
      if (ImGui::Selectable(mods_[size_t(i)].label.c_str(), i == shown_) && i != shown_) {
        shown_ = i;
        std::ifstream f(mods_[size_t(i)].file, std::ios::binary);
        const Bytes b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        view_error_.clear();
        if (!view_.Load(device_, b, &view_error_, 0))
          view_error_ = (fr ? "Vue 3D impossible : " : "No 3D view: ") + view_error_;
      }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("look_view", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (shown_ < 0) {
      ImGui::TextDisabled("%s", fr ? "Choisissez un mod à gauche." : "Pick a mod on the left.");
    } else if (!view_error_.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", view_error_.c_str());
    } else {
      if (ImGui::SmallButton(fr ? "Recentrer" : "Reset view"))
        view_.ResetCamera();
      ImGui::SameLine();
      ImGui::TextDisabled("%s", fr ? "clic gauche : tourner, clic droit : déplacer, molette : zoom"
                                   : "left drag: turn, right drag: move, wheel: zoom");
      const ImVec2 size(ImGui::GetContentRegionAvail().x, std::max(200.0f, ImGui::GetContentRegionAvail().y - 4));
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
    }
    ImGui::EndChild();
  }

  ID3D11Device *device_ = nullptr;
  int kind_ = 0, loaded_ = -1, selected_ = -1;
  std::vector<Pic> pics_;
  bool listed_ = false;
  std::vector<Mod> mods_;
  int shown_ = -1;
  arena_view::View view_;
  std::string view_error_;
};

}  // namespace look_page
