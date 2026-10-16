// WWE '12 PC launcher: settings (written to wwe12.toml next to the exe), saves, mods, and Play.
// Win32 + Direct3D 11 + Dear ImGui, so it runs anywhere the game does (Steam Deck via Proton too).

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <wincodec.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include "changelog.h"
#include "diagnostics.h"
#include "game_process.h"
#include "keybinds.h"
#include "saves.h"
#include "toml_file.h"
#include "updater.h"
#include "vulkan_check.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

namespace {

// ---------------------------------------------------------------------------------------------
// Device
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
  HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels,
                                             2, D3D11_SDK_VERSION, &sd, &g_swapchain, &g_device,
                                             nullptr, &g_context);
  if (FAILED(hr))  // no usable GPU driver: software rendering still shows the launcher
    hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2,
                                       D3D11_SDK_VERSION, &sd, &g_swapchain, &g_device, nullptr,
                                       &g_context);
  if (FAILED(hr))
    return false;
  CreateRenderTarget();
  return true;
}

// Embedded PNG resource (the logo) -> shader resource view, through WIC.
ComPtr<ID3D11ShaderResourceView> LoadPngResource(int id, int &w, int &h) {
  ComPtr<ID3D11ShaderResourceView> srv;
  HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10) /* RT_RCDATA */);
  if (!res)
    return srv;
  HGLOBAL data = LoadResource(nullptr, res);
  const void *bytes = LockResource(data);
  const DWORD size = SizeofResource(nullptr, res);
  ComPtr<IWICImagingFactory> wic;
  if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&wic))))
    return srv;
  ComPtr<IWICStream> stream;
  wic->CreateStream(&stream);
  stream->InitializeFromMemory(static_cast<BYTE *>(const_cast<void *>(bytes)), size);
  ComPtr<IWICBitmapDecoder> decoder;
  if (FAILED(wic->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad,
                                          &decoder)))
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

// ---------------------------------------------------------------------------------------------
// Paths and helpers
fs::path ExeDir() {
  wchar_t buf[MAX_PATH];
  GetModuleFileNameW(nullptr, buf, MAX_PATH);
  return fs::path(buf).parent_path();
}

fs::path DocumentsDir() {
  PWSTR p = nullptr;
  fs::path out;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &p)))
    out = p;
  CoTaskMemFree(p);
  return out;
}

