// I QUIT match (1.4, session Gameplay): WWE '13's I Quit mini-game, rebuilt for WWE '12 with WWE '13's own rules.
//
// Source of the rules (read locally on the player's machine, 2026-11-14; the logic is rewritten here, no WWE '13 code
// or data is copied): WWE '13's InGameLua.pac script (states StateIQuitMiniGame*) and its native mini-game 'IQMG'
// (base xex: create sub_82565E20, per frame sub_82565BF0, gauge sub_825650F0 / sub_82564620 / sub_82563E70, threat
// sub_825641B0 / sub_825642C0, the end sub_82565A30 / sub_825657C0). Time below is in WWE '13's ticks (1/60 s).
//
// - Asking (Lua Navi_Grapple + CheckIQuitAskHim): the pin button (B) next to an opponent lying face up, face down or
//   hurting on the ground, both on the same side of the ropes; no distance check. The attacker calls the referee
//   (motion 13210 / 13211), the referee kneels with his microphone (13200, loop 13201), the asked man answers
//   (13208 face up / 13209 face down).
// - The gauge (only the asked man plays it): a bar 0-124, a cursor starting at 0 going left and right at 6.5 units a
//   tick (difficulty 1; 5 / 6.5 / 8.5 / 10 by difficulty). The "NO" zone: width = health % x 0.40 (6-93), its centre
//   62 +- 62 at random, away from the cursor. A pressed in the zone (+-1): he resists. A outside: the cursor freezes
//   30 ticks and the zone moves. 10 s without resisting: he QUITS. "I QUIT" whitens one step a second (9 steps).
// - The computer asked presses after a reaction delay once the cursor enters the zone, without checking again (a late
//   press misses and takes the 30 ticks).
// - The threat (Lua SetMotionAppeal, IQuit_CheckMakeAThreat): a taunt while the referee asks, within 400 cm: the
//   attacker threatens (13230-13234 by the taunt's direction) and the gauge shakes 200 ticks (70 x (1 - t/200)^2);
//   nothing else changes. Once at a time.
// - A strike on the asked man (the game's own ground strike; WWE '13 plays 13220): the ask goes on as long as he
//   stays on the ground; if he gets up, the ask stops without an answer.
// - Resisted: he plays 13205 (face up) / 13206 (face down), the referee 13207, then stands up; the attacker can ask
//   again. Quit: the match ends I QUIT (GiveUp: the game's own submission end, sub_82261320 reason 2).
// The WWE '13 motions exist only in the player's own game, ported by the Mod Maker from his WWE '13 disc
// (13200-13234 -> 13150-13184, mods\.svrmods\Motions\iquit_*_wwe13); without them (wwe12_iquit_wwe13_motions off)
// the referee uses WWE '12's asking motion 203.
//
// The match is the SUBMISSION one marked I Quit (wwe12_match_modes.cpp: its own ONE ON ONE row; no pinfall, no
// count-out, no rope breaks); the judge (sub_82263440) ends it with the submission reason, whose texts read "I QUIT".
// Submissions of the game in this match: "I QUIT" over the game's own meter (WWE '13's mode 1 runs the same gauge in
// the hold: not done yet, the probe line "[iquit] soumission" logs both superstars' motions to find the held man).

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(wwe12_iquit_ask, true, "WWE12",
                    "I Quit match: B next to a downed opponent calls the referee and starts the I QUIT gauge (as WWE '13).");
REXCVAR_DEFINE_BOOL(wwe12_iquit_wwe13_motions, false, "WWE12",
                    "I Quit match: play WWE '13's motions (ported by the Mod Maker from the player's own WWE '13 as "
                    "13150-13184).");
REXCVAR_DEFINE_INT32(wwe12_iquit_ref_override, 0, "WWE12",
                     "Dev (Mod Maker test): one motion for the referee instead of WWE '13's 13150 / 13151 / 13157 (0 = off).");
REXCVAR_DEFINE_INT32(wwe12_iquit_difficulty, 1, "WWE12", "I Quit match: the gauge's difficulty as WWE '13 (0-3).");

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <random>

#include <imgui.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>

#include "svr_hook.h"
#include "core/logging.h"
#include "core/memory_helpers.h"

