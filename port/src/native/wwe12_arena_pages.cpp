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
//    640 x 640 in WWE '12 and WWE '13) appended to a served evtmov.pac; while it is the chosen arena, the directory
//    answers its base's /MOVT/M0NN with it. evtmov.pac is 273 MB, so that copy lives apart
//    (Documents\wwe12\arena_movies) and is only rebuilt when the videos change. (Appended to the served MatchHD.pac
//    instead, with the directory's file number at +12 changed to it: the video player waited forever on a black
//    screen.)
// 4:3 (the game's flag at 0x8324CE3C, set below 1280 x 720: sub_826A5F60 then reads plist360_4x3.arc, whose file
// numbers are the same with the SD files: 234 = MatchSD.pac, 233 = LoadSD.pac, 190 = evtgdsd.pac): MatchSD.pac gets
// the same banner pack and VS screens (the same 71 textures, 256 x 128); the entrance graphics and pre-match screen
// stay the base's there (they only exist in HD).
//
// Added superstars' own entrances (a WWE '13 one, the same format as WWE '12's: the Mod Maker's entrance13.h; superstar
// mods with entrance_own=NNN and superstar/entrance/EVP_<n>.bin, EVZ_<n>.bin for NNN and its variants 1xNNN): appended
// to served copies of their pac\evt\NyujyoN.pac (Documents\wwe12\entrance_slots, the file of the number's group);
// the directory hasn't them, so a lookup of "/EVP3/246" that finds nothing is asked again with an entry of the same
// file and name length (every field of its answer right) and given the appended entry's place and size.
//
// Test: wwe12_arena_pages_test = true adds one page, the 21 arenas in reverse order, and logs the lookups.

#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <span>
#include <tuple>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <chrono>

#include <imgui.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>

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
#include "../modmaker/svrfmt/jboy.h"
#include "../modmaker/svrfmt/texture.h"

REXCVAR_DEFINE_BOOL(wwe12_arena_pages, true, "WWE12",
                    "Added arenas (arena mods with slot=new) on more pages of the arena select screen (LB / RB).");
REXCVAR_DEFINE_BOOL(wwe12_entrance_mods, true, "WWE12",
                    "Added superstars' own entrances (superstar mods with entrance_own=, from WWE '13), served to the game.");
REXCVAR_DEFINE_DOUBLE(wwe12_belt_scale_test, 0.0, "WWE12",
                      "Test: an added superstar's waist belt object's three floats at wwe12_belt_scale_at set to this (0: off).");
REXCVAR_DEFINE_INT32(wwe12_belt_scale_at, 64, "WWE12", "Test: where in the belt object (bytes) the scale tried is.");
REXCVAR_DEFINE_INT32(wwe12_belt_radius_which, 0, "WWE12", "Test: 0 both fields, 1 only +24, 2 only +272.");
REXCVAR_DEFINE_DOUBLE(wwe12_belt_radius_test, 0.0, "WWE12",
                      "Test: an added superstar's waist belt fields +24 / +272 times this while it is bent (0: off).");
REXCVAR_DEFINE_DOUBLE(wwe12_belt_length_test, 0.0, "WWE12",
                      "Test: every added superstar's waist belt this much longer (0: measured).");
REXCVAR_DEFINE_BOOL(wwe12_belt_length, false, "WWE12",
                    "Added superstars wider at the waist than their base: their waist belt made that much longer.");
// (off: no change seen in an entrance, every attire line written, 2026-11-03: the game never loads BLTS there)
REXCVAR_DEFINE_BOOL(wwe12_belt_fit, false, "WWE12",
                    "Added superstars: the title belt fitted to their model (their base's, scaled to their waist).");
REXCVAR_DEFINE_BOOL(wwe12_entrance_list, true, "WWE12",
                    "CREATE AN ENTRANCE: the mods' own entrances added to the superstars' entrance lists.");
REXCVAR_DEFINE_BOOL(wwe12_entrance_list_log, false, "WWE12",
                    "Test: CREATE AN ENTRANCE's lists as the game fills them, to the log ([entrance list]).");
REXCVAR_DEFINE_BOOL(wwe12_dir_log, false, "WWE12",
                    "Test: log every entry the game looks up in a pac's directory, and whether it was found ([dir]).");
REXCVAR_DEFINE_BOOL(wwe12_arena_pages_test, false, "WWE12",
                    "Test: a second page on the arena select screen (the 21 arenas in reverse order).");

// (wwe12_superstar_mods.cpp: the name an entrant's ribbon / wall lookup is served under, "" if none; the superstar
// entering now, 0 if none)
std::string Wwe12OwnScreen(const std::string &asked, const PPCContext &ctx, uint8_t *base);
// (wwe12_superstar_mods.cpp) a menu pictures mod's layout of MnLayHD.pac / MnLaySD.pac: its place in the served copy
bool Wwe12MenuPictureEntry(const std::string &name, bool sd, uint32_t &sector, uint32_t &units);
uint32_t Wwe12EnteringSuperstar(const PPCContext &ctx, uint8_t *base);

// A ribbon / wall movie the game can play: Bink, 180 frames or more, 1.8 MB at most (the game's own: all 180 frames,
// all under 1.81 MB). Fewer frames or a bigger file froze the loading before the entrance (2026-11-04).
bool OwnScreenFits(const std::vector<uint8_t> &d) {
  if (d.size() < 12 || d.size() > 1810000 || d[0] != 'B' || d[1] != 'I' || d[2] != 'K')
    return false;
  const uint32_t frames = uint32_t(d[8]) | uint32_t(d[9]) << 8 | uint32_t(d[10]) << 16 | uint32_t(d[11]) << 24;
  // (and its largest frame no bigger than the game's own, 32896 bytes at most: 2K14 Antonio Cesaro's wall, 33148, froze
  // the loading before his entrance every time, 2026-11-07)
  const uint32_t largest = d.size() < 16 ? 0 : uint32_t(d[12]) | uint32_t(d[13]) << 8 | uint32_t(d[14]) << 16 | uint32_t(d[15]) << 24;
  return frames >= 180 && largest <= 32896;
}
bool OwnScreenFits(const std::filesystem::path &p) {
  std::vector<uint8_t> d;
  std::error_code ec;
  if (!std::filesystem::is_regular_file(p, ec))
    return false;
  std::ifstream f(p, std::ios::binary);
  d.assign(std::istreambuf_iterator<char>(f), {});
  return OwnScreenFits(d);
}


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
constexpr u16 kMnLayHdIndex = 237;  // pac\menu\MnLayHD.pac

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
  u32 mb_sd_sector = 0, mb_sd_units = 0, mf_sd_sector = 0, mf_sd_units = 0;  // ... and to MatchSD.pac
  std::vector<uint8_t> etex;  // its own entrance graphics (until appended), else empty
  u32 etex_sector = 0, etex_units = 0;  // ... appended to evtgdhd.pac (0: the base's)
  std::vector<uint8_t> load;  // its own pre-match screen (until appended), else empty
  u32 load_sector = 0, load_units = 0;  // ... appended to LoadHD.pac (0: the base's)
  std::vector<uint8_t> movie;  // its own transition video (until appended), else empty
  u32 movie_sector = 0, movie_units = 0;  // ... appended to evtmov.pac (0: the base's)
  std::vector<uint8_t> screens;  // its own wall movie, for every MVSW/M* (until appended), else empty
  u32 screens_sector = 0, screens_units = 0;  // ... appended to evtmov.pac (0: the base's)
  std::vector<uint8_t> ribbon;  // its own ribbon movie, for every MVAB/M* and MVSB/M* (until appended), else empty
  u32 ribbon_sector = 0, ribbon_units = 0;  // ... appended to evtmov.pac (0: the base's)
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
u32 g_mupa_sd_sector = 0, g_mupa_sd_units = 0;  // ... and to MatchSD.pac
int g_chosen = -1;                        // the added arena chosen last (its index), -1: one of the game's
bool g_arena_movies_served = false;  // Documents/wwe12/arena_movies mounted over the game's files (Wwe12MountArenaSlots)
bool g_reload_vs = false;                 // the match setup's VS screen to load again (another arena on the same stage)
// The added entrances by directory name ("/EVP3/246"): where they are in their served Nyujyo file, and an entry of
// the game's in the same file with a name of the same length (asked in their place).
struct AddedEntrance {
  u32 sector = 0, units = 0;
  std::string like;
};
std::map<std::string, AddedEntrance> g_entrances;
// The game's entries a media mod puts its own in place of (evtmov.pac MVSB / MVSW M<n>: a superstar's ribbon and
// wall, made on the Mod Maker's VIDEOS page), by name as asked ("MVSB/M139"); and every (kind 1 ribbon / 2 wall,
// video number) a media mod has, replaced or added.
std::map<std::string, AddedEntrance> g_replaced;
std::set<std::pair<uint32_t, uint32_t>> g_mod_screens;
// The mods' own entrances (entrance_own=), for CREATE AN ENTRANCE's lists: number, name shown.
std::vector<std::pair<int, std::string>> g_mod_entrances;
// Added superstars wider at the waist than their base: number -> how much longer their waist belt is (sub_828C5878).
std::map<uint32_t, float> g_belt_length;

struct Tile {
  u32 stage, banner;  // banner: guest address of the texture name (0: the game's table is kept)
};
std::array<u32, kTiles> g_original{};
std::array<u32, kTiles * 2> g_original_banners{};
bool g_saved = false;
std::vector<std::array<Tile, kTiles>> g_pages;  // page 0 = the game's
int g_page = 0;
// (for the page label, drawn on the UI thread: wwe12_arena_pages' ArenaPageLabel)
std::atomic<int> g_label_page{0}, g_label_pages{0};
std::atomic<int64_t> g_label_seen{0};  // when the arena select was last drawn (ms)
std::atomic<bool> g_label_open{false};
std::atomic<int> g_label_cursor{-1};  // the added arena under the cursor (its index in g_added), -1: none  // the label dialog exists (made on the UI thread while the screen shows)
void OpenPageLabel();                    // (below)
int64_t NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

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

