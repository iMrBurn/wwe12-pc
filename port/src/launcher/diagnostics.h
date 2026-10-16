// Machine facts for bug reports: Windows, processor, memory and graphics cards (with driver
// versions). Nothing personal: no user name, no folder paths. The launcher adds the game's version,
// the Vulkan report and the player's settings, then copies the text to the clipboard.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dxgi.h>

#include <cstdio>
#include <string>

namespace diag {

inline std::string Utf8(const std::wstring &w) {
  if (w.empty())
    return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), int(w.size()), nullptr, 0, nullptr, nullptr);
  std::string s(size_t(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), int(w.size()), s.data(), n, nullptr, nullptr);
  return s;
}

inline std::string WindowsVersion() {
  using RtlGetVersionFn = LONG(WINAPI *)(PRTL_OSVERSIONINFOW);
  const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  auto fn = ntdll ? reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion")) : nullptr;
  RTL_OSVERSIONINFOW v{sizeof(v)};
  if (!fn || fn(&v) != 0)
    return "unknown";
  char buf[64];
  snprintf(buf, sizeof(buf), "%lu.%lu build %lu", v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber);
  return buf;
}

inline std::string CpuName() {
  wchar_t name[256] = {};
  DWORD size = sizeof(name);
  std::string out = "unknown";
  if (RegGetValueW(HKEY_LOCAL_MACHINE, L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                   L"ProcessorNameString", RRF_RT_REG_SZ, nullptr, name, &size) == ERROR_SUCCESS)
    out = Utf8(name);
  while (!out.empty() && out.back() == ' ')
    out.pop_back();
  return out + " (" + std::to_string(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)) + " threads)";
}

inline std::string Memory() {
  MEMORYSTATUSEX m{sizeof(m)};
  if (!GlobalMemoryStatusEx(&m))
    return "unknown";
  return std::to_string((m.ullTotalPhys + (512ull << 20)) >> 30) + " GB";
}

// One indented line per graphics card Windows sees (software adapters left out).
inline std::string Adapters() {
  std::string out;
  IDXGIFactory1 *factory = nullptr;
  if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory))))
    return "  (DXGI unavailable)\n";
  IDXGIAdapter1 *adapter = nullptr;
  for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
    DXGI_ADAPTER_DESC1 d{};
    adapter->GetDesc1(&d);
    if (!(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
      LARGE_INTEGER driver{};
      std::string ver = "?";
      if (SUCCEEDED(adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &driver))) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u", unsigned(HIWORD(driver.HighPart)),
                 unsigned(LOWORD(driver.HighPart)), unsigned(HIWORD(driver.LowPart)),
                 unsigned(LOWORD(driver.LowPart)));
        ver = buf;
      }
      char line[400];
      snprintf(line, sizeof(line), "  %s | %llu MB video memory | driver %s\n", Utf8(d.Description).c_str(),
               static_cast<unsigned long long>(d.DedicatedVideoMemory >> 20), ver.c_str());
      out += line;
    }
    adapter->Release();
  }
  factory->Release();
  return out.empty() ? "  (none)\n" : out;
}

inline bool CopyToClipboard(const std::string &utf8) {
  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), int(utf8.size()), nullptr, 0);
  const HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, (size_t(n) + 1) * sizeof(wchar_t));
  if (!mem)
    return false;
  auto *dst = static_cast<wchar_t *>(GlobalLock(mem));
  MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), int(utf8.size()), dst, n);
  dst[n] = L'\0';
  GlobalUnlock(mem);
  if (!OpenClipboard(GetActiveWindow())) {
    GlobalFree(mem);
    return false;
  }
  EmptyClipboard();
  const bool ok = SetClipboardData(CF_UNICODETEXT, mem) != nullptr;
  CloseClipboard();
  if (!ok)
    GlobalFree(mem);  // the clipboard owns the memory only when SetClipboardData succeeds
  return ok;
}

}  // namespace diag
