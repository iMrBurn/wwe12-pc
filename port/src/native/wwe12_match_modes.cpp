// Match modes for 1.4 (session Gameplay): starting finishers (as WWE 2K14) and squash matches, plus the probe
// that found what they use. All off by default.
//
// Found (code read 2026-10-28 / 2026-11-14, probe test of the user 2026-11-14, test_finishers.log):
// - Momentum gauge: the 'CHP ' component of an actor ([actor+3436], sub_827D5C28), object = component + 24,
//   [obj+32] = the actor. obj+152 = gauge (float, 1 = full). A FULL GAUGE IS THE SIGNATURE, not the finisher
//   (the user's test: a full gauge at the start gave "my signature"). Read by sub_82246DE0 (often, on the game
//   thread), written by sub_82246E48(obj, f1). When full, sub_8224BB58 (per frame) sets +188 = 1200 and counts it
//   down; at 0 the gauge goes to 0 (setter call returning to 0x8224BEAC) and the signature becomes a finisher
//   (sub_8224B008: actor+2776 + 1). A signature done: sub_82349910 sets the gauge to 0.05 (call returning to
//   0x82349960), +2776 + 1 as well.
// - Stored finishers: actor+2776 (0 = none; sub_822480F0 = "has a finisher"; gained up to sub_82248000: 3 for
//   some superstars, else 1). Set to 0 by sub_82349980 and when a match starts (sub_8222B9xx); it went down one
//   by one as finishers were done in the first test.
// - Health: 4 u16 at [actor+3364] + 56, in this order: head, body, arms, legs (the script command SetHP,
//   sub_823D5298, writes all four). Full = 120 in an exhibition (a Royal Rumble entrant: 110); they go down with
//   the hits on each place and below 0 (65535...) when worn out.
//
// Starting finishers (wwe12_starting_finishers: 0 = off, 1-3 = that many, 4 = infinite): each superstar of a
// match starts with that many stored finishers (actor+2776; the gauge is left alone). Each time the game takes
// one away (a finisher done), the ones still owed are put back; infinite keeps at least one.
// Squash: a 1 on 1 match is one with the chance wwe12_squash_chance (0-100 %, as WWE 2K19's slider; the superstar
// with the lower overall is the jobber), or always with wwe12_squash (1 = the 2nd superstar is the jobber, 2 = the
// 1st; for tests). The
// jobber's four health values start at wwe12_squash_health percent of full, the other one with a stored finisher.
// A new superstar in a place (actor pointer changed), or one that looks fresh after a loading screen, is a new
// start: set at his first gauge read, and the jobber's health set again for 5 s if the game puts it back to full.

#include <rex/cvar.h>

REXCVAR_DEFINE_INT32(wwe12_starting_finishers, 0, "WWE12",
                     "Finishers every superstar has at the start of a match: 0 = the game's rules, 1-3, 4 = infinite.");
REXCVAR_DEFINE_INT32(wwe12_squash, 0, "WWE12",
                     "Squash match (1 on 1): 0 = off, 1 = the second superstar is the jobber (low health), 2 = the first.");
REXCVAR_DEFINE_INT32(wwe12_squash_chance, 0, "WWE12",
                     "Squash match: chance (0-100 %) that a 1 on 1 match is a squash; the superstar with the lower overall is the jobber.");
REXCVAR_DEFINE_INT32(wwe12_squash_health, 20, "WWE12",
                     "Squash match: the jobber's starting health, in percent of full (5-90).");
REXCVAR_DEFINE_BOOL(wwe12_match_probe, false, "WWE12",
                    "Log the finisher gauge and the health of the superstars in a match (study, changes nothing).");

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <random>
#include <string>
#include <set>
#include <thread>
#include <vector>

#include <fmt/format.h>

#include <rex/runtime.h>

#include "svr_hook.h"
#include "core/logging.h"
#include "core/memory_helpers.h"

int64_t Wwe12MsSinceLoadScreen();  // wwe12_load_turbo.cpp

