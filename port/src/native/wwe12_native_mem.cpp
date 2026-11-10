// WWE '12: the game's memcpy and memset run as the PC's own (2026-11-05; check mode 2026-11-10).
//
// sub_82A76060 (memcpy: r3 destination, r4 source, r5 bytes; returns r3) and sub_82A76590
// (memset: r3 destination, r4 byte, r5 bytes; returns r3) are the C library of the game, called
// from everywhere (the text grid of Create a Story rebuilds its lines with them). Recompiled, they
// copy through PowerPC loads and stores one by one (memset: bytes up to alignment, then 16-byte
// rounds of words, then the tail; memcpy: the same with 128-byte blocks and cache hints dcbt /
// dcbtst, no dcbz: read 2026-11-10 in the generated code); the PC's memcpy / memset give the same
// bytes far faster. Only plain cases go native: no overlap (the game's forward copy of an overlap
// is kept as it is) and addresses below 0xE0000000 (the physical windows above are placed
// differently on the host). Everything else still runs the original. r3 is left as it came in
// (both return the destination).
//
// wwe12_native_mem = true turns it on (off by default until checked and measured).
// wwe12_native_mem_check = true (dev): each native call is checked against the original, byte for
// byte, 64 bytes around the destination included: the original runs first, its result is kept,
// the bytes are put back as they were, the native one runs, both are compared; a difference is
// logged ("[nativemem] DIFFERENT") and the original's bytes are kept. Slow: tests only.
// Every 10 s "[nativemem]" logs the calls, bytes, and (check mode) the time of both ways.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <vector>

#include <rex/cvar.h>

#include "core/logging.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_native_mem, false, "WWE12",
                    "The game's memcpy / memset run as the PC's own (same bytes, less CPU).");
REXCVAR_DEFINE_BOOL(wwe12_native_mem_check, false, "WWE12",
                    "Dev: check every native memcpy / memset against the game's own, byte for byte (slow).");

REX_EXTERN(__imp__sub_82A76060);
REX_EXTERN(__imp__sub_82A76590);

namespace {

using Clock = std::chrono::steady_clock;
constexpr u32 kMargin = 64;  // bytes checked on each side of the destination

std::atomic<u64> g_calls_native{0}, g_calls_original{0}, g_bytes_native{0};
std::atomic<u64> g_checked{0}, g_different{0}, g_original_ns{0}, g_native_ns{0};
std::atomic<int64_t> g_last_log_ms{0};

int64_t NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch())
      .count();
}

// The span [address, address + size) lies in the plainly mapped guest range.
bool Plain(u32 address, u32 size) {
  return u64(address) + size <= 0xE0000000ull && address != 0;
}

void MaybeLog() {
  const int64_t now = NowMs();
  int64_t last = g_last_log_ms.load(std::memory_order_relaxed);
  if (now - last < 10000 || !g_last_log_ms.compare_exchange_strong(last, now))
    return;
  const u64 checked = g_checked.exchange(0), orig_ns = g_original_ns.exchange(0),
            native_ns = g_native_ns.exchange(0);
  BD_INFO("[nativemem] 10 s: native {} calls ({} KB), original {} calls | checked {}, "
          "different {} | time original {:.2f} ms, native {:.2f} ms",
          g_calls_native.exchange(0), g_bytes_native.exchange(0) >> 10,
          g_calls_original.exchange(0), checked, g_different.load(),
          orig_ns / 1e6, native_ns / 1e6);
}

// Check mode: runs the original then `native` on the same starting bytes and compares.
template <typename Original, typename Native>
void Checked(PPCContext &ctx, u8 *base, u32 dst, u32 size, Original original, Native native,
             const char *what) {
  const u32 lo = dst >= kMargin ? dst - kMargin : 0;
  const u64 hi64 = std::min<u64>(u64(dst) + size + kMargin, 0xE0000000ull);
  const u32 span = u32(hi64 - lo);
  thread_local std::vector<u8> before, by_original;
  before.assign(base + lo, base + lo + span);
  const PPCContext saved = ctx;
  auto t0 = Clock::now();
  original();
  auto t1 = Clock::now();
  by_original.assign(base + lo, base + lo + span);
  std::memcpy(base + lo, before.data(), span);  // back as it was
  auto t2 = Clock::now();
  native();
  auto t3 = Clock::now();
  g_original_ns.fetch_add(u64((t1 - t0).count()), std::memory_order_relaxed);
  g_native_ns.fetch_add(u64((t3 - t2).count()), std::memory_order_relaxed);
  g_checked.fetch_add(1, std::memory_order_relaxed);
  if (std::memcmp(base + lo, by_original.data(), span) != 0) {
    u32 first = 0;
    while (first < span && base[lo + first] == by_original[first])
      ++first;
    if (g_different.fetch_add(1, std::memory_order_relaxed) < 32)
      BD_INFO("[nativemem] DIFFERENT {}: dst {:08X} bytes {} r4 {:08X}, first difference at "
              "{:08X} (original {:02X}, native {:02X})",
              what, dst, size, saved.r4.u32, lo + first, by_original[first], base[lo + first]);
    std::memcpy(base + lo, by_original.data(), span);  // keep the game's own result
  }
  ctx.r3 = saved.r3;  // both return the destination
}

}  // namespace

extern "C" REX_FUNC(sub_82A76060) {
  const u32 dst = ctx.r3.u32, src = ctx.r4.u32, size = ctx.r5.u32;
  if (!REXCVAR_GET(wwe12_native_mem) || !Plain(dst, size) || !Plain(src, size) ||
      (dst < src + size && src < dst + size)) {
    g_calls_original.fetch_add(1, std::memory_order_relaxed);
    __imp__sub_82A76060(ctx, base);
    MaybeLog();
    return;
  }
  g_calls_native.fetch_add(1, std::memory_order_relaxed);
  g_bytes_native.fetch_add(size, std::memory_order_relaxed);
  if (REXCVAR_GET(wwe12_native_mem_check))
    Checked(ctx, base, dst, size, [&] { __imp__sub_82A76060(ctx, base); },
            [&] { std::memcpy(base + dst, base + src, size); }, "memcpy");
  else
    std::memcpy(base + dst, base + src, size);
  MaybeLog();
}

extern "C" REX_FUNC(sub_82A76590) {
  const u32 dst = ctx.r3.u32, size = ctx.r5.u32;
  if (!REXCVAR_GET(wwe12_native_mem) || !Plain(dst, size)) {
    g_calls_original.fetch_add(1, std::memory_order_relaxed);
    __imp__sub_82A76590(ctx, base);
    MaybeLog();
    return;
  }
  const int value = int(ctx.r4.u32 & 0xFF);
  g_calls_native.fetch_add(1, std::memory_order_relaxed);
  g_bytes_native.fetch_add(size, std::memory_order_relaxed);
  if (REXCVAR_GET(wwe12_native_mem_check))
    Checked(ctx, base, dst, size, [&] { __imp__sub_82A76590(ctx, base); },
            [&] { std::memset(base + dst, value, size); }, "memset");
  else
    std::memset(base + dst, value, size);
  MaybeLog();
}
