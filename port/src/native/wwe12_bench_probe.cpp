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

#include <atomic>
#include <string>

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

namespace {
constexpr u32 kMatchRecord = 0x8317C850;
std::atomic<u32> g_lines[5];

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
  if (On(1))
    BD_INFO("[benchprobe] match record reset: record {:08X} type {} from {:08X}", ctx.r3.u32, ctx.r4.u32,
            u32(ctx.lr));
  __imp__sub_826D4888(ctx, base);
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
void Wwe12BenchProbeCharacter(u32 loader, u32 slot, u32 desc) {
  if (!On(4))
    return;
  BD_INFO("[benchprobe] character: loader {:08X} slot {} desc {:08X}: {}", loader, slot, desc, Bytes(desc, 16));
}
