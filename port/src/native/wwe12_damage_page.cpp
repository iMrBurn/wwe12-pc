// OPTIONS > GAMEPLAY > MATCH OPTIONS > BALANCING > DAMAGE (1.4, session Gameplay): the game's page with the match
// modes' settings below its two damage sliders: SQUASH CHANCE, SQUASH HEALTH, STARTING FINISHERS (the cvars of
// wwe12_match_modes.cpp), saved to wwe12.toml.
//
// Why there (2026-11-15, user's choice "in BALANCING"): BALANCING > GENERAL uses all 9 rows of its frame and
// GAMEPLAY > MATCH OPTIONS has no free entry (IN-GAME OPTIONS, BALANCING, SOUND); DAMAGE has 2 sliders, and the
// layout LOPN's frame DrawAIReversal has 6 slider rows. Our page script (tools/damage_page.py,
// wwe12_damage_page.inc) is the game's DAMAGE script ("/MENS/DMGB", 6148 bytes) rewritten on that frame.
// - The script is swapped in luaL_loadbuffer (sub_827E9418, hooked in wwe12_graphics_menu.cpp, which calls
//   Wwe12DamagePageLoad first).
// - Our rows' values: the script calls yOption.InitInGameOptions(130) / UpdateInGameOptions(130) (sub_82DEDD70 /
//   sub_82DEC528, hooked in wwe12_graphics_menu.cpp, which call Wwe12DamagePageNative first): the page array's
//   words 20-22 = values, 23-25 = maxima (the game's own rows use words 0-5).
// - Labels: text ids 44100-44104 (wwe12_match_modes.cpp's text hook asks Wwe12DamagePageText).

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(wwe12_damage_page, true, "WWE12",
                    "OPTIONS > GAMEPLAY > MATCH OPTIONS > BALANCING > DAMAGE also shows the match modes' settings "
                    "(squash chance / health, starting finishers).");
REXCVAR_DECLARE(int32_t, wwe12_squash_chance);
REXCVAR_DECLARE(int32_t, wwe12_squash_health);
REXCVAR_DECLARE(int32_t, wwe12_starting_finishers);

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <string>

#include <rex/filesystem.h>
#include <rex/runtime.h>

#include "../launcher/toml_file.h"
#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REX_EXTERN(__imp__sub_827E74A8);  // lua_tointeger (L, index)

namespace {

#include "wwe12_damage_page.inc"

constexpr u32 kDamageScriptSize = 6148;  // "/MENS/DMGB"
constexpr int kPageArg = 130;
constexpr u32 kOurs = 20;  // first word of our rows in the page array
constexpr u32 kLabel = 44100;

bool g_open = false;  // our DAMAGE page is the page shown

u32 PageArray() {
  const u32 index = bd::mem::try_load<u32>(0x8306B718);
  const u32 page = bd::mem::try_load<u32>(0x8317C430 + index * 4);
  return page ? page + 852 * 4 : 0;
}

// slider positions: chance 0-20 (x 5 %), health 1-18 (x 5 %; 0 reads as 5 %), finishers 0-4
constexpr int kMax[3] = {20, 18, 4};
int Slider(int row) {
  switch (row) {
  case 0: return std::clamp<int32_t>(REXCVAR_GET(wwe12_squash_chance), 0, 100) / 5;
  case 1: return std::clamp<int32_t>(REXCVAR_GET(wwe12_squash_health), 5, 90) / 5;
  default: return std::clamp<int32_t>(REXCVAR_GET(wwe12_starting_finishers), 0, 4);
  }
}

void Save() {
  const auto path = rex::filesystem::GetExecutableFolder() / "wwe12.toml";
  TomlFile f;
  if (!f.Load(path)) {
    BD_WARN("[damage page] cannot read {}", rex::path_to_utf8(path));
    return;
  }
  f.SetInt("wwe12_squash_chance", REXCVAR_GET(wwe12_squash_chance));
  f.SetInt("wwe12_squash_health", REXCVAR_GET(wwe12_squash_health));
  f.SetInt("wwe12_starting_finishers", REXCVAR_GET(wwe12_starting_finishers));
  if (f.Save())
    BD_INFO("[damage page] settings saved to {}", rex::path_to_utf8(path));
}

std::string Percent(int v) { return std::to_string(v) + "%"; }

}  // namespace

