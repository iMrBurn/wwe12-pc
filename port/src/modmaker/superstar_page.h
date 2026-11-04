// Mod Maker, "Superstars" page: a superstar's attires (pac/ch/chNNN.pac: an EPK8 with one "EMD " entry per
// attire, "000NNNAA" = superstar NNN, attire AA; each a PACH with the body model (id 0), its textures (10: normal
// map and parts, 12: "color", 13-15: the colour masks of Superstar Threads) and the eyes (20000)). Pick a superstar
// and an attire, see its pictures and the superstar in 3D, replace or export pictures, and make a mod
// (.svrmod, type=attire, attire/attire.pac: that attire alone, attire_mod.h). An attire never grows past its shipped size (arena_edit.h Fit:
// the game loads it into memory made for it). An attire can also be replaced by one of SvR 2011 (the player's own
// chNNN.pac), converted by port_svr.h over the chosen attire (best: the same superstar in WWE '12).
#pragma once

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "arena_edit.h"
#include "arena_page.h"
#include "arena_view.h"
#include "attire_mod.h"
#include "game_disc.h"
#include "game_discs.h"
#include "game_process.h"
#include "imgui.h"
#include "mod_version.h"
#include "media.h"
#include "media_page.h"
#include "new_star_page.h"
#include "mods.h"
#include "port13_star.h"
#include "port_svr.h"
#include "strings2k.h"
#include "hair_donor.h"
#include "wic_image.h"

namespace superstar_page {

namespace fs = std::filesystem;
using arena_page::Thumb;
using svrfmt::Bytes;

class Page {
 public:
  void Draw(bool fr, ID3D11Device *device, const disc::GameDisc &disc, mods::Manager &mods) {
    disc_ = &disc;
    device_ = device;
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f), yellow(0.94f, 0.75f, 0.24f, 1.0f);
    ImGui::TextWrapped("%s", fr ? "Changez les images d'une tenue (peau, vêtements, accessoires, tatouages...) et "
                                  "voyez le catcheur en 3D. Pour retoucher : Exporter en PNG, modifier, Remplacer."
                                : "Change an attire's pictures (skin, clothes, gear, tattoos...) and see the superstar "
                                  "in 3D. To edit one: Export as PNG, edit it, Replace.");
    if (!disc.Found()) {
      ImGui::TextColored(red, "%s", fr ? "Fichiers du jeu introuvables." : "Game files not found.");
      return;
    }
    if (!listed_)
      List(disc);

    // Superstar, then attire.
    ImGui::SetNextItemWidth(300);
    if (ImGui::BeginCombo(fr ? "Catcheur" : "Superstar", star_ < 0 ? (fr ? "Choisir..." : "Choose...")
                                                                    : stars_[star_].name.c_str(),
                          ImGuiComboFlags_HeightLarge)) {
      ImGui::SetNextItemWidth(-1);
      if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
      ImGui::InputTextWithHint("##sfilter", fr ? "Rechercher" : "Search", star_filter_, sizeof(star_filter_));
      for (int i = 0; i < int(stars_.size()); ++i) {
        std::string a = stars_[i].name, b = star_filter_;
        for (auto *x : {&a, &b})
          std::transform(x->begin(), x->end(), x->begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (!b.empty() && a.find(b) == std::string::npos)
          continue;
        if (ImGui::Selectable(stars_[i].name.c_str(), i == star_)) {
          star_ = i;
          OpenStar(fr, disc);
        }
      }
      ImGui::EndCombo();
    }
    if (star_ >= 0 && !attires_.empty()) {
      ImGui::SameLine();
      ImGui::SetNextItemWidth(200);
      char label[64];
      snprintf(label, sizeof(label), fr ? "Tenue %d" : "Attire %d", attire_ + 1);
      if (ImGui::BeginCombo(fr ? "Tenue" : "Attire", label)) {
        for (int i = 0; i < int(attires_.size()); ++i) {
          snprintf(label, sizeof(label), fr ? "Tenue %d  (%s)" : "Attire %d  (%s)", i + 1, attires_[i].c_str());
          if (ImGui::Selectable(label, i == attire_)) {
            attire_ = i;
            OpenAttire(fr);
          }
        }
        ImGui::EndCombo();
      }
    }
    if (!error_.empty())
      ImGui::TextColored(red, "%s", error_.c_str());
    if (star_ < 0 || attires_.empty() || !loaded_)
      return;
    DrawPortSvr(fr, red, green);

    auto &texs = edit_.Textures();
    int changed = 0;
    for (const auto &t : texs)
      changed += t.replaced;
    const float h = std::max(300.0f, ImGui::GetContentRegionAvail().y - 100);
    // Pictures.
    ImGui::BeginChild("star_grid", ImVec2(ImGui::GetContentRegionAvail().x * 0.42f, h), ImGuiChildFlags_Borders);
    ImGui::TextDisabled(fr ? "%zu images, %d remplacée(s)" : "%zu pictures, %d replaced", texs.size(), changed);
    const float box = 84, cell = box + 14;
    const int per_row = std::max(1, int(ImGui::GetContentRegionAvail().x / cell));
    for (int i = 0, col = 0; i < int(texs.size()); ++i) {
      if (col)
        ImGui::SameLine();
      ImGui::BeginGroup();
      ImGui::PushID(i);
      const ImVec2 at = ImGui::GetCursorScreenPos();
      if (ImGui::InvisibleButton("##t", ImVec2(box, box)))
        selected_ = i;
      ImGui::SetCursorScreenPos(at);
      arena_page::DrawPicture(thumbs_[i], box);
      if (selected_ == i || texs[i].replaced)
        ImGui::GetWindowDrawList()->AddRect(at, ImVec2(at.x + box, at.y + box),
                                            selected_ == i ? IM_COL32(240, 190, 60, 255) : IM_COL32(140, 220, 110, 255),
                                            0, 0, selected_ == i ? 3.0f : 2.0f);
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + box);
      ImGui::TextDisabled("%s", edit_.Get(texs[i]).name.c_str());
      ImGui::PopTextWrapPos();
      ImGui::PopID();
      ImGui::EndGroup();
      col = (col + 1) % per_row;
    }
    ImGui::EndChild();
    ImGui::SameLine();
    // The selected picture, then the superstar in 3D.
    ImGui::BeginChild("star_right", ImVec2(0, h), ImGuiChildFlags_Borders);
    if (selected_ >= 0) {
      auto &t = texs[selected_];
      const auto &tex = edit_.Get(t);
      svrfmt::DdsInfo info;
      svrfmt::DdsInfoOf(tex.data, info);
      ImGui::Text("%s", tex.name.c_str());
      ImGui::SameLine();
      ImGui::TextDisabled("%d x %d", info.w, info.h);
      ImGui::SameLine();
      if (ImGui::SmallButton(fr ? "Remplacer..." : "Replace..."))
        Replace(fr);
      ImGui::SameLine();
      if (ImGui::SmallButton(fr ? "Exporter en PNG..." : "Export as PNG..."))
        Export(fr);
      if (t.replaced) {
        ImGui::SameLine();
        if (ImGui::SmallButton(fr ? "Remettre" : "Restore")) {
          edit_.Restore(t);
          thumbs_[selected_] = arena_page::MakeThumb(device_, edit_.Get(t).data);
          view_stale_ = true;
        }
      }
      if (!note_.empty())
        ImGui::TextColored(red, "%s", note_.c_str());
    } else {
      ImGui::TextDisabled("%s", fr ? "Choisissez une image à gauche pour la changer." : "Choose a picture on the left "
                                                                                       "to change it.");
    }
    Materials(fr);
    Draw3D(fr);
    ImGui::EndChild();

