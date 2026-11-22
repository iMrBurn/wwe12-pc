// Dev (2.0, step 0 of docs/etude_plus_de_6_catcheurs.md, off by default): 8 superstars in one match, without making
// any of the game's lists longer. wwe12_dev_eight_wrestlers = true turns the match's two extra characters of slots
// 7 and 8 (role 4: two characters outside the ring, numbers 150 / 153 in the logs of 2026-11-07) into superstars.
//
// What the game does (read in the code, 2026-11-22):
// - sub_822D23A0(loader, slot, desc, ..) asks for each of the match's 9 characters (hooked in wwe12_discord.cpp, which
//   calls Wwe12EightWrestlersDesc first). desc is an 80-byte record: +0 u32 superstar x 100 + attire, +4 the person
//   (an index for sub_8267CD30: 0-5 superstars, 40 the referee, 41-42 the extra two), +5 the slot, +6 the role
//   (0 superstar, 1 referee, 3 run-in, 4 extra), +8 1 for superstars and the referee, 0 for the extra two, +16 the
//   character class (1 = human for all nine), +20 the heap the character is made in (30 + slot), +32..63 where it stands.
// - sub_822D0618 makes the character: with role 4 most of a fighter's parts are left out (the branch on "role == 4"),
//   with role 0 it gets them all (moves, AI, damage...). Same class, same size.
// - sub_822CE4A0 fills the match's character list 0x8317B770 (9 pointers) from the same records.
// - Heaps (table 0x83073260): 30-35 = 1152 KB (slots 0-5), 36 = 544 KB (referee), 37-38 = 492 KB (slots 7-8). The test
//   launcher grows 37 and 38 by 660 KB (wwe12_heap_grow) so a superstar fits as in slots 0-5.
// - The referee and the two extra characters are made at the first match after start-up and kept: the 8-superstar
//   match must be the first one played after launching.
//
// What this changes (only with the cvar on, only for slots 7-8 still at role 4): +0 = the superstar of
// wwe12_dev_eight_ids, +6 = 0, +8 / +9 copied from slot 1's record, and (wwe12_dev_eight_place) the place +32..63 from
// slot 2's / 3's. Done in the record sub_822D23A0 gets, in the participant table's entry if it is another record, and
// again when sub_822D0618 makes the character (hooked here).
// Test 1 (2026-11-22, 3v3 first match): the records were changed when asked, but slots 7-8 still came out as role 4
// characters (+3428 = 4, outside the ring at y 0, z -880): what makes them is logged since ("[eight] making / made").
// Log lines, to measure: "[eight] slot N: ..." for each character asked (before / after), then every 10 s
// "[eight] list: ..." (the 9 characters: role +3428, position) and "[eight] heaps: ..." (the 9 characters' heaps).
// The frame rate is in the game's own "[perf]" lines (every 10 s); heap failures in "[heaps]" (wwe12_heap_probe).

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include <fmt/format.h>
#include <rex/cvar.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_dev_eight_wrestlers, false, "WWE12",
                    "Dev: the match's two extra characters (slots 7-8) become superstars (8 in the ring). First match "
                    "after start-up only.");
REXCVAR_DEFINE_BOOL(wwe12_dev_eight_place, true, "WWE12",
                    "Dev: wwe12_dev_eight_wrestlers' two superstars start where slots 2 / 3 start (else outside the "
                    "ring, as the extra characters).");
REXCVAR_DEFINE_BOOL(wwe12_dev_eight_force, true, "WWE12",
                    "Dev: every 100 ms, slots 7-8 put back to role 0 (their record and their character) when the game "
                    "has made them role 4 again.");
REXCVAR_DEFINE_STRING(wwe12_dev_eight_persons, "", "WWE12",
                      "Dev: the \"person\" (+4 of the record) given to slots 7-8, \"slot7,slot8\" (e.g. \"4,5\"); empty = "
                      "keep theirs (41, 42: the commentators').");
REXCVAR_DEFINE_STRING(wwe12_dev_eight_ids, "10202,10702,13102,16102,27602,27902,32202,32502,12302,13902", "WWE12",
                      "Dev: superstars for wwe12_dev_eight_wrestlers (superstar x 100 + attire): slots 7 and 8 take the "
                      "first two of this list that are not already in the match.");

