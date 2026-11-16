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
#include <cstring>
#include <string>
#include <vector>

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

void Wwe12SrefMenu(PPCContext &ctx) {
  if (!On())
    return;
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
       {{child(one, 41041), 41041, kRule},  // NORMAL MATCH: the game's own SPECIAL REFEREE rule
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

