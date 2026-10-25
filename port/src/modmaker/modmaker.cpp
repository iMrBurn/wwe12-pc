// WWE '12 Mod Maker ("WWE12 Mod Maker.exe", next to the game): makes mods (.svrmod, game=wwe12) and looks
// inside the game's files. The launcher installs and manages mods; the Mod Maker makes them (and installs what it
// makes, through the same mods::Manager). Based on Kaiko's SvR 2011 Mod Maker (port/modmaker of
// KaikoClanworth1/wwe-svr2011-pc, with his permission): its idea of pages on a rail, its USER MOVIES maker and its
// pac readers (svrfmt/). Plan and state: docs/mod_maker.md.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <shellapi.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <filesystem>
#include <string>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include "arena_page.h"
#include "game_discs.h"
#include "game_disc.h"
#include "media.h"
#include "mods.h"
#include "pac_page.h"
#include "new_star_page.h"
#include "superstar_page.h"
#include "toml_file.h"
#include "ui_style.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

namespace {

using namespace ui;

ComPtr<ID3D11Device> g_device;
ComPtr<ID3D11DeviceContext> g_context;
ComPtr<IDXGISwapChain> g_swapchain;
ComPtr<ID3D11RenderTargetView> g_rtv;
UINT g_resize_w = 0, g_resize_h = 0;

void CreateRenderTarget() {
  ComPtr<ID3D11Texture2D> back;
  g_swapchain->GetBuffer(0, IID_PPV_ARGS(&back));
  g_device->CreateRenderTargetView(back.Get(), nullptr, &g_rtv);
}

bool CreateDevice(HWND hwnd) {
  DXGI_SWAP_CHAIN_DESC sd{};
  sd.BufferCount = 2;
  sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.OutputWindow = hwnd;
  sd.SampleDesc.Count = 1;
  sd.Windowed = TRUE;
  sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
  const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
  HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2,
                                             D3D11_SDK_VERSION, &sd, &g_swapchain, &g_device, nullptr, &g_context);
  if (FAILED(hr))
    hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd,
                                       &g_swapchain, &g_device, nullptr, &g_context);
  if (FAILED(hr))
    return false;
  CreateRenderTarget();
  return true;
}

// The logo (resource 101, the launcher's PNG) through WIC.
ComPtr<ID3D11ShaderResourceView> LoadLogo(int &w, int &h) {
  ComPtr<ID3D11ShaderResourceView> srv;
  HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(101), MAKEINTRESOURCEW(10) /* RT_RCDATA */);
  if (!res)
    return srv;
  const void *bytes = LockResource(LoadResource(nullptr, res));
  const DWORD size = SizeofResource(nullptr, res);
  ComPtr<IWICImagingFactory> wic;
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wic))))
    return srv;
  ComPtr<IWICStream> stream;
  wic->CreateStream(&stream);
  stream->InitializeFromMemory(static_cast<BYTE *>(const_cast<void *>(bytes)), size);
  ComPtr<IWICBitmapDecoder> decoder;
  if (FAILED(wic->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder)))
    return srv;
  ComPtr<IWICBitmapFrameDecode> frame;
  decoder->GetFrame(0, &frame);
  ComPtr<IWICFormatConverter> conv;
  wic->CreateFormatConverter(&conv);
  conv->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0,
                   WICBitmapPaletteTypeCustom);
  UINT uw = 0, uh = 0;
  conv->GetSize(&uw, &uh);
  std::vector<BYTE> pixels(size_t(uw) * uh * 4);
  conv->CopyPixels(nullptr, uw * 4, UINT(pixels.size()), pixels.data());
  D3D11_TEXTURE2D_DESC td{};
  td.Width = uw;
  td.Height = uh;
  td.MipLevels = 1;
  td.ArraySize = 1;
  td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  td.SampleDesc.Count = 1;
  td.Usage = D3D11_USAGE_DEFAULT;
  td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  D3D11_SUBRESOURCE_DATA init{pixels.data(), uw * 4, 0};
  ComPtr<ID3D11Texture2D> tex;
  if (SUCCEEDED(g_device->CreateTexture2D(&td, &init, &tex)))
    g_device->CreateShaderResourceView(tex.Get(), nullptr, &srv);
  w = int(uw);
  h = int(uh);
  return srv;
}

fs::path ExeDir() {
  wchar_t buf[MAX_PATH];
  GetModuleFileNameW(nullptr, buf, MAX_PATH);
  return fs::path(buf).parent_path();
}

