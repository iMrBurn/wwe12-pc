# Version Android du jeu (inclus par CMakeLists.txt seulement si ANDROID).
# Le jeu y est une bibliothèque partagée (libwwe12.so) chargée par l'activité SDL de l'APK
# (android/game), qui appelle sa fonction main(). Le code PC n'est pas touché : tout ce qui
# est propre à Android est ici ou derrière __ANDROID__ dans les sources.

# Le codegen tourne sur le PC : l'exe Android du SDK installé ne peut pas s'y lancer, on
# prend celui du SDK PC (même traduction, port/generated est partagé).
set(WWE12_HOST_REXGLUE "${CMAKE_CURRENT_SOURCE_DIR}/../recomp/rexglue-sdk/out/install/win-amd64/bin/rexglue.exe")
if(TARGET rex::rexglue AND EXISTS "${WWE12_HOST_REXGLUE}")
    set_target_properties(rex::rexglue PROPERTIES
        IMPORTED_LOCATION "${WWE12_HOST_REXGLUE}"
        IMPORTED_LOCATION_RELEASE "${WWE12_HOST_REXGLUE}")
endif()

# Fichiers seulement utiles sur PC (Windows : traces de plantage par dbghelp, outils de test
# RenderDoc / scan mémoire, recensement D3D) : retirés de la cible Android.
get_target_property(_wwe12_sources wwe12 SOURCES)
list(FILTER _wwe12_sources EXCLUDE REGEX
    "/(crash_trace|wwe12_memscan|wwe12_autocap|d3d_census)\\.cpp$")
set_property(TARGET wwe12 PROPERTY SOURCES ${_wwe12_sources})
target_sources(wwe12 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/android/wwe12_android.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/android/wwe12_touch.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/android/wwe12_driver.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/android/wwe12_perf_hint.cpp")

# Pilotes Vulkan personnalisés (Turnip…, choisis dans le launcher) : libadrenotools
# (https://github.com/bylaws/libadrenotools, BSD-2-Clause, avec son sous-module) clonée dans
# recomp/libadrenotools. Facultative et ARM64 seulement : sans elle, le jeu prend toujours le pilote du
# téléphone (le journal le dit). Ses bibliothèques « hook » vont dans l'APK (android/game/build.ps1).
set(WWE12_ADRENOTOOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../recomp/libadrenotools")
if(ANDROID_ABI STREQUAL "arm64-v8a" AND EXISTS "${WWE12_ADRENOTOOLS_DIR}/CMakeLists.txt")
    add_subdirectory("${WWE12_ADRENOTOOLS_DIR}" "${CMAKE_BINARY_DIR}/adrenotools" EXCLUDE_FROM_ALL)
    target_link_libraries(wwe12 PRIVATE adrenotools)
    target_compile_definitions(wwe12 PRIVATE WWE12_ADRENOTOOLS=1)
    add_dependencies(wwe12 hook_impl main_hook file_redirect_hook gsl_alloc_hook)
    message(STATUS "WWE12: custom GPU drivers ON (libadrenotools)")
else()
    message(STATUS "WWE12: custom GPU drivers OFF (no recomp/libadrenotools, or not arm64)")
endif()

# Remplaçant de <windows.h> pour les fichiers du portage qui l'incluent (src/android/compat).
target_include_directories(wwe12 BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/android/compat"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/android")

target_link_libraries(wwe12 PRIVATE android log)

# ARM64 : ARMv8.2 plutôt que l'ARMv8.0 du SDK (rexglue_helpers.cmake) : atomiques LSE en une
# instruction (verrous, lwarx/stwcx du code recompilé) au lieu de boucles. Tous les téléphones
# capables de faire tourner le jeu l'ont (Snapdragon 845 / 7xx 2018 et après) ; le launcher
# prévient sur les autres (LauncherActivity.cpuTooOld). Même réglage pour le SDK (build_sdk.ps1).
if(ANDROID_ABI STREQUAL "arm64-v8a")
    foreach(_t wwe12 wwe12_recomp plume wwe12_native_thirdparty)
        if(TARGET ${_t})
            target_compile_options(${_t} PRIVATE -march=armv8.2-a)
        endif()
    endforeach()
endif()
# SDL appelle main() par son nom (SDLActivity.getMainFunction) : il doit rester visible.
set_target_properties(wwe12 PROPERTIES OUTPUT_NAME "wwe12")