REX_EXTERN(__imp__sub_82684910);  // a superstar's record (roster id)
REX_EXTERN(__imp__sub_82A339B0);  // a record's overall
REX_EXTERN(__imp__sub_82246DE0);  // gauge of a CHP object
REX_EXTERN(__imp__sub_82246E48);  // set the gauge (obj, f1)
REX_EXTERN(__imp__sub_8224B008);  // finisher count +1 (obj)
REX_EXTERN(__imp__sub_822483F0);  // full gauge taken away (obj)
REX_EXTERN(__imp__sub_82248000);  // finishers per full gauge (obj)

namespace {

constexpr u32 kActors = 0x8317B770;
constexpr int kStars = 6;
constexpr int kInfinite = 4;
constexpr u32 kStock = 2776;  // actor+2776: stored finishers (0 = none; sub_822480F0 = "has a finisher")

using Clock = std::chrono::steady_clock;

// game thread only (the gauge hooks), except g_obj read by the probe
std::atomic<u32> g_obj[kStars];
u32 g_actor[kStars] = {};            // the actor each place was started for
bool g_recheck[kStars] = {};         // a loading screen since: the same actor may be a new match
int g_left[kStars] = {};             // finishers still given (starting finishers)
u32 g_seen[kStars] = {};             // actor+2776 at the last read
Clock::time_point g_started_at[kStars];
u32 g_hold[kStars] = {};             // stored finishers given at the start, held 3 s against the game's reset
u16 g_squash_hp[kStars] = {};        // jobber's target health (0 = none)
u16 g_squash_full[kStars] = {};
Clock::time_point g_squash_until[kStars];
std::once_flag g_probe_started;

bool Probe() { return REXCVAR_GET(wwe12_match_probe); }

int StarOf(u32 actor) {
  if (!actor)
    return -1;
  for (int i = 0; i < kStars; ++i)
    if (bd::mem::try_load<u32>(kActors + i * 4) == actor)
      return i;
  return -1;
}

int Stars() {
  int n = 0;
  for (int i = 0; i < kStars; ++i)
    n += bd::mem::try_load<u32>(kActors + i * 4) != 0;
  return n;
}

float F(u32 at) {
  const u32 v = bd::mem::try_load<u32>(at);
  float f;
  std::memcpy(&f, &v, 4);
  return f;
}

int ChosenFinishers(int i);  // below (the character select)

int Finishers() { return std::clamp<int32_t>(REXCVAR_GET(wwe12_starting_finishers), 0, kInfinite); }

void SetHealth(u32 actor, u16 v) {
  const u32 dmg = bd::mem::try_load<u32>(actor + 3364);
  if (dmg)
    for (u32 k = 0; k < 4; ++k)
      bd::mem::store<u16>(dmg + 56 + k * 2, v);
}

// A superstar's first gauge read in his place: the match modes' start.
u32 GuestCall(void (*fn)(PPCContext &, u8 *), const PPCContext &in, u8 *base, u32 r3) {
  PPCContext c = in;
  c.r1.u64 = (in.r1.u32 - 512) & ~15u;
  c.r3.u64 = r3;
  fn(c, base);
  return c.r3.u32;
}

// A superstar's overall (RosterLib GetOverAll: roster id = [actor+80] / 100, sub_82684910, sub_82A339B0).
int Overall(const PPCContext &ctx, u8 *base, u32 actor) {
  const u32 id = bd::mem::try_load<u32>(actor + 80) / 100;
  if (id >= 373)
    return -1;
  const u32 rec = GuestCall(__imp__sub_82684910, ctx, base, id);
  return rec ? int(GuestCall(__imp__sub_82A339B0, ctx, base, rec)) : -1;
}

// The squash of this 1 on 1 match: decided once per pair of superstars. -1 = not a squash, else the jobber's place.
u64 g_squash_pair = 0;
int g_squash_jobber = -1;
std::mt19937 g_rng{std::random_device{}()};

int Jobber(const PPCContext &ctx, u8 *base) {
  if (Stars() != 2)
    return -1;
  const u32 a0 = bd::mem::try_load<u32>(kActors), a1 = bd::mem::try_load<u32>(kActors + 4);
  if (!a0 || !a1)
    return -1;
  const u64 pair = (u64(a0) << 32) | a1;
  if (pair == g_squash_pair)
    return g_squash_jobber;
  g_squash_pair = pair;
  g_squash_jobber = -1;
  const int forced = REXCVAR_GET(wwe12_squash);
  if (forced == 1 || forced == 2) {
    g_squash_jobber = forced == 1 ? 1 : 0;
    return g_squash_jobber;
  }
  const int chance = std::clamp<int32_t>(REXCVAR_GET(wwe12_squash_chance), 0, 100);
  if (chance <= 0)
    return -1;
  const int roll = std::uniform_int_distribution<int>(1, 100)(g_rng);
  const int o0 = Overall(ctx, base, a0), o1 = Overall(ctx, base, a1);
  if (roll > chance) {
    BD_INFO("[match] squash : pas cette fois (tirage {} sur {} %)", roll, chance);
    return -1;
  }
  g_squash_jobber = o0 < o1 ? 0 : o1 < o0 ? 1 : std::uniform_int_distribution<int>(0, 1)(g_rng);
  BD_INFO("[match] squash ! (tirage {} sur {} %) notes {} / {} : catcheur {} est le jobber", roll, chance, o0, o1,
          g_squash_jobber);
  return g_squash_jobber;
}

void GiveStock(int i, u32 actor, int give) {
  bd::mem::store<u32>(actor + kStock, u32(give));
  g_seen[i] = u32(give);
  g_hold[i] = u32(give);
  g_started_at[i] = Clock::now();
}

void Start(int i, u32 actor) {
  g_actor[i] = actor;
  g_left[i] = ChosenFinishers(i);
  g_squash_hp[i] = 0;
  g_hold[i] = 0;
  if (i < 2)
    g_squash_pair = 0;  // a new 1 on 1: its squash is decided again
  g_seen[i] = bd::mem::try_load<u32>(actor + kStock);
  g_started_at[i] = Clock::now();
  if (g_left[i] > 0) {
    GiveStock(i, actor, g_left[i] == kInfinite ? 1 : g_left[i]);
    BD_INFO("[match] catcheur {} : {} finisher(s) au depart", i,
            g_left[i] == kInfinite ? std::string("infini") : std::to_string(g_left[i]));
  }
}

// Once both superstars of a 1 on 1 are started: the squash, if this match is one.
void ApplySquash(const PPCContext &ctx, u8 *base) {
  const u32 a0 = bd::mem::try_load<u32>(kActors), a1 = bd::mem::try_load<u32>(kActors + 4);
  if (Stars() != 2 || !a0 || !a1 || g_actor[0] != a0 || g_actor[1] != a1)
    return;
  if (((u64(a0) << 32) | a1) == g_squash_pair)
    return;
  const int jobber = Jobber(ctx, base);
  if (jobber < 0)
    return;
  const int dom = 1 - jobber;
  const u32 actor = jobber ? a1 : a0;
  const u32 dmg = bd::mem::try_load<u32>(actor + 3364);
  const u16 now = dmg ? bd::mem::try_load<u16>(dmg + 56) : 0;
  const u16 hp_full = now > 0 && now < 1000 ? now : 120;
  const int pct = std::clamp<int32_t>(REXCVAR_GET(wwe12_squash_health), 5, 90);
  g_squash_full[jobber] = hp_full;
  g_squash_hp[jobber] = u16(std::max(1, hp_full * pct / 100));
  g_squash_until[jobber] = Clock::now() + std::chrono::seconds(5);
  SetHealth(actor, g_squash_hp[jobber]);
  BD_INFO("[match] squash : catcheur {} commence a {} sur {} de vie", jobber, g_squash_hp[jobber], hp_full);
  const u32 dom_actor = dom ? a1 : a0;
  if (bd::mem::try_load<u32>(dom_actor + kStock) == 0) {
    GiveStock(dom, dom_actor, 1);
    BD_INFO("[match] squash : catcheur {} commence avec un finisher", dom);
  }
}

// Every gauge read of a started superstar: his stored finishers kept as owed (a use takes them to 0 or down by one).
void Keep(int i, u32 actor) {
  const u32 now = bd::mem::try_load<u32>(actor + kStock);
  if (g_hold[i] && now < g_hold[i] && Clock::now() < g_started_at[i] + std::chrono::seconds(3)) {
    bd::mem::store<u32>(actor + kStock, g_hold[i]);  // the game's own match start cleared them: not a finisher done
    return;
  }
  if (now < g_seen[i] && g_left[i] > 0) {
    if (g_left[i] != kInfinite)
      --g_left[i];  // one finisher used
    const u32 owed = g_left[i] == kInfinite ? 1u : u32(g_left[i]);
    if (now < owed) {
      bd::mem::store<u32>(actor + kStock, owed);
      BD_INFO("[match] catcheur {} : finisher utilise, {} encore", i,
              g_left[i] == kInfinite ? std::string("infini") : std::to_string(owed));
      g_seen[i] = owed;
      return;
    }
  }
  g_seen[i] = now;
}

// ---- probe (reads only) ----

std::string State(int i) {
  const u32 actor = bd::mem::try_load<u32>(kActors + i * 4);
  if (!actor)
    return {};
  const u32 dmg = bd::mem::try_load<u32>(actor + 3364);
  std::string hp = dmg ? fmt::format("{} {} {} {}", bd::mem::try_load<u16>(dmg + 56), bd::mem::try_load<u16>(dmg + 58),
                                     bd::mem::try_load<u16>(dmg + 60), bd::mem::try_load<u16>(dmg + 62))
                       : std::string("-");
  const u32 obj = g_obj[i].load();
  std::string gauge = "-";
  if (obj && bd::mem::try_load<u32>(obj + 32) == actor)
    gauge = fmt::format("jauge {:.2f} minuteur {} | +998 {} +2776 {}", F(obj + 152),
                        bd::mem::try_load<u32>(obj + 188) == 0xFFFFFFFF ? std::string("-1")
                                                                         : fmt::format("{:08X}", bd::mem::try_load<u32>(obj + 188)),
                        bd::mem::try_load<u8>(actor + 998), bd::mem::try_load<u32>(actor + 2776));
  return fmt::format("{:08X} vie {} | {}", actor, hp, gauge);
}

void Poll() {
  std::string last[kStars];
  const auto t0 = Clock::now();
  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count();
    for (int i = 0; i < kStars; ++i) {
      std::string s = State(i);
      if (s != last[i]) {
        if (!s.empty())
          BD_INFO("[vie] {} catcheur {} : {}", ms, i, s);
        last[i] = std::move(s);
      }
    }
  }
}

}  // namespace

