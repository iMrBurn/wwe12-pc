// Android: check the game's files right after the disc is mounted (issue #41).
//
// A player had default.xex readable (the log said "Disque : media ID ... identique") but not
// pac\_startup.pac: the game then ran behind a black screen and nothing in the log said why. Here we
// log what was mounted, stop with a message the launcher shows when the game's files are not
// readable, and write the same kind of message when the graphics device cannot be created.

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include <rex/cvar.h>
#include <rex/filesystem/vfs.h>
#include <rex/logging/macros.h>

REXCVAR_DECLARE(std::string, game_data_root);

// Read by LauncherActivity.onResume (it shows the text, then deletes the file).
void Wwe12AndroidWriteError(const std::string &message) {
  const char *dir = std::getenv("REX_APP_DIR");
  if (!dir || !*dir)
    return;
  std::ofstream out(std::filesystem::path(dir) / "last_error.txt", std::ios::trunc);
  out << message;
}

namespace {

// Size of a file of the mounted disc, or -1 when it can't be found.
long long FileSize(rex::filesystem::VirtualFileSystem *fs, const char *path) {
  rex::filesystem::Entry *entry = fs->ResolvePath(path);
  return entry ? static_cast<long long>(entry->size()) : -1;
}

// First entries of the disc's root, for the log (names and case tell what the player has).
std::string RootListing() {
  std::string text;
  std::error_code ec;
  const std::filesystem::path root(REXCVAR_GET(game_data_root));
  if (!std::filesystem::is_directory(root, ec))
    return text;
  int count = 0;
  for (const auto &e : std::filesystem::directory_iterator(root, ec)) {
    if (count++ == 25) {
      text += " ...";
      break;
    }
    text += " " + e.path().filename().string() + (e.is_directory(ec) ? "/" : "");
  }
  return text;
}

}  // namespace

// Called once the game device is mounted. Quits with a message when the game can't read its files.
void Wwe12AndroidCheckGameFiles(rex::filesystem::VirtualFileSystem *fs) {
  const std::string root = REXCVAR_GET(game_data_root);
  std::error_code ec;
  const bool folder = std::filesystem::is_directory(root, ec);
  const long long xex = FileSize(fs, "game:\\default.xex");
  const long long startup = FileSize(fs, "game:\\pac\\_startup.pac");
  const long long menu = FileSize(fs, "game:\\pac\\menu\\menu.pac");
  REXLOG_INFO("Jeu : source {} ({}), default.xex {} octets, pac\\_startup.pac {} octets, "
              "pac\\menu\\menu.pac {} octets",
              root, folder ? "dossier" : "image ISO", xex, startup, menu);
  if (startup >= 0)
    return;
  REXLOG_ERROR("Jeu : pac\\_startup.pac introuvable, le jeu ne peut pas démarrer. Racine du dossier :{}",
               RootListing());
  Wwe12AndroidWriteError(
      folder
          ? "The game's files are incomplete: pac\\_startup.pac was not found in the game folder.\n\n"
            "The folder Android/data/io.github.imrburn.wwe12/files/game/ must hold the whole "
            "extracted disc (default.xex AND the pac folder), or just your ISO. Copy the missing "
            "files, or empty the folder and choose your ISO with CHOOSE MY ISO."
          : "The game could not read its files from your ISO: pac\\_startup.pac was not found in it.\n\n"
            "The ISO may be incomplete or damaged, or be another game. Use an ISO dumped from your "
            "own WWE '12 disc (Xbox 360), then choose it again with CHOOSE MY ISO.");
  std::_Exit(1);
}
