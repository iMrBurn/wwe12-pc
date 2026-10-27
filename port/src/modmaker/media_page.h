// MEDIA page: a media pack (type=media), after the Media page of Kaiko's SvR 2011 Mod Maker (media_page.cpp; credit in
// docs/mod_maker.md): the game's own superstars' portraits (media/portraits/NNN/SSFx_HD.dds, SSFx_SD.dds: put by the game
// into its DLC pack under their numbers, found before the disc's: superstar_pack.h) and songs for their entrances
// (media/music/NNN.<mp3...>: copied by the game into the music folder, src/native/wwe12_superstar_mods.cpp, where Create
// an Entrance's USER PLAYLIST finds them, their loudness levelled to the game's own music). One pack can hold many.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "arena_page.h"
#include "game_disc.h"
#include "imgui.h"
#include "mod_version.h"
#include "media.h"
#include "mods.h"
#include "svrfmt/texture.h"
#include "wic_image.h"

namespace media_page {

namespace fs = std::filesystem;
using svrfmt::Bytes;

// A picture as the game's portraits: fitted whole into the square (transparent around it, standing on the bottom
// edge), DXT5, no mips: HD SSFA / SSFB 512 x 512, SSFC 256 x 256; SD SSFA / SSFB the square squeezed to 512 x 256,
// SSFC 256 x 256 (as new_star_page.h's own portrait).
inline void MakePortraits(const svrfmt::Image &img, Bytes out[2][3]) {
  const double k = (std::min)(512.0 / img.w, 512.0 / img.h);
  const int w = (std::max)(1, int(img.w * k + 0.5)), h = (std::max)(1, int(img.h * k + 0.5));
  const svrfmt::Image fitted = svrfmt::Resize(img, w, h);
  svrfmt::Image sq;
  sq.w = sq.h = 512;
  sq.rgba.assign(size_t(512) * 512 * 4, 0);
  const int x0 = (512 - w) / 2, y0 = 512 - h;
  for (int y = 0; y < h; ++y)
    std::copy_n(&fitted.rgba[size_t(y) * w * 4], size_t(w) * 4, &sq.rgba[(size_t(y0 + y) * 512 + x0) * 4]);
  const int size[2][3][2] = {{{512, 256}, {512, 256}, {256, 256}}, {{512, 512}, {512, 512}, {256, 256}}};
  for (int hd = 0; hd < 2; ++hd)
    for (int i = 0; i < 3; ++i)
      out[hd][i] = svrfmt::DdsEncode(svrfmt::Resize(sq, size[hd][i][0], size[hd][i][1]), svrfmt::DxtFormat::kDxt5, false);
}

class Page {
 public:
  void Draw(bool fr, ID3D11Device *device, const disc::GameDisc &disc, mods::Manager &mods) {
    device_ = device;
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f);
    ImGui::TextWrapped("%s", fr ? "Un pack média change les portraits et les musiques d'entrée des catcheurs du jeu. Ajoutez "
                                  "autant de catcheurs que vous voulez, puis créez le pack. (Les vidéos d'entrée se font "
                                  "sur la page VIDÉOS, les autres images sur la page TEXTURES.)"
                                : "A media pack changes the game's superstars' portraits and entrance themes. Add as many "
                                  "superstars as you like, then make the pack. (Entrance videos are made on the VIDEOS "
                                  "page, other pictures on the TEXTURES page.)");
    if (!disc.Found()) {
      ImGui::TextColored(red, "%s", fr ? "Fichiers du jeu introuvables." : "Game files not found.");
      return;
    }
    if (stars_.empty())
      List(disc);
    // The superstar.
    ImGui::SetNextItemWidth(320);
    if (ImGui::BeginCombo(fr ? "Catcheur" : "Superstar", star_ < 0 ? (fr ? "Choisir..." : "Choose...") : stars_[star_].second.c_str(),
                          ImGuiComboFlags_HeightLarge)) {
      ImGui::SetNextItemWidth(-1);
      if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
      ImGui::InputTextWithHint("##media_filter", fr ? "Rechercher" : "Search", filter_, sizeof(filter_));
      for (int i = 0; i < int(stars_.size()); ++i) {
        std::string a = stars_[i].second, b = filter_;
        for (auto *x : {&a, &b})
          std::transform(x->begin(), x->end(), x->begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (!b.empty() && a.find(b) == std::string::npos)
          continue;
        if (ImGui::Selectable(stars_[i].second.c_str(), i == star_))
          star_ = i;
      }
      ImGui::EndCombo();
    }
    if (star_ >= 0) {
      Item &it = items_[stars_[star_].first];
      ImGui::SeparatorText(fr ? "Son portrait" : "Its portrait");
      if (it.thumb.srv) {
        arena_page::DrawPicture(it.thumb, 120);
        ImGui::SameLine();
      }
      ImGui::BeginGroup();
      if (ImGui::Button(fr ? "Une image..." : "A picture...")) {
        const fs::path p = arena_page::PickOpen(fr, false);
        svrfmt::Image img;
        if (!p.empty() && wic_image::Load(p, img) && img.w > 0 && img.h > 0) {
          MakePortraits(img, it.portraits);
          it.thumb = arena_page::MakeThumb(device_, it.portraits[1][0]);
        }
      }
      if (it.thumb.srv && ImGui::SmallButton(fr ? "Retirer le portrait" : "Remove the portrait")) {
        for (auto &row : it.portraits)
          for (auto &p : row)
            p.clear();
        it.thumb = {};
      }
      ImGui::TextDisabled("%s", fr ? "(de préférence détouré, fond transparent, en PNG)" : "(best cut out, transparent background, as a PNG)");
      ImGui::EndGroup();
      ImGui::SeparatorText(fr ? "Sa musique d'entrée" : "Its entrance theme");
      if (ImGui::Button(fr ? "Une musique (mp3, m4a, wav, wma, flac)..." : "A song (mp3, m4a, wav, wma, flac)...")) {
        const fs::path p = arena_page::PickOpen(fr, false, fr ? L"Musiques" : L"Songs", L"*.mp3;*.m4a;*.aac;*.wav;*.wma;*.flac");
        if (!p.empty())
          it.music = p;
      }
      ImGui::SameLine();
      ImGui::TextDisabled("%s", it.music.empty() ? (fr ? "(celle du jeu)" : "(the game's)") : media::Utf8(it.music.filename().wstring()).c_str());
      if (!it.music.empty() && ImGui::SmallButton(fr ? "Retirer la musique" : "Remove the song"))
        it.music.clear();
      ImGui::TextDisabled("%s", fr ? "(ajoutée à vos musiques : dans le jeu, CRÉATIONS > Créer une entrée, choisissez ce "
                                     "catcheur puis la musique, PLAYLIST UTILISATEUR ; son volume est réglé comme celui "
                                     "des musiques du jeu)"
                                   : "(added to your songs: in the game, CREATIONS > Create an Entrance, pick this superstar, "
                                     "then the music, USER PLAYLIST; its volume is levelled like the game's own music)");
    }
    // The pack.
    ImGui::SeparatorText(fr ? "Le pack" : "The pack");
    int count = 0;
    for (const auto &[id, it] : items_)
      if (it.thumb.srv || !it.music.empty()) {
        ++count;
        std::string name;
        for (const auto &s : stars_)
          if (s.first == id)
            name = s.second;
        ImGui::BulletText("%s%s%s", name.c_str(), it.thumb.srv ? (fr ? " : portrait" : ": portrait") : "",
                          it.music.empty() ? "" : (fr ? ", musique" : ", theme"));
      }
    if (!count)
      ImGui::TextDisabled("%s", fr ? "(vide)" : "(empty)");
    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##media_name", fr ? "Nom du pack" : "Pack name", name_, sizeof(name_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##media_author", fr ? "Auteur (facultatif)" : "Author (optional)", author_, sizeof(author_));
    mod_version::Input("Version##media_version", version_, sizeof(version_));
    ImGui::BeginDisabled(!count || !name_[0] || game::IsRunning());
    if (ImGui::Button(fr ? "Créer le pack et l'installer..." : "Make the pack and install it..."))
      Make(fr, mods);
    ImGui::EndDisabled();
    if (!message_.empty())
      ImGui::TextColored(message_ok_ ? green : red, "%s", message_.c_str());
  }

