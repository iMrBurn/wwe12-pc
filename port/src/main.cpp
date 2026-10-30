// wwe12 - ReXGlue Recompiled Project

#include "generated/default/wwe12_init.h"

#include "wwe12_app.h"

REXCVAR_DEFINE_STRING(gpu_backend, "any", "GPU",
                      "Backend du plugin GPU : any (D3D12 sous Windows), d3d12 ou vulkan")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);
REXCVAR_DEFINE_BOOL(wwe12_no_mods, false, "WWE12",
                    "Play without mods (the launcher's PLAY WITHOUT MODS): no mods folder, added arenas, texture "
                    "packs or superstar mods this time; the superstar pack already made stays as it is")
    .lifecycle(rex::cvar::Lifecycle::kInitOnly);

REX_DEFINE_APP(wwe12, Wwe12App::Create)

#ifdef _WIN32
// PC portables à deux GPU (NVIDIA Optimus / AMD switchable) : demande la carte dédiée.
// Sans ça, le jeu peut tourner sur le GPU intégré (quelques FPS). Les pilotes ne lisent
// ces symboles que dans l'exécutable lui-même, pas dans une DLL du SDK.
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif
