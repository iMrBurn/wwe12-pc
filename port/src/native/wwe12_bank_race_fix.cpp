// Fix for the random start-up freeze on "NOW LOADING" with few CPU threads (2026-11-21, Rendu;
// docs/savoir_acquis.md). A race in the game's own sound bank loader, found with sync_probe + wwe12_sound_probe.
//
// sub_82DF0228 (the game's "load sound bank" helper, 64 slots of 304 bytes) fills a slot, then calls
// sub_82DF3568 = Wwise LoadBank, asynchronous, with the callback sub_82DEFFF0 and the slot as cookie. Only AFTER
// LoadBank returns does it set the slot's "loading" flag (slot + 260 = 1, under the slot lock). The callback,
// run by the Wwise bank thread (sub_82E75F20) when the bank is loaded, clears that flag only if it is already 1.
// The game's start-up loop waits for the flag to drop to 0 before it posts the next bank.
// On the Xbox 360, and on PCs with many threads, the bank thread needs a millisecond or more, so the flag is set
// first. With 2 threads the bank thread sometimes runs the whole load and the callback BEFORE LoadBank returns to
// the main thread: the callback finds the flag at 0 and does nothing, then the main thread sets it to 1, nobody
// ever clears it, and the first loading screen waits forever (frozen run: the bank handler's log line comes before
// the post's; healthy run: after).
//
// Fix (cvar wwe12_bank_race_fix, default true): while sub_82DF0228 runs on a slot, a callback for that same slot
// is noted; when sub_82DF0228 has returned (the flag is then set), the callback is run again with the same
// arguments, on the main thread, as if it had come at the normal time. The early run had no effect on the flag
// (it was 0); the callback takes the slot lock itself. Nothing changes when the callback comes at the normal time.

#include <cstring>
#include <mutex>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_bank_race_fix, true, "WWE12",
                    "Run a sound bank callback again when it came before the game had marked the bank as loading "
                    "(fixes the random start-up freeze on 'NOW LOADING' with few CPU threads).");

REX_EXTERN(__imp__sub_82DF0228);
REX_EXTERN(__imp__sub_82DEFFF0);

namespace {
std::mutex g_bank_mutex;
u32 g_loading_table = 0;  // r3 of the running sub_82DF0228 (slots start at table + 16), 0 = none
bool g_early = false;     // a callback for a slot of that table came during the call
u64 g_early_args[4]{};    // its r3..r6
}  // namespace

// Wwise bank callback (bank id, result, memory pool, cookie = slot).
extern "C" REX_FUNC(sub_82DEFFF0) {
  if (REXCVAR_GET(wwe12_bank_race_fix)) {
    std::lock_guard<std::mutex> lock(g_bank_mutex);
    const u32 slot = ctx.r6.u32, table = g_loading_table;
    // only a callback for a slot not yet marked as loading (a bank posted earlier and still loading has 1 there)
    if (table && slot >= table + 16 && slot < table + 16 + 64 * 304 && (slot - table - 16) % 304 == 0 &&
        bd::mem::try_load<u32>(slot + 260, 1) == 0) {
      g_early = true;
      g_early_args[0] = ctx.r3.u64;
      g_early_args[1] = ctx.r4.u64;
      g_early_args[2] = ctx.r5.u64;
      g_early_args[3] = ctx.r6.u64;
    }
  }
  __imp__sub_82DEFFF0(ctx, base);
}

extern "C" REX_FUNC(sub_82DF0228) {
  if (!REXCVAR_GET(wwe12_bank_race_fix)) {
    __imp__sub_82DF0228(ctx, base);
    return;
  }
  bool tracked = false;
  {
    std::lock_guard<std::mutex> lock(g_bank_mutex);
    if (g_loading_table == 0) {  // another call already tracked (other thread): leave this one as it is
      g_loading_table = ctx.r3.u32;
      g_early = false;
      tracked = true;
    }
  }
  __imp__sub_82DF0228(ctx, base);
  if (!tracked)
    return;
  bool replay;
  u64 args[4];
  {
    std::lock_guard<std::mutex> lock(g_bank_mutex);
    g_loading_table = 0;
    replay = g_early;
    g_early = false;
    std::memcpy(args, g_early_args, sizeof(args));
  }
  // replay only if the game has now marked that slot as loading (otherwise the early callback was enough)
  if (!replay || bd::mem::try_load<u32>(u32(args[3]) + 260, 0) != 1)
    return;
  static int s_logged = 0;
  if (s_logged < 20) {
    ++s_logged;
    BD_INFO("[bankfix] sound bank {:08X} loaded before the game marked it as loading: callback run again "
            "(result {}, slot {:08X})",
            u32(args[0]), u32(args[1]), u32(args[3]));
  }
  const u64 r3 = ctx.r3.u64, lr = ctx.lr;
  ctx.r3.u64 = args[0];
  ctx.r4.u64 = args[1];
  ctx.r5.u64 = args[2];
  ctx.r6.u64 = args[3];
  __imp__sub_82DEFFF0(ctx, base);
  ctx.r3.u64 = r3;
  ctx.lr = lr;
}
