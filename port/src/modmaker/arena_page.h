// Mod Maker, "Arenas" page (docs/mod_maker.md, step 5; docs/plan_page_arenes.md). Its tabs:
//  - New arena: the assistant, an arena of another game (My game discs: SvR 2011 with port_svr_arena.h, WWE '13 with
//    port13.h; or one SvR 2011 arena file) added as a new arena: its own tile on the arena select screen's next pages,
//    playing on a base arena's number (manifest slot=new, base=NN; arena/arena.pac, arena/banner.dds 256 x 128 DXT5,
//    and what comes with it: VS screen, entrance plates, pre-match screen, transition video;
//    src/native/wwe12_arena_pages.cpp);
//  - Retexture: pick one of the game's arenas, see all its pictures (mat, aprons, banners, ramp, crowd signs...),
//    replace any with a picture of yours (fitted to the original's size and format) or export it as PNG to edit;
//  - Another arena file: put an arena .pac (made elsewhere, another slot's) in a slot;
//  - 3D view.
// The file is checked against the base's shipped size and made to fit (arena_edit.h), and its stage number is set to
// the base's. The banner is a picture of the player's, the game's own (SvR 2011, WWE '13), or the base arena's
// (menu/MatchHD.pac, MLYE/MUPA entry 7001, "arenaNN").
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <shellapi.h>
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
#include "mesh_import.h"
#include "arena_view.h"
#include "port13.h"
#include "port_svr_arena.h"
#include "strings2k.h"
#include "game_disc.h"
#include "game_discs.h"
#include "game_process.h"
#include "imgui.h"
#include "mod_version.h"
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

// Starts the game (wwe12.exe next to the Mod Maker), to try a mod just installed.
inline void LaunchGame() {
  wchar_t me[MAX_PATH];
  GetModuleFileNameW(nullptr, me, MAX_PATH);
  const fs::path dir = fs::path(me).parent_path();
  ShellExecuteW(nullptr, L"open", (dir / L"wwe12.exe").wstring().c_str(), nullptr, dir.wstring().c_str(),
                SW_SHOWNORMAL);
}