bool Wwe12IQuitMatch();  // wwe12_match_modes.cpp: an I Quit match is set up

REX_EXTERN(__imp__sub_8247AD18);  // play an animation on an actor
REX_EXTERN(__imp__sub_82261320);  // the end of a match for a superstar (victim, reason, winner)

namespace {

using Clock = std::chrono::steady_clock;
constexpr u32 kActors = 0x8317B770;
constexpr int kReferee = 6;
constexpr u16 kPadA = 0x1000, kPadB = 0x2000, kPadDpad = 0x000F;
constexpr u16 kDpadUp = 0x1, kDpadDown = 0x2, kDpadLeft = 0x4, kDpadRight = 0x8;

// WWE '13's numbers
constexpr float kBar = 124.f;
constexpr double kLimit = 10.0 * 60;   // ticks before he quits
constexpr double kLockout = 30;        // ticks frozen after a press outside the zone
constexpr double kThreatTicks = 200;
constexpr double kCallTicks = 60;      // the attacker calls, the referee kneels (then the gauge)
constexpr double kAnswerTicks = 90;    // the answer shown

// the WWE '13 motions as ported by the Mod Maker (132NN -> 131(NN + 50))
constexpr u32 W13(u32 m) { return m - 50; }
constexpr u32 kRefIdle = 20152;  // WWE '12's referee standing (seen in the probes)

std::atomic<u16> g_pad[4];
std::atomic<u16> g_pressed[4];  // new presses since the last tick (cleared there)

// the mini-game (written on the game thread, read by the overlay)
enum Phase { kIdle, kCall, kGauge, kAnswer };
std::atomic<int> g_phase{kIdle};
std::atomic<float> g_cursor{0.f}, g_centre{62.f}, g_width{40.f}, g_white{0.f}, g_shake{0.f};
std::atomic<int> g_result{0};      // 0 running, 1 resisted ("NO"), 4 quit
std::atomic<int> g_ask{0};         // a human's B next to a downed opponent: his place + 1
std::atomic<int> g_taunt{0};       // a human attacker's taunt during the ask: the D-pad bit
std::atomic<bool> g_hint{false};   // the "B ASK HIM" prompt is shown
std::atomic<bool> g_sub{false};    // a submission of the game itself in the I Quit match: "I QUIT" shown over it
int g_attacker = -1, g_victim = -1;
bool g_face_down = false, g_cpu_victim = false, g_we_asked = false;
double g_ticks = 0, g_phase_ticks = 0, g_lockout = 0, g_threat = -1, g_cpu_press_at = -1;
float g_speed = 6.5f, g_dir = 1.f;
bool g_was_in_zone = false;
Clock::time_point g_last, g_sub_start, g_down_since[6], g_cpu_next_ask;
std::mt19937 g_rng{std::random_device{}()};

bool Motions13() { return REXCVAR_GET(wwe12_iquit_wwe13_motions); }

// the referee's motion: WWE '13's, or the Mod Maker's test motion
u32 RefMotion(u32 m) {
  const int o = REXCVAR_GET(wwe12_iquit_ref_override);
  return o > 0 && o < 65535 ? u32(o) : W13(m);
}

u32 Actor(int i) { return bd::mem::try_load<u32>(kActors + u32(i) * 4); }

u16 Anim(u32 actor) {
  const u32 st = actor ? bd::mem::try_load<u32>(actor + 680) : 0;
  return st ? bd::mem::try_load<u16>(st) : 0xFFFF;
}

bool IsOneOf(u16 a, std::initializer_list<u32> list) {
  for (u32 v : list)
    if (a == u16(v))
      return true;
  return false;
}

// Lying down or hurting on the ground (animations seen in the probes of 2026-11-14: 0 / 1 lying face up / down,
// 20 / 21 getting up, 440-480 the ground pain and the slow getting up after a big move); and our own asked / answer
// motions (he is still on the ground in them).
bool Down(u32 actor) {
  const u16 a = Anim(actor);
  return a <= 30 || (a >= 440 && a <= 480) ||
         IsOneOf(a, {W13(13205), W13(13206), W13(13208), W13(13209)});
}
bool FaceDown(u32 actor) { return Anim(actor) == 1 || Anim(actor) == W13(13209) || Anim(actor) == W13(13206); }

// The pad of a place (the select object's places: +44, 255 = the computer).
// (in a match the select object no longer says it: user's test of 2026-11-14, both superstars read as the computer.
// Then, as in a 1 on 1 exhibition against the computer, the first superstar is the first pad's.)
int PadOf(int place) {
  const u32 m = bd::mem::try_load<u32>(0x8324DEFC);
  int pads[2] = {-1, -1};
  if (m) {
    const u32 sel = m + bd::mem::try_load<u32>(m + 15960) * 1596 + 124;
    for (int k = 0; k < 2; ++k) {
      const u8 pad = bd::mem::try_load<u8>(sel + u32(k) * 244 + 44);
      pads[k] = pad < 4 ? int(pad) : -1;
    }
  }
  if (pads[0] < 0 && pads[1] < 0)
    pads[0] = 0;
  return place < 2 ? pads[place] : -1;
}

// health % as WWE '13 (sub_8251AA50: the four body parts, current over full)
float HealthRatio(u32 actor) {
  const u32 dmg = bd::mem::try_load<u32>(actor + 3364);
  if (!dmg)
    return 1.f;
  float sum = 0;
  for (u32 k = 0; k < 4; ++k) {
    const int16_t v = int16_t(bd::mem::try_load<u16>(dmg + 56 + k * 2));
    sum += std::clamp(float(v) / 120.f, 0.f, 1.f);
  }
  return sum / 4.f;
}

void PlayAnim(const PPCContext &in, u8 *base, u32 actor, u32 anim) {
  if (!actor || !anim)
    return;
  PPCContext c = in;
  c.r1.u64 = (in.r1.u32 - 512) & ~15u;
  c.r3.u64 = actor;
  c.r4.u64 = anim;
  c.r5.u64 = 0;
  c.r6.u64 = 0;
  c.r8.u64 = 1;
  c.r9.u64 = 0;
  const u32 bits = bd::mem::try_load<u32>(0x82000AD8);  // (the float the game passes, lfs f1,2776 of 0x82000000)
  float f;
  std::memcpy(&f, &bits, 4);
  c.f1.f64 = f;
  __imp__sub_8247AD18(c, base);
  // (for the Mod Maker: the motion queue sub_821D7480 / sub_821A55B8 reads the actor's bytes *[+684] and *[+688]
  // and [+92] with the number; logged to find the group a superstar's motions are read from)
  const u32 p684 = bd::mem::try_load<u32>(actor + 684), p688 = bd::mem::try_load<u32>(actor + 688);
  BD_INFO("[iquit] motion {} sur {:08X} : +92 {} *+684 {} *+688 {} -> anim {}", anim, actor,
          bd::mem::try_load<u32>(actor + 92), p684 ? bd::mem::try_load<u8>(p684) : 255,
          p688 ? bd::mem::try_load<u8>(p688) : 255, Anim(actor));
}

// The game's own submission give-up (sub_82229320 -> sub_82261320(victim, 2, winner), read 2026-11-14): victim +999 =
// 2 (the judge sub_82263440 turns it into the end reason 6, the submission one, through its table at 0x82005080:
// 1->2, 2->6, 3->1, 4->5, 6->8, 7->3 DQ, 20->10), +1000 = the winner, his stats and the bell. (7 was the referee's
// 5 count, a DQ: user's test of 2026-11-14 ended "by way of DQ".)
void GiveUp(const PPCContext &in, u8 *base) {
  const u32 victim = Actor(g_victim);
  if (!victim)
    return;
  if (bd::mem::try_load<u8>(victim + 999) == 21)
    return;
  PPCContext c = in;
  c.r1.u64 = (in.r1.u32 - 512) & ~15u;
  c.r3.u64 = victim;
  c.r4.u64 = 2;
  c.r5.u64 = u32(g_attacker);
  __imp__sub_82261320(c, base);
  BD_INFO("[iquit] catcheur {} abandonne : I QUIT (gagnant {})", g_victim, g_attacker);
}

void ShowGauge();  // below

// WWE '13 sub_82563E70: a new zone, its centre 62 +- 62 at random, at least half its width from the cursor (100 tries)
void RollZone() {
  const float w = g_width.load(), cur = g_cursor.load();
  float c = 62.f;
  for (int i = 0; i < 100; ++i) {
    c = 62.f + (float(std::uniform_int_distribution<int>(0, 255)(g_rng)) - 128.f) / 128.f * 62.f;
    if (std::fabs(c - cur) >= w * 0.5f)
      break;
  }
  g_centre = std::clamp(c, w * 0.5f, kBar - w * 0.5f);
}

// the computer asked: a reaction delay (ticks) once the cursor enters the zone
double CpuDelay() { return std::uniform_int_distribution<int>(2, 12)(g_rng); }

void StartAsk(const PPCContext &in, u8 *base, int attacker, int victim) {
  g_attacker = attacker;
  g_victim = victim;
  const u32 att = Actor(attacker), vic = Actor(victim), ref = Actor(kReferee);
  g_face_down = FaceDown(vic);
  g_cpu_victim = PadOf(victim) < 0;
  g_phase_ticks = 0;
  g_ticks = 0;
  g_result = 0;
  g_white = 0.f;
  g_shake = 0.f;
  g_threat = -1;
  g_cursor = 0.f;
  g_dir = 1.f;
  g_lockout = 0;
  g_cpu_press_at = -1;
  g_was_in_zone = false;
  g_phase = kCall;
  // the attacker calls the referee (13210 / 13211), the referee kneels with his microphone (13200)
  if (Motions13()) {
    PlayAnim(in, base, att, W13(std::uniform_int_distribution<int>(0, 1)(g_rng) ? 13211 : 13210));
    PlayAnim(in, base, ref, RefMotion(13200));
  } else {
    PlayAnim(in, base, ref, 203);
  }
  g_we_asked = true;
  BD_INFO("[iquit] le catcheur {} appelle l'arbitre pour le catcheur {} ({}{})", attacker, victim,
          g_face_down ? "sur le ventre" : "sur le dos", g_cpu_victim ? ", ordinateur" : "");
}

void StartGauge(const PPCContext &in, u8 *base) {
  const u32 vic = Actor(g_victim), ref = Actor(kReferee);
  // WWE '13 sub_82564620: speed and width by difficulty, the width by health
  static constexpr float kSpeed[4] = {5.0f, 6.5f, 8.5f, 10.0f};
  static constexpr float kWidthK[4] = {50.f, 40.f, 30.f, 30.f};
  const int d = std::clamp(int(REXCVAR_GET(wwe12_iquit_difficulty)), 0, 3);
  const float health = HealthRatio(vic);
  g_speed = kSpeed[d];
  g_width = std::clamp(health * 100.f * 0.01f * kWidthK[d], 6.f, 93.f);
  g_cursor = 0.f;
  g_dir = 1.f;
  RollZone();
  g_ticks = 0;
  g_phase = kGauge;
  ShowGauge();
  if (Motions13()) {
    PlayAnim(in, base, vic, W13(g_face_down ? 13209 : 13208));
    PlayAnim(in, base, ref, RefMotion(13201));
  }
  BD_INFO("[iquit] la jauge : vie {:.0f} %, zone {:.0f} de large en {:.0f}, vitesse {}", health * 100, g_width.load(),
          g_centre.load(), g_speed);
}

void Answer(const PPCContext &in, u8 *base, int result) {
  g_result = result;
  g_phase = kAnswer;
  g_phase_ticks = 0;
  g_shake = 0.f;
  const u32 vic = Actor(g_victim), ref = Actor(kReferee);
  if (result == 1) {
    BD_INFO("[iquit] le catcheur {} resiste (NO)", g_victim);
    if (Motions13()) {
      PlayAnim(in, base, vic, W13(g_face_down ? 13206 : 13205));
      PlayAnim(in, base, ref, RefMotion(13207));
    }
  } else {
    GiveUp(in, base);
  }
}

void EndAsk(const PPCContext &in, u8 *base) {
  g_phase = kIdle;
  const u32 ref = Actor(kReferee);
  if (ref && IsOneOf(Anim(ref), {203, W13(13200), W13(13201), W13(13207), RefMotion(13200)}))
    PlayAnim(in, base, ref, kRefIdle);  // (the referee stands up again)
}

}  // namespace

