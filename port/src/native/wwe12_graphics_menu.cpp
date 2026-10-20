// WWE '12 PC: a GRAPHICS screen in the game's OPTIONS menu.
//
// The OPTIONS menu's "IN-GAME MANUAL" entry (string 41184; "COMMENT JOUER" in French) has no use
// on PC: its text is rewritten in memory as the language's "GRAPHICS" (the way wwe12_quit.cpp
// renames Xbox LIVE), and choosing it opens this screen instead of the manual (decide hook in
// wwe12_quit.cpp). Building a real page of the game's menus was not possible (adding a main menu
// entry already broke the menus' navigation), so the screen is drawn over the game with ImGui,
// in the menus' colours, and the game gets no input while it is open.
//
// Controls: up/down to pick a setting, left/right or A/Enter to change it, B/Escape to go back.
// LB/RB (or PageUp/PageDown) switch between the DISPLAY, GRAPHICS and ADVANCED tabs.
// Changes apply at once (the internal resolution at the next start) and are saved to wwe12.toml.
//
// Disabled with wwe12_graphics_menu = false in wwe12.toml (the manual comes back).

#include "wwe12_graphics_menu.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <imgui.h>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/input/input_system.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>

#include "../launcher/toml_file.h"
#include "core/logging.h"

REXCVAR_DEFINE_BOOL(wwe12_graphics_menu, true, "WWE12",
                    "OPTIONS menu: \"IN-GAME MANUAL\" becomes GRAPHICS, a settings screen.");
REXCVAR_DECLARE(uint32_t, user_language);
REXCVAR_DECLARE(int32_t, svr_render_scale);
REXCVAR_DECLARE(int32_t, bd_anisotropy);
REXCVAR_DECLARE(bool, bd_vsync);
REXCVAR_DECLARE(bool, wwe12_60fps);
REXCVAR_DECLARE(std::string, wwe12_button_icons);
REXCVAR_DECLARE(bool, wwe12_crowd);
REXCVAR_DECLARE(bool, fullscreen);
REXCVAR_DECLARE(bool, borderless);
REXCVAR_DECLARE(int32_t, window_width);
REXCVAR_DECLARE(int32_t, window_height);
REXCVAR_DECLARE(int32_t, wwe12_cpu_priority);
REXCVAR_DECLARE(bool, wwe12_quit_entry);
REXCVAR_DECLARE(bool, wwe12_shadows);
REXCVAR_DECLARE(bool, wwe12_dof);
REXCVAR_DECLARE(int32_t, wwe12_fps_limit);
REXCVAR_DECLARE(bool, wwe12_show_fps);

