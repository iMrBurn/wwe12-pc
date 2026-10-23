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
target_sources(wwe12 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/android/wwe12_android.cpp")

# Remplaçant de <windows.h> pour les fichiers du portage qui l'incluent (src/android/compat).
target_include_directories(wwe12 BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/android/compat"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/android")

target_link_libraries(wwe12 PRIVATE android log)
# SDL appelle main() par son nom (SDLActivity.getMainFunction) : il doit rester visible.
set_target_properties(wwe12 PROPERTIES OUTPUT_NAME "wwe12")
