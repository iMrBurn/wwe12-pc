// Mod Maker, "Arenas" page (docs/mod_maker.md, step 5). Two ways to make an arena mod (.svrmod, type=arena,
// files/pac/bg/bgNN.pac):
//  - Retexture: pick one of the game's arenas, see all its pictures (mat, aprons, banners, ramp, crowd signs...),
//    replace any with a picture of yours (fitted to the original's size and format) or export it as PNG to edit;
//  - Another arena file: put an arena .pac (made elsewhere, another slot's) in a slot.
// Either way the file is checked against the slot's shipped size and made to fit (arena_edit.h), and its stage
// number is set to the slot's.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "arena_edit.h"
#include "arena_view.h"
#include "game_disc.h"
#include "game_process.h"
#include "imgui.h"
#include "media.h"
#include "mods.h"
#include "wic_image.h"

namespace arena_page {

namespace fs = std::filesystem;
using Microsoft::WRL::ComPtr;
using svrfmt::Bytes;

// The game's arenas (pac/bg/bgNN.pac = stage NN), found by their 3D objects' prefixes (docs/limit_breaking.md).
struct Slot {
  int number;
  const char *name;
};
inline constexpr Slot kSlots[] = {
    {0, "SmackDown"},          {1, "RAW"},
    {2, "NXT"},                {3, "Superstars"},
    {4, "Over the Limit"},     {5, "Fatal 4-Way"},
    {6, "Money in the Bank"},  {7, "SummerSlam"},
    {8, "Night of Champions"}, {9, "Hell in a Cell"},
    {10, "Bragging Rights"},   {11, "Survivor Series"},
    {12, "TLC"},               {13, "Royal Rumble"},
    {14, "Elimination Chamber"}, {15, "WrestleMania XXVII"},
    {16, "Extreme Rules"},     {17, "WCW Nitro"},
    {18, "WCW Clash of the Champions"}, {19, "Starrcade"},
    {20, "Tribute to the Troops"},
};

inline std::string SlotLabel(const Slot &s) {
  char b[96];
  snprintf(b, sizeof(b), "%s  (bg%02d)", s.name, s.number);
  return b;
}

inline fs::path PickOpen(bool fr, bool pac) {
  IFileOpenDialog *dlg = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
    return {};
  const COMDLG_FILTERSPEC pics[] = {{fr ? L"Images" : L"Pictures", L"*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff"},
                                    {L"*.*", L"*.*"}};
  const COMDLG_FILTERSPEC pacs[] = {{fr ? L"Arènes (*.pac)" : L"Arenas (*.pac)", L"*.pac"}, {L"*.*", L"*.*"}};
  dlg->SetFileTypes(2, pac ? pacs : pics);
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

class Page {
 public:
  void Draw(bool fr, ID3D11Device *device, const disc::GameDisc &disc, mods::Manager &mods) {
    device_ = device;
    ImGui::TextWrapped("%s", fr ? "Changez les images d'une arène (tapis, tabliers, bannières, rampe...) ou mettez un "
                                  "autre fichier d'arène à la place d'une arène du jeu. Le Mod Maker veille à la taille : "
                                  "une arène ne doit jamais dépasser celle qu'elle remplace."
                                : "Change an arena's pictures (mat, aprons, banners, ramp...) or put another arena file "
                                  "in place of one of the game's. The Mod Maker watches the size: an arena must never "
                                  "be bigger than the one it replaces.");
    if (!disc.Found()) {
      ImGui::TextColored(kRed, "%s", fr ? "Fichiers du jeu introuvables." : "Game files not found.");
      return;
    }
    if (ImGui::RadioButton(fr ? "Retoucher une arène" : "Retexture an arena", mode_ == 0))
      mode_ = 0;
    ImGui::SameLine();
    if (ImGui::RadioButton(fr ? "Autre fichier d'arène" : "Another arena file", mode_ == 1))
      mode_ = 1;
    ImGui::SameLine();
    if (ImGui::RadioButton(fr ? "Vue 3D" : "3D view", mode_ == 2)) {
      mode_ = 2;
      view_stale_ = true;
    }

    // The slot (and, to retexture, the arena opened from it).
    ImGui::SetNextItemWidth(360);
    if (ImGui::BeginCombo(fr ? "Arène" : "Arena", slot_ < 0 ? (fr ? "Choisir..." : "Choose...")
                                                            : SlotLabel(kSlots[slot_]).c_str(),
                          ImGuiComboFlags_HeightLarge)) {
      for (int i = 0; i < int(std::size(kSlots)); ++i)
        if (ImGui::Selectable(SlotLabel(kSlots[i]).c_str(), i == slot_)) {
          slot_ = i;
          OpenSlot(fr, disc);
          view_stale_ = true;
        }
      ImGui::EndCombo();
    }
    if (slot_ >= 0) {
      ImGui::SameLine();
      ImGui::TextDisabled("%s %s", fr ? "taille d'origine :" : "shipped size:", Human(budget_).c_str());
    }
    if (!error_.empty())
      ImGui::TextColored(kRed, "%s", error_.c_str());
    if (slot_ < 0 || budget_ == 0)
      return;

    if (mode_ == 2) {
      Draw3D(fr);
      return;
    }
    if (mode_ == 1)
      DrawOtherFile(fr);
    else
      DrawRetexture(fr);

    // The mod.
    ImGui::Separator();
    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##arena_name", fr ? "Nom du mod" : "Mod name", name_, sizeof(name_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##arena_author", fr ? "Auteur (facultatif)" : "Author (optional)", author_,
                             sizeof(author_));
    ImGui::SameLine();
    const bool ready = mode_ == 1 ? other_ok_ : Changed() > 0;
    ImGui::BeginDisabled(!ready || game::IsRunning());
    if (ImGui::Button(fr ? "Créer le mod et l'installer..." : "Make the mod and install it..."))
      Make(fr, mods);
    ImGui::EndDisabled();
    if (game::IsRunning())
      ImGui::TextColored(kRed, "%s", fr ? "Le jeu est lancé : fermez-le pour installer le mod."
                                        : "The game is running: close it to install the mod.");
    if (!message_.empty())
      ImGui::TextColored(message_ok_ ? kGreen : kRed, "%s", message_.c_str());
    if (!reduced_note_.empty())
      ImGui::TextColored(kYellow, "%s", reduced_note_.c_str());
  }

 private:
  static constexpr ImVec4 kRed{1.0f, 0.45f, 0.35f, 1.0f}, kGreen{0.55f, 0.85f, 0.45f, 1.0f},
      kYellow{0.94f, 0.75f, 0.24f, 1.0f};

  static std::string Human(size_t b) {
    char s[32];
    snprintf(s, sizeof(s), "%.2f MB", b / 1048576.0);
    return s;
  }
  std::string Path() const {
    char p[32];
    snprintf(p, sizeof(p), "pac/bg/bg%02d.pac", kSlots[slot_].number);
    return p;
  }

  struct Thumb {
    ComPtr<ID3D11ShaderResourceView> srv;
    int w = 0, h = 0;
  };

  Thumb MakeThumb(const Bytes &dds) {
    Thumb t;
    svrfmt::Image img;
    if (!device_ || !svrfmt::DdsDecode(dds, img) || img.w <= 0)
      return t;
    D3D11_TEXTURE2D_DESC td{};
    td.Width = UINT(img.w);
    td.Height = UINT(img.h);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{img.rgba.data(), UINT(img.w * 4), 0};
    ComPtr<ID3D11Texture2D> tex;
    if (SUCCEEDED(device_->CreateTexture2D(&td, &init, &tex)))
      device_->CreateShaderResourceView(tex.Get(), nullptr, &t.srv);
    t.w = img.w;
    t.h = img.h;
    return t;
  }

  static void DrawPicture(const Thumb &t, float box) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImDrawList *dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(at, ImVec2(at.x + box, at.y + box), IM_COL32(40, 40, 44, 255));
    if (t.srv && t.w > 0 && t.h > 0) {
      const float s = std::min(box / t.w, box / t.h), w = t.w * s, h = t.h * s;
      const ImVec2 p(at.x + (box - w) * 0.5f, at.y + (box - h) * 0.5f);
      dl->AddImage(ImTextureRef(ImTextureID(reinterpret_cast<intptr_t>(t.srv.Get()))), p, ImVec2(p.x + w, p.y + h));
    }
    ImGui::Dummy(ImVec2(box, box));
  }

