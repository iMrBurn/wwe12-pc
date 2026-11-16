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

#include <fmt/format.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REX_EXTERN(__imp__sub_8247AD18);  // play a motion on an actor
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

REXCVAR_DEFINE_BOOL(wwe12_sref_replacement, true, "WWE12",
                    "Special Guest Referee: the game's own referee waits out of sight and takes over when the gauge is "
                    "empty (as WWE '13).");
REX_EXTERN(__imp__sub_82A37378);  // the match's setup: the normal referee kept or not (setup manager)
REX_EXTERN(__imp__sub_823B5D18);  // the match put back in place (its actors' flags by role)
REX_EXTERN(__imp__sub_822CF850);  // an actor drawn or not (actors' manager [0x8317434C], its slot, on)
REX_EXTERN(__imp__sub_822CE3F0);  // an actor updated or not (manager, slot, on, 0)
REX_EXTERN(__imp__sub_82212140);  // an actor the computer's (actor, on)
REX_EXTERN(__imp__sub_823B8F38);  // every actor drawn and updated again (after the match is put in place)
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
std::atomic<bool> g_a_game{false};  // A is the game's (a grapple): a fighter within reach, no hold, no count
u32 g_fin_given = 0, g_fin_before = 0;  // a finisher given for a full gauge (WWE '13's "special skill": 0.75)
std::atomic<int> g_prompt{0};
void RefActions(const PPCContext &ctx, u8 *base, u32 ref, u32 state, double dt, int64_t now);
// (the normal referee waiting, and the hand-over: below)
enum { kOutNone, kOutWait, kOut2050, kOutArrive, kOutDone };
int g_out = kOutNone;
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
extern "C" REX_FUNC(sub_82A36150) {
  if (On() && g_arm && g_cur >= 0 && g_cur < int(g_rows.size()) && ctx.r4.u32 == g_rows[size_t(g_cur)].fighters &&
      ctx.r5.u32 == 0) {
    ctx.r5.u64 = 1;
    g_committed = true;
    BD_INFO("[sref] la place {} : genre 0 -> 1 (l'arbitre), appel {:08X}", ctx.r4.u32, u32(ctx.lr));
  }
  __imp__sub_82A36150(ctx, base);
}

