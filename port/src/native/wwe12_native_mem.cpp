// WWE '12: the game's memcpy and memset run as the PC's own (2026-11-05).
//
// sub_82A76060 (memcpy: r3 destination, r4 source, r5 bytes; returns r3) and sub_82A76590
// (memset: r3 destination, r4 byte, r5 bytes; returns r3) are the C library of the game, called
// from everywhere (the text grid of Create a Story rebuilds its lines with them). Recompiled, they
// copy through PowerPC loads and stores one by one (bytes up to alignment, then words, 128-byte
// blocks with dcbz); the PC's memcpy / memset give the same bytes far faster. Only plain cases go
// native: no overlap (the game's forward copy of an overlap is kept as it is) and addresses below
// 0xE0000000 (the physical windows above are placed differently on the host). Everything else
// still runs the original.
//
// wwe12_native_mem = true turns it on (off by default until it is measured and checked over more
// play: 1.2; on for 2 benchmark passes on 2026-11-06 with no problem seen).

#include <cstring>

#include <rex/cvar.h>

#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_native_mem, false, "WWE12",
                    "The game's memcpy / memset run as the PC's own (same bytes, less CPU).");

REX_EXTERN(__imp__sub_82A76060);
REX_EXTERN(__imp__sub_82A76590);

namespace {
// The span [address, address + size) lies in the plainly mapped guest range.
bool Plain(u32 address, u32 size) {
  return u64(address) + size <= 0xE0000000ull && address != 0;
}
}  // namespace

extern "C" REX_FUNC(sub_82A76060) {
  const u32 dst = ctx.r3.u32, src = ctx.r4.u32, size = ctx.r5.u32;
  if (!REXCVAR_GET(wwe12_native_mem) || !Plain(dst, size) || !Plain(src, size) ||
      (dst < src + size && src < dst + size)) {
    __imp__sub_82A76060(ctx, base);
    return;
  }
  std::memcpy(base + dst, base + src, size);
}

extern "C" REX_FUNC(sub_82A76590) {
  const u32 dst = ctx.r3.u32, size = ctx.r5.u32;
  if (!REXCVAR_GET(wwe12_native_mem) || !Plain(dst, size)) {
    __imp__sub_82A76590(ctx, base);
    return;
  }
  std::memset(base + dst, int(ctx.r4.u32 & 0xFF), size);
}
