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
#include <cmath>
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
#include "mods.h"
#include "saves.h"
#include "toml_file.h"
#include "ui_style.h"
#include "updater.h"
#include "vulkan_check.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

namespace {

using namespace ui;  // ui_style.h: the look shared with the Mod Maker

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
  const char *tab_display, *tab_graphics, *tab_advanced, *borderless, *window_size, *screen_size, *custom,
      *fps_limit, *no_limit, *fps_limit_hint, *show_fps, *cpu_priority, *prio_normal, *prio_above, *prio_high,
      *prio_hint, *more_graphics, *window_note;
  const char *crowd, *crowd_hint, *msaa, *dof, *shadow_res, *shadow_dist, *edge_blur, *edge_blur_hint;
  const char *music, *music_text, *open_music, *music_count, *music_unread, *music_volume, *music_volume_hint,
      *music_empty;
  const char *icons, *icons_auto, *icons_xbox, *icons_ps, *icons_hint;
  const char *gpu_used, *gpu_dedicated, *gpu_integrated, *gpu_others;
  const char *keys_mod_alone;
  const char *api, *api_auto, *api_vulkan, *api_d3d12, *api_hint, *api_d3d12_missing;
  const char *defenses, *defenses_off, *defenses_hint, *qualifiers, *qualifiers_hint;
  const char *mc_unlock, *mc_unlock_hint;
  const char *aspect, *aspect_auto, *aspect_stretch, *aspect_hint;
  const char *lowend, *lowend_hint, *lowend_undo, *lowend_undo_hint;
};

const Texts kEnglish = {
    "PLAY", "SETTINGS", "CONTROLS", "MODS", "SAVES", "ABOUT",
    "PLAY", "Version", "Game files found.",
    "Game files missing: put your WWE '12 (Xbox 360) .iso in the \"game\" folder.",
    "Language", "Internal resolution", "Automatic", "Display mode", "Full screen", "Window", "V-Sync",
    "60 fps entrances and cutscenes",
    "The original game runs them at 30 fps.",
    "Texture sharpness", "Off", "Graphics card", "Automatic", "EXIT entry in the main menu",
    "Replaces SHOP (the closed Xbox LIVE store).", "Changes are saved right away.",
    "Keyboard",
    "Several keys per action, separated by commas. ZQSD and WASD both work.",
    "Default keys", "Xbox and PlayStation controllers work out of the box.",
    "Files put in the mods folder replace the game's own, at the same path as on the disc.\n"
    "Example: mods\\movies\\titantron\\100.bik replaces that titantron video.",
    "Open the mods folder", "file(s) in the mods folder",
    "WWE '12 PC is an unofficial native port of WWE '12 (Xbox 360), made by fans.\n"
    "Native renderer adapted from nefariousjosiah's SmackDown vs. Raw ports, on the ReXGlue SDK\n"
    "and re:Blue. DLC support by Banshee. Mod Maker and texture packs based on Kaiko's\n"
    "SmackDown vs. Raw 2011 port (Bink reader ported from FFmpeg, LGPL). Free software under the GPL-3.0 licence.\n"
    "No game data is included.\n\n"
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
    "Player 1 (shares player 1 with controller 1)", "Player 2 (separate controller)", "Player 3", "Player 4",
    "DISPLAY", "GRAPHICS", "ADVANCED", "Borderless window", "Window size", "Screen size", "Custom",
    "Frame limit", "No limit",
    "The game already runs at 60 frames per second. A 60 limit only evens out short bursts above 60.",
    "Show the FPS counter (F2 shows or hides it in the game)", "CPU priority", "Normal", "Above normal", "High",
    "A higher priority can help on a busy PC; High may slow your other programs.",
    "Graphics options apply the next time you start the game.",
    "Window mode and size apply the next time you start the game.",
    "Draw the crowd", "Off = empty stands, much lighter on small PCs.", "Anti-aliasing (smooth edges)",
    "Depth of field (blur behind the wrestlers)", "Shadows", "Shadow distance",
    "Edge blur filter", "The console's own anti-aliasing blur; off keeps the picture sharp.",
    "MUSIC",
    "Put your own tracks (mp3, m4a, aac, wma, wav, flac) in the music folder. In the game, pick them wherever "
    "it lets you choose the music of a created wrestler. Track names are cut to 39 characters in the game.",
    "Open the music folder", "track(s) found", "not read by the game (unsupported format):", "Volume of your tracks",
    "-20 dB matches the game's own music. Louder songs are lowered to this level. Applies the next time you start the game.",
    "No track yet: put some files in the music folder.",
    "Button icons in the game", "Automatic", "Xbox", "PlayStation",
    "Automatic shows PlayStation icons only if the PlayStation controller was already detected when the game "
    "started; if you still see Xbox icons with your PlayStation controller, pick PlayStation. Applies the next "
    "time you start the game.",
    "Graphics card used: %s", "dedicated", "integrated", "Cards found: %s",
    "Shift, Ctrl or Alt set on its own is a key of its own, so these combinations will do nothing: %s",
    "Graphics API", "Automatic", "Vulkan", "Direct3D 12",
    "Vulkan by default; try Direct3D 12 if you have graphics problems. Applies the next time you start the game.",
    "The Direct3D 12 version of the game (wwe12_d3d12.exe) was not found: the game starts with Vulkan instead.",
    "Title defenses in weekly shows", "Off (original game)",
    "In Universe mode, a match on the weekly card can become a title match. Percentage chance.",
    "Money in the Bank / Elimination Chamber qualifiers",
    "Qualifying matches and news before Money in the Bank and Elimination Chamber (Universe mode).",
    "Extended Match Creator",
    "Unlocks in the Match Creator: time limit everywhere, KO and Last Man Standing in Cage/Hell in a Cell, "
    "Over the Top Rope and Elimination with 3+ Superstars.",
    "Picture format", "Automatic (black bars)", "Stretch (fills the screen)",
    "Automatic keeps the game's 16:9 shape, with black bars on other screens (16:10, 21:9...). Stretch fills the whole screen, but the picture is stretched: about 11 % on a 16:10 screen, more on wider screens.",
    "Low-end PC",
    "480p, no crowd, no shadows, no depth of field: for weak PCs. Nothing else changes.",
    "Undo the preset",
    "Puts back the resolution, crowd, shadows and depth of field you had before the preset."};