void OpenFolder(const fs::path &p) {
  std::error_code ec;
  fs::create_directories(p, ec);
  ShellExecuteW(nullptr, L"open", p.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void OpenUrl(const wchar_t *url) {
  ShellExecuteW(nullptr, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
}

std::string Narrow(const std::wstring &w) {
  if (w.empty())
    return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), int(w.size()), nullptr, 0, nullptr,
                                    nullptr);
  std::string s(size_t(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), int(w.size()), s.data(), n, nullptr, nullptr);
  return s;
}

std::vector<std::string> GpuNames() {
  std::vector<std::string> names;
  ComPtr<IDXGIFactory1> factory;
  if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
    return names;
  ComPtr<IDXGIAdapter1> adapter;
  for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
    DXGI_ADAPTER_DESC1 d{};
    adapter->GetDesc1(&d);
    if (!(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
      const std::string n = Narrow(d.Description);
      if (std::find(names.begin(), names.end(), n) == names.end())
        names.push_back(n);
    }
    adapter.Reset();
  }
  return names;
}

// The game finds its files as assets or game default.xex (or the project assets in a build
// folder), an .iso in game\, or an .iso next to the exe.
bool GameFilesFound(const fs::path &dir) {
  std::error_code ec;
  for (const auto &d : {dir / "assets", dir / "game", dir / ".." / ".." / ".." / "assets"})
    if (fs::exists(d / "default.xex", ec))
      return true;
  for (const auto &d : {dir / "game", dir}) {
    for (const auto &e : fs::directory_iterator(d, ec)) {
      auto ext = e.path().extension().wstring();
      std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
      if (ext == L".iso")
        return true;
    }
  }
  return false;
}

// ---------------------------------------------------------------------------------------------
// Texts (French when the game language is French, English otherwise)
struct Texts {
  const char *play, *settings, *controls, *mods, *saves, *about;
  const char *play_button, *version, *game_found, *game_missing;
  const char *language, *resolution, *res_auto, *display, *fullscreen, *window, *vsync, *fps60,
      *fps60_hint, *sharpness, *off, *gpu, *gpu_auto, *exit_entry, *exit_hint, *saved_note;
  const char *keyboard, *keyboard_hint, *reset_keys, *pad_note;
  const char *mods_text, *open_mods, *mods_count;
  const char *about_text, *github;
  const char *updates, *checking, *up_to_date, *update_available, *install_update, *check_failed,
      *check_now, *check_at_start, *downloading, *update_failed;
  const char *whats_new;
  const char *vulkan_ok, *vulkan_missing;
  const char *support, *support_hint, *diag_copy, *diag_copied, *diag_failed, *open_logs;
  const char *include_beta, *update_game_running, *save_failed, *already_running, *wait_busy;
  const char *keys_howto, *keys_set, *keys_add, *keys_press, *keys_bad, *keys_dup, *keys_restart;
  const char *kb_player, *kb_player_hint, *kb_p1, *kb_p2, *kb_p3, *kb_p4;
};

const Texts kEnglish = {
    "PLAY", "SETTINGS", "CONTROLS", "MODS", "SAVES", "ABOUT",
    "PLAY", "Version", "Game files found.",
    "Game files missing: put your WWE '12 (Xbox 360) .iso in the \"game\" folder.",
    "Language", "Resolution", "Automatic", "Display", "Full screen", "Window", "V-Sync",
    "60 fps entrances and cutscenes (experimental)",
    "Title belts on a shoulder can move oddly at 60 fps.",
    "Texture sharpness", "Off", "Graphics card", "Automatic", "EXIT entry in the main menu",
    "Replaces SHOP (the closed Xbox LIVE store).", "Changes are saved right away.",
    "Keyboard (player 1)",
    "Several keys per action, separated by commas. ZQSD and WASD both work.",
    "Default keys", "Xbox and PlayStation controllers work out of the box.",
    "Files put in the mods folder replace the game's own, at the same path as on the disc.\n"
    "Example: mods\\movies\\titantron\\100.bik replaces that titantron video.",
    "Open the mods folder", "file(s) in the mods folder",
    "WWE '12 PC is an unofficial native port of WWE '12 (Xbox 360), made by fans.\n"
    "Native renderer adapted from nefariousjosiah's SmackDown vs. Raw ports, on the ReXGlue SDK\n"
    "and re:Blue. DLC support by Banshee. Free software under the GPL-3.0 licence. No game data is included.\n\n"
    "Not affiliated with WWE, THQ or Yuke's.",
    "Project page on GitHub",
    "Updates", "Checking for updates...", "Up to date.", "New version available:",
    "Download and install", "Could not check for updates.", "Check now", "Check at startup",
    "Downloading...", "Update failed:",
    "What's new in this version",
    "Graphics card: %s",
    "No Vulkan graphics driver found: the game cannot draw its picture (black window). "
    "Update your graphics drivers. Very old graphics chips (such as the Intel HD 5000 series) "
    "have no Vulkan driver and cannot run the game yet.",
    "Support",
    "Having a problem? Copy the diagnostic info and paste it in your bug report: it holds your "
    "graphics card, driver and settings, nothing personal. The game's logs are in the logs folder.",
    "Copy diagnostic info", "Copied. Paste it in your bug report.", "Could not copy.",
    "Open the logs folder",
    "Include beta versions", "The game is running: close it, then check again.",
    "Could not save your settings. Is the game folder read-only (for example inside Program Files)? "
    "Move the game folder to your Documents or Desktop.",
    "The game is already running.",
    "Please wait: a backup, an import or an update is in progress.",
    "Click Set, then press the key to use: it replaces the current ones. + adds a second key. "
    "You can also type key names separated by commas (for example Z,W).",
    "Set", "+", "Press a key...",
    "Key names the game does not know (they would do nothing): %s. Use Set and press the key instead.",
    "The same key is used for several actions: %s",
    "Changes apply the next time you start the game.",
    "Keyboard plays as",
    "To play with a friend: keep your controller as player 1 and set the keyboard to player 2, so they "
    "are two separate controllers. With player 2 and no controller plugged in, nobody is player 1.",
    "Player 1 (shares player 1 with controller 1)", "Player 2 (separate controller)", "Player 3", "Player 4"};

const Texts kFrench = {
    "JOUER", "RÉGLAGES", "CONTRÔLES", "MODS", "SAUVEGARDES", "À PROPOS",
    "JOUER", "Version", "Fichiers du jeu trouvés.",
    "Fichiers du jeu manquants : mettez l'image .iso de votre WWE '12 (Xbox 360) dans le "
    "dossier \"game\".",
    "Langue", "Résolution", "Automatique", "Affichage", "Plein écran", "Fenêtre", "V-Sync",
    "Entrées et cinématiques à 60 fps (expérimental)",
    "Les ceintures portées à l'épaule peuvent bouger bizarrement à 60 fps.",
    "Netteté des textures", "Désactivée", "Carte graphique", "Automatique",
    "Entrée QUITTER dans le menu principal", "Remplace SHOP (la boutique Xbox LIVE, fermée).",
    "Les changements sont enregistrés tout de suite.",
    "Clavier (joueur 1)",
    "Plusieurs touches par action, séparées par des virgules. ZQSD et WASD marchent tous les deux.",
    "Touches par défaut", "Les manettes Xbox et PlayStation marchent d'office.",
    "Les fichiers posés dans le dossier mods remplacent ceux du jeu, au même chemin que sur le "
    "disque.\nExemple : mods\\movies\\titantron\\100.bik remplace cette vidéo de titantron.",
    "Ouvrir le dossier mods", "fichier(s) dans le dossier mods",
    "WWE '12 PC est un portage natif non officiel de WWE '12 (Xbox 360), fait par des fans.\n"
    "Rendu natif adapté des portages SmackDown vs. Raw de nefariousjosiah, sur le SDK ReXGlue\n"
    "et re:Blue. Support des DLC par Banshee. Logiciel libre sous licence GPL-3.0. Aucune donnée du jeu n'est fournie.\n\n"
    "Sans lien avec WWE, THQ ou Yuke's.",
    "Page du projet sur GitHub",
    "Mises à jour", "Recherche de mises à jour...", "À jour.", "Nouvelle version disponible :",
    "Télécharger et installer", "Impossible de vérifier les mises à jour.", "Vérifier",
    "Vérifier au démarrage", "Téléchargement...", "Échec de la mise à jour :",
    "Nouveautés de cette version",
    "Carte graphique : %s",
    "Aucun pilote graphique Vulkan trouvé : le jeu ne peut pas afficher d'image (fenêtre noire). "
    "Mettez à jour les pilotes de votre carte graphique. Les très vieilles puces (comme la série "
    "Intel HD 5000) n'ont pas de pilote Vulkan et ne peuvent pas encore faire tourner le jeu.",
    "Aide",
    "Un problème ? Copiez les infos de diagnostic et collez-les dans votre rapport de bug : elles "
    "contiennent votre carte graphique, son pilote et vos réglages, rien de personnel. Les journaux "
    "du jeu sont dans le dossier des journaux.",
    "Copier les infos de diagnostic", "Copié. Collez-le dans votre rapport de bug.",
    "Copie impossible.", "Ouvrir le dossier des journaux",
    "Inclure les versions bêta", "Le jeu est lancé : fermez-le, puis vérifiez à nouveau.",
    "Impossible d'enregistrer vos réglages. Le dossier du jeu est-il en lecture seule (par exemple "
    "dans Program Files) ? Déplacez le dossier du jeu dans Documents ou sur le Bureau.",
    "Le jeu est déjà lancé.",
    "Patientez : une copie, un import ou une mise à jour est en cours.",
    "Cliquez sur Définir, puis appuyez sur la touche à utiliser : elle remplace les actuelles. + en ajoute "
    "une deuxième. Vous pouvez aussi taper des noms de touches séparés par des virgules (par exemple Z,W).",
    "Définir", "+", "Appuyez sur une touche...",
    "Noms de touches inconnus du jeu (ils ne feraient rien) : %s. Utilisez Définir puis appuyez sur la touche.",
    "La même touche sert à plusieurs actions : %s",
    "Les changements s'appliquent au prochain lancement du jeu.",
    "Le clavier joue comme",
    "Pour jouer avec un ami : gardez votre manette comme joueur 1 et mettez le clavier en joueur 2 : ce "
    "sont alors deux manettes séparées. Avec le joueur 2 et sans manette branchée, personne n'est joueur 1.",
    "Joueur 1 (partage le joueur 1 avec la manette 1)", "Joueur 2 (manette séparée)", "Joueur 3", "Joueur 4"};

// ---------------------------------------------------------------------------------------------
// Settings
struct KeyBind {
  const char *key;
  const char *label;
  const char *def;
};
const KeyBind kBinds[] = {
    {"keybind_lstick_up", "Left stick up", "Z,W"},
    {"keybind_lstick_down", "Left stick down", "S"},
    {"keybind_lstick_left", "Left stick left", "Q,A"},
    {"keybind_lstick_right", "Left stick right", "D"},
    {"keybind_rstick_up", "Right stick up", "Up"},
    {"keybind_rstick_down", "Right stick down", "Down"},
    {"keybind_rstick_left", "Right stick left", "Left"},
    {"keybind_rstick_right", "Right stick right", "Right"},
    {"keybind_x", "X", "J"},
    {"keybind_a", "A", "K"},
    {"keybind_b", "B", "L"},
    {"keybind_y", "Y", "I"},
    {"keybind_left_shoulder", "LB", "U"},
    {"keybind_right_shoulder", "RB", "O"},
    {"keybind_left_trigger", "LT", "R"},
    {"keybind_right_trigger", "RT", "Space"},
    {"keybind_lstick_press", "L3", "C"},
    {"keybind_rstick_press", "R3", "N"},
    {"keybind_dpad_up", "D-pad up", "T"},
    {"keybind_dpad_down", "D-pad down", "G"},
    {"keybind_dpad_left", "D-pad left", "F"},
    {"keybind_dpad_right", "D-pad right", "H"},
    {"keybind_start", "Start", "Return,Escape"},
    {"keybind_back", "Back", "Tab,Backspace"},
};

struct Settings {
  int language = 1;  // 1 en, 3 de, 4 fr, 5 es, 6 it
  int scale = 0;
  bool fullscreen = true;
  bool vsync = true;
  bool fps60 = false;
  int aniso = 16;
  std::string gpu;
  bool exit_entry = true;
  bool keyboard = true;
  int keyboard_player = 1;  // 1..4: the player the keyboard plays as (2 = separate from pad 1)
  char binds[std::size(kBinds)][64] = {};
};

TomlFile g_toml;
bool g_settings_ok = true;  // false when wwe12.toml could not be written
Settings g_s;

void LoadSettings() {
  g_toml.Load(ExeDir() / "wwe12.toml");
  g_s.language = g_toml.GetInt("user_language", 1);
  g_s.scale = g_toml.GetInt("svr_render_scale", 0);
  g_s.fullscreen = g_toml.GetBool("fullscreen", true);
  g_s.vsync = g_toml.GetBool("bd_vsync", true);
  g_s.fps60 = g_toml.GetBool("wwe12_60fps", false);
  g_s.aniso = g_toml.GetInt("bd_anisotropy", 16);
  g_s.gpu = g_toml.GetString("svr_gpu_device", "");
  g_s.exit_entry = g_toml.GetBool("wwe12_quit_entry", true);
  g_s.keyboard = g_toml.GetBool("mnk_mode", true);
  g_s.keyboard_player = std::clamp(g_toml.GetInt("mnk_player", 1), 1, 4);
  for (size_t i = 0; i < std::size(kBinds); ++i)
    snprintf(g_s.binds[i], sizeof(g_s.binds[i]), "%s",
             g_toml.GetString(kBinds[i].key, kBinds[i].def).c_str());
}

void SaveSettings() {
  g_toml.SetInt("user_language", g_s.language);
  g_toml.SetInt("svr_render_scale", g_s.scale);
  g_toml.SetBool("fullscreen", g_s.fullscreen);
  g_toml.SetBool("bd_vsync", g_s.vsync);
  g_toml.SetBool("wwe12_60fps", g_s.fps60);
  g_toml.SetInt("bd_anisotropy", g_s.aniso);
  g_toml.SetString("svr_gpu_device", g_s.gpu);
  g_toml.SetBool("wwe12_quit_entry", g_s.exit_entry);
  g_toml.SetBool("mnk_mode", g_s.keyboard);
  g_toml.SetInt("mnk_player", g_s.keyboard_player);
  for (size_t i = 0; i < std::size(kBinds); ++i)
    g_toml.SetString(kBinds[i].key, g_s.binds[i]);
  g_settings_ok = g_toml.Save();
}

const Texts &T() { return g_s.language == 4 ? kFrench : kEnglish; }

// ---------------------------------------------------------------------------------------------
// Style
constexpr ImU32 kGold = IM_COL32(240, 190, 60, 255);
constexpr ImU32 kRed = IM_COL32(200, 24, 34, 255);
ImFont *g_font_body = nullptr;
ImFont *g_font_title = nullptr;

void SetupStyle() {
  ImGuiStyle &st = ImGui::GetStyle();
  st.WindowRounding = 0;
  st.FrameRounding = 4;
  st.GrabRounding = 4;
  st.FramePadding = ImVec2(10, 7);
  st.ItemSpacing = ImVec2(10, 10);
  st.WindowBorderSize = 0;
  ImVec4 *c = st.Colors;
  c[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.055f, 0.063f, 1.0f);
  c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_Text] = ImVec4(0.95f, 0.94f, 0.92f, 1.0f);
  c[ImGuiCol_TextDisabled] = ImVec4(0.6f, 0.58f, 0.55f, 1.0f);
  c[ImGuiCol_FrameBg] = ImVec4(0.13f, 0.13f, 0.15f, 1.0f);
  c[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.18f, 0.12f, 1.0f);
  c[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.22f, 0.10f, 1.0f);
  c[ImGuiCol_Button] = ImVec4(0.16f, 0.16f, 0.18f, 1.0f);
  c[ImGuiCol_ButtonHovered] = ImVec4(0.78f, 0.09f, 0.13f, 1.0f);
  c[ImGuiCol_ButtonActive] = ImVec4(0.94f, 0.75f, 0.24f, 1.0f);
  c[ImGuiCol_CheckMark] = ImVec4(0.94f, 0.75f, 0.24f, 1.0f);
  c[ImGuiCol_Header] = ImVec4(0.78f, 0.09f, 0.13f, 0.6f);
  c[ImGuiCol_HeaderHovered] = ImVec4(0.78f, 0.09f, 0.13f, 0.8f);
  c[ImGuiCol_HeaderActive] = ImVec4(0.94f, 0.75f, 0.24f, 0.8f);
  c[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.09f, 0.10f, 0.98f);
  c[ImGuiCol_Separator] = ImVec4(0.94f, 0.75f, 0.24f, 0.35f);
  c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
}

