// WWE '12 PC: OPTIONS > JUKEBOX, the menus play the game's entrance themes.
//
// OPTIONS's CREDITS entry becomes JUKEBOX (wwe12_menu_patch.cpp, wwe12_jukebox_menu); it opens one
// of the game's option pages (run by wwe12_graphics_menu.cpp, script from tools/graphics_pages.py):
// JUKEBOX off / on, ORDER random / one theme, THEME (the game's menu music, then every theme by its
// superstar's name, owned DLC superstars' included), GAME MENU MUSIC no / yes (in the random
// order). With the jukebox on, the menus play entrance themes instead of their own music, one after
// another. Saved in wwe12.toml (wwe12_jukebox, wwe12_jukebox_order, wwe12_jukebox_theme,
// wwe12_jukebox_game_music).
//
// The game's own menu music cannot be chosen song by song (the sound engine picks it): its turn
// gives the menus their music back ([M+32] set, slot 0 stopped: the update starts a song), and
// the jukebox takes over again when the update picks the next one (that song has ended).
//
// Found (2026-10-26):
//   - The music manager M = [[0x83231754] + 52] (sub_8270AC50, sub_8270B008) has players in slots
//     ([M + 4 + 4 * slot]): slot 0 = menu music. Its update sub_8271BFD0(M) (called through its
//     vtable) restarts the menu music when slot 0 has stopped and [M+32] is set, taking the next
//     number of a list of 19 at 0x83074908 into [M+40]. That number does NOT choose the song:
//     sub_8271C9B0(desc, number, flags) always names "@System-Menu_Music", a sound-engine container
//     that picks its own (test of 2026-10-26: list set to 139, a menu theme played).
//   - An entrance theme's number is its superstar's number (John Cena 139, Randy Orton 161):
//     sub_8274A180 puts it in the entrance script (%ENTSSMUSICNO%). Its name for the sound engine
//     is made by sub_8271F080(desc, number, 0, 0) = "@Entrance_Music-MUS_%04d_0_0". The menus'
//     player cannot play it as it is (the play fails at once): it has to be loaded first.
//   - Create an Entrance's theme preview (sub_829EBBE8, a state machine) does: stop slots 0, 1 and
//     2 (sub_8271BB60(M, slot, 0); slot 0 also clears [M+32], so the menus stop restarting their
//     music), wait until they are idle (sub_8271BB00(M, slot, &state): state 0, bit 0x1 or 0x20),
//     sub_8271BC90(M, 2, desc) to load the theme, wait for sub_8271BF58(M, 2) (state bit 0x4 =
//     ready), then sub_8271BD28(M, 2, 0) to play; sub_8271BEE0(M, 2) (bit 0x8) = still playing.
//     The jukebox does the same (test of 2026-10-26: Cena's theme played in the menus).
//   - The themes: the u16 list at 0x83073BA0 (100 ... 343, sorted, then 343 repeated); a
//     superstar's name is in the save's copy of its record ([0x8317FA08] + index * 286, index =
//     u16 table 0x8307B5E0[number], name at +178).
//
// When the menus (re)start their music (a song number picked by the update), the jukebox takes
// over; when the game stops the menu music itself (a match, Create an Entrance's preview), the
// jukebox stops its theme and waits for the menus to come back.

#include "wwe12_jukebox.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <functional>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <random>
#include <set>
#include <vector>

#include <imgui.h>
#include <rex/runtime.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>

#include <rex/cvar.h>
#include <rex/filesystem.h>

#include "../launcher/toml_file.h"
#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_jukebox_menu, true, "WWE12",
                    "OPTIONS menu: CREDITS becomes JUKEBOX (the menus play the entrance themes).");
REXCVAR_DEFINE_BOOL(wwe12_jukebox, false, "WWE12",
                    "Jukebox: the menus play entrance themes instead of their own music.");
REXCVAR_DEFINE_INT32(wwe12_jukebox_order, 0, "WWE12", "Jukebox: 0 = random themes, 1 = the chosen theme only.");
REXCVAR_DEFINE_INT32(wwe12_jukebox_theme, 139, "WWE12",
                     "Jukebox: the chosen theme (its superstar's number; 139 = John Cena; 1 = the game's menu music).");
REXCVAR_DEFINE_BOOL(wwe12_jukebox_game_music, true, "WWE12",
                    "Jukebox: the game's own menu music also comes up in the random order.");
REXCVAR_DEFINE_BOOL(wwe12_jukebox_stop_on_load, true, "WWE12",
                    "Jukebox: its theme stops on every loading screen (false = 1.0: only when the game stops "
                    "the menu music itself; Universe's NEXT MATCH then kept it over the match).");
REXCVAR_DEFINE_BOOL(wwe12_jukebox_keep_theme, true, "WWE12",
                    "Jukebox: while a theme plays, the game's menu music that some screens start on their own "
                    "(second player) is not started (false = 1.1.0: both played at once).");
REXCVAR_DEFINE_BOOL(wwe12_jukebox_log_screens, true, "WWE12",
                    "Jukebox: log the screens that start the game's menu music on their own.");
REXCVAR_DECLARE(uint32_t, user_language);

int64_t Wwe12MsSinceLoadScreen();  // wwe12_load_turbo.cpp
bool Wwe12LoadingScreenUp();       // wwe12_load_turbo.cpp

REX_EXTERN(__imp__sub_8271BFD0);
REX_EXTERN(__imp__sub_8271F080);
REX_EXTERN(__imp__sub_8271BB60);
REX_EXTERN(__imp__sub_8271BB00);
REX_EXTERN(__imp__sub_8271BC90);
REX_EXTERN(__imp__sub_8271BF58);
REX_EXTERN(__imp__sub_8271BD28);
REX_EXTERN(__imp__sub_8271BEE0);
REX_EXTERN(__imp__sub_821D6368);

#include "wwe12_jukebox_page.inc"

std::vector<std::pair<uint32_t, std::string>> Wwe12AddedThemes();  // wwe12_superstar_mods.cpp
ImFont *Wwe12OverlayFont(bool bold);                   // wwe12_display.cpp (Latin, accents; may be null)
rex::ui::ImGuiDrawer *Wwe12OverlayDrawer();            // wwe12_iquit.cpp
void Wwe12OverlayPost(std::function<void()> f);        // wwe12_iquit.cpp: runs f on the UI thread

