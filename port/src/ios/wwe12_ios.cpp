// iOS version: replacements for the Windows-only pieces cmake/ios.cmake removes (same role as
// android/wwe12_android.cpp), plus a boot log and the first-run copy of the default settings.
// Not compiled on any other platform.
#include <mach-o/dyld.h>
#include <mach/mach.h>
#include <mach/vm_map.h>
#include <sys/stat.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "compat/windows.h"

// crash_trace.cpp (crash traces and the freeze watchdog, dbghelp): not on iOS. The renderer
// calls this once per presented frame.
void SvrWatchdogHeartbeat() {}

// compat/windows.h: VirtualQuery from the Mach VM regions. wwe12_quit.cpp walks the guest memory
// with it (readable and writable regions) to rename texts.
SIZE_T VirtualQuery(LPCVOID address, MEMORY_BASIC_INFORMATION* info, SIZE_T length) {
  if (!info || length < sizeof(*info)) return 0;
  const uintptr_t addr = reinterpret_cast<uintptr_t>(address);
  const uintptr_t page = addr & ~uintptr_t(0xFFF);
  vm_address_t region = static_cast<vm_address_t>(addr);
  vm_size_t region_size = 0;
  vm_region_basic_info_data_64_t basic{};
  mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
  mach_port_t object_name = MACH_PORT_NULL;
  const kern_return_t kr = vm_region_64(mach_task_self(), &region, &region_size, VM_REGION_BASIC_INFO_64,
                                        reinterpret_cast<vm_region_info_t>(&basic), &count, &object_name);
  *info = {};
  if (kr != KERN_SUCCESS) {  // nothing at or after this address
    info->BaseAddress = reinterpret_cast<void*>(page);
    info->RegionSize = 0x10000;
    info->State = MEM_FREE;
    info->Protect = PAGE_NOACCESS;
    return sizeof(*info);
  }
  if (addr < region) {  // gap before the next region: free until it
    info->BaseAddress = reinterpret_cast<void*>(page);
    info->RegionSize = region - page;
    info->State = MEM_FREE;
    info->Protect = PAGE_NOACCESS;
    return sizeof(*info);
  }
  const bool r = basic.protection & VM_PROT_READ;
  const bool w = basic.protection & VM_PROT_WRITE;
  const bool x = basic.protection & VM_PROT_EXECUTE;
  DWORD protect = PAGE_NOACCESS;
  if (x) protect = w ? PAGE_EXECUTE_READWRITE : (r ? PAGE_EXECUTE_READ : PAGE_EXECUTE);
  else if (r) protect = w ? PAGE_READWRITE : PAGE_READONLY;
  info->BaseAddress = reinterpret_cast<void*>(page);
  info->AllocationBase = reinterpret_cast<void*>(uintptr_t(region));
  info->RegionSize = (uintptr_t(region) + region_size) - page;
  info->State = MEM_COMMIT;
  info->Protect = protect;
  return sizeof(*info);
}

namespace {

// Before main() runs, stdout/stderr go to Documents/boot.log, so a start-up failure leaves text
// behind whatever the sideload method (idea and code from Ke619's SvR 2011 iOS port).
__attribute__((constructor(101))) void IosBootLog() {
  const char* home = std::getenv("HOME");
  if (!home) return;
  const std::string dir = std::string(home) + "/Documents";
  mkdir(dir.c_str(), 0755);
  const std::string file = dir + "/boot.log";
  if (!std::freopen(file.c_str(), "a", stderr)) return;
  std::freopen(file.c_str(), "a", stdout);
  std::setvbuf(stderr, nullptr, _IONBF, 0);
  std::setvbuf(stdout, nullptr, _IOLBF, 0);
  std::fprintf(stderr, "[boot] started t=%ld home=%s\n", static_cast<long>(std::time(nullptr)), home);
  std::atexit([] { std::fprintf(stderr, "[boot] process exit\n"); });
}

// The app bundle is read-only and the game reads wwe12.toml from "the exe folder", which is
// Documents on iOS (SDK patch in ios-port.yml): copy the bundled default there the first time,
// never over the player's own copy.
__attribute__((constructor(102))) void IosInstallDefaultSettings() {
  namespace fs = std::filesystem;
  const char* home = std::getenv("HOME");
  if (!home) return;
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::vector<char> buf(size + 1, 0);
  if (_NSGetExecutablePath(buf.data(), &size) != 0) return;
  std::error_code ec;
  const fs::path src = fs::path(buf.data()).parent_path() / "wwe12.toml";
  const fs::path dst = fs::path(home) / "Documents" / "wwe12.toml";
  if (!fs::is_regular_file(src, ec) || fs::exists(dst, ec)) return;
  fs::create_directories(dst.parent_path(), ec);
  fs::copy_file(src, dst, ec);
  std::fprintf(stderr, "[boot] wwe12.toml: %s\n", ec ? ec.message().c_str() : "copied");
}

}  // namespace