namespace {

constexpr u32 kList = 0x8317B770;       // the match's 9 characters
constexpr u32 kHeapTable = 0x83232A90;  // -> heap descriptors, 36 bytes each {start, end, ...}
constexpr u32 kDescSize = 80;

std::array<std::atomic<u32>, 9> g_desc{};  // the last record asked for each slot
std::array<std::atomic<u32>, 9> g_made{};  // the character sub_822D0618 made for each slot
std::atomic<bool> g_thread{false};
// Try 4 (2026-11-22): the "first match" was the title screen's demo match (a 1 on 1: slots 0, 1, 6, 7, 8), changed
// too; then, while the player's own match was being set up, the getter hook below changed entries 7-8 again with that
// demo's records, and the loading froze. Now: only a match whose slots 2-5 are asked too (6 superstars), everything
// forgotten when a match asks for slot 0, and the getter only acts in the 10 s after slots 7-8 were made.
std::atomic<bool> g_armed{false};                      // this match's slots 7-8 were changed
std::atomic<bool> g_mapped{false};  // slots 7-8 were mapped to persons 6-7 (set before the models load; try 8 fix)
std::atomic<long long> g_made_ms{0};                   // when slot 7 or 8 was last made (steady clock, ms)
long long NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

std::string Bytes(u32 at, u32 n) {
  std::string s;
  for (u32 i = 0; i < n && at; ++i)
    s += fmt::format("{:02X}", bd::mem::try_load<u8>(at + i));
  return s;
}

int PersonFor(u32 slot) {
  const std::string v = REXCVAR_GET(wwe12_dev_eight_persons);
  if (v.empty())
    return -1;
  const size_t comma = v.find(',');
  const std::string one = slot == 7 ? v.substr(0, comma) : comma == std::string::npos ? "" : v.substr(comma + 1);
  return one.empty() ? -1 : std::atoi(one.c_str());
}

// Try 6 crashed loading the models (sub_82839848, a model file's parts): the two superstars given by default (12302,
// 13902) were already in the match (slots 3 and 1), and the game never loads one superstar twice (the select screen
// forbids it). Slots 7 and 8 now take the first two of the list that the match's persons 0-5 don't have.
u32 PersonId(u32 n);  // below
u32 IdFor(u32 slot) {
  const std::string ids = REXCVAR_GET(wwe12_dev_eight_ids);
  u32 skip = slot == 8 ? 1 : 0;
  for (size_t at = 0; at < ids.size();) {
    const size_t comma = std::min(ids.find(',', at), ids.size());
    const u32 id = u32(std::strtoul(ids.substr(at, comma - at).c_str(), nullptr, 10));
    at = comma + 1;
    if (id < 100)
      continue;
    bool used = false;
    for (u32 n = 0; n < 6 && !used; ++n)
      used = PersonId(n) / 100 == id / 100;
    if (used)
      continue;
    if (skip-- == 0)
      return id;
  }
  return 0;
}

u32 PersonId(u32 n) {  // the superstar x 100 + attire of person n (table [0x8317C84C] + 132 + n x 48, +16)
  const u32 t = bd::mem::try_load<u32>(0x8317C84C);
  return t && n < 43 ? bd::mem::try_load<u32>(t + 132 + n * 48 + 16) : 0;
}

// Test 2 (2026-11-22): slots 7-8 were made at role 0 as superstars (class 82004220, as slots 0-5), but 10 s later
// they were at role 4 again, at their place outside the ring: the game puts the participant table's +6 back to 4 (the
// record is that table's entry: same address), then sub_826DDC30 / sub_826DD2C0 copy it into the character (+3428)
// and the role 4 code puts them back outside. Every 100 ms (wwe12_dev_eight_force): record +6 and character +3428
// back to 0, each time logged (the first 20 times), and the writer of the record's +4..+7 caught by wwe12_pin_trap's
// DR2 (wwe12_watch_addr set here), logged as "[compte] adresse surveillee : ... ecrite par wwe12.exe+X" (wwe12.map).
void Force() {
  static int logged = 0;
  for (u32 slot : {7u, 8u}) {
    const u32 rec = g_desc[slot].load(), made = g_made[slot].load();
    if (rec && bd::mem::try_load<u8>(rec + 6) == 4 && bd::mem::try_load<u32>(rec) == IdFor(slot)) {
      bd::mem::try_store<u8>(rec + 6, 0);
      if (logged++ < 20)
        BD_INFO("[eight] force: slot {} record +6 was 4 again, set to 0", slot);
    }
    const u32 a = bd::mem::try_load<u32>(kList + slot * 4);
    if (a && a == made && bd::mem::try_load<u32>(a + 3428) == 4) {
      bd::mem::try_store<u32>(a + 3428, 0);
      if (logged++ < 20)
        BD_INFO("[eight] force: slot {} character {:08X} role was 4 again, set to 0", slot, a);
    }
  }
}

// Every 100 ms while on: Force; every 10 s: the 9 characters and their heaps.
void Watch() {
  for (u32 tick = 1;; ++tick) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    if (!REXCVAR_GET(wwe12_dev_eight_wrestlers))
      continue;
    if (REXCVAR_GET(wwe12_dev_eight_force) && g_armed.load())
      Force();
    if (tick % 100)
      continue;
    std::string list, heaps;
    int present = 0;
    for (u32 i = 0; i < 9; ++i) {
      const u32 a = bd::mem::try_load<u32>(kList + i * 4);
      if (!a) {
        list += fmt::format(" {}:-", i);
        continue;
      }
      ++present;
      list += fmt::format(" {}:role {} at ({:.0f} {:.0f} {:.0f})", i, int(bd::mem::try_load<u32>(a + 3428)),
                          bd::mem::try_load<float>(a + 752), bd::mem::try_load<float>(a + 756),
                          bd::mem::try_load<float>(a + 760));
      if (i >= 6)  // (+1736 / +1752: 1 1 for the referee too, so not "hidden"; made: the character sub_822D0618 made)
        list += fmt::format(" [{:08X} {} class {:08X} +1736 {} +1752 {}]", a,
                            a == g_made[i].load() ? "made here" : "NOT the one made", bd::mem::try_load<u32>(a),
                            bd::mem::try_load<u32>(a + 1736), bd::mem::try_load<u32>(a + 1752));
    }
    if (!present)
      continue;  // not in a match
    for (u32 slot : {7u, 8u})
      if (const u32 rec = g_desc[slot].load())
        list += fmt::format(" | record {} role {}", slot, bd::mem::try_load<u8>(rec + 6));
    const u32 table = bd::mem::try_load<u32>(kHeapTable);
    for (u32 i = 0; i < 9 && table; ++i) {
      const u32 d = g_desc[i].load();
      const u32 h = d ? bd::mem::try_load<u32>(d + 20) : 0;
      if (!d || h >= 64)
        continue;
      const u32 s = bd::mem::try_load<u32>(table + h * 36), e = bd::mem::try_load<u32>(table + h * 36 + 4);
      heaps += fmt::format(" {}:heap {} {} KB", i, h, e > s ? (e - s) / 1024 : 0);
    }
    BD_INFO("[eight] list ({} characters):{}", present, list);
    BD_INFO("[eight] heaps:{} (fill: wwe12_heap_watermark)", heaps);
  }
}

}  // namespace