void LoadFonts() {
  ImGuiIO &io = ImGui::GetIO();
  wchar_t windir[MAX_PATH];
  GetWindowsDirectoryW(windir, MAX_PATH);
  const fs::path fonts = fs::path(windir) / "Fonts";
  std::error_code ec;
  auto try_font = [&](const char *file, float size) -> ImFont * {
    const fs::path p = fonts / file;
    if (!fs::exists(p, ec))
      return nullptr;
    return io.Fonts->AddFontFromFileTTF(p.string().c_str(), size);
  };
  g_font_body = try_font("segoeui.ttf", 19.0f);
  if (!g_font_body)
    g_font_body = io.Fonts->AddFontDefault();
  g_font_title = try_font("impact.ttf", 40.0f);
  if (!g_font_title)
    g_font_title = try_font("segoeuib.ttf", 34.0f);
  if (!g_font_title)
    g_font_title = g_font_body;
}

// A slanted brush stroke, the game menus' look.
void Stroke(ImDrawList *dl, ImVec2 a, float w, float h, ImU32 col, float slant = 18.0f) {
  dl->AddQuadFilled(ImVec2(a.x + slant, a.y), ImVec2(a.x + w + slant, a.y),
                    ImVec2(a.x + w, a.y + h), ImVec2(a.x, a.y + h), col);
}

