// A safety net for a motion the match hasn't (1.4.2, off by default: wwe12_motion_guard). Study:
// docs/etude_motion_absente_filet.md. The idea is Kaiko's (his SvR 2011 PC port, KaikoClanworth1/wwe-svr2011-pc,
// commit c529dfe: "a wrestler alone in a motion no bank has is started on 446 after 1 s"), rewritten for WWE '12.
//
// What the game does (read 2026-11-19 in the generated code): a motion played by number (sub_8247AD18 ->
// sub_82478AE8) writes the number into the actor's track (+216, what [actor + 680] shows) and asks the load queue
// (sub_821D7480). The queue's reader (sub_821D7C80) asks the motion directory [0x8317B870] for it
// (sub_8246EB78(directory, {number, 255, 255}, 0, 0), called from 0x821D7D00) and DROPS a number it hasn't; the
// loader (sub_821D7E78) gives up (state 5) on a record it can't read. Either way the actor waits for a motion that
// never comes: he stands frozen in it (the I Quit referee, 2026-11-14; 2K16 entrances; mods' moves).
// Here: a number the reader dropped or the loader gave up is noted; an actor whose motion has been such a number
// for 1 s is put back in his free motion (sub_8247C670, the game's own: his brain acts from it - the Special
// Referee's Free(), seen in game). Two men in a hold ('BPMG' at actor + 3404: + 32 = 2, + 36 the holder, + 40 the
// held) are freed together, never one alone (Kaiko: one of a pair started alone crashed his game).
// Not a cure for overwritten memory (the 1.4.0 Royal Rumble, fixed at its cause in 1.4.1).

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(wwe12_motion_guard, false, "WWE12",
                    "Safety net (1.4.2, test): a superstar frozen 1 s in a motion the match hasn't is put back "
                    "standing (idea from Kaiko's SvR 2011 PC port).");
REXCVAR_DEFINE_INT32(wwe12_motion_guard_test, 0, "WWE12",
                     "Dev: N > 0 - in a match, actor 0 is made to play motion N (1852: a number no bank has), 12 s "
                     "after the match's load (a try a second, 5 at most). 0 = never.");
REXCVAR_DEFINE_INT32(wwe12_motion_guard_test_pair, 0, "WWE12",
                     "Dev: N > 0 - in a match, from 12 s after its load, the held man of the first hold seen is made "
                     "to play motion N (1852) - the safety net's hold case. 0 = never.");

#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

#include <fmt/format.h>

#include "svr_hook.h"
#include "core/logging.h"
#include "core/memory_helpers.h"

REX_EXTERN(__imp__sub_8246EB78);  // the motion directory: has it this number? (0 = no)
REX_EXTERN(__imp__sub_8247C670);  // an actor back in his free motion
REX_EXTERN(__imp__sub_8247AD18);  // play a motion on an actor (test aid)

