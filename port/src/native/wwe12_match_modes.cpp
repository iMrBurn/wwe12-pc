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
#include <map>
#include <mutex>
#include <tuple>
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
void Wwe12PinProbeStart();         // below
void Wwe12PinTrapStart();          // wwe12_pin_trap.cpp (dev)
u32 Wwe12SelectScene(int place);
void Wwe12IQuitTick(const PPCContext &ctx, u8 *base);  // wwe12_iquit.cpp

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
std::atomic<u32> g_pad_presses{0};  // A / B / X / Y presses of pad 0 (pin probe; counted in Wwe12MatchPad)
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
  if (i == 0 && Probe()) {  // (I Quit study: the match record's rules, 104 bytes at 0x8317C850)
    std::string bytes;
    for (u32 k = 0; k < 104; ++k)
      bytes += fmt::format("{}{:02X}", k % 8 ? "" : " ", bd::mem::try_load<u8>(0x8317C850 + k));
    BD_INFO("[match] fiche du match (type {}) :{}", bd::mem::try_load<u8>(0x8317C850), bytes);
  }
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
    Wwe12PinProbeStart();
    Wwe12PinTrapStart();
    if (i == 0)
      Wwe12IQuitTick(ctx, base);  // (the I QUIT mini-game, once per frame)
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
  static const char *const k[5] = {"NONE", "1", "2", "3", "INFINITE"};
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

REXCVAR_DEFINE_BOOL(wwe12_select_finishers, true, "WWE12",
                    "Character select: LT / RT choose each superstar's starting finishers (as WWE 2K14).");

// The string hook of the Universe line asks: is this id ours? The game's text to complete, else 0. While the
// select shows: the attire panel's title, CHANGE ATTIRE (text 280), gets the place's finishers.
constexpr u32 kChangeAttireText = 280;
thread_local bool g_bar_text = false;  // the text being completed is the bar's (else the panel's title)
thread_local u32 g_asked = 0;          // the id being completed
std::set<u32> g_logged_ids;            // (game thread)
// The texts completed with the finishers: the attire panel's title (280 CHANGE ATTIRE), and for a superstar without
// other attires the banner under his picture (273 " READY   ADD MANAGER", 287 " READY   ADVANCED"; probe test).
bool Ours(u32 id) { return id == 280 || id == 273 || id == 287; }
// I QUIT (1.4): with wwe12_iquit, ONE ON ONE has an I QUIT row (below): a SUBMISSION match (no pinfall, no
// count-out, no rope breaks: the match record of 2026-11-14) whose name (match type text 27467 = 27400 + 67) and
// ends (2945 GIVEUP, 2950 SUBMISSION, "BY WAY OF" texts 2942-2950) read "I QUIT".
REXCVAR_DEFINE_BOOL(wwe12_iquit, true, "WWE12",
                    "I Quit match: PLAY > ONE ON ONE gets an I QUIT match (as WWE '13): won by giving up only.");
constexpr u32 kSubmissionType = 67;
bool IQuitMatch();  // below: the match set up is an I Quit one
bool IQuitText(u32 id) {
  if (!REXCVAR_GET(wwe12_iquit))
    return false;
  if (id == 43900)
    return true;  // the I QUIT row's label
  // the match's name and its end ("BY WAY OF"), in an I Quit match only
  return IQuitMatch() && (id == 27400 + kSubmissionType || id == 2945 || id == 2950) &&
         bd::mem::try_load<u8>(0x8317C850) == kSubmissionType;
}
thread_local bool g_iquit_text = false;   // the text being completed is an I QUIT one
thread_local bool g_iquit_check = false;  // (any text, in an I Quit match: "SUBMISSION" becomes "I QUIT")

std::string Wwe12DamagePageText(uint32_t id);  // wwe12_damage_page.cpp: BALANCING > DAMAGE's labels
thread_local std::string g_damage_text;      // the label being served
thread_local u32 g_damage_id = 0;
bool Wwe12DamagePageOpen();                   // our DAMAGE page is being set up

