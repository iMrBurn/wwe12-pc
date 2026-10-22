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

#include <string>

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
  __imp__sub_822CDF78(ctx, base);
}

#endif  // WWE12_DEV_PROBES
