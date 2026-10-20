// WWE '12: the match freezing for good (picture stopped, sound going on), seen in Elimination
// Chambers on an Intel iGPU and on a GeForce 940MX (hang traces of 0.8, 2026-10-19/20).
//
// Each wrestler in the match (up to 9, the table at 0x8317B770) has a job slot in an object handed
// to a worker thread. The engine's master thread posts a slot (sub_82475170: reset the slot's event,
// done = 0, flag = 1, wake the worker) and later waits for it (sub_82475210: wait the event, flag =
// 0). The worker (sub_824750C0, this thread's main) runs every slot whose done is 0, then signals the
// slot's event, then sets done = 1. On the console the master is never fast enough to wake up and
// post the same slot again between the signal and that last store. On a PC the worker can be
// preempted right after the signal (the woken master gets the core, fewer cores, bigger matches):
// the master posts again (done = 0), the worker's late store puts done back to 1, the new job is
// never run, its event never signalled, and the master waits forever in sub_82475210. Both traces
// show exactly that: master in sub_82475210 under sub_821E2B98, worker parked on its wake event.
//
// The worker below is the game's, with done = 1 stored before the signal.

#include <atomic>

#include <rex/cvar.h>

#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_actor_jobs_fix, true, "WWE12",
                    "Fix a race in the game's per-wrestler job thread that can freeze matches "
                    "(Elimination Chamber).");

REX_EXTERN(__imp__sub_824750C0);
REX_EXTERN(sub_827D5C28);  // component lookup: r3 = owner + 1696, r4 = type tag
REX_EXTERN(sub_824B9F80);  // the wrestler's job
REX_EXTERN(sub_821AEC28);  // NtSetEvent
REX_EXTERN(sub_821AE708);  // wait (r4 = -1: no timeout)

namespace {

constexpr u32 kWrestlers = 0x8317B770;  // 9 pointers
constexpr u32 kSlots = 9;
constexpr u32 kJobTag = 0x59614648;  // 'YaFH'

}  // namespace

// r3 = the job object: +24 wake event, +28 + 4i slot events, +100 + 4i done flags.
extern "C" REX_FUNC(sub_824750C0) {
  if (!REXCVAR_GET(wwe12_actor_jobs_fix)) {
    __imp__sub_824750C0(ctx, base);
    return;
  }
  const u32 obj = ctx.r3.u32;
  // A frame of the game's size, as the callees may write into their caller's frame.
  const u32 frame = ctx.r1.u32 - 144;
  bd::mem::store<u32>(frame, ctx.r1.u32);
  ctx.r1.u64 = frame;
  for (;;) {
    bool ran = false;
    for (u32 i = 0; i < kSlots; ++i) {
      const u32 done = obj + 100 + 4 * i;
      if (bd::mem::load<u32>(done) != 0)
        continue;
      if (const u32 wrestler = bd::mem::load<u32>(kWrestlers + 4 * i)) {
        ctx.r3.u64 = wrestler + 1696;
        ctx.r4.u64 = kJobTag;
        sub_827D5C28(ctx, base);
        ctx.r3.u64 = ctx.r3.u32 ? bd::mem::load<u32>(ctx.r3.u32 + 24) : 0;
        sub_824B9F80(ctx, base);
      }
      bd::mem::store<u32>(done, 1);
      std::atomic_thread_fence(std::memory_order_seq_cst);
      ctx.r3.u64 = bd::mem::load<u32>(obj + 28 + 4 * i);
      sub_821AEC28(ctx, base);
      ran = true;
    }
    if (!ran) {
      ctx.r3.u64 = bd::mem::load<u32>(obj + 24);
      ctx.r4.s64 = -1;
      sub_821AE708(ctx, base);
    }
  }
}