extern "C" REX_FUNC(sub_82246DE0) {
  const u32 obj = ctx.r3.u32;
  const u32 actor = obj ? bd::mem::try_load<u32>(obj + 32) : 0;
  // A loading screen since the last read (its time went down): a new match, every place starts again (the same
  // actor addresses can come back in the next match).
  static int64_t s_since = INT64_MAX;
  const int64_t since = Wwe12MsSinceLoadScreen();
  if (since < s_since)
    std::fill(std::begin(g_recheck), std::end(g_recheck), true);
  s_since = since;
  const int i = StarOf(actor);
  if (i >= 0) {
    g_obj[i] = obj;
    if (Probe())
      std::call_once(g_probe_started, [] {
        std::thread(Poll).detach();
        BD_INFO("[finisher] sonde active");
      });
    bool fresh = actor != g_actor[i];
    if (!fresh && g_recheck[i]) {
      // Same address after a loading screen: a new match only if he looks like one (empty gauge, full health;
      // loading-screen calls also come without a new match).
      const u32 dmg = bd::mem::try_load<u32>(actor + 3364);
      fresh = F(obj + 152) == 0.f && bd::mem::try_load<u32>(obj + 188) == 0xFFFFFFFF && dmg &&
              bd::mem::try_load<u16>(dmg + 56) >= 100 && bd::mem::try_load<u16>(dmg + 56) < 1000;
    }
    g_recheck[i] = false;
    if (fresh) {
      Start(i, actor);
    } else {
      Keep(i, actor);
    }
    if (i < 2)
      ApplySquash(ctx, base);
    if (!fresh && g_squash_hp[i] && Clock::now() < g_squash_until[i]) {
      const u32 dmg = bd::mem::try_load<u32>(actor + 3364);
      if (dmg && bd::mem::try_load<u16>(dmg + 56) == g_squash_full[i]) {
        SetHealth(actor, g_squash_hp[i]);  // the game put the health back to full at the bell
        BD_INFO("[match] squash : vie du catcheur {} remise a {}", i, g_squash_hp[i]);
      }
    }
  }
  __imp__sub_82246DE0(ctx, base);
}

