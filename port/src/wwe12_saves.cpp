// Import de sauvegardes Xbox 360 (paquets STFS de type « sauvegarde », jeu 54510890).
//
// Le launcher (page SAUVEGARDES) vérifie ce que le joueur a choisi (un fichier, ou un dossier
// copié depuis la console : NNCreateSuperStar.cas, NNEncodeDat.enc, SaveData.dat...), fait une copie
// de sécurité de la sauvegarde actuelle, puis écrit la liste des chemins dans saves_import.txt à
// côté de l'exe (un chemin par ligne, fichier ou dossier). Ici, au démarrage, chaque paquet de la
// liste est extrait dans Documents\wwe12\<profil>\<titre>\00000001\<nom du paquet>\ avec son
// fichier .header, comme le fait le jeu pour SaveData.dat : un paquet du même nom est remplacé,
// les autres sauvegardes restent. La liste est ensuite supprimée (pas de ré-import à chaque
// lancement) et un résumé est écrit dans saves_import_result.txt, que le launcher affiche.
//
// Les signatures des paquets ne sont pas vérifiées : inutile de les « re-signer » comme sur console.

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <rex/filesystem.h>
#include <rex/filesystem/devices/stfs_container_device.h>
#include <rex/logging/macros.h>
#include <rex/string.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xam/content_device.h>
#include <rex/system/xam/content_manager.h>
#include <rex/system/xam/user_profile.h>

namespace {

using rex::system::XContentType;
using rex::system::xam::XCONTENT_AGGREGATE_DATA;
}  // namespace
using rex::X_RESULT;  // les macros X_ERROR_* / X_STATUS_* s'écrivent avec ces noms
using rex::X_STATUS;
namespace {
namespace fs = std::filesystem;

bool HasStfsMagic(const fs::path& path) {
  FILE* f = rex::filesystem::OpenFile(path, "rb");
  if (!f)
    return false;
  char magic[4] = {};
  const bool ok = std::fread(magic, 1, sizeof(magic), f) == sizeof(magic);
  std::fclose(f);
  return ok && (std::memcmp(magic, "LIVE", 4) == 0 || std::memcmp(magic, "PIRS", 4) == 0 ||
                std::memcmp(magic, "CON ", 4) == 0);
}

// Copie un fichier ou un dossier du paquet vers le disque (même boucle que ContentManager, dont
// la version est privée au SDK).
bool ExtractEntry(rex::filesystem::Entry* entry, const fs::path& base_path) {
  const auto dest = base_path / rex::to_path(rex::string::utf8_fix_path_separators(entry->path()));
  std::error_code ec;
  if (entry->attributes() & rex::filesystem::kFileAttributeDirectory) {
    fs::create_directories(dest, ec);
    return !ec;
  }
  fs::create_directories(dest.parent_path(), ec);
  rex::filesystem::File* in_file = nullptr;
  if (entry->Open(rex::filesystem::FileAccess::kFileReadData, &in_file) != X_STATUS_SUCCESS)
    return false;
  auto out_file = rex::filesystem::OpenFile(dest, "wb");
  if (!out_file) {
    in_file->Destroy();
    return false;
  }
  constexpr size_t kBufferSize = 4 * 1024 * 1024;
  auto buffer = std::make_unique<uint8_t[]>(kBufferSize);
  size_t remaining = entry->size();
  size_t offset = 0;
  bool ok = true;
  while (remaining > 0) {
    size_t bytes_read = 0;
    const size_t to_read = std::min(remaining, kBufferSize);
    in_file->ReadSync(std::span<uint8_t>(buffer.get(), to_read), offset, &bytes_read);
    if (bytes_read == 0) {
      ok = false;  // paquet tronqué
      break;
    }
    if (std::fwrite(buffer.get(), 1, bytes_read, out_file) != bytes_read) {
      ok = false;  // disque plein
      break;
    }
    offset += bytes_read;
    remaining -= bytes_read;
  }
  std::fclose(out_file);
  in_file->Destroy();
  return ok;
}

// Installe un paquet. Le dossier d'un paquet du même nom est d'abord supprimé.
bool InstallSave(rex::system::KernelState* kernel, rex::system::xam::ContentManager* content,
                 const fs::path& user_data_root, const fs::path& package,
                 const rex::filesystem::StfsHeader& header) {
  const uint64_t xuid = kernel->user_profile()->xuid();
  const uint32_t title_id = kernel->title_id();

  XCONTENT_AGGREGATE_DATA data;
  data.device_id = static_cast<uint32_t>(rex::system::xam::DummyDeviceId::HDD);
  data.content_type = XContentType::kSavedGame;
  data.title_id = title_id;
  data.xuid = xuid;
  data.set_file_name(rex::path_to_utf8(package.filename()));
  const std::u16string display = header.metadata.display_name(rex::system::XLanguage::kEnglish);
  data.set_display_name(display.empty() ? rex::path_to_utf16(package.filename()) : display);

  const fs::path install_dir = user_data_root / fmt::format("{:016X}", xuid) /
                               fmt::format("{:08X}", title_id) / "00000001" / data.file_name();
  // Le nouveau paquet doit s'ouvrir avant de supprimer l'ancien du même nom.
  auto device = std::make_unique<rex::filesystem::StfsContainerDevice>("", package);
  if (!device->Initialize())
    return false;
  content->DeleteContent(xuid, data);  // ancien paquet du même nom (dossier + .header), s'il existe
  std::error_code ec;
  fs::create_directories(install_dir, ec);
  auto* root = device->ResolvePath("");
  bool ok = !ec && root;
  std::queue<rex::filesystem::Entry*> queue;
  if (ok)
    queue.push(root);
  while (ok && !queue.empty()) {
    auto* entry = queue.front();
    queue.pop();
    for (auto& child : entry->children())
      queue.push(child.get());
    ok = ExtractEntry(entry, install_dir);
  }
  // Le .header n'est écrit qu'à la fin : sans lui le paquet n'existe pas pour le jeu.
  if (ok)
    ok = content->WriteContentHeaderFile(xuid, data, 0) == X_ERROR_SUCCESS;
  if (!ok)
    content->DeleteContent(xuid, data);  // pas de paquet à moitié extrait
  return ok;
}

void CollectPackages(const fs::path& path, std::vector<fs::path>& out) {
  std::error_code ec;
  if (fs::is_regular_file(path, ec)) {
    out.push_back(path);
  } else if (fs::is_directory(path, ec)) {
    for (auto it = fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied, ec);
         !ec && it != fs::recursive_directory_iterator(); it.increment(ec))
      if (it->is_regular_file(ec))
        out.push_back(it->path());
  } else {
    REXLOG_WARN("Import de sauvegarde : {} introuvable", rex::path_to_utf8(path));
  }
}

}  // namespace

