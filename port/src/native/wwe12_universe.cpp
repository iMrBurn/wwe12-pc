// WWE '12: Universe mode, reset and probe (step 1 of the in-game RESET UNIVERSE, see docs/univers.md).
//
// Everything here is off unless wwe12_universe_dev = true (tools/dev/lancer_test_univers.bat).
//
// Probe. The Universe menu (MATCH CARDS ... EDIT BRAND, string ids 27041-27047) is its own screen,
// not a menu.pac menu, so the generic "decide" hook never sees it. Its texts come from the
// game's string manager (object at *0x8317B86C, vtable 0x82000C04, slot 5 = sub_821A4400(this,
// id) returns the text). Each distinct request for a Universe text (ids 27000-27060) is logged
// once with the call chain (return addresses up the guest stack). First result (2026-10-20): the
// Universe menu is drawn by sub_8255EC88 (a state machine, state at +44, vtable near 0x82023DF8).
//
// Reset, test version. The reset does what a new save does to the Universe:
//   1. sub_826D5938(U): clears the Universe block (U = sub_82683A30()).
//   2. sub_826BE3C0(entry 4 of BATL/SAVE, size): the Universe's starting data (superstars, show
//      cards, PPVs, brands, relationships), the new-save callback of entry 4 (table 0x83073240).
//   3. The Universe championship table (0x83226508, 23 titles x 2 u16 holders, 512 = nobody) gets
//      the default holders of the table at 0x8324E3A8 (16-byte records: +0 holder, +4 partner,
//      +12 kind, 2 = tag), as sub_82A39E08 does, but for the Universe table only.
// Clearing alone crashes on entering the Universe (tested 2026-10-18): step 2 is what a new save
// adds. The game reads the Universe start data ("BATL/SAVE") on the way into the Universe
// (sub_826EC008 is its "read end" callback, args (this, data)); a copy is kept then. F8 while the
// Universe menu shows runs the reset (from the text hook, game thread); leave the Universe and come
// back to see it. Test it only on a copy of the player's save.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <tuple>

#include <fmt/format.h>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_universe_dev, false, "WWE12",
                    "Dev: Universe probe (log of the Universe menu's texts and callers) and F8 on the "
                    "Universe menu = reset the Universe. Test only on a copy of the save.");

REX_EXTERN(__imp__sub_827D8E30);  // pack: index of an entry id (pack, id), -1 if none
REX_EXTERN(__imp__sub_827D9528);  // pack: new expanded copy of an entry (pack, index)
REX_EXTERN(__imp__sub_827D2EA0);  // free (data, 0)
REX_EXTERN(__imp__sub_82683A30);  // Universe block
REX_EXTERN(__imp__sub_826D5938);  // Universe block clear
REX_EXTERN(__imp__sub_826BE3C0);  // Universe new-save init (data, size)

