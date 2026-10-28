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
option(WWE12_D3D12_EXE "Also build wwe12_d3d12.exe (D3D12 renderer)" ${WIN32})
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
set(WWE12_HOST_SHADER_HEADERS_DXBC "")
# wwe12_d3d11.exe: the Windows SDK's fxc (Shader Model 5.0) compiles a converted copy of each host
# shader (tools/host_shader_d3d11.py) and the game's shaders (tools/convert_shaders_d3d11.py).
if(WIN32)
    file(GLOB WWE12_FXC_CANDIDATES "C:/Program Files (x86)/Windows Kits/10/bin/10.*/x64/fxc.exe")
    list(SORT WWE12_FXC_CANDIDATES)
    list(POP_BACK WWE12_FXC_CANDIDATES WWE12_FXC)
    find_package(Python3 COMPONENTS Interpreter QUIET)
endif()
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
    if(WWE12_FXC AND Python3_FOUND)
        string(REPLACE "_6_0" "_5_0" profile_sm5 "${PROFILE}")
        set(out_dxbc "${WWE12_NATIVE_GEN}/src/gpu/shaders/hlsl/${STEM}.hlsl.dxbc.h")
        add_custom_command(
            OUTPUT "${out_dxbc}"
            COMMAND "${Python3_EXECUTABLE}" -I "${WWE12_NATIVE_ROOT}/../tools/host_shader_d3d11.py"
                    "${WWE12_FXC}" ${profile_sm5} "${WWE12_HLSL_DIR}/${STEM}.hlsl" "${out_dxbc}"
                    g_${STEM}_dxbc -I "${WWE12_NATIVE_ROOT}"
            DEPENDS "${WWE12_HLSL_DIR}/${STEM}.hlsl" ${hlsl_includes}
                    "${WWE12_NATIVE_ROOT}/../tools/host_shader_d3d11.py"
            COMMENT "Compiling ${STEM}.hlsl (${profile_sm5}, DXBC)"
            VERBATIM)
        set(WWE12_HOST_SHADER_HEADERS_DXBC ${WWE12_HOST_SHADER_HEADERS_DXBC} "${out_dxbc}"
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

# D3D12: every spec-constant variant of the game's DXIL shaders linked at build time by
# wwe12_prelink (port/src/prelink), compiled into wwe12_d3d12.exe as linked_shader_cache.cpp, so
# the exe needs no DXC at run time (dxcompiler.dll / dxil.dll are never shipped: Microsoft's
# dxil.dll licence). The tool runs DXC on the build machine; the two DLLs land next to it in the
# build folder, where the dev exe can still link a missing variant.
if(PLUME_D3D12_ENABLED)
    set(WWE12_DXC_DIR "${WWE12_XENOS_DIR}/thirdparty/dxc-bin")
    set(WWE12_MINIZ_DIR "${WWE12_XENOS_DIR}/thirdparty/smol-v/testing/external/miniz")
    add_executable(wwe12_prelink
        "${WWE12_NATIVE_ROOT}/src/prelink/wwe12_prelink.cpp"
        "${WWE12_NATIVE_ROOT}/src/reblue/gpu/shaders/dxc_link.cpp"
        "${WWE12_NATIVE_ROOT}/generated/native/shader_cache.cpp"
        "${WWE12_NATIVE_ROOT}/third_party/reblue_thirdparty/zstd/zstddeclib.c"
        "${WWE12_MINIZ_DIR}/miniz.c")
    target_include_directories(wwe12_prelink PRIVATE
        "${WWE12_NATIVE_ROOT}/src/reblue"
        "${WWE12_NATIVE_ROOT}/third_party/reblue_thirdparty/zstd"
        "${WWE12_MINIZ_DIR}"
        "${WWE12_DXC_DIR}/inc")
    target_link_libraries(wwe12_prelink PRIVATE "${WWE12_DXC_DIR}/lib/x64/dxcompiler.lib")
    add_custom_command(TARGET wwe12_prelink POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WWE12_DXC_DIR}/bin/x64/dxcompiler.dll" "${WWE12_DXC_DIR}/bin/x64/dxil.dll"
            $<TARGET_FILE_DIR:wwe12_prelink>
        VERBATIM)
    set(WWE12_LINKED_SHADER_CACHE "${WWE12_NATIVE_GEN}/linked_shader_cache.cpp")
    add_custom_command(
        OUTPUT "${WWE12_LINKED_SHADER_CACHE}"
        COMMAND wwe12_prelink "${WWE12_LINKED_SHADER_CACHE}"
        DEPENDS wwe12_prelink "${WWE12_NATIVE_ROOT}/generated/native/shader_cache.cpp"
        COMMENT "Pre-linking the D3D12 spec-constant shader variants"
        VERBATIM)
