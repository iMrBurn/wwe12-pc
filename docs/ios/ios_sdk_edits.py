#!/usr/bin/env python3
"""iOS edits to the ReXGlue SDK and to plume, run by docs/ios/ios-port.yml after the patches of
port/patches are applied. Usage: ios_sdk_edits.py <sdk dir> <plume dir> <plume_ios.mm>

Every edit asserts its anchor text exists exactly once, so a SDK that moved fails here with the
file and the text, not later in the build. Based on the edits of Ke619's SvR 2011 iOS port
(github.com/Ke619/WWE-SvR11-IOS, .github/workflows/ios-port.yml), re-anchored on our patched SDK.
The anchors were checked on a copy of recomp/rexglue-sdk (commit c94f5eb + port/patches).
"""
import re
import shutil
import sys
from pathlib import Path

sdk, plume, plume_ios = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])

TARGET_COND = "#if defined(__APPLE__)\n#include <TargetConditionals.h>\n#endif\n"


def read(p):
    return Path(p).read_text(encoding="utf-8")


def write(p, s):
    Path(p).write_text(s, encoding="utf-8", newline="\n")


def replace_once(path, old, new, what):
    s = read(path)
    assert s.count(old) == 1, f"{what}: anchor found {s.count(old)} times in {path}: {old!r}"
    write(path, s.replace(old, new))
    print("ok:", what)


def prepend(path, text, what):
    write(path, text + read(path))
    print("ok:", what)


# MoltenVK: no AppKit on iOS.
replace_once(sdk / "thirdparty/moltenvk/Common/CMakeLists.txt",
             "find_library(APPKIT_LIBRARY AppKit REQUIRED)\n",
             "if(NOT IOS)\nfind_library(APPKIT_LIBRARY AppKit REQUIRED)\nendif()\n",
             "moltenvk: AppKit not on iOS")

# Thread id: pthread_gettid_np is Android's. (Upstream macOS builds do not have this line in our
# SDK; the edit applies only when it is there.)
p = sdk / "src/core/threading_posix.cpp"
s = read(p)
old = "return pthread_gettid_np(thread_);"
if s.count(old) == 1:
    write(p, s.replace(old, "return [&]{ uint64_t t__ = 0; pthread_threadid_np(thread_, &t__); "
                            "return static_cast<pid_t>(t__); }();"))
    print("ok: threading_posix thread id")
else:
    print("skip: threading_posix thread id (anchor not present)")

# No mach/mach_vm.h on iOS: vm_region_64 does the same job.
write(sdk / "src/core/ios_mach_vm_shim.h", """// iOS: the SDK has no mach/mach_vm.h; vm_region_64 does the same job.
#pragma once
#include <mach/mach.h>
#include <mach/mach_init.h>
#include <mach/vm_map.h>
#include <mach/vm_region.h>
static inline kern_return_t mach_vm_region(vm_map_t task, mach_vm_address_t* address,
                                           mach_vm_size_t* size, vm_region_flavor_t flavor,
                                           vm_region_info_t info, mach_msg_type_number_t* count,
                                           mach_port_t* object_name) {
  vm_address_t a = static_cast<vm_address_t>(*address);
  vm_size_t s = 0;
  kern_return_t kr = vm_region_64(task, &a, &s, flavor, info, count, object_name);
  *address = a;
  *size = s;
  return kr;
}
""")
replace_once(sdk / "src/core/memory_posix.cpp", "#include <mach/mach_vm.h>",
             '#include "ios_mach_vm_shim.h"', "memory_posix: mach_vm shim")

