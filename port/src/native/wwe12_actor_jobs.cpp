// WWE '12: matches freezing for good (picture stopped, sound going on). Seen in Elimination
// Chambers on an Intel iGPU and a GeForce 940MX, and after a loading screen on a 940MX (hang traces
// of 0.8, 2026-10-18/20).
//
// The engine has job objects with one slot per wrestler (up to 9, the table at 0x8317B770), each
// served by a worker thread. The engine's master thread posts a slot (reset the slot's event,
// done = 0, flag = 1, wake the worker) and later waits for it (wait the event, flag = 0). The
// worker runs every slot whose done is 0, then signals the slot's event, and only then sets
// done = 1. On the console the master is never fast enough to wake up and post the same slot again
// between the signal and that last store. On a PC the worker can be preempted right after the
// signal (the woken master gets the core, fewer cores, more wrestlers): the master posts again
// (done = 0), the worker's late store puts done back to 1, the new job never runs, its event is
// never signalled, and the master waits forever. The traces show exactly that state.
//
// Two workers have this loop:
// - sub_824750C0 (one object at [0x8317B8B4]): post sub_82475170, wait sub_82475210 (under
//   sub_821E2B98, the Chamber traces);
// - sub_821E0AD0 (two objects at 0x83113804, two threads): posts sub_821DB228/sub_821DB850, wait
//   sub_821DC2E0 (under sub_822137B0, the trace after a loading screen).
// The workers below are the game's, with done = 1 stored before the signal.

#include <atomic>

#include <rex/cvar.h>

#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_actor_jobs_fix, true, "WWE12",
                    "Fix a race in the game's per-wrestler job threads that can freeze matches "
                    "(Elimination Chamber, after loading screens).");

REXCVAR_DEFINE_BOOL(wwe12_cloth_30hz, false, "WWE12",
                    "Test: in 60 fps entrances, run the wrestlers' hair/cloth job (sub_821E0AD0) "
                    "every other frame only, as at 30 fps.");

bool Wwe12EntranceAt60();  // wwe12_fps.cpp
namespace bd::gpu {
u64 SvrFrameIndex();
}

REX_EXTERN(__imp__sub_824750C0);
REX_EXTERN(__imp__sub_821E0AD0);
REX_EXTERN(sub_827D5C28);  // component lookup: r3 = owner + 1696, r4 = type tag
REX_EXTERN(sub_821AEC28);  // NtSetEvent
REX_EXTERN(sub_821AE708);  // wait (r4 = -1: no timeout)
// sub_824750C0's job
REX_EXTERN(sub_824B9F80);
// sub_821E0AD0's job
REX_EXTERN(sub_821DBFA0);
REX_EXTERN(sub_821E0188);
REX_EXTERN(sub_821DDAF0);
REX_EXTERN(sub_821DDEE0);
REX_EXTERN(sub_821DD1C8);
REX_EXTERN(sub_821DEA60);

namespace {

constexpr u32 kWrestlers = 0x8317B770;  // 9 pointers
constexpr u32 kSlots = 9;

// The wrestler's component of type tag, or 0.
u32 Component(PPCContext &ctx, uint8_t *base, u32 wrestler, u32 tag) {
  ctx.r3.u64 = wrestler + 1696;
  ctx.r4.u64 = tag;
  sub_827D5C28(ctx, base);
  return ctx.r3.u32 ? bd::mem::load<u32>(ctx.r3.u32 + 24) : 0;
}

// r3 = the job object: +24 wake event, +28 + 4i slot events, +100 + 4i done flags.
template <typename Job>
[[noreturn]] void Worker(PPCContext &ctx, uint8_t *base, Job job) {
  const u32 obj = ctx.r3.u32;
  // A frame of the game's size, as the callees may write into their caller's frame.
  const u32 frame = ctx.r1.u32 - 160;
  bd::mem::store<u32>(frame, ctx.r1.u32);
  ctx.r1.u64 = frame;
  for (;;) {
    bool ran = false;
    for (u32 i = 0; i < kSlots; ++i) {
      const u32 done = obj + 100 + 4 * i;
      if (bd::mem::load<u32>(done) != 0)
        continue;
      if (const u32 wrestler = bd::mem::load<u32>(kWrestlers + 4 * i))
        job(wrestler);
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

}  // namespace

extern "C" REX_FUNC(sub_824750C0) {
  if (!REXCVAR_GET(wwe12_actor_jobs_fix)) {
    __imp__sub_824750C0(ctx, base);
    return;
  }
  Worker(ctx, base, [&](u32 wrestler) {
    ctx.r3.u64 = Component(ctx, base, wrestler, 0x59614648);
    sub_824B9F80(ctx, base);
  });
}

extern "C" REX_FUNC(sub_821E0AD0) {
  if (!REXCVAR_GET(wwe12_actor_jobs_fix)) {
    __imp__sub_821E0AD0(ctx, base);
    return;
  }
  Worker(ctx, base, [&](u32 wrestler) {
    if (REXCVAR_GET(wwe12_cloth_30hz) && Wwe12EntranceAt60() && (bd::gpu::SvrFrameIndex() & 1))
      return;
    const u32 c = Component(ctx, base, wrestler, 0x67426148);
    if (!c)
      return;
    const auto call = [&](void (*f)(PPCContext &, uint8_t *)) {
      ctx.r3.u64 = c;
      f(ctx, base);
    };
    call(sub_821DBFA0);
    call(sub_821E0188);
    call(sub_821DDAF0);
    call(sub_821DDEE0);
    call(sub_821DD1C8);
    if (bd::mem::load<u32>(c + 32072) != 0 || bd::mem::load<u32>(c + 32076) != 0)
      call(sub_821DD1C8);
    call(sub_821DEA60);
  });
}