const Texts kFrench = {
    "JOUER", "RÉGLAGES", "CONTRÔLES", "MODS", "SAUVEGARDES", "À PROPOS",
    "JOUER", "Version", "Fichiers du jeu trouvés.",
    "Fichiers du jeu manquants : mettez l'image .iso de votre WWE '12 (Xbox 360) dans le "
    "dossier \"game\".",
    "Langue", "Résolution interne", "Automatique", "Mode d'affichage", "Plein écran", "Fenêtre", "V-Sync",
    "Entrées et cinématiques à 60 fps",
    "Le jeu d'origine les fait tourner à 30 images par seconde.",
    "Netteté des textures", "Désactivée", "Carte graphique", "Automatique",
    "Entrée QUITTER dans le menu principal", "Remplace SHOP (la boutique Xbox LIVE, fermée).",
    "Les changements sont enregistrés tout de suite.",
    "Clavier",
    "Plusieurs touches par action, séparées par des virgules. ZQSD et WASD marchent tous les deux.",
    "Touches par défaut", "Les manettes Xbox et PlayStation marchent d'office.",
    "Les fichiers posés dans le dossier mods remplacent ceux du jeu, au même chemin que sur le "
    "disque.\nExemple : mods\\movies\\titantron\\100.bik remplace cette vidéo de titantron.",
    "Ouvrir le dossier mods", "fichier(s) dans le dossier mods",
    "WWE '12 PC est un portage natif non officiel de WWE '12 (Xbox 360), fait par des fans.\n"
    "Rendu natif adapté des portages SmackDown vs. Raw de nefariousjosiah, sur le SDK ReXGlue\n"
    "et re:Blue. Support des DLC par Banshee. Mod Maker et packs de textures basés sur le\n"
    "portage SmackDown vs. Raw 2011 de Kaiko (lecteur Bink porté de FFmpeg, LGPL). Logiciel libre sous licence GPL-3.0.\n"
    "Aucune donnée du jeu n'est fournie.\n\n"
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
    "Joueur 1 (partage le joueur 1 avec la manette 1)", "Joueur 2 (manette séparée)", "Joueur 3", "Joueur 4",
    "AFFICHAGE", "GRAPHISMES", "AVANCÉ", "Fenêtre sans bordure", "Taille de la fenêtre", "Taille de l'écran",
    "Personnalisée", "Limite d'images", "Aucune limite",
    "Le jeu tourne déjà à 60 images par seconde. Une limite à 60 sert seulement à lisser les courtes pointes au-dessus de 60.",
    "Afficher le compteur d'images par seconde (F2 l'affiche ou le cache dans le jeu)", "Priorité du processeur",
    "Normale", "Supérieure", "Haute",
    "Une priorité plus haute peut aider sur un PC chargé ; Haute peut ralentir vos autres programmes.",
    "Les options graphiques s'appliquent au prochain lancement du jeu.",
    "Le mode et la taille de la fenêtre s'appliquent au prochain lancement du jeu.",
    "Afficher la foule", "Désactivée = gradins vides, bien plus léger sur les petits PC.",
    "Anticrénelage (bords lissés)", "Profondeur de champ (flou derrière les catcheurs)", "Ombres",
    "Distance des ombres", "Filtre de flou des bords",
    "Le flou anticrénelage de la console ; désactivé garde l'image nette.",
    "MUSIQUE",
    "Mettez vos propres morceaux (mp3, m4a, aac, wma, wav, flac) dans le dossier music. Dans le jeu, choisissez-les "
    "là où il laisse choisir la musique d'un catcheur créé. Les noms sont coupés à 39 caractères dans le jeu.",
    "Ouvrir le dossier musique", "morceau(x) trouvé(s)", "non lus par le jeu (format non pris en charge) :",
    "Volume de vos morceaux",
    "-20 dB correspond à la musique du jeu. Les morceaux plus forts sont baissés à ce niveau. S'applique au prochain lancement du jeu.",
    "Aucun morceau : mettez des fichiers dans le dossier musique.",
    "Icônes des boutons dans le jeu", "Automatique", "Xbox", "PlayStation",
    "Automatique affiche les icônes PlayStation seulement si la manette PlayStation était déjà détectée au "
    "démarrage du jeu ; si vous voyez encore les icônes Xbox avec votre manette PlayStation, choisissez "
    "PlayStation. S'applique au prochain lancement du jeu.",
    "Carte graphique utilisée : %s", "dédiée", "intégrée", "Cartes détectées : %s",
    "Shift, Ctrl ou Alt réglé seul devient une touche à part entière : ces combinaisons ne marcheront donc pas : %s",
    "API graphique", "Automatique", "Vulkan", "Direct3D 12",
    "Vulkan par défaut ; essayez Direct3D 12 si vous avez des problèmes graphiques. S'applique au prochain lancement du jeu.",
    "La version Direct3D 12 du jeu (wwe12_d3d12.exe) est introuvable : le jeu démarre avec Vulkan à la place.",
    "Défenses de titre en weekly", "Désactivées (jeu d'origine)",
    "Dans le mode Univers, un match de la carte weekly peut devenir un match de titre. Pourcentage de chances.",
    "Qualifications Money in the Bank / Elimination Chamber",
    "Matchs de qualification et news avant Money in the Bank et Elimination Chamber (mode Univers).",
    "Match Creator étendu",
    "Débloque dans le Match Creator : limite de temps partout, K.O. et Last Man Standing en cage/Hell in a Cell, "
    "Over the Top Rope et Élimination à 3 catcheurs ou plus.",
    "Format de l'image", "Automatique (bandes noires)", "Étirer (remplit l'écran)",
    "Automatique garde la forme 16:9 du jeu, avec des bandes noires sur les autres écrans (16:10, 21:9...). Étirer remplit tout l'écran, mais l'image est étirée : d'environ 11 % sur un écran 16:10, davantage sur un écran plus large.",
    "PC modeste",
    "480p, sans foule, sans ombres, sans profondeur de champ : pour les PC peu puissants. Rien d'autre ne change.",
    "Annuler le préréglage",
    "Remet la résolution, la foule, les ombres et la profondeur de champ que vous aviez avant le préréglage."};

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
  bool borderless = false;  // with fullscreen off: a window without title bar
  int win_w = 0, win_h = 0;  // window size in pixels (0 = the game's default)
  int fps_limit = 0;         // 0 = none
  bool show_fps = false;
  int cpu_priority = 0;  // 0 normal, 1 above normal, 2 high
  bool vsync = true;
  bool fps60 = false;
  int aniso = 16;
  int title_defenses = 0;    // wwe12_universe_title_defenses, percent 0..100 (0 = the original game)
  bool qualifiers = false;   // wwe12_universe_qualifiers
  int aspect = 5;            // bd_aspect_ratio: 5 automatic (black bars), 6 stretch
  bool mc_unlock = true;     // wwe12_mc_unlock (extended Match Creator), on by default
  int api = 0;  // gpu_backend: 0 automatic ("any"), 1 vulkan, 2 d3d12 (wwe12_d3d12.exe)
  bool crowd = true;
  bool shadows = true;
  bool dof = true;  // depth of field
  int icons = 0;       // wwe12_button_icons: 0 auto, 1 xbox, 2 playstation
  int music_db = -20;  // xmp_music_level_db, -40..-6
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
  g_s.borderless = g_toml.GetBool("borderless", false);
  g_s.win_w = g_toml.GetInt("window_width", 0);
  g_s.win_h = g_toml.GetInt("window_height", 0);
  g_s.fps_limit = g_toml.GetInt("wwe12_fps_limit", 0);
  g_s.show_fps = g_toml.GetBool("wwe12_show_fps", false);
  g_s.cpu_priority = std::clamp(g_toml.GetInt("wwe12_cpu_priority", 0), 0, 2);
  g_s.vsync = g_toml.GetBool("bd_vsync", true);
  g_s.fps60 = g_toml.GetBool("wwe12_60fps", false);
  g_s.aniso = g_toml.GetInt("bd_anisotropy", 16);
  {
    const std::string api = g_toml.GetString("gpu_backend", "any");
    g_s.api = api == "d3d12" ? 2 : api == "vulkan" ? 1 : 0;
  }
  g_s.title_defenses = std::clamp(g_toml.GetInt("wwe12_universe_title_defenses", 0), 0, 100);
  g_s.qualifiers = g_toml.GetBool("wwe12_universe_qualifiers", false);
  g_s.mc_unlock = g_toml.GetBool("wwe12_mc_unlock", true);
  g_s.aspect = g_toml.GetInt("bd_aspect_ratio", 5);
  g_s.crowd = g_toml.GetBool("wwe12_crowd", true);
  g_s.shadows = g_toml.GetBool("wwe12_shadows", true);
  g_s.dof = g_toml.GetBool("wwe12_dof", true);
  {
    const std::string ic = g_toml.GetString("wwe12_button_icons", "auto");
    g_s.icons = ic == "xbox" ? 1 : ic == "playstation" ? 2 : 0;
  }
  g_s.music_db = std::clamp(g_toml.GetInt("xmp_music_level_db", -20), -40, -6);
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
  g_toml.SetBool("borderless", g_s.borderless);
  g_toml.SetInt("window_width", g_s.win_w);
  g_toml.SetInt("window_height", g_s.win_h);
  g_toml.SetInt("wwe12_fps_limit", g_s.fps_limit);
  g_toml.SetBool("wwe12_show_fps", g_s.show_fps);
  g_toml.SetInt("wwe12_cpu_priority", g_s.cpu_priority);
  g_toml.SetBool("bd_vsync", g_s.vsync);
  g_toml.SetBool("wwe12_60fps", g_s.fps60);
  g_toml.SetInt("bd_anisotropy", g_s.aniso);
  g_toml.SetString("gpu_backend", g_s.api == 2 ? "d3d12" : g_s.api == 1 ? "vulkan" : "any");
  g_toml.SetInt("wwe12_universe_title_defenses", g_s.title_defenses);
  g_toml.SetBool("wwe12_universe_qualifiers", g_s.qualifiers);
  g_toml.SetBool("wwe12_mc_unlock", g_s.mc_unlock);
  g_toml.SetInt("bd_aspect_ratio", g_s.aspect);
  g_toml.SetBool("wwe12_crowd", g_s.crowd);
  g_toml.SetBool("wwe12_shadows", g_s.shadows);
  g_toml.SetBool("wwe12_dof", g_s.dof);
  g_toml.SetString("wwe12_button_icons", g_s.icons == 1 ? "xbox" : g_s.icons == 2 ? "playstation" : "auto");
  g_toml.SetInt("xmp_music_level_db", g_s.music_db);
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
// Pages
enum Page { kPlay, kSettings, kControls, kMods, kSaves, kMusic, kAbout };
Page g_page = kPlay;
ComPtr<ID3D11ShaderResourceView> g_logo;
int g_logo_w = 0, g_logo_h = 0;
saves::Manager g_saves;
mods::Manager g_mods;
bool g_launch = false;
int g_play_note = 0;  // 1 game already running, 2 something is busy
Updater g_updater;
VulkanCheck g_vulkan;
std::string g_changelog;  // CHANGELOG.md next to the exe (the notes of this version), prepared

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
    {
      // The card the game will use (like its own choice), then the others, if any.
      const auto used = g_vulkan.UsedDevice(g_s.gpu);
      if (!used.name.empty()) {
        std::string label = used.name;
        if (used.kind == 1)
          label += std::string(" (") + t.gpu_dedicated + ")";
        else if (used.kind == 2)
          label += std::string(" (") + t.gpu_integrated + ")";
        ImGui::TextDisabled(t.gpu_used, label.c_str());
        const std::string all = g_vulkan.Detail();
        if (all.find(", ") != std::string::npos)
          ImGui::TextDisabled(t.gpu_others, all.c_str());
      } else {
        ImGui::TextDisabled(t.vulkan_ok, g_vulkan.Detail().c_str());
      }
    }
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

