// LOOK page: viewers, after the LOOK pages of Kaiko's SvR 2011 Mod Maker (icons_page.cpp, installed_page.cpp; credit in
// docs/mod_maker.md). Read-only:
//  - the game's pictures by what they are for: superstars' portraits (menu\menuHD.pac, DLC_HD.pac SSFA / SSFB / SSFC),
//    arena banners (MatchHD.pac MLYE/MUPA), loading screens (LoadHD.pac LOADSCnn), crowd signs (audience.pac
//    AUDE/BORD), the main menu superstar (MnLayHD.pac MLYE/EM00), title belts (MLYE/BELT), shows' logos (MLYE/TIMG);
//    each one can be saved as a PNG (to start a mod from it);
//  - the installed mods in 3D: added superstars, attires and arenas (their models, as the game will draw them);
//  - animations: a superstar playing the game's motions (pac\m.pac, motions.h / yanm.h), after Kaiko's Animations page.
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
#include "motions.h"
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
      // (test aid: WWE12_MM_ANIM=<bank>,<motion>[,<block>,<frame>] opens the Animations tab on that motion, paused)
      static const char *test = getenv("WWE12_MM_ANIM");
      if (ImGui::BeginTabItem("Animations", nullptr, test && !an_test_done_ ? ImGuiTabItemFlags_SetSelected : 0)) {
        Animations(fr, disc);
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

  // ---- animations
  struct AnimStar {
    int id = 0;
    std::string name, file;
  };
  void ListAnimStars(const disc::GameDisc &disc) {
    an_listed_ = true;
    const auto names = disc::SuperstarNames(disc);
    for (const auto &f : disc.List("pac/ch")) {
      if (f.size() != 9 || _strnicmp(f.c_str(), "ch", 2) || _stricmp(f.c_str() + 5, ".pac") || !isdigit(f[2]))
        continue;
      const int id = atoi(f.c_str() + 2);
      const auto it = names.find(id);
      if (it != names.end())
        an_stars_.push_back({id, it->second, "pac/ch/" + f});
    }
    std::sort(an_stars_.begin(), an_stars_.end(), [](const AnimStar &a, const AnimStar &b) { return a.name < b.name; });
    for (int i = 0; i < int(an_stars_.size()); ++i) {
      if (an_stars_[size_t(i)].id == 139)  // (John Cena first, Randy Orton facing him)
        an_star_ = i;
      if (an_stars_[size_t(i)].id == 135)
        an_other_ = i;
    }
    if (an_other_ < 0 && !an_stars_.empty())
      an_other_ = 0;
  }
  // A star's first attire: (file, entry index).
  bool ReadStar(const disc::GameDisc &disc, int i, Bytes &file, size_t &section) {
    section = 0;
    if (i < 0 || !disc.Read(an_stars_[size_t(i)].file, file))
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
  void LoadAnimOther(const disc::GameDisc &disc) {
    an_other_loaded_ = an_other_;
    Bytes f;
    size_t section = 0;
    std::string why;
    if (ReadStar(disc, an_other_, f, section))
      an_view_.Add(f, &why, section, 1);
    an_posed_ = -1;
  }
  void LoadAnimStar(bool fr, const disc::GameDisc &disc) {
    an_error_.clear();
    an_attires_.clear();
    an_file_.clear();
    if (an_star_ < 0 || !disc.Read(an_stars_[size_t(an_star_)].file, an_file_)) {
      an_error_ = fr ? "Lecture du catcheur impossible." : "Could not read the superstar.";
      return;
    }
    svrfmt::Archive a;
    if (svrfmt::ArchiveOpen(an_file_, a, nullptr))
      for (auto &e : a.root.kids)  // his attires: entries 000NNNA2 (A = 0-3)
        if (e.name.size() == 8 && e.name[7] == '2')
          an_attires_.push_back({int(&e - &a.root.kids[0]), e.name});
    if (an_attires_.empty())
      an_attires_.push_back({0, ""});
    an_attire_ = 0;
    LoadAnimModel(fr);
  }
  void LoadAnimModel(bool fr) {
    an_view_.keep_skin = true;
    an_error_.clear();
    if (!an_view_.Load(device_, an_file_, &an_error_, size_t(an_attires_[size_t(an_attire_)].first)))
      an_error_ = (fr ? "Vue 3D impossible : " : "No 3D view: ") + an_error_;
    else
      an_view_.SetAngles(3.14159265f, 0.12f, 420);
    an_posed_ = -1;
    an_other_loaded_ = -1;  // (Load took him away too)
  }
  void LoadAnimBank(bool fr, const disc::GameDisc &disc) {
    an_bank_loaded_ = an_bank_;
    an_motions_.clear();
    an_motion_ = -1;
    an_record_ = {};
    std::string why;
    if (!motions::ReadBank(disc, motions::Banks()[size_t(an_bank_)].entry, an_motions_, &why))
      an_bank_error_ = (fr ? "Animations illisibles : " : "Motions not read: ") + why;
    else
      an_bank_error_.clear();
  }
  void PickMotion(int i) {
    an_motion_ = i;
    an_record_ = {};
    yanm::Read(an_motions_[size_t(i)].record, an_record_);
    an_block_ = 0;
    an_frame_ = 0;
    an_playing_ = true;
    an_posed_ = -1;
  }
  // A block's name: "PPPPWWWW", P its part, W who (0000 the superstar, 0001 the other person).
  static std::string BlockLabel(bool fr, const std::string &n) {
    if (n.size() != 8)
      return n;
    const int part = atoi(n.substr(0, 4).c_str()), who = atoi(n.substr(4).c_str());
    char b[64];
    if (who >= 2)
      snprintf(b, sizeof(b), fr ? "Partie %d (personne %d)" : "Part %d (person %d)", part + 1, who + 1);
    else
      snprintf(b, sizeof(b), fr ? "Partie %d%s" : "Part %d%s", part + 1,
               who ? (fr ? " (l'adversaire)" : " (the opponent)") : "");
    return b;
  }

  void Animations(bool fr, const disc::GameDisc &disc) {
    if (!disc.Found()) {
      ImGui::TextDisabled("%s", fr ? "Fichiers du jeu introuvables." : "Game files not found.");
      return;
    }
    if (!an_listed_)
      ListAnimStars(disc);
    if (an_file_.empty() && an_star_ >= 0 && an_error_.empty())
      LoadAnimStar(fr, disc);
    if (const char *test = getenv("WWE12_MM_ANIM"); test && !an_test_done_) {
      an_test_done_ = true;
      int bank = 0, motion = 0, block = 0;
      float frame = 0;
      sscanf(test, "%d,%d,%d,%f", &bank, &motion, &block, &frame);
      an_bank_ = std::clamp(bank, 0, int(motions::Banks().size()) - 1);
      LoadAnimBank(fr, disc);
      for (int i = 0; i < int(an_motions_.size()); ++i)
        if (an_motions_[size_t(i)].number == motion)
          PickMotion(i);
      an_block_ = std::clamp(block, 0, std::max(0, int(an_record_.blocks.size()) - 1));
      an_frame_ = frame;
      an_playing_ = false;
    }
    if (an_bank_loaded_ != an_bank_)
      LoadAnimBank(fr, disc);
    if (an_error_.empty() && !an_file_.empty() && an_other_loaded_ != an_other_)
      LoadAnimOther(disc);
    ImGui::BeginChild("an_left", ImVec2(320, 0), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted(fr ? "Catcheur" : "Superstar");
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##an_star", an_star_ >= 0 ? an_stars_[size_t(an_star_)].name.c_str() : "")) {
      for (int i = 0; i < int(an_stars_.size()); ++i)
        if (ImGui::Selectable((an_stars_[size_t(i)].name + "##s" + std::to_string(i)).c_str(), i == an_star_)) {
          an_star_ = i;
          LoadAnimStar(fr, disc);
        }
      ImGui::EndCombo();
    }
    if (an_attires_.size() > 1) {
      ImGui::TextDisabled("%s", fr ? "Tenue" : "Attire");
      for (int i = 0; i < int(an_attires_.size()); ++i) {
        ImGui::SameLine();
        if (ImGui::RadioButton((std::to_string(i + 1) + "##at").c_str(), an_attire_ == i) && an_attire_ != i) {
          an_attire_ = i;
          LoadAnimModel(fr);
        }
      }
    }
    ImGui::Checkbox(fr ? "Adversaire" : "Opponent", &an_other_on_);
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip("%s", fr ? "Les prises à deux : il joue la part de l'adversaire en face."
                                 : "Two-person moves: he plays the opponent's part, facing him.");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##an_other", an_other_ >= 0 ? an_stars_[size_t(an_other_)].name.c_str() : "")) {
      for (int i = 0; i < int(an_stars_.size()); ++i)
        if (ImGui::Selectable((an_stars_[size_t(i)].name + "##o" + std::to_string(i)).c_str(), i == an_other_))
          an_other_ = i;
      ImGui::EndCombo();
    }
    ImGui::Spacing();
    ImGui::TextUnformatted(fr ? "Catégorie" : "Kind");
    ImGui::SetNextItemWidth(-1);
    const auto &banks = motions::Banks();
    if (ImGui::BeginCombo("##an_bank", fr ? banks[size_t(an_bank_)].fr : banks[size_t(an_bank_)].en)) {
      for (int i = 0; i < int(banks.size()); ++i)
        if (ImGui::Selectable(fr ? banks[size_t(i)].fr : banks[size_t(i)].en, i == an_bank_))
          an_bank_ = i;
      ImGui::EndCombo();
    }
    if (!an_bank_error_.empty())
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", an_bank_error_.c_str());
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##an_find", fr ? "Numéro de l'animation" : "Motion number", an_filter_, sizeof(an_filter_));
    ImGui::BeginChild("an_list", ImVec2(0, an_record_.blocks.size() > 1 ? -150.0f : 0.0f), ImGuiChildFlags_Borders);
    for (int i = 0; i < int(an_motions_.size()); ++i) {
      const std::string n = std::to_string(an_motions_[size_t(i)].number);
      if (an_filter_[0] && n.find(an_filter_) == std::string::npos)
        continue;
      if (ImGui::Selectable(((fr ? "Animation " : "Motion ") + n + "##m" + std::to_string(i)).c_str(), i == an_motion_))
        PickMotion(i);
    }
    ImGui::EndChild();
    if (an_record_.blocks.size() > 1) {
      ImGui::TextDisabled("%s", fr ? "Ses parties" : "Its parts");
      ImGui::BeginChild("an_blocks", ImVec2(0, 0), ImGuiChildFlags_Borders);
      for (int i = 0; i < int(an_record_.blocks.size()); ++i) {
        const auto &b = an_record_.blocks[size_t(i)];
        char label[96];
        snprintf(label, sizeof(label), "%s, %.1f s##b%d", BlockLabel(fr, b.name).c_str(), b.frames / 60.0f, i);
        if (ImGui::Selectable(label, i == an_block_)) {
          an_block_ = i;
          an_frame_ = 0;
          an_posed_ = -1;
        }
      }
      ImGui::EndChild();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("an_view", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (!an_error_.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", an_error_.c_str());
      ImGui::EndChild();
      return;
    }
    // playback
    const yanm::Block *blk = an_block_ < int(an_record_.blocks.size()) ? &an_record_.blocks[size_t(an_block_)] : nullptr;
    const int frames = blk ? std::max(1, blk->frames) : 1;
    if (blk && an_playing_) {
      an_frame_ += ImGui::GetIO().DeltaTime * 60.0f * an_speed_;
      if (an_frame_ > float(frames - 1))
        an_frame_ = an_loop_ ? std::fmod(an_frame_, float(frames)) : float(frames - 1);
    }
    const float shown = blk ? an_frame_ : -1.0f;
    if (shown != an_posed_) {
      an_posed_ = shown;
      std::map<std::string, arena_view::BonePose> pose;
      if (blk)
        for (const auto &t : blk->tracks) {
          if (t.name.empty() || t.name[0] == '#')
            continue;
          arena_view::BonePose bp;
          yanm::Sample(t, an_frame_, bp.has_rot, bp.rot, bp.has_pos, bp.pos);
          pose[t.name] = bp;
        }
      an_view_.SetPose(pose);
      // the other person: the same part's track for person 0001, when the superstar plays person 0000
      const yanm::Block *other = nullptr;
      if (blk && blk->name.size() == 8 && blk->name.compare(4, 4, "0000") == 0)
        for (const auto &b : an_record_.blocks)
          if (b.name == blk->name.substr(0, 4) + "0001")
            other = &b;
      an_view_.show_others = an_other_on_ && other;
      if (an_view_.show_others) {
        std::map<std::string, arena_view::BonePose> op;
        for (const auto &t : other->tracks) {
          if (t.name.empty() || t.name[0] == '#')
            continue;
          arena_view::BonePose bp;
          yanm::Sample(t, an_frame_, bp.has_rot, bp.rot, bp.has_pos, bp.pos);
          op[t.name] = bp;
        }
        an_view_.SetPose(op, 1);
      }
      if (an_follow_ && blk)  // (the camera on his hips: some motions take him far across the ring)
        an_view_.SetTarget(an_view_.Hips());
    }
    if (ImGui::Button(an_playing_ ? (fr ? "Pause" : "Pause") : (fr ? "Lecture" : "Play"), ImVec2(80, 0)))
      an_playing_ = !an_playing_;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x - 420));
    float f = an_frame_;
    if (ImGui::SliderFloat("##an_frame", &f, 0, float(frames - 1), fr ? "image %.0f" : "frame %.0f"))
      an_frame_ = f, an_playing_ = false;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100);
    ImGui::SliderFloat("##an_speed", &an_speed_, 0.1f, 2.0f, fr ? "vitesse x%.1f" : "speed x%.1f");
    ImGui::SameLine();
    ImGui::Checkbox(fr ? "Boucle" : "Loop", &an_loop_);
    ImGui::SameLine();
    ImGui::Checkbox(fr ? "Suivre" : "Follow", &an_follow_);
    ImGui::SameLine();
    if (ImGui::SmallButton(fr ? "Recentrer" : "Reset view"))
      an_view_.ResetCamera(), an_view_.SetAngles(3.14159265f, 0.12f, 420);
    if (!blk)
      ImGui::TextDisabled("%s", fr ? "Choisissez une catégorie puis une animation à gauche."
                                   : "Pick a kind, then a motion on the left.");
    else
      ImGui::TextDisabled(fr ? "Animation %d, %s : %d images (%.1f s à 60 images/s). Clic gauche : tourner, droit : "
                               "déplacer, molette : zoom."
                             : "Motion %d, %s: %d frames (%.1f s at 60 frames/s). Left drag: turn, right: move, "
                               "wheel: zoom.",
                          an_record_.number, BlockLabel(fr, blk->name).c_str(), frames, frames / 60.0f);
    const ImVec2 size(ImGui::GetContentRegionAvail().x, std::max(200.0f, ImGui::GetContentRegionAvail().y - 4));
    if (ID3D11ShaderResourceView *pic = an_view_.Render(int(size.x), int(size.y))) {
      ImGui::Image(ImTextureRef(ImTextureID(reinterpret_cast<intptr_t>(pic))), size);
      if (ImGui::IsItemHovered()) {
        const ImGuiIO &io = ImGui::GetIO();
        if (io.MouseWheel != 0)
          an_view_.Zoom(io.MouseWheel);
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0))
          an_view_.Orbit(io.MouseDelta.x, io.MouseDelta.y);
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0))
          an_view_.Pan(io.MouseDelta.x, io.MouseDelta.y);
      }
    }
    ImGui::EndChild();
  }

  bool an_listed_ = false;
  std::vector<AnimStar> an_stars_;
  int an_star_ = -1, an_attire_ = 0, an_bank_ = 0, an_bank_loaded_ = -1, an_motion_ = -1, an_block_ = 0;
  Bytes an_file_;
  std::vector<std::pair<int, std::string>> an_attires_;  // (entry index, name)
  std::vector<motions::Motion> an_motions_;
  yanm::Record an_record_;
  std::string an_error_, an_bank_error_;
  char an_filter_[16] = {};
  float an_frame_ = 0, an_speed_ = 1, an_posed_ = -2;
  bool an_playing_ = true, an_loop_ = true, an_follow_ = true, an_other_on_ = true;
  int an_other_ = -1, an_other_loaded_ = -1;
  bool an_test_done_ = false;
  arena_view::View an_view_;

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