void OpenFolder(const fs::path &p) {
  std::error_code ec;
  fs::create_directories(p, ec);
  ShellExecuteW(nullptr, L"open", p.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

// ---------------------------------------------------------------------------------------------
// State

enum Page { kHome, kVideos, kTextures, kArenas, kStars, kNewStar, kFiles, kDiscs, kAbout };
Page g_page = kHome;
bool g_fr = false;  // the game's language (wwe12.toml user_language = 4)
TomlFile g_toml;
disc::GameDisc g_disc;
mods::Manager g_mods;
media::EntranceVideos g_videos;
media::TextureMods g_texture_mods;
pac_page::Browser g_files;
arena_page::Page g_arenas;
superstar_page::Page g_stars;
new_star_page::Page g_new_star;
game_discs::Discs g_discs;
ComPtr<ID3D11ShaderResourceView> g_logo;
int g_logo_w = 0, g_logo_h = 0;

// ---------------------------------------------------------------------------------------------
// Pages

void PageHome() {
  Title("MOD MAKER");
  ImGui::TextWrapped("%s", g_fr ? "Créez vos mods pour WWE '12 PC. Chaque page fabrique un mod (.svrmod) que vous pouvez "
                                  "installer tout de suite et partager. Le launcher (page MODS) les active, les "
                                  "désactive et les supprime."
                                : "Make your own mods for WWE '12 PC. Each page makes a mod (.svrmod) that you can "
                                  "install at once and share. The launcher (MODS page) turns them on and off and "
                                  "removes them.");
  ImGui::Dummy(ImVec2(0, 6));
  if (g_disc.Found())
    ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.45f, 1.0f), "%s", g_fr ? "Fichiers du jeu trouvés." : "Game files found.");
  else
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s",
                       g_fr ? "Fichiers du jeu introuvables : mettez le Mod Maker à côté de wwe12.exe."
                            : "Game files not found: put the Mod Maker next to wwe12.exe.");
  ImGui::Dummy(ImVec2(0, 10));
  struct Item {
    Page page;
    const char *en, *fr, *what_en, *what_fr;
  };
  const Item items[] = {
      {kVideos, "Entrance videos", "Vidéos d'entrée", "A superstar's titantron video from any video or picture.",
       "La vidéo de titantron d'un catcheur, à partir de n'importe quelle vidéo ou image."},
      {kTextures, "Textures", "Textures", "Change any picture of the game: menus, renders, logos, arenas...",
       "Changer n'importe quelle image du jeu : menus, renders, logos, arènes..."},
      {kArenas, "Arenas", "Arènes", "A new arena from your other games (SvR 2011, WWE '13, 2K14-2K17), step by step; or the game's own retextured.",
       "Une nouvelle arène depuis vos autres jeux (SvR 2011, WWE '13, 2K14-2K17), étape par étape ; ou celles du jeu retouchées."},
      {kStars, "Superstars", "Catcheurs", "Change an attire's pictures and see the superstar in 3D.",
       "Changer les images d'une tenue et voir le catcheur en 3D."},
      {kNewStar, "New superstar", "Nouveau catcheur", "Add a superstar to the game, next to the others (also from SvR 2011, WWE '13 or 2K14-2K17).",
       "Ajouter un catcheur au jeu, à côté des autres (aussi depuis SvR 2011, WWE '13 ou 2K14-2K17)."},
      {kFiles, "Game files", "Fichiers du jeu", "Look inside the game's archives (.pac) and export what's in them.",
       "Regarder dans les archives du jeu (.pac) et exporter ce qu'elles contiennent."},
      {kDiscs, "My game discs", "Mes disques de jeu", "Your other WWE games (ISO or folder), added once: the ports read them.",
       "Vos autres jeux WWE (ISO ou dossier), ajoutés une fois : les portages les lisent."},
      {kAbout, "Help", "Aide", "How to make each kind of mod, step by step; what isn't possible yet.",
       "Comment faire chaque sorte de mod, pas à pas ; ce qui n'est pas encore possible."},
  };
  for (const auto &it : items) {
    if (ImGui::Button(g_fr ? it.fr : it.en, ImVec2(220, 0)))
      g_page = it.page;
    ImGui::SameLine();
    ImGui::TextDisabled("%s", g_fr ? it.what_fr : it.what_en);
  }
  ImGui::Dummy(ImVec2(0, 10));
  ImGui::TextDisabled("%s", g_fr ? "Bientôt : prises et entrées des catcheurs portés, "
                                   "renders du menu, sons."
                                 : "Coming: ported superstars' moves and entrances, menu "
                                   "renders, sounds.");
  if (ImGui::Button(g_fr ? "Ouvrir le dossier mods" : "Open the mods folder"))
    OpenFolder(ExeDir() / "mods");
}

