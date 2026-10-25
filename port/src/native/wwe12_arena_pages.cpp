// WWE '12: added arenas, on more pages of the arena select screen (docs/mod_maker.md, "Nouveaux emplacements
// d'arènes").
//
// The screen. The arena select screen of the match setup (title ARENAS, 21 banners: four big ones on top) is a
// native class: constructor sub_825A67A0(this, arena list, mode, current stage, ?), 12,272 bytes. Its tiles come
// from a data block at 0x82028560 (in the image's read-only section: its host page is made writable first):
//   +0    22 x {u32 stage, u32 flag}: stages 0..20, then 300 (random);
//   +176  a 4 x 6 grid of cells {record pointer, up/down/left/right}: which record each tile shows;
//   +368  21 x {stage, banner texture name "arenaNN"}: sub_825A3CD8 finds each tile's banner by stage.
// A tile is available when sub_825A3318(this, stage) says so (in the screen's list, unlocked). The chosen arena is
// read through sub_825A56C8 (grid cell -> record -> stage); the match setup then stores it (u16 at
// matchSettings+898, sub_826CBFD0). Tabs (this+3260: arenas, brawl areas, ...) change with LB / RB (widget vtable
// 0x820287E0 slots 10 / 13 -> sub_825A5C88(this, 0 = previous / 1 = next)), which only stops on tabs whose flag is
// set: in an exhibition only the arena tab is, and the widget then doesn't even pass LB / RB on. So the pages read
// the pad themselves while the screen draws (sub_825A6A98, every frame): XInputGetState (sub_82A8C070, the
// keyboard's bindings included), LB 0x0100 / RB 0x0200, on the press. (A look-alike class, sub_826361A8 with its
// table at 0x83071ED0, is not this screen.)
//
// Added arenas. An installed mod (mods\.svrmods\Arenas\<id>, not disabled) whose manifest says slot=new and
// base=NN, with arena\arena.pac (a whole bgNN.pac) and arena\banner.dds (256 x 128 DXT5, no mips). Each one plays
// under its base arena's stage number (its lights, VS screen and memory room). The grid holds 21 tiles, so they go
// on pages after the game's: on those pages the records hold made-up stage numbers (kFirstAdded + n), each with its
// banner, and sub_825A56C8 gives the base's number back for them.
// Files. The game finds a file's data through its directory (plist360.arc): sub_827F29F0(directory, "/GRP/NAME",
// out) fills out with the file index (u16 at +12), where the data starts (+0, in 2 KB sectors) and its size (+8,
// in 256-byte units). At startup (Wwe12MountArenaSlots, before the mods overlay so it comes first),
// Documents\wwe12\arena_slots gets:
//  - MatchHD.pac with the banner pack (MLYE/MUPA, PACH entry 7001: 71 DDS) rebuilt and appended, the added arenas'
//    banners in its 35 unused CAR_dummyNN textures (all 256 x 128, all the same picture); the directory answers
//    /MLYE/MUPA with the appended pack;
//  - for each base arena, its bgNN.pac (the one in mods\ if any), padded to the largest of it and its added
//    arenas. Choosing an arena writes the whole file of the one that plays there into it (Serve): the arena loads
//    from a normal arena file. (Appending an added arena's data to the base's file and pointing the directory at
//    it loaded, then crashed on the arena's textures: the game reads more of the file than the directory says.)
//    The directory answers /STG/00NN with the added arena's own start and size;
//  - an added arena's own VS screen ("match card", ported from WWE '13 by the Mod Maker: arena\card_mb.pac and
//    arena\card_mf.pac, MatchHD.pac's MLYE/MBnn = layout and pictures and MENU/MnnF = its font, nn = the stage, as
//    stored) appended to that MatchHD.pac too; while it is the chosen arena, the directory answers its base's
//    /MLYE/MBnn and /MENU/MnnF with them;
//  - an added arena's own entrance graphics (arena\etex_hd.pac: its ETEX/00NN group of pac\evt\evtgdhd.pac, as its
//    game stores it: pictures, the 2D animations of the location plate and the name plates, their fonts; the same
//    layout in WWE '12, WWE '13 and SvR 2011) appended to a served evtgdhd.pac; while it is the chosen arena, the
//    directory answers its base's /ETEX/00NN with it. (A whole evtgdhd.pac rebuilt in mods crashed: the directory
//    keeps every entry's place, and they had all moved.)
//  - an added arena's own pre-match screen (arena\load_hd.pac: its LOAD/90NN of pac\menu\LoadHD.pac, as its game
//    stores it: the VS screen and its animation before the match; the same in WWE '12 and WWE '13) appended to a
//    served LoadHD.pac; while it is the chosen arena, the directory answers its base's /LOAD/90NN with it;
//  - an added arena's own transition video (arena\transition.bik: its MOVT/M0NN of pac\evt\evtmov.pac, Bink 1 at
//    640 x 640 in WWE '12 and WWE '13): evtmov.pac (273 MB) is too big to copy at every start, so it goes at the end
//    of the served MatchHD.pac, and the directory answers the base's /MOVT/M0NN with that file's number (+12) and
//    the video's place.
//
// Test: wwe12_arena_pages_test = true adds one page, the 21 arenas in reverse order, and logs the lookups.

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <span>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/filesystem/devices/host_path_device.h>
#include <rex/filesystem/entry.h>
#include <rex/filesystem/file.h>
#include <rex/filesystem/vfs.h>
#include <rex/runtime.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

