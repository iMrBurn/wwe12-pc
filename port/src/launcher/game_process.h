// Is the game (wwe12.exe, or wwe12_d3d12.exe / wwe12_d3d11.exe, the DirectX builds) running? Its files are locked then: saves, updates and imports wait.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>

namespace game {

inline bool IsRunning() {
  const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE)
    return false;
  PROCESSENTRY32W e{sizeof(e)};
  bool found = false;
  for (BOOL ok = Process32FirstW(snap, &e); ok && !found; ok = Process32NextW(snap, &e))
    found = _wcsicmp(e.szExeFile, L"wwe12.exe") == 0 || _wcsicmp(e.szExeFile, L"wwe12_d3d12.exe") == 0 ||
            _wcsicmp(e.szExeFile, L"wwe12_d3d11.exe") == 0;
  CloseHandle(snap);
  return found;
}

}  // namespace game
