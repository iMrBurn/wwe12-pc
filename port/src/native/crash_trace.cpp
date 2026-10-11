// Native renderer dev aid: host stacks of a crash the runtime doesn't handle, and of a hang.
//
// The runtime's own exception handler (installed first) resolves MMIO and memory-watch faults; an
// access violation it passes on reaches this handler, which walks the host stack and writes each
// frame as an offset into svr2010.exe to logs/crash_trace.txt. A watchdog thread writes every
// thread's stack to logs/hang_trace.txt when no frame has been presented for 10 s (the renderer
// calls SvrWatchdogHeartbeat per frame). The frames can be named from the linker map
// (out/build/<preset>/svr2010.map).

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <tlhelp32.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>

namespace {

std::atomic<bool> g_traced{false};

constexpr DWORD64 kGuestBase = 0x100000000ull;

bool ReadHost(DWORD64 address, void *out, size_t size) {
  SIZE_T done = 0;
  return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<LPCVOID>(address), out, size,
                           &done) &&
         done == size;
}

unsigned long Swap32(unsigned long v) { return _byteswap_ulong(v); }

// Recompiled code keeps the PPCContext pointer in a callee-saved register; a register pointing at
// something whose r1 (+0x10) is a guest stack address is taken to be it. Layout (rex/ppc/context.h):
// r3, r0, r1, r2, then r4..r31 at N*8, lr at 0x100, ctr at 0x108.
void DumpGuestContext(FILE *f, const CONTEXT *ctx) {
  const DWORD64 candidates[] = {ctx->Rdi, ctx->Rbx, ctx->Rsi, ctx->Rbp, ctx->R12,
                                ctx->R13, ctx->R14, ctx->R15, ctx->Rcx};
  for (DWORD64 p : candidates) {
    DWORD64 regs[34];
    if (!ReadHost(p, regs, sizeof(regs)))
      continue;
    const DWORD64 r1 = regs[2];
    if (r1 < 0x40000000ull || r1 >= 0x80000000ull)
      continue;
    DWORD64 gpr[32];
    gpr[3] = regs[0];
    gpr[0] = regs[1];
    gpr[1] = regs[2];
    gpr[2] = regs[3];
    for (int n = 4; n < 32; ++n)
      gpr[n] = regs[n];
    fprintf(f, "guest ctx at %016llX\n", static_cast<unsigned long long>(p));
    for (int n = 0; n < 32; ++n)
      fprintf(f, "r%-2d=%08llX%s", n, static_cast<unsigned long long>(gpr[n] & 0xFFFFFFFFull),
              (n % 8 == 7) ? "\n" : " ");
    fprintf(f, "lr=%08llX ctr=%08llX\n", static_cast<unsigned long long>(regs[32] & 0xFFFFFFFFull),
            static_cast<unsigned long long>(regs[33] & 0xFFFFFFFFull));
    // Guest stack, big-endian dwords from r1 (the frames of the callers' saved registers).
    unsigned long stack[0x600 / 4];
    if (ReadHost(kGuestBase + r1, stack, sizeof(stack))) {
      fprintf(f, "guest stack at %08llX\n", static_cast<unsigned long long>(r1));
      for (size_t i = 0; i < _countof(stack); ++i) {
        if (i % 8 == 0)
          fprintf(f, "  +%03zX:", i * 4);
        fprintf(f, " %08lX%s", Swap32(stack[i]), (i % 8 == 7) ? "\n" : "");
      }
    }
    return;
  }
}

// Documents\wwe12\logs\<name>, the folder created if needed: where players' traces land too.
void LogPath(char *path, size_t size, const char *name) {
  const char *home = getenv("USERPROFILE");
  char dir[MAX_PATH * 2];
  snprintf(dir, sizeof(dir), "%s\\Documents\\wwe12", home ? home : ".");
  CreateDirectoryA(dir, nullptr);
  strncat(dir, "\\logs", sizeof(dir) - strlen(dir) - 1);
  CreateDirectoryA(dir, nullptr);
  snprintf(path, size, "%s\\%s", dir, name);
}

