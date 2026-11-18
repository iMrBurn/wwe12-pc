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
#include "belt_port.h"
#include "game_disc.h"
#include "game_discs.h"
#include "game_icons.h"
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
  const game_discs::Discs *discs_ = nullptr;  // My game discs (set by the Mod Maker)
  bool want_discs_ = false;                   // the player asked for My game discs (the Mod Maker opens it)

  void Draw(bool fr, ID3D11Device *device, const disc::GameDisc &game, mods::Manager &mods) {
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f);
    // (two things: the pictures, as before; a belt of another game in place of one of WWE '12's, 2026-11-18)
    if (ImGui::RadioButton(fr ? "Changer les images" : "Change the pictures", mode_ == 0))
      mode_ = 0;
    ImGui::SameLine();
    if (ImGui::RadioButton(fr ? "Porter une ceinture d'un autre jeu" : "Port a belt from another game", mode_ == 1))
      mode_ = 1;
    ImGui::Separator();
    if (mode_ == 1) {
      DrawPort(fr, device, game, mods);
      return;
    }
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
  // ---- A belt of another game (belt_port.h)
  static constexpr uint32_t kW12 = 0x54510890;
  static const char *GameName(uint32_t t) { return t == kW12 ? "WWE '12" : star_port::GameName(t); }

  void DrawPort(bool fr, ID3D11Device *device, const disc::GameDisc &game, mods::Manager &mods) {
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f);
    ImGui::TextWrapped("%s", fr ? "Une ceinture d'un autre jeu WWE (le titre WWE de 2013 de 2K14...) à la place d'une ceinture du "
                                  "jeu : dans les mains, sur l'épaule et à la taille, partout où le jeu montre ce titre. Le "
                                  "jeu a 23 titres fixes (sa sauvegarde) : pour en « ajouter » un, mettez la ceinture à la "
                                  "place d'un titre que vous n'utilisez pas (Hardcore, Cruiserweight, European...), puis "
                                  "donnez-le à qui vous voulez (MODIFIER L'UNIVERS)."
                                : "A belt of another WWE game (2K14's 2013 WWE title...) in place of one of the game's: in "
                                  "the hands, on the shoulder and at the waist, wherever the game shows that title. The game "
                                  "has 23 fixed titles (its save): to \"add\" one, put the belt in place of a title you don't "
                                  "use (Hardcore, Cruiserweight, European...), then give it to whoever you like (EDIT "
                                  "UNIVERSE).");
    if (w12_.families.empty()) {
      ImGui::BeginDisabled(!game.Found());
      if (ImGui::Button(fr ? "Lire les ceintures du jeu (quelques secondes)" : "Read the game's belts (a few seconds)")) {
        w12_ = belt_port::Read(game, false);
        if (w12_.families.empty())
          port_message_ = fr ? "Ceintures du jeu illisibles (evtobj.pac)." : "The game's belts unreadable (evtobj.pac).";
      }
      ImGui::EndDisabled();
      if (!port_message_.empty())
        ImGui::TextColored(red, "%s", port_message_.c_str());
      return;
    }
    // 1. the game
    ImGui::SeparatorText(fr ? "1. Le jeu d'où vient la ceinture" : "1. The game the belt comes from");
    ImGui::BeginChild("belt_games", ImVec2(0, src_title_ ? 140.0f : 300.0f), ImGuiChildFlags_Borders);
    game_icons::DrawList(
        fr, device, discs_, icons_, src_title_, [](const game_discs::Known &k) { return k.title != kW12; },
        [](uint32_t t) { return GameName(t); }, [fr](uint32_t) { return fr ? "ceintures" : "belts"; },
        [&](uint32_t t) { ChooseSource(fr, t); }, [this]() { want_discs_ = true; });
    ImGui::EndChild();
    if (!port_message_.empty())
      ImGui::TextColored(port_ok_ ? green : red, "%s", port_message_.c_str());
    if (!src_title_ || src_.families.empty())
      return;
    // 2. its belt, 3. in place of
    const float h = 300, half = (ImGui::GetContentRegionAvail().x - 8) / 2;
    auto list = [&](const char *id, const belt_port::Belts &b, int &pick, std::vector<arena_page::Thumb> &thumbs) {
      ImGui::BeginChild(id, ImVec2(half, h), ImGuiChildFlags_Borders);
      if (thumbs.size() != b.families.size())
        thumbs.assign(b.families.size(), arena_page::Thumb());
      if (pick >= 0 && size_t(pick) < b.families.size()) {  // (its picture above the list)
        auto &t = thumbs[size_t(pick)];
        if (!t.srv)
          if (const Bytes dds = belt_port::Picture(b, b.families[size_t(pick)]); !dds.empty())
            t = arena_page::MakeThumb(device, dds);
        arena_page::DrawPicture(t, 120);
      }
      ImGui::BeginChild("rows", ImVec2(0, 0));
      for (size_t i = 0; i < b.families.size(); ++i) {
        ImGui::PushID(int(i));
        if (ImGui::Selectable(b.families[i].label.c_str(), pick == int(i)))
          pick = int(i), made_ = false;
        ImGui::PopID();
      }
      ImGui::EndChild();
      ImGui::EndChild();
    };
    ImGui::SeparatorText(fr ? "2. Sa ceinture, puis 3. à la place de quelle ceinture du jeu"
                            : "2. Its belt, then 3. in place of which of the game's belts");
    ImGui::TextDisabled("%s", GameName(src_title_));
    ImGui::SameLine(half + 16);
    ImGui::TextDisabled("WWE '12");
    list("belt_src", src_, src_pick_, src_thumbs_);
    ImGui::SameLine();
    list("belt_w12", w12_, w12_pick_, w12_thumbs_);
    if (src_pick_ < 0 || size_t(src_pick_) >= src_.families.size() || w12_pick_ < 0 ||
        size_t(w12_pick_) >= w12_.families.size())
      return;
    const auto &sf = src_.families[size_t(src_pick_)], &tf = w12_.families[size_t(w12_pick_)];
    if (!made_)  // (once a choice: an SvR one is converted)
      made_ = true, files_out_ = belt_port::Make(w12_, tf, src_, sf, star_port::Svr(src_title_), swaps_);
    if (swaps_.empty()) {
      ImGui::TextColored(red, "%s", fr ? "Aucune forme de cette ceinture ne va à la place de celle-ci (os différents)."
                                       : "No shape of that belt fits in place of this one (other bones).");
      return;
    }
    std::string done, kept;
    for (const auto &w : swaps_)
      done += (done.empty() ? "" : ", ") + std::to_string(w.to) + " <- " + std::to_string(w.from);
    for (size_t m : tf.members)
      if (std::none_of(swaps_.begin(), swaps_.end(), [&](const belt_port::Swap &w) { return w.to == w12_.objs[m].number; }))
        kept += (kept.empty() ? "" : ", ") + std::to_string(w12_.objs[m].number);
    ImGui::TextWrapped(fr ? "Formes remplacées : %s." : "Shapes replaced: %s.", done.c_str());
    if (!kept.empty())
      ImGui::TextWrapped(fr ? "Gardées du jeu (pliées / roulées, que %s n'a pas) : %s."
                            : "Kept from the game (folded / rolled, which %s hasn't): %s.",
                         GameName(src_title_), kept.c_str());
    if (src_title_ != 0x545408B2)
      ImGui::TextColored(ImVec4(0.94f, 0.75f, 0.24f, 1.0f), "%s",
                         fr ? "Pas encore vérifié en jeu pour ce jeu-là (la première essayée : 2K14)."
                            : "Not yet checked in the game for this one (the first tried: 2K14).");
    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##belt_name", fr ? "Nom du mod" : "Mod name", belt_name_, sizeof(belt_name_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##belt_author", fr ? "Auteur (facultatif)" : "Author (optional)", author_, sizeof(author_));
    mod_version::Input("Version##belt_version", version_, sizeof(version_));
    ImGui::BeginDisabled(!belt_name_[0]);
    if (ImGui::Button(fr ? "Créer le mod et l'installer..." : "Make the mod and install it..."))
      MakeBelt(fr, mods, sf, tf);
    ImGui::EndDisabled();
  }

  void ChooseSource(bool fr, uint32_t t) {
    src_title_ = t;
    src_ = {};
    src_pick_ = -1;
    src_thumbs_.clear();
    made_ = false;
    port_ok_ = false;
    disc::GameDisc g;
    if (!discs_ || !g.OpenPath(discs_->PathOf(t))) {
      port_message_ = fr ? "Disque illisible." : "Disc unreadable.";
      return;
    }
    src_ = belt_port::Read(g, star_port::Svr(t), &w12_);
    port_message_ = src_.families.empty() ? (fr ? "Aucune ceinture lue dans ce jeu." : "No belt read in that game.") : "";
  }

  void MakeBelt(bool fr, mods::Manager &mods, const belt_port::Family &sf, const belt_port::Family &tf) {
    port_ok_ = false;
    std::string id = "belt_";
    for (unsigned char c : tf.label)  // (one mod a title of the game: a new one for it replaces the last)
      id += std::isalnum(c) ? char(std::tolower(c)) : '_';
    std::wstring file;
    for (unsigned char c : std::string(belt_name_))
      file += std::isalnum(c) ? wchar_t(c) : L'_';
    const fs::path save = media::PickSave(fr, (file.empty() ? L"belt" : file) + L".svrmod");
    if (save.empty())
      return;
    std::string manifest = "game=wwe12\ntype=belt\nid=" + id + "\nname=" + std::string(belt_name_) +
                           "\nversion=" + mod_version::Of(version_) + "\n";
    if (author_[0])
      manifest += std::string("author=") + author_ + "\n";
    manifest += "description=" + sf.label + " from " + GameName(src_title_) + " in place of WWE '12's " + tf.label +
                ", made with the WWE '12 Mod Maker.\n";
    std::vector<std::pair<std::string, Bytes>> entries = {{"manifest.txt", Bytes(manifest.begin(), manifest.end())}};
    for (const auto &[n, d] : files_out_)
      entries.push_back({"belt/SOBJ_" + std::to_string(n) + ".bin", d});
    if (!mods::Manager::WriteZip(save, entries)) {
      port_message_ = (fr ? "Impossible d'écrire " : "Could not write ") + media::Utf8(save.wstring());
      return;
    }
    mods.InstallFile(fr, save);
    port_ok_ = true;
    port_message_ = (fr ? "Mod créé et installé : " : "Mod made and installed: ") + media::Utf8(save.wstring());
  }

  int mode_ = 0;
  game_icons::Icons icons_;
  uint32_t src_title_ = 0;
  belt_port::Belts w12_, src_;
  int src_pick_ = -1, w12_pick_ = -1;
  std::vector<arena_page::Thumb> src_thumbs_, w12_thumbs_;
  std::vector<belt_port::Swap> swaps_;
  bool made_ = false;
  std::vector<std::pair<int, Bytes>> files_out_;
  char belt_name_[64] = {};
  std::string port_message_;
  bool port_ok_ = false;

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
