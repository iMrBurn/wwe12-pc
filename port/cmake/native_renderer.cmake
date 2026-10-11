# Native renderer (adapted from re:Blue, BSD-3-Clause, see src/reblue/LICENSE.reblue, through
# nefariousjosiah's SvR 2008-2010 ports, GPL-3.0).
#
# Draws through plume (Vulkan) by observing the game's XDK D3D functions, instead of emulating
# the Xenos GPU. Game shaders come precompiled from generated/native/shader_cache.cpp: the
# shaders extracted from the game files (tools/find_shaders.py --bpe) converted with the patched
# reblue-XenosRecomp (see the README).
#
# The renderer's sources go straight into the executable: its REX_HOOK overrides are strong
# definitions nothing else references, so a static library would drop them at link time.

enable_language(C)  # zstd and xxHash are C

set(WWE12_NATIVE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(WWE12_RECOMP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../recomp")
set(WWE12_XENOS_DIR "${WWE12_RECOMP_DIR}/reblue-XenosRecomp")
set(WWE12_PLUME_DIR "${WWE12_RECOMP_DIR}/plume")
set(WWE12_NATIVE_GEN "${CMAKE_BINARY_DIR}/native_gen")

# Rendering hardware interface: Vulkan. plume's D3D12 backend stays off, as in the SvR ports.
set(PLUME_D3D12_ENABLED OFF CACHE BOOL "" FORCE)
add_subdirectory("${WWE12_PLUME_DIR}" "${CMAKE_BINARY_DIR}/plume" EXCLUDE_FROM_ALL)

# Small third-party pieces the renderer uses.
add_library(wwe12_native_thirdparty STATIC
    "${WWE12_NATIVE_ROOT}/third_party/reblue_thirdparty/zstd/zstddeclib.c"
    "${WWE12_XENOS_DIR}/thirdparty/xxHash/xxhash.c"
    "${WWE12_XENOS_DIR}/thirdparty/smol-v/source/smolv.cpp")
target_include_directories(wwe12_native_thirdparty PUBLIC
    "${WWE12_NATIVE_ROOT}/third_party/reblue_thirdparty/zstd"
    "${WWE12_XENOS_DIR}/thirdparty/xxHash"
    "${WWE12_XENOS_DIR}/thirdparty/smol-v/source")

# Host shaders (copy, ImGui overlay, MSAA resolve, gamma) compiled to SPIR-V headers with the
# DXC shipped in reblue-XenosRecomp (the Windows SDK's dxc has no SPIR-V backend).
set(WWE12_DXC "${WWE12_XENOS_DIR}/thirdparty/dxc-bin/bin/x64/dxc.exe")
set(WWE12_HLSL_DIR "${WWE12_NATIVE_ROOT}/src/reblue/gpu/shaders/hlsl")
set(WWE12_HOST_SHADER_HEADERS "")
function(wwe12_host_shader STEM PROFILE)
    file(GLOB hlsl_includes "${WWE12_HLSL_DIR}/*.hlsli")
    set(args -spirv -fvk-use-dx-layout)
    if(PROFILE MATCHES "^vs")
        list(APPEND args -fvk-invert-y)
    endif()
    set(out "${WWE12_NATIVE_GEN}/src/gpu/shaders/hlsl/${STEM}.hlsl.spirv.h")
    add_custom_command(
        OUTPUT "${out}"
        COMMAND "${WWE12_DXC}" -T ${PROFILE} -HV 2021 -all-resources-bound -Wno-ignored-attributes
                ${args} -I "${WWE12_NATIVE_ROOT}" -Fh "${out}" "${WWE12_HLSL_DIR}/${STEM}.hlsl"
                -Vn g_${STEM}_spirv
        DEPENDS "${WWE12_HLSL_DIR}/${STEM}.hlsl" ${hlsl_includes}
        COMMENT "Compiling ${STEM}.hlsl (${PROFILE}, SPIR-V)"
        VERBATIM)
    set(WWE12_HOST_SHADER_HEADERS ${WWE12_HOST_SHADER_HEADERS} "${out}" PARENT_SCOPE)
endfunction()
foreach(shader IN ITEMS copy_vs imgui_vs)
    wwe12_host_shader(${shader} vs_6_0)
endforeach()
foreach(shader IN ITEMS copy_color_ps copy_depth_ps gamma_correction_ps pfx_occlusion_count_ps
        imgui_ps resolve_msaa_color_2x resolve_msaa_color_4x resolve_msaa_color_8x
        resolve_msaa_depth_2x resolve_msaa_depth_4x resolve_msaa_depth_8x)
    wwe12_host_shader(${shader} ps_6_0)
endforeach()

file(GLOB_RECURSE WWE12_NATIVE_SOURCES CONFIGURE_DEPENDS
    "${WWE12_NATIVE_ROOT}/src/reblue/*.cpp" "${WWE12_NATIVE_ROOT}/src/reblue/*.h"
    "${WWE12_NATIVE_ROOT}/src/native/*.cpp" "${WWE12_NATIVE_ROOT}/src/native/*.h")

function(wwe12_add_native_renderer target)
    if(NOT EXISTS "${WWE12_NATIVE_ROOT}/generated/native/shader_cache.cpp")
        message(FATAL_ERROR "generated/native/shader_cache.cpp manquant : extraire et convertir "
                            "les shaders du jeu d'abord (voir le README, rendu natif)")
    endif()
    target_sources(${target} PRIVATE
        ${WWE12_NATIVE_SOURCES}
        ${WWE12_HOST_SHADER_HEADERS}
        "${WWE12_NATIVE_ROOT}/generated/native/shader_cache.cpp")
    # Secondary shader keys (tools/gen_shader_keys.py): match containers the game altered in memory.
    if(EXISTS "${WWE12_NATIVE_ROOT}/generated/native/shader_keys.cpp")
        target_sources(${target} PRIVATE "${WWE12_NATIVE_ROOT}/generated/native/shader_keys.cpp")
        target_compile_definitions(${target} PRIVATE WWE12_SHADER_KEYS=1)
    endif()
    target_include_directories(${target} PRIVATE
        "${WWE12_NATIVE_ROOT}/src/reblue"
        "${WWE12_NATIVE_ROOT}/src/native"
        "${WWE12_NATIVE_GEN}"
        "${WWE12_PLUME_DIR}"
        "${WWE12_XENOS_DIR}/XenosRecomp")
    target_compile_definitions(${target} PRIVATE SVR_NATIVE_RENDERER=1)
    # WWE '12's addresses for the D3D functions re:Blue's hooks name (D3DDevice_Clear -> sub_...).
    set_source_files_properties(${WWE12_NATIVE_SOURCES} PROPERTIES
        COMPILE_OPTIONS "-include;${WWE12_NATIVE_ROOT}/src/native/wwe12_d3d_names.h")
    target_link_libraries(${target} PRIVATE plume wwe12_native_thirdparty rex::o1heap)
    # Linker map: names recompiled functions in crash stacks.
    target_link_options(${target} PRIVATE "-Wl,/MAP:${CMAKE_BINARY_DIR}/wwe12.map")
endfunction()
