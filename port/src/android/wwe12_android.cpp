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