LONG CALLBACK CrashTraceHandler(PEXCEPTION_POINTERS info) {
  const DWORD code = info->ExceptionRecord->ExceptionCode;
  if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_ILLEGAL_INSTRUCTION &&
      code != EXCEPTION_INT_DIVIDE_BY_ZERO && code != EXCEPTION_STACK_OVERFLOW)
    return EXCEPTION_CONTINUE_SEARCH;
  // Faults inside the guest address space (host 0x1'00000000 + guest address), and any fault in
  // svr2010.exe's own code (drivers may take and handle access violations of their own).
  if (code == EXCEPTION_ACCESS_VIOLATION) {
    const auto addr = info->ExceptionRecord->ExceptionInformation[1];
    const bool guest = addr >= 0x100000000ull && addr < 0x200000000ull;
    HMODULE exe = GetModuleHandleW(nullptr);
    HMODULE at = nullptr;
    const bool in_exe =
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(info->ContextRecord->Rip), &at) &&
        at == exe;
    if (!guest && !in_exe)
      return EXCEPTION_CONTINUE_SEARCH;
  }
  if (g_traced.exchange(true))
    return EXCEPTION_CONTINUE_SEARCH;

  char path[MAX_PATH * 2];
  LogPath(path, sizeof(path), "crash_trace.txt");
  FILE *f = fopen(path, "w");
  if (!f)
    return EXCEPTION_CONTINUE_SEARCH;

  const auto base = reinterpret_cast<DWORD64>(GetModuleHandleW(nullptr));
  fprintf(f, "code %08lX thread %lu\n", code, GetCurrentThreadId());
  if (code == EXCEPTION_ACCESS_VIOLATION)
    fprintf(f, "%s 0x%016llX\n", info->ExceptionRecord->ExceptionInformation[0] ? "write" : "read",
            static_cast<unsigned long long>(info->ExceptionRecord->ExceptionInformation[1]));
  CONTEXT ctx = *info->ContextRecord;
  const DWORD64 *regs = &ctx.Rax;
  static const char *kNames[16] = {"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
                                   "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15"};
  for (int i = 0; i < 16; ++i)
    fprintf(f, "%s=%016llX%s", kNames[i], static_cast<unsigned long long>(regs[i]),
            (i % 4 == 3) ? "\n" : " ");
  fprintf(f, "base %016llX\n", static_cast<unsigned long long>(base));
  DumpGuestContext(f, info->ContextRecord);

  for (int frame = 0; frame < 48 && ctx.Rip; ++frame) {
    DWORD64 image_base = 0;
    PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(ctx.Rip, &image_base, nullptr);
    char module[MAX_PATH] = "?";
    HMODULE mod = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(ctx.Rip), &mod)) {
      GetModuleFileNameA(mod, module, MAX_PATH);
    }
    const char *name = strrchr(module, '\\');
    fprintf(f, "#%d %s+0x%llX\n", frame, name ? name + 1 : module,
            static_cast<unsigned long long>(ctx.Rip - reinterpret_cast<DWORD64>(mod)));
    if (!fn) {
      // Leaf function: the return address is at [rsp].
      ctx.Rip = *reinterpret_cast<DWORD64 *>(ctx.Rsp);
      ctx.Rsp += 8;
      continue;
    }
    void *handler_data = nullptr;
    DWORD64 establisher = 0;
    RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, ctx.Rip, fn, &ctx, &handler_data, &establisher,
                     nullptr);
  }
  fclose(f);
  return EXCEPTION_CONTINUE_SEARCH;
}

FILE *OpenLog(const char *name) {
  char path[MAX_PATH * 2];
  LogPath(path, sizeof(path), name);
  return fopen(path, "w");
}

