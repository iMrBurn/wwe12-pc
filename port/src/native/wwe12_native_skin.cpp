// WWE '12: the superstars' CPU skinning as PC code (2026-11-20, Rendu; plan in docs/rendu_petit_pc.md on branch
// 2.0-natif, "Piste 2"). Off by default (wwe12_native_skin), with a check mode (wwe12_native_skin_check) that runs
// the game's own version and this one from the same state on every call and compares the output byte for byte.
//
// sub_821E6C20 (544 bytes) is the hottest of the skinning variants that sub_821E8788 picks per mesh, on its own
// game thread: 54 % of that thread on 2 slow cores (sampling of 2026-11-20). Read from the generated code
// (port/generated/default/wwe12_recomp.65.cpp):
//   r4  mesh: +0 vertex count (int, reread each vertex); +104 -> pointer -> source vertices, 28 bytes (position
//       xyz, normal xyz, 4 unread); +108 bone records, 16 bytes (byte +0 bone A, float +4 weight A, byte +8 bone B;
//       255 = none); +112 UV pairs (2 floats per vertex);
//   r5  bone matrices, 64 bytes (4 rows of 4 floats, row 3 = translation);
//   r6  output vertices, 28 bytes: position xyz (+0), normal xyz (+12), UV as 2 Xbox half floats (+24).
// Per vertex, for each present bone: pos = row0*x + row1*y + row2*z + row3, nrm = row0*nx + row1*ny + row2*nz, both
// times the weight (A: its float; B: f30 - (A's weight, or f31 when A is absent), in double rounded to single);
// output = A + B, an absent bone counting as zeros. Exactly the generated code's operations in its order: separate
// multiply then add (no fused multiply-add), lane by lane, with the guest's flush mode (denormals as zero) on, as
// simde_mm_mul_ps / simde_mm_add_ps, so x86 and arm64 give the same bits as the recompiled code on each.
// The UVs go through sub_8282D920's packing (wwe12_native_vmx.cpp), with the same lane-0 sign loss.
// Volatile registers are not reproduced (the caller, sub_821E8788, reads none after the call); non-volatile ones are
// never touched (the original saves and restores them); the guest's flush mode is left off, as the original does.

#include <atomic>
#include <chrono>
#include <cstring>
#include <vector>

#include <rex/cvar.h>
#include <rex/platform.h>

#include "core/logging.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_native_skin, false, "WWE12",
                    "The superstars' CPU skinning (2-bone meshes, sub_821E6C20) runs as PC code (same bytes, less "
                    "CPU).");
REXCVAR_DEFINE_BOOL(wwe12_native_skin_check, false, "WWE12",
                    "Dev: run the game's own skinning too on every call and compare the output byte for byte "
                    "(slow); '[nativeskin]' line every 10 s.");

REX_EXTERN(__imp__sub_821E6C20);