namespace {

// The game's participant table entry of a slot: sub_8268B380([0x8322DD44], slot) = [0x8322DD44] + 528 + slot x 80. Read
// by sub_826DD2C0 / sub_826DDC30, which copy its +6 into the character's role (+3428) for characters at role 4.
u32 TableEntry(u32 slot) {
  const u32 owner = bd::mem::try_load<u32>(0x8322DD44);
  return owner ? owner + 528 + slot * kDescSize : 0;
}

// One record of slot 7 / 8 made a superstar's (the record sub_822D23A0 gets, or the participant table's entry, or the
// record again when sub_822D0618 makes the character). True if it was changed now.
bool Rewrite(u32 slot, u32 rec, const char *where) {
  if (!rec || bd::mem::try_load<u8>(rec + 6) != 4)
    return false;
  const u32 one = g_desc[1].load();
  const u32 want = IdFor(slot);
  if (!one || want < 100) {
    BD_WARN("[eight] {} slot {} left as it is (slot 1 seen: {}, superstar from wwe12_dev_eight_ids: {})", where, slot,
            one != 0, want);
    return false;
  }
  bd::mem::store<u32>(rec, want);
  bd::mem::store<u8>(rec + 6, 0);
  bd::mem::store<u8>(rec + 8, bd::mem::try_load<u8>(one + 8));
  bd::mem::store<u8>(rec + 9, bd::mem::try_load<u8>(one + 9));
  if (const int person = PersonFor(slot); person >= 0)
    bd::mem::store<u8>(rec + 4, u8(person));
  // Where it starts: the role 4 characters stand outside the ring, on the floor (test 2026-11-22: (-60 0 -880),
  // (80 0 -880); superstars at y -120). With wwe12_dev_eight_place: slot 2's / 3's place (+32 position, +48 the next
  // 16 bytes, both copied into the character at +752 / +736 by sub_822D0618), 60 units to the side.
  if (REXCVAR_GET(wwe12_dev_eight_place)) {
    const u32 from = g_desc[slot == 7 ? 2 : 3].load();
    if (from && from != rec) {
      for (u32 b = 32; b < 64; b += 4)
        bd::mem::store<u32>(rec + b, bd::mem::try_load<u32>(from + b));
      bd::mem::store<float>(rec + 32, bd::mem::try_load<float>(from + 32) + (slot == 7 ? 60.0f : -60.0f));
    }
  }
  static std::atomic<int> logged{0};
  if (logged++ < 40)
    BD_INFO("[eight] {}: slot {} is now superstar {} attire {} (role 0) at {:08X}: {}", where, slot, want / 100,
            want % 100, rec, Bytes(rec, 64));
  return true;
}

}  // namespace

