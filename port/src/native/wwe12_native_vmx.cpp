// WWE '12: small hot VMX functions of the game run as plain PC code (2026-11-20, Rendu; docs/rendu_petit_pc.md
// on branch 2.0-natif). Off by default (wwe12_native_vmx), with a check mode (wwe12_native_vmx_check) that runs
// the original and the native version from the same state and compares every byte and register they write.
//
// sub_8282D920 (48 bytes): packs two floats (f1, f2) into two half floats at r3 / r3 + 2, through the stack and
// vpkd3d128 v63,v63,5,2,2 (D3D "FLOAT16_2"). The superstars' CPU skinning (sub_821E6C20 and its variants, on
// their own game thread) calls it once per vertex; on 2 slow cores it was 19 % of that thread (sampling of
// 2026-11-20). The recompiled body emulates the whole 4-lane vector and writes the host's MXCSR twice per call.
//
// What the original does, all reproduced here:
//   r11 = r10 = r1 - 16; f1, f2 stored as singles at r1 - 16 / r1 - 12 (flush mode off for stfs);
//   v63 = the 16 bytes at ea = (r1 - 16) & ~15; flush mode on (unconditionally);
//   the 4 words become 4 Xbox half floats (NOT IEEE: no rounding, NaN or > 65504 -> 0x7FFF, small values
//   shifted down, sign kept), written as the first 8 bytes of v63; the last 8 bytes keep words 2 and 3;
//   v63 stored back at ea; r8 = half at r1 - 14, r9 = half at r1 - 16; halves stored at r3 and r3 + 2.
// The half conversion is the generated code's own formula, so the bits are the same. Single conversion of a
// denormal differs with flush mode on or off only by giving +-0 instead of a tiny value, and both give the
// same half (+-0): the native version converts once, without changing the host's flush mode for it.

#include <atomic>
#include <chrono>
#include <cstring>

#include <rex/cvar.h>
#include <rex/platform.h>

#include "core/logging.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_native_vmx, false, "WWE12",
                    "Small hot vector functions of the game (half float packing of the superstars' skinning) "
                    "run as plain PC code (same bytes, less CPU).");
REXCVAR_DEFINE_BOOL(wwe12_native_vmx_check, false, "WWE12",
                    "Dev: run the game's own version too and compare every byte and register (slow); "
                    "'[nativevmx]' line every 10 s.");

REX_EXTERN(__imp__sub_8282D920);

