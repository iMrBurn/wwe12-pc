// Probe (wwe12_down_probe, off by default): how a move ends, face up or face down.
//
// Question from the Mods session (2026-11-09): an added move (SvR 2011 Batista Bomb on DLC
// Batista, move 57284, anim 50399) ends with the opponent on his back, then the game rolls him
// onto his stomach; the DLC's own Batista Bomb (anim 50429) does not. Which state does the game
// choose at the end of the move, and from what?
//
// A host thread polls the match actors (0x8317B770, 0-5 = superstars) every 5 ms and logs each
// change of their state fields ("[au_sol]"):
//   st   = u16 at [actor+680]  (state / animation in progress)
//   move = u16 at actor+700    (move number)
//   e1   = u8 at [actor+684], e2 = u8 at [actor+688] (step bytes)
//   a/b/c = u32 at actor+1076, +1096, +1188 (read by move events, sub_824A5458)
//   opp  = actor+3032 (opponent's index)
// Comparing both moves' lines shows which state differs at the end.

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(wwe12_down_probe, false, "WWE12",
                    "Probe: log the superstars' state changes in a match (face up / face down at the end of a move).");

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <atomic>
#include <algorithm>
#include <chrono>
#include <string>
#include <thread>

#include <fmt/format.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

void Wwe12DownProbeStart();

