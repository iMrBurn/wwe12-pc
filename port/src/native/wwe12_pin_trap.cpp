// Dev probe for the second pin system (1.4, session Gameplay): who reads the kick-out hold counter.
//
// Found with the pin probe (2026-11-14): while a human superstar is pinned, his 'COPE' component (node of
// [actor+3436] with tag 'COPE', object at node+24) has +244 = 64, +256 = 3, and +292 counts the frames A is held
// (the game's own kick-out gauge: hold A, release in the small zone). This probe (wwe12_pin_trap, off) puts a
// PAGE_GUARD trap on that word during the pin and logs each distinct host instruction that touches it, with its
// host call chain ("wwe12.exe+X", translate with wwe12.map): the code that compares it with the zone is among them.

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(wwe12_pin_trap, false, "WWE12",
                    "Dev: log the code that reads the pinned superstar's kick-out hold counter (slow during a pin).");

#include <windows.h>

#include <atomic>
#include <chrono>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include <fmt/format.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

void Wwe12PinTrapStart();

namespace {

constexpr u32 kActors = 0x8317B770;
constexpr u32 kCope = 0x434F5045;  // 'COPE'
constexpr int kMax = 96;

u64 g_exe = 0;
std::atomic<u64> g_host{0};  // host address of the watched u32
std::atomic<u64> g_page{0};
thread_local u64 t_rearm = 0;
std::atomic<int> g_n{0};
u64 g_rip[kMax];
u32 g_write[kMax];
u64 g_chain[kMax][8];

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

bool Seen(u64 rip) {
  const int n = std::min(g_n.load(), kMax);
  for (int i = 0; i < n; ++i)
    if (g_rip[i] == rip)
      return true;
  return false;
}

LONG CALLBACK Handler(EXCEPTION_POINTERS *ep) {
  const DWORD code = ep->ExceptionRecord->ExceptionCode;
  if (code == STATUS_SINGLE_STEP) {
    if (!t_rearm)
      return EXCEPTION_CONTINUE_SEARCH;
    const u64 page = t_rearm;
    t_rearm = 0;
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
  if (h && addr < h + 4 && h < addr + 8) {
    const u64 rip = ep->ContextRecord->Rip - g_exe;
    if (!Seen(rip)) {
      const int i = g_n.fetch_add(1);
      if (i < kMax) {
        g_rip[i] = rip;
        g_write[i] = ep->ExceptionRecord->ExceptionInformation[0] == 1;
        Chain(ep->ContextRecord, g_chain[i]);
      }
    }
  }
  t_rearm = page;
  ep->ContextRecord->EFlags |= 0x100;  // trap flag: guard again right after this access
  return EXCEPTION_CONTINUE_EXECUTION;
}

u32 CopeOf(u32 actor) {
  std::vector<u32> todo{bd::mem::try_load<u32>(actor + 3436)};
  std::set<u32> seen;
  while (!todo.empty() && seen.size() < 32) {
    const u32 node = todo.back();
    todo.pop_back();
    if (!node || !seen.insert(node).second)
      continue;
    if (bd::mem::try_load<u32>(node + 4) == kCope)
      return bd::mem::try_load<u32>(node + 24);
    todo.push_back(bd::mem::try_load<u32>(node + 16));
    todo.push_back(bd::mem::try_load<u32>(node + 20));
  }
  return 0;
}

void Poll() {
  int flushed = 0;
  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const int n = std::min(g_n.load(), kMax);
    for (; flushed < n; ++flushed) {
      std::string chain;
      for (u64 o : g_chain[flushed])
        if (o)
          chain += fmt::format(" {:X}", o);
      BD_INFO("[piege292] {} par wwe12.exe+{:X} pile:{}", g_write[flushed] ? "ecriture" : "lecture", g_rip[flushed],
              chain);
    }
    u32 watch = 0;
    for (u32 i = 0; i < 2 && !watch; ++i) {
      const u32 actor = bd::mem::try_load<u32>(kActors + i * 4);
      const u32 cope = actor ? CopeOf(actor) : 0;
      if (cope && bd::mem::try_load<u32>(cope + 244) == 64 && bd::mem::try_load<u32>(cope + 256) == 3)
        watch = cope + 292;
    }
    u8 *host = watch ? bd::mem::try_at<u8>(watch) : nullptr;
    const u64 want = host ? u64(host) : 0;
    if (want != g_host.load()) {
      const u64 old_page = g_page.load();
      g_host = want;
      g_page = want ? want & ~u64(0xFFF) : 0;
      if (old_page && old_page != g_page.load()) {
        DWORD o = 0;
        VirtualProtect(reinterpret_cast<void *>(old_page), 0x1000, PAGE_READWRITE, &o);
      }
      if (want) {
        Guard(g_page.load());
        BD_INFO("[piege292] piege pose sur {:08X}", watch);
      } else {
        BD_INFO("[piege292] piege retire");
      }
    }
  }
}

}  // namespace

void Wwe12PinTrapStart() {
  static std::atomic<bool> started{false};
  if (!REXCVAR_GET(wwe12_pin_trap) || started.exchange(true))
    return;
  g_exe = u64(GetModuleHandleW(nullptr));
  AddVectoredExceptionHandler(1, &Handler);
  std::thread(Poll).detach();
  BD_INFO("[piege292] sonde active");
}