namespace {

// ---- texts ----------------------------------------------------------------------------------

enum Lang { kEn, kFr, kDe, kEs, kIt };

Lang CurrentLang() {
  switch (REXCVAR_GET(user_language)) {
  case 3: return kDe;
  case 4: return kFr;
  case 5: return kEs;
  case 6: return kIt;
  default: return kEn;
  }
}

// The manual entry's text in each language (string.pac), and what it becomes. The new text is
// shorter, so it is written in place.
struct EntryText {
  const char *manual;
  const char *graphics;
};
constexpr EntryText kEntryTexts[] = {
    {"IN-GAME MANUAL", "GRAPHICS"},
    {"COMMENT JOUER", "GRAPHISMES"},
    {"SPIELANLEITUNG", "GRAFIK"},
    {"MANUAL DEL JUEGO", "GR\xC3\x81" "FICOS"},
    {"INFORMAZIONI", "GRAFICA"},
};

// Upper case like the game's menus; header = the page band (the menu we come from), title = the
// centred subtitle.
struct Texts {
  const char *title, *resolution, *aniso, *vsync, *fps60;
  const char *off, *on, *yes, *no, *automatic;
  const char *next_start, *header, *change, *back, *fps60_note;
  const char *tab_display, *tab_graphics, *tab_advanced, *msaa, *shadow_res, *shadow_dist, *dof, *crowd,
      *edge_blur, *frame_limit, *show_fps, *none, *tabs;
  const char *mode, *mode_full, *mode_window, *mode_border, *win_size, *screen_size, *cpu, *prio_normal,
      *prio_above, *prio_high, *exit_entry;
};
constexpr Texts kTexts[] = {
    {"GRAPHICS", "INTERNAL RESOLUTION", "TEXTURE SHARPNESS", "VERTICAL SYNC",
     "60 FPS ENTRANCES", "OFF", "ON", "YES", "NO", "AUTO",
     "APPLIED THE NEXT TIME THE GAME STARTS", "OPTIONS", "CHANGE", "BACK",
     "EXPERIMENTAL: TITLE BELTS CAN MOVE ODDLY",
     "DISPLAY", "GRAPHICS", "ADVANCED", "ANTI-ALIASING", "SHADOWS", "SHADOW DISTANCE",
     "DEPTH OF FIELD", "CROWD", "EDGE BLUR", "FRAME LIMIT", "FPS COUNTER", "NONE", "TABS",
     "DISPLAY MODE", "FULL SCREEN", "WINDOW", "BORDERLESS WINDOW", "WINDOW SIZE", "SCREEN SIZE",
     "CPU PRIORITY", "NORMAL", "ABOVE NORMAL", "HIGH", "EXIT ENTRY IN THE MAIN MENU"},
    {"GRAPHISMES", "R\xC3\x89SOLUTION INTERNE", "NETTET\xC3\x89 DES TEXTURES", "SYNCHRO VERTICALE",
     "ENTR\xC3\x89" "ES \xC3\x80 60 FPS", "D\xC3\x89SACTIV\xC3\x89", "ACTIV\xC3\x89", "OUI", "NON",
     "AUTO", "APPLIQU\xC3\x89 AU PROCHAIN LANCEMENT DU JEU", "OPTIONS", "CHANGER", "RETOUR",
     "EXP\xC3\x89RIMENTAL : LES CEINTURES PEUVENT BOUGER BIZARREMENT",
     "AFFICHAGE", "GRAPHISMES", "AVANC\xC3\x89", "ANTICR\xC3\x89" "NELAGE", "OMBRES",
     "DISTANCE DES OMBRES", "PROFONDEUR DE CHAMP", "FOULE", "FLOU DES BORDS", "LIMITE D'IMAGES",
     "COMPTEUR FPS", "AUCUNE", "ONGLETS",
     "MODE D'AFFICHAGE", "PLEIN \xC3\x89" "CRAN", "FEN\xC3\x8A" "TRE", "FEN\xC3\x8A" "TRE SANS BORDURE",
     "TAILLE DE LA FEN\xC3\x8A" "TRE", "TAILLE DE L'\xC3\x89" "CRAN", "PRIORIT\xC3\x89 DU PROCESSEUR",
     "NORMALE", "SUP\xC3\x89" "RIEURE", "HAUTE", "ENTR\xC3\x89" "E QUITTER DANS LE MENU PRINCIPAL"},
    {"GRAFIK", "INTERNE AUFL\xC3\x96SUNG", "TEXTURSCH\xC3\x84RFE", "VERTIKALE SYNC.",
     "EINZ\xC3\x9CGE MIT 60 FPS", "AUS", "AN", "JA", "NEIN", "AUTO",
     "WIRD BEIM N\xC3\x84" "CHSTEN SPIELSTART \xC3\x9C" "BERNOMMEN", "OPTIONEN",
     "\xC3\x84NDERN", "ZUR\xC3\x9C" "CK",
     "EXPERIMENTELL: TITELG\xC3\x9CRTEL K\xC3\x96NNEN SICH SELTSAM BEWEGEN",
     "ANZEIGE", "GRAFIK", "ERWEITERT", "KANTENGL\xC3\x84" "TTUNG", "SCHATTEN",
     "SCHATTENDISTANZ", "TIEFENSCH\xC3\x84" "RFE", "PUBLIKUM", "KANTENWEICHZEICHNUNG", "BILDRATENLIMIT",
     "FPS-ANZEIGE", "KEINE", "REITER",
     "ANZEIGEMODUS", "VOLLBILD", "FENSTER", "RANDLOSES FENSTER", "FENSTERGR\xC3\x96" "SSE",
     "BILDSCHIRMGR\xC3\x96" "SSE", "CPU-PRIORIT\xC3\x84" "T", "NORMAL", "\xC3\x9C" "BER NORMAL", "HOCH",
     "EXIT-EINTRAG IM HAUPTMEN\xC3\x9C"},
    {"GR\xC3\x81" "FICOS", "RESOLUCI\xC3\x93N INTERNA", "NITIDEZ DE TEXTURAS",
     "SINCRONIZACI\xC3\x93N VERTICAL", "ENTRADAS A 60 FPS", "NO", "S\xC3\x8D", "S\xC3\x8D", "NO",
     "AUTO", "SE APLICA LA PR\xC3\x93XIMA VEZ QUE SE INICIE EL JUEGO", "OPCIONES", "CAMBIAR",
     "VOLVER", "EXPERIMENTAL: LOS CINTURONES PUEDEN MOVERSE DE FORMA EXTRA\xC3\x91" "A",
     "PANTALLA", "GR\xC3\x81" "FICOS", "AVANZADO", "ANTIALIASING", "SOMBRAS",
     "DISTANCIA DE SOMBRAS", "PROFUNDIDAD DE CAMPO", "P\xC3\x9A" "BLICO", "DESENFOQUE DE BORDES",
     "L\xC3\x8D" "MITE DE FPS", "CONTADOR DE FPS", "NINGUNO", "PESTA\xC3\x91" "AS",
     "MODO DE PANTALLA", "PANTALLA COMPLETA", "VENTANA", "VENTANA SIN BORDES", "TAMA\xC3\x91" "O DE VENTANA",
     "TAMA\xC3\x91" "O DE PANTALLA", "PRIORIDAD DE CPU", "NORMAL", "SUPERIOR", "ALTA",
     "ENTRADA SALIR EN EL MEN\xC3\x9A" " PRINCIPAL"},
    {"GRAFICA", "RISOLUZIONE INTERNA", "NITIDEZZA TEXTURE", "SINCRONIA VERTICALE",
     "INGRESSI A 60 FPS", "NO", "S\xC3\x8C", "S\xC3\x8C", "NO", "AUTO",
     "APPLICATO AL PROSSIMO AVVIO DEL GIOCO", "OPZIONI", "CAMBIA", "INDIETRO",
     "SPERIMENTALE: LE CINTURE POSSONO MUOVERSI IN MODO STRANO",
     "SCHERMO", "GRAFICA", "AVANZATE", "ANTIALIASING", "OMBRE", "DISTANZA OMBRE",
     "PROFONDIT\xC3\x80 DI CAMPO", "FOLLA", "SFOCATURA BORDI", "LIMITE FPS", "CONTATORE FPS", "NESSUNO",
     "SCHEDE",
     "MODALIT\xC3\x80 SCHERMO", "SCHERMO INTERO", "FINESTRA", "FINESTRA SENZA BORDI", "DIMENSIONE FINESTRA",
     "DIMENSIONE SCHERMO", "PRIORIT\xC3\x80 CPU", "NORMALE", "SUPERIORE", "ALTA", "VOCE ESCI NEL MENU PRINCIPALE"},
};

const Texts &T() { return kTexts[CurrentLang()]; }

// ---- state shared with the app --------------------------------------------------------------

rex::ui::ImGuiDrawer *g_drawer = nullptr;
std::function<void(std::function<void()>)> g_run_in_ui;
std::filesystem::path g_toml_path;
rex::input::InputSystem *g_input = nullptr;
std::atomic<bool> g_open{false};
ImFont *g_menu_font = nullptr;  // Impact, close to the menus' font (null: ImGui's default)

// ---- controller ----------------------------------------------------------------------------

// XInput read directly: the game's input is switched off while the screen is open.
struct XPad {
  WORD buttons = 0;
  SHORT ly = 0, lx = 0;
};
using XInputGetStateFn = DWORD(WINAPI *)(DWORD, void *);

XPad ReadPads() {
  static XInputGetStateFn get = [] {
    for (const char *dll : {"xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll"})
      if (HMODULE m = LoadLibraryA(dll))
        if (auto f = reinterpret_cast<XInputGetStateFn>(GetProcAddress(m, "XInputGetState")))
          return f;
    return XInputGetStateFn(nullptr);
  }();
  XPad all;
  // Every pad the game knows (PlayStation and other pads reach it through SDL, not XInput), read
  // through the input system, which hands out the real state even though the game's own input is
  // switched off while this screen is open.
  if (g_input) {
    for (uint32_t user = 0; user < 4; ++user) {
      rex::input::X_INPUT_STATE st = {};
      if (g_input->GetStateIgnoringActive(user, &st) != 0)
        continue;
      all.buttons |= WORD(uint16_t(st.gamepad.buttons));
      const SHORT lx = st.gamepad.thumb_lx, ly = st.gamepad.thumb_ly;
      if (std::abs(ly) > std::abs(all.ly))
        all.ly = ly;
      if (std::abs(lx) > std::abs(all.lx))
        all.lx = lx;
    }
  }
  if (!get)
    return all;
  struct {
    DWORD packet;
    WORD buttons;
    BYTE lt, rt;
    SHORT lx, ly, rx, ry;
  } st;
  for (DWORD i = 0; i < 4; ++i) {
    if (get(i, &st) != ERROR_SUCCESS)
      continue;
    all.buttons |= st.buttons;
    if (std::abs(st.ly) > std::abs(all.ly))
      all.ly = st.ly;
    if (std::abs(st.lx) > std::abs(all.lx))
      all.lx = st.lx;
  }
  return all;
}

enum Action { kNone, kUp, kDown, kLeft, kRight, kAccept, kBack, kTabPrev, kTabNext };

// Edge-triggered actions from keyboard and pads, with auto-repeat on held directions.
class InputReader {
 public:
  Action Poll() {
    const XPad p = ReadPads();
    constexpr SHORT kDead = 16000;
    bool held[9] = {};
    held[kUp] = (p.buttons & 0x0001) || p.ly > kDead || ImGui::IsKeyDown(ImGuiKey_UpArrow) ||
                ImGui::IsKeyDown(ImGuiKey_W) || ImGui::IsKeyDown(ImGuiKey_Z);
    held[kDown] = (p.buttons & 0x0002) || p.ly < -kDead ||
                  ImGui::IsKeyDown(ImGuiKey_DownArrow) || ImGui::IsKeyDown(ImGuiKey_S);
    held[kLeft] = (p.buttons & 0x0004) || p.lx < -kDead ||
                  ImGui::IsKeyDown(ImGuiKey_LeftArrow) || ImGui::IsKeyDown(ImGuiKey_A) ||
                  ImGui::IsKeyDown(ImGuiKey_Q);
    held[kRight] = (p.buttons & 0x0008) || p.lx > kDead ||
                   ImGui::IsKeyDown(ImGuiKey_RightArrow) || ImGui::IsKeyDown(ImGuiKey_D);
    held[kAccept] = (p.buttons & 0x1000) || ImGui::IsKeyDown(ImGuiKey_Enter) ||
                    ImGui::IsKeyDown(ImGuiKey_KeypadEnter) || ImGui::IsKeyDown(ImGuiKey_Space) ||
                    ImGui::IsKeyDown(ImGuiKey_K);
    held[kBack] = (p.buttons & 0x2000) || (p.buttons & 0x0010) ||
                  ImGui::IsKeyDown(ImGuiKey_Escape) || ImGui::IsKeyDown(ImGuiKey_Backspace) ||
                  ImGui::IsKeyDown(ImGuiKey_L);
    held[kTabPrev] = (p.buttons & 0x0100) || ImGui::IsKeyDown(ImGuiKey_PageUp);
    held[kTabNext] = (p.buttons & 0x0200) || ImGui::IsKeyDown(ImGuiKey_PageDown);
    const double now = ImGui::GetTime();
    Action result = kNone;
    for (int a = kUp; a <= kTabNext; ++a) {
      if (held[a] && !was_[a]) {
        if (armed_[a])
          result = Action(a);
        next_repeat_[a] = now + 0.40;
      } else if (held[a] && a <= kRight && armed_[a] && now >= next_repeat_[a]) {
        result = Action(a);
        next_repeat_[a] = now + 0.12;
      }
      // A button already down when the screen opens (the A that chose the entry) is ignored
      // until it is released.
      if (!held[a])
        armed_[a] = true;
      was_[a] = held[a];
    }
    return result;
  }

