// Crash fix for moves whose animation events need a superstar that is not given.
//
// sub_824A5458(motion r3, out r4, count r5, superstar r6, superstar r7) walks the
// motion's event list ([motion+12], 4-byte entries: u8 type, u8, u16 frame; count
// u8 at [motion+5]) and returns the frame of the first event whose condition
// holds. Types 11 and 13-17 read the superstar in r7 (+92, +700, +1076, +1096,
// +1188...) without checking it. sub_82481C78 calls it with r7 = 0, so a motion
// that has such an event reads guest address 0x5C and the game crashes.
// Seen: Batista's (DLC pack 4) rope move Beast Bomb, move 49899, anim 60354,
// crash read 0x5C, even with no mod (2026-11-09, reported by the Mods session).
//
// The fix: when r7 is 0 and the list has such an event, run the same walk here
// with those events counted as "condition not met". Everything else calls the
// game's own function unchanged.

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(wwe12_move_event_fix, true, "WWE12",
                    "Fix a crash in moves (Batista's Beast Bomb) whose animation events need a superstar that the game does not give.");

#include <atomic>
#include <mutex>

#include "svr_hook.h"
#include "core/logging.h"
#include "core/memory_helpers.h"

REX_EXTERN(__imp__sub_824A5458);  // first event frame of a motion
REX_EXTERN(__imp__sub_824A3E40);  // event value of a motion for a type
REX_EXTERN(__imp__sub_821A8400);  // a superstar's state (r3 superstar, r4 0/1, r5 count)

void Wwe12DownProbeStart();  // wwe12_down_probe.cpp

namespace {

std::atomic<int> g_logged{0};

bool NeedsSuperstar(u8 type) {
  return type == 11 || (type >= 13 && type <= 17);
}

int32_t Call8400(const PPCContext &in, u8 *base, u32 who, u32 which, u32 count) {
  PPCContext x = in;
  x.r3.u64 = who;
  x.r4.u64 = which;
  x.r5.u64 = count;
  __imp__sub_821A8400(x, base);
  return x.r3.s32;
}

u32 Call3E40(const PPCContext &in, u8 *base, u32 motion, u32 type) {
  PPCContext x = in;
  x.r3.u64 = motion;
  x.r4.u64 = type;
  __imp__sub_824A3E40(x, base);
  return x.r3.u32;
}

}  // namespace

extern "C" REX_FUNC(sub_824A5458) {
  static std::once_flag probe;  // a match is running: start the down-state probe if asked
  std::call_once(probe, Wwe12DownProbeStart);
  const u32 motion = ctx.r3.u32;
  if (!REXCVAR_GET(wwe12_move_event_fix) || ctx.r7.u32 != 0 || motion == 0) {
    __imp__sub_824A5458(ctx, base);
    return;
  }
  const u32 events = bd::mem::load<u32>(motion + 12);
  const u32 n = bd::mem::load<u8>(motion + 5);
  bool risky = false;
  for (u32 i = 0; i < n && events; ++i)
    if (NeedsSuperstar(bd::mem::load<u8>(events + i * 4))) {
      risky = true;
      break;
    }
  if (!risky) {
    __imp__sub_824A5458(ctx, base);
    return;
  }

  if (g_logged.fetch_add(1) < 20)
    BD_INFO("[move_events] motion {:08X}: {} events, no superstar given; superstar conditions skipped",
            motion, n);

  const u32 out = ctx.r4.u32;
  const u32 count = ctx.r5.u32 ? ctx.r5.u32 : 1;
  const u32 other = ctx.r6.u32;
  int32_t frame = -1;
  u32 value = 0xFFFFFFFF;
  for (u32 i = 0; i < n; ++i) {
    const u32 e = events + i * 4;
    const u8 type = bd::mem::load<u8>(e);
    bool hit = false;
    switch (type) {
    case 10:
      hit = frame < 0;
      break;
    case 12:
      hit = other && (Call8400(ctx, base, other, 1, count) == 8 || Call8400(ctx, base, other, 0, count) == 6);
      break;
    case 13:
      // The game then asks the missing superstar (sub_82200638): not met.
      hit = other && Call8400(ctx, base, other, 0, count) == 9;
      break;
    default:  // 11, 14-17 need the superstar; other types are not conditions
      break;
    }
    if (hit) {
      frame = bd::mem::load<u16>(e + 2);
      value = Call3E40(ctx, base, motion, type);
    }
  }
  if (out)
    bd::mem::store<u32>(out, value);
  ctx.r3.s64 = frame;
}
