# wwe12 - ReXGlue Recompiled Project
#
# SDK boilerplate lives in generated/rexglue.cmake.

cmake_minimum_required(VERSION 3.25)
project(wwe12 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Numéro de version du portage : fichier VERSION à la racine du dépôt (lu aussi par
# tools/package.ps1). Écrit dans le journal au démarrage.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION" WWE12_VERSION LIMIT_COUNT 1)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../VERSION")
add_compile_definitions(WWE12_VERSION="${WWE12_VERSION}")

include(generated/rexglue.cmake)

# Sources
set(WWE12_SOURCES
    src/main.cpp
    src/wwe12_dlc.cpp
    src/wwe12_saves.cpp
)

# Recensement des appels à la bibliothèque D3D (identification des fonctions pour le rendu
# natif) : génère src/d3d_census.cpp avec tools/gen_d3d_census.py, puis -DWWE12_D3D_CENSUS=ON.
option(WWE12_D3D_CENSUS "Compter les appels aux fonctions D3D de la XDK (d3d_census.csv)" OFF)
# Rendu natif (cmake/native_renderer.cmake) : ses crochets remplacent les mêmes fonctions D3D
# que le recensement, les deux options sont donc exclusives.
# Activé par défaut : c'est la version principale. OFF revient à l'émulation du GPU Xenos.
option(WWE12_NATIVE_RENDERER "Dessiner via plume (Vulkan) au lieu d'émuler le GPU Xenos" ON)
if(WWE12_NATIVE_RENDERER AND WWE12_D3D_CENSUS)
    message(FATAL_ERROR "WWE12_NATIVE_RENDERER et WWE12_D3D_CENSUS crochètent les mêmes fonctions")
endif()
if(WWE12_D3D_CENSUS)
    list(APPEND WWE12_SOURCES src/d3d_census.cpp)
endif()
# Icône et informations de fichier de l'exe (Propriétés > Détails), numéro tiré de VERSION.
if(WIN32)
    string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ver "${WWE12_VERSION}")
    set(WWE12_VERSION_MAJOR ${CMAKE_MATCH_1})
    set(WWE12_VERSION_MINOR ${CMAKE_MATCH_2})
    set(WWE12_VERSION_PATCH ${CMAKE_MATCH_3})
    set(WWE12_RES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/res")
    configure_file(res/wwe12.rc.in "${CMAKE_CURRENT_BINARY_DIR}/wwe12.rc" @ONLY)
    list(APPEND WWE12_SOURCES "${CMAKE_CURRENT_BINARY_DIR}/wwe12.rc")
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/wwe12.rc" PROPERTIES
        OBJECT_DEPENDS "${WWE12_RES_DIR}/wwe12.ico")
endif()

if(WIN32)
    add_executable(wwe12 WIN32 ${WWE12_SOURCES})
else()
    add_executable(wwe12 ${WWE12_SOURCES})
endif()

# Launcher (« WWE12 Launcher.exe ») : réglages, sauvegardes, mods, mises à jour, bouton Jouer.
# Win32 + Direct3D 11 + Dear ImGui (les sources d'ImGui du SDK).
if(WIN32)
    set(WWE12_IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../recomp/rexglue-sdk/thirdparty/imgui")
    configure_file(res/launcher.rc.in "${CMAKE_CURRENT_BINARY_DIR}/launcher.rc" @ONLY)
    set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/launcher.rc" PROPERTIES
        OBJECT_DEPENDS "${WWE12_RES_DIR}/wwe12.ico;${WWE12_RES_DIR}/wwe12.png")
    add_executable(wwe12_launcher WIN32
        src/launcher/launcher.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/launcher.rc"
        "${WWE12_IMGUI_DIR}/imgui.cpp"
        "${WWE12_IMGUI_DIR}/imgui_draw.cpp"
        "${WWE12_IMGUI_DIR}/imgui_tables.cpp"
        "${WWE12_IMGUI_DIR}/imgui_widgets.cpp"
        "${WWE12_IMGUI_DIR}/backends/imgui_impl_win32.cpp"
        "${WWE12_IMGUI_DIR}/backends/imgui_impl_dx11.cpp")
    target_include_directories(wwe12_launcher PRIVATE "${WWE12_IMGUI_DIR}" "${WWE12_IMGUI_DIR}/backends"
        "${CMAKE_CURRENT_SOURCE_DIR}/../recomp/rexglue-sdk/thirdparty/vulkan-headers/include")
    target_link_libraries(wwe12_launcher PRIVATE d3d11 dxgi d3dcompiler winhttp windowscodecs ole32 shell32)
    set_target_properties(wwe12_launcher PROPERTIES OUTPUT_NAME "WWE12 Launcher")
    add_dependencies(wwe12 wwe12_launcher)
endif()

# GPU Xenos émulé (plugin chargé à l'exécution, copié à côté de l'exe). Sans lui, le runtime
# démarre « without graphics system » et rien ne s'affiche tant qu'il n'y a pas de rendu natif.
# Rendu natif ajouté AVANT le SDK : plume embarque sa propre copie de VMA (Vulkan Memory
# Allocator 3.2.1) et rexruntime.dll exporte la sienne (3.3.0, SDK compilé avec Vulkan). plume
# doit précéder la bibliothèque d'import du SDK sur la ligne d'édition des liens, sinon les
# appels de reblue à VMA se résolvent vers la DLL puis entrent en conflit avec plume
# (lld : « vmaGetHeapBudgets was replaced »).
if(WWE12_NATIVE_RENDERER)
    include(cmake/native_renderer.cmake)
    wwe12_add_native_renderer(wwe12)
endif()

# Outils de test (sondes, captures automatiques) : compilés seulement si la variable
# d'environnement WWE12_DEV_PROBES est définie au moment de la configuration (build.ps1).
if(DEFINED ENV{WWE12_DEV_PROBES})
    target_compile_definitions(wwe12 PRIVATE WWE12_DEV_PROBES)
endif()

rexglue_setup_target(wwe12 GPU_PLUGINS xenos)

# Réglages d'exécution par défaut, lus au démarrage depuis le dossier de l'exe.
add_custom_command(TARGET wwe12 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/config/wwe12.toml
        $<TARGET_FILE_DIR:wwe12>/wwe12.toml
    VERBATIM
)