// The match set up from a rule: ours only when it comes from the select's commit (sub_828AD3A0, its call returns
// to 0x828AD3D0) of a select set up for a row of ours - never a Universe's or a story's match.
extern "C" REX_FUNC(sub_82A39AC0) {
  if (On()) {
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
        if (!part[n] || part[n] == ref || bd::mem::try_load<u32>(part[n] + 16) == 51200)
          continue;
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
  if (buttons & ~before & 0x8000)
    g_y = true;  // (Y: a disqualification pronounced, the screw job)
  const u32 st = g_state.load();
  const bool counting = st == 1 || st == 3 || st == 4 || st == 5;
  // (WWE '13: the special referee's grapple button is always a referee's action, never a grapple of his own)
  // (WWE '13: from close to a fighter, 200 cm or less, his grapple button is a real grapple - the tick says when)
  const bool ours = counting ? REXCVAR_GET(wwe12_sref_own_count) : REXCVAR_GET(wwe12_sref_actions) && !g_a_game.load();
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
    if (idle > 250)
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
        static const char *const kPrompt[5] = {"", "A  COUNT", "A  CHECK THE SUBMISSION", "Y  DISQUALIFY", "Y  SCREW JOB"};
        const int p = g_prompt.load();
        if (p > 0 && p < 5) {
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
  const u32 actor = m.actor, to = m.to;
  m = {};
  if (to)
    Play(ctx, base, actor, to, 0);
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
  const u32 idle = 20152;  // (the motion after ours: standing)

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
        if (idx >= 0 && !DownAnim(Anim(g_target)) && !in_hold && Anim(g_target) < 13000) {
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
          const u32 back = Anim(g_target);
          float rx, rz, tx, tz;
          bool behind = false;
          if (Spot(ref, rx, rz) && Spot(g_target, tx, tz)) {
            const float h = LoadF(g_target + 740);
            behind = (-std::sin(h)) * (rx - tx) + (-std::cos(h)) * (rz - tz) < 0.f;
          }
          if (live >= 3) {
            Face(g_target, ref);
            Start(g_tm, g_target, W(2024), 0, 287, back);
            g_argue = true;
            g_argue_t = 0;
            BD_INFO("[sref] avertissement : le catcheur {} conteste (3e avertissement)", idx);
          } else {
            Start(g_tm, g_target, W(behind ? 2021 : 2022), 0, behind ? 191 : 261, back);
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
          Start(g_rm, ref, W(2026), 0, 152, idle);
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
      } else if (g_check_block <= 1 && !hold && !g_rm.waiting) {
        Start(g_rm, ref, m, 2, len[2], idle);
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
    const bool free = IdleAnim(anim);
    if (y && dq_open && (free || anim <= 1)) {
      int winner = -1;
      for (u32 k = 0; k < 6 && winner < 0; ++k)
        if (const u32 o = Actor(k); o && o != offender && o != ref && bd::mem::try_load<u32>(o + 3428) == 0)
          winner = int(k);
      BD_INFO("[sref] disqualification prononcee : l'acteur {} perd, le catcheur {} gagne", IndexOf(offender), winner);
      Start(g_rm, ref, W(anim == 0 ? 2041 : anim == 1 ? 2042 : 2040), 0, anim == 0 ? 621 : anim == 1 ? 536 : 231,
            anim <= 1 ? 0 : idle);
      g_act = kActDq;
      g_act_t = 0;
      g_foul_by = 0;
      EndMatch(ctx, base, offender, 7, winner);
    } else if (y && hold && g_gauge.load() >= 1.f && free) {
      g_by = by, g_held = held;
      Start(g_rm, ref, W(2029), 0, 306, idle);
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
        Start(g_rm, ref, W(2020), 0, 183, idle);
        g_act = kActCaution;
        BD_INFO("[sref] avertissement au catcheur {} (a {:.0f})", IndexOf(target), std::sqrt(best));
      } else {
        Start(g_rm, ref, W(2049), 0, 151, idle);
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
  g_prompt = dq_open ? 3 : hold && g_gauge.load() >= 1.f ? 4 : hold && g_act != kActCheck ? 2 : counting && !in_count ? 1 : 0;
}
}  // namespace

// ---- the game's own referee waits, and takes over when the gauge is empty (WWE '13's 'SRLV', read 2026-11-16) ----
// WWE '13 makes a spare referee at the match's setup, parks him with the role 6 and hides him; when the special
// referee's gauge is empty: he plays 2050; 180 frames later the spare one appears in the ring's middle playing 2043
// while he plays 2051; when 2043 is over the spare one has the role 1 and he the role 0, and must leave.
// In WWE '12 (read the same day):
// - the match's setup drops the normal referee when a place of the kind 1 holds a superstar (sub_82A37378). For
//   that call the special referee's place has the kind 0, so the normal referee is made as in any match - with the
//   role 1, which he needs when he is made: the actor's brain ('CMIF', actor + 3384) is not built for the roles
//   4 and 6 (sub_822143D0);
// - he is then parked: the role 6 (never a target, not counted, his pad silent) in his actor, his participant
//   record (40) and his creation record (slot 6) - before the match is put in place (sub_823B5D18), which gives the
//   referee's start place to the last man of the role 1 - and each tick not drawn, not updated, "not there"
//   (his flags' first word) and far up the ramp, where the game leaves the men who went away;
// - the hand-over: he is shown in the ring's middle playing 2043, then has the role 1 and is the computer's; the
//   special referee has the role 0 and + 998 = 4 (out of the pins), and walks away once outside the ring.
namespace {
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
// (the special referee's participant number: his actors' index, as the game maps them for the six places)
u32 ParticipantOf(const PPCContext &ctx, u8 *base, u32 index) {
  PPCContext c = ctx;
  c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
  c.r3.u64 = index;
  __imp__sub_823F8270(c, base);
  return c.r3.u32;
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
    if (bd::mem::try_load<u32>(n + 3428) == 1 && bd::mem::try_load<u32>(n + 3384) != 0 && Referee() && Referee() != n) {
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
  const u32 s3 = g_ex ? g_ex : Referee();
  const int sn = Step(ctx, base, g_nm, dt);
  if (s3)
    Step(ctx, base, g_rm, dt);
  g_out_t += dt;
  if (g_out == kOutWait) {  // (once he stands free: 2050)
    if (s3 && IdleAnim(Anim(s3)) && !g_rm.anim) {
      g_act = kActNone;
      g_tm = {};
      Start(g_rm, s3, W(2050), 0, 251, 20152);
      g_out = kOut2050;
      g_out_t = 0;
      g_ex = 0;
      BD_INFO("[sref] remplacement : l'arbitre special joue 2050");
    } else if (g_out_t > 20.0) {
      g_out_t = 0;  // (still busy: tried again)
    }
  } else if (g_out == kOut2050 && g_out_t >= 3.0) {  // (180 frames: the game's referee comes)
    SetRole(ctx, base, 6, 40, 6);
    Shown(ctx, base, n, true);
    for (u32 o : {8u, 88u, 144u})
      bd::mem::store<u32>(n + 1360 + o, 0);
    bd::mem::store<u32>(n + 1360 + 16, 1);
    std::memcpy(bd::mem::at<u8>(n + 1028), bd::mem::at<u8>(n + 1360), 332);
    bd::mem::store<u8>(n + 998, 1);
    Place(n, 0.f, -120.f, 0.f);
    bd::mem::store<u32>(n + 740, 0);
    Start(g_nm, n, W(2043), 0, 621, 20152);
    if (s3)
      Start(g_rm, s3, W(2051), 0, 331, 20152);
    g_out = kOutArrive;
    g_out_t = 0;
    BD_INFO("[sref] remplacement : l'arbitre du jeu arrive (2043), l'arbitre special joue 2051");
  } else if (g_out == kOutArrive && (sn == 2 || sn == 3 || sn == 0 || g_out_t > 12.0) && !g_nm.waiting) {
    // the hand-over
    SetRole(ctx, base, 6, 40, 1);
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
      g_ex = s3;
      g_left = false;
    }
    g_ejected = true;
    g_ejected_ms = now;
    g_pad = -1;
    g_out = kOutDone;
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
    if (n && bd::mem::try_load<u32>(n + 3384) != 0) {
      if (g_out == kOutDone && g_ex) {  // (a rematch after a hand-over)
        const int index = IndexOf(g_ex);
        if (index >= 0)
          SetRole(ctx, base, u32(index), ParticipantOf(ctx, base, u32(index)), 1);
        g_ex = 0;
        g_out = kOutNone;
        g_started = false;
        g_ejected = false;
      }
      if (g_out == kOutNone && Referee() && Referee() != n)
        SetRole(ctx, base, 6, 40, 6);
    } else if (n && bd::mem::try_load<u32>(n + 3428) == 1 && Referee() && Referee() != n) {
      // (his brain is not made yet: the role 6 for this call only, so the special referee has the start place;
      // the role 1 again after it, which the brain's making wants)
      bd::mem::store<u32>(n + 3428, 6);
      __imp__sub_823B5D18(ctx, base);
      bd::mem::store<u32>(n + 3428, 1);
      return;
    }
  }
  __imp__sub_823B5D18(ctx, base);
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
  if (bd::mem::try_load<u32>(n + 3428) == 1 && bd::mem::try_load<u32>(n + 3384) != 0 && Referee() && Referee() != n) {
    SetRole(ctx, base, 6, 40, 6);
    BD_INFO("[sref] l'arbitre du jeu ({:08X}) attend hors de vue (role 6, mise en place)", n);
  }
  if (Parked(n)) {
    Shown(ctx, base, n, false);
    bd::mem::store<u8>(n + 998, 1);
    Place(n, 0.f, -120.f, 3200.f);
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
  if (!On() || !Match()) {
    g_pad = -1;
    g_step = 0;
    g_started = false;
    g_hud_want = false;
    g_ejected = false;
    g_ex = 0;
    return;
  }
  HandOver(ctx, base);
  // (the man who was the referee: once he is outside the ring he walks away, the game's own behaviour for a man out
  // of the match - sub_82371C78, as its free state does for + 998 = 4 and the area byte + 996 = 1 or 16)
  if (g_ex)
    g_tick_ms = NowMs();
  if (g_ex && !g_left) {
    const u32 area = bd::mem::try_load<u8>(g_ex + 996);
    bool here = false;
    for (u32 k = 0; k < 6; ++k)
      here = here || Actor(k) == g_ex;
    if (here && (area == 1 || area == 16)) {
      PPCContext e = ctx;
      e.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
      e.r3.u64 = g_ex;
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
    if (line != g_last_roles) {
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
    if ((g_gauge.load() <= 0.f || g_faults >= 30) && REXCVAR_GET(wwe12_sref_replacement) && Parked(Actor(6))) {
      if (g_out == kOutNone) {
        g_out = kOutWait;
        g_out_t = 0;
        g_ejected_ms = now;
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
  if (!g_press.exchange(false) || !REXCVAR_GET(wwe12_sref_own_count))
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
  if (On() && Match() && ctx.r3.u32 && bd::mem::try_load<u32>(ctx.r3.u32 + 3428) == 1) {
    static std::atomic<int> s_lines{0};
    const u32 pad = bd::mem::try_load<u32>(ctx.r3.u32 + 3376);
    if (s_lines.fetch_add(1) < 200)
      BD_INFO("[sref] le jeu : bouton d'action de l'arbitre (mot de la manette {:08X}, etat du juge {})",
              pad ? bd::mem::try_load<u32>(pad + 52) : 0, g_state.load());
  }
  __imp__sub_823C97B0(ctx, base);
}