bool BigButton(const char *label, ImVec2 size, ImU32 fill, ImU32 text_col) {
  ImGui::PushID(label);
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::InvisibleButton("##b", size);
  const bool hovered = ImGui::IsItemHovered();
  ImDrawList *dl = ImGui::GetWindowDrawList();
  Stroke(dl, p, size.x - 18, size.y, hovered ? kRed : fill);
  ImGui::PushFont(g_font_title, size.y >= 80 ? 46.0f : 30.0f);
  const ImVec2 ts = ImGui::CalcTextSize(label);
  dl->AddText(ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f),
              hovered ? IM_COL32_WHITE : text_col, label);
  ImGui::PopFont();
  ImGui::PopID();
  return clicked;
}

// ---------------------------------------------------------------------------------------------
// Pages
enum Page { kPlay, kSettings, kControls, kMods, kSaves, kAbout };
Page g_page = kPlay;
ComPtr<ID3D11ShaderResourceView> g_logo;
int g_logo_w = 0, g_logo_h = 0;
saves::Manager g_saves;
bool g_launch = false;
int g_play_note = 0;  // 1 game already running, 2 something is busy
Updater g_updater;
VulkanCheck g_vulkan;
std::string g_changelog;  // CHANGELOG.md next to the exe (the notes of this version), prepared

