// Mod Maker, "Arenas" page (docs/mod_maker.md, step 5). Two ways to make an arena mod (.svrmod, type=arena,
// files/pac/bg/bgNN.pac):
//  - Retexture: pick one of the game's arenas, see all its pictures (mat, aprons, banners, ramp, crowd signs...),
//    replace any with a picture of yours (fitted to the original's size and format) or export it as PNG to edit;
//  - Another arena file: put an arena .pac (made elsewhere, another slot's) in a slot.
// Either way the file is checked against the slot's shipped size and made to fit (arena_edit.h), and its stage
// number is set to the slot's. A third way: an arena of WWE '13 (the player's own copy) ported with port13.h, the arena
// chosen above as its base. The mod either replaces the arena, or (the default) is added as a new arena: its own
// tile on the arena select screen's next pages, playing on the chosen arena's number (manifest slot=new, base=NN;
// arena/arena.pac and arena/banner.dds, 256 x 128 DXT5: src/native/wwe12_arena_pages.cpp). The banner is a picture
// of the player's, or the base arena's own banner (menu/MatchHD.pac, MLYE/MUPA entry 7001, "arenaNN").
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
#include "port13.h"
#include "port_svr_arena.h"
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

struct Thumb {
  ComPtr<ID3D11ShaderResourceView> srv;
  int w = 0, h = 0;
};