namespace {

std::string Line(u32 actor) {
  using bd::mem::try_field;
  const u32 st = try_field<u32>(actor, 680);
  const u32 p1 = try_field<u32>(actor, 684);
  const u32 p2 = try_field<u32>(actor, 688);
  return fmt::format("st {} move {} e1 {} e2 {} a {} b {} c {} opp {}",
                     st ? try_field<u16>(st, 0) : 0xFFFF, try_field<u16>(actor, 700),
                     p1 ? try_field<u8>(p1, 0) : 0xFF, p2 ? try_field<u8>(p2, 0) : 0xFF,
                     try_field<u32>(actor, 1076), try_field<u32>(actor, 1096),
                     try_field<u32>(actor, 1188), try_field<u32>(actor, 3032));
}


// Step 3 (2026-11-09): sub_822936C0 is not what sets the animation at a move's end. Page-guard
// trap: when a superstar's animation is a move (>= 50000), the u16 at [actor+680] is watched;
// each write that changes it is logged with the host instruction ("wwe12.exe+X", translate with
// wwe12.map), until the next animation is set (the slot is reset to 65535 first, by sub_8246FC80). One victim at a time (e2 = 1).

std::atomic<u32> g_watch{0};       // guest address of the watched u16 (0 = none)
std::atomic<u64> g_page{0};
std::atomic<u64> g_host{0};        // host address of the watched u16
std::atomic<u16> g_before{0};
u64 g_exe = 0;
thread_local u64 t_rearm = 0;
thread_local u64 t_rip = 0;
constexpr int kMax = 64;
std::atomic<int> g_n{0};
u64 g_rip[kMax];
u16 g_val[kMax];
u16 g_old[kMax];
u64 g_chain[kMax][8];

// Host call chain of the writing thread (from the single-step context), offsets into wwe12.exe.
void Chain(const CONTEXT *in, u64 *out) {
  CONTEXT c = *in;
  for (int f = 0; f < 8; ++f) {
    out[f] = c.Rip ? c.Rip - g_exe : 0;
    DWORD64 image = 0;
    PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(c.Rip, &image, nullptr);
    if (!fn) {
      for (int k = f + 1; k < 8; ++k)
        out[k] = 0;
      return;
    }
    void *data = nullptr;
    DWORD64 frame = 0;
    RtlVirtualUnwind(UNW_FLAG_NHANDLER, image, c.Rip, fn, &c, &data, &frame, nullptr);
  }
}

void Guard(u64 page) {
  DWORD old = 0;
  VirtualProtect(reinterpret_cast<void *>(page), 0x1000, PAGE_READWRITE | PAGE_GUARD, &old);
}

LONG CALLBACK TrapHandler(EXCEPTION_POINTERS *ep) {
  const DWORD code = ep->ExceptionRecord->ExceptionCode;
  if (code == STATUS_SINGLE_STEP) {
    if (!t_rearm)
      return EXCEPTION_CONTINUE_SEARCH;
    const u64 page = t_rearm;
    t_rearm = 0;
    if (const u64 h = g_host.load(); h && t_rip) {
      const u16 v = _byteswap_ushort(*reinterpret_cast<volatile u16 *>(h));
      const u16 b = g_before.load();
      if (v != b) {
        const int i = g_n.fetch_add(1);
        if (i < kMax) {
          g_rip[i] = t_rip - g_exe;
          g_val[i] = v;
          g_old[i] = b;
          Chain(ep->ContextRecord, g_chain[i]);
        }
        g_before = v;
        if (v != 0xFFFF) {  // the next animation is set (65535 = slot reset first): done
          g_watch = 0;
          g_host = 0;
          g_page = 0;
        }
      }
    }
    t_rip = 0;
    if (g_page.load() == page)
      Guard(page);
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  if (code != STATUS_GUARD_PAGE_VIOLATION || ep->ExceptionRecord->NumberParameters < 2)
    return EXCEPTION_CONTINUE_SEARCH;
  const u64 addr = u64(ep->ExceptionRecord->ExceptionInformation[1]);
  const u64 page = addr & ~u64(0xFFF);
  if (page != g_page.load())
    return EXCEPTION_CONTINUE_SEARCH;
  const u64 h = g_host.load();
  const bool write = ep->ExceptionRecord->ExceptionInformation[0] == 1;
  t_rip = (write && h && addr <= h + 1 && h < addr + 16) ? ep->ContextRecord->Rip : 0;
  t_rearm = page;
  ep->ContextRecord->EFlags |= 0x100;  // trap flag: guard again right after this access
  return EXCEPTION_CONTINUE_EXECUTION;
}

int g_flushed = 0;
void FlushTrap() {
  const int n = std::min(g_n.load(), kMax);
  for (; g_flushed < n; ++g_flushed)
  {
    std::string chain;
    for (u64 o : g_chain[g_flushed])
      if (o)
        chain += fmt::format(" {:X}", o);
    BD_INFO("[au_sol] ecriture anim {} -> {} par wwe12.exe+{:X} pile:{}", g_old[g_flushed], g_val[g_flushed],
            g_rip[g_flushed], chain);
  }
}

void Arm(u32 guest_u16, u16 value) {
  if (g_watch.load())
    return;
  auto *host = bd::mem::try_at<u8>(guest_u16);
  if (!host)
    return;
  g_before = value;
  g_host = u64(host);
  g_watch = guest_u16;
  g_page = u64(host) & ~u64(0xFFF);
  Guard(g_page.load());
  BD_INFO("[au_sol] piege sur {:08X} (anim {})", guest_u16, value);
}

void Poll() {
  constexpr u32 kActors = 0x8317B770;
  std::string last[6];
  const auto t0 = std::chrono::steady_clock::now();
  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    const long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - t0)
                             .count();
    FlushTrap();
    for (u32 i = 0; i < 6; ++i) {
      const u32 actor = bd::mem::try_load<u32>(kActors + i * 4);
      std::string line = actor ? Line(actor) : std::string("-");
      if (actor) {
        const u32 st = bd::mem::try_field<u32>(actor, 680);
        const u16 cur = st ? bd::mem::try_field<u16>(st, 0) : 0;
        const u32 p2 = bd::mem::try_field<u32>(actor, 688);
        // the victim of a move: e2 = 1 at the move's start (seen in step 1)
        if (st && cur >= 50000 && p2 && bd::mem::try_field<u8>(p2, 0) == 1)
          Arm(st, cur);
      }
      if (line != last[i]) {
        BD_INFO("[au_sol] {} acteur {} : {}", ms, i, line);
        last[i] = std::move(line);
      }
    }
  }
}

}  // namespace

void Wwe12DownProbeStart() {
  if (!REXCVAR_GET(wwe12_down_probe))
    return;
  g_exe = u64(GetModuleHandleW(nullptr));
  AddVectoredExceptionHandler(1, &TrapHandler);
  std::thread(Poll).detach();
  BD_INFO("[au_sol] sonde active");
}

