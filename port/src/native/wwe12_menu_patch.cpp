// WWE '12: the main menu's SHOP entry (the Xbox LIVE marketplace, dead today) becomes "EXIT"
// (string 45061, translated by the game into its five languages); choosing it closes the game
// (decide hook in wwe12_quit.cpp). Xbox LIVE keeps its own entry, labelled Online.
//
// The menu table lives raw in pac/menu/menu.pac, in two copies (two layouts of the same menus):
// a 16-byte header {1, record count, records kept in memory, ...}, then 0x74-byte big-endian
// records, the first being a root. Fields: +0x18 label string id, +0x30 page (1 = main menu),
// +0x34 record id. Only the label of SHOP's record changes.
//
// A real 8th entry was tried: appended to a table, the game loads it (once the kept-records count
// grows too) but numbers its list position by runs of same-page records, and the main menu
// screen lists its own entries anyway; records cannot move either (menus link by position:
// inserting one after SHOP broke the way back from sub-menus). Left for later.
//
// The patched file is built at startup from the player's own menu.pac into
// Documents\wwe12\patched and served over the original through the VFS overlay (never shipped).

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <vector>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/filesystem/devices/host_path_device.h>
#include <rex/filesystem/entry.h>
#include <rex/filesystem/file.h>
#include <rex/filesystem/vfs.h>
#include <rex/logging/macros.h>

REXCVAR_DECLARE(bool, wwe12_quit_entry);

namespace {

constexpr uint32_t kRecordSize = 0x74;
constexpr uint32_t kExitStringId = 45061;
constexpr uint32_t kShopStringId = 41035;

uint32_t Be32(const uint8_t *p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}
void SetBe32(uint8_t *p, uint32_t v) {
  p[0] = uint8_t(v >> 24);
  p[1] = uint8_t(v >> 16);
  p[2] = uint8_t(v >> 8);
  p[3] = uint8_t(v);
}

bool ReadGameFile(rex::filesystem::VirtualFileSystem *fs, const char *path,
                  std::vector<uint8_t> &out) {
  rex::filesystem::Entry *entry = fs->ResolvePath(path);
  if (!entry)
    return false;
  rex::filesystem::File *file = nullptr;
  if (entry->Open(rex::filesystem::FileAccess::kFileReadData, &file) != 0 || !file)
    return false;
  out.resize(entry->size());
  size_t read = 0;
  const bool ok = file->ReadSync(std::span<uint8_t>(out.data(), out.size()), 0, &read) == 0 &&
                  read == out.size();
  file->Destroy();
  return ok;
}

// Relabels the main menu's SHOP record of the table whose header is at 'base'.
bool RelabelShop(std::vector<uint8_t> &d, size_t base) {
  const uint32_t count = Be32(&d[base + 4]);
  const size_t first = base + 0x10;
  if (first + size_t(count) * kRecordSize > d.size())
    return false;
  for (uint32_t k = 0; k < count; ++k) {
    uint8_t *r = &d[first + size_t(k) * kRecordSize];
    if (Be32(r + 0x30) == 1 && Be32(r + 0x18) == kShopStringId) {
      SetBe32(r + 0x18, kExitStringId);
      return true;
    }
  }
  return false;
}

}  // namespace

// Called once the game device and the mods overlay are mounted.
void Wwe12MountMenuPatch(rex::filesystem::VirtualFileSystem *fs, const std::string &game_device,
                         const std::filesystem::path &user_root) {
  if (!REXCVAR_GET(wwe12_quit_entry) || user_root.empty())
    return;
  std::vector<uint8_t> d;
  if (!ReadGameFile(fs, "game:\\pac\\menu\\menu.pac", d)) {
    REXLOG_WARN("Menu : pac\\menu\\menu.pac illisible, pas d'entrée QUITTER");
    return;
  }
  // Each table starts with the header {1, count} followed by the root and the PLAY record
  // (page 1, id 1, label 41001).
  int patched = 0;
  for (size_t p = 0; p + 0x10 + 2 * kRecordSize <= d.size(); p += 4) {
    if (Be32(&d[p]) != 1)
      continue;
    const uint8_t *play = &d[p + 0x10 + kRecordSize];
    if (Be32(play + 0x18) != 41001 || Be32(play + 0x30) != 1 || Be32(play + 0x34) != 1)
      continue;
    const uint32_t count = Be32(&d[p + 4]);
    if (count < 8 || count > 4096)
      continue;
    if (RelabelShop(d, p))
      ++patched;
    p += size_t(count) * kRecordSize;
  }
  if (!patched) {
    REXLOG_WARN("Menu : entrée SHOP introuvable dans menu.pac, pas d'entrée QUITTER");
    return;
  }
  const std::filesystem::path dir = user_root / "patched";
  std::error_code ec;
  std::filesystem::create_directories(dir / "pac" / "menu", ec);
  {
    std::ofstream f(dir / "pac" / "menu" / "menu.pac", std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char *>(d.data()), std::streamsize(d.size()));
    if (!f) {
      REXLOG_WARN("Menu : impossible d'écrire {}", (dir / "pac" / "menu" / "menu.pac").string());
      return;
    }
  }
  auto overlay = std::make_unique<rex::filesystem::HostPathDevice>(game_device, dir, true);
  if (!overlay->Initialize() || !fs->RegisterOverlay(game_device, std::move(overlay))) {
    REXLOG_WARN("Menu : impossible de servir le menu.pac modifié");
    return;
  }
  REXLOG_INFO("Menu : SHOP devient QUITTER ({} table(s))", patched);
}
