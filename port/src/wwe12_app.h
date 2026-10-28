// wwe12 - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <array>
#include <cctype>
#include <cstdlib>
#include <string>
#include <filesystem>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging/macros.h>
#include <rex/rex_app.h>
#include <rex/filesystem/devices/host_path_device.h>
#include <rex/filesystem/vfs.h>
#include <rex/system/gpu_plugin.h>
#include <rex/system/kernel_state.h>

#if defined(SVR_NATIVE_RENDERER)
#include "core/threading.h"
#include "gpu/device.h"
#include "gpu/imgui_overlay_drawer.h"
#include "native/wwe12_display.h"
#include "native/wwe12_graphics_menu.h"
#include "native/wwe12_jukebox.h"
#include <rex/input/input_system.h>
#endif

#if defined(SVR_NATIVE_RENDERER)
void Wwe12MountMenuPatch(rex::filesystem::VirtualFileSystem* fs, const std::string& game_device,
                         const std::filesystem::path& user_root);
// Arènes ajoutées (native/wwe12_arena_pages.cpp) : fichiers servis avant le dossier mods.
void Wwe12MountArenaSlots(rex::filesystem::VirtualFileSystem* fs, const std::string& game_device,
                          const std::filesystem::path& user_root, const std::filesystem::path& mods);
// Fond du menu principal tiré au sort (native/wwe12_superstar_mods.cpp), servi devant le dossier mods.
void Wwe12MountMenuBackground(rex::filesystem::VirtualFileSystem* fs, const std::string& game_device);
// Catcheurs ajoutés (native/wwe12_superstar_mods.cpp) : un pack DLC fait des mods au démarrage.
void Wwe12MountSuperstarMods(rex::filesystem::VirtualFileSystem* fs, const std::filesystem::path& user_root,
                             const std::filesystem::path& mods);
#else
inline void Wwe12MountMenuBackground(rex::filesystem::VirtualFileSystem*, const std::string&) {}
inline void Wwe12MountSuperstarMods(rex::filesystem::VirtualFileSystem*, const std::filesystem::path&,
                                    const std::filesystem::path&) {}
inline void Wwe12MountMenuPatch(rex::filesystem::VirtualFileSystem*, const std::string&,
                                const std::filesystem::path&) {}
inline void Wwe12MountArenaSlots(rex::filesystem::VirtualFileSystem*, const std::string&,
                                 const std::filesystem::path&, const std::filesystem::path&) {}
#endif

// Défini dans main.cpp. ReXApp charge toujours le plugin GPU avec le backend « any »
// (D3D12 en priorité sous Windows) : ce réglage permet de forcer Vulkan.
REXCVAR_DECLARE(std::string, gpu_backend);
REXCVAR_DECLARE(bool, wwe12_precise_timer);

// Fenêtre d'erreur quand Vulkan ne démarre pas (native/wwe12_vulkan_error.cpp).
void Wwe12ShowVulkanError();
// AUTO : démarre l'exe suivant (wwe12.exe -> wwe12_d3d12.exe -> wwe12_d3d11.exe) ; false sinon.
bool Wwe12TryNextBackend();
namespace rex::input { class InputSystem; }
void Wwe12ButtonsSetInput(rex::input::InputSystem* input);  // wwe12_buttons.cpp
void Wwe12KbdInit(rex::ui::Window* window);  // wwe12_kbd.cpp: keyboard typing in letter grids

// Installe les DLC posés dans le dossier « dlc » à côté de l'exe (wwe12_dlc.cpp).
void Wwe12InstallDlc(const std::filesystem::path& user_data_root);
#if REX_PLATFORM_ANDROID
void Wwe12AndroidFreshCache(const std::filesystem::path& cache_dir);  // android/wwe12_replay.cpp
#endif
void Wwe12CheckDiscVersion(const std::filesystem::path& user_data_root);  // wwe12_disc.cpp
void Wwe12ImportSaves(const std::filesystem::path& user_data_root);  // wwe12_saves.cpp
#if defined(__ANDROID__)
void Wwe12AndroidCaptureStderr();  // android/wwe12_android.cpp: plume's errors into the log
#endif

class Wwe12App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Wwe12App>(new Wwe12App(ctx, "wwe12",
        PPCImageConfig));
  }

  // GPU Xenos émulé par défaut ; --gpu_plugin=<nom> permet d'en choisir un autre,
  // et --gpu_backend=d3d12|vulkan le backend de rendu.
  void OnPreSetup(rex::RuntimeConfig& config) override {
#if defined(__ANDROID__)
    Wwe12AndroidCaptureStderr();
#endif
    if (REXCVAR_GET(wwe12_precise_timer)) {
      bd::EnableHighResTimer();
      REXLOG_INFO("Horloge Windows : précision 1 ms");
    }
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
    }