namespace {

constexpr u32 kUniverseTitles = 0x83226508;   // 23 titles x {holder, partner}, u16
constexpr u32 kDefaultChampions = 0x8324E3A8; // 16-byte records, loaded flag at +368
constexpr u32 kTitleCount = 23;
constexpr u16 kNobody = 512;

// Entry 4 of BATL/SAVE (the Universe's starting data), copied into guest memory of our own.
u32 g_start_data = 0;
u32 g_start_size = 0;

// Calls a guest function with up to three arguments, on a copy of the hooked call's context.
template <typename Fn> u32 Call(const PPCContext &in, u8 *base, Fn fn, u32 a, u32 b = 0, u32 c = 0) {
  PPCContext x = in;
  x.r3.u64 = a;
  x.r4.u64 = b;
  x.r5.u64 = c;
  fn(x, base);
  return x.r3.u32;
}

// Return addresses up the guest stack: lr, then each frame's saved LR (stored at back chain - 8).
std::array<u32, 5> CallChain(const PPCContext &ctx) {
  std::array<u32, 5> chain{u32(ctx.lr)};
  u32 frame = ctx.r1.u32;
  for (size_t i = 1; i < chain.size() && frame; ++i) {
    const u32 back = bd::mem::load<u32>(frame);
    if (!back || back <= frame || back - frame > 0x10000)
      break;
    chain[i] = bd::mem::load<u32>(back - 8);
    frame = back;
  }
  return chain;
}

void LogTextRequest(u32 id, const PPCContext &ctx) {
  static std::mutex s_mu;
  static std::set<std::tuple<u32, u32, u32>> s_seen;
  const auto chain = CallChain(ctx);
  std::lock_guard lock(s_mu);
  if (s_seen.size() >= 400 || !s_seen.insert({id, chain[0], chain[1]}).second)
    return;
  BD_INFO("[univers] text {} asked by {:08X} <- {:08X} <- {:08X} <- {:08X} <- {:08X} (this 0x{:08X})",
          id, chain[0], chain[1], chain[2], chain[3], chain[4], ctx.r3.u32);
}

// The read-end callback gets the opened BATL/SAVE pack (a reader handle, on the loader's stack):
// its entry with id 4 is the Universe start data, BPE-compressed, itself a pack (entries 0
// superstars, 1 relationships, 2 shows, 3 PPVs, 4 settings, 5 brands, as sub_826BE3C0 reads them).
// The game's reader gives it back expanded: sub_827D8E30(pack, id) = index, sub_827D9528(pack,
// index) = a new copy of the entry (freed with sub_827D2EA0(copy, 0)). A "PACH" pack: "PACH", u32
// count, then count x {id, offset, size} (little-endian), offsets from the end of that table.
u32 PackSize(u32 pack) {
  const u8 *p = bd::mem::at<u8>(pack);
  if (!p || std::memcmp(p, "PACH", 4) != 0)
    return 0;
  const auto le32 = [p](u32 o) { return u32(p[o]) | u32(p[o + 1]) << 8 | u32(p[o + 2]) << 16 | u32(p[o + 3]) << 24; };
  const u32 count = le32(4);
  if (!count || count > 64)
    return 0;
  const u32 table_end = 8 + count * 12;
  u32 end = table_end;
  for (u32 i = 0; i < count; ++i)
    end = std::max(end, table_end + le32(8 + i * 12 + 4) + le32(8 + i * 12 + 8));
  return end < 0x100000 ? end : 0;
}

void KeepStartData(const PPCContext &in, u8 *base, u32 batl_save) {
  auto *memory = REX_KERNEL_MEMORY();
  const u32 index = Call(in, base, __imp__sub_827D8E30, batl_save, 4);
  if (index == 0xFFFFFFFFu) {
    BD_INFO("[univers] BATL/SAVE read: no entry 4 in pack 0x{:08X}", batl_save);
    return;
  }
  const u32 pack = Call(in, base, __imp__sub_827D9528, batl_save, index);
  const u32 size = pack ? PackSize(pack) : 0;
  if (!size) {
    BD_INFO("[univers] BATL/SAVE read: entry 4 (0x{:08X}) is not a PACH pack, not kept", pack);
    if (pack)
      Call(in, base, __imp__sub_827D2EA0, pack, 0);
    return;
  }
  if (g_start_data && g_start_size < size) {
    memory->SystemHeapFree(g_start_data);
    g_start_data = 0;
  }
  if (!g_start_data)
    g_start_data = memory->SystemHeapAlloc(size, 0x20);
  if (g_start_data) {
    std::memcpy(bd::mem::at<u8>(g_start_data), bd::mem::at<u8>(pack), size);
    g_start_size = size;
  }
  Call(in, base, __imp__sub_827D2EA0, pack, 0);
  BD_INFO("[univers] BATL/SAVE read: Universe start data kept, {} bytes", g_start_size);
}

void ResetUniverseChampions() {
  if (!bd::mem::load<u32>(kDefaultChampions + 368)) {
    BD_INFO("[univers] reset: default champions not loaded, Universe champions kept");
    return;
  }
  for (u32 t = 0; t < kTitleCount; ++t) {
    const u32 rec = kDefaultChampions + t * 16;
    const u16 holder = u16(bd::mem::load<u32>(rec));
    const u16 partner = bd::mem::load<u8>(rec + 12) == 2 ? u16(bd::mem::load<u32>(rec + 4)) : kNobody;
    bd::mem::store<u16>(kUniverseTitles + t * 4, holder);
    bd::mem::store<u16>(kUniverseTitles + t * 4 + 2, partner);
  }
}

void ResetUniverse(const PPCContext &in, u8 *base) {
  const u32 universe = Call(in, base, __imp__sub_82683A30, 0);
  if (!g_start_data || !g_start_size || !universe) {
    BD_INFO("[univers] reset: no start data yet (0x{:08X}, {} bytes), nothing done", g_start_data,
            g_start_size);
    return;
  }
  Call(in, base, __imp__sub_826D5938, universe);
  Call(in, base, __imp__sub_826BE3C0, g_start_data, g_start_size);
  ResetUniverseChampions();
  BD_INFO("[univers] reset: done (block 0x{:08X} cleared, start data applied, default champions); "
          "leave the Universe and come back",
          universe);
}

}  // namespace

// The string manager's "text of id" method: the probe, and F8. It runs many times per frame on
// the game thread; a fresh F8 press resets the Universe if the text being asked for is a Universe
// one (ids 27000-27999: the menu, match cards, schedule...), i.e. while a Universe screen shows.
// (The menu list itself, 27041-27047, is only asked for when the menu is built, by sub_8255EC88.)
REX_EXTERN(__imp__sub_821A4400);
extern "C" REX_FUNC(sub_821A4400) {
  if (REXCVAR_GET(wwe12_universe_dev)) {
    const u32 id = ctx.r4.u32;
    if (id >= 27000 && id <= 27060)
      LogTextRequest(id, ctx);
    static bool s_was_down = true;  // a key already down at start is not a press
    const bool down = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
    if (down && !s_was_down) {
      s_was_down = true;  // first: the reset asks for texts itself (this hook runs again inside)
      const auto chain = CallChain(ctx);
      if (id >= 27000 && id <= 27999)
        ResetUniverse(ctx, base);
      else
        BD_INFO("[univers] F8 outside the Universe screens (text {} for {:08X} <- {:08X}): ignored",
                id, chain[0], chain[1]);
    } else if (!down) {
      s_was_down = false;
    }
  }
  __imp__sub_821A4400(ctx, base);
}

// BATL/SAVE read end (this, data): keep the Universe's start data.
REX_EXTERN(__imp__sub_826EC008);
extern "C" REX_FUNC(sub_826EC008) {
  const PPCContext in = ctx;
  __imp__sub_826EC008(ctx, base);
  if (REXCVAR_GET(wwe12_universe_dev))
    KeepStartData(in, base, in.r4.u32);
}