// Step 2: sub_822936C0(actor r3, animation r4) sets a superstar's animation (it writes
// [actor+680]). When the probe is on and a move animation (>= 50000) is replaced by a low one
// (< 100: down, face up / face down...), log the caller (lr) and the code addresses found on the
// guest stack, to find the code that picks face up or face down.
REX_EXTERN(__imp__sub_822936C0);

extern "C" REX_FUNC(sub_822936C0) {
  if (REXCVAR_GET(wwe12_down_probe) && ctx.r3.u32 && ctx.r4.u32 < 100) {
    const u32 actor = ctx.r3.u32;
    const u32 st = bd::mem::try_field<u32>(actor, 680);
    const u16 cur = st ? bd::mem::try_field<u16>(st, 0) : 0;
    if (cur >= 50000) {
      std::string chain;
      const u32 sp = ctx.r1.u32;
      for (u32 off = 0, n = 0; off < 0x600 && n < 12; off += 4) {
        const u32 v = bd::mem::try_load<u32>(sp + off);
        if (v >= 0x82000000 && v < 0x82C00000 && (v & 3) == 0) {
          chain += fmt::format(" {:08X}", v);
          ++n;
        }
      }
      BD_INFO("[au_sol] set anim {} -> {} actor {:08X} lr {:08X} r5 {:08X} r6 {:08X} pile:{}", cur,
              ctx.r4.u32, actor, u32(ctx.lr), ctx.r5.u32, ctx.r6.u32, chain);
    }
  }
  __imp__sub_822936C0(ctx, base);
}

// Step 4: the write of the victim's next animation (0 face up / 1 face down) comes from
// sub_824AE820 (called by sub_824ACFF0 through [[r4]+28]): it reads a flag at record+3 and the
// animation as a little-endian u32 at record+4, then sub_8247AD18 -> sub_8247A7E8 ->
// sub_82478AE8 writes it. Log the record's bytes (and around it) so the Mods session can find it
// in the move's files.
REX_EXTERN(__imp__sub_824AE820);

extern "C" REX_FUNC(sub_824AE820) {
  static std::atomic<int> logged{0};
  if (REXCVAR_GET(wwe12_down_probe) && ctx.r4.u32 && logged.fetch_add(1) < 400) {
    const u32 rec = ctx.r4.u32;
    std::string bytes;
    for (u32 i = 0; i < 48; ++i) {
      if (i == 16)
        bytes += " |";
      bytes += fmt::format(" {:02X}", bd::mem::try_load<u8>(rec - 16 + i));
    }
    BD_INFO("[au_sol] commande anim r3 {:08X} rec {:08X} lr {:08X} octets(rec-16..+32):{}", ctx.r3.u32, rec,
            u32(ctx.lr), bytes);
  }
  __imp__sub_824AE820(ctx, base);
}

// The dispatcher sub_824ACFF0(r3, object r4, command r5: u16 id at +0): log the command when
// the object's method is sub_824AE820.
REX_EXTERN(__imp__sub_824ACFF0);

extern "C" REX_FUNC(sub_824ACFF0) {
  static std::atomic<int> logged{0};
  if (REXCVAR_GET(wwe12_down_probe) && ctx.r4.u32 && ctx.r5.u32) {
    const u32 vt = bd::mem::try_load<u32>(ctx.r4.u32);
    if (vt && bd::mem::try_load<u32>(vt + 28) == 0x824AE820 && logged.fetch_add(1) < 400) {
      std::string bytes;
      for (u32 i = 0; i < 32; ++i)
        bytes += fmt::format(" {:02X}", bd::mem::try_load<u8>(ctx.r5.u32 + i));
      BD_INFO("[au_sol] commande {} objet {:08X} (vt {:08X}) cmd {:08X} octets:{}",
              bd::mem::try_load<u16>(ctx.r5.u32), ctx.r4.u32, vt, ctx.r5.u32, bytes);
    }
  }
  __imp__sub_824ACFF0(ctx, base);
}