    // The mod.
    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##star_name", fr ? "Nom du mod" : "Mod name", name_, sizeof(name_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##star_author", fr ? "Auteur (facultatif)" : "Author (optional)", author_,
                             sizeof(author_));
    mod_version::Input("Version##star_version", version_, sizeof(version_));
    // As a new attire (the superstar keeps this one): the next free of his 4 (the disc's and the installed mods').
    const int free_attire = attires_[attire_].size() == 8 && attires_[attire_][7] == '2' ? FreeAttire(mods) : 0;
    if (!free_attire)
      as_new_ = false;
    ImGui::BeginDisabled(!free_attire);
    char new_label[96];
    if (free_attire)
      snprintf(new_label, sizeof(new_label), fr ? "En nouvelle tenue (tenue %d, garde l'originale)"
                                                : "As a new attire (attire %d, keeps the original)", free_attire);
    else
      snprintf(new_label, sizeof(new_label), "%s", fr ? "En nouvelle tenue (déjà 4 tenues)" : "As a new attire (4 already)");
    ImGui::Checkbox(new_label, &as_new_);
    ImGui::EndDisabled();
    // Its own name on the select screen and its own render (both optional: else the game's).
    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##attire_label", fr ? "Nom de la tenue (facultatif)" : "Attire name (optional)",
                             attire_label_, sizeof(attire_label_));
    ImGui::SameLine();
    if (ImGui::Button(fr ? "Son render (une image)..." : "Its render (a picture)...")) {
      const fs::path p = arena_page::PickOpen(fr, false);
      svrfmt::Image img;
      if (!p.empty() && wic_image::Load(p, img) && img.w > 0 && img.h > 0) {
        Bytes all[2][3];
        media_page::MakePortraits(img, all);
        for (int hd = 0; hd < 2; ++hd)
          for (int k = 0; k < 3; ++k)
            render_[hd][k] = std::move(all[hd][k]);
        render_thumb_ = arena_page::MakeThumb(device_, render_[1][1]);
      }
    }
    if (render_thumb_.srv) {
      ImGui::SameLine();
      arena_page::DrawPicture(render_thumb_, 48);
      ImGui::SameLine();
      if (ImGui::SmallButton(fr ? "Retirer##render" : "Remove##render")) {
        for (auto &hd : render_)
          for (auto &b : hd)
            b.clear();
        render_thumb_ = {};
      }
    }
    DrawRenderFromGame(fr);
    ImGui::SameLine();
    ImGui::BeginDisabled((changed == 0 && !ported_ && !edit_.MaterialsChanged() && !as_new_ && !attire_label_[0] &&
                          render_[1][1].empty()) ||
                         game::IsRunning());
    if (ImGui::Button(fr ? "Créer le mod et l'installer..." : "Make the mod and install it..."))
      Make(fr, mods);
    ImGui::EndDisabled();
    if (game::IsRunning())
      ImGui::TextColored(red, "%s", fr ? "Le jeu est lancé : fermez-le pour installer le mod."
                                       : "The game is running: close it to install the mod.");
    if (!message_.empty())
      ImGui::TextColored(message_ok_ ? green : red, "%s", message_.c_str());
    if (!reduced_note_.empty())
      ImGui::TextColored(yellow, "%s", reduced_note_.c_str());
  }

 private:
  const disc::GameDisc *disc_ = nullptr;  // (the game files, for the hair's collisions)

 public:
  const game_discs::Discs *discs_ = nullptr;  // My game discs (set by the Mod Maker): other games' attires
  fs::path user_root_;                         // Documents\wwe12 (set by the Mod Maker): the official DLC installed

 private:
  struct Star {
    int id = 0;
    std::string name, file;  // "pac/ch/ch139.pac"
    fs::path dlc;            // an official DLC superstar's file (its package's pac\ch\chNN_dlcMM.pac), else empty
    bool added = false;      // an added superstar (a NEW SUPERSTAR mod installed: dlc = its superstar\ch.pac, id = its slot)
  };

  void List(const disc::GameDisc &disc) {
    listed_ = true;
    const auto names = disc::SuperstarNames(disc);
    for (const auto &f : disc.List("pac/ch")) {
      // chNNN.pac (not chEtc.pac, not the DLC packages)
      if (f.size() != 9 || _strnicmp(f.c_str(), "ch", 2) || _stricmp(f.c_str() + 5, ".pac") || !isdigit(f[2]))
        continue;
      Star s;
      s.id = atoi(f.c_str() + 2);
      s.file = "pac/ch/" + f;
      const auto it = names.find(s.id);
      s.name = it != names.end() ? it->second : "Superstar " + std::to_string(s.id);
      stars_.push_back(s);
    }
    // The official DLC the game installed (Documents\wwe12\...\54510890\00000002\<package>, not our pack
    // WWE12PCMODS): their superstars' chNN_dlcMM.pac, named from the package's own chEtc.pac.
    std::error_code ec;
    std::set<int> have;
    for (const auto &s : stars_)
      have.insert(s.id);
    const fs::path content = user_root_ / "0000000000000000" / "54510890" / "00000002";
    std::vector<fs::path> packs;
    for (fs::directory_iterator it(content, ec), end; !user_root_.empty() && !ec && it != end; it.increment(ec))
      if (it->is_directory(ec) && it->path().filename() != "WWE12PCMODS" && fs::is_directory(it->path() / "pac" / "ch", ec))
        packs.push_back(it->path());
    std::sort(packs.begin(), packs.end());
    for (const auto &pk : packs) {
      disc::GameDisc d;
      if (!d.OpenPath(pk))
        continue;
      const auto dlc_names = disc::SuperstarNames(d);
      for (const auto &f : d.List("pac/ch")) {
        // chNN_dlcMM.pac
        if (f.size() < 12 || _strnicmp(f.c_str(), "ch", 2) || !isdigit(static_cast<unsigned char>(f[2])) ||
            f.find("_dlc") == std::string::npos || _stricmp(f.c_str() + f.size() - 4, ".pac"))
          continue;
        Star s;
        s.id = atoi(f.c_str() + 2);
        if (!have.insert(s.id).second)
          continue;
        s.file = "pac/ch/" + f;
        s.dlc = pk / "pac" / "ch" / f;
        const auto it = dlc_names.find(s.id), it2 = names.find(s.id);
        s.name = (it != dlc_names.end() ? it->second : it2 != names.end() ? it2->second : "Superstar " + std::to_string(s.id)) +
                 " (DLC)";
        stars_.push_back(s);
      }
    }
    // The added superstars installed (NEW SUPERSTAR mods, mods\.svrmods\Superstars\<id>, with their own model): their
    // number in the game is their slot (Documents\wwe12\superstar_slots.txt "<id>=<slot>"); an attire mod for that
    // number goes into their file (superstar_pack.h). Jay's reports n°4 / n°6, 2026-11-04: they weren't listed, so no
    // attire could be added to one after it was made.
    {
      wchar_t me[MAX_PATH];
      GetModuleFileNameW(nullptr, me, MAX_PATH);
      const fs::path mods_dir = fs::path(me).parent_path() / "mods" / ".svrmods" / "Superstars";
      std::ifstream slots(user_root_ / "superstar_slots.txt");
      for (std::string line; !user_root_.empty() && std::getline(slots, line);) {
        if (!line.empty() && line.back() == 0x0D)
          line.pop_back();
        const size_t eq = line.rfind('=');
        if (eq == std::string::npos)
          continue;
        const std::string key = line.substr(0, eq);
        const int slot = atoi(line.c_str() + eq + 1);
        const fs::path dir = mods_dir / key;
        if (slot < 50 || slot >= 373 || !fs::is_regular_file(dir / "superstar" / "ch.pac", ec) || fs::exists(dir / "disabled", ec))
          continue;
        Star s;
        s.id = slot;
        s.added = true;
        s.dlc = dir / "superstar" / "ch.pac";
        s.file = media::Utf8(s.dlc.wstring());
        s.name = key;
        std::ifstream m(dir / "manifest.txt");
        for (std::string l; std::getline(m, l);) {
          if (!l.empty() && l.back() == 0x0D)
            l.pop_back();
          if (l.rfind("name=", 0) == 0 && l.size() > 5)
            s.name = l.substr(5);
        }
        s.name += " (mod)";
        stars_.push_back(s);
      }
    }
    std::sort(stars_.begin(), stars_.end(), [](const Star &a, const Star &b) { return a.name < b.name; });
  }

  void OpenStar(bool fr, const disc::GameDisc &disc) {
    error_.clear();
    message_.clear();
    reduced_note_.clear();
    attires_.clear();
    loaded_ = false;
    if (!stars_[star_].dlc.empty()) {
      std::ifstream f(stars_[star_].dlc, std::ios::binary);
      file_.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }
    if (stars_[star_].dlc.empty() ? !disc.Read(stars_[star_].file, file_) : file_.empty()) {
      error_ = (fr ? "Lecture impossible : " : "Could not read: ") + stars_[star_].file;
      return;
    }
    std::string why;
    // (an added superstar's file has its base's attire names, "00026102": named as in the game, his slot's)
    if (stars_[star_].added) {
      svrfmt::Archive a;
      char num[8];
      snprintf(num, sizeof(num), "%03d", stars_[star_].id);
      if (svrfmt::ArchiveOpen(file_, a, &why)) {
        bool ok = true;
        for (size_t i = 0; i < a.root.kids.size() && ok; ++i) {
          std::string n = a.root.kids[i].name;
          if (n.size() == 8)
            ok = svrfmt::ArchiveRename(a.root, i, n.replace(3, 3, num), &why);
        }
        if (ok) {
          svrfmt::ArchiveTouch(a.root);
          file_ = svrfmt::ArchiveSave(a);
        }
      }
    }
    arena_edit::Arena probe;
    if (!probe.Load(file_, &why, 0)) {
      error_ = (fr ? "Fichier illisible : " : "Unreadable file: ") + why;
      return;
    }
    for (const auto &[group, name] : probe.Sections())
      attires_.push_back(name);
    attire_ = 0;
    snprintf(name_, sizeof(name_), "%s", stars_[star_].name.c_str());
    OpenAttire(fr);
  }

  void OpenAttire(bool fr) {
    loaded_ = false;
    ported_ = false;
    svr_note_.clear();
    thumbs_.clear();
    selected_ = -1;
    message_.clear();
    reduced_note_.clear();
    std::string why;
    if (!edit_.Load(file_, &why, attire_)) {
      error_ = (fr ? "Tenue illisible : " : "Unreadable attire: ") + why;
      return;
    }
    error_.clear();
    budget_ = edit_.SectionSize();
    for (const auto &t : edit_.Textures())
      thumbs_.push_back(arena_page::MakeThumb(device_, edit_.Get(t).data));
    loaded_ = true;
    view_stale_ = true;
  }

  // An attire of another game in place of the chosen one (or as a new one: "As a new attire"): its file (from My game
  // discs or one of the player's), one of its attires, then the port (SvR 2007-2011: port_svr.h; WWE '13 and the 2K
  // games: port13_star.h, as the New superstar page does).
  void DrawPortSvr(bool fr, const ImVec4 &red, const ImVec4 &green) {
    if (!ImGui::CollapsingHeader(fr ? "Porter une tenue d'un autre jeu (SvR, WWE '13, 2K)" : "Port an attire from another game (SvR, WWE '13, 2K)"))
      return;
    // From My game discs: a game, then one of its superstars (its pac/ch file: chNNN.pac; SvR 2008 / 2007 chNN.pac).
    const char *game_name = "";
    for (const auto &[title, name] : kGames)
      if (title == svr_game_)
        game_name = name;
    ImGui::SetNextItemWidth(160);
    if (discs_ && ImGui::BeginCombo(fr ? "##svr_game" : "##svr_game", svr_game_ ? game_name : (fr ? "Jeu..." : "Game..."))) {
      for (const auto &[title, name] : kGames)
        if (!discs_->PathOf(title).empty() && ImGui::Selectable(name, title == svr_game_) && title != svr_game_) {
          svr_game_ = title;
          svr_pick_name_.clear();
          ListSvrGame();
        }
      ImGui::EndCombo();
    }
    if (svr_game_) {
      ImGui::SameLine();
      ImGui::SetNextItemWidth(300);
      if (ImGui::BeginCombo(fr ? "Catcheur##svr_star" : "Superstar##svr_star", svr_pick_name_.c_str(), ImGuiComboFlags_HeightLarge)) {
        for (const auto &[file, nm] : svr_list_)
          if (ImGui::Selectable((nm + "  (" + file + ")").c_str())) {
            disc::GameDisc d;
            Bytes b;
            if (d.OpenPath(discs_->PathOf(svr_game_)) && d.Read("pac/ch/" + file, b)) {
              svr_pick_name_ = nm;
              LoadSvrBytes(fr, fs::path(game_name) / file, std::move(b));
              svr_from_ = svr_game_;
            } else {
              svr_note_ = (fr ? "Lecture impossible : " : "Could not read: ") + file;
            }
          }
        ImGui::EndCombo();
      }
    }
    ImGui::TextWrapped("%s", fr ? "La tenue choisie ci-dessus est remplacée par une tenue de SvR 2011 (le chNNN.pac de "
                                  "votre jeu) : son modèle et ses images. Choisissez si possible le même catcheur dans "
                                  "WWE '12 : son squelette et ses matériaux servent de base."
                                : "The attire chosen above is replaced by an attire of SvR 2011 (your game's chNNN.pac): "
                                  "its model and pictures. Best choose the same superstar in WWE '12: its skeleton and "
                                  "materials are the base.");
    if (ImGui::Button(fr ? "Ou un fichier (chNNN.pac)..." : "Or a file (chNNN.pac)...")) {
      const fs::path p = arena_page::PickOpen(fr, true);
      if (!p.empty())
        LoadSvr(fr, p);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", svr_path_.empty() ? (fr ? "(SvR, WWE '13, 2K)" : "(SvR, WWE '13, 2K)")
                                                : media::Utf8(svr_path_.wstring()).c_str());
    if (!svr_attires_.empty()) {
      char label[64];
      snprintf(label, sizeof(label), fr ? "Tenue %d  (%s)" : "Attire %d  (%s)", svr_attire_ + 1,
               svr_attires_[svr_attire_].c_str());
      ImGui::SetNextItemWidth(260);
      if (ImGui::BeginCombo(fr ? "Tenue de SvR" : "SvR attire", label)) {
        for (int i = 0; i < int(svr_attires_.size()); ++i) {
          snprintf(label, sizeof(label), fr ? "Tenue %d  (%s)" : "Attire %d  (%s)", i + 1, svr_attires_[i].c_str());
          if (ImGui::Selectable(label, i == svr_attire_))
            svr_attire_ = i;
        }
        ImGui::EndCombo();
      }
      ImGui::SameLine();
      snprintf(label, sizeof(label), fr ? "Porter sur la tenue %d" : "Port onto attire %d", attire_ + 1);
      ImGui::BeginDisabled(ported_);
      if (ImGui::Button(label))
        PortSvr(fr);
      ImGui::EndDisabled();
    }
    if (!svr_note_.empty()) {
      ImGui::PushTextWrapPos(0);
      ImGui::TextColored(ported_ ? green : red, "%s", svr_note_.c_str());
      ImGui::PopTextWrapPos();
    }
  }

  void LoadSvr(bool fr, const fs::path &p) {
    svr_note_.clear();
    svr_attires_.clear();
    svr_attire_ = 0;
    std::ifstream f(p, std::ios::binary);
    svr_pick_name_.clear();
    LoadSvrBytes(fr, p, Bytes(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()));
  }

  void LoadSvrBytes(bool fr, const fs::path &p, Bytes file) {
    if (ported_)
      OpenAttire(fr);  // (another one: from the attire as shipped again)
    svr_from_ = 0;
    svr_note_.clear();
    svr_attires_.clear();
    svr_attire_ = 0;
    svr_path_ = p;
    svr_file_ = std::move(file);
    // (an EPK8 of "EMD " entries, any names: WWE 2K16's all "0001"; WWE 2K17's an HSPC of SHDCs)
    svrfmt::Archive a;
    std::string why;
    if (svr_file_.empty() || !svrfmt::ArchiveOpen(svr_file_, a, &why)) {
      svr_note_ = (fr ? "Fichier illisible : " : "Unreadable file: ") + why;
      return;
    }
    svr_hspc_ = a.root.kind == svrfmt::ArcKind::Hspc;
    for (const auto &k : a.root.kids)
      if (svr_hspc_ ? svrfmt::ArchiveKindOf(k) == "SHDC" : k.group == "EMD ")
        svr_attires_.push_back(svr_hspc_ ? "2K17" : k.name);
    if (svr_attires_.empty())
      svr_note_ = fr ? "Ce n'est pas un fichier de catcheur." : "Not a superstar file.";
  }

  // The superstars of game svr_game_ on My game discs: its pac/ch files, named from its strings.
  void ListSvrGame() { ListGame(svr_game_, svr_list_); }
  void ListGame(uint32_t title, std::vector<std::pair<std::string, std::string>> &svr_list_) const {
    svr_list_.clear();
    disc::GameDisc d;
    if (!discs_ || !d.OpenPath(discs_->PathOf(title)))
      return;
    const auto names = strings2k::SuperstarNames(d);
    std::set<int> seen;
    for (const auto &f : d.List("pac/ch")) {
      size_t k = 2;
      while (k < f.size() && isdigit(static_cast<unsigned char>(f[k])))
        ++k;
      if (k == 2 || k > 5 || _strnicmp(f.c_str(), "ch", 2) != 0 || f.size() < 4 ||
          _stricmp(f.c_str() + f.size() - 4, ".pac") != 0 ||
          (k != f.size() - 4 && _strnicmp(f.c_str() + k, "_dlc", 4) != 0))
        continue;
      const int n = atoi(f.c_str() + 2);
      if (!seen.insert(n).second)
        continue;
      const auto it = names.find(n);
      svr_list_.push_back({f, it != names.end() && it->second.size() > 1 ? it->second : f.substr(0, f.size() - 4)});
    }
    std::sort(svr_list_.begin(), svr_list_.end(), [](const auto &x, const auto &y) { return x.second < y.second; });
  }

  void PortSvr(bool fr) {
    svr_note_.clear();
    svrfmt::Archive s, w;
    std::string why;
    port_svr::Report rep;
    port13_star::Report rep13;
    // (WWE '13 and the 2K games: picked from My game discs, or a 2K17 file)
    const bool w13 = svr_hspc_ || svr_from_ == 0x545108B4 || svr_from_ == 0x545408B2 || svr_from_ == 0x545408B6 ||
                     svr_from_ == 0x315A07D2 || svr_from_ == 0x545408BA;
    if (!svrfmt::ArchiveOpen(svr_file_, s, &why) || !svrfmt::ArchiveOpen(file_, w, &why) ||
        size_t(svr_attire_) >= s.root.kids.size() || size_t(attire_) >= w.root.kids.size() ||
        !(w13 ? port13_star::PortAttire(s.root.kids[size_t(svr_attire_)], w.root.kids[size_t(attire_)], &rep13, &why)
              : port_svr::PortAttire(s.root.kids[size_t(svr_attire_)], w.root.kids[size_t(attire_)], &rep, &why,
                                     disc_ ? &hair_donor::Collision(*disc_) : nullptr))) {
      svr_note_ = (fr ? "Portage impossible : " : "Can't port it: ") + why;
      return;
    }
    svrfmt::ArchiveTouch(w.root);
    const Bytes out = svrfmt::ArchiveSave(w);
    if (!edit_.Load(out, &why, attire_)) {  // (the budget stays the shipped attire's)
      svr_note_ = (fr ? "Portage illisible : " : "Unreadable port: ") + why;
      return;
    }
    thumbs_.clear();
    selected_ = -1;
    for (const auto &t : edit_.Textures())
      thumbs_.push_back(arena_page::MakeThumb(device_, edit_.Get(t).data));
    view_stale_ = true;
    ported_ = true;
    message_.clear();
    reduced_note_.clear();
    char b[256];
    snprintf(b, sizeof(b),
             fr ? "Portée : %d maillages convertis%s. Voyez-la en 3D, puis créez le mod ci-dessous."
                : "Ported: %d meshes converted%s. See it in 3D, then make the mod below.",
             w13 ? rep13.meshes : rep.meshes,
             !w13 && rep.unmapped_bones ? (fr ? " (quelques os sans équivalent suivent leur parent)"
                                      : " (a few bones without a counterpart follow their parent)")
                                : "");
    svr_note_ = b;
  }

  void Replace(bool fr) {
    note_.clear();
    const fs::path p = arena_page::PickOpen(fr, false);
    if (p.empty())
      return;
    svrfmt::Image img;
    if (!wic_image::Load(p, img)) {
      note_ = fr ? "Cette image ne s'ouvre pas." : "This picture can't be opened.";
      return;
    }
    auto &t = edit_.Textures()[selected_];
    svrfmt::DdsInfo info;
    if (!svrfmt::DdsInfoOf(edit_.Original(t).data, info) || info.format == svrfmt::DxtFormat::kUnknown) {
      note_ = fr ? "Format de texture inconnu." : "Unknown texture format.";
      return;
    }
    if (img.w != info.w || img.h != info.h)
      img = svrfmt::Resize(img, info.w, info.h);
    edit_.Unreduce();
    edit_.Replace(t, svrfmt::DdsEncode(img, info.format, info.mips > 1));
    thumbs_[selected_] = arena_page::MakeThumb(device_, edit_.Get(t).data);
    view_stale_ = true;
    message_.clear();
    reduced_note_.clear();
  }

  void Export(bool fr) {
    const auto &tex = edit_.Get(edit_.Textures()[selected_]);
    svrfmt::Image img;
    if (!svrfmt::DdsDecode(tex.data, img))
      return;
    std::string base = stars_[star_].name + "_" + tex.name;
    std::wstring name;
    for (unsigned char c : base)
      name += std::isalnum(c) ? wchar_t(c) : L'_';
    const fs::path out = media::PickSave(fr, name + L".png");
    if (!out.empty() && !wic_image::SavePng(out, img))
      note_ = fr ? "Export impossible." : "Export failed.";
  }

  // Materials (Kaiko's Mod Maker has them): the shine and tint of the skin and of each part of the attire
  // (arena_edit::Material, written into the attire's body model).
  void Materials(bool fr) {
    if (!ImGui::CollapsingHeader(fr ? "Matières (brillance, teinte)" : "Materials (shine, tint)"))
      return;
    // Several parts picked (the boxes, as Kaiko's Materials tab): a change to one of them goes to all of them.
    const std::vector<std::string> groups = edit_.MaterialGroups();
    if (ImGui::SmallButton(fr ? "Tout cocher" : "Pick all"))
      picked_.insert(groups.begin(), groups.end());
    ImGui::SameLine();
    if (ImGui::SmallButton(fr ? "Tout décocher" : "Pick none"))
      picked_.clear();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", fr ? "(les parties cochées changent ensemble)" : "(the picked parts change together)");
    for (const auto &g : groups) {
      ImGui::PushID(g.c_str());
      arena_edit::Material m = edit_.MaterialOf(g);
      std::string label = g == "skin" ? (fr ? "Peau" : "Skin") : (fr ? "Tenue : " : "Attire: ") + g.substr(5);
      bool changed = false;
      bool pick = picked_.count(g) != 0;
      if (ImGui::Checkbox("##pick", &pick)) {
        if (pick)
          picked_.insert(g);
        else
          picked_.erase(g);
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(160);
      changed |= ImGui::SliderFloat("##shine", &m.shine, 0.0f, 3.0f, fr ? "brillance x%.2f" : "shine x%.2f");
      ImGui::SameLine();
      changed |= ImGui::ColorEdit3("##tint", m.tint, ImGuiColorEditFlags_NoInputs);
      ImGui::SameLine();
      ImGui::TextUnformatted(label.c_str());
      if (!m.Default()) {
        ImGui::SameLine();
        if (ImGui::SmallButton(fr ? "Remettre" : "Reset")) {
          m = arena_edit::Material();
          changed = true;
        }
      }
      if (changed) {
        edit_.SetMaterial(g, m);
        if (picked_.count(g))
          for (const auto &p : picked_)
            if (p != g && std::find(groups.begin(), groups.end(), p) != groups.end())
              edit_.SetMaterial(p, m);
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
          view_stale_ = true;
      }
      if (ImGui::IsItemDeactivatedAfterEdit())
        view_stale_ = true;
      ImGui::PopID();
    }
    ImGui::TextDisabled("%s", fr ? "La teinte multiplie la couleur de l'image (blanc = inchangé)."
                                 : "The tint multiplies the picture's colour (white = unchanged).");
  }

  void Draw3D(bool fr) {
    if (view_stale_) {
      view_stale_ = false;
      view_error_.clear();
      const Bytes now = edit_.Build();
      if (!view_.Load(device_, now, &view_error_, size_t(attire_)))
        view_error_ = (fr ? "Vue 3D impossible : " : "No 3D view: ") + view_error_;
    }
    if (!view_error_.empty()) {
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", view_error_.c_str());
      return;
    }
    if (ImGui::SmallButton(fr ? "Recentrer" : "Reset view"))
      view_.ResetCamera();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", fr ? "clic gauche : tourner, clic droit : déplacer, molette : zoom"
                                 : "left drag: turn, right drag: move, wheel: zoom");
    const ImVec2 size(ImGui::GetContentRegionAvail().x, std::max(200.0f, ImGui::GetContentRegionAvail().y - 4));
    ID3D11ShaderResourceView *pic = view_.Render(int(size.x), int(size.y));
    if (!pic)
      return;
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

  static constexpr std::pair<uint32_t, const char *> kGames[] = {
      {0x5451085D, "SvR 2011"}, {0x54510844, "SvR 2010"}, {0x54510826, "SvR 2009"}, {0x5451080B, "SvR 2008"},
      {0x545107E0, "SvR 2007"}, {0x545108B4, "WWE '13"},  {0x545408B2, "WWE 2K14"}, {0x545408B6, "WWE 2K15"},
      {0x315A07D2, "WWE 2K16"}, {0x545408BA, "WWE 2K17"}};
  static const char *GameName(uint32_t title) {
    for (const auto &[t, name] : kGames)
      if (t == title)
        return name;
    return "";
  }

  // Its render from a game of My game discs: a game, one of its superstars (the ported one's to start with), its
  // portrait as that game shows it (new_star_page's way, each game's own place).
  void DrawRenderFromGame(bool fr) {
    if (!discs_)
      return;
    if (!render_game_ && svr_from_) {  // (the superstar ported, to start with)
      render_game_ = svr_from_;
      ListGame(render_game_, render_list_);
      render_pick_ = -1;
      for (int i = 0; i < int(render_list_.size()); ++i)
        if (render_list_[i].second == svr_pick_name_)
          render_pick_ = i;
    }
    ImGui::TextDisabled("%s", fr ? "Ou celui d'un jeu :" : "Or a game's:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140);
    if (ImGui::BeginCombo("##render_game", render_game_ ? GameName(render_game_) : (fr ? "Jeu..." : "Game..."))) {
      for (const auto &[title, name] : kGames)
        if (!discs_->PathOf(title).empty() && ImGui::Selectable(name, title == render_game_) && title != render_game_) {
          render_game_ = title;
          render_pick_ = -1;
          ListGame(render_game_, render_list_);
        }
      ImGui::EndCombo();
    }
    if (!render_game_)
      return;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(240);
    if (ImGui::BeginCombo("##render_star", render_pick_ >= 0 ? render_list_[size_t(render_pick_)].second.c_str() : "",
                          ImGuiComboFlags_HeightLarge)) {
      for (int i = 0; i < int(render_list_.size()); ++i)
        if (ImGui::Selectable((render_list_[size_t(i)].second + "  (" + render_list_[size_t(i)].first + ")").c_str(),
                              i == render_pick_))
          render_pick_ = i;
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(render_pick_ < 0);
    if (ImGui::Button(fr ? "Prendre son render" : "Take its render")) {
      Bytes all[2][3];
      const int number = atoi(render_list_[size_t(render_pick_)].first.c_str() + 2);
      if (render_helper_.discs_ = discs_; render_helper_.PortraitsFor(render_game_, number, all)) {
        for (int hd = 0; hd < 2; ++hd)
          for (int k = 0; k < 3; ++k)
            render_[hd][k] = std::move(all[hd][k]);
        render_thumb_ = arena_page::MakeThumb(device_, render_[1][1]);
        render_note_.clear();
      } else {
        render_note_ = fr ? "Pas de render trouvé pour lui dans ce jeu." : "No render found for him in that game.";
      }
    }
    ImGui::EndDisabled();
    if (!render_note_.empty())
      ImGui::TextDisabled("%s", render_note_.c_str());
  }

  // The first attire (1-4) neither the superstar's file nor an installed attire mod gives, 0 if none.
  int FreeAttire(const mods::Manager &mods) const {
    const Star &s = stars_[star_];
    std::set<std::string> used(attires_.begin(), attires_.end());
    for (const auto &m : mods.List()) {
      if (m.type != "attire")
        continue;
      std::ifstream f(m.dir / "manifest.txt");
      std::string line, superstar, attire;
      while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r')
          line.pop_back();
        if (line.rfind("superstar=", 0) == 0)
          superstar = line.substr(10);
        else if (line.rfind("attire=", 0) == 0)
          attire = line.substr(7);
      }
      if (atoi(superstar.c_str()) == s.id)
        used.insert(attire);
    }
    for (int n = 1; n <= 4; ++n)
      if (!used.count(attire_mod::EntryName(s.id, n)))
        return n;
    return 0;
  }

  void Make(bool fr, mods::Manager &mods) {
    message_ok_ = false;
    reduced_note_.clear();
    Bytes out;
    const auto reduced = edit_.Fit(budget_, out, true);
    if (edit_.SectionSize() > budget_) {
      message_ = fr ? "Impossible de faire tenir la tenue dans sa taille d'origine, même en réduisant ses images."
                    : "The attire can't be made to fit its shipped size, even with its pictures reduced.";
      return;
    }
    if (!reduced.empty()) {
      reduced_note_ = fr ? "Réduites de moitié pour tenir dans la taille d'origine :" : "Halved to fit the shipped size:";
      for (const auto &n : reduced)
        reduced_note_ += " " + n;
    }
    const Star &s = stars_[star_];
    const int new_attire = as_new_ ? FreeAttire(mods) : 0;
    if (as_new_ && !new_attire) {
      message_ = fr ? "Ce catcheur a déjà 4 tenues." : "This superstar has 4 attires already.";
      return;
    }
    const std::string entry = new_attire ? attire_mod::EntryName(s.id, new_attire) : attires_[attire_];
    std::string id = "attire_ch" + std::to_string(s.id) + "_" + entry + "_";
    std::wstring file;
    for (unsigned char c : std::string(name_)) {
      id += std::isalnum(c) ? char(c) : '_';
      file += std::isalnum(c) ? wchar_t(c) : L'_';
    }
    const fs::path save = media::PickSave(fr, (file.empty() ? L"attire" : file) + L".svrmod");
    if (save.empty())
      return;
    // Only this attire goes in the mod (attire_mod.h): the game puts it into the superstar's file at startup.
    Bytes one;
    std::string why;
    if (!attire_mod::OneAttire(out, attires_[attire_], one, &why)) {
      message_ = (fr ? "Tenue introuvable : " : "Attire not found: ") + why;
      return;
    }
    if (new_attire) {  // (renamed: the game adds it to his attires, superstar_pack.h)
      svrfmt::Archive a;
      if (!svrfmt::ArchiveOpen(one, a, &why) || a.root.kids.empty() || !svrfmt::ArchiveRename(a.root, 0, entry, &why)) {
        message_ = (fr ? "Tenue illisible : " : "Unreadable attire: ") + why;
        return;
      }
      svrfmt::ArchiveTouch(a.root);
      one = svrfmt::ArchiveSave(a);
    }
    std::string manifest = "game=wwe12\ntype=attire\nid=" + id + "\nname=" + std::string(name_) + "\nversion=" + mod_version::Of(version_) + "\n" +
                           "superstar=" + std::to_string(s.id) + "\nattire=" + entry + "\n" +
                           (attire_label_[0] ? "attire_name=" + std::string(attire_label_) + "\n" : std::string());
    if (author_[0])
      manifest += std::string("author=") + author_ + "\n";
    manifest += "description=" + s.name + ", attire " + std::to_string(new_attire ? new_attire : attire_ + 1) + " (" + entry +
                (new_attire ? ", a new one made from attire " + std::to_string(attire_ + 1) : std::string()) + ")" +
                (ported_ ? ", replaced by an attire ported from another game (the player's own file)" : "") +
                ", made with the WWE '12 Mod Maker.\n";
    std::vector<std::pair<std::string, Bytes>> zip = {{"manifest.txt", Bytes(manifest.begin(), manifest.end())},
                                                      {"attire/attire.pac", std::move(one)}};
    // (SSFA / SSFB / SSFC "ANNN" in the game's pack, superstar_pack.h: the select screen's, the VS screen's...)
    for (int hd = 0; hd < 2; ++hd)
      for (int k = 0; k < 3; ++k)
        if (!render_[hd][k].empty())
          zip.push_back({std::string("attire/SSF") + char('A' + k) + (hd ? "_HD.dds" : "_SD.dds"), render_[hd][k]});
    if (!mods::Manager::WriteZip(save, zip)) {
      message_ = (fr ? "Impossible d'écrire " : "Could not write ") + media::Utf8(save.wstring());
      return;
    }
    mods.InstallFile(fr, save);
    message_ok_ = true;
    message_ = (fr ? "Mod créé et installé : " : "Mod made and installed: ") + media::Utf8(save.wstring());
  }

  ID3D11Device *device_ = nullptr;
  bool listed_ = false, loaded_ = false;
  std::vector<Star> stars_;
  int star_ = -1, attire_ = 0, selected_ = -1;
  bool as_new_ = false;  // the mod adds this attire as a new one
  char attire_label_[32] = {};  // its own name on the select screen (empty: the game's)
  Bytes render_[2][3];          // its own portraits [SD, HD][SSFA, SSFB, SSFC] (empty: none)
  Thumb render_thumb_;
  uint32_t render_game_ = 0;  // its render from a game: that game, its superstars (file, name), the one picked
  std::vector<std::pair<std::string, std::string>> render_list_;
  int render_pick_ = -1;
  std::string render_note_;
  new_star_page::Page render_helper_;  // (finds a superstar's portraits in each game)
  char star_filter_[64] = {}, name_[64] = {}, author_[64] = {}, version_[16] = {};
  Bytes file_;
  std::vector<std::string> attires_;
  arena_edit::Arena edit_;
  size_t budget_ = 0;
  std::vector<Thumb> thumbs_;
  arena_view::View view_;
  bool view_stale_ = true;
  std::set<std::string> picked_;  // Materials: the parts changed together
  std::string error_, note_, view_error_, message_, reduced_note_;
  // Porting from SvR 2011.
  fs::path svr_path_;
  Bytes svr_file_;
  uint32_t svr_game_ = 0;                                      // the game picked in My game discs
  std::vector<std::pair<std::string, std::string>> svr_list_;  // its superstars: (file, name)
  std::string svr_pick_name_;
  bool svr_hspc_ = false;                                       // (a 2K17 file)
  uint32_t svr_from_ = 0;  // the game of the file loaded, picked in My game discs (0: a file of the player's, SvR)
  std::vector<std::string> svr_attires_;
  int svr_attire_ = 0;
  bool ported_ = false;
  std::string svr_note_;
  bool message_ok_ = false;
};

}  // namespace superstar_page