#if defined(SVR_NATIVE_RENDERER)
    // Rendu natif : le backend « null » du plugin Xenos fait avancer le flux de commandes D3D
    // du jeu (fences, interruptions, vblank) sans rien dessiner ; les crochets de src/reblue
    // dessinent via plume.
    if (!config.graphics) {
      config.graphics = rex::system::LoadGpuPlugin(config.gpu_plugin, "null");
      REXLOG_INFO("Rendu natif : GPU invité {}", config.graphics ? "null (synchro seule)" : "ABSENT");
    }
    return;
#endif
    const std::string& backend = REXCVAR_GET(gpu_backend);
    if (!config.graphics && !backend.empty() && backend != "any") {
      config.graphics = rex::system::LoadGpuPlugin(config.gpu_plugin, backend);
      if (!config.graphics) {
        // ReXApp retentera avec le backend par défaut.
        REXLOG_ERROR("Backend GPU '{}' indisponible, retour au backend par défaut", backend);
      }
    }
  }

  // Sans --game_data_root (lancement par double-clic), cherche les fichiers du jeu
  // (default.xex) dans un dossier « assets » à côté de l'exe, puis dans port/assets en
  // remontant depuis le dossier de compilation (port/out/build/<preset>/wwe12.exe).
  void OnConfigurePaths(rex::PathConfig& paths) override {
    user_data_root_ = paths.user_data_root;
    if (!paths.game_data_root.empty()) {
      return;
    }
    const std::filesystem::path exe_dir = rex::filesystem::GetExecutableFolder();
    const std::array<std::filesystem::path, 3> candidates = {
        exe_dir / "assets",
        exe_dir / "game",
        exe_dir / ".." / ".." / ".." / "assets",
    };
    for (const auto& dir : candidates) {
      std::error_code ec;
      if (std::filesystem::exists(dir / "default.xex", ec)) {
        paths.game_data_root = std::filesystem::weakly_canonical(dir, ec);
        if (ec) {
          paths.game_data_root = dir;
        }
        return;
      }
    }
    // Sinon, l'image disque du jeu (.iso) posée dans game\ ou à côté de l'exe, lue sans extraction.
    for (const auto& dir : {exe_dir / "game", exe_dir}) {
      std::error_code ec;
      for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        std::string ext = rex::path_to_utf8(entry.path().extension());
        for (auto& c : ext)
          c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (entry.is_regular_file(ec) && ext == ".iso") {
          paths.game_data_root = entry.path();
          return;
        }
      }
    }
  }

  // Partition « cache » du disque dur Xbox : le jeu y recopie des données (Spectacular.rps,
  // EditEx.cac...) pour charger plus vite. Le runtime ne la monte pas (cache: sans
  // périphérique) : on la monte, comme Xenia, sur Documents/wwe12/xbox_cache.
  // Version du disque du joueur (journal, et avertissement si elle diffère de celle du portage).
  void OnPostLoadXexImage() override { Wwe12CheckDiscVersion(user_data_root_); }

  void OnPostSetup() override {
    REXLOG_INFO("WWE '12 PC version {}", WWE12_VERSION);
    if (user_data_root_.empty())
      return;
    const std::filesystem::path dir = user_data_root_ / "xbox_cache";
    std::error_code ec;
#if REX_PLATFORM_ANDROID
    Wwe12AndroidFreshCache(dir);  // android/wwe12_replay.cpp: stale cache = endless loading
#endif
    std::filesystem::create_directories(dir, ec);
    auto* fs = REX_KERNEL_FS();
    auto device = std::make_unique<rex::filesystem::HostPathDevice>("\\CACHE", dir, false);
    if (!fs || !device->Initialize() || !fs->RegisterDevice(std::move(device))) {
      REXLOG_WARN("Cache Xbox : impossible de monter {}", rex::path_to_utf8(dir));
      return;
    }
    fs->RegisterSymbolicLink("cache:", "\\CACHE");
    fs->RegisterSymbolicLink("cache0:", "\\CACHE");
    fs->RegisterSymbolicLink("cache1:", "\\CACHE");
    REXLOG_INFO("Cache Xbox monté sur {}", rex::path_to_utf8(dir));
    MountModsOverlay(fs);
    // DLC : extraits une fois pour toutes dans Documents\wwe12\0000000000000000\, où le jeu
    // les trouve comme sur le disque dur de la console.
    Wwe12InstallDlc(user_data_root_);
    // Catcheurs ajoutés : les mods de catcheurs deviennent un pack DLC de plus, avant que le jeu liste les siens.
    Wwe12MountSuperstarMods(fs, user_data_root_, rex::filesystem::GetExecutableFolder() / "mods");
    {
      std::string game_device;
      if (fs->FindSymbolicLink("game:", game_device))
        Wwe12MountMenuBackground(fs, game_device);
    }
    // Sauvegardes Xbox 360 choisies dans le launcher (saves_import.txt).
    Wwe12ImportSaves(user_data_root_);
  }

  // Dossier « mods » à côté de l'exe : un fichier posé là au même chemin que sur le disque le
  // remplace (game:\movies\titantron\100.bik -> mods\movies\titantron\100.bik), pour des
  // vidéos HD, des textures, plus tard les fichiers de la mise à jour du jeu.
  void MountModsOverlay(rex::filesystem::VirtualFileSystem* fs) {
    std::string game_device;
    if (!fs->FindSymbolicLink("game:", game_device))
      return;
    const std::filesystem::path mods = rex::filesystem::GetExecutableFolder() / "mods";
    std::error_code ec;
    std::filesystem::create_directories(mods, ec);
    // Arènes ajoutées : leurs fichiers passent avant ceux du dossier mods (wwe12_arena_pages.cpp).
    Wwe12MountArenaSlots(fs, game_device, user_data_root_, mods);
    auto overlay = std::make_unique<rex::filesystem::HostPathDevice>(game_device, mods, true);
    if (!overlay->Initialize() || !fs->RegisterOverlay(game_device, std::move(overlay))) {
      REXLOG_WARN("Mods : impossible d'utiliser {}", rex::path_to_utf8(mods));
      return;
    }
    REXLOG_INFO("Mods : les fichiers de {} remplacent ceux du jeu", rex::path_to_utf8(mods));
    // Le menu principal gagne une 8e entrée QUITTER (wwe12_menu_patch.cpp), servie après les
    // mods : un menu.pac posé dans mods\ garde la priorité.
    Wwe12MountMenuPatch(fs, game_device, user_data_root_);
  }