void Wwe12PinTrapStart();  // wwe12_pin_trap.cpp (dev)

// From wwe12_discord.cpp's sub_822D23A0 hook, before the game: a match character is asked for (slot, its record).
void Wwe12EightWrestlersDesc(u32 slot, u32 desc) {
  if (!REXCVAR_GET(wwe12_dev_eight_wrestlers) || !desc || slot >= 9)
    return;
  if (!g_thread.exchange(true))
    std::thread(Watch).detach();
  if (slot == 0) {  // a new match: forget the last one
    for (auto &d : g_desc)
      d = 0;
    for (auto &m : g_made)
      m = 0;
    g_armed = false;
  }
  g_desc[slot] = desc;
  const u32 table = TableEntry(slot);
  BD_INFO("[eight] slot {}: id {} role {} person {} heap {} (record {:08X}: {}; participant table entry {:08X}, role {})",
          slot, bd::mem::try_load<u32>(desc), bd::mem::try_load<u8>(desc + 6), bd::mem::try_load<u8>(desc + 4),
          bd::mem::try_load<u32>(desc + 20), desc, Bytes(desc, 24), table,
          table ? int(bd::mem::try_load<u8>(table + 6)) : -1);
  if (slot != 7 && slot != 8)
    return;
  if (!g_desc[2].load() || !g_desc[3].load() || !g_desc[4].load() || !g_desc[5].load()) {
    BD_INFO("[eight] slot {}: not a match of 6 superstars (the title screen's demo, a 1 on 1...): left as it is", slot);
    return;
  }
  g_armed = true;
  if (Rewrite(slot, desc, "asked") && slot == 7) {
    // who writes the record's +6 afterwards: wwe12_pin_trap's DR2 on the word +4..+7 (needs --wwe12_pin_trap=true)
    rex::cvar::SetFlagByName("wwe12_watch_addr", std::to_string(int32_t(desc + 4)));
    Wwe12PinTrapStart();
    BD_INFO("[eight] DR2 on slot 7's record +4 ({:08X}) if wwe12_pin_trap is on", desc + 4);
  }
  if (table && table != desc)
    Rewrite(slot, table, "participant table");
}

