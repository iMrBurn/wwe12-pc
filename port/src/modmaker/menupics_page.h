// UNIVERSE PICTURES page: the Universe's show logos, brand logos, match-up screen pictures and arena photos replaced by
// the player's pictures (type=menupics, pictures/<slot>.dds; menu_pictures.h). Each slot shows the game's picture
// (read from the player's own game files) and takes a picture file in its place; the game fits it to every place that
// shows it at each start (src/native/wwe12_superstar_mods.cpp). Adding new ones is for 1.2 (the game's tables are full).
#pragma once

#include <algorithm>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "arena_page.h"
#include "game_disc.h"
#include "imgui.h"
#include "media.h"
#include "menu_pictures.h"
#include "mod_version.h"
#include "mods.h"
#include "svrfmt/texture.h"
#include "wic_image.h"

namespace menupics_page {

namespace fs = std::filesystem;
using svrfmt::Bytes;

class Page {
 public:
  void Draw(bool fr, ID3D11Device *device, const disc::GameDisc &disc, mods::Manager &mods) {
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f);
    ImGui::TextWrapped("%s", fr ? "Les logos de show et de marque, les images des matchup screens et les photos d'arène du "
                                  "mode Univers (SHOW EDIT). Remplacez ceux que vous voulez par vos images (PNG, JPG...), "
                                  "puis créez le pack : le jeu les met partout où il les montre. On ne peut pas encore en "
                                  "AJOUTER (les listes du jeu sont pleines) : remplacez ceux dont vous ne vous servez pas."
                                : "The Universe's show and brand logos, match-up screen pictures and arena photos (SHOW "
                                  "EDIT). Replace the ones you want with your pictures (PNG, JPG...), then make the pack: "
                                  "the game puts them wherever it shows them. They can't be ADDED yet (the game's lists "
                                  "are full): replace the ones you don't use.");
    if (!disc.Found()) {
      ImGui::TextColored(red, "%s", fr ? "Fichiers du jeu introuvables." : "Game files not found.");
      return;
    }
    if (!loaded_) {
      loaded_ = true;
      Bytes mnlay;
      std::vector<std::string> firsts;
      for (const auto &g : menu_pictures::Groups())
        for (const auto &s : g.slots)
          firsts.push_back(s.names[0]);
      if (disc.Read("pac/menu/MnLayHD.pac", mnlay))
        for (auto &[name, dds] : menu_pictures::FindAll(mnlay, firsts)) {
          game_[name] = arena_page::MakeThumb(device, dds);
          game_dds_[name] = std::move(dds);
        }
    }
    for (const auto &g : menu_pictures::Groups()) {
      int mine = 0;
      for (const auto &s : g.slots)
        mine += chosen_.count(s.names[0]) ? 1 : 0;
      const std::string head = std::string(fr ? g.fr : g.en) + (mine ? " (" + std::to_string(mine) + ")" : "") + "##" + g.en;
      if (!ImGui::CollapsingHeader(head.c_str()))
        continue;
      // What size each kind is in the game, and how a picture is fitted into it.
      const std::string what = g.en;
      const char *help =
          what == "Show logos"
              ? (fr ? "Taille dans le jeu : 256 x 128 (et 256 x 64 pour le petit logo de marque de RAW, SmackDown, NXT, "
                      "ECW, AWA). Votre image est mise ENTIÈRE, centrée, transparente autour. Idéal : un PNG à fond "
                      "transparent d'environ 512 x 256."
                    : "Size in the game: 256 x 128 (and 256 x 64 for the small brand logo of RAW, SmackDown, NXT, ECW, "
                      "AWA). Your picture is put WHOLE, centred, clear around. Best: a PNG with a transparent background, "
                      "about 512 x 256.")
          : what == "Brand logos"
              ? (fr ? "Taille dans le jeu : 256 x 128, et 256 x 64 pour le petit logo des écrans de l'Univers. Votre "
                      "image est mise ENTIÈRE, centrée, transparente autour. Idéal : un PNG à fond transparent d'environ "
                      "512 x 256."
                    : "Size in the game: 256 x 128, and 256 x 64 for the small logo of the Universe screens. Your "
                      "picture is put WHOLE, centred, clear around. Best: a PNG with a transparent background, about "
                      "512 x 256.")
          : what == "Match-up screens"
              ? (fr ? "Taille dans le jeu : 512 x 128 (4 fois plus large que haut). Votre image est RECADRÉE pour remplir "
                      "la case (le centre est gardé). Idéal : 1024 x 256, l'important au milieu."
                    : "Size in the game: 512 x 128 (4 times as wide as high). Your picture is CUT to fill it (its "
                      "middle kept). Best: 1024 x 256, what matters in the middle.")
              : (fr ? "Taille dans le jeu : 256 x 64 (4 fois plus large que haut). Votre image est RECADRÉE pour remplir "
                      "la case (le centre est gardé). Idéal : 1024 x 256, l'arène au milieu."
                    : "Size in the game: 256 x 64 (4 times as wide as high). Your picture is CUT to fill it (its middle "
                      "kept). Best: 1024 x 256, the arena in the middle.");
      ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.94f, 0.75f, 0.24f, 1.0f));
      ImGui::TextWrapped("%s", help);
      ImGui::PopStyleColor();
      const float cell = 200;
      const int per_row = std::max(1, int(ImGui::GetContentRegionAvail().x / (cell + 12)));
      int i = 0;
      for (const auto &s : g.slots) {
        const std::string &key = s.names[0];
        ImGui::PushID(key.c_str());
        if (i % per_row)
          ImGui::SameLine();
        ImGui::BeginGroup();
        const auto c = chosen_.find(key);
        const auto t = game_.find(key);
        if (c != chosen_.end())
          arena_page::DrawPicture(c->second.thumb, 90);
        else if (t != game_.end())
          arena_page::DrawPicture(t->second, 90);
        else
          ImGui::Dummy(ImVec2(90, 90));
        if (c != chosen_.end())
          ImGui::TextColored(green, "%s *", s.label.c_str());
        else
          ImGui::TextUnformatted(s.label.c_str());
        const bool pick = ImGui::SmallButton(fr ? "Remplacer..." : "Replace...");
        if (ImGui::IsItemHovered()) {
          // (the game's exact size of this one)
          const auto d = game_dds_.find(key);
          svrfmt::DdsInfo info;
          if (d != game_dds_.end() && svrfmt::DdsInfoOf(d->second, info))
            ImGui::SetTooltip(fr ? "%d x %d dans le jeu - %s" : "%d x %d in the game - %s", info.w, info.h,
                              s.fill ? (fr ? "recadrée" : "cut to fill") : (fr ? "entière, centrée" : "whole, centred"));
        }
        if (pick) {
          const fs::path p = arena_page::PickOpen(fr, false);
          svrfmt::Image img;
          if (!p.empty() && wic_image::Load(p, img) && img.w > 0 && img.h > 0) {
            // (kept at most 1024 wide, DXT5: the game fits it to each place at each start)
            if (img.w > 1024)
              img = svrfmt::Resize(img, 1024, std::max(1, img.h * 1024 / img.w));
            Mine m;
            m.dds = svrfmt::DdsEncode(img, svrfmt::DxtFormat::kDxt5, false);
            // the preview as the game will show it: fitted to the game's own picture
            const auto like_it = game_dds_.find(key);
            const Bytes like = like_it == game_dds_.end() ? Bytes() : like_it->second;
            m.thumb = arena_page::MakeThumb(device, like.empty() ? m.dds : menu_pictures::Like(like, img, s.fill));
            chosen_[key] = std::move(m);
          }
        }
        if (c != chosen_.end()) {
          ImGui::SameLine();
          if (ImGui::SmallButton(fr ? "Remettre" : "Undo"))
            chosen_.erase(key);
        }
        ImGui::EndGroup();
        ImGui::PopID();
        ++i;
      }
    }
    ImGui::SeparatorText(fr ? "Le pack" : "The pack");
    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##mp_name", fr ? "Nom du pack" : "Pack name", name_, sizeof(name_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##mp_author", fr ? "Auteur (facultatif)" : "Author (optional)", author_, sizeof(author_));
    mod_version::Input("Version##mp_version", version_, sizeof(version_));
    ImGui::BeginDisabled(chosen_.empty() || !name_[0] || game::IsRunning());
    if (ImGui::Button(fr ? "Créer le pack et l'installer..." : "Make the pack and install it..."))
      Make(fr, mods);
    ImGui::EndDisabled();
    if (!message_.empty())
      ImGui::TextColored(message_ok_ ? green : red, "%s", message_.c_str());
  }

 private:
  struct Mine {
    Bytes dds;  // the player's picture (DXT5)
    arena_page::Thumb thumb;
  };

  void Make(bool fr, mods::Manager &mods) {
    message_ok_ = false;
    std::wstring file;
    std::string id = "menupics_";
    for (unsigned char c : std::string(name_)) {
      id += std::isalnum(c) ? char(std::tolower(c)) : '_';
      file += std::isalnum(c) ? wchar_t(c) : L'_';
    }
    const fs::path save = media::PickSave(fr, (file.empty() ? L"universe_pictures" : file) + L".svrmod");
    if (save.empty())
      return;
    std::string manifest = "game=wwe12\ntype=menupics\nid=" + id + "\nname=" + std::string(name_) + "\nversion=" +
                           mod_version::Of(version_) + "\n";
    if (author_[0])
      manifest += std::string("author=") + author_ + "\n";
    manifest += "description=" + std::string(name_) + ", Universe logos and pictures (" + std::to_string(chosen_.size()) +
                "), made with the WWE '12 Mod Maker.\n";
    std::vector<std::pair<std::string, Bytes>> entries = {{"manifest.txt", Bytes(manifest.begin(), manifest.end())}};
    for (const auto &[key, m] : chosen_)
      entries.push_back({"pictures/" + key + ".dds", m.dds});
    if (!mods::Manager::WriteZip(save, entries)) {
      message_ = (fr ? "Impossible d'écrire " : "Could not write ") + media::Utf8(save.wstring());
      return;
    }
    mods.InstallFile(fr, save);
    message_ok_ = true;
    message_ = (fr ? "Pack créé et installé : " : "Pack made and installed: ") + media::Utf8(save.wstring()) +
               (fr ? ". Il compte au prochain lancement du jeu." : ". It counts from the game's next start.");
  }

  bool loaded_ = false;
  std::map<std::string, arena_page::Thumb> game_;  // a slot's first name -> the game's picture
  std::map<std::string, Mine> chosen_;             // ... -> the player's
  std::map<std::string, Bytes> game_dds_;         // ... -> the game's picture (DDS)
  char name_[64] = {}, author_[64] = {}, version_[16] = {};
  std::string message_;
  bool message_ok_ = false;
};

}  // namespace menupics_page