void Title(const char *text) {
  ImGui::PushFont(g_font_title, 44.0f);
  const ImVec2 p = ImGui::GetCursorScreenPos();
  Stroke(ImGui::GetWindowDrawList(), ImVec2(p.x - 10, p.y + ImGui::GetTextLineHeight() - 4), 300,
         10, kRed, 6);
  ImGui::TextColored(ImVec4(0.94f, 0.75f, 0.24f, 1.0f), "%s", text);
  ImGui::PopFont();
  ImGui::Dummy(ImVec2(0, 6));
}

void PagePlay() {
  const Texts &t = T();
  Title("WWE '12 PC");
  ImGui::TextDisabled("%s %s", t.version, WWE12_VERSION);
  ImGui::Dummy(ImVec2(0, 10));
  const bool found = GameFilesFound(ExeDir());
  if (found)
    ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.45f, 1.0f), "%s", t.game_found);
  else
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", t.game_missing);
  switch (g_vulkan.GetState()) {
  case VulkanCheck::State::Ok:
    ImGui::TextDisabled(t.vulkan_ok, g_vulkan.Detail().c_str());
    break;
  case VulkanCheck::State::Missing:
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 560.0f * ImGui::GetStyle().FontScaleDpi);
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", t.vulkan_missing);
    ImGui::PopTextWrapPos();
    break;
  default:
    break;
  }
  ImGui::Dummy(ImVec2(0, 20));
  if (BigButton(t.play_button, ImVec2(340, 90), kGold, IM_COL32(20, 20, 20, 255)))
    g_launch = true;
  if (g_play_note)
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", g_play_note == 1 ? t.already_running : t.wait_busy);
  ImGui::Dummy(ImVec2(0, 30));
  ImGui::Separator();
  ImGui::Text("%s", t.updates);
  g_updater.Draw({t.checking, t.up_to_date, t.update_available, t.install_update, t.check_failed,
                  t.check_now, t.check_at_start, t.downloading, t.update_failed, t.include_beta,
                  t.update_game_running});
  if (!g_changelog.empty()) {
    ImGui::Dummy(ImVec2(0, 10));
    if (ImGui::CollapsingHeader(t.whats_new, ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::BeginChild("##changelog", ImVec2(0, std::max(ImGui::GetTextLineHeight() * 9, ImGui::GetContentRegionAvail().y)),
                        ImGuiChildFlags_Borders);
      changelog::Draw(g_changelog);
      ImGui::EndChild();
    }
  }
}

void PageSettings() {
  const Texts &t = T();
  bool changed = false;
  Title(t.settings);
  ImGui::PushItemWidth(320);

  static const int kLangValues[] = {1, 3, 4, 5, 6};
  static const char *kLangNames[] = {"English", "Deutsch", "Français", "Español", "Italiano"};
  int li = 0;
  for (int i = 0; i < 5; ++i)
    if (kLangValues[i] == g_s.language)
      li = i;
  if (ImGui::Combo(t.language, &li, kLangNames, 5)) {
    g_s.language = kLangValues[li];
    changed = true;
  }

  // svr_render_scale holds a height (720, 1080, 1440, 2160) or, from older files, a multiple.
  static const int kResHeights[] = {0, 540, 720, 1080, 1440, 2160};
  int ri = 0;
  switch (g_s.scale) {
  case 540: ri = 1; break;
  case 1: case 720: ri = 2; break;
  case 1080: ri = 3; break;
  case 2: case 1440: ri = 4; break;
  case 3: case 4: case 2160: case 2880: ri = 5; break;
  default: ri = 0; break;
  }
  const char *res_names[] = {t.res_auto, "540p", "720p", "1080p", "1440p", "4K (2160p)"};
  if (ImGui::Combo(t.resolution, &ri, res_names, 6)) {
    g_s.scale = kResHeights[ri];
    changed = true;
  }

  int disp = g_s.fullscreen ? 0 : 1;
  const char *disp_names[] = {t.fullscreen, t.window};
  if (ImGui::Combo(t.display, &disp, disp_names, 2)) {
    g_s.fullscreen = disp == 0;
    changed = true;
  }

  static const int kAniso[] = {0, 2, 4, 8, 16};
  int ai = 4;
  for (int i = 0; i < 5; ++i)
    if (kAniso[i] == g_s.aniso)
      ai = i;
  const char *aniso_names[] = {t.off, "2x", "4x", "8x", "16x"};
  if (ImGui::Combo(t.sharpness, &ai, aniso_names, 5)) {
    g_s.aniso = kAniso[ai];
    changed = true;
  }

  static std::vector<std::string> gpus = GpuNames();
  const char *preview = g_s.gpu.empty() ? t.gpu_auto : g_s.gpu.c_str();
  if (ImGui::BeginCombo(t.gpu, preview)) {
    if (ImGui::Selectable(t.gpu_auto, g_s.gpu.empty())) {
      g_s.gpu.clear();
      changed = true;
    }
    for (const auto &n : gpus)
      if (ImGui::Selectable(n.c_str(), g_s.gpu == n)) {
        g_s.gpu = n;
        changed = true;
      }
    ImGui::EndCombo();
  }
  ImGui::PopItemWidth();

  ImGui::Dummy(ImVec2(0, 6));
  changed |= ImGui::Checkbox(t.vsync, &g_s.vsync);
  changed |= ImGui::Checkbox(t.fps60, &g_s.fps60);
  ImGui::SameLine();
  ImGui::TextDisabled("(%s)", t.fps60_hint);
  changed |= ImGui::Checkbox(t.exit_entry, &g_s.exit_entry);
  ImGui::SameLine();
  ImGui::TextDisabled("(%s)", t.exit_hint);

  if (changed)
    SaveSettings();
  ImGui::Dummy(ImVec2(0, 12));
  ImGui::TextDisabled("%s", t.saved_note);
  if (!g_settings_ok)
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", t.save_failed);
}