// (kind / pattern: another kind of file, e.g. L"FBX", L"*.fbx")
inline fs::path PickOpen(bool fr, bool pac, const wchar_t *kind = nullptr, const wchar_t *pattern = nullptr) {
  IFileOpenDialog *dlg = nullptr;
  if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg))))
    return {};
  const COMDLG_FILTERSPEC pics[] = {{fr ? L"Images" : L"Pictures", L"*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff"},
                                    {L"*.*", L"*.*"}};
  const COMDLG_FILTERSPEC pacs[] = {{fr ? L"Arènes (*.pac)" : L"Arenas (*.pac)", L"*.pac"}, {L"*.*", L"*.*"}};
  const COMDLG_FILTERSPEC other[] = {{kind ? kind : L"", pattern ? pattern : L"*.*"}, {L"*.*", L"*.*"}};
  dlg->SetFileTypes(2, kind ? other : pac ? pacs : pics);
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
  // A colour zone mask (Mask1-3: one zone per channel, mostly transparent): each zone in its own colour.
  svrfmt::DdsInfo info;
  if (svrfmt::DdsInfoOf(dds, info) && info.format == svrfmt::DxtFormat::kArgb4444) {
    static const int kZone[4][3] = {{230, 70, 60}, {80, 200, 90}, {70, 120, 235}, {240, 200, 60}};
    for (size_t i = 0; i < img.rgba.size(); i += 4) {
      int c[3] = {24, 24, 28};
      for (int z = 0; z < 4; ++z)
        if (img.rgba[i + z] >= 128)
          for (int k = 0; k < 3; ++k)
            c[k] = std::max(c[k], kZone[z][k]);
      for (int k = 0; k < 3; ++k)
        img.rgba[i + k] = uint8_t(c[k]);
      img.rgba[i + 3] = 255;
    }
  }
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
    // The tabs: a new arena from another game (the assistant), the game's arenas retextured, another arena file in
    // place of one, the 3D view.
    int tab = tab_;
    if (ImGui::BeginTabBar("arena_tabs")) {
      const char *names[] = {fr ? "Nouvelle arène" : "New arena", fr ? "Retoucher une arène du jeu" : "Retexture an arena",
                             fr ? "Autre fichier d'arène" : "Another arena file", fr ? "Vue 3D" : "3D view",
                             fr ? "Éditeur 3D" : "3D editor"};
      for (int i = 0; i < 5; ++i)
        if (ImGui::BeginTabItem(names[i])) {
          tab = i;
          ImGui::EndTabItem();
        }
      ImGui::EndTabBar();
    }
    if (tab != tab_) {
      tab_ = tab;
      view_port_ = tab_ == 0;
      view_stale_ = true;
      message_.clear();
    }
    if (tab_ == 0) {
      if (wiz_src_ >= 0)
        mode_ = wiz_src_ == 1 ? 3 : 4;
      DrawWizard(fr, disc, mods);
      return;
    }
    mode_ = tab_ == 1 ? 0 : tab_ == 2 ? 1 : tab_ == 3 ? 2 : 5;

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
    else if (mode_ == 5)
      DrawEditor(fr);
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
    mod_version::Input("Version##arena_version", version_, sizeof(version_));
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
    int n = int(arena_.Moved());
    for (const auto &t : arena_.Textures())
      n += t.replaced;
    return n;
  }

  // The 3D editor: an object of the arena moved, turned or hidden (arena_edit.h Move), seen in the 3D view.
  void DrawEditor(bool fr) {
    const float h = std::max(300.0f, ImGui::GetContentRegionAvail().y - 110);
    ImGui::BeginChild("ed_list", ImVec2(300, h), ImGuiChildFlags_Borders);
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##ed_filter", fr ? "Rechercher un objet" : "Search an object", ed_filter_, sizeof(ed_filter_));
    if (ed_models_.empty())
      ed_models_ = arena_.Models();
    for (const auto &[id, name] : ed_models_) {
      if (id >= 950 && id < 1000)  // (the ring's pieces, 951-980: the game puts them in place itself; moved, they stay)
        continue;
      std::string a = name, b = ed_filter_;
      for (auto *x : {&a, &b})
        std::transform(x->begin(), x->end(), x->begin(), [](unsigned char c) { return char(std::tolower(c)); });
      if (!b.empty() && a.find(b) == std::string::npos)
        continue;
      const bool moved = !arena_.MoveOf(id).None();
      char label[96];
      snprintf(label, sizeof(label), "%s%s%s##%u", name.c_str(), arena_.IsCopy(id) ? (fr ? " (copie)" : " (copy)") : "",
               moved ? "  *" : "", id);
      if (ImGui::Selectable(label, ed_pick_ == int(id))) {
        ed_pick_ = int(id);
        ed_move_ = arena_.MoveOf(id);
      }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("ed_right", ImVec2(0, h), ImGuiChildFlags_Borders);
    if (ed_pick_ >= 0) {
      bool changed = false;
      ImGui::SetNextItemWidth(360);
      changed |= ImGui::DragFloat3(fr ? "Décalage X / Y / Z (cm, Y vers le bas)" : "Offset X / Y / Z (cm, Y down)",
                                   ed_move_.d, 1.0f, -5000.0f, 5000.0f, "%.0f");
      ImGui::SetNextItemWidth(160);
      changed |= ImGui::DragFloat(fr ? "Rotation (degrés)" : "Turn (degrees)", &ed_move_.turn, 1.0f, -180.0f, 180.0f, "%.0f");
      changed |= ImGui::Checkbox(fr ? "Cacher" : "Hide", &ed_move_.hide);
      ImGui::SameLine();
      if (ImGui::SmallButton(fr ? "Remettre" : "Reset")) {
        ed_move_ = arena_edit::Move();
        changed = true;
      }
      if (changed) {
        arena_.SetMove(uint32_t(ed_pick_), ed_move_);
        view_stale_ = true;
      }
      // Another one of it (a second barricade, screen, ladder...), placed beside it; a copy can be taken away again.
      if (ImGui::Button(fr ? "Dupliquer" : "Duplicate")) {
        if (const uint32_t id = arena_.AddCopy(uint32_t(ed_pick_))) {
          arena_.Build();
          ed_models_.clear();
          ed_pick_ = int(id);
          ed_move_ = arena_.MoveOf(id);
          view_stale_ = true;
        }
      }
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", fr ? "Ajoute une copie de cet objet dans l'arène, 3 m à côté : déplacez-la ensuite."
                                   : "Adds a copy of this object to the arena, 3 m beside it: then move it.");
      // An object of one's own from a 3D file (OBJ / FBX), made on this one (its material, its place), and this one
      // written out as OBJ (Kaiko's arena objects).
      ImGui::SameLine();
      if (ImGui::Button(fr ? "Ajouter un objet 3D (OBJ, FBX)..." : "Add a 3D object (OBJ, FBX)..."))
        AddObjectFromFile(fr);
      if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", fr ? "Un modèle 3D (OBJ en mètres, ou FBX) devient un nouvel objet de l'arène, posé à la place "
                                     "de cet objet-ci (sa matière, sa lumière) avec l'image du fichier : déplacez-le ensuite."
                                   : "A 3D model (OBJ in metres, or FBX) becomes a new object of the arena, put where this "
                                     "object is (its material, its light) with the file's picture: then move it.");
      ImGui::SameLine();
      if (ImGui::Button(fr ? "Exporter en OBJ..." : "Export as OBJ..."))
        ExportObject(fr);
      ImGui::SameLine();
      if (ImGui::Button(fr ? "Voir cet objet" : "Look at it"))
        FocusOn(uint32_t(ed_pick_));
      // Where a 3D file's object goes, set before it is added (then it moves as any object).
      ImGui::TextDisabled("%s", fr ? "Objet 3D à ajouter :" : "3D object to add:");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(80);
      ImGui::DragFloat(fr ? "taille x" : "size x", &ed_scale_, 0.01f, 0.01f, 100.0f, "%.2f");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(220);
      ImGui::DragFloat3(fr ? "décalage (cm)" : "offset (cm)", ed_import_.d, 1.0f, -5000.0f, 5000.0f, "%.0f");
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::CalcTextSize(fr ? "Objet 3D à ajouter :" : "3D object to add:").x +
                           ImGui::GetStyle().ItemSpacing.x);
      ImGui::SetNextItemWidth(80);
      ImGui::DragFloat(fr ? "rotation" : "turn", &ed_import_.turn, 1.0f, -180.0f, 180.0f, "%.0f");
      if (!ed_note_.empty())
        ImGui::TextColored(ed_note_ok_ ? kGreen : kRed, "%s", ed_note_.c_str());
      if (arena_.IsCopy(uint32_t(ed_pick_))) {
        ImGui::SameLine();
        if (ImGui::Button(fr ? "Supprimer la copie" : "Remove the copy")) {
          arena_.RemoveCopy(uint32_t(ed_pick_));
          arena_.Build();
          ed_models_.clear();
          ed_pick_ = -1;
          view_stale_ = true;
        }
      }
    } else {
      ImGui::TextDisabled("%s", fr ? "Choisissez un objet à gauche, puis déplacez-le, tournez-le ou cachez-le."
                                   : "Pick an object on the left, then move, turn or hide it.");
    }
    Draw3D(fr);
    ImGui::EndChild();
  }

  void AddObjectFromFile(bool fr) {
    const fs::path f = PickOpen(fr, false, fr ? L"Modèles 3D (*.obj, *.fbx)" : L"3D models (*.obj, *.fbx)", L"*.obj;*.fbx");
    if (f.empty())
      return;
    mesh_import::TriMesh tm;
    std::string why;
    ed_note_ok_ = false;
    if (!mesh_import::Load(f, tm, why)) {
      ed_note_ = (fr ? "Modèle illisible : " : "Model unreadable: ") + why;
      return;
    }
    for (auto &p : tm.pos)
      for (float &x : p)
        x *= ed_scale_;
    Bytes dds;
    std::string picture;
    svrfmt::Image img;
    if (!tm.texture.empty() && wic_image::Load(tm.texture, img) && img.w > 0) {
      int w = 1, h = 1;  // (powers of two, 1024 at most, as the game's)
      while (w < img.w && w < 1024)
        w *= 2;
      while (h < img.h && h < 1024)
        h *= 2;
      if (w != img.w || h != img.h)
        img = svrfmt::Resize(img, w, h);
      dds = svrfmt::DdsEncode(img, svrfmt::DxtFormat::kDxt5, true);
      picture = "mm_obj_" + std::to_string(arena_.Models().size());
    }
    const uint32_t id = arena_.AddMeshObject(uint32_t(ed_pick_), tm.pos, tm.normal, tm.uv, tm.tris, dds, picture);
    if (!id) {
      ed_note_ = fr ? "L'objet n'a pas pu être ajouté (plus de numéro d'objet libre)." : "The object couldn't be added (no free object number).";
      return;
    }
    if (!ed_import_.None())
      arena_.SetMove(id, ed_import_);
    // Room: the arena must keep its slot's size, its pictures halved if need be (as Make does): else the object goes.
    Bytes fitted;
    arena_.Fit(Room(), fitted);
    const size_t total = fitted.size();
    arena_.Unreduce();
    if (total > Room()) {
      arena_.RemoveMeshObject(id);
      arena_.Build();
      ed_models_.clear();
      view_stale_ = true;
      char note[320];
      snprintf(note, sizeof(note),
               fr ? "Trop gros : avec cet objet (%zu triangles), l'arène ferait %s pour une place de %s, même en réduisant "
                    "ses images. Il n'est pas ajouté : allégez le modèle (moins de triangles) ou son image."
                  : "Too big: with this object (%zu triangles) the arena would take %s for a room of %s, even with its "
                    "pictures reduced. It isn't added: make the model lighter (fewer triangles) or its picture smaller.",
               tm.tris.size() / 3, Human(total).c_str(), Human(Room()).c_str());
      ed_note_ = note;
      return;
    }
    arena_.Build();
    ed_models_.clear();
    for (size_t i = thumbs_.size(); i < arena_.Textures().size(); ++i)  // (its picture, one more)
      thumbs_.push_back(MakeThumb(device_, arena_.Get(arena_.Textures()[i]).data));
    ed_pick_ = int(id);
    ed_move_ = arena_.MoveOf(id);
    FocusOn(id);
    char note[256];
    snprintf(note, sizeof(note), fr ? "Objet ajouté : %zu triangles, arène %s sur %s de place%s."
                                    : "Object added: %zu triangles, arena %s of a %s room%s.",
             tm.tris.size() / 3, Human(total).c_str(), Human(Room()).c_str(),
             dds.empty() ? (fr ? " (sans image : celle de l'objet de départ)" : " (no picture: the host's)") : "");
    ed_note_ = note;
    ed_note_ok_ = true;
  }

  // The 3D view kept on one object (its middle, far enough to see it whole) until Reset view.
  void FocusOn(uint32_t id) {
    svrfmt::Model m;
    if (!arena_.ModelOf(id, m))
      return;
    float lo[3] = {1e30f, 1e30f, 1e30f}, hi[3] = {-1e30f, -1e30f, -1e30f};
    for (const auto &me : m.meshes)
      for (const auto &v : me.verts)
        for (int q = 0; q < 3; ++q)
          lo[q] = std::min(lo[q], v.pos[q]), hi[q] = std::max(hi[q], v.pos[q]);
    if (lo[0] > hi[0])
      return;
    const float extent = std::max({hi[0] - lo[0], hi[1] - lo[1], hi[2] - lo[2]});
    for (int q = 0; q < 3; ++q)
      ed_focus_[q] = (lo[q] + hi[q]) / 2;
    ed_focus_[3] = std::max(150.0f, extent * 2.2f);
    ed_focus_on_ = true;
    view_stale_ = true;
  }

  void ExportObject(bool fr) {
    svrfmt::Model m;
    if (!arena_.ModelOf(uint32_t(ed_pick_), m))
      return;
    std::wstring name;
    for (unsigned char c : m.name)
      name += std::isalnum(c) ? wchar_t(c) : L'_';
    const fs::path out = media::PickSave(fr, (name.empty() ? L"object" : name) + L".obj");
    if (out.empty())
      return;
    auto picture = [&](const svrfmt::Mesh &me) -> std::string {
      for (const auto &p : me.params)
        if ((p.name == "sampDiffusePrm" || p.name == "texDiffuse" || p.name == "g_mDiffusePrm") && p.value.size() >= 4) {
          const uint32_t slot = svrfmt::Be32(p.value.data());
          if (slot >= m.textures.size())
            return "";
          const svrfmt::BundleTexture *t = arena_.TextureNamed(m.textures[slot]);
          svrfmt::Image img;
          if (!t || !svrfmt::DdsDecode(t->data, img))
            return "";
          const std::string file = m.textures[slot] + ".png";
          wic_image::SavePng(out.parent_path() / file, img);
          return file;
        }
      return "";
    };
    message_ = mesh_import::SaveObj(out, m, picture) ? (fr ? "Exporté : " : "Exported: ") + media::Utf8(out.wstring())
                                                     : (fr ? "Export impossible." : "Export failed.");
  }

  void OpenSlot(bool fr, const disc::GameDisc &disc) {
    ed_models_.clear();
    ed_pick_ = -1;
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
    RingColours(fr);
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
      static const char *fmts[] = {"DXT1", "DXT3", "DXT5", "ARGB", "?", "ARGB 4444"};
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

  // Ring colours (the "ring kit" of Kaiko's Mod Maker): the ropes, turnbuckle pads or posts in one colour. Their
  // pictures have the same names in every arena (ar_rop01; rin_tb00, rin_tb20; rin_pos00, rin_pos01): each is
  // tinted from the game's own picture (its brightness times the colour, transparency kept), at its size and format.
  struct RingPart {
    const char *fr, *en;
    std::vector<const char *> names;
  };
  static const std::vector<RingPart> &RingParts() {
    static const std::vector<RingPart> parts = {{"Cordes", "Ropes", {"ar_rop01"}},
                                                {"Coins (protections)", "Turnbuckle pads", {"rin_tb00", "rin_tb20"}},
                                                {"Poteaux", "Ring posts", {"rin_pos00", "rin_pos01"}}};
    return parts;
  }

  void RingColours(bool fr) {
    if (!ImGui::CollapsingHeader(fr ? "Couleurs du ring" : "Ring colours"))
      return;
    ImGui::TextDisabled("%s", fr ? "Une couleur pour les cordes, les coins ou les poteaux : l'image du jeu est teintée "
                                   "(ses reliefs gardés)."
                                 : "One colour for the ropes, turnbuckle pads or posts: the game's picture is tinted "
                                   "(its shading kept).");
    const auto &parts = RingParts();
    for (size_t i = 0; i < parts.size(); ++i) {
      ImGui::PushID(int(i));
      ImGui::SetNextItemWidth(220);
      ImGui::ColorEdit3(fr ? parts[i].fr : parts[i].en, ring_colour_[i], ImGuiColorEditFlags_NoInputs);
      ImGui::SameLine(260);
      if (ImGui::SmallButton(fr ? "Appliquer" : "Apply"))
        TintRingPart(i);
      ImGui::SameLine();
      if (ImGui::SmallButton(fr ? "Remettre" : "Back to the game's"))
        TintRingPart(i, true);
      ImGui::PopID();
    }
    if (!ring_note_.empty())
      ImGui::TextDisabled("%s", ring_note_.c_str());
  }

  void TintRingPart(size_t part, bool restore = false) {
    const float *c = ring_colour_[part];
    int done = 0;
    auto &texs = arena_.Textures();
    for (size_t i = 0; i < texs.size(); ++i) {
      auto &t = texs[i];
      const auto &orig = arena_.Original(t);
      if (std::none_of(RingParts()[part].names.begin(), RingParts()[part].names.end(),
                       [&](const char *n) { return orig.name == n; }))
        continue;
      if (restore) {
        arena_.Restore(t);
      } else {
        svrfmt::DdsInfo info;
        svrfmt::Image img;
        if (!svrfmt::DdsInfoOf(orig.data, info) || info.format == svrfmt::DxtFormat::kUnknown ||
            !svrfmt::DdsDecode(orig.data, img))
          continue;
        for (size_t p = 0; p + 3 < img.rgba.size(); p += 4) {
          // Brightness, lifted a little so a dark rope still takes the colour; then the colour times it.
          const float l = (0.299f * img.rgba[p] + 0.587f * img.rgba[p + 1] + 0.114f * img.rgba[p + 2]) / 255.0f;
          const float k = std::min(1.0f, 0.25f + l * 1.1f);
          for (int ch = 0; ch < 3; ++ch)
            img.rgba[p + size_t(ch)] = uint8_t(std::clamp(c[ch] * k * 255.0f, 0.0f, 255.0f));
        }
        arena_.Unreduce();
        arena_.Replace(t, svrfmt::DdsEncode(img, info.format, info.mips > 1));
      }
      if (i < thumbs_.size())
        thumbs_[i] = MakeThumb(device_, arena_.Get(t).data);
      ++done;
    }
    view_stale_ = true;
    message_.clear();
    reduced_note_.clear();
    ring_note_ = done ? std::to_string(done) + " image(s)" : "(-)";
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
      else if (ed_focus_on_ && mode_ == 5) {  // (the 3D editor: its object kept in view)
        view_.SetTarget(ed_focus_);
        view_.SetAngles(0.0f, 0.35f, ed_focus_[3]);
      }
    }
    if (!view_error_.empty()) {
      ImGui::TextColored(kRed, "%s", view_error_.c_str());
      return;
    }
    if (ImGui::Checkbox(fr ? "Objets des entrées (scène, rampe)" : "Entrance objects (stage, ramp)",
                        &view_.show_entrance))
      ;
    ImGui::SameLine();
    if (ImGui::Button(fr ? "Recentrer" : "Reset view")) {
      view_.ResetCamera();
      ed_focus_on_ = false;
    }
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

  // ---- The assistant "New arena" (docs/plan_page_arenes.md): an arena of another game, step by step: where it comes
  // from (My game discs, or one file), the arena it plays in place of (its base), its name and banner, what comes
  // with it, a look at it, then the mod. The ports themselves are the ones below (PortSvr, Port13).

  // The last step that can be reached: the arena chosen opens the base step, the port done all the others.
  int WizardReach() const {
    const bool source = (wiz_src_ == 1 && w13_pick_ >= 0) || ((wiz_src_ == 0 || wiz_src_ == 2) && !svr_file_.empty());
    if (!source)
      return 0;
    return slot_ >= 0 && port_ok_ ? 5 : 1;
  }

  // The source arena's ramp height (cm; SvR 2011 counts in 10 cm).
  static float SourceRamp(const Bytes &file, float to_cm) {
    svrfmt::Archive a;
    if (!svrfmt::ArchiveOpen(file, a, nullptr) || a.root.kids.empty() || !svrfmt::ArchiveExpand(a.root.kids[0]))
      return 0;
    return port13::RampHeight(a.root.kids[0]) * to_cm;
  }
  // An arena's apron picture (rin_ep00 in its texture package, 400), as stored (DDS); empty if none.
  static Bytes Apron(const Bytes &file) {
    svrfmt::Archive a;
    if (!svrfmt::ArchiveOpen(file, a, nullptr) || a.root.kids.empty() || !svrfmt::ArchiveExpand(a.root.kids[0]))
      return {};
    for (const auto &k : a.root.kids[0].kids) {
      std::vector<svrfmt::BundleTexture> t;
      if (k.id != 400 || !svrfmt::BundleRead(svrfmt::ArchiveData(k), t))
        continue;
      for (const auto &x : t)
        if (_stricmp(x.name.c_str(), "rin_ep00") == 0)
          return x.data;
    }
    return {};
  }
  static bool FlatBase(int number) {
    return std::find(std::begin(port13::kFlatBases12), std::end(port13::kFlatBases12), number) !=
           std::end(port13::kFlatBases12);
  }

  // The source arena chosen: read, measured, and ported again if a base is chosen already.
  void WizardSourceChosen(bool fr) {
    port_ok_ = false;
    port_note_.clear();
    ramp_note_.clear();
    if (wiz_src_ == 1) {
      char rel[32];
      snprintf(rel, sizeof(rel), "pac/bg/bg%02d.pac", w13_pick_);
      const Bytes w13 = Read13(rel);
      wiz_src_size_ = w13.size();
      wiz_src_ramp_ = SourceRamp(w13, 1);
      wiz_apron_ = MakeThumb(device_, Apron(w13));
    } else {
      wiz_src_size_ = svr_file_.size();
      wiz_src_ramp_ = SourceRamp(svr_file_, 10);
      wiz_apron_ = MakeThumb(device_, Apron(svr_file_));
    }
    mode_ = wiz_src_ == 1 ? 3 : 4;
    if (slot_ >= 0 && budget_) {
      if (wiz_src_ == 1)
        Port13(fr);
      else
        PortSvr(fr);
    }
  }

  // The room of each base arena (its shipped file's size), read once.
  void WizardRooms(const disc::GameDisc &disc) {
    if (!wiz_rooms_.empty())
      return;
    for (const auto &sl : kSlots) {
      char p[32];
      snprintf(p, sizeof(p), "pac/bg/bg%02d.pac", sl.number);
      Bytes b;
      wiz_rooms_.push_back(disc.Read(p, b) ? b.size() : 0);
    }
  }

  // The base proposed: a ramp like the source's, free (no other added arena on it), room for it; then the most room.
  int ProposedBase(const mods::Manager &mods) const {
    int best = -1, best_score = -1;
    for (int i = 0; i < int(std::size(kSlots)); ++i) {
      const int n = kSlots[i].number;
      const size_t room = i < int(wiz_rooms_.size()) ? wiz_rooms_[size_t(i)] : 0;
      int score = 0;
      score += FlatBase(n) == (wiz_src_ramp_ < 60) ? 1000 : 0;  // (else the wrestlers float or sink at the entrances)
      score += SameBase(mods, n).empty() ? 100 : 0;
      score += room >= wiz_src_size_ ? 10 : 0;
      if (score > best_score || (score == best_score && room > wiz_rooms_[size_t(best)])) {
        best = i;
        best_score = score;
      }
    }
    return best;
  }

  void WizardChooseBase(bool fr, const disc::GameDisc &disc, int i) {
    slot_ = i;
    OpenSlot(fr, disc);
    view_stale_ = true;
    if (!budget_)
      return;
    if (wiz_src_ == 1)
      Port13(fr);
    else
      PortSvr(fr);
  }

  void DrawWizard(bool fr, const disc::GameDisc &disc, mods::Manager &mods) {
    const char *steps_fr[] = {"1. Point de départ", "2. Arène de base", "3. Nom et bannière",
                              "4. Ce qui est repris", "5. Aperçu", "6. Terminer"};
    const char *steps_en[] = {"1. Start from", "2. Base arena", "3. Name and banner", "4. What comes with it",
                              "5. Look at it", "6. Finish"};
    const int reach = WizardReach();
    wiz_step_ = std::min(wiz_step_, reach);
    const float h = std::max(320.0f, ImGui::GetContentRegionAvail().y - 72);
    ImGui::BeginChild("wiz_steps", ImVec2(190, h), ImGuiChildFlags_Borders);
    for (int i = 0; i < 6; ++i) {
      ImGui::BeginDisabled(i > reach);
      if (ImGui::Selectable(fr ? steps_fr[i] : steps_en[i], wiz_step_ == i, 0, ImVec2(0, 26)))
        wiz_step_ = i;
      ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("wiz_body", ImVec2(0, h), ImGuiChildFlags_Borders);
    switch (wiz_step_) {
    case 0: WizardStart(fr); break;
    case 1: WizardBase(fr, disc, mods); break;
    case 2: WizardName(fr); break;
    case 3: WizardWith(fr); break;
    case 4: WizardLook(fr); break;
    default: WizardFinish(fr, mods); break;
    }
    ImGui::EndChild();

    // Below: back / next and the progress; then what keeps it from being done (red) or is worth a look (yellow).
    ImGui::BeginDisabled(wiz_step_ == 0);
    if (ImGui::Button(fr ? "Retour" : "Back", ImVec2(110, 0)))
      --wiz_step_;
    ImGui::EndDisabled();
    ImGui::SameLine();
    char prog[32];
    snprintf(prog, sizeof(prog), fr ? "Étape %d sur 6" : "Step %d of 6", wiz_step_ + 1);
    ImGui::ProgressBar((wiz_step_ + 1) / 6.0f, ImVec2(ImGui::GetContentRegionAvail().x - 120, 0), prog);
    ImGui::SameLine();
    ImGui::BeginDisabled(wiz_step_ >= 5 || wiz_step_ + 1 > reach);
    if (ImGui::Button(fr ? "Suivant" : "Next", ImVec2(110, 0))) {
      ++wiz_step_;
      if (wiz_step_ == 1 && slot_ < 0) {  // (the proposed base, chosen for a start)
        WizardRooms(disc);
        const int b = ProposedBase(mods);
        if (b >= 0)
          WizardChooseBase(fr, disc, b);
      }
    }
    ImGui::EndDisabled();
    WizardProblems(fr, mods);
  }

  void WizardProblems(bool fr, const mods::Manager &mods) {
    if (wiz_src_ < 0) {
      ImGui::TextColored(kRed, "%s", fr ? "Choisissez d'où vient l'arène." : "Choose where the arena comes from.");
      return;
    }
    if (WizardReach() == 0) {
      ImGui::TextColored(kRed, "%s", fr ? "Choisissez une arène dans la liste." : "Choose an arena in the list.");
      return;
    }
    if (slot_ < 0) {
      ImGui::TextColored(kRed, "%s", fr ? "Choisissez l'arène de base (étape 2)." : "Choose the base arena (step 2).");
      return;
    }
    if (!port_ok_) {
      ImGui::TextColored(kRed, "%s", port_note_.empty() ? (fr ? "Le portage n'est pas fait." : "Not ported yet.")
                                                        : port_note_.c_str());
      return;
    }
    // (another added arena on the same base is fine; only going straight from one to the other keeps the first one's
    // VS screen: the game loads it again only when the stage number changes)
    const std::string other = SameBase(mods, kSlots[slot_].number);
    if (!ramp_note_.empty())
      ImGui::TextColored(kYellow, "%s", fr ? "La rampe n'est pas à la même hauteur que celle de la base (étape 2)."
                                           : "The ramp doesn't stand at the base's height (step 2).");
    else
      ImGui::TextColored(kGreen, "%s", fr ? "Tout est prêt." : "All set.");
    if (!other.empty())
      ImGui::TextDisabled(fr ? "(%s est aussi utilisée par « %s » : pour voir le bon écran VS, passez par une autre "
                               "arène entre les deux.)"
                             : "(%s is also used by \"%s\": to see the right VS screen, choose another arena in "
                               "between.)",
                          kSlots[slot_].name, other.c_str());
  }

  // A big choice button (the one chosen in gold).
  bool ChoiceCard(int id, bool on, bool enabled, const std::string &title, const std::string &what) {
    ImGui::PushID(id);
    ImGui::PushStyleColor(ImGuiCol_Button, on ? ImVec4(0.94f, 0.75f, 0.24f, 1.0f) : ImVec4(0.17f, 0.17f, 0.19f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, on ? ImVec4(0.08f, 0.08f, 0.08f, 1.0f) : ImVec4(0.92f, 0.92f, 0.92f, 1.0f));
    ImGui::BeginDisabled(!enabled);
    const bool hit = ImGui::Button((title + "\n" + what).c_str(), ImVec2(card_w_, 58));
    ImGui::EndDisabled();
    ImGui::PopStyleColor(2);
    ImGui::PopID();
    return hit;
  }

  // Step 1: the game (My game discs) and its arena, or one arena file.
  void WizardStart(bool fr) {
    ImGui::TextWrapped("%s", fr ? "Votre arène vient d'un autre jeu WWE : choisissez le jeu (ajouté dans MES DISQUES), "
                                  "puis l'arène. Elle jouera à la place d'une arène du jeu (étape suivante) et aura sa "
                                  "propre case dans le choix d'arène."
                                : "Your arena comes from another WWE game: choose the game (added in MY GAME DISCS), then "
                                  "the arena. It plays in place of one of the game's arenas (next step) and has its own "
                                  "tile on the arena select screen.");
    ImGui::Dummy(ImVec2(0, 6));
    const fs::path svr = discs_ ? discs_->PathOf(0x5451085D) : fs::path(),
                   w13 = discs_ ? discs_->PathOf(0x545108B4) : fs::path();
    const std::string not_added = fr ? "pas dans MES DISQUES" : "not in MY GAME DISCS",
                      yours = fr ? "depuis vos disques" : "from your discs";
    card_w_ = std::min(250.0f, (ImGui::GetContentRegionAvail().x - 2 * ImGui::GetStyle().ItemSpacing.x) / 3);
    if (ChoiceCard(0, wiz_src_ == 0, !svr.empty(), "SvR 2011", svr.empty() ? not_added : yours)) {
      wiz_src_ = 0;
      svr_path_.clear();
      if (svr_game_ != svr || svr_list_.empty())
        OpenSvr(fr, svr);
      svr_file_.clear();
      svr_pick_ = -1;
      port_ok_ = false;
      wiz_apron_ = {};
    }
    ImGui::SameLine();
    if (ChoiceCard(1, wiz_src_ == 1 && y_title_ == kWwe13, !w13.empty(), "WWE '13", w13.empty() ? not_added : yours)) {
      wiz_src_ = 1;
      if (y_title_ != kWwe13 || w13_game_ != w13 || w13_list_.empty()) {
        y_title_ = kWwe13;
        Open13(w13);
      }
      w13_pick_ = -1;
      port_ok_ = false;
      wiz_apron_ = {};
    }
    ImGui::SameLine();
    if (ChoiceCard(2, wiz_src_ == 2, true, fr ? "Un fichier d'arène" : "One arena file", "SvR 2011 (bgNN.pac)")) {
      const fs::path p = PickOpen(fr, true);
      if (!p.empty()) {
        std::ifstream f(p, std::ios::binary);
        svr_file_.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        svr_path_ = p;
        svr_pick_ = -1;
        wiz_src_ = 2;
        WizardSourceChosen(fr);
      }
    }
    // WWE 2K14 to 2K17: their arenas are WWE '13's kind (WWE '12's models; 2K16 xcompressed, 2K17 an HSPC put back
    // in an EPAC): the same recipe (port13.h).
    struct TwoK {
      uint32_t title;
      const char *name;
    };
    static constexpr TwoK kTwoK[] = {
        {0x545408B2, "WWE 2K14"}, {0x545408B6, "WWE 2K15"}, {0x315A07D2, "WWE 2K16"}, {0x545408BA, "WWE 2K17"}};
    for (int i = 0; i < 4; ++i) {
      const fs::path p = discs_ ? discs_->PathOf(kTwoK[i].title) : fs::path();
      if (i % 3)
        ImGui::SameLine();
      if (ChoiceCard(3 + i, wiz_src_ == 1 && y_title_ == kTwoK[i].title, !p.empty(), kTwoK[i].name,
                     p.empty() ? not_added : yours)) {
        wiz_src_ = 1;
        if (y_title_ != kTwoK[i].title || w13_game_ != p || w13_list_.empty()) {
          y_title_ = kTwoK[i].title;
          Open13(p);
        }
        w13_pick_ = -1;
        port_ok_ = false;
        wiz_apron_ = {};
      }
    }
    // The games that can't bring arenas yet.
    if (discs_) {
      std::string soon;
      for (const auto &k : game_discs::kKnown)
        if (k.arenas == game_discs::Can::kSoon && !discs_->PathOf(k.title).empty())
          soon += std::string(soon.empty() ? "" : ", ") + k.name;
      if (!soon.empty())
        ImGui::TextDisabled("%s %s", fr ? "Bientôt :" : "Coming:", soon.c_str());
    }
    ImGui::Dummy(ImVec2(0, 6));
    // The arenas of the game chosen.
    const std::vector<int> *list = wiz_src_ == 0 ? &svr_list_ : wiz_src_ == 1 ? &w13_list_ : nullptr;
    if (list && !list->empty()) {
      int clicked = -1;
      ImGui::BeginChild("wiz_list", ImVec2(440, std::max(160.0f, ImGui::GetContentRegionAvail().y - 4)),
                        ImGuiChildFlags_Borders);
      for (int n : *list) {
        std::string label;
        if (wiz_src_ == 0) {
          char b[96];
          snprintf(b, sizeof(b), "%s  (bg%02d)", port_svr_arena::FindSvr(n)->name, n);
          label = b;
        } else {
          label = Label13(n);
        }
        if (ImGui::Selectable(label.c_str(), n == (wiz_src_ == 0 ? svr_pick_ : w13_pick_)))
          clicked = n;
      }
      ImGui::EndChild();
      // The arena chosen, recognised by its apron (the show's name on it).
      if (wiz_apron_.srv) {
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextDisabled("%s", fr ? "Son tablier :" : "Its apron:");
        const float w = std::min(ImGui::GetContentRegionAvail().x, 420.0f);
        ImGui::Image(ImTextureRef(ImTextureID(reinterpret_cast<intptr_t>(wiz_apron_.srv.Get()))),
                     ImVec2(w, w * wiz_apron_.h / std::max(1, wiz_apron_.w)));
        ImGui::EndGroup();
      }
      if (clicked >= 0 && wiz_src_ == 0) {
        char rel[32];
        snprintf(rel, sizeof(rel), "pac/bg/bg%02d.pac", clicked);
        if (svr_disc_.Read(rel, svr_file_)) {
          svr_pick_ = clicked;
          WizardSourceChosen(fr);
        }
      } else if (clicked >= 0) {
        w13_pick_ = clicked;
        WizardSourceChosen(fr);
      }
    } else if (wiz_src_ == 2 && !svr_path_.empty()) {
      if (wiz_apron_.srv) {
        const float w = std::min(ImGui::GetContentRegionAvail().x, 420.0f);
        ImGui::Image(ImTextureRef(ImTextureID(reinterpret_cast<intptr_t>(wiz_apron_.srv.Get()))),
                     ImVec2(w, w * wiz_apron_.h / std::max(1, wiz_apron_.w)));
      }
      ImGui::Text("%s", media::Utf8(svr_path_.filename().wstring()).c_str());
    }
  }

  // Step 2: the base arena (its number, room and lights): free ones, the ramp, the room.
  void WizardBase(bool fr, const disc::GameDisc &disc, const mods::Manager &mods) {
    WizardRooms(disc);
    ImGui::TextWrapped("%s", fr ? "Votre arène joue à la place d'une arène du jeu : son numéro, sa place en mémoire, sa "
                                  "rampe pour les entrées. Choisissez une base avec une rampe comme la sienne, libre de "
                                  "préférence (plusieurs arènes peuvent partager une base)."
                                : "Your arena plays in place of one of the game's: its number, its room in memory, its "
                                  "ramp for the entrances. Choose a base with a ramp like its own, a free one if you "
                                  "can (several arenas can share a base).");
    ImGui::Text("%s %s, %s %s", fr ? "Votre arène :" : "Your arena:", Human(wiz_src_size_).c_str(),
                fr ? "rampe" : "ramp",
                wiz_src_ramp_ < 60 ? (fr ? "au sol" : "at floor level") : (fr ? "surélevée" : "raised"));
    const int proposed = ProposedBase(mods);
    int clicked = -1;
    if (ImGui::BeginTable("bases", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollY,
                          ImVec2(0, std::max(180.0f, ImGui::GetContentRegionAvail().y - 60)))) {
      ImGui::TableSetupColumn(fr ? "Arène de base" : "Base arena", ImGuiTableColumnFlags_WidthFixed, 280);
      ImGui::TableSetupColumn(fr ? "Place" : "Room", ImGuiTableColumnFlags_WidthFixed, 80);
      ImGui::TableSetupColumn(fr ? "Rampe" : "Ramp", ImGuiTableColumnFlags_WidthFixed, 90);
      ImGui::TableSetupColumn("");
      ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableHeadersRow();
      for (int i = 0; i < int(std::size(kSlots)); ++i) {
        const int n = kSlots[i].number;
        const std::string other = SameBase(mods, n);
        const bool ramp_ok = FlatBase(n) == (wiz_src_ramp_ < 60);
        const size_t room = i < int(wiz_rooms_.size()) ? wiz_rooms_[size_t(i)] : 0;
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        if (ImGui::Selectable(SlotLabel(kSlots[i]).c_str(), slot_ == i, ImGuiSelectableFlags_SpanAllColumns))
          clicked = i;
        ImGui::TableNextColumn();
        ImGui::TextColored(room >= wiz_src_size_ ? kGreen : kYellow, "%s", Human(room).c_str());
        ImGui::TableNextColumn();
        ImGui::TextColored(ramp_ok ? kGreen : kYellow, "%s",
                           FlatBase(n) ? (fr ? "au sol" : "floor") : (fr ? "surélevée" : "raised"));
        ImGui::TableNextColumn();
        if (!other.empty())
          ImGui::TextDisabled("%s%s %s", i == proposed ? (fr ? "proposée, " : "proposed, ") : "",
                              fr ? "aussi utilisée par" : "also used by", other.c_str());
        else if (i == proposed)
          ImGui::TextColored(kGreen, "%s", fr ? "proposée" : "proposed");
      }
      ImGui::EndTable();
    }
    if (clicked >= 0)
      WizardChooseBase(fr, disc, clicked);
    ImGui::PushTextWrapPos(0);
    if (!port_note_.empty())
      ImGui::TextColored(port_ok_ ? kGreen : kRed, "%s", port_note_.c_str());
    if (port_ok_ && !ramp_note_.empty())
      ImGui::TextColored(kYellow, "%s", ramp_note_.c_str());
    ImGui::PopTextWrapPos();
  }

  // Step 3: the name on the tile, the author, the banner.
  void WizardName(bool fr) {
    ImGui::SetNextItemWidth(320);
    ImGui::InputText(fr ? "Nom" : "Name", name_, sizeof(name_));
    ImGui::SetNextItemWidth(320);
    ImGui::InputTextWithHint(fr ? "Auteur" : "Author", fr ? "(facultatif)" : "(optional)", author_, sizeof(author_));
    mod_version::Input("Version##arena_version3", version_, sizeof(version_));
    ImGui::Dummy(ImVec2(0, 6));
    DrawBanner(fr);
  }

  // Step 4: what the port brought besides the arena; each one kept, or the base's.
  void WizardWith(bool fr) {
    ImGui::TextWrapped("%s", fr ? "Ce qui vient avec l'arène. Décochez pour garder celui de l'arène de base."
                                : "What comes with the arena. Untick to keep the base arena's.");
    ImGui::Dummy(ImVec2(0, 4));
    const auto row = [&](bool have, bool &keep, const char *what, const char *none) {
      ImGui::BeginDisabled(!have);
      bool k = have && keep;
      if (ImGui::Checkbox(what, &k))
        keep = k;
      ImGui::EndDisabled();
      if (!have) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", none);
      }
    };
    const char *none = fr ? "(pas trouvé : celui de la base)" : "(not found: the base's)";
    const bool w13 = wiz_src_ == 1;
    row(!card_mb_.empty(), wiz_keep_vs_,
        w13 ? (fr ? "Son écran VS" : "Its VS screen")
            : (fr ? "Son écran VS (un écran de WWE '12 avec ses images)" : "Its VS screen (a WWE '12 one with its pictures)"),
        none);
    row(!etex_.empty(), wiz_keep_etex_, fr ? "Ses plaques d'entrée et son locator" : "Its entrance plates and locator",
        w13 ? none : (fr ? "(SvR 2011 : pas encore, ceux de la base)" : "(SvR 2011: not yet, the base's)"));
    row(!load_.empty(), wiz_keep_load_, fr ? "Son écran d'avant-match" : "Its pre-match screen",
        w13 ? none : (fr ? "(SvR 2011 : celui de la base)" : "(SvR 2011: the base's)"));
    if (!w13 && !real_mb_.empty()) {  // (SvR 2011's own VS screen, a test)
      ImGui::Indent();
      if (ImGui::Checkbox(fr ? "Son vrai écran VS de SvR 2011 (sinon : un écran de WWE '12 avec ses images)"
                             : "Its real SvR 2011 VS screen (else: a WWE '12 one with its pictures)",
                          &svr_real_vs_))
        UseRealVs();
      ImGui::Unindent();
    }
    row(!movie_.empty(), wiz_keep_movie_, fr ? "Sa vidéo de transition" : "Its transition video", none);
    if (!w13)
      ImGui::TextDisabled("%s", fr ? "Sa foule : la sienne (celle de la base si elle ne se convertit pas)."
                                   : "Its crowd: its own (the base's if it can't be converted).");
  }

  // Step 5: the port's report and the arena in 3D.
  void WizardLook(bool fr) {
    ImGui::PushTextWrapPos(0);
    if (!port_note_.empty())
      ImGui::TextColored(kGreen, "%s", port_note_.c_str());
    if (!ramp_note_.empty())
      ImGui::TextColored(kYellow, "%s", ramp_note_.c_str());
    ImGui::PopTextWrapPos();
    if (!view_port_) {
      view_port_ = true;
      view_stale_ = true;
    }
    Draw3D(fr);
  }

  // Step 6: the mod made (the .svrmod saved where you choose, to share) and installed.
  void WizardFinish(bool fr, mods::Manager &mods) {
    ImGui::PushTextWrapPos(0);
    ImGui::TextWrapped(fr ? "« %s » jouera à la place de %s, sur sa propre case du choix d'arène (pages suivantes, LB / "
                            "RB). Le mod (.svrmod) est enregistré où vous voulez, pour le partager, puis installé."
                          : "\"%s\" will play in place of %s, on its own tile of the arena select screen (next pages, LB "
                            "/ RB). The mod (.svrmod) is saved where you want, to share it, then installed.",
                       name_, kSlots[slot_].name);
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::BeginDisabled(!port_ok_ || game::IsRunning());
    if (ImGui::Button(fr ? "Créer le mod et l'installer..." : "Make the mod and install it...", ImVec2(260, 36))) {
      as_new_ = true;
      // (what was unticked at step 4 stays out of the mod)
      const Bytes vs_b = card_mb_, vs_f = card_mf_, et = etex_, ld = load_, mv = movie_;
      if (!wiz_keep_vs_) {
        card_mb_.clear();
        card_mf_.clear();
      }
      if (!wiz_keep_etex_)
        etex_.clear();
      if (!wiz_keep_load_)
        load_.clear();
      if (!wiz_keep_movie_)
        movie_.clear();
      Make(fr, mods);
      card_mb_ = vs_b;
      card_mf_ = vs_f;
      etex_ = et;
      load_ = ld;
      movie_ = mv;
    }
    ImGui::EndDisabled();
    if (game::IsRunning())
      ImGui::TextColored(kRed, "%s", fr ? "Le jeu est lancé : fermez-le pour installer le mod."
                                        : "The game is running: close it to install the mod.");
    // Once installed: the game started from here (wwe12.exe next to the Mod Maker), to see it at once.
    if (message_ok_) {
      ImGui::SameLine();
      ImGui::BeginDisabled(game::IsRunning());
      if (ImGui::Button(fr ? "Tester en jeu" : "Test in game", ImVec2(160, 36)))
        LaunchGame();
      ImGui::EndDisabled();
    }
    if (!message_.empty())
      ImGui::TextColored(message_ok_ ? kGreen : kRed, "%s", message_.c_str());
    if (message_ok_)
      ImGui::TextDisabled("%s", fr ? "Dans le jeu : choix d'arène, puis LB / RB pour les pages suivantes."
                                   : "In the game: arena select, then LB / RB for the next pages.");
    if (!reduced_note_.empty())
      ImGui::TextColored(kYellow, "%s", reduced_note_.c_str());
    ImGui::PopTextWrapPos();
  }

  // WWE '13's (or a 2K game's) ISO or folder: its arenas (the 2K games': bg00-65; from bg66 on, as in WWE '12, the
  // match structures).
  void Open13(const fs::path &p) {
    w13_list_.clear();
    w13_pick_ = -1;
    w13_game_ = p;
    names2k_.clear();
    if (!w13_disc_.OpenPath(p))
      return;
    if (y_title_ == 0x545408B2)  // (2K14: named from its aprons)
      for (const auto &a : strings2k::kArenas2K14)
        names2k_[a.file] = a.name;
    else if (y_title_ != kWwe13)
      names2k_ = strings2k::ArenaNames(w13_disc_);
    for (const auto &n : w13_disc_.List("pac/bg"))
      if (n.size() == 8 && _strnicmp(n.c_str(), "bg", 2) == 0 && isdigit(n[2]) && isdigit(n[3]) &&
          (y_title_ != kWwe13 ? atoi(n.c_str() + 2) < 66
                              : port13::Find13(atoi(n.c_str() + 2)) != nullptr))  // (WWE '13: arenas only)
        w13_list_.push_back(atoi(n.c_str() + 2));
    std::sort(w13_list_.begin(), w13_list_.end());
  }
  Bytes Read13(const std::string &rel) const {
    Bytes b;
    w13_disc_.Read(rel, b);
    return b;
  }

  // SvR 2011's ISO (or folder): its arenas listed by name.
  void OpenSvr(bool fr, const fs::path &p) {
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
    if (out.size() > Room())
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
    // Its VS screen: a WWE '12 one wearing its pictures (port_svr_arena::Card).
    Bytes matchhd12;
    if (!matchhd.empty() && disc_ && disc_->Read("pac/menu/MatchHD.pac", matchhd12) &&
        port_svr_arena::Card(matchhd, matchhd12, svr_pick_, img, card_mb_, card_mf_))
      port_note_ += fr ? " Avec son écran VS." : " With its VS screen.";
    // ... or its own SvR 2011 VS screen (port_svr_arena::SvrCard, after Kaiko's notes; Breaking Point seen in the game)
    dressed_mb_ = card_mb_;
    dressed_mf_ = card_mf_;
    real_mb_.clear();
    real_mf_.clear();
    if (!matchhd12.empty() && svr_pick_ >= 0)
      port_svr_arena::SvrCard(matchhd, matchhd12, svr_pick_, real_mb_, real_mf_);
    UseRealVs();
    // Its entrance plates and locator (evtgdhd.pac ETEX/00NN, keys of its 2D animations made WWE '12's).
    Bytes evtgd;
    if (svr_pick_ >= 0 && svr_disc_.Read("pac/evt/evtgdhd.pac", evtgd) && port_svr_arena::SvrEtex(evtgd, svr_pick_, etex_))
      port_note_ += fr ? " Avec ses plaques d'entrée." : " With its entrance plates.";
    // Its transition video (evtmov.pac MOVT/M0NN: Bink 1 at 640 x 640, as WWE '12's).
    Bytes evtmov;
    if (svr_pick_ >= 0 && svr_disc_.Read("pac/evt/evtmov.pac", evtmov) &&
        port13::TransitionVideo(evtmov, svr_pick_, movie_))
      port_note_ += fr ? " Avec sa transition." : " With its transition.";
    const std::string stem = svr_pick_ >= 0 ? port_svr_arena::FindSvr(svr_pick_)->name
                                            : media::Utf8(svr_path_.stem().wstring());
    snprintf(name_, sizeof(name_), "%s (SvR 2011)", stem.c_str());
    as_new_ = true;
  }

  // The name of an installed added arena (another one than this) playing in place of stage `base`, else "".
  std::string SameBase(const mods::Manager &mods, int base) const {
    for (const auto &m : mods.List()) {
      if (m.type != "arena" || !m.on)
        continue;
      std::ifstream f(m.dir / "manifest.txt");
      std::string line, slot, b, name;
      while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r')
          line.pop_back();
        if (line.rfind("slot=", 0) == 0)
          slot = line.substr(5);
        else if (line.rfind("base=", 0) == 0)
          b = line.substr(5);
        else if (line.rfind("name=", 0) == 0)
          name = line.substr(5);
      }
      if (slot == "new" && atoi(b.c_str()) == base && name != name_)
        return name;
    }
    return {};
  }

  // The game of a WWE '13-kind source (WWE '13, 2K14 to 2K17) and its arenas' names (WWE '13's known; the 2K
  // games' read from their discs, strings2k.h).
  const char *YukesGame() const {
    return y_title_ == 0x545408B2   ? "WWE 2K14"
           : y_title_ == 0x545408B6 ? "WWE 2K15"
           : y_title_ == 0x315A07D2 ? "WWE 2K16"
           : y_title_ == 0x545408BA ? "WWE 2K17"
                                    : "WWE '13";
  }
  std::string Label13(int n) const {
    char b[96];
    const auto it = names2k_.find(n);
    const char *name = y_title_ == kWwe13 ? port13::ArenaName(n) : it != names2k_.end() ? it->second.c_str() : "";
    snprintf(b, sizeof(b), "%s  (bg%02d)", *name ? name : (std::string(YukesGame()) + (" arena")).c_str(), n);
    return b;
  }

  // The recipe (port13.h) on the chosen arena, its base the slot chosen above.
  void Port13(bool fr) {
    port_ok_ = false;
    char rel[32];
    snprintf(rel, sizeof(rel), "pac/bg/bg%02d.pac", w13_pick_);
    const Bytes w13 = Read13(rel);
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
    if (out.size() > Room())
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
    // Its own banner from WWE '13's menus (the 2K games': their Universe's pictures), else the base's.
    const Bytes matchhd = Read13("pac/menu/MatchHD.pac");
    svrfmt::Image img;
    // (a 2K game's own: its layout has an element less than WWE '12's (no names, a black box), so a WWE '12 VS screen
    // wears its pictures, as SvR 2011's)
    Bytes matchhd12;
    std::vector<svrfmt::BundleTexture> pictures2k;
    if (y_title_ != kWwe13 && !matchhd.empty() && disc_ && disc_->Read("pac/menu/MatchHD.pac", matchhd12)) {
      svrfmt::Archive m;
      char nb[8];
      snprintf(nb, sizeof(nb), "MB%02d", w13_pick_);
      if (svrfmt::ArchiveOpen(matchhd, m, nullptr))
        for (auto &k : m.root.kids)
          if (k.group == "MLYE" && k.name == nb)
            if (auto *p = port_svr_arena::detail::Pictures(k))
              svrfmt::BundleRead(svrfmt::ArchiveData(*p), pictures2k);
    }
    if (y_title_ != kWwe13 ? port_svr_arena::CardWearing(pictures2k, matchhd12, svrfmt::Image(), card_mb_, card_mf_, nullptr, true)
                           : !matchhd.empty() && port13::Card(matchhd, w13_pick_, card_mb_, card_mf_))
      port_note_ += fr ? " Avec son écran VS." : " With its VS screen.";
    // Its entrance graphics (location plate, name plates).
    const Bytes evtgd = Read13("pac/evt/evtgdhd.pac");
    // (a 2K game's own ETEX found or not: 2K17's are HSPC hashes and 2K15's names longer, and their plates were left
    // out, the base's shown, 2026-10-31: their plates come from their movies anyway)
    const bool own_etex = !evtgd.empty() && port13::Etex(evtgd, w13_pick_, etex_);
    if (own_etex || y_title_ != kWwe13) {
      // (a 2K game's has no plates of its own: WWE '12's WrestleMania XXVII ones wearing its logo and copyright plate)
      svrfmt::Archive e;
      bool plates = false;
      svrfmt::Image plate, logo;
      if (svrfmt::ArchiveOpen(etex_, e, nullptr))
        for (auto &k : e.root.kids) {
          plates = plates || k.id == 0;
          if (k.id == 20 && svrfmt::ArchiveExpand(k) && !k.kids.empty())
            svrfmt::DdsDecode(svrfmt::ArchiveData(k.kids[0]), plate);
        }
      Bytes evtgd12;
      if (!plates && y_title_ != kWwe13) {
        etex_.clear();
        if (const auto *l = port_svr_arena::detail::Named(pictures2k, "logo"))
          svrfmt::DdsDecode(l->data, logo);
        if (logo.rgba.empty())
          logo = banner_;
        // ... and its own plates, a picture of each of its plate movies (2K15-2K17: Bink 2, when a decoder is
        // installed; 2K14: Bink 1 with alpha, read by ours)
        port_svr_arena::Plate2K own[2];
        for (int n = 0; n < 2; ++n) {
          Bytes bk2;
          if (!port13::PlateMovie2K([&](const char *p) { return Read13(p); }, w13_pick_, n == 1, bk2))
            continue;
          wchar_t tmp[MAX_PATH];
          GetTempPathW(MAX_PATH, tmp);
          const fs::path in = fs::path(tmp) / L"wwe12_plate.bk2";
          std::ofstream(in, std::ios::binary).write(reinterpret_cast<const char *>(bk2.data()), std::streamsize(bk2.size()));
          svrfmt::Image still;
          if (bink2::Still(bink2::Found(), in, still, nullptr))
            own[n] = port_svr_arena::PlateOnScreen(still, n == 1);
          std::error_code ec;
          fs::remove(in, ec);
        }
        if (disc_ && disc_->Read("pac/evt/evtgdhd.pac", evtgd12))
          port_svr_arena::EtexWearing(evtgd12, logo, 3.4, plate, etex_, own[0], own[1]);
        if (!etex_.empty() && (!own[0].img.rgba.empty() || !own[1].img.rgba.empty()))
          port_note_ += fr ? " Avec ses plaques d'entrée (images de ses vidéos)."
                           : " With its entrance plates (pictures of its movies).";
      }
      if (!etex_.empty() && port_note_.find(fr ? "plaques d'entrée" : "entrance plates") == std::string::npos)
        port_note_ += fr ? " Avec ses plaques d'entrée." : " With its entrance plates.";
    }
    // Its pre-match screen, and its transition video.
    const Bytes loadhd = Read13("pac/menu/LoadHD.pac");
    if (!loadhd.empty())
      port13::PreMatch(loadhd, w13_pick_, load_);
    const Bytes evtmov = Read13("pac/evt/evtmov.pac");
    if (!evtmov.empty() && port13::TransitionVideo(evtmov, w13_pick_, movie_)) {
      port_note_ += fr ? " Avec sa transition." : " With its transition.";
    } else if (y_title_ != kWwe13) {
      // WWE 2K15-2K17: Bink 2, made into WWE '12's Bink 1 (320 x 320 as its M021: our encoder packs little, 640 x 640
      // came out 13 MB against WWE '12's 0.4-0.9 MB; a fade in and out for its alpha)
      Bytes bk2;
      if (port13::Transition2K([&](const char *p) { return Read13(p); }, w13_pick_, bk2)) {
        wchar_t tmp[MAX_PATH];
        GetTempPathW(MAX_PATH, tmp);
        const fs::path in = fs::path(tmp) / L"wwe12_transition.bk2", out = fs::path(tmp) / L"wwe12_transition.bik";
        std::ofstream(in, std::ios::binary).write(reinterpret_cast<const char *>(bk2.data()), std::streamsize(bk2.size()));
        std::wstring err;
        if (bink2::ToBink1(bink2::Found(), in, out, nullptr, nullptr, &err, 320, 320, 8)) {
          std::ifstream f(out, std::ios::binary);
          movie_.assign(std::istreambuf_iterator<char>(f), {});
          port_note_ += fr ? " Avec sa transition." : " With its transition.";
        } else {
          port_note_ += fr ? " Sa transition (Bink 2) n'a pas pu être convertie : " + media::Utf8(err) + "."
                           : " Its transition (Bink 2) couldn't be converted: " + media::Utf8(err) + ".";
        }
        std::error_code ec;
        fs::remove(in, ec);
        fs::remove(out, ec);
      }
    }
    if (y_title_ == kWwe13 ? !matchhd.empty() && port13::Banner(matchhd, w13_pick_, img)
                           : port13::Banner2K([&](const char *p) { return Read13(p); }, y_title_, w13_pick_, img)) {
      SetBanner(Fill(img, 256, 128));
      banner_custom_ = true;
    } else {
      banner_custom_ = false;
      LoadBaseBanner();
    }
    if (y_title_ == kWwe13 && *port13::ArenaName(w13_pick_))
      snprintf(name_, sizeof(name_), "%s (WWE '13)", port13::ArenaName(w13_pick_));
    else
      snprintf(name_, sizeof(name_), "%s bg%02d", YukesGame(), w13_pick_);
    if (const auto it = names2k_.find(w13_pick_); it != names2k_.end())
      snprintf(name_, sizeof(name_), "%s (%s)", it->second.c_str(), YukesGame());
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
                fr ? "place disponible :" : "room:", Human(Room()).c_str());
    if (other_size_ > Room())
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
    arena_edit::Arena &a = mode_ == 1 || (mode_ >= 3 && mode_ != 5) ? other_ : arena_;
    a.SetStage(kSlots[slot_].number);
    Bytes out;
    const auto reduced = a.Fit(Room(), out);
    if (out.size() > Room()) {
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
    std::string manifest = "game=wwe12\ntype=arena\nid=" + id + "\nname=" + std::string(name_) + "\nversion=" + mod_version::Of(version_) + "\n";
    if (author_[0])
      manifest += std::string("author=") + author_ + "\n";
    if (as_new_)
      manifest += "slot=new\nbase=" + std::to_string(kSlots[slot_].number) + "\ndescription=" +
                  (mode_ == 3   ? std::string(name_) + ", ported from " + std::string(YukesGame()) + " (the player's own copy), playing as "
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
      if ((mode_ == 3 || mode_ == 4) && !card_mb_.empty()) {  // its own VS screen (MatchHD.pac MLYE/MBnn, MENU/MnnF)
        entries.push_back({"arena/card_mb.pac", card_mb_});
        entries.push_back({"arena/card_mf.pac", card_mf_});
      }
      if ((mode_ == 3 || mode_ == 4) && !etex_.empty())  // its entrance graphics (evtgdhd.pac ETEX/00NN)
        entries.push_back({"arena/etex_hd.pac", etex_});
      if (mode_ == 3 && !load_.empty())  // its pre-match screen (LoadHD.pac LOAD/90NN)
        entries.push_back({"arena/load_hd.pac", load_});
      if ((mode_ == 3 || mode_ == 4) && !movie_.empty())  // its transition video (evtmov.pac MOVT/M0NN)
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
  // An added arena (slot=new) is served under its own size: its room is the arena heap's (heap 12, ~18.7 MB, wants
  // the file + 5 MB free), not its base's file (WWE 2K16's WrestleMania 31, 12.2 MB packed as BPE, had pictures
  // halved for bg10's 10.4 MB). Replacing an arena keeps the shipped size.
  static constexpr size_t kAddedRoom = size_t(12.5 * 1024 * 1024);
  size_t Room() const { return as_new_ ? std::max(budget_, kAddedRoom) : budget_; }
  Bytes shipped_;
  arena_edit::Arena arena_;
  std::vector<Thumb> thumbs_;
  char filter_[64] = {}, name_[64] = {}, author_[64] = {}, version_[16] = {};
  float ring_colour_[3][3] = {{0.8f, 0.1f, 0.1f}, {0.1f, 0.1f, 0.1f}, {0.6f, 0.6f, 0.6f}};  // ropes, pads, posts
  std::string ring_note_;
  std::string error_, message_, reduced_note_, pick_note_;
  bool message_ok_ = false;

  arena_view::View view_;
  bool view_stale_ = true;
  std::string view_error_;

  // Porting from WWE '13.
  disc::GameDisc w13_disc_;  // WWE '13's ISO or folder (My game discs)
  fs::path w13_game_;
  std::map<int, std::string> names2k_;  // a 2K game's arenas' names by number (strings2k.h), else empty
  static constexpr uint32_t kWwe13 = 0x545108B4;
  uint32_t y_title_ = kWwe13;  // the WWE '13-kind game read through w13_disc_ (WWE '13, 2K14, 2K15, 2K16)
  std::vector<int> w13_list_;
  int w13_pick_ = -1;
  bool port_ok_ = false, view_port_ = false;
  std::string port_note_, ramp_note_;
  Bytes card_mb_, card_mf_;  // its VS screen (port13::Card, port_svr_arena::Card)
  // An SvR 2011 arena's two VS screens: a WWE '12 one wearing its pictures, its own (port_svr_arena::SvrCard); one in
  // card_mb_ / card_mf_ (svr_real_vs_).
  Bytes dressed_mb_, dressed_mf_, real_mb_, real_mf_;
  bool svr_real_vs_ = true;
  void UseRealVs() {
    const bool real = svr_real_vs_ && !real_mb_.empty();
    card_mb_ = real ? real_mb_ : dressed_mb_;
    card_mf_ = real ? real_mf_ : dressed_mf_;
  }
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

  // The assistant.
  int tab_ = 0, wiz_step_ = 0;
  std::vector<std::pair<uint32_t, std::string>> ed_models_;  // the 3D editor's list (the arena opened)
  int ed_pick_ = -1;
  arena_edit::Move ed_move_;
  float ed_scale_ = 1.0f;     // a 3D file added: its size
  arena_edit::Move ed_import_;  // ... its offset and turn
  std::string ed_note_;
  bool ed_note_ok_ = false;
  float ed_focus_[4] = {};      // the object the 3D view is kept on (x, y, z, distance)
  bool ed_focus_on_ = false;
  char ed_filter_[64] = {};
  float card_w_ = 250;
  Thumb wiz_apron_;  // the source arena's apron (step 1)  // (the start step's three choices share the width)
  int wiz_src_ = -1;  // 0 SvR 2011 (My game discs), 1 WWE '13 (My game discs), 2 one SvR 2011 arena file
  size_t wiz_src_size_ = 0;
  float wiz_src_ramp_ = 0;  // cm
  std::vector<size_t> wiz_rooms_;
  bool wiz_keep_vs_ = true, wiz_keep_etex_ = true, wiz_keep_load_ = true, wiz_keep_movie_ = true;

 public:
  const game_discs::Discs *discs_ = nullptr;  // My game discs (set by the Mod Maker)

 private:

  fs::path other_path_;
  arena_edit::Arena other_;
  bool other_ok_ = false;
  size_t other_size_ = 0;
  std::string other_note_;
};

}  // namespace arena_page