extern "C" REX_FUNC(sub_82246E48) {
  const u32 obj = ctx.r3.u32, from = u32(ctx.lr);
  const double v = ctx.f1.f64;
  const float before = obj ? F(obj + 152) : 0.f;
  __imp__sub_82246E48(ctx, base);
  static std::atomic<int> n{0};
  if (Probe() && std::abs(v - before) > 0.05 && n.fetch_add(1) < 300)
    BD_INFO("[finisher] catcheur {} : jauge {:.2f} -> {:.2f} (appel {:08X})",
            StarOf(obj ? bd::mem::try_load<u32>(obj + 32) : 0), before, v, from);
}

extern "C" REX_FUNC(sub_822483F0) {
  if (Probe())
    BD_INFO("[finisher] catcheur {} : jauge videe (appel {:08X})",
            StarOf(ctx.r3.u32 ? bd::mem::try_load<u32>(ctx.r3.u32 + 32) : 0), u32(ctx.lr));
  __imp__sub_822483F0(ctx, base);
}

extern "C" REX_FUNC(sub_8224B008) {
  if (!Probe()) {
    __imp__sub_8224B008(ctx, base);
    return;
  }
  const u32 obj = ctx.r3.u32;
  const u32 actor = bd::mem::try_load<u32>(obj + 32);
  const u32 before = bd::mem::try_load<u32>(actor + 2776);
  __imp__sub_8224B008(ctx, base);
  BD_INFO("[finisher] catcheur {} : compteur {} -> {} (appel {:08X})", StarOf(actor), before,
          bd::mem::try_load<u32>(actor + 2776), u32(ctx.lr));
}

