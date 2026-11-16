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

#include <atomic>
#include <cstring>
#include <string>

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

REXCVAR_DEFINE_INT32(wwe12_sref_from, 13, "WWE12",
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

bool On() { return REXCVAR_GET(wwe12_sref) && bd::mem::ready(); }
bool Match() { return bd::mem::try_load<u8>(kMatch) == kRule; }
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

// The character select set up for a match's rule, from the menus (sub_82548818: PLAY's rows). The select keeps the
// rule (its entry + 0, the record at + 1592, the layout + 1588, the people + 92) and hands it to the match's setup
// (sub_828AD3A0 -> sub_82A39AC0) once everyone is chosen. Changed there only (user's test of 2026-11-15, 22:13), the
// match had the kind 83 but three fighters and the normal referee: the places had been made from TRIPLE THREAT's
// rule. So the rule is changed here, before the places are made, with rule 83's own options (r5 = 0).
extern "C" REX_FUNC(sub_828AE798) {
  const u32 select = ctx.r3.u32, rule = ctx.r4.u32;
  const bool ours = On() && int(rule) == REXCVAR_GET(wwe12_sref_from);
  if (On())
    BD_INFO("[sref] ecran de choix prepare pour la regle {}{} (options {:08X}, appel {:08X})", rule, ours ? " -> 83" : "",
            ctx.r5.u32, u32(ctx.lr));
  if (ours) {
    Layout();
    ctx.r4.u64 = kRule;
    ctx.r5.u64 = 0;
  }
  __imp__sub_828AE798(ctx, base);
  if (ours) {
    const u32 e = select + bd::mem::try_load<u32>(select + 15960) * 1596;
    BD_INFO("[sref] ecran de choix : regle {} gabarit {} personnes {}", bd::mem::try_load<u32>(e), bd::mem::try_load<u32>(e + 1588),
            bd::mem::try_load<u32>(e + 92));
  }
}

// A place's kind. The rule's own kinds (83: the third place 1, the referee) are set by sub_82A39AC0, but the select
// then sets each chosen place's kind itself (sub_828AD3A0: 0 a fighter, 2 a manager, 3...), so the third man was a
// fighter and the normal referee was made (user's test of 2026-11-15, 22:17: roles 0 0 0, referee in slot 6).
// In a rule-83 setup the third place keeps the kind 1; the game then makes no normal referee (sub_82A37378).
extern "C" REX_FUNC(sub_82A36150) {
  if (On() && ctx.r3.u32 && bd::mem::try_load<u32>(ctx.r3.u32 + 8) == kRule && ctx.r4.u32 == 2 && ctx.r5.u32 == 0) {
    ctx.r5.u64 = 1;
    BD_INFO("[sref] la 3e place : genre 0 -> 1 (l'arbitre), appel {:08X}", u32(ctx.lr));
  }
  __imp__sub_82A36150(ctx, base);
}

// (the match set up from the select's rule: logged)
extern "C" REX_FUNC(sub_82A39AC0) {
  if (On())
    BD_INFO("[sref] match prepare avec la regle {} (options {:08X}, appel {:08X})", ctx.r4.u32, ctx.r5.u32, u32(ctx.lr));
  __imp__sub_82A39AC0(ctx, base);
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
  if (!On() || !REXCVAR_GET(wwe12_sref_own_count) || !Match() || g_pad.load() != int(user))
    return 0;
  const u32 st = g_state.load();
  if (st != 1 && st != 3 && st != 4 && st != 5)
    return 0;
  if (buttons & ~before & kPadA)
    g_press = true;
  return kPadA;
}

// Once per game frame during a match (from wwe12_iquit.cpp's tick).
void Wwe12SrefTick(const PPCContext &ctx, u8 *base) {
  if (!On() || !Match()) {
    g_pad = -1;
    g_step = 0;
    return;
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
  g_pad = pad < 4 ? int(pad) : 0;  // (not a pad's number: the first pad, until the log says what it holds)
  if (g_t < 100000)
    ++g_t;
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

