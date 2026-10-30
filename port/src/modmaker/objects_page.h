// BELTS & WEAPONS page: the pictures of the title belts and of the other objects (pac\evt\evtobj.pac SOBJ/NNNN, named
// by SOBJ/LIST: 64-byte records {u16 object, name}), of the weapons (pac/bg/bgEtc.pac STG3/WPON, STG3/WPO2,
// STGR/WPON) and of the loading screens (pac\menu\LoadHD.pac LOAD/NNNN), replaced from any picture into a texture
// pack (type=textures: textures/<name>_<hash>.dds, read by the game's texture packs, src/native/wwe12_texture_packs.cpp).
// Kaiko's Mod Maker retextures belts and weapons the same way (credit in docs/mod_maker.md). A texture is known by XXH3
// of its top level as svrfmt's DDS holds it (untiled, tightly packed blocks): the game's own hash (checked on the
// menu's dumps, 2026-10-30), so a pack needs no dump.
#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

#define XXH_INLINE_ALL
#include <xxhash.h>

#include "arena_page.h"
#include "game_disc.h"
#include "imgui.h"
#include "media.h"
#include "mod_version.h"
#include "mods.h"
#include "svrfmt/archive.h"
#include "svrfmt/texture.h"
#include "wic_image.h"

namespace objects_page {

namespace fs = std::filesystem;
using svrfmt::Bytes;

// The game's hash of a texture (wwe12_texture_packs.h): XXH3 of its top level's blocks.
inline bool GameHash(const Bytes &dds, uint64_t &hash) {
  svrfmt::DdsInfo i;
  if (!svrfmt::DdsInfoOf(dds, i) || i.w <= 0 || i.h <= 0)
    return false;
  const size_t bw = size_t(i.w + 3) / 4, bh = size_t(i.h + 3) / 4;
  size_t n = 0;
  if (i.format == svrfmt::DxtFormat::kDxt1)
    n = bw * bh * 8;
  else if (i.format == svrfmt::DxtFormat::kDxt3 || i.format == svrfmt::DxtFormat::kDxt5)
    n = bw * bh * 16;
  else if (i.format == svrfmt::DxtFormat::kArgb)
    n = size_t(i.w) * size_t(i.h) * 4;
  if (!n || dds.size() < 128 + n)
    return false;
  hash = XXH3_64bits(dds.data() + 128, n);
  return true;
}

class Page {
 public:
  void Draw(bool fr, ID3D11Device *device, const disc::GameDisc &game, mods::Manager &mods) {
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f);
    ImGui::TextWrapped("%s", fr ? "Les images des ceintures, des armes, des autres objets et des écrans de chargement. Remplacez-les par "
                                  "les vôtres : le pack de textures fait est installé tout de suite (page TEXTURES du "
                                  "launcher pour le couper)."
                                : "The pictures of the game's title belts, weapons, other objects and loading screens. Replace them with "
                                  "yours: the texture pack made is installed at once (launcher's TEXTURES page to turn "
                                  "it off).");
    if (objects_.empty()) {
      ImGui::BeginDisabled(!game.Found());
      if (ImGui::Button(fr ? "Lire les objets du jeu (quelques secondes)" : "Read the game's objects (a few seconds)"))
        Load(fr, game);
      ImGui::EndDisabled();
      if (!message_.empty())
        ImGui::TextColored(message_ok_ ? green : red, "%s", message_.c_str());
      return;
    }
    const char *kinds_fr[] = {"Ceintures", "Armes", "Autres objets", "Écrans de chargement"},
               *kinds_en[] = {"Title belts", "Weapons", "Other objects", "Loading screens"};
    for (int k = 0; k < 4; ++k) {
      if (k)
        ImGui::SameLine();
      if (ImGui::RadioButton(fr ? kinds_fr[k] : kinds_en[k], kind_ == k))
        kind_ = k;
    }
    ImGui::BeginChild("objects", ImVec2(300, 420), ImGuiChildFlags_Borders);
    for (size_t i = 0; i < objects_.size(); ++i) {
      if (objects_[i].kind != kind_)
        continue;
      ImGui::PushID(int(i));
      const bool changed = std::any_of(objects_[i].pics.begin(), objects_[i].pics.end(),
                                       [](const Pic &p) { return !p.mine.empty(); });
      if (ImGui::Selectable((objects_[i].label + (changed ? " *" : "")).c_str(), selected_ == int(i))) {
        selected_ = int(i);
        Open(device, objects_[i]);
      }
      ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("pictures", ImVec2(0, 420), ImGuiChildFlags_Borders);
    if (selected_ >= 0 && size_t(selected_) < objects_.size()) {
      Object &o = objects_[size_t(selected_)];
      if (o.pics.empty())
        ImGui::TextDisabled("%s", fr ? "Aucune image." : "No picture.");
      const int per_row = std::max(1, int(ImGui::GetContentRegionAvail().x / 170));
      for (size_t i = 0; i < o.pics.size(); ++i) {
        Pic &p = o.pics[i];
        ImGui::PushID(int(i));
        if (i % size_t(per_row))
          ImGui::SameLine();
        ImGui::BeginGroup();
        arena_page::DrawPicture(p.mine.empty() ? p.thumb : p.mine_thumb, 150);
        ImGui::TextUnformatted((p.name + (p.mine.empty() ? "" : " *")).substr(0, 22).c_str());
        if (ImGui::SmallButton(fr ? "Remplacer..." : "Replace..."))
          Replace(fr, device, p);
        ImGui::SameLine();
        if (ImGui::SmallButton("PNG..."))
          ExportPng(fr, p);
        if (!p.mine.empty()) {
          ImGui::SameLine();
          if (ImGui::SmallButton(fr ? "Remettre" : "Undo"))
            p.mine.clear();
        }
        ImGui::EndGroup();
        ImGui::PopID();
      }
    }
    ImGui::EndChild();
    ImGui::SeparatorText(fr ? "Le pack de textures" : "The texture pack");
    int count = 0;
    for (const auto &o : objects_)
      for (const auto &p : o.pics)
        count += !p.mine.empty();
    ImGui::Text(fr ? "%d image(s) remplacée(s)" : "%d picture(s) replaced", count);
    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##obj_name", fr ? "Nom du pack" : "Pack name", name_, sizeof(name_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##obj_author", fr ? "Auteur (facultatif)" : "Author (optional)", author_, sizeof(author_));
    mod_version::Input("Version##obj_version", version_, sizeof(version_));
    ImGui::BeginDisabled(!count || !name_[0]);
    if (ImGui::Button(fr ? "Créer le pack et l'installer..." : "Make the pack and install it..."))
      Make(fr, mods);
    ImGui::EndDisabled();
    if (!message_.empty())
      ImGui::TextColored(message_ok_ ? green : red, "%s", message_.c_str());
  }