#include "../modmaker/bpe_like.h"
#include "../modmaker/svrfmt/archive.h"
#include "../modmaker/svrfmt/texture.h"

REXCVAR_DEFINE_BOOL(wwe12_arena_pages, true, "WWE12",
                    "Added arenas (arena mods with slot=new) on more pages of the arena select screen (LB / RB).");
REXCVAR_DEFINE_BOOL(wwe12_arena_pages_test, false, "WWE12",
                    "Test: a second page on the arena select screen (the 21 arenas in reverse order).");

namespace {

namespace fs = std::filesystem;

constexpr u32 kRecords = 0x82028560;      // 22 x {stage, flag}; the first 21 are the tiles
constexpr u32 kBanners = kRecords + 368;  // 21 x {stage, banner name}
constexpr int kTiles = 21;
constexpr u32 kFirstAdded = 150;  // made-up stage numbers of the added arenas' tiles
constexpr u32 kEmptyTile = 149;   // a page's tiles past its last arena (not available)
constexpr u16 kMatchHdIndex = 234;  // pac\menu\MatchHD.pac in the directory (its line in plist360.h)
constexpr u16 kEvtGdHdIndex = 190;  // pac\evt\evtgdhd.pac
constexpr u16 kLoadHdIndex = 233;   // pac\menu\LoadHD.pac
constexpr u16 kEvtMovIndex = 192;   // pac\evt\evtmov.pac

struct Added {
  std::string id, name;
  u32 base = 0;        // the game's arena it plays as
  fs::path file;       // its arena.pac
  u32 sector = 0;      // its STG data: start (2 KB sectors) and size (256 bytes), from its own STG record
  u32 units = 0;
  std::string banner;  // the CAR_dummyNN texture holding its banner ("" = none)
  u32 banner_name = 0;  // that name in guest memory
  std::vector<uint8_t> card_mb, card_mf;  // its own VS screen (until appended), else empty
  u32 mb_sector = 0, mb_units = 0, mf_sector = 0, mf_units = 0;  // ... appended to MatchHD.pac (0: the base's)
  std::vector<uint8_t> etex;  // its own entrance graphics (until appended), else empty
  u32 etex_sector = 0, etex_units = 0;  // ... appended to evtgdhd.pac (0: the base's)
  std::vector<uint8_t> load;  // its own pre-match screen (until appended), else empty
  u32 load_sector = 0, load_units = 0;  // ... appended to LoadHD.pac (0: the base's)
  std::vector<uint8_t> movie;  // its own transition video (until appended), else empty
  u32 movie_sector = 0, movie_units = 0;  // ... appended to MatchHD.pac (0: the base's)
};
std::vector<Added> g_added;
// A base arena's file as served (Documents\wwe12\arena_slots\pac\bg\bgNN.pac): the whole file of the arena
// that plays there now, the game's or an added one, padded to a fixed size (the largest of them).
struct Served {
  fs::path path, original;  // served file, the game's own copy
  size_t size = 0;
  int current = -1;         // the added arena in it, -1: the game's
};
std::map<u32, Served> g_served;
u32 g_mupa_sector = 0, g_mupa_units = 0;  // the rebuilt banner pack, appended to MatchHD.pac
int g_chosen = -1;                        // the added arena chosen last (its index), -1: one of the game's

struct Tile {
  u32 stage, banner;  // banner: guest address of the texture name (0: the game's table is kept)
};
std::array<u32, kTiles> g_original{};
std::array<u32, kTiles * 2> g_original_banners{};
bool g_saved = false;
std::vector<std::array<Tile, kTiles>> g_pages;  // page 0 = the game's
int g_page = 0;

// ---- startup: the served files

std::string Value(const std::string &text, const char *key) {
  const std::string k = std::string(key) + "=";
  std::string found;
  size_t pos = 0;
  while (pos < text.size()) {
    size_t end = text.find('\n', pos);
    if (end == std::string::npos)
      end = text.size();
    std::string line = text.substr(pos, end - pos);
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    if (line.compare(0, k.size(), k) == 0)
      found = line.substr(k.size());
    pos = end + 1;
  }
  return found;
}

bool ReadHost(const fs::path &p, std::vector<uint8_t> &out) {
  std::ifstream f(p, std::ios::binary);
  if (!f)
    return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return true;
}

bool WriteHost(const fs::path &p, const std::vector<uint8_t> &data) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  f.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));
  return bool(f);
}

