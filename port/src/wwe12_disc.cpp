// Version du disque du joueur.
//
// Le code de wwe12.exe a été recompilé à partir d'un disque en version 4 (media ID 712076A5, une
// réédition qui contient déjà la dernière Title Update), alors que le jeu lit les données (pac,
// vidéos, sons) et l'image xex sur le disque du joueur. Avec une autre version, code et données ne
// vont plus ensemble : des catcheurs DLC manquent (Batista avec le disque d'origine), d'autres bugs
// sont possibles. Au démarrage, on écrit la version du disque dans le journal, et on prévient le
// joueur une fois si elle diffère (wwe12_disc_check = false pour ne plus être prévenu).

#include <filesystem>
#include <fstream>
#include <string>

#include <fmt/format.h>
#include <rex/cvar.h>
#include <rex/logging/macros.h>
#include <rex/system.h>
#include <rex/system/kernel_state.h>
#include <rex/system/user_module.h>

REXCVAR_DEFINE_BOOL(wwe12_disc_check, true, "WWE12",
                    "Warn when the game disc is not the version the port was built from.");

namespace {

constexpr uint32_t kBuiltMediaId = 0x712076A5;
constexpr uint32_t kBuiltVersion = 0x00000004;  // 0.0.0.4

std::string VersionText(uint32_t v) {
  return fmt::format("{}.{}.{}.{}", v >> 28, (v >> 24) & 0xF, (v >> 8) & 0xFFFF, v & 0xFF);
}

}  // namespace

void Wwe12CheckDiscVersion(const std::filesystem::path& user_data_root) {
  auto* kernel = REX_KERNEL_STATE();
  if (!kernel)
    return;
  auto module = kernel->GetExecutableModule();
  const auto* xex = module ? module->xex_module() : nullptr;
  const auto* info = xex ? xex->opt_execution_info() : nullptr;
  if (!info) {
    REXLOG_WARN("Disque : version inconnue (pas d'en-tête d'exécution)");
    return;
  }
  const uint32_t media = info->media_id;
  const uint32_t version = info->version_value;
  const bool same = media == kBuiltMediaId && version == kBuiltVersion;
  REXLOG_INFO("Disque : media ID {:08X}, version {} (base {}) : {}", media, VersionText(version),
              VersionText(info->base_version_value),
              same ? "identique à celui du portage"
                   : fmt::format("DIFFÉRENT du portage ({:08X}, version {})", kBuiltMediaId,
                                 VersionText(kBuiltVersion)));
  if (same || !REXCVAR_GET(wwe12_disc_check))
    return;

  // Une seule fois par disque : un fichier mémorise le dernier disque signalé.
  const auto marker = user_data_root / "disc_warned.txt";
  const std::string id = fmt::format("{:08X} {:08X}", media, version);
  {
    std::ifstream in(marker);
    std::string seen;
    if (in && std::getline(in, seen) && seen == id)
      return;
  }
  std::ofstream(marker) << id << '\n';
  rex::ShowSimpleMessageBox(
      rex::SimpleMessageBoxType::Warning,
      fmt::format(
          "Your WWE '12 disc is version {} (media ID {:08X}). This port was made from version {} "
          "(media ID {:08X}), which already includes the last Title Update.\n\n"
          "The game will run, but some DLC wrestlers (Batista) may be missing and other bugs may "
          "happen. Support for other disc versions is planned.\n\n"
          "Votre disque de WWE '12 est en version {} (media ID {:08X}). Ce portage a été fait à "
          "partir de la version {} (media ID {:08X}), qui contient déjà la dernière mise à jour. "
          "Le jeu fonctionne, mais certains catcheurs DLC (Batista) peuvent manquer et d'autres "
          "bugs sont possibles. La prise en charge des autres versions est prévue.",
          VersionText(version), media, VersionText(kBuiltVersion), kBuiltMediaId,
          VersionText(version), media, VersionText(kBuiltVersion), kBuiltMediaId));
}