# The iOS sandbox refuses shm_open, which is how the guest memory (4.5 GB) is created. A plain file
# in the app's tmp folder, deleted at once (the open handle keeps it alive; sparse on APFS).
mem = sdk / "src/core/memory_posix.cpp"
anchor = "#else\n  int oflag;\n  switch (access) {\n"
new = """#elif defined(__APPLE__) && TARGET_OS_IPHONE
  (void)access;
  (void)commit;
  const char* ios_tmp = std::getenv("TMPDIR");
  std::filesystem::path ios_dir = ios_tmp ? ios_tmp : "/tmp";
  std::error_code ios_ec;
  std::filesystem::create_directories(ios_dir, ios_ec);
  std::filesystem::path ios_backing = ios_dir / path.filename();
  int ios_fd = open(ios_backing.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600);
  if (ios_fd < 0) {
    return kFileMappingHandleInvalid;
  }
  unlink(ios_backing.c_str());
  if (rex_ftruncate64(ios_fd, static_cast<off_t>(length)) != 0) {
    close(ios_fd);
    return kFileMappingHandleInvalid;
  }
  return static_cast<FileMappingHandle>(ios_fd);
"""
replace_once(mem, anchor, new + anchor, "memory_posix: guest memory in a tmp file")
prepend(mem, TARGET_COND + "#include <cstdlib>\n#include <filesystem>\n#include <system_error>\n",
        "memory_posix: includes")

# The app bundle is read-only: the game folder (the "exe folder") is the app's Documents folder.
fs_ = sdk / "src/core/filesystem_posix.cpp"
replace_once(fs_, "std::filesystem::path GetExecutableFolder() {\n",
             "std::filesystem::path GetExecutableFolder() {\n"
             "#if defined(__APPLE__) && TARGET_OS_IPHONE\n"
             "  // [ios] The app bundle is read-only: the game folder is the app's Documents folder.\n"
             '  if (const char* home = std::getenv("HOME")) {\n'
             '    std::filesystem::path docs = std::filesystem::path(home) / "Documents";\n'
             "    std::error_code ec;\n"
             "    std::filesystem::create_directories(docs, ec);\n"
             "    return docs;\n"
             "  }\n"
             "#endif\n", "filesystem: Documents is the exe folder")
prepend(fs_, TARGET_COND + "#include <cstdlib>\n#include <system_error>\n", "filesystem: includes")

# GPU plugin dylibs ship in <bundle>/Frameworks.
gpl = sdk / "src/system/gpu_plugin_loader.cpp"
replace_once(gpl, "  auto path = rex::filesystem::GetExecutableFolder() / PluginFileName(name);\n",
             "#if defined(__APPLE__) && TARGET_OS_IPHONE\n"
             '  auto path = rex::filesystem::GetExecutablePath().parent_path() / "Frameworks" / PluginFileName(name);\n'
             "#else\n"
             "  auto path = rex::filesystem::GetExecutableFolder() / PluginFileName(name);\n"
             "#endif\n", "gpu_plugin_loader: Frameworks")
prepend(gpl, TARGET_COND + "#include <cstdlib>\n#include <system_error>\n", "gpu_plugin_loader: includes")

# SDL must own the iOS entry point (UIApplicationMain): main becomes SDL_main. Without it
# SDL_InitSubSystem(VIDEO) fails and the app exits with code 1. MoltenVK (Metal) has no geometry
# shaders or non-solid fill mode: do not require them.
m = sdk / "src/ui/windowed_app_main_sdl.cpp"
replace_once(m, "#include <rex/ui/windowed_app_context_sdl.h>\n",
             "#include <rex/ui/windowed_app_context_sdl.h>\n\n#if defined(__APPLE__)\n"
             "#include <TargetConditionals.h>\n#if TARGET_OS_IPHONE\n#include <SDL3/SDL_main.h>\n"
             "#endif\n#endif\n", "windowed_app_main_sdl: SDL_main")
replace_once(m, "  rex::cvar::ApplyEnvironment();\n  rex::InitLoggingEarly();\n",
             "  rex::cvar::ApplyEnvironment();\n#if defined(__APPLE__) && TARGET_OS_IPHONE\n"
             '  rex::cvar::SetFlagByName("vulkan_require_geometry_shader", "false");\n'
             '  rex::cvar::SetFlagByName("vulkan_require_fill_mode_non_solid", "false");\n#endif\n'
             "  rex::InitLoggingEarly();\n", "windowed_app_main_sdl: MoltenVK features")

# SDL's video driver is "uikit" on iOS ("cocoa" does not exist there).
c = sdk / "src/ui/windowed_app_context_sdl.cpp"
replace_once(c, '    requested_driver = "cocoa";\n',
             '#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE\n    requested_driver = "uikit";\n'
             '#else\n    requested_driver = "cocoa";\n#endif\n', "windowed_app_context_sdl: uikit")