namespace {

constexpr u32 kDescVtable = 0x82002EC4u;  // the sound description the callers build on the stack
constexpr int kThemeSlot = 2;              // Create an Entrance's preview slot
constexpr u32 kThemeList = 0x83073BA0u;    // u16 theme numbers, sorted
constexpr u32 kRoster = 0x8317FA08u;       // the save's superstar records
constexpr u32 kRosterIndex = 0x8307B5E0u;  // u16 [373]: number -> record index
constexpr u32 kLabelText = 43400, kOnOffText = 43410, kOrderText = 43420, kNoYesText = 43430;
constexpr u32 kThemeText = 44000;
constexpr int kMaxThemes = 400;
constexpr u32 kGameMusic = 1;              // the "theme" that is the game's own menu music
constexpr u32 kContentList = 0x8322E238u;  // the downloadable content's state (owned superstars)

std::filesystem::path g_toml_path;

// ---- texts ---------------------------------------------------------------------------------

struct Texts {
  const char *jukebox, *order, *theme, *off, *on, *random, *one, *theme_n, *game_music, *game_music_row, *no,
      *yes, *subtitle;
};
constexpr Texts kTexts[] = {
    {"JUKEBOX", "ORDER", "THEME", "OFF", "ON", "RANDOM", "CHOSEN THEME ONLY", "THEME ", "GAME MENU MUSIC",
     "GAME MENU MUSIC TOO", "NO", "YES", "MENU MUSIC   -   Y  MY PLAYLIST"},
    {"JUKEBOX", "ORDRE", "TH\xC3\x88ME", "D\xC3\x89SACTIV\xC3\x89", "ACTIV\xC3\x89", "AL\xC3\x89" "ATOIRE",
     "TH\xC3\x88ME CHOISI SEULEMENT", "TH\xC3\x88ME ", "MUSIQUE DES MENUS (JEU)", "MUSIQUE DES MENUS DU JEU AUSSI",
     "NON", "OUI", "MUSIQUE DES MENUS   -   Y  MA PLAYLIST"},
    {"JUKEBOX", "REIHENFOLGE", "THEMA", "AUS", "AN", "ZUF\xC3\x84LLIG", "NUR GEW\xC3\x84HLTES THEMA", "THEMA ",
     "MEN\xC3\x9CMUSIK DES SPIELS", "AUCH MEN\xC3\x9CMUSIK DES SPIELS", "NEIN", "JA", "MEN\xC3\x9CMUSIK   -   Y  MEINE PLAYLIST"},
    {"JUKEBOX", "ORDEN", "TEMA", "NO", "S\xC3\x8D", "ALEATORIO", "SOLO EL TEMA ELEGIDO", "TEMA ",
     "M\xC3\x9ASICA DE LOS MEN\xC3\x9AS (JUEGO)", "TAMBI\xC3\x89N M\xC3\x9ASICA DEL JUEGO", "NO", "S\xC3\x8D",
     "M\xC3\x9ASICA DE LOS MEN\xC3\x9AS   -   Y  MI LISTA"},
    {"JUKEBOX", "ORDINE", "TEMA", "NO", "S\xC3\x8C", "CASUALE", "SOLO IL TEMA SCELTO", "TEMA ",
     "MUSICA DEI MEN\xC3\x99 (GIOCO)", "ANCHE LA MUSICA DEL GIOCO", "NO", "S\xC3\x8C", "MUSICA DEI MEN\xC3\x99   -   Y  LA MIA PLAYLIST"},
};

const Texts &T() {
  switch (REXCVAR_GET(user_language)) {
  case 3: return kTexts[2];
  case 4: return kTexts[1];
  case 5: return kTexts[3];
  case 6: return kTexts[4];
  default: return kTexts[0];
  }
}

// ---- themes --------------------------------------------------------------------------------

struct Theme {
  u32 number;
  std::string name;
  bool mod = false;  // an added superstar's theme (Mod Maker)
};
std::vector<Theme> g_themes;  // game thread

std::string SuperstarName(u32 number) {
  const u32 roster = bd::mem::try_load<u32>(kRoster);
  const u32 index = number < 373 ? bd::mem::try_load<u16>(kRosterIndex + number * 2) : 0xFFFF;
  if (!roster || index >= 373)
    return {};
  std::string name;
  for (u32 i = 0; i < 40; ++i) {
    const char c = char(bd::mem::try_load<u8>(roster + index * 286 + 178 + i));
    if (!c)
      break;
    name += char(std::toupper(static_cast<unsigned char>(c)));
  }
  return name;
}

// The downloadable content's state of a superstar (2 = owned), as the select screen's DLC tile
// reads it (wwe12_roster_probe.cpp): the content list [0x8322E238] + 164, 16-byte entries
// {kind 1, number, 0, state}.
u32 OwnedState(u32 number) {
  const u32 obj = bd::mem::try_load<u32>(kContentList);
  if (!obj)
    return 0;
  const u32 entries = bd::mem::try_load<u32>(obj + 164 + 4), count = bd::mem::try_load<u32>(obj + 164 + 12);
  for (u32 i = 0; i < count && i < 4096 && entries; ++i) {
    const u32 e = entries + i * 16;
    if (bd::mem::try_load<u32>(e) == 1 && bd::mem::try_load<u32>(e + 4) == number && bd::mem::try_load<u32>(e + 8) == 0)
      return bd::mem::try_load<u32>(e + 12);
  }
  return 0;
}

// The game's menu music, then the game's theme numbers with their superstars' names (themes
// without one: "THEME n"), then the owned DLC superstars' (numbers 50-99).
void LoadThemes() {
  g_themes.clear();
  g_themes.push_back({kGameMusic, T().game_music});
  u32 last = 0;
  for (int i = 0; i < kMaxThemes; ++i) {
    const u32 n = bd::mem::try_load<u16>(kThemeList + i * 2);
    if (n <= last || n >= 800)
      break;
    last = n;
    if (n == 272)  // the user playlist's number (sub_8274A180), not a theme
      continue;
    std::string name = SuperstarName(n);
    if (name.empty() || name == "0")
      name = T().theme_n + std::to_string(n);
    g_themes.push_back({n, name});
  }
  for (u32 n = 50; n < 100; ++n) {
    const std::string name = SuperstarName(n);
    if (!name.empty() && name != "0" && OwnedState(n) == 2)
      g_themes.push_back({n, name});
  }
  // the added superstars' own themes (a WWE '13 one, in the mods pack's sound package)
  for (const auto &[n, name] : Wwe12AddedThemes()) {
    std::string up;
    for (char c : name)
      up += char(std::toupper(static_cast<unsigned char>(c)));
    g_themes.push_back({n, up, true});
  }
  BD_INFO("[jukebox] {} themes (first {} {}, last {} {})", g_themes.size(),
          g_themes.empty() ? 0 : g_themes.front().number, g_themes.empty() ? "" : g_themes.front().name,
          g_themes.empty() ? 0 : g_themes.back().number, g_themes.empty() ? "" : g_themes.back().name);
}

int ThemeIndex(u32 number) {
  for (int i = 0; i < int(g_themes.size()); ++i)
    if (g_themes[i].number == number)
      return i;
  return 0;
}

// ---- player (game thread, in the music manager's update) -----------------------------------

enum class Step { Idle, Stop, WaitStop, Prepare, WaitReady, Play, Playing, GameSong };
Step g_step = Step::Idle;
int g_frames = 0;          // updates spent in the current step
u32 g_playing = 0;         // theme being played
bool g_menus = false;      // the menus' music is ours to replace
bool g_own_call = false;   // our own sub_8271BB60 calls
std::atomic<bool> g_restart{false};   // the page asked for its theme now
bool g_play_chosen = false;           // the next theme is the chosen one
std::atomic<bool> g_give_back{false}; // the page turned the jukebox off: menu music back
std::mt19937 g_rng{std::random_device{}()};
std::vector<u32> g_failed;  // themes that could not be loaded (left out of the random order)

// ---- MY PLAYLIST (2.0): the themes the random order plays ------------------------------------
//
// Every theme is in the playlist unless the player left it out: Documents\wwe12\jukebox_playlist.txt (the user data
// root, next to universe_stats.txt) has one line "off <number>  # NAME" per theme left out, so a new theme (DLC,
// mod) comes in by itself. The random order draws from the playlist only (nothing left in it: every theme, as before);
// CHOSEN THEME ONLY ignores it. No key in wwe12.toml (user's decision, 2026-11-28).
std::mutex g_list_mutex;  // g_off and the screen's copy of the themes (game thread, UI thread)
std::set<u32> g_off;      // themes left out of the playlist
bool g_off_loaded = false;
u32 g_listen_now = 0;     // (game thread) the theme the screen's X asked for
std::atomic<u32> g_listen_ask{0};  // (pad -> music manager's update) X pressed on that theme

std::filesystem::path PlaylistFile() {
  auto *state = REX_KERNEL_STATE();
  if (!state || !state->emulator() || state->emulator()->user_data_root().empty())
    return {};
  return state->emulator()->user_data_root() / "jukebox_playlist.txt";
}

void LoadPlaylist() {
  std::lock_guard<std::mutex> lock(g_list_mutex);
  if (g_off_loaded)
    return;
  const std::filesystem::path path = PlaylistFile();
  if (path.empty())
    return;  // (too early: tried again next time)
  g_off_loaded = true;
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    unsigned n = 0;
    if (std::sscanf(line.c_str(), " off %u", &n) == 1)
      g_off.insert(n);
  }
  BD_INFO("[jukebox] playlist: {} theme(s) left out ({})", g_off.size(), rex::path_to_utf8(path));
}

