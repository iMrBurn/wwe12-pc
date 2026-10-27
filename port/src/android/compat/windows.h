// Version Android : petit remplaçant de <windows.h> pour les fichiers du portage qui
// l'incluent (cmake/android.cmake le place devant les autres chemins). Il imite seulement les
// quelques fonctions qu'ils appellent ; ce qui n'a pas de sens sur Android ne fait rien et
// renvoie « échec », comme si Windows avait refusé. Les fichiers PC ne sont pas modifiés.
#pragma once

#ifndef __ANDROID__
#error "compat/windows.h is only for the Android build"
#endif

#include <sched.h>

#include <cstddef>
#include <cstdint>

// ---- Types ---------------------------------------------------------------------------------
using BYTE = uint8_t;
using WORD = uint16_t;
using DWORD = uint32_t;
using DWORD64 = uint64_t;
using BOOL = int;
using LONG = int32_t;
using UINT = unsigned int;
using ULONG_PTR = uintptr_t;
using SIZE_T = size_t;
using HANDLE = void *;
using HWND = void *;
using HMODULE = void *;
using LPARAM = intptr_t;
using WPARAM = uintptr_t;
using LPVOID = void *;
using LPCVOID = const void *;
using PDWORD = DWORD *;
union LARGE_INTEGER {
  struct {
    DWORD LowPart;
    LONG HighPart;
  };
  int64_t QuadPart;
};

#define WINAPI
#define CALLBACK
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#define INFINITE 0xFFFFFFFFu

// ---- Threads and process -------------------------------------------------------------------
#define THREAD_PRIORITY_LOWEST (-2)
#define THREAD_PRIORITY_BELOW_NORMAL (-1)
#define THREAD_PRIORITY_NORMAL 0
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#define ABOVE_NORMAL_PRIORITY_CLASS 0x8000u
#define HIGH_PRIORITY_CLASS 0x80u
#define NORMAL_PRIORITY_CLASS 0x20u

inline HANDLE GetCurrentThread() { return nullptr; }
inline HANDLE GetCurrentProcess() { return nullptr; }
inline DWORD GetCurrentProcessId() { return 0; }
inline BOOL SetThreadPriority(HANDLE, int) { return TRUE; }  // Android gère seul les priorités
inline BOOL SetPriorityClass(HANDLE, DWORD) { return FALSE; }
inline void YieldProcessor() {
#if defined(__x86_64__)
  __builtin_ia32_pause();
#elif defined(__aarch64__)
  asm volatile("yield");
#else
  sched_yield();
#endif
}
inline HMODULE GetModuleHandleW(const wchar_t *) { return nullptr; }

// ---- Timers (limite d'images : sans minuteur, wwe12_display.cpp dort avec sleep_for) --------
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x2u
#define TIMER_ALL_ACCESS 0x1F0003u
inline HANDLE CreateWaitableTimerExW(void *, const wchar_t *, DWORD, DWORD) { return nullptr; }
inline BOOL SetWaitableTimer(HANDLE, const LARGE_INTEGER *, LONG, void *, void *, BOOL) {
  return FALSE;
}
inline DWORD WaitForSingleObject(HANDLE, DWORD) { return 0; }

// ---- Keyboard ------------------------------------------------------------------------------
#define VK_F9 0x78
inline short GetAsyncKeyState(int) { return 0; }

// ---- Memory --------------------------------------------------------------------------------
#define PAGE_NOACCESS 0x01u
#define PAGE_READONLY 0x02u
#define PAGE_READWRITE 0x04u
#define PAGE_EXECUTE 0x10u
#define PAGE_EXECUTE_READ 0x20u
#define PAGE_EXECUTE_READWRITE 0x40u
#define PAGE_GUARD 0x100u
#define MEM_COMMIT 0x1000u
#define MEM_RESERVE 0x2000u
#define MEM_FREE 0x10000u
struct MEMORY_BASIC_INFORMATION {
  void *BaseAddress;
  void *AllocationBase;
  DWORD AllocationProtect;
  SIZE_T RegionSize;
  DWORD State;
  DWORD Protect;
  DWORD Type;
};
// Lit /proc/self/maps (src/android/wwe12_android.cpp).
SIZE_T VirtualQuery(LPCVOID address, MEMORY_BASIC_INFORMATION *info, SIZE_T length);
inline BOOL VirtualProtect(void *, SIZE_T, DWORD, DWORD *) { return FALSE; }

