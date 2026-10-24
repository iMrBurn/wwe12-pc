// Mod Maker, "New superstar" page: a superstar added to the game, next to the others (docs/mod_maker.md, "Ajouter
// des catcheurs"). The mod (.svrmod, type=superstar) holds only its name, its base superstar (whose record, moves
// and portraits it starts from) and, if it has its own look, superstar/ch.pac; the game makes a DLC pack of the
// superstar mods at startup (src/native/wwe12_superstar_mods.cpp, modmaker/superstar_pack.h). Its look: the base's,
// another superstar file (a chNNN.pac), or an attire of SvR 2011 ported over the base's first attire (port_svr.h).
#pragma once

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "arena_edit.h"
#include "arena_page.h"
#include "arena_view.h"
#include "game_disc.h"
#include "game_process.h"
#include "imgui.h"
#include "media.h"
#include "mods.h"
#include "port_svr.h"

namespace new_star_page {

namespace fs = std::filesystem;
using svrfmt::Bytes;

class Page {
 public:
  void Draw(bool fr, ID3D11Device *device, const disc::GameDisc &disc, mods::Manager &mods) {
    device_ = device;
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f), yellow(0.94f, 0.75f, 0.24f, 1.0f);
    ImGui::TextWrapped("%s", fr ? "Ajoutez un catcheur au jeu, à côté des autres (il apparaît dans la case DLC de "
                                  "l'écran de choix). Il part d'un catcheur de base (ses prises, ses stats, ses "
                                  "portraits) ; son apparence peut être celle de la base, un autre fichier de catcheur, "
                                  "ou une tenue portée de SvR 2011."
                                : "Add a superstar to the game, next to the others (shown in the select screen's DLC "
                                  "tile). It starts from a base superstar (moves, ratings and portraits); its look can "
                                  "be the base's, another superstar file, or an attire ported from SvR 2011.");
    if (!disc.Found()) {
      ImGui::TextColored(red, "%s", fr ? "Fichiers du jeu introuvables." : "Game files not found.");
      return;
    }
    if (!listed_)
      List(disc);

