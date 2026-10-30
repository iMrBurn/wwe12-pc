# iOS version of the game (included by CMakeLists.txt only if IOS; built by docs/ios/ios-port.yml).
# An executable (not a library like Android): SDL owns UIApplicationMain, main() is SDL_main
# (SDK patch in the workflow). Like cmake/android.cmake, nothing PC-only is touched: what is
# specific to the phone is here or behind WWE12_IOS in the sources.

# Files only useful on the PC (Windows crash traces by dbghelp, RenderDoc / memory-scan test
# tools, the D3D census) leave the target; src/ios/wwe12_ios.cpp replaces what they provided.
get_target_property(_wwe12_sources wwe12 SOURCES)
list(FILTER _wwe12_sources EXCLUDE REGEX
    "/(crash_trace|wwe12_memscan|wwe12_autocap|d3d_census)\\.cpp$")
set_property(TARGET wwe12 PROPERTY SOURCES ${_wwe12_sources})
target_sources(wwe12 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/ios/wwe12_ios.cpp")
target_compile_definitions(wwe12 PRIVATE WWE12_IOS=1)

# The stand-in for <windows.h> and the MSVC runtime functions are the Android ones (the header
# accepts WWE12_IOS, see the workflow), shared so there is one copy to keep.
target_include_directories(wwe12 BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/android/compat"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/android")

# The SDK is built from source here (PC and Android link the installed one, which ships the headers):
# the Dear ImGui headers that rex/ui/style.h and wwe12_display.cpp include.
target_include_directories(wwe12 PRIVATE "${REXSDK_DIR}/thirdparty/imgui")
target_compile_options(wwe12 PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/src/android/compat/wwe12_crt_compat.h>")

# encoding.cpp converts texts with iconv (libiconv ships with the iOS SDK, not linked by default).
target_link_libraries(wwe12 PRIVATE iconv)