// The added arenas' transition videos and screens' movies appended to a copy of evtmov.pac in movies_root
// (pac\evt\evtmov.pac), kept between starts with a key (the base's size, each video's size and hash) and rebuilt when it changes. False: none
// (or no copy), so nothing to serve.
bool ServeMovies(rex::filesystem::VirtualFileSystem *vfs, const fs::path &mods, const fs::path &movies_root) {
  std::error_code ec;
  const fs::path served = movies_root / "pac" / "evt" / "evtmov.pac", key_file = movies_root / "key.txt";
  if (std::none_of(g_added.begin(), g_added.end(), [](const Added &a) { return !a.movie.empty() || !a.screens.empty() || !a.ribbon.empty(); })) {
    fs::remove_all(movies_root, ec);
    return false;
  }
  // The game's evtmov.pac: the mods folder's, else the disc's. (The added superstars' ribbons and walls are appended
  // to this copy afterwards, by MountEntrances, which runs later: this copy is served over theirs.)
  const fs::path host_base = mods / "pac" / "evt" / "evtmov.pac";
  rex::filesystem::File *file = nullptr;
  uint64_t base_size = 0;
  if (fs::exists(host_base, ec)) {
    base_size = fs::file_size(host_base, ec);
  } else {
    rex::filesystem::Entry *entry = vfs->ResolvePath("game:\\pac\\evt\\evtmov.pac");
    if (!entry || entry->Open(rex::filesystem::FileAccess::kFileReadData, &file) != 0 || !file)
      return false;
    base_size = entry->size();
  }
  // Where each video goes, and the key.
  uint64_t end = (base_size + 0x7FF) & ~uint64_t(0x7FF);
  std::string key = std::to_string(base_size);
  if (!file) {  // (a host copy: its last 16 MB in the key too, where the added superstars' ribbons and walls are)
    std::ifstream in(host_base, std::ios::binary);
    const uint64_t from = base_size > (16u << 20) ? base_size - (16u << 20) : 0;
    in.seekg(std::streamoff(from));
    std::vector<char> tail(size_t(base_size - from));
    in.read(tail.data(), std::streamsize(tail.size()));
    uint64_t h = 1469598103934665603ull;
    for (char c : tail)
      h = (h ^ uint8_t(c)) * 1099511628211ull;
    key += "/" + std::to_string(h);
  }
  for (auto &a : g_added)
    for (auto [m, sector, units] : {std::tuple{&a.movie, &a.movie_sector, &a.movie_units},
                                    std::tuple{&a.screens, &a.screens_sector, &a.screens_units},
                                    std::tuple{&a.ribbon, &a.ribbon_sector, &a.ribbon_units}}) {
      if (m->empty())
        continue;
      *sector = u32(end / 0x800);
      *units = u32((m->size() + 0xFF) / 0x100);
      end = (end + m->size() + 0x7FF) & ~uint64_t(0x7FF);
      uint64_t h = 1469598103934665603ull;
      for (uint8_t c : *m)
        h = (h ^ c) * 1099511628211ull;
      key += " " + std::to_string(m->size()) + ":" + std::to_string(h);
    }
  std::vector<uint8_t> old_key;
  if (ReadHost(key_file, old_key) && std::string(old_key.begin(), old_key.end()) == key &&
      fs::file_size(served, ec) == end && !ec) {
    if (file)
      file->Destroy();
    for (auto &a : g_added)
      a.movie = {}, a.screens = {}, a.ribbon = {};
    return true;
  }
  fs::remove_all(movies_root, ec);
  fs::create_directories(served.parent_path(), ec);
  bool ok = true;
  if (!file) {
    ok = fs::copy_file(host_base, served, fs::copy_options::overwrite_existing, ec) && !ec;
  } else {
    std::ofstream o(served, std::ios::binary | std::ios::trunc);
    std::vector<uint8_t> chunk(8u << 20);
    for (uint64_t at = 0; ok && at < base_size;) {
      const size_t n = size_t(std::min<uint64_t>(chunk.size(), base_size - at));
      size_t read = 0;
      ok = file->ReadSync(std::span<uint8_t>(chunk.data(), n), at, &read) == 0 && read == n &&
           o.write(reinterpret_cast<const char *>(chunk.data()), std::streamsize(n));
      at += n;
    }
    file->Destroy();
  }
  if (ok) {
    std::fstream o(served, std::ios::binary | std::ios::in | std::ios::out);
    for (auto &a : g_added)
      for (auto [m, sector] : {std::pair{&a.movie, a.movie_sector}, std::pair{&a.screens, a.screens_sector},
                               std::pair{&a.ribbon, a.ribbon_sector}}) {
        if (m->empty())
          continue;
        o.seekp(std::streamoff(uint64_t(sector) * 0x800));
        o.write(reinterpret_cast<const char *>(m->data()), std::streamsize(m->size()));
        *m = {};
      }
    o.close();
    ok = bool(o);
    if (ok) {
      fs::resize_file(served, end, ec);
      ok = !ec;
    }
  }
  if (!ok) {
    REXLOG_WARN("[arena pages] could not write {}", rex::path_to_utf8(served));
    fs::remove_all(movies_root, ec);
    for (auto &a : g_added)
      a.movie_units = a.screens_units = a.ribbon_units = 0;
    return false;
  }
  WriteHost(key_file, std::vector<uint8_t>(key.begin(), key.end()));
  REXLOG_INFO("[arena pages] transition and screens videos served in {}", rex::path_to_utf8(served));
  return true;
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

// The banner pack (MatchHD.pac / MatchSD.pac MLYE/MUPA, PACH entry 7001) with the added arenas' banners in
// CAR_dummyNN, appended to that file. The textures keep their size, so the pack unpacks to the same size as the game's.
// hd: the arenas take their banner's texture name from it; MatchSD.pac's must be the same.
bool BuildBanners(std::vector<uint8_t> &matchhd, const std::vector<std::vector<uint8_t>> &banners, bool hd,
                  u32 &sector, u32 &units) {
  const char *file = hd ? "MatchHD.pac" : "MatchSD.pac";
  svrfmt::Archive a;
  std::string why;
  if (!svrfmt::ArchiveOpen(matchhd, a, &why)) {
    REXLOG_WARN("[arena pages] {} unreadable: {}", file, why);
    return false;
  }
  svrfmt::ArcNode *mupa = nullptr;
  for (auto &k : a.root.kids)
    if (k.group == "MLYE" && k.name == "MUPA")
      mupa = &k;
  if (!mupa || !svrfmt::ArchiveExpand(*mupa)) {
    REXLOG_WARN("[arena pages] {}: no MLYE/MUPA banner pack", file);
    return false;
  }
  svrfmt::ArcNode *pack = nullptr;
  for (auto &k : mupa->kids)
    if (k.id == 7001)
      pack = &k;
  std::vector<svrfmt::BundleTexture> texs;
  if (!pack || !svrfmt::BundleRead(svrfmt::ArchiveData(*pack), texs)) {
    REXLOG_WARN("[arena pages] {}: banner pack unreadable", file);
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
    if (!hd && texs[next].name != g_added[i].banner) {
      REXLOG_WARN("[arena pages] MatchSD.pac: its banner pack differs from MatchHD.pac's");
      return false;
    }
    texs[next].data = banners[i];
    if (hd)
      g_added[i].banner = texs[next].name;
    ++next;
  }
  const svrfmt::Bytes raw = svrfmt::BundleWrite(texs);
  pack->stored = svrfmt::IsBpe(pack->stored) ? bpe_like::EncodeLike(raw, pack->stored) : raw;
  pack->dirty = true;
  svrfmt::ArchiveSave(a);  // (rebuilds mupa->stored)
  Append(matchhd, mupa->stored.data(), mupa->stored.size(), sector, units);
  return true;
}

}  // namespace

