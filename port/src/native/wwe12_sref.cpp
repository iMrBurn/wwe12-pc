// WWE '12: the SPECIAL GUEST REFEREE match brought back (1.4), played as in WWE '13.
//
// What WWE '12 kept of it (read in its code and files, 2026-11-15; docs/limit_breaking.md):
// - the match rule 83 "SPECIAL REFEREE" (misc.pac, RUL): 3 people, the third place of kind 1 = the referee. Set up
//   from a menu row (sub_82449898 gives 83 / 180 for the unused row 1 of page 32), the third superstar is made with
//   the role 1 (actor + 3428) and the game makes no normal referee (sub_82A37378: a place of kind 1 holds a
//   superstar -> participant 40 cleared);
// - the rule's record + 52 (the character select's layout, sub_827A1BF0) is 0, ONE ON ONE's: two places only.
//   TRIPLE THREAT's (rule 13: 6, three places) is given to it here;
// - the judge ([0x83174054]: + 92 its state - 1 a pin, 3 the 5 count, 4 the ring-out count; + 100 the count; + 108
//   the victim; + 112 = 2 when the man with the role 1 is a superstar) counts by itself only when the match has no
//   referee. With one, the count goes up from the events of the referee's motions, whoever plays them
//   (sub_8233F660: motion 190, event 22 of its block 1 and event 30 of its block 3; sub_8233F898: 191 and 200).
//   WWE '12 has nothing that makes a played superstar play them: that part is written here.
//
// WWE '13 (its script and code, read the same day): the special referee walks and fights as a superstar; his
// grapple button does the referee's work for the situation - a pin: motion 190 block 0 (he gets down), then one
// press a count (block 1, block 1, block 3; not sooner than 40 frames after the start, 18 after a count); the
// ring-out count: motion 191, a press a count, 50 frames apart; the 5 count: motion 200.
// Not done yet: his gauge (0.5 at the start, down when he does not do his work, the normal referee takes over at 0,
// the "screw job" at 1), the caution, the DQ he pronounces himself, the submission check, the computer as referee.

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(wwe12_sref, false, "WWE12",
                    "Special Guest Referee (rule 83): the superstar who referees counts with A (as WWE '13). Dev for now.");

// The game still has the played referee's count (read 2026-11-16): an actor with the role 1 whose pad presses the
// "action 2" button goes to sub_823C97B0 (from his free state, sub_8234FD30), which starts the count's motion for
// the judge's state (sub_823C9458: 190 / 200 / 191 / 202); the next counts come from motion 190's own handler
// (sub_8233F660: the button again, block 1, then block 3 for the last). So by default nothing is played from here.
// Which button "action 2" is comes from the controls' table, filled when the game runs: not readable in the code.
// So A does it here, through the game's own functions: the start by sub_823C9458 itself (it picks the motion and
// sets the actor's "counting" flag + 2560), the next counts by the block the game's handler would replay (block 1,
// or block 3 once the judge's count is the needed one less one - sub_822605A0: 3, or 2). The game's own button
// still works beside it (calls of sub_823C97B0 are logged, to learn which one it is).
REXCVAR_DEFINE_BOOL(wwe12_sref_own_count, true, "WWE12",
                    "Special Guest Referee: A counts (the game's own count functions, called for the A button).");

#include <algorithm>
#include <array>
#include <map>
#include <mutex>
#include <set>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <chrono>
#include <cstring>
#include <functional>
#include <iterator>
#include <string>
#include <vector>

#include <imgui.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>

#include <rex/runtime.h>
#include <rex/ppc/func.h>
#include <rex/filesystem.h>
#include <thread>
#include <cctype>
#include <filesystem>
#include <fstream>
REXCVAR_DECLARE(std::string, user_data_root);

#include <fmt/format.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REX_EXTERN(__imp__sub_8247AD18);  // play a motion on an actor
REX_EXTERN(__imp__sub_822143D0);  // an actor's set-up (r3 = actor + 1696; makes his brain 'CMIF' for the role 1)
REX_EXTERN(__imp__sub_8247C670);  // an actor back in his free motion (1350: the one his brain acts from)
REX_EXTERN(__imp__sub_8247C590);  // an actor's free motion's key (actor, key out)
REX_EXTERN(__imp__sub_82479730);  // a motion key put on one of an actor's channels (actor, key, channel, f1)
REX_EXTERN(__imp__sub_8232E200);  // an actor's navigation ('oNav') made if he has none
REX_EXTERN(__imp__sub_827A1BF0);  // the character select's layout of a rule
REX_EXTERN(__imp__sub_82A39AC0);  // a match set up from a rule (setup manager, rule, its options or 0)
REX_EXTERN(__imp__sub_828AE798);  // the character select set up for a rule (select, rule, options or 0, r6, r7)
REX_EXTERN(__imp__sub_82A36150);  // a place's kind set in the match's setup (setup manager, place, kind)
REX_EXTERN(__imp__sub_823C9458);  // the played referee starts his count for the judge's state (actor): 1 if started
REX_EXTERN(__imp__sub_823C97B0);  // an actor's "action 2" button (the referee: sub_823C9458 first)
REX_EXTERN(__imp__sub_822605A0);  // the counts a pin needs (judge): 3, or 2
REX_EXTERN(__imp__sub_82A3F828);  // the match's entrances built from an order record (ranks by side, + 0: six s32)
REX_EXTERN(__imp__sub_823F8270);  // a place's participant number
REX_EXTERN(__imp__sub_8267CD30);  // a participant's record (number): + 8 kind, + 13 side, + 16 superstar (51200 none)
REX_EXTERN(__imp__sub_82596848);  // the character select: a place's "COM" / "1P" label, once chosen (place panel)
REX_EXTERN(__imp__sub_825962D0);  // the same while choosing
REX_EXTERN(__imp__sub_8256CEB0);  // the character select's screen laid out from its template (screen)
REX_EXTERN(__imp__sub_82568C78);  // a pad's move between the places (screen, place, origin, direction, table r7, ...)
REX_EXTERN(__imp__sub_82573BA0);  // the match card: a place's portrait in a slot (card, place r4, card kind r5, widgets)

REXCVAR_DEFINE_BOOL(wwe12_sref_middle, true, "WWE12",
                    "Special Guest Referee, 1 on 1: the referee in the middle of the character select and of the match card "
                    "(as WWE '13).");

REXCVAR_DEFINE_BOOL(wwe12_sref_entrance, true, "WWE12",
                    "Special Guest Referee: the referee has his entrance, the first one (as WWE '13).");
REXCVAR_DEFINE_BOOL(wwe12_sref_gauge, true, "WWE12",
                    "Special Guest Referee: his gauge (as WWE '13: down when he does not do his work or hits a superstar; "
                    "empty, he is the referee no more; full, Y during a hold is the screw job).");
REX_EXTERN(__imp__sub_822A7600);  // an attack's hit resolved (attack object: + 256 attacker, + 272 victim, + 288 guarded)
REX_EXTERN(__imp__sub_82371C78);  // an actor leaves the match's area (the game's own behaviour for a man out of the match)
REX_EXTERN(__imp__sub_82261320);  // the end of a match for a superstar (victim, reason, winner's number)
REX_EXTERN(__imp__sub_82520B90);  // a world point to the screen (float[4] in and out), the game's own
rex::ui::ImGuiDrawer *Wwe12OverlayDrawer();           // wwe12_iquit.cpp
void Wwe12OverlayPost(std::function<void()> f);  // wwe12_iquit.cpp: on the UI thread
// wwe12_iquit.cpp: WWE '13's images (0 the shirt, 1 the bar's frame, 2 its back, 3 its fill) and the motions' queue
bool Wwe12HudRefImg(ImDrawList *dl, int which, float x, float y, float wide, float high, ImU32 colour, float part);
bool Wwe12MotionReady(const PPCContext &in, u8 *base, u32 motion);
float Wwe12HudRefPrompt(ImDrawList *dl, int button, int row, float x, float y, float high);

REXCVAR_DEFINE_BOOL(wwe12_sref_replacement, true, "WWE12",
                    "Special Guest Referee: the game's own referee waits out of sight and takes over when the gauge is "
                    "empty (as WWE '13).");
REX_EXTERN(__imp__sub_82A37378);  // the match's setup: the normal referee kept or not (setup manager)
REX_EXTERN(__imp__sub_823B5D18);  // the match put back in place (its actors' flags by role)
REX_EXTERN(__imp__sub_822CF850);  // an actor drawn or not (actors' manager [0x8317434C], its slot, on)
REX_EXTERN(__imp__sub_822CE3F0);  // an actor updated or not (manager, slot, on, 0)
REX_EXTERN(__imp__sub_82212140);  // an actor the computer's (actor, on)
REX_EXTERN(__imp__sub_823B8F38);  // every actor drawn and updated again (after the match is put in place)
REXCVAR_DEFINE_BOOL(wwe12_sref_scene, true, "WWE12",
                    "Special Guest Referee: WWE '13's scene on the entrance stage when he is replaced (its motion 2043 "
                    "and its camera).");
REX_EXTERN(__imp__sub_82482388);  // a motion's camera started (motions container, &key, &position, 0, 1)
REX_EXTERN(__imp__sub_8247EE60);  // the camera director's current camera (director, id)
REX_EXTERN(__imp__sub_821CCFE0);  // a camera of the director by id
REX_EXTERN(__imp__sub_82481ED0);  // a camera's active state (camera, on)
REX_EXTERN(__imp__sub_82478510);  // a motion's block found in a motions container (container, motion, x, y)
REX_EXTERN(__imp__sub_82254518);  // a parked object of a type made live again (-1, type, 0, 0)
REX_EXTERN(__imp__sub_82250258);  // an object parked
REX_EXTERN(__imp__sub_82A37AB0);  // a place of the match's setup given a superstar (setup, place, superstar, pad, attire, ...)
REXCVAR_DEFINE_INT32(wwe12_sref_gm, 0, "WWE12",
                     "Special Guest Referee: the superstar number of the General Manager of the stage scene (WWE '13's John "
                     "Laurinaitis, added by the Mod Maker); 0 = found by his name among the added superstars "
                     "(superstar_slots.txt), else the first man at ringside plays him.");
REXCVAR_DEFINE_BOOL(wwe12_sref_actions, true, "WWE12",
                    "Special Guest Referee: his actions as WWE '13, with its motions (A: caution a superstar / check a "
                    "submission; Y: pronounce a disqualification / the screw job).");

REXCVAR_DEFINE_INT32(wwe12_sref_from, -1, "WWE12",
                     "Dev: with wwe12_sref, a match set up with this rule (13 = TRIPLE THREAT) is a Special Guest Referee one.");

namespace {
constexpr u32 kMatch = 0x8317C850, kActors = 0x8317B770, kJudgePtr = 0x83174054, kRulesPtr = 0x8324E238;
constexpr u32 kRule = 83, kRuleSize = 108;
constexpr u16 kPadA = 0x1000;

std::atomic<u16> g_prev[4] = {};
std::atomic<bool> g_press{false};  // the referee's pad: A newly pressed
std::atomic<int> g_pad{-1};        // the referee's pad (-1: not a played man)
std::atomic<u32> g_state{0};       // the judge's state, for the pad's thread
int g_step = 0;                    // a pin count: 0 not started, 1 down, 2 / 3 one / two counted, 4 three
int g_t = 1000;                    // frames since his last motion of ours
u32 g_last_state = 0xFFFFFFFF, g_last_count = 0xFFFFFFFF;
u16 g_last_anim = 0xFFFF;
std::string g_last_roles;

// The menu's rows of ours (SPECIALTY > SPECIAL REFEREE > a match > a stipulation), made by Wwe12SrefMenu.
// A row is a copy of the game's own row of that match, so it plays that match's rule; the rule's record is given
// one more place, the referee's, while the row is the chosen one (ONE ON ONE > NORMAL MATCH plays rule 83 itself).
struct Row {
  u32 id = 0, rule = 0, fighters = 0;
  bool tag = false;
};
std::vector<Row> g_rows;           // (written when the menus' table is parsed, read from the game's thread)
std::vector<u32> g_nodes;          // our sub-menus' ids
std::vector<u32> g_game_rows;      // the game's own match rows' ids
std::atomic<int> g_sel{-1};        // the row chosen in the menu (-1: another match)
int g_cur = -1;                    // the row the character select was set up for
bool g_arm = false;                // the match's setup running now comes from that select
std::atomic<bool> g_committed{false};  // the match set up is one of ours
u32 g_patched = 0xFFFFFFFF;        // the rule whose record has the referee's place now
u8 g_saved[kRuleSize] = {};        // (its own bytes)

bool On() { return REXCVAR_GET(wwe12_sref) && bd::mem::ready(); }
bool Match() { return g_committed.load(); }

// ---- the gauge (WWE '13's 'SREF' component, read in its code 2026-11-16) ----
// 0.5 at the bell, between 0 and 1. A duty he does not do (the judge waits for a count and he is not in the count's
// motion) drains it after a grace time: 3 s for a pin (motion 190), 5 s for the ring-out and the down counts (191)
// and the 5 count (200), then 0.02 a second. A pin count he gives up (motion 190's block 2 while the pin holds):
// 0.2. His attack on a fighter that connects: 0.1. It comes back by 1 / 150 a second while nothing drains it and he
// carries nothing. Empty, or thirty faults: he is the referee no more. Full: the screw job (Y during a hold - the
// man applying it wins). Not WWE '13's: the cautions, the DQ he pronounces, the normal referee who replaces him
// (their motions 2020-2051 are not in WWE '12; here the judge counts alone once he is out, as in a match without
// a referee, and he cannot pin anyone).
using Clock = std::chrono::steady_clock;
int64_t NowMs() { return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count(); }
std::atomic<float> g_gauge{0.5f};
std::atomic<bool> g_hud_want{false}, g_hud_shown{false}, g_ejected{false};
std::atomic<int64_t> g_tick_ms{0}, g_ejected_ms{0};
std::atomic<bool> g_hit{false}, g_y{false};
std::atomic<int64_t> g_hit_ms{0};
std::atomic<u32> g_hit_victim{0};
bool g_started = false, g_draining = false, g_left = false;
double g_grace = 0;
u32 g_duty = 0xFFFFFFFF, g_last_block = 255, g_ex = 0;
int g_faults = 0;
int64_t g_prev_ms = 0;
// (his actions' state: RefActions, below)
enum { kActNone, kActCaution, kActNothing, kActCheck, kActScrew, kActDq };
struct Timed {
  u32 actor = 0, anim = 0, x = 0, to = 0;
  double left = 0, waited = 0;
  bool waiting = false;
};
Timed g_rm, g_tm;  // the referee's motion of ours, the cautioned fighter's
int g_act = kActNone, g_check_block = -1;
double g_act_t = 0, g_argue_t = 0, g_hold_grace = 3.0;
bool g_resolved = false, g_argue = false, g_answered = false, g_hold_drain = false;
u32 g_target = 0, g_by = 0, g_held = 0, g_check_anim = 0;
int64_t g_strike[6][3] = {};
std::atomic<u32> g_foul_by{0};
std::atomic<int64_t> g_foul_ms{0};
std::atomic<bool> g_want_y{false};
std::atomic<bool> g_a_game{false};
std::atomic<int64_t> g_live_ms{0};  // the tick's last run (the match runs)
std::atomic<bool> g_b{false};
u16 g_ref_idle = 20150;             // the referee's standing motion, as last seen
double g_hold_gone = 0;             // how long no hold has been seen while he checks  // A is the game's (a grapple): a fighter within reach, no hold, no count
u32 g_fin_given = 0, g_fin_before = 0;  // a finisher given for a full gauge (WWE '13's "special skill": 0.75)
std::atomic<int> g_prompt{0};
void RefActions(const PPCContext &ctx, u8 *base, u32 ref, u32 state, double dt, int64_t now);
// (the normal referee waiting, and the hand-over: below)
enum { kOutNone, kOutWait, kOut2050, kOutScene, kOutArrive, kOutDone };
int g_out = kOutNone;
int64_t g_ex_ms = 0, g_watch_ms = 0;
u32 g_sref_man = 0;  // the special referee when his replacement began
bool g_judge_alone = false;  // the game's referee set aside for a count the judge made alone
bool g_scene_ran = false;
// The game's main thread (the one that reads each actor's pad word and updates every actor's components). The tick also
// runs on a worker thread, inside a brain's update: an object taken out of its park there (the scene's microphone,
// sub_82254518) waited for the main thread, which waited for it - the game froze (user's test of 2026-11-16, 18:03).
// The replacement - its scene, its objects, its motions - is done on the main thread only.
std::atomic<std::thread::id> g_main_tid{};
std::atomic<bool> g_scene_on{false};  // (the stage scene is played: nothing of ours over it)
// The game's referee walks from the stage into the ring after the scene, as in WWE '13. WWE '12's referee never leaves the
// ring in a match, his brain does not walk him outside it (user's tests of 2026-11-16: still on the stage, at the ramp's
// foot); the game itself walks men through a virtual pad - the leaving walk 'RRTR' (sub_82371D10 -> sub_8225C4D8) took
// the old referee out through the ropes (his motion 717) and up the ramp, test of 19:57 - and so does our I Quit referee
// (wwe12_iquit.cpp). Here his pad's stick: toward the ramp's foot, then toward the ring.
std::atomic<bool> g_walk{false};
std::atomic<int> g_walk_phase{0};  // 0: to the ramp's foot, 1: into the ring
std::atomic<int> g_walk_seen{0};   // (his pad words given the stick)
// Into the ring: at the apron the computer's man got in through the ropes with the motion 716, started from about
// 366 cm out, facing the ring (study of 2026-11-16, 20:17; the game picks 712 / 714 / 716 by the man's build - the
// pairs 712-713, 714-715, 716-717 its preload table loads, sub_8261DAA8, the second of each the way out: the old
// referee left by 717). The pad's way (stick + buttons 0x1400, even as a fighter) did not take our referee in (test
// of 20:22): the motion is played on him here, the first of 712 / 714 / 716 the game has ready.
std::atomic<bool> g_climb{false};
std::atomic<u32> g_climb_frames{0};
u32 g_climb_anim = 0;
std::atomic<int64_t> g_in_ring_ms{0};  // (when he was put in place in the ring after his climb)
// The game's referee in the ring (tests of 2026-11-17): the game gives him no input at all (his pad words stay 0;
// even the referee's behaviour sub_823C97B0 called for him wrote none - an ordinary referee's input component is not
// his). So his work is done from here with the pieces seen working: the virtual pad's stick (he walks near the
// fighters; to a pin, running) and the game's own count functions (sub_823C9458 starts it, motion 190's blocks 1 / 3
// the next counts - as the special referee's A).
std::atomic<int64_t> g_auto_ms{0};  // (when this count was started from here, 0: not)
std::atomic<bool> g_ref_moving{false};
// The old referee's way out (test of 2026-11-17 08:21: Mason Ryan, sent out from the ring's middle, left it by the
// far side's ropes and stuck by the steel steps): first walked to the ramp's side of the ring, then the game's leaving
// walk 'RRTR' takes him out by the near ropes.
std::atomic<bool> g_ex_walk{false};
int64_t g_climb_ms = 0;
float WalkTargetZ(int phase) { return phase == 0 ? 430.f : 120.f; }
bool OnMainThread() { return g_main_tid.load() == std::this_thread::get_id(); }
int g_gm_place = -1;  // the General Manager's place (and actor number), -1: none
bool g_gone = false;
double g_out_t = 0;
Timed g_nm;  // the normal referee's motion of ours
void HandOver(const PPCContext &ctx, u8 *base);
void SetRole(const PPCContext &ctx, u8 *base, u32 index, u32 participant, u32 role);
void Shown(const PPCContext &ctx, u8 *base, u32 actor, bool on);
bool Parked(u32 n) { return n && bd::mem::try_load<u32>(n + 3428) == 6; }
void ShowHud();  // below: the gauge's drawing, made on the UI thread
// Where the referee is on screen (as wwe12_iquit.cpp: sub_82520B90 turns a world point into back-buffer pixels with
// the calling thread's view; a bone = a pointer to a 4 x 4 matrix, its place at + 48; the body's bone at actor + 120).
std::atomic<float> g_sx{-1.f}, g_sy{-1.f};
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
  return sx > -0.2f && sx < 1.2f && sy > -0.2f && sy < 1.2f;
}
void Cost(float c, const char *why) {
  const float v = std::clamp(g_gauge.load() - c, 0.f, 1.f);
  g_gauge = v;
  ++g_faults;
  BD_INFO("[sref] jauge : - {:.2f} ({}) -> {:.2f}, {} fautes", c, why, v, g_faults);
}
u32 Actor(u32 i) { return bd::mem::try_load<u32>(kActors + i * 4); }
u16 Anim(u32 actor) {
  const u32 st = actor ? bd::mem::try_load<u32>(actor + 680) : 0;
  return st ? bd::mem::try_load<u16>(st) : 0xFFFF;
}
// the superstar who referees: the first of the six with the role 1
u32 Referee() {
  for (u32 i = 0; i < 6; ++i)
    if (const u32 a = Actor(i); a && bd::mem::try_load<u32>(a + 3428) == 1)
      return a;
  return 0;
}
void Play(const PPCContext &in, u8 *base, u32 actor, u32 anim, u32 x) {
  PPCContext c = in;
  c.r1.u64 = (in.r1.u32 - 512) & ~15u;
  c.r3.u64 = actor;
  c.r4.u64 = anim;
  c.r5.u64 = x;
  c.r6.u64 = 0;
  c.r8.u64 = 1;
  c.r9.u64 = 0;
  const u32 bits = bd::mem::try_load<u32>(0x82000AD8);  // (the float the game passes)
  float f;
  std::memcpy(&f, &bits, 4);
  c.f1.f64 = f;
  __imp__sub_8247AD18(c, base);
  g_t = 0;
  BD_INFO("[sref] motion {} bloc {} sur l'arbitre {:08X} -> motion {}", anim, x, actor, Anim(actor));
}
}  // namespace

// The character select's layout: rule 83's record says ONE ON ONE's (two places). TRIPLE THREAT's given to it.
namespace {
void Layout() {
  const u32 rules = bd::mem::try_load<u32>(kRulesPtr);
  if (!rules)
    return;
  const u32 mine = rules + kRule * kRuleSize + 52, his = rules + 13 * kRuleSize + 52;
  const u32 want = bd::mem::try_load<u32>(his);
  if (bd::mem::try_load<u32>(mine) != want) {
    BD_INFO("[sref] ecran de choix : fiche 83 + 52 = {} -> {} (celle du TRIPLE THREAT)", bd::mem::try_load<u32>(mine), want);
    bd::mem::store<u32>(mine, want);
  }
}
}  // namespace