 private:
  bool was_[9] = {};
  bool armed_[9] = {};
  double next_repeat_[9] = {};
};

// ---- settings -------------------------------------------------------------------------------

struct Setting {
  const char *Texts::*label;
  std::vector<std::string> values;
  int index = 0;
  bool next_start = false;
  int tab = 0;  // 0 display, 1 graphics, 2 advanced
  bool touched = false;
  const char *Texts::*note = nullptr;
  std::function<void(int)> apply;  // sets the cvar
  std::function<void(TomlFile &, int)> save;
};

std::vector<Setting> BuildSettings() {
  const Texts &t = T();
  std::vector<Setting> s;
  {
    Setting r;
    r.label = &Texts::resolution;
    // svr_render_scale holds a height (720, 1080, 1440, 2160) or, from older files, a multiple.
    static constexpr int kHeights[] = {0, 540, 720, 1080, 1440, 2160};
    r.values = {t.automatic, "540P", "720P", "1080P", "1440P", "4K"};
    switch (REXCVAR_GET(svr_render_scale)) {
    case 540: r.index = 1; break;
    case 1: case 720: r.index = 2; break;
    case 1080: r.index = 3; break;
    case 2: case 1440: r.index = 4; break;
    case 3: case 4: case 2160: case 2880: r.index = 5; break;
    default: r.index = 0; break;
    }
    r.next_start = true;
    r.tab = 0;
    r.apply = [](int i) {
      rex::cvar::SetFlagByName("svr_render_scale", std::to_string(kHeights[i]));
    };
    r.save = [](TomlFile &f, int i) { f.SetInt("svr_render_scale", kHeights[i]); };
    s.push_back(std::move(r));
  }
  {
    static constexpr int kAniso[] = {0, 2, 4, 8, 16};
    Setting a;
    a.label = &Texts::aniso;
    a.tab = 1;
    a.values = {t.off, "2X", "4X", "8X", "16X"};
    const int cur = REXCVAR_GET(bd_anisotropy);
    a.index = 0;
    for (int i = 0; i < 5; ++i)
      if (kAniso[i] <= cur)
        a.index = i;
    a.apply = [](int i) { rex::cvar::SetFlagByName("bd_anisotropy", std::to_string(kAniso[i])); };
    a.save = [](TomlFile &f, int i) { f.SetInt("bd_anisotropy", kAniso[i]); };
    s.push_back(std::move(a));
  }
  {
    Setting v;
    v.label = &Texts::vsync;
    v.tab = 0;
    v.values = {t.no, t.yes};
    v.index = REXCVAR_GET(bd_vsync) ? 1 : 0;
    v.apply = [](int i) { rex::cvar::SetFlagByName("bd_vsync", i ? "true" : "false"); };
    v.save = [](TomlFile &f, int i) { f.SetBool("bd_vsync", i != 0); };
    s.push_back(std::move(v));
  }
  {
    Setting f60;
    f60.label = &Texts::fps60;
    f60.tab = 2;
    f60.values = {t.no, t.yes};
    f60.index = REXCVAR_GET(wwe12_60fps) ? 1 : 0;
    f60.apply = [](int i) { rex::cvar::SetFlagByName("wwe12_60fps", i ? "true" : "false"); };
    f60.save = [](TomlFile &f, int i) { f.SetBool("wwe12_60fps", i != 0); };
    s.push_back(std::move(f60));
  }
  // Frame limit and FPS counter (display tab).
  {
    Setting fl;
    fl.label = &Texts::frame_limit;
    fl.tab = 0;
    fl.values = {t.none, "60"};
    fl.index = REXCVAR_GET(wwe12_fps_limit) == 60 ? 1 : 0;
    fl.apply = [](int i) { rex::cvar::SetFlagByName("wwe12_fps_limit", i ? "60" : "0"); };
    fl.save = [](TomlFile &f, int i) { f.SetInt("wwe12_fps_limit", i ? 60 : 0); };
    s.push_back(std::move(fl));
    Setting sf;
    sf.label = &Texts::show_fps;
    sf.tab = 2;
    sf.values = {t.no, t.yes};
    sf.index = REXCVAR_GET(wwe12_show_fps) ? 1 : 0;
    sf.apply = [](int i) { rex::cvar::SetFlagByName("wwe12_show_fps", i ? "true" : "false"); };
    sf.save = [](TomlFile &f, int i) { f.SetBool("wwe12_show_fps", i != 0); };
    s.push_back(std::move(sf));
  }
  // Graphics tab.
  {
    Setting c;
    c.label = &Texts::crowd;
    c.tab = 1;
    c.values = {t.no, t.yes};
    c.index = REXCVAR_GET(wwe12_crowd) ? 1 : 0;
    c.apply = [](int i) { rex::cvar::SetFlagByName("wwe12_crowd", i ? "true" : "false"); };
    c.save = [](TomlFile &f, int i) { f.SetBool("wwe12_crowd", i != 0); };
    s.push_back(std::move(c));

    Setting sh;
    sh.label = &Texts::shadow_res;
    sh.tab = 1;
    sh.values = {t.no, t.yes};
    sh.index = REXCVAR_GET(wwe12_shadows) ? 1 : 0;
    sh.apply = [](int i) { rex::cvar::SetFlagByName("wwe12_shadows", i ? "true" : "false"); };
    sh.save = [](TomlFile &f, int i) { f.SetBool("wwe12_shadows", i != 0); };
    s.push_back(std::move(sh));

    Setting d;
    d.label = &Texts::dof;
    d.tab = 1;
    d.values = {t.no, t.yes};
    d.index = REXCVAR_GET(wwe12_dof) ? 1 : 0;
    d.apply = [](int i) { rex::cvar::SetFlagByName("wwe12_dof", i ? "true" : "false"); };
    d.save = [](TomlFile &f, int i) { f.SetBool("wwe12_dof", i != 0); };
    s.push_back(std::move(d));
  }
  // Display mode and window size (next start), CPU priority, EXIT entry (next start).
  {
    Setting m;
    m.label = &Texts::mode;
    m.tab = 0;
    m.next_start = true;
    m.values = {t.mode_full, t.mode_window, t.mode_border};
    m.index = REXCVAR_GET(fullscreen) ? 0 : (REXCVAR_GET(borderless) ? 2 : 1);
    m.apply = [](int i) {
      rex::cvar::SetFlagByName("fullscreen", i == 0 ? "true" : "false");
      rex::cvar::SetFlagByName("borderless", i == 2 ? "true" : "false");
    };
    m.save = [](TomlFile &f, int i) {
      f.SetBool("fullscreen", i == 0);
      f.SetBool("borderless", i == 2);
    };
    s.push_back(std::move(m));

    static const int kSize[6][2] = {{0, 0},       {1280, 720},  {1600, 900}, {1920, 1080},
                                    {2560, 1440}, {GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)}};
    static const std::string kScreen = std::to_string(kSize[5][0]) + "X" + std::to_string(kSize[5][1]);
    Setting w;
    w.label = &Texts::win_size;
    w.tab = 0;
    w.next_start = true;
    w.values = {t.automatic, "1280X720", "1600X900", "1920X1080", "2560X1440",
                std::string(t.screen_size) + " (" + kScreen + ")"};
    w.index = 0;
    for (int i = 0; i < 6; ++i)
      if (kSize[i][0] == REXCVAR_GET(window_width) && kSize[i][1] == REXCVAR_GET(window_height))
        w.index = i;
    w.apply = [](int i) {
      rex::cvar::SetFlagByName("window_width", std::to_string(kSize[i][0]));
      rex::cvar::SetFlagByName("window_height", std::to_string(kSize[i][1]));
    };
    w.save = [](TomlFile &f, int i) {
      f.SetInt("window_width", kSize[i][0]);
      f.SetInt("window_height", kSize[i][1]);
    };
    s.push_back(std::move(w));

    Setting c;
    c.label = &Texts::cpu;
    c.tab = 2;
    c.next_start = true;
    c.values = {t.prio_normal, t.prio_above, t.prio_high};
    c.index = std::clamp(int(REXCVAR_GET(wwe12_cpu_priority)), 0, 2);
    c.apply = [](int i) { rex::cvar::SetFlagByName("wwe12_cpu_priority", std::to_string(i)); };
    c.save = [](TomlFile &f, int i) { f.SetInt("wwe12_cpu_priority", i); };
    s.push_back(std::move(c));

    Setting e;
    e.label = &Texts::exit_entry;
    e.tab = 2;
    e.next_start = true;
    e.values = {t.no, t.yes};
    e.index = REXCVAR_GET(wwe12_quit_entry) ? 1 : 0;
    e.apply = [](int i) { rex::cvar::SetFlagByName("wwe12_quit_entry", i ? "true" : "false"); };
    e.save = [](TomlFile &f, int i) { f.SetBool("wwe12_quit_entry", i != 0); };
    s.push_back(std::move(e));
  }
  // Same order as the launcher's tabs.
  {
    const std::vector<const char *Texts::*> order = {
        &Texts::mode,    &Texts::win_size, &Texts::resolution, &Texts::frame_limit, &Texts::vsync,
        &Texts::aniso,   &Texts::crowd,    &Texts::shadow_res, &Texts::dof,         &Texts::cpu,
        &Texts::fps60,   &Texts::exit_entry, &Texts::show_fps};
    const auto rank = [&](const Setting &x) {
      for (size_t i = 0; i < order.size(); ++i)
        if (order[i] == x.label)
          return int(i);
      return 99;
    };
    std::stable_sort(s.begin(), s.end(),
                     [&](const Setting &a, const Setting &b) { return rank(a) < rank(b); });
  }
  return s;
}