u32 Wwe12MatchModesGameText(u32 id) {
  g_iquit_text = false;
  g_iquit_check = false;
  g_damage_text = Wwe12DamagePageText(id);
  if (!g_damage_text.empty()) {
    g_damage_id = id;
    return id;
  }
  // The borrowed frame DrawAIReversal's subtitle, A.I. REVERSALS (text 41145, test of 2026-11-16), reads the game's
  // own DAMAGE (41146, the BALANCING list's entry) while our page sets up: in the player's language.
  if (id == 41145 && Wwe12DamagePageOpen()) {
    g_asked = id;  // (Wwe12MatchModesText then gives the game's text back as is)
    return 41146;
  }
  if (IQuitText(id)) {
    g_iquit_text = true;
    return id;
  }
  // An I Quit match chosen: every text of the game reading "SUBMISSION" (the match card, the result's "BY WAY OF"
  // ...) reads "I QUIT" (Wwe12MatchModesText looks at the game's text).
  // (not the SUBMISSION row of the ONE ON ONE list, 41049: it stays itself next to I QUIT)
  if (REXCVAR_GET(wwe12_iquit) && IQuitMatch() && !g_select.load() && id != 41049) {
    g_iquit_check = true;
    return id;
  }
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
  if (Ours(id))
    return id;
  // other banners under the picture (user's test of 2026-11-14: Mason Ryan, David Otunga had none of the three):
  // every text of the select is looked at, those reading " READY..." get the finishers too
  if (REXCVAR_GET(wwe12_select_finishers))
    return id;
  // (probe: every text the select asks goes through Wwe12MatchModesText once, to log it)
  if (Probe() && g_logged_ids.count(id) == 0)
    return id;
  return 0;
}