bool ReadGameFile(rex::filesystem::VirtualFileSystem *vfs, const std::string &path, std::vector<uint8_t> &out) {
  rex::filesystem::Entry *entry = vfs->ResolvePath(path);
  if (!entry)
    return false;
  rex::filesystem::File *file = nullptr;
  if (entry->Open(rex::filesystem::FileAccess::kFileReadData, &file) != 0 || !file)
    return false;
  out.resize(entry->size());
  size_t read = 0;
  const bool ok = file->ReadSync(std::span<uint8_t>(out.data(), out.size()), 0, &read) == 0 && read == out.size();
  file->Destroy();
  return ok;
}

// A disc file as the game would get it without us: the mods folder's copy, else the disc's.
bool ReadForGame(rex::filesystem::VirtualFileSystem *vfs, const fs::path &mods, const std::string &rel,
                 std::vector<uint8_t> &out) {
  if (ReadHost(mods / fs::path(rel), out))
    return true;
  std::string path = "game:\\" + rel;
  for (auto &c : path)
    if (c == '/')
      c = '\\';
  return ReadGameFile(vfs, path, out);
}

// Appends data on a 2 KB boundary; its place (sector) and size (256-byte units).
void Append(std::vector<uint8_t> &file, const uint8_t *data, size_t size, u32 &sector, u32 &units) {
  file.resize((file.size() + 0x7FF) & ~size_t(0x7FF), 0);
  sector = u32(file.size() / 0x800);
  file.insert(file.end(), data, data + size);
  file.resize((file.size() + 0x7FF) & ~size_t(0x7FF), 0);
  units = u32((size + 0xFF) / 0x100);
}

// The banner pack (MatchHD.pac MLYE/MUPA, PACH entry 7001) with the added arenas' banners in CAR_dummyNN, appended
// to MatchHD.pac. The textures keep their size, so the pack unpacks to the same size as the game's.
bool BuildBanners(std::vector<uint8_t> &matchhd, const std::vector<std::vector<uint8_t>> &banners) {
  svrfmt::Archive a;
  std::string why;
  if (!svrfmt::ArchiveOpen(matchhd, a, &why)) {
    REXLOG_WARN("[arena pages] MatchHD.pac unreadable: {}", why);
    return false;
  }
  svrfmt::ArcNode *mupa = nullptr;
  for (auto &k : a.root.kids)
    if (k.group == "MLYE" && k.name == "MUPA")
      mupa = &k;
  if (!mupa || !svrfmt::ArchiveExpand(*mupa)) {
    REXLOG_WARN("[arena pages] MatchHD.pac: no MLYE/MUPA banner pack");
    return false;
  }
  svrfmt::ArcNode *pack = nullptr;
  for (auto &k : mupa->kids)
    if (k.id == 7001)
      pack = &k;
  std::vector<svrfmt::BundleTexture> texs;
  if (!pack || !svrfmt::BundleRead(svrfmt::ArchiveData(*pack), texs)) {
    REXLOG_WARN("[arena pages] MatchHD.pac: banner pack unreadable");
    return false;
  }
  size_t next = 0;
  for (size_t i = 0; i < g_added.size(); ++i) {
    if (banners[i].empty())
      continue;
    while (next < texs.size() && (texs[next].name.rfind("CAR_dummy", 0) != 0 ||
                                  texs[next].data.size() != banners[i].size()))
      ++next;
    if (next == texs.size())
      break;  // no room left: the other arenas keep the "dummy" banner
    texs[next].data = banners[i];
    g_added[i].banner = texs[next].name;
    ++next;
  }
  const svrfmt::Bytes raw = svrfmt::BundleWrite(texs);
  pack->stored = svrfmt::IsBpe(pack->stored) ? bpe_like::EncodeLike(raw, pack->stored) : raw;
  pack->dirty = true;
  svrfmt::ArchiveSave(a);  // (rebuilds mupa->stored)
  Append(matchhd, mupa->stored.data(), mupa->stored.size(), g_mupa_sector, g_mupa_units);
  return true;
}

}  // namespace

