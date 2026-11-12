// WWE '12 PC: the game's own heaps running out (wwe12_heap_probe, on by default: it only logs failures).
//
// With many added superstars (LA Max, 44: a crash entering the select screen; our test with 50: a crash loading the
// match, a write at 0x10 in sub_821EAC60) the game got 0 from its allocator and used it. Its objects come from
// sub_827D2548(size, ?) and sub_827D3600(size, ?): the current heap (sub_827D1F18: the calling thread's, from a table
// of thread ids at 0x83232828, else its default) out of the heap descriptors at [0x83232A90], 36 bytes each
// ({start, end, first free 16-byte unit, ...}). Each failure is logged once per caller and size: the heap, its span,
// what was asked; the first one also logs every heap's span. A first step, after Kaiko's model_mem.cpp (SvR 2011 PC
// port, MIT): know what runs out before giving it room.

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <mutex>
#include <set>
#include <tuple>

#include <fmt/format.h>
#include <rex/cvar.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_heap_probe, true, "WWE12",
                    "Log the game's own allocations that fail (its heaps full), once per caller and size.");
// (1.4, how many added superstars fit: the highest place each heap's blocks have reached, logged each time it rises by
// another MB - "[heaps] heap 12 reached 31 MB of 34 MB (91 %)" -, for the tests with many added superstars)
REXCVAR_DEFINE_BOOL(wwe12_heap_watermark, false, "WWE12",
                    "Log how high each of the game's heaps has been filled (each new MB), for tests with many mods.");

REX_EXTERN(__imp__sub_827D2548);
REX_EXTERN(__imp__sub_827D3600);
REX_EXTERN(__imp__sub_827D1F18);

namespace {

std::mutex g_mutex;
std::set<std::tuple<uint32_t, uint32_t, uint32_t>> g_logged;  // (allocator, caller, size)
bool g_heaps_logged = false;

constexpr uint32_t kHeapTable = 0x83232A90;  // -> heap descriptors, 36 bytes each

void LogFailure(PPCContext &ctx, uint8_t *base, uint32_t which, uint32_t size, uint32_t arg, uint32_t caller) {
  PPCContext x = ctx;
  __imp__sub_827D1F18(x, base);
  const uint32_t heap = x.r3.u32;
  std::lock_guard lock(g_mutex);
  if (!g_logged.insert({which, caller, size}).second)
    return;
  const uint32_t table = bd::mem::load<uint32_t>(kHeapTable);
  uint32_t start = 0, end = 0;
  if (table && heap < 64) {
    start = bd::mem::load<uint32_t>(table + heap * 36);
    end = bd::mem::load<uint32_t>(table + heap * 36 + 4);
  }
  REXLOG_WARN("[heaps] {} failed: {} bytes ({:08X}) for {:08X}, heap {} ({:08X}-{:08X}, {} KB)",
              which == 1 ? "sub_827D2548" : "sub_827D3600", size, arg, caller, heap, start, end,
              end > start ? (end - start) / 1024 : 0);
  if (!g_heaps_logged && table) {
    g_heaps_logged = true;
    for (uint32_t h = 0; h < 32; ++h) {
      const uint32_t s = bd::mem::load<uint32_t>(table + h * 36), e = bd::mem::load<uint32_t>(table + h * 36 + 4);
      if (!s && !e)
        continue;
      REXLOG_WARN("[heaps]   heap {}: {:08X}-{:08X} ({} KB)", h, s, e, e > s ? (e - s) / 1024 : 0);
    }
  }
}

// Per heap: the lowest start and the highest end of the blocks given out (from the heap's start; a heap may fill from
// either end), and the span last logged, in MB.
uint32_t g_low[64], g_high[64] = {}, g_span_logged[64] = {};
bool g_low_set = false;

void Watermark(PPCContext &ctx, uint8_t *base, uint32_t ptr, uint32_t size) {
  PPCContext x = ctx;
  __imp__sub_827D1F18(x, base);
  const uint32_t heap = x.r3.u32, table = bd::mem::load<uint32_t>(kHeapTable);
  if (heap >= 64 || !table)
    return;
  const uint32_t start = bd::mem::load<uint32_t>(table + heap * 36), end = bd::mem::load<uint32_t>(table + heap * 36 + 4);
  if (ptr < start || ptr >= end || end <= start)
    return;
  std::lock_guard lock(g_mutex);
  if (!g_low_set) {
    std::fill(std::begin(g_low), std::end(g_low), 0xFFFFFFFFu);
    g_low_set = true;
  }
  const uint32_t lo = ptr - start, hi = std::min(ptr + size, end) - start;
  if (lo >= g_low[heap] && hi <= g_high[heap])
    return;
  g_low[heap] = std::min(g_low[heap], lo);
  g_high[heap] = std::max(g_high[heap], hi);
  const uint32_t mb = (g_high[heap] - g_low[heap]) >> 20;
  if (mb + 1 <= g_span_logged[heap])
    return;
  g_span_logged[heap] = mb + 1;
  REXLOG_INFO("[heaps] heap {}: blocks from {} KB to {} KB of its {} KB ({} % used at most)", heap, g_low[heap] / 1024,
              g_high[heap] / 1024, (end - start) / 1024, uint64_t(g_high[heap] - g_low[heap]) * 100 / (end - start));
}

}  // namespace

