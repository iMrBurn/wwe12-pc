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
#include <random>
#include <vector>

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
REXCVAR_DECLARE(uint32_t, user_language);

REX_EXTERN(__imp__sub_8271BFD0);
REX_EXTERN(__imp__sub_8271F080);
REX_EXTERN(__imp__sub_8271BB60);
REX_EXTERN(__imp__sub_8271BB00);
REX_EXTERN(__imp__sub_8271BC90);
REX_EXTERN(__imp__sub_8271BF58);
REX_EXTERN(__imp__sub_8271BD28);
REX_EXTERN(__imp__sub_8271BEE0);

#include "wwe12_jukebox_page.inc"

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
     "GAME MENU MUSIC TOO", "NO", "YES", "MENU MUSIC"},
    {"JUKEBOX", "ORDRE", "TH\xC3\x88ME", "D\xC3\x89SACTIV\xC3\x89", "ACTIV\xC3\x89", "AL\xC3\x89" "ATOIRE",
     "TH\xC3\x88ME CHOISI SEULEMENT", "TH\xC3\x88ME ", "MUSIQUE DES MENUS (JEU)", "MUSIQUE DES MENUS DU JEU AUSSI",
     "NON", "OUI", "MUSIQUE DES MENUS"},
    {"JUKEBOX", "REIHENFOLGE", "THEMA", "AUS", "AN", "ZUF\xC3\x84LLIG", "NUR GEW\xC3\x84HLTES THEMA", "THEMA ",
     "MEN\xC3\x9CMUSIK DES SPIELS", "AUCH MEN\xC3\x9CMUSIK DES SPIELS", "NEIN", "JA", "MEN\xC3\x9CMUSIK"},
    {"JUKEBOX", "ORDEN", "TEMA", "NO", "S\xC3\x8D", "ALEATORIO", "SOLO EL TEMA ELEGIDO", "TEMA ",
     "M\xC3\x9ASICA DE LOS MEN\xC3\x9AS (JUEGO)", "TAMBI\xC3\x89N M\xC3\x9ASICA DEL JUEGO", "NO", "S\xC3\x8D",
     "M\xC3\x9ASICA DE LOS MEN\xC3\x9AS"},
    {"JUKEBOX", "ORDINE", "TEMA", "NO", "S\xC3\x8C", "CASUALE", "SOLO IL TEMA SCELTO", "TEMA ",
     "MUSICA DEI MEN\xC3\x99 (GIOCO)", "ANCHE LA MUSICA DEL GIOCO", "NO", "S\xC3\x8C", "MUSICA DEI MEN\xC3\x99"},
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

void Go(Step s) {
  g_step = s;
  g_frames = 0;
}

u32 NextTheme() {
  if (g_themes.empty())
    LoadThemes();
  if (g_themes.empty())
    return 0;
  const u32 chosen = u32(REXCVAR_GET(wwe12_jukebox_theme));
  const bool play_chosen = g_play_chosen;
  g_play_chosen = false;
  if (REXCVAR_GET(wwe12_jukebox_order) == 1 || play_chosen)
    return g_themes[ThemeIndex(chosen)].number;
  std::vector<u32> pool;
  for (const Theme &t : g_themes)
    if (t.number != g_playing && (t.number != kGameMusic || REXCVAR_GET(wwe12_jukebox_game_music)) &&
        std::find(g_failed.begin(), g_failed.end(), t.number) == g_failed.end())
      pool.push_back(t.number);
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
  const u32 before = bd::mem::try_field<u32>(m, 40);
  __imp__sub_8271BFD0(ctx, base);
  if (g_give_back.exchange(false)) {
    if (g_menus)
      GiveBack(saved, base, m);
    return;
  }
  const bool picked = bd::mem::try_field<u32>(m, 40) != before;
  if (g_restart.exchange(false)) {
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
