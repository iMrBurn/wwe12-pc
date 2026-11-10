// THQ's Title Update fixes, redone as hooks on the disc's code (1.3, "way B": docs/title_update_corrections.md).
// Each fix has its own setting. Addresses are the disc's (v0.0.0.4); what TU2 (v0.0.2.4) does is
// quoted from the patched image compared with ours (work/tu_study, never in the repo).
//
// 1. Created Superstars in a show's roster (TU2, wwe12_tu_caw_roster). The disc asks "a created
//    Superstar?" (sub_82A330D8(id) == 1, ids < 50) in three places and TU2 removed those calls:
//    - sub_82583B78, EDIT BRAND: a created Superstar was skipped when counting the chosen brands'
//      Superstars (call returning to 0x82583BD4): now counted;
//    - sub_82599DB0, a contract: same in its count of the show (returns to 0x82599F94): now counted;
//    - sub_826D3B58, "may this Superstar leave his show?": a created Superstar could always leave
//      (second call, returns to 0x826D3C24); now he is checked like the others (show > 15).
//    TU2 also counts the created Superstars in that last roster (sub_826D0598's 3rd argument
//    0 -> 1, call returning to 0x826D3C84: 0 starts its count at id 100, non-zero at 0). That call and the Universe line's own EDIT BRAND recount are hooked
//    in wwe12_universe.cpp: they ask Wwe12TuCawRoster().
//
// 2. Title index checks (TU2, wwe12_tu_title_index). The Universe keeps 23 titles' records of 964
//    bytes at U + 1492 (and a second set at U + 23664). TU2 checks the title index (< 23) before
//    reading a record in sub_826E1E60 (r4), sub_826E21A8 (r4), sub_826E23C0 (r5) and sub_826E24F8
//    (r5, cases 0 and 16 of its kind byte); out of range, the answer is 512 ("nobody"), as the
//    functions' own failure. sub_826E1F08's TU2 change is something else (a sub_826CE440 test), not done.
//
// 3. Probe of the three settings TU2 changed (wwe12_tu_tune_probe, off by default, changes nothing).
//    sub_82276590 fills 64 settings at start; TU2 changes 0x831740C0 / C4 / C8 from 40 / 60 / 80 to
//    70 / 80 / 90. Their only reader is sub_822769C8(level): level 1 -> [C8], 2 -> [C4], 3 -> [C0],
//    else 100, returned as a factor (value / 100). Its only caller is sub_8223BF68 (a damage
//    computation called from many places): factor = sub_8223BF68's f30 x the level of the object
//    sub_82327550(r28) +156. The probe logs each call (level, the caller's registers) to tell what
//    the level is; the TU2 values themselves are not applied.

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(wwe12_tu_caw_roster, true, "WWE12",
                    "Title Update fix: created Superstars count in a Universe show's roster (EDIT BRAND, contracts) and are checked before leaving a show.");
REXCVAR_DEFINE_BOOL(wwe12_tu_title_index, true, "WWE12",
                    "Title Update fix: a Universe title number out of range (23 titles) is refused instead of reading past the titles' records.");
REXCVAR_DEFINE_BOOL(wwe12_tu_tune_probe, false, "WWE12",
                    "Log the use of three gameplay settings changed by the Title Update (study, changes nothing).");

#include <atomic>

#include "svr_hook.h"
#include "core/logging.h"
#include "core/memory_helpers.h"

bool Wwe12TuCawRoster() { return REXCVAR_GET(wwe12_tu_caw_roster); }

// ---- 1. created Superstars in a show's roster ----------------------------------------------------

REX_EXTERN(__imp__sub_82A330D8);  // a created Superstar? (id)

namespace {

constexpr u32 kEditBrandCaw = 0x82583BD4;  // sub_82583B78: skipped from the brands' count
constexpr u32 kContractCaw = 0x82599F94;   // sub_82599DB0: skipped from the show's count
constexpr u32 kLeaveCaw = 0x826D3C24;      // sub_826D3B58: may always leave

std::atomic<int> g_caw_logged{0};

const char *CawPlace(u32 from) {
  switch (from) {
  case kEditBrandCaw: return "EDIT BRAND";
  case kContractCaw: return "contrat";
  case kLeaveCaw: return "depart d'une emission";
  default: return nullptr;
  }
}

}  // namespace