// Called at startup, before the mods overlay is mounted (wwe12_app.h): the files the added arenas need, served
// from Documents\wwe12\arena_slots ahead of the mods folder.
namespace {

// WWE '12's entrance groups by number (the last three digits; Nyujyo0 EVP1 100.. NyujyoA EVPK 50..55), -1 for none.
int EntranceFile(int number) {
  const int n = number % 1000;
  if (n >= 50 && n <= 55)
    return 10;
  static const int from[] = {100, 137, 179, 272, 328, 480, 512, 532, 552, 600};
  for (int i = 9; i >= 0; --i)
    if (n >= from[i])
      return n > 855 && i == 9 ? -1 : i;
  return -1;
}

std::string Trimmed(std::string s) {
  while (!s.empty() && (s.back() == ' ' || s.back() == '\0'))
    s.pop_back();
  return s;
}

// A superstar file's (chNNN.pac) first attire: its waist's width and depth where a title belt is worn, from the
// vertices of the pelvis, spine and thighs between 50 % and 54 % of its height (bind pose: y up is negative, the feet
// at 0). False if it can't be read.
bool Waist(const std::vector<uint8_t> &ch, float &width, float &depth) {
  svrfmt::Archive a;
  if (!svrfmt::ArchiveOpen(ch, a, nullptr))
    return false;
  for (auto &k : a.root.kids) {
    if (k.group != "EMD " || !svrfmt::ArchiveExpand(k))
      continue;
    svrfmt::Model m;
    for (auto &x : k.kids)
      if (x.id == 0 && !svrfmt::JboyRead(svrfmt::ArchiveData(x), m, nullptr))
        return false;
    float top = 0;
    for (const auto &me : m.meshes)
      for (const auto &v : me.verts)
        top = std::min(top, v.pos[1]);
    const float y0 = top * 0.54f, y1 = top * 0.50f;
    float lx = 1e9f, hx = -1e9f, lz = 1e9f, hz = -1e9f;
    int n = 0;
    for (const auto &me : m.meshes)
      for (size_t i = 0; i < me.verts.size() && !me.weights.empty(); ++i) {
        const auto &v = me.verts[i];
        const uint8_t s = me.weights[0][i].bones[0];
        if (v.pos[1] < y0 || v.pos[1] > y1 || s == 255 || s >= me.palette.size() || me.palette[s] < 1 ||
            size_t(me.palette[s] - 1) >= m.nodes.size())
          continue;
        const std::string &bone = m.nodes[size_t(me.palette[s] - 1)].name;
        if (bone != "Bip" && bone != "Bip Spine" && bone != "Bip Spine1" && bone.find("Thigh") == std::string::npos)
          continue;
        lx = std::min(lx, v.pos[0]), hx = std::max(hx, v.pos[0]);
        lz = std::min(lz, v.pos[2]), hz = std::max(hz, v.pos[2]);
        ++n;
      }
    if (n < 20)
      return false;
    width = hx - lx, depth = hz - lz;
    return true;
  }
  return false;
}

void MountEntrances(rex::filesystem::VirtualFileSystem *vfs, const std::string &game_device, const fs::path &user_root,
                    const fs::path &mods) {
  const fs::path out = user_root / "entrance_slots";
  std::error_code ec;
  fs::remove_all(out, ec);
  g_entrances.clear();
  g_replaced.clear();
  g_mod_screens.clear();
  if (!REXCVAR_GET(wwe12_entrance_mods))
    return;
  static const char kGroups[] = "12345EFGHIK";
  // The parts by file: the script and tracks (EVP_n / EVZ_n) in their Nyujyo file, the game event script (GMEV_n) in
  // gm.pac, the effects (PCE6_n) in prtcls.pac; each with its group and entry name.
  struct Part {
    std::string group, name;
    std::vector<uint8_t> data;
    bool replace = false;  // (the game's own entry of that name, if it has one, replaced)
  };
  std::map<std::string, std::vector<Part>> parts;
  // The added superstars' slots (wwe12_superstar_mods.cpp, which runs before: "<mod>=<number>"): a victory scene of
  // theirs (superstar/victory/EVP.bin, EVZ.bin: WWE '13's, entrance13::Victory) is served as Winning0.pac's
  // /EVPA/<6000 + slot>, the name the game asks for (6000 + the superstar's number, every group letter tried).
  std::map<std::string, int> slots;
  {
    std::vector<uint8_t> t;
    if (ReadHost(user_root / "superstar_slots.txt", t)) {
      std::string text(t.begin(), t.end()), line;
      for (size_t at = 0; at <= text.size(); ++at)
        if (at == text.size() || text[at] == '\n') {
          if (!line.empty() && line.back() == '\r')
            line.pop_back();
          const size_t eq = line.rfind('=');
          if (eq != std::string::npos)
            slots[line.substr(0, eq)] = atoi(line.c_str() + eq + 1);
          line.clear();
        } else {
          line += text[at];
        }
    }
  }
  // ... and their own video numbers (superstar_movies.txt, "<mod>=<number>": the same, written there last time)
  std::map<std::string, uint32_t> movies;
  {
    std::vector<uint8_t> t;
    if (ReadHost(user_root / "superstar_movies.txt", t)) {
      std::string text(t.begin(), t.end()), line;
      for (size_t at = 0; at <= text.size(); ++at)
        if (at == text.size() || text[at] == '\n') {
          if (!line.empty() && line.back() == '\r')
            line.pop_back();
          const size_t eq = line.rfind('=');
          if (eq != std::string::npos)
            movies[line.substr(0, eq)] = uint32_t(atoi(line.c_str() + eq + 1));
          line.clear();
        } else {
          line += text[at];
        }
    }
  }
  // The game's victory scenes by (name, "EVP" / "EVZ"): their file and group (Winning0-3, WinningA; read when a moves
  // mod asks).
  std::map<std::pair<std::string, std::string>, std::pair<std::string, std::string>> victories;
  bool victories_read = false;
  auto game_victory = [&](const std::string &name, const std::string &kind) -> const std::pair<std::string, std::string> * {
    if (!victories_read) {
      victories_read = true;
      for (const char *f : {"pac/evt/Winning0.pac", "pac/evt/Winning1.pac", "pac/evt/Winning2.pac", "pac/evt/Winning3.pac",
                            "pac/evt/WinningA.pac"}) {
        std::vector<uint8_t> pac;
        svrfmt::Archive a;
        if (!ReadForGame(vfs, mods, f, pac) || !svrfmt::ArchiveOpen(pac, a, nullptr))
          continue;
        for (const auto &k : a.root.kids) {
          const std::string g = Trimmed(k.group);
          if (g.size() == 4 && (g.rfind("EVP", 0) == 0 || g.rfind("EVZ", 0) == 0))
            victories.emplace(std::make_pair(Trimmed(k.name), g.substr(0, 3)), std::make_pair(std::string(f), g));
        }
      }
    }
    const auto it = victories.find({name, kind});
    return it == victories.end() ? nullptr : &it->second;
  };
  // The superstar mods, then the moves mods (type=moves, superstar=NNN: a superstar the game has given another one's
  // entrance and victory scene, the Mod Maker's MOVES & ENTRANCES page, 2026-11-03).
  std::vector<std::pair<fs::path, bool>> dirs;  // (its folder, a moves mod)
  for (const char *kind : {"Superstars", "Moves"}) {
    std::vector<fs::path> one;
    for (fs::directory_iterator it(mods / ".svrmods" / kind, ec), end; !ec && it != end; it.increment(ec))
      if (it->is_directory(ec))
        one.push_back(it->path());
    ec.clear();
    std::sort(one.begin(), one.end());
    for (const auto &d : one)
      dirs.push_back({d, kind[0] == 'M'});
  }
  g_mod_entrances.clear();
  g_belt_length.clear();
  for (const auto &[d, moves_mod] : dirs) {
    std::vector<uint8_t> m;
    if (fs::exists(d / "disabled", ec) || !ReadHost(d / "manifest.txt", m))
      continue;
    // A moves mod's victory scene: the superstar's own, 6000 + his number, in place of the game's wherever it is (its
    // file and group); a superstar without one gets it added to Winning0.pac as an added superstar's.
    if (moves_mod) {
      const int superstar = atoi(Value(std::string(m.begin(), m.end()), "superstar").c_str());
      for (const char *kind : {"EVP", "EVZ"}) {
        Part part;
        if (superstar < 50 || superstar >= 373 ||
            !ReadHost(d / "superstar" / "victory" / (std::string(kind) + ".bin"), part.data) || part.data.empty())
          continue;
        part.name = std::to_string(6000 + superstar);
        if (const auto *at = game_victory(part.name, kind)) {
          part.group = at->second;
          part.replace = true;
          parts[at->first].push_back(std::move(part));
        } else {
          part.group = std::string(kind) + "A";
          parts["pac/evt/Winning0.pac"].push_back(std::move(part));
        }
      }
    }
    const auto slot = moves_mod ? slots.end() : slots.find(d.filename().string());
    if (slot != slots.end() && slot->second > 0)
      for (const char *kind : {"EVP", "EVZ"}) {
        Part part;
        if (!ReadHost(d / "superstar" / "victory" / (std::string(kind) + ".bin"), part.data) || part.data.empty())
          continue;
        part.group = std::string(kind) + "A";
        part.name = std::to_string(6000 + slot->second);
        parts["pac/evt/Winning0.pac"].push_back(std::move(part));
      }
    // its own ribbon and wall (superstar\ribbon.bik, wall.bik: Bink 1, 1024 x 32 and 256 x 256, made by the Mod
    // Maker) as evtmov.pac's MVSB / MVSW M<its own video number> (wwe12_superstar_mods.cpp, Wwe12OwnScreen: asked
    // without a leading slash)
    if (const auto movie = moves_mod ? movies.end() : movies.find(d.filename().string()); movie != movies.end())
      for (const auto &[group, file] : {std::pair<const char *, const char *>{"MVSB", "ribbon.bik"}, {"MVSW", "wall.bik"}}) {
        Part part;
        if (!ReadHost(d / "superstar" / file, part.data) || part.data.empty())
          continue;
        // (the game's own are all 180 frames in 1.8 MB at most: Seth Rollins' 2K16 ribbon of 122 frames froze the
        // loading before his entrance, every time, 2026-11-04; left to the arena's, as OwnScreenFits)
        if (!OwnScreenFits(part.data)) {
          REXLOG_WARN("[arena pages] {}: its own {} left out ({} bytes; the game's are 180 frames, 1.8 MB at most);"
                      " make it again with the Mod Maker", d.filename().string(), file, part.data.size());
          continue;
        }
        char name[8];
        snprintf(name, sizeof(name), "M%03u", movie->second);
        part.group = group;
        part.name = name;
        parts["pac/evt/evtmov.pac"].push_back(std::move(part));
      }
    // the objects of its entrance and victory scene that WWE '12 hasn't (superstar\objects\SOBJ_<model>.bin: Cena's
    // chain, Kane's mask..., entrance13::Objects) as evtobj.pac's SOBJ/<model>
    for (fs::directory_iterator it(d / "superstar" / "objects", ec), end; !ec && it != end; it.increment(ec)) {
      const std::string f = it->path().filename().string();
      const int model = f.rfind("SOBJ_", 0) == 0 ? atoi(f.c_str() + 5) : 0;
      Part part;
      if (model < 1000 || model > 9999 || !ReadHost(it->path(), part.data) || part.data.empty())
        continue;
      part.group = "SOBJ";
      part.name = std::to_string(model);
      parts["pac/evt/evtobj.pac"].push_back(std::move(part));
    }
    ec.clear();
    const std::string manifest(m.begin(), m.end());
    if (const int own = atoi(Value(manifest, "entrance_own").c_str()); own > 0) {
      // (its name in CREATE AN ENTRANCE: a superstar mod's name; a moves mod's, whose entrance it carries: its
      // description "X with the ... of <superstar> from <game>")
      std::string label = moves_mod ? Value(manifest, "description") : Value(manifest, "name");
      if (const size_t of = label.rfind(" of "); moves_mod && of != std::string::npos)
        label = label.substr(of + 4);
      if (const size_t end = label.find_first_of("(,."); moves_mod && end != std::string::npos)
        label = label.substr(0, end);  // ("John Cena from SvR 2010 (the player's own copy), made with ...")
      while (!label.empty() && label.back() == ' ')
        label.pop_back();
      if (label.empty())
        label = "MOD " + std::to_string(own);
      for (char &c : label)
        c = char(toupper(uint8_t(c)));
      if (std::none_of(g_mod_entrances.begin(), g_mod_entrances.end(), [&](const auto &e) { return e.first == own; }))
        g_mod_entrances.push_back({own, label});
    } else {
      continue;
    }
    for (fs::directory_iterator it(d / "superstar" / "entrance", ec), end; !ec && it != end; it.increment(ec)) {
      const std::string f = it->path().filename().string();
      const size_t bar = f.find('_');
      // (only "<KIND>_<number>.bin": a copy beside it, "EVZ_497.bin.bak", was served too, and in its place, 2026-11-02)
      if (bar == std::string::npos || f.size() < bar + 6 || f.compare(f.size() - 4, 4, ".bin") != 0 ||
          f.find_first_not_of("0123456789", bar + 1) != f.size() - 4)
        continue;
      const std::string kind = f.substr(0, bar);
      const int number = atoi(f.c_str() + bar + 1);
      Part part;
      std::string rel;
      if (kind == "EVP" || kind == "EVZ") {
        const int file = EntranceFile(number);
        if (file < 0)
          continue;
        char r[32];
        snprintf(r, sizeof(r), "pac/evt/Nyujyo%c.pac", file < 10 ? char('0' + file) : 'A');
        rel = r;
        part.group = kind + kGroups[file];
        part.name = std::to_string(number);
      } else if (kind == "GMEV" || kind == "PCE6" || kind == "LPE6") {  // (LPE6: the effects' second set, asked too)
        char n[8];
        snprintf(n, sizeof(n), "%04d", number % 10000);
        rel = kind == "GMEV" ? "pac/gm.pac" : "pac/prtcls.pac";
        part.group = kind;
        part.name = n;
      } else {
        continue;
      }
      if (!ReadHost(it->path(), part.data) || part.data.empty())
        continue;
      parts[rel].push_back(std::move(part));
    }
  }
  // The media mods' ribbons and walls of the game's superstars (media/screens/ribbon_NNN.bik, wall_NNN.bik: NNN its
  // video number), in place of the superstar's own, or added (then sub_82A31578 says it has one:
  // wwe12_superstar_mods.cpp).
  {
    std::vector<fs::path> media;
    for (fs::directory_iterator it(mods / ".svrmods" / "Media", ec), end; !ec && it != end; it.increment(ec))
      if (it->is_directory(ec) && !fs::exists(it->path() / "disabled", ec))
        media.push_back(it->path());
    ec.clear();
    std::sort(media.begin(), media.end());
    for (const auto &d : media)
      for (fs::directory_iterator it(d / "media" / "screens", ec), end; !ec && it != end; it.increment(ec)) {
        const std::string f = it->path().filename().string();
        const bool ribbon = f.rfind("ribbon_", 0) == 0, wall = f.rfind("wall_", 0) == 0;
        const int n = atoi(f.c_str() + (ribbon ? 7 : 5));
        if ((!ribbon && !wall) || n <= 0 || n > 999)
          continue;
        Part part;
        if (!ReadHost(it->path(), part.data) || part.data.empty())
          continue;
        char name[8];
        snprintf(name, sizeof(name), "M%03d", n);
        part.group = ribbon ? "MVSB" : "MVSW";
        part.name = name;
        part.replace = true;
        parts["pac/evt/evtmov.pac"].push_back(std::move(part));
        g_mod_screens.insert({ribbon ? 1u : 2u, uint32_t(n)});
      }
    ec.clear();
  }
  // The attire mods' entrance scripts (attire\entrance\EVP_<n>.bin: the superstar's script and its segments 1x<n>
  // with the props of his entrance outfits taken out, entrance13::OutfitProps: Cena's cap over a ported attire) in
  // place of the game's /EVP*/<n>, wherever it is (Nyujyo0-A.pac; the first mod giving one wins). (Their earlier
  // attire\objects\SOBJ_<model>.bin, invisible props in evtobj.pac, froze the match's loading: ignored.)
  {
    std::vector<fs::path> attires;
    for (fs::directory_iterator it(mods / ".svrmods" / "Attires", ec), end; !ec && it != end; it.increment(ec))
      if (it->is_directory(ec) && !fs::exists(it->path() / "disabled", ec) &&
          fs::is_directory(it->path() / "attire" / "entrance", ec))
        attires.push_back(it->path());
    ec.clear();
    std::sort(attires.begin(), attires.end());
    std::map<std::string, std::pair<std::string, std::string>> where;  // (name -> file, group) of the game's scripts
    if (!attires.empty())
      for (const char *f : {"pac/evt/Nyujyo0.pac", "pac/evt/Nyujyo1.pac", "pac/evt/Nyujyo2.pac", "pac/evt/Nyujyo3.pac",
                            "pac/evt/Nyujyo4.pac", "pac/evt/Nyujyo5.pac", "pac/evt/Nyujyo6.pac", "pac/evt/Nyujyo7.pac",
                            "pac/evt/Nyujyo8.pac", "pac/evt/Nyujyo9.pac", "pac/evt/NyujyoA.pac"}) {
        std::vector<uint8_t> pac;
        svrfmt::Archive a;
        if (!ReadForGame(vfs, mods, f, pac) || !svrfmt::ArchiveOpen(pac, a, nullptr))
          continue;
        for (const auto &k : a.root.kids)
          if (const std::string g = Trimmed(k.group); g.size() == 4 && g.rfind("EVP", 0) == 0)
            where.emplace(Trimmed(k.name), std::make_pair(std::string(f), g));
      }
    std::set<std::string> given;
    for (const auto &d : attires) {
      for (fs::directory_iterator it(d / "attire" / "entrance", ec), end; !ec && it != end; it.increment(ec)) {
        const std::string f = it->path().filename().string();
        const std::string name = f.rfind("EVP_", 0) == 0 && f.size() > 8 ? std::to_string(atoi(f.c_str() + 4)) : "";
        const auto at = where.find(name);
        Part part;
        if (at == where.end() || given.count(name) || !ReadHost(it->path(), part.data) || part.data.empty())
          continue;
        given.insert(name);
        part.group = at->second.second;
        part.name = name;
        part.replace = true;
        parts[at->second.first].push_back(std::move(part));
      }
      ec.clear();
    }
  }
  // Title belts (pac/wuniData.pac /WWEU/BLTS: a PACH of texts by superstar number, two lines "x,y,z,sx,sy,sz": the belt
  // on the shoulder, then at the waist). An added superstar had the text of whoever had his number before (Jeff Hardy
  // at 109: the belt went through his clothes at the waist, 2026-11-03): he gets his base's, its waist's last two
  // scales times how much wider and deeper his model's waist is than the base's (Waist). Documents\wwe12\belts.txt
  // ("<number>=<line 1>|<line 2>") overrides one, to try values without a build.
  if (REXCVAR_GET(wwe12_belt_fit) || REXCVAR_GET(wwe12_belt_length)) {
    std::vector<uint8_t> wuni;
    svrfmt::Archive a;
    svrfmt::ArcNode *blts = nullptr;
    if (ReadForGame(vfs, mods, "pac/wuniData.pac", wuni) && svrfmt::ArchiveOpen(wuni, a, nullptr))
      for (auto &k : a.root.kids)
        if (Trimmed(k.group) == "WWEU" && Trimmed(k.name) == "BLTS")
          blts = &k;
    // (a PACH of one child, id 0, the PACH of the texts)
    std::vector<uint8_t> t = blts ? svrfmt::ArchiveData(*blts) : std::vector<uint8_t>();
    std::map<uint32_t, std::string> texts;
    const auto u32le = [&](size_t o) { return uint32_t(t[o]) | uint32_t(t[o + 1]) << 8 | uint32_t(t[o + 2]) << 16 | uint32_t(t[o + 3]) << 24; };
    if (t.size() >= 28 && !memcmp(t.data(), "PACH", 4) && u32le(4) == 1 && !memcmp(t.data() + 20 + u32le(12), "PACH", 4)) {
      const size_t at = 20 + u32le(12), size = u32le(16);
      t = at + size <= t.size() ? std::vector<uint8_t>(t.begin() + long(at), t.begin() + long(at + size)) : std::vector<uint8_t>();
    }
    if (t.size() >= 8 && !memcmp(t.data(), "PACH", 4)) {
      const uint32_t n = u32le(4);
      const size_t data = 8 + size_t(n) * 12;
      for (uint32_t i = 0; i < n && data <= t.size() && 8 + size_t(i + 1) * 12 <= t.size(); ++i) {
        const uint32_t id = u32le(8 + i * 12), off = u32le(12 + i * 12), size = u32le(16 + i * 12);
        if (data + off + size <= t.size())
          texts[id] = std::string(t.begin() + long(data + off), t.begin() + long(data + off + size));
      }
    }
    std::map<uint32_t, std::string> mine;
    for (const auto &[d, moves_mod] : dirs) {
      std::vector<uint8_t> m;
      const auto slot = slots.find(d.filename().string());
      if (moves_mod || slot == slots.end() || fs::exists(d / "disabled", ec) || !ReadHost(d / "manifest.txt", m))
        continue;
      const int base = atoi(Value(std::string(m.begin(), m.end()), "base").c_str());
      const auto from = texts.find(uint32_t(base));
      if (from == texts.end()) {
        REXLOG_INFO("[belts] superstar {}: no belt text for its base {}", slot->second, base);
        continue;
      }
      std::string text = from->second;
      std::vector<uint8_t> own, theirs;
      float w = 0, dp = 0, bw = 0, bd = 0;
      if (ReadHost(d / "superstar" / "ch.pac", own) && Waist(own, w, dp) &&
          ReadForGame(vfs, mods, "pac/ch/ch" + std::to_string(base) + ".pac", theirs) && Waist(theirs, bw, bd) && bw > 0 &&
          bd > 0) {
        // (the waist line's last two numbers, scaled: the ratios kept within 0.85-1.5)
        const size_t nl = text.find('\n');
        std::vector<std::string> v;
        if (nl != std::string::npos) {
          std::string line = text.substr(nl + 1), cell;
          for (char c : line + ",")
            if (c == ',') {
              v.push_back(cell);
              cell.clear();
            } else if (c != '\r' && c != '\n') {
              cell += c;
            }
        }
        if (v.size() == 6) {
          // (the three scales, by the larger ratio)
          const float rw = std::clamp(w / bw, 0.85f, 1.5f), rd = std::clamp(dp / bd, 0.85f, 1.5f);
          const float k = std::max(rw, rd);
          if (k > 1.02f)
            g_belt_length[uint32_t(slot->second)] = k;
          for (int i = 3; i < 6; ++i) {
            char s[32];
            snprintf(s, sizeof(s), "%g", strtof(v[size_t(i)].c_str(), nullptr) * k);
            v[size_t(i)] = s;
          }
          text = text.substr(0, nl + 1) + v[0] + "," + v[1] + "," + v[2] + "," + v[3] + "," + v[4] + "," + v[5];
          REXLOG_INFO("[belts] superstar {}: base {}'s belt, waist {:.0f} x {:.0f} against {:.0f} x {:.0f}: x{:.2f}",
                      slot->second, base, w, dp, bw, bd, k);
        }
      }
      mine[uint32_t(slot->second)] = text;
    }
    {
      std::vector<uint8_t> o;
      if (ReadHost(user_root / "belts.txt", o)) {
        std::string all(o.begin(), o.end()), line;
        for (char c : all + "\n")
          if (c == '\n') {
            const size_t eq = line.find('='), bar = line.find('|');
            if (eq != std::string::npos && bar != std::string::npos && bar > eq) {
              mine[uint32_t(atoi(line.c_str()))] = line.substr(eq + 1, bar - eq - 1) + "\r\n" + line.substr(bar + 1);
              REXLOG_INFO("[belts] superstar {}: belts.txt's", atoi(line.c_str()));
            }
            line.clear();
          } else if (c != '\r') {
            line += c;
          }
      }
    }
    REXLOG_INFO("[belts] {} belt texts in the game's table ({}), {} for the added superstars", texts.size(),
                blts ? "found" : "not found", mine.size());
    if (REXCVAR_GET(wwe12_belt_fit) && !texts.empty() && !mine.empty()) {
      // (and its attires' lines, 1000 x attire + number: they come first, 1109 the same as 109 for the slot's old
      // owner, and the belt didn't change while only 109 did, 2026-11-03)
      for (const auto &[id, text] : mine)
        for (uint32_t a = 0; a <= 5; ++a)
          if (a == 0 || texts.count(a * 1000 + id))
            texts[a * 1000 + id] = text;
      std::vector<uint8_t> out = {'P', 'A', 'C', 'H'}, body;
      const auto put = [](std::vector<uint8_t> &b, uint32_t x) {
        for (int i = 0; i < 4; ++i)
          b.push_back(uint8_t(x >> (8 * i)));
      };
      put(out, uint32_t(texts.size()));
      for (const auto &[id, text] : texts) {
        while (body.size() % 4)
          body.push_back(0);
        put(out, id);
        put(out, uint32_t(body.size()));
        put(out, uint32_t(text.size()));
        body.insert(body.end(), text.begin(), text.end());
      }
      out.insert(out.end(), body.begin(), body.end());
      std::vector<uint8_t> outer = {'P', 'A', 'C', 'H'};
      put(outer, 1);
      put(outer, 0);
      put(outer, 0);
      put(outer, uint32_t(out.size()));
      outer.insert(outer.end(), out.begin(), out.end());
      Part part;
      part.group = "WWEU";
      part.name = "BLTS";
      part.data = std::move(outer);
      part.replace = true;
      parts["pac/wuniData.pac"].push_back(std::move(part));
    }
  }
  for (auto &[rel, list] : parts) {
    std::vector<uint8_t> pac;
    svrfmt::Archive a;
    // (evtmov.pac with an added arena's transition video: that copy, served over this folder's, Wwe12MountArenaSlots,
    // which runs first; the ribbons and walls appended to it, in its place. Appended to the disc's here, they were
    // answered past the end of the arena's copy and the loading before such a superstar's entrance waited forever:
    // Seth Rollins with the RAW (SvR 2007) arena added, 2026-11-04)
    // (only when that copy is served: a copy left by an arena mod since removed isn't, and 2K14 Antonio Cesaro's wall
    // was appended to it, answered past the end of the disc's evtmov.pac the game read: endless loading before his
    // entrance, 2026-11-07. Such a copy is removed.)
    const fs::path arena_copy = out.parent_path() / "arena_movies" / fs::path(rel);
    if (rel == "pac/evt/evtmov.pac" && !g_arena_movies_served && fs::exists(arena_copy, ec)) {
      std::error_code e4;
      fs::remove_all(out.parent_path() / "arena_movies", e4);
      REXLOG_INFO("[entrances] arena_movies not served this time (no added arena with videos): removed");
    }
    const bool over_arena = rel == "pac/evt/evtmov.pac" && g_arena_movies_served && fs::exists(arena_copy, ec);
    if (!(over_arena ? ReadHost(arena_copy, pac) : ReadForGame(vfs, mods, rel, pac)) || !svrfmt::ArchiveOpen(pac, a, nullptr)) {
      REXLOG_WARN("[entrances] {} can't be read: its added entrances left out", rel);
      continue;
    }
    // the game's entries of this file, by group and name; whether each is stored BPE-packed
    std::map<std::string, std::vector<std::string>> names;
    std::set<std::string> packed;
    for (const auto &k : a.root.kids) {
      names[Trimmed(k.group)].push_back(Trimmed(k.name));
      if (svrfmt::IsBpe(k.stored))
        packed.insert("/" + Trimmed(k.group) + "/" + Trimmed(k.name));
    }
    a = {};
    const std::string lead = rel == "pac/evt/evtmov.pac" ? "" : "/";  // (its names are asked "MVSB/M261")
    for (auto &part : list) {
      const std::string &group = part.group, &name = part.name;
      const auto &have = names[group];
      if (std::find(have.begin(), have.end(), name) != have.end()) {
        if (part.replace) {  // (in its place: the directory's answer gets this one's place and size)
          // (stored as the entry it replaces: a moves mod's victory scene in place of a BPE-packed Winning entry)
          if (!lead.empty() && packed.count(lead + group + "/" + name) && !svrfmt::IsBpe(part.data))
            part.data = svrfmt::BpeEncode(part.data);
          AddedEntrance e;
          Append(pac, part.data.data(), part.data.size(), e.sector, e.units);
          part.data = {};
          g_replaced[lead + group + "/" + name] = e;
          if (rel == "pac/evt/evtobj.pac")  // (asked both ways, as the added objects below)
            g_replaced[group + "/" + name] = e;
          REXLOG_INFO("[entrances] {}{}/{} replaced by a mod's", lead, group, name);
        } else {
          REXLOG_WARN("[entrances] /{}/{} is the game's already: left out", group, name);
        }
        continue;
      }
      // (one of its kind: a numbered name for a numbered one; evtobj.pac's SOBJ/LIST, BPE-packed, lent its packing to
      // added objects and the game crashed reading them, 2026-11-02)
      const auto digits = [](const std::string &t) {
        return !t.empty() && std::all_of(t.begin(), t.end(), [](char c) { return c >= '0' && c <= '9'; });
      };
      std::string like;
      for (const auto &h : have)
        if (h.size() == name.size() && digits(h) == digits(name)) {
          like = lead + group + "/" + h;
          break;
        }
      if (like.empty())
        continue;
      AddedEntrance e;
      e.like = like;
      // Stored as the entry it borrows the place of: WWE '12's Nyujyo entries are BPE-packed (Mark Henry's script, 63
      // KB, is 3 KB there); a WWE '13 script served unpacked was never played, the superstar never showed
      // (2026-11-01). gm.pac's GMEV and prtcls.pac's PCE6 are stored as they are.
      if (packed.count(like) && !svrfmt::IsBpe(part.data))
        part.data = svrfmt::BpeEncode(part.data);
      Append(pac, part.data.data(), part.data.size(), e.sector, e.units);
      part.data = {};
      g_entrances[lead + group + "/" + name] = e;
      if (rel == "pac/evt/evtobj.pac")  // (how the game asks its objects' names isn't known yet: both ways)
        g_entrances[group + "/" + name] = e;
    }
    if (over_arena) {
      WriteHost(arena_copy, pac);
      std::error_code e3;  // (no longer the arena's alone: made again from the disc's next time)
      fs::remove(out.parent_path() / "arena_movies" / "key.txt", e3);
    } else {
      WriteHost(out / fs::path(rel), pac);
    }
  }
  if (g_entrances.empty() && g_replaced.empty())
    return;
  auto overlay = std::make_unique<rex::filesystem::HostPathDevice>(game_device, out, true);
  if (!overlay->Initialize() || !vfs->RegisterOverlay(game_device, std::move(overlay))) {
    REXLOG_WARN("[entrances] could not serve {}", rex::path_to_utf8(out));
    g_entrances.clear();
    g_replaced.clear();
    g_mod_screens.clear();
    return;
  }
  REXLOG_INFO("[entrances] {} added entrance entries, {} replaced, served from {}", g_entrances.size(), g_replaced.size(),
              rex::path_to_utf8(out));
}

}  // namespace