// sub_822D0618(loader, slot, record, ..) makes a match character (directly from sub_822D23A0, or later from the loader's
// job sub_822D2260, with the same record). Test of 2026-11-22: slots 7-8 were still made as role 4 characters (+3428 = 4,
// hidden, outside the ring) although their records said role 0 when asked. Here the record is set again just before
// the character is made, and what was made is logged (its class: vtable +0; role +3428; hidden +1736 / +1752).
REX_EXTERN(__imp__sub_822D0618);
extern "C" REX_FUNC(sub_822D0618) {
  const u32 loader = ctx.r3.u32, slot = ctx.r4.u32, rec = ctx.r5.u32;
  const bool on = REXCVAR_GET(wwe12_dev_eight_wrestlers) && slot < 9;
  if (on) {
    const u32 id = bd::mem::try_load<u32>(rec);
    const u8 role = bd::mem::try_load<u8>(rec + 6);
    BD_INFO("[eight] making slot {}: record {:08X} id {} role {}", slot, rec, id, role);
    if ((slot == 7 || slot == 8) && g_armed.load())
      Rewrite(slot, rec, "making");  // (logs when the record had gone back to role 4)
  }
  __imp__sub_822D0618(ctx, base);
  if (!on)
    return;
  const u32 table = bd::mem::try_load<u32>(loader);
  const u32 a = table ? bd::mem::try_load<u32>(table + slot * 4) : 0;
  g_made[slot] = a;
  if (slot == 7 || slot == 8)
    g_made_ms = NowMs();
  BD_INFO("[eight] made slot {}: character {:08X} class {:08X} role {} hidden {} {}", slot, a,
          a ? bd::mem::try_load<u32>(a) : 0, a ? int(bd::mem::try_load<u32>(a + 3428)) : -1,
          a ? bd::mem::try_load<u32>(a + 1736) : 0, a ? bd::mem::try_load<u32>(a + 1752) : 0);
}

// sub_8268B380(table, slot) = table + 528 + slot x 80: how the game reads a participant table entry (84 callers).
// Try 3 (2026-11-22, iMrBurn's screenshot): slots 7-8 kept at role 0 by Force came into the ring, but as the two
// commentators, standing still. DR2 on the record caught sub_828C38E0 (a match setup step: wwe12.exe+291A8DD..9AE,
// called from sub_828E5C08 / sub_828E5DA0) rebuilding the table ~150 ms after the characters were made: entry cleared
// by sub_824C0F90, then refilled from the persons (id 15006 / 15306, person 41 / 42, role 4). So every read of entries
// 7-8 through this getter now finds them as our superstars again (the model, the role and what follows read them so).
REX_EXTERN(__imp__sub_8268B380);
extern "C" REX_FUNC(sub_8268B380) {
  const u32 table = ctx.r3.u32, slot = ctx.r4.u32;
  __imp__sub_8268B380(ctx, base);
  if ((slot != 7 && slot != 8) || !REXCVAR_GET(wwe12_dev_eight_wrestlers) || !g_armed.load() ||
      NowMs() - g_made_ms.load() > 10000)
    return;  // (only this match's slots 7-8, in the 10 s after they were made: the set-up's rebuilds)
  const u32 entry = ctx.r3.u32;
  if (table && table == bd::mem::try_load<u32>(0x8322DD44) && bd::mem::try_load<u8>(entry + 6) == 4 && IdFor(slot) >= 100)
    Rewrite(slot, entry, "read");
}

namespace {

// ---- try 6 (2026-11-22): slots 7-8 as persons 6-7 ------------------------------------------------------------------
// Try 5: still the two commentators, standing still (iMrBurn). The look and the behaviour come from the match's
// "persons", not from the participant records: the person table (sub_8267CD30 / sub_82686058: [0x8317C84C] + 132 +
// n x 48, 43 persons, 0-42) holds +16 superstar x 100 + attire (51200 = none), +8 the slot, +21 / +22 / +23 / +45
// more, and sub_828C38E0 rebuilds the participant entries (and the load list) from them. Which person each slot is
// comes from sub_8268B218 (15 callers): slots 0-8 = persons 0, 1, 2, 3, 4, 5, then 40 (referee), 41, 42 (the
// commentators). Persons 6-39 are there for the Royal Rumble's entrants. So: slots 7-8 = persons 6-7, filled as
// copies of persons 2 and 5 (one from each side: a 3 on 3 becomes 4 on 4) with the superstars of wwe12_dev_eight_ids
// and their own slot.
constexpr u32 kPersons = 0x8317C84C;  // -> + 132 + n x 48
constexpr u32 kPersonSize = 48;

u32 Person(u32 n) {
  const u32 t = bd::mem::try_load<u32>(kPersons);
  return t && n < 43 ? t + 132 + n * kPersonSize : 0;
}

bool FillPersons() {
  for (u32 n = 0; n < 6; ++n) {  // (a match of 6 chosen: persons 0-5 all set)
    const u32 p = Person(n);
    if (!p || bd::mem::try_load<u32>(p + 16) == 51200 || bd::mem::try_load<u32>(p + 16) == 0)
      return false;
  }
  static std::atomic<int> logged{0};
  for (u32 slot : {7u, 8u}) {
    const u32 to = Person(slot - 1), from = Person(slot == 7 ? 2 : 5), id = IdFor(slot);
    if (!to || !from || id < 100)
      return false;
    if (bd::mem::try_load<u32>(to + 16) == id && bd::mem::try_load<u32>(to + 8) == slot)
      continue;  // (done already)
    for (u32 b = 0; b < kPersonSize; b += 4)
      bd::mem::store<u32>(to + b, bd::mem::try_load<u32>(from + b));
    bd::mem::store<u32>(to + 16, id);
    bd::mem::store<u32>(to + 8, slot);
    if (logged++ < 20)
      BD_INFO("[eight] person {} (slot {}) = person {}'s copy, superstar {} attire {}: {}", slot - 1, slot,
              slot == 7 ? 2 : 5, id / 100, id % 100, Bytes(to, kPersonSize));
  }
  return true;
}

}  // namespace

