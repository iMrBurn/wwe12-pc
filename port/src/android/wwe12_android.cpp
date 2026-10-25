// Version Android : remplaçants des morceaux propres à Windows retirés par cmake/android.cmake.

// crash_trace.cpp (traces de plantage et chien de garde des gels, dbghelp) : pas encore sur
// Android. Le rendu l'appelle à chaque image présentée.
void SvrWatchdogHeartbeat() {}

// compat/windows.h : VirtualQuery d'après /proc/self/maps. wwe12_quit.cpp s'en sert pour
// parcourir la mémoire du jeu (régions lisibles et inscriptibles) et y renommer des textes.
#include <cstdio>
#include <cstring>

#include "compat/windows.h"

SIZE_T VirtualQuery(LPCVOID address, MEMORY_BASIC_INFORMATION *info, SIZE_T length) {
  if (!info || length < sizeof(*info)) return 0;
  const uintptr_t addr = reinterpret_cast<uintptr_t>(address);
  FILE *maps = std::fopen("/proc/self/maps", "r");
  if (!maps) return 0;
  uintptr_t next_start = UINTPTR_MAX, prev_end = 0;
  bool found = false;
  char line[512];
  while (std::fgets(line, sizeof(line), maps)) {
    unsigned long long lo = 0, hi = 0;
    char perms[5] = {};
    if (std::sscanf(line, "%llx-%llx %4s", &lo, &hi, perms) != 3) continue;
    if (addr >= lo && addr < hi) {
      DWORD protect = PAGE_NOACCESS;
      const bool r = perms[0] == 'r', w = perms[1] == 'w', x = perms[2] == 'x';
      if (x) protect = w ? PAGE_EXECUTE_READWRITE : (r ? PAGE_EXECUTE_READ : PAGE_EXECUTE);
      else if (r) protect = w ? PAGE_READWRITE : PAGE_READONLY;
      *info = {};
      info->BaseAddress = reinterpret_cast<void *>(addr & ~uintptr_t(0xFFF));
      info->AllocationBase = reinterpret_cast<void *>(uintptr_t(lo));
      info->RegionSize = SIZE_T(hi) - reinterpret_cast<uintptr_t>(info->BaseAddress);
      info->State = MEM_COMMIT;
      info->Protect = protect;
      found = true;
      break;
    }
    if (lo > addr) {
      next_start = lo;
      break;
    }
    prev_end = hi;
  }
  std::fclose(maps);
  if (!found) {  // trou entre deux régions : libre jusqu'à la suivante
    *info = {};
    const uintptr_t base = addr & ~uintptr_t(0xFFF);
    info->BaseAddress = reinterpret_cast<void *>(base);
    info->RegionSize = (next_start == UINTPTR_MAX ? base + 0x10000 : next_start) - base;
    info->State = MEM_FREE;
    info->Protect = PAGE_NOACCESS;
    (void)prev_end;
  }
  return sizeof(*info);
}

// Android : stderr ne va nulle part. plume y écrit la cause de ses échecs (codes VkResult de
// vkCreateSwapchainKHR, des pipelines...) : on la renvoie dans le journal du jeu, que les testeurs
// partagent (SHARE LAST LOG). Appelé au démarrage, une fois le journal prêt (wwe12_app.h).
#include <unistd.h>

#include <string>
#include <thread>

#include <rex/logging/macros.h>

#include <chrono>

extern "C" void plume_save_pipeline_cache();  // plume-android-cache.patch

void Wwe12AndroidCaptureStderr() {
  int fds[2];
  if (pipe(fds) != 0) return;
  std::fflush(stderr);
  if (dup2(fds[1], STDERR_FILENO) < 0) {
    close(fds[0]);
    close(fds[1]);
    return;
  }
  close(fds[1]);
  std::setvbuf(stderr, nullptr, _IOLBF, 0);
  std::thread([fd = fds[0]] {
    std::string line;
    char buf[512];
    int lines = 0;  // a chatty driver must not flood the log: 500 lines, then silence
    for (ssize_t n; (n = read(fd, buf, sizeof(buf))) > 0;) {
      for (ssize_t i = 0; i < n; ++i) {
        if (buf[i] == '\n') {
          if (!line.empty() && lines < 500 && ++lines)
            REXLOG_WARN("[stderr] {}{}", line, lines == 500 ? " (stderr: no more lines logged)" : "");
          line.clear();
        } else if (line.size() < 1000) {
          line += buf[i];
        }
      }
    }
  }).detach();
  std::fprintf(stderr, "stderr goes to this log\n");

  // The Vulkan pipeline cache (plume-android-cache.patch) is written every 10 s if it grew: the
  // game ends with a hard exit, never a clean shutdown.
  std::thread([] {
    for (;;) {
      std::this_thread::sleep_for(std::chrono::seconds(10));
      plume_save_pipeline_cache();
    }
  }).detach();
}
