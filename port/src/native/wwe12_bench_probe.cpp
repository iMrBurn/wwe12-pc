// Dev probe (1.3, off by default): step 0 of the benchmark's fixed match (docs/perf_natif_steam_deck.md,
// "match imposé"). wwe12_bench_probe = true logs "[benchprobe]" lines while the player starts a match by hand
// (an Armageddon Hell in a Cell with 6 superstars, in exhibition):
//   - sub_82449428, the exhibition menus' function that starts a match without a choice screen (type 49 written):
//     when it runs, from where;
//   - sub_826D4888(record, type), the match record's reset: who sets the match type, which type;
//   - sub_822AF500, a scene request's constructor: who asks for the next scene (the step that replaces PLAY);
//   - each loading screen opening: the match record at 0x8317C850 (104 bytes) and the match setup's stage
//     (sub_828A9898() + 88, wwe12_arena_pages.cpp);
//   - each match character asked (sub_822D23A0, called by wwe12_discord.cpp's hook): slot, the description's
//     address (to find the list it comes from) and its first 16 bytes (+0 superstar x 100 + attire, +6 role).
// Nothing is changed in the game. At most 200 lines per kind.

#include <algorithm>
#include <atomic>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <fmt/format.h>
#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_bench_probe, false, "WWE12",
                    "Dev: log how the game sets up and starts a match (benchmark fixed match, step 0).");

REX_EXTERN(__imp__sub_82449428);
REX_EXTERN(__imp__sub_826D4888);
REX_EXTERN(__imp__sub_822AF500);
REX_EXTERN(__imp__sub_828A9898);  // match setup: its object (+88: the stage set)
REX_EXTERN(__imp__sub_821D5430);  // the match start (scene request "go to the match", record +12456)

namespace {
constexpr u32 kMatchRecord = 0x8317C850;
std::atomic<u32> g_lines[6];
u32 g_ids[6] = {};  // the match characters' desc +0 (superstar x 100 + attire), slots 0-5
u32 g_desc0 = 0;

bool On(u32 kind) {
  return REXCVAR_GET(wwe12_bench_probe) && g_lines[kind].fetch_add(1) < 200;
}

std::string Bytes(u32 at, u32 n) {
  std::string out;
  for (u32 i = 0; i < n && at; ++i)
    out += fmt::format("{}{:02X}", i && i % 4 == 0 ? " " : "", bd::mem::try_load<u8>(at + i));
  return out;
}
}  // namespace

extern "C" REX_FUNC(sub_82449428) {
  if (On(0))
    BD_INFO("[benchprobe] sub_82449428 (start a match without a choice screen): this {:08X} from {:08X} | "
            "record before: {}",
            ctx.r3.u32, u32(ctx.lr), Bytes(kMatchRecord, 32));
  __imp__sub_82449428(ctx, base);
}

extern "C" REX_FUNC(sub_826D4888) {
  if (ctx.r4.u32 != 119 && On(1))  // (type 119: ~200 records reset at start-up)
    BD_INFO("[benchprobe] match record reset: record {:08X} type {} from {:08X}", ctx.r3.u32, ctx.r4.u32,
            u32(ctx.lr));
  __imp__sub_826D4888(ctx, base);
}

extern "C" REX_FUNC(sub_821D5430) {
  if (On(5))
    BD_INFO("[benchprobe] match start sub_821D5430: this {:08X} r4 {:08X} r5 {:08X} from {:08X} | record {}",
            ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, u32(ctx.lr), Bytes(kMatchRecord, 104));
  __imp__sub_821D5430(ctx, base);
}

extern "C" REX_FUNC(sub_822AF500) {
  if (On(2))
    BD_INFO("[benchprobe] scene request {:08X} built from {:08X}", ctx.r3.u32, u32(ctx.lr));
  __imp__sub_822AF500(ctx, base);
}

// wwe12_load_turbo.cpp: a loading screen opens (game thread).
void Wwe12BenchProbeLoadScreen(PPCContext &ctx, u8 *base) {
  if (!On(3))
    return;
  PPCContext probe = ctx;
  __imp__sub_828A9898(probe, base);
  const u32 setup = probe.r3.u32;
  BD_INFO("[benchprobe] loading screen: match record {} | setup {:08X} stage {}", Bytes(kMatchRecord, 104), setup,
          setup ? int(bd::mem::try_load<u32>(setup + 88)) : -1);
}

// wwe12_discord.cpp's sub_822D23A0 hook: a match character is asked.
// Where else the chosen superstars are kept: once slots 0 and 1 are known, guest memory is searched for their
// numbers close together (u32 superstar x 100 + attire, or u16 superstar number), the description list excluded.
void ScanForChoices(u8 *base) {
#if defined(_WIN32)
  const u32 a = g_ids[0], b = g_ids[1];
  auto be32 = [](u32 v) { return std::vector<u8>{u8(v >> 24), u8(v >> 16), u8(v >> 8), u8(v)}; };
  auto be16 = [](u32 v) { return std::vector<u8>{u8(v >> 8), u8(v)}; };
  const std::vector<u8> pats[2][2] = {{be32(a), be32(b)}, {be16(a / 100), be16(b / 100)}};
  const char *names[2] = {"u32 id", "u16 superstar"};
  for (int k = 0; k < 2; ++k) {
    int hits = 0;
    for (const u8 *h = base; h < base + 0x100000000ull && hits < 32;) {
      MEMORY_BASIC_INFORMATION mbi{};
      if (VirtualQuery(h, &mbi, sizeof(mbi)) != sizeof(mbi))
        break;
      const u8 *start = static_cast<const u8 *>(mbi.BaseAddress);
      const u8 *end = std::min(start + mbi.RegionSize, base + 0x100000000ull);
      if (mbi.State == MEM_COMMIT && mbi.Protect && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
        for (const u8 *q = std::max(start, h); hits < 32;) {
          q = std::search(q, end, pats[k][0].begin(), pats[k][0].end());
          if (q == end)
            break;
          const u32 at = u32(q - base);
          const u8 *window_end = std::min(end, q + 256);
          const u8 *second = std::search(q + 1, window_end, pats[k][1].begin(), pats[k][1].end());
          if (second != window_end && (at < g_desc0 || at >= g_desc0 + 6 * 80)) {
            BD_INFO("[benchprobe] choices? {} at {:08X}, slot 1's {} bytes later: {}", names[k], at, second - q,
                    Bytes(at >= 16 ? at - 16 : at, 96));
            ++hits;
          }
          ++q;
        }
      h = end;
    }
    BD_INFO("[benchprobe] choices scan ({}): {} place(s)", names[k], hits);
  }
#endif
}

void Wwe12BenchProbeCharacter(u32 loader, u32 slot, u32 desc, u8 *base) {
  if (!REXCVAR_GET(wwe12_bench_probe))
    return;
  if (On(4))
    BD_INFO("[benchprobe] character: loader {:08X} slot {} desc {:08X}: {}", loader, slot, desc, Bytes(desc, 16));
  if (slot < 6 && desc)
    g_ids[slot] = bd::mem::try_load<u32>(desc);
  if (slot == 0)
    g_desc0 = desc;
  static bool scanned = false;
  if (slot == 1 && !scanned && g_ids[0] && g_ids[1]) {
    scanned = true;
    ScanForChoices(base);
  }
}