// (called with g_list_mutex held) names: for the reader of the file only
void SavePlaylist(const std::vector<Theme> &themes) {
  const std::filesystem::path path = PlaylistFile();
  if (path.empty())
    return;
  std::ofstream out(path, std::ios::trunc);
  out << "# WWE '12 PC jukebox: the themes left out of MY PLAYLIST (OPTIONS > JUKEBOX, Y).\n"
         "# One line per theme: off <number>. Delete this file to put every theme back.\n";
  for (const u32 n : g_off) {
    std::string name;
    for (const Theme &t : themes)
      if (t.number == n)
        name = t.name;
    out << "off " << n << (name.empty() ? "" : "  # " + name) << "\n";
  }
  BD_INFO("[jukebox] playlist saved: {} theme(s) left out", g_off.size());
}

using GuestFn = void (*)(PPCContext &, u8 *);

// Calls a guest function with r3-r6, on a stack below the caller's; Scratch() (out-parameters, the
// sound description) is 512 bytes of that stack, above the callee's frame.
u32 Call(GuestFn fn, PPCContext &ctx, u8 *base, u32 r3, u32 r4, u32 r5 = 0, u32 r6 = 0) {
  PPCContext c = ctx;
  c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
  c.r3.u64 = r3;
  c.r4.u64 = r4;
  c.r5.u64 = r5;
  c.r6.u64 = r6;
  fn(c, base);
  return c.r3.u32;
}
u32 Scratch(const PPCContext &ctx) { return ((ctx.r1.u32 - 1024) & ~15u) + 256; }

u32 SlotState(PPCContext &ctx, u8 *base, u32 m, int slot) {
  const u32 out = Scratch(ctx);
  bd::mem::store<u32>(out, 0);
  Call(__imp__sub_8271BB00, ctx, base, m, slot, out);
  return bd::mem::try_load<u32>(out);
}

void StopSlot(PPCContext &ctx, u8 *base, u32 m, int slot) {
  g_own_call = true;
  Call(__imp__sub_8271BB60, ctx, base, m, slot, 0);
  g_own_call = false;
}

bool SlotBusy(PPCContext &ctx, u8 *base, u32 m, int slot) {
  const u32 st = SlotState(ctx, base, m, slot);
  return st != 0 && !(st & 0x21);
}

// While a theme plays, the menu music started on slot 0 or 1 by any other way (title screen -> main menu:
// neither the update's pick nor sub_821D6368 was seen, the theme "went on" and the game's music was heard,
// user 2026-11-11, log wwe12_530) is stopped at once; each stop is logged with the slot's state.
void GuardTheme(PPCContext &ctx, u8 *base, u32 m) {
  if (REXCVAR_GET(wwe12_jukebox_log_screens)) {  // (the three slots' states when they change, while a theme plays)
    static u32 s_last[3] = {~0u, ~0u, ~0u};
    u32 now[3];
    for (int slot = 0; slot <= 2; ++slot)
      now[slot] = SlotState(ctx, base, m, slot);
    if (std::memcmp(now, s_last, sizeof now) != 0) {
      std::memcpy(s_last, now, sizeof now);
      BD_INFO("[jukebox] slots during theme {}: 0 = {:X}, 1 = {:X}, 2 = {:X} ([M+32] {})", g_playing, now[0], now[1],
              now[2], bd::mem::try_field<u32>(m, 32));
    }
  }
  if (!REXCVAR_GET(wwe12_jukebox_keep_theme))
    return;
  // (one stop per start: a song already playing fades out over ~2 s, still "playing" (8) meanwhile; it was stopped
  // again every update, test of 2026-11-11)
  static bool s_stopping[2] = {};
  for (int slot = 0; slot <= 1; ++slot) {
    const u32 st = SlotState(ctx, base, m, slot);
    if (st == 0 || (st & 0x21)) {
      s_stopping[slot] = false;
      continue;
    }
    if (s_stopping[slot])
      continue;
    s_stopping[slot] = true;
    StopSlot(ctx, base, m, slot);
    static u32 s_logged = 0;
    if (s_logged < 50) {
      ++s_logged;
      BD_INFO("[jukebox] the game's music started on slot {} (state {:X}, [M+32] {}, [M+40] {}) over theme {}: stopped",
              slot, st, bd::mem::try_field<u32>(m, 32), bd::mem::try_field<u32>(m, 40), g_playing);
    }
  }
}