extern "C" REX_FUNC(sub_82A330D8) {
  const u32 from = u32(ctx.lr);
  const u32 id = ctx.r3.u32;
  __imp__sub_82A330D8(ctx, base);
  if (ctx.r3.u32 != 1 || !REXCVAR_GET(wwe12_tu_caw_roster))
    return;
  const char *place = CawPlace(from);
  if (!place)
    return;
  ctx.r3.u64 = 0;  // TU2: this call is gone, the created Superstar is treated like the others
  if (g_caw_logged.fetch_add(1) < 40)
    BD_INFO("[tu] catcheur cree {} compte comme les autres ({})", id, place);
}

// ---- 2. title index checks -----------------------------------------------------------------------

REX_EXTERN(__imp__sub_826E1E60);
REX_EXTERN(__imp__sub_826E21A8);
REX_EXTERN(__imp__sub_826E23C0);
REX_EXTERN(__imp__sub_826E24F8);

namespace {

constexpr u32 kTitles = 23;
constexpr u32 kNobody = 512;
std::atomic<int> g_title_logged{0};

// True when the call is refused (r3 = 512, the function's own failure answer).
bool RefuseTitle(PPCContext &ctx, const char *fn, u32 title) {
  if (title < kTitles || !REXCVAR_GET(wwe12_tu_title_index))
    return false;
  if (g_title_logged.fetch_add(1) < 40)
    BD_WARN("[tu] titre {} hors limites dans {} (appel de {:08X}) : refuse", title, fn, u32(ctx.lr));
  ctx.r3.u64 = kNobody;
  return true;
}

}  // namespace

// (U, title r4, place r5, r6 < 2, set r7 <= 1): a holder of a title's record
extern "C" REX_FUNC(sub_826E1E60) {
  if (!RefuseTitle(ctx, "sub_826E1E60", ctx.r4.u32))
    __imp__sub_826E1E60(ctx, base);
}

// (U, title r4, r5, set r6 <= 1)
extern "C" REX_FUNC(sub_826E21A8) {
  if (!RefuseTitle(ctx, "sub_826E21A8", ctx.r4.u32))
    __imp__sub_826E21A8(ctx, base);
}

// (U, entry r4: kind byte 3-18, title r5, r6)
extern "C" REX_FUNC(sub_826E23C0) {
  if (!RefuseTitle(ctx, "sub_826E23C0", ctx.r5.u32))
    __imp__sub_826E23C0(ctx, base);
}

// (U, entry r4: kind byte 0-18, title r5 for kinds 0 and 16; kinds 3, 4, 17, 18 go to sub_826E23C0)
extern "C" REX_FUNC(sub_826E24F8) {
  const u32 entry = ctx.r4.u32;
  const u8 kind = entry ? bd::mem::load<u8>(entry) : 0xFF;
  if ((kind == 0 || kind == 16) && RefuseTitle(ctx, "sub_826E24F8", ctx.r5.u32))
    return;
  __imp__sub_826E24F8(ctx, base);
}

// ---- 3. probe of the three settings --------------------------------------------------------------

REX_EXTERN(__imp__sub_822769C8);  // a factor for a level 1-3 (settings 0x831740C8 / C4 / C0)

extern "C" REX_FUNC(sub_822769C8) {
  if (!REXCVAR_GET(wwe12_tu_tune_probe)) {
    __imp__sub_822769C8(ctx, base);
    return;
  }
  // A leaf called from sub_8223BF68: r25 / r28 / r31 still hold the caller's values
  // (r31 = its first argument, r25 = its second, r28 = the object whose +156 is the level).
  const u32 level = ctx.r3.u32;
  const double before = ctx.f30.f64;
  const u32 r31 = ctx.r31.u32, r25 = ctx.r25.u32, r28 = ctx.r28.u32, from = u32(ctx.lr);
  __imp__sub_822769C8(ctx, base);
  int who = -1, other = -1;
  for (int i = 0; i < 6; ++i) {
    const u32 actor = bd::mem::try_load<u32>(0x8317B770 + i * 4);
    if (actor && actor == r31) who = i;
    if (actor && actor == r28) other = i;
  }
  static std::atomic<int> logged{0};
  if (logged.fetch_add(1) < 400)
    BD_INFO("[tu_reglage] niveau {} facteur {:.2f} valeur {:.3f} -> {:.3f} | r31 {:08X} (acteur {}) r25 {} "
            "r28 {:08X} (acteur {}) appel {:08X} reglages {}/{}/{}",
            level, ctx.f1.f64, before, before * ctx.f1.f64, r31, who, r25, r28, other, from,
            bd::mem::try_load<u32>(0x831740C8), bd::mem::try_load<u32>(0x831740C4),
            bd::mem::try_load<u32>(0x831740C0));
}
