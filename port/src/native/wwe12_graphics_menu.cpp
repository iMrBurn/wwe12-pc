// WWE '12 PC: OPTIONS > GRAPHICS, in the game's own menus.
//
// The OPTIONS menu's "IN-GAME MANUAL" entry (string 41184; "COMMENT JOUER" in French) has no use
// on PC: its text is rewritten in memory as the language's "GRAPHICS" (the way wwe12_quit.cpp
// renames Xbox LIVE). Its sub-menu (TUTORIAL / ABILITIES / ATTRIBUTES) becomes DISPLAY / GRAPHICS /
// ADVANCED (wwe12_menu_patch.cpp), and each entry opens one of the game's option pages with our
// settings (see "The native pages" below). Changes apply at once (window and resolution at the
// next start: rows marked " *") and are saved to wwe12.toml, the same keys as the launcher's.
// Until 2026-10-20 the screen was drawn over the game with ImGui; the user wanted the game's real
// menus.
//
// Disabled with wwe12_graphics_menu = false in wwe12.toml (the manual comes back).

#include "wwe12_graphics_menu.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <rex/cvar.h>
#include <rex/filesystem.h>

#include "../launcher/toml_file.h"
#include "core/logging.h"

REXCVAR_DEFINE_BOOL(wwe12_graphics_menu, true, "WWE12",
                    "OPTIONS menu: \"IN-GAME MANUAL\" becomes GRAPHICS (DISPLAY / GRAPHICS / ADVANCED "
                    "pages in the game's menus).");