s = read(c)
write(c, TARGET_COND + s)

# The Vulkan runtime lives in <app bundle>/vulkan: look beside the real executable, not Documents.
v = sdk / "src/ui/vulkan/vulkan_moltenvk.cpp"
replace_once(v, "  const fs::path executable_dir = rex::filesystem::GetExecutableFolder();\n",
             "#if defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE\n"
             "  const fs::path executable_dir = rex::filesystem::GetExecutablePath().parent_path();\n"
             "#else\n  const fs::path executable_dir = rex::filesystem::GetExecutableFolder();\n#endif\n",
             "vulkan_moltenvk: bundle folder")
prepend(v, TARGET_COND, "vulkan_moltenvk: include")

# REXGLUE_ROOT only exists in the SDK's own scope: used from the game's target it is empty and the
# post-build copy of MoltenVK_icd.json looks for /cmake/MoltenVK_icd.json. The file sits next to
# rexglue_helpers.cmake.
replace_once(sdk / "cmake/rexglue_helpers.cmake",
             'set(_rexglue_moltenvk_icd "${REXGLUE_ROOT}/cmake/MoltenVK_icd.json")',
             'set(_rexglue_moltenvk_icd "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/MoltenVK_icd.json")',
             "rexglue_helpers: MoltenVK_icd.json next to the helpers")

# plume: no Metal backend on iOS (Vulkan only), UIKit window wrapper instead of AppKit's.
pc = plume / "CMakeLists.txt"
s = read(pc)
pat = re.compile(r"if\(APPLE\)\n(\s+list\(APPEND PLUME_SOURCES\n\s+plume_metal\.cpp)")
assert len(pat.findall(s)) == 1, "plume: APPLE source list anchor"
s = pat.sub(lambda mo: "if(APPLE AND NOT IOS)\n" + mo.group(1), s, count=1)
s += "\nif(IOS)\n  target_sources(plume PRIVATE plume_ios.mm)\nendif()\n"
write(pc, s)
shutil.copy(plume_ios, plume / "plume_ios.mm")
print("ok: plume: no Metal, plume_ios.mm")

# plume starts Vulkan with a bare dlopen("libvulkan.dylib"): on macOS the game relaunches itself with
# DYLD_LIBRARY_PATH for that, which iOS forbids. Load the MoltenVK of the app bundle by its full
# path instead and hand its vkGetInstanceProcAddr to volk (no Vulkan loader, no ICD file needed).
pv = plume / "plume_vulkan.cpp"
old = "#else\n        VkResult res = volkInitialize();\n#endif\n"
new = r"""#elif defined(__APPLE__) && TARGET_OS_IPHONE
        VkResult res = VK_ERROR_INITIALIZATION_FAILED;
        {
            uint32_t exeSize = 0;
            _NSGetExecutablePath(nullptr, &exeSize);
            std::string exePath(exeSize, '\0');
            if (_NSGetExecutablePath(exePath.data(), &exeSize) == 0) {
                std::string dir(exePath.c_str());
                dir = dir.substr(0, dir.find_last_of('/'));
                const std::string mvk = dir + "/vulkan/lib/libMoltenVK.dylib";
                void *mvkLib = dlopen(mvk.c_str(), RTLD_NOW | RTLD_LOCAL);
                if (mvkLib == nullptr) {
                    fprintf(stderr, "MoltenVK: dlopen %s failed: %s\n", mvk.c_str(), dlerror());
                } else {
                    auto getInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(mvkLib, "vkGetInstanceProcAddr"));
                    if (getInstanceProcAddr != nullptr) {
                        volkInitializeCustom(getInstanceProcAddr);
                        res = VK_SUCCESS;
                    } else {
                        fprintf(stderr, "MoltenVK: no vkGetInstanceProcAddr in %s\n", mvk.c_str());
                    }
                }
            }
        }
#else
        VkResult res = volkInitialize();
#endif
"""
replace_once(pv, old, new, "plume_vulkan: MoltenVK of the bundle on iOS")
prepend(pv, "#if defined(__APPLE__)\n#include <TargetConditionals.h>\n#include <dlfcn.h>\n#include <mach-o/dyld.h>\n#include <cstdio>\n#include <string>\n#endif\n",
        "plume_vulkan: includes")