// A rule's record given the referee's place: one more person, his place of the kind 1 on a side of his own, and the
// character select's layout of that many people. Counts and layouts are copied from the game's own rules of that
// size (13: three, 14: four, 38: five men each for himself; 5: a tag match and a fifth person), so nothing depends
// on how the record is laid out in memory. Put back when another match is chosen.
namespace {
void Restore() {
  const u32 rules = bd::mem::try_load<u32>(kRulesPtr);
  if (g_patched == 0xFFFFFFFF || !rules)
    return;
  std::memcpy(bd::mem::at<u8>(rules + g_patched * kRuleSize), g_saved, kRuleSize);
  BD_INFO("[sref] regle {} remise comme avant", g_patched);
  g_patched = 0xFFFFFFFF;
}
bool Patch(const Row &row) {
  const u32 rules = bd::mem::try_load<u32>(kRulesPtr);
  if (!rules || row.rule >= 119 || row.fighters < 2 || row.fighters > 4)
    return false;
  const u32 rec = rules + row.rule * kRuleSize;
  const u32 like = row.fighters == 2 ? 13 : row.fighters == 3 ? 14 : row.tag ? 5 : 38;  // a rule of that many people
  const u32 src = rules + like * kRuleSize;
  std::memcpy(g_saved, bd::mem::at<u8>(rec), kRuleSize);
  g_patched = row.rule;
  u8 first = 0, team = 0;
  for (u32 k = 0; k < row.fighters; ++k) {
    first = std::max(first, bd::mem::load<u8>(rec + 2 + 3 * k));
    team = std::max(team, bd::mem::load<u8>(rec + 3 + 3 * k));
  }
  bd::mem::store<u16>(rec, bd::mem::load<u16>(src));            // the people
  bd::mem::store<u32>(rec + 52, bd::mem::load<u32>(src + 52));  // the select's layout
  // The pads' moves between the places come from the record's + 24 (sub_8256D298: one of eight tables at 0x82024CF8,
  // by people: 1 two, 3 three, 5 four, 7 five men, 8 a tag match and a fifth). Left as the rule's own, the new
  // place could not be entered (user's test of 2026-11-16: TRIPLE THREAT + referee, the fourth place out of reach).
  bd::mem::store<u32>(rec + 24, bd::mem::load<u32>(src + 24));
  // (no managers' places in these matches: the layouts of that many people have none - the record's + 79)
  bd::mem::store<u8>(rec + 79, 0);
  // (the match's name on the card before the match: the game's SPECIAL REFEREE, rule 83's + 56)
  bd::mem::store<u32>(rec + 56, bd::mem::load<u32>(rules + kRule * kRuleSize + 56));
  const u32 p = rec + 2 + 3 * row.fighters;
  bd::mem::store<u8>(p, u8(first + 1));
  bd::mem::store<u8>(p + 1, u8(team + 1));
  bd::mem::store<u8>(p + 2, 1);
  BD_INFO("[sref] regle {} : {} combattants + l'arbitre (place {}, cote {}), gabarit {}, table des manettes {}", row.rule,
          row.fighters, first + 1, team + 1, bd::mem::load<u32>(rec + 52), bd::mem::load<u32>(rec + 24));
  return true;
}
}  // namespace

// The character select set up for a match's rule, from the menus (sub_82548818: PLAY's rows). The select keeps the
// rule (its entry + 0, the record at + 1592, the layout + 1588, the people + 92) and hands it to the match's setup
// (sub_828AD3A0 -> sub_82A39AC0) once everyone is chosen. So the rule is made a refereed one here, before the
// places are made (user's tests of 2026-11-15: changed later, the places were the fighters' only).
extern "C" REX_FUNC(sub_828AE798) {
  const u32 select = ctx.r3.u32, rule = ctx.r4.u32;
  if (!On()) {
    __imp__sub_828AE798(ctx, base);
    return;
  }
  Restore();
  g_cur = -1;
  const int sel = g_sel.load();
  bool ours = false;
  if (sel >= 0 && sel < int(g_rows.size()) && g_rows[size_t(sel)].rule == rule) {
    const Row &row = g_rows[size_t(sel)];
    ours = rule == kRule ? true : Patch(row);
    if (ours)
      g_cur = sel;
  } else if (int(rule) == REXCVAR_GET(wwe12_sref_from)) {  // (dev: a game's row played as rule 83)
    g_rows.push_back(Row{0, kRule, 2, false});
    g_cur = int(g_rows.size()) - 1;
    ctx.r4.u64 = kRule;
    ours = true;
  }
  if (ours && ctx.r4.u32 == kRule) {
    Layout();
    ctx.r5.u64 = 0;  // (rule 83's own options)
  }
  BD_INFO("[sref] ecran de choix prepare pour la regle {}{} (options {:08X}, appel {:08X})", rule,
          ours ? " : avec un arbitre special" : "", ctx.r5.u32, u32(ctx.lr));
  __imp__sub_828AE798(ctx, base);
  if (ours) {
    const u32 e = select + bd::mem::try_load<u32>(select + 15960) * 1596;
    BD_INFO("[sref] ecran de choix : regle {} gabarit {} personnes {}", bd::mem::try_load<u32>(e), bd::mem::try_load<u32>(e + 1588),
            bd::mem::try_load<u32>(e + 92));
  }
}

// A place's kind. The rule's own kinds (the referee's place: 1) are set by sub_82A39AC0, but the select then sets
// each chosen place's kind itself (sub_828AD3A0: 0 a fighter, 2 a manager, 3...), so the referee was a fighter and
// the normal referee was made (user's test of 2026-11-15, 22:17: roles 0 0 0, referee in slot 6).
// In a setup of ours the place after the fighters keeps the kind 1; the game then makes no normal referee
// (sub_82A37378). Seen working on 2026-11-16 (rule 83: role 1, no actor 6, the judge's + 112 = 2).
// The General Manager's superstar number: the setting, else the added superstar whose mod's name holds "laurinaitis"
// (the Mod Maker's superstar mods get their numbers in <user data>\superstar_slots.txt: "superstar_<name>=<number>").
namespace {
int GmNumber() {
  if (const int n = REXCVAR_GET(wwe12_sref_gm); n > 0)
    return n;
  std::filesystem::path root = REXCVAR_GET(user_data_root);
  if (root.empty())
    root = rex::filesystem::GetUserFolder() / "wwe12";
  std::ifstream f(root / "superstar_slots.txt");
  std::string line;
  while (std::getline(f, line)) {
    std::string low = line;
    for (char &ch : low)
      ch = char(std::tolower(static_cast<unsigned char>(ch)));
    const size_t eq = low.rfind('=');
    if (eq != std::string::npos && low.substr(0, eq).find("laurinaitis") != std::string::npos)
      return atoi(low.c_str() + eq + 1);
  }
  return 0;
}
}  // namespace

extern "C" REX_FUNC(sub_82A36150) {
  const bool ref_place = On() && g_arm && g_cur >= 0 && g_cur < int(g_rows.size()) &&
                         ctx.r4.u32 == g_rows[size_t(g_cur)].fighters && ctx.r5.u32 == 0;
  if (ref_place) {
    ctx.r5.u64 = 1;
    g_committed = true;
    BD_INFO("[sref] la place {} : genre 0 -> 1 (l'arbitre), appel {:08X}", ctx.r4.u32, u32(ctx.lr));
  }
  const u32 m = ctx.r3.u32, gm_place = ctx.r4.u32 + 1;
  __imp__sub_82A36150(ctx, base);
  const int gm = ref_place ? GmNumber() : 0;
  if (ref_place && gm > 0 && gm < 373 && gm_place < 6 && REXCVAR_GET(wwe12_sref_scene) && REXCVAR_GET(wwe12_sref_replacement)) {
    PPCContext c = ctx;
    c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
    c.r3.u64 = m, c.r4.u64 = gm_place, c.r5.u64 = u32(gm), c.r6.s64 = -1, c.r7.u64 = 0, c.r8.u64 = 2, c.r9.u64 = 1,
    c.r10.u64 = 1;
    __imp__sub_82A37AB0(c, base);
    c = ctx;
    c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
    c.r3.u64 = m, c.r4.u64 = gm_place, c.r5.u64 = 6;
    __imp__sub_82A36150(c, base);
    g_gm_place = int(gm_place);
    BD_INFO("[sref] le manager general (catcheur {}) a la place {}, genre 6 : il attend hors de vue jusqu'a la scene", gm,
            gm_place);
  }
}

// The match set up from a rule: ours only when it comes from the select's commit (sub_828AD3A0, its call returns
// to 0x828AD3D0) of a select set up for a row of ours - never a Universe's or a story's match.
namespace {
void ResetMatchState();  // (below, after the scene's code)
}
extern "C" REX_FUNC(sub_82A39AC0) {
  if (On()) {
    ResetMatchState();
    g_committed = false;
    g_arm = g_cur >= 0 && u32(ctx.lr) == 0x828AD3D0 && g_cur < int(g_rows.size()) && g_rows[size_t(g_cur)].rule == ctx.r4.u32;
    BD_INFO("[sref] match prepare avec la regle {} (options {:08X}, appel {:08X}){}", ctx.r4.u32, ctx.r5.u32, u32(ctx.lr),
            g_arm ? " : avec un arbitre special" : "");
  }
  __imp__sub_82A39AC0(ctx, base);
}

// ---- the menu: SPECIALTY > SPECIAL REFEREE > ONE ON ONE / TWO ON TWO / TRIPLE THREAT / FATAL 4-WAY > stipulations ----
// (as WWE '13's own menu, user's captures of 2026-11-16). The menus' table (wwe12_match_modes.cpp: 0x74-byte
// big-endian records from + 0x28; header + 4 the records, + 8 those shown) is handed to the game's parser with our
// records added: a sub-menu is a copy of the game's FREE BRAWL sub-menu record, a stipulation a copy of the game's
// own row of that match, found by its text and its parent - so each plays what the game's row plays. A page's
// records follow each other and end with a "last" one (kind bit 2); a record's + 22 is its depth, + 64 bit 0
// (of its first 16 bits) hides it.
// The game finds an entry from its id by a binary search (sub_82DE8718, and the same search inlined elsewhere): the
// ids must go up along the table, as the game's do (a record's id is its index). A page's records must also follow
// each other (its page table: first entry and count, sub_82DE8F88). So a row added inside a page cannot have a new
// id (user's test of 2026-11-16: SPECIAL REFEREE after SPECIALTY's last row with the id 226 - B did not go back
// from its sub-menu, its parent was not found). It takes the id of the row before it instead, a match row that has
// no sub-menu of its own (CHAMPIONSHIP SCRAMBLE): the search then finds one of the two, both on SPECIALTY's page,
// and the sub-menus are found from their parent's id as before. Our pages go at the table's end, ids above all.
// SPECIAL REFEREE is SPECIALTY's second row, as in WWE '13.
namespace {
constexpr u32 kRec = 0x74, kHead = 0x28;
constexpr u32 kLabel = 0x00, kDepth = 0x16, kPage = 0x18, kId = 0x1C, kParent = 0x38, kKind = 0x3C, kHide = 0x40, kLink = 0x54,
              kRowRule = 0x5C;
u32 BE(const u8 *p) { return (u32(p[0]) << 24) | (u32(p[1]) << 16) | (u32(p[2]) << 8) | p[3]; }
void PutBE(u8 *p, u32 v) {
  p[0] = u8(v >> 24);
  p[1] = u8(v >> 16);
  p[2] = u8(v >> 8);
  p[3] = u8(v);
}
}  // namespace

bool Wwe12SrefOn() { return On(); }
bool Wwe12SrefWanted() { return REXCVAR_GET(wwe12_sref); }  // (asked before the game's memory is there)
bool Wwe12SrefWantsMic() {
  return On() && REXCVAR_GET(wwe12_sref_replacement) && REXCVAR_GET(wwe12_sref_scene) && (Match() || g_cur >= 0);
}

// ---- the referee's entrance ----
// The entrances are built by sub_82A3F828(order) from the participants (sub_82A3F0D8: a fighter - kind 0 - is a
// member of his side's entrance, kinds 2 and 7 its manager, any other kind is left out: the referee had none).
// The order record's first six words are each side's rank (-1: none). As WWE '13 (sub_82292268, read 2026-11-16):
// the referee's side has the rank 0 - he comes first - and the others follow in the participants' order; he is a
// fighter's kind for the time of the call. Only for a match of ours being set up (the select's commit calls this
// after the kinds are set; the game's two other callers run their own setups).
extern "C" REX_FUNC(sub_82A3F828) {
  const u32 order = ctx.r3.u32;
  u32 ref = 0;
  if (On() && Match() && REXCVAR_GET(wwe12_sref_entrance) && order) {
    u32 part[6] = {};
    for (u32 n = 0; n < 6; ++n) {
      PPCContext c = ctx;
      c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
      c.r3.u64 = n;
      __imp__sub_823F8270(c, base);
      __imp__sub_8267CD30(c, base);
      part[n] = c.r3.u32;
      if (!ref && part[n] && bd::mem::try_load<u32>(part[n] + 8) == 1 && bd::mem::try_load<u32>(part[n] + 16) != 51200)
        ref = part[n];
    }
    const u32 side = ref ? bd::mem::try_load<u8>(ref + 13) : 255;
    if (ref && side < 6) {
      int rank[6] = {-1, -1, -1, -1, -1, -1};
      rank[side] = 0;
      int next = 1;
      for (u32 n = 0; n < 6; ++n) {
        if (!part[n] || part[n] == ref || bd::mem::try_load<u32>(part[n] + 16) == 51200 ||
            bd::mem::try_load<u32>(part[n] + 8) == 6)
          continue;  // (nor the General Manager, who waits out of sight)
        const u32 s = bd::mem::try_load<u8>(part[n] + 13);
        if (s < 6 && rank[s] < 0)
          rank[s] = next++;
      }
      std::string was, now;
      for (u32 k = 0; k < 6; ++k) {
        was += fmt::format(" {}", int32_t(bd::mem::try_load<u32>(order + k * 4)));
        now += fmt::format(" {}", rank[k]);
        bd::mem::store<u32>(order + k * 4, u32(rank[k]));
      }
      BD_INFO("[sref] entrees : l'arbitre (cote {}) entre le premier ; rangs par cote{} ->{}", side, was, now);
      bd::mem::store<u32>(ref + 8, 0);
    } else {
      ref = 0;
    }
  }
  __imp__sub_82A3F828(ctx, base);
  if (ref)
    bd::mem::store<u32>(ref + 8, 1);
}

// ---- the character select: the referee's place reads SPECIAL REFEREE ----
// A place's label ("COM", "1P"... texts 40355, 40357 + pad) is set by the place panel's two functions (r3 = the
// panel, its + 284 the place), through the game's text getter. For the place after the fighters of a select of
// ours, that text is the game's own "SPECIAL REFEREE" (40775), in the player's language (asked from
// wwe12_match_modes.cpp's text hook while one of the two functions runs).
namespace {
thread_local bool t_label = false;
bool RefPanel(u32 panel) {
  return On() && panel && g_cur >= 0 && g_cur < int(g_rows.size()) &&
         bd::mem::try_load<u32>(panel + 284) == g_rows[size_t(g_cur)].fighters;
}
}  // namespace
extern "C" REX_FUNC(sub_82596848) {
  t_label = RefPanel(ctx.r3.u32);
  __imp__sub_82596848(ctx, base);
  t_label = false;
}
extern "C" REX_FUNC(sub_825962D0) {
  t_label = RefPanel(ctx.r3.u32);
  __imp__sub_825962D0(ctx, base);
  t_label = false;
}
bool Wwe12SrefLabel(uint32_t id) { return t_label && (id == 40355 || (id >= 40357 && id <= 40360)); }

// ---- 1 on 1: the referee in the middle (as WWE '13's select and match card; user, 2026-11-16) ----
// The fighters stay the places 0 and 1 and the referee the place 2: the HUD (slots by actor number) and several
// functions of the match take the actors 0 and 1 as the two fighters (read 2026-11-16). Only what is shown moves.
// - The select's screen is laid out by sub_8256CEB0 from a template: 0x82025030 + 324 x the select entry's layout
//   (+ 1588); a template = a 12-byte header and six 52-byte place blocks. Layout 6 (three people): the place 0 in
//   the centre, 1 on the left, 2 on the right. A copy of it with the blocks turned (place 0 left, 1 right, 2 centre)
//   is given for the time of that call, through a layout number that leads to our copy; the screen keeps the
//   pointer (its + 30268), which its other readers use too.
// - The pads' moves (sub_82568C78, table in r7: 7 places x 4 directions; tables of three people at 0x82024D38 for
//   the pads 0-1 and 0x82024D54 for 2-3) are the game's own with the places renamed the same way.
// - The match card (sub_82573BA0(card, place, 33 = the three-people card, ...)): the places 1 and 2 swapped.
namespace {
constexpr u32 kTemplates = 0x82025030, kTemplate = 324, kLayout3 = 6;
u32 g_template = 0, g_moves = 0;  // our copies (guest memory)
bool Middle() {
  return On() && REXCVAR_GET(wwe12_sref_middle) && g_cur >= 0 && g_cur < int(g_rows.size()) &&
         g_rows[size_t(g_cur)].fighters == 2;
}
}  // namespace

extern "C" REX_FUNC(sub_8256CEB0) {
  const u32 screen = ctx.r3.u32, m = bd::mem::try_load<u32>(0x8324DEFC);
  const u32 e = m ? m + bd::mem::try_load<u32>(m + 15960) * 1596 : 0;
  if (!Middle() || !screen || !e || bd::mem::try_load<u32>(screen + 30264) != 0 || bd::mem::try_load<u32>(e + 1588) != kLayout3) {
    __imp__sub_8256CEB0(ctx, base);
    return;
  }
  if (!g_template) {
    const u32 raw = REX_KERNEL_MEMORY()->SystemHeapAlloc(kTemplate * 3, 0x10);
    if (raw) {
      // (an address the game's "templates + 324 x layout" reaches)
      const u32 at = raw + (kTemplate - (raw - kTemplates) % kTemplate) % kTemplate;
      const u8 *src = bd::mem::at<u8>(kTemplates + kLayout3 * kTemplate);
      u8 *dst = bd::mem::at<u8>(at);
      std::memcpy(dst, src, kTemplate);
      std::memcpy(dst + 12 + 52 * 0, src + 12 + 52 * 1, 52);  // the place 0: on the left
      std::memcpy(dst + 12 + 52 * 1, src + 12 + 52 * 2, 52);  // the place 1: on the right
      std::memcpy(dst + 12 + 52 * 2, src + 12 + 52 * 0, 52);  // the place 2, the referee: in the centre
      g_template = at;
    }
  }
  if (!g_template) {
    __imp__sub_8256CEB0(ctx, base);
    return;
  }
  bd::mem::store<u32>(e + 1588, (g_template - kTemplates) / kTemplate);
  __imp__sub_8256CEB0(ctx, base);
  bd::mem::store<u32>(e + 1588, kLayout3);
  static std::atomic<int> s_lines{0};
  if (s_lines.fetch_add(1) < 10)
    BD_INFO("[sref] ecran de choix : l'arbitre au milieu (gabarit {:08X}, l'ecran garde {:08X})", g_template,
            bd::mem::try_load<u32>(screen + 30268));
}

extern "C" REX_FUNC(sub_82568C78) {
  if (Middle() && (ctx.r7.u32 == 0x82024D38 || ctx.r7.u32 == 0x82024D54)) {
    if (!g_moves) {
      g_moves = REX_KERNEL_MEMORY()->SystemHeapAlloc(64, 0x10);
      if (g_moves) {
        static const int8_t kA[28] = {2, 6, 1, 1, 2, 6, 0, 0, 6, 0, 0, 1, -1, -1, -1, -1, -1, -1,
                                      -1, -1, -1, -1, -1, -1, 0, 2, -1, -1};
        static const int8_t kB[28] = {2, 6, 1, 1, 2, 6, 0, 0, 6, 1, 0, 1, -1, -1, -1, -1, -1, -1,
                                      -1, -1, -1, -1, -1, -1, 1, 2, -1, -1};
        std::memcpy(bd::mem::at<u8>(g_moves), kA, 28);
        std::memcpy(bd::mem::at<u8>(g_moves + 32), kB, 28);
      }
    }
    if (g_moves)
      ctx.r7.u64 = ctx.r7.u32 == 0x82024D38 ? g_moves : g_moves + 32;
  }
  __imp__sub_82568C78(ctx, base);
}

extern "C" REX_FUNC(sub_82573BA0) {
  // (r6 = the slot's widget of sub_82573CC8's card of three people, r4 = the place it shows. Seen on screen
  // (user's tests of 2026-11-16): widget 39 is the MIDDLE slot (the game puts the place 0 there), 45 the left one,
  // 51 the right one. So the referee (place 2) goes to 39, the place 0 to 45 and the place 1 stays on the right.)
  if (Middle() && ctx.r4.u32 <= 2 && (ctx.r6.u32 == 39 || ctx.r6.u32 == 45 || ctx.r6.u32 == 51)) {
    const u32 want = ctx.r6.u32 == 39 ? 2 : ctx.r6.u32 == 45 ? 0 : 1;
    static std::atomic<int> s_lines{0};
    if (s_lines.fetch_add(1) < 10)
      BD_INFO("[sref] carte du match : la case {} montre la place {} (au lieu de {})", ctx.r6.u32, want, ctx.r4.u32);
    ctx.r4.u64 = want;
  }
  __imp__sub_82573BA0(ctx, base);
}