    // 1. Name and base.
    ImGui::SeparatorText(fr ? "1. Nom et catcheur de base" : "1. Name and base superstar");
    ImGui::SetNextItemWidth(300);
    ImGui::InputTextWithHint("##ns_name", fr ? "Nom (comme en jeu)" : "Name (as in the game)", name_, sizeof(name_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300);
    if (ImGui::BeginCombo(fr ? "Base" : "Base", base_ < 0 ? (fr ? "Choisir..." : "Choose...") : stars_[base_].name.c_str(),
                          ImGuiComboFlags_HeightLarge)) {
      ImGui::SetNextItemWidth(-1);
      if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
      ImGui::InputTextWithHint("##ns_filter", fr ? "Rechercher" : "Search", filter_, sizeof(filter_));
      for (int i = 0; i < int(stars_.size()); ++i) {
        std::string a = stars_[i].name, b = filter_;
        for (auto *x : {&a, &b})
          std::transform(x->begin(), x->end(), x->begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (!b.empty() && a.find(b) == std::string::npos)
          continue;
        if (ImGui::Selectable(stars_[i].name.c_str(), i == base_)) {
          base_ = i;
          OpenBase(fr, disc);
        }
      }
      ImGui::EndCombo();
    }
    if (!error_.empty())
      ImGui::TextColored(red, "%s", error_.c_str());
    if (base_ < 0)
      return;

    // 2. Look.
    ImGui::SeparatorText(fr ? "2. Apparence" : "2. Look");
    int look = look_;
    ImGui::RadioButton(fr ? "Celle de la base" : "The base's", &look, 0);
    ImGui::SameLine();
    ImGui::RadioButton(fr ? "Un autre fichier de catcheur (.pac)" : "Another superstar file (.pac)", &look, 1);
    ImGui::SameLine();
    ImGui::RadioButton(fr ? "Une tenue de SvR 2011" : "An attire from SvR 2011", &look, 2);
    if (look != look_) {
      look_ = look;
      note_.clear();
      if (look_ == 0)
        SetModel(base_file_);
      else
        SetModel(look_ == 1 ? other_ : ported_);
    }
    if (look_ == 1) {
      if (ImGui::Button(fr ? "Choisir le fichier (chNNN.pac)..." : "Choose the file (chNNN.pac)...")) {
        const fs::path p = arena_page::PickOpen(fr, true);
        if (!p.empty())
          LoadOther(fr, p);
      }
      ImGui::SameLine();
      ImGui::TextDisabled("%s", other_path_.empty() ? "" : media::Utf8(other_path_.wstring()).c_str());
    } else if (look_ == 2) {
      if (ImGui::Button(fr ? "Fichier de SvR 2011 (chNNN.pac)..." : "SvR 2011 file (chNNN.pac)...")) {
        const fs::path p = arena_page::PickOpen(fr, true);
        if (!p.empty())
          LoadSvr(fr, p);
      }
      ImGui::SameLine();
      ImGui::TextDisabled("%s", svr_path_.empty() ? (fr ? "(vos fichiers de SvR 2011)" : "(your SvR 2011 files)")
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
        if (ImGui::Button(fr ? "Porter" : "Port"))
          PortSvr(fr);
      }
    }
    if (!note_.empty()) {
      ImGui::PushTextWrapPos(0);
      ImGui::TextColored(note_ok_ ? green : red, "%s", note_.c_str());
      ImGui::PopTextWrapPos();
    }

    // 3D view of the model's first attire.
    const float h = std::max(260.0f, ImGui::GetContentRegionAvail().y - 110);
    ImGui::BeginChild("ns_view", ImVec2(0, h), ImGuiChildFlags_Borders);
    Draw3D(fr);
    ImGui::EndChild();

    // 3. The mod.
    ImGui::SeparatorText(fr ? "3. Le mod" : "3. The mod");
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##ns_author", fr ? "Auteur (facultatif)" : "Author (optional)", author_, sizeof(author_));
    ImGui::SameLine();
    const bool ready = name_[0] && !model_.empty() && (look_ == 0 || model_ != base_file_);
    ImGui::BeginDisabled(!ready || game::IsRunning());
    if (ImGui::Button(fr ? "Créer le mod et l'installer..." : "Make the mod and install it..."))
      Make(fr, mods);
    ImGui::EndDisabled();
    if (!name_[0]) {
      ImGui::SameLine();
      ImGui::TextDisabled("%s", fr ? "(donnez-lui un nom)" : "(give it a name)");
    }
    if (game::IsRunning())
      ImGui::TextColored(red, "%s", fr ? "Le jeu est lancé : fermez-le pour installer le mod."
                                       : "The game is running: close it to install the mod.");
    if (!message_.empty())
      ImGui::TextColored(message_ok_ ? green : red, "%s", message_.c_str());
    if (!reduced_.empty())
      ImGui::TextColored(yellow, "%s", reduced_.c_str());
  }

 private:
  struct Star {
    int id = 0;
    std::string name, file;
  };

  void List(const disc::GameDisc &disc) {
    listed_ = true;
    const auto names = disc::SuperstarNames(disc);
    for (const auto &f : disc.List("pac/ch")) {
      if (f.size() != 9 || _strnicmp(f.c_str(), "ch", 2) || _stricmp(f.c_str() + 5, ".pac") || !isdigit(f[2]))
        continue;
      Star s;
      s.id = atoi(f.c_str() + 2);
      s.file = "pac/ch/" + f;
      const auto it = names.find(s.id);
      if (it == names.end())
        continue;
      s.name = it->second;
      stars_.push_back(s);
    }
    std::sort(stars_.begin(), stars_.end(), [](const Star &a, const Star &b) { return a.name < b.name; });
  }

  void OpenBase(bool fr, const disc::GameDisc &disc) {
    error_.clear();
    message_.clear();
    reduced_.clear();
    base_file_.clear();
    ported_.clear();
    if (!disc.Read(stars_[base_].file, base_file_)) {
      error_ = (fr ? "Lecture impossible : " : "Could not read: ") + stars_[base_].file;
      return;
    }
    if (look_ == 2 && !svr_attires_.empty())
      PortSvr(fr);  // (ported over the new base)
    SetModel(look_ == 0 ? base_file_ : look_ == 1 ? other_ : ported_);
  }

  void SetModel(const Bytes &m) {
    model_ = m;
    view_stale_ = true;
  }

  static Bytes ReadAll(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    return Bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  }

  // A superstar file: an EPK8 of "EMD " entries named 000NNNAA.
  static bool IsSuperstarFile(const Bytes &b, std::vector<std::string> *attires, std::string *why) {
    svrfmt::Archive a;
    if (!svrfmt::ArchiveOpen(b, a, why))
      return false;
    for (const auto &k : a.root.kids) {
      if (k.group != "EMD " || k.name.size() != 8) {
        if (why)
          *why = "not a superstar file (chNNN.pac)";
        return false;
      }
      if (attires)
        attires->push_back(k.name);
    }
    return !a.root.kids.empty();
  }

  void LoadOther(bool fr, const fs::path &p) {
    other_path_ = p;
    other_ = ReadAll(p);
    std::string why;
    if (!IsSuperstarFile(other_, nullptr, &why)) {
      other_.clear();
      note_ok_ = false;
      note_ = (fr ? "Fichier refusé : " : "File refused: ") + why;
    } else {
      note_ok_ = true;
      note_ = fr ? "Fichier chargé." : "File loaded.";
    }
    SetModel(other_);
  }

  void LoadSvr(bool fr, const fs::path &p) {
    svr_path_ = p;
    svr_file_ = ReadAll(p);
    svr_attires_.clear();
    svr_attire_ = 0;
    std::string why;
    if (!IsSuperstarFile(svr_file_, &svr_attires_, &why)) {
      svr_attires_.clear();
      note_ok_ = false;
      note_ = (fr ? "Fichier refusé : " : "File refused: ") + why;
    } else {
      note_.clear();
    }
  }

  // The SvR attire over the base's first attire (port_svr.h), made to fit that attire's room.
  void PortSvr(bool fr) {
    note_.clear();
    ported_.clear();
    svrfmt::Archive s, w;
    std::string why;
    port_svr::Report rep;
    if (!svrfmt::ArchiveOpen(svr_file_, s, &why) || !svrfmt::ArchiveOpen(base_file_, w, &why) ||
        size_t(svr_attire_) >= s.root.kids.size() || w.root.kids.empty() ||
        !port_svr::PortAttire(s.root.kids[size_t(svr_attire_)], w.root.kids[0], &rep, &why)) {
      note_ok_ = false;
      note_ = (fr ? "Portage impossible : " : "Can't port it: ") + why;
      SetModel(ported_);
      return;
    }
    const size_t budget = w.root.kids[0].stored.size();  // (before Save: the base's attire as shipped)
    svrfmt::ArchiveTouch(w.root);
    const Bytes out = svrfmt::ArchiveSave(w);
    arena_edit::Arena fit;
    if (!fit.Load(out, &why, 0)) {
      note_ok_ = false;
      note_ = (fr ? "Portage illisible : " : "Unreadable port: ") + why;
      return;
    }
    const auto halved = fit.Fit(budget, ported_, true);
    reduced_.clear();
    if (!halved.empty()) {
      reduced_ = fr ? "Réduites de moitié pour tenir dans la place d'une tenue :" : "Halved to fit an attire's room:";
      for (const auto &n : halved)
        reduced_ += " " + n;
    }
    note_ok_ = true;
    char b[200];
    snprintf(b, sizeof(b), fr ? "Portée : %d maillages convertis (tenue 1 du nouveau catcheur)."
                              : "Ported: %d meshes converted (the new superstar's attire 1).",
             rep.meshes);
    note_ = b;
    SetModel(ported_);
  }

  void Draw3D(bool fr) {
    if (model_.empty()) {
      ImGui::TextDisabled("%s", fr ? "Pas encore de modèle." : "No model yet.");
      return;
    }
    if (view_stale_) {
      view_stale_ = false;
      view_error_.clear();
      if (!view_.Load(device_, model_, &view_error_, 0))
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

  void Make(bool fr, mods::Manager &mods) {
    message_ok_ = false;
    std::string id = "superstar_";
    std::wstring file;
    for (unsigned char c : std::string(name_)) {
      id += std::isalnum(c) ? char(std::tolower(c)) : '_';
      file += std::isalnum(c) ? wchar_t(c) : L'_';
    }
    const fs::path save = media::PickSave(fr, (file.empty() ? L"superstar" : file) + L".svrmod");
    if (save.empty())
      return;
    const Star &b = stars_[base_];
    std::string manifest = "game=wwe12\ntype=superstar\nid=" + id + "\nname=" + std::string(name_) +
                           "\nbase=" + std::to_string(b.id) + "\nversion=1.0\n";
    if (author_[0])
      manifest += std::string("author=") + author_ + "\n";
    manifest += "description=" + std::string(name_) + ", a new superstar starting from " + b.name +
                (look_ == 2 ? ", its look ported from SvR 2011 (the player's own file)"
                 : look_ == 1 ? ", with its own look"
                              : "") +
                ", made with the WWE '12 Mod Maker.\n";
    std::vector<std::pair<std::string, Bytes>> entries = {{"manifest.txt", Bytes(manifest.begin(), manifest.end())}};
    if (look_ != 0)
      entries.push_back({"superstar/ch.pac", model_});
    else
      entries.push_back({"superstar/look.txt", Bytes{'b', 'a', 's', 'e', '\n'}});  // (a mod needs a superstar/ file)
    if (!mods::Manager::WriteZip(save, entries)) {
      message_ = (fr ? "Impossible d'écrire " : "Could not write ") + media::Utf8(save.wstring());
      return;
    }
    mods.InstallFile(fr, save);
    message_ok_ = true;
    message_ = (fr ? "Mod créé et installé : " : "Mod made and installed: ") + media::Utf8(save.wstring()) +
               (fr ? " (au prochain lancement du jeu : case DLC de l'écran de choix)"
                   : " (next time the game starts: the select screen's DLC tile)");
  }

  ID3D11Device *device_ = nullptr;
  bool listed_ = false;
  std::vector<Star> stars_;
  int base_ = -1, look_ = 0;
  char name_[32] = {}, author_[64] = {}, filter_[64] = {};
  Bytes base_file_, other_, ported_, model_, svr_file_;
  fs::path other_path_, svr_path_;
  std::vector<std::string> svr_attires_;
  int svr_attire_ = 0;
  arena_view::View view_;
  bool view_stale_ = true;
  std::string error_, note_, view_error_, message_, reduced_;
  bool note_ok_ = false, message_ok_ = false;
};

}  // namespace new_star_page
