// Probe (read only, off by default: wwe12_sound_probe) for the start-up freeze on "NOW LOADING" with 2 threads
// (2026-11-21, Rendu; docs/savoir_acquis.md). sync_probe showed that in a frozen start-up the main thread posts
// ONE command to the Wwise bank thread (guest thread 13, sub_82E75F20) at ~3.8 s, the bank thread runs it (one
// 4509-byte read of System.pck at 0xe9000 by the I/O thread sub_82EDDCB0) and then nothing is ever posted again;
// a healthy start-up posts 13 commands in 30 s. No lost wake: the poster sub_82E72E90 pushes the command and sets
// the event while holding the queue lock (bank manager + 48), and the bank thread drains the whole queue under the
// same lock before it waits again.
//
// Logged, with the time since start:
//   - every bank command post sub_82E72E90 (bank manager = *(0x8229EB8C)): command type (high word of r4),
//     caller, result (1 = queued and signalled);
//   - every command run on the bank thread (the 10 handlers of sub_82E75F20's switch): start, duration, result;
//   - every call of the game's sound layer entry points that lead to the posts (sub_82DF2350, sub_82DF2378,
//     sub_82DF23A0, sub_82DF3568, sub_82DF3630, sub_82DEF090, sub_82DF0228): caller, r4/r5, result.
// The first 600 lines only.

#include <atomic>
#include <chrono>

#include <rex/cvar.h>

#include "core/logging.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_sound_probe, false, "WWE12",
                    "Debug: log the Wwise bank commands and the game's sound calls ('[soundprobe]'), for the "
                    "start-up freeze on 2 threads.");

namespace {
std::atomic<u32> g_sound_lines{0};

double SoundMs() {
  static const auto start = std::chrono::steady_clock::now();
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

bool SoundLogAllowed() { return g_sound_lines.fetch_add(1, std::memory_order_relaxed) < 600; }
}  // namespace

REX_EXTERN(__imp__sub_82E72E90);
extern "C" REX_FUNC(sub_82E72E90) {
  if (!REXCVAR_GET(wwe12_sound_probe)) {
    __imp__sub_82E72E90(ctx, base);
    return;
  }
  const u32 lr = u32(ctx.lr), type = u32(ctx.r4.u64 >> 32), arg = u32(ctx.r4.u64);
  const u64 r5 = ctx.r5.u64;
  const double t = SoundMs();
  __imp__sub_82E72E90(ctx, base);
  if (SoundLogAllowed())
    BD_INFO("[soundprobe] {:.1f} ms: bank command post type {} ({:08X}, r5 {:016X}) from lr {:08X} -> {}", t, type,
            arg, r5, lr, ctx.r3.u32);
}

#define SOUND_PROBE_HANDLER(addr)                                                                             \
  REX_EXTERN(__imp__sub_##addr);                                                                              \
  extern "C" REX_FUNC(sub_##addr) {                                                                           \
    if (!REXCVAR_GET(wwe12_sound_probe)) {                                                                    \
      __imp__sub_##addr(ctx, base);                                                                           \
      return;                                                                                                 \
    }                                                                                                         \
    const double t0 = SoundMs();                                                                              \
    __imp__sub_##addr(ctx, base);                                                                             \
    if (SoundLogAllowed())                                                                                    \
      BD_INFO("[soundprobe] {:.1f} ms: bank thread ran sub_" #addr " ({:.2f} ms) -> {:08X}", t0, SoundMs() - t0, \
              ctx.r3.u32);                                                                                    \
  }

SOUND_PROBE_HANDLER(82E74D90)
SOUND_PROBE_HANDLER(82E72F40)
SOUND_PROBE_HANDLER(82E759C8)
SOUND_PROBE_HANDLER(82E75B68)
SOUND_PROBE_HANDLER(82E718B0)
SOUND_PROBE_HANDLER(82E755F8)
SOUND_PROBE_HANDLER(82E73000)
SOUND_PROBE_HANDLER(82E75720)
SOUND_PROBE_HANDLER(82E74F78)
SOUND_PROBE_HANDLER(82E73B58)

#define SOUND_PROBE_GAME(addr)                                                                                 \
  REX_EXTERN(__imp__sub_##addr);                                                                               \
  extern "C" REX_FUNC(sub_##addr) {                                                                            \
    if (!REXCVAR_GET(wwe12_sound_probe)) {                                                                     \
      __imp__sub_##addr(ctx, base);                                                                            \
      return;                                                                                                  \
    }                                                                                                          \
    const u32 lr = u32(ctx.lr), r3 = ctx.r3.u32, r4 = ctx.r4.u32, r5 = ctx.r5.u32;                             \
    const double t0 = SoundMs();                                                                               \
    __imp__sub_##addr(ctx, base);                                                                              \
    if (SoundLogAllowed())                                                                                     \
      BD_INFO("[soundprobe] {:.1f} ms: sub_" #addr "(r3 {:08X}, r4 {:08X}, r5 {:08X}) from lr {:08X} -> {:08X}", \
              t0, r3, r4, r5, lr, ctx.r3.u32);                                                                 \
  }

SOUND_PROBE_GAME(82DF2350)
SOUND_PROBE_GAME(82DF2378)
SOUND_PROBE_GAME(82DF23A0)
SOUND_PROBE_GAME(82DF3568)
SOUND_PROBE_GAME(82DF3630)
SOUND_PROBE_GAME(82DEF090)
SOUND_PROBE_GAME(82DF0228)
