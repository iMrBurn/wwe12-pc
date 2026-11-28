// WWE '12: option to skip the instant replay after matches (2.0, docs/etude_replay_option_2_0.md).
//
// The game has its own switch: MATCH CREATOR > ENVIRONMENT > POST MATCH REPLAY. The Match Creator keeps
// one 28-byte record per match type (119 types) in the settings block, sub_82684BE8() (= 0x831CEEA8)
// + 0x57786 + type * 28: byte 0 ring type, 1 ENTRANCE, 2 POST MATCH REPLAY, 3 MOMENTUM BAR CHARGE
// SPEED (read so by the ENVIRONMENT panel, sub_82593F88). The match set-ups copy the first 25 bytes
// of the type's record into the match's own rules with sub_82A3DEF0(dst, src) (sub_825493B0,
// sub_828A99D0, sub_826EAAF8, ...).
//
// With wwe12_skip_replay = true, each such copy out of the per-type table gets byte 2 = 0 (OFF) in the
// copy. The settings block itself (the player's save) is never changed, so the Match Creator still
// shows the player's own choice and turning the option off brings the replay back.
//
// (First try, 2026-11-28: a hook on the end-of-match automaton sub_823F4910 never ran before the
// replay: that automaton comes after the victory scene. Removed.)

#include <atomic>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_skip_replay, false, "WWE12",
                    "Skip the instant replay after matches (as MATCH CREATOR's POST MATCH REPLAY = OFF, "
                    "for every match type; the save is not changed).");

namespace {
constexpr u32 kSettings = 0x831CEEA8u;             // sub_82684BE8()
constexpr u32 kTypeTable = kSettings + 0x57786u;    // 119 records of 28 bytes
constexpr u32 kTypeRecord = 28, kTypeCount = 119;
constexpr u32 kSettingsEnd = kSettings + 0x70000u;  // the block, to never write into the save
constexpr u32 kReplayByte = 2;
}  // namespace

REX_EXTERN(__imp__sub_82A3DEF0);
extern "C" REX_FUNC(sub_82A3DEF0) {
  const u32 dst = ctx.r3.u32, src = ctx.r4.u32;
  __imp__sub_82A3DEF0(ctx, base);
  if (!REXCVAR_GET(wwe12_skip_replay))
    return;
  if (src < kTypeTable || src >= kTypeTable + kTypeCount * kTypeRecord ||
      (src - kTypeTable) % kTypeRecord != 0)
    return;  // not a copy of a match type's record
  if (dst >= kSettings && dst < kSettingsEnd)
    return;  // into the settings block itself: leave the player's save alone
  const u8 before = bd::mem::load<u8>(dst + kReplayByte);
  bd::mem::store<u8>(dst + kReplayByte, 0);
  static std::atomic<int> s_logged{0};
  if (s_logged.fetch_add(1) < 32)
    BD_INFO("[replay] skip (wwe12_skip_replay): match type {} rules copied to 0x{:08X}, "
            "POST MATCH REPLAY {} -> 0",
            (src - kTypeTable) / kTypeRecord, dst, before);
}
