// Match modes for 1.4 (session Gameplay): starting finishers (as WWE 2K14) and squash matches, plus the probe
// that found what they use. All off by default.
//
// Found (code read 2026-10-28 / 2026-11-14, probe test of the user 2026-11-14, test_finishers.log):
// - Finisher gauge: the 'CHP ' component of an actor ([actor+3436], sub_827D5C28), object = component + 24,
//   [obj+32] = the actor. obj+152 = gauge (float, 1 = full = finisher ready). Read by sub_82246DE0 (often, on
//   the game thread), written by sub_82246E48(obj, f1) (a plain store outside online play). +188 = -1 while the
//   gauge is not full (the per-frame update sub_8224BB58 handles the full state; a full gauge left unused is lost
//   after a while: seen by the user). A finisher done: sub_82349910(actor) sets the gauge to 0.05 (call returning
//   to 0x82349960) and +188 = -1; actor+2776 counts finishers (sub_8224B008, max sub_82248000: 3 for some, else 1)
//   and goes back down later. sub_82349648 is a per-frame check, not "a finisher done".
// - Health: 4 u16 at [actor+3364] + 56, in this order: head, body, arms, legs (the script command SetHP,
//   sub_823D5298, writes all four). Full = 120 in an exhibition (a Royal Rumble entrant: 110); they go down with
//   the hits on each place and below 0 (65535...) when worn out.
//
// Starting finishers (wwe12_starting_finishers: 0 = off, 1-3 = that many, 4 = infinite): each superstar of a
// match starts with a full gauge; after a finisher, while some are left (or always, infinite), the gauge is set
// full again instead of 0.05, and the finisher count actor+2776 is put back to 0. A full gauge the game takes
// away after a while (sub_822483F0) is given back while some are left.
// Squash (wwe12_squash: 0 = off, 1 = the 2nd superstar is the jobber, 2 = the 1st): 1 on 1 matches only; the
// jobber's four health values start at wwe12_squash_health percent of full, the other one with a full gauge.
// A new superstar in a place (actor pointer changed), or any after a loading screen, is a new start: set at his first gauge read,
// and his health set again for 5 s if the game puts it back to full.

#include <rex/cvar.h>

REXCVAR_DEFINE_INT32(wwe12_starting_finishers, 0, "WWE12",
                     "Finishers every superstar has at the start of a match: 0 = the game's rules, 1-3, 4 = infinite.");
REXCVAR_DEFINE_INT32(wwe12_squash, 0, "WWE12",
                     "Squash match (1 on 1): 0 = off, 1 = the second superstar is the jobber (low health), 2 = the first.");
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
#include <string>
#include <thread>

#include <fmt/format.h>

#include "svr_hook.h"
#include "core/logging.h"
#include "core/memory_helpers.h"

int64_t Wwe12MsSinceLoadScreen();  // wwe12_load_turbo.cpp

REX_EXTERN(__imp__sub_82246DE0);  // gauge of a CHP object
REX_EXTERN(__imp__sub_82246E48);  // set the gauge (obj, f1)
REX_EXTERN(__imp__sub_8224B008);  // finisher count +1 (obj)
REX_EXTERN(__imp__sub_822483F0);  // full gauge taken away (obj)
REX_EXTERN(__imp__sub_82248000);  // finishers per full gauge (obj)