// From the XInputGetState hook (through wwe12_match_modes.cpp): every pad's buttons. Returns the buttons the game
// must not see: in an I Quit match, B next to a downed opponent is ours (the game would start a submission: user's
// test of 2026-11-14); during the ask, A / B, and the attacker's D-pad (his threat).
u16 Wwe12IQuitPad(u32 user, u16 buttons) {
  if (user >= 4)
    return 0;
  const u16 before = g_pad[user].exchange(buttons);
  const u16 newly = u16(buttons & ~before);
  if (newly)
    g_pressed[user].fetch_or(newly);
  if (!REXCVAR_GET(wwe12_iquit_ask) || !Wwe12IQuitMatch())
    return 0;
  if (g_phase.load() != kIdle) {
    const bool attacker = g_attacker >= 0 && PadOf(g_attacker) == int(user);
    if (attacker && g_phase.load() == kGauge && (newly & kPadDpad))
      g_taunt = newly & kPadDpad;
    return u16(kPadA | kPadB | (attacker ? kPadDpad : 0));
  }
  for (int a = 0; a < 2; ++a)
    if (PadOf(a) == int(user) && !Down(Actor(a)) && Down(Actor(1 - a))) {
      if (newly & kPadB)
        g_ask = a + 1;  // (asked now, next to him: the tick starts the mini-game)
      return kPadB;
    }
  return 0;
}