 private:
  struct Item {
    Bytes portraits[2][3];
    arena_page::Thumb thumb;
    fs::path music;
  };

  void List(const disc::GameDisc &disc) {
    const auto names = disc::SuperstarNames(disc);
    for (const auto &f : disc.List("pac/ch")) {
      if (f.size() != 9 || _strnicmp(f.c_str(), "ch", 2) || !isdigit(f[2]))
        continue;
      const int id = atoi(f.c_str() + 2);
      if (auto n = names.find(id); n != names.end())
        stars_.push_back({id, n->second});
    }
    std::sort(stars_.begin(), stars_.end(), [](const auto &a, const auto &b) { return a.second < b.second; });
  }

  void Make(bool fr, mods::Manager &mods) {
    message_ok_ = false;
    std::wstring file;
    std::string id = "media_";
    for (unsigned char c : std::string(name_)) {
      id += std::isalnum(c) ? char(std::tolower(c)) : '_';
      file += std::isalnum(c) ? wchar_t(c) : L'_';
    }
    const fs::path save = media::PickSave(fr, (file.empty() ? L"media" : file) + L".svrmod");
    if (save.empty())
      return;
    std::string manifest = "game=wwe12\ntype=media\nid=" + id + "\nname=" + std::string(name_) + "\nversion=" + mod_version::Of(version_) + "\n";
    if (author_[0])
      manifest += std::string("author=") + author_ + "\n";
    manifest += "description=" + std::string(name_) + ", portraits and entrance themes of the game's superstars, made with the "
                "WWE '12 Mod Maker.\n";
    std::vector<std::pair<std::string, Bytes>> entries = {{"manifest.txt", Bytes(manifest.begin(), manifest.end())}};
    for (const auto &[sid, it] : items_) {
      const std::string dir = "media/portraits/" + std::to_string(sid) + "/";
      for (int hd = 0; hd < 2; ++hd)
        for (int k = 0; k < 3; ++k)
          if (!it.portraits[hd][k].empty())
            entries.push_back({dir + "SSF" + char('A' + k) + (hd ? "_HD.dds" : "_SD.dds"), it.portraits[hd][k]});
      if (!it.music.empty()) {
        std::ifstream f(it.music, std::ios::binary);
        Bytes song((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        std::string ext = it.music.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return char(std::tolower(c)); });
        if (!song.empty())
          entries.push_back({"media/music/" + std::to_string(sid) + ext, std::move(song)});
      }
    }
    if (!mods::Manager::WriteZip(save, entries)) {
      message_ = (fr ? "Impossible d'écrire " : "Could not write ") + media::Utf8(save.wstring());
      return;
    }
    mods.InstallFile(fr, save);
    message_ok_ = true;
    message_ = (fr ? "Pack créé et installé : " : "Pack made and installed: ") + media::Utf8(save.wstring());
  }

  ID3D11Device *device_ = nullptr;
  std::vector<std::pair<int, std::string>> stars_;
  std::map<int, Item> items_;
  int star_ = -1;
  char filter_[64] = {}, name_[64] = {}, author_[64] = {}, version_[16] = {};
  std::string message_;
  bool message_ok_ = false;
};

}  // namespace media_page