// ---- the screen -----------------------------------------------------------------------------

class GraphicsMenu final : public rex::ui::ImGuiDialog {
 public:
  explicit GraphicsMenu(rex::ui::ImGuiDrawer *drawer)
      : ImGuiDialog(drawer), settings_(BuildSettings()) {}

 protected:
  void OnClose() override {
    Save();
    g_open = false;
    BD_INFO("[graphics] screen closed");
  }

  void OnDraw(ImGuiIO &io) override {
    const std::vector<int> rows = Rows();
    int &selected = selected_[tab_];
    selected = std::min(selected, int(rows.size()) - 1);
    switch (input_.Poll()) {
    case kUp: selected = (selected + int(rows.size()) - 1) % int(rows.size()); break;
    case kDown: selected = (selected + 1) % int(rows.size()); break;
    case kTabPrev: tab_ = (tab_ + 2) % 3; return;
    case kTabNext: tab_ = (tab_ + 1) % 3; return;
    case kLeft: Change(-1); break;
    case kRight:
    case kAccept: Change(+1); break;
    case kBack: Close(); return;
    default: break;
    }

    // Laid out like the game's option pages (GAMEPLAY > IN-GAME OPTIONS), in 1080p coordinates
    // scaled to the window: orange page band, white rule, centred title, rows of large italic
    // capitals with the chosen one red and underlined, red arrows around its value, and the
    // A/B prompts at the bottom.
    const Texts &t = T();
    const ImVec2 screen = io.DisplaySize;
    const float k = screen.y / 1080.0f;
    const float ox = (screen.x - 1920.0f * k) * 0.5f;  // centred 16:9 area on wider windows
    const auto P = [&](float x, float y) { return ImVec2(ox + x * k, y * k); };
    ImFont *font = g_menu_font ? g_menu_font : ImGui::GetFont();
    ImDrawList *dl = ImGui::GetForegroundDrawList();

    // The menu underneath disappears behind a dark lower half; the artwork above stays.
    // Opaque, so nothing of the menu (its "MAIN MENU" label, list and prompts) shows through.
    dl->AddRectFilledMultiColor(ImVec2(0, 470 * k), ImVec2(screen.x, 530 * k),
                                IM_COL32(6, 6, 6, 0), IM_COL32(6, 6, 6, 0),
                                IM_COL32(6, 6, 6, 255), IM_COL32(6, 6, 6, 255));
    dl->AddRectFilled(ImVec2(0, 530 * k), screen, IM_COL32(6, 6, 6, 255));

    // Page band (orange, fading out on the right) with the parent menu's name in black.
    const ImU32 orange = IM_COL32(240, 172, 48, 255);
    const ImU32 clear_orange = IM_COL32(240, 172, 48, 0);
    dl->AddRectFilled(P(205, 580), P(980, 624), orange);
    dl->AddRectFilledMultiColor(P(980, 580), P(1120, 624), orange, clear_orange, clear_orange,
                                orange);
    Text(dl, font, 32 * k, P(228, 584), IM_COL32(20, 14, 6, 255), t.header);
    // White rule under the band, fading at both ends.
    const ImU32 white = IM_COL32(255, 255, 255, 255);
    const ImU32 clear_white = IM_COL32(255, 255, 255, 0);
    dl->AddRectFilledMultiColor(P(190, 624), P(400, 628), clear_white, white, white, clear_white);
    dl->AddRectFilled(P(400, 624), P(1520, 628), white);
    dl->AddRectFilledMultiColor(P(1520, 624), P(1730, 628), white, clear_white, clear_white,
                                white);

    // Centred subtitle like "IN-GAME OPTIONS": the open tab's name, with red arrows either side
    // (L1/R1 or LB/RB switch tabs) and its place among the tabs.
    {
      const char *tab_names[] = {t.tab_display, t.tab_graphics, t.tab_advanced};
      const std::string title = tab_names[tab_];
      const ImVec2 sz = font->CalcTextSizeA(34 * k, FLT_MAX, 0.0f, title.c_str());
      const float half = sz.x * 0.5f / k;
      TextCentered(dl, font, 34 * k, P(960, 640), white, title.c_str());
      const float cy = 662 * k;
      const ImU32 red_arrow = IM_COL32(214, 22, 32, 255);
      dl->AddTriangleFilled(ImVec2(ox + (960 - half - 30) * k, cy),
                            ImVec2(ox + (960 - half - 14) * k, cy - 9 * k),
                            ImVec2(ox + (960 - half - 14) * k, cy + 9 * k), red_arrow);
      dl->AddTriangleFilled(ImVec2(ox + (960 + half + 30) * k, cy),
                            ImVec2(ox + (960 + half + 14) * k, cy - 9 * k),
                            ImVec2(ox + (960 + half + 14) * k, cy + 9 * k), red_arrow);
      // One dot per tab under the title.
      for (int i = 0; i < 3; ++i)
        dl->AddCircleFilled(ImVec2(ox + (960 + (i - 1) * 22) * k, 684 * k), 5 * k,
                            i == tab_ ? orange : IM_COL32(90, 90, 94, 255));
    }

    // Rows.
    const ImU32 red = IM_COL32(214, 22, 32, 255);
    float y = 700;
    for (int i = 0; i < int(rows.size()); ++i) {
      const Setting &s = settings_[rows[i]];
      const bool sel = i == selected;
      const ImU32 col = sel ? red : white;
      Text(dl, font, 42 * k, P(295, y), col, t.*s.label);
      std::string value = s.values[s.index];
      if (s.next_start)
        value += " *";
      TextCentered(dl, font, 42 * k, P(1380, y), col, value.c_str());
      if (sel) {
        // Red arrows either side of the value.
        const float cy = (y + 24) * k;
        dl->AddTriangleFilled(ImVec2(ox + 1184 * k, cy), ImVec2(ox + 1204 * k, cy - 11 * k),
                              ImVec2(ox + 1204 * k, cy + 11 * k), red);
        dl->AddTriangleFilled(ImVec2(ox + 1576 * k, cy), ImVec2(ox + 1556 * k, cy - 11 * k),
                              ImVec2(ox + 1556 * k, cy + 11 * k), red);
      }
      // Separator; the chosen row's is red.
      dl->AddRectFilled(P(240, y + 46), P(1665, y + 48), sel ? red : IM_COL32(70, 70, 74, 255));
      y += 48;
    }

    // Note for the chosen row (next-start setting, experimental option).
    const Setting &cur = settings_[rows[selected]];
    const char *note = cur.note ? t.*cur.note : (cur.next_start ? t.next_start : nullptr);
    if (note)
      Text(dl, font, 24 * k, P(295, y + 2), IM_COL32(240, 196, 64, 255),
           (std::string(cur.next_start ? "* " : "") + note).c_str());

    // Button prompts, like the game's: round buttons and yellow words. The icons follow
    // wwe12_button_icons (PlayStation icons when it is "playstation").
    const ImU32 yellow = IM_COL32(242, 200, 72, 255);
    const bool ps = REXCVAR_GET(wwe12_button_icons) == "playstation";
    Button(dl, font, k, P(228, 984), ps ? kBtnCross : kBtnA, ps, "A");
    Text(dl, font, 30 * k, P(252, 966), yellow, t.change);
    Button(dl, font, k, P(482, 984), ps ? kBtnCircle : kBtnB, ps, "B");
    Text(dl, font, 30 * k, P(506, 966), yellow, t.back);
    Text(dl, font, 30 * k, P(700, 966), yellow,
         (std::string(ps ? "L1 / R1  " : "LB / RB  ") + t.tabs).c_str());
  }