// "Low-end PC" preset: 480p, no crowd, no shadows, no depth of field. The values from before are kept
// in lowend_preset_backup.txt (next to the launcher) so the preset can be undone; clicking the preset
// again never overwrites that backup.
fs::path LowEndBackupPath() { return ExeDir() / "lowend_preset_backup.txt"; }

bool LowEndBackupExists() {
  std::error_code ec;
  return fs::exists(LowEndBackupPath(), ec);
}

void ApplyLowEndPreset() {
  if (!LowEndBackupExists()) {
    std::ofstream f(LowEndBackupPath(), std::ios::trunc);
    f << "svr_render_scale=" << g_s.scale << "\n"
      << "wwe12_crowd=" << (g_s.crowd ? 1 : 0) << "\n"
      << "wwe12_shadows=" << (g_s.shadows ? 1 : 0) << "\n"
      << "wwe12_dof=" << (g_s.dof ? 1 : 0) << "\n";
    if (!f)
      return;  // no backup, no preset: the player could not get his settings back
  }
  g_s.scale = 480;
  g_s.crowd = false;
  g_s.shadows = false;
  g_s.dof = false;
  SaveSettings();
}

void UndoLowEndPreset() {
  std::ifstream f(LowEndBackupPath());
  std::string line;
  while (std::getline(f, line)) {
    const size_t eq = line.find('=');
    if (eq == std::string::npos)
      continue;
    const std::string key = line.substr(0, eq);
    int value = 0;
    try {
      value = std::stoi(line.substr(eq + 1));
    } catch (...) {
      continue;
    }
    if (key == "svr_render_scale")
      g_s.scale = value;
    else if (key == "wwe12_crowd")
      g_s.crowd = value != 0;
    else if (key == "wwe12_shadows")
      g_s.shadows = value != 0;
    else if (key == "wwe12_dof")
      g_s.dof = value != 0;
  }
  f.close();
  std::error_code ec;
  fs::remove(LowEndBackupPath(), ec);
  SaveSettings();
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
  ImGui::Dummy(ImVec2(0, 4));

  // One-click preset for weak PCs, and a way back.
  if (ImGui::Button(t.lowend)) {
    ApplyLowEndPreset();
    changed = true;
  }
  if (LowEndBackupExists()) {
    ImGui::SameLine();
    if (ImGui::Button(t.lowend_undo)) {
      UndoLowEndPreset();
      changed = true;
    }
  }
  ImGui::PushTextWrapPos(0);
  ImGui::TextDisabled("%s", t.lowend_hint);
  if (LowEndBackupExists())
    ImGui::TextDisabled("%s", t.lowend_undo_hint);
  ImGui::PopTextWrapPos();
  ImGui::Dummy(ImVec2(0, 4));

  if (ImGui::BeginTabBar("settings_tabs")) {
    if (ImGui::BeginTabItem(t.tab_display)) {
      // Window mode: fullscreen, a window, or a window without title bar.
      int mode = g_s.fullscreen ? 0 : (g_s.borderless ? 2 : 1);
      const char *mode_names[] = {t.fullscreen, t.window, t.borderless};
      if (ImGui::Combo(t.display, &mode, mode_names, 3)) {
        g_s.fullscreen = mode == 0;
        g_s.borderless = mode == 2;
        changed = true;
      }

      // Window size (used when not fullscreen).
      struct Size {
        int w, h;
      };
      const Size sizes[] = {{0, 0},       {1280, 720},  {1600, 900}, {1920, 1080},
                            {2560, 1440}, {GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)}};
      char names[7][48];
      snprintf(names[0], sizeof(names[0]), "%s", t.res_auto);
      for (int i = 1; i < 5; ++i)
        snprintf(names[i], sizeof(names[i]), "%dx%d", sizes[i].w, sizes[i].h);
      snprintf(names[5], sizeof(names[5]), "%s (%dx%d)", t.screen_size, sizes[5].w, sizes[5].h);
      int count = 6, si = -1;
      for (int i = 0; i < 6; ++i)
        if (sizes[i].w == g_s.win_w && sizes[i].h == g_s.win_h) {
          si = i;
          break;
        }
      if (si < 0) {  // a size typed by hand in wwe12.toml: keep it, shown as custom
        snprintf(names[6], sizeof(names[6]), "%s (%dx%d)", t.custom, g_s.win_w, g_s.win_h);
        si = count++;
      }
      const char *size_names[7];
      for (int i = 0; i < count; ++i)
        size_names[i] = names[i];
      ImGui::BeginDisabled(g_s.fullscreen);
      if (ImGui::Combo(t.window_size, &si, size_names, count) && si < 6) {
        g_s.win_w = sizes[si].w;
        g_s.win_h = sizes[si].h;
        changed = true;
      }
      ImGui::EndDisabled();

      // svr_render_scale holds a height (360, 480, 540, 720, 1080, 1440, 2160) or, from older files, a multiple.
      static const int kResHeights[] = {0, 360, 480, 540, 720, 1080, 1440, 2160};
      int ri = 0;
      switch (g_s.scale) {
      case 360: ri = 1; break;
      case 480: ri = 2; break;
      case 540: ri = 3; break;
      case 1: case 720: ri = 4; break;
      case 1080: ri = 5; break;
      case 2: case 1440: ri = 6; break;
      case 3: case 4: case 2160: case 2880: ri = 7; break;
      default: ri = 0; break;
      }
      const char *res_names[] = {t.res_auto, "360p", "480p", "540p", "720p", "1080p", "1440p", "4K (2160p)"};
      if (ImGui::Combo(t.resolution, &ri, res_names, 8)) {
        g_s.scale = kResHeights[ri];
        changed = true;
      }

      // Picture format: bd_aspect_ratio 5 (automatic, black bars) or 6 (stretch to the screen).
      {
        int ai = g_s.aspect == 6 ? 1 : 0;
        char custom_aspect[48];
        const char *aspect_names[3] = {t.aspect_auto, t.aspect_stretch, custom_aspect};
        int aspect_count = 2;
        if (g_s.aspect != 5 && g_s.aspect != 6) {  // another ratio typed by hand in wwe12.toml
          snprintf(custom_aspect, sizeof(custom_aspect), "%s (%d)", t.custom, g_s.aspect);
          ai = aspect_count++;
        }
        if (ImGui::Combo(t.aspect, &ai, aspect_names, aspect_count) && ai < 2) {
          g_s.aspect = ai == 1 ? 6 : 5;
          changed = true;
        }
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("%s", t.aspect_hint);
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0, 4));
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

      // Frame limit: none or 60. The game paces itself at 60 (a lower limit would slow the game
      // down, so it is not offered; wwe12.toml still accepts any value).
      static const int kLimits[] = {0, 60};
      int fi = -1;
      for (int i = 0; i < 2; ++i)
        if (kLimits[i] == g_s.fps_limit)
          fi = i;
      char custom_limit[48];
      const char *limit_names[3] = {t.no_limit, "60", custom_limit};
      int limit_count = 2;
      if (fi < 0) {  // another value typed by hand in wwe12.toml
        snprintf(custom_limit, sizeof(custom_limit), "%s (%d)", t.custom, g_s.fps_limit);
        fi = limit_count++;
      }
      if (ImGui::Combo(t.fps_limit, &fi, limit_names, limit_count) && fi < 2) {
        g_s.fps_limit = kLimits[fi];
        changed = true;
      }
      ImGui::PopItemWidth();
      ImGui::TextDisabled("%s", t.fps_limit_hint);

      ImGui::Dummy(ImVec2(0, 6));
      changed |= ImGui::Checkbox(t.vsync, &g_s.vsync);
      ImGui::PushItemWidth(320);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(t.tab_graphics)) {
      {
        const char *api_names[] = {t.api_auto, t.api_vulkan, t.api_d3d12};
        changed |= ImGui::Combo(t.api, &g_s.api, api_names, 3);
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("%s", t.api_hint);
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0, 6));
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
      changed |= ImGui::Checkbox(t.crowd, &g_s.crowd);
      ImGui::SameLine();
      ImGui::TextDisabled("(%s)", t.crowd_hint);
      changed |= ImGui::Checkbox(t.shadow_res, &g_s.shadows);
      changed |= ImGui::Checkbox(t.dof, &g_s.dof);
      ImGui::Dummy(ImVec2(0, 8));
      ImGui::TextDisabled("%s", t.more_graphics);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(t.tab_advanced)) {
      const char *prio_names[] = {t.prio_normal, t.prio_above, t.prio_high};
      if (ImGui::Combo(t.cpu_priority, &g_s.cpu_priority, prio_names, 3))
        changed = true;
      ImGui::TextDisabled("%s", t.prio_hint);
      ImGui::Dummy(ImVec2(0, 6));
      changed |= ImGui::Checkbox(t.fps60, &g_s.fps60);
      ImGui::SameLine();
      ImGui::TextDisabled("(%s)", t.fps60_hint);
      changed |= ImGui::Checkbox(t.exit_entry, &g_s.exit_entry);
      ImGui::SameLine();
      ImGui::TextDisabled("(%s)", t.exit_hint);
      changed |= ImGui::Checkbox(t.show_fps, &g_s.show_fps);

      // Universe mode (the same two settings are in the game's OPTIONS).
      ImGui::Dummy(ImVec2(0, 8));
      {
        static const int kDefenses[] = {0, 10, 25, 50, 100};
        int di = -1;
        for (int i = 0; i < 5; ++i)
          if (kDefenses[i] == g_s.title_defenses)
            di = i;
        char names[5][48], custom[48];
        snprintf(names[0], sizeof(names[0]), "%s", t.defenses_off);
        for (int i = 1; i < 5; ++i)
          snprintf(names[i], sizeof(names[i]), "%d %%", kDefenses[i]);
        const char *shown[6] = {names[0], names[1], names[2], names[3], names[4], custom};
        int count = 5;
        if (di < 0) {  // another value typed by hand in wwe12.toml
          snprintf(custom, sizeof(custom), "%s (%d %%)", t.custom, g_s.title_defenses);
          di = count++;
        }
        if (ImGui::Combo(t.defenses, &di, shown, count) && di < 5) {
          g_s.title_defenses = kDefenses[di];
          changed = true;
        }
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("%s", t.defenses_hint);
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0, 4));
        changed |= ImGui::Checkbox(t.qualifiers, &g_s.qualifiers);
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("%s", t.qualifiers_hint);
        ImGui::PopTextWrapPos();
        ImGui::Dummy(ImVec2(0, 4));
        changed |= ImGui::Checkbox(t.mc_unlock, &g_s.mc_unlock);
        ImGui::PushTextWrapPos(0);
        ImGui::TextDisabled("%s", t.mc_unlock_hint);
        ImGui::PopTextWrapPos();
      }
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::PopItemWidth();

  if (changed)
    SaveSettings();
  ImGui::Dummy(ImVec2(0, 12));
  ImGui::TextDisabled("%s", t.saved_note);
  ImGui::TextDisabled("%s", t.window_note);
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
  {
    const char *icon_names[] = {t.icons_auto, t.icons_xbox, t.icons_ps};
    ImGui::SetNextItemWidth(380);
    changed |= ImGui::Combo(t.icons, &g_s.icons, icon_names, 3);
    ImGui::PushTextWrapPos(0);
    ImGui::TextDisabled("%s", t.icons_hint);
    ImGui::PopTextWrapPos();
    ImGui::Dummy(ImVec2(0, 6));
  }
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
  {
    std::vector<std::string> all;
    for (size_t i = 0; i < std::size(kBinds); ++i)
      all.push_back(keys::Normalize(g_s.binds[i]).text);
    const std::string conflicts = keys::ModifierConflicts(all);
    if (!conflicts.empty())
      ImGui::TextColored(ImVec4(0.94f, 0.75f, 0.24f, 1.0f), t.keys_mod_alone, conflicts.c_str());
  }
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

