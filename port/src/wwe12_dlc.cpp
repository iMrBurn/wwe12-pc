// Contenus téléchargeables (DLC) : dossier « dlc » à côté de l'exe.
//
// Sur Xbox 360, un DLC est un paquet STFS (en-tête « LIVE », « PIRS » ou « CON ») installé dans
// Content\0000000000000000\<title id>\00000002\. Le runtime sait déjà énumérer et monter ce
// contenu (XamContentCreateEnumerator / XamContentCreate) une fois extrait sous
// Documents\wwe12\0000000000000000\<title id>\00000002\<nom>\, avec son fichier .header.
//
// Ici, au démarrage, chaque paquet posé dans dlc\ (à la racine ou dans des sous-dossiers, par
// exemple la copie d'un dossier Content de la console) est vérifié puis installé une seule fois
// avec ContentManager::InstallContent. Les paquets déjà installés sont ignorés : pour réinstaller,
// supprimer le dossier correspondant dans Documents\wwe12\0000000000000000\.

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

#include <fmt/format.h>
#include <rex/filesystem.h>
#include <rex/filesystem/devices/stfs_container_device.h>
#include <rex/logging/macros.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xam/content_device.h>
#include <rex/system/xam/content_manager.h>

namespace {

using rex::system::XContentType;
using rex::system::xam::XCONTENT_AGGREGATE_DATA;
}  // namespace
using rex::X_RESULT;  // les macros X_ERROR_* s'écrivent (X_RESULT)(n)
namespace {

// Lecture rapide des 4 premiers octets : évite d'analyser les .txt, images, etc.
bool HasStfsMagic(const std::filesystem::path& path) {
  FILE* f = rex::filesystem::OpenFile(path, "rb");
  if (!f)
    return false;
  char magic[4] = {};
  const bool ok = std::fread(magic, 1, sizeof(magic), f) == sizeof(magic);
  std::fclose(f);
  return ok && (std::memcmp(magic, "LIVE", 4) == 0 || std::memcmp(magic, "PIRS", 4) == 0 ||
                std::memcmp(magic, "CON ", 4) == 0);
}

// Même nom que celui qu'InstallContent donnera au paquet (42 caractères au plus).
std::string ContentFileName(const std::filesystem::path& path) {
  XCONTENT_AGGREGATE_DATA data;
  data.set_file_name(rex::path_to_utf8(path.filename()));
  return data.file_name();
}

}  // namespace

void Wwe12InstallDlc(const std::filesystem::path& user_data_root) {
  auto* kernel = REX_KERNEL_STATE();
  auto* content = kernel ? kernel->content_manager() : nullptr;
  if (!content)
    return;

  const std::filesystem::path dlc_dir = rex::filesystem::GetExecutableFolder() / "dlc";
  std::error_code ec;
  std::filesystem::create_directories(dlc_dir, ec);

  const uint32_t title_id = kernel->title_id();
  int installed = 0, present = 0, failed = 0;

  auto it = std::filesystem::recursive_directory_iterator(
      dlc_dir, std::filesystem::directory_options::skip_permission_denied, ec);
  for (; !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
    const auto& path = it->path();
    std::error_code fec;
    if (!it->is_regular_file(fec) || !HasStfsMagic(path))
      continue;

    const auto header = rex::filesystem::StfsContainerDevice::ReadPackageHeader(path);
    if (!header) {
      REXLOG_WARN("DLC : {} illisible (paquet STFS invalide)", rex::path_to_utf8(path));
      ++failed;
      continue;
    }
    const XContentType type = header->metadata.content_type;
    const uint32_t pkg_title = header->metadata.execution_info.title_id;
    if (type != XContentType::kMarketplaceContent) {
      // Mise à jour du jeu (0x000B0000), sauvegarde, profil... : pas un DLC.
      REXLOG_WARN("DLC : {} ignoré (type de contenu {:08X}, pas un DLC)",
                  rex::path_to_utf8(path.filename()), uint32_t(type));
      continue;
    }
    if (pkg_title != title_id) {
      REXLOG_WARN("DLC : {} ignoré (jeu {:08X}, attendu {:08X})",
                  rex::path_to_utf8(path.filename()), pkg_title, title_id);
      continue;
    }

    // Déjà installé : le fichier .header n'est écrit qu'à la fin d'une installation réussie.
    // Comparer à X_ERROR_SUCCESS, pas XSUCCEEDED : les X_ERROR_* sont des codes Win32 (2 pour
    // FILE_NOT_FOUND) que XSUCCEEDED, fait pour les NTSTATUS, prend pour des réussites.
    const std::string name = ContentFileName(path);
    XCONTENT_AGGREGATE_DATA existing;
    if (content->ReadContentHeaderFile(name, 0, title_id, XContentType::kMarketplaceContent,
                                       existing) == X_ERROR_SUCCESS) {
      ++present;
      continue;
    }

    // Paquet copié à la main là où le runtime veut créer son dossier (comme sur la console) :
    // le jeu n'y voit rien (il n'énumère que des dossiers) et l'installation échouerait.
    const auto install_dir = user_data_root / "0000000000000000" / fmt::format("{:08X}", title_id) /
                             "00000002" / name;
    if (std::filesystem::is_regular_file(install_dir, fec)) {
      REXLOG_WARN("DLC : {} est un fichier, pas un dossier installé : supprimez-le (le paquet "
                  "se met dans dlc\\, il est installé au lancement suivant)",
                  rex::path_to_utf8(install_dir));
      ++failed;
      continue;
    }

    const std::u16string display =
        header->metadata.display_name(rex::system::XLanguage::kEnglish);
    REXLOG_INFO("DLC : installation de {} ({})...", rex::path_to_utf8(path.filename()),
                rex::path_to_utf8(std::filesystem::path(display)));
    const X_RESULT result = content->InstallContent(path);
    if (result == X_ERROR_SUCCESS) {
      ++installed;
      continue;
    }
    // Installation interrompue : on retire les fichiers à moitié extraits, sinon le jeu
    // verrait un DLC incomplet (le runtime liste aussi les dossiers sans .header).
    REXLOG_ERROR("DLC : échec de l'installation de {} ({:08X})",
                 rex::path_to_utf8(path.filename()), result);
    XCONTENT_AGGREGATE_DATA partial;
    partial.device_id = static_cast<uint32_t>(rex::system::xam::DummyDeviceId::HDD);
    partial.content_type = XContentType::kMarketplaceContent;
    partial.title_id = title_id;
    partial.xuid = 0;
    partial.set_file_name(name);
    content->DeleteContent(0, partial);
    ++failed;
  }
  if (ec)
    REXLOG_WARN("DLC : lecture de {} impossible ({})", rex::path_to_utf8(dlc_dir), ec.message());

  REXLOG_INFO("DLC : {} installé(s), {} déjà présent(s), {} en échec (dossier {})", installed,
              present, failed, rex::path_to_utf8(dlc_dir));
}