void PageVideos() {
  Title(g_fr ? "VIDÉOS D'ENTRÉE" : "ENTRANCE VIDEOS");
  g_videos.Draw(g_fr, g_device.Get(), ExeDir(), g_mods);
}

void PageTextures() {
  Title("TEXTURES");
  bool dump = g_toml.GetBool("wwe12_texture_dump", false);
  if (ImGui::Checkbox(g_fr ? "Dump des textures (pour faire des packs)" : "Dump textures (to make texture packs)",
                      &dump)) {
    g_toml.SetBool("wwe12_texture_dump", dump);
    g_toml.Save();
  }
  if (ImGui::Button(g_fr ? "Ouvrir texture_dumps" : "Open texture_dumps"))
    OpenFolder(ExeDir() / "texture_dumps");
  ImGui::SameLine();
  if (ImGui::Button(g_fr ? "Ouvrir textures" : "Open textures"))
    OpenFolder(ExeDir() / "textures");
  ImGui::Dummy(ImVec2(0, 6));
  g_texture_mods.Draw(g_fr);
}

void PageArenas() {
  Title(g_fr ? "ARÈNES" : "ARENAS");
  g_arenas.Draw(g_fr, g_device.Get(), g_disc, g_mods);
}

void PageStars() {
  Title(g_fr ? "CATCHEURS" : "SUPERSTARS");
  g_stars.Draw(g_fr, g_device.Get(), g_disc, g_mods);
}

void PageNewStar() {
  Title(g_fr ? "NOUVEAU CATCHEUR" : "NEW SUPERSTAR");
  g_new_star.Draw(g_fr, g_device.Get(), g_disc, g_mods);
}

void PageFiles() {
  Title(g_fr ? "FICHIERS DU JEU" : "GAME FILES");
  g_files.Draw(g_fr, g_device.Get(), g_disc);
}