void Go(Step s) {
  g_step = s;
  g_frames = 0;
}

u32 NextTheme() {
  if (g_themes.empty())
    LoadThemes();
  if (g_themes.empty())
    return 0;
  if (const u32 listen = g_listen_now) {  // (MY PLAYLIST's X: that theme now)
    g_listen_now = 0;
    return listen;
  }
  const u32 chosen = u32(REXCVAR_GET(wwe12_jukebox_theme));
  const bool play_chosen = g_play_chosen;
  g_play_chosen = false;
  if (REXCVAR_GET(wwe12_jukebox_order) == 1 || play_chosen)
    return g_themes[ThemeIndex(chosen)].number;
  LoadPlaylist();
  std::vector<u32> pool;
  for (int pass = 0; pass < 2 && pool.empty(); ++pass) {  // (pass 1: nothing left in the playlist, every theme)
    std::lock_guard<std::mutex> lock(g_list_mutex);
    for (const Theme &t : g_themes)
      if (t.number != g_playing && (t.number != kGameMusic || REXCVAR_GET(wwe12_jukebox_game_music)) &&
          std::find(g_failed.begin(), g_failed.end(), t.number) == g_failed.end() &&
          (pass == 1 || !g_off.count(t.number)))
        pool.push_back(t.number);
    if (pass == 0 && pool.empty() && !g_off.empty()) {
      static int s_logged = 0;
      if (s_logged++ < 5)
        BD_INFO("[jukebox] playlist: nothing to play in it ({} theme(s) left out), every theme instead", g_off.size());
    }
  }
  if (pool.empty())
    return g_playing ? g_playing : kGameMusic;
  return pool[std::uniform_int_distribution<size_t>(0, pool.size() - 1)(g_rng)];
}

void Run(PPCContext &ctx, u8 *base, u32 m) {
  ++g_frames;
  switch (g_step) {
  case Step::Stop:
    for (int slot = 0; slot <= 2; ++slot)
      StopSlot(ctx, base, m, slot);
    Go(Step::WaitStop);
    break;
  case Step::WaitStop: {
    bool idle = true;
    for (int slot = 0; slot <= 2; ++slot) {
      const u32 st = SlotState(ctx, base, m, slot);
      if (st != 0 && !(st & 0x21))
        idle = false;
    }
    if (idle || g_frames > 120)
      Go(Step::Prepare);
    break;
  }
  case Step::Prepare: {
    g_playing = NextTheme();
    if (!g_playing) {
      BD_WARN("[jukebox] no theme found: menu music left as it is");
      g_menus = false;
      Go(Step::Idle);
      break;
    }
    if (g_playing == kGameMusic) {
      // The game's turn: its update starts a menu song (slot 0 is stopped).
      bd::mem::store<u32>(m + 32, 1);
      BD_INFO("[jukebox] now playing the game's menu music");
      Go(Step::GameSong);
      break;
    }
    const u32 desc = Scratch(ctx) + 16;
    for (u32 i = 0; i < 160; i += 4)
      bd::mem::store<u32>(desc + i, 0);
    bd::mem::store<u32>(desc, kDescVtable);
    bd::mem::store<u32>(desc + 136, 1);
    Call(__imp__sub_8271F080, ctx, base, desc, g_playing, 0, 0);
    Call(__imp__sub_8271BC90, ctx, base, m, kThemeSlot, desc);
    Go(Step::WaitReady);
    break;
  }
  case Step::WaitReady:
    if (Call(__imp__sub_8271BF58, ctx, base, m, kThemeSlot)) {
      Go(Step::Play);
    } else if (g_frames > 150) {
      BD_WARN("[jukebox] theme {} could not be loaded (state {:X}): next one", g_playing,
              SlotState(ctx, base, m, kThemeSlot));
      g_failed.push_back(g_playing);
      StopSlot(ctx, base, m, kThemeSlot);
      Go(Step::Prepare);
    }
    break;
  case Step::Play:
    Call(__imp__sub_8271BD28, ctx, base, m, kThemeSlot, 0);
    BD_INFO("[jukebox] now playing theme {} ({})", g_playing, g_themes.empty() ? "" : g_themes[ThemeIndex(g_playing)].name);
    Go(Step::Playing);
    break;
  case Step::Playing:
    GuardTheme(ctx, base, m);
    if (g_frames > 30 && !Call(__imp__sub_8271BEE0, ctx, base, m, kThemeSlot))
      Go(Step::Prepare);  // the theme ended: the next one
    break;
  default:
    break;
  }
}

// The menus' music back (the jukebox turned off): our theme stopped, [M+32] set so the update
// restarts the menu music.
void GiveBack(PPCContext &ctx, u8 *base, u32 m) {
  StopSlot(ctx, base, m, kThemeSlot);
  bd::mem::store<u32>(m + 32, 1);
  g_menus = false;
  g_playing = 0;
  Go(Step::Idle);
  BD_INFO("[jukebox] off: the menus' own music again");
}

void Save() {
  if (g_toml_path.empty())
    return;
  TomlFile f;
  if (!f.Load(g_toml_path)) {
    BD_WARN("[jukebox] cannot read {}", rex::path_to_utf8(g_toml_path));
    return;
  }
  f.SetBool("wwe12_jukebox", REXCVAR_GET(wwe12_jukebox));
  f.SetInt("wwe12_jukebox_order", REXCVAR_GET(wwe12_jukebox_order));
  f.SetInt("wwe12_jukebox_theme", REXCVAR_GET(wwe12_jukebox_theme));
  f.SetBool("wwe12_jukebox_game_music", REXCVAR_GET(wwe12_jukebox_game_music));
  if (f.Save())
    BD_INFO("[jukebox] settings saved to {}", rex::path_to_utf8(g_toml_path));
}

