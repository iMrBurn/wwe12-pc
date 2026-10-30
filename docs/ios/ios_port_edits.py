#!/usr/bin/env python3
"""iOS edits to the port itself (port/), run by docs/ios/ios-port.yml on the checkout it builds.
Usage: ios_port_edits.py <port dir>

The PC and Android builds are not touched: everything is behind WWE12_IOS (defined by
cmake/ios.cmake) or inside `if(IOS)`. Every edit asserts its anchor text. The anchors were checked
on port/ at the commit that added this file.
"""
import re
import sys
from pathlib import Path

port = Path(sys.argv[1])


def read(p):
    return Path(p).read_text(encoding="utf-8")


def write(p, s):
    Path(p).write_text(s, encoding="utf-8", newline="\n")


def replace(path, old, new, what, count=1):
    s = read(path)
    assert s.count(old) == count, f"{what}: anchor found {s.count(old)} times (want {count}) in {path}: {old!r}"
    write(path, s.replace(old, new))
    print("ok:", what)


# ---- CMake -----------------------------------------------------------------------------------
cm = port / "CMakeLists.txt"
replace(cm, "project(wwe12 LANGUAGES C CXX)", "project(wwe12 LANGUAGES C CXX OBJC OBJCXX)",
        "CMakeLists: Objective-C for plume_ios.mm")
replace(cm, "if(ANDROID)\n    include(cmake/android.cmake)\nendif()\n",
        "if(ANDROID)\n    include(cmake/android.cmake)\nendif()\nif(IOS)\n    include(cmake/ios.cmake)\nendif()\n",
        "CMakeLists: include cmake/ios.cmake")

nr = port / "cmake/native_renderer.cmake"
# The host shaders (13 small HLSL files) are compiled to SPIR-V headers with dxc.exe, a Windows
# program: the macOS runner copies the headers made on the PC (host_shaders/ of the private repo,
# -DWWE12_PREBUILT_SPIRV=<folder>) instead.
s = read(nr)
pat = re.compile(r'(COMMAND )"\$\{WWE12_DXC\}" -T \$\{PROFILE\} -HV 2021 -all-resources-bound -Wno-ignored-attributes\n'
                 r'\s+\$\{args\} -I "\$\{WWE12_NATIVE_ROOT\}" -Fh "\$\{out\}" "\$\{WWE12_HLSL_DIR\}/\$\{STEM\}\.hlsl"\n'
                 r'\s+-Vn g_\$\{STEM\}_spirv\n')
assert len(pat.findall(s)) == 1, "native_renderer.cmake: SPIR-V dxc command anchor"
s = pat.sub(lambda m: m.group(1) + '${CMAKE_COMMAND} -E copy_if_different\n'
            '                "${WWE12_PREBUILT_SPIRV}/${STEM}.hlsl.spirv.h" "${out}"\n', s, count=1)
s = s.replace('set(WWE12_DXC "${WWE12_XENOS_DIR}/thirdparty/dxc-bin/bin/x64/dxc.exe")',
              'set(WWE12_DXC "${WWE12_XENOS_DIR}/thirdparty/dxc-bin/bin/x64/dxc.exe")\n'
              'if(IOS AND NOT WWE12_PREBUILT_SPIRV)\n'
              '    message(FATAL_ERROR "iOS: pass -DWWE12_PREBUILT_SPIRV=<folder with the *.hlsl.spirv.h files>")\n'
              'endif()', 1)
write(nr, s)
print("ok: native_renderer.cmake: prebuilt SPIR-V host shaders")
# The PC/Android builds link the installed SDK, where the export names the library rex::o1heap; here
# the SDK is built from source and the target is plain `o1heap`.
replace(nr, "    target_link_libraries(${target} PRIVATE plume wwe12_native_thirdparty rex::o1heap)",
        "    if(NOT TARGET rex::o1heap AND TARGET o1heap)\n"
        "        add_library(rex::o1heap ALIAS o1heap)\n"
        "    endif()\n"
        "    target_link_libraries(${target} PRIVATE plume wwe12_native_thirdparty rex::o1heap)",
        "native_renderer.cmake: rex::o1heap alias for the in-tree SDK")