extern "C" REX_FUNC(sub_82248000) {
  const u32 obj = ctx.r3.u32;
  __imp__sub_82248000(ctx, base);
  if (!Probe())
    return;
  static std::atomic<u32> last[kStars];
  const int i = StarOf(obj ? bd::mem::try_load<u32>(obj + 32) : 0);
  if (i >= 0 && last[i].exchange(ctx.r3.u32 + 1) != ctx.r3.u32 + 1)
    BD_INFO("[finisher] catcheur {} : finishers par jauge pleine = {}", i, ctx.r3.u32);
}

// ---- the character select: starting finishers chosen per superstar with LT / RT (as WWE 2K14) ----
//
// Found (probe test 2026-11-14): the character select asks the button bar 11 (also 9, 42, 2, 525 by state) from
// sub_825697xx (call returning to 0x82569864). Its scene object S (methods such as sub_8259E5B0, r3 = S) has
// [S+284] = the place being chosen (0-5), [S+272] its grid; S is built for each place by sub_825A1200(S, grid,
// place); the select object M = *0x8324DEFC has the places
// (M + [M+15960] * 1596 + 124 + i * 244: +0 id, +16 attire, +44 pad, 255 = CPU). The pads come through the
// XInputGetState hook of wwe12_intros.cpp (Wwe12MatchPad: X_INPUT_STATE +6 / +7 = LT / RT, 0-255).
// While the select shows: LT / RT on any pad change the value of the place being chosen (game's rules, 1, 2,
// 3, infinite), shown in the attire panel's title ("CHANGE ATTIRE  |  FINISHERS: 2", text 280 completed through
// the Universe line's string hook: Wwe12MatchModesGameText / Wwe12MatchModesText; first try was the button bar's
// last label, the user wanted it in a panel as WWE 2K14). Values start
// at wwe12_starting_finishers each time the select opens; the match gives place i's value to the superstar in
// place i. wwe12_select_finishers = false turns the select's part off.