void Wwe12SrefMenu(PPCContext &ctx) {
  if (!On())
    return;
  // (the menus are read again when a match is over: the rule's record is the game's own again - a demo or a
  // Universe match started without a choice in these menus must not find the referee's place in it)
  Restore();
  g_sel = -1;
  const u32 table = ctx.r4.u32, size = ctx.r5.u32;
  if (!table || size < kHead + kRec)
    return;
  const u8 *t = bd::mem::at<u8>(table);
  const u32 total = BE(t + 4), shown = BE(t + 8);
  const u32 n = std::min(total, (size - kHead) / kRec);
  if (!n || shown > total)
    return;
  auto rec = [&](u32 i) { return t + kHead + i * kRec; };
  auto hidden = [&](u32 i) { return (rec(i)[kHide + 1] & 1) != 0; };
  auto find = [&](u32 label, u32 parent) {
    for (u32 i = 0; i < n; ++i)
      if (!hidden(i) && BE(rec(i) + kLabel) == label && BE(rec(i) + kParent) == parent)
        return int(i);
    return -1;
  };
  u32 max_id = 0, max_page = 0;
  int play = -1;
  std::vector<u32> game_rows;
  for (u32 i = 0; i < n; ++i) {
    if (!hidden(i) && BE(rec(i) + kLink) == 0x7D0)
      game_rows.push_back(BE(rec(i) + kId));
    max_id = std::max(max_id, BE(rec(i) + kId));
    max_page = std::max(max_page, BE(rec(i) + kPage));
    if (play < 0 && !hidden(i) && BE(rec(i) + kLabel) == 41001)
      play = int(i);
  }
  if (play < 0)
    return;
  const u32 pid = BE(rec(u32(play)) + kId);
  const int spec = find(41187, pid);
  if (spec < 0)
    return;
  const u32 sid = BE(rec(u32(spec)) + kId);
  const int brawl = find(41053, sid);  // (a sub-menu's record, the model of ours)
  int last = -1;                       // the row SPECIAL REFEREE goes after: SPECIALTY's first match row, not its last
  for (u32 i = 0; i < n && last < 0; ++i)
    if (!hidden(i) && BE(rec(i) + kParent) == sid && BE(rec(i) + kLink) == 0x7D0 && BE(rec(i) + kKind) == 5)
      last = int(i);
  for (u32 i = 0; i < n && last >= 0; ++i)
    if (BE(rec(i) + kParent) == BE(rec(u32(last)) + kId))
      last = -1;  // (it has rows of its own: its id cannot be shared)
  if (brawl < 0 || last < 0) {
    BD_INFO("[sref] menu : SPECIALTY introuvable ou inattendu, rien ajoute");
    return;
  }
  auto child = [&](int parent_rec, u32 label) { return parent_rec < 0 ? -1 : find(label, BE(rec(u32(parent_rec)) + kId)); };
  const int one = find(41009, pid), two = find(41010, pid), three = find(41011, pid), four = find(41012, pid);
  const int tornado = child(two, 41056);
  struct Leaf {
    int from;        // the game's row copied
    u32 label, rule; // (rule 0xFFFFFFFF: the row's own)
  };
  struct Group {
    u32 label, fighters;
    bool tag;
    std::vector<Leaf> leaves;
  };
  std::vector<Group> groups = {
      {41009, 2, false,
       // NORMAL MATCH: ONE ON ONE's own rule, with its disqualifications and count-out (user, 2026-11-16: "en 1v1
       // il devrait y avoir les regles classiques" - the game's SPECIAL REFEREE rule 83, played first, has none)
       {{child(one, 41041), 41041, 0xFFFFFFFF},
        {child(one, 41054), 41054, 0xFFFFFFFF},
        {child(one, 41045), 41045, 0xFFFFFFFF},
        {child(one, 41046), 41046, 0xFFFFFFFF},
        {child(one, 41048), 41048, 0xFFFFFFFF},
        {child(one, 41049), 41049, 0xFFFFFFFF}}},
      {41010, 4, true,
       {{child(two, 41055), 41133, 0xFFFFFFFF},  // TAG TEAM, shown as NORMAL
        {child(tornado, 41065), 41065, 0xFFFFFFFF},
        {child(tornado, 41059), 41059, 0xFFFFFFFF}}},
      {41011, 3, false,
       {{child(three, 41066), 41066, 0xFFFFFFFF}, {child(three, 41073), 41073, 0xFFFFFFFF}, {child(three, 41068), 41068, 0xFFFFFFFF}}},
      {41012, 4, false,
       {{child(four, 41074), 41074, 0xFFFFFFFF}, {child(four, 41082), 41082, 0xFFFFFFFF}, {child(four, 41077), 41077, 0xFFFFFFFF}}},
  };
  for (Group &g : groups)
    g.leaves.erase(std::remove_if(g.leaves.begin(), g.leaves.end(),
                                  [&](const Leaf &l) { return l.from < 0 || BE(rec(u32(l.from)) + kLink) != 0x7D0; }),
                   g.leaves.end());
  groups.erase(std::remove_if(groups.begin(), groups.end(), [](const Group &g) { return g.leaves.empty(); }), groups.end());
  if (groups.empty())
    return;

  std::vector<u8> out(t, t + kHead + (u32(last) + 1) * kRec);  // up to the row before ours
  std::vector<u8> tail(t + kHead + (u32(last) + 1) * kRec, t + size);
  u32 added = 0;
  auto add = [&](std::vector<u8> &to, int from, u32 label, u32 id, u32 parent, u32 page, u32 depth, u32 kind) {
    const size_t at = to.size();
    to.insert(to.end(), rec(u32(from)), rec(u32(from)) + kRec);
    u8 *r = to.data() + at;
    PutBE(r + kLabel, label);
    PutBE(r + kId, id);
    PutBE(r + kParent, parent);
    PutBE(r + kPage, page);
    PutBE(r + kKind, kind);
    r[kDepth] = u8(depth);
    r[kHide + 1] &= u8(~1u);
    ++added;
    return at;
  };
  const u32 depth = rec(u32(last))[kDepth];
  // SPECIAL REFEREE, with the id of the row before it (see above)
  const u32 sr = BE(rec(u32(last)) + kId);
  add(out, brawl, 40775, sr, sid, BE(rec(u32(last)) + kPage), depth, 1);
  // (the rest of the game's table, then our pages at its end)
  out.insert(out.end(), tail.begin(), tail.begin() + (n - u32(last) - 1) * kRec);
  const size_t extra = tail.size() - (n - u32(last) - 1) * kRec;  // (bytes after the last record, kept at the end)
  std::vector<Row> rows;
  std::vector<u32> nodes = {sr};
  const u32 types_page = ++max_page;
  std::vector<u32> ids;
  for (size_t g = 0; g < groups.size(); ++g) {
    ids.push_back(++max_id);
    nodes.push_back(ids.back());
    add(out, brawl, groups[g].label, ids.back(), sr, types_page, depth + 1, g + 1 == groups.size() ? 3 : 1);
  }
  for (size_t g = 0; g < groups.size(); ++g) {
    const u32 page = ++max_page;
    for (size_t k = 0; k < groups[g].leaves.size(); ++k) {
      const Leaf &l = groups[g].leaves[k];
      const u32 id = ++max_id;
      const size_t at = add(out, l.from, l.label, id, ids[g], page, depth + 2, k + 1 == groups[g].leaves.size() ? 7 : 5);
      if (l.rule != 0xFFFFFFFF)
        PutBE(out.data() + at + kRowRule, l.rule);
      rows.push_back(Row{id, BE(out.data() + at + kRowRule), groups[g].fighters, groups[g].tag});
    }
  }
  out.insert(out.end(), tail.end() - extra, tail.end());
  PutBE(out.data() + 4, total + added);
  PutBE(out.data() + 8, shown + added);
  const u32 copy = REX_KERNEL_MEMORY()->SystemHeapAlloc(u32(out.size()), 0x10);
  if (!copy)
    return;
  std::memcpy(bd::mem::at<u8>(copy), out.data(), out.size());
  g_rows = rows;
  g_nodes = nodes;
  g_game_rows = game_rows;
  g_sel = -1;
  std::string list;
  for (const Row &r : rows)
    list += fmt::format(" {}:{}/{}", r.id, r.rule, r.fighters);
  BD_INFO("[sref] menu : SPECIAL REFEREE ajoute a SPECIALTY (id {}, {} lignes ajoutees, pages {} a {}) ; lignes id:regle/combattants :{}",
          sr, added, types_page, max_page, list);
  ctx.r4.u64 = copy;
  ctx.r5.u64 = u32(out.size());
}

// The menus' "decide" (through wwe12_match_modes.cpp): the chosen entry's id. A row of ours: the match is a refereed
// one (true). Another match's row of the game, or a choice in a higher menu: the rule's record put back.
// (Other "decides" - a screen's own choices on the way to the match - change nothing.)
bool Wwe12SrefDecide(uint32_t id, uint32_t parent, uint32_t label) {
  if (!On())
    return false;
  for (size_t i = 0; i < g_rows.size(); ++i)
    if (g_rows[i].id == id && id != 0) {
      g_sel = int(i);
      BD_INFO("[sref] choix du menu : ligne {} (parent {}, texte {}) -> regle {} avec un arbitre special, {} combattants", id,
              parent, label, g_rows[i].rule, g_rows[i].fighters);
      return true;
    }
  if (parent >= 8 && std::find(g_game_rows.begin(), g_game_rows.end(), id) == g_game_rows.end())
    return false;
  if (g_sel.exchange(-1) >= 0)
    BD_INFO("[sref] choix du menu {} (parent {}) : plus de match avec un arbitre special", id, parent);
  Restore();
  return false;
}

extern "C" REX_FUNC(sub_827A1BF0) {
  if (On()) {
    Layout();
    static std::atomic<int> s_lines{0};
    if (s_lines.fetch_add(1) < 30)
      BD_INFO("[sref] ecran de choix : gabarit demande pour la regle {}", ctx.r5.s32);
  }
  __imp__sub_827A1BF0(ctx, base);
}

// From the XInputGetState hook (through wwe12_iquit.cpp): the buttons the game must not see. While the judge waits
// for a count, the referee's A is ours.
u16 Wwe12SrefPad(u32 user, u16 buttons) {
  if (user >= 4)
    return 0;
  const u16 before = g_prev[user].exchange(buttons);
  if (!On() || !Match() || g_pad.load() != int(user))
    return 0;
  // (the match stands still - the pause menu, a replay: its buttons are the game's. User's test of 2026-11-16: A did
  // not work in the pause menu.)
  if (NowMs() - g_live_ms.load() > 250)
    return 0;
  if (buttons & ~before & 0x2000)
    g_b = true;  // (B: he stops checking a hold, as WWE '13's cancel)
  if (buttons & ~before & 0x8000)
    g_y = true;  // (Y: a disqualification pronounced, the screw job)
  const u32 st = g_state.load();
  const bool counting = st == 1 || st == 3 || st == 4 || st == 5;
  // (WWE '13: the special referee's grapple button is always a referee's action, never a grapple of his own)
  // (WWE '13: from close to a fighter, 200 cm or less, his grapple button is a real grapple - the tick says when)
  const bool ours = g_out != kOutNone ? true
                    : counting      ? REXCVAR_GET(wwe12_sref_own_count)
                                    : REXCVAR_GET(wwe12_sref_actions) && !g_a_game.load();
  if (ours && (buttons & ~before & kPadA))
    g_press = true;
  return u16((ours ? kPadA : 0) | (g_want_y.load() ? 0x8000 : 0));
}

// ---- the gauge on the screen ----
// A bar at the bottom of the screen, WWE '13's blue, a striped shirt at its end (drawn here: no image of WWE '13).
namespace {
class SrefOverlay final : public rex::ui::ImGuiDialog {
 public:
  explicit SrefOverlay(rex::ui::ImGuiDrawer *drawer) : ImGuiDialog(drawer) {}

 protected:
  void OnClose() override { g_hud_shown = false; }
  void OnDraw(ImGuiIO &io) override {
    const int64_t now = NowMs(), idle = now - g_tick_ms.load();
    if (!g_hud_want.load() || idle > 5000) {
      Close();  // (no match of ours: the drawer stops drawing ImGui every frame)
      return;
    }
    if (idle > 250 || g_scene_on.load())
      return;  // (the pause menu, a replay: nothing of ours over them)
    const float k = std::max(0.6f, io.DisplaySize.y / 1080.0f);
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    ImFont *font = ImGui::GetFont();
    // (on the referee, above his body, when he is on screen; else at the bottom of the screen)
    const float px = g_sx.load(), py = g_sy.load();
    const bool on_him = px >= 0.f && py >= 0.f && !g_ejected.load();
    const float w = (on_him ? 150 : 260) * k, h = (on_him ? 10 : 16) * k;
    const float x = on_him ? std::clamp(px * io.DisplaySize.x - w * 0.5f, 8 * k, io.DisplaySize.x - w - 50 * k)
                           : io.DisplaySize.x * 0.5f - w * 0.5f;
    const float y = on_him ? std::clamp(py * io.DisplaySize.y + 16 * k, 40 * k, io.DisplaySize.y - 40 * k)
                           : io.DisplaySize.y * 0.905f;
    if (g_ejected.load()) {
      if (now - g_ejected_ms.load() > 6000) {
        g_hud_want = false;
        return;
      }
      const char *msg = "YOU ARE NO LONGER THE REFEREE";
      const float ms = 30.f * k;
      const ImVec2 sz = font->CalcTextSizeA(ms, FLT_MAX, 0.f, msg);
      dl->AddText(font, ms, ImVec2(io.DisplaySize.x * 0.5f - sz.x * 0.5f + 2 * k, y - 10 * k + 2 * k), IM_COL32(0, 0, 0, 220), msg);
      dl->AddText(font, ms, ImVec2(io.DisplaySize.x * 0.5f - sz.x * 0.5f, y - 10 * k), IM_COL32(255, 70, 70, 255), msg);
      return;
    }
    const float v = std::clamp(g_gauge.load(), 0.f, 1.f);
    // WWE '13's own parts when the exe has them (its main HUD: iquit13.bin since 2026-11-16, the user's decision),
    // laid out as its sub_823381E0 does at 720p: the back 68 x 6 and the frame 88 x 16 on the same centre, the fill
    // (tinted 10, 80, 190) from the back's left edge, the striped shirt 20 x 20 at + 46. Full: the fill pulses blue
    // and white; a quarter or less: red and white. Under it, what his buttons do now.
    {
      const float u = io.DisplaySize.y / 720.f * 1.35f;
      const float cx = std::clamp(on_him ? px * io.DisplaySize.x : io.DisplaySize.x * 0.5f, 60 * u, io.DisplaySize.x - 70 * u);
      const float cy = y + 8 * u;
      if (Wwe12HudRefImg(nullptr, 1, 0, 0, 0, 0, 0, 0.f)) {
        const bool pulse = (now / 200) % 2 == 0;
        const ImU32 fill = v >= 1.f ? (pulse ? IM_COL32(10, 80, 190, 255) : IM_COL32(255, 255, 255, 255))
                           : v <= 0.25f ? (pulse ? IM_COL32(234, 10, 6, 255) : IM_COL32(255, 255, 255, 255))
                                        : IM_COL32(10, 80, 190, 255);
        Wwe12HudRefImg(dl, 2, cx - 34 * u, cy - 3 * u, 68 * u, 6 * u, IM_COL32_WHITE, 1.f);
        Wwe12HudRefImg(dl, 3, cx - 34 * u, cy - 3 * u, 68 * u, 6 * u, fill, v);
        Wwe12HudRefImg(dl, 1, cx - 44 * u, cy - 8 * u, 88 * u, 16 * u, IM_COL32_WHITE, 1.f);
        Wwe12HudRefImg(dl, 0, cx + 36 * u, cy - 10 * u, 20 * u, 20 * u, IM_COL32_WHITE, 1.f);
        static const char *const kPrompt[6] = {"",           "A  COUNT",     "A  CHECK THE SUBMISSION",
                                               "Y  DISQUALIFY", "Y  SCREW JOB", "Y  FINISHER"};
        const int p = g_prompt.load();
        // (WWE '13's own: the button and the word, in the player's language - COUNT, CHECK, DQ, SCREW JOB)
        static constexpr int kButton[5] = {0, 1, 1, 0, 0}, kRow[5] = {0, 1, 0, 3, 4};
        if (p == 5 && Wwe12HudRefPrompt(nullptr, 0, -1, 0, 0, 1.f) > 0.f) {
          // (WWE '13's words have no FINISHER: its Y button, then the word in white with a dark shadow, as its own)
          const float h = 16 * u, bw = Wwe12HudRefPrompt(nullptr, 0, -1, 0, 0, h), ms = 17.f * u;
          const ImVec2 sz = font->CalcTextSizeA(ms, FLT_MAX, 0.f, "FINISHER");
          const float x0 = cx - (bw + h * 0.2f + sz.x) * 0.5f, yy = cy + 18 * u;
          Wwe12HudRefPrompt(dl, 0, -1, x0, yy, h);
          for (float d : {2.f, 1.f})
            dl->AddText(font, ms, ImVec2(x0 + bw + h * 0.2f + d * u, yy - sz.y * 0.5f + d * u), IM_COL32(0, 0, 0, 200), "FINISHER");
          dl->AddText(font, ms, ImVec2(x0 + bw + h * 0.2f, yy - sz.y * 0.5f), IM_COL32(255, 255, 255, 255), "FINISHER");
        } else if (p > 0 && p < 5 && Wwe12HudRefPrompt(nullptr, kButton[p], -1, 0, 0, 1.f) > 0.f) {
          const float h = 16 * u, w = Wwe12HudRefPrompt(nullptr, kButton[p], -1, 0, 0, h) + h * 0.1f +
                                      h * 0.95f * 256.f / 25.6f * 0.45f;  // (about: the word's ink is left aligned)
          Wwe12HudRefPrompt(dl, kButton[p], kRow[p], cx - w * 0.5f, cy + 18 * u, h);
        } else if (p > 0 && p < 6) {
          const float ms = 15.f * u;
          const ImVec2 sz = font->CalcTextSizeA(ms, FLT_MAX, 0.f, kPrompt[p]);
          dl->AddText(font, ms, ImVec2(cx - sz.x * 0.5f + 1 * u, cy + 11 * u), IM_COL32(0, 0, 0, 230), kPrompt[p]);
          dl->AddText(font, ms, ImVec2(cx - sz.x * 0.5f, cy + 10 * u), IM_COL32(255, 235, 150, 255), kPrompt[p]);
        }
        return;
      }
    }
    dl->AddRectFilled(ImVec2(x - 3 * k, y - 3 * k), ImVec2(x + w + 3 * k, y + h + 3 * k), IM_COL32(225, 225, 225, 255), 3 * k);
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(15, 15, 20, 255));
    const ImU32 fill = v >= 1.f ? IM_COL32(255, 205, 40, 255) : v < 0.2f ? IM_COL32(220, 40, 40, 255) : IM_COL32(40, 90, 220, 255);
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w * v, y + h), fill);
    // the striped shirt
    const float sx = x + w + 8 * k, sw = (on_him ? 20 : 30) * k, sh = sw, sy = y + h * 0.5f - sh * 0.5f;
    dl->AddRectFilled(ImVec2(sx - 2 * k, sy - 2 * k), ImVec2(sx + sw + 2 * k, sy + sh + 2 * k), IM_COL32(15, 15, 20, 255), 3 * k);
    dl->AddRectFilled(ImVec2(sx, sy), ImVec2(sx + sw, sy + sh), IM_COL32(245, 245, 245, 255));
    for (int i = 0; i < 3; ++i)
      dl->AddRectFilled(ImVec2(sx + sw * (0.1f + 0.3f * float(i)), sy), ImVec2(sx + sw * (0.25f + 0.3f * float(i)), sy + sh),
                        IM_COL32(15, 15, 20, 255));
    if (v >= 1.f) {
      const char *msg = "Y  SCREW JOB (DURING A SUBMISSION)";
      const float ms = 20.f * k;
      const ImVec2 sz = font->CalcTextSizeA(ms, FLT_MAX, 0.f, msg);
      dl->AddText(font, ms, ImVec2(io.DisplaySize.x * 0.5f - sz.x * 0.5f + 1 * k, y - 28 * k + 1 * k), IM_COL32(0, 0, 0, 220), msg);
      dl->AddText(font, ms, ImVec2(io.DisplaySize.x * 0.5f - sz.x * 0.5f, y - 28 * k), IM_COL32(255, 220, 90, 255), msg);
    }
  }
};
void ShowHud() {
  if (rex::ui::ImGuiDrawer *d = Wwe12OverlayDrawer())
    new SrefOverlay(d);  // deletes itself when no match of ours runs
  else
    g_hud_shown = false;
}
}  // namespace

// ---- his attack on a fighter that connects ----
// The collision callback sub_822A8360 resolves a hit with sub_822A7600(attack object) at 0x822A8E14, then tests
// the object's + 288 (0: not guarded, not reversed) - where WWE '13 charges the gauge (its sub_82634AC0).
extern "C" REX_FUNC(sub_822A7600) {
  const u32 obj = ctx.r3.u32, from = u32(ctx.lr);
  __imp__sub_822A7600(ctx, base);
  if (!On() || !Match() || from != 0x822A8E18 || !obj || ctx.r3.u32 == 0 || g_pad.load() < 0)
    return;
  const u32 by = bd::mem::try_load<u32>(obj + 256), hit = bd::mem::try_load<u32>(obj + 272);
  if (!by || !hit || by == hit || bd::mem::try_load<u32>(obj + 288) != 0)
    return;
  const u32 referee = Referee();
  const int64_t now = NowMs();
  // (a fighter's blow on the referee: a foul, his to pronounce - WWE '13's sub_825B5198)
  if (hit == referee && bd::mem::try_load<u32>(by + 3428) == 0) {
    if (g_foul_by.exchange(by) != by || now - g_foul_ms.load() > 5000)
      BD_INFO("[sref] faute : l'acteur {:08X} frappe l'arbitre", by);
    g_foul_ms = now;
    return;
  }
  if (by != referee || bd::mem::try_load<u32>(hit + 3428) != 0)
    return;
  if (g_hit_victim.exchange(hit) == hit && now - g_hit_ms.load() < 700)
    return;  // (the same blow seen again)
  g_hit_ms = now;
  g_hit = true;
}