  static constexpr ImU32 kBtnA = IM_COL32(60, 170, 50, 255);
  static constexpr ImU32 kBtnB = IM_COL32(210, 30, 40, 255);
  static constexpr ImU32 kBtnCross = IM_COL32(150, 160, 240, 255);
  static constexpr ImU32 kBtnCircle = IM_COL32(225, 70, 90, 255);

  // Italic text: the glyphs' vertices are sheared, as the menus' font slants.
  static void Text(ImDrawList *dl, ImFont *font, float size, ImVec2 pos, ImU32 col,
                   const char *text) {
    const int first = dl->VtxBuffer.Size;
    dl->AddText(font, size, pos, col, text);
    const float base = pos.y + size;
    for (int v = first; v < dl->VtxBuffer.Size; ++v)
      dl->VtxBuffer[v].pos.x += (base - dl->VtxBuffer[v].pos.y) * 0.16f;
  }
  static void TextCentered(ImDrawList *dl, ImFont *font, float size, ImVec2 center_top,
                           ImU32 col, const char *text) {
    const ImVec2 sz = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
    Text(dl, font, size, ImVec2(center_top.x - sz.x * 0.5f, center_top.y), col, text);
  }
  static void Button(ImDrawList *dl, ImFont *font, float k, ImVec2 center, ImU32 col, bool ps,
                     const char *letter) {
    if (ps) {
      // PlayStation face button: a dark disc with a coloured cross (A) or ring (B).
      dl->AddCircleFilled(center, 15 * k, IM_COL32(25, 25, 40, 255));
      dl->AddCircle(center, 15 * k, IM_COL32(120, 120, 140, 255), 0, 1.5f * k);
      if (letter[0] == 'A') {
        const float d = 8 * k;
        dl->AddLine(ImVec2(center.x - d, center.y - d), ImVec2(center.x + d, center.y + d), col, 3 * k);
        dl->AddLine(ImVec2(center.x - d, center.y + d), ImVec2(center.x + d, center.y - d), col, 3 * k);
      } else {
        dl->AddCircle(center, 8 * k, col, 0, 3 * k);
      }
      return;
    }
    dl->AddCircleFilled(center, 15 * k, IM_COL32(255, 255, 255, 255));
    dl->AddCircleFilled(center, 12.5f * k, col);
    const ImVec2 sz = font->CalcTextSizeA(20 * k, FLT_MAX, 0.0f, letter);
    dl->AddText(font, 20 * k, ImVec2(center.x - sz.x * 0.5f, center.y - sz.y * 0.5f),
                IM_COL32(255, 255, 255, 255), letter);
  }