int g_capture_idx = -1;      // binding waiting for a key press (-1: none)
bool g_capture_add = false;  // the key is added to the current ones instead of replacing them

void PageControls() {
  const Texts &t = T();
  bool changed = false;
  Title(t.controls);
  ImGui::TextDisabled("%s", t.pad_note);
  changed |= ImGui::Checkbox(t.keyboard, &g_s.keyboard);
  ImGui::TextDisabled("%s", t.keyboard_hint);
  {
    const char *players[] = {t.kb_p1, t.kb_p2, t.kb_p3, t.kb_p4};
    int pi = g_s.keyboard_player - 1;
    ImGui::SetNextItemWidth(380);
    if (ImGui::Combo(t.kb_player, &pi, players, 4)) {
      g_s.keyboard_player = pi + 1;
      changed = true;
    }
    ImGui::TextDisabled("%s", t.kb_player_hint);
  }
  ImGui::TextWrapped("%s", t.keys_howto);

  // The key the player pressed after "Set" / "+": written with the game's exact name.
  if (g_capture_idx >= 0) {
    std::string key;
    if (keys::Capture(key)) {
      char *buf = g_s.binds[g_capture_idx];
      std::string now = key;
      if (g_capture_add) {
        const std::string current = keys::Normalize(buf).text;
        if (!current.empty())
          now = (("," + current + ",").find("," + key + ",") != std::string::npos) ? current : current + "," + key;
      }
      snprintf(buf, sizeof(g_s.binds[0]), "%s", now.c_str());
      g_capture_idx = -1;
      changed = true;
    }
  }

  std::string bad_list, dup_list;
  if (ImGui::BeginTable("binds", 4, ImGuiTableFlags_SizingStretchSame)) {
    ImGui::TableSetupColumn("a", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn("b", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableSetupColumn("c", ImGuiTableColumnFlags_WidthFixed, 150.0f);
    ImGui::TableSetupColumn("d", ImGuiTableColumnFlags_WidthStretch);
    const ImGuiStyle &st = ImGui::GetStyle();
    const float set_w = ImGui::CalcTextSize(t.keys_set).x + st.FramePadding.x * 2;
    const float side_w = set_w + ImGui::GetFrameHeight() + st.ItemSpacing.x * 2;
    std::unordered_map<std::string, size_t> first_use;  // key -> first action using it
    for (size_t i = 0; i < std::size(kBinds); ++i) {
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(kBinds[i].label);
      ImGui::TableNextColumn();
      ImGui::PushID(int(i));
      const keys::Result norm = keys::Normalize(g_s.binds[i]);
      for (const std::string &b : norm.bad)
        bad_list += (bad_list.empty() ? "" : ", ") + b + " (" + kBinds[i].label + ")";
      for (size_t from = 0; from < norm.text.size();) {  // same key for two actions?
        size_t comma = norm.text.find(',', from);
        if (comma == std::string::npos)
          comma = norm.text.size();
        const std::string key = norm.text.substr(from, comma - from);
        from = comma + 1;
        const auto it = first_use.emplace(key, i);
        if (!it.second && it.first->second != i)
          dup_list += (dup_list.empty() ? "" : ", ") + key + " (" + kBinds[it.first->second].label + " / " +
                      kBinds[i].label + ")";
      }
      const float field_w = ImGui::GetContentRegionAvail().x - side_w;
      if (g_capture_idx == int(i)) {  // waiting for a key: click to cancel
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.78f, 0.09f, 0.13f, 1.0f));
        if (ImGui::Button(t.keys_press, ImVec2(field_w, 0)))
          g_capture_idx = -1;
        ImGui::PopStyleColor();
      } else {
        if (!norm.bad.empty())
          ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.45f, 0.10f, 0.10f, 1.0f));
        ImGui::SetNextItemWidth(field_w);
        changed |= ImGui::InputText("##k", g_s.binds[i], sizeof(g_s.binds[i]));
        if (ImGui::IsItemDeactivatedAfterEdit()) {  // typed: write the names the way the game wants them
          const keys::Result fixed = keys::Normalize(g_s.binds[i]);
          if (fixed.bad.empty()) {
            snprintf(g_s.binds[i], sizeof(g_s.binds[i]), "%s", fixed.text.c_str());
            changed = true;
          }
        }
        if (!norm.bad.empty())
          ImGui::PopStyleColor();
      }
      ImGui::SameLine();
      if (ImGui::Button(t.keys_set)) {
        g_capture_idx = int(i);
        g_capture_add = false;
      }
      ImGui::SameLine();
      if (ImGui::Button(t.keys_add)) {
        g_capture_idx = int(i);
        g_capture_add = true;
      }
      ImGui::PopID();
    }
    ImGui::EndTable();
  }
  if (!bad_list.empty())
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), t.keys_bad, bad_list.c_str());
  if (!dup_list.empty())
    ImGui::TextColored(ImVec4(0.94f, 0.75f, 0.24f, 1.0f), t.keys_dup, dup_list.c_str());
  if (ImGui::Button(t.reset_keys)) {
    for (size_t i = 0; i < std::size(kBinds); ++i)
      snprintf(g_s.binds[i], sizeof(g_s.binds[i]), "%s", kBinds[i].def);
    g_capture_idx = -1;
    changed = true;
  }
  if (changed)
    SaveSettings();
  ImGui::TextDisabled("%s", t.keys_restart);
  if (!g_settings_ok)
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", t.save_failed);
}