endif()

file(GLOB_RECURSE WWE12_NATIVE_SOURCES CONFIGURE_DEPENDS
    "${WWE12_NATIVE_ROOT}/src/reblue/*.cpp" "${WWE12_NATIVE_ROOT}/src/reblue/*.h"
    "${WWE12_NATIVE_ROOT}/src/native/*.cpp" "${WWE12_NATIVE_ROOT}/src/native/*.h")

# wwe12_add_native_renderer(<target> [D3D12|D3D11]): D3D12 builds re:Blue with REBLUE_D3D12 (D3D12
# only, DXIL shaders); D3D11 adds REBLUE_D3D11 to it (plume_d3d11, DXBC shaders, in progress);
# without either, Vulkan.
function(wwe12_add_native_renderer target)
    cmake_parse_arguments(ARG "D3D12;D3D11" "" "" ${ARGN})
    if(NOT EXISTS "${WWE12_NATIVE_ROOT}/generated/native/shader_cache.cpp")
        message(FATAL_ERROR "generated/native/shader_cache.cpp manquant : extraire et convertir "
                            "les shaders du jeu d'abord (voir le README, rendu natif)")
    endif()
    set(sources ${WWE12_NATIVE_SOURCES})
    if(ARG_D3D11)
        if(NOT EXISTS "${WWE12_NATIVE_ROOT}/generated/native/dxbc_shader_cache.cpp")
            message(FATAL_ERROR "generated/native/dxbc_shader_cache.cpp manquant : "
                                "tools/convert_shaders_d3d11.py d'abord")
        endif()
        set(host_shaders ${WWE12_HOST_SHADER_HEADERS_DXBC})
        target_compile_definitions(${target} PRIVATE REBLUE_D3D12=1 REBLUE_D3D11=1)
        target_link_libraries(${target} PRIVATE d3d11 dxgi dxguid)
        target_include_directories(${target} PRIVATE "${WWE12_MINIZ_DIR}")
        list(FILTER sources EXCLUDE REGEX "/gpu/shaders/(shader_linker|dxc_link)\\.(cpp|h)$")
        # plume's D3D11 backend (port/patches/plume-d3d11.patch, from the SvR 2011 PC port).
        target_sources(${target} PRIVATE "${WWE12_MINIZ_DIR}/miniz.c"
            "${WWE12_PLUME_DIR}/plume_d3d11.cpp"
            "${WWE12_NATIVE_ROOT}/generated/native/dxbc_shader_cache.cpp")
    elseif(ARG_D3D12)
        set(host_shaders ${WWE12_HOST_SHADER_HEADERS_DXIL})
        target_compile_definitions(${target} PRIVATE REBLUE_D3D12=1)
        # dxc_link.cpp links the game's spec-constant shaders at run time (DXC, dxcompiler.dll).
        target_include_directories(${target} PRIVATE "${WWE12_XENOS_DIR}/thirdparty/dxc-bin/inc")
        # dxcompiler.dll is loaded only when a variant is missing from the pre-linked cache (dev
        # builds): the shipped exe starts without it (guest_shaders.cpp checks before linking).
        target_link_libraries(${target} PRIVATE d3d12 dxgi dxguid delayimp
            "${WWE12_DXC_DIR}/lib/x64/dxcompiler.lib")
        target_link_options(${target} PRIVATE "-Wl,/DELAYLOAD:dxcompiler.dll")
        # miniz (public domain) unpacks the pre-linked cache.
        target_include_directories(${target} PRIVATE "${WWE12_MINIZ_DIR}")
        target_sources(${target} PRIVATE "${WWE12_MINIZ_DIR}/miniz.c" "${WWE12_LINKED_SHADER_CACHE}")
    else()
        set(host_shaders ${WWE12_HOST_SHADER_HEADERS})
        # re:Blue's DXIL linker serves the D3D12 exe only.
        list(FILTER sources EXCLUDE REGEX "/gpu/shaders/(shader_linker|dxc_link)\\.(cpp|h)$")
    endif()
    target_sources(${target} PRIVATE
        ${sources}
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