REX_EXTERN(__imp__sub_828942C0);
REX_EXTERN(__imp__sub_8259E5B0);

namespace {
constexpr u32 kSelectBarCall = 0x82569864;
std::atomic<u32> g_bar_set{0}, g_bar_from{0};
std::atomic<bool> g_select{false};      // the character select shows (its bar was the last asked)
std::atomic<u32> g_scene{0};            // its scene object S (the active one, when known)
std::atomic<u32> g_scenes[kStars];      // the scene objects by place
constexpr u32 kOurLabel = 27060;        // the button bar's last label while the select shows (superstars with
                                        // only their original attire have no attire panel: shown there too)
u32 g_patched_entry = 0;                // bar entry whose label is ours (guest address of its u16), 0 = none
u16 g_patched_label = 0;                // the game's label there
std::atomic<int> g_choice[kStars];      // per place: 0 = game's rules, 1-3, 4 = infinite
std::atomic<bool> g_chosen{false};      // the values come from the select (else the setting)
u32 g_text = 0;                         // our text (guest)

std::string Places() {
  const u32 m = bd::mem::try_load<u32>(0x8324DEFC);
  if (!m)
    return "-";
  const u32 sel = m + bd::mem::try_load<u32>(m + 15960) * 1596 + 124;
  std::string out = fmt::format("[+15960] {}:", bd::mem::try_load<u32>(m + 15960));
  for (u32 i = 0; i < 6; ++i) {
    const u32 pl = sel + i * 244;
    out += fmt::format(" | {} id {} manette {}", i, bd::mem::try_load<u32>(pl), bd::mem::try_load<u8>(pl + 44));
  }
  return out;
}

// The place being chosen: the select builds the six places' objects at once (sub_825A1200), and the one being
// chosen has [S+268] = 2 (0 for the others; probe test 2026-11-14: also +6460 = 7, +6724 = 3; a place already
// chosen: +6460 = 17). Place 0 if none is found.
int Place() {
  for (int k = 0; k < kStars; ++k) {
    const u32 sc = g_scenes[k].load();
    if (sc && bd::mem::try_load<u32>(sc + 268) == 2 && bd::mem::try_load<u32>(sc + 284) == u32(k))
      return k;
  }
  return 0;
}

const char *ValueName(int v) {
  static const char *const k[5] = {"NORMAL", "1", "2", "3", "INF"};
  return k[std::clamp(v, 0, 4)];
}

// The bar entry of set n (H = *0x8324DF18, 20-byte entries at [H+48], count [H+52], set id u16 at +12, three
// {button u16, label u16} places): our label in its last used place while the select shows.
void PatchBar(u32 n, bool on) {
  if (g_patched_entry) {
    bd::mem::store<u16>(g_patched_entry, g_patched_label);
    g_patched_entry = 0;
  }
  if (!on)
    return;
  const u32 h = bd::mem::try_load<u32>(0x8324DF18);
  const u32 list = h ? bd::mem::try_load<u32>(h + 48) : 0, count = h ? bd::mem::try_load<u32>(h + 52) : 0;
  for (u32 k = 0; list && k < count && k < 2048; ++k) {
    const u32 e = list + k * 20;
    if (bd::mem::try_load<u16>(e + 12) != n)
      continue;
    for (int place = 2; place >= 0; --place) {
      const u32 at = e + u32(place) * 4 + 2;
      const u16 label = bd::mem::try_load<u16>(at);
      if (label && label != 0xFFFF) {
        g_patched_entry = at;
        g_patched_label = label;
        bd::mem::store<u16>(at, u16(kOurLabel));
        return;
      }
    }
    return;
  }
}

void ResetChoices() {
  const int v = Finishers();
  for (auto &c : g_choice)
    c = v;
  g_chosen = true;
}
}  // namespace