void PageModsTextures(bool fr);
std::string g_modmaker_error;

void OpenModMaker(bool fr) {
  const fs::path exe = ExeDir() / L"WWE12 Mod Maker.exe";
  g_modmaker_error.clear();
  if (reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", exe.wstring().c_str(), nullptr,
                                              ExeDir().wstring().c_str(), SW_SHOWNORMAL)) <= 32)
    g_modmaker_error = fr ? "« WWE12 Mod Maker.exe » est introuvable à côté du launcher."
                          : "\"WWE12 Mod Maker.exe\" was not found next to the launcher.";
}
void PageModsFolder(const Texts &t);

void PageMods() {
  const Texts &t = T();
  Title(t.mods);
  const bool fr = g_s.language == 4;
  // Making mods is the Mod Maker's job (entrance videos, texture mods, the game's files); the launcher
  // installs and manages them.
  if (ImGui::Button(fr ? "Ouvrir le Mod Maker" : "Open the Mod Maker"))
    OpenModMaker(fr);
  ImGui::SameLine();
  ImGui::TextDisabled("%s", fr ? "Créer des mods : vidéos d'entrée, textures..." : "Make mods: entrance videos, "
                                                                                   "textures...");
  if (!g_modmaker_error.empty())
    ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", g_modmaker_error.c_str());
  if (!ImGui::BeginTabBar("modtabs"))
    return;
  if (ImGui::BeginTabItem(fr ? "Mods installés" : "Installed mods")) {
    ImGui::Dummy(ImVec2(0, 4));
    g_mods.Draw(fr);
    ImGui::Dummy(ImVec2(0, 10));
    PageModsFolder(t);
    ImGui::EndTabItem();
  }
  if (ImGui::BeginTabItem("Textures")) {
    ImGui::Dummy(ImVec2(0, 4));
    PageModsTextures(fr);
    ImGui::EndTabItem();
  }
  ImGui::EndTabBar();
}