// ---- his actions, as WWE '13 (its script's Function_PadAction_SpecialRefree and natives, read 2026-11-16) ----
// WWE '13's motion 20NN is here 13115 + NN - 20 (iquit13.bin). Times are WWE '13's frames at 60 a second.
// A, the judge waiting for nothing:
//   - a hold is on: he checks it (2037 for a standing hold, 2038 on the mat: block 0, then 1 again and again, 2
//     when the hold is over). A tap-out while he checks: block 10 and the match is over. (When he does not check,
//     WWE '12's own referee code still takes the tap - WWE '13 would not: to be done.)
//   - a fighter within reach and 0.1 of gauge: the caution, 2020. One second in, if the fighter stands free: a
//     strike against him (three, a minute each). The third: he argues (2024) and the referee answers a second later
//     (2026, 0.5 of gauge, the strikes gone); else he complies (2021 with the referee behind him, else 2022), and
//     it costs 0.1 since he was in his right (in a match without partners every fighter is).
//   - else: 2049, nothing to do.
// Y: a fighter hit him within the last five seconds and the match has disqualifications: he pronounces it (2040;
//   lying on his back 2041, on his front 2042) and the match is over. A hold is on and the gauge is full: the screw
//   job, 2029 - at its frame 100 the man applying the hold wins by submission.
// Not WWE '13's yet: the shove before a caution from close (2027 / 2028, two men), the weapon dropped, the rope
// breaks he grants, his taunts 2045-2048.
namespace {
constexpr u32 W(u32 n) { return 13115 + (n - 2020); }
bool IdleAnim(u16 a) { return (a >= 20000 && a <= 20598) || a == 210; }
bool DownAnim(u16 a) { return a <= 30 || (a >= 440 && a <= 480); }
bool Spot(u32 actor, float &x, float &z) {
  const u32 p = actor ? bd::mem::try_load<u32>(actor + 768) : 0;
  if (!p)
    return false;
  x = LoadF(p), z = LoadF(p + 8);
  return std::isfinite(x) && std::isfinite(z);
}
void Face(u32 actor, u32 to) {  // (an actor faces (-sin h, -cos h): seen in the I Quit's study)
  float ax, az, bx, bz;
  if (!Spot(actor, ax, az) || !Spot(to, bx, bz))
    return;
  const float h = std::atan2(-(bx - ax), -(bz - az));
  u32 bits;
  std::memcpy(&bits, &h, 4);
  bd::mem::store<u32>(actor + 740, bits);
}
int IndexOf(u32 actor) {
  for (u32 k = 0; k < 6; ++k)
    if (actor && Actor(k) == actor)
      return int(k);
  return -1;
}
// a motion of ours on an actor: asked from the queue, played once read, then for its length; `to` after it
int Step(const PPCContext &ctx, u8 *base, Timed &m, double dt) {  // 0 none, 1 running, 2 just over, 3 lost
  if (!m.anim)
    return 0;
  if (m.waiting) {
    if (Wwe12MotionReady(ctx, base, m.anim)) {
      Play(ctx, base, m.actor, m.anim, m.x);
      m.waiting = false;
      return 1;
    }
    if ((m.waited += dt) > 4.0) {
      BD_INFO("[sref] motion {} pas prete apres 4 s : laissee", m.anim);
      m = {};
      return 3;
    }
    return 1;
  }
  if (Anim(m.actor) != u16(m.anim)) {  // (the game gave him another motion: a blow, a move)
    m = {};
    return 3;
  }
  m.left -= dt;
  if (m.left > 0)
    return 1;
  if (m.left > -0.25)
    return 1;  // (a quarter of a second for the game to give him his next motion)
  const u32 actor = m.actor, to = m.to;
  m = {};
  if (to)
    Play(ctx, base, actor, to, 0);
  else if (actor == Referee() && Anim(actor) >= 13115 && Anim(actor) < 13150)
    Play(ctx, base, actor, g_ref_idle, 0);  // (still in it: standing again)
  return 2;
}
void Start(Timed &m, u32 actor, u32 anim, u32 x, double frames, u32 to) {
  m = {};
  m.actor = actor, m.anim = anim, m.x = x, m.to = to, m.left = frames / 60.0, m.waiting = true;
}
bool Hold(u32 ref, u32 &by, u32 &held) {
  for (u32 i = 0; i < 6; ++i) {
    const u32 a = Actor(i), h = a ? bd::mem::try_load<u32>(a + 3404) : 0;
    if (!h || bd::mem::try_load<u32>(h + 32) != 2)
      continue;
    by = bd::mem::try_load<u32>(h + 36), held = bd::mem::try_load<u32>(h + 40);
    if (by && held && by != ref && held != ref && IndexOf(by) >= 0 && IndexOf(held) >= 0)
      return true;
  }
  return false;
}
void EndMatch(const PPCContext &ctx, u8 *base, u32 loser, u32 reason, int winner) {
  PPCContext e = ctx;
  e.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
  e.r3.u64 = loser;
  e.r4.u64 = reason;
  e.r5.u64 = u32(winner);
  __imp__sub_82261320(e, base);
  for (u32 w = 0; w < 6; ++w)
    bd::mem::store<u32>(0x8317B7A4 + w * 4, 0);  // (the request to the referee, if one was filed)
}

void RefActions(const PPCContext &ctx, u8 *base, u32 ref, u32 state, double dt, int64_t now) {
  {  // (every motion of ours kept asked, one a tick: read before it is wanted)
    static constexpr u32 kAll[] = {2020, 2021, 2022, 2024, 2026, 2029, 2037, 2038, 2040, 2041, 2042, 2049, 2050, 2051, 2043};
    static size_t s_next = 0;
    Wwe12MotionReady(ctx, base, W(kAll[s_next++ % std::size(kAll)]));
  }
  const u16 anim = Anim(ref);
  u32 by = 0, held = 0;
  const bool hold = Hold(ref, by, held);
  const bool counting = state == 1 || state == 3 || state == 4 || state == 5;
  const u32 offender = g_foul_by.load();
  const bool dq_open = offender && now - g_foul_ms.load() < 5000 && bd::mem::try_load<u8>(kMatch + 27) != 0 &&
                       IndexOf(offender) >= 0 && bd::mem::try_load<u32>(offender + 3428) == 0;
  const int rs = Step(ctx, base, g_rm, dt);
  Step(ctx, base, g_tm, dt);

  if (g_act != kActNone) {
    g_act_t += dt;
    if (rs == 3) {
      BD_INFO("[sref] action {} interrompue (motion {})", g_act, anim);
      g_act = kActNone;
    } else if (g_act == kActCaution) {
      if (!g_resolved && g_rm.anim == W(2020) && !g_rm.waiting && g_act_t >= 1.0) {
        g_resolved = true;
        const int idx = IndexOf(g_target);
        u32 hb = 0, hh = 0;
        const bool in_hold = Hold(ref, hb, hh) && (hb == g_target || hh == g_target);
        BD_INFO("[sref] avertissement : le catcheur {} joue la motion {}", idx, Anim(g_target));
        const u16 ta = Anim(g_target);
        if (idx >= 0 && !DownAnim(ta) && !in_hold && !(ta >= 13115 && ta < 13150) && !(ta >= 190 && ta <= 220)) {
          int live = 0, spare = -1;
          for (int k = 0; k < 3; ++k) {
            if (g_strike[idx][k] > now)
              ++live;
            else if (spare < 0)
              spare = k;
          }
          if (spare >= 0) {
            g_strike[idx][spare] = now + 60000;
            ++live;
          }
          // (after his reaction the game gives him his next motion itself: replaying his earlier one froze him)
          float rx, rz, tx, tz;
          bool behind = false;
          if (Spot(ref, rx, rz) && Spot(g_target, tx, tz)) {
            const float h = LoadF(g_target + 740);
            behind = (-std::sin(h)) * (rx - tx) + (-std::cos(h)) * (rz - tz) < 0.f;
          }
          if (live >= 3) {
            Face(g_target, ref);
            Start(g_tm, g_target, W(2024), 0, 287, 0);
            g_argue = true;
            g_argue_t = 0;
            BD_INFO("[sref] avertissement : le catcheur {} conteste (3e avertissement)", idx);
          } else {
            Start(g_tm, g_target, W(behind ? 2021 : 2022), 0, behind ? 191 : 261, 0);
            Cost(0.1f, "avertissement a un catcheur dans son droit");
            BD_INFO("[sref] avertissement : le catcheur {} obeit ({} avertissement(s))", idx, live);
          }
        } else {
          BD_INFO("[sref] avertissement : le catcheur n'est pas libre, rien");
        }
      }
      if (g_argue && !g_answered) {
        g_argue_t += dt;
        if (g_argue_t >= 1.0 && (rs == 0 || rs == 2 || g_rm.anim == W(2020))) {
          Start(g_rm, ref, W(2026), 0, 152, 0);
          g_answered = true;
          Cost(0.5f, "il repond au catcheur qui conteste");
          if (const int idx = IndexOf(g_target); idx >= 0)
            std::memset(g_strike[idx], 0, sizeof(g_strike[idx]));
        }
      } else if (rs == 2 || rs == 0) {
        g_act = kActNone;
      }
    } else if (g_act == kActNothing || g_act == kActDq) {
      if (rs == 2 || rs == 0)
        g_act = kActNone;
    } else if (g_act == kActScrew) {
      if (!g_resolved && !g_rm.waiting && g_act_t >= 100.0 / 60.0) {
        g_resolved = true;
        const int winner = IndexOf(g_by);
        if (winner >= 0 && IndexOf(g_held) >= 0) {
          BD_INFO("[sref] screw job : le catcheur {} gagne par soumission", winner);
          EndMatch(ctx, base, g_held, 2, winner);
        }
      }
      if (rs == 2 || rs == 0)
        g_act = kActNone;
    } else if (g_act == kActCheck) {
      static constexpr double kLen[2][4] = {{36, 461, 36, 261}, {113, 318, 141, 246}};  // 2037, 2038: blocks 0, 1, 2, 10
      const u32 m = g_check_anim;
      const double *len = kLen[m == W(2038) ? 1 : 0];
      const bool tap = bd::mem::try_load<u32>(0x8317B7A4) == 3;
      if (g_check_block <= 1 && tap && IndexOf(g_held) >= 0 && IndexOf(g_by) >= 0) {
        BD_INFO("[sref] soumission : abandon accepte, le catcheur {} gagne", IndexOf(g_by));
        Start(g_rm, ref, m, 10, len[3], 0);
        g_check_block = 10;
        EndMatch(ctx, base, g_held, 2, IndexOf(g_by));
      } else if (g_check_block <= 1 && !g_rm.waiting && ((g_hold_gone = hold ? 0.0 : g_hold_gone + dt) > 0.5 ||
                                                        g_b.exchange(false))) {
        Start(g_rm, ref, m, 2, len[2], 0);
        g_check_block = 2;
      } else if (rs == 2 || rs == 0) {
        if (g_check_block <= 1) {
          Start(g_rm, ref, m, 1, len[1], 0);  // (in, then the loop again and again)
          g_check_block = 1;
        } else {
          g_act = kActNone;
        }
      }
    }
  } else {
    const bool a = !counting && g_press.exchange(false);
    const bool y = g_y.exchange(false);
    g_b = false;
    g_hold_gone = 0;
    const bool free = IdleAnim(anim);
    if (anim >= 20000 && anim <= 20598)
      g_ref_idle = anim;
    if (y && dq_open && (free || anim <= 1)) {
      int winner = -1;
      const auto side = [&](u32 k) {  // (his side: his participant record's + 13)
        PPCContext c = ctx;
        c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
        c.r3.u64 = k;
        __imp__sub_823F8270(c, base);
        __imp__sub_8267CD30(c, base);
        return c.r3.u32 ? int(bd::mem::try_load<u8>(c.r3.u32 + 13)) : -1;
      };
      const int off_index = IndexOf(offender), off_side = off_index >= 0 ? side(u32(off_index)) : -1;
      for (u32 k = 0; k < 6 && winner < 0; ++k)
        if (const u32 o = Actor(k); o && o != offender && o != ref && bd::mem::try_load<u32>(o + 3428) == 0 &&
                                     (off_side < 0 || side(k) != off_side))
          winner = int(k);
      BD_INFO("[sref] disqualification prononcee : l'acteur {} perd, le catcheur {} gagne", IndexOf(offender), winner);
      Start(g_rm, ref, W(anim == 0 ? 2041 : anim == 1 ? 2042 : 2040), 0, anim == 0 ? 621 : anim == 1 ? 536 : 231,
            0);
      g_act = kActDq;
      g_act_t = 0;
      g_foul_by = 0;
      EndMatch(ctx, base, offender, 7, winner);
    } else if (y && hold && g_gauge.load() >= 1.f && free) {
      g_by = by, g_held = held;
      Start(g_rm, ref, W(2029), 0, 306, 0);
      g_act = kActScrew;
      g_act_t = 0;
      g_resolved = false;
      BD_INFO("[sref] screw job : commence");
    } else if (a && free && hold) {
      g_by = by, g_held = held;
      g_check_anim = W(DownAnim(Anim(held)) ? 2038 : 2037);
      Face(ref, held);
      Start(g_rm, ref, g_check_anim, 0, g_check_anim == W(2038) ? 113 : 36, 0);
      g_check_block = 0;
      g_act = kActCheck;
      g_act_t = 0;
      BD_INFO("[sref] soumission : il verifie (motion {})", g_check_anim);
    } else if (a && free) {
      u32 target = 0;
      float best = 450.f * 450.f, rx, rz;
      if (g_gauge.load() >= 0.1f && Spot(ref, rx, rz))
        for (u32 k = 0; k < 6; ++k) {
          const u32 o = Actor(k);
          float ox, oz;
          if (!o || o == ref || bd::mem::try_load<u32>(o + 3428) != 0 || !Spot(o, ox, oz))
            continue;
          const float d = (ox - rx) * (ox - rx) + (oz - rz) * (oz - rz);
          if (d < best)
            best = d, target = o;
        }
      g_act_t = 0;
      g_resolved = g_argue = g_answered = false;
      if (target) {
        g_target = target;
        Face(ref, target);
        Start(g_rm, ref, W(2020), 0, 183, 0);
        g_act = kActCaution;
        BD_INFO("[sref] avertissement au catcheur {} (a {:.0f})", IndexOf(target), std::sqrt(best));
      } else {
        Start(g_rm, ref, W(2049), 0, 151, 0);
        g_act = kActNothing;
      }
    }
  }
  // the hold he does not check: three seconds, then the gauge drains
  if (hold && g_act != kActCheck && g_act != kActScrew) {
    g_hold_grace -= dt;
    if (g_hold_grace <= 0) {
      if (!g_hold_drain) {
        g_hold_drain = true;
        ++g_faults;
        BD_INFO("[sref] jauge : il ne verifie pas la soumission, elle baisse ; {} fautes", g_faults);
      }
      g_gauge = std::clamp(g_gauge.load() - float(0.02 * dt), 0.f, 1.f);
    }
  } else {
    g_hold_drain = false;
    if (!hold)
      g_hold_grace = 3.0;
  }
  const bool in_count = anim == 190 || anim == 191 || anim == 200;
  {  // a fighter within a grapple's reach: A is the game's
    bool close_by = false;
    float rx, rz;
    if (!hold && !counting && g_act == kActNone && Spot(ref, rx, rz))
      for (u32 k = 0; k < 6 && !close_by; ++k) {
        const u32 o = Actor(k);
        float ox, oz;
        close_by = o && o != ref && bd::mem::try_load<u32>(o + 3428) == 0 && Spot(o, ox, oz) &&
               (ox - rx) * (ox - rx) + (oz - rz) * (oz - rz) < 170.f * 170.f;
      }
    if (close_by != g_a_game.load()) {
      static int s_lines = 0;
      if (s_lines++ < 200)
        BD_INFO("[sref] A : {}", close_by ? "un catcheur a portee, la prise du jeu" : "l'action de l'arbitre");
    }
    g_a_game = close_by;
  }
  // his finisher, as WWE '13's "special skill": with a full gauge he has one (the game's finisher button, Y), and
  // doing it costs 0.75. (A role-1 man gains no momentum: his stored finishers, actor + 2776, are only ours.)
  {
    const u32 have = bd::mem::try_load<u32>(ref + 2776);
    if (g_fin_given && have < g_fin_given) {
      g_fin_given = 0;
      Cost(0.75f, "son finisher");
    } else if (!g_fin_given && g_gauge.load() >= 1.f && !hold && !dq_open) {
      g_fin_before = have;
      g_fin_given = have + 1;
      bd::mem::store<u32>(ref + 2776, g_fin_given);
      BD_INFO("[sref] jauge pleine : un finisher pour l'arbitre (Y)");
    } else if (g_fin_given && g_gauge.load() < 1.f && have == g_fin_given) {
      bd::mem::store<u32>(ref + 2776, g_fin_before);
      g_fin_given = 0;
    }
  }
  g_want_y = dq_open || (hold && g_gauge.load() >= 1.f);
  g_prompt = dq_open ? 3 : hold && g_gauge.load() >= 1.f ? 4 : hold && g_act != kActCheck ? 2 : counting && !in_count ? 1
             : g_fin_given && bd::mem::try_load<u32>(ref + 2776) >= g_fin_given ? 5 : 0;
}
}  // namespace