REXCVAR_DEFINE_BOOL(wwe12_select_finishers, false, "WWE12",
                    "Character select: LT / RT choose each superstar's starting finishers (as WWE 2K14).");

// The string hook of the Universe line asks: is this id ours? The game's text to complete, else 0. While the
// select shows: the attire panel's title, CHANGE ATTIRE (text 280), gets the place's finishers.
constexpr u32 kChangeAttireText = 280;
thread_local bool g_bar_text = false;  // the text being completed is the bar's (else the panel's title)
thread_local u32 g_asked = 0;          // the id being completed
std::atomic<u32> g_banner_id{0};       // the text id of "ADD MANAGER" (the banner of a superstar without other attires)
std::set<u32> g_logged_ids, g_banner_tried;  // (game thread)
u32 Wwe12MatchModesGameText(u32 id) {
  if (!g_select.load())
    return 0;
  if (Probe()) {
    static std::mutex m;
    static std::vector<u32> seen;
    std::lock_guard<std::mutex> l(m);
    if (seen.size() < 200 && std::find(seen.begin(), seen.end(), id) == seen.end()) {
      seen.push_back(id);
      BD_INFO("[selection] texte demande pendant la selection : {}", id);
    }
  }
  g_bar_text = false;
  g_asked = id;
  if (id == kChangeAttireText || id == g_banner_id.load())
    return id;
  // (probe: every text the select asks goes through Wwe12MatchModesText once, to log it)
  if (Probe() && g_logged_ids.count(id) == 0)
    return id;
  // the "X ADD MANAGER" banner of a superstar without other attires: found by its text (once) when unknown
  if (!g_banner_id.load() && g_banner_tried.count(id) == 0)
    return id;
  return 0;
}

// The game's text (guest address) completed with the place's value.
u32 Wwe12MatchModesText(u32 game) {
  const char *g = game ? bd::mem::try_at<const char>(game) : nullptr;
  if (!g)
    return game;
  const u32 asked = g_asked;
  if (asked != kChangeAttireText && asked != g_banner_id.load()) {
    if (Probe() && g_logged_ids.insert(asked).second)
      BD_INFO("[selection] texte {} = \"{}\"", asked, g);
    if (!g_banner_id.load() && g_banner_tried.insert(asked).second && std::strstr(g, "MANAGER")) {
      g_banner_id = asked;
      BD_INFO("[selection] bandeau MANAGER = texte {}", asked);
    }
    if (asked != g_banner_id.load())
      return game;
  }
  static u32 s_bar = 0;
  u32 &buf = g_bar_text ? s_bar : g_text;
  if (!buf)
    buf = REX_KERNEL_MEMORY()->SystemHeapAlloc(256, 0x10);
  if (!buf)
    return game;
  const int p = Place();
  static std::atomic<int> n{0};
  if (Probe() && n.fetch_add(1) < 20)
    BD_INFO("[selection] titre du cadre complete (place {})", p);
  // LT / RT icons: the game's private characters U+E00C / U+E00F (found 2026-11-14: E000 START, E001 BACK,
  // E002-E006 d-pad, E007 Y, E008 B, E009 A, E00A X, E00B LB, E00C LT, E00D LS, E00E RB, E00F RT).
  const std::string text = std::string(g) + "    FINISHERS: " +
                           ValueName(p >= 0 ? g_choice[p].load() : 0) + " ";
  std::memset(bd::mem::at<char>(buf), 0, 256);
  std::memcpy(bd::mem::at<char>(buf), text.data(), std::min<size_t>(text.size(), 255));
  return buf;
}

