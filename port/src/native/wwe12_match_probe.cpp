// Match probe for the 1.4 match modes (starting finishers, squash): logs, changes nothing.
// Off by default (wwe12_match_probe). Lines "[finisher]" and "[vie]".
//
// What the code says (read 2026-10-28 and 2026-11-14, to be checked by this probe):
// - Finisher gauge: the 'CHP ' component of an actor ([actor+3436], sub_827D5C28), object = component + 24;
//   [obj+32] = the actor. obj+152 = gauge (float, 1 = full = finisher ready), +176 written by SetMomentumMax,
//   +180 mode (2 = gauge kept full every frame), +188 timer of the "finisher ready" state (-1 after a finisher).
//   Read by sub_82246DE0, written by sub_82246E48(obj, f1). A finisher done: sub_82349648(actor, obj, move) calls
//   sub_8224B008(obj) (count actor+2776, max sub_82248000(obj): 3 with FIRED UP, else 1) then sub_822483F0(obj)
//   (gauge emptied) unless actor+998 is set.
// - Health: script command SetHP (sub_823D5298) writes 4 u16 at [actor+3364] + 56 (one value for 4 places:
//   head, torso, arms, legs, as Create a Story's STARTING HEALTH). Damage level: sub_8224A7E8(CHP obj).
//
// A polling thread (every 50 ms, reads only) logs each change of these fields per superstar of the match
// (actors 0x8317B770 + 4 * i); the hooks log the finisher calls on the game thread.

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(wwe12_match_probe, false, "WWE12",
                    "Log the finisher gauge and the health of the superstars in a match (study for 1.4, changes nothing).");

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

REX_EXTERN(__imp__sub_82246DE0);  // gauge of a CHP object
REX_EXTERN(__imp__sub_82246E48);  // set the gauge (obj, f1)
REX_EXTERN(__imp__sub_82349648);  // a finisher done (actor, obj, move)
REX_EXTERN(__imp__sub_8224B008);  // finisher count +1 (obj)
REX_EXTERN(__imp__sub_822483F0);  // gauge emptied (obj)
REX_EXTERN(__imp__sub_82248000);  // finishers per full gauge (obj)

namespace {

constexpr u32 kActors = 0x8317B770;
constexpr int kStars = 6;

std::atomic<u32> g_obj[kStars];  // CHP object of each superstar, learned from the hooks
std::once_flag g_started;

int StarOf(u32 actor) {
  if (!actor)
    return -1;
  for (int i = 0; i < kStars; ++i)
    if (bd::mem::try_load<u32>(kActors + i * 4) == actor)
      return i;
  return -1;
}

int Learn(u32 obj) {
  if (!obj)
    return -1;
  const int i = StarOf(bd::mem::try_load<u32>(obj + 32));
  if (i >= 0)
    g_obj[i] = obj;
  return i;
}

float F(u32 at) {
  const u32 v = bd::mem::try_load<u32>(at);
  float f;
  std::memcpy(&f, &v, 4);
  return f;
}

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
  if (obj && bd::mem::try_load<u32>(obj + 32) == actor) {
    const float timer = F(obj + 188);
    gauge = fmt::format("jauge {:.2f} max {:.2f} mode {} minuteur {} | +998 {} +2776 {}", F(obj + 152), F(obj + 176),
                        bd::mem::try_load<u32>(obj + 180),
                        bd::mem::try_load<u32>(obj + 188) == 0xFFFFFFFF ? std::string("-1") : fmt::format("{:.1f}", timer),
                        bd::mem::try_load<u8>(actor + 998), bd::mem::try_load<u32>(actor + 2776));
  }
  return fmt::format("{:08X} vie {} | {}", actor, hp, gauge);
}

void Poll() {
  std::string last[kStars];
  const auto t0 = std::chrono::steady_clock::now();
  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const long long ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
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

void Start() {
  std::call_once(g_started, [] {
    std::thread(Poll).detach();
    BD_INFO("[finisher] sonde active");
  });
}

bool On() { return REXCVAR_GET(wwe12_match_probe); }

}  // namespace

extern "C" REX_FUNC(sub_82246DE0) {
  if (On()) {
    Start();
    Learn(ctx.r3.u32);
  }
  __imp__sub_82246DE0(ctx, base);
}

extern "C" REX_FUNC(sub_82246E48) {
  if (!On()) {
    __imp__sub_82246E48(ctx, base);
    return;
  }
  Start();
  const u32 obj = ctx.r3.u32, from = u32(ctx.lr);
  const int i = Learn(obj);
  const double v = ctx.f1.f64;
  const float before = obj ? F(obj + 152) : 0.f;
  __imp__sub_82246E48(ctx, base);
  static std::atomic<int> n{0};
  if (std::abs(v - before) > 0.05 && n.fetch_add(1) < 300)  // (small steps of the gauge filling are left out)
    BD_INFO("[finisher] catcheur {} : jauge {:.2f} -> {:.2f} (appel {:08X})", i, before, v, from);
}

extern "C" REX_FUNC(sub_82349648) {
  if (!On()) {
    __imp__sub_82349648(ctx, base);
    return;
  }
  const u32 actor = ctx.r3.u32, obj = ctx.r4.u32, move = ctx.r5.u32, from = u32(ctx.lr);
  const int i = StarOf(actor);
  const float before = obj ? F(obj + 152) : 0.f;
  __imp__sub_82349648(ctx, base);
  BD_INFO("[finisher] catcheur {} : prise {:08X} (appel {:08X}) jauge {:.2f} -> {:.2f}, resultat {}, +998 {}", i, move,
          from, before, obj ? F(obj + 152) : 0.f, ctx.r3.u32, bd::mem::try_load<u8>(actor + 998));
}

extern "C" REX_FUNC(sub_8224B008) {
  if (!On()) {
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

extern "C" REX_FUNC(sub_822483F0) {
  if (On())
    BD_INFO("[finisher] catcheur {} : jauge videe (appel {:08X})", Learn(ctx.r3.u32), u32(ctx.lr));
  __imp__sub_822483F0(ctx, base);
}

extern "C" REX_FUNC(sub_82248000) {
  const u32 obj = ctx.r3.u32;
  __imp__sub_82248000(ctx, base);
  if (!On())
    return;
  static std::atomic<u32> last[kStars];
  const int i = Learn(obj);
  if (i >= 0 && last[i].exchange(ctx.r3.u32 + 1) != ctx.r3.u32 + 1)
    BD_INFO("[finisher] catcheur {} : finishers par jauge pleine = {}", i, ctx.r3.u32);
}