 private:
  std::vector<int> Rows() const {
    std::vector<int> r;
    for (int i = 0; i < int(settings_.size()); ++i)
      if (settings_[i].tab == tab_)
        r.push_back(i);
    return r;
  }

  void Change(int delta) {
    const std::vector<int> rows = Rows();
    Setting &s = settings_[rows[std::min(selected_[tab_], int(rows.size()) - 1)]];
    s.touched = true;
    const int n = int(s.values.size());
    s.index = (s.index + delta + n) % n;
    s.apply(s.index);
    dirty_ = true;
  }

  void Save() {
    if (!dirty_ || g_toml_path.empty())
      return;
    TomlFile f;
    if (!f.Load(g_toml_path)) {
      BD_WARN("[graphics] cannot read {}", rex::path_to_utf8(g_toml_path));
      return;
    }
    for (const Setting &s : settings_)
      if (s.touched)
        s.save(f, s.index);
    if (f.Save())
      BD_INFO("[graphics] settings saved to {}", rex::path_to_utf8(g_toml_path));
    else
      BD_WARN("[graphics] cannot write {}", rex::path_to_utf8(g_toml_path));
  }

  std::vector<Setting> settings_;
  int selected_[3] = {};
  int tab_ = 0;
  bool dirty_ = false;
  InputReader input_;
};

}  // namespace

void Wwe12GraphicsMenuInit(rex::ui::ImGuiDrawer *drawer,
                           std::function<void(std::function<void()>)> run_in_ui_thread,
                           std::filesystem::path toml_path) {
  g_drawer = drawer;
  g_run_in_ui = std::move(run_in_ui_thread);
  g_toml_path = std::move(toml_path);
}

void Wwe12GraphicsMenuConfigureFonts(ImFontAtlas *atlas) {
  // Impact ships with Windows; elsewhere (Steam Deck) ImGui's default font is used.
  char windir[MAX_PATH] = {};
  if (!GetWindowsDirectoryA(windir, MAX_PATH))
    return;
  const std::filesystem::path path = std::filesystem::path(windir) / "Fonts" / "impact.ttf";
  std::error_code ec;
  if (!std::filesystem::exists(path, ec))
    return;
  ImFontConfig cfg;
  cfg.OversampleH = cfg.OversampleV = 2;
  static const ImWchar kRanges[] = {0x0020, 0x00FF, 0};  // Latin-1: the accented capitals
  g_menu_font = atlas->AddFontFromFileTTF(rex::path_to_utf8(path).c_str(), 64.0f, &cfg, kRanges);
}

void Wwe12GraphicsMenuSetInput(rex::input::InputSystem *input) { g_input = input; }

bool Wwe12GraphicsMenuIsOpen() { return g_open.load(std::memory_order_relaxed); }

void Wwe12GraphicsMenuRequest() {
  if (!g_drawer || !g_run_in_ui || g_open.exchange(true))
    return;
  BD_INFO("[graphics] screen opened");
  g_run_in_ui([] { new GraphicsMenu(g_drawer); });  // deletes itself when closed
}