// sub_825A1200(S, grid, place): the select builds a scene object for each place it asks (S+284 = place).
REX_EXTERN(__imp__sub_825A1200);
extern "C" REX_FUNC(sub_825A1200) {
  const u32 s = ctx.r3.u32, place = ctx.r5.u32;
  __imp__sub_825A1200(ctx, base);
  if (place < kStars)
    g_scenes[place] = s;  // (all six are built at once when the select opens: which one is active is found below)
  if (Probe())
    BD_INFO("[selection] nouvel objet de choix {:08X} pour la place {}", s, place);
}

extern "C" REX_FUNC(sub_8259E5B0) {
  const u32 s = ctx.r3.u32;
  if (g_scene.exchange(s) != s && Probe())
    BD_INFO("[selection] objet de l'ecran {:08X}, place {}", s, bd::mem::try_load<u32>(s + 284));
  __imp__sub_8259E5B0(ctx, base);
}

extern "C" REX_FUNC(sub_828942C0) {
  const u32 n = ctx.r3.u32, from = u32(ctx.lr);
  const bool select = from == kSelectBarCall && REXCVAR_GET(wwe12_select_finishers);
  if (select && !g_select.exchange(true))
    ResetChoices();  // the select opens: every place starts at the setting
  if (!select)
    g_select = false;
  PatchBar(n, false);  // (the bottom bar is not used any more: the user did not like it there)
  if (Probe() && (n != g_bar_set.exchange(n) || from != g_bar_from.exchange(from)))
    BD_INFO("[selection] barre de boutons {} (appel {:08X}) place {} | {}", n, from, Place(), Places());
  __imp__sub_828942C0(ctx, base);
}

// From the XInputGetState hook (wwe12_intros.cpp), every pad: buttons and triggers (0-255).
void Wwe12MatchPad(u32 user, u16 buttons, u8 lt, u8 rt) {
  static u8 s_down[4] = {};
  if (user >= 4)
    return;
  const u8 now = u8((lt > 128 ? 1 : 0) | (rt > 128 ? 2 : 0));
  const u8 pressed = u8(now & ~s_down[user]);
  s_down[user] = now;
  if (!pressed || !g_select.load())
    return;
  const int p = Place();
  if (p < 0) {
    BD_INFO("[selection] LT/RT : place inconnue (objet {:08X})", g_scene.load());
    return;
  }
  if (Probe())
    for (int k = 0; k < kStars; ++k) {
      const u32 sc = g_scenes[k].load();
      if (!sc)
        continue;
      std::string w;
      for (u32 o = 256; o <= 340; o += 4)
        w += fmt::format(" {}:{:X}", o, bd::mem::try_load<u32>(sc + o));
      for (u32 o : {6460u, 6724u, 6740u, 25056u, 25060u, 25064u, 25084u, 25088u, 25188u, 25200u, 27088u, 27092u})
        w += fmt::format(" {}:{:X}", o, bd::mem::try_load<u32>(sc + o));
      BD_INFO("[selection] objet place {} {:08X}{}", k, sc, w);
    }
  int v = g_choice[p].load() + ((pressed & 2) ? 1 : -1);
  v = (v + 5) % 5;
  g_choice[p] = v;
  BD_INFO("[selection] place {} : finishers {} (manette {}) | {}", p, ValueName(v), user, Places());
}

// Start(): the value of the superstar's place when the select chose it.
namespace {
int ChosenFinishers(int i) { return g_chosen.load() && i >= 0 && i < kStars ? g_choice[i].load() : Finishers(); }
}  // namespace