// The help: short step-by-step guides (like the manual in Kaiko's Mod Maker), then the credits.
struct Guide {
  const char *title_en, *title_fr;
  const char *steps_en[7], *steps_fr[7];
};
const Guide kGuides[] = {
    {"Before anything: add your other games",
     "Avant tout : ajouter vos autres jeux",
     {"Go to MY GAME DISCS.",
      "Add each WWE game you own (SvR 2011, WWE '13, 2K14 to 2K17): its disc image (.iso) or its folder.",
      "The Mod Maker reads them where they are: nothing is copied, nothing is shared.",
      "Added once, they are offered in every assistant."},
     {"Allez dans MES DISQUES.",
      "Ajoutez chaque jeu WWE que vous possédez (SvR 2011, WWE '13, 2K14 à 2K17) : son image disque (.iso) ou son "
      "dossier.",
      "Le Mod Maker les lit là où ils sont : rien n'est copié, rien n'est partagé.",
      "Ajoutés une fois, ils sont proposés dans tous les assistants."}},
    {"A new arena (from another game)",
     "Une nouvelle arène (d'un autre jeu)",
     {"ARENAS, tab New arena.",
      "1. Start from: the game, then the arena (its apron is shown).",
      "2. Base arena: one of WWE '12's, used for what the other game lacks. One proposed; one base per arena is best.",
      "3. Name and banner: the name and the picture on the arena select screen.",
      "4. What comes with it: VS screen, plates, crowd, transition video (untick one if the game has trouble with it).",
      "5. Look at it in 3D, then 6. Finish: the mod is made and installed; Test in game.",
      "In the game: arena select, then LB / RB for the next pages (also in Universe)."},
     {"ARÈNES, onglet Nouvelle arène.",
      "1. Départ : le jeu, puis l'arène (son tablier est montré).",
      "2. Arène de base : une arène de WWE '12, qui donne ce qui manque à l'autre jeu. Une est proposée ; mieux vaut "
      "une base par arène.",
      "3. Nom et bannière : le nom et l'image sur l'écran de choix des arènes.",
      "4. Ce qui vient avec : écran VS, plaques, public, vidéo de transition (décochez-en un si le jeu a du mal).",
      "5. Regardez-la en 3D, puis 6. Terminer : le mod est créé et installé ; Tester en jeu.",
      "Dans le jeu : choix de l'arène, puis LB / RB pour les pages suivantes (aussi dans l'Univers)."}},
    {"A new superstar",
     "Un nouveau catcheur",
     {"NEW SUPERSTAR.",
      "1. Look: a superstar of your other games (with its portraits when the game has them), a file, or the base.",
      "2. Base: gives the moves, ratings and entrance. A superstar WWE '12 also has is its own base at once.",
      "3. Name, 4. Entrance video: the base's, its own from SvR 2011, or one of yours (any video or picture).",
      "5. Look at it in 3D, then 6. Finish: the mod is made and installed; Test in game.",
      "In the game: superstar select screen, DLC tile. Added superstars are everywhere (Universe, Create a Story)."},
     {"NOUVEAU.",
      "1. Apparence : un catcheur de vos autres jeux (avec ses portraits quand le jeu en a), un fichier, ou la base.",
      "2. Base : donne les prises, les stats et l'entrée. Un catcheur qui existe aussi dans WWE '12 est sa propre "
      "base d'office.",
      "3. Nom, 4. Vidéo d'entrée : celle de la base, la sienne de SvR 2011, ou une à vous (vidéo ou image).",
      "5. Regardez-le en 3D, puis 6. Terminer : le mod est créé et installé ; Tester en jeu.",
      "Dans le jeu : écran de choix des catcheurs, case DLC. Les catcheurs ajoutés sont partout (Univers, Créer un "
      "scénario)."}},
    {"An entrance video, pictures, textures",
     "Une vidéo d'entrée, des images, des textures",
     {"VIDEOS: a superstar's titantron (big screen) and minitron (stage screen) from any video or picture.",
      "TEXTURES: change any picture of the game (menus, renders, logos) from the texture dumps.",
      "SUPERSTARS / ARENAS (Retexture): change an attire's or an arena's pictures and see them in 3D."},
     {"VIDÉOS : le titantron (grand écran) et le minitron (écran de la scène) d'un catcheur, depuis n'importe quelle "
      "vidéo ou image.",
      "TEXTURES : changer n'importe quelle image du jeu (menus, renders, logos) depuis les dumps de textures.",
      "CATCHEURS / ARÈNES (Retoucher) : changer les images d'une tenue ou d'une arène et les voir en 3D."}},
    {"Your mods: turn on, off, share",
     "Vos mods : activer, désactiver, partager",
     {"Every mod is a .svrmod file, saved where you chose: share that file.",
      "The launcher (MODS page) turns mods on and off and removes them; a .svrmod received is installed there.",
      "Close the game before making or installing a mod (its files are in use)."},
     {"Chaque mod est un fichier .svrmod, enregistré où vous l'avez choisi : c'est ce fichier qu'on partage.",
      "Le launcher (page MODS) active, désactive et supprime les mods ; un .svrmod reçu s'y installe.",
      "Fermez le jeu avant de créer ou d'installer un mod (ses fichiers sont utilisés)."}},
    {"Not yet",
     "Pas encore",
     {"Moves and entrances of other games' superstars (they take their base's).",
      "WWE '13 and 2K entrance videos (Bink 2), WWE 2K17 portraits, WWE 2K14 arena names.",
      "SvR 2011 VS screens exactly like the original."},
     {"Les prises et les entrées des catcheurs des autres jeux (ils prennent celles de leur base).",
      "Les vidéos d'entrée de WWE '13 et des 2K (Bink 2), les portraits de WWE 2K17, les noms des arènes de WWE 2K14.",
      "Les écrans VS de SvR 2011 exactement comme l'original."}},
};

