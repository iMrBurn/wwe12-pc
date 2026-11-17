// Dev probe for the second pin system (1.4, session Gameplay): who writes the referee's count of a pin.
//
// Found with the pin probe (2026-11-14): [actor+2760] is the count of a pin on that superstar (0, 1, 2, 3) and it
// goes back to 0 the moment he kicks out; [actor+2464] is the pin's time left. This probe (wwe12_pin_trap, off) puts
// hardware data breakpoints (debug registers DR0 / DR1, writes only, 4 bytes) on [actor+2760] of the superstars 0
// and 1, on every thread of the process, and logs each distinct host instruction that writes it with the value
// written and its host call chain ("wwe12.exe+X", translate with wwe12.map): the kick-out's code writes the 0.
// (A first version used a PAGE_GUARD trap on the COPE component: the game crashed. Debug registers only stop on
// that one word.)

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(wwe12_pin_trap, false, "WWE12",
                    "Dev: log the code that writes a pin's count (hardware breakpoints on two words).");
REXCVAR_DEFINE_INT32(wwe12_watch_addr, 0, "WWE12",
                     "Dev: a guest address (as a decimal number) whose 4-byte writes are logged with their code (DR2).");
REXCVAR_DEFINE_BOOL(wwe12_watch_select, false, "WWE12",
                    "Dev: DR2 on the character select's place 0 state ([S+268], 2 = attire panel): who writes it.");
uint32_t Wwe12SelectScene(int place);  // wwe12_match_modes.cpp

#if defined(__ANDROID__) || defined(WWE12_IOS)
void Wwe12PinTrapStart() {}  // (dev, Windows only)
#else

#include <windows.h>
#include <tlhelp32.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include <fmt/format.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

void Wwe12PinTrapStart();

namespace {

constexpr u32 kActors = 0x8317B770;
constexpr int kMax = 128;

u64 g_exe = 0;
std::atomic<u64> g_host[3];  // host addresses watched by DR0 / DR1 / DR2
std::atomic<int> g_n{0};
u64 g_rip[kMax];
u32 g_value[kMax];
int g_slot[kMax];
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

LONG CALLBACK Handler(EXCEPTION_POINTERS *ep) {
  if (ep->ExceptionRecord->ExceptionCode != STATUS_SINGLE_STEP)
    return EXCEPTION_CONTINUE_SEARCH;
  CONTEXT *c = ep->ContextRecord;
  const u64 hit = c->Dr6 & 7;
  if (!hit)
    return EXCEPTION_CONTINUE_SEARCH;
  const int slot = (hit & 1) ? 0 : (hit & 2) ? 1 : 2;
  const u64 h = g_host[slot].load();
  const u64 rip = c->Rip - g_exe;
  const u32 value = h ? _byteswap_ulong(*reinterpret_cast<volatile u32 *>(h)) : 0;
  bool seen = false;
  const int n = std::min(g_n.load(), kMax);
  for (int i = 0; i < n && !seen; ++i)
    seen = slot != 2 && g_rip[i] == rip && g_value[i] == value;  // (DR2: every write, in order)
  if (!seen) {
    const int i = g_n.fetch_add(1);
    if (i < kMax) {
      g_rip[i] = rip;
      g_value[i] = value;
      g_slot[i] = slot;
      Chain(c, g_chain[i]);
    }
  }
  c->Dr6 = 0;
  return EXCEPTION_CONTINUE_EXECUTION;
}

// Debug registers of every thread but this one: DR0 / DR1 = the two words (0 = off), writes, 4 bytes.
void SetAll(u64 a0, u64 a1, u64 a2) {
  const DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snap == INVALID_HANDLE_VALUE)
    return;
  THREADENTRY32 te{};
  te.dwSize = sizeof te;
  for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
    if (te.th32OwnerProcessID != pid || te.th32ThreadID == self)
      continue;
    HANDLE t = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
    if (!t)
      continue;
    if (SuspendThread(t) != DWORD(-1)) {
      CONTEXT c{};
      c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
      if (GetThreadContext(t, &c)) {
        c.Dr0 = a0;
        c.Dr1 = a1;
        c.Dr2 = a2;
        u64 dr7 = c.Dr7 & ~u64(0xFFF0003F);
        if (a0)
          dr7 |= 1 | (u64(0b01) << 16) | (u64(0b11) << 18);  // L0, write, 4 bytes
        if (a1)
          dr7 |= 4 | (u64(0b01) << 20) | (u64(0b11) << 22);  // L1, write, 4 bytes
        if (a2)
          dr7 |= 16 | (u64(0b01) << 24) | (u64(0b11) << 26);  // L2, write, 4 bytes
        c.Dr7 = dr7;
        SetThreadContext(t, &c);
      }
      ResumeThread(t);
    }
    CloseHandle(t);
  }
  CloseHandle(snap);
}

void Poll() {
  int flushed = 0;
  u64 set[3] = {};
  auto t_last = std::chrono::steady_clock::now();
  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const int n = std::min(g_n.load(), kMax);
    for (; flushed < n; ++flushed) {
      std::string chain;
      for (u64 o : g_chain[flushed])
        if (o)
          chain += fmt::format(" {:X}", o);
      BD_INFO("[compte] {} : valeur {} ecrite par wwe12.exe+{:X} pile:{}",
              g_slot[flushed] == 2 ? std::string("adresse surveillee") : fmt::format("compte du catcheur {}", g_slot[flushed]),
              g_value[flushed], g_rip[flushed], chain);
    }
    u64 want[3] = {};
    for (u32 i = 0; i < 2; ++i) {
      const u32 actor = bd::mem::try_load<u32>(kActors + i * 4);
      u8 *host = actor ? bd::mem::try_at<u8>(actor + 2760) : nullptr;
      want[i] = host ? u64(host) : 0;
    }
    if (REXCVAR_GET(wwe12_watch_select)) {
      const u32 sc = Wwe12SelectScene(0);
      u8 *host = sc ? bd::mem::try_at<u8>(sc + 268) : nullptr;
      want[2] = host ? u64(host) : 0;
    } else if (const int32_t w = REXCVAR_GET(wwe12_watch_addr)) {
      u8 *host = bd::mem::try_at<u8>(u32(w));
      want[2] = host ? u64(host) : 0;
    }
    // (again every 2 s too: threads created since then)
    const auto now = std::chrono::steady_clock::now();
    if (want[0] != set[0] || want[1] != set[1] || want[2] != set[2] || now - t_last > std::chrono::seconds(2)) {
      if (want[0] != set[0] || want[1] != set[1] || want[2] != set[2])
        BD_INFO("[compte] points d'arret sur {:X} / {:X} / {:X}", want[0], want[1], want[2]);
      for (int k = 0; k < 3; ++k) {
        g_host[k] = want[k];
        set[k] = want[k];
      }
      SetAll(want[0], want[1], want[2]);
      t_last = now;
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
  BD_INFO("[compte] sonde active");
}

#endif  // !__ANDROID__ && !WWE12_IOS