REXCVAR_DECLARE(uint32_t, user_language);
REXCVAR_DECLARE(int32_t, svr_render_scale);
REXCVAR_DECLARE(int32_t, bd_anisotropy);
REXCVAR_DECLARE(bool, bd_vsync);
REXCVAR_DECLARE(bool, wwe12_60fps);
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
REXCVAR_DECLARE(std::string, gpu_backend);

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
  const char *api;
};
constexpr Texts kTexts[] = {
    {"GRAPHICS", "INTERNAL RESOLUTION", "TEXTURE SHARPNESS", "VERTICAL SYNC",
     "60 FPS ENTRANCES", "OFF", "ON", "YES", "NO", "AUTO",
     "APPLIED THE NEXT TIME THE GAME STARTS", "OPTIONS", "CHANGE", "BACK",
     "EXPERIMENTAL: TITLE BELTS CAN MOVE ODDLY",
     "DISPLAY", "GRAPHICS", "ADVANCED", "ANTI-ALIASING", "SHADOWS", "SHADOW DISTANCE",
     "DEPTH OF FIELD", "CROWD", "EDGE BLUR", "FRAME LIMIT", "FPS COUNTER", "NONE", "TABS",
     "DISPLAY MODE", "FULL SCREEN", "WINDOW", "BORDERLESS WINDOW", "WINDOW SIZE", "SCREEN SIZE",
     "CPU PRIORITY", "NORMAL", "ABOVE NORMAL", "HIGH", "EXIT ENTRY IN THE MAIN MENU",
     "GRAPHICS API"},
    {"GRAPHISMES", "R\xC3\x89SOLUTION INTERNE", "NETTET\xC3\x89 DES TEXTURES", "SYNCHRO VERTICALE",
     "ENTR\xC3\x89" "ES \xC3\x80 60 FPS", "D\xC3\x89SACTIV\xC3\x89", "ACTIV\xC3\x89", "OUI", "NON",
     "AUTO", "APPLIQU\xC3\x89 AU PROCHAIN LANCEMENT DU JEU", "OPTIONS", "CHANGER", "RETOUR",
     "EXP\xC3\x89RIMENTAL : LES CEINTURES PEUVENT BOUGER BIZARREMENT",
     "AFFICHAGE", "GRAPHISMES", "AVANC\xC3\x89", "ANTICR\xC3\x89" "NELAGE", "OMBRES",
     "DISTANCE DES OMBRES", "PROFONDEUR DE CHAMP", "FOULE", "FLOU DES BORDS", "LIMITE D'IMAGES",
     "COMPTEUR FPS", "AUCUNE", "ONGLETS",
     "MODE D'AFFICHAGE", "PLEIN \xC3\x89" "CRAN", "FEN\xC3\x8A" "TRE", "FEN\xC3\x8A" "TRE SANS BORDURE",
     "TAILLE DE LA FEN\xC3\x8A" "TRE", "TAILLE DE L'\xC3\x89" "CRAN", "PRIORIT\xC3\x89 DU PROCESSEUR",
     "NORMALE", "SUP\xC3\x89" "RIEURE", "HAUTE", "ENTR\xC3\x89" "E QUITTER DANS LE MENU PRINCIPAL",
     "API GRAPHIQUE"},
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
     "EXIT-EINTRAG IM HAUPTMEN\xC3\x9C", "GRAFIK-API"},
    {"GR\xC3\x81" "FICOS", "RESOLUCI\xC3\x93N INTERNA", "NITIDEZ DE TEXTURAS",
     "SINCRONIZACI\xC3\x93N VERTICAL", "ENTRADAS A 60 FPS", "NO", "S\xC3\x8D", "S\xC3\x8D", "NO",
     "AUTO", "SE APLICA LA PR\xC3\x93XIMA VEZ QUE SE INICIE EL JUEGO", "OPCIONES", "CAMBIAR",
     "VOLVER", "EXPERIMENTAL: LOS CINTURONES PUEDEN MOVERSE DE FORMA EXTRA\xC3\x91" "A",
     "PANTALLA", "GR\xC3\x81" "FICOS", "AVANZADO", "ANTIALIASING", "SOMBRAS",
     "DISTANCIA DE SOMBRAS", "PROFUNDIDAD DE CAMPO", "P\xC3\x9A" "BLICO", "DESENFOQUE DE BORDES",
     "L\xC3\x8D" "MITE DE FPS", "CONTADOR DE FPS", "NINGUNO", "PESTA\xC3\x91" "AS",
     "MODO DE PANTALLA", "PANTALLA COMPLETA", "VENTANA", "VENTANA SIN BORDES", "TAMA\xC3\x91" "O DE VENTANA",
     "TAMA\xC3\x91" "O DE PANTALLA", "PRIORIDAD DE CPU", "NORMAL", "SUPERIOR", "ALTA",
     "ENTRADA SALIR EN EL MEN\xC3\x9A" " PRINCIPAL", "API GR\xC3\x81" "FICA"},
    {"GRAFICA", "RISOLUZIONE INTERNA", "NITIDEZZA TEXTURE", "SINCRONIA VERTICALE",
     "INGRESSI A 60 FPS", "NO", "S\xC3\x8C", "S\xC3\x8C", "NO", "AUTO",
     "APPLICATO AL PROSSIMO AVVIO DEL GIOCO", "OPZIONI", "CAMBIA", "INDIETRO",
     "SPERIMENTALE: LE CINTURE POSSONO MUOVERSI IN MODO STRANO",
     "SCHERMO", "GRAFICA", "AVANZATE", "ANTIALIASING", "OMBRE", "DISTANZA OMBRE",
     "PROFONDIT\xC3\x80 DI CAMPO", "FOLLA", "SFOCATURA BORDI", "LIMITE FPS", "CONTATORE FPS", "NESSUNO",
     "SCHEDE",
     "MODALIT\xC3\x80 SCHERMO", "SCHERMO INTERO", "FINESTRA", "FINESTRA SENZA BORDI", "DIMENSIONE FINESTRA",
     "DIMENSIONE SCHERMO", "PRIORIT\xC3\x80 CPU", "NORMALE", "SUPERIORE", "ALTA", "VOCE ESCI NEL MENU PRINCIPALE",
     "API GRAFICA"},
};

const Texts &T() { return kTexts[CurrentLang()]; }

// ---- state shared with the app --------------------------------------------------------------