uint32_t Wwe12RelabelGraphicsEntry(uint8_t *region_start, uint8_t *region_end) {
  if (!REXCVAR_GET(wwe12_graphics_menu))
    return 0;
  // Only the loaded language's text: the tables in memory are the player's language (plus
  // English, which some screens keep).
  const EntryText &lang = kEntryTexts[CurrentLang()];
  uint32_t patched = 0;
  for (const EntryText *e : {&lang, &kEntryTexts[kEn]}) {
    const size_t len = std::strlen(e->manual);
    for (uint8_t *t = region_start; t + len + 1 <= region_end;) {
      t = std::search(t, region_end - len, reinterpret_cast<const uint8_t *>(e->manual),
                      reinterpret_cast<const uint8_t *>(e->manual) + len);
      if (t >= region_end - len)
        break;
      if (t[len] != 0 || (t > region_start && t[-1] != 0)) {
        ++t;
        continue;
      }
      // The table entry {text offset, length with NUL, 41184, 0}, little- or big-endian, before
      // the text; the table itself starts with a u32 0.
      bool hit = false;
      uint8_t *const first = reinterpret_cast<uint8_t *>((uintptr_t(t) - 16) & ~uintptr_t(3));
      for (uint8_t *p = first; p >= region_start && p > t - 0x40000 && !hit; p -= 4) {
        const uint32_t *w = reinterpret_cast<const uint32_t *>(p);
        for (int big = 0; big < 2 && !hit; ++big) {
          const auto rd = [&](int i) { return big ? _byteswap_ulong(w[i]) : w[i]; };
          if (rd(2) != kWwe12GraphicsStringId || rd(1) != len + 1 || rd(3) != 0)
            continue;
          uint8_t *table = t - rd(0);
          hit = table >= region_start && table <= p &&
                *reinterpret_cast<const uint32_t *>(table) == 0;
        }
      }
      if (hit) {
        std::memset(t, 0, len);
        std::memcpy(t, e->graphics, std::min(std::strlen(e->graphics), len));
        ++patched;
      }
      t += len;
    }
    if (&lang == &kEntryTexts[kEn])
      break;
  }
  return patched;
}

// ==== The native GRAPHICS pages (wwe12_graphics_native) ====================================
//
// The same settings in the game's own menus (the user wants real game menus, not a screen drawn
// over the game). Found on 2026-10-20:
// - OPTIONS's IN-GAME MANUAL entry (string 41184, relabelled GRAPHICS) is a sub-menu of menu.pac
//   (record kind 1); its first three entries become DISPLAY / GRAPHICS / ADVANCED (texts
//   kNativeTabText + tab), each linked to 1304 (wwe12_menu_patch.cpp), the link of GAMEPLAY >
//   IN-GAME OPTIONS: sub_825470B0(1304) makes the option page sub_8255E7C0(9), which runs the page
//   script "/MENS/0014" of MScript.pac.
// - Option pages are Lua 5.1 scripts, compiled (numbers as 4-byte floats). The game's Lua has a
//   source parser too, but it crashes (tested: a guest access violation in lcode's jump lists), so
//   our pages are compiled in advance by tools/graphics_pages.py (graphics_pages/*.lua ->
//   wwe12_graphics_pages.inc). The page loads its script with luaL_loadbuffer
//   (sub_827E9418(L, buffer, size, name), called by sub_824CFD08): when one of our entries was
//   chosen (the decide hook in wwe12_quit.cpp tells us which), the script is replaced by ours,
//   written from the original's logic (frames DrawIngameOptions: 4 rows, DrawRTWMOptions: 5 rows,
//   of the layout "LOPN").
// - Values: the page's script calls yOption.InitInGameOptions(n), then reads the rows with
//   yFrame.Get32BitArray(i) (value i, enabled N+i, maximum 2N+i); on A it writes them back with
//   Set32BitArray(i, v) and calls yOption.UpdateInGameOptions(n). The array is the current page's
//   words 852+i (page = [0x8317C430 + 4 * [0x8306B718]]). Our script passes n = 100 + tab, which
//   the game ignores; the hooks of those two natives (sub_82DEDD70, sub_82DEC528) fill the array
//   from the cvars and apply the chosen values, then save wwe12.toml (changed rows only).
// - Texts: titles, labels and values are fixed text ids from 43100 (unused in every string table;
//   layout in tools/graphics_pages.py), served in the player's language by the string manager hook
//   (wwe12_universe.cpp asks Wwe12NativeGraphicsGuestText).

REXCVAR_DEFINE_BOOL(wwe12_graphics_native, false, "WWE12",
                    "OPTIONS > GRAPHICS in the game's own menus (DISPLAY / GRAPHICS / ADVANCED pages "
                    "of the game) instead of the screen drawn over the game.");

#include <map>

#include <rex/runtime.h>

#include "core/memory_helpers.h"
#include "svr_hook.h"

REX_EXTERN(__imp__sub_827E74A8);  // lua_tointeger (L, index)

namespace {

constexpr u32 kNativeTabText = 43100;   // 43100-43102: the three entries (fixed: menu.pac)
// 43110-43149: the rows' labels, 43150-43299: their values (tools/graphics_pages.py)
constexpr u32 kNativeTextEnd = 43300;
constexpr int kNativePage = 100;        // InitInGameOptions(100 + tab)
constexpr u32 kIngameOptionsScriptSize = 5158;  // "/MENS/0014", the script we replace

int g_pending_tab = -1;  // an entry was chosen, its page script not loaded yet
int g_native_tab = -1;   // the page shown
std::vector<Setting> g_native_settings;
std::vector<int> g_native_rows;  // indices into g_native_settings, for g_native_tab

#include "wwe12_graphics_pages.inc"

struct NativePage {
  const uint8_t *script;
  size_t size;
  const int *values;  // number of values of each row
  int rows;
};
const NativePage kNativePages[] = {
    {kGraphicsPage0, sizeof(kGraphicsPage0), kGraphicsPage0Values, int(std::size(kGraphicsPage0Values))},
    {kGraphicsPage1, sizeof(kGraphicsPage1), kGraphicsPage1Values, int(std::size(kGraphicsPage1Values))},
    {kGraphicsPage2, sizeof(kGraphicsPage2), kGraphicsPage2Values, int(std::size(kGraphicsPage2Values))},
};

const char *TabName(int tab) {
  const Texts &t = T();
  return tab == 0 ? t.tab_display : tab == 1 ? t.tab_graphics : t.tab_advanced;
}

// The settings of one tab (g_native_settings / g_native_rows), checked against the page script
// (same rows, same number of values: tools/graphics_pages.py).
bool LoadNativeSettings(int tab) {
  g_native_settings = BuildSettings();
  g_native_rows.clear();
  for (int i = 0; i < int(g_native_settings.size()); ++i)
    if (g_native_settings[i].tab == tab)
      g_native_rows.push_back(i);
  const NativePage &page = kNativePages[tab];
  bool same = int(g_native_rows.size()) == page.rows;
  for (int r = 0; same && r < page.rows; ++r)
    same = int(g_native_settings[g_native_rows[r]].values.size()) == page.values[r];
  if (!same)
    BD_WARN("[graphics] native page {}: the settings do not match the page script "
            "(tools/graphics_pages.py), page not replaced", tab);
  return same;
}

// The text of one of our ids (see tools/graphics_pages.py), in the current language.
std::string NativeText(u32 id) {
  if (id >= kNativeTabText && id < kNativeTabText + 3)
    return TabName(int(id - kNativeTabText));
  const int tab = g_native_tab;
  if (tab < 0 || g_native_rows.empty())
    return {};
  const Texts &t = T();
  if (id >= 43110 && id < 43150) {
    const int r = int(id - 43110) - tab * 10;
    if (r < 0 || r >= int(g_native_rows.size()))
      return {};
    const Setting &s = g_native_settings[g_native_rows[r]];
    return std::string(t.*s.label) + (s.next_start ? " *" : "");
  }
  const int k = int(id - 43150) - tab * 50;
  const int r = k / 8, v = k % 8;
  if (k < 0 || r >= int(g_native_rows.size()))
    return {};
  const Setting &s = g_native_settings[g_native_rows[r]];
  return v < int(s.values.size()) ? s.values[v] : std::string();
}

// The current option page's 32-bit array (yFrame.Get32BitArray / Set32BitArray).
u32 PageArray() {
  const u32 index = bd::mem::load<u32>(0x8306B718);
  const u32 page = bd::mem::load<u32>(0x8317C430 + index * 4);
  return page ? page + 852 * 4 : 0;
}

void SaveNativeSettings() {
  if (g_toml_path.empty())
    return;
  TomlFile f;
  if (!f.Load(g_toml_path)) {
    BD_WARN("[graphics] cannot read {}", rex::path_to_utf8(g_toml_path));
    return;
  }
  for (const Setting &s : g_native_settings)
    if (s.touched)
      s.save(f, s.index);
  if (f.Save())
    BD_INFO("[graphics] native page: settings saved to {}", rex::path_to_utf8(g_toml_path));
}

// The page number our script passed (100 + tab), or -1. Natives get the Lua state in r3; the
// argument is read with lua_tointeger(L, -1), as the game's own natives do.
int NativePageArg(const PPCContext &ctx, u8 *base) {
  if (!REXCVAR_GET(wwe12_graphics_native) || g_native_tab < 0)
    return -1;
  PPCContext x = ctx;
  x.r4.u64 = ~uint64_t(0);  // index -1: the top of the Lua stack
  __imp__sub_827E74A8(x, base);
  const int arg = int(x.r3.u32);
  return arg >= kNativePage && arg < kNativePage + 3 ? arg - kNativePage : -1;
}

}  // namespace