namespace {

using Clock = std::chrono::steady_clock;

// The generated code's address rule (REX_PHYS_HOST_OFFSET): on Windows (and Apple arm64) the physical range
// from 0xE0000000 sits 4 KB further in the host mapping.
inline u8 *Raw(u8 *base, u32 a) {
#if REX_PLATFORM_WIN32 || (REX_PLATFORM_MAC && REX_ARCH_ARM64)
  return base + a + (a >= 0xE0000000u ? 0x1000u : 0u);
#else
  return base + a;
#endif
}
inline u32 LoadBE32(u8 *base, u32 a) {
  u32 v;
  std::memcpy(&v, Raw(base, a), 4);
  return __builtin_bswap32(v);
}
inline void StoreBE32(u8 *base, u32 a, u32 v) {
  v = __builtin_bswap32(v);
  std::memcpy(Raw(base, a), &v, 4);
}
inline u16 LoadBE16(u8 *base, u32 a) {
  u16 v;
  std::memcpy(&v, Raw(base, a), 2);
  return __builtin_bswap16(v);
}
inline void StoreBE16(u8 *base, u32 a, u16 v) {
  v = __builtin_bswap16(v);
  std::memcpy(Raw(base, a), &v, 2);
}

// vpkd3d128 FLOAT16_2 lane, as the recompiler writes it (port/generated, sub_8282D920).
inline u16 XenosHalf(u32 w) {
  float abs_f;
  const u32 abs_bits = w & 0x7FFFFFFFu;
  std::memcpy(&abs_f, &abs_bits, 4);
  const u8 e = (abs_f != abs_f) || (abs_f > 65504.0f) ? 0xFF : u8((w & 0x7f800000u) >> 23);
  const u16 m = e != 0xFF ? u16((w & 0x7FE000u) >> 13) : 0;
  u16 h = e != 0xFF ? (e > 0x70 ? u16(((e - 0x70) << 10) + m)
                                : (0x71 - e > 31 ? u16(0) : u16((0x400 + m) >> (0x71 - e))))
                    : u16(0x7FFF);
  h |= u16((w & 0x80000000u) >> 16);
  return h;
}

inline u32 FloatBits(double d) {
  const float f = float(d);
  u32 b;
  std::memcpy(&b, &f, 4);
  return b;
}

void PackHalf2Native(PPCContext &ctx, u8 *base) {
  const u32 sp16 = ctx.r1.u32 - 16;
  ctx.r11.s64 = ctx.r1.s64 + -16;
  ctx.r10.s64 = ctx.r1.s64 + -16;
  StoreBE32(base, sp16, FloatBits(ctx.f1.f64));
  StoreBE32(base, sp16 + 4, FloatBits(ctx.f2.f64));
  const u32 ea = sp16 & ~0xFu;
  const u32 w0 = LoadBE32(base, ea), w1 = LoadBE32(base, ea + 4), w2 = LoadBE32(base, ea + 8),
            w3 = LoadBE32(base, ea + 12);
  // Lane 0 loses its sign in the generated code: it writes the half into v63.u16[7], the upper half of the very
  // word u32[3] it then reads the sign from, so the sign it ORs in is bit 15 of the new half (always 0). Seen by
  // the check mode on 2026-11-20 (f1 -0.0174: game 2474, sign kept A474). The game on PC has always run with
  // this, so the native version does the same (h0 without its sign); the console's own vpkd3d128 likely keeps
  // it (a recompiler bug, noted in docs/rendu_petit_pc.md).
  const u16 h0 = u16(XenosHalf(w0) & 0x7FFFu), h1 = XenosHalf(w1), h2 = XenosHalf(w2), h3 = XenosHalf(w3);
  // v63 in the host layout of the generated code (lane order reversed: u32[3] = guest word 0).
  ctx.v63.u16[7] = h0;
  ctx.v63.u16[6] = h1;
  ctx.v63.u16[5] = h2;
  ctx.v63.u16[4] = h3;
  ctx.v63.u32[1] = w2;
  ctx.v63.u32[0] = w3;
  StoreBE16(base, ea, h0);
  StoreBE16(base, ea + 2, h1);
  StoreBE16(base, ea + 4, h2);
  StoreBE16(base, ea + 6, h3);
  // bytes ea + 8 .. ea + 15 keep w2 / w3 (the original stores them back unchanged)
  ctx.fpscr.enableFlushMode();  // the original leaves flush mode on
  ctx.r8.u64 = LoadBE16(base, ctx.r1.u32 - 14);
  ctx.r9.u64 = LoadBE16(base, ctx.r1.u32 - 16);
  StoreBE16(base, ctx.r3.u32, ctx.r9.u16);
  StoreBE16(base, ctx.r3.u32 + 2, ctx.r8.u16);
}

std::atomic<u64> g_checked{0}, g_different{0}, g_orig_ns{0}, g_native_ns{0};
std::atomic<int64_t> g_last_log_ms{0};

void MaybeLog() {
  const int64_t now =
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
  int64_t last = g_last_log_ms.load(std::memory_order_relaxed);
  if (now - last < 10000 || !g_last_log_ms.compare_exchange_strong(last, now))
    return;
  if (last == 0)
    return;
  BD_INFO("[nativevmx] 10 s: half pack (8282D920) checked {}, different {} (since start) | time original "
          "{:.2f} ms, native {:.2f} ms",
          g_checked.exchange(0), g_different.load(), g_orig_ns.exchange(0) / 1e6,
          g_native_ns.exchange(0) / 1e6);
}

// Check mode: both versions from the same state; the game's own result is kept.
void PackHalf2Checked(PPCContext &ctx, u8 *base) {
  const u32 ea = (ctx.r1.u32 - 16) & ~0xFu;
  const u32 dst = ctx.r3.u32;
  u8 stack_before[16], dst_before[4], stack_orig[16], dst_orig[4];
  std::memcpy(stack_before, Raw(base, ea), 16);
  std::memcpy(dst_before, Raw(base, dst), 4);
  const PPCContext start = ctx;

  const auto t0 = Clock::now();
  __imp__sub_8282D920(ctx, base);
  const auto t1 = Clock::now();
  const PPCContext by_original = ctx;
  std::memcpy(stack_orig, Raw(base, ea), 16);
  std::memcpy(dst_orig, Raw(base, dst), 4);

  // back to the start, then the native version
  std::memcpy(Raw(base, ea), stack_before, 16);
  std::memcpy(Raw(base, dst), dst_before, 4);
  ctx = start;
  ctx.fpscr.setcsr(start.fpscr.csr);
  const auto t2 = Clock::now();
  PackHalf2Native(ctx, base);
  const auto t3 = Clock::now();
  g_orig_ns.fetch_add(u64((t1 - t0).count()), std::memory_order_relaxed);
  g_native_ns.fetch_add(u64((t3 - t2).count()), std::memory_order_relaxed);
  g_checked.fetch_add(1, std::memory_order_relaxed);

  const bool same = std::memcmp(Raw(base, ea), stack_orig, 16) == 0 &&
                    std::memcmp(Raw(base, dst), dst_orig, 4) == 0 && ctx.r8.u64 == by_original.r8.u64 &&
                    ctx.r9.u64 == by_original.r9.u64 && ctx.r10.u64 == by_original.r10.u64 &&
                    ctx.r11.u64 == by_original.r11.u64 &&
                    std::memcmp(ctx.v63.u8, by_original.v63.u8, 16) == 0 &&
                    (ctx.fpscr.csr & ctx.fpscr.FlushMask) == (by_original.fpscr.csr & by_original.fpscr.FlushMask);
  if (!same) {
    if (g_different.fetch_add(1, std::memory_order_relaxed) < 32)
      BD_INFO("[nativevmx] DIFFERENT half pack: f1 {} f2 {} -> original {:04X} {:04X}, native {:04X} {:04X} "
              "(r1 {:08X}, r3 {:08X})",
              start.f1.f64, start.f2.f64, u32(by_original.r9.u16), u32(by_original.r8.u16), u32(ctx.r9.u16),
              u32(ctx.r8.u16), start.r1.u32, dst);
  }
  // keep the game's own result in every case
  std::memcpy(Raw(base, ea), stack_orig, 16);
  std::memcpy(Raw(base, dst), dst_orig, 4);
  ctx = by_original;
  ctx.fpscr.setcsr(by_original.fpscr.csr);
}

}  // namespace

extern "C" REX_FUNC(sub_8282D920) {
  if (REXCVAR_GET(wwe12_native_vmx_check)) {
    PackHalf2Checked(ctx, base);
    MaybeLog();
    return;
  }
  if (!REXCVAR_GET(wwe12_native_vmx)) {
    __imp__sub_8282D920(ctx, base);
    return;
  }
  PackHalf2Native(ctx, base);  // (no counter here: called once per skinned vertex)
}