void PageAbout() {
  Title(g_fr ? "AIDE" : "HELP");
  for (const auto &g : kGuides) {
    if (!ImGui::CollapsingHeader(g_fr ? g.title_fr : g.title_en, &g == kGuides ? ImGuiTreeNodeFlags_DefaultOpen : 0))
      continue;
    ImGui::Indent(12);
    for (int i = 0; i < 7; ++i)
      if (const char *t = g_fr ? g.steps_fr[i] : g.steps_en[i]) {
        ImGui::Bullet();
        ImGui::TextWrapped("%s", t);
      }
    ImGui::Unindent(12);
    ImGui::Dummy(ImVec2(0, 4));
  }
  ImGui::Dummy(ImVec2(0, 10));
  ImGui::SeparatorText(g_fr ? "À propos" : "About");
  ImGui::TextWrapped("%s", g_fr ? "Mod Maker de WWE '12 PC, portage natif non officiel fait par des fans.\n\n"
                                  "Basé sur le Mod Maker du portage SmackDown vs. Raw 2011 de Kaiko, avec sa permission : "
                                  "format .svrmod, fabricant de vidéos d'entrée et encodeur Bink, lecteurs d'archives "
                                  ".pac (svrfmt), packs de textures. Lecteur Bink porté de FFmpeg (LGPL 2.1 ou plus "
                                  "récent). Logiciel libre sous licence GPL-3.0. Aucune donnée du jeu n'est fournie : "
                                  "le Mod Maker lit vos propres fichiers.\n\nSans lien avec WWE, THQ ou Yuke's."
                                : "WWE '12 PC Mod Maker, for the unofficial native port made by fans.\n\n"
                                  "Based on the Mod Maker of Kaiko's SmackDown vs. Raw 2011 port, with his permission: "
                                  "the .svrmod format, the entrance movie maker and Bink encoder, the .pac readers "
                                  "(svrfmt), texture packs. Bink reader ported from FFmpeg (LGPL 2.1 or later). Free "
                                  "software under the GPL-3.0 licence. No game data is included: the Mod Maker reads "
                                  "your own files.\n\nNot affiliated with WWE, THQ or Yuke's.");
  ImGui::Dummy(ImVec2(0, 8));
  if (ImGui::Button(g_fr ? "Page du projet sur GitHub" : "Project page on GitHub"))
    ShellExecuteW(nullptr, L"open", L"https://github.com/iMrBurn/wwe12-pc", nullptr, nullptr, SW_SHOWNORMAL);
  ImGui::SameLine();
  if (ImGui::Button(g_fr ? "Mod Maker de Kaiko (SvR 2011)" : "Kaiko's Mod Maker (SvR 2011)"))
    ShellExecuteW(nullptr, L"open", L"https://github.com/KaikoClanworth1/wwe-svr2011-pc", nullptr, nullptr,
                  SW_SHOWNORMAL);
  ImGui::Dummy(ImVec2(0, 8));
  ImGui::TextDisabled("Version %s", WWE12_VERSION);
}