// ---- the MY PLAYLIST screen (2.0) ------------------------------------------------------------
//
// Our own screen, drawn over the menu with ImGui (as the I Quit's gauge: wwe12_iquit.cpp's drawer), opened from the
// JUKEBOX page (its script's FromSub handlers call yOption.InitInGameOptions(104-106): wwe12_graphics_menu.cpp). While
// it is open the pad is ours (wwe12_intros.cpp's XInputGetState hook hides it all from the game): A puts the theme
// under the cursor in or out of the playlist, X plays it (jukebox on), Y all / none, LB / RB a page, B or START back.
// The buttons still held when it closes stay hidden until released (B would leave the page behind it).

struct PlaylistWords {
  const char *title, *count, *help, *page, *nothing, *listen_off, *mod;
};
constexpr PlaylistWords kPlaylistWords[] = {
    {"MY PLAYLIST", "%d / %d IN THE PLAYLIST",
     "A  ADD / REMOVE      X  LISTEN      Y  ALL / NONE      LB / RB  PAGE      B  BACK", "PAGE %d / %d",
     "Nothing checked: every theme plays.", "Turn the JUKEBOX on (and confirm the page) to listen.", "MOD"},
    {"MA PLAYLIST", "%d / %d DANS LA PLAYLIST",
     "A  AJOUTER / RETIRER      X  \xC3\x89" "COUTER      Y  TOUT / RIEN      LB / RB  PAGE      B  RETOUR",
     "PAGE %d / %d", "Rien de coch\xC3\xA9 : tous les th\xC3\xA8mes passent.",
     "Activez le JUKEBOX (et validez la page) pour \xC3\xA9" "couter.", "MOD"},
    {"MEINE PLAYLIST", "%d / %d IN DER PLAYLIST",
     "A  HINZUF\xC3\x9CGEN / ENTFERNEN      X  ANH\xC3\x96REN      Y  ALLE / KEINE      LB / RB  SEITE      B  ZUR\xC3\x9C" "CK",
     "SEITE %d / %d", "Nichts ausgew\xC3\xA4hlt: alle Themes laufen.",
     "Schalte die JUKEBOX ein (und best\xC3\xA4tige die Seite), um zuzuh\xC3\xB6ren.", "MOD"},
    {"MI LISTA", "%d / %d EN LA LISTA",
     "A  A\xC3\x91" "ADIR / QUITAR      X  ESCUCHAR      Y  TODO / NADA      LB / RB  P\xC3\x81GINA      B  VOLVER",
     "P\xC3\x81GINA %d / %d", "Nada marcado: suenan todos los temas.",
     "Activa el JUKEBOX (y confirma la p\xC3\xA1gina) para escuchar.", "MOD"},
    {"LA MIA PLAYLIST", "%d / %d NELLA PLAYLIST",
     "A  AGGIUNGI / TOGLI      X  ASCOLTA      Y  TUTTI / NESSUNO      LB / RB  PAGINA      B  INDIETRO",
     "PAGINA %d / %d", "Niente selezionato: suonano tutti i temi.",
     "Attiva il JUKEBOX (e conferma la pagina) per ascoltare.", "MOD"},
};
const PlaylistWords &PW() {
  switch (REXCVAR_GET(user_language)) {
  case 3: return kPlaylistWords[2];
  case 4: return kPlaylistWords[1];
  case 5: return kPlaylistWords[3];
  case 6: return kPlaylistWords[4];
  default: return kPlaylistWords[0];
  }
}

constexpr int kRows = 12, kCols = 3, kPerPage = kRows * kCols;  // a page: 3 columns of 12, top to bottom
std::atomic<bool> g_list_open{false}, g_overlay_up{false};
std::vector<Theme> g_list;  // (g_list_mutex) the screen's copy of the themes
int g_cursor = 0;           // (g_list_mutex)
std::string g_note;         // (g_list_mutex) a line under the list, for a few seconds
int64_t g_note_ms = 0;
u16 g_pad_before[4] = {};
u16 g_hold[4] = {};         // buttons held when the screen closed: hidden until released

int64_t NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

void Note(const char *text) {  // (g_list_mutex held)
  g_note = text;
  g_note_ms = NowMs();
}

class PlaylistOverlay final : public rex::ui::ImGuiDialog {
 public:
  explicit PlaylistOverlay(rex::ui::ImGuiDrawer *drawer) : ImGuiDialog(drawer) {}