// A texture (DDS) as a picture ImGui can show.
inline Thumb MakeThumb(ID3D11Device *device_, const Bytes &dds) {
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

// The picture fitted into a box x box square (dark behind).
inline void DrawPicture(const Thumb &t, float box) {
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

class Page {
 public:
  void Draw(bool fr, ID3D11Device *device, const disc::GameDisc &disc, mods::Manager &mods) {
    device_ = device;
    disc_ = &disc;  // (the ports read the game's menus as soon as an arena is chosen)
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
    if (ImGui::RadioButton(fr ? "Porter de WWE '13" : "Port from WWE '13", mode_ == 3))
      mode_ = 3;
    ImGui::SameLine();
    if (ImGui::RadioButton(fr ? "Porter de SvR 2011" : "Port from SvR 2011", mode_ == 4))
      mode_ = 4;
    ImGui::SameLine();
    if (ImGui::RadioButton(fr ? "Vue 3D" : "3D view", mode_ == 2)) {
      mode_ = 2;
      view_port_ = false;
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
          if (mode_ == 3 && w13_pick_ >= 0)
            Port13(fr);  // (the recipe takes files from the base: done again with the new one)
          else if (mode_ == 4 && !svr_file_.empty())
            PortSvr(fr);
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
    else if (mode_ == 3)
      DrawPort13(fr);
    else if (mode_ == 4)
      DrawPortSvr(fr);
    else
      DrawRetexture(fr);

    // The mod.
    ImGui::Separator();
    disc_ = &disc;
    if (ImGui::RadioButton(fr ? "Ajouter comme nouvelle arène (garde l'originale)" : "Add as a new arena (keeps the "
                                                                                    "original)",
                           as_new_))
      as_new_ = true;
    ImGui::SameLine();
    if (ImGui::RadioButton(fr ? "Remplacer l'arène" : "Replace the arena", !as_new_))
      as_new_ = false;
    if (as_new_)
      DrawBanner(fr);
    ImGui::SetNextItemWidth(260);
    ImGui::InputTextWithHint("##arena_name", fr ? "Nom du mod" : "Mod name", name_, sizeof(name_));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(200);
    ImGui::InputTextWithHint("##arena_author", fr ? "Auteur (facultatif)" : "Author (optional)", author_,
                             sizeof(author_));
    ImGui::SameLine();
    const bool ready = mode_ == 1 ? other_ok_ : mode_ >= 3 ? port_ok_ : Changed() > 0;
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
      thumbs_.push_back(MakeThumb(device_, arena_.Get(t).data));
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
          thumbs_[selected_] = MakeThumb(device_, arena_.Get(t).data);
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
    thumbs_[selected_] = MakeThumb(device_, arena_.Get(t).data);
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
      Bytes now = view_port_ && port_ok_ ? other_.Build() : arena_.Build();
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

  // An arena of WWE '13: its folder (the game's, with pac/bg), then one of its arenas; it plays as the arena
  // chosen above (its base: the recipe takes a few files from it).
  void DrawPort13(bool fr) {
    if (ImGui::Button(fr ? "Dossier de WWE '13..." : "WWE '13 folder...")) {
      const fs::path p = PickFolder(fr);
      if (!p.empty()) {
        w13_dir_ = fs::exists(p / "pac" / "bg") ? p : fs::exists(p / "bg") ? p.parent_path() : p;
        w13_list_.clear();
        std::error_code ec;
        for (fs::directory_iterator it(w13_dir_ / "pac" / "bg", ec), end; !ec && it != end; it.increment(ec)) {
          const std::string n = it->path().filename().string();
          if (n.size() == 8 && _strnicmp(n.c_str(), "bg", 2) == 0 && isdigit(n[2]) && isdigit(n[3]) &&
              port13::Find13(atoi(n.c_str() + 2)))  // (arenas only: not backstage rooms or match structures)
            w13_list_.push_back(atoi(n.c_str() + 2));
        }
        std::sort(w13_list_.begin(), w13_list_.end());
        if (w13_list_.empty())
          port_note_ = fr ? "Pas d'arènes ici (dossier pac\\bg de WWE '13 attendu)." : "No arenas here (WWE '13's "
                                                                                        "pac\\bg folder expected).";
        else
          port_note_.clear();
      }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", w13_dir_.empty() ? (fr ? "(vos fichiers de WWE '13)" : "(your WWE '13 files)")
                                               : media::Utf8(w13_dir_.wstring()).c_str());
    if (!w13_list_.empty()) {
      ImGui::SetNextItemWidth(360);
      if (ImGui::BeginCombo(fr ? "Arène de WWE '13" : "WWE '13 arena", w13_pick_ < 0 ? (fr ? "Choisir..." : "Choose...")
                                                                                     : Label13(w13_pick_).c_str(),
                            ImGuiComboFlags_HeightLarge)) {
        for (int n : w13_list_)
          if (ImGui::Selectable(Label13(n).c_str(), n == w13_pick_)) {
            w13_pick_ = n;
            Port13(fr);
          }
        ImGui::EndCombo();
      }
    }
    if (!port_note_.empty())
      ImGui::TextColored(port_ok_ ? kGreen : kRed, "%s", port_note_.c_str());
    if (port_ok_ && !ramp_note_.empty()) {
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 720);
      ImGui::TextColored(kYellow, "%s", ramp_note_.c_str());
      ImGui::PopTextWrapPos();
    }
    if (port_ok_) {
      if (ImGui::Button(fr ? "Voir en 3D" : "See it in 3D")) {
        view_port_ = true;
        view_stale_ = true;
        mode_ = 2;
      }
      ImGui::SameLine();
      ImGui::TextDisabled("%s", fr ? "(revenez ensuite sur « Porter de WWE '13 » pour créer le mod)"
                                   : "(then come back to \"Port from WWE '13\" to make the mod)");
    }
  }

  // An arena of SvR 2011 (its pac/bg/bgNN.pac); it plays as the arena chosen above (its base: the recipe takes the
  // ring, the crowd and the match lights from it).
  void DrawPortSvr(bool fr) {
    // The game: its ISO (or its folder), whose arenas are listed by name; or one arena file.
    if (ImGui::Button(fr ? "ISO ou dossier de SvR 2011..." : "SvR 2011 ISO or folder...")) {
      const fs::path p = PickGame(fr);
      if (!p.empty()) {
        svr_list_.clear();
        svr_pick_ = -1;
        if (svr_disc_.OpenPath(p)) {
          svr_game_ = p;
          for (const auto &n : svr_disc_.List("pac/bg"))
            if (n.size() == 8 && _strnicmp(n.c_str(), "bg", 2) == 0 && isdigit(n[2]) && isdigit(n[3]) &&
                port_svr_arena::FindSvr(atoi(n.c_str() + 2)))
              svr_list_.push_back(atoi(n.c_str() + 2));
          std::sort(svr_list_.begin(), svr_list_.end());
        }
        port_note_ = svr_list_.empty() ? (fr ? "Pas d'arènes de SvR 2011 ici." : "No SvR 2011 arenas here.") : "";
        port_ok_ = false;
      }
    }
    ImGui::SameLine();
    if (ImGui::Button(fr ? "Un fichier d'arène (bgNN.pac)..." : "One arena file (bgNN.pac)...")) {
      const fs::path p = PickOpen(fr, true);
      if (!p.empty()) {
        std::ifstream f(p, std::ios::binary);
        svr_file_.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        svr_path_ = p;
        svr_pick_ = -1;
        PortSvr(fr);
      }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%s", !svr_game_.empty() && svr_pick_ >= 0 ? media::Utf8(svr_game_.filename().wstring()).c_str()
                              : !svr_path_.empty()                ? media::Utf8(svr_path_.wstring()).c_str()
                              : !svr_game_.empty() ? media::Utf8(svr_game_.filename().wstring()).c_str()
                                                   : (fr ? "(votre ISO de SvR 2011)" : "(your SvR 2011 ISO)"));
    if (!svr_list_.empty()) {
      ImGui::SetNextItemWidth(360);
      const auto label = [](int n) {
        char b[96];
        snprintf(b, sizeof(b), "%s  (bg%02d)", port_svr_arena::FindSvr(n)->name, n);
        return std::string(b);
      };
      if (ImGui::BeginCombo(fr ? "Arène de SvR 2011" : "SvR 2011 arena",
                            svr_pick_ < 0 ? (fr ? "Choisir..." : "Choose...") : label(svr_pick_).c_str(),
                            ImGuiComboFlags_HeightLarge)) {
        for (int n : svr_list_)
          if (ImGui::Selectable(label(n).c_str(), n == svr_pick_)) {
            char rel[32];
            snprintf(rel, sizeof(rel), "pac/bg/bg%02d.pac", n);
            if (svr_disc_.Read(rel, svr_file_)) {
              svr_pick_ = n;
              svr_path_.clear();
              PortSvr(fr);
            }
          }
        ImGui::EndCombo();
      }
    }
    if (!port_note_.empty()) {
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 720);
      ImGui::TextColored(port_ok_ ? kGreen : kRed, "%s", port_note_.c_str());
      ImGui::PopTextWrapPos();
    }
    if (port_ok_ && !ramp_note_.empty()) {
      ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 720);
      ImGui::TextColored(kYellow, "%s", ramp_note_.c_str());
      ImGui::PopTextWrapPos();
    }
    if (port_ok_) {
      if (ImGui::Button(fr ? "Voir en 3D##svr" : "See it in 3D##svr")) {
        view_port_ = true;
        view_stale_ = true;
        mode_ = 2;
      }
      ImGui::SameLine();
      ImGui::TextDisabled("%s", fr ? "(revenez ensuite sur « Porter de SvR 2011 » pour créer le mod)"
                                   : "(then come back to \"Port from SvR 2011\" to make the mod)");
    }
  }

  // The recipe (port_svr_arena.h) on the chosen file, its base the slot chosen above.
  void PortSvr(bool fr) {
    port_ok_ = false;
    ramp_note_.clear();
    card_mb_.clear();
    card_mf_.clear();
    etex_.clear();
    load_.clear();
    movie_.clear();
    Bytes out;
    std::string why;
    port_svr_arena::PortInfo info;
    if (!port_svr_arena::Port(svr_file_, shipped_, out, &why, &info) || !other_.Load(out, &why)) {
      port_note_ = (fr ? "Portage impossible : " : "Can't port it: ") + why;
      return;
    }
    port_ok_ = true;
    other_size_ = out.size();
    char b[300];
    snprintf(b, sizeof(b),
             fr ? "Prête : %d objets convertis, jouée comme %s (%s)."
                : "Ready: %d objects converted, played as %s (%s).",
             info.models, kSlots[slot_].name, Human(out.size()).c_str());
    port_note_ = b;
    if (info.models_left) {
      snprintf(b, sizeof(b), fr ? " %d objet(s) laissé(s) tel(s) quel(s)." : " %d object(s) left as they are.",
               info.models_left);
      port_note_ += b;
    }
    if (out.size() > budget_)
      port_note_ += fr ? " Plus grande que la place : ses plus grandes images seront réduites."
                       : " Bigger than the room: its biggest pictures will be halved.";
    if (!info.RampsMatch()) {
      snprintf(b, sizeof(b),
               fr ? "Attention : la rampe de cette arène et celle de %s ne sont pas à la même hauteur (%.2f m contre "
                    "%.2f m) : les catcheurs ne suivront pas la rampe pendant les entrées."
                  : "Warning: this arena's ramp and %s's don't stand at the same height (%.2f m against %.2f m): the "
                    "wrestlers won't follow the ramp during the entrances.",
               kSlots[slot_].name, info.ramp_svr / 100, info.ramp12 / 100);
      ramp_note_ = b;
    }
    // Its own banner from SvR 2011's menus (from the ISO), else the base's.
    Bytes matchhd;
    svrfmt::Image img;
    if (svr_pick_ >= 0 && svr_disc_.Read("pac/menu/MatchHD.pac", matchhd) &&
        port_svr_arena::Banner(matchhd, svr_pick_, img)) {
      SetBanner(Fill(img, 256, 128));
      banner_custom_ = true;
    } else {
      banner_custom_ = false;
      LoadBaseBanner();
    }
    const std::string stem = svr_pick_ >= 0 ? port_svr_arena::FindSvr(svr_pick_)->name
                                            : media::Utf8(svr_path_.stem().wstring());
    snprintf(name_, sizeof(name_), "%s (SvR 2011)", stem.c_str());
    as_new_ = true;
  }

  // An ISO (or a game folder).
  static fs::path PickGame(bool fr) {
    IFileOpenDialog *dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
      return {};
    const COMDLG_FILTERSPEC filter[] = {{fr ? L"Image du jeu (*.iso)" : L"Game image (*.iso)", L"*.iso"},
                                        {L"*.*", L"*.*"}};
    dlg->SetFileTypes(2, filter);
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);
    dlg->SetTitle(fr ? L"ISO de SvR 2011" : L"SvR 2011 ISO");
    fs::path picked;
    if (SUCCEEDED(dlg->Show(GetActiveWindow()))) {
      IShellItem *item = nullptr;
      if (SUCCEEDED(dlg->GetResult(&item))) {
        PWSTR q = nullptr;
        if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &q))) {
          picked = q;
          CoTaskMemFree(q);
        }
        item->Release();
      }
    }
    dlg->Release();
    return picked;
  }

  static std::string Label13(int n) {
    char b[96];
    const char *name = port13::ArenaName(n);
    snprintf(b, sizeof(b), "%s  (bg%02d)", *name ? name : "WWE '13 arena", n);
    return b;
  }

  static fs::path PickFolder(bool fr) {
    IFileOpenDialog *dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
      return {};
    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    dlg->SetTitle(fr ? L"Dossier du jeu WWE '13" : L"The WWE '13 game folder");
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

  // The recipe (port13.h) on the chosen arena, its base the slot chosen above.
  void Port13(bool fr) {
    port_ok_ = false;
    char rel[32];
    snprintf(rel, sizeof(rel), "bg%02d.pac", w13_pick_);
    std::ifstream f(w13_dir_ / "pac" / "bg" / rel, std::ios::binary);
    const Bytes w13((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    Bytes out;
    std::string why;
    port13::PortInfo info;
    ramp_note_.clear();
    card_mb_.clear();
    card_mf_.clear();
    etex_.clear();
    load_.clear();
    movie_.clear();
    if (w13.empty() || !port13::Port(w13, shipped_, out, &why, &info) || !other_.Load(out, &why)) {
      port_note_ = (fr ? "Portage impossible : " : "Can't port it: ") + why;
      return;
    }
    port_ok_ = true;
    other_size_ = out.size();
    port_note_ = (fr ? "Prête : " : "Ready: ") + Label13(w13_pick_) + (fr ? ", jouée comme " : ", played as ") +
                 kSlots[slot_].name + " (" + Human(out.size()) + ").";
    if (out.size() > budget_)
      port_note_ += fr ? " Plus grande que la place : ses plus grandes images seront réduites."
                       : " Bigger than the room: its biggest pictures will be halved.";
    // The entrance follows the base's ramp: a WWE '13 ramp at floor level wants a base without a raised one.
    if (!info.RampsMatch()) {
      char b[400];
      if (info.ramp13 < 60)
        snprintf(b, sizeof(b),
                 fr ? "Attention : la rampe de cette arène est au sol, celle de %s est à %.2f m : les catcheurs "
                      "flotteront pendant les entrées. Choisissez Royal Rumble ou Bragging Rights comme arène de base."
                    : "Warning: this arena's ramp is at floor level, %s's stands %.2f m high: the wrestlers will float "
                      "during the entrances. Choose Royal Rumble or Bragging Rights as the base arena.",
                 kSlots[slot_].name, info.ramp12 / 100);
      else
        snprintf(b, sizeof(b),
                 fr ? "Attention : la rampe de cette arène est à %.2f m, celle de %s est au sol : les entrées ne "
                      "suivront pas la rampe. Choisissez une arène de base avec une rampe (SmackDown, RAW...)."
                    : "Warning: this arena's ramp stands %.2f m high, %s's is at floor level: the entrances won't "
                      "follow the ramp. Choose a base arena with a ramp (SmackDown, RAW...).",
                 info.ramp13 / 100, kSlots[slot_].name);
      ramp_note_ = b;
    }
    // Its own banner from WWE '13's menus, else the base's.
    std::ifstream m(w13_dir_ / "pac" / "menu" / "MatchHD.pac", std::ios::binary);
    const Bytes matchhd((std::istreambuf_iterator<char>(m)), std::istreambuf_iterator<char>());
    svrfmt::Image img;
    if (!matchhd.empty() && port13::Card(matchhd, w13_pick_, card_mb_, card_mf_))
      port_note_ += fr ? " Avec son écran VS." : " With its VS screen.";
    // Its entrance graphics (location plate, name plates).
    std::ifstream ev(w13_dir_ / "pac" / "evt" / "evtgdhd.pac", std::ios::binary);
    const Bytes evtgd((std::istreambuf_iterator<char>(ev)), std::istreambuf_iterator<char>());
    if (!evtgd.empty() && port13::Etex(evtgd, w13_pick_, etex_))
      port_note_ += fr ? " Avec ses plaques d'entrée." : " With its entrance plates.";
    // Its pre-match screen, and its transition video.
    std::ifstream ld(w13_dir_ / "pac" / "menu" / "LoadHD.pac", std::ios::binary);
    const Bytes loadhd((std::istreambuf_iterator<char>(ld)), std::istreambuf_iterator<char>());
    if (!loadhd.empty())
      port13::PreMatch(loadhd, w13_pick_, load_);
    std::ifstream mv(w13_dir_ / "pac" / "evt" / "evtmov.pac", std::ios::binary);
    const Bytes evtmov((std::istreambuf_iterator<char>(mv)), std::istreambuf_iterator<char>());
    if (!evtmov.empty() && port13::TransitionVideo(evtmov, w13_pick_, movie_))
      port_note_ += fr ? " Avec sa transition." : " With its transition.";
    if (!matchhd.empty() && port13::Banner(matchhd, w13_pick_, img)) {
      SetBanner(Fill(img, 256, 128));
      banner_custom_ = true;
    } else {
      banner_custom_ = false;
      LoadBaseBanner();
    }
    snprintf(name_, sizeof(name_), "%s (WWE '13)", *port13::ArenaName(w13_pick_) ? port13::ArenaName(w13_pick_)
                                                                                  : Label13(w13_pick_).c_str());
    as_new_ = true;
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

  // The new arena's banner (256 x 128, the big tiles' size): a picture of the player's, else the base's own.
  void DrawBanner(bool fr) {
    if (banner_slot_ != slot_ && !banner_custom_) {
      banner_slot_ = slot_;
      LoadBaseBanner();
    }
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(at, ImVec2(at.x + 256, at.y + 128), IM_COL32(40, 40, 44, 255));
    if (banner_thumb_.srv)
      ImGui::Image(ImTextureRef(ImTextureID(reinterpret_cast<intptr_t>(banner_thumb_.srv.Get()))), ImVec2(256, 128));
    else
      ImGui::Dummy(ImVec2(256, 128));
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::TextWrapped("%s", fr ? "Sa case dans le choix d'arène (pages suivantes, LB / RB). Elle joue comme l'arène "
                                  "choisie plus haut : son numéro, son éclairage, son écran VS."
                                : "Its tile on the arena select screen (next pages, LB / RB). It plays as the arena "
                                  "chosen above: its number, its lights, its VS screen.");
    if (ImGui::Button(fr ? "Choisir l'image de la bannière..." : "Choose the banner picture...")) {
      const fs::path p = PickOpen(fr, false);
      svrfmt::Image img;
      if (!p.empty() && wic_image::Load(p, img)) {
        SetBanner(Fill(img, 256, 128));
        banner_custom_ = true;
      }
    }
    ImGui::SameLine();
    if (ImGui::Button(fr ? "Bannière de l'arène de base" : "The base arena's banner")) {
      banner_custom_ = false;
      LoadBaseBanner();
    }
    ImGui::EndGroup();
  }

  // The picture cut to w:h (its middle) and resized.
  static svrfmt::Image Fill(const svrfmt::Image &img, int w, int h) {
    int cw = img.w, ch = img.w * h / w;
    if (ch > img.h) {
      ch = img.h;
      cw = img.h * w / h;
    }
    const int x0 = (img.w - cw) / 2, y0 = (img.h - ch) / 2;
    svrfmt::Image crop;
    crop.w = cw;
    crop.h = ch;
    crop.rgba.resize(size_t(cw) * ch * 4);
    for (int y = 0; y < ch; ++y)
      memcpy(&crop.rgba[size_t(y) * cw * 4], &img.rgba[(size_t(y0 + y) * img.w + x0) * 4], size_t(cw) * 4);
    return svrfmt::Resize(crop, w, h);
  }

  void SetBanner(svrfmt::Image img) {
    for (size_t i = 3; i < img.rgba.size(); i += 4)
      img.rgba[i] = 255;  // (a tile has no transparency)
    banner_ = std::move(img);
    banner_thumb_ = MakeThumb(device_, svrfmt::DdsEncode(banner_, svrfmt::DxtFormat::kDxt5, false));
  }

  // The base arena's banner, from the game's menu/MatchHD.pac (read once).
  void LoadBaseBanner() {
    if (!banners_read_ && disc_) {
      banners_read_ = true;
      Bytes matchhd;
      svrfmt::Archive a;
      if (disc_->Read("pac/menu/MatchHD.pac", matchhd) && svrfmt::ArchiveOpen(matchhd, a, nullptr))
        for (auto &k : a.root.kids)
          if (k.group == "MLYE" && k.name == "MUPA" && svrfmt::ArchiveExpand(k))
            for (auto &e : k.kids)
              if (e.id == 7001)
                svrfmt::BundleRead(svrfmt::ArchiveData(e), game_banners_);
    }
    char name[16];
    snprintf(name, sizeof(name), "arena%02d", slot_ >= 0 ? kSlots[slot_].number : 0);
    for (const auto &t : game_banners_) {
      svrfmt::Image img;
      if (t.name == name && svrfmt::DdsDecode(t.data, img)) {
        SetBanner(Fill(img, 256, 128));
        return;
      }
    }
    svrfmt::Image grey;
    grey.w = 256;
    grey.h = 128;
    grey.rgba.assign(size_t(256) * 128 * 4, 90);
    SetBanner(grey);
  }

  void Make(bool fr, mods::Manager &mods) {
    message_ok_ = false;
    reduced_note_.clear();
    arena_edit::Arena &a = mode_ == 1 || mode_ >= 3 ? other_ : arena_;
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
    std::string id = (as_new_ ? "newarena_" : "arena_") + slot + "_";
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
    if (as_new_)
      manifest += "slot=new\nbase=" + std::to_string(kSlots[slot_].number) + "\ndescription=" +
                  (mode_ == 3   ? std::string(name_) + ", ported from WWE '13 (the player's own copy), playing as "
                   : mode_ == 4 ? std::string(name_) + ", ported from SvR 2011 (the player's own file), playing as "
                                : std::string("A new arena playing as ")) +
                  kSlots[slot_].name + ", made with the WWE '12 Mod Maker.\n";
    else
      manifest += "description=" + std::string(kSlots[slot_].name) + " (" + slot + ".pac)" +
                  (mode_ >= 3 ? " replaced by " + std::string(name_)
                   : mode_ == 1 ? " replaced by another arena"
                                : ", " + std::to_string(Changed()) + " picture(s) changed") +
                  ", made with the WWE '12 Mod Maker.\n";
    std::vector<std::pair<std::string, Bytes>> entries = {{"manifest.txt", Bytes(manifest.begin(), manifest.end())}};
    if (as_new_) {
      if (banner_.rgba.empty())
        LoadBaseBanner();
      entries.push_back({"arena/arena.pac", std::move(out)});
      entries.push_back({"arena/banner.dds", svrfmt::DdsEncode(banner_, svrfmt::DxtFormat::kDxt5, false)});
      if (mode_ == 3 && !card_mb_.empty()) {  // its own VS screen (MatchHD.pac MLYE/MBnn and MENU/MnnF)
        entries.push_back({"arena/card_mb.pac", card_mb_});
        entries.push_back({"arena/card_mf.pac", card_mf_});
      }
      if (mode_ == 3 && !etex_.empty())  // its entrance graphics (evtgdhd.pac ETEX/00NN)
        entries.push_back({"arena/etex_hd.pac", etex_});
      if (mode_ == 3 && !load_.empty())  // its pre-match screen (LoadHD.pac LOAD/90NN)
        entries.push_back({"arena/load_hd.pac", load_});
      if (mode_ == 3 && !movie_.empty())  // its transition video (evtmov.pac MOVT/M0NN)
        entries.push_back({"arena/transition.bik", movie_});
    } else {
      entries.push_back({"files/" + Path(), std::move(out)});
    }
    if (!mods::Manager::WriteZip(save, entries)) {
      message_ = (fr ? "Impossible d'écrire " : "Could not write ") + media::Utf8(save.wstring());
      return;
    }
    mods.InstallFile(fr, save);
    message_ok_ = true;
    message_ = (fr ? "Mod créé et installé : " : "Mod made and installed: ") + media::Utf8(save.wstring()) +
               (as_new_ ? (fr ? " (au prochain lancement du jeu : choix d'arène, LB / RB)"
                              : " (next time the game starts: arena select, LB / RB)")
                        : "");
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

  // Porting from WWE '13.
  fs::path w13_dir_;
  std::vector<int> w13_list_;
  int w13_pick_ = -1;
  bool port_ok_ = false, view_port_ = false;
  std::string port_note_, ramp_note_;
  Bytes card_mb_, card_mf_;  // its VS screen from WWE '13 (port13::Card)
  Bytes etex_;                // its entrance graphics from WWE '13 (port13::Etex)
  Bytes load_;                // its pre-match screen from WWE '13 (port13::PreMatch)
  Bytes movie_;               // its transition video from WWE '13 (port13::TransitionVideo)
  Bytes svr_file_;            // the SvR 2011 arena file (port_svr_arena.h)
  fs::path svr_path_;         // ... a file chosen alone
  disc::GameDisc svr_disc_;   // ... or the game's ISO / folder, and its arena chosen
  fs::path svr_game_;
  std::vector<int> svr_list_;
  int svr_pick_ = -1;

  // New arena (added, not replacing) and its banner.
  bool as_new_ = true, banner_custom_ = false, banners_read_ = false;
  int banner_slot_ = -2;
  svrfmt::Image banner_;
  Thumb banner_thumb_;
  std::vector<svrfmt::BundleTexture> game_banners_;
  const disc::GameDisc *disc_ = nullptr;

  fs::path other_path_;
  arena_edit::Arena other_;
  bool other_ok_ = false;
  size_t other_size_ = 0;
  std::string other_note_;
};

}  // namespace arena_page
