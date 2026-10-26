// BACKGROUNDS page: main menu backgrounds (type=background), after the Main menu backgrounds page of Kaiko's SvR 2011
// Mod Maker (backgrounds_page.cpp; credit in docs/mod_maker.md). The main menu shows a superstar beside the menu
// (MnLayHD.pac MLYE/EM00 ortontex02, 1024 x 1024, cut out) and another on the title screen (ortontex01, 512 x 1024).
// A pack gives backgrounds/NN/menu.dds (and title.dds); at each start the game draws one of the installed ones, or
// its own (superstar_pack.h DressMenu, src/native/wwe12_superstar_mods.cpp).
#pragma once

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#include "arena_page.h"
#include "imgui.h"
#include "media.h"
#include "mods.h"
#include "svrfmt/texture.h"
#include "wic_image.h"

namespace backgrounds_page {

namespace fs = std::filesystem;
using svrfmt::Bytes;

// A picture fitted whole into w x h (transparent around it, standing on the bottom edge), 32-bit as the game's.
inline Bytes Fit(const svrfmt::Image &img, int W, int H) {
  const double k = (std::min)(double(W) / img.w, double(H) / img.h);
  const int w = (std::max)(1, int(img.w * k + 0.5)), h = (std::max)(1, int(img.h * k + 0.5));
  const svrfmt::Image fitted = svrfmt::Resize(img, w, h);
  svrfmt::Image out;
  out.w = W, out.h = H;
  out.rgba.assign(size_t(W) * H * 4, 0);
  const int x0 = (W - w) / 2, y0 = H - h;
  for (int y = 0; y < h; ++y)
    std::copy_n(&fitted.rgba[size_t(y) * w * 4], size_t(w) * 4, &out.rgba[(size_t(y0 + y) * W + x0) * 4]);
  return svrfmt::DdsEncode(out, svrfmt::DxtFormat::kArgb, false);
}

class Page {
 public:
  void Draw(bool fr, ID3D11Device *device, mods::Manager &mods) {
    const ImVec4 red(1.0f, 0.45f, 0.35f, 1.0f), green(0.55f, 0.85f, 0.45f, 1.0f);
    ImGui::TextWrapped("%s", fr ? "Le catcheur du menu principal (à côté du menu) et celui de l'écran-titre. Ajoutez des "
                                  "fonds : à chaque lancement, le jeu en tire un au sort parmi les vôtres et le sien. "
                                  "De préférence des catcheurs détourés (fond transparent, PNG)."
                                : "The superstar on the main menu (beside the menu) and on the title screen. Add "
                                  "backgrounds: at each start the game draws one of yours or its own. Best: superstars "
                                  "cut out (transparent background, PNG).");
    for (size_t i = 0; i < list_.size(); ++i) {
      ImGui::PushID(int(i));
      Item &it = list_[i];
      arena_page::DrawPicture(it.menu_thumb, 110);
      ImGui::SameLine();
      arena_page::DrawPicture(it.title_thumb, 110);
      ImGui::SameLine();
      ImGui::BeginGroup();
      ImGui::Text(fr ? "Fond %d" : "Background %d", int(i + 1));
      if (ImGui::SmallButton(fr ? "Menu principal..." : "Main menu..."))
        Pick(fr, device, it.menu, it.menu_thumb, 1024, 1024);
      if (ImGui::SmallButton(fr ? "Écran-titre (facultatif)..." : "Title screen (optional)..."))
        Pick(fr, device, it.title, it.title_thumb, 512, 1024);
      if (ImGui::SmallButton(fr ? "Retirer ce fond" : "Remove this background")) {
        list_.erase(list_.begin() + long(i));
        ImGui::EndGroup();
        ImGui::PopID();
        break;
      }
      ImGui::EndGroup();
      ImGui::PopID();
    }
    if (list_.size() < 20 && ImGui::Button(fr ? "Ajouter un fond" : "Add a background"))
      list_.emplace_back();
    ImGui::SeparatorText(fr ? "Le pack" : "The pack");
    int count = 0;
    for (const auto &it : list_)
      count += !it.menu.empty();
    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##bg_name", fr ? "Nom du pack" : "Pack name", name_, sizeof(name_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##bg_author", fr ? "Auteur (facultatif)" : "Author (optional)", author_, sizeof(author_));
    ImGui::BeginDisabled(!count || !name_[0] || game::IsRunning());
    if (ImGui::Button(fr ? "Créer le pack et l'installer..." : "Make the pack and install it..."))
      Make(fr, mods);
    ImGui::EndDisabled();
    if (!message_.empty())
      ImGui::TextColored(message_ok_ ? green : red, "%s", message_.c_str());
  }

 private:
  struct Item {
    Bytes menu, title;
    arena_page::Thumb menu_thumb, title_thumb;
  };

  static void Pick(bool fr, ID3D11Device *device, Bytes &out, arena_page::Thumb &thumb, int w, int h) {
    const fs::path p = arena_page::PickOpen(fr, false);
    svrfmt::Image img;
    if (p.empty() || !wic_image::Load(p, img) || img.w <= 0 || img.h <= 0)
      return;
    out = Fit(img, w, h);
    thumb = arena_page::MakeThumb(device, out);
  }

  void Make(bool fr, mods::Manager &mods) {
    message_ok_ = false;
    std::wstring file;
    std::string id = "backgrounds_";
    for (unsigned char c : std::string(name_)) {
      id += std::isalnum(c) ? char(std::tolower(c)) : '_';
      file += std::isalnum(c) ? wchar_t(c) : L'_';
    }
    const fs::path save = media::PickSave(fr, (file.empty() ? L"backgrounds" : file) + L".svrmod");
    if (save.empty())
      return;
    std::string manifest = "game=wwe12\ntype=background\nid=" + id + "\nname=" + std::string(name_) + "\nversion=1.0\n";
    if (author_[0])
      manifest += std::string("author=") + author_ + "\n";
    manifest += "description=" + std::string(name_) + ", main menu backgrounds, made with the WWE '12 Mod Maker.\n";
    std::vector<std::pair<std::string, Bytes>> entries = {{"manifest.txt", Bytes(manifest.begin(), manifest.end())}};
    int n = 0;
    for (const auto &it : list_) {
      if (it.menu.empty())
        continue;
      char dir[32];
      snprintf(dir, sizeof(dir), "backgrounds/%02d/", ++n);
      entries.push_back({std::string(dir) + "menu.dds", it.menu});
      if (!it.title.empty())
        entries.push_back({std::string(dir) + "title.dds", it.title});
    }
    if (!mods::Manager::WriteZip(save, entries)) {
      message_ = (fr ? "Impossible d'écrire " : "Could not write ") + media::Utf8(save.wstring());
      return;
    }
    mods.InstallFile(fr, save);
    message_ok_ = true;
    message_ = (fr ? "Pack créé et installé : " : "Pack made and installed: ") + media::Utf8(save.wstring());
  }

  std::vector<Item> list_ = std::vector<Item>(1);
  char name_[64] = {}, author_[64] = {};
  std::string message_;
  bool message_ok_ = false;
};

}  // namespace backgrounds_page