 protected:
  void OnClose() override { g_overlay_up = false; }
  void OnDraw(ImGuiIO &io) override {
    if (!g_list_open.load()) {
      Close();  // (the drawer stops drawing ImGui every frame)
      return;
    }
    std::lock_guard<std::mutex> lock(g_list_mutex);
    const PlaylistWords &w = PW();
    const float W = io.DisplaySize.x, H = io.DisplaySize.y, k = std::max(0.5f, H / 1080.0f);
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    ImFont *bold = Wwe12OverlayFont(true), *text = Wwe12OverlayFont(false);
    if (!bold)
      bold = ImGui::GetFont();
    if (!text)
      text = bold;
    auto put = [&](ImFont *f, float px, float x, float y, ImU32 col, const char *s) {
      dl->AddText(f, px, ImVec2(x + 2 * k, y + 2 * k), IM_COL32(0, 0, 0, 200), s);
      dl->AddText(f, px, ImVec2(x, y), col, s);
    };
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), IM_COL32(0, 0, 0, 215));
    const float x0 = W * 0.06f, x1 = W * 0.94f, y0 = H * 0.07f, y1 = H * 0.93f;
    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(18, 18, 24, 235), 6 * k);
    dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(185, 0, 0, 255), 6 * k, 0, 2 * k);
    // the title, the count
    const float title_px = 46 * k, pad = 26 * k;
    put(bold, title_px, x0 + pad, y0 + pad * 0.7f, IM_COL32(255, 255, 255, 255), w.title);
    int on = 0;
    for (const Theme &t : g_list)
      on += g_off.count(t.number) ? 0 : 1;
    char line[160];
    std::snprintf(line, sizeof line, w.count, on, int(g_list.size()));
    const float count_px = 26 * k;
    ImVec2 sz = text->CalcTextSizeA(count_px, FLT_MAX, 0.f, line);
    put(text, count_px, x1 - pad - sz.x, y0 + pad * 0.7f + (title_px - count_px) * 0.5f, IM_COL32(230, 190, 60, 255), line);
    // the list: this page's themes, 3 columns of 12
    const int pages = std::max(1, (int(g_list.size()) + kPerPage - 1) / kPerPage);
    const int page = std::clamp(g_cursor / kPerPage, 0, pages - 1);
    const float ly0 = y0 + pad * 0.7f + title_px + 18 * k, ly1 = y1 - 90 * k;
    const float cw = (x1 - x0 - 2 * pad) / kCols, ch = (ly1 - ly0) / kRows;
    const float name_px = std::min(26 * k, ch * 0.62f);
    for (int i = 0; i < kPerPage; ++i) {
      const int n = page * kPerPage + i;
      if (n >= int(g_list.size()))
        break;
      const Theme &t = g_list[n];
      const bool in = !g_off.count(t.number);
      const float cx = x0 + pad + cw * float(i / kRows), cy = ly0 + ch * float(i % kRows);
      if (n == g_cursor)
        dl->AddRectFilled(ImVec2(cx - 6 * k, cy + 2 * k), ImVec2(cx + cw - 10 * k, cy + ch - 2 * k), IM_COL32(185, 0, 0, 230),
                          3 * k);
      const float box = ch * 0.5f, bx = cx + 4 * k, by = cy + (ch - box) * 0.5f;
      if (in) {
        dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + box, by + box), IM_COL32(230, 190, 60, 255), 2 * k);
        dl->AddLine(ImVec2(bx + box * 0.2f, by + box * 0.52f), ImVec2(bx + box * 0.42f, by + box * 0.75f),
                    IM_COL32(20, 20, 20, 255), 2.5f * k);
        dl->AddLine(ImVec2(bx + box * 0.42f, by + box * 0.75f), ImVec2(bx + box * 0.82f, by + box * 0.25f),
                    IM_COL32(20, 20, 20, 255), 2.5f * k);
      } else {
        dl->AddRect(ImVec2(bx, by), ImVec2(bx + box, by + box), IM_COL32(150, 150, 150, 255), 2 * k, 0, 1.5f * k);
      }
      const float nx = bx + box + 12 * k, right = cx + cw - 16 * k;
      float name_right = right;
      if (t.mod) {
        const ImVec2 ms = text->CalcTextSizeA(name_px * 0.7f, FLT_MAX, 0.f, w.mod);
        name_right = right - ms.x - 10 * k;
        dl->AddText(text, name_px * 0.7f, ImVec2(right - ms.x, cy + (ch - ms.y) * 0.5f), IM_COL32(120, 190, 255, 255), w.mod);
      }
      dl->PushClipRect(ImVec2(nx, cy), ImVec2(name_right, cy + ch), true);
      const ImVec2 ns = text->CalcTextSizeA(name_px, FLT_MAX, 0.f, t.name.c_str());
      put(text, name_px, nx, cy + (ch - ns.y) * 0.5f, in || n == g_cursor ? IM_COL32(255, 255, 255, 255)
                                                                            : IM_COL32(140, 140, 140, 255), t.name.c_str());
      dl->PopClipRect();
    }
    // the page, a note, the buttons
    const float foot_px = 22 * k;
    std::snprintf(line, sizeof line, w.page, page + 1, pages);
    sz = text->CalcTextSizeA(foot_px, FLT_MAX, 0.f, line);
    put(text, foot_px, x1 - pad - sz.x, ly1 + 10 * k, IM_COL32(200, 200, 200, 255), line);
    if (!g_note.empty() && NowMs() - g_note_ms < 4000)
      put(text, foot_px, x0 + pad, ly1 + 10 * k, IM_COL32(230, 190, 60, 255), g_note.c_str());
    else if (on == 0)
      put(text, foot_px, x0 + pad, ly1 + 10 * k, IM_COL32(230, 190, 60, 255), w.nothing);
    sz = text->CalcTextSizeA(foot_px, FLT_MAX, 0.f, w.help);
    put(text, foot_px, (x0 + x1) * 0.5f - sz.x * 0.5f, y1 - pad - foot_px, IM_COL32(255, 255, 255, 255), w.help);
  }
};

}  // namespace

void Wwe12JukeboxInit(std::filesystem::path toml_path) { g_toml_path = std::move(toml_path); }

bool Wwe12JukeboxMenuEnabled() { return REXCVAR_GET(wwe12_jukebox_menu); }

const uint8_t *Wwe12JukeboxPageScript(size_t *size) {
  *size = sizeof(kJukeboxPage);
  return kJukeboxPage;
}

int Wwe12JukeboxPageRows() { return kJukeboxPageRows; }

void Wwe12JukeboxPageInit(uint32_t array) {
  LoadThemes();
  const u32 n = kJukeboxPageRows;
  const u32 values[4] = {REXCVAR_GET(wwe12_jukebox) ? 1u : 0u, REXCVAR_GET(wwe12_jukebox_order) == 1 ? 1u : 0u,
                         u32(ThemeIndex(u32(REXCVAR_GET(wwe12_jukebox_theme)))),
                         REXCVAR_GET(wwe12_jukebox_game_music) ? 1u : 0u};
  const u32 maxima[4] = {1, 1, g_themes.empty() ? 0u : u32(g_themes.size() - 1), 1};
  for (u32 r = 0; r < n; ++r) {
    bd::mem::store<u32>(array + r * 4, values[r]);
    bd::mem::store<u32>(array + (n + r) * 4, 1);
    bd::mem::store<u32>(array + (2 * n + r) * 4, maxima[r]);
  }
}

void Wwe12JukeboxPageUpdate(uint32_t array) {
  const bool on = bd::mem::load<u32>(array) != 0;
  const int order = bd::mem::load<u32>(array + 4) ? 1 : 0;
  const u32 index = bd::mem::load<u32>(array + 8);
  const bool game_music = bd::mem::load<u32>(array + 12) != 0;
  const u32 theme = index < g_themes.size() ? g_themes[index].number : u32(REXCVAR_GET(wwe12_jukebox_theme));
  const bool was_on = REXCVAR_GET(wwe12_jukebox);
  const bool changed = on != was_on || order != REXCVAR_GET(wwe12_jukebox_order) ||
                       int(theme) != REXCVAR_GET(wwe12_jukebox_theme) ||
                       game_music != REXCVAR_GET(wwe12_jukebox_game_music);
  rex::cvar::SetFlagByName("wwe12_jukebox_game_music", game_music ? "true" : "false");
  rex::cvar::SetFlagByName("wwe12_jukebox", on ? "true" : "false");
  rex::cvar::SetFlagByName("wwe12_jukebox_order", std::to_string(order));
  rex::cvar::SetFlagByName("wwe12_jukebox_theme", std::to_string(theme));
  if (changed)
    Save();
  if (on && changed)
    g_restart = true;  // the chosen theme now
  if (!on && was_on)
    g_give_back = true;
  BD_INFO("[jukebox] page: {}, order {}, theme {}{}", on ? "on" : "off", order, theme, changed ? "" : " (no change)");
}