// Called at startup, before the mods overlay is mounted (wwe12_app.h): the files the added arenas need, served
// from Documents\wwe12\arena_slots ahead of the mods folder.
void Wwe12MountArenaSlots(rex::filesystem::VirtualFileSystem *vfs, const std::string &game_device,
                          const fs::path &user_root, const fs::path &mods) {
  if (!REXCVAR_GET(wwe12_arena_pages) || user_root.empty())
    return;
  const fs::path out = user_root / "arena_slots";
  std::error_code ec;
  fs::remove_all(out, ec);
  // The added arenas, in folder order.
  std::vector<fs::path> dirs;
  for (fs::directory_iterator it(mods / ".svrmods" / "Arenas", ec), end; !ec && it != end; it.increment(ec))
    if (it->is_directory(ec))
      dirs.push_back(it->path());
  std::sort(dirs.begin(), dirs.end());
  std::vector<std::vector<uint8_t>> banners;
  for (const auto &d : dirs) {
    std::vector<uint8_t> m, pac;
    if (fs::exists(d / "disabled", ec) || !ReadHost(d / "manifest.txt", m))
      continue;
    const std::string text(m.begin(), m.end());
    if (Value(text, "slot") != "new" || Value(text, "base").empty() || !ReadHost(d / "arena" / "arena.pac", pac))
      continue;
    Added a;
    a.id = d.filename().string();
    a.name = Value(text, "name");
    a.base = u32(atoi(Value(text, "base").c_str()));
    // Its STG data: the EPAC's STG record at 0x800 = {"STG ", ..., +0x10 start (sectors), +0x14 size (256 bytes)}.
    if (a.base > 22 || pac.size() < 0x818 || memcmp(&pac[0x800], "STG ", 4) != 0) {
      REXLOG_WARN("[arena pages] {}: not an arena file", a.id);
      continue;
    }
    u32 start, units;
    memcpy(&start, &pac[0x810], 4);
    memcpy(&units, &pac[0x814], 4);
    start += 8;  // (counted from the data, which starts at 0x4000, after the EPAC header)
    if (size_t(start) * 0x800 + size_t(units) * 0x100 > pac.size()) {
      REXLOG_WARN("[arena pages] {}: arena file cut short", a.id);
      continue;
    }
    char rel[32];
    snprintf(rel, sizeof(rel), "pac/bg/bg%02u.pac", a.base);
    Served &sv = g_served[a.base];
    if (sv.path.empty()) {  // the game's file of this base, kept beside, and served as it is for now
      std::vector<uint8_t> game;
      if (!ReadForGame(vfs, mods, rel, game) || !WriteHost(out / "game" / rel, game)) {
        REXLOG_WARN("[arena pages] {}: {} unreadable", a.id, rel);
        g_served.erase(a.base);
        continue;
      }
      sv.path = out / rel;
      sv.original = out / "game" / rel;
      sv.size = game.size();
    }
    sv.size = std::max(sv.size, (pac.size() + 0x7FF) & ~size_t(0x7FF));
    a.file = d / "arena" / "arena.pac";
    a.sector = start;
    a.units = units;
    std::vector<uint8_t> banner;
    ReadHost(d / "arena" / "banner.dds", banner);
    banners.push_back(banner.size() == 32896 ? banner : std::vector<uint8_t>{});
    if (ReadHost(d / "arena" / "card_mb.pac", a.card_mb) && ReadHost(d / "arena" / "card_mf.pac", a.card_mf) &&
        (a.card_mb.size() < 8 || memcmp(a.card_mb.data(), "PACH", 4) != 0 || a.card_mf.size() < 8 ||
         memcmp(a.card_mf.data(), "PACH", 4) != 0)) {
      REXLOG_WARN("[arena pages] {}: its VS screen files aren't packs", a.id);
      a.card_mb.clear();
    }
    if (a.card_mb.empty() || a.card_mf.empty())
      a.card_mb.clear(), a.card_mf.clear();
    if (ReadHost(d / "arena" / "etex_hd.pac", a.etex) && (a.etex.size() < 8 || memcmp(a.etex.data(), "PACH", 4) != 0)) {
      REXLOG_WARN("[arena pages] {}: its entrance graphics aren't a pack", a.id);
      a.etex.clear();
    }
    if (ReadHost(d / "arena" / "load_hd.pac", a.load) && (a.load.size() < 8 || memcmp(a.load.data(), "PACH", 4) != 0)) {
      REXLOG_WARN("[arena pages] {}: its pre-match screen isn't a pack", a.id);
      a.load.clear();
    }
    if (ReadHost(d / "arena" / "transition.bik", a.movie) && (a.movie.size() < 8 || memcmp(a.movie.data(), "BIK", 3) != 0)) {
      REXLOG_WARN("[arena pages] {}: its transition isn't a Bink video", a.id);
      a.movie.clear();
    }
    g_added.push_back(a);
    if (g_added.size() == 35)  // (the banner pack has 35 free textures)
      break;
  }
  if (g_added.empty())
    return;
  for (auto &[stage, sv] : g_served) {
    std::vector<uint8_t> game;
    ReadHost(sv.original, game);
    game.resize(sv.size, 0);
    if (!WriteHost(sv.path, game))
      REXLOG_WARN("[arena pages] could not write {}", rex::path_to_utf8(sv.path));
  }
  std::vector<uint8_t> matchhd;
  if (ReadForGame(vfs, mods, "pac/menu/MatchHD.pac", matchhd) && BuildBanners(matchhd, banners)) {
    for (auto &a : g_added) {
      if (a.card_mb.empty())
        continue;
      Append(matchhd, a.card_mb.data(), a.card_mb.size(), a.mb_sector, a.mb_units);
      Append(matchhd, a.card_mf.data(), a.card_mf.size(), a.mf_sector, a.mf_units);
      a.card_mb = {};
      a.card_mf = {};
    }
    for (auto &a : g_added) {  // (the transition videos: see above)
      if (a.movie.empty())
        continue;
      Append(matchhd, a.movie.data(), a.movie.size(), a.movie_sector, a.movie_units);
      a.movie = {};
    }
    WriteHost(out / "pac" / "menu" / "MatchHD.pac", matchhd);
  }
  if (std::any_of(g_added.begin(), g_added.end(), [](const Added &a) { return !a.etex.empty(); })) {
    std::vector<uint8_t> evtgd;
    if (ReadForGame(vfs, mods, "pac/evt/evtgdhd.pac", evtgd)) {
      for (auto &a : g_added) {
        if (a.etex.empty())
          continue;
        Append(evtgd, a.etex.data(), a.etex.size(), a.etex_sector, a.etex_units);
        a.etex = {};
      }
      WriteHost(out / "pac" / "evt" / "evtgdhd.pac", evtgd);
    }
  }
  if (std::any_of(g_added.begin(), g_added.end(), [](const Added &a) { return !a.load.empty(); })) {
    std::vector<uint8_t> loadhd;
    if (ReadForGame(vfs, mods, "pac/menu/LoadHD.pac", loadhd)) {
      for (auto &a : g_added) {
        if (a.load.empty())
          continue;
        Append(loadhd, a.load.data(), a.load.size(), a.load_sector, a.load_units);
        a.load = {};
      }
      WriteHost(out / "pac" / "menu" / "LoadHD.pac", loadhd);
    }
  }
  auto overlay = std::make_unique<rex::filesystem::HostPathDevice>(game_device, out, true);
  if (!overlay->Initialize() || !vfs->RegisterOverlay(game_device, std::move(overlay))) {
    REXLOG_WARN("[arena pages] could not serve {}", rex::path_to_utf8(out));
    g_added.clear();
    return;
  }
  REXLOG_INFO("[arena pages] {} added arena(s), served from {}", g_added.size(), rex::path_to_utf8(out));
}