// ---- the game's own referee waits, and takes over when the gauge is empty (WWE '13's 'SRLV', read 2026-11-16) ----
// WWE '13 makes a spare referee at the match's setup, parks him with the role 6 and hides him; when the special
// referee's gauge is empty: he plays 2050; 180 frames later the spare one appears in the ring's middle playing 2043
// while he plays 2051; when 2043 is over the spare one has the role 1 and he the role 0, and must leave.
// In WWE '12 (read the same day):
// - the match's setup drops the normal referee when a place of the kind 1 holds a superstar (sub_82A37378). For
//   that call the special referee's place has the kind 0, so the normal referee is made as in any match - with the
//   role 1, which he needs when he is made: the actor's brain (his component 'CMIF', see Brain) is not built for the roles
//   4 and 6 (sub_822143D0);
// - he is then parked: the role 6 (never a target, not counted, his pad silent) in his actor, his participant
//   record (40) and his creation record (slot 6) - before the match is put in place (sub_823B5D18), which gives the
//   referee's start place to the last man of the role 1 - and each tick not drawn, not updated, "not there"
//   (his flags' first word) and far up the ramp, where the game leaves the men who went away;
// - the hand-over: he is shown in the ring's middle playing 2043, then has the role 1 and is the computer's; the
//   special referee has the role 0 and + 998 = 4 (out of the pins), and walks away once outside the ring.
namespace {
// The actor's brain: his component 'CMIF' (the game's own look-up, sub_827D5C28 on the actor's components at + 3436, read
// here without its cache write; sub_822143D0 makes it only when there is none, then stores it with sub_8247B038).
// Not actor + 3384 (written by other code: it stayed 0 for the referee who had his brain - user's test of 2026-11-16,
// 17:25, the hand-over refused for that).
u32 Brain(u32 actor) {
  const u32 tag = 0x434D4946;  // 'CMIF'
  const u32 list = actor ? bd::mem::try_load<u32>(actor + 3436) : 0;
  if (!list)
    return 0;
  u32 found = 0;
  if (const u32 c = bd::mem::try_load<u32>(list + 12); c && bd::mem::try_load<u32>(c + 4) == tag)
    found = c;
  for (u32 c = bd::mem::try_load<u32>(list + 20), k = 0; !found && c && k < 256; c = bd::mem::try_load<u32>(c + 20), ++k)
    if (bd::mem::try_load<u32>(c + 4) == tag)
      found = c;
  for (u32 c = list, k = 0; !found && c && k < 256; c = bd::mem::try_load<u32>(c + 16), ++k)
    if (bd::mem::try_load<u32>(c + 4) == tag)
      found = c;
  if (!found)
    return 0;
  const u32 brain = bd::mem::try_load<u32>(found + 24);
  return brain ? brain : found;
}

u32 SlotOf(u32 index) {  // the manager's slot of an actors' table index
  const u32 mgr = bd::mem::try_load<u32>(0x8317434C);
  for (u32 s2 = 0; mgr && s2 < 16; ++s2)
    if (bd::mem::try_load<u32>(mgr + (s2 + 32) * 4) == index)
      return s2;
  return 0xFFFFFFFF;
}
void Shown(const PPCContext &ctx, u8 *base, u32 actor, bool on) {
  const int index = [&] {
    for (u32 k = 0; k < 9; ++k)
      if (actor && Actor(k) == actor)
        return int(k);
    return -1;
  }();
  const u32 mgr = bd::mem::try_load<u32>(0x8317434C), slot = index >= 0 ? SlotOf(u32(index)) : 0xFFFFFFFF;
  if (!mgr || slot == 0xFFFFFFFF)
    return;
  PPCContext c = ctx;
  c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
  c.r3.u64 = mgr, c.r4.u64 = slot, c.r5.u64 = on ? 1 : 0;
  __imp__sub_822CF850(c, base);
  c = ctx;
  c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
  c.r3.u64 = mgr, c.r4.u64 = slot, c.r5.u64 = on ? 1 : 0, c.r6.u64 = 0;
  __imp__sub_822CE3F0(c, base);
  bd::mem::store<u32>(actor + 1360, on ? 1 : 0);  // ("there" or not, for targeting)
  bd::mem::store<u32>(actor + 1028, on ? 1 : 0);
}
void SetRole(const PPCContext &ctx, u8 *base, u32 index, u32 participant, u32 role) {
  if (const u32 a = Actor(index))
    bd::mem::store<u32>(a + 3428, role);
  PPCContext c = ctx;
  c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
  c.r3.u64 = participant;
  __imp__sub_8267CD30(c, base);
  if (c.r3.u32)
    bd::mem::store<u32>(c.r3.u32 + 8, role);
  if (const u32 made = bd::mem::try_load<u32>(0x8322DD44))
    bd::mem::store<u8>(made + 528 + 80 * index + 6, u8(role));
}
void Place(u32 actor, float x, float y, float z) {
  const float v[3] = {x, y, z};
  const u32 p = bd::mem::try_load<u32>(actor + 768);
  for (u32 i = 0; i < 3; ++i) {
    u32 bits;
    std::memcpy(&bits, &v[i], 4);
    bd::mem::store<u32>(actor + 752 + i * 4, bits);
    if (p)
      bd::mem::store<u32>(p + i * 4, bits);
  }
}
// An actor back in his free motion (sub_8247C670: the motion 1350, his walking / standing set). His brain ('CMIF',
// sub_8232E3B0) acts only from it: after a motion of ours he stays still until he is in it again (user's test of
// 2026-11-16, 17:45: the old referee left in the referee's idle 20150, the game's referee in 446 - both stood still).
void Free(const PPCContext &ctx, u8 *base, u32 actor) {
  if (!actor)
    return;
  PPCContext c = ctx;
  c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
  c.r3.u64 = actor;
  __imp__sub_8247C670(c, base);
  // (his flags' + 68 - actor + 1096, and its copy + 1428 - stays 1 after a motion of ours, which plays with the
  // flag r8 = 1; it is 0 on an ordinary match's referee, who walks, and was 1 on ours and on the old one, who both
  // stood still a whole minute: the study of 2026-11-16, 18:38 / 18:40)
  bd::mem::store<u32>(actor + 1096, 0);
  bd::mem::store<u32>(actor + 1428, 0);
}
// A man put in place as the game does for every man when a match is put in place (sub_823B5C58): his two motion
// layers' weights reset ([+3396] + 68 / + 76: + 272 / + 276 = 0, + 292 |= 3), his free motion on his channels 0 and 2,
// his navigation ('oNav', sub_8232E200 via sub_8232E3A8). The game's referee after the hand-over and his climb stood
// still in the ring every time (user's tests of 2026-11-16), his brain's flag + 1096 kept at 1: a man the game never
// put in place since he was parked. On the main thread only (sub_8232E200 may make a component).
std::string Channels(u32 a) {  // (his three channels' motions, for the log)
  return fmt::format("{} {} {}", bd::mem::try_load<u16>(a + 216), bd::mem::try_load<u16>(a + 264), bd::mem::try_load<u16>(a + 312));
}
void PutInPlace(const PPCContext &ctx, u8 *base, u32 a) {
  if (!a)
    return;
  const std::string before = Channels(a);
  if (const u32 t = bd::mem::try_load<u32>(a + 3396))
    for (u32 off : {68u, 76u})
      if (const u32 o = bd::mem::try_load<u32>(t + off)) {
        bd::mem::store<u32>(o + 272, 0);
        bd::mem::store<u32>(o + 276, 0);
        bd::mem::store<u32>(o + 292, bd::mem::try_load<u32>(o + 292) | 3);
      }
  PPCContext c = ctx;
  c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
  const u32 key = c.r1.u32 + 256;
  bd::mem::store<u32>(key, 0);
  c.r3.u64 = a, c.r4.u64 = key;
  __imp__sub_8247C590(c, base);
  for (u32 ch : {0u, 2u}) {
    c = ctx;
    c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
    c.r3.u64 = a, c.r4.u64 = key, c.r5.u64 = 0, c.r6.u64 = ch;
    c.f1.f64 = 0.0;
    __imp__sub_82479730(c, base);
  }
  c = ctx;
  c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
  c.r3.u64 = a;
  __imp__sub_8232E200(c, base);
  bd::mem::store<u32>(a + 1096, 0);
  bd::mem::store<u32>(a + 1428, 0);
  BD_INFO("[sref] {:08X} remis en place comme au debut d'un match : motion {} (cle {:08X}), canaux {} -> {}", a, Anim(a),
          bd::mem::try_load<u32>(key), before, Channels(a));
}
// (the special referee's participant number: his actors' index, as the game maps them for the six places)
u32 ParticipantOf(const PPCContext &ctx, u8 *base, u32 index) {
  PPCContext c = ctx;
  c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
  c.r3.u64 = index;
  __imp__sub_823F8270(c, base);
  return c.r3.u32;
}

// ---- WWE '13's scene on the entrance stage (its motion 2043, here 13138; read in its code and data 2026-11-16, and in
// the user's video of it) ----
// 2043 is a scene of three: block (0, 0) the referee who comes, (0, 1) the General Manager (WWE '13's character 300,
// John Laurinaitis), (0, 2) his microphone (object 142), and block (0, 60) its camera ("Camera001" / "fov.Camera001":
// an eye with its rotation, and its field of view). All are played from (0, -120, 0), heading 0: the stage's place is
// in the motion (about 25 m along + z). WWE '13 lifts the bodies with the ramp as they go (its 'CERI': the ramp from
// z 900 to 2380, up to 180 cm). Its camera: shot A until frame 100, shot B to 380, then the match's camera again;
// the bodies' end at frame 500. WWE '12 plays a motion's camera as the bell's camera does (sub_823EC6F8): the motion
// camera ([0x8317B8C4], id 1) reads the two nodes whose names' hashes are at its + 4224 and + 4496 ("Camera01" by
// default): WWE '13's names are put there for the scene, then the game's again.
// Not done yet: WWE '13's voice line; the General Manager himself - his body here is the first man at ringside (the
// game's actor 7), put back after.
namespace {
bool g_scene = false, g_scene_cam = false;
double g_scene_t = 0;
u32 g_gm = 0, g_mic = 0, g_cam_h1 = 0, g_cam_h2 = 0;
float g_gm_pos[3] = {}, g_gm_head = 0;
u16 g_gm_anim = 0;
void Call(const PPCContext &in, u8 *base, u32 fn, u32 r3) {  // a function of the game by its address (a virtual)
  PPCContext c = in;
  c.r1.u64 = (in.r1.u32 - 1024) & ~15u;
  c.r3.u64 = r3;
  if (PPCFunc *f = rex::runtime::ResolveIndirectFunction(fn))
    f(c, base);
}
void PlayBlock(const PPCContext &in, u8 *base, u32 who, u32 motion, u32 x, u32 y) {
  PPCContext c = in;
  c.r1.u64 = (in.r1.u32 - 512) & ~15u;
  c.r3.u64 = who, c.r4.u64 = motion, c.r5.u64 = x, c.r6.u64 = y, c.r8.u64 = 1, c.r9.u64 = 0;
  const u32 bits = bd::mem::try_load<u32>(0x82000AD8);
  float f;
  std::memcpy(&f, &bits, 4);
  c.f1.f64 = f;
  __imp__sub_8247AD18(c, base);
}
bool BlockThere(const PPCContext &in, u8 *base, u32 motion, u32 x, u32 y) {
  PPCContext c = in;
  c.r1.u64 = (in.r1.u32 - 512) & ~15u;
  c.r3.u64 = bd::mem::try_load<u32>(0x8317B760), c.r4.u64 = motion, c.r5.u64 = x, c.r6.u64 = y;
  if (!c.r3.u32)
    return false;
  __imp__sub_82478510(c, base);
  return c.r3.u32 != 0;
}
float RampY(float z) {  // (WWE '13's CERI: the ramp from z 900 to 2380, 180 cm up - the world's y points down)
  return z <= 900.f ? 0.f : z >= 2380.f ? -180.f : -180.f * (z - 900.f) / 1480.f;
}
void Lift(u32 a) {
  const u32 q = a ? bd::mem::try_load<u32>(a + 768) : 0;
  if (!q)
    return;
  const float y = RampY(LoadF(q + 8)) - 120.f;
  u32 bits;
  std::memcpy(&bits, &y, 4);
  bd::mem::store<u32>(q + 4, bits);
  bd::mem::store<u32>(a + 756, bits);
}
void CameraBack(const PPCContext &ctx, u8 *base) {
  if (!g_scene_cam)
    return;
  g_scene_cam = false;
  const u32 cam = bd::mem::try_load<u32>(0x8317B8C4), dir = bd::mem::try_load<u32>(0x8317B8C0);
  if (cam) {
    Call(ctx, base, bd::mem::try_load<u32>(bd::mem::try_load<u32>(cam) + 8), cam);  // (stop)
    bd::mem::store<u32>(cam + 4224, g_cam_h1);
    bd::mem::store<u32>(cam + 4496, g_cam_h2);
  }
  if (dir) {
    PPCContext c = ctx;
    c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
    c.r3.u64 = dir, c.r4.u64 = 1;
    __imp__sub_821CCFE0(c, base);
    if (c.r3.u32) {
      c.r4.u64 = 0;
      __imp__sub_82481ED0(c, base);
    }
    c = ctx;
    c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
    c.r3.u64 = dir, c.r4.u64 = 0;
    __imp__sub_8247EE60(c, base);
    Call(ctx, base, bd::mem::try_load<u32>(bd::mem::try_load<u32>(dir) + 4), dir);  // (update)
  }
  BD_INFO("[sref] scene : la camera du match reprend");
}
bool SceneStart(const PPCContext &ctx, u8 *base, u32 n) {
  if (!REXCVAR_GET(wwe12_sref_scene))
    return false;
  const u32 m = W(2043);
  const bool b0 = BlockThere(ctx, base, m, 0, 0), b1 = BlockThere(ctx, base, m, 0, 1), cam60 = BlockThere(ctx, base, m, 0, 60);
  g_gm = g_gm_place >= 0 ? Actor(u32(g_gm_place)) : Actor(7);
  BD_INFO("[sref] scene : motion {} trouvee : corps {} / {}, camera {} ; manager general (acteur {}) {:08X}", m, b0, b1, cam60,
          g_gm_place >= 0 ? g_gm_place : 7, g_gm);
  if (!b0 || !b1 || !cam60 || !g_gm)
    return false;
  // the General Manager's body: the first man at ringside, his place kept
  if (const u32 q = bd::mem::try_load<u32>(g_gm + 768))
    for (u32 i = 0; i < 3; ++i)
      g_gm_pos[i] = LoadF(q + i * 4);
  g_gm_head = LoadF(g_gm + 740);
  g_gm_anim = Anim(g_gm);
  // the two bodies at the scene's origin
  const float zero = 0.f;
  u32 zb;
  std::memcpy(&zb, &zero, 4);
  for (u32 a : {n, g_gm}) {
    Place(a, 0.f, -120.f, 0.f);
    bd::mem::store<u32>(a + 740, zb);
  }
  Shown(ctx, base, n, true);
  for (u32 o : {8u, 88u, 144u})
    bd::mem::store<u32>(n + 1360 + o, 0);
  bd::mem::store<u32>(n + 1360 + 16, 1);
  std::memcpy(bd::mem::at<u8>(n + 1028), bd::mem::at<u8>(n + 1360), 332);
  if (g_gm_place >= 0) {  // (our General Manager, unseen until now)
    Shown(ctx, base, g_gm, true);
    for (u32 o : {8u, 88u, 144u})
      bd::mem::store<u32>(g_gm + 1360 + o, 0);
    bd::mem::store<u32>(g_gm + 1360 + 16, 1);
    std::memcpy(bd::mem::at<u8>(g_gm + 1028), bd::mem::at<u8>(g_gm + 1360), 332);
  }
  PlayBlock(ctx, base, n, m, 0, 0);
  PlayBlock(ctx, base, g_gm, m, 0, 1);
  // his microphone (loaded and parked for these matches, the I Quit's way)
  g_mic = 0;
  for (u32 i = 0; i < 72 && !g_mic; ++i)
    if (const u32 o = bd::mem::try_load<u32>(0x83113630 + i * 4); o && bd::mem::try_load<u32>(o + 80) == 142)
      g_mic = o;
  if (g_mic && bd::mem::try_load<u32>(g_mic + 3236) != 0) {
    PPCContext c = ctx;
    c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
    c.r3.s64 = -1, c.r4.u64 = 142, c.r5.u64 = 0, c.r6.u64 = 0;
    __imp__sub_82254518(c, base);
    g_mic = c.r3.u32;
  }
  if (g_mic) {
    Place(g_mic, 0.f, -120.f, 0.f);
    bd::mem::store<u32>(g_mic + 740, zb);
    bd::mem::store<u32>(g_mic + 96, g_gm);  // (in the General Manager's hand, as in WWE '13)
    PlayBlock(ctx, base, g_mic, m, 0, 2);
  }
  // its camera
  const u32 cam = bd::mem::try_load<u32>(0x8317B8C4), dir = bd::mem::try_load<u32>(0x8317B8C0);
  if (cam && dir) {
    g_cam_h1 = bd::mem::try_load<u32>(cam + 4224), g_cam_h2 = bd::mem::try_load<u32>(cam + 4496);
    bd::mem::store<u32>(cam + 4224, 0x6FEFAF1E);  // ("Camera001")
    bd::mem::store<u32>(cam + 4496, 0x69825289);  // ("fov.Camera001")
    PPCContext c = ctx;
    c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
    const u32 key = c.r1.u32 + 256, pos = key + 16;
    bd::mem::store<u32>(key, m);
    bd::mem::store<u32>(key + 4, 0);
    bd::mem::store<u32>(key + 8, 60);
    // (lifted with the stage, as the bodies are: in 2043 the camera's heights are 0.6 m under the floor (shot A) and
    // 0.1 m over it (shot B), the people's hips 1.2 m over it - WWE '13 lifts the whole scene by the stage's 180 cm.
    // Not lifted, the camera was under the stage: user's test of 2026-11-16, 17:45, "la camera visait mal".)
    const float p[5] = {0.f, RampY(2380.f) - 120.f, 0.f, 0.f, 0.f};
    for (u32 i = 0; i < 5; ++i) {
      u32 b;
      std::memcpy(&b, &p[i], 4);
      bd::mem::store<u32>(pos + i * 4, b);
    }
    c.r3.u64 = bd::mem::try_load<u32>(0x8317B760), c.r4.u64 = key, c.r5.u64 = pos, c.r6.u64 = 0, c.r7.u64 = 1;
    __imp__sub_82482388(c, base);
    c = ctx;
    c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
    c.r3.u64 = dir, c.r4.u64 = 1;
    __imp__sub_8247EE60(c, base);
    g_scene_cam = true;
  }
  g_scene = true;
  g_scene_on = true;
  g_scene_t = 0;
  BD_INFO("[sref] scene : commence (arbitre {:08X}, manager {:08X} motion {}, micro {:08X}, camera {})", n, g_gm, Anim(g_gm),
          g_mic, g_scene_cam);
  return true;
}
// the scene going on; true once it is over
bool SceneTick(const PPCContext &ctx, u8 *base, u32 n, u32 s3, double dt) {
  g_scene_t += dt;
  const u32 m = W(2043);
  for (u32 a : {n, g_gm, g_mic})
    if (a && Anim(a) == u16(m))
      Lift(a);
  if (g_scene_cam && g_scene_t >= 380.0 / 60.0) {
    CameraBack(ctx, base);
    if (s3)
      Start(g_rm, s3, W(2051), 0, 331, 0);
  }
  if (g_scene_t < 500.0 / 60.0)
    return false;
  CameraBack(ctx, base);
  // the General Manager back at ringside (or out of sight again, ours), the microphone parked
  if (g_gm && g_gm_place >= 0) {
    Shown(ctx, base, g_gm, false);
    Place(g_gm, 0.f, -120.f, 3200.f);
  } else if (g_gm) {
    Place(g_gm, g_gm_pos[0], g_gm_pos[1], g_gm_pos[2]);
    u32 b;
    std::memcpy(&b, &g_gm_head, 4);
    bd::mem::store<u32>(g_gm + 740, b);
    if (g_gm_anim && g_gm_anim != 0xFFFF)
      PlayBlock(ctx, base, g_gm, g_gm_anim, 0, 0);
  }
  if (g_mic && bd::mem::try_load<u32>(g_mic + 3236) == 0) {
    bd::mem::store<u32>(g_mic + 96, 0);
    PPCContext c = ctx;
    c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
    c.r3.u64 = g_mic;
    __imp__sub_82250258(c, base);
  }
  g_scene = false;
  g_scene_on = false;
  BD_INFO("[sref] scene : finie (l'arbitre est sur la scene, z {:.0f})", [&] {
    float x, z;
    return Spot(n, x, z) ? z : 0.f;
  }());
  return true;
}
}  // namespace

// A new match set up (or a rematch): what the last one's replacement left is undone. The motion camera keeps the node
// names it reads (+ 4224 / + 4496): WWE '13's are put back to the game's if a match ended during the scene (the
// review of 2026-11-16: every later match's bell camera would read "Camera001"). The scene's objects belong to the
// last match: only forgotten.
void ResetMatchState() {
  if (g_scene_cam) {
    if (const u32 cam = bd::mem::try_load<u32>(0x8317B8C4)) {
      bd::mem::store<u32>(cam + 4224, g_cam_h1);
      bd::mem::store<u32>(cam + 4496, g_cam_h2);
    }
    g_scene_cam = false;
  }
  if (g_out != kOutNone || g_ex || g_walk.load() || g_climb.load())
    BD_INFO("[sref] nouveau match : le remplacement du match d'avant est oublie (etape {})", g_out);
  g_scene = false;
  g_scene_on = false;
  g_scene_ran = false;
  g_scene_t = 0;
  g_gm = 0, g_mic = 0;
  g_gm_place = -1;
  g_out = kOutNone;
  g_out_t = 0;
  g_walk = false;
  g_climb = false;
  g_ex = 0;
  g_sref_man = 0;
  g_gone = false;
  g_left = false;
  g_ejected = false;
  g_watch_ms = 0;
  g_started = false;
  g_rm = {};
  g_tm = {};
  g_nm = {};
  g_act = kActNone;
  g_pad = -1;
  g_judge_alone = false;
  g_in_ring_ms = 0;
  g_auto_ms = 0;
  g_ref_moving = false;
  g_ex_walk = false;
}

void HandOver(const PPCContext &ctx, u8 *base) {
  if (!REXCVAR_GET(wwe12_sref_replacement))
    return;
  const u32 n = Actor(6);
  if (!n)
    return;
  const int64_t now = NowMs();
  static int64_t s_prev = 0;
  const double dt = std::clamp(double(now - s_prev) / 1000.0, 0.0, 0.1);
  s_prev = now;
  // parked: his brain made (he had the role 1 until here), then the role 6, unseen, far up the ramp
  if (g_out == kOutNone || g_out == kOutWait || g_out == kOut2050) {
    if (bd::mem::try_load<u32>(n + 3428) == 1 && Referee() && Referee() != n) {
      SetRole(ctx, base, 6, 40, 6);
      BD_INFO("[sref] l'arbitre du jeu ({:08X}) attend hors de vue (role 6)", n);
    }
    if (Parked(n)) {
      Shown(ctx, base, n, false);
      bd::mem::store<u8>(n + 998, 1);
      Place(n, 0.f, -120.f, 3200.f);
    }
  }
  if (g_out == kOutNone || g_out == kOutDone)
    return;
  g_tick_ms = now;
  const u32 s3 = g_ex ? g_ex : Referee() ? Referee() : g_sref_man;
  Step(ctx, base, g_nm, dt);
  if (s3)
    Step(ctx, base, g_rm, dt);
  g_out_t += dt;
  if (g_out == kOutWait) {  // (once he stands free: 2050)
    if (s3 && IdleAnim(Anim(s3)) && !g_rm.anim) {
      g_act = kActNone;
      g_tm = {};
      Start(g_rm, s3, W(2050), 0, 251, 0);
      g_out = kOut2050;
      g_out_t = 0;
      g_ex = 0;
      BD_INFO("[sref] remplacement : l'arbitre special joue 2050");
    } else if (g_out_t > 20.0) {
      g_out_t = 0;  // (still busy: tried again)
    }
  } else if (g_out == kOut2050 && g_out_t >= 3.0) {  // (180 frames: WWE '13's scene, then his 2051)
    // (WWE '13, filmed by the user on 2026-11-16: after 2050 a scene on the entrance stage - its General Manager at
    // the microphone, a referee beside him - then back in the ring the man who was the referee plays 2051 and leaves
    // the ring by himself, and the referee comes from the stage to the ring.)
    g_scene_ran = SceneStart(ctx, base, n);
    if (g_scene_ran) {
      g_out = kOutScene;
    } else {
      if (s3)
        Start(g_rm, s3, W(2051), 0, 331, 0);
      g_out = kOutArrive;
      BD_INFO("[sref] remplacement : pas de scene, l'arbitre special joue 2051");
    }
    g_out_t = 0;
  } else if (g_out == kOutScene) {
    if (SceneTick(ctx, base, n, s3, dt)) {
      g_out = kOutArrive;
      g_out_t = 100;  // (the hand-over now: 2051 was played during the scene)
    }
  } else if (g_out == kOutArrive && g_out_t > 5.5) {  // (2051 is 331 frames)
    // the hand-over. The game's referee: at the ramp's foot, outside the ring (ring's edge 313; the ramp is on the
    // + z side), at the height of the people who stand at ringside (actors 7, 8), facing the ring - his brain then
    // takes him into the ring, as the referee comes from the stage in WWE '13.
    {
      float y = -60.f, ax, az;
      for (u32 k : {7u, 8u})
        if (const u32 a = Actor(k); a && Spot(a, ax, az))
          if (const u32 q = bd::mem::try_load<u32>(a + 768); q && std::isfinite(LoadF(q + 4))) {
            y = LoadF(q + 4);
            break;
          }
      for (u32 o : {8u, 88u, 144u})
        bd::mem::store<u32>(n + 1360 + o, 0);
      bd::mem::store<u32>(n + 1360 + 16, 1);
      std::memcpy(bd::mem::at<u8>(n + 1028), bd::mem::at<u8>(n + 1360), 332);
      // (without the scene: at the ramp's foot, outside the ring. After it he is on the stage, where 2043 leaves him,
      // and walks down as in WWE '13 - helped if not, see the tick)
      if (!g_scene_ran)
        Place(n, 0.f, y, 560.f);
      const float h = 0.f;  // (facing - z: the ring. An actor faces (-sin h, -cos h).)
      u32 bits;
      std::memcpy(&bits, &h, 4);
      bd::mem::store<u32>(n + 740, bits);
      bd::mem::store<u8>(n + 996, 1);  // (outside the ring)
      BD_INFO("[sref] remplacement : l'arbitre du jeu arrive par la rampe (hauteur {:.0f})", y);
    }
    SetRole(ctx, base, 6, 40, 1);
    // (his brain is made at the bell, see sub_822143D0's hook; never his set-up here, in the middle of the match: the
    // game stopped on a broken component just after - user's test of 2026-11-16, 17:58)
    BD_INFO("[sref] remplacement : cerveau de l'arbitre du jeu {:08X}", Brain(n));
    Free(ctx, base, n);
    {
      PPCContext c = ctx;
      c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
      c.r3.u64 = n, c.r4.u64 = 1;
      __imp__sub_82212140(c, base);
    }
    Shown(ctx, base, n, true);
    bd::mem::store<u32>(n + 3140, 1);
    bd::mem::store<u32>(n + 2536, 0xFFFFFFFFu);
    bd::mem::store<u8>(n + 998, 1);
    if (s3) {
      const int index = IndexOf(s3);
      if (index >= 0)
        SetRole(ctx, base, u32(index), ParticipantOf(ctx, base, u32(index)), 0);
      bd::mem::store<u8>(s3 + 998, 4);
      bd::mem::store<u32>(s3 + 3140, 0);
      // (still in 2051's last frame: the game gave him nothing after it - he stayed so, even outside the ring, user's
      // test of 2026-11-16: standing again)
      g_rm = {};
      Free(ctx, base, s3);
      BD_INFO("[sref] remplacement : l'ancien arbitre dans sa motion libre ({}), cerveau {:08X}", Anim(s3), Brain(s3));
      bd::mem::store<u32>(s3 + 2568, 1);
      PPCContext c = ctx;
      c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
      c.r3.u64 = s3, c.r4.u64 = 1;
      __imp__sub_82212140(c, base);  // (the computer's now: the player has no man of his any more)
      g_ex_ms = now;
      g_ex = s3;
      g_left = false;
      if (bd::mem::try_load<u8>(s3 + 1015) == 0) {  // (in the ring: to its ramp's side, then the game's walk away)
        bd::mem::store<u32>(s3 + 2568, 0);
        g_ex_walk = true;
        BD_INFO("[sref] remplacement : l'ancien arbitre va d'abord vers le cote rampe du ring");
      }
    }
    g_ejected = true;
    g_ejected_ms = now;
    g_pad = -1;
    g_out = kOutDone;
    g_watch_ms = now;
    g_walk_phase = g_scene_ran ? 0 : 1;
    g_walk_seen = 0;
    g_walk = true;
    BD_INFO("[sref] remplacement : l'arbitre du jeu a le role 1, l'ancien arbitre le role 0 (hors des tombes)");
  }
}
}  // namespace