std::string Wwe12JukeboxSubtitle() { return T().subtitle; }

std::string Wwe12JukeboxText(uint32_t id) {
  const Texts &t = T();
  switch (id) {
  case kWwe12JukeboxText: return t.jukebox;
  case kLabelText: return t.jukebox;
  case kLabelText + 1: return t.order;
  case kLabelText + 2: return t.theme;
  case kLabelText + 3: return t.game_music_row;
  case kNoYesText: return t.no;
  case kNoYesText + 1: return t.yes;
  case kOnOffText: return t.off;
  case kOnOffText + 1: return t.on;
  case kOrderText: return t.random;
  case kOrderText + 1: return t.one;
  default: break;
  }
  if (id >= kThemeText && id < kThemeText + g_themes.size())
    return g_themes[id - kThemeText].name;
  return {};
}

// sub_8271BB60(M, slot, flag): stops a slot. The game stopping the menu music itself (a match is
// loading, Create an Entrance plays a theme) ends the jukebox's turn: its theme stops too.
extern "C" REX_FUNC(sub_8271BB60) {
  const u32 m = ctx.r3.u32;
  const u32 slot = ctx.r4.u32;
  if (!g_own_call && slot == 0 && g_menus && REXCVAR_GET(wwe12_jukebox)) {
    PPCContext c = ctx;
    if (g_step != Step::Idle) {
      StopSlot(c, base, m, kThemeSlot);
      BD_INFO("[jukebox] the game stopped the menu music: theme {} stopped", g_playing);
    }
    g_menus = false;
    g_playing = 0;
    Go(Step::Idle);
  }
  __imp__sub_8271BB60(ctx, base);
}

// The music manager's update: when the menus pick their next song, the jukebox takes over.
extern "C" REX_FUNC(sub_8271BFD0) {
  if (!REXCVAR_GET(wwe12_jukebox) && !g_give_back.load()) {
    __imp__sub_8271BFD0(ctx, base);
    return;
  }
  PPCContext saved = ctx;
  const u32 m = ctx.r3.u32;
  // A loading screen opened or closed since the last update (its time since then went down): a
  // match (or a mode) is loading, whatever the way in. Universe's pause > NEXT MATCH loads the next
  // match without the game stopping the menu music itself, and the theme went on over the match
  // until the game was closed (Discord, 1.0.0). The menus' next song starts the jukebox again.
  static int64_t s_since = INT64_MAX;
  const int64_t since = Wwe12MsSinceLoadScreen();
  // Only with a loading screen up: entering the wrestler select also calls the loading screens'
  // shared methods (a close, no screen up), and the theme was cut there with nothing after it
  // (slot 0 stopped by us, so the menus never pick a song again; tested 2026-11-07).
  const bool load_event = since < s_since;
  s_since = since;
  const bool load_screen = load_event && Wwe12LoadingScreenUp();
  if (load_event && !load_screen && g_menus)
    BD_INFO("[jukebox] loading-screen call without a loading screen up: theme {} goes on", g_playing);
  if (load_screen && g_menus && REXCVAR_GET(wwe12_jukebox_stop_on_load)) {
    if (g_step != Step::Idle)
      StopSlot(saved, base, m, kThemeSlot);
    BD_INFO("[jukebox] loading screen: theme {} stopped", g_playing);
    g_menus = false;
    g_playing = 0;
    Go(Step::Idle);
  }
  const u32 before = bd::mem::try_field<u32>(m, 40);
  __imp__sub_8271BFD0(ctx, base);
  if (g_give_back.exchange(false)) {
    if (g_menus)
      GiveBack(saved, base, m);
    return;
  }
  const bool picked = bd::mem::try_field<u32>(m, 40) != before;
  if (const u32 listen = g_listen_ask.exchange(0)) {
    // MY PLAYLIST's X: that theme now (then the playlist's random order again).
    g_listen_now = listen;
    g_menus = true;
    Go(Step::Stop);
  } else if (g_restart.exchange(false)) {
    // The page: the chosen theme now (the menus' music and ours stopped first).
    g_play_chosen = true;
    g_menus = true;
    Go(Step::Stop);
  } else if (picked && !g_menus) {
    g_menus = true;
    Go(Step::Stop);
  } else if (picked && g_step == Step::GameSong && g_frames > 1) {
    // The update picked the song after the game's (that one has ended): the jukebox again, unless
    // the game's music is the only one chosen.
    const bool only_game =
        REXCVAR_GET(wwe12_jukebox_order) == 1 && u32(REXCVAR_GET(wwe12_jukebox_theme)) == kGameMusic;
    if (!only_game)
      Go(Step::Stop);
  }
  if (g_menus && g_step != Step::Idle)
    Run(saved, base, m);
}

// sub_821D6368(song): a screen starts the game's menu music ("@System-Menu_Music") on the music
// manager's player 1 itself, not through the update (so [M+40] does not change and the jukebox
// does not see it). Callers: screen methods sub_8261A020 (lr 0x8261A07C: starts it when player 1
// is silent, without stopping player 0) and sub_8261B688 (lr 0x8261B6E0: stops player 0 first);
// the song is the u16 at [screen+892]+0x140000. The jukebox's Stop step silences player 1, so the
// next such screen started the menu music over the theme (Jay, Discord, 1.1.0: "default menu music
// plays over jukebox sometimes"). While a theme plays (or is being loaded), that start is skipped.
// At the call r31 is still the caller's: the screen object.
// With no theme on (the game stopped slot 0 itself: a match, Create an Entrance, or START on the title screen,
// after which no song is picked by the update any more, so the menu music came back under nothing - user,
// 2026-11-11), that start is skipped too and the jukebox takes over again (its Stop step, then a theme), unless
// a loading screen is up or slot 2 is busy (Create an Entrance's preview).
constexpr u32 kMusicManager = 0x83231754u;  // M = [[kMusicManager] + 52]

