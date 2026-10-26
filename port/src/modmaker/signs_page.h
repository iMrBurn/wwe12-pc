// SIGNS page: crowd signs (type=signs), after the Crowd signs page of Kaiko's SvR 2011 Mod Maker (signs_page.cpp;
// credit in docs/mod_maker.md). The crowd holds up a superstar's signs while he's in the match: four per superstar
// (pac\audience.pac AUDE/BORD, superstar x 10 + 1..4), 128 x 64 DXT1 with mipmaps. A pack gives signs/NNN_K.dds; the
// game puts them into its DLC pack (superstar_pack.h), replacing his or adding them (an added superstar has his
// base's until then).
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
#include "mods.h"
#include "svrfmt/texture.h"
#include "wic_image.h"

namespace signs_page {

namespace fs = std::filesystem;
using svrfmt::Bytes;

// A picture as a sign: fitted whole into 128 x 64 (the rest white, as a cardboard), DXT1 with mipmaps.
inline Bytes MakeSign(const svrfmt::Image &img) {
  const double k = (std::min)(128.0 / img.w, 64.0 / img.h);
  const int w = (std::max)(1, int(img.w * k + 0.5)), h = (std::max)(1, int(img.h * k + 0.5));
  const svrfmt::Image fitted = svrfmt::Resize(img, w, h);
  svrfmt::Image sign;
  sign.w = 128, sign.h = 64;
  sign.rgba.assign(size_t(128) * 64 * 4, 255);
  const int x0 = (128 - w) / 2, y0 = (64 - h) / 2;
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const uint8_t *s = &fitted.rgba[(size_t(y) * w + x) * 4];
      uint8_t *d = &sign.rgba[(size_t(y0 + y) * 128 + x0 + x) * 4];
      for (int c = 0; c < 3; ++c)  // (over white: a transparent picture's background is the cardboard)
        d[c] = uint8_t((s[c] * s[3] + 255 * (255 - s[3])) / 255);
    }
  return svrfmt::DdsEncode(sign, svrfmt::DxtFormat::kDxt1, true);
}

class Page {
 public:
  void Draw(bool fr, ID3D11Device *device, const disc::GameDisc &disc, mods::Manager &mods) {
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f);
    ImGui::TextWrapped("%s", fr ? "Les pancartes que la foule brandit pour un catcheur pendant son match : quatre par "
                                  "catcheur. Choisissez un catcheur, donnez-lui jusqu'à quatre images, puis créez le pack. "
                                  "(Les catcheurs ajoutés ont déjà celles de leur base.)"
                                : "The signs the crowd holds up for a superstar during his match: four a superstar. Pick a "
                                  "superstar, give him up to four pictures, then make the pack. (Added superstars already "
                                  "have their base's.)");
    if (!disc.Found()) {
      ImGui::TextColored(red, "%s", fr ? "Fichiers du jeu introuvables." : "Game files not found.");
      return;
    }
    if (stars_.empty()) {
      const auto names = disc::SuperstarNames(disc);
      for (const auto &[id, n] : names)
        if (id >= 50)
          stars_.push_back({id, n});
      std::sort(stars_.begin(), stars_.end(), [](const auto &a, const auto &b) { return a.second < b.second; });
    }
    ImGui::SetNextItemWidth(320);
    if (ImGui::BeginCombo(fr ? "Catcheur" : "Superstar", star_ < 0 ? (fr ? "Choisir..." : "Choose...") : stars_[star_].second.c_str(),
                          ImGuiComboFlags_HeightLarge)) {
      ImGui::SetNextItemWidth(-1);
      if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();
      ImGui::InputTextWithHint("##signs_filter", fr ? "Rechercher" : "Search", filter_, sizeof(filter_));
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
      const int id = stars_[star_].first;
      for (int k = 1; k <= 4; ++k) {
        ImGui::PushID(k);
        Sign &s = signs_[id * 10 + k];
        if (s.thumb.srv)
          arena_page::DrawPicture(s.thumb, 96);
        else
          ImGui::Dummy(ImVec2(96, 96));
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::Text(fr ? "Pancarte %d" : "Sign %d", k);
        if (ImGui::SmallButton(fr ? "Une image..." : "A picture...")) {
          const fs::path p = arena_page::PickOpen(fr, false);
          svrfmt::Image img;
          if (!p.empty() && wic_image::Load(p, img) && img.w > 0 && img.h > 0) {
            s.dds = MakeSign(img);
            s.thumb = arena_page::MakeThumb(device, s.dds);
          }
        }
        if (s.thumb.srv && ImGui::SmallButton(fr ? "Retirer" : "Remove")) {
          s.dds.clear();
          s.thumb = {};
        }
        ImGui::EndGroup();
        ImGui::PopID();
      }
    }
    ImGui::SeparatorText(fr ? "Le pack" : "The pack");
    int count = 0;
    for (const auto &[sid, s] : signs_)
      count += !s.dds.empty();
    ImGui::Text(fr ? "%d pancarte(s)" : "%d sign(s)", count);
    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##signs_name", fr ? "Nom du pack" : "Pack name", name_, sizeof(name_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##signs_author", fr ? "Auteur (facultatif)" : "Author (optional)", author_, sizeof(author_));
    ImGui::BeginDisabled(!count || !name_[0] || game::IsRunning());
    if (ImGui::Button(fr ? "Créer le pack et l'installer..." : "Make the pack and install it..."))
      Make(fr, mods);
    ImGui::EndDisabled();
    if (!message_.empty())
      ImGui::TextColored(message_ok_ ? green : red, "%s", message_.c_str());
  }

 private:
  struct Sign {
    Bytes dds;
    arena_page::Thumb thumb;
  };

  void Make(bool fr, mods::Manager &mods) {
    message_ok_ = false;
    std::wstring file;
    std::string id = "signs_";
    for (unsigned char c : std::string(name_)) {
      id += std::isalnum(c) ? char(std::tolower(c)) : '_';
      file += std::isalnum(c) ? wchar_t(c) : L'_';
    }
    const fs::path save = media::PickSave(fr, (file.empty() ? L"signs" : file) + L".svrmod");
    if (save.empty())
      return;
    std::string manifest = "game=wwe12\ntype=signs\nid=" + id + "\nname=" + std::string(name_) + "\nversion=1.0\n";
    if (author_[0])
      manifest += std::string("author=") + author_ + "\n";
    manifest += "description=" + std::string(name_) + ", crowd signs, made with the WWE '12 Mod Maker.\n";
    std::vector<std::pair<std::string, Bytes>> entries = {{"manifest.txt", Bytes(manifest.begin(), manifest.end())}};
    for (const auto &[sid, s] : signs_)
      if (!s.dds.empty())
        entries.push_back({"signs/" + std::to_string(sid / 10) + "_" + std::to_string(sid % 10) + ".dds", s.dds});
    if (!mods::Manager::WriteZip(save, entries)) {
      message_ = (fr ? "Impossible d'écrire " : "Could not write ") + media::Utf8(save.wstring());
      return;
    }
    mods.InstallFile(fr, save);
    message_ok_ = true;
    message_ = (fr ? "Pack créé et installé : " : "Pack made and installed: ") + media::Utf8(save.wstring());
  }

  std::vector<std::pair<int, std::string>> stars_;
  std::map<int, Sign> signs_;  // superstar x 10 + K
  int star_ = -1;
  char filter_[64] = {}, name_[64] = {}, author_[64] = {};
  std::string message_;
  bool message_ok_ = false;
};

}  // namespace signs_page