# Apple's linker has no -Map=.
replace(nr, '    else()\n        target_link_options(${target} PRIVATE "-Wl,-Map=${CMAKE_BINARY_DIR}/${target}.map")',
        '    elseif(NOT APPLE)\n        target_link_options(${target} PRIVATE "-Wl,-Map=${CMAKE_BINARY_DIR}/${target}.map")',
        "native_renderer.cmake: no -Map on Apple")

# ---- Sources ---------------------------------------------------------------------------------
# The stand-in for <windows.h> is the Android one; it also serves iOS.
replace(port / "src/android/compat/windows.h",
        '#ifndef __ANDROID__\n#error "compat/windows.h is only for the Android build"\n#endif',
        '#if !defined(__ANDROID__) && !defined(WWE12_IOS)\n#error "compat/windows.h is only for the Android and iOS builds"\n#endif',
        "compat/windows.h: accept iOS")

# What the Android build does for a phone, iOS does too.
PHONE_OLD = "#if defined(__ANDROID__)\n"
PHONE_NEW = "#if defined(__ANDROID__) || defined(WWE12_IOS)\n"
for rel, n, why in [
    ("src/native/gpu_svr_stubs.cpp", 1, "3 pipeline workers (phone drivers are slow per pipeline)"),
    ("src/native/wwe12_quit.cpp", 4, "no Windows window: EXIT sends an SDL quit; guest page table scan"),
    ("src/reblue/gpu/constant_buffers.cpp", 1, "reuse constant blocks"),
    ("src/reblue/gpu/pipeline/pipeline_cache.cpp", 1, "remember pipelines the driver could not build"),
    ("src/reblue/gpu/shaders/guest_shaders.cpp", 1, "Inf/NaN preservation off (Intel PC bug only)"),
]:
    replace(port / rel, PHONE_OLD, PHONE_NEW, f"{rel}: {why}", count=n)

# MoltenVK: the macOS code starts the game again (execv) with DYLD_LIBRARY_PATH set; iOS forbids
# it, and the Vulkan runtime is found beside the executable (SDK patch). The window is a UIWindow.
nw = port / "src/reblue/platform/native_window.cpp"
replace(nw, '  if (std::getenv("REBLUE_MOLTENVK_RELAUNCHED"))\n    return; // already relaunched\n',
        '#if defined(WWE12_IOS)\n  return; // iOS: no exec, the Vulkan runtime is in the app bundle\n#endif\n'
        '  if (std::getenv("REBLUE_MOLTENVK_RELAUNCHED"))\n    return; // already relaunched\n',
        "native_window.cpp: no execv on iOS")
replace(nw, '  out.window = SDL_GetPointerProperty(\n      props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);\n',
        '#if defined(WWE12_IOS)\n'
        '  out.window = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr);\n'
        '#else\n'
        '  out.window = SDL_GetPointerProperty(\n      props, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);\n'
        '#endif\n',
        "native_window.cpp: UIWindow instead of NSWindow")

# ---- Generated code: the translation of the game runs on the PC, never here ---------------------
# generated/rexglue.cmake re-runs `rexglue codegen` when an input changed; the macOS runner has
# no rexglue tool (and no game file), so that command is dropped and only the target stays.
rg = port / "generated/rexglue.cmake"
s = read(rg)
pat = re.compile(r'add_custom_command\(\s*OUTPUT "\$\{CMAKE_CURRENT_SOURCE_DIR\}/generated/default/codegen\.build\.stamp".*?'
                 r'add_custom_target\(wwe12_codegen\s+DEPENDS "[^"]*"\)', re.S)
assert len(pat.findall(s)) == 1, "generated/rexglue.cmake: codegen command anchor"
write(rg, pat.sub("add_custom_target(wwe12_codegen)", s, count=1))
print("ok: generated/rexglue.cmake: codegen not re-run")
