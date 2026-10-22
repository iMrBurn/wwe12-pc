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

#if defined(WWE12_DEV_PROBES)

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
    g_last_kind = to;
  } else if (kind != g_last_kind) {
    BD_INFO("[kind] match en cours : type {}", kind);
    g_last_kind = kind;
  }
}

#endif  // WWE12_DEV_PROBES