extern "C" REX_FUNC(sub_827D2548) {
  const uint32_t size = ctx.r3.u32, arg = ctx.r4.u32, caller = uint32_t(ctx.lr);
  __imp__sub_827D2548(ctx, base);
  if (ctx.r3.u32 == 0 && size && REXCVAR_GET(wwe12_heap_probe))
    LogFailure(ctx, base, 1, size, arg, caller);
  else if (ctx.r3.u32 && REXCVAR_GET(wwe12_heap_watermark))
    Watermark(ctx, base, ctx.r3.u32, size);
}

extern "C" REX_FUNC(sub_827D3600) {
  const uint32_t size = ctx.r3.u32, arg = ctx.r4.u32, caller = uint32_t(ctx.lr);
  __imp__sub_827D3600(ctx, base);
  // (its own inner tries, from 827D37DC inside it, fail in every match, the game's too, and are taken care of by it)
  if (ctx.r3.u32 == 0 && size && REXCVAR_GET(wwe12_heap_probe) && !(caller >= 0x827D3600 && caller < 0x827D3900))
    LogFailure(ctx, base, 2, size, arg, caller);
  else if (ctx.r3.u32 && REXCVAR_GET(wwe12_heap_watermark))
    Watermark(ctx, base, ctx.r3.u32, size);
}

// The heaps' sizes: sub_8269F140 / sub_8269F680 make them from the table at 0x83073260, 45 x {heap, KB} (heap 12 =
// 19,106 KB, heap 15 = 4,608 KB...: 153 MB in all) inside the regions sub_821ABC28 maps (184 and 278 MB), with ~330 MB
// of physical memory still free after. wwe12_heap_grow ("heap:KB,...") adds to them before they are made. Default
// "12:16384,15:4096": with 50 added superstars heap 15 ran out loading a match (the crash, 148 bytes refused) after
// heap 12 had refused big blocks; with these the match loaded and played (2026-11-04; heap 12 still refuses a few
// big blocks there, which the game survives). Empty: the game's sizes.
REXCVAR_DEFINE_STRING(wwe12_heap_grow, "12:16384,15:4096", "WWE12",
                      "KB added to the game's heaps at start-up, \"heap:KB,heap:KB\" (room for many added superstars).");
// (2026-11-05: the room is taken from the heap made last in the space left, heap 54 - menus, the Universe, the arena
// transitions -, 31 MB -> 11 MB: with 8 added superstars the game crashed after a match, 393 KB refused for the
// transition, and in the Universe, 713 KB refused. Grown only from this many added superstars on.)
REXCVAR_DEFINE_INT32(wwe12_heap_grow_from, 25, "WWE12",
                     "wwe12_heap_grow applies from this many added superstars on (fewer: the game's sizes).");