REXCVAR_DEFINE_BOOL(wwe12_dev_eight_persons_map, true, "WWE12",
                    "Dev (wwe12_dev_eight_wrestlers): slots 7-8 are persons 6-7 (copies of persons 2 / 5 with "
                    "wwe12_dev_eight_ids) instead of the commentators 41-42.");

REX_EXTERN(__imp__sub_8268B218);
extern "C" REX_FUNC(sub_8268B218) {
  const u32 out = ctx.r3.u32;
  __imp__sub_8268B218(ctx, base);
  if (!REXCVAR_GET(wwe12_dev_eight_wrestlers) || !REXCVAR_GET(wwe12_dev_eight_persons_map) || !out)
    return;
  if (!FillPersons())
    return;
  bd::mem::store<u32>(out + 28, 6);
  bd::mem::store<u32>(out + 32, 7);
  g_mapped = true;
  static std::atomic<int> logged{0};
  if (logged++ < 10)
    BD_INFO("[eight] slots -> persons: 7 -> 6, 8 -> 7 (from {:08X})", u32(ctx.lr));
}

// ---- try 8 (2026-11-22): what memory is refused ---------------------------------------------------------------------
// Tries 6 and 7 crashed the same way while loading slot 7's model (sub_82837EC8, a part's buffer from sub_82A9C000), with
// two superstars not in the match the second time (102, 107): "PhysicalHeap::Alloc unable to alloc physical memory in
// parent heap", then the empty result is used. The guest's physical memory is the console's 512 MB (shared by the game's
// heaps, mapped at start-up, and its physical allocations). sub_821AC5E8(size, attributes) is the game's allocator for
// such memory (XMemAlloc-like, by category); when it gives 0 with the 8 superstars on, the size, the attributes and
// the caller are logged, with how much physical memory is left in 4 KB / 64 KB / 16 MB pages.
REX_EXTERN(__imp__sub_821AC5E8);
extern "C" REX_FUNC(sub_821AC5E8) {
  const u32 size = ctx.r3.u32, attr = ctx.r4.u32, caller = u32(ctx.lr);
  __imp__sub_821AC5E8(ctx, base);
  if (ctx.r3.u32 || !REXCVAR_GET(wwe12_dev_eight_wrestlers) || !g_mapped.load())
    return;  // (only once slots 7-8 are mapped, before the models load - try 8 logged nothing while it waited for
             // slot 7 to be asked, which comes after; the 446 refusals of every start-up are the game's own probing)
  static std::atomic<int> logged{0};
  if (logged++ >= 40)
    return;
  std::string left;
  if (auto *memory = REX_KERNEL_MEMORY())
    for (uint32_t ps : {4096u, 65536u, 16u << 20})
      if (auto *h = memory->LookupHeapByType(true, ps))
        left += fmt::format(" {}K pages {} of {} free;", ps >> 10, h->GetUnreservedPageCount(), h->GetTotalPageCount());
  BD_WARN("[eight] memory refused: {} bytes ({} KB), attributes {:08X}, for {:08X} |{}", size, size / 1024, attr, caller,
          left);
}