namespace {

using Clock = std::chrono::steady_clock;

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
inline void StoreBE16(u8 *base, u32 a, u16 v) {
  v = __builtin_bswap16(v);
  std::memcpy(Raw(base, a), &v, 2);
}
inline float BitsToFloat(u32 b) {
  float f;
  std::memcpy(&f, &b, 4);
  return f;
}
inline u32 FloatToBits(float f) {
  u32 b;
  std::memcpy(&b, &f, 4);
  return b;
}

// The 4 floats at a (big-endian guest words), lane i = element i.
inline simde__m128 LoadVec(u8 *base, u32 a) {
  return simde_mm_setr_ps(BitsToFloat(LoadBE32(base, a)), BitsToFloat(LoadBE32(base, a + 4)),
                          BitsToFloat(LoadBE32(base, a + 8)), BitsToFloat(LoadBE32(base, a + 12)));
}

// vpkd3d128 FLOAT16_2 lane, the recompiler's formula (see wwe12_native_vmx.cpp).
inline u16 XenosHalf(u32 w) {
  const float abs_f = BitsToFloat(w & 0x7FFFFFFFu);
  const u8 e = (abs_f != abs_f) || (abs_f > 65504.0f) ? 0xFF : u8((w & 0x7f800000u) >> 23);
  const u16 m = e != 0xFF ? u16((w & 0x7FE000u) >> 13) : 0;
  u16 h = e != 0xFF ? (e > 0x70 ? u16(((e - 0x70) << 10) + m)
                                : (0x71 - e > 31 ? u16(0) : u16((0x400 + m) >> (0x71 - e))))
                    : u16(0x7FFF);
  h |= u16((w & 0x80000000u) >> 16);
  return h;
}

// One bone: (pos, nrm) times weight w, as the generated code orders it.
inline void Bone(u8 *base, u32 rows, simde__m128 px, simde__m128 py, simde__m128 pz, simde__m128 nx,
                 simde__m128 ny, simde__m128 nz, simde__m128 w, simde__m128 &pos, simde__m128 &nrm) {
  // lvx: 16-byte aligned rows
  const simde__m128 r0 = LoadVec(base, rows & ~0xFu), r1 = LoadVec(base, (rows + 16) & ~0xFu),
                    r2 = LoadVec(base, (rows + 32) & ~0xFu), r3 = LoadVec(base, (rows + 48) & ~0xFu);
  simde__m128 n = simde_mm_mul_ps(nz, r2);                       // vmulfp128 v7,v53,v0
  simde__m128 p = simde_mm_add_ps(simde_mm_mul_ps(r2, pz), r3);  // vmaddfp v0,v0,v9,v11
  n = simde_mm_add_ps(simde_mm_mul_ps(ny, r1), n);               // vmaddfp v11,v8,v13,v7
  p = simde_mm_add_ps(simde_mm_mul_ps(py, r1), p);               // vmaddfp v0,v6,v13,v0
  n = simde_mm_add_ps(simde_mm_mul_ps(nx, r0), n);               // vmaddfp v13,v5,v12,v11
  p = simde_mm_add_ps(simde_mm_mul_ps(px, r0), p);               // vmaddfp v0,v10,v12,v0
  nrm = simde_mm_mul_ps(n, w);                                   // vmulfp128 v59,v13,v51
  pos = simde_mm_mul_ps(p, w);                                   // vmulfp128 v58,v0,v51
}

void SkinNative(PPCContext &ctx, u8 *base) {
  const u32 mesh = ctx.r4.u32, mats = ctx.r5.u32;
  const u32 bones0 = LoadBE32(base, mesh + 108), uv0 = LoadBE32(base, mesh + 112);
  // f30 / f31: the same constants the original loads (lis r11,-32238 / lis r10,-32256)
  const double f30 = double(BitsToFloat(LoadBE32(base, 0x82120000u - 20428)));
  const double f31 = double(BitsToFloat(LoadBE32(base, 0x82000000u + 2776)));
  ctx.fpscr.enableFlushMode();  // vector math with denormals as zero, as the original
  const simde__m128 zero = simde_mm_setzero_ps();
  u32 out = ctx.r6.u32, src_off = 0, rec = bones0, uv = uv0;
  for (int32_t i = 0; i < int32_t(LoadBE32(base, mesh + 0)); ++i) {
    const u32 src = LoadBE32(base, LoadBE32(base, mesh + 104)) + src_off;
    const simde__m128 px = simde_mm_set1_ps(BitsToFloat(LoadBE32(base, src))),
                      py = simde_mm_set1_ps(BitsToFloat(LoadBE32(base, src + 4))),
                      pz = simde_mm_set1_ps(BitsToFloat(LoadBE32(base, src + 8))),
                      nx = simde_mm_set1_ps(BitsToFloat(LoadBE32(base, src + 12))),
                      ny = simde_mm_set1_ps(BitsToFloat(LoadBE32(base, src + 16))),
                      nz = simde_mm_set1_ps(BitsToFloat(LoadBE32(base, src + 20)));
    simde__m128 pos_a = zero, nrm_a = zero, pos_b = zero, nrm_b = zero;
    double f0 = f31;
    const u32 bone_a = Raw(base, rec)[0];
    if (bone_a != 255) {
      const u32 wa_bits = LoadBE32(base, rec + 4);
      f0 = double(BitsToFloat(wa_bits));
      Bone(base, (bone_a << 6) + mats, px, py, pz, nx, ny, nz, simde_mm_set1_ps(BitsToFloat(wa_bits)), pos_a,
           nrm_a);
    }
    const u32 bone_b = Raw(base, rec + 8)[0];
    if (bone_b != 255) {
      // fsubs f0,f30,f0: in double, rounded to single. (The original does it with flush mode off; a denormal
      // result would only differ as an input of the multiply, where denormals count as zero either way.)
      const float wb = float(f30 - f0);
      Bone(base, (bone_b << 6) + mats, px, py, pz, nx, ny, nz, simde_mm_set1_ps(wb), pos_b, nrm_b);
    }
    const simde__m128 pos = simde_mm_add_ps(pos_a, pos_b), nrm = simde_mm_add_ps(nrm_a, nrm_b);
    alignas(16) float p4[4], n4[4];
    simde_mm_store_ps(p4, pos);
    simde_mm_store_ps(n4, nrm);
    // stvewx: word stores at address & ~3
    StoreBE32(base, out & ~3u, FloatToBits(p4[0]));
    StoreBE32(base, (out + 4) & ~3u, FloatToBits(p4[1]));
    StoreBE32(base, (out + 8) & ~3u, FloatToBits(p4[2]));
    StoreBE32(base, (out + 12) & ~3u, FloatToBits(n4[0]));
    StoreBE32(base, (out + 16) & ~3u, FloatToBits(n4[1]));
    StoreBE32(base, (out + 20) & ~3u, FloatToBits(n4[2]));
    // sub_8282D920 on (uv[0], uv[1]): lane 0 without its sign (the recompiled code's own result)
    StoreBE16(base, out + 24, u16(XenosHalf(LoadBE32(base, uv)) & 0x7FFFu));
    StoreBE16(base, out + 26, XenosHalf(LoadBE32(base, uv + 4)));
    out += 28;
    src_off += 28;
    rec += 16;
    uv += 8;
  }
  ctx.fpscr.disableFlushMode();  // the original returns with flush mode off
}

std::atomic<u64> g_calls{0}, g_vertices{0}, g_different{0}, g_orig_ns{0}, g_native_ns{0};
std::atomic<int64_t> g_last_log_ms{0};

void MaybeLog() {
  const int64_t now =
      std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count();
  int64_t last = g_last_log_ms.load(std::memory_order_relaxed);
  if (now - last < 10000 || !g_last_log_ms.compare_exchange_strong(last, now))
    return;
  if (last == 0)
    return;
  BD_INFO("[nativeskin] 10 s: 2-bone skinning (821E6C20) checked {} calls, {} vertices, different {} (since "
          "start) | time original {:.2f} ms, native {:.2f} ms",
          g_calls.exchange(0), g_vertices.exchange(0), g_different.load(), g_orig_ns.exchange(0) / 1e6,
          g_native_ns.exchange(0) / 1e6);
}

// Registers the caller may rely on after the call (PowerPC ABI): r1, r14-r31, f14-f31, v64-v127 (VMX128), and
// the guest's flush mode. (The context's fields are not in register order, so they are listed one by one.)
bool SameNonVolatile(const PPCContext &a, const PPCContext &b) {
#define R(n) (a.r##n.u64 == b.r##n.u64)
#define F(n) (a.f##n.u64 == b.f##n.u64)
  const bool gpr = R(1) && R(14) && R(15) && R(16) && R(17) && R(18) && R(19) && R(20) && R(21) && R(22) &&
                   R(23) && R(24) && R(25) && R(26) && R(27) && R(28) && R(29) && R(30) && R(31);
  const bool fpr = F(14) && F(15) && F(16) && F(17) && F(18) && F(19) && F(20) && F(21) && F(22) && F(23) &&
                   F(24) && F(25) && F(26) && F(27) && F(28) && F(29) && F(30) && F(31);
#undef R
#undef F
  if (!gpr || !fpr)
    return false;
  // v64-v127: the 64 vector registers that follow v63 in the context
  const PPCVRegister *va = &a.v64, *vb = &b.v64;
  for (int i = 0; i < 64; ++i)
    if (std::memcmp(va[i].u8, vb[i].u8, 16) != 0)
      return false;
  return (a.fpscr.csr & a.fpscr.FlushMask) == (b.fpscr.csr & b.fpscr.FlushMask);
}

void SkinChecked(PPCContext &ctx, u8 *base) {
  const int32_t count = int32_t(LoadBE32(base, ctx.r4.u32));
  const u32 out = ctx.r6.u32;
  const u32 bytes = count > 0 ? u32(count) * 28u : 0u;
  thread_local std::vector<u8> before, by_original;
  before.assign(Raw(base, out), Raw(base, out) + bytes);
  const PPCContext start = ctx;

  const auto t0 = Clock::now();
  __imp__sub_821E6C20(ctx, base);
  const auto t1 = Clock::now();
  const PPCContext after_original = ctx;
  by_original.assign(Raw(base, out), Raw(base, out) + bytes);

  std::memcpy(Raw(base, out), before.data(), bytes);
  ctx = start;
  ctx.fpscr.setcsr(start.fpscr.csr);
  const auto t2 = Clock::now();
  SkinNative(ctx, base);
  const auto t3 = Clock::now();
  g_orig_ns.fetch_add(u64((t1 - t0).count()), std::memory_order_relaxed);
  g_native_ns.fetch_add(u64((t3 - t2).count()), std::memory_order_relaxed);
  g_calls.fetch_add(1, std::memory_order_relaxed);
  g_vertices.fetch_add(u64(count > 0 ? count : 0), std::memory_order_relaxed);

  const bool same_bytes = bytes == 0 || std::memcmp(Raw(base, out), by_original.data(), bytes) == 0;
  if (!same_bytes || !SameNonVolatile(ctx, after_original)) {
    if (g_different.fetch_add(1, std::memory_order_relaxed) < 32) {
      u32 first = 0;
      while (first < bytes && Raw(base, out)[first] == by_original[first])
        ++first;
      u32 orig_word = 0, native_word = 0;
      if (first < bytes) {
        std::memcpy(&orig_word, by_original.data() + (first & ~3u), 4);
        orig_word = __builtin_bswap32(orig_word);
        native_word = LoadBE32(base, out + (first & ~3u));
      }
      BD_INFO("[nativeskin] DIFFERENT: {} vertices, first at vertex {} byte {} (original {:08X}, native {:08X}), "
              "registers same {} | mesh {:08X} out {:08X}",
              count, first / 28, first % 28, orig_word, native_word, SameNonVolatile(ctx, after_original),
              start.r4.u32, out);
    }
  }
  // keep the game's own result in every case
  std::memcpy(Raw(base, out), by_original.data(), bytes);
  ctx = after_original;
  ctx.fpscr.setcsr(after_original.fpscr.csr);
}

}  // namespace

extern "C" REX_FUNC(sub_821E6C20) {
  if (REXCVAR_GET(wwe12_native_skin_check)) {
    SkinChecked(ctx, base);
    MaybeLog();
    return;
  }
  if (!REXCVAR_GET(wwe12_native_skin)) {
    __imp__sub_821E6C20(ctx, base);
    return;
  }
  SkinNative(ctx, base);
}