// Texture dumps and packs (game: src/native/wwe12_texture_packs.h).
void PageModsTextures(bool fr) {
  bool dump = g_toml.GetBool("wwe12_texture_dump", false);
  if (ImGui::Checkbox(fr ? "Dump des textures (pour faire des packs)" : "Dump textures (to make texture packs)",
                      &dump)) {
    g_toml.SetBool("wwe12_texture_dump", dump);
    SaveSettings();
  }
  ImGui::TextDisabled("%s", fr ? "Chaque texture affichée en jeu est enregistrée une fois en PNG dans texture_dumps. "
                                 "Une image du dossier textures nommée ..._<empreinte>.png (ou .dds) la remplace, à "
                                 "n'importe quelle taille."
                               : "Every texture shown in the game is saved once as a PNG in texture_dumps. A picture "
                                 "in the textures folder named ..._<hash>.png (or .dds) replaces it, at any size.");
  if (ImGui::Button(fr ? "Ouvrir texture_dumps" : "Open texture_dumps")) {
    std::error_code tec;
    fs::create_directories(ExeDir() / "texture_dumps", tec);
    OpenFolder(ExeDir() / "texture_dumps");
  }
  ImGui::SameLine();
  if (ImGui::Button(fr ? "Ouvrir textures" : "Open textures")) {
    std::error_code tec;
    fs::create_directories(ExeDir() / "textures", tec);
    OpenFolder(ExeDir() / "textures");
  }
  ImGui::Dummy(ImVec2(0, 6));
  ImGui::TextDisabled("%s", fr ? "Pour retoucher une image pas à pas et en faire un mod : le Mod Maker."
                               : "To edit a picture step by step and make a mod of it: the Mod Maker.");
}

