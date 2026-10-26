// Android: after a match the instant replay can freeze on its first frame, the victory scene
// never comes and the game cannot be left (issues #22, #23, #24, #29, #31, also on real phones
// at 60 fps: #31's log). The game saves each kept highlight into cache:\Spectacular.rps during
// the match: its job thread sub_821D9428 opens the file (sub_822B8130, buffered read of 16 KB,
// then writes). In the frozen runs that thread reads, then never gets the work to write and
// waits forever; in the good runs it writes hundreds of 16 KB blocks right after the read, the
// replay plays and the victory scene follows. Seen 2026-10-26 with the probes replay_io_log and
// the [replay] lines of wwe12_game_log; random (about one run in two on the emulator), cause not
// found yet. Match loadings that never end (right after the commentary banks Comm_XXXX) look
// like the same family.
//
// Until the cause is found, the instant replay is off by default (players report that matches
// end fine without it): the game's own switch, set by sub_822B6D30 (startup) to 1, and back to
// 0 when it cannot make the 150 MB file; with 0 no highlight is kept and MATCH HIGHLIGHTS is
// greyed (as in PC issue #9). wwe12_fresh_cache empties the cache partition at each start, for
// tests (two good runs with a fresh cache, then a frozen one).

#include <filesystem>
#include <system_error>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "../native/svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_instant_replay, false, "WWE12",
                    "Android: instant replay and highlights after matches (off by default: they "
                    "can freeze the game after the match there for now).");

REXCVAR_DEFINE_BOOL(wwe12_fresh_cache, false, "WWE12",
                    "Android, for tests: empty the cache partition (xbox_cache) at each start.");

void Wwe12AndroidFreshCache(const std::filesystem::path &cache_dir) {
  if (!REXCVAR_GET(wwe12_fresh_cache))
    return;
  std::error_code ec;
  for (const char *name : {"EditEx.cac", "Spectacular.rps"})
    std::filesystem::remove(cache_dir / name, ec);
  BD_INFO("Xbox cache emptied (wwe12_fresh_cache = true)");
}

REX_EXTERN(__imp__sub_822B6D30);
extern "C" REX_FUNC(sub_822B6D30) {
  __imp__sub_822B6D30(ctx, base);
  if (REXCVAR_GET(wwe12_instant_replay))
    return;
  bd::mem::store<u32>(0x83174250, 0);
  BD_INFO("Instant replay off (wwe12_instant_replay = false): matches end without highlights");
}