void Wwe12GraphicsNativeDecide(uint32_t label) {
  if (!REXCVAR_GET(wwe12_graphics_native))
    return;
  if (label >= kNativeTabText && label < kNativeTabText + 3) {
    g_pending_tab = int(label - kNativeTabText);
    BD_INFO("[graphics] native page {} chosen", g_pending_tab);
  }
}

bool Wwe12GraphicsNativeEnabled() { return REXCVAR_GET(wwe12_graphics_native); }

uint32_t Wwe12NativeGraphicsGuestText(uint32_t id) {
  if (id < kNativeTabText || id >= kNativeTextEnd || !REXCVAR_GET(wwe12_graphics_native))
    return 0;
  const std::string text = NativeText(id);
  if (text.empty())
    return 0;
  // One 128-byte guest slot per id, rewritten when its text changes (language).
  constexpr u32 kSlot = 128;
  static u32 s_block = 0;
  static std::map<u32, std::string> s_written;
  if (!s_block)
    s_block = REX_KERNEL_MEMORY()->SystemHeapAlloc((kNativeTextEnd - kNativeTabText) * kSlot, 0x20);
  if (!s_block)
    return 0;
  const u32 at = s_block + (id - kNativeTabText) * kSlot;
  if (s_written[id] != text) {
    std::memset(bd::mem::at<char>(at), 0, kSlot);
    std::memcpy(bd::mem::at<char>(at), text.data(), std::min<size_t>(text.size(), kSlot - 1));
    s_written[id] = text;
  }
  return at;
}

// luaL_loadbuffer (L, buffer, size, name): our page script instead of IN-GAME OPTIONS's.
REX_EXTERN(__imp__sub_827E9418);
extern "C" REX_FUNC(sub_827E9418) {
  if (g_pending_tab >= 0 && REXCVAR_GET(wwe12_graphics_native) &&
      ctx.r5.u32 == kIngameOptionsScriptSize && bd::mem::try_load<u8>(ctx.r4.u32) == 0x1B) {
    const int tab = g_pending_tab;
    g_pending_tab = -1;
    if (LoadNativeSettings(tab)) {
      const NativePage &page = kNativePages[tab];
      static u32 s_buffers[3] = {};
      if (!s_buffers[tab])
        s_buffers[tab] = REX_KERNEL_MEMORY()->SystemHeapAlloc(u32(page.size), 0x20);
      if (s_buffers[tab]) {
        std::memcpy(bd::mem::at<u8>(s_buffers[tab]), page.script, page.size);
        ctx.r4.u64 = s_buffers[tab];
        ctx.r5.u64 = page.size;
        g_native_tab = tab;
        BD_INFO("[graphics] native page {}: script of {} bytes, {} rows", tab, page.size, page.rows);
      }
    }
  }
  __imp__sub_827E9418(ctx, base);
}

// yOption.InitInGameOptions(n): for our pages, the rows' values from the cvars.
REX_EXTERN(__imp__sub_82DEDD70);
extern "C" REX_FUNC(sub_82DEDD70) {
  const int tab = NativePageArg(ctx, base);
  if (tab < 0 || tab != g_native_tab) {
    __imp__sub_82DEDD70(ctx, base);
    return;
  }
  const u32 array = PageArray();
  const u32 n = u32(g_native_rows.size());
  if (array)
    for (u32 r = 0; r < n; ++r) {
      const Setting &s = g_native_settings[g_native_rows[r]];
      bd::mem::store<u32>(array + r * 4, u32(s.index));
      bd::mem::store<u32>(array + (n + r) * 4, 1);
      bd::mem::store<u32>(array + (2 * n + r) * 4, u32(s.values.size() - 1));
    }
  BD_INFO("[graphics] native page {}: {} rows read from the settings", tab, n);
  ctx.r3.u64 = 0;  // no results for Lua
}

// yOption.UpdateInGameOptions(n): for our pages, the chosen values applied and saved.
REX_EXTERN(__imp__sub_82DEC528);
extern "C" REX_FUNC(sub_82DEC528) {
  const int tab = NativePageArg(ctx, base);
  if (tab < 0 || tab != g_native_tab) {
    __imp__sub_82DEC528(ctx, base);
    return;
  }
  const u32 array = PageArray();
  bool changed = false;
  for (u32 r = 0; array && r < g_native_rows.size(); ++r) {
    Setting &s = g_native_settings[g_native_rows[r]];
    const int v = int(bd::mem::load<u32>(array + r * 4));
    if (v >= 0 && v < int(s.values.size()) && v != s.index) {
      s.index = v;
      s.apply(v);
      s.touched = changed = true;
    }
  }
  if (changed)
    SaveNativeSettings();
  BD_INFO("[graphics] native page {}: {}", tab, changed ? "changes applied" : "no change");
  ctx.r3.u64 = 0;
}