// From the luaL_loadbuffer hook (wwe12_graphics_menu.cpp): our page instead of the game's DAMAGE page.
void Wwe12DamagePageLoad(PPCContext &ctx) {
  const bool damage = ctx.r5.u32 == kDamageScriptSize && bd::mem::try_load<u8>(ctx.r4.u32) == 0x1B;
  g_open = false;
  if (!damage || !REXCVAR_GET(wwe12_damage_page))
    return;
  static u32 s_buffer = 0;
  if (!s_buffer)
    s_buffer = REX_KERNEL_MEMORY()->SystemHeapAlloc(u32(sizeof(kDamagePage)), 0x20);
  if (!s_buffer)
    return;
  std::memcpy(bd::mem::at<u8>(s_buffer), kDamagePage, sizeof(kDamagePage));
  ctx.r4.u64 = s_buffer;
  ctx.r5.u64 = sizeof(kDamagePage);
  g_open = true;
  BD_INFO("[damage page] BALANCING > DAMAGE with the match modes ({} bytes)", sizeof(kDamagePage));
}

// From the InitInGameOptions / UpdateInGameOptions hooks (wwe12_graphics_menu.cpp): true = handled (our page's 130).
bool Wwe12DamagePageNative(bool update, PPCContext &ctx, u8 *base) {
  if (!g_open)
    return false;
  PPCContext x = ctx;
  x.r4.u64 = ~uint64_t(0);  // the top of the Lua stack
  __imp__sub_827E74A8(x, base);
  if (int(x.r3.u32) != kPageArg)
    return false;
  const u32 a = PageArray();
  if (a && !update) {
    for (u32 r = 0; r < 3; ++r) {
      bd::mem::store<u32>(a + (kOurs + r) * 4, u32(Slider(int(r))));
      bd::mem::store<u32>(a + (kOurs + 3 + r) * 4, u32(kMax[r]));
    }
  } else if (a) {
    const int v0 = std::clamp<int>(int(bd::mem::load<u32>(a + kOurs * 4)), 0, kMax[0]);
    const int v1 = std::clamp<int>(int(bd::mem::load<u32>(a + (kOurs + 1) * 4)), 0, kMax[1]);
    const int v2 = std::clamp<int>(int(bd::mem::load<u32>(a + (kOurs + 2) * 4)), 0, kMax[2]);
    const bool changed = v0 != Slider(0) || std::max(1, v1) != Slider(1) || v2 != Slider(2);
    REXCVAR_SET(wwe12_squash_chance, v0 * 5);
    REXCVAR_SET(wwe12_squash_health, std::max(1, v1) * 5);
    REXCVAR_SET(wwe12_starting_finishers, v2);
    BD_INFO("[damage page] squash {} %, jobber health {} %, starting finishers {}{}", v0 * 5, std::max(1, v1) * 5, v2,
            changed ? "" : " (no change)");
    if (changed)
      Save();
  }
  ctx.r3.u64 = 0;
  return true;
}

// The labels (wwe12_match_modes.cpp's text hook): "" = not ours.
std::string Wwe12DamagePageText(uint32_t id) {
  if (id < kLabel || id > kLabel + 4 || !REXCVAR_GET(wwe12_damage_page))
    return {};
  const u32 a = g_open ? PageArray() : 0;
  const auto live = [&](u32 r) { return a ? int(bd::mem::try_load<u32>(a + (kOurs + r) * 4)) : Slider(int(r)); };
  switch (id - kLabel) {
  case 0: return "SQUASH CHANCE   " + Percent(std::clamp(live(0), 0, 20) * 5);
  case 1: return "SQUASH: JOBBER HEALTH   " + Percent(std::max(1, std::clamp(live(1), 0, 18)) * 5);
  case 2: {
    static const char *const k[5] = {"NORMAL", "1", "2", "3", "INFINITE"};
    return std::string("STARTING FINISHERS   ") + k[std::clamp(live(2), 0, 4)];
  }
  case 3: return "DAMAGE: PLAYERS";  // (the game's two rows: their names, user's screenshot pending)
  default: return "DAMAGE: CPU";
  }
}