 private:
  struct Pic {
    std::string name;
    Bytes dds;
    uint64_t hash = 0;
    arena_page::Thumb thumb;
    Bytes mine;  // the replacement (DDS), empty = the game's
    arena_page::Thumb mine_thumb;
  };
  struct Object {
    std::string label;
    int kind = 2;            // 0 belts, 1 weapons, 2 others, 3 loading screens
    int file = 0;            // 0 evtobj.pac, 1 bgEtc.pac, 2 menu\LoadHD.pac
    size_t node = 0;         // its entry (root child)
    bool opened = false;
    std::vector<Pic> pics;
  };

  static std::string Trim(std::string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\0'))
      s.pop_back();
    return s;
  }

  void Load(bool fr, const disc::GameDisc &game) {
    message_ok_ = false;
    objects_.clear();
    if (!game.Read("pac/evt/evtobj.pac", files_[0]) || !svrfmt::ArchiveOpen(files_[0], archives_[0], nullptr) ||
        !game.Read("pac/bg/bgEtc.pac", files_[1]) || !svrfmt::ArchiveOpen(files_[1], archives_[1], nullptr)) {
      message_ = fr ? "Fichiers du jeu illisibles (evtobj.pac, bgEtc.pac)." : "Game files unreadable (evtobj.pac, bgEtc.pac).";
      return;
    }
    std::map<int, std::string> names;
    for (auto &k : archives_[0].root.kids)
      if (Trim(k.group) == "SOBJ" && Trim(k.name) == "LIST") {
        const Bytes list = svrfmt::ArchiveData(k);
        for (size_t at = 0; at + 64 <= list.size(); at += 64) {
          std::string n(reinterpret_cast<const char *>(&list[at + 2]), 62);
          n = Trim(n.substr(0, n.find('\0')));
          if (!n.empty())
            names[list[at] | list[at + 1] << 8] = n;
        }
      }
    for (size_t i = 0; i < archives_[0].root.kids.size(); ++i) {
      const auto &k = archives_[0].root.kids[i];
      if (Trim(k.group) != "SOBJ" || Trim(k.name) == "LIST")
        continue;
      Object o;
      o.node = i;
      const int id = atoi(Trim(k.name).c_str());
      const auto it = names.find(id);
      o.label = it != names.end() ? it->second : "Object " + Trim(k.name);
      o.kind = o.label.find("Title") != std::string::npos || o.label.find("Championship") != std::string::npos ? 0 : 2;
      objects_.push_back(std::move(o));
    }
    int w = 0;
    for (size_t i = 0; i < archives_[1].root.kids.size(); ++i) {
      const auto &k = archives_[1].root.kids[i];
      const std::string n = Trim(k.name);
      if (n != "WPON" && n != "WPO2")
        continue;
      Object o;
      o.file = 1;
      o.node = i;
      o.kind = 1;
      o.label = (fr ? "Armes " : "Weapons ") + std::to_string(++w) + " (" + Trim(k.group) + "/" + n + ")";
      objects_.push_back(std::move(o));
    }
    // The loading screens (menu\LoadHD.pac LOAD/NNNN: the general ones GN00-GN49, in-game IG, Universe UN, others),
    // HD only: the game shows the HD ones from 720p up.
    if (game.Read("pac/menu/LoadHD.pac", files_[2]) && svrfmt::ArchiveOpen(files_[2], archives_[2], nullptr))
      for (size_t i = 0; i < archives_[2].root.kids.size(); ++i) {
        const auto &k = archives_[2].root.kids[i];
        if (Trim(k.group) != "LOAD")
          continue;
        Object o;
        o.file = 2;
        o.node = i;
        o.kind = 3;
        const std::string n = Trim(k.name);
        o.label = (n.rfind("GN", 0) == 0 ? (fr ? "Général " : "General ") : n.rfind("UN", 0) == 0 ? "Universe " : "") + n;
        objects_.push_back(std::move(o));
      }
    std::stable_sort(objects_.begin(), objects_.end(), [](const Object &a, const Object &b) { return a.kind < b.kind; });
    message_.clear();
  }