namespace {

// ---- the screen

bool g_unwritable = false;

void SaveOriginal() {
  if (g_saved || g_unwritable)
    return;
  // The records sit in read-only data: the host page(s) holding them are made writable once (if that fails,
  // no pages: writing there would crash).
  for (u32 va = kRecords & ~0xFFFu; va < kBanners + kTiles * 8; va += 0x1000) {
    DWORD old = 0;
    if (!VirtualProtect(bd::mem::at<u8>(va), 0x1000, PAGE_READWRITE, &old)) {
      REXLOG_WARN("[arena pages] page {:08X} could not be made writable: no arena pages", va);
      g_unwritable = true;
      return;
    }
  }
  for (int i = 0; i < kTiles; ++i)
    g_original[i] = bd::mem::load<u32>(kRecords + i * 8);
  for (int i = 0; i < kTiles * 2; ++i)
    g_original_banners[i] = bd::mem::load<u32>(kBanners + i * 4);
  // The banners' names in guest memory, for the table.
  for (auto &a : g_added)
    if (!a.banner.empty() && !a.banner_name) {
      a.banner_name = REX_KERNEL_MEMORY()->SystemHeapAlloc(u32(a.banner.size() + 1), 4);
      if (a.banner_name)
        memcpy(bd::mem::at<char>(a.banner_name), a.banner.c_str(), a.banner.size() + 1);
    }
  g_saved = true;
}

void BuildPages() {
  g_pages.clear();
  std::array<Tile, kTiles> page{};
  for (int i = 0; i < kTiles; ++i)
    page[i] = {g_original[i], 0};
  g_pages.push_back(page);
  if (REXCVAR_GET(wwe12_arena_pages_test)) {
    for (int i = 0; i < kTiles; ++i)
      page[i] = {g_original[kTiles - 1 - i], 0};
    g_pages.push_back(page);
  }
  for (size_t first = 0; first < g_added.size(); first += kTiles) {
    for (int i = 0; i < kTiles; ++i) {
      const size_t n = first + size_t(i);
      page[i] = n < g_added.size() ? Tile{kFirstAdded + u32(n), g_added[n].banner_name ? g_added[n].banner_name
                                                                                        : 0x82033460 /* "dummy" */}
                                   : Tile{kEmptyTile, 0x82033460};
    }
    g_pages.push_back(page);
  }
}

void WritePage(int page) {
  const auto &p = g_pages[size_t(page)];
  const bool game_banners = p[0].banner == 0;
  for (int i = 0; i < kTiles; ++i) {
    bd::mem::store<u32>(kRecords + i * 8, p[i].stage);
    if (game_banners) {
      bd::mem::store<u32>(kBanners + i * 8, g_original_banners[size_t(i) * 2]);
      bd::mem::store<u32>(kBanners + i * 8 + 4, g_original_banners[size_t(i) * 2 + 1]);
    } else {
      bd::mem::store<u32>(kBanners + i * 8, p[i].stage);
      bd::mem::store<u32>(kBanners + i * 8 + 4, p[i].banner);
    }
  }
}

u32 Field(u32 screen, u32 offset) { return bd::mem::load<u32>(screen + offset); }

// Any tab but the arenas' that the screen allows (the checks of sub_825A5C88).
bool OtherTabs(u32 screen) {
  return Field(screen, 10868) != 0 || Field(screen, 10856) != 0 || int32_t(Field(screen, 10864)) > 0 ||
         Field(screen, 10872) != 0;
}

bool IsAdded(u32 stage) { return stage >= kFirstAdded && stage < kFirstAdded + g_added.size(); }

// Puts the arena that will play on `base` (an added one, or -1 for the game's) in its served file. The file keeps
// its size (padding at the end), so the file system's view of it stays right; it is only opened while an arena
// loads.
void Serve(u32 base, int added) {
  auto it = g_served.find(base);
  if (it == g_served.end() || it->second.current == added)
    return;
  Served &sv = it->second;
  std::vector<uint8_t> bytes;
  if (!ReadHost(added < 0 ? sv.original : g_added[size_t(added)].file, bytes) || bytes.size() > sv.size) {
    REXLOG_WARN("[arena pages] could not read the arena for stage {}", base);
    return;
  }
  bytes.resize(sv.size, 0);
  if (!WriteHost(sv.path, bytes)) {
    REXLOG_WARN("[arena pages] could not write {}", rex::path_to_utf8(sv.path));
    return;
  }
  sv.current = added;
  REXLOG_INFO("[arena pages] stage {} now plays {}", base, added < 0 ? "the game's arena" : g_added[size_t(added)].id);
}

}  // namespace

