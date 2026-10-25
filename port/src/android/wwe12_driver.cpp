// Version Android : pilote Vulkan personnalisé (Mesa Turnip…) choisi dans le launcher
// (DriverManager.java, d'après le travail de Yexclsv). Le launcher pose WWE12_DRIVER_* dans
// l'environnement ; plume (plume-android-driver.patch) appelle wwe12_custom_vulkan_driver() avant
// de charger Vulkan : un handle = ce pilote à la place de libvulkan.so, nullptr = pilote du téléphone.
// libadrenotools (recomp/libadrenotools, BSD-2-Clause) charge le pilote dans son propre espace de
// noms du linker. Sans elle (WWE12_ADRENOTOOLS non défini, x86_64) : toujours le pilote du téléphone.
#include <cstdlib>
#include <mutex>
#include <string>

#include <rex/logging/macros.h>

#if defined(WWE12_ADRENOTOOLS)
#include <dlfcn.h>

#include <adrenotools/driver.h>
#endif

extern "C" __attribute__((visibility("default"))) void *wwe12_custom_vulkan_driver() {
  static std::once_flag once;
  static void *handle = nullptr;
  std::call_once(once, [] {
    const char *dir = std::getenv("WWE12_DRIVER_DIR");
    if (!dir || !dir[0]) {
      REXLOG_INFO("GPU driver: the phone's own driver");
      return;
    }
    const char *name = std::getenv("WWE12_DRIVER_NAME");
#if defined(WWE12_ADRENOTOOLS)
    const char *lib = std::getenv("WWE12_DRIVER_LIB");
    const char *tmp = std::getenv("WWE12_DRIVER_TMP");
    const char *hooks = std::getenv("REX_NATIVE_LIB_DIR");
    if (!lib || !tmp || !hooks) {
      REXLOG_WARN("GPU driver: custom driver setup incomplete, using the phone's own driver");
      return;
    }
    // adrenotools wants the hook folder with a trailing slash.
    std::string hook_dir = hooks;
    if (hook_dir.back() != '/') hook_dir += '/';
    REXLOG_INFO("GPU driver: loading custom driver {} ({} from {})", name ? name : lib, lib, dir);
    handle = adrenotools_open_libvulkan(RTLD_NOW | RTLD_LOCAL, ADRENOTOOLS_DRIVER_CUSTOM, tmp,
                                        hook_dir.c_str(), dir, lib, nullptr, nullptr);
    if (handle)
      REXLOG_INFO("GPU driver: custom driver loaded");
    else
      REXLOG_ERROR("GPU driver: custom driver FAILED to load, using the phone's own driver");
#else
    REXLOG_WARN("GPU driver: custom driver {} chosen, but this build cannot load drivers: using "
                "the phone's own driver", name ? name : dir);
#endif
  });
  return handle;
}
