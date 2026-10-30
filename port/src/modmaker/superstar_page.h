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
#include <string>
#include <vector>

#include "arena_edit.h"
#include "arena_page.h"
#include "arena_view.h"
#include "attire_mod.h"
#include "game_disc.h"
#include "game_process.h"
#include "imgui.h"
#include "mod_version.h"
#include "media.h"
#include "mods.h"
#include "port_svr.h"
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
    ImGui::SameLine();
    ImGui::BeginDisabled((changed == 0 && !ported_ && !edit_.MaterialsChanged()) || game::IsRunning());
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
  struct Star {
    int id = 0;
    std::string name, file;  // "pac/ch/ch139.pac"
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
    std::sort(stars_.begin(), stars_.end(), [](const Star &a, const Star &b) { return a.name < b.name; });
  }

  void OpenStar(bool fr, const disc::GameDisc &disc) {
    error_.clear();
    message_.clear();
    reduced_note_.clear();
    attires_.clear();
    loaded_ = false;
    if (!disc.Read(stars_[star_].file, file_)) {
      error_ = (fr ? "Lecture impossible : " : "Could not read: ") + stars_[star_].file;
      return;
    }
    arena_edit::Arena probe;
    std::string why;
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

  // An attire of SvR 2011 in place of the chosen one: its file, one of its attires, then the port (port_svr.h).
  void DrawPortSvr(bool fr, const ImVec4 &red, const ImVec4 &green) {
    if (!ImGui::CollapsingHeader(fr ? "Porter une tenue de SvR 2011" : "Port an attire from SvR 2011"))
      return;
    ImGui::TextWrapped("%s", fr ? "La tenue choisie ci-dessus est remplacée par une tenue de SvR 2011 (le chNNN.pac de "
                                  "votre jeu) : son modèle et ses images. Choisissez si possible le même catcheur dans "
                                  "WWE '12 : son squelette et ses matériaux servent de base."
                                : "The attire chosen above is replaced by an attire of SvR 2011 (your game's chNNN.pac): "
                                  "its model and pictures. Best choose the same superstar in WWE '12: its skeleton and "
                                  "materials are the base.");
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
    svr_path_ = p;
    std::ifstream f(p, std::ios::binary);
    svr_file_.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    arena_edit::Arena probe;
    std::string why;
    if (svr_file_.empty() || !probe.Load(svr_file_, &why, 0)) {
      svr_note_ = (fr ? "Fichier illisible : " : "Unreadable file: ") + why;
      return;
    }
    for (const auto &[group, name] : probe.Sections())
      svr_attires_.push_back(name);
  }

  void PortSvr(bool fr) {
    svr_note_.clear();
    svrfmt::Archive s, w;
    std::string why;
    port_svr::Report rep;
    if (!svrfmt::ArchiveOpen(svr_file_, s, &why) || !svrfmt::ArchiveOpen(file_, w, &why) ||
        size_t(svr_attire_) >= s.root.kids.size() || size_t(attire_) >= w.root.kids.size() ||
        !port_svr::PortAttire(s.root.kids[size_t(svr_attire_)], w.root.kids[size_t(attire_)], &rep, &why,
                              disc_ ? &hair_donor::Collision(*disc_) : nullptr)) {
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
             rep.meshes,
             rep.unmapped_bones ? (fr ? " (quelques os sans équivalent suivent leur parent)"
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
    for (const auto &g : edit_.MaterialGroups()) {
      ImGui::PushID(g.c_str());
      arena_edit::Material m = edit_.MaterialOf(g);
      std::string label = g == "skin" ? (fr ? "Peau" : "Skin") : (fr ? "Tenue : " : "Attire: ") + g.substr(5);
      bool changed = false;
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
    std::string id = "attire_ch" + std::to_string(s.id) + "_" + attires_[attire_] + "_";
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
    std::string manifest = "game=wwe12\ntype=attire\nid=" + id + "\nname=" + std::string(name_) + "\nversion=" + mod_version::Of(version_) + "\n" +
                           "superstar=" + std::to_string(s.id) + "\nattire=" + attires_[attire_] + "\n";
    if (author_[0])
      manifest += std::string("author=") + author_ + "\n";
    manifest += "description=" + s.name + ", attire " + std::to_string(attire_ + 1) + " (" + attires_[attire_] + ")" +
                (ported_ ? ", replaced by an attire ported from SvR 2011 (the player's own file)" : "") +
                ", made with the WWE '12 Mod Maker.\n";
    if (!mods::Manager::WriteZip(save, {{"manifest.txt", Bytes(manifest.begin(), manifest.end())},
                                        {"attire/attire.pac", std::move(one)}})) {
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
  char star_filter_[64] = {}, name_[64] = {}, author_[64] = {}, version_[16] = {};
  Bytes file_;
  std::vector<std::string> attires_;
  arena_edit::Arena edit_;
  size_t budget_ = 0;
  std::vector<Thumb> thumbs_;
  arena_view::View view_;
  bool view_stale_ = true;
  std::string error_, note_, view_error_, message_, reduced_note_;
  // Porting from SvR 2011.
  fs::path svr_path_;
  Bytes svr_file_;
  std::vector<std::string> svr_attires_;
  int svr_attire_ = 0;
  bool ported_ = false;
  std::string svr_note_;
  bool message_ok_ = false;
};

}  // namespace superstar_page
