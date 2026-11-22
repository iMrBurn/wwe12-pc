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
// wwe12_dev_eight_ids, +6 = 0, +8 / +9 copied from slot 1's record. Nothing else (the person +4 and the place +32 stay).
// Log lines, to measure: "[eight] slot N: ..." for each character asked (before / after), then every 10 s
// "[eight] list: ..." (the 9 characters: role +3428, position) and "[eight] heaps: ..." (the 9 characters' heaps).
// The frame rate is in the game's own "[perf]" lines (every 10 s); heap failures in "[heaps]" (wwe12_heap_probe).

#include <array>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include <fmt/format.h>
#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"

REXCVAR_DEFINE_BOOL(wwe12_dev_eight_wrestlers, false, "WWE12",
                    "Dev: the match's two extra characters (slots 7-8) become superstars (8 in the ring). First match "
                    "after start-up only.");
REXCVAR_DEFINE_STRING(wwe12_dev_eight_ids, "12302,13902", "WWE12",
                      "Dev: the two superstars of wwe12_dev_eight_wrestlers, superstar x 100 + attire, \"slot7,slot8\".");

namespace {

constexpr u32 kList = 0x8317B770;       // the match's 9 characters
constexpr u32 kHeapTable = 0x83232A90;  // -> heap descriptors, 36 bytes each {start, end, ...}
constexpr u32 kDescSize = 80;

std::array<std::atomic<u32>, 9> g_desc{};  // the last record asked for each slot
std::atomic<bool> g_thread{false};

std::string Bytes(u32 at, u32 n) {
  std::string s;
  for (u32 i = 0; i < n && at; ++i)
    s += fmt::format("{:02X}", bd::mem::try_load<u8>(at + i));
  return s;
}

u32 IdFor(u32 slot) {
  const std::string ids = REXCVAR_GET(wwe12_dev_eight_ids);
  const size_t comma = ids.find(',');
  const std::string one = slot == 7 ? ids.substr(0, comma) : comma == std::string::npos ? "" : ids.substr(comma + 1);
  return u32(std::strtoul(one.c_str(), nullptr, 10));
}

// Every 10 s while on: the 9 characters and their heaps.
void Watch() {
  for (;;) {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    if (!REXCVAR_GET(wwe12_dev_eight_wrestlers))
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
    }
    if (!present)
      continue;  // not in a match
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

// From wwe12_discord.cpp's sub_822D23A0 hook, before the game: a match character is asked for (slot, its record).
void Wwe12EightWrestlersDesc(u32 slot, u32 desc) {
  if (!REXCVAR_GET(wwe12_dev_eight_wrestlers) || !desc || slot >= 9)
    return;
  if (!g_thread.exchange(true))
    std::thread(Watch).detach();
  g_desc[slot] = desc;
  const u32 id = bd::mem::try_load<u32>(desc);
  const u8 role = bd::mem::try_load<u8>(desc + 6);
  BD_INFO("[eight] slot {}: id {} role {} person {} heap {} (record {:08X}: {})", slot, id, role,
          bd::mem::try_load<u8>(desc + 4), bd::mem::try_load<u32>(desc + 20), desc, Bytes(desc, 24));
  if ((slot != 7 && slot != 8) || role != 4)
    return;
  const u32 one = g_desc[1].load();
  const u32 want = IdFor(slot);
  if (!one || want < 100) {
    BD_WARN("[eight] slot {} left as it is (slot 1 seen: {}, superstar from wwe12_dev_eight_ids: {})", slot, one != 0,
            want);
    return;
  }
  // (the records are said to be in one list of 80-byte records: logged, to check)
  BD_INFO("[eight] slot {}: record {} bytes after slot 1's", slot, int(desc) - int(one));
  bd::mem::store<u32>(desc, want);
  bd::mem::store<u8>(desc + 6, 0);
  bd::mem::store<u8>(desc + 8, bd::mem::try_load<u8>(one + 8));
  bd::mem::store<u8>(desc + 9, bd::mem::try_load<u8>(one + 9));
  BD_INFO("[eight] slot {} is now superstar {} attire {} (role 0): {}", slot, want / 100, want % 100, Bytes(desc, 24));
}