void Wwe12ImportSaves(const fs::path& user_data_root) {
  auto* kernel = REX_KERNEL_STATE();
  auto* content = kernel ? kernel->content_manager() : nullptr;
  if (!content)
    return;
  const fs::path exe_dir = rex::filesystem::GetExecutableFolder();
  const fs::path list_file = exe_dir / "saves_import.txt";
  std::error_code ec;
  if (!fs::exists(list_file, ec))
    return;

  std::vector<fs::path> files;
  {
    std::ifstream list(list_file, std::ios::binary);
    std::string line;
    while (std::getline(list, line)) {
      while (!line.empty() && (line.back() == '\r' || line.back() == ' '))
        line.pop_back();
      if (!line.empty())
        CollectPackages(fs::path(std::u8string(line.begin(), line.end())), files);
    }
  }
  // Retirée tout de suite : un plantage ne doit pas relancer l'import à chaque démarrage.
  fs::remove(list_file, ec);

  int installed = 0, skipped = 0, failed = 0;
  for (const auto& path : files) {
    if (!HasStfsMagic(path))
      continue;  // ni paquet ni sauvegarde (readme, mp3...) : pas compté
    const auto header = rex::filesystem::StfsContainerDevice::ReadPackageHeader(path);
    if (!header) {
      REXLOG_WARN("Sauvegarde : {} illisible (paquet STFS invalide)", rex::path_to_utf8(path));
      ++failed;
      continue;
    }
    const XContentType type = header->metadata.content_type;
    const uint32_t pkg_title = header->metadata.execution_info.title_id;
    if (type != XContentType::kSavedGame || pkg_title != kernel->title_id()) {
      REXLOG_WARN("Sauvegarde : {} ignoré (type {:08X}, jeu {:08X})",
                  rex::path_to_utf8(path.filename()), uint32_t(type), pkg_title);
      ++skipped;
      continue;
    }
    REXLOG_INFO("Sauvegarde : installation de {}...", rex::path_to_utf8(path.filename()));
    if (InstallSave(kernel, content, user_data_root, path, *header)) {
      ++installed;
    } else {
      REXLOG_ERROR("Sauvegarde : échec de l'installation de {}", rex::path_to_utf8(path.filename()));
      ++failed;
    }
  }
  REXLOG_INFO("Sauvegardes : {} installée(s), {} ignorée(s), {} en échec", installed, skipped, failed);
  std::ofstream(exe_dir / "saves_import_result.txt", std::ios::trunc)
      << "installed=" << installed << "\nskipped=" << skipped << "\nfailed=" << failed << "\n";
}