#if defined(SVR_NATIVE_RENDERER)
  // Le device plume dessine les overlays ImGui au moment du Present.
  std::unique_ptr<rex::ui::ImmediateDrawer> OnCreateImmediateDrawer() override {
    return std::make_unique<bd::gpu::ImGuiOverlayDrawer>();
  }

  void OnPreLaunchModule() override {
    if (!bd::gpu::Video::CreateHostDevice(window())) {
      REXLOG_ERROR("Rendu natif : échec de création du device hôte");
      // Sans device le jeu tournait quand même, fenêtre noire et sans explication (Intel HD 5500 :
      // pas de Vulkan sous Windows). On le dit au joueur, puis on quitte.
      if (Wwe12TryNextBackend()) {
        REXLOG_ERROR("Rendu natif : API indisponible, AUTO passe à l'exe suivant");
      } else {
        Wwe12ShowVulkanError();
        REXLOG_ERROR("Rendu natif : aucune API graphique disponible, arrêt");
      }
      std::_Exit(1);
    }
    // Present tourne sur le fil du jeu et ImGui sur le fil de l'interface : l'overlay est
    // dessiné de façon synchrone sur le fil de l'interface dans l'image présentée.
    bd::gpu::Video::SetOverlayDrawHook([this](plume::RenderCommandList* cmd,
                                              plume::RenderFramebuffer* fb, uint32_t w,
                                              uint32_t h) {
      if (!imgui_drawer() || !imgui_drawer()->HasDialogs())
        return;
      app_context().CallInUIThreadSynchronous([this, cmd, fb, w, h] {
        bd::gpu::ReblueUIDrawContext ctx(w, h, cmd, fb);
        imgui_drawer()->Draw(ctx);
      });
    });
    // OPTIONS > GRAPHICS (wwe12_graphics_menu.cpp): the game's own option pages, saved here.
    Wwe12GraphicsMenuInit(rex::filesystem::GetExecutableFolder() / "wwe12.toml");
    Wwe12JukeboxInit(rex::filesystem::GetExecutableFolder() / "wwe12.toml");  // OPTIONS > JUKEBOX
    Wwe12DisplayInit(imgui_drawer());  // FPS counter (F2), CPU priority
    Wwe12KbdInit(window());  // physical keyboard in the game's letter grids (Create a Story...)
    if (auto* input = static_cast<rex::input::InputSystem*>(runtime()->input_system())) {
      Wwe12ButtonsSetInput(input);
    }
  }

  void OnWindowPixelSizeChanged(uint32_t, uint32_t) override { bd::gpu::Video::RequestResize(); }
#endif

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostSetup() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}

 private:
  std::filesystem::path user_data_root_;
};
