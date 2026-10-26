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

#include "wwe12_graphics_menu.h"
#include "wwe12_jukebox.h"

REXCVAR_DECLARE(bool, wwe12_quit_entry);
REXCVAR_DECLARE(bool, wwe12_graphics_menu);
REXCVAR_DECLARE(bool, wwe12_jukebox_menu);

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

// OPTIONS > GRAPHICS in the game's menus (wwe12_graphics_menu, wwe12_graphics_menu.cpp): the
// IN-GAME MANUAL record (label 41184, kind 1 = a sub-menu; its entries are the records whose parent,
// +0x50, is its id, +0x34: TUTORIAL, ABILITIES, ATTRIBUTES and a hidden fourth, kind 7) keeps its
// sub-menu, whose three visible entries become DISPLAY / GRAPHICS / ADVANCED (labels
// kWwe12GraphicsNativeTabText + n), each linked to 1304 (the game's IN-GAME OPTIONS page, whose
// script is then replaced). Tested 2026-10-20: +0x30 is not the parent (it matched other records).
bool PatchGraphicsPages(std::vector<uint8_t> &d, size_t base) {
  const uint32_t count = Be32(&d[base + 4]);
  const size_t first = base + 0x10;
  if (first + size_t(count) * kRecordSize > d.size())
    return false;
  uint32_t manual = 0xFFFFFFFFu;
  for (uint32_t k = 0; k < count; ++k) {
    const uint8_t *r = &d[first + size_t(k) * kRecordSize];
    if (Be32(r + 0x18) == 41184 && Be32(r + 0x54) == 1)
      manual = Be32(r + 0x34);
  }
  if (manual == 0xFFFFFFFFu)
    return false;
  uint32_t n = 0;
  for (uint32_t k = 0; k < count && n < 3; ++k) {
    uint8_t *r = &d[first + size_t(k) * kRecordSize];
    if (Be32(r + 0x50) != manual || Be32(r + 0x54) != 5)
      continue;
    SetBe32(r + 0x18, kWwe12GraphicsNativeTabText + n);
    SetBe32(r + 0x6C, 1304);
    SetBe32(r + 0x70, 2);
    ++n;
  }
  return n == 3;
}

// OPTIONS > JUKEBOX (wwe12_jukebox_menu, wwe12_jukebox.cpp): the OPTIONS menu's CREDITS record
// (label 41039, kind 5 = a screen, link 1315) becomes JUKEBOX (label kWwe12JukeboxText), linked to
// 1304 like the GRAPHICS pages: the game's IN-GAME OPTIONS page, whose script is then replaced.
bool PatchJukeboxEntry(std::vector<uint8_t> &d, size_t base) {
  const uint32_t count = Be32(&d[base + 4]);
  const size_t first = base + 0x10;
  if (first + size_t(count) * kRecordSize > d.size())
    return false;
  for (uint32_t k = 0; k < count; ++k) {
    uint8_t *r = &d[first + size_t(k) * kRecordSize];
    if (Be32(r + 0x18) != 41039 || Be32(r + 0x54) != 5)
      continue;
    SetBe32(r + 0x18, kWwe12JukeboxText);
    SetBe32(r + 0x6C, 1304);
    SetBe32(r + 0x70, 2);
    return true;
  }
  return false;
}

// Relabels the main menu's SHOP record of the table whose header is at 'base'.
bool RelabelShop(std::vector<uint8_t> &d, size_t base) {
  const uint32_t count = Be32(&d[base + 4]);
  const size_t first = base + 0x10;
  bool found = false;
  if (first + size_t(count) * kRecordSize > d.size())
    return false;
  for (uint32_t k = 0; k < count; ++k) {
    uint8_t *r = &d[first + size_t(k) * kRecordSize];
    if (REXCVAR_GET(wwe12_quit_entry) && Be32(r + 0x30) == 1 && Be32(r + 0x18) == kShopStringId) {
      SetBe32(r + 0x18, kExitStringId);
      found = true;
    }
  }
  return found;
}

}  // namespace

// Called once the game device and the mods overlay are mounted.
void Wwe12MountMenuPatch(rex::filesystem::VirtualFileSystem *fs, const std::string &game_device,
                         const std::filesystem::path &user_root) {
  const bool native_graphics = REXCVAR_GET(wwe12_graphics_menu);
  const bool jukebox = REXCVAR_GET(wwe12_jukebox_menu);
  if ((!REXCVAR_GET(wwe12_quit_entry) && !native_graphics && !jukebox) || user_root.empty())
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
    if (native_graphics && PatchGraphicsPages(d, p)) {
      ++patched;
      REXLOG_INFO("Menu : OPTIONS > GRAPHICS dans les menus du jeu (table à 0x{:X})", p);
    }
    if (jukebox && PatchJukeboxEntry(d, p)) {
      ++patched;
      REXLOG_INFO("Menu : OPTIONS > CREDITS devient JUKEBOX (table à 0x{:X})", p);
    }
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
      REXLOG_WARN("Menu : impossible d'écrire {}", rex::path_to_utf8(dir / "pac" / "menu" / "menu.pac"));
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