void DrawUi() {
  const ImGuiViewport *vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->Pos);
  ImGui::SetNextWindowSize(vp->Size);
  ImGui::Begin("modmaker", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoSavedSettings);
  ImDrawList *dl = ImGui::GetWindowDrawList();
  const ImVec2 o = vp->Pos;
  const float W = vp->Size.x, H = vp->Size.y;
  constexpr float kSide = 260;
  dl->AddRectFilled(o, ImVec2(o.x + kSide, o.y + H), IM_COL32(8, 8, 9, 255));
  Stroke(dl, ImVec2(o.x - 30, o.y + H - 34), kSide + 40, 12, kRed, 30);
  Stroke(dl, ImVec2(o.x - 30, o.y + H - 16), kSide - 40, 5, kGold, 30);
  dl->AddRectFilledMultiColor(ImVec2(o.x + kSide, o.y), ImVec2(o.x + W, o.y + H), IM_COL32(26, 22, 14, 255),
                              IM_COL32(14, 14, 16, 255), IM_COL32(14, 14, 16, 255), IM_COL32(14, 14, 16, 255));

  ImGui::SetCursorPos(ImVec2(20, 18));
  if (g_logo) {
    const float lw = kSide - 60, lh = lw * float(g_logo_h) / float(g_logo_w);
    ImGui::Image(ImTextureRef(ImTextureID(reinterpret_cast<intptr_t>(g_logo.Get()))), ImVec2(lw, lh));
  }
  ImGui::SetCursorPosX(20);
  ImGui::PushFont(g_font_title, 30.0f);
  ImGui::TextColored(ImVec4(0.94f, 0.75f, 0.24f, 1.0f), "MOD MAKER");
  ImGui::PopFont();
  ImGui::Dummy(ImVec2(0, 8));
  const char *names_en[] = {"HOME",       "VIDEOS",     "TEXTURES",     "ARENAS", "SUPERSTARS",
                            "NEW SUPERSTAR", "GAME FILES", "MY GAME DISCS", "HELP"};
  const char *names_fr[] = {"ACCUEIL", "VIDÉOS",   "TEXTURES",   "ARÈNES",  "CATCHEURS",
                            "NOUVEAU", "FICHIERS", "MES DISQUES", "AIDE"};
  for (int i = 0; i < int(std::size(names_en)); ++i) {
    ImGui::SetCursorPosX(16);
    const bool sel = g_page == i;
    if (BigButton(g_fr ? names_fr[i] : names_en[i], ImVec2(kSide - 32, 44), sel ? kGold : IM_COL32(28, 28, 32, 255),
                  sel ? IM_COL32(20, 20, 20, 255) : IM_COL32(235, 235, 235, 255)))
      g_page = Page(i);
  }

  ImGui::SetCursorPos(ImVec2(kSide + 36, 26));
  ImGui::BeginChild("page", ImVec2(W - kSide - 60, H - 50));
  switch (g_page) {
  case kHome: PageHome(); break;
  case kVideos: PageVideos(); break;
  case kTextures: PageTextures(); break;
  case kArenas: PageArenas(); break;
  case kStars: PageStars(); break;
  case kNewStar: PageNewStar(); break;
  case kFiles: PageFiles(); break;
  case kDiscs:
    Title(g_fr ? "MES DISQUES DE JEU" : "MY GAME DISCS");
    g_discs.Draw(g_fr);
    break;
  case kAbout: PageAbout(); break;
  }
  ImGui::EndChild();
  ImGui::End();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
    return TRUE;
  switch (msg) {
  case WM_SIZE:
    if (wp != SIZE_MINIMIZED) {
      g_resize_w = LOWORD(lp);
      g_resize_h = HIWORD(lp);
    }
    return 0;
  case WM_GETMINMAXINFO: {
    auto *mm = reinterpret_cast<MINMAXINFO *>(lp);
    mm->ptMinTrackSize = {1000, 640};
    return 0;
  }
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR, int) {
  CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
  ImGui_ImplWin32_EnableDpiAwareness();
  g_toml.Load(ExeDir() / "wwe12.toml");
  g_fr = g_toml.GetInt("user_language", 1) == 4;
  g_disc.Open(ExeDir());
  g_mods.Init(ExeDir() / "mods");
  g_discs.Load(ExeDir() / "modmaker_discs.txt");
  g_arenas.discs_ = &g_discs;
  g_new_star.discs_ = &g_discs;

  WNDCLASSEXW wc{sizeof(wc)};
  wc.style = CS_CLASSDC;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = inst;
  wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
  wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512) /* IDC_ARROW */);
  wc.lpszClassName = L"WWE12ModMaker";
  RegisterClassExW(&wc);
  const float dpi = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY));
  const int w = int(1240 * dpi), h = int(820 * dpi);
  HWND hwnd = CreateWindowW(wc.lpszClassName, L"WWE '12 Mod Maker", WS_OVERLAPPEDWINDOW,
                            (GetSystemMetrics(SM_CXSCREEN) - w) / 2, (GetSystemMetrics(SM_CYSCREEN) - h) / 2, w, h,
                            nullptr, nullptr, inst, nullptr);
  if (!CreateDevice(hwnd)) {
    MessageBoxW(hwnd, L"Direct3D 11 is not available.", L"WWE '12 Mod Maker", MB_ICONERROR);
    return 1;
  }
  ShowWindow(hwnd, SW_SHOWDEFAULT);
  UpdateWindow(hwnd);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;
  SetupStyle();
  ImGui::GetStyle().ScaleAllSizes(dpi);
  ImGui::GetStyle().FontScaleDpi = dpi;
  LoadFonts();
  ImGui_ImplWin32_Init(hwnd);
  ImGui_ImplDX11_Init(g_device.Get(), g_context.Get());
  g_logo = LoadLogo(g_logo_w, g_logo_h);

  bool done = false;
  while (!done) {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
      if (msg.message == WM_QUIT)
        done = true;
    }
    if (done)
      break;
    if (g_resize_w && g_resize_h) {
      g_rtv.Reset();
      g_swapchain->ResizeBuffers(0, g_resize_w, g_resize_h, DXGI_FORMAT_UNKNOWN, 0);
      g_resize_w = g_resize_h = 0;
      CreateRenderTarget();
    }
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::PushFont(g_font_body, 0.0f);
    DrawUi();
    ImGui::PopFont();
    ImGui::Render();
    const float clear[4] = {0.05f, 0.05f, 0.06f, 1.0f};
    g_context->OMSetRenderTargets(1, g_rtv.GetAddressOf(), nullptr);
    g_context->ClearRenderTargetView(g_rtv.Get(), clear);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    g_swapchain->Present(1, 0);
  }

  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
  DestroyWindow(hwnd);
  CoUninitialize();
  return 0;
}