// The match's setup: the normal referee is dropped when a place of the kind 1 holds a superstar. In a match of ours
// that place is a fighter's for the time of this call, and "a referee in this match" (the setup's + 45) is set.
extern "C" REX_FUNC(sub_82A37378) {
  const u32 m = ctx.r3.u32;
  u32 place = 0xFFFFFFFF;
  if (On() && Match() && REXCVAR_GET(wwe12_sref_replacement) && m)
    for (u32 i = 0; i < 6 && place == 0xFFFFFFFF; ++i)
      if (bd::mem::try_load<u32>(m + 684 + 2244 * i) == 1 && bd::mem::try_load<u32>(m + 692 + 2244 * i) != 51200)
        place = i;
  if (place != 0xFFFFFFFF) {
    bd::mem::store<u32>(m + 684 + 2244 * place, 0);
    bd::mem::store<u8>(m + 45, 1);
    BD_INFO("[sref] l'arbitre du jeu est garde pour ce match (il attendra hors de vue ; arbitre special a la place {})", place);
  }
  __imp__sub_82A37378(ctx, base);
  if (place != 0xFFFFFFFF)
    bd::mem::store<u32>(m + 684 + 2244 * place, 1);
}

// The match put in place (at its start, at a rematch): the roles as at the bell - the game's referee parked, the
// special one the referee again - before the game reads them (the referee's start place goes to the last man of
// the role 1).
extern "C" REX_FUNC(sub_823B5D18) {
  if (On() && Match() && REXCVAR_GET(wwe12_sref_replacement)) {
    const u32 n = Actor(6);
    if (n) {
      // (a rematch, the last match's replacement begun or done - not while the match runs: its tick ran within 1 s)
      BD_INFO("[sref] le match est remis en place (etape {}, dernier tick il y a {} ms)", g_out, NowMs() - g_live_ms.load());
      if (g_out != kOutNone && NowMs() - g_live_ms.load() > 1000) {
        const u32 man = g_sref_man ? g_sref_man : g_ex;
        const int index = man ? IndexOf(man) : -1;
        if (index >= 0) {
          SetRole(ctx, base, u32(index), ParticipantOf(ctx, base, u32(index)), 1);
          bd::mem::store<u8>(man + 998, 1);
          bd::mem::store<u32>(man + 3140, 1);
          bd::mem::store<u32>(man + 2568, 0);
          if (bd::mem::try_load<u32>(man + 1760) < 4) {  // (a player's man: no more the computer's)
            PPCContext c = ctx;
            c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
            c.r3.u64 = man, c.r4.u64 = 0;
            __imp__sub_82212140(c, base);
          }
        }
        BD_INFO("[sref] revanche : l'arbitre special (acteur {}) l'est a nouveau", index);
        ResetMatchState();
      }
      if (g_out == kOutNone && Referee() && Referee() != n)
        SetRole(ctx, base, 6, 40, 6);
    }
  }
  __imp__sub_823B5D18(ctx, base);
}

// The start places' sort (by role: the last man of the role 1 gets the referee's place): the game's referee who
// waits has the role 6 for the time of it, so the special referee is the one at the referee's place.
REX_EXTERN(__imp__sub_823B7DB8);
extern "C" REX_FUNC(sub_823B7DB8) {
  const u32 n = Actor(6);
  const bool swap = On() && Match() && REXCVAR_GET(wwe12_sref_replacement) && (g_out == kOutNone || g_out == kOutWait) && n &&
                    bd::mem::try_load<u32>(n + 3428) == 1 && Referee() && Referee() != n;
  if (swap)
    bd::mem::store<u32>(n + 3428, 6);
  __imp__sub_823B7DB8(ctx, base);
  if (swap)
    bd::mem::store<u32>(n + 3428, 1);
}

// Each actor's pad word, every frame (from wwe12_iquit.cpp's hook of sub_8225C4D8): the game's referee who waits is
// kept out of sight from the first frame on - the tick only runs once the match's HUD is drawn (user's test of
// 2026-11-16: before the bell he was seen half in the ring's middle).
namespace {
u32 Comp(u32 actor, u32 tag) {  // an actor's component (sub_827D5C28 on actor + 3436 / 3440 / 3444), its object (+ 24)
  for (u32 off : {3436u, 3440u, 3444u}) {
    const u32 list = actor ? bd::mem::try_load<u32>(actor + off) : 0;
    for (u32 c = list, k = 0; c && k < 256; c = bd::mem::try_load<u32>(c + 16), ++k)
      if (bd::mem::try_load<u32>(c + 4) == tag)
        return bd::mem::try_load<u32>(c + 24);
    for (u32 c = list ? bd::mem::try_load<u32>(list + 20) : 0, k = 0; c && k < 256; c = bd::mem::try_load<u32>(c + 20), ++k)
      if (bd::mem::try_load<u32>(c + 4) == tag)
        return bd::mem::try_load<u32>(c + 24);
  }
  return 0;
}
void RefDiag(const char *who, u32 a) {
  if (!a)
    return;
  const u32 b = Brain(a);
  std::string sw;
  for (u32 tag : {0x43415348u, 0x5245464Cu, 0x47524156u, 0x4E617669u})  // CASH REFL GRAV Navi
    if (const u32 o = Comp(a, tag))
      sw += fmt::format(" {:c}{:c}{:c}{:c}+10={}", char(tag >> 24), char(tag >> 16), char(tag >> 8), char(tag), bd::mem::try_load<u16>(o + 10));
    else
      sw += fmt::format(" {:c}{:c}{:c}{:c}:aucun", char(tag >> 24), char(tag >> 16), char(tag >> 8), char(tag));
  float x = 0, z = 0;
  Spot(a, x, z);
  BD_INFO("[sref] etude {} : role {} motion {} a {:.0f} {:.0f} | +1736 {} +1752 {} +1044 {} +1096 {} +1880 {} +2155 {} +2140 {} "
          "+998 {} +996 {} +3140 {} +1760 {} | cerveau {:08X} +36 {} +40 {} +44 {} +64 {} |{}",
          who, bd::mem::try_load<u32>(a + 3428), Anim(a), x, z, bd::mem::try_load<u32>(a + 1736), bd::mem::try_load<u32>(a + 1752),
          bd::mem::try_load<u32>(a + 1044), bd::mem::try_load<u32>(a + 1096), bd::mem::try_load<u32>(a + 1880),
          bd::mem::try_load<u8>(a + 2155), bd::mem::try_load<u32>(a + 2140), bd::mem::try_load<u8>(a + 998),
          bd::mem::try_load<u8>(a + 996), bd::mem::try_load<u32>(a + 3140), bd::mem::try_load<u32>(a + 1760), b,
          b ? bd::mem::try_load<u32>(b + 36) : 0, b ? bd::mem::try_load<u32>(b + 40) : 0, b ? bd::mem::try_load<u32>(b + 44) : 0,
          b ? bd::mem::try_load<u32>(b + 64) : 0, sw);
}
}  // namespace

// Study (ordinary matches): the computer's man's pad words, from wwe12_iquit.cpp's hook of sub_8225C4D8.
namespace {
void ProbeWord(u32 actor, u32 word, u32 lr);
}
void Wwe12SrefPadStudy(u32 actor, u32 word, u32 lr) {
  ProbeWord(actor, word, lr);
  if (!actor || actor != Actor(1) || !On() || Match())
    return;
  static u32 s_last = 0xFFFFFFFF;
  static u16 s_anim = 0xFFFF;
  static int s_lines = 0;
  const u16 anim = Anim(actor);
  if ((word == s_last && anim == s_anim) || s_lines >= 900)
    return;
  s_last = word, s_anim = anim;
  ++s_lines;
  float x = 0, z = 0;
  Spot(actor, x, z);
  BD_INFO("[sref] etude manette ordi : mot {:08X} motion {} a {:.0f} {:.0f} {:.0f} zone {}", word, anim, x,
          LoadF(bd::mem::try_load<u32>(actor + 768) + 4), z, bd::mem::try_load<u8>(actor + 996));
}

// ---- probe (2026-11-17): why the game's referee stands still in the ring after the hand-over ----
// His brain sends him (role 1) to the free state sub_8234FD30, which leaves at once (nothing done) on: the motion layer
// [[+3396]+68] there, (int)f[+24] == 0 and its weight f[+276] <= 0; [+1972] > 0; sub_82349648 != 0; sub_82207918 != 0.
// Past them it calls sub_8229E060, then its referee's branch. Counted for the game's referee (actor 6, role 1), an
// ordinary match's (the reference: he walks) and ours, once a second, with the motions asked for him and his pad words.
namespace {
std::atomic<u32> g_pr_actor{0};
std::atomic<u32> g_pr_free{0}, g_pr_49648{0}, g_pr_49648_hit{0}, g_pr_07918{0}, g_pr_07918_hit{0}, g_pr_9e060{0};
std::atomic<u32> g_pr_asks{0}, g_pr_ask_last{0}, g_pr_ask_lr{0}, g_pr_words{0}, g_pr_word_last{0}, g_pr_word_nz{0};
}  // namespace
REX_EXTERN(__imp__sub_82349648);
REX_EXTERN(__imp__sub_82207918);
REX_EXTERN(__imp__sub_8229E060);
extern "C" REX_FUNC(sub_82349648) {
  const u32 a = ctx.r3.u32;
  __imp__sub_82349648(ctx, base);
  if (a && a == g_pr_actor.load(std::memory_order_relaxed)) {
    g_pr_49648.fetch_add(1);
    if (ctx.r3.u32)
      g_pr_49648_hit.fetch_add(1);
  }
}
extern "C" REX_FUNC(sub_82207918) {
  const u32 a = ctx.r3.u32;
  __imp__sub_82207918(ctx, base);
  if (a && a == g_pr_actor.load(std::memory_order_relaxed)) {
    g_pr_07918.fetch_add(1);
    if (ctx.r3.u32)
      g_pr_07918_hit.fetch_add(1);
  }
}
// The probe of 2026-11-17 08:12-08:15: ours passes the free state's first gates as the ordinary match's referee does
// (sub_8229E060 reached each frame), but his pad words stay 0 (the other's: the stick) and the referee's own behaviour
// sub_823C97B0 (his walk around the fighters, his count: sub_823C9458 plays 190 / 191 when the judge waits) was never
// called for a role 1 man in any special referee match of the log. Between them the free state leaves on
// sub_8227C720 (if + 2984), sub_82B00368 (actor, pad) != 0, sub_8222F538 (actor, pad) != 0, or calls it only when his
// pad's ([+3376]) virtual + 36 says so (sub_8225B340(pad, 2, 0): "action 2" asked on his pad). Counted here. Test of
// 08:21: 8227C720 / 82B00368 0, 8222F538 never called (the pad's + 156 / + 160 0), so + 36: no "action 2" - his pad
// gets no input; called from here anyway (800 times) it wrote no word either: dropped, see g_auto_ms.
std::atomic<u32> g_pr_ai{0}, g_pr_forced{0}, g_pr_b0368{0}, g_pr_b0368_hit{0}, g_pr_2f538{0}, g_pr_2f538_hit{0};
std::atomic<u32> g_pr_7c720{0}, g_pr_7c720_hit{0};
std::atomic<int64_t> g_ai_game_ms{0};
REX_EXTERN(__imp__sub_823C97B0);
REX_EXTERN(__imp__sub_82B00368);
REX_EXTERN(__imp__sub_8222F538);
REX_EXTERN(__imp__sub_8227C720);
extern "C" REX_FUNC(sub_82B00368) {
  const u32 a = ctx.r3.u32;
  __imp__sub_82B00368(ctx, base);
  if (a && a == g_pr_actor.load(std::memory_order_relaxed)) {
    g_pr_b0368.fetch_add(1);
    if (ctx.r3.u32)
      g_pr_b0368_hit.fetch_add(1);
  }
}
extern "C" REX_FUNC(sub_8222F538) {
  const u32 a = ctx.r3.u32;
  __imp__sub_8222F538(ctx, base);
  if (a && a == g_pr_actor.load(std::memory_order_relaxed)) {
    g_pr_2f538.fetch_add(1);
    if (ctx.r3.u32)
      g_pr_2f538_hit.fetch_add(1);
  }
}
extern "C" REX_FUNC(sub_8227C720) {
  const u32 a = ctx.r3.u32;
  __imp__sub_8227C720(ctx, base);
  if (a && a == g_pr_actor.load(std::memory_order_relaxed)) {
    g_pr_7c720.fetch_add(1);
    if (ctx.r3.u32)
      g_pr_7c720_hit.fetch_add(1);
  }
}
extern "C" REX_FUNC(sub_8229E060) {
  const u32 a = ctx.r3.u32;
  if (a && a == g_pr_actor.load(std::memory_order_relaxed))
    g_pr_9e060.fetch_add(1);
  __imp__sub_8229E060(ctx, base);
}
void Wwe12SrefFreeProbe(u32 actor) {  // (from wwe12_dtprobe2.cpp's hook of sub_8234FD30)
  if (actor && actor == g_pr_actor.load(std::memory_order_relaxed))
    g_pr_free.fetch_add(1);
}
void Wwe12SrefMotionAsk(u32 actor, u32 motion, u32 lr) {  // (from wwe12_iquit.cpp's hook of sub_8247AD18)
  if (actor && actor == g_pr_actor.load(std::memory_order_relaxed)) {
    g_pr_asks.fetch_add(1);
    g_pr_ask_last = motion;
    g_pr_ask_lr = lr;
  }
}
namespace {
// (who writes his pad words: the caller's return address, how many words, how many not 0, the last not 0)
std::mutex g_pr_mu;
std::map<u32, std::array<u32, 3>> g_pr_lr;
void ProbeWord(u32 actor, u32 word, u32 lr) {
  if (actor && actor == g_pr_actor.load(std::memory_order_relaxed)) {
    g_pr_words.fetch_add(1);
    g_pr_word_last = word;
    if (word)
      g_pr_word_nz.fetch_add(1);
    std::lock_guard<std::mutex> lock(g_pr_mu);
    auto &e = g_pr_lr[lr];
    ++e[0];
    if (word)
      ++e[1], e[2] = word;
  }
}
// all his components (the lists at + 3436 / + 3440 / + 3444, as sub_827D5C28 walks them: + 20 then + 16)
std::string Components(u32 a) {
  std::string out;
  for (u32 off : {3436u, 3440u, 3444u}) {
    out += fmt::format(" [+{}]", off);
    const u32 list = bd::mem::try_load<u32>(a + off);
    std::set<u32> seen;
    for (u32 c = list, k = 0; c && k < 64 && seen.insert(c).second; c = bd::mem::try_load<u32>(c + 16), ++k) {
      const u32 t = bd::mem::try_load<u32>(c + 4), o = bd::mem::try_load<u32>(c + 24);
      out += fmt::format(" {:c}{:c}{:c}{:c}:{:08X}/{:08X}", char(t >> 24), char(t >> 16), char(t >> 8), char(t), o,
                         o ? bd::mem::try_load<u32>(o) : 0);
    }
    for (u32 c = list ? bd::mem::try_load<u32>(list + 20) : 0, k = 0; c && k < 64 && seen.insert(c).second;
         c = bd::mem::try_load<u32>(c + 20), ++k) {
      const u32 t = bd::mem::try_load<u32>(c + 4), o = bd::mem::try_load<u32>(c + 24);
      out += fmt::format(" {:c}{:c}{:c}{:c}:{:08X}/{:08X}", char(t >> 24), char(t >> 16), char(t >> 8), char(t), o,
                         o ? bd::mem::try_load<u32>(o) : 0);
    }
  }
  return out;
}
// once a second, on the main thread: the line, the counters reset
void ProbeLine(u32 actor) {
  const bool ours = Match();
  if (!actor || !On() || bd::mem::try_load<u32>(actor + 3428) != 1 || (ours && g_out != kOutDone)) {
    g_pr_actor = 0;
    return;
  }
  g_pr_actor = actor;
  static int64_t s_last = 0;
  static int s_lines[2] = {};
  if (NowMs() - s_last < 1000)
    return;
  s_last = NowMs();
  if (s_lines[ours ? 1 : 0]++ >= (ours ? 90 : 45))
    return;
  {
    std::string w;
    {
      std::lock_guard<std::mutex> lock(g_pr_mu);
      for (const auto &[lr, e] : g_pr_lr)
        w += fmt::format(" {:08X}:{}/{}/{:08X}", lr, e[1], e[0], e[2]);
      g_pr_lr.clear();
    }
    BD_INFO("[sref] sonde {} (3) : ecrivains de sa manette (adresse:non nuls/mots/dernier){} | +3424 {} +3428 {} +1760 {}",
            ours ? "notre arbitre" : "arbitre normal", w, int32_t(bd::mem::try_load<u32>(actor + 3424)),
            bd::mem::try_load<u32>(actor + 3428), bd::mem::try_load<u32>(actor + 1760));
    static int s_comp[2] = {};
    const u16 an = Anim(actor);
    if (s_comp[ours ? 1 : 0] < 4 && (an >= 20150 && an < 20160) && s_lines[ours ? 1 : 0] % 5 == 2) {
      ++s_comp[ours ? 1 : 0];
      BD_INFO("[sref] sonde {} (4) : composants{}", ours ? "notre arbitre" : "arbitre normal", Components(actor));
      // (5) his pad words come from his brain (CMIF = [+3384], made by sub_82959AA8): brain + 76 = its input object
      // (sub_8295D968: + 0 the core, sub_82978DB8, 1872 bytes; + 8 the writer's object), whose word the pad's update
      // sub_8225BD20 sends each frame (sub_829598A0 -> sub_8295D960 -> sub_8297D498: [[x + 20] + 12]). Dumped, to
      // find what differs from an ordinary match's referee.
      const auto dump = [](u32 o, u32 words) {
        std::string d;
        for (u32 i = 0; o && i < words; ++i)
          d += fmt::format("{}{:08X}", i % 8 ? " " : (i ? " | " : ""), bd::mem::try_load<u32>(o + 4 * i));
        return d;
      };
      const u32 br = bd::mem::try_load<u32>(actor + 3384), w = br ? bd::mem::try_load<u32>(br + 76) : 0;
      const u32 core = w ? bd::mem::try_load<u32>(w) : 0, x = w ? bd::mem::try_load<u32>(w + 8) : 0;
      const u32 x20 = x ? bd::mem::try_load<u32>(x + 20) : 0;
      BD_INFO("[sref] sonde {} (5) : +3468 {} +3464 {:08X} +3472 {:08X} | cerveau {:08X} : {} | entree {:08X} : {} | "
              "x {:08X} : {} | x+20 {:08X} : {}",
              ours ? "notre arbitre" : "arbitre normal", int32_t(bd::mem::try_load<u32>(actor + 3468)),
              bd::mem::try_load<u32>(actor + 3464), bd::mem::try_load<u32>(actor + 3472), br, dump(br, 24), w, dump(w, 12), x,
              dump(x, 12), x20, dump(x20, 8));
      BD_INFO("[sref] sonde {} (6) : coeur {:08X} : {}", ours ? "notre arbitre" : "arbitre normal", core, dump(core, 468));
    }
  }
  const u32 lay = bd::mem::try_load<u32>(actor + 3396), o68 = lay ? bd::mem::try_load<u32>(lay + 68) : 0;
  const u32 b = Brain(actor);
  float x = 0, z = 0;
  Spot(actor, x, z);
  const u32 pad = bd::mem::try_load<u32>(actor + 3376), vt = pad ? bd::mem::try_load<u32>(pad) : 0;
  BD_INFO("[sref] sonde {} (2) : comportement d'arbitre {} par le jeu, {} d'ici | 8227C720 {}/{} | 82B00368 {}/{} | "
          "8222F538 {}/{} | manette {:08X} classe {:08X} +36 {:08X} +156 {:08X} +160 {:08X} | +2984 {:08X} +2968 {} +2972 {} "
          "+993 {} +992 {} +2560 {}",
          ours ? "notre arbitre" : "arbitre normal", g_pr_ai.exchange(0), g_pr_forced.load(), g_pr_7c720_hit.exchange(0),
          g_pr_7c720.exchange(0), g_pr_b0368_hit.exchange(0), g_pr_b0368.exchange(0), g_pr_2f538_hit.exchange(0),
          g_pr_2f538.exchange(0), pad, vt, vt ? bd::mem::try_load<u32>(vt + 36) : 0, vt ? bd::mem::try_load<u32>(vt + 156) : 0,
          vt ? bd::mem::try_load<u32>(vt + 160) : 0, bd::mem::try_load<u32>(actor + 2984),
          int32_t(bd::mem::try_load<u32>(actor + 2968)), int32_t(bd::mem::try_load<u32>(actor + 2972)),
          bd::mem::try_load<u8>(actor + 993), bd::mem::try_load<u8>(actor + 992), bd::mem::try_load<u32>(actor + 2560));
  BD_INFO("[sref] sonde {} : motion {} a {:.0f} {:.0f} zone {} | libre {} | porte1 +24 {:.2f} couche {:08X} poids {:.2f} | "
          "+1972 {} | 82349648 {}/{} | 82207918 {}/{} | 8229E060 {} | motions demandees {} (derniere {} par {:08X}) | "
          "manette {} mots, {} non nuls, dernier {:08X} | +2746 {} cerveau +104 {} +44 {:08X} | vitesse {:.1f} {:.1f}",
          ours ? "notre arbitre" : "arbitre normal", Anim(actor), x, z, bd::mem::try_load<u8>(actor + 996), g_pr_free.exchange(0),
          LoadF(actor + 24), o68, o68 ? LoadF(o68 + 276) : -1.f, int32_t(bd::mem::try_load<u32>(actor + 1972)),
          g_pr_49648_hit.exchange(0), g_pr_49648.exchange(0), g_pr_07918_hit.exchange(0), g_pr_07918.exchange(0),
          g_pr_9e060.exchange(0), g_pr_asks.exchange(0), g_pr_ask_last.load(), g_pr_ask_lr.load(), g_pr_words.exchange(0),
          g_pr_word_nz.exchange(0), g_pr_word_last.load(), bd::mem::try_load<u8>(actor + 2746), b ? bd::mem::try_load<u32>(b + 104) : 0,
          b ? bd::mem::try_load<u32>(b + 44) : 0, LoadF(actor + 752), LoadF(actor + 760));
}
}  // namespace