// The game's text (guest address) completed with the place's value.
u32 Wwe12MatchModesText(u32 game) {
  if (!g_damage_text.empty()) {
    static u32 s_buf = 0;
    if (!s_buf)
      s_buf = REX_KERNEL_MEMORY()->SystemHeapAlloc(5 * 64, 0x10);
    if (!s_buf)
      return game;
    const u32 slot = s_buf + (g_damage_id - 44100) % 5 * 64;  // (one 64-byte slot per label id 44100-44104)
    std::memset(bd::mem::at<char>(slot), 0, 64);
    std::memcpy(bd::mem::at<char>(slot), g_damage_text.data(), std::min<size_t>(g_damage_text.size(), 63));
    g_damage_text.clear();
    return slot;
  }
  if (g_iquit_check) {
    g_iquit_check = false;
    const char *g = game ? bd::mem::try_at<const char>(game) : nullptr;
    if (!g || std::strcmp(g, "SUBMISSION") != 0)
      return game;
    g_iquit_text = true;
  }
  if (g_iquit_text) {
    g_iquit_text = false;
    static u32 s_iquit = 0;
    if (!s_iquit) {
      s_iquit = REX_KERNEL_MEMORY()->SystemHeapAlloc(16, 0x10);
      if (s_iquit)
        std::memcpy(bd::mem::at<char>(s_iquit), "I QUIT", 7);
    }
    return s_iquit ? s_iquit : game;
  }
  const char *g = game ? bd::mem::try_at<const char>(game) : nullptr;
  if (!g)
    return game;
  const u32 asked = g_asked;
  // (" READY   ADD MANAGER", "READY"...: spaces skipped; not the "READY TITLE" button)
  const char *t = g;
  while (*t == ' ')
    ++t;
  const bool ready_banner = REXCVAR_GET(wwe12_select_finishers) && std::strncmp(t, "READY", 5) == 0 &&
                            std::strncmp(t, "READY TITLE", 11) != 0;
  if (Probe() && ready_banner && g_logged_ids.insert(asked).second)
    BD_INFO("[selection] banniere {} = \"{}\" : finishers ajoutes", asked, g);
  if (!Ours(asked) && !ready_banner) {
    if (Probe() && g_logged_ids.insert(asked).second)
      BD_INFO("[selection] texte {} = \"{}\"", asked, g);
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
  Wwe12PinTrapStart();  // (dev, wwe12_pin_trap + wwe12_watch_select: who writes the place's state)
  if (Probe())
    BD_INFO("[selection] nouvel objet de choix {:08X} pour la place {}", s, place);
}

extern "C" REX_FUNC(sub_8259E5B0) {
  const u32 s = ctx.r3.u32;
  if (g_scene.exchange(s) != s && Probe())
    BD_INFO("[selection] objet de l'ecran {:08X}, place {}", s, bd::mem::try_load<u32>(s + 284));
  __imp__sub_8259E5B0(ctx, base);
  // (probe, 2026-11-14: a superstar with one attire goes from his picture to READY without the attire panel; the
  // words 256-340 of each place's object are logged when they change, to find the step it skips)
  if (Probe()) {
    static u32 s_last[kStars][22];
    for (int k = 0; k < kStars; ++k) {
      const u32 sc = g_scenes[k].load();
      if (!sc)
        continue;
      std::string w;
      for (u32 i = 0; i < 22; ++i) {
        const u32 v = bd::mem::try_load<u32>(sc + 256 + i * 4);
        if (v != s_last[k][i]) {
          w += fmt::format(" {}:{:X}->{:X}", 256 + i * 4, s_last[k][i], v);
          s_last[k][i] = v;
        }
      }
      if (!w.empty())
        BD_INFO("[selection] place {} change{}", k, w);
    }
  }
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
u16 Wwe12IQuitPad(u32 user, u16 buttons);  // wwe12_iquit.cpp: the buttons it hides from the game

u16 Wwe12MatchPad(u32 user, u16 buttons, u8 lt, u8 rt) {
  const u16 hide = Wwe12IQuitPad(user, buttons);
  static u8 s_down[4] = {};
  static u16 s_buttons[4] = {};
  if (user >= 4)
    return hide;
  if (user == 0) {  // (pin probe: face button presses, 0x1000 A, 0x2000 B, 0x4000 X, 0x8000 Y)
    const u16 newly = u16(buttons & ~s_buttons[0] & 0xF000);
    for (u16 m = newly; m; m &= u16(m - 1))
      g_pad_presses.fetch_add(1);
  }
  s_buttons[user] = buttons;
  const u8 now = u8((lt > 128 ? 1 : 0) | (rt > 128 ? 2 : 0));
  const u8 pressed = u8(now & ~s_down[user]);
  s_down[user] = now;
  if (!pressed || !g_select.load())
    return hide;
  const int p = Place();
  if (p < 0) {
    BD_INFO("[selection] LT/RT : place inconnue (objet {:08X})", g_scene.load());
    return hide;
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
  return hide;
}

// Start(): the value of the superstar's place when the select chose it.
namespace {
int ChosenFinishers(int i) { return g_chosen.load() && i >= 0 && i < kStars ? g_choice[i].load() : Finishers(); }
}  // namespace

// ---- pin probe (1.4, a second pin system: study) ----
// wwe12_pin_probe: every 100 ms, a record to pin_dump.bin (current folder): time (ms), then for the actors 0, 1
// and the referee (actor list place 6) their actor pointer, state ([actor+680] u16) and 4 KiB of the actor, 512
// bytes of his CHP object and 256 of [actor+3364]. Read offline (what changes during a pin, the count, the kick-out).
// "[tombe]" lines: each state change of these three.

REXCVAR_DEFINE_BOOL(wwe12_pin_probe, false, "WWE12",
                    "Study of the pins: dumps the superstars and the referee to pin_dump.bin (dev, slow file).");

#include <cstdio>

namespace {

// The components of an actor ([actor+3436]: node +4 tag, +16 next, +20 child chain, +24 object), as
// sub_827D5C28 walks them.
void Components(u32 head, std::vector<std::pair<u32, u32>> &out) {
  std::vector<u32> todo{head};
  std::set<u32> seen;
  while (!todo.empty() && out.size() < 24) {
    const u32 node = todo.back();
    todo.pop_back();
    if (!node || !seen.insert(node).second)
      continue;
    out.push_back({bd::mem::try_load<u32>(node + 4), bd::mem::try_load<u32>(node + 24)});
    todo.push_back(bd::mem::try_load<u32>(node + 16));
    todo.push_back(bd::mem::try_load<u32>(node + 20));
  }
}

void PinDumpLoop() {
  std::FILE *f = std::fopen("pin_dump.bin", "wb");
  if (!f)
    return;
  const auto t0 = Clock::now();
  u16 last[3] = {0xFFFF, 0xFFFF, 0xFFFF};
  std::vector<u8> buf;
  std::string comps_logged[2];
  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const u32 ms = u32(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - t0).count());
    const u32 slots[3] = {0, 1, 6};
    bool any = false;
    for (int k = 0; k < 3; ++k)
      any |= bd::mem::try_load<u32>(kActors + slots[k] * 4) != 0;
    if (!any)
      continue;
    buf.clear();
    auto put32 = [&](u32 v) { for (int b = 0; b < 4; ++b) buf.push_back(u8(v >> (8 * b))); };
    put32(0x32444E50);  // "PND2"
    put32(ms);
    put32(g_pad_presses.load());
    for (int k = 0; k < 3; ++k) {
      const u32 actor = bd::mem::try_load<u32>(kActors + slots[k] * 4);
      const u32 st = actor ? bd::mem::try_load<u32>(actor + 680) : 0;
      const u16 state = st ? bd::mem::try_load<u16>(st) : 0xFFFF;
      put32(actor);
      put32(state);
      if (state != last[k]) {
        BD_INFO("[tombe] {} {} : etat {} -> {}", ms, k == 2 ? std::string("arbitre") : fmt::format("catcheur {}", k),
                last[k], state);
        last[k] = state;
      }
      for (u32 o = 0; o < 4096; o += 4)
        put32(actor ? bd::mem::try_load<u32>(actor + o) : 0);
      std::vector<std::pair<u32, u32>> comps;
      if (actor && k < 2)
        Components(bd::mem::try_load<u32>(actor + 3436), comps);
      put32(u32(comps.size()));
      std::string names;
      for (const auto &[tag, obj] : comps) {
        put32(tag);
        put32(obj);
        for (u32 o = 0; o < 1024; o += 4)
          put32(obj ? bd::mem::try_load<u32>(obj + o) : 0);
        names += fmt::format(" {:c}{:c}{:c}{:c}@{:08X}", char(tag >> 24), char(tag >> 16), char(tag >> 8), char(tag), obj);
      }
      if (k < 2 && names != comps_logged[k]) {
        comps_logged[k] = names;
        BD_INFO("[tombe] composants du catcheur {} :{}", k, names);
      }
    }
    std::fwrite(buf.data(), 1, buf.size(), f);
    std::fflush(f);
  }
}
}  // namespace

void Wwe12PinProbeStart() {
  static std::once_flag once;
  if (REXCVAR_GET(wwe12_pin_probe))
    std::call_once(once, [] {
      std::thread(PinDumpLoop).detach();
      BD_INFO("[tombe] sonde active (pin_dump.bin)");
    });
}

// ---- pin probe 2: the conditions the moves check during a pin (wwe12_pin_probe) ----
// While the referee counts (state 190 / 191 / 207 / 202), logs each change of sub_821A8400(actor, which, count)
// (a superstar's state code read by the moves' event conditions) per actor and argument, and each motion event
// walk of sub_824A5458 (wwe12_move_events.cpp calls Wwe12PinEventLog) with its result.

REX_EXTERN(__imp__sub_821A8400);

namespace {
bool Counting() {
  const u32 ref = bd::mem::try_load<u32>(kActors + 6 * 4);
  const u32 st = ref ? bd::mem::try_load<u32>(ref + 680) : 0;
  const u16 s = st ? bd::mem::try_load<u16>(st) : 0;
  return s == 190 || s == 191 || s == 207 || s == 202;
}
}  // namespace

extern "C" REX_FUNC(sub_821A8400) {
  const u32 actor = ctx.r3.u32, which = ctx.r4.u32, count = ctx.r5.u32, from = u32(ctx.lr);
  __imp__sub_821A8400(ctx, base);
  if (!REXCVAR_GET(wwe12_pin_probe) || !Counting())
    return;
  static std::mutex m;
  static std::map<std::tuple<u32, u32, u32>, u32> last;
  const int i = StarOf(actor);
  std::lock_guard<std::mutex> l(m);
  auto key = std::make_tuple(actor, which, from);
  auto it = last.find(key);
  if (it == last.end() || it->second != ctx.r3.u32) {
    last[key] = ctx.r3.u32;
    BD_INFO("[tombe] etat sub_821A8400 catcheur {} which {} count {} -> {} (appel {:08X})", i, which, count,
            ctx.r3.u32, from);
  }
}

bool Wwe12PinCounting() { return REXCVAR_GET(wwe12_pin_probe) && Counting(); }

void Wwe12PinEventLog(u32 motion, u32 r6, u32 r7, int32_t frame, u32 types) {
  if (!REXCVAR_GET(wwe12_pin_probe) || !Counting())
    return;
  static std::mutex m;
  static std::map<u32, int32_t> last;
  std::lock_guard<std::mutex> l(m);
  auto it = last.find(motion);
  if (it == last.end() || it->second != frame) {
    last[motion] = frame;
    BD_INFO("[tombe] evenements motion {:08X} (types {:08X}) r6 catcheur {} r7 catcheur {} -> image {}", motion, types,
            StarOf(r6), StarOf(r7), frame);
  }
}

// ---- I QUIT as a new match of ONE ON ONE (1.4) ----
//
// The menus' table (menu.pac MFLO/0000) is parsed by sub_82DE92F8(menus, table, size): header +4 = the records,
// +8 = those shown, then 0x74-byte big-endian records from +0x28 (fields below; ONE ON ONE's matches are records
// 36-46, parent 8, page 6). As Kaiko does it for SvR
// 2011, a copy is handed to the parser with one record added: I QUIT (our text 43900) after the last match of
// PLAY > ONE ON ONE (parent 8), a copy of its SUBMISSION record (label 41049), so no other row moves. The game
// turns a menu row into a match with sub_82449898(menu, row r4, &a, &b): the I QUIT row has no case there, so the
// hook asks for the SUBMISSION row instead and marks the match as an I Quit one (g_iquit).

REX_EXTERN(__imp__sub_82DE92F8);
REX_EXTERN(__imp__sub_82449898);

REXCVAR_DEFINE_INT32(wwe12_rule_from, -1, "WWE12",
                     "Dev (Special Referee study): a menu row giving this match rule plays wwe12_rule_to instead (-1 = off).");
REXCVAR_DEFINE_INT32(wwe12_rule_to, 83, "WWE12", "Dev: the rule played instead (83 = SPECIAL REFEREE).");
REXCVAR_DEFINE_INT32(wwe12_rule_to_b, 180, "WWE12",
                     "Dev: its second number (180 = the one the game's own unused row gives with 83; -1 = kept).");

namespace {
constexpr u32 kIQuitLabel = 43900;
constexpr u32 kRecord = 0x74;
constexpr u32 kHeader = 0x28;  // records from +0x28 (the parsed copy; menu.pac's other copy has a 12-byte header)
// a record's fields (big-endian u32): label, page, id, parent, kind (5 a choice, 7 the page's last), link (0x7D0 a match)
constexpr u32 kLabel = 0x00, kPage = 0x18, kId = 0x1C, kParent = 0x38, kKind = 0x3C, kLink = 0x54;
std::atomic<bool> g_iquit{false};   // the match being set up is an I Quit
std::atomic<int> g_iquit_row{-1};   // the I QUIT row's index in its menu, once known
int g_submission_row = -1;          // SUBMISSION's index among the ONE ON ONE rows
}  // namespace

void Wwe12SrefMenu(PPCContext &ctx);  // wwe12_sref.cpp: SPECIALTY > SPECIAL REFEREE (the table handed on may be a longer copy)
bool Wwe12SrefDecide(uint32_t id, uint32_t parent, uint32_t label);  // wwe12_sref.cpp: a row of it chosen?
bool Wwe12SrefOn();                   // wwe12_sref.cpp: the mode is on (cvar wwe12_sref)

extern "C" REX_FUNC(sub_82DE92F8) {
  Wwe12SrefMenu(ctx);
  const u32 table = ctx.r4.u32, size = ctx.r5.u32;
  const u32 total = table ? bd::mem::try_load<u32>(table + 4) : 0, shown = table ? bd::mem::try_load<u32>(table + 8) : 0;
  if (REXCVAR_GET(wwe12_iquit))
    BD_INFO("[iquit] table des menus {:08X} : {} lignes, {} affichees, {} octets", table, total, shown, size);
  // (only the shown records are read: the header's total may run past the table's end)
  if (!REXCVAR_GET(wwe12_iquit) || !table || shown == 0 || shown > total || size < kHeader + shown * kRecord) {
    __imp__sub_82DE92F8(ctx, base);
    return;
  }
  auto rec = [&](u32 i) { return table + kHeader + i * kRecord; };
  int sub = -1, last = -1, row = 0, sub_row = -1;
  u32 max_id = 0;
  for (u32 i = 0; i < shown; ++i) {
    const u32 r = rec(i);
    max_id = std::max(max_id, bd::mem::try_load<u32>(r + kId));
    if (bd::mem::try_load<u32>(r + kParent) != 8 || bd::mem::try_load<u32>(r + kLink) != 0x7D0)
      continue;
    if (bd::mem::try_load<u32>(r + kLabel) == 41049) {
      sub = int(i);
      sub_row = row;
    }
    last = int(i);
    ++row;
  }
  if (sub < 0 || last < 0) {
    BD_INFO("[iquit] ONE ON ONE / SUBMISSION introuvables : rien ajoute");
    __imp__sub_82DE92F8(ctx, base);
    return;
  }
  const u32 copy = REX_KERNEL_MEMORY()->SystemHeapAlloc(size + kRecord, 0x10);
  if (!copy) {
    __imp__sub_82DE92F8(ctx, base);
    return;
  }
  const u32 at = kHeader + u32(last + 1) * kRecord;  // the new record's offset
  std::memcpy(bd::mem::at<u8>(copy), bd::mem::at<u8>(table), at);
  std::memcpy(bd::mem::at<u8>(copy + at), bd::mem::at<u8>(rec(u32(sub))), kRecord);
  std::memcpy(bd::mem::at<u8>(copy + at + kRecord), bd::mem::at<u8>(table + at), size - at);
  const u32 added = copy + at;
  bd::mem::store<u32>(added + kLabel, kIQuitLabel);
  // The game finds an entry from its id by a binary search: the ids must go up along the table. A new id here,
  // in the table's middle, hides the entries after it from that search (read 2026-11-16 with the Special Guest
  // Referee's menu: B did not go back from a sub-menu whose parent was not found). With that mode on, I QUIT takes
  // the id of the row before it (the page's last match): the search finds one of the two, on the same page.
  // (Without it the id stays the one I QUIT has had since it was made: to be changed once tested, see docs.)
  bd::mem::store<u32>(added + kId, Wwe12SrefOn() ? bd::mem::try_load<u32>(rec(u32(last)) + kId) : max_id + 1);
  bd::mem::store<u32>(added + kKind, 7);                                 // the page's last choice now
  bd::mem::store<u32>(copy + kHeader + u32(last) * kRecord + kKind, 5);  // (the old last one: a choice)
  bd::mem::store<u32>(copy + 4, total + 1);
  bd::mem::store<u32>(copy + 8, shown + 1);
  g_iquit_row = row;  // (after the page's last match)
  g_submission_row = sub_row;
  BD_INFO("[iquit] ligne I QUIT ajoutee (ligne {} de ONE ON ONE, SUBMISSION = ligne {}, id {})", row, sub_row,
          max_id + 1);
  ctx.r4.u64 = copy;
  ctx.r5.u64 = size + kRecord;
  __imp__sub_82DE92F8(ctx, base);
}

// (the row's match from the game into a and b, on a scratch stack: r10 = 128, r11 = -1 when the row has no case)
void RowMatch(const PPCContext &in, u8 *base, u32 row, u32 &a, u32 &b) {
  PPCContext c = in;
  c.r1.u64 = (in.r1.u32 - 512) & ~15u;
  const u32 scratch = c.r1.u32 + 256;
  bd::mem::store<u32>(scratch, 128);
  bd::mem::store<u32>(scratch + 4, 0xFFFFFFFF);
  c.r4.u64 = row;
  c.r5.u64 = scratch;
  c.r6.u64 = scratch + 4;
  __imp__sub_82449898(c, base);
  a = bd::mem::load<u32>(scratch);
  b = bd::mem::load<u32>(scratch + 4);
}

extern "C" REX_FUNC(sub_82449898) {
  const u32 menu = ctx.r3.u32, row = ctx.r4.u32, out_a = ctx.r5.u32, out_b = ctx.r6.u32, from = u32(ctx.lr);
  const int iq = g_iquit_row.load();
  // The I QUIT row has no case of its own: when this row gives nothing and SUBMISSION's row gives a match on the
  // same page (ONE ON ONE), the SUBMISSION match is used, marked as an I Quit.
  bool asked_iquit = false;
  if (iq >= 0 && int(row) == iq && g_submission_row >= 0) {
    u32 a = 0, b = 0, sa = 0, sb = 0;
    RowMatch(ctx, base, row, a, b);
    RowMatch(ctx, base, u32(g_submission_row), sa, sb);
    if (a == 128 && b == 0xFFFFFFFF && !(sa == 128 && sb == 0xFFFFFFFF)) {
      asked_iquit = true;
      ctx.r4.u64 = u32(g_submission_row);
    }
  }
  __imp__sub_82449898(ctx, base);
  const u32 a = out_a ? bd::mem::try_load<u32>(out_a) : 0, b = out_b ? bd::mem::try_load<u32>(out_b) : 0;
  if (Probe())
    BD_INFO("[iquit] ligne de menu : page {} ligne {}{} -> {} / {} (appel {:08X})", bd::mem::try_load<u32>(menu + 48),
            row, asked_iquit ? " (I QUIT)" : "", a, b, from);
  if (asked_iquit)
    g_iquit = true;
  else if (out_a || out_b)
    g_iquit = false;  // another match chosen
  // (dev, Special Referee study: the match of a row played with another rule, from the menu on - so the select and
  // the participants are built from that rule's own record; 83's third place is the referee's kind)
  if (const int rf = REXCVAR_GET(wwe12_rule_from); rf >= 0 && out_a && int(a) == rf) {
    bd::mem::store<u32>(out_a, u32(REXCVAR_GET(wwe12_rule_to)));
    if (out_b && REXCVAR_GET(wwe12_rule_to_b) >= 0)
      bd::mem::store<u32>(out_b, u32(REXCVAR_GET(wwe12_rule_to_b)));
    BD_INFO("[regle] page {} ligne {} : regle {} / {} -> {} / {} (appel {:08X})", bd::mem::try_load<u32>(menu + 48), row,
            a, b, bd::mem::try_load<u32>(out_a), out_b ? bd::mem::try_load<u32>(out_b) : 0, from);
  }
}

bool IQuitMatch() { return g_iquit.load(); }
bool Wwe12IQuitMatch() { return g_iquit.load() && bd::mem::try_load<u8>(0x8317C850) == kSubmissionType; }

// The menus' "decide" (wwe12_quit.cpp, sub_8253C3F0): the chosen entry's id, parent and label. A choice among PLAY's
// match lists (ONE ON ONE = parent 8, the others 9-14) says whether the match set up is an I Quit.
void Wwe12MatchDecide(uint32_t id, uint32_t parent, uint32_t label) {
  if (Wwe12SrefDecide(id, parent, label)) {  // (a refereed match: not an I Quit one, whatever was chosen before)
    g_iquit = false;
    return;
  }
  // a choice in a higher menu (PLAY, the main menu: Universe, Road to WrestleMania...): no I Quit match any more (else
  // a submission match of the Universe after an I Quit exhibition would be one)
  if (parent < 8) {
    if (g_iquit.exchange(false))
      BD_INFO("[iquit] choix du menu {} (parent {}) : plus de match I Quit", id, parent);
    return;
  }
  if (parent > 14)
    return;
  const bool iq = label == kIQuitLabel;
  if (g_iquit.exchange(iq) != iq || iq)
    BD_INFO("[iquit] choix du menu : id {} parent {} texte {} -> {}", id, parent, label, iq ? "I QUIT" : "autre match");
}

// (dev, wwe12_pin_trap.cpp's wwe12_watch_select) the character select's scene object of a place, 0 when unknown
u32 Wwe12SelectScene(int place) { return place >= 0 && place < kStars ? g_scenes[place].load() : 0; }
