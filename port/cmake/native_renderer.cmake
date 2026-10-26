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

# Rendering hardware interface: Vulkan (wwe12.exe), plus D3D12 on Windows (wwe12_d3d12.exe, the
# same game code with re:Blue built REBLUE_D3D12; the launcher starts one or the other from
# gpu_backend). plume builds both backends into one library; each exe uses one.
# OFF until the D3D12 exe builds (re:Blue shader_linker missing): the other conversations build too.
option(WWE12_D3D12_EXE "Also build wwe12_d3d12.exe (D3D12 renderer)" OFF)
if(WIN32 AND WWE12_D3D12_EXE)
    set(PLUME_D3D12_ENABLED ON CACHE BOOL "" FORCE)
else()
    set(PLUME_D3D12_ENABLED OFF CACHE BOOL "" FORCE)
endif()
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

# Host shaders (copy, ImGui overlay, MSAA resolve, gamma) compiled to SPIR-V headers (Vulkan) and
# DXIL headers (D3D12) with the DXC shipped in reblue-XenosRecomp (the Windows SDK's dxc has no
# SPIR-V backend). The HLSL keeps its Vulkan-only parts under #ifdef __spirv__.
set(WWE12_DXC "${WWE12_XENOS_DIR}/thirdparty/dxc-bin/bin/x64/dxc.exe")
set(WWE12_HLSL_DIR "${WWE12_NATIVE_ROOT}/src/reblue/gpu/shaders/hlsl")
set(WWE12_HOST_SHADER_HEADERS "")
set(WWE12_HOST_SHADER_HEADERS_DXIL "")
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
    if(PLUME_D3D12_ENABLED)
        set(out_dxil "${WWE12_NATIVE_GEN}/src/gpu/shaders/hlsl/${STEM}.hlsl.dxil.h")
        add_custom_command(
            OUTPUT "${out_dxil}"
            COMMAND "${WWE12_DXC}" -T ${PROFILE} -HV 2021 -all-resources-bound
                    -Wno-ignored-attributes -I "${WWE12_NATIVE_ROOT}" -Fh "${out_dxil}"
                    "${WWE12_HLSL_DIR}/${STEM}.hlsl" -Vn g_${STEM}_dxil
            DEPENDS "${WWE12_HLSL_DIR}/${STEM}.hlsl" ${hlsl_includes}
            COMMENT "Compiling ${STEM}.hlsl (${PROFILE}, DXIL)"
            VERBATIM)
        set(WWE12_HOST_SHADER_HEADERS_DXIL ${WWE12_HOST_SHADER_HEADERS_DXIL} "${out_dxil}"
            PARENT_SCOPE)
    endif()
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

# wwe12_add_native_renderer(<target> [D3D12]): D3D12 builds re:Blue with REBLUE_D3D12 (D3D12
# only, DXIL shaders); without it, Vulkan.
function(wwe12_add_native_renderer target)
    cmake_parse_arguments(ARG "D3D12" "" "" ${ARGN})
    if(NOT EXISTS "${WWE12_NATIVE_ROOT}/generated/native/shader_cache.cpp")
        message(FATAL_ERROR "generated/native/shader_cache.cpp manquant : extraire et convertir "
                            "les shaders du jeu d'abord (voir le README, rendu natif)")
    endif()
    if(ARG_D3D12)
        set(host_shaders ${WWE12_HOST_SHADER_HEADERS_DXIL})
        target_compile_definitions(${target} PRIVATE REBLUE_D3D12=1)
        target_link_libraries(${target} PRIVATE d3d12 dxgi dxguid)
    else()
        set(host_shaders ${WWE12_HOST_SHADER_HEADERS})
    endif()
    target_sources(${target} PRIVATE
        ${WWE12_NATIVE_SOURCES}
        ${host_shaders}
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
        "${WWE12_XENOS_DIR}/XenosRecomp"
        # stb_image (PNG reader) for the texture packs (src/native/wwe12_texture_packs.cpp)
        "${WWE12_RECOMP_DIR}/rexglue-sdk/thirdparty/stb")
    target_compile_definitions(${target} PRIVATE SVR_NATIVE_RENDERER=1)
    # WWE '12's addresses for the D3D functions re:Blue's hooks name (D3DDevice_Clear -> sub_...).
    set_source_files_properties(${WWE12_NATIVE_SOURCES} PROPERTIES
        COMPILE_OPTIONS "-include;${WWE12_NATIVE_ROOT}/src/native/wwe12_d3d_names.h")
    target_link_libraries(${target} PRIVATE plume wwe12_native_thirdparty rex::o1heap)
    # Linker map: names recompiled functions in crash stacks.
    if(WIN32)
        target_link_options(${target} PRIVATE "-Wl,/MAP:${CMAKE_BINARY_DIR}/${target}.map")
    else()
        target_link_options(${target} PRIVATE "-Wl,-Map=${CMAKE_BINARY_DIR}/${target}.map")
    endif()
endfunction()