void PageMods() {
  const Texts &t = T();
  Title(t.mods);
  ImGui::TextWrapped("%s", t.mods_text);
  ImGui::Dummy(ImVec2(0, 8));
  const fs::path mods = ExeDir() / "mods";
  std::vector<std::string> files;
  std::error_code ec;
  for (auto it = fs::recursive_directory_iterator(mods, ec); it != fs::recursive_directory_iterator();
       it.increment(ec)) {
    if (it->is_regular_file(ec) && it->path().filename() != "HOW_TO_USE_MODS.txt")
      files.push_back(fs::relative(it->path(), mods, ec).string());
  }
  if (ImGui::Button(t.open_mods))
    OpenFolder(mods);
  ImGui::SameLine();
  ImGui::TextDisabled("%zu %s", files.size(), t.mods_count);
  ImGui::BeginChild("modlist", ImVec2(0, 0), ImGuiChildFlags_Borders);
  for (const auto &f : files)
    ImGui::TextUnformatted(f.c_str());
  ImGui::EndChild();
}

void PageSaves() {
  Title(T().saves);
  g_saves.Draw(g_s.language == 4);
}

// Facts a tester pastes into a bug report. Nothing personal: no user name, no folder paths.
std::string BuildDiagnostics() {
  std::string r = "WWE '12 PC diagnostic\n";
  r += std::string("Version: ") + WWE12_VERSION + "\n";
  r += "Windows: " + diag::WindowsVersion() + "\n";
  r += "CPU: " + diag::CpuName() + "\n";
  r += "RAM: " + diag::Memory() + "\n";
  r += "Graphics (Windows):\n" + diag::Adapters();
  switch (g_vulkan.GetState()) {
  case VulkanCheck::State::Ok: r += "Vulkan: OK\n"; break;
  case VulkanCheck::State::Missing: r += "Vulkan: MISSING\n"; break;
  default: r += "Vulkan: still checking\n"; break;
  }
  r += g_vulkan.Report();
  r += "Settings:";
  for (const char *key : {"user_language", "svr_render_scale", "fullscreen", "bd_vsync", "bd_anisotropy",
                          "wwe12_60fps", "svr_gpu_device", "wwe12_quit_entry", "wwe12_graphics_menu",
                          "mnk_mode"}) {
    const auto v = g_toml.Raw(key);
    r += std::string(" ") + key + "=" + (v ? *v : "default");
  }
  r += "\n";
  std::error_code ec;
  int mods = 0, dlc = 0;
  for (auto it = fs::recursive_directory_iterator(ExeDir() / "mods", ec); it != fs::recursive_directory_iterator();
       it.increment(ec)) {
    if (ec)
      break;
    if (it->is_regular_file(ec) && it->path().filename() != "HOW_TO_USE_MODS.txt")
      ++mods;
  }
  for (const auto &e : fs::directory_iterator(DocumentsDir() / "wwe12" / "0000000000000000" / "54510890" / "00000002", ec))
    if (e.is_directory(ec))
      ++dlc;
  r += std::string("Game files: ") + (GameFilesFound(ExeDir()) ? "found" : "missing") + "\n";
  r += "Mod files: " + std::to_string(mods) + ", DLC installed: " + std::to_string(dlc) + "\n";
  return r;
}

int g_diag_msg = 0;  // 1 copied, 2 failed

void PageAbout() {
  const Texts &t = T();
  Title(t.about);
  ImGui::TextWrapped("%s", t.about_text);
  ImGui::Dummy(ImVec2(0, 12));
  if (ImGui::Button(t.github))
    OpenUrl(L"https://github.com/iMrBurn/wwe12-pc");
  ImGui::Dummy(ImVec2(0, 14));
  ImGui::Separator();
  ImGui::Text("%s", t.support);
  ImGui::TextWrapped("%s", t.support_hint);
  if (ImGui::Button(t.diag_copy))
    g_diag_msg = diag::CopyToClipboard(BuildDiagnostics()) ? 1 : 2;
  ImGui::SameLine();
  if (ImGui::Button(t.open_logs))
    OpenFolder(DocumentsDir() / "wwe12" / "logs");
  if (g_diag_msg == 1)
    ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.45f, 1.0f), "%s", t.diag_copied);
  else if (g_diag_msg == 2)
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", t.diag_failed);
  ImGui::Dummy(ImVec2(0, 8));
  ImGui::TextDisabled("%s %s", t.version, WWE12_VERSION);
}