size_t Wwe12AddedSuperstarCount();
REX_EXTERN(__imp__sub_8269F680);

namespace {
constexpr uint32_t kSizeTable = 0x83073260;

void LogPhysical(const char *when) {
  auto *memory = REX_KERNEL_MEMORY();
  if (!memory)
    return;
  std::string s;
  for (uint32_t ps : {4096u, 65536u, 16u << 20})
    if (auto *h = memory->LookupHeapByType(true, ps))
      s += fmt::format(" {}K pages {} of {} free;", ps >> 10, h->GetUnreservedPageCount(), h->GetTotalPageCount());
  REXLOG_INFO("[heaps] physical memory {}:{}", when, s);
}
}  // namespace

namespace {
void BeforeHeaps(const char *who) {
  REXLOG_INFO("[heaps] {} makes the heaps", who);
  static bool grown = false;  // (the table is the same at every call: grown once)
  const size_t added = Wwe12AddedSuperstarCount();
  const bool many = int64_t(added) >= int64_t(REXCVAR_GET(wwe12_heap_grow_from));
  const std::string grow = grown || !many ? std::string() : std::string(REXCVAR_GET(wwe12_heap_grow));
  if (!grown)
    REXLOG_INFO("[heaps] {} added superstar(s): the heaps {}", added, many ? "grown" : "as the game has them");
  grown = true;
  std::string table;
  for (uint32_t i = 0; i < 45; ++i) {
    const uint32_t heap = bd::mem::load<uint32_t>(kSizeTable + i * 8), kb = bd::mem::load<uint32_t>(kSizeTable + i * 8 + 4);
    table += fmt::format(" {}:{}", heap, kb);
  }
  REXLOG_INFO("[heaps] sizes (heap:KB):{}", table);
  if (!grow.empty()) {  // (the table may sit in read-only data: its host page made writable)
    DWORD old = 0;
    VirtualProtect(bd::mem::at<uint8_t>(kSizeTable & ~0xFFFu), 0x2000, PAGE_READWRITE, &old);
  }
  for (size_t at = 0; at < grow.size();) {
    const size_t comma = std::min(grow.find(',', at), grow.size()), colon = grow.find(':', at);
    if (colon < comma) {
      const uint32_t heap = uint32_t(atoi(grow.c_str() + at)), add = uint32_t(atoi(grow.c_str() + colon + 1));
      for (uint32_t i = 0; i < 45; ++i)
        if (bd::mem::load<uint32_t>(kSizeTable + i * 8) == heap) {
          const uint32_t kb = bd::mem::load<uint32_t>(kSizeTable + i * 8 + 4);
          bd::mem::store<uint32_t>(kSizeTable + i * 8 + 4, kb + add);
          REXLOG_INFO("[heaps] heap {}: {} KB -> {} KB", heap, kb, kb + add);
          break;
        }
    }
    at = comma + 1;
  }
  LogPhysical("before the heaps");
}
}  // namespace

REX_EXTERN(__imp__sub_8269F140);
extern "C" REX_FUNC(sub_8269F680) {
  BeforeHeaps("sub_8269F680");
  __imp__sub_8269F680(ctx, base);
  LogPhysical("after the heaps");
}
extern "C" REX_FUNC(sub_8269F140) {
  BeforeHeaps("sub_8269F140");
  __imp__sub_8269F140(ctx, base);
  LogPhysical("after the heaps");
  const uint32_t table = bd::mem::load<uint32_t>(kHeapTable);
  std::string spans;
  for (uint32_t h = 0; table && h < 44; ++h) {
    const uint32_t s = bd::mem::load<uint32_t>(table + h * 36), e = bd::mem::load<uint32_t>(table + h * 36 + 4);
    if (s || e)
      spans += fmt::format(" {}:{:08X}+{}K", h, s, e > s ? (e - s) / 1024 : 0);
  }
  REXLOG_INFO("[heaps] made:{}", spans);
}