// Once per game frame during a match (wwe12_match_modes.cpp, from the gauge hook of the first superstar).
void Wwe12IQuitTick(const PPCContext &ctx, u8 *base) {
  if (!REXCVAR_GET(wwe12_iquit_ask) || !Wwe12IQuitMatch()) {
    g_phase = kIdle;
    g_sub = false;
    g_hint = false;
    return;
  }
  u16 pressed[4];
  for (int p = 0; p < 4; ++p)
    pressed[p] = g_pressed[p].exchange(0);
  const auto now = Clock::now();
  // WWE '13's ticks (1/60 s) from the real time (the game runs at 30 or 60)
  const double dt = std::clamp(std::chrono::duration<double>(now - g_last).count() * 60.0, 0.0, 4.0);
  g_last = now;

  const int phase = g_phase.load();
  if (phase != kIdle) {
    g_phase_ticks += dt;
    const u32 vic = Actor(g_victim), att = Actor(g_attacker);
    if (phase == kAnswer) {
      if (g_phase_ticks >= kAnswerTicks)
        EndAsk(ctx, base);
      return;
    }
    // he got up (a strike that lifts him, a roll away...): the ask stops without an answer (WWE '13 result 3)
    // (65535: no motion read, e.g. a WWE '13 motion the game did not find: user's test of 2026-11-14 stopped there)
    if (!vic || !att || (!Down(vic) && Anim(vic) != 0xFFFF)) {
      BD_INFO("[iquit] le catcheur {} n'est plus au sol (anim {}) : plus de question", g_victim, Anim(vic));
      EndAsk(ctx, base);
      return;
    }
    if (phase == kCall) {
      if (g_phase_ticks >= kCallTicks)
        StartGauge(ctx, base);
      return;
    }
    // the gauge
    g_ticks += dt;
    g_white = float(std::min(9.0, std::floor(g_ticks / 60.0)) / 9.0);
    // the threat: the attacker's taunt (a human's D-pad; the computer once, at random) while the referee asks
    int taunt = g_taunt.exchange(0);
    if (PadOf(g_attacker) < 0 && g_threat < 0 && g_ticks > 90 && g_ticks < 120 &&
        std::uniform_int_distribution<int>(0, 99)(g_rng) < 2)
      taunt = kDpadUp;
    if (taunt && g_threat < 0) {
      g_threat = 0;
      const int n = (taunt & kDpadUp) ? 1 : (taunt & kDpadRight) ? 2 : (taunt & kDpadDown) ? 3 : 4;
      if (Motions13())
        PlayAnim(ctx, base, att, W13(13229 + u32(n)));
      BD_INFO("[iquit] le catcheur {} menace son adversaire", g_attacker);
    }
    if (g_threat >= 0) {
      g_threat += dt;
      const double f = 1.0 - g_threat / kThreatTicks;
      g_shake = g_threat < kThreatTicks ? float(70.0 * f * f * std::sin(g_threat * 1.3)) : 0.f;
      if (g_threat >= kThreatTicks)
        g_threat = -1;
    }
    // the cursor, left and right
    if (g_lockout > 0) {
      g_lockout -= dt;
    } else {
      float c = g_cursor.load() + g_speed * float(dt) * g_dir;
      if (c >= kBar) {
        c = kBar;
        g_dir = -1.f;
      } else if (c <= 0.f) {
        c = 0.f;
        g_dir = 1.f;
      }
      g_cursor = c;
    }
    const auto in_zone = [] { return std::fabs(g_cursor.load() - g_centre.load()) <= g_width.load() * 0.5f + 1.f; };
    bool press = false;
    if (g_cpu_victim) {
      const bool in = in_zone();
      if (in && !g_was_in_zone && g_cpu_press_at < 0 && g_lockout <= 0)
        g_cpu_press_at = g_ticks + CpuDelay();
      g_was_in_zone = in;
      if (g_cpu_press_at >= 0 && g_ticks >= g_cpu_press_at) {
        g_cpu_press_at = -1;
        press = true;
      }
    } else {
      const int pad = PadOf(g_victim);
      press = pad >= 0 && (pressed[pad] & kPadA) && g_lockout <= 0;
    }
    if (press) {
      if (in_zone()) {
        Answer(ctx, base, 1);
        return;
      }
      g_lockout = kLockout;  // (missed: frozen 30 ticks, the zone moves)
      RollZone();
    }
    if (g_ticks >= kLimit)
      Answer(ctx, base, 4);
    return;
  }

  // a submission of the game itself (the referee asks with his own animation, not ours): "I QUIT" over it
  {
    const u32 ref = Actor(kReferee);
    const bool asking = ref && Anim(ref) == 203;
    if (!asking)
      g_we_asked = false;
    if (asking && !g_we_asked) {
      if (!g_sub.exchange(true)) {
        g_sub_start = now;
        ShowGauge();
        BD_INFO("[iquit] soumission du jeu : catcheur 0 anim {}, catcheur 1 anim {}", Anim(Actor(0)), Anim(Actor(1)));
      }
      g_white = float(std::min(9.0, std::floor(std::chrono::duration<double>(now - g_sub_start).count())) / 9.0);
    } else {
      g_sub = false;
    }
  }
  // who can ask whom: two superstars, one lying down
  bool hint = false;  // a human standing over his downed opponent: "B ASK HIM" shown
  for (int a = 0; a < 2; ++a) {
    const int v = 1 - a;
    const u32 att = Actor(a), vic = Actor(v);
    if (!att || !vic || Down(att))
      continue;
    if (!Down(vic)) {
      g_down_since[v] = now;
      continue;
    }
    const int pad = PadOf(a);
    if (pad >= 0)
      hint = true;
    const bool human_asks = pad >= 0 && g_ask.load() == a + 1;
    // the computer asks a worn-out opponent (under 70 % health) who has been down 3 s, at most every 15 s
    const bool cpu_asks = pad < 0 && now - g_down_since[v] > std::chrono::seconds(3) && now > g_cpu_next_ask &&
                          HealthRatio(vic) < 0.7f;
    if (human_asks || cpu_asks) {
      g_ask = 0;
      if (cpu_asks)
        g_cpu_next_ask = now + std::chrono::seconds(15);
      g_hint = false;
      StartAsk(ctx, base, a, v);
      return;
    }
  }
  g_ask = 0;  // (a B the game did not see but that asked no one now)
  if (hint && !g_hint.exchange(true))
    ShowGauge();
  else if (!hint)
    g_hint = false;
}