REX_EXTERN(__imp__sub_825A67A0);  // arena select: constructor
REX_EXTERN(__imp__sub_825A5C88);  // arena select: LB / RB (this, 0 previous / 1 next)
REX_EXTERN(__imp__sub_824C3000);  // (this + 3444, 0): what a tab change does first
REX_EXTERN(__imp__sub_825A3CD8);  // arena select: bind the tiles' banners (this)
REX_EXTERN(__imp__sub_825A3AB8);  // arena select: cursor on (this, row, column)
REX_EXTERN(__imp__sub_828BF398);  // grid widget (this + 44): vtable 0x820287E0 slot 4
REX_EXTERN(__imp__sub_824C3220);  // (this + 3368)
REX_EXTERN(__imp__sub_824D19E8);  // arena select: redraw (this)
REX_EXTERN(__imp__sub_825A6A98);  // arena select: draw (every frame)
REX_EXTERN(__imp__sub_825A3318);  // arena select: is this stage available (this, stage)
REX_EXTERN(__imp__sub_825A56C8);  // arena select: the chosen stage (this)
REX_EXTERN(__imp__sub_827F29F0);  // file directory: find "/GRP/NAME" (directory, name, out)
REX_EXTERN(sub_82A8C070);         // XInputGetState(user, state) (hooked by wwe12_intros.cpp: call the hooked one)

namespace {
u32 g_screen = 0;     // the arena select screen (its constructor's this)
u32 g_pad_state = 0;  // guest X_INPUT_STATE for our own reads
u16 g_last_buttons = 0;
}  // namespace