// The stick of a pad word toward a point of the floor (x, z), as the I Quit referee's walk (wwe12_iquit.cpp)
namespace {
bool StickWord(u32 actor, float tx, float tz, u32 *word) {
  const u32 rp = bd::mem::try_load<u32>(actor + 768);
  if (!rp)
    return false;
  u32 info = 0;
  for (u32 a : {actor, Actor(1), Actor(0)}) {  // (the camera's angle, as wwe12_iquit.cpp reads it)
    const u32 ai = a ? bd::mem::try_load<u32>(a + 3384) : 0, w = ai ? bd::mem::try_load<u32>(ai + 76) : 0;
    info = w ? bd::mem::try_load<u32>(w) : 0;
    if (info)
      break;
  }
  const float dx = tx - LoadF(rp), dz = tz - LoadF(rp + 8);
  const float a = std::atan2(-dz, dx) - LoadF(0x82000ADC) + 4.f * 6.2831853f;
  const u32 dir8 = (((u32(a * LoadF(0x82002E4C)) & 0xFFF) + 256) >> 9) & 7;
  const u32 cam = info ? bd::mem::try_load<u16>(info + 162) : 0;
  const u32 rel = u32(int32_t((dir8 << 9) - cam + 256) >> 9) & 7;
  const u32 v = bd::mem::try_load<u32>(0x8307B34C + rel * 4);
  static constexpr u32 kCode[8] = {2, 1, 8, 3, 7, 4, 5, 6};  // sub_8298FB98, for v = 2..9
  if (v < 2 || v > 9)
    return false;
  *word = (0x80u | (((kCode[v - 2] - 1) & 7) << 4)) << 16;
  return true;
}
// The run: the game's own "go to a point" pad (sub_8228F5B8) writes the stick with the button 0x400.
constexpr u32 kRun = 0x400u;

// The game's referee in the ring (see g_auto_ms): near the fighters (170 cm from their middle, on his side), to a
// pinned man running; still in his count's motions (only his walk 2015x / free 1350 are steered).
bool RefereeWord(u32 actor, u32 *word) {
  if (!g_in_ring_ms.load() || g_walk.load() || g_climb.load() || g_out != kOutDone || g_judge_alone ||
      bd::mem::try_load<u32>(actor + 3428) != 1)
    return false;
  const u16 anim = Anim(actor);
  if (!(anim >= 20150 && anim < 20160) && anim != 1350)
    return false;
  float rx = 0, rz = 0;
  if (!Spot(actor, rx, rz))
    return false;
  float tx = 0, tz = 0, go = 110.f, stop = 60.f;
  bool run = false;
  const u32 j = bd::mem::try_load<u32>(kJudgePtr);
  const u32 victim = j && g_state.load() == 1 ? bd::mem::try_load<u32>(j + 108) : 0;
  float vx = 0, vz = 0;
  if (victim && Spot(victim, vx, vz)) {
    float ux = rx - vx, uz = rz - vz;
    const float l = std::hypot(ux, uz);
    ux = l > 1.f ? ux / l : 0.f, uz = l > 1.f ? uz / l : 1.f;
    tx = vx + ux * 80.f, tz = vz + uz * 80.f;
    go = 60.f, stop = 45.f, run = true;
  } else {
    float cx = 0, cz = 0;
    int k = 0;
    for (u32 i = 0; i < 6; ++i) {
      const u32 f = Actor(i);
      float fx, fz;
      if (f && f != actor && f != g_ex && bd::mem::try_load<u32>(f + 3428) == 0 && bd::mem::try_load<u8>(f + 996) == 0 &&
          Spot(f, fx, fz))
        cx += fx, cz += fz, ++k;
    }
    if (!k)
      return false;
    cx /= float(k), cz /= float(k);
    float ux = rx - cx, uz = rz - cz;
    const float l = std::hypot(ux, uz);
    ux = l > 1.f ? ux / l : 0.f, uz = l > 1.f ? uz / l : 1.f;
    tx = cx + ux * 170.f, tz = cz + uz * 170.f;
  }
  tx = std::clamp(tx, -225.f, 225.f), tz = std::clamp(tz, -225.f, 225.f);
  const float d = std::hypot(tx - rx, tz - rz);
  if (g_ref_moving.load() ? d < stop : d < go) {
    g_ref_moving = false;
    *word = 0;
    return true;
  }
  if (!StickWord(actor, tx, tz, word))
    return false;
  g_ref_moving = true;
  if (run || d > 260.f)
    *word |= kRun;
  return true;
}
}  // namespace

// The stick of the walking referee's pad word (see g_walk); false: the word is left as it is.
bool Wwe12SrefWalkWord(u32 actor, u32 *word) {
  if (!actor || !On() || !Match())
    return false;
  if (g_ex_walk.load() && actor == g_ex) {  // (the old referee to the ramp's side of the ring)
    float x = 0, z = 0;
    Spot(actor, x, z);
    return StickWord(actor, std::clamp(x, -120.f, 120.f), 240.f, word);
  }
  if (actor != Actor(6))
    return false;
  if (!g_walk.load())
    return RefereeWord(actor, word);
  const u32 rp = bd::mem::try_load<u32>(actor + 768);
  if (!rp)
    return false;
  u32 info = 0;
  for (u32 a : {actor, Actor(1), Actor(0)}) {  // (the camera's angle, as wwe12_iquit.cpp reads it)
    const u32 ai = a ? bd::mem::try_load<u32>(a + 3384) : 0, w = ai ? bd::mem::try_load<u32>(ai + 76) : 0;
    info = w ? bd::mem::try_load<u32>(w) : 0;
    if (info)
      break;
  }
  const int phase = g_walk_phase.load();
  if (phase >= 2)
    return false;  // (at the apron: his climb is a motion of ours, no stick)
  const float dx = 0.f - LoadF(rp), dz = WalkTargetZ(phase) - LoadF(rp + 8);
  const float a = std::atan2(-dz, dx) - LoadF(0x82000ADC) + 4.f * 6.2831853f;
  const u32 dir8 = (((u32(a * LoadF(0x82002E4C)) & 0xFFF) + 256) >> 9) & 7;
  const u32 cam = info ? bd::mem::try_load<u16>(info + 162) : 0;
  const u32 rel = u32(int32_t((dir8 << 9) - cam + 256) >> 9) & 7;
  const u32 v = bd::mem::try_load<u32>(0x8307B34C + rel * 4);
  static constexpr u32 kCode[8] = {2, 1, 8, 3, 7, 4, 5, 6};  // sub_8298FB98, for v = 2..9
  if (v < 2 || v > 9)
    return false;
  *word = ((0x80u | (((kCode[v - 2] - 1) & 7) << 4)) << 16) | kRun;  // (running, as WWE '13's)
  if (g_walk_seen.fetch_add(1) < 30)
    BD_INFO("[sref] l'arbitre du jeu marche (etape {}) : a {:.0f} {:.0f} {:.0f}, ecart {:.0f} {:.0f}, mot {:08X}",
            g_walk_phase.load(), LoadF(rp), LoadF(rp + 4), LoadF(rp + 8), dx, dz, *word);
  return true;
}

void Wwe12SrefActorFrame(const PPCContext &ctx, u8 *base, u32 actor) {
  g_main_tid = std::this_thread::get_id();
  if (actor && actor == Actor(6))
    ProbeLine(actor);
  if (actor && (actor == Actor(6) || actor == Actor(0)) && On() && !Match()) {  // (study: motions, ordinary match)
    static u32 s_key[2] = {};
    static int s_lines = 0;
    const int k = actor == Actor(6) ? 1 : 0;
    const u32 blk = bd::mem::try_load<u32>(actor + 684);
    const u32 key = (u32(Anim(actor)) << 16) ^ (blk ? bd::mem::try_load<u32>(blk) : 0);
    if (key != s_key[k] && s_lines < 400) {
      s_key[k] = key;
      ++s_lines;
      float ax = 0, az = 0;
      Spot(actor, ax, az);
      BD_INFO("[sref] etude motions {} : motion {} bloc {:08X} a {:.0f} {:.0f} {:.0f} cap {:.2f} zone {} +1096 {}",
              k ? "arbitre" : "joueur", Anim(actor), blk ? bd::mem::try_load<u32>(blk) : 0, ax,
              LoadF(bd::mem::try_load<u32>(actor + 768) + 4), az, LoadF(actor + 740), bd::mem::try_load<u8>(actor + 996),
              bd::mem::try_load<u32>(actor + 1096));
    }
  }
  if (actor && actor == Actor(6) && On() && !Match()) {  // (study: the reference, an ordinary match's referee)
    static int64_t s_last = 0;
    static int s_lines = 0;
    if (NowMs() - s_last >= 1000 && s_lines < 40) {
      s_last = NowMs();
      ++s_lines;
      RefDiag("arbitre d'un match normal", actor);
    }
  }
  if (!actor || !On() || !Match() || !REXCVAR_GET(wwe12_sref_replacement) || g_out >= kOutScene)
    return;
  {
    static u32 s_role = 99;
    const u32 n6 = Actor(6), r6 = n6 ? bd::mem::try_load<u32>(n6 + 3428) : 99;
    if (r6 != s_role) {
      BD_INFO("[sref] l'arbitre du jeu : role {} -> {} (vu par la lecture de manette ; cerveau {:08X})", s_role, r6,
              Brain(n6));
      s_role = r6;
    }
  }
  {  // (study: the two who wait, twice a second, 80 lines)
    static int64_t s_last = 0;
    static int s_lines = 0;
    if (NowMs() - s_last >= 500 && s_lines < 80) {
      s_last = NowMs();
      ++s_lines;
      std::string line;
      for (int k : {6, g_gm_place}) {
        const u32 a = k >= 0 ? Actor(u32(k)) : 0;
        float x = 0, z = 0;
        Spot(a, x, z);
        if (a)
          line += fmt::format(" | acteur {} : role {} dessine {} mis a jour {} motion {} position {:.0f} {:.0f} cerveau {:08X}", k,
                              bd::mem::try_load<u32>(a + 3428), bd::mem::try_load<u32>(a + 1752),
                              bd::mem::try_load<u32>(a + 1736), Anim(a), x, z, Brain(a));
      }
      BD_INFO("[sref] en attente :{}", line);
    }
  }
  if (g_gm_place >= 0 && actor == Actor(u32(g_gm_place))) {
    Shown(ctx, base, actor, false);
    Place(actor, 0.f, -120.f, 3200.f);
    return;
  }
  if (g_out != kOutNone || actor != Actor(6))
    return;
  if (Parked(actor) || (bd::mem::try_load<u32>(actor + 3428) == 1 && Referee() && Referee() != actor)) {
    Shown(ctx, base, actor, false);
    Place(actor, 0.f, -120.f, 3200.f);
  }
}

namespace {
void KeepOutOfSight(const PPCContext &ctx, u8 *base, u32 actor) {
  if (!actor || !On() || !Match() || !REXCVAR_GET(wwe12_sref_replacement))
    return;
  const u32 n = Actor(6);
  const bool waiting_ref = actor == n && g_out < kOutScene && Referee() && Referee() != n;
  const bool waiting_gm = g_gm_place >= 0 && actor == Actor(u32(g_gm_place)) && g_out != kOutScene;
  if (!waiting_ref && !waiting_gm)
    return;
  if (bd::mem::try_load<u32>(actor + 1752) != 0 || bd::mem::try_load<u32>(actor + 1736) != 0)
    Shown(ctx, base, actor, false);
  Place(actor, 0.f, -120.f, 3200.f);
}
}  // namespace
REX_EXTERN(__imp__sub_8225BD20);
extern "C" REX_FUNC(sub_8225BD20) {
  if (ctx.r3.u32)
    KeepOutOfSight(ctx, base, bd::mem::try_load<u32>(ctx.r3.u32 + 84));
  __imp__sub_8225BD20(ctx, base);
}
REX_EXTERN(__imp__sub_822CF768);
extern "C" REX_FUNC(sub_822CF768) {
  __imp__sub_822CF768(ctx, base);
  KeepOutOfSight(ctx, base, Actor(6));
  if (g_gm_place >= 0)
    KeepOutOfSight(ctx, base, Actor(u32(g_gm_place)));
}

// The actor's set-up (it makes his brain, 'CMIF', for a role other than 4 / 6; it runs again at the bell - user's test
// of 2026-11-16, 17:13: the waiting referee's brain made at the first frame, gone after the bell, the hand-over then
// refused). The game's referee who waits has the role 1 for the time of it.
REX_EXTERN(__imp__sub_822143D0);
extern "C" REX_FUNC(sub_822143D0) {
  const u32 n = Actor(6);
  const bool his = On() && Match() && REXCVAR_GET(wwe12_sref_replacement) && n && ctx.r3.u32 == n + 1696 && g_out < kOutScene;
  const u32 role = his ? bd::mem::try_load<u32>(n + 3428) : 0, from = u32(ctx.lr);
  // (the set-up copies the role from his participant record first - sub_8267CD30's + 8 into actor + 3428 - so the
  // role 1 is put in all three records for the time of it: with the role 6 there his brain was not made at the
  // bell's last set-up, user's tests of 2026-11-16)
  if (his && role == 6)
    SetRole(ctx, base, 6, 40, 1);
  __imp__sub_822143D0(ctx, base);
  if (his && role == 6)
    SetRole(ctx, base, 6, 40, 6);
  if (his)
    BD_INFO("[sref] l'arbitre du jeu : sa mise en place (appel {:08X}, role {}), cerveau {:08X}", from, role, Brain(n));
}

// The game's referee who waits is never drawn nor updated while he waits: every call of the game that turns an
// actor's drawing (sub_822CF850) or update (sub_822CE3F0) on for his slot is turned into "off". (User's tests of
// 2026-11-16: before the bell he was seen in the ring - the game shows its actors from several places, the
// entrance scenes among them, before any frame of ours runs. WWE '13 hides its waiting referee the same way, at
// each putting in place of the match, sub_8254C820.) Our own calls (the hand-over) go to the game's directly.
namespace {
bool WaitingSlot(u32 mgr, u32 slot) {
  if (!On() || !Match() || !REXCVAR_GET(wwe12_sref_replacement) || (g_out >= kOutScene && g_gm_place < 0) || !mgr ||
      slot >= 16)
    return false;
  const u32 index = bd::mem::try_load<u32>(mgr + (slot + 32) * 4);
  if (g_gm_place >= 0 && index == u32(g_gm_place) && Actor(index) && g_out != kOutScene)
    return true;  // (the General Manager: seen only in the scene)
  return index == 6 && g_out < kOutScene && Actor(6) && Actor(6) != Referee();
}
}  // namespace
extern "C" REX_FUNC(sub_822CF850) {
  if (ctx.r5.u32 != 0 && WaitingSlot(ctx.r3.u32, ctx.r4.u32)) {
    static int s_lines = 0;
    if (s_lines++ < 40)
      BD_INFO("[sref] dessin de la case {} refuse (appel {:08X})", ctx.r4.u32, u32(ctx.lr));
    ctx.r5.u64 = 0;
  }
  __imp__sub_822CF850(ctx, base);
}
extern "C" REX_FUNC(sub_822CE3F0) {
  if (ctx.r5.u32 != 0 && WaitingSlot(ctx.r3.u32, ctx.r4.u32) && Brain(Actor(6)) != 0)
    ctx.r5.u64 = 0;
  __imp__sub_822CE3F0(ctx, base);
}

// Every actor drawn and updated again, two frames after the match is put in place (and at other times): the game's
// referee who waits is parked if he is not yet (his brain is made by now) and hidden again. (The tick does the
// same, but it only runs once the match's HUD is drawn.)
extern "C" REX_FUNC(sub_823B8F38) {
  __imp__sub_823B8F38(ctx, base);
  if (!On() || !Match() || !REXCVAR_GET(wwe12_sref_replacement) || g_out != kOutNone)
    return;
  const u32 n = Actor(6);
  if (!n)
    return;
  if (Parked(n) || (bd::mem::try_load<u32>(n + 3428) == 1 && Referee() && Referee() != n)) {
    Shown(ctx, base, n, false);
    Place(n, 0.f, -120.f, 3200.f);
  }
  if (g_gm_place >= 0)
    if (const u32 gm = Actor(u32(g_gm_place))) {
      Shown(ctx, base, gm, false);
      Place(gm, 0.f, -120.f, 3200.f);
    }
}