  int Changed() {
    int n = 0;
    for (const auto &t : arena_.Textures())
      n += t.replaced;
    return n;
  }

  void OpenSlot(bool fr, const disc::GameDisc &disc) {
    error_.clear();
    message_.clear();
    reduced_note_.clear();
    thumbs_.clear();
    selected_ = -1;
    budget_ = 0;
    shipped_.clear();
    if (!disc.Read(Path(), shipped_)) {
      error_ = (fr ? "Lecture impossible : " : "Could not read: ") + Path();
      return;
    }
    budget_ = shipped_.size();
    std::string why;
    if (!arena_.Load(shipped_, &why)) {
      error_ = (fr ? "Arène illisible : " : "Unreadable arena: ") + why;
      return;
    }
    for (const auto &t : arena_.Textures())
      thumbs_.push_back(MakeThumb(arena_.Get(t).data));
    if (!name_[0])
      snprintf(name_, sizeof(name_), "%s", kSlots[slot_].name);
  }

  void DrawRetexture(bool fr) {
    auto &texs = arena_.Textures();
    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##arena_filter", fr ? "Rechercher une image" : "Search a picture", filter_,
                             sizeof(filter_));
    ImGui::SameLine();
    ImGui::TextDisabled("%zu %s, %d %s", texs.size(), fr ? "images" : "pictures", Changed(),
                        fr ? "remplacée(s)" : "replaced");
    const float h = std::max(260.0f, ImGui::GetContentRegionAvail().y - 110);
    ImGui::BeginChild("arena_grid", ImVec2(ImGui::GetContentRegionAvail().x * 0.58f, h), ImGuiChildFlags_Borders);
    const float box = 96, cell = box + 16;
    const int per_row = std::max(1, int(ImGui::GetContentRegionAvail().x / cell));
    int col = 0;
    for (int i = 0; i < int(texs.size()); ++i) {
      const std::string &name = arena_.Get(texs[i]).name;
      if (filter_[0] && name.find(filter_) == std::string::npos)
        continue;
      if (col)
        ImGui::SameLine();
      ImGui::BeginGroup();
      ImGui::PushID(i);
      const ImVec2 at = ImGui::GetCursorScreenPos();
      if (ImGui::InvisibleButton("##t", ImVec2(box, box)))
        selected_ = i;
      ImGui::SetCursorScreenPos(at);
      DrawPicture(thumbs_[i], box);
      ImDrawList *dl = ImGui::GetWindowDrawList();
      if (selected_ == i)
        dl->AddRect(at, ImVec2(at.x + box, at.y + box), IM_COL32(240, 190, 60, 255), 0, 0, 3);
      else if (texs[i].replaced)
        dl->AddRect(at, ImVec2(at.x + box, at.y + box), IM_COL32(140, 220, 110, 255), 0, 0, 2);
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + box);
      ImGui::TextDisabled("%s", name.c_str());
      ImGui::PopTextWrapPos();
      ImGui::PopID();
      ImGui::EndGroup();
      col = (col + 1) % per_row;
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("arena_pick", ImVec2(0, h), ImGuiChildFlags_Borders);
    if (selected_ < 0) {
      ImGui::TextDisabled("%s", fr ? "Choisissez une image à gauche." : "Choose a picture on the left.");
    } else {
      auto &t = texs[selected_];
      const auto &tex = arena_.Get(t);
      svrfmt::DdsInfo info;
      svrfmt::DdsInfoOf(tex.data, info);
      static const char *fmts[] = {"DXT1", "DXT3", "DXT5", "ARGB", "?"};
      ImGui::Text("%s", tex.name.c_str());
      ImGui::TextDisabled("%d x %d  %s%s", info.w, info.h, fmts[int(info.format)],
                          t.replaced ? (fr ? "  -  remplacée" : "  -  replaced") : "");
      DrawPicture(thumbs_[selected_], std::min(ImGui::GetContentRegionAvail().x, 360.0f));
      if (ImGui::Button(fr ? "Remplacer par une image..." : "Replace with a picture..."))
        ReplaceSelected(fr);
      ImGui::SameLine();
      if (ImGui::Button(fr ? "Exporter en PNG..." : "Export as PNG..."))
        ExportSelected(fr);
      if (t.replaced) {
        ImGui::SameLine();
        if (ImGui::Button(fr ? "Remettre celle du jeu" : "Back to the game's")) {
          arena_.Restore(t);
          thumbs_[selected_] = MakeThumb(arena_.Get(t).data);
        }
      }
      ImGui::TextWrapped("%s", fr ? "Votre image est mise à la taille et au format de celle du jeu (la transparence "
                                    "est gardée). Pour retoucher l'originale : Exporter en PNG, modifier, Remplacer."
                                  : "Your picture is fitted to the game's size and format (transparency is kept). To "
                                    "edit the original: Export as PNG, edit it, Replace.");
      if (!pick_note_.empty())
        ImGui::TextColored(kRed, "%s", pick_note_.c_str());
    }
    ImGui::EndChild();
  }

  void ReplaceSelected(bool fr) {
    pick_note_.clear();
    const fs::path p = PickOpen(fr, false);
    if (p.empty())
      return;
    svrfmt::Image img;
    if (!wic_image::Load(p, img)) {
      pick_note_ = fr ? "Cette image ne s'ouvre pas." : "This picture can't be opened.";
      return;
    }
    auto &t = arena_.Textures()[selected_];
    svrfmt::DdsInfo info;
    if (!svrfmt::DdsInfoOf(arena_.Original(t).data, info) || info.format == svrfmt::DxtFormat::kUnknown) {
      pick_note_ = fr ? "Format de texture inconnu." : "Unknown texture format.";
      return;
    }
    if (img.w != info.w || img.h != info.h)
      img = svrfmt::Resize(img, info.w, info.h);
    arena_.Unreduce();
    arena_.Replace(t, svrfmt::DdsEncode(img, info.format, info.mips > 1));
    thumbs_[selected_] = MakeThumb(arena_.Get(t).data);
    view_stale_ = true;
    message_.clear();
    reduced_note_.clear();
  }

  void ExportSelected(bool fr) {
    const auto &tex = arena_.Get(arena_.Textures()[selected_]);
    svrfmt::Image img;
    if (!svrfmt::DdsDecode(tex.data, img))
      return;
    const fs::path out = media::PickSave(fr, std::wstring(tex.name.begin(), tex.name.end()) + L".png");
    if (!out.empty() && !wic_image::SavePng(out, img))
      pick_note_ = fr ? "Export impossible." : "Export failed.";
  }

  // The arena (with the pictures replaced so far) in 3D. Left drag: turn; right drag: move; wheel: zoom.
  void Draw3D(bool fr) {
    if (view_stale_) {
      view_stale_ = false;
      view_error_.clear();
      Bytes now = arena_.Build();
      if (!view_.Load(device_, now, &view_error_))
        view_error_ = (fr ? "Vue 3D impossible : " : "No 3D view: ") + view_error_;
    }
    if (!view_error_.empty()) {
      ImGui::TextColored(kRed, "%s", view_error_.c_str());
      return;
    }
    if (ImGui::Checkbox(fr ? "Objets des entrées (scène, rampe)" : "Entrance objects (stage, ramp)",
                        &view_.show_entrance))
      ;
    ImGui::SameLine();
    if (ImGui::Button(fr ? "Recentrer" : "Reset view"))
      view_.ResetCamera();
    ImGui::SameLine();
    ImGui::TextDisabled(fr ? "%zu objets, %zu triangles  -  clic gauche : tourner, clic droit : déplacer, molette : zoom"
                           : "%zu objects, %zu triangles  -  left drag: turn, right drag: move, wheel: zoom",
                        view_.Models(), view_.Triangles());
    const ImVec2 size(ImGui::GetContentRegionAvail().x, std::max(240.0f, ImGui::GetContentRegionAvail().y - 4));
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

  void DrawOtherFile(bool fr) {
    if (ImGui::Button(fr ? "Choisir un fichier d'arène (.pac)..." : "Choose an arena file (.pac)...")) {
      const fs::path p = PickOpen(fr, true);
      if (!p.empty()) {
        other_path_ = p;
        std::ifstream f(p, std::ios::binary);
        Bytes bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        std::string why;
        other_ok_ = other_.Load(bytes, &why);
        other_note_ = other_ok_ ? "" : (fr ? "Pas une arène : " : "Not an arena: ") + why;
        other_size_ = bytes.size();
      }
    }
    if (other_path_.empty())
      return;
    ImGui::SameLine();
    ImGui::TextDisabled("%s", media::Utf8(other_path_.filename().wstring()).c_str());
    if (!other_note_.empty()) {
      ImGui::TextColored(kRed, "%s", other_note_.c_str());
      return;
    }
    ImGui::Text("%s %s, %s %s", fr ? "Taille :" : "Size:", Human(other_size_).c_str(),
                fr ? "place disponible :" : "room:", Human(budget_).c_str());
    if (other_size_ > budget_)
      ImGui::TextColored(kYellow, "%s", fr ? "Trop grande : ses plus grandes images seront réduites de moitié pour tenir."
                                           : "Too big: its biggest pictures will be halved to fit.");
    if (other_.Stage() != kSlots[slot_].number)
      ImGui::TextDisabled(fr ? "Son numéro de scène (%d) sera mis à celui de l'arène remplacée (%d)."
                             : "Its stage number (%d) will be set to the replaced arena's (%d).",
                          other_.Stage(), kSlots[slot_].number);
  }

  void Make(bool fr, mods::Manager &mods) {
    message_ok_ = false;
    reduced_note_.clear();
    arena_edit::Arena &a = mode_ == 1 ? other_ : arena_;
    a.SetStage(kSlots[slot_].number);
    Bytes out;
    const auto reduced = a.Fit(budget_, out);
    if (out.size() > budget_) {
      message_ = fr ? "Impossible de faire tenir l'arène dans la taille d'origine, même en réduisant ses images."
                    : "The arena can't be made to fit the shipped size, even with its pictures reduced.";
      return;
    }
    if (!reduced.empty()) {
      reduced_note_ = fr ? "Réduites de moitié pour tenir dans la taille d'origine :" : "Halved to fit the shipped size:";
      for (const auto &n : reduced)
        reduced_note_ += " " + n;
    }
    const std::string slot = Path().substr(7, 4);  // "bg15"
    std::string id = "arena_" + slot + "_";
    std::wstring file;
    for (unsigned char c : std::string(name_)) {
      id += std::isalnum(c) ? char(c) : '_';
      file += std::isalnum(c) ? wchar_t(c) : L'_';
    }
    const fs::path save = media::PickSave(fr, (file.empty() ? L"arena" : file) + L".svrmod");
    if (save.empty())
      return;
    std::string manifest = "game=wwe12\ntype=arena\nid=" + id + "\nname=" + std::string(name_) + "\nversion=1.0\n";
    if (author_[0])
      manifest += std::string("author=") + author_ + "\n";
    manifest += "description=" + std::string(kSlots[slot_].name) + " (" + slot + ".pac)" +
                (mode_ == 1 ? " replaced by another arena" : ", " + std::to_string(Changed()) + " picture(s) changed") +
                ", made with the WWE '12 Mod Maker.\n";
    if (!mods::Manager::WriteZip(save, {{"manifest.txt", Bytes(manifest.begin(), manifest.end())},
                                        {"files/" + Path(), std::move(out)}})) {
      message_ = (fr ? "Impossible d'écrire " : "Could not write ") + media::Utf8(save.wstring());
      return;
    }
    mods.InstallFile(fr, save);
    message_ok_ = true;
    message_ = (fr ? "Mod créé et installé : " : "Mod made and installed: ") + media::Utf8(save.wstring());
  }

  ID3D11Device *device_ = nullptr;
  int mode_ = 0, slot_ = -1, selected_ = -1;
  size_t budget_ = 0;
  Bytes shipped_;
  arena_edit::Arena arena_;
  std::vector<Thumb> thumbs_;
  char filter_[64] = {}, name_[64] = {}, author_[64] = {};
  std::string error_, message_, reduced_note_, pick_note_;
  bool message_ok_ = false;

  arena_view::View view_;
  bool view_stale_ = true;
  std::string view_error_;

  fs::path other_path_;
  arena_edit::Arena other_;
  bool other_ok_ = false;
  size_t other_size_ = 0;
  std::string other_note_;
};

}  // namespace arena_page