// ---- Exceptions (pièges de test « PAGE_GUARD » : jamais installés sur Android) --------------
#define STATUS_SINGLE_STEP 0x80000004u
#define STATUS_GUARD_PAGE_VIOLATION 0x80000001u
#define EXCEPTION_CONTINUE_EXECUTION (-1)
#define EXCEPTION_CONTINUE_SEARCH 0
struct EXCEPTION_RECORD {
  DWORD ExceptionCode;
  DWORD ExceptionFlags;
  EXCEPTION_RECORD *ExceptionRecord;
  void *ExceptionAddress;
  DWORD NumberParameters;
  ULONG_PTR ExceptionInformation[15];
};
struct CONTEXT {
  DWORD64 Rip;
  DWORD EFlags;
};
struct EXCEPTION_POINTERS {
  EXCEPTION_RECORD *ExceptionRecord;
  CONTEXT *ContextRecord;
};
using PVECTORED_EXCEPTION_HANDLER = LONG (*)(EXCEPTION_POINTERS *);
inline void *AddVectoredExceptionHandler(UINT, PVECTORED_EXCEPTION_HANDLER) { return nullptr; }

// ---- Windows (fenêtres) : pas de fenêtres Win32 sur Android ---------------------------------
#define WM_CLOSE 0x0010u
#define GW_OWNER 4
using WNDENUMPROC = BOOL (*)(HWND, LPARAM);
inline BOOL EnumWindows(WNDENUMPROC, LPARAM) { return TRUE; }
inline DWORD GetWindowThreadProcessId(HWND, DWORD *pid) {
  if (pid) *pid = 0;
  return 0;
}
inline BOOL IsWindowVisible(HWND) { return FALSE; }
inline HWND GetWindow(HWND, UINT) { return nullptr; }
inline BOOL PostMessageW(HWND, UINT, WPARAM, LPARAM) { return FALSE; }

// ---- Divers (MSVC / Win32) -----------------------------------------------------------------
#include <ctime>
inline unsigned short _byteswap_ushort(unsigned short v) { return __builtin_bswap16(v); }
inline unsigned long _byteswap_ulong(unsigned long v) {
  return __builtin_bswap32(static_cast<uint32_t>(v));
}
inline unsigned long long _byteswap_uint64(unsigned long long v) { return __builtin_bswap64(v); }
#include "wwe12_crt_compat.h"  // localtime_s

// Taille de l'écran (liste des tailles de fenêtre du menu GRAPHICS) : sans objet sur Android.
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
inline int GetSystemMetrics(int) { return 0; }

// Ressources de l'exe (.rc) : pas de ressources Win32 dans une bibliothèque Android, donc
// « absente » (les icônes PlayStation, ps3_buttons.bin, seront intégrées autrement plus tard).
using HRSRC = void *;
using HGLOBAL = void *;
#define MAKEINTRESOURCEW(i) (reinterpret_cast<const wchar_t *>(static_cast<uintptr_t>(i)))
inline HRSRC FindResourceW(HMODULE, const wchar_t *, const wchar_t *) { return nullptr; }
inline HGLOBAL LoadResource(HMODULE, HRSRC) { return nullptr; }
inline void *LockResource(HGLOBAL) { return nullptr; }
inline DWORD SizeofResource(HMODULE, HRSRC) { return 0; }

// DXC (wwe12_dxc.cpp, rendu D3D12 seulement) : jamais présent sur Android.
#define LOAD_LIBRARY_SEARCH_APPLICATION_DIR 0x00000200
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
inline HMODULE LoadLibraryExW(const wchar_t *, void *, DWORD) { return nullptr; }

// Horloge en millisecondes (Univers) : temps monotone depuis le démarrage.
inline unsigned long long GetTickCount64() {
  timespec ts{};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return static_cast<unsigned long long>(ts.tv_sec) * 1000ull + ts.tv_nsec / 1000000;
}