// Return addresses of a (suspended) thread's stack. Memory is read with ReadProcessMemory, so a
// damaged stack ends the walk instead of faulting.
int WalkStack(CONTEXT ctx, DWORD64 *rips, int max) {
  int count = 0;
  while (count < max && ctx.Rip) {
    rips[count++] = ctx.Rip;
    DWORD64 image_base = 0;
    PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(ctx.Rip, &image_base, nullptr);
    if (!fn) {
      DWORD64 ret = 0;
      if (!ReadHost(ctx.Rsp, &ret, sizeof(ret)))
        break;
      ctx.Rip = ret;
      ctx.Rsp += 8;
      continue;
    }
    void *handler_data = nullptr;
    DWORD64 establisher = 0;
    RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, ctx.Rip, fn, &ctx, &handler_data, &establisher,
                     nullptr);
  }
  return count;
}

void PrintFrames(FILE *f, const DWORD64 *rips, int count) {
  for (int i = 0; i < count; ++i) {
    char module[MAX_PATH] = "?";
    HMODULE mod = nullptr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                               GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCSTR>(rips[i]), &mod)) {
      GetModuleFileNameA(mod, module, MAX_PATH);
    }
    const char *name = strrchr(module, '\\');
    fprintf(f, "#%d %s+0x%llX\n", i, name ? name + 1 : module,
            static_cast<unsigned long long>(rips[i] - reinterpret_cast<DWORD64>(mod)));
  }
}

// Every other thread is suspended only while its context and return addresses are captured
// (nothing that allocates or takes the loader lock runs meanwhile), then named and printed.
void DumpAllThreads() {
  FILE *f = OpenLog("hang_trace.txt");
  if (!f)
    return;
  fprintf(f, "no frame presented for 10 s; stacks of all threads\n");
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snap == INVALID_HANDLE_VALUE) {
    fclose(f);
    return;
  }
  const DWORD self_pid = GetCurrentProcessId();
  const DWORD self_tid = GetCurrentThreadId();
  THREADENTRY32 te{};
  te.dwSize = sizeof(te);
  for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
    if (te.th32OwnerProcessID != self_pid || te.th32ThreadID == self_tid)
      continue;
    HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT |
                              THREAD_QUERY_LIMITED_INFORMATION,
                          FALSE, te.th32ThreadID);
    if (!h)
      continue;
    DWORD64 rips[40];
    int count = 0;
    CONTEXT ctx{};
    ctx.ContextFlags = CONTEXT_FULL;
    if (SuspendThread(h) != DWORD(-1)) {
      if (GetThreadContext(h, &ctx))
        count = WalkStack(ctx, rips, 40);
      ResumeThread(h);
    }
    char name[128] = "";
    PWSTR wname = nullptr;
    if (SUCCEEDED(GetThreadDescription(h, &wname)) && wname) {
      WideCharToMultiByte(CP_UTF8, 0, wname, -1, name, sizeof(name), nullptr, nullptr);
      LocalFree(wname);
    }
    fprintf(f, "\nthread %lu '%s'\n", te.th32ThreadID, name);
    PrintFrames(f, rips, count);
    if (count)
      DumpGuestContext(f, &ctx);
    CloseHandle(h);
  }
  CloseHandle(snap);
  fclose(f);
}

