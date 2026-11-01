// Mod Maker: the choice of a superstar's ribbon around the arena or stage wall (entrance_screens.h), with a preview of
// what the game will show; used by NEW SUPERSTAR (its own, else the arena's) and VIDEOS (a superstar of the game: its
// own, else the arena's, replaced or added by the mod).
#pragma once

#include <windows.h>
#include <d3d11.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <filesystem>
#include <string>
#include <vector>

#include "entrance_screens.h"
#include "imgui.h"

namespace screens_ui {

namespace fs = std::filesystem;
using Bytes = entrance_screens::Bytes;

class ScreenChoice {
 public:
  explicit ScreenChoice(bool ribbon) : ribbon_(ribbon) {}

  // Forgets the choice. `game`: what the game shows now (its own movie, Bink 1, else empty: the arena's) for the
  // preview of choice 0.
  void Reset(Bytes game = {}) {
    pick_ = 0;
    versions_.clear();
    version_ = 0;
    picture_.clear();
    game_ = std::move(game);
    preview_of_.clear();
  }
  // Its versions in another game (entrance_screens::Versions), added to choice 1's list (every game's: "WWE 2K15
  // M013910"); choice 1 picked if `pick`.
  void Offer(std::vector<entrance_screens::Version> versions, const std::string &game_name, bool pick) {
    if (versions.empty())
      return;
    if (!versions_.empty() && game_name != game_name_)
      game_name_.clear();  // (several games)
    else if (versions_.empty())
      game_name_ = game_name;
    for (auto &v : versions) {
      v.label = game_name + "  " + v.label;
      versions_.push_back(std::move(v));
    }
    if (pick)
      pick_ = 1;
  }
  bool Changed() const { return pick_ != 0; }

  // The radio buttons, the version / picture, and the preview. zero: the label of choice 0 ("The arena's", "The
  // game's").
  void Draw(bool fr, ID3D11Device *device, const char *zero) {
    ImGui::PushID(ribbon_ ? "screen_ribbon" : "screen_wall");
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::TextUnformatted(ribbon_ ? (fr ? "Ruban autour de l'arène (son nom, 1024 x 32)" : "Ribbon around the arena (its name, 1024 x 32)")
                                   : (fr ? "Mur de la scène (256 x 256)" : "Stage wall (256 x 256)"));
    if (ImGui::RadioButton(zero, pick_ == 0))
      pick_ = 0;
    if (!versions_.empty()) {
      ImGui::SameLine();
      char label[96];
      if (game_name_.empty())
        snprintf(label, sizeof(label), fr ? "Le sien dans mes jeux (%zu)" : "Its own in my games (%zu)", versions_.size());
      else
        snprintf(label, sizeof(label), fr ? "Le sien dans %s" : "Its own in %s", game_name_.c_str());
      if (ImGui::RadioButton(label, pick_ == 1))
        pick_ = 1;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton(fr ? "Une image à vous" : "A picture of yours", pick_ == 2)) {
      pick_ = 2;
      if (picture_.empty())
        picture_ = Pick(fr);
    }
    if (pick_ == 1 && versions_.size() > 1) {
      ImGui::SetNextItemWidth(260);
      if (ImGui::BeginCombo("Version", versions_[size_t(version_)].label.c_str(), ImGuiComboFlags_HeightLarge)) {
        for (size_t i = 0; i < versions_.size(); ++i)
          if (ImGui::Selectable(versions_[i].label.c_str(), int(i) == version_))
            version_ = int(i);
        ImGui::EndCombo();
      }
    }
    if (pick_ == 2) {
      if (ImGui::Button(fr ? "Choisir l'image..." : "Choose the picture...")) {
        const fs::path p = Pick(fr);
        if (!p.empty())
          picture_ = p;
      }
      ImGui::SameLine();
      ImGui::TextDisabled("%s", picture_.empty() ? (fr ? "(aucune)" : "(none)")
                                                 : reinterpret_cast<const char *>(picture_.filename().u8string().c_str()));
    }
    // the preview, made again when the choice changes
    const std::string of = std::to_string(pick_) + "|" + std::to_string(version_) + "|" + picture_.string();
    if (of != preview_of_) {
      preview_of_ = of;
      preview_.Reset();
      std::vector<uint8_t> px;
      const Bytes none;
      const Bytes &movie = pick_ == 0 ? game_ : pick_ == 1 && size_t(version_) < versions_.size() ? versions_[size_t(version_)].movie : none;
      if (entrance_screens::Frame(movie, pick_ == 2 ? picture_ : fs::path(), ribbon_, 60, px))
        preview_ = Texture(device, px);
    }
    if (preview_)
      ImGui::Image(ImTextureRef(ImTextureID(reinterpret_cast<intptr_t>(preview_.Get()))),
                   ribbon_ ? ImVec2(768, 24) : ImVec2(192, 192));
    else if (pick_ == 0)
      ImGui::TextDisabled("%s", fr ? "(celui de l'arène du match)" : "(the one of the match's arena)");
    ImGui::PopID();
  }

  // The chosen movie as WWE '12's (Bink 1 at the screen's size), empty for choice 0.
  Bytes Movie() const {
    Bytes out;
    if (pick_ == 1 && size_t(version_) < versions_.size()) {
      const Bytes &m = versions_[size_t(version_)].movie;
      if (m.size() > 4 && memcmp(m.data(), "KB2", 3) == 0)
        entrance_screens::FromBink2(m, ribbon_, out);
    } else if (pick_ == 2 && !picture_.empty()) {
      entrance_screens::FromPicture(picture_, ribbon_, out);
    }
    return out;
  }

 private:
  // A picture of the player's (as arena_page::PickOpen; that header includes media.h, which includes this one).
  static fs::path Pick(bool fr) {
    IFileOpenDialog *dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
      return {};
    const COMDLG_FILTERSPEC pics[] = {{fr ? L"Images" : L"Pictures", L"*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff;*.dds"},
                                      {L"*.*", L"*.*"}};
    dlg->SetFileTypes(2, pics);
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);
    fs::path picked;
    if (SUCCEEDED(dlg->Show(GetActiveWindow()))) {
      IShellItem *item = nullptr;
      if (SUCCEEDED(dlg->GetResult(&item))) {
        PWSTR p = nullptr;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p))) {
          picked = p;
          CoTaskMemFree(p);
        }
        item->Release();
      }
    }
    dlg->Release();
    return picked;
  }
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> Texture(ID3D11Device *device, const std::vector<uint8_t> &px) const {
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    const int w = ribbon_ ? entrance_screens::kRibbonW : entrance_screens::kWall;
    const int h = ribbon_ ? entrance_screens::kRibbonH : entrance_screens::kWall;
    D3D11_TEXTURE2D_DESC td{};
    td.Width = UINT(w);
    td.Height = UINT(h);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{px.data(), UINT(w * 4), 0};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    if (device && SUCCEEDED(device->CreateTexture2D(&td, &init, &tex)))
      device->CreateShaderResourceView(tex.Get(), nullptr, &srv);
    return srv;
  }

  bool ribbon_;
  int pick_ = 0;  // 0 the game's / the arena's, 1 its own in another game, 2 a picture
  std::vector<entrance_screens::Version> versions_;
  std::string game_name_;
  int version_ = 0;
  fs::path picture_;
  Bytes game_;
  Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> preview_;
  std::string preview_of_;
};

}  // namespace screens_ui