  void Open(ID3D11Device *device, Object &o) {
    if (o.opened)
      return;
    o.opened = true;
    std::vector<uint64_t> seen;
    std::function<void(svrfmt::ArcNode &)> walk = [&](svrfmt::ArcNode &n) {
      const std::string kind = svrfmt::ArchiveKindOf(n);
      if (n.kind == svrfmt::ArcKind::Leaf && kind != "PACH" && kind != "EPAC" && kind != "EPK8") {
        std::vector<svrfmt::BundleTexture> t;
        const Bytes d = svrfmt::ArchiveData(n);
        if (kind == "DDS")
          t.push_back({"texture", "dds", d});
        else if (!svrfmt::BundleRead(d, t))
          return;
        for (auto &x : t) {
          Pic p;
          if (!GameHash(x.data, p.hash) || std::find(seen.begin(), seen.end(), p.hash) != seen.end())
            continue;
          seen.push_back(p.hash);
          p.name = x.name;
          p.dds = x.data;
          p.thumb = arena_page::MakeThumb(device, p.dds);
          o.pics.push_back(std::move(p));
        }
        return;
      }
      if (!svrfmt::ArchiveExpand(n))
        return;
      for (auto &k : n.kids)
        walk(k);
    };
    walk(archives_[o.file].root.kids[o.node]);
  }

  static void Replace(bool fr, ID3D11Device *device, Pic &p) {
    const fs::path f = arena_page::PickOpen(fr, false);
    svrfmt::Image img;
    svrfmt::DdsInfo want;
    if (f.empty() || !wic_image::Load(f, img) || img.w <= 0 || !svrfmt::DdsInfoOf(p.dds, want))
      return;
    // At the game's size (a pack may be bigger, but the object's mapping is made for this shape).
    if (img.w != want.w || img.h != want.h)
      img = svrfmt::Resize(img, want.w, want.h);
    p.mine = svrfmt::DdsEncode(img, svrfmt::DxtFormat::kDxt5, true);
    p.mine_thumb = arena_page::MakeThumb(device, p.mine);
  }

  static void ExportPng(bool fr, const Pic &p) {
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

  void Make(bool fr, mods::Manager &mods) {
    message_ok_ = false;
    std::string id = "textures_";
    std::wstring file;
    for (unsigned char c : std::string(name_)) {
      id += std::isalnum(c) ? char(std::tolower(c)) : '_';
      file += std::isalnum(c) ? wchar_t(c) : L'_';
    }
    const fs::path save = media::PickSave(fr, (file.empty() ? L"objects" : file) + L".svrmod");
    if (save.empty())
      return;
    std::string manifest = "game=wwe12\ntype=textures\nid=" + id + "\nname=" + std::string(name_) +
                           "\nversion=" + mod_version::Of(version_) + "\n";
    if (author_[0])
      manifest += std::string("author=") + author_ + "\n";
    std::vector<std::pair<std::string, Bytes>> entries = {{"manifest.txt", Bytes()}};
    for (const auto &o : objects_)
      for (const auto &p : o.pics) {
        if (p.mine.empty())
          continue;
        std::string n;
        for (unsigned char c : p.name)
          n += std::isalnum(c) ? char(c) : '_';
        char hash[20];
        snprintf(hash, sizeof(hash), "%016llx", static_cast<unsigned long long>(p.hash));
        entries.push_back({"textures/" + (n.empty() ? std::string("tex") : n) + "_" + hash + ".dds", p.mine});
      }
    manifest += "description=" + std::to_string(entries.size() - 1) +
                " picture(s) of belts, weapons or objects, made with the WWE '12 Mod Maker.\n";
    entries[0].second.assign(manifest.begin(), manifest.end());
    if (!mods::Manager::WriteZip(save, entries)) {
      message_ = (fr ? "Impossible d'écrire " : "Could not write ") + media::Utf8(save.wstring());
      return;
    }
    mods.InstallFile(fr, save);
    message_ok_ = true;
    message_ = (fr ? "Pack créé et installé : " : "Pack made and installed: ") + media::Utf8(save.wstring());
  }

  Bytes files_[3];
  svrfmt::Archive archives_[3];
  std::vector<Object> objects_;
  int kind_ = 0, selected_ = -1;
  char name_[64] = {}, author_[64] = {}, version_[16] = {};
  std::string message_;
  bool message_ok_ = false;
};

}  // namespace objects_page