// ---- the WWE '13 motions: known to the game's motion directory ----
// Why they stayed frozen (2026-11-14, read in the generated code + the Mod Maker's trace): a motion played by number
// is queued (sub_821D7480) and the queue's reader (sub_821D7C80) drops any number that has no record in the motion
// directory [0x8217B870]; a superstar's records (directory slot k + 11) are built by sub_8246E3B0(directory, bitmap,
// slot) from a 65536-bit bitmap (bit n = word n / 32, 1 << n % 32; sub_821B4610) that sub_823B4628 fills with his
// moveset's motions and a fixed list (10125, 13100-13135 the referee's). Then the archives are walked
// (sub_824868F8, /ZMOT/CA00-CA09) and only the entries whose number has a record are read. Here: the Mod Maker's
// numbers 13140-13199 are added to the bitmap of every superstar / referee slot (11-22), so the game reads them from
// the Mod Maker's archive and the queue finds them.
REX_EXTERN(__imp__sub_8246E3B0);
extern "C" REX_FUNC(sub_8246E3B0) {
  const u32 bitmap = ctx.r4.u32, slot = ctx.r5.u32;
  if (REXCVAR_GET(wwe12_iquit_wwe13_motions) && bitmap && slot >= 11 && slot <= 22) {
    for (u32 n = 13140; n < 13200; ++n) {
      const u32 w = bitmap + (n >> 5) * 4;
      bd::mem::store<u32>(w, bd::mem::try_load<u32>(w) | (1u << (n & 31)));
    }
    static std::atomic<int> s_logged{0};
    if (s_logged.fetch_add(1) < 12)
      BD_INFO("[iquit] motions 13140-13199 ajoutees au repertoire (case {})", slot);
  }
  __imp__sub_8246E3B0(ctx, base);
}