namespace {

constexpr u32 kActors = 0x8317B770;
constexpr int kStars = 6;
constexpr u32 kFinisherDone = 0x82349960;  // sub_82349910's call to the gauge setter (0.05 after a finisher)
constexpr int kInfinite = 4;

using Clock = std::chrono::steady_clock;

// game thread only (the gauge hooks), except g_obj read by the probe
std::atomic<u32> g_obj[kStars];
u32 g_actor[kStars] = {};            // the actor each place was started for
bool g_recheck[kStars] = {};         // a loading screen since: the same actor may be a new match
int g_left[kStars] = {};             // finishers still given (starting finishers)
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

void SetGauge(const PPCContext &in, u8 *base, u32 obj, double v) {
  PPCContext c = in;
  c.r1.u64 = (in.r1.u32 - 512) & ~15u;
  c.r3.u64 = obj;
  c.f1.f64 = v;
  __imp__sub_82246E48(c, base);
}

int Finishers() { return std::clamp<int32_t>(REXCVAR_GET(wwe12_starting_finishers), 0, kInfinite); }

void SetHealth(u32 actor, u16 v) {
  const u32 dmg = bd::mem::try_load<u32>(actor + 3364);
  if (dmg)
    for (u32 k = 0; k < 4; ++k)
      bd::mem::store<u16>(dmg + 56 + k * 2, v);
}

// A superstar's first gauge read in his place: the match modes' start.
void Start(const PPCContext &ctx, u8 *base, int i, u32 actor, u32 obj) {
  g_actor[i] = actor;
  g_left[i] = Finishers();
  g_squash_hp[i] = 0;
  bool full = g_left[i] > 0;
  const int squash = REXCVAR_GET(wwe12_squash);
  if (squash == 1 || squash == 2) {
    if (Stars() == 2 && i < 2) {
      const int jobber = squash == 1 ? 1 : 0;
      if (i == jobber) {
        const u32 dmg = bd::mem::try_load<u32>(actor + 3364);
        const u16 now = dmg ? bd::mem::try_load<u16>(dmg + 56) : 0;
        const u16 hp_full = now > 0 && now < 1000 ? now : 120;
        const int pct = std::clamp<int32_t>(REXCVAR_GET(wwe12_squash_health), 5, 90);
        g_squash_full[i] = hp_full;
        g_squash_hp[i] = u16(std::max(1, hp_full * pct / 100));
        g_squash_until[i] = Clock::now() + std::chrono::seconds(5);
        SetHealth(actor, g_squash_hp[i]);
        BD_INFO("[match] squash : catcheur {} commence a {} sur {} de vie", i, g_squash_hp[i], hp_full);
      } else {
        full = true;
        BD_INFO("[match] squash : catcheur {} commence avec son finisher", i);
      }
    }
  }
  if (full && F(obj + 152) < 1.f)
    SetGauge(ctx, base, obj, 1.0);
  if (g_left[i] > 0)
    BD_INFO("[match] catcheur {} : {} finisher(s) au depart", i, g_left[i] == kInfinite ? std::string("infini")
                                                                                      : std::to_string(g_left[i]));
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
    if (fresh)
      Start(ctx, base, i, actor, obj);
    else if (g_squash_hp[i] && Clock::now() < g_squash_until[i]) {
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
  int i = -1;
  if (from == kFinisherDone && obj) {
    const u32 actor = bd::mem::try_load<u32>(obj + 32);
    i = StarOf(actor);
    if (i >= 0 && actor == g_actor[i] && g_left[i] > 0) {
      if (g_left[i] != kInfinite)
        --g_left[i];
      if (g_left[i] > 0) {
        ctx.f1.f64 = 1.0;  // one more finisher: the gauge stays full
        bd::mem::store<u32>(actor + 2776, 0);
        BD_INFO("[match] catcheur {} : finisher fait, jauge gardee pleine (reste {})", i,
                g_left[i] == kInfinite ? std::string("infini") : std::to_string(g_left[i]));
      }
    }
  }
  __imp__sub_82246E48(ctx, base);
  static std::atomic<int> n{0};
  if (Probe() && std::abs(v - before) > 0.05 && n.fetch_add(1) < 300)
    BD_INFO("[finisher] catcheur {} : jauge {:.2f} -> {:.2f} (appel {:08X})", i, before, ctx.f1.f64, from);
}

extern "C" REX_FUNC(sub_822483F0) {
  const u32 obj = ctx.r3.u32;
  const u32 actor = obj ? bd::mem::try_load<u32>(obj + 32) : 0;
  const int i = StarOf(actor);
  const bool was_full = obj && F(obj + 152) >= 1.f;
  if (Probe())
    BD_INFO("[finisher] catcheur {} : jauge videe (appel {:08X})", i, u32(ctx.lr));
  const PPCContext in = ctx;
  __imp__sub_822483F0(ctx, base);
  if (i >= 0 && actor == g_actor[i] && g_left[i] > 0 && was_full) {
    SetGauge(in, base, obj, 1.0);  // a finisher still owed: not taken away
    BD_INFO("[match] catcheur {} : finisher garde (reste {})", i,
            g_left[i] == kInfinite ? std::string("infini") : std::to_string(g_left[i]));
  }
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