std::filesystem::path g_toml_path;

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
    static constexpr int kHeights[] = {0, 360, 480, 540, 720, 1080, 1440, 2160};
    r.values = {t.automatic, "360P", "480P", "540P", "720P", "1080P", "1440P", "4K"};
    switch (REXCVAR_GET(svr_render_scale)) {
    case 360: r.index = 1; break;
    case 480: r.index = 2; break;
    case 540: r.index = 3; break;
    case 1: case 720: r.index = 4; break;
    case 1080: r.index = 5; break;
    case 2: case 1440: r.index = 6; break;
    case 3: case 4: case 2160: case 2880: r.index = 7; break;
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

    // Graphics API (next start): the launcher starts wwe12.exe (Vulkan) or wwe12_d3d12.exe
    // (Direct3D 12) from gpu_backend; "any" = Vulkan.
    Setting g;
    g.label = &Texts::api;
    g.tab = 2;
    g.next_start = true;
    g.values = {t.automatic, "VULKAN", "DIRECTX 12"};
    const std::string &backend = REXCVAR_GET(gpu_backend);
    g.index = backend == "d3d12" ? 2 : backend == "vulkan" ? 1 : 0;
    g.apply = [](int i) {
      rex::cvar::SetFlagByName("gpu_backend", i == 2 ? "d3d12" : i == 1 ? "vulkan" : "any");
    };
    g.save = [](TomlFile &f, int i) {
      f.SetString("gpu_backend", i == 2 ? "d3d12" : i == 1 ? "vulkan" : "any");
    };
    s.push_back(std::move(g));
  }
  // Same order as the launcher's tabs.
  {
    const std::vector<const char *Texts::*> order = {
        &Texts::mode,    &Texts::win_size, &Texts::resolution, &Texts::frame_limit, &Texts::vsync,
        &Texts::aniso,   &Texts::crowd,    &Texts::shadow_res, &Texts::dof,         &Texts::api, &Texts::cpu,
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

}  // namespace

void Wwe12GraphicsMenuInit(std::filesystem::path toml_path) { g_toml_path = std::move(toml_path); }

uint32_t Wwe12RelabelGraphicsEntry(uint8_t *region_start, uint8_t *region_end) {
  if (!REXCVAR_GET(wwe12_graphics_menu))
    return 0;
  // Only the loaded language's text: the tables in memory are the player's language (plus
  // English, which some screens keep).
  const EntryText &lang = kEntryTexts[CurrentLang()];
  uint32_t patched = 0;
  for (const EntryText *e : {&lang, &kEntryTexts[kEn]}) {
    const size_t len = std::strlen(e->manual);
    // Skips ahead by the text's length on a mismatch: the scan reads all of the game's memory.
    const std::boyer_moore_horspool_searcher find(reinterpret_cast<const uint8_t *>(e->manual),
                                                  reinterpret_cast<const uint8_t *>(e->manual) + len);
    for (uint8_t *t = region_start; t + len + 1 <= region_end;) {
      t = std::search(t, region_end - len, find);
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

// ==== The native pages ===================================================================
//
// Found on 2026-10-20:
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

#include <map>

#include <rex/runtime.h>

#include "core/memory_helpers.h"
#include "svr_hook.h"
#include "wwe12_jukebox.h"

REX_EXTERN(__imp__sub_827E74A8);  // lua_tointeger (L, index)

namespace {

constexpr u32 kNativeTabText = 43100;   // 43100-43102: the three entries (fixed: menu.pac)
// 43110-43149: the rows' labels, 43150-43299: their values (tools/graphics_pages.py)
constexpr u32 kNativeTextEnd = 43300;
constexpr int kNativePage = 100;        // InitInGameOptions(100 + tab)
constexpr u32 kIngameOptionsScriptSize = 5158;  // "/MENS/0014", the script we replace

int g_pending_tab = -1;  // an entry was chosen, its page script not loaded yet
int g_native_tab = -1;   // the page shown
bool g_jukebox_open = false;  // the JUKEBOX page is open (its subtitle replaces "IN-GAME OPTIONS")
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
  if (g_native_tab < 0 || (g_native_tab == kWwe12JukeboxPage ? !Wwe12JukeboxMenuEnabled()
                                                              : !REXCVAR_GET(wwe12_graphics_menu)))
    return -1;
  PPCContext x = ctx;
  x.r4.u64 = ~uint64_t(0);  // index -1: the top of the Lua stack
  __imp__sub_827E74A8(x, base);
  const int arg = int(x.r3.u32);
  if (arg == kNativePage + kWwe12JukeboxPage)
    return kWwe12JukeboxPage;  // OPTIONS > JUKEBOX (wwe12_jukebox.cpp)
  return arg >= kNativePage && arg < kNativePage + 3 ? arg - kNativePage : -1;
}

}  // namespace

void Wwe12GraphicsNativeDecide(uint32_t label) {
  g_jukebox_open = false;
  if (label == kWwe12JukeboxText && Wwe12JukeboxMenuEnabled()) {
    g_pending_tab = kWwe12JukeboxPage;
    BD_INFO("[graphics] jukebox page chosen");
    return;
  }
  if (!REXCVAR_GET(wwe12_graphics_menu))
    return;
  if (label >= kNativeTabText && label < kNativeTabText + 3) {
    g_pending_tab = int(label - kNativeTabText);
    BD_INFO("[graphics] native page {} chosen", g_pending_tab);
  }
}

bool Wwe12GraphicsNativeEnabled() { return REXCVAR_GET(wwe12_graphics_menu) || Wwe12JukeboxMenuEnabled(); }

uint32_t Wwe12NativeGraphicsGuestText(uint32_t id) {
  if (Wwe12JukeboxMenuEnabled()) {
    const std::string text =
        id == kWwe12JukeboxSubtitleText ? (g_jukebox_open ? Wwe12JukeboxSubtitle() : std::string()) : Wwe12JukeboxText(id);
    if (!text.empty()) {
      // One 128-byte guest slot per id, rewritten when its text changes.
      static std::map<u32, std::pair<u32, std::string>> s_slots;
      auto &slot = s_slots[id];
      if (!slot.first)
        slot.first = REX_KERNEL_MEMORY()->SystemHeapAlloc(128, 0x20);
      if (!slot.first)
        return 0;
      if (slot.second != text) {
        std::memset(bd::mem::at<char>(slot.first), 0, 128);
        std::memcpy(bd::mem::at<char>(slot.first), text.data(), std::min<size_t>(text.size(), 127));
        slot.second = text;
      }
      return slot.first;
    }
  }
  if (id < kNativeTabText || id >= kNativeTextEnd || !REXCVAR_GET(wwe12_graphics_menu))
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
  g_jukebox_open = false;
  if (g_pending_tab == kWwe12JukeboxPage && ctx.r5.u32 == kIngameOptionsScriptSize &&
      bd::mem::try_load<u8>(ctx.r4.u32) == 0x1B) {
    g_pending_tab = -1;
    size_t size = 0;
    const uint8_t *script = Wwe12JukeboxPageScript(&size);
    static u32 s_buffer = 0;
    if (!s_buffer)
      s_buffer = REX_KERNEL_MEMORY()->SystemHeapAlloc(u32(size), 0x20);
    if (s_buffer) {
      std::memcpy(bd::mem::at<u8>(s_buffer), script, size);
      ctx.r4.u64 = s_buffer;
      ctx.r5.u64 = size;
      g_native_tab = kWwe12JukeboxPage;
      g_jukebox_open = true;
      BD_INFO("[graphics] jukebox page: script of {} bytes", size);
    }
  }
  if (g_pending_tab >= 0 && g_pending_tab < 3 && REXCVAR_GET(wwe12_graphics_menu) &&
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
  if (tab == kWwe12JukeboxPage) {
    if (array)
      Wwe12JukeboxPageInit(array);
    ctx.r3.u64 = 0;
    return;
  }
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
  if (tab == kWwe12JukeboxPage) {
    if (array)
      Wwe12JukeboxPageUpdate(array);
    ctx.r3.u64 = 0;
    return;
  }
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