extern "C" REX_FUNC(sub_825A67A0) {
  g_screen = ctx.r3.u32;
  g_last_buttons = 0xFFFF;  // (a button still held from the screen before doesn't count)
  g_chosen = -1;
  if (REXCVAR_GET(wwe12_arena_pages)) {
    SaveOriginal();
    if (g_saved) {
      BuildPages();
      g_page = 0;
      WritePage(0);
      REXLOG_INFO("[arena pages] arena select opened: {} page(s)", g_pages.size());
    }
  }
  __imp__sub_825A67A0(ctx, base);
}

namespace {

// What sub_825A5C88 does on a tab change, for a page change: banners bound again, cursor on the first tile.
void Refresh(const PPCContext &in, u8 *base, u32 screen) {
  PPCContext x = in;
  auto call = [&](void (*fn)(PPCContext &, u8 *), u32 a, u32 b = 0, u32 c = 0) {
    x = in;
    x.r3.u64 = a;
    x.r4.u64 = b;
    x.r5.u64 = c;
    fn(x, base);
  };
  call(__imp__sub_824C3000, screen + 3444, 0);
  call(__imp__sub_825A3CD8, screen);
  call(__imp__sub_825A3AB8, screen, 0, 0);
  call(__imp__sub_828BF398, screen + 44);
  call(__imp__sub_824C3220, screen + 3368);
  call(__imp__sub_824D19E8, screen);
}

// LB / RB on the arena tab: true when it was a page change (else the tab change goes on).
bool TurnPage(const PPCContext &in, u8 *base, u32 screen, bool next) {
  const int pages = int(g_pages.size());
  if (!g_saved || pages < 2 || Field(screen, 3260) != 0)
    return false;
  const int page = g_page + (next ? 1 : -1);
  if (page < 0 || page >= pages) {
    if (OtherTabs(screen)) {
      g_page = next ? 0 : pages - 1;  // on to another tab; the arena tab comes back on the page next to it
      WritePage(g_page);
      return false;
    }
  }
  g_page = (page + pages) % pages;
  WritePage(g_page);
  Refresh(in, base, screen);
  REXLOG_INFO("[arena pages] page {} of {}", g_page + 1, pages);
  return true;
}

}  // namespace

// The tab buttons, when the widget passes them on (screens with more than one tab).
extern "C" REX_FUNC(sub_825A5C88) {
  if (TurnPage(ctx, base, ctx.r3.u32, ctx.r4.u32 != 0))
    return;
  __imp__sub_825A5C88(ctx, base);
}

// Every frame the screen draws: LB / RB read from the pad (one tab only: the widget drops them).
extern "C" REX_FUNC(sub_825A6A98) {
  const u32 screen = g_screen;
  const bool ours = screen && (ctx.r3.u32 == screen || ctx.r3.u32 == screen + 28);
  if (ours && g_pages.size() > 1 && !OtherTabs(screen)) {
    if (!g_pad_state)
      g_pad_state = REX_KERNEL_MEMORY()->SystemHeapAlloc(16, 0x10);
    PPCContext x = ctx;
    x.r3.u64 = 0;
    x.r4.u64 = g_pad_state;
    sub_82A8C070(x, base);
    const u16 buttons = x.r3.u32 == 0 ? bd::mem::load<u16>(g_pad_state + 4) : 0;
    const u16 pressed = u16(buttons & ~g_last_buttons);
    g_last_buttons = buttons;
    if (pressed & 0x0200)
      TurnPage(ctx, base, screen, true);
    else if (pressed & 0x0100)
      TurnPage(ctx, base, screen, false);
  }
  __imp__sub_825A6A98(ctx, base);
}

// An added arena's tile is available; a page's empty tiles are not.
extern "C" REX_FUNC(sub_825A3318) {
  const u32 stage = ctx.r4.u32;
  if (IsAdded(stage) || stage == kEmptyTile) {
    ctx.r3.u64 = stage == kEmptyTile ? 0 : 1;
    return;
  }
  __imp__sub_825A3318(ctx, base);
}

// The chosen arena: an added one plays as its base (and its file is served, see the directory below).
extern "C" REX_FUNC(sub_825A56C8) {
  __imp__sub_825A56C8(ctx, base);
  const u32 stage = ctx.r3.u32;
  if (IsAdded(stage)) {
    const int n = int(stage - kFirstAdded);
    if (g_chosen != n)
      REXLOG_INFO("[arena pages] chosen: {} ({}), plays as stage {}", g_added[size_t(n)].name, g_added[size_t(n)].id,
                  g_added[size_t(n)].base);
    g_chosen = n;
    Serve(g_added[size_t(n)].base, n);
    ctx.r3.u64 = g_added[size_t(n)].base;
  } else if (stage < kFirstAdded) {
    g_chosen = -1;
    Serve(stage, -1);  // (one of the game's: its own file back, if an added arena was in it)
  }
}

