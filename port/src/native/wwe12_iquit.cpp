// I QUIT match (1.4, session Gameplay): WWE '13's I Quit mini-game, rebuilt for WWE '12 with WWE '13's own rules.
//
// Source of the rules (read locally on the player's machine, 2026-11-14; the logic is rewritten here, no WWE '13 code
// or data is copied): WWE '13's InGameLua.pac script (states StateIQuitMiniGame*) and its native mini-game 'IQMG'
// (base xex: create sub_82565E20, per frame sub_82565BF0, gauge sub_825650F0 / sub_82564620 / sub_82563E70, threat
// sub_825641B0 / sub_825642C0, the end sub_82565A30 / sub_825657C0). Time below is in WWE '13's ticks (1/60 s).
//
// - Asking (Lua Navi_Grapple + CheckIQuitAskHim): the pin button (B) next to an opponent lying face up, face down or
//   hurting on the ground, both on the same side of the ropes; no distance check. The referee asks.
// - The gauge (only the asked man plays it): a bar 0-124, a cursor starting at 0 going left and right at 6.5 units a
//   tick (difficulty 1; 5 / 6.5 / 8.5 / 10 by difficulty). The "NO" zone: width = health % x 0.40 (6-93), its centre
//   62 +- 62 at random, away from the cursor. A pressed in the zone (+-1): he resists. A outside: the cursor freezes
//   30 ticks and the zone moves. 10 s without resisting: he QUITS. "I QUIT" whitens one step a second (9 steps).
// - The computer asked presses after a reaction delay once the cursor enters the zone, without checking again (a late
//   press misses and takes the 30 ticks).
// - The threat (Lua SetMotionAppeal, IQuit_CheckMakeAThreat): a taunt while the referee asks: the gauge shakes
//   200 ticks (70 x (1 - t/200)^2); nothing else changes. Once at a time.
// - A strike on the asked man: the ask goes on as long as he stays on the ground; if he gets up, the ask stops
//   without an answer.
// - Resisted: the referee stands up; the attacker can ask again. Quit: the match ends I QUIT (GiveUp: the game's own
//   submission end, sub_82261320 reason 2).
// The user's decision (2026-11-14): WWE '13's I Quit "the WWE '12 way": only WWE '12's own motions. The referee asks
// with his submission-check motion 203 (wwe12_iquit_ref_anim), stands with 20152; the threat is the game's own taunt,
// the strike the game's own ground strike, the asked man stays in his own motion on the ground. (WWE '13's motions,
// ported for a test, broke the game's motion pack: a wake-up taunt left the man on the ground. Gone.)
//
// The match is the SUBMISSION one marked I Quit (wwe12_match_modes.cpp: its own ONE ON ONE row; no pinfall, no
// count-out, no rope breaks); the judge (sub_82263440) ends it with the submission reason, whose texts read "I QUIT".
// Submissions of the game in this match: "I QUIT" over the game's own meter (WWE '13's mode 1 runs the same gauge in
// the hold: not done yet, the probe line "[iquit] soumission" logs both superstars' motions to find the held man).

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(wwe12_iquit_ask, true, "WWE12",
                    "I Quit match: B next to a downed opponent calls the referee and starts the I QUIT gauge (as WWE '13).");
REXCVAR_DEFINE_INT32(wwe12_iquit_ref_anim, 203, "WWE12",
                     "I Quit match: the referee's motion when he asks (a WWE '12 motion: 203 = his submission check).");
REXCVAR_DEFINE_INT32(wwe12_iquit_ref_anim_cycle, 0, "WWE12",
                     "Dev: each ask plays the next of this many motions from wwe12_iquit_ref_anim on (0 = always the same).");
REXCVAR_DEFINE_STRING(wwe12_iquit_ref_anim_list, "", "WWE12",
                      "Dev: each ask plays the next motion of this list (numbers with commas), instead of the above.");
REXCVAR_DEFINE_BOOL(wwe12_iquit_wwe13_motions, true, "WWE12",
                    "I Quit match: WWE '13's own motions and images (in the exe since 2026-11-15, 132NN as 131(NN + 50); "
                    "false: WWE '12's motions and our drawing).");
REXCVAR_DEFINE_INT32(wwe12_iquit_difficulty, 1, "WWE12", "I Quit match: the gauge's difficulty as WWE '13 (0-3).");
REXCVAR_DEFINE_BOOL(wwe12_iquit_mic, true, "WWE12",
                    "I Quit match: the referee holds the game's microphone while he asks (as WWE '13).");

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <vector>

#include <imgui.h>
#include <rex/filesystem.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>

#include "../modmaker/svrfmt/texture.h"

REXCVAR_DECLARE(uint32_t, user_language);

#include "svr_hook.h"
#include "core/logging.h"
#include "core/memory_helpers.h"

bool Wwe12IQuitMatch();  // wwe12_match_modes.cpp: an I Quit match is set up
bool IQuitMatch();       // wwe12_match_modes.cpp: I QUIT was the match chosen in the menu (true while it loads too)

REX_EXTERN(__imp__sub_8247AD18);  // play an animation on an actor
REX_EXTERN(__imp__sub_82261320);  // the end of a match for a superstar (victim, reason, winner)
REX_EXTERN(__imp__sub_82520B90);  // a world point to the screen (float[4] in and out), the game's own
REX_EXTERN(__imp__sub_821D7480);  // the motion queue: ask for a motion (queue, number, byte); 4 = in its list
REX_EXTERN(__imp__sub_82324418);  // a submission hold's decision, each frame of the struggle
REX_EXTERN(__imp__sub_82229320);  // the game's tap-out (victim, winner)
REX_EXTERN(__imp__sub_82323FD0);  // a hold: "he taps" (pressure at its top)
REX_EXTERN(__imp__sub_82527010);  // the struggle's own meter and prompts, drawn
REX_EXTERN(__imp__sub_8225C4D8);  // an actor's pad word delivered (pad object, word): the stick that walks him
REX_EXTERN(__imp__sub_82254518);  // a parked object of a type made live again (-1, type, 0, 0): the object, 0 if none
REX_EXTERN(__imp__sub_8221FAD0);  // play a motion on an actor and on the object he holds (actor, motion, block x)
REX_EXTERN(__imp__sub_82250258);  // park an object (hidden, kept for later)
REX_EXTERN(__imp__sub_82251388);  // an object's motion is over: what it does next (let go, lie on the mat...)
REX_EXTERN(__imp__sub_821F5B70);  // a match's list of object models to load (142 with the record's byte + 12097)
REX_EXTERN(__imp__sub_822EE080);  // a match's objects made at its start (a parked 142 with that byte)

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
// WWE '13's motions as the motions mod numbers them (132NN -> 131(NN + 50)), and what WWE '13 does with them (read
// in its code, 2026-11-15; its mini-game 'IQMG' and the states IQuitMiniGame*):
// - the attacker calls the referee: 13210 (86 frames);
// - the referee asks a downed man with 13201: block 0 comes in (112 frames), 1 is the loop (165), 2 goes out after
//   a "NO" or when the ask stops (211), 10 after "I QUIT" (291). (His microphone is a prop WWE '12 hasn't: none.
//   Blocks 100-110 are a second layer WWE '13 blends in by the hand's distance: left out.)
// - the asked man, once the referee is in his loop: 13208 lying face up, 13209 face down, held until the answer;
// - "NO": he plays 13205 (face up, 406 frames) / 13206 (face down, 377), block 0; the attacker 13207 (429) if he
//   stands idle (motions 20000-20598); 290 frames later the referee goes out;
// - "I QUIT": he plays block 10 of 13205 / 13206; 265 frames later the referee goes out with block 10 and the match
//   ends;
// - the threat: the attacker plays 13229 + the taunt's direction (13230-13233: 211, 231, 231, 281 frames).
constexpr u32 W13(u32 m) { return m - 50; }
constexpr double kCallTicks13 = 112, kNoTicks13 = 290, kQuitTicks13 = 265;

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
std::atomic<bool> g_asked_human{false}, g_asker_human{false};  // (for the overlay's prompts: a pad plays him)
// where the attacker (0) and the asked man (1) stand on screen, 0-1 across and down; < 0: not known
std::atomic<float> g_spot[2][2] = {{-1.f, -1.f}, {-1.f, -1.f}};
std::atomic<float> g_threat_left{0.f};  // the running threat: what is left of it, 0-1
int g_attacker = -1, g_victim = -1;
int g_hint_place = -1;  // the human standing over his downed opponent (the "B ASK" prompt is his)
// A submission hold in an I Quit match (WWE '13's second mode): the held man plays the gauge in the hold; "NO"
// breaks the hold, ten seconds without it and he quits. g_hold: our gauge runs for a hold; g_hold_answer: its answer
// (1 "NO", 4 "I QUIT") for the hold's own decision function to take.
bool g_hold = false;
// The tick's last run (ms of the steady clock). The match's pause stops the tick: then the pads are the menu's again
// and the overlay shows nothing (user, 2026-11-15: stuck in the pause menu during the gauge - A and B were ours).
std::atomic<long long> g_tick_ms{0};
long long NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
bool Paused() { return NowMs() - g_tick_ms.load() > 250; }
std::atomic<int> g_hold_answer{0};
// place a stands next to his downed opponent (the tick measures it; the pad hook reads it): only then can he ask
// (user, 2026-11-15: "B ASK" showed from anywhere in the ring; WWE '13 asks from grappling distance, 200 cm)
std::atomic<bool> g_near[2];
constexpr float kAskDistance = 170.f;
bool g_face_down = false, g_cpu_victim = false, g_we_asked = false;
double g_ticks = 0, g_phase_ticks = 0, g_lockout = 0, g_threat = -1, g_cpu_press_at = -1;
float g_speed = 6.5f, g_dir = 1.f;
bool g_was_in_zone = false;
Clock::time_point g_last, g_sub_start, g_down_since[6], g_cpu_next_ask;
std::mt19937 g_rng{std::random_device{}()};


bool Motions13() { return REXCVAR_GET(wwe12_iquit_wwe13_motions); }