// ---- he fights as a superstar (as WWE '13: a hit on him is a hit, his grapples are grapples) ----
// WWE '12 treats a man of the role 1 as the referee in its hits, grapples and damage (read 2026-11-16, the role-1
// tests that WWE '13 skips for its special referee with sub_82537D20): a strike on him is the referee's bump
// (sub_8228C610, sub_8228BE40: motions 220 / 1386), a grapple on him the referee's reaction (sub_821D3F58), his
// damage the referee's (sub_8224B790), a grapple from him refused or not resolved (sub_821D1E08, sub_8222EBF0;
// sub_82230B50). For the time of these calls he has the role 0; the judge, the counts and the rest still see 1.
namespace {
struct AsFighter {
  u32 ref = 0;
  AsFighter() {
    if (On() && Match() && g_out == kOutNone && REXCVAR_GET(wwe12_sref_actions)) {
      ref = Referee();
      if (ref)
        bd::mem::store<u32>(ref + 3428, 0);
    }
  }
  ~AsFighter() {
    if (ref)
      bd::mem::store<u32>(ref + 3428, 1);
  }
};
}  // namespace
#define SREF_AS_FIGHTER(addr)                 \
  REX_EXTERN(__imp__##addr);                  \
  extern "C" REX_FUNC(addr) {                 \
    AsFighter as_fighter;                     \
    __imp__##addr(ctx, base);                 \
  }
SREF_AS_FIGHTER(sub_8228C610)
SREF_AS_FIGHTER(sub_8228BE40)
SREF_AS_FIGHTER(sub_821D3F58)
SREF_AS_FIGHTER(sub_8224B790)
SREF_AS_FIGHTER(sub_821D1E08)
SREF_AS_FIGHTER(sub_8222EBF0)
#undef SREF_AS_FIGHTER

// Once per game frame during a match (from wwe12_iquit.cpp's tick).
void Wwe12SrefTick(const PPCContext &ctx, u8 *base) {
  g_live_ms = NowMs();
  if (!On() || !Match()) {
    g_pad = -1;
    g_step = 0;
    g_started = false;
    g_hud_want = false;
    g_ejected = false;
    g_ex = 0;
    return;
  }
  {  // (the game's referee parked - the role 6 - once his brain is made and the bell's set-up is over)
    const u32 n = Actor(6);
    if (REXCVAR_GET(wwe12_sref_replacement) && g_out == kOutNone && n && bd::mem::try_load<u32>(n + 3428) == 1 &&
        Referee() && Referee() != n) {
      SetRole(ctx, base, 6, 40, 6);
      BD_INFO("[sref] l'arbitre du jeu ({:08X}) attend hors de vue (role 6, apres la cloche ; cerveau {:08X})", n,
              Brain(n));
    }
  }
  const bool main_thread = OnMainThread();
  if (main_thread)
    HandOver(ctx, base);
  if (g_out == kOutDone && g_watch_ms && NowMs() - g_watch_ms < 60000) {
    // (study, for the walk down the ramp as in WWE '13: the motions - and their blocks - the game's referee plays on his
    // way into the ring, and the old one on his way out)
    static u32 s_key[2] = {};
    int k = 0;
    for (u32 a : {Actor(6), g_ex}) {
      const u32 st = a ? bd::mem::try_load<u32>(a + 680) : 0, blk = a ? bd::mem::try_load<u32>(a + 684) : 0;
      const u32 key = (u32(Anim(a)) << 16) ^ (blk ? bd::mem::try_load<u32>(blk) : 0) ^ (st ? 1u : 0u);
      if (a && key != s_key[k]) {
        s_key[k] = key;
        float ax = 0, az = 0;
        Spot(a, ax, az);
        const u32 q = bd::mem::try_load<u32>(a + 768);
        BD_INFO("[sref] {} : motion {} bloc {:08X} a {:.0f} {:.0f} {:.0f} cap {:.2f} zone {}", k ? "l'ancien arbitre" : "l'arbitre du jeu",
                Anim(a), blk ? bd::mem::try_load<u32>(blk) : 0, ax, q ? LoadF(q + 4) : 0.f, az, LoadF(a + 740),
                bd::mem::try_load<u8>(a + 996));
      }
      ++k;
    }
    static int64_t s_last = 0;
    if (NowMs() - s_last >= 1000) {
      s_last = NowMs();
      if (NowMs() - g_watch_ms < 20000) {
        RefDiag("l'arbitre du jeu apres le relais", Actor(6));
        RefDiag("l'ancien arbitre", g_ex);
      }
      const u32 n = Actor(6), j = bd::mem::try_load<u32>(kJudgePtr);
      float nx = 0, nz = 0;
      Spot(n, nx, nz);
      const u32 q = n ? bd::mem::try_load<u32>(n + 768) : 0;
      const u32 cmif = Brain(n);
      BD_INFO("[sref] arbitre du jeu : motion {} position {:.0f} {:.0f} {:.0f} role {} dessine {} cerveau {:08X} ordi {} | juge etat {} "
              "+112 {} | ancien : motion {} zone {} position", Anim(n), nx, q ? LoadF(q + 4) : 0.f, nz,
              n ? bd::mem::try_load<u32>(n + 3428) : 99, n ? bd::mem::try_load<u32>(n + 1752) : 99, cmif,
              cmif ? bd::mem::try_load<u32>(cmif + 64) : 99, j ? bd::mem::try_load<u32>(j + 92) : 99,
              j ? bd::mem::try_load<u32>(j + 112) : 99, g_ex ? Anim(g_ex) : 0, g_ex ? bd::mem::try_load<u8>(g_ex + 996) : 0);
    }
  }
  if (main_thread && g_out == kOutDone && g_watch_ms && Actor(6) &&
      (bd::mem::try_load<u32>(Actor(6) + 3428) == 1 || g_walk.load() || g_climb.load())) {
    static int s_help = 0;
    static int64_t s_for = 0;
    if (s_for != g_watch_ms)
      s_for = g_watch_ms, s_help = 0;
    const u32 n = Actor(6), q = bd::mem::try_load<u32>(n + 768);
    float nx = 0, nz = 0;
    Spot(n, nx, nz);
    const bool in_ring = q && std::fabs(nx) < 300.f && std::fabs(nz) < 300.f && LoadF(q + 4) < -150.f;
    const int64_t since = NowMs() - g_watch_ms;
    {
      const u16 anim = Anim(n);
      if (g_climb.load()) {
        // (his climb: over once the game gave him another motion, or after 3 s; still outside then: a short step)
        // (712 onto the apron, 750, 7010 through the ropes... - test of 20:42: over once he walks again, 2015x)
        const bool in_chain = !(anim >= 20150 && anim < 20160);
        if ((!in_chain && NowMs() - g_climb_ms > 300) || NowMs() - g_climb_ms > 12000) {  // (712 2 s, 750 3.4 s, 7010...: test of 20:47)
          g_climb = false;
          g_walk = false;
          if (!in_ring) {
            const float k = 270.f / std::max(1.f, std::max(std::fabs(nx), std::fabs(nz)));
            Place(n, nx * k, -203.f, nz * k);
            bd::mem::store<u8>(n + 996, 0);
          }
          PutInPlace(ctx, base, n);
          g_in_ring_ms = NowMs();
          BD_INFO("[sref] l'arbitre du jeu : montee {} finie (motion {}, a {:.0f} {:.0f}, {})", g_climb_anim, anim, nx, nz,
                  in_ring ? "dans le ring" : "pas entre : passe derriere les cordes");
        }
      } else if (!in_ring && g_walk.load() && g_walk_phase.load() == 1 && std::fabs(nx) < 420.f && std::fabs(nz) < 420.f) {
        g_walk_phase = 2;
        u32 m = 0;
        for (u32 c : {712u, 714u, 716u})
          if (!m && Wwe12MotionReady(ctx, base, c))
            m = c;
        // (facing the ring's middle: an actor faces (-sin h, -cos h))
        const float h = std::atan2(nx, nz);
        u32 bits;
        std::memcpy(&bits, &h, 4);
        bd::mem::store<u32>(n + 740, bits);
        g_climb = true;
        g_climb_ms = NowMs();
        g_climb_anim = m;
        if (m)
          Play(ctx, base, n, m, 0);
        BD_INFO("[sref] l'arbitre du jeu au bord du ring (a {:.0f} {:.0f}) : il monte, motion {}", nx, nz, m);
      }
    }
    if (in_ring && s_help < 3 && !g_climb.load()) {
      s_help = 3;
      g_walk = false;
      BD_INFO("[sref] l'arbitre du jeu est dans le ring ({:.1f} s apres le relais)", since / 1000.0);
    } else if (!in_ring && s_help < 2 && !g_climb.load()) {
      // (his walk: the ramp's foot reached -> into the ring; no progress for 5 s -> helped one step; 30 s -> the ring)
      static float s_best = 1e9f;
      static int64_t s_best_ms = 0, s_best_for = 0;
      const int phase = g_walk_phase.load();
      const float d = std::hypot(nx, nz - WalkTargetZ(phase));
      if (s_best_for != g_watch_ms)
        s_best_for = g_watch_ms, s_best = 1e9f, s_best_ms = NowMs();
      if (d < s_best - 20.f)
        s_best = d, s_best_ms = NowMs();
      if (phase == 0 && d < 90.f) {
        g_walk_phase = 1;
        s_best = 1e9f, s_best_ms = NowMs();
        for (u32 m : {712u, 714u, 716u})
          Wwe12MotionReady(ctx, base, m);  // (asked ahead: ready at the apron)
        BD_INFO("[sref] l'arbitre du jeu est au pied de la rampe ({:.1f} s) : il monte dans le ring", since / 1000.0);
      } else if (NowMs() - s_best_ms > 5000 || since > 30000) {
        if (phase == 0 && since <= 30000) {
          float y = -60.f, ax, az;
          for (u32 k : {7u, 8u})
            if (const u32 a = Actor(k); a && Spot(a, ax, az))
              if (const u32 aq = bd::mem::try_load<u32>(a + 768); aq && std::isfinite(LoadF(aq + 4))) {
                y = LoadF(aq + 4);
                break;
              }
          Place(n, 0.f, y, 430.f);
          bd::mem::store<u8>(n + 996, 1);
          Free(ctx, base, n);
          g_walk_phase = 1;
          s_best = 1e9f, s_best_ms = NowMs();
          BD_INFO("[sref] l'arbitre du jeu n'avance plus (a {:.0f} {:.0f}) : mis au pied de la rampe", nx, nz);
        } else {
          s_help = 2;
          g_walk = false;
          SetRole(ctx, base, 6, 40, 1);
          bd::mem::store<u8>(n + 998, 1);
          Place(n, 0.f, -203.f, 200.f);
          bd::mem::store<u8>(n + 996, 0);
          PutInPlace(ctx, base, n);
          g_in_ring_ms = NowMs();
          BD_INFO("[sref] l'arbitre du jeu n'entre pas dans le ring (a {:.0f} {:.0f}) : mis dans le ring", nx, nz);
        }
      }
    }
  }
  if (main_thread && g_ex_walk.load() && g_ex && g_out == kOutDone) {
    float x = 0, z = 0;
    Spot(g_ex, x, z);
    if (z > 150.f || NowMs() - g_ex_ms > 4000 || bd::mem::try_load<u8>(g_ex + 996) != 0) {
      g_ex_walk = false;
      PPCContext e = ctx;
      e.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
      e.r3.u64 = g_ex;
      bd::mem::store<u32>(g_ex + 2568, 0);
      __imp__sub_82371C78(e, base);
      g_left = true;
      BD_INFO("[sref] l'ancien arbitre (a {:.0f} {:.0f}) s'en va a pied (RRTR)", x, z);
    }
  }
  if (main_thread && g_out == kOutDone && g_in_ring_ms.load() && !g_judge_alone && Actor(6) &&
      bd::mem::try_load<u32>(Actor(6) + 3428) == 1 && !g_walk.load() && !g_climb.load()) {
    static int64_t s_state_ms = 0, s_last = 0;
    static u32 s_state = 0;
    const u32 n = Actor(6), j = bd::mem::try_load<u32>(kJudgePtr);
    const u32 st = j ? bd::mem::try_load<u32>(j + 92) : 0, cnt = j ? bd::mem::try_load<u32>(j + 100) : 0;
    const int64_t now = NowMs();
    if (st != s_state) {
      s_state = st;
      s_state_ms = now;
      s_last = 0;
      g_auto_ms = 0;
    }
    PPCContext c = ctx;
    c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
    if (j && st == 1) {  // a pin: down by the man (or after 1.2 s), then a count each 0.8 s
      const u32 v = bd::mem::try_load<u32>(j + 108);
      float vx = 0, vz = 0, rx = 0, rz = 0;
      const bool close_by = v && Spot(v, vx, vz) && Spot(n, rx, rz) && std::hypot(vx - rx, vz - rz) < 130.f;
      if (!g_auto_ms.load()) {
        if ((close_by || now - s_state_ms > 1200) && cnt == 0) {
          c.r3.u64 = n;
          __imp__sub_823C9458(c, base);
          g_auto_ms = now;
          s_last = now;
          BD_INFO("[sref] l'arbitre du jeu commence le compte du tombe ({} ; motion {}, {})", c.r3.u32, Anim(n),
                  close_by ? "pres du catcheur" : "pas encore pres");
        }
      } else if (Anim(n) == 190) {
        const u32 sub = bd::mem::try_load<u32>(n + 684);
        const u32 block = sub ? bd::mem::try_load<u8>(sub) : 255;
        c.r3.u64 = j;
        __imp__sub_822605A0(c, base);
        const u32 need = c.r3.u32;
        if (((block == 0 && now - s_last > 750) || (block == 1 && now - s_last > 800)) && cnt < need) {
          Play(ctx, base, n, 190, cnt + 1 >= need ? 3 : 1);
          s_last = now;
          BD_INFO("[sref] l'arbitre du jeu compte {} (sur {})", cnt + 1, need);
        }
      }
    } else if (j && (st == 3 || st == 4 || st == 5) && now - s_state_ms > 400 && now - s_last > 1000) {
      c.r3.u64 = n;  // (the 5 count, the counts outside or on the floor: one call a count)
      __imp__sub_823C9458(c, base);
      s_last = now;
      if (!g_auto_ms.load()) {
        g_auto_ms = now;
        BD_INFO("[sref] l'arbitre du jeu compte (etat du juge {}) : {}", st, c.r3.u32);
      }
    }
  }
  if (main_thread && g_judge_alone && g_out == kOutDone && Actor(6)) {  // (the count over: the referee again)
    const u32 j = bd::mem::try_load<u32>(kJudgePtr);
    if (j && bd::mem::try_load<u32>(j + 92) == 0) {
      g_judge_alone = false;
      SetRole(ctx, base, 6, 40, 1);
      BD_INFO("[sref] le compte du juge est fini : l'arbitre du jeu l'est a nouveau");
    }
  }
  if (main_thread && g_out == kOutDone && g_watch_ms && Actor(6) && bd::mem::try_load<u32>(Actor(6) + 3428) == 1 &&
      !g_walk.load() && !g_climb.load()) {
    static int64_t s_wait = 0, s_wait_for = 0;
    if (s_wait_for != g_watch_ms)
      s_wait_for = g_watch_ms, s_wait = 0;
    const u32 j = bd::mem::try_load<u32>(kJudgePtr);
    const u32 st = j ? bd::mem::try_load<u32>(j + 92) : 0, cnt = j ? bd::mem::try_load<u32>(j + 100) : 0;
    if ((st == 1 || st == 3) && cnt == 0) {
      if (!s_wait)
        s_wait = NowMs();
      if (NowMs() - s_wait > (g_in_ring_ms.load() ? 5000 : 3000)) {  // (in the ring he counts from here)
        SetRole(ctx, base, 6, 40, 6);
        s_wait = 0;
        g_judge_alone = true;
        BD_INFO("[sref] l'arbitre du jeu ne compte pas (etat du juge {}, 3 s a 0) : le juge compte seul", st);
      }
    } else {
      s_wait = 0;
    }
  }
  // (the man who was the referee: once he is outside the ring he walks away, the game's own behaviour for a man out
  // of the match - sub_82371C78, as its free state does for + 998 = 4 and the area byte + 996 = 1 or 16)
  if (g_ex)
    g_tick_ms = NowMs();
  float ex_x = 0, ex_z = 0;
  const bool ex_in_ring = g_ex && Spot(g_ex, ex_x, ex_z) && std::fabs(ex_x) < 300.f && std::fabs(ex_z) < 300.f &&
                          LoadF(bd::mem::try_load<u32>(g_ex + 768) + 4) < -150.f;
  if (main_thread && g_ex && g_out == kOutDone && ex_in_ring && NowMs() - g_ex_ms > 12000 && !g_gone) {
    // (three seconds after the hand-over and still in the ring: put outside it, on the ramp's side, at the height of
    // the people who stand at ringside - the game's own leaving walk then takes him up the ramp, and the game hides
    // him at its end. Taken far away at once, the camera, which follows the player's man, showed the stands' dark.)
    g_gone = true;
    float y = 0.f, ax, az;
    bool have = false;
    for (u32 k : {7u, 8u})
      if (const u32 a = Actor(k); !have && a && Spot(a, ax, az))
        if (const u32 q = bd::mem::try_load<u32>(a + 768); q && std::isfinite(LoadF(q + 4)))
          y = LoadF(q + 4), have = true;
    float ex, ez;
    Spot(g_ex, ex, ez);
    Place(g_ex, std::clamp(ex, -150.f, 150.f), have ? y : -60.f, 430.f);
    bd::mem::store<u8>(g_ex + 996, 1);  // (outside the ring)
    Free(ctx, base, g_ex);
    BD_INFO("[sref] l'ancien arbitre est mis hors du ring cote rampe (hauteur {:.0f}{})", have ? y : -60.f,
            have ? "" : ", estimee");
  }
  if (main_thread && g_ex && !g_left) {
    const u32 area = bd::mem::try_load<u8>(g_ex + 996);
    bool here = false;
    for (u32 k = 0; k < 6; ++k)
      here = here || Actor(k) == g_ex;
    if (here && (area == 1 || area == 16)) {
      PPCContext e = ctx;
      e.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
      e.r3.u64 = g_ex;
      bd::mem::store<u32>(g_ex + 2568, 0);
      __imp__sub_82371C78(e, base);
      g_left = true;
      BD_INFO("[sref] l'ancien arbitre est sorti du ring (zone {}) : il s'en va", area);
    }
  }
  const u32 judge = bd::mem::try_load<u32>(kJudgePtr), ref = Referee();
  const u32 state = judge ? bd::mem::try_load<u32>(judge + 92) : 0, count = judge ? bd::mem::try_load<u32>(judge + 100) : 0;
  g_state = state;
  // who is who (logged when it changes): each actor's role, pad number and "is he the computer's" as far as known
  {
    std::string line;
    for (u32 i = 0; i < 9; ++i) {
      const u32 a = Actor(i);
      line += a ? fmt::format(" {}:r{}/p{}", i, bd::mem::try_load<u32>(a + 3428), bd::mem::try_load<u32>(a + 1760)) : " -";
    }
    line += fmt::format(" | juge +112 = {}", judge ? bd::mem::try_load<u32>(judge + 112) : 99);
    if (ref)
      line += fmt::format(" | arbitre +998 = {} +2560 = {}", bd::mem::try_load<u8>(ref + 998), bd::mem::try_load<u32>(ref + 2560));
    if (main_thread && line != g_last_roles) {  // (a std::string: written by one thread only)
      BD_INFO("[sref] acteurs (role / manette) :{}", line);
      g_last_roles = line;
    }
  }
  if (state != g_last_state || count != g_last_count) {
    BD_INFO("[sref] juge : etat {} compte {} victime {:08X} (arbitre {:08X}, motion {})", state, count,
            judge ? bd::mem::try_load<u32>(judge + 108) : 0, ref, Anim(ref));
    g_last_state = state;
    g_last_count = count;
  }
  if (const u16 a = Anim(ref); a != g_last_anim) {
    static int s_lines = 0;
    if (s_lines++ < 3000)
      BD_INFO("[sref] l'arbitre : motion {} -> {} (etat du juge {})", g_last_anim, a, state);
    g_last_anim = a;
  }
  if (!ref) {
    g_pad = -1;
    return;
  }
  const u32 pad = bd::mem::try_load<u32>(ref + 1760);
  g_pad = pad < 4 ? int(pad) : -1;  // (seen 2026-11-16: 0 for the first pad, 6 for a man of the computer's)
  if (g_t < 100000)
    ++g_t;
  // the gauge (a played referee only)
  if (REXCVAR_GET(wwe12_sref_gauge) && pad < 4) {
    const int64_t now = NowMs();
    const double dt = std::clamp(double(now - g_prev_ms) / 1000.0, 0.0, 0.1);
    g_prev_ms = now;
    if (!g_started) {
      g_started = true;
      g_gauge = 0.5f;
      g_faults = 0;
      g_draining = false;
      g_duty = 0xFFFFFFFF;
      g_hit = false;
      g_y = false;
      g_act = kActNone;
      g_rm = {};
      g_tm = {};
      g_nm = {};
      g_out = kOutNone;
      g_scene_ran = false;
      g_walk = false;
      g_climb = false;
      g_gone = false;
      g_foul_by = 0;
      g_hold_grace = 3.0;
      g_hold_drain = false;
      g_fin_given = 0;
      g_a_game = false;
      std::memset(g_strike, 0, sizeof(g_strike));
      BD_INFO("[sref] jauge : debut du match, 0.50");
    }
    const u16 anim = Anim(ref);
    const u32 sub0 = bd::mem::try_load<u32>(ref + 684);
    const u32 block0 = sub0 ? bd::mem::try_load<u8>(sub0) : 255;
    const bool duty = state == 1 || state == 3 || state == 4 || state == 5;
    const bool doing = (state == 1 && anim == 190) || ((state == 4 || state == 5) && anim == 191) || (state == 3 && anim == 200);
    if (state != g_duty) {
      g_duty = state;
      g_grace = state == 1 ? 3.0 : 5.0;
      g_draining = false;
    }
    float v = g_gauge.load();
    if (duty && !doing) {
      g_grace -= dt;
      if (g_grace <= 0) {
        if (!g_draining) {
          g_draining = true;
          ++g_faults;
          BD_INFO("[sref] jauge : il ne fait pas son travail (etat du juge {}), elle baisse ; {} fautes", state, g_faults);
        }
        v -= float(0.02 * dt);
      }
    } else {
      g_draining = false;
      if (duty && state != 1)
        g_grace = 5.0;
    }
    if (!g_draining && !g_hold_drain && bd::mem::try_load<u32>(ref + 2536) == 0xFFFFFFFFu)
      v += float(dt / 150.0);
    g_gauge = std::clamp(v, 0.f, 1.f);
    // (WWE '13 takes 0.2 for a pin count he gives up himself, with its cancel button. Here the count's "given up"
    // block is only played by the game - a kick-out, a rope break: charged, it was a fault he had not made, user's
    // test of 2026-11-16. No cancel button of ours yet, so no fault.)
    g_last_block = anim == 190 ? block0 : 255;
    if (g_hit.exchange(false))
      Cost(0.1f, "son coup touche un catcheur");
    {  // (where he is on screen: the bar follows him, as WWE '13's)
      // (under him, at his feet, as WWE '13 - user, 2026-11-16; the feet as the game's own icon there takes them:
      // half-way between the bones at + 196 and + 180, the lowest of six bones - the world's y points down)
      float a[3], b[3], feet[3], sx = -1.f, sy = -1.f;
      bool ok = Bone(ref, 196, a) && Bone(ref, 180, b);
      if (ok) {
        feet[0] = (a[0] + b[0]) * 0.5f, feet[2] = (a[2] + b[2]) * 0.5f, feet[1] = -1.0e9f;
        for (u32 o : {176u, 192u, 132u, 148u, 164u, 144u})
          if (Bone(ref, o, a))
            feet[1] = std::max(feet[1], a[1]);
        ok = feet[1] > -1.0e8f && OnScreen(ctx, base, feet, sx, sy);
      }
      if (!ok)
        sx = sy = -1.f;
      g_sx = sx;
      g_sy = sy;
    }
    g_hud_want = true;
    g_tick_ms = now;
    if (!g_hud_shown.exchange(true))
      Wwe12OverlayPost([] { ShowHud(); });
    if (REXCVAR_GET(wwe12_sref_actions) && g_out == kOutNone)
      RefActions(ctx, base, ref, state, dt, now);
    // empty: he is the referee no more. Without a man of the role 1 the judge counts by itself (sub_82265710 sets
    // its + 112 to 0 the next frame); his + 998 = 4 keeps him out of the pins (sub_82261C28 wants 0 on both men).
    // The role is also put in his participant record and his creation record, which the game copies back at times.
    if ((g_gauge.load() <= 0.f || g_faults >= 30) && REXCVAR_GET(wwe12_sref_replacement) && Parked(Actor(6)) &&
        Brain(Actor(6)) != 0) {
      if (g_out == kOutNone) {
        g_out = kOutWait;
        g_out_t = 0;
        g_ejected_ms = now;
        g_sref_man = ref;
        BD_INFO("[sref] jauge vide ({:.2f}, {} fautes) : l'arbitre du jeu va le remplacer", g_gauge.load(), g_faults);
      }
    } else if (g_gauge.load() <= 0.f || g_faults >= 30) {
      u32 index = 0;
      for (u32 k = 0; k < 6; ++k)
        if (Actor(k) == ref)
          index = k;
      bd::mem::store<u32>(ref + 3428, 0);
      bd::mem::store<u8>(ref + 998, 4);
      {
        PPCContext e = ctx;
        e.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
        e.r3.u64 = index;
        __imp__sub_823F8270(e, base);
        __imp__sub_8267CD30(e, base);
        if (e.r3.u32 && bd::mem::try_load<u32>(e.r3.u32 + 8) == 1)
          bd::mem::store<u32>(e.r3.u32 + 8, 0);
      }
      if (const u32 made = bd::mem::try_load<u32>(0x8322DD44))
        if (bd::mem::try_load<u8>(made + 528 + 80 * index + 6) == 1)
          bd::mem::store<u8>(made + 528 + 80 * index + 6, 0);
      g_ejected = true;
      g_ejected_ms = now;
      g_ex = ref;
      g_left = false;
      g_pad = -1;
      BD_INFO("[sref] jauge vide ({:.2f}, {} fautes) : l'acteur {} n'est plus l'arbitre, le juge compte seul", g_gauge.load(),
              g_faults, index);
      return;
    }
  }
  if (!g_press.exchange(false) || !REXCVAR_GET(wwe12_sref_own_count) || g_out != kOutNone)
    return;
  if (state != 1 && state != 3 && state != 4 && state != 5)
    return;
  PPCContext c = ctx;
  c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
  const u32 sub = bd::mem::try_load<u32>(ref + 684);
  const u32 block = sub ? bd::mem::try_load<u8>(sub) : 255;
  if (state == 1 && Anim(ref) == 190) {  // he is counting a pin: the next count
    c.r3.u64 = judge;
    __imp__sub_822605A0(c, base);
    const u32 need = c.r3.u32;
    if ((block == 0 && g_t > 40) || (block == 1 && g_t > 18)) {
      BD_INFO("[sref] A : compte suivant (bloc {} depuis {} images, compte {} sur {})", block, g_t, count, need);
      Play(ctx, base, ref, 190, count + 1 >= need ? 3 : 1);
    } else {
      BD_INFO("[sref] A : trop tot ou bloc {} (depuis {} images, compte {} sur {})", block, g_t, count, need);
    }
    return;
  }
  if (g_t <= 50 && state != 1)
    return;
  c.r3.u64 = ref;
  __imp__sub_823C9458(c, base);
  BD_INFO("[sref] A : debut du compte par le jeu (etat {}) -> {} ; l'arbitre joue la motion {} bloc {}, +2560 = {}", state,
          c.r3.u32, Anim(ref), block, bd::mem::try_load<u32>(ref + 2560));
  g_t = 0;
}

// (study: the game's own "action 2" of the referee, to learn which button it is on)
extern "C" REX_FUNC(sub_823C97B0) {
  if (ctx.r3.u32 && ctx.r3.u32 == g_pr_actor.load(std::memory_order_relaxed)) {
    g_pr_ai.fetch_add(1);
    g_ai_game_ms = NowMs();
  }
  if (On() && Match() && ctx.r3.u32 && bd::mem::try_load<u32>(ctx.r3.u32 + 3428) == 1) {
    static std::atomic<int> s_lines{0};
    const u32 pad = bd::mem::try_load<u32>(ctx.r3.u32 + 3376);
    if (s_lines.fetch_add(1) < 200)
      BD_INFO("[sref] le jeu : bouton d'action de l'arbitre (mot de la manette {:08X}, etat du juge {})",
              pad ? bd::mem::try_load<u32>(pad + 52) : 0, g_state.load());
  }
  __imp__sub_823C97B0(ctx, base);
}