extern "C" REX_FUNC(sub_821D6368) {
  const u32 song = ctx.r3.u32;
  const u32 caller = u32(ctx.lr);
  const u32 screen = ctx.r31.u32;
  const bool theme_on = REXCVAR_GET(wwe12_jukebox) && g_menus && g_step != Step::Idle && g_step != Step::GameSong;
  bool take_over = false;
  if (REXCVAR_GET(wwe12_jukebox) && REXCVAR_GET(wwe12_jukebox_keep_theme) && !g_menus && g_step == Step::Idle &&
      !Wwe12LoadingScreenUp()) {
    const u32 holder = bd::mem::try_load<u32>(kMusicManager);
    const u32 m = holder ? bd::mem::try_load<u32>(holder + 52) : 0;
    PPCContext c = ctx;
    take_over = m && !SlotBusy(c, base, m, kThemeSlot);
  }
  const bool skip = (theme_on && REXCVAR_GET(wwe12_jukebox_keep_theme)) || take_over;
  if (REXCVAR_GET(wwe12_jukebox_log_screens)) {
    static u32 s_last[4] = {};
    const u32 vtable = bd::mem::try_load<u32>(screen);
    const u32 now[4] = {song, caller, vtable, skip ? 1u : 0u};
    if (std::memcmp(now, s_last, sizeof now) != 0) {
      std::memcpy(s_last, now, sizeof now);
      BD_INFO("[jukebox] a screen starts the menu music on player 1: song {}, from {:08X}, screen {:08X} "
              "(vtable {:08X}){}",
              song, caller, screen, vtable,
              take_over ? ": skipped, the jukebox takes over"
                        : (skip ? ": skipped, the theme goes on"
                                : (theme_on ? ": played with the theme (keep_theme off)" : "")));
    }
  }
  if (take_over) {
    g_menus = true;
    Go(Step::Stop);  // (run by the next update: slots 0-2 stopped, player 1 included, then a theme)
  }
  if (skip) {
    ctx.r3.u64 = 1;
    return;
  }
  __imp__sub_821D6368(ctx, base);
}

// ---- MY PLAYLIST: opening, the pad ------------------------------------------------------------

// From the JUKEBOX page (wwe12_graphics_menu.cpp, yOption.InitInGameOptions(104 + handler - 1)): the screen opens. Game
// thread.
void Wwe12JukeboxOpenPlaylist(int handler) {
  if (g_list_open.load())
    return;
  LoadThemes();
  LoadPlaylist();
  {
    std::lock_guard<std::mutex> lock(g_list_mutex);
    g_list = g_themes;
    g_cursor = 0;
    for (int i = 0; i < int(g_list.size()); ++i)
      if (g_list[i].number == g_playing)
        g_cursor = i;
    g_note.clear();
  }
  int out = 0;
  {
    std::lock_guard<std::mutex> lock(g_list_mutex);
    for (const Theme &t : g_list)
      out += g_off.count(t.number) ? 1 : 0;
  }
  BD_INFO("[jukebox] MY PLAYLIST opened (page handler FromSub{}): {} themes, {} left out", handler, g_list.size(), out);
  g_list_open = true;
  if (!g_overlay_up.exchange(true)) {
    if (Wwe12OverlayDrawer())
      Wwe12OverlayPost([] {
        if (rex::ui::ImGuiDrawer *d = Wwe12OverlayDrawer())
          new PlaylistOverlay(d);  // deletes itself once the screen is closed
        else
          g_overlay_up = false;
      });
    else {
      g_overlay_up = false;
      g_list_open = false;
      BD_WARN("[jukebox] MY PLAYLIST: no overlay drawer, screen not opened");
    }
  }
}

// From the XInputGetState hook (wwe12_intros.cpp), every pad: -1 = the screen is open, hide the whole pad (buttons,
// triggers, sticks) from the game; else the buttons to hide (still held from the screen).
int Wwe12JukeboxPad(uint32_t user, uint16_t buttons, int16_t lx, int16_t ly) {
  if (user >= 4)
    return 0;
  const u16 before = g_pad_before[user];
  g_pad_before[user] = buttons;
  if (!g_list_open.load()) {
    g_hold[user] &= buttons;
    return g_hold[user];
  }
  constexpr u16 kUp = 0x0001, kDown = 0x0002, kLeft = 0x0004, kRight = 0x0008, kStart = 0x0010, kLb = 0x0100,
                kRb = 0x0200, kA = 0x1000, kB = 0x2000, kX = 0x4000, kY = 0x8000;
  // directions: the D-pad or the left stick, again and again while held (after 0.4 s, every 0.09 s)
  u16 dir = u16(buttons & (kUp | kDown | kLeft | kRight));
  if (ly > 16000) dir |= kUp;
  if (ly < -16000) dir |= kDown;
  if (lx < -16000) dir |= kLeft;
  if (lx > 16000) dir |= kRight;
  static u16 s_dir[4] = {};
  static int64_t s_next[4] = {};
  const int64_t now = NowMs();
  u16 step = 0;
  if (dir != s_dir[user]) {
    step = u16(dir & ~s_dir[user]);
    s_next[user] = now + 400;
  } else if (dir && now >= s_next[user]) {
    step = dir;
    s_next[user] = now + 90;
  }
  s_dir[user] = dir;
  const u16 newly = u16(buttons & ~before);
  std::lock_guard<std::mutex> lock(g_list_mutex);
  const int count = int(g_list.size());
  if (count == 0 || (newly & (kB | kStart))) {
    SavePlaylist(g_list);
    g_list_open = false;
    g_hold[user] = buttons;
    BD_INFO("[jukebox] MY PLAYLIST closed");
    return -1;
  }
  int c = g_cursor;
  if (step & kUp) c -= 1;
  if (step & kDown) c += 1;
  if (step & kLeft) c -= kRows;
  if (step & kRight) c += kRows;
  if (newly & kLb) c -= kPerPage;
  if (newly & kRb) c += kPerPage;
  if (c != g_cursor) {
    if (newly & (kLb | kRb))  // (a page: wraps around)
      c = ((c % count) + count) % count;
    g_cursor = std::clamp(c, 0, count - 1);
  }
  const Theme &t = g_list[g_cursor];
  if (newly & kA) {
    if (!g_off.erase(t.number))
      g_off.insert(t.number);
    BD_INFO("[jukebox] MY PLAYLIST: {} ({}) {}", t.number, t.name, g_off.count(t.number) ? "left out" : "in");
  }
  if (newly & kY) {
    bool all = true;
    for (const Theme &e : g_list)
      all = all && !g_off.count(e.number);
    for (const Theme &e : g_list)
      if (all)
        g_off.insert(e.number);
      else
        g_off.erase(e.number);
    BD_INFO("[jukebox] MY PLAYLIST: {}", all ? "none" : "all");
  }
  if (newly & kX) {
    if (REXCVAR_GET(wwe12_jukebox)) {
      g_listen_ask = t.number;
      BD_INFO("[jukebox] MY PLAYLIST: listen to {} ({})", t.number, t.name);
    } else {
      Note(PW().listen_off);
    }
  }
  return -1;
}