// Developer profiler: SVR_SAMPLE_STACKS=<ms> records every thread's return addresses at that
// interval to logs/samples.txt, one line per thread per sample:
//   <ms since start> <tid> <thread name> module+0xOFF;module+0xOFF;...   (innermost first)
// Naming the frames from the linker map shows where each thread spends its time (e.g. loading).
DWORD WINAPI SamplerThread(LPVOID param) {
  const DWORD interval = static_cast<DWORD>(reinterpret_cast<uintptr_t>(param));
  FILE *f = OpenLog("samples.txt");
  if (!f)
    return 0;
  const DWORD self_pid = GetCurrentProcessId();
  const DWORD self_tid = GetCurrentThreadId();
  const ULONGLONG start = GetTickCount64();
  for (;;) {
    Sleep(interval);
    const ULONGLONG now = GetTickCount64() - start;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE)
      continue;
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
      if (te.th32OwnerProcessID != self_pid || te.th32ThreadID == self_tid)
        continue;
      HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_LIMITED_INFORMATION,
                            FALSE, te.th32ThreadID);
      if (!h)
        continue;
      DWORD64 rips[24];
      int count = 0;
      CONTEXT ctx{};
      ctx.ContextFlags = CONTEXT_FULL;
      if (SuspendThread(h) != DWORD(-1)) {
        if (GetThreadContext(h, &ctx))
          count = WalkStack(ctx, rips, 24);
        ResumeThread(h);
      }
      char name[64] = "-";
      PWSTR wname = nullptr;
      if (SUCCEEDED(GetThreadDescription(h, &wname)) && wname) {
        if (*wname)
          WideCharToMultiByte(CP_UTF8, 0, wname, -1, name, sizeof(name), nullptr, nullptr);
        LocalFree(wname);
      }
      for (char *c = name; *c; ++c)
        if (*c == ' ' || *c == '\t')
          *c = '_';
      fprintf(f, "%llu %lu %s ", static_cast<unsigned long long>(now), te.th32ThreadID, name);
      for (int i = 0; i < count; ++i) {
        char module[MAX_PATH] = "?";
        HMODULE mod = nullptr;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                   GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCSTR>(rips[i]), &mod))
          GetModuleFileNameA(mod, module, MAX_PATH);
        const char *base = strrchr(module, '\\');
        fprintf(f, "%s%s+0x%llX", i ? ";" : "", base ? base + 1 : module,
                static_cast<unsigned long long>(rips[i] - reinterpret_cast<DWORD64>(mod)));
      }
      fputc('\n', f);
      CloseHandle(h);
    }
    CloseHandle(snap);
    fflush(f);
  }
}

std::atomic<ULONGLONG> g_heartbeat_ms{0};  // last presented frame, 0 = none yet

DWORD WINAPI WatchdogThread(LPVOID) {
  for (;;) {
    Sleep(1000);
    const ULONGLONG last = g_heartbeat_ms.load(std::memory_order_relaxed);
    if (last && GetTickCount64() - last > 10000) {
      DumpAllThreads();
      return 0;
    }
  }
}

// Appended after the runtime's handler (which is added first in the chain), so only faults the
// runtime didn't resolve get here.
struct CrashTraceInstaller {
  CrashTraceInstaller() {
    // The game is a windowed app with no console: plume reports Vulkan failures (vkQueueSubmit,
    // vkWaitForFences, ...) on stderr, so give stderr a file.
    char path[MAX_PATH * 2];
    LogPath(path, sizeof(path), "stderr.txt");
    if (freopen(path, "w", stderr))
      setvbuf(stderr, nullptr, _IONBF, 0);
    AddVectoredExceptionHandler(0, CrashTraceHandler);
    if (HANDLE h = CreateThread(nullptr, 0, WatchdogThread, nullptr, 0, nullptr)) {
      SetThreadDescription(h, L"SvR hang watchdog");
      CloseHandle(h);
    }
    if (const char *ms = getenv("SVR_SAMPLE_STACKS"); ms && atoi(ms) > 0) {
      if (HANDLE h = CreateThread(nullptr, 0, SamplerThread,
                                  reinterpret_cast<LPVOID>(static_cast<uintptr_t>(atoi(ms))), 0, nullptr)) {
        SetThreadDescription(h, L"SvR stack sampler");
        CloseHandle(h);
      }
    }
  }
} g_installer;

}  // namespace

void SvrWatchdogHeartbeat() { g_heartbeat_ms.store(GetTickCount64(), std::memory_order_relaxed); }

#else

void SvrWatchdogHeartbeat() {}

#endif  // _WIN32