// The files the player put in the mods folder by hand.
void PageModsFolder(const Texts &t) {
  ImGui::TextWrapped("%s", t.mods_text);
  ImGui::Dummy(ImVec2(0, 8));
  const fs::path mods = ExeDir() / "mods";
  // The player's own files: not the installed .svrmod mods nor the files the launcher put in for them.
  std::vector<std::string> files;
  std::error_code ec;
  for (auto it = fs::recursive_directory_iterator(mods, ec); it != fs::recursive_directory_iterator();
       it.increment(ec)) {
    if (it->is_directory(ec) && it->path().filename() == ".svrmods") {
      it.disable_recursion_pending();
      continue;
    }
    if (!it->is_regular_file(ec) || it->path().filename() == "HOW_TO_USE_MODS.txt")
      continue;
    const std::u8string rel = fs::relative(it->path(), mods, ec).generic_u8string();
    if (!g_mods.IsDeployed(std::string(rel.begin(), rel.end())))
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

void PageMusic() {
  const Texts &t = T();
  Title(t.music);
  ImGui::TextWrapped("%s", t.music_text);
  ImGui::Dummy(ImVec2(0, 8));
  // Same folder as the game: xmp_music_folder, or "music" next to the exe when empty.
  const std::string custom = g_toml.GetString("xmp_music_folder", "");
  const fs::path dir = custom.empty() ? ExeDir() / "music" : fs::path(custom);
  std::vector<std::string> tracks, unread;
  std::error_code ec;
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
    if (!it->is_regular_file(ec))
      continue;
    std::string ext = it->path().extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    const std::string name = it->path().filename().string();
    if (ext == ".mp3" || ext == ".m4a" || ext == ".aac" || ext == ".wma" || ext == ".wav" || ext == ".flac")
      tracks.push_back(name);
    else if (ext == ".ogg" || ext == ".opus" || ext == ".mid" || ext == ".midi")
      unread.push_back(name);
  }
  std::sort(tracks.begin(), tracks.end());
  if (ImGui::Button(t.open_music)) {
    fs::create_directories(dir, ec);
    OpenFolder(dir);
  }
  ImGui::SameLine();
  ImGui::TextDisabled("%zu %s", tracks.size(), t.music_count);

  ImGui::Dummy(ImVec2(0, 6));
  ImGui::SetNextItemWidth(320);
  if (ImGui::SliderInt(t.music_volume, &g_s.music_db, -40, -6, "%d dB")) {
    SaveSettings();
  }
  ImGui::TextDisabled("%s", t.music_volume_hint);
  ImGui::Dummy(ImVec2(0, 6));

  if (!unread.empty()) {
    ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.3f, 1.0f), "%zu %s", unread.size(), t.music_unread);
    for (const auto &f : unread)
      ImGui::BulletText("%s", f.c_str());
  }
  ImGui::BeginChild("musiclist", ImVec2(0, 0), ImGuiChildFlags_Borders);
  if (tracks.empty())
    ImGui::TextDisabled("%s", t.music_empty);
  for (const auto &f : tracks)
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
                          "mnk_mode", "wwe12_crowd", "wwe12_shadows", "wwe12_dof"}) {
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
  {
    std::error_code e2;
    const bool d3d12_present = fs::exists(ExeDir() / L"wwe12_d3d12.exe", e2);
    const bool use_d3d12 = g_s.api == 2 && d3d12_present;
    r += std::string("Graphics API: ") + (use_d3d12 ? "Direct3D 12 (wwe12_d3d12.exe)" : "Vulkan (wwe12.exe)");
    r += std::string(", gpu_backend=") + (g_s.api == 2 ? "d3d12" : g_s.api == 1 ? "vulkan" : "any");
    r += std::string(", wwe12_d3d12.exe ") + (d3d12_present ? "present" : "missing");
    r += std::string(", dxcompiler.dll ") + (fs::exists(ExeDir() / L"dxcompiler.dll", e2) ? "present" : "missing");
    r += std::string(", dxil.dll ") + (fs::exists(ExeDir() / L"dxil.dll", e2) ? "present" : "missing") + "\n";
  }
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
  const char *names[] = {t.play, t.settings, t.controls, t.mods, t.saves, t.music, t.about};
  for (int i = 0; i < 7; ++i) {
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
  case kMusic: PageMusic(); break;
  case kAbout: PageAbout(); break;
  }
  ImGui::EndChild();
  ImGui::End();
}