// The file directory: the banner pack and the chosen added arena come from the data appended to their files.
extern "C" REX_FUNC(sub_827F29F0) {
  const u32 name = ctx.r4.u32, out = ctx.r5.u32;
  __imp__sub_827F29F0(ctx, base);
  if (!name || !out || ctx.r3.u32 == 0)
    return;
  const char *n = bd::mem::str(name);
  if (g_mupa_units && strcmp(n, "/MLYE/MUPA") == 0 && bd::mem::load<u16>(out + 12) == kMatchHdIndex) {
    bd::mem::store<u32>(out, g_mupa_sector);
    bd::mem::store<u32>(out + 8, g_mupa_units);
  } else if (g_chosen >= 0 && strncmp(n, "/STG/", 5) == 0 && u32(atoi(n + 5)) == g_added[size_t(g_chosen)].base &&
             n[5] >= '0' && n[5] <= '9') {
    // The added arena is the served file now: its own start, and room for the bigger of the two. Never less than
    // the game's size: the arena's packed entries are unpacked in place in that room, which needs the slack.
    const Added &a = g_added[size_t(g_chosen)];
    bd::mem::store<u32>(out, a.sector);
    bd::mem::store<u32>(out + 8, std::max(a.units, bd::mem::load<u32>(out + 8)));
    REXLOG_INFO("[arena pages] {}: the added arena {} ({} KB)", n, a.id, a.units / 4);
  } else if (g_chosen >= 0 && g_added[size_t(g_chosen)].mb_units && bd::mem::load<u16>(out + 12) == kMatchHdIndex) {
    // Its own VS screen, in place of its base's.
    const Added &a = g_added[size_t(g_chosen)];
    char mb[16], mf[16];
    snprintf(mb, sizeof(mb), "/MLYE/MB%02u", a.base);
    snprintf(mf, sizeof(mf), "/MENU/M%02uF", a.base);
    const bool is_mb = strcmp(n, mb) == 0;
    if (is_mb || strcmp(n, mf) == 0) {
      bd::mem::store<u32>(out, is_mb ? a.mb_sector : a.mf_sector);
      bd::mem::store<u32>(out + 8, is_mb ? a.mb_units : a.mf_units);
      REXLOG_INFO("[arena pages] {}: the VS screen of {}", n, a.id);
    }
  } else if (g_chosen >= 0 && g_added[size_t(g_chosen)].etex_units &&
             bd::mem::load<u16>(out + 12) == kEvtGdHdIndex) {
    // Its own entrance graphics (location plate, name plates), in place of its base's.
    const Added &a = g_added[size_t(g_chosen)];
    char etex[16];
    snprintf(etex, sizeof(etex), "/ETEX/%04u", a.base);
    if (strcmp(n, etex) == 0) {
      bd::mem::store<u32>(out, a.etex_sector);
      bd::mem::store<u32>(out + 8, a.etex_units);
      REXLOG_INFO("[arena pages] {}: the entrance graphics of {}", n, a.id);
    }
  } else if (g_chosen >= 0 && g_added[size_t(g_chosen)].load_units && bd::mem::load<u16>(out + 12) == kLoadHdIndex) {
    // Its own pre-match screen (VS screen and its animation), in place of its base's.
    const Added &a = g_added[size_t(g_chosen)];
    char load[16];
    snprintf(load, sizeof(load), "/LOAD/90%02u", a.base);
    if (strcmp(n, load) == 0) {
      bd::mem::store<u32>(out, a.load_sector);
      bd::mem::store<u32>(out + 8, a.load_units);
      REXLOG_INFO("[arena pages] {}: the pre-match screen of {}", n, a.id);
    }
  } else if (g_chosen >= 0 && g_added[size_t(g_chosen)].movie_units && bd::mem::load<u16>(out + 12) == kEvtMovIndex) {
    // Its own transition video, from the served MatchHD.pac (its file number too).
    const Added &a = g_added[size_t(g_chosen)];
    char movt[16];
    snprintf(movt, sizeof(movt), "/MOVT/M%03u", a.base);
    if (strcmp(n, movt) == 0) {
      bd::mem::store<u32>(out, a.movie_sector);
      bd::mem::store<u32>(out + 8, a.movie_units);
      bd::mem::store<u16>(out + 12, kMatchHdIndex);
      REXLOG_INFO("[arena pages] {}: the transition of {}", n, a.id);
    }
  }
  if (REXCVAR_GET(wwe12_arena_pages_test) &&
      (strncmp(n, "/STG", 4) == 0 || strcmp(n, "/MLYE/MUPA") == 0))
    REXLOG_INFO("[arena pages] directory {} -> file {} start {:08X} size {:08X}", n, bd::mem::load<u16>(out + 12),
                bd::mem::load<u32>(out), bd::mem::load<u32>(out + 8));
}
