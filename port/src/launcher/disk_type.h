// Is a folder on a hard disk (HDD)? Used for a discreet "loading will be slow" line on the PLAY page.
// Windows: open the volume with no access rights (no administrator needed) and ask for the "seek penalty"
// property: a spinning disk has one, an SSD does not. Anything unclear (network drive, RAID, virtual disk,
// USB adapter that doesn't answer, Wine / Proton such as the Steam Deck) = Unknown = no warning at all.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winioctl.h>

#include <filesystem>
#include <string>

namespace disk {

enum class Kind { Unknown, Ssd, Hdd };

inline Kind KindOf(const std::filesystem::path &folder) {
  try {
    const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll && GetProcAddress(ntdll, "wine_get_version"))
      return Kind::Unknown;  // Wine / Proton: the drive letters mean nothing
    std::error_code ec;
    std::wstring full = std::filesystem::absolute(folder, ec).wstring();
    if (ec || full.size() < 3 || full[1] != L':' || full.rfind(L"\\\\", 0) == 0)
      return Kind::Unknown;  // (no drive letter: network path)
    const std::wstring root = std::wstring(L"\\\\.\\") + full.substr(0, 2);
    if (GetDriveTypeW((full.substr(0, 2) + L"\\").c_str()) != DRIVE_FIXED)
      return Kind::Unknown;
    const HANDLE h = CreateFileW(root.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE)
      return Kind::Unknown;
    STORAGE_PROPERTY_QUERY q{};
    q.PropertyId = StorageDeviceSeekPenaltyProperty;
    q.QueryType = PropertyStandardQuery;
    DEVICE_SEEK_PENALTY_DESCRIPTOR d{};
    DWORD got = 0;
    const BOOL ok = DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof(q), &d, sizeof(d), &got, nullptr);
    CloseHandle(h);
    if (!ok || got < sizeof(d))
      return Kind::Unknown;
    return d.IncursSeekPenalty ? Kind::Hdd : Kind::Ssd;
  } catch (...) {
    return Kind::Unknown;
  }
}

}  // namespace disk