// wwe12_d3d12.exe when the player chose Direct3D 12 (gpu_backend = "d3d12") and it is there,
// wwe12.exe (Vulkan) otherwise, with a message if the Direct3D 12 build is missing.
bool LaunchGame(HWND hwnd) {
  const fs::path dir = ExeDir();
  fs::path exe_path = dir / L"wwe12.exe";
  if (g_s.api == 2) {
    std::error_code ec;
    if (fs::exists(dir / L"wwe12_d3d12.exe", ec)) {
      exe_path = dir / L"wwe12_d3d12.exe";
    } else {
      const std::string msg = T().api_d3d12_missing;
      std::wstring wide(msg.size() + 1, L'\0');
      const int n = MultiByteToWideChar(CP_UTF8, 0, msg.c_str(), -1, wide.data(), int(wide.size()));
      wide.resize(n > 0 ? size_t(n - 1) : 0);
      MessageBoxW(hwnd, wide.c_str(), L"WWE '12 PC", MB_ICONINFORMATION);
    }
  }
  const std::wstring exe = exe_path.wstring();
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
  g_mods.Init(ExeDir() / "mods");
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
  const int w = int(1120 * dpi), h = int(780 * dpi);
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
      else if (LaunchGame(hwnd))
        done = true;
      else
        MessageBoxW(hwnd, L"The game could not be started.", L"WWE '12 PC", MB_ICONERROR);
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