// the referee's asking motion (a WWE '12 one; dev: the asks take turns over wwe12_iquit_ref_anim_cycle motions)
int g_asks = 0;
u16 g_ref_seen = 0xFFFF;
u32 RefAsk() {
  const int o = REXCVAR_GET(wwe12_iquit_ref_anim), cycle = REXCVAR_GET(wwe12_iquit_ref_anim_cycle);
  {
    static const std::vector<u32> list = [] {
      std::vector<u32> out;
      const std::string text = REXCVAR_GET(wwe12_iquit_ref_anim_list);
      for (size_t at = 0; at < text.size();) {
        const size_t end = std::min(text.find(',', at), text.size());
        const int v = atoi(text.substr(at, end - at).c_str());
        if (v > 0 && v < 65535)
          out.push_back(u32(v));
        at = end + 1;
      }
      return out;
    }();
    if (!list.empty())
      return list[size_t(std::max(g_asks - 1, 0)) % list.size()];
  }
  if (o <= 0 || o >= 65535)
    return 203;
  return u32(o) + (cycle > 1 && g_asks > 0 ? u32((g_asks - 1) % cycle) : 0);
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
// 20 / 21 getting up, 440-480 the ground pain and the slow getting up after a big move).
bool Down(u32 actor) {
  const u16 a = Anim(actor);
  // (and WWE '13's asked / answering motions: he is still on the ground in them)
  return a <= 30 || (a >= 440 && a <= 480) || IsOneOf(a, {W13(13205), W13(13206), W13(13208), W13(13209)});
}
bool FaceDown(u32 actor) { return IsOneOf(Anim(actor), {1, W13(13209), W13(13206)}); }

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

// (x, y: the motion's block - a record's blocks are keyed {number, x, y})
void PlayAnim(const PPCContext &in, u8 *base, u32 actor, u32 anim, u32 x = 0, u32 y = 0) {
  if (!actor)
    return;
  PPCContext c = in;
  c.r1.u64 = (in.r1.u32 - 512) & ~15u;
  c.r3.u64 = actor;
  c.r4.u64 = anim;
  c.r5.u64 = x;
  c.r6.u64 = y;
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
  BD_INFO("[iquit] motion {} bloc {} sur {:08X} : +92 {} *+684 {} *+688 {} -> anim {}", anim, x, actor,
          bd::mem::try_load<u32>(actor + 92), p684 ? bd::mem::try_load<u8>(p684) : 255,
          p688 ? bd::mem::try_load<u8>(p688) : 255, Anim(actor));
}

// ---- where a superstar stands on screen (for the overlay: WWE '13 hangs its I Quit HUD on the superstars) ----
// Read in the generated code, 2026-11-15: sub_82520B90(float[4] v) turns a world point (x, y, z; w set to 1) into
// back-buffer pixels (x right, y down; v[3] = clip w, > 0 in front of the camera) with the calling thread's
// view-projection, the size at [0x83248D08] + 300 / + 304. The game's own finisher icon at the feet (sub_8251DB38)
// takes x and z half-way between the bones at actor + 196 and + 180 and the lowest y (the world's y points down) of
// the bones at + 176, + 192, + 132, + 148, + 164, + 144; a bone = a pointer to a 4 x 4 matrix, its place at + 48.
// WWE '13 hangs the I Quit gauge on the body's bone (actor + 120 here), 100 pixels higher on screen.
float LoadF(u32 at) {
  const u32 bits = bd::mem::try_load<u32>(at);
  float f;
  std::memcpy(&f, &bits, 4);
  return f;
}
bool Bone(u32 actor, u32 offset, float out[3]) {
  const u32 m = actor ? bd::mem::try_load<u32>(actor + offset) : 0;
  if (!m)
    return false;
  for (u32 i = 0; i < 3; ++i)
    out[i] = LoadF(m + 48 + i * 4);
  return std::isfinite(out[0]) && std::isfinite(out[1]) && std::isfinite(out[2]);
}
bool OnScreen(const PPCContext &in, u8 *base, const float world[3], float &sx, float &sy) {
  const u32 g = bd::mem::try_load<u32>(0x83248D08);
  const u32 w = g ? bd::mem::try_load<u32>(g + 300) : 0, h = g ? bd::mem::try_load<u32>(g + 304) : 0;
  if (!w || !h || w > 16384 || h > 16384)
    return false;
  PPCContext c = in;
  c.r1.u64 = (in.r1.u32 - 512) & ~15u;
  const u32 v = c.r1.u32 + 256;
  for (u32 i = 0; i < 3; ++i) {
    u32 bits;
    std::memcpy(&bits, &world[i], 4);
    bd::mem::store<u32>(v + i * 4, bits);
  }
  bd::mem::store<u32>(v + 12, 0x3F800000u);
  c.r3.u64 = v;
  __imp__sub_82520B90(c, base);
  const float x = LoadF(v), y = LoadF(v + 4), cw = LoadF(v + 12);
  if (!(cw > 0.f) || !std::isfinite(x) || !std::isfinite(y))
    return false;
  sx = x / float(w), sy = y / float(h);
  return sx > -0.5f && sx < 1.5f && sy > -0.5f && sy < 1.5f;
}
// the attacker's feet and the asked man's body on screen, for the overlay (-1: not known, it keeps its fixed places)
void Spots(const PPCContext &in, u8 *base, int attacker, int victim) {
  float sx = -1.f, sy = -1.f, a[3], b[3], world[3];
  const u32 att = attacker >= 0 ? Actor(attacker) : 0, vic = victim >= 0 ? Actor(victim) : 0;
  bool ok = Bone(att, 196, a) && Bone(att, 180, b);
  if (ok) {
    world[0] = (a[0] + b[0]) * 0.5f, world[2] = (a[2] + b[2]) * 0.5f, world[1] = -1.0e9f;
    for (u32 o : {176u, 192u, 132u, 148u, 164u, 144u})
      if (Bone(att, o, a))
        world[1] = std::max(world[1], a[1]);
    ok = world[1] > -1.0e8f && OnScreen(in, base, world, sx, sy);
  }
  g_spot[0][0] = ok ? sx : -1.f, g_spot[0][1] = ok ? sy : -1.f;
  ok = Bone(vic, 120, world) && OnScreen(in, base, world, sx, sy);
  g_spot[1][0] = ok ? sx : -1.f, g_spot[1][1] = ok ? sy : -1.f;
  static Clock::time_point s_next;
  if (Clock::now() >= s_next) {  // (one line a second: where they are, and how far the referee is from the asked man)
    s_next = Clock::now() + std::chrono::seconds(1);
    float r[3] = {}, m[3] = {};
    const bool both = Bone(Actor(kReferee), 120, r) && Bone(vic, 120, m);
    BD_INFO("[iquit] a l'ecran : attaquant {:.3f} {:.3f}, interroge {:.3f} {:.3f} ; arbitre a {:.0f} de lui", g_spot[0][0].load(),
            g_spot[0][1].load(), g_spot[1][0].load(), g_spot[1][1].load(),
            both ? std::sqrt((r[0] - m[0]) * (r[0] - m[0]) + (r[2] - m[2]) * (r[2] - m[2])) : -1.f);
  }
}

// ---- WWE '13's motions: the referee's blocks, and a superstar's motion of ours with what he goes back to ----
int g_ref_x = -1;    // the referee's block of 13201 (-1: not in it)
// The referee walks to the asked man before he asks (WWE '13: he comes and kneels next to him; WWE '12's own referee
// stops two metres from a hold and never comes closer - measured in the user's match of 2026-11-15).
// Read in the code the same day: a computer-driven actor walks through a virtual pad. Each frame the pad update
// (sub_8225BD20) asks the actor's AI for a pad word and delivers it with sub_8225C4D8(pad object, word); the word's
// second byte is the stick, 0x80 | direction << 4 (eight directions, decoded by sub_82259848), its low half the
// buttons. The AI's own "walk toward" (sub_82994230 -> sub_8298BCC0) makes it so: the world angle from the actor to
// the point (sub_827D6BC8: atan2(-dz, dx) minus the constant at 0x82000ADC), in eighths of a turn, minus the camera's
// angle (u16 at AI info + 162, 4096 a turn; info = [[[actor + 3384] + 76]]), looked up in the table at 0x8307B34C and
// renumbered by sub_8298FB98. Here: while he must walk, the referee's word is replaced by that stick toward the spot.
std::atomic<bool> g_ref_walk{false};                  // he is walking to the asked man
std::atomic<float> g_ref_to_x{0.f}, g_ref_to_z{0.f};  // ... to this spot
std::atomic<int> g_ref_pad_seen{0};                   // (his pad words delivered while he walks: 0 = no such pad)
double g_ref_walk_t = 0, g_ref_walk_best = 0, g_ref_walk_best_t = 0;
double g_ref_t = 0;  // ticks in it
// A motion is asked from the game's queue first and played only once the queue has read it. (Played the frame it
// was asked, the game looked it up in a list whose record was half made: a read at 0x00000F66 in sub_821D7150 when
// the asked man said "NO" with 13205 - user's test of 2026-11-15. The game's own code asks ahead too.)
// The queue is [0x831137E8] + 56; its lists at + 96 and + 112: 16-byte entries {u16 number, ..., + 8 state}, 4 = read.
bool Ready(const PPCContext &in, u8 *base, u32 motion) {
  const u32 holder = bd::mem::try_load<u32>(0x831137E8);
  if (!holder)
    return false;
  const u32 q = holder + 56;
  PPCContext c = in;
  c.r1.u64 = (in.r1.u32 - 512) & ~15u;
  c.r3.u64 = q;
  c.r4.u64 = motion;
  c.r5.u64 = 255;
  __imp__sub_821D7480(c, base);  // (asks for it, or keeps it wanted)
  for (u32 list : {96u, 112u}) {
    const u32 entries = bd::mem::try_load<u32>(q + list), count = bd::mem::try_load<u32>(q + list + 8);
    for (u32 i = 0; entries && i < count && i < 64; ++i)
      if (bd::mem::try_load<u16>(entries + i * 16) == u16(motion))
        return bd::mem::try_load<u32>(entries + i * 16 + 8) == 4;
  }
  return false;
}
// The referee's microphone (WWE '13: he holds one while he asks). Read in both games' code and files, 2026-11-15:
// WWE '13's is its object type 143, made and parked when an I Quit match starts, then given by "play a motion on the
// actor and on the object he holds": the object plays the SAME motion, block (x, 1) - the prop's own track, which
// WWE '13's referee motions carry (ours too: 13150-13152 have (0,1) (1,1) (2,1) (10,1)). WWE '12 has the very same
// machinery and a microphone of its own, object type 142 (pac/bg/bgEtc.pac, WPON packs: models "2038_0x", texture
// "wwe_mic"; WWE '13's 143 is the same model with two parts less), used by its story scenes. The game loads and
// parks one when the match record's byte + 12097 is set (two readers only: the model list sub_821F5B70 and the
// match-start spawner sub_822EE080) - set for an I Quit match in Wwe12IQuitRules. So no WWE '13 model is needed.
// Objects: 72 pointers at 0x83113630; object + 80 its type, + 1760 its slot, + 3236 parked, + 96 its holder; an
// actor's + 2536 = the slot he holds (-1: nothing).
constexpr u32 kObjects = 0x83113630, kMicType = 142;
u32 Mic() {
  for (u32 i = 0; i < 72; ++i)
    if (const u32 o = bd::mem::try_load<u32>(kObjects + i * 4))
      if (bd::mem::try_load<u32>(o + 80) == kMicType)
        return o;
  return 0;
}
// The referee's block of his question, with the microphone in his hand when the match has one.
void RefPlay(const PPCContext &in, u8 *base, u32 ref, u32 anim, u32 x) {
  u32 mic = REXCVAR_GET(wwe12_iquit_mic) ? Mic() : 0;
  PPCContext c = in;
  c.r1.u64 = (in.r1.u32 - 1024) & ~15u;
  if (mic && bd::mem::try_load<u32>(mic + 3236) != 0) {  // parked: made live again (in its own slot)
    c.r3.s64 = -1;
    c.r4.u64 = kMicType;
    c.r5.u64 = 0;
    c.r6.u64 = 0;
    __imp__sub_82254518(c, base);
    mic = c.r3.u32;
    BD_INFO("[iquit] micro : sorti ({:08X})", mic);
  }
  const u32 slot = mic ? bd::mem::try_load<u32>(mic + 1760) : 0xFFFFFFFFu;
  if (!mic || slot >= 72 || bd::mem::try_load<u32>(kObjects + slot * 4) != mic) {
    PlayAnim(in, base, ref, anim, x, 0);
    return;
  }
  bd::mem::store<u32>(ref + 2536, slot);
  c = in;
  c.r1.u64 = (in.r1.u32 - 1024) & ~15u;
  c.r3.u64 = ref;
  c.r4.u64 = anim;
  c.r5.u64 = x;
  __imp__sub_8221FAD0(c, base);
}
// His question is over: the microphone parked again.
void MicAway(const PPCContext &in, u8 *base) {
  const u32 ref = Actor(kReferee), mic = Mic();
  if (!ref || !mic || bd::mem::try_load<u32>(mic + 3236) != 0)
    return;
  if (bd::mem::try_load<u32>(ref + 2536) == bd::mem::try_load<u32>(mic + 1760))
    bd::mem::store<u32>(ref + 2536, 0xFFFFFFFFu);
  bd::mem::store<u32>(mic + 96, 0);
  PPCContext c = in;
  c.r1.u64 = (in.r1.u32 - 1024) & ~15u;
  c.r3.u64 = mic;
  __imp__sub_82250258(c, base);
  BD_INFO("[iquit] micro : range");
}
struct OurMotion {
  u32 ours = 0;     // the motion we gave him (0: none)
  u32 to = 0;       // ... and the one he takes back when it is over (0: left to the game)
  double left = 0;  // ticks
} g_ours[2];
// what waits for the queue: the two superstars' (0, 1) and the referee's (2)
struct Waiting {
  u32 anim = 0, x = 0, to = 0;
  double ticks = 0, waited = 0;
} g_wait[3];
void RefBlock(const PPCContext &in, u8 *base, int x) {
  (void)in, (void)base;
  g_wait[2] = {W13(13201), u32(x), 0, 0, 0};
  g_ref_x = x;
  g_ref_t = 0;
}
void PlayFor(const PPCContext &in, u8 *base, int place, u32 anim, u32 x, double ticks, u32 to) {
  (void)in, (void)base;
  if (place < 0 || place > 1)
    return;
  g_wait[place] = {anim, x, to, ticks, 0};
  g_ours[place] = {};
}
bool Idle(u32 actor) {
  const u16 a = Anim(actor);
  return a >= 20000 && a <= 20598;
}
void MotionsTick(const PPCContext &ctx, u8 *base, double dt) {
  for (int i = 0; i < 3; ++i) {
    Waiting &w = g_wait[i];
    if (!w.anim)
      continue;
    const u32 actor = Actor(i == 2 ? kReferee : i);
    if (actor && Ready(ctx, base, w.anim)) {
      if (i == 2)
        RefPlay(ctx, base, actor, w.anim, w.x);
      else
        PlayAnim(ctx, base, actor, w.anim, w.x, 0);
      if (i == 2)
        g_ref_t = 0;
      else
        g_ours[i] = {w.anim, w.to, w.ticks};
      w = {};
    } else if (!actor || (w.waited += dt) > 240) {
      BD_INFO("[iquit] motion {} pas prete apres 4 s : laissee", w.anim);
      if (i == 2)
        g_ref_x = -1;
      w = {};
    }
  }
  if (g_ref_x >= 0 && !g_wait[2].anim) {
    const u32 ref = Actor(kReferee);
    g_ref_t += dt;
    static constexpr double kLen[4] = {112, 165, 211, 291};
    const double len = kLen[g_ref_x == 0 ? 0 : g_ref_x == 1 ? 1 : g_ref_x == 2 ? 2 : 3];
    if (!ref || Anim(ref) != u16(W13(13201))) {
      g_ref_x = -1;  // (the game took him back)
      MicAway(ctx, base);
    } else if (g_ref_t >= len) {
      if (g_ref_x <= 1) {
        RefBlock(ctx, base, 1);  // in, then the loop again and again
      } else {
        MicAway(ctx, base);
        PlayAnim(ctx, base, ref, kRefIdle);  // gone out: he stands
        g_ref_x = -1;
      }
    }
  }
  for (int p = 0; p < 2; ++p) {
    OurMotion &m = g_ours[p];
    if (!m.ours)
      continue;
    const u32 a = Actor(p);
    m.left -= dt;
    if (!a || Anim(a) != u16(m.ours)) {
      m = {};  // (the game gave him another motion)
    } else if (m.left <= 0) {
      if (m.to || m.ours == W13(13205) || m.ours == W13(13206))
        PlayAnim(ctx, base, a, m.to);  // (0 / 1: lying on the mat again)
      m = {};
    }
  }
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

// the computer asked: a reaction delay (ticks) once the cursor enters the zone. WWE '13 takes it from its generic AI
// reaction table (not found); here it grows as he gets worn out, so a hurt computer misses and can quit (user's test
// of 2026-11-14: with 2-12 ticks the computer resisted every time, at 67 % health too).
float g_cpu_health = 1.f;
double CpuDelay() {
  const double tired = 1.0 - std::clamp<double>(g_cpu_health, 0.0, 1.0);
  return std::uniform_int_distribution<int>(2, 8)(g_rng) +
         std::uniform_real_distribution<double>(0.0, 40.0 * tired)(g_rng);
}

void StartAsk(const PPCContext &in, u8 *base, int attacker, int victim) {
  g_attacker = attacker;
  g_victim = victim;
  const u32 att = Actor(attacker), vic = Actor(victim), ref = Actor(kReferee);
  g_face_down = FaceDown(vic);
  g_cpu_victim = PadOf(victim) < 0;
  g_asked_human = !g_cpu_victim;
  g_asker_human = PadOf(attacker) >= 0;
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
  ++g_asks;
  if (Motions13()) {
    // WWE '13: the attacker calls the referee, the referee comes in with his question
    PlayFor(in, base, attacker, W13(13210), 0, 86, Idle(att) ? Anim(att) : 0);
    g_ref_walk_t = 0, g_ref_walk_best = 1.0e9, g_ref_walk_best_t = 0;
    g_ref_pad_seen = 0;
    g_ref_walk = true;  // (the tick walks him there, then starts his question)
    {
      std::string types;
      for (u32 i = 0; i < 72; ++i)
        if (const u32 o = bd::mem::try_load<u32>(kObjects + i * 4))
          types += fmt::format(" {}{}", bd::mem::try_load<u32>(o + 80), bd::mem::try_load<u32>(o + 3236) ? "p" : "");
      BD_INFO("[iquit] micro : drapeau {} objet {:08X} ; objets du match (p = range) :{}", bd::mem::try_load<u8>(0x8317C850 + 12097),
              Mic(), types);
    }
  } else {
    PlayAnim(in, base, ref, RefAsk());  // the referee asks
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
  g_cpu_health = health;
  g_speed = kSpeed[d];
  g_width = std::clamp(health * 100.f * 0.01f * kWidthK[d], 6.f, 93.f);
  g_cursor = 0.f;
  g_dir = 1.f;
  RollZone();
  g_ticks = 0;
  g_phase = kGauge;
  ShowGauge();
  (void)ref;
  if (Motions13() && !g_hold)
    PlayFor(in, base, g_victim, W13(g_face_down ? 13209 : 13208), 0, 1.0e9, 0);  // the asked man, held until he answers
  BD_INFO("[iquit] la jauge : vie {:.0f} %, zone {:.0f} de large en {:.0f}, vitesse {}", health * 100, g_width.load(),
          g_centre.load(), g_speed);
}

void Answer(const PPCContext &in, u8 *base, int result) {
  g_result = result;
  g_phase = kAnswer;
  g_phase_ticks = 0;
  g_shake = 0.f;
  const u32 vic = Actor(g_victim), ref = Actor(kReferee);
  (void)ref;
  if (g_hold) {
    g_hold_answer = result;
    BD_INFO("[iquit] prise : le catcheur {} {}", g_victim, result == 1 ? "resiste (NO), la prise casse" : "abandonne (I QUIT)");
    if (result == 4)
      GiveUp(in, base);
    return;
  }
  if (result == 1) {
    BD_INFO("[iquit] le catcheur {} resiste (NO)", g_victim);
    if (Motions13()) {
      // "NO": his answer, then lying again; the attacker's reaction if he stands idle
      PlayFor(in, base, g_victim, W13(g_face_down ? 13206 : 13205), 0, g_face_down ? 377 : 406, g_face_down ? 1 : 0);
      const u32 att = Actor(g_attacker);
      if (Idle(att))
        PlayFor(in, base, g_attacker, W13(13207), 0, 429, Anim(att));
    }
  } else if (Motions13()) {
    PlayFor(in, base, g_victim, W13(g_face_down ? 13206 : 13205), 10, 1.0e9, 0);  // "I QUIT": the match ends after it
  } else {
    GiveUp(in, base);
  }
}

void EndAsk(const PPCContext &in, u8 *base) {
  g_phase = kIdle;
  g_ref_walk = false;
  const u32 ref = Actor(kReferee);
  if (g_ref_x == 0 || g_ref_x == 1) {  // WWE '13: the referee goes out (MotionsTick stands him up after it)
    RefBlock(in, base, g_result.load() == 4 ? 10 : 2);
    return;
  }
  if (ref && IsOneOf(Anim(ref), {203, RefAsk()}))
    PlayAnim(in, base, ref, kRefIdle);  // (the referee stands up again)
}

}  // namespace

// From the XInputGetState hook (through wwe12_match_modes.cpp): every pad's buttons. Returns the buttons the game
// must not see: in an I Quit match, B next to a downed opponent is ours (the game would start a submission: user's
// test of 2026-11-14); during the ask, A / B, and the attacker's D-pad (his threat).
static u16 IQuitPad(u32 user, u16 buttons) {
  if (user >= 4)
    return 0;
  const u16 before = g_pad[user].exchange(buttons);
  const u16 newly = u16(buttons & ~before);
  if (newly)
    g_pressed[user].fetch_or(newly);
  if (!REXCVAR_GET(wwe12_iquit_ask) || !Wwe12IQuitMatch() || Paused())
    return 0;
  if (g_phase.load() != kIdle) {
    const bool attacker = g_attacker >= 0 && PadOf(g_attacker) == int(user);
    if (attacker && g_phase.load() == kGauge && (newly & kPadDpad))
      g_taunt = newly & kPadDpad;
    // (the attacker's D-pad: the game's own taunt, our threat too; with WWE '13's motions the threat has its own)
    // Once he has said "NO" the D-pad is the game's again: hidden through the 290 ticks of his answer, it took the
    // wake-up taunt away for five seconds after every ask (user, 2026-11-15: "no wake-up taunt in an I Quit match").
    const bool said_no = g_phase.load() == kAnswer && g_result.load() == 1;
    return u16(kPadA | kPadB | (attacker && Motions13() && !said_no ? kPadDpad : 0));
  }
  for (int a = 0; a < 2; ++a)
    if (PadOf(a) == int(user) && g_near[a].load() && !Down(Actor(a)) && Down(Actor(1 - a))) {
      if (newly & kPadB)
        g_ask = a + 1;  // (asked now, next to him: the tick starts the mini-game)
      return kPadB;
    }
  return 0;
}

u16 Wwe12SrefPad(u32 user, u16 buttons);  // wwe12_sref.cpp: the Special Guest Referee's count button
u16 Wwe12IQuitPad(u32 user, u16 buttons) { return u16(IQuitPad(user, buttons) | Wwe12SrefPad(user, buttons)); }

// From the hook of sub_8267CF38 (wwe12_kind_probe.cpp, DEV builds only) and from the tick.
void Wwe12IQuitRules() {
  // The game's own microphone, loaded and parked for this match (RefPlay). Asked for as soon as I QUIT is the match
  // chosen in the menu: the model list is made while the match loads, when the record is not the match's yet (user's
  // test of 2026-11-15, 19:02: the rules below were only set at the bell, and no microphone had been made).
  if (REXCVAR_GET(wwe12_iquit_ask) && REXCVAR_GET(wwe12_iquit_mic) && REXCVAR_GET(wwe12_iquit_wwe13_motions) && IQuitMatch() &&
      bd::mem::load<u8>(0x8317C850 + 12097) == 0) {
    bd::mem::store<u8>(0x8317C850 + 12097, 1);
    static std::atomic<int> s_lines{0};
    if (s_lines.fetch_add(1) < 40)
      BD_INFO("[iquit] micro : demande au jeu (fiche du match type {})", bd::mem::load<u8>(0x8317C850));
  }
  if (!REXCVAR_GET(wwe12_iquit_ask) || !Wwe12IQuitMatch())
    return;
  for (u32 k : {3u, 10u, 27u, 63u})
    if (bd::mem::load<u8>(0x8317C850 + k) != 0) {
      bd::mem::store<u8>(0x8317C850 + k, 0);
      BD_INFO("[iquit] regle du match + {} mise a 0 (comme EXTREME RULES)", k);
    }
}

// Once per game frame during a match (wwe12_match_modes.cpp, from the gauge hook of the first superstar).
void Wwe12SrefTick(const PPCContext &ctx, u8 *base);  // wwe12_sref.cpp
void Wwe12IQuitTick(const PPCContext &ctx, u8 *base) {
  Wwe12SrefTick(ctx, base);
  if (!REXCVAR_GET(wwe12_iquit_ask) || !Wwe12IQuitMatch()) {
    g_phase = kIdle;
    g_sub = false;
    g_hint = false;
    return;
  }
  // An I Quit match has EXTREME RULES' rules (user, 2026-11-15: the referee still took the chair away and counted,
  // as in a match with disqualifications). The match's record (104 bytes at 0x8317C850, read live by the game)
  // of a ONE ON ONE > EXTREME RULES match differs from the submission match's and the normal one's by four bytes
  // that are 1 there and 0 in it (the user's own matches, logged the same day): + 3, + 10, + 27, + 63. Set here.
  // (+ 27, not + 29 as first written: a miscount - the pause menu's MATCH RULES still said "DQ ON" and the referee
  // took a table away, user's test of 2026-11-15. EXTREME RULES' record, logged that day:
  // 4D01000000000001 0000000000000000 0000000000000000 0001000000000000 0001020000000100 0000010000000200 ...)
  // (2026-11-15, later: set once a frame, the referee still took a table away - so now set before every read of
  // the record, Wwe12IQuitRules below, in case something copies the rules while the match loads.)
  Wwe12IQuitRules();
  u16 pressed[4];
  for (int p = 0; p < 4; ++p)
    pressed[p] = g_pressed[p].exchange(0);
  const auto now = Clock::now();
  // WWE '13's ticks (1/60 s) from the real time (the game runs at 30 or 60)
  const double dt = std::clamp(std::chrono::duration<double>(now - g_last).count() * 60.0, 0.0, 4.0);
  g_last = now;
  g_tick_ms = NowMs();

  MotionsTick(ctx, base, dt);
  const int phase = g_phase.load();
  if (phase != kIdle)
    Spots(ctx, base, g_attacker, g_victim);
  else if (g_hint.load() && g_hint_place >= 0)
    Spots(ctx, base, g_hint_place, 1 - g_hint_place);
  {  // Study for "the referee walks to the downed man" (WWE '13's referee is sent there by his own AI, state 9,
     // when the attacker plays 13210 / 13211; ours asks from where he stands). Read in the code, 2026-11-15: an
     // actor's position is the three floats at [actor + 768] (copied at actor + 752 / 756 / 760), its heading the
     // float at actor + 740, wrapped by the constants at 0x82000A94 / 0x82000A98. Logged twice a second, the first
     // 900 lines: the referee's position, heading and body matrix, and both superstars' positions - to learn the
     // heading's unit and sense and how the game itself walks him to a hold.
    static Clock::time_point s_next;
    static int s_lines = 0;
    const u32 ref = Actor(kReferee);
    if (ref && s_lines < 900 && now >= s_next) {
      s_next = now + std::chrono::milliseconds(500);
      ++s_lines;
      const u32 pos = bd::mem::try_load<u32>(ref + 768), m = bd::mem::try_load<u32>(ref + 120);
      std::string line = fmt::format("pos {:.0f} {:.0f} {:.0f} cap {:.3f} (bornes {:.3f} {:.3f}) motion {}", pos ? LoadF(pos) : 0.f,
                                     pos ? LoadF(pos + 4) : 0.f, pos ? LoadF(pos + 8) : 0.f, LoadF(ref + 740),
                                     LoadF(0x82000A94), LoadF(0x82000A98), Anim(ref));
      if (m)
        line += fmt::format(" | corps X {:.2f} {:.2f} {:.2f} Y {:.2f} {:.2f} {:.2f} Z {:.2f} {:.2f} {:.2f} en {:.0f} {:.0f} {:.0f}",
                            LoadF(m), LoadF(m + 4), LoadF(m + 8), LoadF(m + 16), LoadF(m + 20), LoadF(m + 24),
                            LoadF(m + 32), LoadF(m + 36), LoadF(m + 40), LoadF(m + 48), LoadF(m + 52), LoadF(m + 56));
      for (int p = 0; p < 2; ++p)
        if (const u32 a = Actor(p))
          if (const u32 ap = bd::mem::try_load<u32>(a + 768))
            line += fmt::format(" | catcheur {} pos {:.0f} {:.0f} {:.0f} cap {:.3f} motion {}", p, LoadF(ap), LoadF(ap + 4),
                                LoadF(ap + 8), LoadF(a + 740), Anim(a));
      BD_INFO("[iquit] etude arbitre : {}", line);
    }
  }
  {  // (every change of the referee's motion, the first 1500: what he does with a weapon left in the ring)
    static int s_ref_lines = 0;
    const u16 a = Actor(kReferee) ? Anim(Actor(kReferee)) : 0xFFFF;
    if (a != g_ref_seen && s_ref_lines < 1500) {
      ++s_ref_lines;
      BD_INFO("[iquit] arbitre : motion {} -> {} (phase {})", g_ref_seen, a, phase);
    }
    g_ref_seen = a;
  }
  // The wake-up taunt on a worn-out man. Seen in the user's two tests of 2026-11-15 (motions logged): the taunt
  // (motions 37000-37999, a stored finisher) lifts the downed man 0.65 s after it starts (his motion 0 -> 20, held)
  // when his health is above 0, and never once his head's health is under 0 - which a match that only ends on
  // "I QUIT" reaches every time, a normal match hardly. So, while a superstar with a stored finisher taunts over
  // his downed opponent, that opponent's health under 0 is put back to 1. (Likely the cause, not proved in the
  // game's code.)
  for (int a = 0; a < 2; ++a) {
    const u32 att = Actor(a), vic = Actor(1 - a);
    if (!att || !vic || Anim(att) < 37000 || Anim(att) > 37999 || !Down(vic) || bd::mem::try_load<u32>(att + 2776) == 0)
      continue;
    const u32 dmg = bd::mem::try_load<u32>(vic + 3364);
    for (u32 k = 0; dmg && k < 4; ++k)
      if (int16_t(bd::mem::try_load<u16>(dmg + 56 + k * 2)) < 1) {
        BD_INFO("[iquit] provocation au releve : sante {} du catcheur {} remise a 1 (etait {})", k, 1 - a,
                int16_t(bd::mem::try_load<u16>(dmg + 56 + k * 2)));
        bd::mem::store<u16>(dmg + 56 + k * 2, 1);
      }
  }
  {  // (each change of a superstar's motion, the first 4000: what a taunt or an ask really gave him)
    static u16 s_seen[2] = {0xFFFF, 0xFFFF};
    static int s_lines = 0;
    for (int p = 0; p < 2; ++p) {
      const u16 a = Actor(p) ? Anim(Actor(p)) : 0xFFFF;
      if (a != s_seen[p] && s_lines < 4000) {
        ++s_lines;
        BD_INFO("[iquit] catcheur {} : motion {} -> {} (phase {})", p, s_seen[p], a, phase);
      }
      s_seen[p] = a;
    }
  }
  if (phase != kIdle) {
    g_phase_ticks += dt;
    const u32 vic = Actor(g_victim), att = Actor(g_attacker);
    if (g_hold) {
      // a hold: over with the answer shown, or as soon as the hold itself is (released, the ropes, a hit)
      u32 hold = 0;
      for (int i = 0; i < 2 && !hold; ++i)
        hold = Actor(i) ? bd::mem::try_load<u32>(Actor(i) + 3404) : 0;
      const bool struggle = hold && bd::mem::try_load<u32>(hold + 32) == 2;
      if ((phase == kAnswer && g_phase_ticks >= kAnswerTicks) || (phase != kAnswer && !struggle && g_ticks > 10)) {
        g_phase = kIdle;
        g_hold = false;
        g_hold_answer = 0;
        return;
      }
      if (phase == kAnswer)
        return;
    }
    if (phase == kAnswer) {
      const bool quit = g_result.load() == 4;
      if (g_phase_ticks >= (Motions13() ? (quit ? kQuitTicks13 : kNoTicks13) : kAnswerTicks)) {
        if (quit && Motions13())
          GiveUp(ctx, base);  // (WWE '13: the match ends once he has said it)
        EndAsk(ctx, base);
      }
      return;
    }
    // he got up (a strike that lifts him, a roll away...): the ask stops without an answer (WWE '13 result 3)
    // (65535: no motion read, e.g. a WWE '13 motion the game did not find: user's test of 2026-11-14 stopped there)
    if (!g_hold && (!vic || !att || (!Down(vic) && Anim(vic) != 0xFFFF))) {
      BD_INFO("[iquit] le catcheur {} n'est plus au sol (anim {}) : plus de question", g_victim, Anim(vic));
      EndAsk(ctx, base);
      return;
    }
    if (phase == kCall && g_ref_walk.load()) {
      // The spot: 70 beside the asked man's chest, across his body's line (chest to head), on the referee's side.
      const u32 ref = Actor(kReferee);
      const u32 rp = ref ? bd::mem::try_load<u32>(ref + 768) : 0;
      float chest[3], head[3];
      bool arrived = !rp || !Bone(vic, 120, chest);
      if (!arrived) {
        const float rx = LoadF(rp), rz = LoadF(rp + 8);
        float ax = 1.f, az = 0.f;
        if (Bone(vic, 132, head) && std::hypot(head[0] - chest[0], head[2] - chest[2]) > 1.f) {
          const float l = std::hypot(head[0] - chest[0], head[2] - chest[2]);
          ax = -(head[2] - chest[2]) / l, az = (head[0] - chest[0]) / l;  // (across his line)
        }
        if ((rx - chest[0]) * ax + (rz - chest[2]) * az < 0)
          ax = -ax, az = -az;
        const float tx = chest[0] + ax * 70.f, tz = chest[2] + az * 70.f;
        g_ref_to_x = tx, g_ref_to_z = tz;
        const double d = std::hypot(tx - rx, tz - rz);
        g_ref_walk_t += dt;
        if (d < g_ref_walk_best - 3.0)
          g_ref_walk_best = d, g_ref_walk_best_t = g_ref_walk_t;
        // there, or held back (the ropes: the man is outside), or 5 s gone, or no pad of his to push
        const char *why = d < 30.0                                             ? "arrive"
                          : g_ref_walk_t - g_ref_walk_best_t > 18              ? "retenu"
                          : g_ref_walk_t > 300                                 ? "trop long"
                          : (g_ref_walk_t > 20 && g_ref_pad_seen.load() == 0) ? "pas de manette"
                                                                               : nullptr;
        if (why) {
          arrived = true;
          BD_INFO("[iquit] l'arbitre : {} a {:.0f} de sa place apres {:.0f} ticks ({} mots de manette)", why, d,
                  g_ref_walk_t, g_ref_pad_seen.load());
          // he faces the man (an actor faces (-sin h, -cos h): measured the same day)
          bd::mem::store<float>(ref + 740, std::atan2(-(chest[0] - rx), -(chest[2] - rz)));
        }
      }
      if (arrived) {
        g_ref_walk = false;
        RefBlock(ctx, base, 0);
        g_phase_ticks = 0;  // (his question's way in, then the gauge)
      }
      return;
    }
    if (phase == kCall) {
      if (g_phase_ticks >= (Motions13() ? kCallTicks13 : kCallTicks))
        StartGauge(ctx, base);
      return;
    }
    // the gauge
    g_ticks += dt;
    if (Motions13() && !g_hold)
      Ready(ctx, base, W13(g_face_down ? 13206 : 13205));  // (his answer's motion, asked ahead)
    g_white = float(std::min(9.0, std::floor(g_ticks / 60.0)) / 9.0);
    // the threat: the attacker's taunt (a human's D-pad; the computer once, at random) while the referee asks
    int taunt = g_taunt.exchange(0);
    if (PadOf(g_attacker) < 0 && g_threat < 0 && g_ticks > 90 && g_ticks < 120 &&
        std::uniform_int_distribution<int>(0, 99)(g_rng) < 2)
      taunt = kDpadUp;
    if (taunt && g_threat < 0) {
      g_threat = 0;
      const int n = (taunt & kDpadUp) ? 1 : (taunt & kDpadRight) ? 2 : (taunt & kDpadDown) ? 3 : 4;
      if (Motions13() && !g_hold) {  // (in a hold he keeps the hold: the gauge shakes, no motion)
        static constexpr double kLen[4] = {211, 231, 231, 281};
        PlayFor(ctx, base, g_attacker, W13(13229 + u32(n)), 0, kLen[n - 1], Idle(att) ? Anim(att) : 0);
      }
      BD_INFO("[iquit] le catcheur {} menace son adversaire", g_attacker);
    }
    if (g_threat >= 0) {
      g_threat += dt;
      const double f = 1.0 - g_threat / kThreatTicks;
      g_shake = g_threat < kThreatTicks ? float(70.0 * f * f * std::sin(g_threat * 1.3)) : 0.f;
      g_threat_left = float(std::clamp(f, 0.0, 1.0));
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
    g_near[a] = false;
    if (!att || !vic || Down(att))
      continue;
    if (!Down(vic)) {
      g_down_since[v] = now;
      continue;
    }
    // next to him: their bodies' bones less than kAskDistance apart on the mat
    float pa[3], pv[3];
    const bool beside = Bone(att, 120, pa) && Bone(vic, 120, pv) &&
                      std::hypot(pa[0] - pv[0], pa[2] - pv[2]) < kAskDistance;
    g_near[a] = beside;
    if (!beside)
      continue;
    const int pad = PadOf(a);
    if (pad >= 0) {
      hint = true;
      g_hint_place = a;
    }
    const bool human_asks = pad >= 0 && g_ask.load() == a + 1;
    // the computer asks an opponent it stands over once he has been down a second, if he is hurt at all (under 85 %
    // health), at most every 10 s (user, 2026-11-15: it never asked - a human is up again before the 3 s it waited)
    const bool cpu_asks = pad < 0 && now - g_down_since[v] > std::chrono::seconds(1) && now > g_cpu_next_ask &&
                          HealthRatio(vic) < 0.85f;
    if (human_asks || cpu_asks) {
      g_ask = 0;
      if (cpu_asks)
        g_cpu_next_ask = now + std::chrono::seconds(10);
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

// ---- a submission hold in an I Quit match: the gauge in the hold ----
// Read in the generated code, 2026-11-15: a hold is one object ('BPMG', at [attacker + 3404]): + 36 the attacker,
// + 40 the held man, + 32 its state (2 = the struggle), + 92 why it ends (0 it goes on, 7 he breaks free, 4 he
// taps, 6 released / interrupted), + 112 the frames the attacker did nothing (5 s: released). Each frame of the
// struggle sub_823260C8 zeroes + 92, calls sub_82324418 to decide and acts on + 92 at once. Here, in an I Quit
// match: our gauge starts with the struggle; the game's own "he breaks free" and "he taps" are taken out, ours put
// in ("NO" = 7; "I QUIT" = 4, the match ended by GiveUp); the idle counter kept at 0 so the hold lasts the ten
// seconds. The game's tap-out (sub_82229320, sub_82323FD0) answers nothing while a hold runs, and the struggle's
// own meter and prompts (sub_82527010) are not drawn.
namespace {
u32 HoldObject() {
  for (int i = 0; i < 2; ++i)
    if (const u32 a = Actor(i))
      if (const u32 o = bd::mem::try_load<u32>(a + 3404))
        return o;
  return 0;
}
bool HoldRules() { return REXCVAR_GET(wwe12_iquit_ask) && Wwe12IQuitMatch(); }
}  // namespace

extern "C" REX_FUNC(sub_82324418) {
  const u32 obj = ctx.r3.u32;
  __imp__sub_82324418(ctx, base);
  if (!HoldRules() || !obj)
    return;
  const u32 att = bd::mem::try_load<u32>(obj + 36), def = bd::mem::try_load<u32>(obj + 40);
  const int pa = att && att == Actor(0) ? 0 : att && att == Actor(1) ? 1 : -1;
  const int pv = def && def == Actor(0) ? 0 : def && def == Actor(1) ? 1 : -1;
  if (pa < 0 || pv < 0 || pa == pv)
    return;
  bd::mem::store<u32>(obj + 112, 0);  // (the attacker is not "doing nothing": the gauge decides)
  if (!g_hold && g_phase.load() == kIdle) {
    g_hold = true;
    g_hold_answer = 0;
    g_attacker = pa;
    g_victim = pv;
    g_face_down = false;
    g_cpu_victim = PadOf(pv) < 0;
    g_asked_human = !g_cpu_victim;
    g_asker_human = PadOf(pa) >= 0;
    g_phase_ticks = 0;
    g_result = 0;
    g_white = 0.f;
    g_shake = 0.f;
    g_threat = -1;
    g_lockout = 0;
    g_cpu_press_at = -1;
    g_was_in_zone = false;
    g_hint = false;
    BD_INFO("[iquit] prise de soumission : le catcheur {} tient le catcheur {} (motion {})", pa, pv,
            bd::mem::try_load<u32>(obj + 44));
    StartGauge(ctx, base);
  }
  if (!g_hold)
    return;
  u32 reason = bd::mem::try_load<u32>(obj + 92);
  const int answer = g_hold_answer.exchange(0);
  if (answer == 1)
    reason = 7;
  else if (answer == 4)
    reason = 4;
  else if (reason == 4 || reason == 7)
    reason = 0;  // (the game's own tap or escape: not in an I Quit match)
  bd::mem::store<u32>(obj + 92, reason);
}

extern "C" REX_FUNC(sub_82229320) {
  if (HoldRules() && HoldObject()) {
    static std::atomic<int> s_logged{0};
    if (s_logged.fetch_add(1) < 8)
      BD_INFO("[iquit] abandon du jeu ignore pendant une prise (la jauge decide)");
    return;
  }
  __imp__sub_82229320(ctx, base);
}

extern "C" REX_FUNC(sub_82323FD0) {
  __imp__sub_82323FD0(ctx, base);
  if (HoldRules())
    ctx.r3.u64 = 0;
}

extern "C" REX_FUNC(sub_82527010) {
  if (HoldRules() && HoldObject())
    return;
  __imp__sub_82527010(ctx, base);
}

// ---- the microphone asked from the game, at the two places that read the request ----
// (Set from the hook of sub_8267CF38 only, the request was not there when these two ran - user's test of 2026-11-15,
// 20:59: "demande au jeu" logged at the bell, no object 142 in the match. So it is set right before each.)
namespace {
void AskMic(const char *where) {
  if (!REXCVAR_GET(wwe12_iquit_ask) || !REXCVAR_GET(wwe12_iquit_mic) || !REXCVAR_GET(wwe12_iquit_wwe13_motions) || !IQuitMatch())
    return;
  const u8 before = bd::mem::try_load<u8>(0x8317C850 + 12097);
  bd::mem::store<u8>(0x8317C850 + 12097, 1);
  BD_INFO("[iquit] micro : demande ({}), fiche du match type {}, drapeau avant {}", where, bd::mem::try_load<u8>(0x8317C850),
          before);
}
}  // namespace
extern "C" REX_FUNC(sub_821F5B70) {
  AskMic("liste des modeles");
  __imp__sub_821F5B70(ctx, base);
}
extern "C" REX_FUNC(sub_822EE080) {
  AskMic("objets du match");
  __imp__sub_822EE080(ctx, base);
  if (IQuitMatch())
    BD_INFO("[iquit] micro : apres la creation des objets, objet {:08X}", Mic());
}

// ---- the referee's microphone stays in his hand between the blocks of his question ----
// When an object's motion is over the game lets it go (holder + 2536 = -1) and plays its "on the mat" motion: the
// question is four blocks played one after the other, so that would drop the microphone at each. While he asks,
// the microphone he holds is left as it is; MicAway parks it at the end.
extern "C" REX_FUNC(sub_82251388) {
  const u32 obj = ctx.r3.u32;
  if (obj && bd::mem::try_load<u32>(obj + 80) == kMicType && Wwe12IQuitMatch() && (g_ref_x >= 0 || g_wait[2].anim)) {
    const u32 ref = Actor(kReferee);
    if (ref && bd::mem::try_load<u32>(obj + 96) == ref)
      return;
  }
  __imp__sub_82251388(ctx, base);
}

// ---- the referee walks to the asked man: his pad's stick pushed toward the spot ----
extern "C" REX_FUNC(sub_8225C4D8) {
  const u32 pad = ctx.r3.u32;
  if (g_ref_walk.load() && pad) {
    const u32 ref = Actor(kReferee);
    if (ref && bd::mem::try_load<u32>(pad + 116) == ref) {
      g_ref_pad_seen.fetch_add(1);
      const u32 rp = bd::mem::try_load<u32>(ref + 768);
      u32 info = 0;
      for (u32 a : {ref, Actor(1), Actor(0)}) {  // (the camera's angle: his own AI's, else a superstar's)
        const u32 ai = a ? bd::mem::try_load<u32>(a + 3384) : 0, w = ai ? bd::mem::try_load<u32>(ai + 76) : 0;
        info = w ? bd::mem::try_load<u32>(w) : 0;
        if (info)
          break;
      }
      if (rp) {
        const float dx = g_ref_to_x.load() - LoadF(rp), dz = g_ref_to_z.load() - LoadF(rp + 8);
        const float a = std::atan2(-dz, dx) - LoadF(0x82000ADC) + 4.f * 6.2831853f;
        const u32 dir8 = (((u32(a * LoadF(0x82002E4C)) & 0xFFF) + 256) >> 9) & 7;
        const u32 cam = info ? bd::mem::try_load<u16>(info + 162) : 0;
        const u32 rel = u32(int32_t((dir8 << 9) - cam + 256) >> 9) & 7;
        const u32 v = bd::mem::try_load<u32>(0x8307B34C + rel * 4);
        static constexpr u32 kCode[8] = {2, 1, 8, 3, 7, 4, 5, 6};  // sub_8298FB98, for v = 2..9
        if (v >= 2 && v <= 9) {
          const u32 k = (kCode[v - 2] - 1) & 7;
          ctx.r4.u64 = (0x80u | (k << 4)) << 16;
        }
        static std::atomic<int> s_lines{0};
        if (s_lines.fetch_add(1) < 40)
          BD_INFO("[iquit] l'arbitre marche : vers {:.0f} {:.0f} (ecart {:.0f} {:.0f}), huitieme {} camera {} -> {} table {} "
                  "mot {:08X}",
                  g_ref_to_x.load(), g_ref_to_z.load(), dx, dz, dir8, cam, rel, v, ctx.r4.u32);
      }
    }
  }
  __imp__sub_8225C4D8(ctx, base);
}

// ---- study: who gives the referee his motions (the callers of "play a motion" for him, in an I Quit match) ----
extern "C" REX_FUNC(sub_8247AD18) {
  if (Wwe12IQuitMatch() && ctx.r3.u32 && ctx.r3.u32 == Actor(kReferee)) {
    static std::atomic<int> s_lines{0};
    if (s_lines.fetch_add(1) < 500)
      BD_INFO("[iquit] etude arbitre : motion {} bloc {} {} demandee par {:08X}", ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, u32(ctx.lr));
  }
  __imp__sub_8247AD18(ctx, base);
}

// ---- no disqualification in an I Quit match ----
// The match's end for a superstar is sub_82261320(victim, reason, winner); reason 7 is the referee's five count, the
// disqualification (the judge sub_82263440 makes it the end "by way of DQ"). An I Quit match only ends when a man
// says it: chairs and the rest are fair (user, 2026-11-15). The referee may still count; his count ends nothing.
extern "C" REX_FUNC(sub_82261320) {
  if (ctx.r4.u32 == 7 && Wwe12IQuitMatch()) {
    static std::atomic<int> s_logged{0};
    if (s_logged.fetch_add(1) < 8)
      BD_INFO("[iquit] disqualification ignoree (match I QUIT)");
    return;
  }
  __imp__sub_82261320(ctx, base);
}

// ---- a guard for the game's replay recorder ----
// Each frame its thread (sub_829597D8 -> sub_82959CC0 -> ... -> sub_829741E0) asks every superstar's current
// motion for an event (sub_82967DA8: the set at [[[actor + 3396] + 68] + 36] - a count at + 2, a table at + 4, its
// data at + 8 - walked by sub_824A3AB8). The first WWE '13 motion a superstar played (the attacker's call, whose row
// named a motion WWE '12 hasn't) handed it a set that was no set: a read at 0x07FD020C, the game gone (user's test
// of 2026-11-15). The row is put right when the pack is built (m3_params::Insert); here a set whose pointers are no
// guest addresses is answered "no event" instead of being walked.
REX_EXTERN(__imp__sub_82967DA8);
extern "C" REX_FUNC(sub_82967DA8) {
  const auto in_guest = [](u32 p) { return p >= 0x30000000u && p < 0xF0000000u; };
  const u32 actor = ctx.r4.u32;
  const u32 part = in_guest(actor) ? bd::mem::try_load<u32>(actor + 3396) : 0;
  const u32 motion = in_guest(part) ? bd::mem::try_load<u32>(part + 68) : 0;
  const u32 set = in_guest(motion) ? bd::mem::try_load<u32>(motion + 36) : 0;
  const bool bad = !in_guest(motion) ||
                   (set && (!in_guest(set) || (bd::mem::try_load<u16>(set + 2) &&
                                               (!in_guest(bd::mem::try_load<u32>(set + 4)) ||
                                                !in_guest(bd::mem::try_load<u32>(set + 8))))));
  if (bad) {
    static std::atomic<int> s_logged{0};
    if (s_logged.fetch_add(1) < 8)
      BD_INFO("[iquit] ralenti : evenements illisibles pour l'acteur {:08X} (motion {}, objet {:08X}, jeu {:08X}) : ignores",
              actor, in_guest(actor) ? Anim(actor) : 0xFFFF, motion, set);
    ctx.r3.u64 = 0;
    return;
  }
  __imp__sub_82967DA8(ctx, base);
}

// ---- motions added by a motions mod (WWE '13's, ported by the Mod Maker): known to the game's motion directory ----
// A motion played by number is queued (sub_821D7480) and the queue's reader (sub_821D7C80) drops any number without a
// record in the motion directory [0x8317B870]. In a match the directory's general slot (1) is built by
// sub_8246E3B0(directory, bitmap, slot) from a 65536-bit bitmap (bit n = word n / 32, 1 << n % 32) of the numbers the
// game wants; the archives are then walked and only those numbers read. (The superstars' slots 11-22 and their fixed
// list 13100-13135 are not built in a match: 13120, in that list, was dropped all the same, user's test of
// 2026-11-15.) Here the free numbers of the referee's hundred, 13115-13199, are added to slot 1's bitmap: a number
// no file has just stays an entry without a record, as the game's own do.
REXCVAR_DEFINE_BOOL(wwe12_iquit_added_motions, false, "WWE12",
                    "I Quit match: let the game load motions 13115-13199 added by a motions mod (WWE '13's, test).");
REX_EXTERN(__imp__sub_8246E3B0);
extern "C" REX_FUNC(sub_8246E3B0) {
  const u32 bitmap = ctx.r4.u32, slot = ctx.r5.u32;
  if (REXCVAR_GET(wwe12_iquit_added_motions) || REXCVAR_GET(wwe12_iquit_wwe13_motions)) {
    const bool made = bd::mem::try_load<u32>(ctx.r3.u32 + (slot + 188) * 4) != 0;
    static std::atomic<int> s_calls{0};
    if (s_calls.fetch_add(1) < 60)
      BD_INFO("[iquit] repertoire des motions : case {} (appel {:08X}, deja faite {})", slot, u32(ctx.lr), made);
    if (bitmap && slot == 1 && !made) {
      for (u32 n = 13115; n < 13200; ++n) {
        const u32 w = bitmap + (n >> 5) * 4;
        bd::mem::store<u32>(w, bd::mem::try_load<u32>(w) | (1u << (n & 31)));
      }
      BD_INFO("[iquit] motions 13115-13199 ajoutees au repertoire (case 1)");
    }
  }
  __imp__sub_8246E3B0(ctx, base);
}

// ---- WWE '13's I Quit files in the exe ----
// Resource 102 (port/res/iquit13.bin, tools/pack_iquit13.py): the 17 motions (<number>.high / .m3) and the HUD
// images (hud/<language>/<name>.dds). The user's decision of 2026-11-15: the I Quit match as WWE '13's for every
// player, no mod needed - WWE '13's data in the exe, the DMCA risk his (as the PS3 button icons). A motions mod
// with the same numbers, or a hud folder of its own, goes first.
bool Wwe12SrefWanted();  // wwe12_sref.cpp: the cvar wwe12_sref
const std::vector<std::pair<std::string, std::vector<uint8_t>>> &Wwe12IQuit13Files() {
  static const std::vector<std::pair<std::string, std::vector<uint8_t>>> files = [] {
    std::vector<std::pair<std::string, std::vector<uint8_t>>> out;
#ifdef _WIN32
    HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(102), MAKEINTRESOURCEW(10));  // 10 = RT_RCDATA
    HGLOBAL mem = res ? LoadResource(nullptr, res) : nullptr;
    const uint8_t *d = mem ? static_cast<const uint8_t *>(LockResource(mem)) : nullptr;
    const size_t size = res ? SizeofResource(nullptr, res) : 0;
    if (d && size >= 8 && !std::memcmp(d, "IQ13", 4)) {
      uint32_t count;
      std::memcpy(&count, d + 4, 4);
      size_t o = 8;
      for (uint32_t i = 0; i < count && o + 2 <= size; ++i) {
        uint16_t nlen;
        std::memcpy(&nlen, d + o, 2);
        o += 2;
        if (o + nlen + 4 > size)
          break;
        std::string name(reinterpret_cast<const char *>(d + o), nlen);
        o += nlen;
        uint32_t len;
        std::memcpy(&len, d + o, 4);
        o += 4;
        if (o + len > size)
          break;
        // (the Special Guest Referee's files - its motions 13115-13149, its HUD parts "0_90_..." - only with that
        // mode on: without it the pack built from these files is the I Quit's alone, the one players have had)
        const int number = atoi(name.c_str());
        const bool sref = (number >= 13115 && number < 13150) || name.find("/0_90_") != std::string::npos;
        if (!sref || Wwe12SrefWanted())
          out.emplace_back(std::move(name), std::vector<uint8_t>(d + o, d + o + len));
        o += len;
      }
    }
#endif
    return out;
  }();
  return files;
}

// ---- WWE '13's own I Quit images (the player's own, from his WWE '13) ----
// WWE '13 keeps its I Quit HUD in pac\game2dHD.pac, one set per language (G?2D/BASE, child 16): "I QUIT" red and
// grey, a glow, the A button and "NO!", the D-pad and "THREATEN", the B button and "ASK HIM", the "NO" mark of the
// gauge. A motions mod carries them as hud\<en|fr|de|it|es>\<name>.dds (name = the texture's place in child 16).
// They ride in the overlay's font atlas (rects added in the font set-up, the atlas built, the pixels copied in -
// checked against the SDK's ImGui outside the game, 2026-11-15): no texture of ours to make while the game draws.
// Without them the overlay keeps its own drawing.
namespace {
// (the last two: the Special Guest Referee's gauge, WWE '13's main HUD - child 0, sub 90: the striped shirt and a
// bar's frame; optional, a motions mod's own hud folder may not have them)
enum {
  kImgQuitRed, kImgQuitGrey, kImgGlow, kImgA, kImgNo, kImgDpad, kImgThreat, kImgB, kImgAsk, kImgNoMark,
  kImgRefShirt, kImgRefFrame, kImgRefBack, kImgRefFill, kImgCount
};
constexpr int kImgNeeded = kImgRefShirt;  // (the images before this one: all or nothing)
constexpr const char *kImgFile[kImgCount] = {"0_0", "0_1", "0_2", "1_0", "1_1", "2_0", "2_1", "3_0", "3_1", "90_0",
                                             "0_90_122", "0_90_47", "0_90_48", "0_90_125"};
struct HudImg {
  ImFontAtlasRectId id = -1;
  int w = 0, h = 0;
};
HudImg g_img[kImgCount];
ImFontAtlas *g_atlas = nullptr;
bool g_hud = false;

// an image `high` pixels high, its left edge at x and its middle at y; its width (0: no image)
float Img(ImDrawList *dl, int i, float x, float y, float high, ImU32 colour = IM_COL32_WHITE) {
  ImFontAtlasRect r;
  if (!g_hud || g_img[i].id < 0 || !g_atlas->GetCustomRect(g_img[i].id, &r) || g_img[i].h <= 0)
    return 0.f;
  const float wide = high * float(g_img[i].w) / float(g_img[i].h);
  dl->AddImage(g_atlas->TexRef, ImVec2(x, y - high * 0.5f), ImVec2(x + wide, y + high * 0.5f), r.uv0, r.uv1, colour);
  return wide;
}
float Wide(int i, float high) { return g_hud && g_img[i].h > 0 ? high * float(g_img[i].w) / float(g_img[i].h) : 0.f; }
}  // namespace

// From the app's font set-up (wwe12_app.h, OnConfigureFonts): before the overlay's atlas is built.
void Wwe12IQuitFonts(ImFontAtlas *atlas) {
  namespace fs = std::filesystem;
  if (!atlas)
    return;
  const char *lang = "en";
  switch (REXCVAR_GET(user_language)) {
    case 3: lang = "de"; break;
    case 4: lang = "fr"; break;
    case 5: lang = "es"; break;
    case 6: lang = "it"; break;
    default: break;
  }
  std::error_code ec;
  fs::path dir;
  for (fs::directory_iterator it(rex::filesystem::GetExecutableFolder() / "mods" / ".svrmods" / "Motions", ec), end;
       !ec && it != end && dir.empty(); it.increment(ec)) {
    if (!it->is_directory(ec) || fs::exists(it->path() / "disabled", ec))
      continue;
    for (const char *l : {lang, "en"})
      if (dir.empty() && fs::exists(it->path() / "hud" / l / "3_1.dds", ec))
        dir = it->path() / "hud" / l;
  }
  const bool builtin = dir.empty();
  if (builtin && (!REXCVAR_GET(wwe12_iquit_wwe13_motions) || Wwe12IQuit13Files().empty()))
    return;
  // each image cut to what it shows (the textures are padded: the B button sits at the right of a 256 x 32 one)
  std::vector<svrfmt::Image> cut(kImgCount);
  for (int i = 0; i < kImgCount; ++i) {
    svrfmt::Bytes dds;
    if (builtin) {
      for (const char *l : {lang, "en"})
        for (const auto &[name, data] : Wwe12IQuit13Files())
          if (dds.empty() && name == std::string("hud/") + l + "/" + kImgFile[i] + ".dds")
            dds = data;
    } else {
      std::ifstream f(dir / (std::string(kImgFile[i]) + ".dds"), std::ios::binary);
      dds.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }
    svrfmt::Image im;
    if (dds.empty() || !svrfmt::DdsDecode(dds, im) || im.w <= 0 || im.h <= 0 || im.rgba.size() < size_t(im.w) * im.h * 4) {
      if (i >= kImgNeeded)
        continue;
      return;
    }
    int x0 = im.w, y0 = im.h, x1 = -1, y1 = -1;
    for (int y = 0; y < im.h; ++y)
      for (int x = 0; x < im.w; ++x)
        if (im.rgba[(size_t(y) * im.w + x) * 4 + 3] > 8)
          x0 = std::min(x0, x), x1 = std::max(x1, x), y0 = std::min(y0, y), y1 = std::max(y1, y);
    if (x1 < x0 || y1 < y0) {
      if (i >= kImgNeeded)
        continue;
      return;
    }
    svrfmt::Image &c = cut[size_t(i)];
    c.w = x1 - x0 + 1, c.h = y1 - y0 + 1;
    c.rgba.resize(size_t(c.w) * c.h * 4);
    for (int y = 0; y < c.h; ++y)
      std::memcpy(&c.rgba[size_t(y) * c.w * 4], &im.rgba[(size_t(y0 + y) * im.w + x0) * 4], size_t(c.w) * 4);
  }
  for (int i = 0; i < kImgCount; ++i) {
    g_img[i].w = cut[size_t(i)].w, g_img[i].h = cut[size_t(i)].h;
    if (g_img[i].w <= 0 || g_img[i].h <= 0)
      continue;  // (an optional image that is not there)
    g_img[i].id = atlas->AddCustomRect(g_img[i].w + 2, g_img[i].h + 2);  // (a clear pixel all round: no bleeding)
    if (g_img[i].id < 0)
      return;
  }
  unsigned char *px = nullptr;
  int tw = 0, th = 0;
  atlas->GetTexDataAsRGBA32(&px, &tw, &th);  // (builds the atlas: the rects have their place)
  if (!px)
    return;
  for (int i = 0; i < kImgCount; ++i) {
    ImFontAtlasRect r;
    if (g_img[i].id < 0)
      continue;
    if (!atlas->GetCustomRect(g_img[i].id, &r) || r.x + r.w > tw || r.y + r.h > th)
      return;
    for (int y = 0; y < r.h; ++y)
      std::memset(px + (size_t(r.y + y) * tw + r.x) * 4, 0, size_t(r.w) * 4);
    for (int y = 0; y < g_img[i].h; ++y)
      std::memcpy(px + (size_t(r.y + 1 + y) * tw + r.x + 1) * 4, &cut[size_t(i)].rgba[size_t(y) * g_img[i].w * 4],
                  size_t(g_img[i].w) * 4);
  }
  g_atlas = atlas;
  g_hud = true;
  BD_INFO("[iquit] images de WWE '13 chargees ({}), atlas {} x {}", builtin ? std::string("de l'exe, ") + lang : rex::path_to_utf8(dir),
          tw, th);
}

// ---- the gauge, drawn over the game ----

namespace {
rex::ui::ImGuiDrawer *g_drawer = nullptr;
std::function<void(std::function<void()>)> g_post;  // runs a function on the UI thread
std::atomic<bool> g_shown{false};                    // the gauge's dialog exists (UI thread owns it)

// WWE '13's I Quit HUD, laid out on the game itself (the user's captures of 2026-11-15): the gauge floats over the
// asked man - a dark bar in a white slanted frame, a fifth of the screen wide, the red zone, a thin white cursor,
// "I QUIT" over it whitening with the seconds, the "NO" mark under the zone; the prompts sit at the attacker's feet
// ("B ASK"; the D-pad and "INTIMIDATE" under a blue bar that empties while a threat lasts); when he quits, the red
// "I QUIT" in the middle of the screen. Sizes in WWE '13's own 720 lines. Where a superstar stands on screen comes
// from g_spot (not known yet: fixed places meanwhile).
void Draw13(ImGuiIO &io, ImDrawList *dl, int phase, bool sub_only, bool hint_only) {
  const float k7 = io.DisplaySize.y / 720.0f, W = io.DisplaySize.x, H = io.DisplaySize.y;
  auto spot = [&](int who, float fx, float fy) {  // a superstar's feet on screen, else a fixed place
    const float sx = g_spot[who][0].load(), sy = g_spot[who][1].load();
    return sx >= 0.f ? ImVec2(sx * W, sy * H) : ImVec2(fx * W, fy * H);
  };
  if (hint_only || ((phase == kGauge || phase == kCall) && g_asker_human.load())) {
    const ImVec2 at = spot(0, 0.50f, 0.90f);
    float px = at.x - 40.f * k7;
    const float py = at.y + 14.f * k7;
    if (hint_only) {
      px += Img(dl, kImgB, px, py, float(g_img[kImgB].h) * k7) + 5.f * k7;
      Img(dl, kImgAsk, px, py, float(g_img[kImgAsk].h) * k7);
    } else if (phase == kGauge && !g_result.load()) {
      // the threat's blue bar (it empties while the threat lasts), the D-pad and "INTIMIDATE" under it
      const float bw = 118.f * k7, bh = 9.f * k7, by = py - 20.f * k7, left = g_threat_left.load();
      dl->AddRectFilled(ImVec2(px - 2 * k7, by - 2 * k7), ImVec2(px + bw + 2 * k7, by + bh + 2 * k7),
                        IM_COL32(235, 235, 235, 255));
      dl->AddRectFilled(ImVec2(px, by), ImVec2(px + bw, by + bh), IM_COL32(15, 15, 20, 255));
      dl->AddRectFilled(ImVec2(px, by), ImVec2(px + bw * (left > 0.f ? left : 1.f), by + bh), IM_COL32(40, 90, 220, 255));
      px += Img(dl, kImgDpad, px, py, float(g_img[kImgDpad].h) * k7) + 5.f * k7;
      Img(dl, kImgThreat, px, py, float(g_img[kImgThreat].h) * k7);
    }
  }
  if (hint_only || phase == kCall)
    return;
  const float shake = g_shake.load() * k7 * 0.5f;  // (the threat shakes the gauge)
  const ImVec2 over = spot(1, 0.50f, 0.74f);       // the asked man: the gauge a little above him
  const float w = 272.f * k7, h = 15.f * k7, slant = 6.f * k7;
  const float x = std::clamp(over.x - w * 0.5f, 20.f * k7, W - w - 20.f * k7) + shake;
  const float y = std::clamp(over.y - 100.f * k7, 60.f * k7, H - 80.f * k7) + shake * 0.3f;
  // "I QUIT" over the bar, whiter and whiter
  const float white = g_white.load();
  const int g = int(150 + 105 * white);
  const float th = float(g_img[kImgQuitGrey].h) * k7 * 1.25f;
  Img(dl, kImgQuitGrey, x + w * 0.5f - Wide(kImgQuitGrey, th) * 0.5f, y - th * 0.5f - 5.f * k7, th, IM_COL32(g, g, g, 255));
  if (!sub_only) {
    // the bar: a white slanted frame, dark inside, the red zone, the thin white cursor
    const ImVec2 f0(x - 3 * k7 + slant, y - 3 * k7), f1(x + w + 3 * k7 + slant, y - 3 * k7),
        f2(x + w + 3 * k7 - slant, y + h + 3 * k7), f3(x - 3 * k7 - slant, y + h + 3 * k7);
    dl->AddQuadFilled(f0, f1, f2, f3, IM_COL32(235, 235, 235, 255));
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(18, 18, 22, 255));
    const float zc = g_centre.load() / kBar, zw = g_width.load() / kBar;
    const float z0 = x + w * std::max(0.f, zc - zw * 0.5f), z1 = x + w * std::min(1.f, zc + zw * 0.5f);
    dl->AddRectFilled(ImVec2(z0, y + 2 * k7), ImVec2(z1, y + h - 2 * k7), IM_COL32(225, 25, 25, 255));
    const float cx = x + w * (g_cursor.load() / kBar);
    dl->AddRectFilled(ImVec2(cx - 1.5f * k7, y - 1 * k7), ImVec2(cx + 1.5f * k7, y + h + 1 * k7),
                      IM_COL32(255, 255, 255, 255));
    const float mh = float(g_img[kImgNoMark].h) * k7;
    Img(dl, kImgNoMark, (z0 + z1) * 0.5f - Wide(kImgNoMark, mh) * 0.5f, y + h + 4.f * k7 + mh * 0.5f, mh);
    if (g_asked_human.load() && !g_result.load()) {  // the asked man's own prompt: A and "NO!"
      const float py = y + h + 4.f * k7 + mh + 16.f * k7;
      const float aw = Img(dl, kImgA, x + w * 0.5f - 40.f * k7, py, float(g_img[kImgA].h) * k7);
      Img(dl, kImgNo, x + w * 0.5f - 40.f * k7 + aw + 5.f * k7, py, float(g_img[kImgNo].h) * k7);
    }
  }
  if (g_result.load() == 4) {  // he quits: the red "I QUIT" in the middle of the screen
    const float rh = float(g_img[kImgQuitRed].h) * k7 * 1.35f, gl = float(g_img[kImgGlow].h) * k7 * 4.f;
    Img(dl, kImgGlow, W * 0.5f - Wide(kImgGlow, gl) * 0.5f, H * 0.51f, gl, IM_COL32(255, 255, 255, 110));
    Img(dl, kImgQuitRed, W * 0.5f - Wide(kImgQuitRed, rh) * 0.5f, H * 0.51f, rh);
  }
}

class IQuitOverlay final : public rex::ui::ImGuiDialog {
 public:
  explicit IQuitOverlay(rex::ui::ImGuiDrawer *drawer) : ImGuiDialog(drawer) {}

 protected:
  void OnClose() override { g_shown = false; }
  void OnDraw(ImGuiIO &io) override {
    if (Paused() && (g_phase.load() != kIdle || g_hint.load() || g_sub.load()))
      return;  // (the pause menu: nothing of ours over it; the gauge is back with the match)
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
    if (g_hud) {
      Draw13(io, dl, phase, sub_only, hint_only);
      return;
    }
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

// (the Special Guest Referee's images, for wwe12_sref.cpp: 0 the shirt, 1 the bar's frame, 2 its back, 3 its fill;
// drawn into the box x, y, wide, high, in a colour, the first `part` of its width only; false: no such image)
bool Wwe12HudRefImg(ImDrawList *dl, int which, float x, float y, float wide, float high, ImU32 colour, float part) {
  static constexpr int kOf[4] = {kImgRefShirt, kImgRefFrame, kImgRefBack, kImgRefFill};
  ImFontAtlasRect r;
  if (which < 0 || which > 3 || !g_hud || g_img[kOf[which]].id < 0 || !g_atlas->GetCustomRect(g_img[kOf[which]].id, &r))
    return false;
  if (dl && part > 0.f)
    dl->AddImage(g_atlas->TexRef, ImVec2(x, y), ImVec2(x + wide * part, y + high), r.uv0,
                 ImVec2(r.uv0.x + (r.uv1.x - r.uv0.x) * part, r.uv1.y), colour);
  return true;
}
// (a motion asked from the game's queue, and whether the queue has read it: as Ready above)
bool Wwe12MotionReady(const PPCContext &in, u8 *base, u32 motion) { return Ready(in, base, motion); }
// (the same drawer and UI thread for the Special Guest Referee's gauge, wwe12_sref.cpp)
rex::ui::ImGuiDrawer *Wwe12OverlayDrawer() { return g_drawer; }
void Wwe12OverlayPost(std::function<void()> f) {
  if (g_post)
    g_post(std::move(f));
}

namespace {
void ShowGauge() {
  if (!g_drawer || !g_post || g_shown.exchange(true))
    return;
  g_post([] { new IQuitOverlay(g_drawer); });  // deletes itself when the mini-game is over
}
}  // namespace