// Whether a media mod has a ribbon (kind 1) / wall (kind 2) for video number n (wwe12_superstar_mods.cpp).
bool Wwe12ModScreen(uint32_t kind, uint32_t n) { return g_mod_screens.count({kind, n}) != 0; }

// (after the superstar mods: their slots and video numbers, superstar_slots.txt / superstar_movies.txt, are this
// start's; mounted with the arena slots, before them, an added superstar had his entrance, victory, ribbon and wall
// only from the next start, 2026-11-02)
void Wwe12MountEntrances(rex::filesystem::VirtualFileSystem *vfs, const std::string &game_device,
                         const fs::path &user_root, const fs::path &mods) {
  if (!user_root.empty())
    MountEntrances(vfs, game_device, user_root, mods);
}

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
    // (the game's own transitions: 870 KB at most, frames of 47 KB at most. The game takes about three times the
    // largest frame from a small heap: a Mod Maker SvR 2007 transition with 131 KB frames, 1.95 MB, crashed the game
    // after the match - 393 KB not found in heap 54, 2026-11-05. Bigger ones are left out: the base's plays.)
    if (a.movie.size() >= 16) {
      const uint32_t largest = uint32_t(a.movie[12]) | uint32_t(a.movie[13]) << 8 | uint32_t(a.movie[14]) << 16;
      if (largest > 50000 || a.movie.size() > 1000000) {
        REXLOG_WARN("[arena pages] {}: its transition is too big for the game ({} bytes, a frame of {}): the base's plays",
                    a.id, a.movie.size(), largest);
        a.movie.clear();
      }
    }
    // (its screens' movies, a wall and a ribbon, as the game's own walls (MVSW, 256 x 256) and ribbons (MVAB, MVSB,
    // 1024 x 32): 1.8 MB, frames of 32 KB at most; WWE 2K14's arenas show theirs in the entrances too)
    for (auto [file, m, max_frame, max_size] :
         {std::tuple{"screens.bik", &a.screens, 33000u, 1810000u}, std::tuple{"ribbon.bik", &a.ribbon, 33000u, 1810000u}}) {
      if (!ReadHost(d / "arena" / file, *m))
        continue;
      const uint32_t largest = m->size() >= 16 ? uint32_t((*m)[12]) | uint32_t((*m)[13]) << 8 | uint32_t((*m)[14]) << 16 : ~0u;
      if (memcmp(m->data(), "BIK", 3) != 0 || largest > max_frame || m->size() > max_size) {
        REXLOG_WARN("[arena pages] {}: its {} isn't one the game takes ({} bytes, a frame of {}): the base's plays", a.id,
                    file, m->size(), largest);
        m->clear();
      }
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
  for (const bool hd : {true, false}) {  // (HD first: it names the banners)
    std::vector<uint8_t> match;
    if (!ReadForGame(vfs, mods, hd ? "pac/menu/MatchHD.pac" : "pac/menu/MatchSD.pac", match) ||
        !BuildBanners(match, banners, hd, hd ? g_mupa_sector : g_mupa_sd_sector, hd ? g_mupa_units : g_mupa_sd_units))
      continue;
    for (auto &a : g_added) {
      if (a.card_mb.empty())
        continue;
      Append(match, a.card_mb.data(), a.card_mb.size(), hd ? a.mb_sector : a.mb_sd_sector,
             hd ? a.mb_units : a.mb_sd_units);
      Append(match, a.card_mf.data(), a.card_mf.size(), hd ? a.mf_sector : a.mf_sd_sector,
             hd ? a.mf_units : a.mf_sd_units);
    }
    WriteHost(out / "pac" / "menu" / (hd ? "MatchHD.pac" : "MatchSD.pac"), match);
  }
  for (auto &a : g_added) {
    a.card_mb = {};
    a.card_mf = {};
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
  const fs::path movies = user_root / "arena_movies";
  const bool serve_movies = ServeMovies(vfs, mods, movies);
  auto overlay = std::make_unique<rex::filesystem::HostPathDevice>(game_device, out, true);
  if (!overlay->Initialize() || !vfs->RegisterOverlay(game_device, std::move(overlay))) {
    REXLOG_WARN("[arena pages] could not serve {}", rex::path_to_utf8(out));
    g_added.clear();
    return;
  }
  if (serve_movies) {
    auto movie_overlay = std::make_unique<rex::filesystem::HostPathDevice>(game_device, movies, true);
    if (!movie_overlay->Initialize() || !vfs->RegisterOverlay(game_device, std::move(movie_overlay))) {
      REXLOG_WARN("[arena pages] could not serve {}", rex::path_to_utf8(movies));
      for (auto &a : g_added)
        a.movie_units = 0;
    } else {
      g_arena_movies_served = true;
    }
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
  g_label_page = page;
  g_label_pages = int(g_pages.size());
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
REX_EXTERN(__imp__sub_823BEED0);  // a stage object: loads its arena ("/STG/%04d" of its +88)
REX_EXTERN(__imp__sub_825A3318);  // arena select: is this stage available (this, stage)
REX_EXTERN(__imp__sub_825A56C8);  // arena select: the chosen stage (this)
REX_EXTERN(__imp__sub_827F29F0);  // file directory: find "/GRP/NAME" (directory, name, out)
REX_EXTERN(__imp__sub_828A9898);  // match setup: its object (+88: the stage set)
REX_EXTERN(__imp__sub_8253A028);  // menus: the selected entry's record (menu)
REX_EXTERN(sub_82A8C070);         // XInputGetState(user, state) (hooked by wwe12_intros.cpp: call the hooked one)

namespace {
u32 g_screen = 0;     // the arena select screen (its constructor's this)
u32 g_pad_state = 0;  // guest X_INPUT_STATE for our own reads
u16 g_last_buttons = 0;
}  // namespace

bool Wwe12UniverseArenaPages();  // wwe12_universe.cpp: SHOW EDIT's ARENA row open, added arenas allowed

extern "C" REX_FUNC(sub_825A67A0) {
  g_screen = ctx.r3.u32;
  g_last_buttons = 0xFFFF;  // (a button still held from the screen before doesn't count)
  g_chosen = -1;
  if (REXCVAR_GET(wwe12_arena_pages)) {
    SaveOriginal();
    if (g_saved) {
      BuildPages();
      // In the Universe (*0x8324E51C == 1) only on SHOW EDIT's ARENA row (wwe12_universe.cpp keeps each show's added
      // arena and puts it on its base's stage before the show's matches); elsewhere there the game's arenas only.
      if (bd::mem::load<u32>(0x8324E51C) == 1 && !Wwe12UniverseArenaPages())
        g_pages.resize(1);
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
  if (ours && g_pages.size() > 1) {
    g_label_seen = NowMs();
    // the tile under the cursor: row at +72, column at +88 (sub_825A3AB8 sets them), the grid's cell (kRecords + 176,
    // 4 x 6 cells of 8 bytes) -> its tile record -> the record's stage
    const u32 row = Field(screen, 72), col = Field(screen, 88);
    int cursor = -1;
    if (row < 4 && col < 6) {
      // (the cell's first word is the tile record's address: kRecords + 8 x its index)
      const u32 at = bd::mem::load<u32>(kRecords + 176 + (row * 6 + col) * 8);
      const u32 record = at >= kRecords && at < kRecords + kTiles * 8 ? (at - kRecords) / 8 : 0xFFFFFFFFu;
      const u32 stage = record < u32(kTiles) ? bd::mem::load<u32>(kRecords + record * 8) : 0;
      if (IsAdded(stage))
        cursor = int(stage - kFirstAdded);
      static u32 last = 0xFFFFFFFFu;
      if (REXCVAR_GET(wwe12_arena_pages_test) && (row << 8 | col) != last) {
        last = row << 8 | col;
        REXLOG_INFO("[arena pages] cursor row {} column {}: record {}, stage {}", row, col, record, stage);
      }
    }
    g_label_cursor = cursor;
    if (!g_label_open.exchange(true))
      OpenPageLabel();
  }
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

// The game's stage object loads its arena (stage at +88) only when the stage asked differs from the one it holds: a
// second match on the same stage number kept the arena of the first, an added arena after its base (or the base
// after it, or another added arena on the same base) showing the arena loaded before with the new one's pictures
// (WrestleMania 24 then 25 on WM 27's stage, a player's report; RAW then RAW 2002: no load at all in the second
// match, 2026-11-06). Kept: the object (the same one from match to match) and what it holds (-1: the game's arena).
u32 g_stage_object = 0;
int g_stage_holds = -1;
extern "C" REX_FUNC(sub_823BEED0) {
  g_stage_object = ctx.r3.u32;
  g_stage_holds = g_chosen;
  if (REXCVAR_GET(wwe12_dir_log))
    REXLOG_INFO("[arena pages] stage object {:08X} loads stage {}", ctx.r3.u32, bd::mem::load<u32>(ctx.r3.u32 + 88));
  __imp__sub_823BEED0(ctx, base);
}

// After a choice: if the stage object holds the chosen stage number with another arena in it, it forgets its stage
// (-1), so the next match loads it again.
void ForgetHeldArena(u32 stage) {
  if (!g_stage_object || g_stage_holds == g_chosen || bd::mem::load<u32>(g_stage_object + 88) != stage)
    return;
  bd::mem::store<u32>(g_stage_object + 88, 0xFFFFFFFFu);
  REXLOG_INFO("[arena pages] stage {} held another arena: loaded again", stage);
}

// The chosen arena: an added one plays as its base (and its file is served, see the directory below).
extern "C" REX_FUNC(sub_825A56C8) {
  __imp__sub_825A56C8(ctx, base);
  const u32 stage = ctx.r3.u32;
  if (IsAdded(stage)) {
    const int n = int(stage - kFirstAdded);
    if (g_chosen != n) {
      REXLOG_INFO("[arena pages] chosen: {} ({}), plays as stage {}", g_added[size_t(n)].name, g_added[size_t(n)].id,
                  g_added[size_t(n)].base);
      g_reload_vs = true;
    }
    g_chosen = n;
    Serve(g_added[size_t(n)].base, n);
    ForgetHeldArena(g_added[size_t(n)].base);
    ctx.r3.u64 = g_added[size_t(n)].base;
  } else if (stage < kFirstAdded) {
    if (g_chosen >= 0)
      g_reload_vs = true;  // (from an added arena back to the game's)
    g_chosen = -1;
    Serve(stage, -1);  // (one of the game's: its own file back, if an added arena was in it)
    ForgetHeldArena(stage);
  }
}

// After choosing, the match setup (sub_82576120) compares the stage chosen with the one it has (+88 of the object
// sub_828A9898 gives) and sets it again, its VS screen with it (sub_828AC150), only when they differ: an added arena
// plays on its base's stage number, so choosing it after its base (or the base after it) kept the VS screen shown
// (WrestleMania 31 showed Superstars', 2026-10-26). After such a choice (g_reload_vs, set by sub_825A56C8), the
// next sub_828A9898 forgets the stage it had (-1): the two differ.
extern "C" REX_FUNC(sub_828A9898) {
  __imp__sub_828A9898(ctx, base);
  if (g_reload_vs && ctx.r3.u32) {
    g_reload_vs = false;
    bd::mem::store<u32>(ctx.r3.u32 + 88, 0xFFFFFFFFu);
  }
}

// A menu's selected entry (sub_8253A028(menu) -> record: +8 page), asked when an entry is decided. A main menu
// entry (page 1: a mode) forgets the added arena chosen before: until then it kept playing on its base's stage, in
// a Universe match on that stage too.
extern "C" REX_FUNC(sub_8253A028) {
  __imp__sub_8253A028(ctx, base);
  const u32 entry = ctx.r3.u32;
  if (g_chosen >= 0 && entry && entry != 0xFFFFFFFFu && bd::mem::load<u32>(entry + 8) == 1) {
    REXLOG_INFO("[arena pages] main menu: {} no longer chosen", g_added[size_t(g_chosen)].id);
    const u32 held = g_added[size_t(g_chosen)].base;
    Serve(held, -1);
    g_chosen = -1;
    ForgetHeldArena(held);
    g_reload_vs = true;
  }
}

// 4:3: the directory's file numbers are the SD files' (plist360_4x3.arc).
static bool FourThree() { return bd::mem::load<u32>(0x8324CE3C) != 0; }

// The chosen added arena's VS screen in the served MatchHD.pac / MatchSD.pac: {mb sector, units, mf sector, units}.
static std::array<u32, 4> Card(const Added &a) {
  return FourThree() ? std::array<u32, 4>{a.mb_sd_sector, a.mb_sd_units, a.mf_sd_sector, a.mf_sd_units}
                     : std::array<u32, 4>{a.mb_sector, a.mb_units, a.mf_sector, a.mf_units};
}

// The file directory: the banner pack and the chosen added arena come from the data appended to their files.
extern "C" REX_FUNC(sub_827F29F0) {
  const u32 directory = ctx.r3.u32, name = ctx.r4.u32, out = ctx.r5.u32;
  // An added arena's VS screen: the caller's "/MLYE/MB<base>" rewritten for good to a VS screen entry no arena
  // shows, MB21 / MB22 (the arena select has stages 0-20), one per added arena. The game keeps what it has read of an
  // entry: under its base's entry, after its base's VS screen had been shown, it got that one back (WrestleMania 31
  // showed Superstars', 2026-10-26; the directory answered, the data was the old). Answered below with its own.
  static constexpr u32 kSpareMb[] = {21, 22};
  if (name && (name < 0x82000000 || name >= 0x84000000) && g_chosen >= 0 && Card(g_added[size_t(g_chosen)])[1]) {
    // (a string the game built, never one of its image's: "/MLYE/MB00" is there)
    char mb[16], spare[16];
    snprintf(mb, sizeof(mb), "/MLYE/MB%02u", g_added[size_t(g_chosen)].base);
    snprintf(spare, sizeof(spare), "/MLYE/MB%02u", kSpareMb[size_t(g_chosen) % std::size(kSpareMb)]);
    if (strcmp(bd::mem::str(name), mb) == 0)
      for (size_t i = 0; i <= strlen(spare); ++i)
        bd::mem::store<u8>(name + u32(i), u8(spare[i]));
  }
  // An added superstar's own ribbon / wall: the lookup of its base's (a string the game built) renamed for good to
  // its own, which the added entries below answer.
  if (name && (name < 0x82000000 || name >= 0x84000000) && !g_entrances.empty()) {
    const std::string asked = bd::mem::str(name), own = Wwe12OwnScreen(asked, ctx, base);
    if (!own.empty() && own.size() == asked.size())
      for (size_t i = 0; i <= own.size(); ++i)
        bd::mem::store<u8>(name + u32(i), u8(own[i]));
  }
  __imp__sub_827F29F0(ctx, base);
  if (REXCVAR_GET(wwe12_dir_log) && name)
    REXLOG_INFO("[dir] {} {} out {:08X} {:08X} {:08X} {:08X} {:08X} {:08X}", bd::mem::str(name), ctx.r3.u32 ? "found" : "MISSING",
                out ? bd::mem::load<u32>(out) : 0, out ? bd::mem::load<u32>(out + 4) : 0, out ? bd::mem::load<u32>(out + 8) : 0,
                out ? bd::mem::load<u32>(out + 12) : 0, out ? bd::mem::load<u32>(out + 16) : 0, out ? bd::mem::load<u32>(out + 20) : 0);
  // A replaced entry (a media mod's ribbon / wall, a moves mod's victory scene): the answer gets its place and size.
  if (name && out && ctx.r3.u32 && !g_replaced.empty() && (name < 0x82000000 || name >= 0x84000000)) {
    const auto r = g_replaced.find(bd::mem::str(name));
    if (r != g_replaced.end()) {
      bd::mem::store<u32>(out, r->second.sector);
      bd::mem::store<u32>(out + 8, r->second.units);
      REXLOG_INFO("[entrances] {}: a mod's own", r->first);
    }
  }
  // An added entrance the directory hasn't: asked again as an entry of the same file and name length, then its own
  // place (a string the game built: written for the call, put back after).
  if (name && out && ctx.r3.u32 == 0 && !g_entrances.empty() && (name < 0x82000000 || name >= 0x84000000)) {
    const std::string asked = bd::mem::str(name);
    const auto e = g_entrances.find(asked);
    if (e != g_entrances.end() && e->second.like.size() == asked.size()) {
      for (size_t i = 0; i <= asked.size(); ++i)
        bd::mem::store<u8>(name + u32(i), u8(e->second.like[i]));
      ctx.r3.u32 = directory;
      ctx.r4.u32 = name;
      ctx.r5.u32 = out;
      __imp__sub_827F29F0(ctx, base);
      for (size_t i = 0; i <= asked.size(); ++i)
        bd::mem::store<u8>(name + u32(i), u8(asked[i]));
      if (ctx.r3.u32) {
        bd::mem::store<u32>(out, e->second.sector);
        bd::mem::store<u32>(out + 8, e->second.units);
        // The answer keeps the entry's name at +14 (after its file index): the asked one, not the borrowed one, or the
        // game took it for that other entry and the superstar never showed (2026-10-28).
        for (size_t i = 0; i <= asked.size(); ++i)
          bd::mem::store<u8>(out + 14 + u32(i), u8(asked[i]));
        REXLOG_INFO("[entrances] {}: the added entrance (as {}) out {:08X} {:08X} {:08X} {:08X} {:08X} {:08X}", asked, e->second.like,
                    bd::mem::load<u32>(out), bd::mem::load<u32>(out + 4), bd::mem::load<u32>(out + 8), bd::mem::load<u32>(out + 12),
                    bd::mem::load<u32>(out + 16), bd::mem::load<u32>(out + 20));
      }
      return;
    }
  }
  if (!name || !out || ctx.r3.u32 == 0)
    return;
  const char *n = bd::mem::str(name);
  const bool sd = FourThree();
  // A menu pictures mod's layout (the Universe's logos, match-up screens, arena photos): its dressed copy, appended to
  // the served MnLayHD.pac / MnLaySD.pac (wwe12_superstar_mods.cpp).
  {
    u32 sector = 0, units = 0;
    if (strncmp(n, "/MLY", 4) == 0 && Wwe12MenuPictureEntry(n, sd, sector, units)) {
      const u16 file = bd::mem::load<u16>(out + 12);
      if (file == kMnLayHdIndex) {
        bd::mem::store<u32>(out, sector);
        bd::mem::store<u32>(out + 8, units);
      }
      static int logged = 0;
      if (logged++ < 64)
        REXLOG_INFO("[menu pictures] {} (file {}): {}", n, file,
                    file == kMnLayHdIndex ? "the mod's" : "another file, kept");
      return;
    }
  }
  if ((sd ? g_mupa_sd_units : g_mupa_units) && strcmp(n, "/MLYE/MUPA") == 0 &&
      bd::mem::load<u16>(out + 12) == kMatchHdIndex) {
    bd::mem::store<u32>(out, sd ? g_mupa_sd_sector : g_mupa_sector);
    bd::mem::store<u32>(out + 8, sd ? g_mupa_sd_units : g_mupa_units);
  } else if (g_chosen >= 0 && strncmp(n, "/STG/", 5) == 0 && u32(atoi(n + 5)) == g_added[size_t(g_chosen)].base &&
             n[5] >= '0' && n[5] <= '9') {
    // The added arena is the served file now: its own start, and room for the bigger of the two. Never less than
    // the game's size: the arena's packed entries are unpacked in place in that room, which needs the slack.
    const Added &a = g_added[size_t(g_chosen)];
    bd::mem::store<u32>(out, a.sector);
    bd::mem::store<u32>(out + 8, std::max(a.units, bd::mem::load<u32>(out + 8)));
    REXLOG_INFO("[arena pages] {}: the added arena {} ({} KB)", n, a.id, a.units / 4);
  } else if (g_chosen >= 0 && Card(g_added[size_t(g_chosen)])[1] && bd::mem::load<u16>(out + 12) == kMatchHdIndex) {
    // Its own VS screen, in place of its base's.
    const Added &a = g_added[size_t(g_chosen)];
    const auto card = Card(a);
    char mb[16], mf[16];
    snprintf(mb, sizeof(mb), "/MLYE/MB%02u", kSpareMb[size_t(g_chosen) % std::size(kSpareMb)]);
    snprintf(mf, sizeof(mf), "/MENU/M%02uF", a.base);
    const bool is_mb = strcmp(n, mb) == 0;
    if (is_mb || strcmp(n, mf) == 0) {
      bd::mem::store<u32>(out, card[is_mb ? 0 : 2]);
      bd::mem::store<u32>(out + 8, card[is_mb ? 1 : 3]);
      REXLOG_INFO("[arena pages] {}: the VS screen of {}", n, a.id);
    }
  } else if (g_chosen >= 0 && g_added[size_t(g_chosen)].etex_units && !sd &&
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
  } else if (g_chosen >= 0 && g_added[size_t(g_chosen)].load_units && !sd &&
             bd::mem::load<u16>(out + 12) == kLoadHdIndex) {
    // Its own pre-match screen (VS screen and its animation), in place of its base's.
    const Added &a = g_added[size_t(g_chosen)];
    char load[16];
    snprintf(load, sizeof(load), "/LOAD/90%02u", a.base);
    if (strcmp(n, load) == 0) {
      bd::mem::store<u32>(out, a.load_sector);
      bd::mem::store<u32>(out + 8, a.load_units);
      REXLOG_INFO("[arena pages] {}: the pre-match screen of {}", n, a.id);
    }
  } else if (g_chosen >= 0 && bd::mem::load<u16>(out + 12) == kEvtMovIndex) {
    // Its own transition video, and its own screens' movie, in place of its base's.
    const Added &a = g_added[size_t(g_chosen)];
    char movt[16];
    snprintf(movt, sizeof(movt), "/MOVT/M%03u", a.base);
    // (its ribbons and walls in place of the arena's and every superstar's: asked "MVAB/M001", "MVSB/M110",
    // "MVSW/M414", no slash first - the dir log, 2026-11-06)
    const char *g = n[0] == '/' ? n + 1 : n;
    const bool ribbon = strncmp(g, "MVAB/M", 6) == 0 || strncmp(g, "MVSB/M", 6) == 0, wall = strncmp(g, "MVSW/M", 6) == 0;
    if (a.movie_units && strcmp(n, movt) == 0) {
      bd::mem::store<u32>(out, a.movie_sector);
      bd::mem::store<u32>(out + 8, a.movie_units);
      REXLOG_INFO("[arena pages] {}: the transition of {}", n, a.id);
    } else if (a.ribbon_units && ribbon) {
      bd::mem::store<u32>(out, a.ribbon_sector);
      bd::mem::store<u32>(out + 8, a.ribbon_units);
      REXLOG_INFO("[arena pages] {}: the ribbon of {}", n, a.id);
    } else if (a.screens_units && wall) {
      bd::mem::store<u32>(out, a.screens_sector);
      bd::mem::store<u32>(out + 8, a.screens_units);
      REXLOG_INFO("[arena pages] {}: the wall of {}", n, a.id);
    }
  }
  if (REXCVAR_GET(wwe12_arena_pages_test) &&
      (strncmp(n, "/STG", 4) == 0 || strcmp(n, "/MLYE/MUPA") == 0))
    REXLOG_INFO("[arena pages] directory {} -> file {} start {:08X} size {:08X}", n, bd::mem::load<u16>(out + 12),
                bd::mem::load<u32>(out), bd::mem::load<u32>(out + 8));
}

// Probe (2026-11-02, 2K16 / 2K17 entrances: the ring announcer and the superstar waiting stood under the mat; a crash
// in sub_8246E1D0 when objects 12+ or object 9 were made as WWE '12's: a "250NN" not found). sub_8246E1D0(r3): r3 +2 a
// count, +4 entries of 12 bytes (+4 a name 25000 + object, -1 none; +8 a frame written back), +8 a bank whose +12 is
// its first offset; each name looked up there (sub_827E6BB8) and its event tags (254, 80-95) read. Logged, then run.
REXCVAR_DEFINE_BOOL(wwe12_entrance_probe, false, "WWE12", "Log the entrance objects' event frame lookups (sub_8246E1D0)");
REX_EXTERN(__imp__sub_8246E1D0);

extern "C" REX_FUNC(sub_8246E1D0) {
  if (REXCVAR_GET(wwe12_entrance_probe)) {
    const u32 me = ctx.r3.u32, n = bd::mem::load<u16>(me + 2), entries = bd::mem::load<u32>(me + 4),
              bank = bd::mem::load<u32>(me + 8);
    std::string names, head;
    char w[16];
    for (u32 i = 0; i < n && i < 40; ++i) {
      snprintf(w, sizeof(w), " %X", bd::mem::load<u32>(entries + 12 * i + 4));
      names += w;
    }
    for (u32 o = 0; bank && o < 0x80; o += 4) {
      snprintf(w, sizeof(w), "%s%08X", o % 32 ? " " : " | ", bd::mem::load<u32>(bank + o));
      head += w;
    }
    REXLOG_INFO("[entrance probe] {:08X}: {} entries at {:08X}:{}; bank {:08X}:{}", me, n, entries, names, bank, head);
  }
  __imp__sub_8246E1D0(ctx, base);
}

// CREATE AN ENTRANCE's lists (MOTION, CHAMPION ENTRANCE...): sub_829F9F90(list, ?, table, kind, first) fills one from
// the table's entries of that kind (u16 count at table + (kind + 100) * 2, the entries at [table + (kind + 31) * 4], 6
// bytes each: s16 value - the entrance's number, + first -, s16 its text id, s16 the superstar it needs unlocked, < 0
// none), each added by sub_829FEB48(list, value, text, 0, 0, superstar) (a growing array: [list] count, [list + 8]
// items of 24 bytes, +0 the value). A mod's own entrance isn't there: the editor showed the list's first item for it
// (ALBERTO DEL RIO) and SAVE made that his entrance (2026-11-03). The lists that hold John Cena's (139) get the mods'
// own entrances too, under their names.
REX_EXTERN(__imp__sub_829F9F90);
REX_EXTERN(__imp__sub_829FEB48);

extern "C" REX_FUNC(sub_829F9F90) {
  const u32 list = ctx.r3.u32, table = ctx.r5.u32, kind = ctx.r6.u32, first = ctx.r7.u32;
  const PPCContext saved = ctx;
  __imp__sub_829F9F90(ctx, base);
  if (!table || kind > 64)
    return;
  const u32 count = bd::mem::load<u16>(table + (kind + 100) * 2), entries = bd::mem::load<u32>(table + (kind + 31) * 4);
  bool superstars = false;
  std::string seen;
  for (u32 i = 0; entries && i < count; ++i) {
    const int value = int16_t(bd::mem::load<u16>(entries + i * 6)) + int(first);
    superstars = superstars || value == 139;
    if (REXCVAR_GET(wwe12_entrance_list_log) && i < 400)
      seen += " " + std::to_string(value) + "/" + std::to_string(int16_t(bd::mem::load<u16>(entries + i * 6 + 2))) + "/" +
              std::to_string(int16_t(bd::mem::load<u16>(entries + i * 6 + 4)));
  }
  if (REXCVAR_GET(wwe12_entrance_list_log))
    REXLOG_INFO("[entrance list] list {:08X} kind {} first {}: {} entries (value/text/superstar):{}", list, kind, first,
                count, seen);
  if (!superstars || g_mod_entrances.empty() || !REXCVAR_GET(wwe12_entrance_list))
    return;
  static std::map<std::string, u32> texts;
  for (const auto &[number, label] : g_mod_entrances) {
    bool there = false;
    const u32 n = bd::mem::load<u32>(list), items = bd::mem::load<u32>(list + 8);
    for (u32 i = 0; items && i < n && !there; ++i)
      there = bd::mem::load<u32>(items + i * 24) == u32(number);
    if (there)
      continue;
    u32 &text = texts[label];
    if (!text) {
      text = REX_KERNEL_MEMORY()->SystemHeapAlloc(u32(label.size() + 1), 4);
      if (!text)
        continue;
      std::memcpy(bd::mem::at<char>(text), label.c_str(), label.size() + 1);
    }
    PPCContext c = saved;
    c.r1.u64 = (saved.r1.u32 - 1024) & ~15u;
    c.r3.u64 = list;
    c.r4.u64 = u32(number);
    c.r5.u64 = text;
    c.r6.u64 = 0;
    c.r7.u64 = 0;
    c.r8.u64 = 0xFFFFFFFFull;
    __imp__sub_829FEB48(c, base);
    REXLOG_INFO("[entrance list] kind {}: {} ({}) added", kind, label, number);
  }
}

// The title belt worn at the waist (study, 2026-11-03): sub_828C5878(this, belt) bends the belt model's bones
// (L_bone01..12, R_bone01..12, found by name) round the wearer; its length is scaled by 1 + (v - 3.8) * 0.25 when
// v = |this+24 + this+272| * 0.1 is over 3.8 (with this+1892 set), else 1.0. Probe: those fields, once per value.
REXCVAR_DEFINE_BOOL(wwe12_waist_belt_probe, false, "WWE12", "Log the waist belt's bending inputs (sub_828C5878)");
REX_EXTERN(__imp__sub_828C5878);

extern "C" REX_FUNC(sub_828C5878) {
  if (REXCVAR_GET(wwe12_waist_belt_probe)) {
    const u32 me = ctx.r3.u32, belt = ctx.r4.u32;
    const float a = bd::mem::load<float>(me + 24), b = bd::mem::load<float>(me + 272);
    const u32 flag = bd::mem::load<u32>(me + 1892);
    static std::set<std::string> seen;
    char line[256];
    snprintf(line, sizeof(line), "this %08X belt %08X +24 %.3f +272 %.3f +1892 %08X +1888 %.3f +1444 %.3f lr %08X", me,
             belt, a, b, flag, bd::mem::load<float>(me + 1888), bd::mem::load<float>(me + 1444), u32(ctx.lr));
    if (seen.insert(line).second && seen.size() < 200) {
      // (where a superstar's number sits in it: u16 / u32 equal to an added one's or John Cena's)
      std::string where;
      for (u32 o = 0; o < 0x900; o += 2) {
        const u32 v16 = bd::mem::load<u16>(me + o);
        if (v16 == 104 || v16 == 108 || v16 == 109 || v16 == 111 || v16 == 139)
          where += " +" + std::to_string(o) + "=" + std::to_string(v16);
      }
      // (and the belt's own object: where it holds three 1.0 in a row, a scale)
      std::string ones;
      for (u32 o = 0; o + 12 <= 0x600; o += 4)
        if (bd::mem::load<float>(belt + o) == 1.0f && bd::mem::load<float>(belt + o + 4) == 1.0f &&
            bd::mem::load<float>(belt + o + 8) == 1.0f)
          ones += " +" + std::to_string(o);
      REXLOG_INFO("[belt probe] {} (v {:.3f}) numbers:{} belt ones:{}", line, std::fabs(-a - b) * 0.1f, where, ones);
    }
  }
  // An added superstar wider at the waist than his base (g_belt_length) entering: the threshold the game reads (3.8,
  // read-only data at 0x8200EA04) lowered for this call, so its own formula lengthens his belt that much. (The fields
  // aren't the wearer's build: Chavo Guerrero's v 3.60, Jeff Hardy's 3.46; the belt fits WWE '12's bodies, not a
  // ported superstar's looser clothes, 2026-11-03.)
  constexpr u32 kThreshold = 0x8200EA04;
  float k = 0;
  if (REXCVAR_GET(wwe12_belt_length) && !g_belt_length.empty() && bd::mem::load<u32>(ctx.r3.u32 + 1892)) {
    const auto it = g_belt_length.find(Wwe12EnteringSuperstar(ctx, base));
    if (it != g_belt_length.end())
      k = it->second;
    if (it != g_belt_length.end() && REXCVAR_GET(wwe12_belt_length_test) > 0)
      k = float(REXCVAR_GET(wwe12_belt_length_test));
  }
  static bool writable = false, tried = false;
  if (k > 1.0f && !tried) {
    tried = true;
    DWORD old = 0;
    writable = VirtualProtect(bd::mem::at<u8>(kThreshold & ~0xFFFu), 0x1000, PAGE_READWRITE, &old) != 0;
    if (!writable)
      REXLOG_WARN("[belts] the belt length threshold can't be written: added superstars' belts left as they are");
  }
  if (REXCVAR_GET(wwe12_belt_scale_test) > 0 && g_belt_length.count(Wwe12EnteringSuperstar(ctx, base))) {
    const u32 belt = ctx.r4.u32, at = u32(REXCVAR_GET(wwe12_belt_scale_at));
    const float sc = float(REXCVAR_GET(wwe12_belt_scale_test));
    __imp__sub_828C5878(ctx, base);
    for (u32 i = 0; i < 3; ++i)
      bd::mem::store<float>(belt + at + 4 * i, sc);
    static bool told = false;
    if (!told)
      REXLOG_INFO("[belts] scale test: belt {:08X} +{} = {:.2f}", belt, at, sc);
    told = true;
    return;
  }
  if (REXCVAR_GET(wwe12_belt_radius_test) > 0 && REXCVAR_GET(wwe12_belt_length) &&
      g_belt_length.count(Wwe12EnteringSuperstar(ctx, base))) {
    const u32 me = ctx.r3.u32;
    const float r = float(REXCVAR_GET(wwe12_belt_radius_test)), a = bd::mem::load<float>(me + 24),
                b = bd::mem::load<float>(me + 272);
    const int which = REXCVAR_GET(wwe12_belt_radius_which);
    bd::mem::store<float>(me + 24, which == 2 ? a : a * r);
    bd::mem::store<float>(me + 272, which == 1 ? b : b * r);
    __imp__sub_828C5878(ctx, base);
    bd::mem::store<float>(me + 24, a);
    bd::mem::store<float>(me + 272, b);
    static bool told = false;
    if (!told)
      REXLOG_INFO("[belts] radius test x{:.2f}: +24 {:.2f}, +272 {:.2f}", r, a, b);
    told = true;
    return;
  }
  const float v = std::fabs(-bd::mem::load<float>(ctx.r3.u32 + 24) - bd::mem::load<float>(ctx.r3.u32 + 272)) * 0.1f;
  const float game = v > 3.8f ? 1.0f + (v - 3.8f) * 0.25f : 1.0f;
  if (k > game && writable) {
    const float before = bd::mem::load<float>(kThreshold);
    bd::mem::store<float>(kThreshold, v - (k - 1.0f) / 0.25f);
    __imp__sub_828C5878(ctx, base);
    bd::mem::store<float>(kThreshold, before);
    static std::set<float> told;
    if (told.insert(k).second)
      REXLOG_INFO("[belts] waist belt x{:.2f} (the game's x{:.2f})", k, game);
    return;
  }
  __imp__sub_828C5878(ctx, base);
}

// ---- the page label (after Kaiko's SvR 2011 port, arena_mods.cpp ArenaPageLabel, MIT, credited in THIRD_PARTY.md)
// Players thought their added arenas weren't there (2026-11-04): while the arena select is shown with pages, a label
// above the grid says which page it is and that LB / RB (L1 / R1 with a PlayStation pad) turn them.
bool Wwe12PlayStationIcons();  // (wwe12_buttons.cpp)

namespace {
class ArenaPageLabel final : public rex::ui::ImGuiDialog {
 public:
  explicit ArenaPageLabel(rex::ui::ImGuiDrawer *drawer) : ImGuiDialog(drawer) {}

 protected:
  void OnClose() override { g_label_open = false; }
  void OnDraw(ImGuiIO &io) override {
    const int pages = g_label_pages.load(), page = g_label_page.load();
    const int64_t idle = NowMs() - g_label_seen.load();
    if (idle > 1000) {  // (the screen left: no dialog, the overlay isn't drawn at all without one)
      Close();
      return;
    }
    if (pages < 2 || idle > 250)
      return;
    const float w = io.DisplaySize.x, h = io.DisplaySize.y;
    const float gw = std::min(w, h * 16.0f / 9.0f), gh = gw * 9.0f / 16.0f;
    const float x0 = (w - gw) * 0.5f, y0 = (h - gh) * 0.5f, scale = gh / 720.0f;
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    ImFont *font = ImGui::GetFont();
    const bool ps = Wwe12PlayStationIcons();
    const char *left = ps ? "L1" : "LB", *right = ps ? "R1" : "RB";
    char text[48];
    snprintf(text, sizeof(text), "%s   PAGE %d / %d   %s", left, page + 1, pages, right);
    const float size = 26.0f * scale;
    const ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
    const ImVec2 c(x0 + gw * 0.5f, y0 + gh * 0.085f);
    const float arrow = 10.0f * scale, gap = 18.0f * scale;
    const float half = ts.x * 0.5f + gap + arrow;
    const ImVec2 pad(14.0f * scale, 6.0f * scale);
    const ImVec2 a(c.x - half - pad.x, c.y - ts.y * 0.5f - pad.y), b(c.x + half + pad.x, c.y + ts.y * 0.5f + pad.y);
    dl->AddRectFilled(a, b, IM_COL32(10, 12, 18, 215), 6.0f * scale);
    dl->AddRect(a, b, IM_COL32(220, 220, 225, 255), 6.0f * scale, 0, 2.0f * scale);
    dl->AddText(font, size, ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), IM_COL32(255, 255, 255, 255), text);
    const ImU32 yellow = IM_COL32(255, 210, 60, 255);
    const float lx = c.x - half, rx = c.x + half;
    dl->AddTriangleFilled(ImVec2(lx, c.y), ImVec2(lx + arrow, c.y - arrow), ImVec2(lx + arrow, c.y + arrow), yellow);
    dl->AddTriangleFilled(ImVec2(rx, c.y), ImVec2(rx - arrow, c.y + arrow), ImVec2(rx - arrow, c.y - arrow), yellow);
    // the added arena under the cursor, beside the label (as Kaiko's)
    if (const int ci = g_label_cursor.load(); ci >= 0 && ci < int(g_added.size()) && !g_added[size_t(ci)].name.empty()) {
      const std::string &name = g_added[size_t(ci)].name;
      const float ns = 22.0f * scale;
      const ImVec2 nt = font->CalcTextSizeA(ns, FLT_MAX, 0.0f, name.c_str());
      const ImVec2 na(b.x + 12.0f * scale, c.y - nt.y * 0.5f - pad.y), nb(na.x + nt.x + pad.x * 2, c.y + nt.y * 0.5f + pad.y);
      dl->AddRectFilled(na, nb, IM_COL32(10, 12, 18, 220), 6.0f * scale);
      dl->AddText(font, ns, ImVec2(na.x + pad.x, c.y - nt.y * 0.5f), yellow, name.c_str());
    }
    // on the game's page: where the added arenas are
    if (page == 0) {
      char more[64];
      snprintf(more, sizeof(more), "%s / %s: YOUR ADDED ARENAS", left, right);
      const float ms = 20.0f * scale;
      const ImVec2 mt = font->CalcTextSizeA(ms, FLT_MAX, 0.0f, more);
      const ImVec2 mp(c.x - mt.x * 0.5f, b.y + 6.0f * scale);
      dl->AddRectFilled(ImVec2(mp.x - pad.x, mp.y - 3.0f * scale), ImVec2(mp.x + mt.x + pad.x, mp.y + mt.y + 3.0f * scale),
                        IM_COL32(10, 12, 18, 200), 5.0f * scale);
      dl->AddText(font, ms, mp, yellow, more);
    }
  }
};
}  // namespace

namespace {
rex::ui::ImGuiDrawer *g_label_drawer = nullptr;
std::function<void(std::function<void()>)> g_label_post;  // runs a function on the UI thread

// (made on the UI thread; it closes itself once the screen is gone)
void OpenPageLabel() {
  if (!g_label_drawer || !g_label_post) {
    g_label_open = false;
    return;
  }
  g_label_post([] { new ArenaPageLabel(g_label_drawer); });
}
}  // namespace

void Wwe12ArenaPagesInstallLabel(rex::ui::ImGuiDrawer *drawer, std::function<void(std::function<void()>)> post) {
  g_label_drawer = drawer;
  g_label_post = std::move(post);
}

// ---- For the Universe (wwe12_universe.cpp: a show's added arena, universe_arenas.txt) ----------------------------

// The added arena chosen last on the arena select (its index), -1: one of the game's.
int Wwe12ArenaChosen() { return g_chosen; }

// An added arena's mod id ("" if none).
std::string Wwe12ArenaId(int n) { return n >= 0 && size_t(n) < g_added.size() ? g_added[size_t(n)].id : std::string(); }

// An added arena by its mod id: its index and base stage (-1: not installed).
int Wwe12ArenaFind(const std::string &id, uint32_t &base) {
  for (size_t i = 0; i < g_added.size(); ++i)
    if (g_added[i].id == id) {
      base = g_added[i].base;
      return int(i);
    }
  return -1;
}

// Puts added arena n on its base's stage, or (n < 0) the game's arena back on `stage`, as a choice on the arena
// select does (sub_825A56C8 above): its file served, the stage object loads it again if it changed. Called just
// before sub_828AC150 sets the match's stage: no "VS screen again" (g_reload_vs) there, also none left from SHOW
// EDIT's choice: in the Universe nothing sets the stage again after sub_828A9898 forgets it, and the match loaded
// stage -1 (crash before the entrance, or a white screen, 2026-11-06).
void Wwe12ArenaPlay(int n, uint32_t stage) {
  if (n >= 0 && size_t(n) < g_added.size()) {
    if (g_chosen != n)
      REXLOG_INFO("[arena pages] Universe show: {} ({}), plays as stage {}", g_added[size_t(n)].name,
                  g_added[size_t(n)].id, g_added[size_t(n)].base);
    g_chosen = n;
    Serve(g_added[size_t(n)].base, n);
    ForgetHeldArena(g_added[size_t(n)].base);
  } else if (stage < kFirstAdded) {
    g_chosen = -1;
    Serve(stage, -1);
    ForgetHeldArena(stage);
  }
  g_reload_vs = false;
}
