// Dev-only: compiled only with the WWE12_DEV_PROBES environment variable at configure time.
// WWE '12: test probe for removed match types (limit breaking, docs/limit_breaking.md).
//
// A match is described by a 104-byte record whose byte 0 is the match kind (names = texts
// 27400 + kind: 82 GAUNTLET, 83 SPECIAL REFEREE...). sub_8267CF38 returns the current match
// record (0x8317C850, part of a larger match object), read by most match code. sub_826D4888
// (record, kind) resets other records (Universe cards, menus) but exhibition matches write the
// kind into the current record elsewhere (several writers), so the probe watches the reader.
//
// Special Referee (83) has no menu entry in WWE '12, but match code still tests it
// (sub_82A2FB18 is called from sub_821D0530, sub_8227EEC0, sub_82328E28, sub_823292C8,
// sub_824EFA78). wwe12_kind_from = kind picked in the menus, wwe12_kind_to = kind to play; when
// the current record holds "from", it is rewritten to "to". Both -1 (default) = off. Each new
// kind seen in the current record is logged as "[kind]" while the probe is on.

// WWE12_DEV_PROBES recompiles the whole game; for a quick test, set this to 1 locally instead
// (never commit it at 1).
#define WWE12_KIND_PROBE_LOCAL 0

#if defined(WWE12_DEV_PROBES) || WWE12_KIND_PROBE_LOCAL

#include <chrono>
#include <mutex>
#include <string>
#include <thread>

#include <fmt/format.h>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_INT32(wwe12_kind_from, -1, "WWE12",
                     "Test: match kind to replace in the current match (-1 = off).");
REXCVAR_DEFINE_INT32(wwe12_kind_to, -1, "WWE12",
                     "Test: match kind played instead of wwe12_kind_from (83 = Special Referee).");

namespace {
int g_last_kind = -1;
int g_wrestlers = 0;  // wrestlers created since the current match kind last changed
}

REX_EXTERN(__imp__sub_8267CF38);
extern "C" REX_FUNC(sub_8267CF38) {
  __imp__sub_8267CF38(ctx, base);
  const int from = REXCVAR_GET(wwe12_kind_from);
  const int to = REXCVAR_GET(wwe12_kind_to);
  if (from < 0 || to < 0)
    return;
  const u32 record = ctx.r3.u32;
  const int kind = bd::mem::load<u8>(record);
  if (kind == from) {
    bd::mem::store<u8>(record, u8(to));
    BD_INFO("[kind] match en cours : type {} -> {}", kind, to);
    if (g_last_kind != to)
      g_wrestlers = 0;
    g_last_kind = to;
  } else if (kind != g_last_kind) {
    BD_INFO("[kind] match en cours : type {}", kind);
    g_last_kind = kind;
    g_wrestlers = 0;
  }
}

// Participant roles. sub_822CDF78(type, desc, ...) creates the match actors: type 1 = wrestler
// (class sub_82213DB0, or sub_823A3310 when desc+6 == 4), 2 = sub_826F6878, 3 = sub_8240BC00,
// 4 = sub_8244BC30. sub_82213DB0 copies desc+6 into actor+3428, the actor's role (0-4); match
// code treats role 1 specially when the kind is 83 (sub_824EFA78, sub_821D0530). The probe logs
// each creation ("[role]") and, in a kind-83 match, gives wwe12_ref_role to the wrestler number
// wwe12_ref_slot (0-based, creation order) to see whether that wrestler becomes the referee.
REXCVAR_DEFINE_INT32(wwe12_ref_slot, 2, "WWE12",
                     "Test: wrestler (creation order) given wwe12_ref_role in a kind-83 match.");
REXCVAR_DEFINE_INT32(wwe12_ref_role, 1, "WWE12",
                     "Test: role byte given to that wrestler (-1 = only log the roles).");
// Pad assignment (sub_826E09D8, sub_826E0B18) skips actors with role 1 or 3, so a referee can't be
// played. wwe12_ref_skip_normal: in a kind-83 match, the normal referee (a role-1 actor that is not
// wwe12_ref_slot) is not created (the factory returns 0, as when its allocation fails), to see
// whether the wrestler given role 1 then takes over the referee's job. Not creating it crashes
// (sub_822D0618 writes into the new actor at once, lr 822D06D8), so the referee is created with
// wwe12_ref_normal_role instead (4 = like the two actors outside the ring; -1 = keep role 1).
// The referee and the role-4 actors are created at the first match and reused afterwards, so the
// kind-83 match must be the first one after launch.
REXCVAR_DEFINE_INT32(wwe12_ref_normal_role, 4, "WWE12",
                     "Test: role given to the normal referee in a kind-83 match (-1 = unchanged).");