// ---- the gauge, drawn over the game ----

namespace {
rex::ui::ImGuiDrawer *g_drawer = nullptr;
std::function<void(std::function<void()>)> g_post;  // runs a function on the UI thread
std::atomic<bool> g_shown{false};                    // the gauge's dialog exists (UI thread owns it)

class IQuitOverlay final : public rex::ui::ImGuiDialog {
 public:
  explicit IQuitOverlay(rex::ui::ImGuiDrawer *drawer) : ImGuiDialog(drawer) {}

 protected:
  void OnClose() override { g_shown = false; }
  void OnDraw(ImGuiIO &io) override {
    const int phase = g_phase.load();
    const bool gauge = phase == kGauge || phase == kAnswer;
    const bool sub_only = phase == kIdle && g_sub.load();
    const bool hint_only = phase == kIdle && !sub_only && g_hint.load();
    if (!gauge && !sub_only && !hint_only) {
      if (phase == kCall)
        return;  // (the referee is coming: the gauge next)
      Close();   // (no dialog left: the drawer stops drawing ImGui every frame)
      return;
    }
    const float k = std::max(0.6f, io.DisplaySize.y / 1080.0f);
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    ImFont *font = ImGui::GetFont();
    if (hint_only) {
      // the button prompt: a red B and "ASK HIM", low in the middle of the screen
      const char *ask = "ASK HIM";
      const float as = 30.f * k, r = 17.f * k;
      const ImVec2 asz = font->CalcTextSizeA(as, FLT_MAX, 0.f, ask);
      const float total = 2 * r + 10 * k + asz.x;
      const float bx = io.DisplaySize.x * 0.5f - total * 0.5f + r, by = io.DisplaySize.y * 0.80f;
      dl->AddCircleFilled(ImVec2(bx + 2 * k, by + 2 * k), r, IM_COL32(0, 0, 0, 160));
      dl->AddCircleFilled(ImVec2(bx, by), r, IM_COL32(205, 30, 30, 255));
      dl->AddCircle(ImVec2(bx, by), r, IM_COL32(255, 255, 255, 220), 0, 2 * k);
      const ImVec2 bsz = font->CalcTextSizeA(as * 0.85f, FLT_MAX, 0.f, "B");
      dl->AddText(font, as * 0.85f, ImVec2(bx - bsz.x * 0.5f, by - bsz.y * 0.5f), IM_COL32(255, 255, 255, 255), "B");
      const float tx = bx + r + 10 * k, ty = by - asz.y * 0.5f;
      dl->AddText(font, as, ImVec2(tx + 2 * k, ty + 2 * k), IM_COL32(0, 0, 0, 200), ask);
      dl->AddText(font, as, ImVec2(tx, ty), IM_COL32(255, 255, 255, 255), ask);
      return;
    }
    const float shake = g_shake.load() * k * 0.5f;  // (the threat shakes the gauge)
    const float w = 400.f * k, h = 26.f * k;
    const float x = io.DisplaySize.x * 0.5f - w * 0.5f + shake, y = io.DisplaySize.y * 0.58f + shake * 0.3f;
    // "I QUIT", whiter and whiter (one step a second)
    const float white = g_white.load();
    const int g = int(90 + 165 * white);
    const char *title = "I QUIT";
    const float ts = 34.f * k;
    const ImVec2 tsz = font->CalcTextSizeA(ts, FLT_MAX, 0.f, title);
    dl->AddText(font, ts, ImVec2(x + w * 0.5f - tsz.x * 0.5f + 2 * k, y - tsz.y - 6 * k + 2 * k), IM_COL32(0, 0, 0, 200),
                title);
    dl->AddText(font, ts, ImVec2(x + w * 0.5f - tsz.x * 0.5f, y - tsz.y - 6 * k), IM_COL32(g, g, g, 255), title);
    if (sub_only)
      return;  // (a submission of the game: its own meter, our title over it)
    // the bar (0-124), the "NO" zone and the cursor
    dl->AddRectFilled(ImVec2(x - 4 * k, y - 4 * k), ImVec2(x + w + 4 * k, y + h + 4 * k), IM_COL32(230, 230, 230, 255),
                      3 * k);
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(20, 20, 20, 255));
    const float zc = g_centre.load() / kBar, zw = g_width.load() / kBar;
    const float z0 = x + w * (zc - zw * 0.5f), z1 = x + w * (zc + zw * 0.5f);
    dl->AddRectFilled(ImVec2(z0, y + 3 * k), ImVec2(z1, y + h - 3 * k), IM_COL32(210, 20, 20, 255));
    const float cx = x + w * (g_cursor.load() / kBar);
    dl->AddRectFilled(ImVec2(cx - 3 * k, y - 6 * k), ImVec2(cx + 3 * k, y + h + 6 * k), IM_COL32(255, 255, 255, 255));
    const char *non = "NO";
    const float ns = 26.f * k;
    const ImVec2 nsz = font->CalcTextSizeA(ns, FLT_MAX, 0.f, non);
    const float nx = (z0 + z1) * 0.5f - nsz.x * 0.5f;
    dl->AddText(font, ns, ImVec2(nx + 2 * k, y + h + 10 * k + 2 * k), IM_COL32(0, 0, 0, 200), non);
    dl->AddText(font, ns, ImVec2(nx, y + h + 10 * k), IM_COL32(255, 255, 255, 255), non);
    if (const int r = g_result.load()) {
      const char *msg = r == 1 ? "NO !" : "I QUIT !";
      const float ms = 48.f * k;
      const ImVec2 msz = font->CalcTextSizeA(ms, FLT_MAX, 0.f, msg);
      const ImU32 col = r == 1 ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 60, 60, 255);
      dl->AddText(font, ms, ImVec2(io.DisplaySize.x * 0.5f - msz.x * 0.5f + 3 * k, y + h + 50 * k + 3 * k),
                  IM_COL32(0, 0, 0, 220), msg);
      dl->AddText(font, ms, ImVec2(io.DisplaySize.x * 0.5f - msz.x * 0.5f, y + h + 50 * k), col, msg);
    }
  }
};
}  // namespace

// From the app's start-up (wwe12_app.h, next to Wwe12DisplayInit): the drawer and a way to the UI thread.
void Wwe12IQuitOverlayInit(rex::ui::ImGuiDrawer *drawer, std::function<void(std::function<void()>)> post) {
  g_drawer = drawer;
  g_post = std::move(post);
}

namespace {
void ShowGauge() {
  if (!g_drawer || !g_post || g_shown.exchange(true))
    return;
  g_post([] { new IQuitOverlay(g_drawer); });  // deletes itself when the mini-game is over
}
}  // namespace