void DrawUi() {
  const ImGuiViewport *vp = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(vp->Pos);
  ImGui::SetNextWindowSize(vp->Size);
  ImGui::Begin("launcher", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);
  ImDrawList *dl = ImGui::GetWindowDrawList();
  const ImVec2 o = vp->Pos;
  const float W = vp->Size.x, H = vp->Size.y;
  constexpr float kSide = 280;
  // Sidebar: black, with gold and red brush strokes like the game's menus.
  dl->AddRectFilled(o, ImVec2(o.x + kSide, o.y + H), IM_COL32(8, 8, 9, 255));
  Stroke(dl, ImVec2(o.x - 30, o.y + H - 34), kSide + 40, 12, kRed, 30);
  Stroke(dl, ImVec2(o.x - 30, o.y + H - 16), kSide - 40, 5, kGold, 30);
  // Content: a soft gold glow in the corner.
  dl->AddRectFilledMultiColor(ImVec2(o.x + kSide, o.y), ImVec2(o.x + W, o.y + H),
                              IM_COL32(26, 22, 14, 255), IM_COL32(14, 14, 16, 255),
                              IM_COL32(14, 14, 16, 255), IM_COL32(14, 14, 16, 255));

  ImGui::SetCursorPos(ImVec2(20, 18));
  if (g_logo) {
    const float lw = kSide - 40, lh = lw * float(g_logo_h) / float(g_logo_w);
    ImGui::Image(ImTextureRef(ImTextureID(reinterpret_cast<intptr_t>(g_logo.Get()))),
                 ImVec2(lw, lh));
  }
  ImGui::Dummy(ImVec2(0, 14));
  const Texts &t = T();
  const char *names[] = {t.play, t.settings, t.controls, t.mods, t.saves, t.about};
  for (int i = 0; i < 6; ++i) {
    ImGui::SetCursorPosX(16);
    const bool sel = g_page == i;
    if (BigButton(names[i], ImVec2(kSide - 32, 52), sel ? kGold : IM_COL32(28, 28, 32, 255),
                  sel ? IM_COL32(20, 20, 20, 255) : IM_COL32(235, 235, 235, 255)))
      g_page = Page(i);
  }

  if (g_page != kControls)
    g_capture_idx = -1;  // never keep waiting for a key on another page
  ImGui::SetCursorPos(ImVec2(kSide + 40, 30));
  ImGui::BeginChild("page", ImVec2(W - kSide - 70, H - 60));
  switch (g_page) {
  case kPlay: PagePlay(); break;
  case kSettings: PageSettings(); break;
  case kControls: PageControls(); break;
  case kMods: PageMods(); break;
  case kSaves: PageSaves(); break;
  case kAbout: PageAbout(); break;
  }
  ImGui::EndChild();
  ImGui::End();
}

bool LaunchGame() {
  const fs::path dir = ExeDir();
  const std::wstring exe = (dir / L"wwe12.exe").wstring();
  std::wstring cmd = L"\"" + exe + L"\"";
  STARTUPINFOW si{sizeof(si)};
  PROCESS_INFORMATION pi{};
  if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr,
                      dir.wstring().c_str(), &si, &pi))
    return false;
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  return true;
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
    mm->ptMinTrackSize = {960, 600};
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
  LoadSettings();
  g_saves.Init(DocumentsDir() / "wwe12", ExeDir());
  g_vulkan.Start();
  g_updater.Start(ExeDir());
  {
    std::ifstream f(ExeDir() / "CHANGELOG.md", std::ios::binary);
    g_changelog = changelog::Prepare(std::string(std::istreambuf_iterator<char>(f), {}));
  }

  WNDCLASSEXW wc{sizeof(wc)};
  wc.style = CS_CLASSDC;
  wc.lpfnWndProc = WndProc;
  wc.hInstance = inst;
  wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
  wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512) /* IDC_ARROW */);
  wc.lpszClassName = L"WWE12Launcher";
  RegisterClassExW(&wc);
  const float dpi = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY));
  const int w = int(1120 * dpi), h = int(700 * dpi);
  HWND hwnd = CreateWindowW(wc.lpszClassName, L"WWE '12 PC", WS_OVERLAPPEDWINDOW,
                            (GetSystemMetrics(SM_CXSCREEN) - w) / 2,
                            (GetSystemMetrics(SM_CYSCREEN) - h) / 2, w, h, nullptr, nullptr, inst,
                            nullptr);
  if (!CreateDevice(hwnd)) {
    MessageBoxW(hwnd, L"Direct3D 11 is not available.", L"WWE '12 PC", MB_ICONERROR);
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
  g_logo = LoadPngResource(101, g_logo_w, g_logo_h);

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

    if (g_launch) {
      g_launch = false;
      g_play_note = 0;
      if (game::IsRunning())
        g_play_note = 1;  // one game at a time
      else if (g_saves.Busy() || g_updater.Busy())
        g_play_note = 2;  // closing now would cut it short
      else if (LaunchGame())
        done = true;
      else
        MessageBoxW(hwnd, L"wwe12.exe could not be started.", L"WWE '12 PC", MB_ICONERROR);
    }
    if (g_updater.RestartRequested())
      done = true;
  }

  g_saves.Wait();
  g_updater.Shutdown();
  ImGui_ImplDX11_Shutdown();
  ImGui_ImplWin32_Shutdown();
  ImGui::DestroyContext();
  DestroyWindow(hwnd);
  CoUninitialize();
  return 0;
}