namespace {
constexpr u32 kActors = 0x8317B770;  // the match's actors (six)
constexpr int64_t kHoldMs = 1000;

int64_t NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
u32 Actor(u32 i) { return bd::mem::try_load<u32>(kActors + i * 4); }
u16 Anim(u32 actor) {
  const u32 st = actor ? bd::mem::try_load<u32>(actor + 680) : 0;
  return st ? bd::mem::try_load<u16>(st) : 0xFFFF;
}
int IndexOf(u32 actor) {
  for (u32 i = 0; i < 6; ++i)
    if (actor && Actor(i) == actor)
      return int(i);
  return -1;
}

std::mutex g_mutex;
std::unordered_map<u16, int> g_missing;  // numbers dropped (1) or given up (5) since the directory was last made
struct Watch {
  u16 anim = 0xFFFF;
  int64_t since = 0;
  bool done = false;  // (acted or logged for this stay in it)
};
std::unordered_map<u32, Watch> g_watch;  // per actor (main thread only)
std::atomic<int64_t> g_match_ms{0};      // the match's load (its motion directory made)
std::atomic<bool> g_test_done{false};
std::atomic<int64_t> g_test_stood{0};  // (the last try)
int g_tries = 0;                        // (main thread)
bool g_pair_done = false;               // (the hold test aid, main thread)
int g_pair_tries = 0;
int64_t g_pair_last = 0;  // (the last waiting line)
int64_t g_pair_try = 0;   // (the last try: a hold may last less than a second, the first try goes at once)
std::atomic<int> g_lines{0};
bool More() { return g_lines.fetch_add(1) < 60; }

bool Missing(u16 n, int *how) {
  std::lock_guard lock(g_mutex);
  const auto it = g_missing.find(n);
  if (it == g_missing.end())
    return false;
  *how = it->second;
  return true;
}
void Note(u16 n, int how) {
  bool fresh;
  {
    std::lock_guard lock(g_mutex);
    fresh = g_missing.emplace(n, how).second;
  }
  if (fresh && More())
    BD_INFO("[motion guard] motion {} absente du match ({})", n,
            how == 1 ? "jetee par la file : pas dans le repertoire" : "abandonnee par le chargeur : illisible");
}

void Free(const PPCContext &ctx, u8 *base, u32 actor) {
  PPCContext c = ctx;
  c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
  c.r3.u64 = actor;
  __imp__sub_8247C670(c, base);
  bd::mem::store<u32>(actor + 1096, 0);  // (as the Special Referee's Free(): his flag left at 1 keeps him still)
  bd::mem::store<u32>(actor + 1428, 0);
}

// The other man of a hold this actor is in (0: none).
u32 Partner(u32 actor) {
  for (u32 i = 0; i < 6; ++i) {
    const u32 a = Actor(i), h = a ? bd::mem::try_load<u32>(a + 3404) : 0;
    if (!h || bd::mem::try_load<u32>(h + 32) != 2)
      continue;
    const u32 by = bd::mem::try_load<u32>(h + 36), held = bd::mem::try_load<u32>(h + 40);
    if (by == actor && held)
      return held;
    if (held == actor && by)
      return by;
  }
  return 0;
}

// A motion asked on an actor as the game does it (the Special Referee's Play()).
void PlayOn(const PPCContext &ctx, u8 *base, u32 actor, u32 n) {
  PPCContext c = ctx;
  c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
  c.r3.u64 = actor;
  c.r4.u64 = n;
  c.r5.u64 = 0;
  c.r6.u64 = 0;
  c.r8.u64 = 1;
  c.r9.u64 = 0;
  const u32 bits = bd::mem::try_load<u32>(0x82000AD8);  // (the float the game passes)
  float f;
  std::memcpy(&f, &bits, 4);
  c.f1.f64 = f;
  __imp__sub_8247AD18(c, base);
}

// (test aid, simpler after the first try of 2026-11-19 20:39 / 20:43 - it never fired: "player 1 stood 2 s in 1350"
// never held in the benchmark's 33 s of match): from 12 s after the match's load, motion N asked on actor 0, once a
// second, until he shows it (5 tries at most); each try and why it is skipped logged.
void TestAid(const PPCContext &ctx, u8 *base, int64_t now) {
  const int n = REXCVAR_GET(wwe12_motion_guard_test);
  const int64_t start = g_match_ms.load();
  if (n <= 0 || n >= 65535 || g_test_done || !start)
    return;
  const u32 actor = Actor(0);
  if (now - start < 12000) {
    static int64_t s_last = 0;
    if (now - s_last >= 2000) {
      s_last = now;
      BD_INFO("[motion guard] essai : attente ({} s depuis le chargement), acteur 0 {:08X} motion {}", (now - start) / 1000,
              actor, Anim(actor));
    }
    return;
  }
  if (now - g_test_stood.load() < 1000)
    return;
  g_test_stood = now;
  if (!actor) {
    BD_INFO("[motion guard] essai : pas d'acteur 0 encore");
    return;
  }
  if (Anim(actor) == u16(n) || ++g_tries > 5) {
    g_test_done = true;
    g_tries = 0;
    BD_INFO("[motion guard] essai : fini (acteur 0 motion {})", Anim(actor));
    return;
  }
  const u16 before = Anim(actor);
  PlayOn(ctx, base, actor, u32(n));
  BD_INFO("[motion guard] essai {} : l'acteur 0 ({:08X}) doit jouer la motion {} : motion {} -> {} (filet {})", g_tries,
          actor, n, before, Anim(actor), REXCVAR_GET(wwe12_motion_guard) ? "actif" : "coupe");
}

// (test aid, the hold case: Kaiko's lesson - one man of a hold freed alone crashed his game) From 12 s after the
// match's load, the first hold seen ('BPMG' at actor + 3404: + 32 = 2, + 36 the holder, + 40 the held - read in the
// code for the Special Referee's screw job, never seen in game) gets motion N on its held man; up to 5 holds, until
// he shows it. While waiting, each actor's hold object is logged every 5 s (a dozen lines at most).
void PairAid(const PPCContext &ctx, u8 *base, int64_t now) {
  const int n = REXCVAR_GET(wwe12_motion_guard_test_pair);
  const int64_t start = g_match_ms.load();
  if (n <= 0 || n >= 65535 || g_pair_done || !start || now - start < 12000)
    return;
  u32 by = 0, held = 0, obj = 0;
  std::string seen;
  for (u32 i = 0; i < 6; ++i) {
    const u32 a = Actor(i), h = a ? bd::mem::try_load<u32>(a + 3404) : 0;
    if (!h)
      continue;
    const u32 state = bd::mem::try_load<u32>(h + 32);
    seen += fmt::format(" | acteur {} objet {:08X} ({:08X}) etat {} {:08X} {:08X}", i, h, bd::mem::try_load<u32>(h), state,
                        bd::mem::try_load<u32>(h + 36), bd::mem::try_load<u32>(h + 40));
    const u32 b = bd::mem::try_load<u32>(h + 36), d = bd::mem::try_load<u32>(h + 40);
    if (!obj && state == 2 && b != d && IndexOf(b) >= 0 && IndexOf(d) >= 0)
      by = b, held = d, obj = h;
  }
  if (!obj) {
    static int s_lines = 0;
    if (now - g_pair_last >= 5000 && s_lines < 12) {
      g_pair_last = now;
      ++s_lines;
      BD_INFO("[motion guard] essai prise : attente d'une prise ({} s depuis le chargement){}", (now - start) / 1000,
              seen.empty() ? std::string(" | aucun objet de prise") : seen);
    }
    return;
  }
  if (g_pair_try && now - g_pair_try < 1000)
    return;
  g_pair_try = now;
  if (Anim(held) == u16(n) || ++g_pair_tries > 5) {
    g_pair_done = true;
    BD_INFO("[motion guard] essai prise : fini (pris {} motion {}, porteur {} motion {})", IndexOf(held), Anim(held),
            IndexOf(by), Anim(by));
    return;
  }
  const u16 before = Anim(held), by_anim = Anim(by);
  PlayOn(ctx, base, held, u32(n));
  BD_INFO("[motion guard] essai prise {} : objet {:08X}, porteur {} ({:08X}, motion {}), pris {} ({:08X}) doit jouer la "
          "motion {} : motion {} -> {} (filet {})",
          g_pair_tries, obj, IndexOf(by), by, by_anim, IndexOf(held), held, n, before, Anim(held),
          REXCVAR_GET(wwe12_motion_guard) ? "actif" : "coupe");
}

void Check(const PPCContext &ctx, u8 *base, u32 actor, int64_t now) {
  const u16 a = Anim(actor);
  Watch &w = g_watch[actor];
  if (a != w.anim) {
    w = {a, now, false};
    return;
  }
  int how = 0;
  if (w.done || now - w.since < kHoldMs || a == 0xFFFF || !Missing(a, &how))
    return;
  w.done = true;
  const u32 other = Partner(actor);
  if (!REXCVAR_GET(wwe12_motion_guard)) {
    if (More())
      BD_INFO("[motion guard] acteur {} ({:08X}) fige depuis 1 s dans la motion {} absente du match (filet coupe){}",
              IndexOf(actor), actor, a,
              other ? fmt::format(" ; partenaire de prise {} ({:08X}, motion {})", IndexOf(other), other, Anim(other))
                    : std::string(" ; pas de partenaire de prise"));
    return;
  }
  Free(ctx, base, actor);
  if (other) {
    Free(ctx, base, other);
    g_watch[other].done = true;
  }
  if (More())
    BD_INFO("[motion guard] acteur {} ({:08X}) fige depuis 1 s dans la motion {} absente du match -> remis libre "
            "(motion {}){}",
            IndexOf(actor), actor, a, Anim(actor),
            other ? fmt::format(", avec son partenaire de prise {} ({:08X}, motion {})", IndexOf(other), other,
                                Anim(other))
                  : std::string());
}
}  // namespace

