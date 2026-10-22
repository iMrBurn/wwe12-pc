// WWE '12 PC: blood for female wrestlers (work in progress).
//
// Found so far (2026-10-21/22):
//   - The BLOOD option is byte 69 of the settings block sub_82684BE8() + 0x57720 =
//     0x8322660D (1 = on), written by UpdateInGameOptions (sub_82DEC528).
//   - At match set-up, sub_82783808(manager) copies it into the match's rules entry, a 52-byte
//     record from sub_827829B0(manager) ([manager+556] + (type * 10 + sub) * 52): +32 = 1 means
//     "no blood" (BLOOD off and [manager+647] does not force it).
//
// In a match (First Blood, 2026-10-22) the option is NOT read: only the MATCH CREATOR reads it
// (sub_825910F8, sub_82591750 grey out First Blood when BLOOD is off); the match uses a copy.
//
// Probe (wwe12_blood_trap): the BLOOD option's word gets a page-guard trap; every host
// instruction that reads or writes it is logged once
// ("wwe12.exe+offset", symbolise with llvm-symbolizer and wwe12.pdb), to find the code that
// decides bleeding, and next to it the check that keeps female wrestlers from bleeding.

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include <windows.h>

#include <fmt/format.h>
#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"


namespace {

constexpr u32 kBloodOption = 0x831CEEA8u + 0x57720u + 69u;  // 0x8322660D, 1 = BLOOD on

REXCVAR_DEFINE_BOOL(wwe12_blood_trap, false, "WWE12",
                    "Probe: log the game code that reads the match's no-blood flag (page guard).");

constexpr int kMaxEvents = 128;
struct Event {
  u64 rip_off;
  u32 guest;  // first address seen for this instruction
  bool write;
  std::atomic<u32> count;
};
Event g_events[kMaxEvents];
std::atomic<int> g_used{0};
int g_logged = 0;
std::atomic_flag g_lock = ATOMIC_FLAG_INIT;

std::atomic<u32> g_watch{0};       // guest address of the watched word (0 = none)
std::atomic<u64> g_page{0};        // its host page
std::atomic<u64> g_guest_base{0};  // host address of guest 0
u64 g_exe_base = 0;
thread_local u64 t_rearm = 0;

void Guard(u64 page) {
  DWORD old = 0;
  VirtualProtect(reinterpret_cast<void*>(page), 0x1000, PAGE_READWRITE | PAGE_GUARD, &old);
}

void Record(u64 rip, u32 guest, bool write) {
  const u64 off = rip - g_exe_base;
  while (g_lock.test_and_set(std::memory_order_acquire)) {
  }
  const int used = g_used.load();
  int i = 0;
  for (; i < used; ++i) {
    if (g_events[i].rip_off == off && g_events[i].write == write) break;
  }
  if (i == used && used < kMaxEvents) {
    g_events[i].rip_off = off;
    g_events[i].guest = guest;
    g_events[i].write = write;
    g_events[i].count = 0;
    g_used = used + 1;
  }
  if (i < kMaxEvents) g_events[i].count.fetch_add(1);
  g_lock.clear(std::memory_order_release);
}

LONG CALLBACK Handler(EXCEPTION_POINTERS* ep) {
  const DWORD code = ep->ExceptionRecord->ExceptionCode;
  if (code == STATUS_SINGLE_STEP) {
    if (!t_rearm) return EXCEPTION_CONTINUE_SEARCH;
    if (g_page.load() == t_rearm) Guard(t_rearm);
    t_rearm = 0;
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  if (code != STATUS_GUARD_PAGE_VIOLATION || ep->ExceptionRecord->NumberParameters < 2) {
    return EXCEPTION_CONTINUE_SEARCH;
  }
  const u64 addr = u64(ep->ExceptionRecord->ExceptionInformation[1]);
  const u64 page = addr & ~u64(0xFFF);
  if (page != g_page.load()) return EXCEPTION_CONTINUE_SEARCH;
  const u32 guest = u32(addr - g_guest_base.load());
  const u32 watch = g_watch.load();
  // Byte reads hit the byte itself; block copies (memcpy) start up to 15 bytes before it.
  if (guest <= watch && watch - guest < 16) {
    Record(ep->ContextRecord->Rip, guest, ep->ExceptionRecord->ExceptionInformation[0] == 1);
  }
  t_rearm = page;
  ep->ContextRecord->EFlags |= 0x100;  // trap flag: guard again right after this access
  return EXCEPTION_CONTINUE_EXECUTION;
}

void FlushEvents() {
  const int used = g_used.load();
  for (; g_logged < used; ++g_logged) {
    const auto& e = g_events[g_logged];
    BD_INFO("[sang] {} par wwe12.exe+{:X} (a {:08X})", e.write ? "ecriture" : "lecture", e.rip_off,
            e.guest);
  }
}

void Watch(u32 guest) {
  static bool installed = false;
  if (!installed) {
    installed = true;
    g_exe_base = u64(GetModuleHandleW(nullptr));
    AddVectoredExceptionHandler(1, &Handler);
    std::thread([] {  // log from a host thread (the readers run on game threads)
      for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        FlushEvents();
      }
    }).detach();
  }
  auto* host = bd::mem::try_at<u8>(guest);
  if (!host) return;
  g_guest_base = u64(host) - guest;
  const u64 page = u64(host) & ~u64(0xFFF);
  g_watch = guest;
  g_page = page;
  Guard(page);
  BD_INFO("[sang] piege sur {:08X} (valeur {})", guest, bd::mem::try_field<u8>(guest, 0));
}

}  // namespace

// The trap is armed on the BLOOD option byte itself as soon as guest memory exists: every reader
// of the option (match set-up of every mode, and anything in the match) shows up.
namespace {
struct TrapStarter {
  TrapStarter() {
    std::thread([] {
      while (!bd::mem::ready()) std::this_thread::sleep_for(std::chrono::milliseconds(100));
      std::this_thread::sleep_for(std::chrono::seconds(2));
      if (REXCVAR_GET(wwe12_blood_trap)) Watch(kBloodOption);
    }).detach();
  }
} g_trap_starter;
}  // namespace