void WatchActorsOnce();

REX_EXTERN(__imp__sub_822CDF78);
extern "C" REX_FUNC(sub_822CDF78) {
  if (REXCVAR_GET(wwe12_kind_from) < 0) {
    __imp__sub_822CDF78(ctx, base);
    return;
  }
  constexpr u32 kMatchInfo = 0x8317C850;  // sub_8267CF38
  const u32 type = ctx.r3.u32;
  const u32 desc = ctx.r4.u32;
  const int kind = bd::mem::load<u8>(kMatchInfo);
  const int role = desc ? bd::mem::load<u8>(desc + 6) : -1;
  int index = -1;
  if (type == 1)
    index = g_wrestlers++;
  const int new_role = REXCVAR_GET(wwe12_ref_role);
  const bool give = desc && type == 1 && kind == 83 && new_role >= 0 &&
                    index == REXCVAR_GET(wwe12_ref_slot);
  BD_INFO("[role] creation type {} n {} role {}{} (match type {})", type, index, role,
          give ? " -> " + std::to_string(new_role) : std::string(), kind);
  if (give)
    bd::mem::store<u8>(desc + 6, u8(new_role));
  const int normal_role = REXCVAR_GET(wwe12_ref_normal_role);
  if (desc && type == 1 && kind == 83 && role == 1 && !give && normal_role >= 0) {
    BD_INFO("[role] arbitre normal (n {}) -> role {}", index, normal_role);
    bd::mem::store<u8>(desc + 6, u8(normal_role));
  }
  __imp__sub_822CDF78(ctx, base);
  WatchActorsOnce();
}

// The match actors are listed at 0x8317B770 (9 pointers, see sub_82386168, which places them at
// match start). A host thread logs each actor's role (+3428) and +998 every 500 ms when they
// change ("[acteurs]"), to see whether a role given at creation survives into the match.
// Result 2026-10-22: the role given at creation is set back to 0 within 0.5 s (actor list:
// 0-5 wrestlers, 6 = referee with role 1, 7-8 role 4; all go to role 10 at the match end).
// wwe12_ref_force = actor index whose role 0 is turned back into wwe12_ref_role (every 100 ms,
// kind-83 matches only; -1 = off), to see whether a lasting role 1 makes that wrestler referee.
REXCVAR_DEFINE_INT32(wwe12_ref_force, 2, "WWE12",
                     "Test: match actor (0-8) kept at wwe12_ref_role in a kind-83 match (-1 = off).");

namespace {
void WatchActors() {
  constexpr u32 kActors = 0x8317B770;
  std::string last;
  for (int tick = 0;; ++tick) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const int force = REXCVAR_GET(wwe12_ref_force);
    const int role = REXCVAR_GET(wwe12_ref_role);
    if (force >= 0 && force < 9 && role >= 0 && bd::mem::load<u8>(0x8317C850) == 83) {
      const u32 actor = bd::mem::try_load<u32>(kActors + force * 4);
      if (actor && bd::mem::try_field<u32>(actor, 3428) == 0)
        bd::mem::store<u32>(actor + 3428, u32(role));
    }
    if (tick % 5)
      continue;
    std::string line;
    for (u32 i = 0; i < 9; ++i) {
      const u32 actor = bd::mem::try_load<u32>(kActors + i * 4);
      if (!actor) {
        line += " -";
        continue;
      }
      line += fmt::format(" {}:{}/{}", i, bd::mem::try_field<u32>(actor, 3428),
                          bd::mem::try_field<u8>(actor, 998));
    }
    if (line != last) {
      BD_INFO("[acteurs] role/998 :{} (match type {})", line, bd::mem::load<u8>(0x8317C850));
      last = line;
    }
  }
}
}  // namespace

void WatchActorsOnce() {
  static std::once_flag once;
  std::call_once(once, [] { std::thread(WatchActors).detach(); });
}

#endif  // WWE12_DEV_PROBES