// The motion directory was made again (wwe12_iquit.cpp, sub_8246E3B0 slot 1: each match's load): the numbers noted
// missing belong to the last match.
// It is also the test aid's clock: the match is loading.
void Wwe12MotionGuardDirectoryMade() {
  {
    std::lock_guard lock(g_mutex);
    g_missing.clear();
  }
  g_match_ms = NowMs();
  g_test_done = false;
  g_test_stood = 0;
  g_tries = 0;
  g_pair_done = false;
  g_pair_tries = 0;
  g_pair_last = 0;
  g_pair_try = 0;
}

// The loader's verdict on a queue entry (wwe12_motion_trace.cpp, sub_821D7E78): 5 = given up.
void Wwe12MotionGuardLoaded(u32 number, u32 state) {
  if (state == 5 && number && number < 0xFFFF)
    Note(u16(number), 5);
}

// The directory asked by the queue's reader (0x821D7D00): 0 = the number is dropped.
extern "C" REX_FUNC(sub_8246EB78) {
  const u32 lr = u32(ctx.lr), key = ctx.r4.u32;
  const u16 n = lr == 0x821D7D04 && key ? bd::mem::try_load<u16>(key) : 0xFFFF;
  __imp__sub_8246EB78(ctx, base);
  if (n != 0xFFFF && n && ctx.r3.u32 == 0)
    Note(n, 1);
}

// Each actor's pad word (wwe12_iquit.cpp, sub_8225C4D8: the main thread): one pass over the six actors a frame at
// most, whichever pad is delivered (player 1's may not come this way: the first test saw nothing of him).
void Wwe12MotionGuardActor(const PPCContext &ctx, u8 *base, u32 actor) {
  if (!actor || !bd::mem::ready())
    return;
  static int64_t s_pass = 0;
  const int64_t now = NowMs();
  if (now - s_pass < 15)
    return;
  s_pass = now;
  TestAid(ctx, base, now);
  PairAid(ctx, base, now);
  for (u32 i = 0; i < 6; ++i)
    if (const u32 a = Actor(i))
      Check(ctx, base, a, now);
}
