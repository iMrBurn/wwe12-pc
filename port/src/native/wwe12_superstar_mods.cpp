// WWE '12: added superstars (docs/mod_maker.md, "Ajouter des catcheurs"). At startup, after the DLC packs are
// installed and before the game lists them, the superstar mods (mods\.svrmods\Superstars\<id>, type=superstar,
// not disabled: manifest name=, base=NNN; optional in superstar\: ch.pac, titantron.bik, its portraits SSFA_HD.dds ...
// SSFC_SD.dds) become one more DLC pack, WWE12PCMODS, made by
// modmaker/superstar_pack.h from the player's own files: the latest installed pack's (else the disc's) chEtc.pac,
// DLC.pac and portraits, the base superstars' models when a mod has none. Each mod keeps its number from one start
// to the next (Documents\wwe12\superstar_slots.txt: "<mod>=<number>"), so a save's records never move. The pack is
// written again only when its contents change (then the game's cache, made from the packs present, is emptied as
// for a new DLC: wwe12_dlc.cpp). A save made with added superstars needs them in the pack (else the game refuses it:
// "downloadable content missing or damaged"), so the pack is never removed, and a superstar it holds whose mod is gone
// keeps it as it was (a warning in the log and Documents\wwe12\mods_warning.txt, shown by the launcher) until the
// Mod Maker's CHECK MODS takes him out of the pack's list (wwe12pc_stars.txt). Before the pack changes, the save is
// copied (Documents\wwe12\SaveBackups, as the launcher's automatic backups).
//
// Entrance videos. An added superstar plays its base's entrance, whose script names the video to show (titantron
// on top, minitron below): sub_828F2410 reads that number (command + 172, from sub_828D83B8; 999 = the superstar's own,
// for created ones), and sub_82A316A0(n) opens movies/titantron/%03d.bik (the packs first).
// A mod's superstar/titantron.bik goes into the pack under a number no other video has, and when an entrance video
// is opened for that superstar (the one entering: sub_828CEF58(sub_828C21A8()) -> {+8 set, +16 place}, its Superstar
// sub_8267CD30(sub_823F8270(place)) + 16 = number x 100 + attire), the path is made with that number instead.
//
// Moves mods (mods\.svrmods\Moves\<id>, type=moves, superstar=NNN: the Mod Maker's MOVES & ENTRANCES page,
// 2026-11-03): a superstar the game has gets another superstar's moves (moves=, superstar\moves\ for the moves WWE '12
// hasn't), entrance (entrance_own=, superstar\entrance\: served by wwe12_arena_pages.cpp) and theme (superstar\theme.wem,
// played under his entrance's number): his CHAR / PRO record in the pack's chEtc.pac (superstar_pack.h, Moveset), and
// the save's own copy of it made again at the select screen when it isn't the mod's yet (below: a moveset edited in the
// game stays). The pack is made for these mods alone too.

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <memory>
#include <span>
#include <string>
#include <vector>

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

#include "../modmaker/menu_pictures.h"
#include "../modmaker/superstar_pack.h"
#include "../modmaker/svrfmt/jboy.h"

REXCVAR_DEFINE_BOOL(wwe12_superstar_mods, true, "WWE12",
                    "Added superstars (superstar mods) as a DLC pack made at startup.");
REXCVAR_DEFINE_BOOL(wwe12_belt_donor, true, "WWE12",
                    "Added superstars with their own model: the waist belt's fit of the game's superstar nearest in build.");

// (wwe12_arena_pages.cpp) a ribbon / wall movie the game can play: 180 frames or more, 1.8 MB at most
bool OwnScreenFits(const std::filesystem::path &p);

namespace {

namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;

constexpr const char *kPackName = "WWE12PCMODS";

std::map<uint32_t, uint32_t> g_movies;  // added superstar -> the number of its own entrance video
std::map<std::string, uint32_t> g_mod_movies;          // ... by its mod's folder name (wwe12_arena_pages.cpp)
std::set<uint32_t> g_mod_minitrons;  // video numbers whose titantron a VIDEOS mod replaced (sub_82A31538 below)
std::set<std::pair<uint32_t, uint32_t>> g_own_screens;  // (kind: 1 ribbon, 2 wall; its video number) its mod has
std::vector<std::pair<uint32_t, std::string>> g_themes;  // added superstars' own themes: music number, name
fs::path g_menu_dir;                    // the dressed menu files of this start, "" = the game's
// The menu pictures mods' layouts (menu_pictures.h), appended to the served MnLayHD.pac / MnLaySD.pac: directory name
// -> {first 2 KB sector, size in 256 bytes} in that copy ([0] HD, [1] SD); answered by the file directory
// (Wwe12MenuPictureEntry, wwe12_arena_pages.cpp).
std::map<std::string, std::pair<uint32_t, uint32_t>> g_menu_entries[2];
std::map<uint32_t, Bytes> g_records;    // added superstar -> its record as the save keeps it (286 bytes)
fs::path g_relations_file;  // Documents/wwe12/relations_cleared.txt: the added superstars whose Universe relations went
std::map<uint32_t, Bytes> g_moves;      // ... its moves and entrance (CHAR / PRO) as the save keeps them (1068 bytes)
Bytes g_cena_moves;                     // John Cena's (139), as the save keeps them: where they are is checked on his
// The superstars the moves mods are for (Documents\wwe12\moves_mods.txt, one number a line): this start's, and those
// whose mod is gone since. The save keeps its own copy of their moves and entrance: a gone one's is put back to the
// game's record once (else the save kept the mod's, its entrance a number nothing serves any more), then forgotten.
fs::path g_moves_file;
std::set<uint32_t> g_moves_mods, g_moves_restore;
// The save's copy is made again only when it isn't the mod's yet, so a moveset or entrance edited in the game stays
// (Discord, 2026-11-03: "the movesets and entrances don't save", undone at each select screen): when the mod's own
// changed since it was last made (Documents\wwe12\moves_applied.txt, "<number>=<hash>"), when the copy is another
// superstar's (its bytes past the moves and entrance blocks differ: an added superstar's slot in a save that never had
// him), or when it is still the game's own record (a moves mod's superstar in a save made before the mod).
fs::path g_applied_file;
std::map<uint32_t, uint64_t> g_applied;
// (and what the save's copy held before it was made: "<number>=<hash>:<before>". Found again unchanged, the game
// wasn't saved since (closed or frozen before): made again. Taken for an edit made in the game, Seth Rollins' stale
// copy had no entrance, and the game skipped it, 2026-11-04)
std::map<uint32_t, uint64_t> g_applied_before;
std::map<uint32_t, Bytes> g_originals;  // a moves mod's superstar -> the game's record, as the save keeps it

void WriteAppliedFile() {
  std::string text;
  for (const auto &[id, h] : g_applied) {
    text += std::to_string(id) + "=" + std::to_string(h);
    if (const auto b = g_applied_before.find(id); b != g_applied_before.end())
      text += ":" + std::to_string(b->second);
    text += "\n";
  }
  std::error_code ec;
  if (text.empty())
    fs::remove(g_applied_file, ec);
  else
    std::ofstream(g_applied_file, std::ios::binary | std::ios::trunc) << text;
}

void WriteMovesFile() {
  std::string text;
  std::set<uint32_t> all = g_moves_mods;
  all.insert(g_moves_restore.begin(), g_moves_restore.end());
  for (uint32_t id : all)
    text += std::to_string(id) + "\n";
  std::error_code ec;
  if (text.empty())
    fs::remove(g_moves_file, ec);
  else
    std::ofstream(g_moves_file, std::ios::binary | std::ios::trunc) << text;
}

// A CHAR / PRO record (1072 bytes) as the save keeps it: without its first 4 bytes, its 16-bit numbers big-endian up
// to byte 486 (exact on 204 of 273 superstars of a save; the others were empty records or edited, 2026-10-26).
Bytes SavedMoves(const Bytes &d) {
  Bytes r(d.begin() + 4, d.end());
  for (size_t a = 0; a + 1 < 486 && a + 1 < r.size(); a += 2)
    std::swap(r[a], r[a + 1]);
  return r;
}

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

bool ReadHost(const fs::path &p, Bytes &out) {
  std::ifstream f(p, std::ios::binary);
  if (!f)
    return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return !out.empty();
}

bool WriteHost(const fs::path &p, const Bytes &data) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  f.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));
  return bool(f);
}

// A file as the game sees it on its disc (the mods folder first: the overlay is mounted).
bool ReadGame(rex::filesystem::VirtualFileSystem *vfs, const std::string &rel, Bytes &out) {
  std::string path = "game:\\" + rel;
  for (auto &c : path)
    if (c == '/')
      c = '\\';
  rex::filesystem::Entry *entry = vfs ? vfs->ResolvePath(path) : nullptr;
  rex::filesystem::File *file = nullptr;
  if (!entry || entry->Open(rex::filesystem::FileAccess::kFileReadData, &file) != 0 || !file)
    return false;
  out.resize(entry->size());
  size_t read = 0;
  const bool ok = file->ReadSync(std::span<uint8_t>(out.data(), out.size()), 0, &read) == 0 && read == out.size();
  file->Destroy();
  return ok;
}

// A manifest's moves= ("OFF:NUMBER,...": hex offset in CHAR / PRO, move number).
std::vector<std::pair<int, int>> ParseMoves(const std::string &v) {
  std::vector<std::pair<int, int>> out;
  for (size_t at = 0; at < v.size();) {
    char *end = nullptr;
    const long off = strtol(v.c_str() + at, &end, 16);
    if (end && *end == ':') {
      const long id = strtol(end + 1, nullptr, 10);
      if (id > 0 && id < 0x10000)
        out.push_back({int(off), int(id)});
    }
    const size_t comma = v.find(',', at);
    at = comma == std::string::npos ? v.size() : comma + 1;
  }
  return out;
}

// A mod's moves WWE '12 hasn't: superstar\moves\<number>.waze, _low.yanm, _high.yanm (and .magg, optional).
std::vector<superstar_pack::NewMove> ReadNewMoves(const fs::path &dir) {
  std::vector<superstar_pack::NewMove> out;
  std::error_code ec;
  for (fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
    const fs::path w = it->path();
    if (w.extension() != ".waze")
      continue;
    superstar_pack::NewMove nm;
    nm.number = atoi(w.stem().string().c_str());
    const fs::path d = w.parent_path(), stem = w.stem();
    if (nm.number > 0 && ReadHost(w, nm.waze) && ReadHost(d / (stem.string() + "_low.yanm"), nm.low) &&
        ReadHost(d / (stem.string() + "_high.yanm"), nm.high)) {
      ReadHost(d / (stem.string() + ".magg"), nm.magg);  // (optional: its RGRP/MAGG records)
      out.push_back(std::move(nm));
    }
  }
  return out;
}

uint64_t Fnv(const Bytes &b, uint64_t h) {
  for (uint8_t c : b)
    h = (h ^ c) * 0x100000001B3ull;
  return h;
}

// A part of a file as the game sees it on its disc (sound\Music.pck is 216 MB: its header and banks are read).
bool ReadGamePart(rex::filesystem::VirtualFileSystem *vfs, const std::string &rel, uint64_t at, uint32_t size, Bytes &out) {
  std::string path = "game:\\" + rel;
  for (auto &c : path)
    if (c == '/')
      c = '\\';
  rex::filesystem::Entry *entry = vfs ? vfs->ResolvePath(path) : nullptr;
  rex::filesystem::File *file = nullptr;
  if (!entry || at + size > entry->size() || entry->Open(rex::filesystem::FileAccess::kFileReadData, &file) != 0 || !file)
    return false;
  out.resize(size);
  size_t read = 0;
  const bool ok = file->ReadSync(std::span<uint8_t>(out.data(), out.size()), at, &read) == 0 && read == out.size();
  file->Destroy();
  return ok;
}

// The save copied as the launcher's automatic backups do (SaveBackups\<date>\: the save package and the achievements,
// backup.txt "auto" + why; the latest 10 automatic ones kept): before the pack changes.
void BackupSave(const fs::path &user_root, const std::string &why) {
  const fs::path save = user_root / "B13EBABEBABEBABE" / "54510890",
                 ach = user_root / "achievements" / "54510890.toml", all = user_root / "SaveBackups";
  std::error_code ec;
  if (!fs::exists(save, ec))
    return;
  const std::time_t now = std::time(nullptr);
  std::tm tm{};
  localtime_s(&tm, &now);
  char stamp[32];
  std::strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", &tm);
  std::string name = stamp;
  for (int n = 2; fs::exists(all / name, ec); ++n)
    name = std::string(stamp) + "_" + std::to_string(n);
  const fs::path dest = all / name;
  fs::create_directories(dest / "B13EBABEBABEBABE", ec);
  fs::copy(save, dest / "B13EBABEBABEBABE" / "54510890", fs::copy_options::recursive, ec);
  if (!ec && fs::exists(ach)) {
    fs::create_directories(dest / "achievements", ec);
    fs::copy_file(ach, dest / "achievements" / ach.filename(), fs::copy_options::overwrite_existing, ec);
  }
  if (ec) {
    REXLOG_WARN("[superstar mods] the save could not be copied before the pack changed: {}", ec.message());
    std::error_code ignore;
    fs::remove_all(dest, ignore);
    return;
  }
  std::ofstream(dest / "backup.txt") << "auto\n" << why << "\n";
  REXLOG_INFO("[superstar mods] save copied to SaveBackups\\{} ({})", name, why);
  // the latest 10 automatic copies kept (names are dates: newest last)
  std::vector<std::string> autos;
  for (fs::directory_iterator it(all, ec), end; !ec && it != end; it.increment(ec)) {
    std::ifstream info(it->path() / "backup.txt");
    std::string kind;
    if (std::getline(info, kind) && kind == "auto")
      autos.push_back(it->path().filename().string());
  }
  std::sort(autos.begin(), autos.end());
  for (size_t i = 0; i + 10 < autos.size(); ++i)
    fs::remove_all(all / autos[i], ec);
}

// The pack's superstars ("<mod>=<number>" lines, wwe12pc_stars.txt in the pack).
std::map<std::string, int> PackStars(const fs::path &pack) {
  std::map<std::string, int> out;
  std::ifstream f(pack / "wwe12pc_stars.txt");
  std::string line;
  while (std::getline(f, line)) {
    if (!line.empty() && line.back() == '\r')
      line.pop_back();
    const size_t eq = line.rfind('=');
    if (eq != std::string::npos && eq > 0)
      out[line.substr(0, eq)] = atoi(line.c_str() + eq + 1);
  }
  return out;
}

// The game's CHAR / PRO records (a chEtc.pac) of the superstars whose moves mod is gone, as the save keeps them, into
// g_moves (and John Cena's, where they are checked).
void AddRestores(const Bytes &chetc) {
  svrfmt::Archive etc;
  if (g_moves_restore.empty() || !svrfmt::ArchiveOpen(chetc, etc, nullptr))
    return;
  for (auto &k : etc.root.kids)
    if (k.group == "CHAR" && k.name == "PRO " && svrfmt::ArchiveExpand(k))
      for (auto &c : k.kids) {
        const Bytes d = svrfmt::ArchiveData(c);
        if (d.size() != 1072)
          continue;
        if (c.id == 139 && g_cena_moves.empty())
          g_cena_moves = SavedMoves(d);
        if (g_moves_restore.count(c.id)) {
          g_moves[c.id] = SavedMoves(d);
          REXLOG_INFO("[superstar mods] superstar {}: his moves mod is gone, the save's copy of his moves and entrance "
                      "goes back to the game's", c.id);
        }
      }
}

// Title belts at the waist (2026-11-03). A superstar file's attire keeps the belt's fit in two parts: 40, a text of
// attachment points whose last number is a build (Chavo Guerrero 2, Ted DiBiase 3, Mark Henry 8; the bending,
// sub_828C5878, reads it at +1888) and 80 (2816 bytes, the body's volumes: what sets how far out the belt goes, +24 and
// +272 there). A ported model kept its base's, so a belt sized for the base went through Jeff Hardy's loose top;
// Mark Henry's two parts put it outside (a bit big). An added superstar with his own model gets those of the game's
// superstar whose waist is nearest his (Discord, "grab one off a bigger guy and swap it in").

// A superstar file's first attire's waist where the belt sits: the width and depth of its pelvis, spine and thigh
// vertices between 50 % and 54 % of its height (bind pose, y up negative, feet at 0), clothes included. (Bip / Bip
// Spine sphere radii were tried first: they hold the thighs too, Rey Mysterio came out as Brock Lesnar, and Jerry
// Lawler's fit left Jeff Hardy's belt a bit big.)
bool WaistOf(const Bytes &ch, float &width, float &depth) {
  svrfmt::Archive a;
  if (!svrfmt::ArchiveOpen(ch, a, nullptr))
    return false;
  for (auto &k : a.root.kids) {
    if (k.group != "EMD " || !svrfmt::ArchiveExpand(k))
      continue;
    svrfmt::Model m;
    bool read = false;
    for (auto &x : k.kids)
      if (x.id == 0)
        read = svrfmt::JboyRead(svrfmt::ArchiveData(x), m, nullptr);
    if (!read)
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

// A superstar file's first attire's part `id` (40, 80), empty if none.
Bytes AttirePart(const Bytes &ch, uint32_t id) {
  svrfmt::Archive a;
  if (svrfmt::ArchiveOpen(ch, a, nullptr))
    for (auto &k : a.root.kids)
      if (k.group == "EMD " && svrfmt::ArchiveExpand(k)) {
        for (auto &x : k.kids)
          if (x.id == id)
            return svrfmt::ArchiveData(x);
        break;
      }
  return {};
}

// The game's superstars' waists (Documents\wwe12\belt_waists.txt, "<number>=<width>,<depth>", 0 for none: read once from
// their files, 50-372, then kept).
std::map<int, std::pair<float, float>> GameWaists(rex::filesystem::VirtualFileSystem *vfs, const fs::path &user_root) {
  std::map<int, std::pair<float, float>> out;
  const fs::path file = user_root / "belt_waists.txt";
  std::ifstream in(file);
  for (std::string line; std::getline(in, line);)
    if (const size_t eq = line.find('='), comma = line.find(','); eq != std::string::npos && comma != std::string::npos)
      out[atoi(line.c_str())] = {strtof(line.c_str() + eq + 1, nullptr), strtof(line.c_str() + comma + 1, nullptr)};
  in.close();
  if (out.size() >= 323)
    return out;
  out.clear();
  std::string text;
  for (int n = 50; n <= 372; ++n) {
    Bytes ch;
    float bip = 0, spine = 0;
    if (ReadGame(vfs, "pac/ch/ch" + std::to_string(n) + ".pac", ch))
      WaistOf(ch, bip, spine);
    out[n] = {bip, spine};
    char line[64];
    snprintf(line, sizeof(line), "%d=%.2f,%.2f\n", n, bip, spine);
    text += line;
  }
  WriteHost(file, Bytes(text.begin(), text.end()));
  REXLOG_INFO("[belts] the game's superstars' waists measured ({})", rex::path_to_utf8(file));
  return out;
}

// The game's divas (CHAR / DAT byte 0xEE bit 0, set on all of 11 divas and none of 12 men, 2026-11-03).
std::set<int> Divas(const Bytes &chetc) {
  std::set<int> out;
  svrfmt::Archive etc;
  if (svrfmt::ArchiveOpen(chetc, etc, nullptr))
    for (auto &k : etc.root.kids)
      if (k.group == "CHAR" && k.name == "DAT " && svrfmt::ArchiveExpand(k))
        for (auto &c : k.kids)
          if (const Bytes d = svrfmt::ArchiveData(c); d.size() > 0xEE && (d[0xEE] & 1))
            out.insert(int(c.id));
  return out;
}

// An added superstar's own model given the belt parts (40, 80) of the game's superstar with the nearest waist, among
// the men or among the divas as his base (Johnny Nitro got Kelly Kelly's first).
void FitBelt(rex::filesystem::VirtualFileSystem *vfs, const fs::path &user_root, superstar_pack::Star &st,
             const std::set<int> &divas) {
  float bip = 0, spine = 0;
  if (!WaistOf(st.model, bip, spine))
    return;
  static std::map<int, std::pair<float, float>> waists;
  if (waists.empty())
    waists = GameWaists(vfs, user_root);
  int best = 0;
  float best_d = 1e9f;
  // (aimed 5 % slimmer: the game's belts sit a little loose on their own superstar, and the one with Jeff Hardy's very
  // measures, David Otunga, left his belt a bit big)
  constexpr float kSlimmer = 0.95f;
  bip *= kSlimmer, spine *= kSlimmer;
  bool diva = divas.count(st.base) != 0;
  for (const auto &[at, bytes] : st.record)  // (its gender chosen in the Mod Maker: record byte 0xEE bit 0)
    if (at <= 0xEE && size_t(0xEE - at) < bytes.size())
      diva = bytes[size_t(0xEE - at)] & 1;
  for (const auto &[n, w] : waists)
    if (w.first > 0 && w.second > 0 && (divas.count(n) != 0) == diva) {
      const float d = (w.first - bip) * (w.first - bip) + (w.second - spine) * (w.second - spine);
      if (d < best_d)
        best = n, best_d = d;
    }
  Bytes donor;
  if (!best || !ReadGame(vfs, "pac/ch/ch" + std::to_string(best) + ".pac", donor))
    return;
  const Bytes p40 = AttirePart(donor, 40), p80 = AttirePart(donor, 80);
  svrfmt::Archive a;
  if (p40.empty() || p80.empty() || !svrfmt::ArchiveOpen(st.model, a, nullptr))
    return;
  int done = 0;
  for (auto &k : a.root.kids) {
    if (k.group != "EMD " || !svrfmt::ArchiveExpand(k))
      continue;
    for (auto &x : k.kids)
      if (x.id == 40 || x.id == 80) {
        svrfmt::ArchiveReplace(x, x.id == 40 ? p40 : p80);
        ++done;
      }
    svrfmt::ArchiveTouch(k);
  }
  if (!done)
    return;
  svrfmt::ArchiveTouch(a.root);
  st.model = svrfmt::ArchiveSave(a);
  REXLOG_INFO("[belts] {}: waist {:.1f} / {:.1f}, the belt fitted as superstar {}'s ({:.1f} / {:.1f})", st.key, bip,
              spine, best, waists[best].first, waists[best].second);
}

// The game's chEtc.pac: the newest installed pack's (not ours), else the disc's.
Bytes GameChEtc(rex::filesystem::VirtualFileSystem *vfs, const fs::path &content) {
  std::vector<std::pair<int, fs::path>> packs;
  std::error_code ec;
  for (fs::directory_iterator it(content, ec), end; !ec && it != end; it.increment(ec)) {
    Bytes cat;
    if (it->path().filename() != kPackName && ReadHost(it->path() / "info" / "catalog.dlc", cat) && cat.size() >= 12)
      packs.push_back({cat[8] | cat[9] << 8, it->path()});
  }
  std::sort(packs.begin(), packs.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
  Bytes out;
  for (const auto &[n, dir] : packs)
    if (ReadHost(dir / "pac" / "ch" / "chEtc.pac", out))
      return out;
  ReadGame(vfs, "pac/ch/chEtc.pac", out);
  return out;
}

// No mods for the pack: it is KEPT. Every build shares Documents\wwe12 (saves and installed DLC); a save made with the
// added superstars needs their pack, else the game refuses it ("downloadable content missing or damaged"): a test
// build with an empty mods folder removed it and the player's 0.9 couldn't load his save (2026-10-26).
void RemovePack(const fs::path &content, const fs::path &headers) {
  (void)headers;
  std::error_code ec;
  if (fs::exists(content / kPackName, ec))
    REXLOG_INFO("[superstar mods] no superstar mods now: pack {} kept (a save may use its superstars)", kPackName);
}

// The menu pictures mods (type=menupics, mods\.svrmods\MenuPictures\<id>\pictures\<name>.dds, later folders over
// earlier ones): the Universe's logos, match-up screens and arena photos (modmaker/menu_pictures.h). The layouts
// showing them, dressed, are appended to the served MnLayHD.pac / MnLaySD.pac (the main menu background's copy when
// one was drawn, kept in place) and the file directory answers their names with that place (g_menu_entries). Dressing
// takes seconds: the dressed layouts are kept in Documents\wwe12\menu_pictures and made again only when the pictures
// or the game's file change.
void MenuPictures(rex::filesystem::VirtualFileSystem *vfs, const fs::path &user_root, const fs::path &mods) {
  g_menu_entries[0].clear();
  g_menu_entries[1].clear();
  std::error_code ec;
  std::vector<fs::path> dirs;
  for (fs::directory_iterator it(mods / ".svrmods" / "MenuPictures", ec), end; !ec && it != end; it.increment(ec))
    if (it->is_directory(ec) && !fs::exists(it->path() / "disabled", ec))
      dirs.push_back(it->path());
  ec.clear();
  std::sort(dirs.begin(), dirs.end());
  std::map<std::string, Bytes> pics;
  for (const auto &d : dirs)
    for (fs::directory_iterator it(d / "pictures", ec), end; !ec && it != end; it.increment(ec)) {
      Bytes b;
      if (_stricmp(it->path().extension().string().c_str(), ".dds") == 0 && ReadHost(it->path(), b))
        pics[it->path().stem().string()] = std::move(b);
    }
  if (pics.empty())
    return;
  uint64_t key = 0xCBF29CE484222325ull;
  for (const auto &[name, b] : pics)
    key = Fnv(b, Fnv(Bytes(name.begin(), name.end()), key));
  const fs::path dir = user_root / "menu_background", cache = user_root / "menu_pictures";
  if (g_menu_dir.empty())
    fs::remove_all(dir, ec);
  int served = 0;
  for (int sd = 0; sd < 2; ++sd) {
    const std::string rel = sd ? "pac/menu/MnLaySD.pac" : "pac/menu/MnLayHD.pac";
    Bytes game;
    if (!ReadGame(vfs, rel, game) || game.size() < 0x10000)
      continue;
    char k[64];
    snprintf(k, sizeof(k), "%016llX %016llX %zu", static_cast<unsigned long long>(key),
             static_cast<unsigned long long>(Fnv(Bytes(game.begin(), game.begin() + 0x10000), 0xCBF29CE484222325ull)),
             game.size());
    const char *tag = sd ? "sd" : "hd";
    // The dressed layouts: the cache's (index lines "<name> <size>", then the bytes one after another), else made.
    std::vector<menu_pictures::Dressed> dressed;
    Bytes kept, index, blob;
    if (ReadHost(cache / (std::string(tag) + ".key"), kept) && std::string(kept.begin(), kept.end()) == k &&
        ReadHost(cache / (std::string(tag) + ".idx"), index) && ReadHost(cache / (std::string(tag) + ".bin"), blob)) {
      size_t at = 0;
      const std::string text(index.begin(), index.end());
      for (size_t p = 0; p < text.size();) {
        size_t e = text.find('\n', p);
        if (e == std::string::npos)
          e = text.size();
        const std::string line = text.substr(p, e - p);
        p = e + 1;
        const size_t sp = line.find(' ');
        const size_t size = sp == std::string::npos ? 0 : size_t(strtoull(line.c_str() + sp + 1, nullptr, 10));
        if (!size || at + size > blob.size())
          break;
        dressed.push_back({line.substr(0, sp), Bytes(blob.begin() + long(at), blob.begin() + long(at + size))});
        at += size;
      }
    }
    if (dressed.empty()) {
      int n = 0;
      const auto t0 = std::chrono::steady_clock::now();
      dressed = menu_pictures::Dress(game, pics, &n);
      std::string idx;
      blob.clear();
      for (const auto &d : dressed) {
        idx += d.name + " " + std::to_string(d.data.size()) + "\n";
        blob.insert(blob.end(), d.data.begin(), d.data.end());
      }
      WriteHost(cache / (std::string(tag) + ".idx"), Bytes(idx.begin(), idx.end()));
      WriteHost(cache / (std::string(tag) + ".bin"), blob);
      WriteHost(cache / (std::string(tag) + ".key"), Bytes(k, k + strlen(k)));
      REXLOG_INFO("[menu pictures] {}: {} picture(s) in {} layout(s), {} ms", rel, n, dressed.size(),
                  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count());
    }
    if (dressed.empty())
      continue;
    // The served file: the background's copy (EM00 dressed in place) or the game's, the layouts appended on 2 KB.
    Bytes file;
    if (g_menu_dir.empty() || !ReadHost(dir / fs::path(rel), file))
      file = std::move(game);
    for (const auto &d : dressed) {
      file.resize((file.size() + 2047) / 2048 * 2048, 0);
      g_menu_entries[sd][d.name] = {uint32_t(file.size() / 2048), uint32_t((d.data.size() + 255) / 256)};
      file.insert(file.end(), d.data.begin(), d.data.end());
    }
    file.resize((file.size() + 2047) / 2048 * 2048, 0);
    if (!WriteHost(dir / fs::path(rel), file)) {
      g_menu_entries[sd].clear();
      continue;
    }
    ++served;
    REXLOG_INFO("[menu pictures] {}: {} layout(s) served from {}", rel, dressed.size(), rex::path_to_utf8(dir));
  }
  if (served)
    g_menu_dir = dir;
}

}  // namespace

// (wwe12_arena_pages.cpp, the file directory) a menu pictures mod's layout of MnLayHD.pac / MnLaySD.pac ("/MLYE/USEI"):
// its place in the served copy, false if none.
bool Wwe12MenuPictureEntry(const std::string &name, bool sd, uint32_t &sector, uint32_t &units) {
  const auto it = g_menu_entries[sd ? 1 : 0].find(name);
  if (it == g_menu_entries[sd ? 1 : 0].end())
    return false;
  sector = it->second.first;
  units = it->second.second;
  return true;
}

// The attire mods' own names (attire_name=): text numbers from kAttireText, which no string.pac table has (theirs end at
// 53114), answered by the game's text lookup hooked below. (Served as new texts in a string.pac over the game's, the
// game crashed right after reading it, whatever its tables' packing: 2026-11-02.)
constexpr uint32_t kAttireText = 60000;
std::map<uint32_t, std::string> g_attire_texts;  // number -> text
std::map<uint32_t, uint32_t> g_attire_text_at;   // number -> its copy in guest memory

void Wwe12MountSuperstarMods(rex::filesystem::VirtualFileSystem *vfs, const fs::path &user_root,
                             const fs::path &mods) {
  g_relations_file = user_root / "relations_cleared.txt";
  // (the VIDEOS page's titantrons of the game's superstars, mods/movies/titantron/NNN.bik: each has its own minitron)
  {
    g_mod_minitrons.clear();
    std::error_code e0;
    for (fs::directory_iterator it(mods / "movies" / "titantron", e0), end; !e0 && it != end; it.increment(e0)) {
      const std::string f = it->path().filename().string();
      if (f.size() == 7 && isdigit(static_cast<unsigned char>(f[0])) && _stricmp(f.c_str() + 3, ".bik") == 0)
        g_mod_minitrons.insert(uint32_t(atoi(f.c_str())));
    }
  }
  if (!REXCVAR_GET(wwe12_superstar_mods) || user_root.empty())
    return;
  const fs::path content = user_root / "0000000000000000" / "54510890" / "00000002";
  const fs::path headers = user_root / "0000000000000000" / "54510890" / "Headers" / "00000002";
  std::error_code ec;
  // The superstar mods, in folder order.
  struct Mod {
    std::string key, name;
    int base = 0;
    std::vector<uint8_t> attributes;  // attributes=12 numbers (0-100), else empty (the base's)
    std::vector<std::pair<int, std::vector<uint8_t>>> record;  // record=OFF:HEX,... (CHAR / DAT bytes)
    std::vector<std::pair<int, int>> moves;  // moves=OFF:NUMBER,... (hex offset in CHAR / PRO), else the base's
    int entrance = 0;                         // entrance=NUMBER: that superstar's entrance, else the base's
    int own_entrance = 0;                     // entrance_own=NUMBER: its own (superstar/entrance/), else 0
    std::vector<superstar_pack::NewMove> new_moves;  // superstar/moves/<number>.waze, _low.yanm, _high.yanm
    fs::path model, movie;
    fs::path theme;  // superstar/entrance/MUS_<own entrance>.bin: its own theme (a WWE '13 one), else empty
  };
  std::vector<Mod> list;
  std::vector<fs::path> dirs;
  for (fs::directory_iterator it(mods / ".svrmods" / "Superstars", ec), end; !ec && it != end; it.increment(ec))
    if (it->is_directory(ec))
      dirs.push_back(it->path());
  std::sort(dirs.begin(), dirs.end());
  for (const auto &d : dirs) {
    Bytes m;
    if (fs::exists(d / "disabled", ec) || !ReadHost(d / "manifest.txt", m))
      continue;
    const std::string text(m.begin(), m.end());
    Mod mod;
    mod.key = d.filename().string();
    mod.name = Value(text, "name");
    mod.base = atoi(Value(text, "base").c_str());
    {
      const std::string a = Value(text, "attributes");
      for (size_t at = 0; at < a.size() && mod.attributes.size() < 12;) {
        mod.attributes.push_back(uint8_t(std::clamp(atoi(a.c_str() + at), 0, 100)));
        const size_t comma = a.find(',', at);
        at = comma == std::string::npos ? a.size() : comma + 1;
      }
      if (mod.attributes.size() != 12)
        mod.attributes.clear();
    }
    {
      const std::string v = Value(text, "record");
      for (size_t at = 0; at < v.size();) {
        const size_t comma = std::min(v.find(',', at), v.size()), colon = v.find(':', at);
        if (colon < comma) {
          std::vector<uint8_t> bytes;
          for (size_t k = colon + 1; k + 1 < comma; k += 2)
            bytes.push_back(uint8_t(strtoul(v.substr(k, 2).c_str(), nullptr, 16)));
          if (!bytes.empty())
            mod.record.push_back({int(strtoul(v.substr(at, colon - at).c_str(), nullptr, 16)), bytes});
        }
        at = comma + 1;
      }
    }
    mod.moves = ParseMoves(Value(text, "moves"));
    mod.entrance = std::clamp(atoi(Value(text, "entrance").c_str()), 0, 372);
    mod.own_entrance = std::clamp(atoi(Value(text, "entrance_own").c_str()), 0, 999);
    if (mod.own_entrance > 0 &&
        fs::exists(d / "superstar" / "entrance" / ("MUS_" + std::to_string(mod.own_entrance) + ".bin"), ec))
      mod.theme = d / "superstar" / "entrance" / ("MUS_" + std::to_string(mod.own_entrance) + ".bin");
    if (fs::exists(d / "superstar" / "theme.wem", ec))  // (its game's theme, made PCM by the Mod Maker)
      mod.theme = d / "superstar" / "theme.wem";
    mod.new_moves = ReadNewMoves(d / "superstar" / "moves");
    mod.model = d / "superstar" / "ch.pac";
    mod.movie = d / "superstar" / "titantron.bik";
    if (mod.base < 50 || mod.base >= 373) {
      REXLOG_WARN("[superstar mods] {}: base={} is not a superstar of the game", mod.key, Value(text, "base"));
      continue;
    }
    list.push_back(mod);
  }
  // The moves mods (type=moves, superstar=NNN), in folder order: several on one superstar, the last one wins (the
  // others are left out, said in the log).
  std::vector<superstar_pack::Moveset> movesets;
  {
    std::vector<fs::path> mdirs;
    for (fs::directory_iterator it(mods / ".svrmods" / "Moves", ec), end; !ec && it != end; it.increment(ec))
      if (it->is_directory(ec))
        mdirs.push_back(it->path());
    ec.clear();
    std::sort(mdirs.begin(), mdirs.end());
    for (const auto &d : mdirs) {
      Bytes m;
      if (fs::exists(d / "disabled", ec) || !ReadHost(d / "manifest.txt", m))
        continue;
      const std::string text(m.begin(), m.end());
      superstar_pack::Moveset mv;
      mv.key = d.filename().string();
      mv.name = Value(text, "name");
      mv.superstar = atoi(Value(text, "superstar").c_str());
      if (mv.superstar < 50 || mv.superstar >= 373) {
        REXLOG_WARN("[superstar mods] moves mod {}: superstar={} is not a superstar of the game", mv.key,
                    Value(text, "superstar"));
        continue;
      }
      mv.moves = ParseMoves(Value(text, "moves"));
      mv.own_entrance = std::clamp(atoi(Value(text, "entrance_own").c_str()), 0, 999);
      ReadHost(d / "superstar" / "theme.wem", mv.theme);
      mv.new_moves = ReadNewMoves(d / "superstar" / "moves");
      for (auto it = movesets.begin(); it != movesets.end(); ++it)
        if (it->superstar == mv.superstar) {
          REXLOG_WARN("[superstar mods] moves mods {} and {} are both for superstar {}: {} is used", it->key, mv.key,
                      mv.superstar, mv.key);
          movesets.erase(it);
          break;
        }
      REXLOG_INFO("[superstar mods] moves mod {}: superstar {}, {} move(s){}{}{}", mv.key, mv.superstar, mv.moves.size(),
                  mv.new_moves.empty() ? "" : ", " + std::to_string(mv.new_moves.size()) + " added",
                  mv.own_entrance ? ", entrance " + std::to_string(mv.own_entrance) : std::string(),
                  mv.theme.empty() ? "" : ", its own theme");
      movesets.push_back(std::move(mv));
    }
    // (their superstars, and those of the moves mods gone since the last start: moves_mods.txt)
    g_moves_file = user_root / "moves_mods.txt";
    g_moves.clear();
    g_moves_mods.clear();
    for (const auto &mv : movesets)
      g_moves_mods.insert(uint32_t(mv.superstar));
    std::ifstream f(g_moves_file);
    for (std::string line; std::getline(f, line);)
      if (const uint32_t id = uint32_t(atoi(line.c_str())); id >= 50 && id < 373 && !g_moves_mods.count(id))
        g_moves_restore.insert(id);
    f.close();
    WriteMovesFile();
    g_applied_file = user_root / "moves_applied.txt";
    g_applied.clear();
    g_applied_before.clear();
    std::ifstream a(g_applied_file);
    for (std::string line; std::getline(a, line);)
      if (const size_t eq = line.find('='); eq != std::string::npos) {
        const uint32_t id = uint32_t(atoi(line.c_str()));
        g_applied[id] = std::strtoull(line.c_str() + eq + 1, nullptr, 10);
        if (const size_t colon = line.find(':', eq); colon != std::string::npos)
          g_applied_before[id] = std::strtoull(line.c_str() + colon + 1, nullptr, 10);
      }
  }
  // The attire mods (type=attire with attire/attire.pac; older ones, a whole file, go through the mods folder).
  std::vector<superstar_pack::Attire> attires;
  {
    std::vector<fs::path> adirs;
    for (fs::directory_iterator it(mods / ".svrmods" / "Attires", ec), end; !ec && it != end; it.increment(ec))
      if (it->is_directory(ec))
        adirs.push_back(it->path());
    ec.clear();
    std::sort(adirs.begin(), adirs.end());
    for (const auto &d : adirs) {
      Bytes m;
      superstar_pack::Attire at;
      if (fs::exists(d / "disabled", ec) || !ReadHost(d / "manifest.txt", m) || !ReadHost(d / "attire" / "attire.pac", at.file))
        continue;
      const std::string text(m.begin(), m.end());
      at.key = d.filename().string();
      at.superstar = atoi(Value(text, "superstar").c_str());
      at.entry = Value(text, "attire");
      char nnn[8];
      snprintf(nnn, sizeof(nnn), "%06d", at.superstar);
      if (at.superstar < 50 || at.superstar >= 373 || at.entry.size() != 8 || at.entry.compare(0, 6, nnn) != 0) {
        REXLOG_WARN("[superstar mods] {}: superstar={} attire={} don't go together", at.key, at.superstar, at.entry);
        continue;
      }
      // A new attire: one his file (the game's, the latest pack's or the disc's) hasn't. An official DLC superstar's
      // file is in its package (content\<package>\pac\ch\chNN_dlcMM.pac; not ours, WWE12PCMODS).
      {
        Bytes game;
        bool read = ReadGame(vfs, "pac/ch/ch" + std::to_string(at.superstar) + ".pac", game);
        const std::string prefix = "ch" + std::to_string(at.superstar) + "_dlc";
        for (fs::directory_iterator it(content, ec), end; !read && !ec && it != end; it.increment(ec)) {
          if (it->path().filename() == "WWE12PCMODS")
            continue;
          std::error_code e2;
          for (fs::directory_iterator f(it->path() / "pac" / "ch", e2), fend; !read && !e2 && f != fend; f.increment(e2)) {
            const std::string name = f->path().filename().string();
            if (_strnicmp(name.c_str(), prefix.c_str(), prefix.size()) == 0)
              read = ReadHost(f->path(), game);
          }
        }
        ec.clear();
        svrfmt::Archive g;
        // (an entrance outfit, kind 4 "000NNNA4", worn only during the entrance, is never a new attire to offer: put in
        // his file alone, as the game's own, Cena's 00013904)
        at.added = at.entry[7] == '2' && read && svrfmt::ArchiveOpen(game, g, nullptr) &&
                   std::none_of(g.root.kids.begin(), g.root.kids.end(), [&](const auto &k) { return k.name == at.entry; });
      }
      // Its own name and portraits (attire_name=, attire/SSFA_SD.dds ... SSFC_HD.dds), if it gives them.
      at.label = Value(text, "attire_name");
      for (int hd = 0; hd < 2; ++hd)
        for (int k = 0; k < 3; ++k)
          ReadHost(d / "attire" / (std::string("SSF") + char('A' + k) + (hd ? "_HD.dds" : "_SD.dds")), at.render[hd][k]);
      REXLOG_INFO("[superstar mods] attire mod {}: superstar {}, attire {}{}{}{}", at.key, at.superstar, at.entry,
                  at.added ? " (a new one)" : "", at.label.empty() ? "" : ", named \"" + at.label + "\"",
                  at.render[1][1].empty() ? "" : ", its own render");
      attires.push_back(std::move(at));
    }
  }
  // The media mods' portraits of the game's superstars (media/portraits/NNN/SSFx_HD.dds, SSFx_SD.dds).
  std::vector<superstar_pack::MediaPortraits> media;
  {
    std::vector<fs::path> mdirs;
    for (fs::directory_iterator it(mods / ".svrmods" / "Media", ec), end; !ec && it != end; it.increment(ec))
      if (it->is_directory(ec))
        mdirs.push_back(it->path());
    ec.clear();
    std::sort(mdirs.begin(), mdirs.end());
    for (const auto &d : mdirs) {
      if (fs::exists(d / "disabled", ec))
        continue;
      // Its songs: into the music folder beside the exe ("<pack> <number>.mp3"), where Create an Entrance's USER
      // PLAYLIST lists them (the SDK's music player, rexglue-sdk-xmp-music.patch).
      {
        Bytes m;
        ReadHost(d / "manifest.txt", m);
        std::string pack_name = Value(std::string(m.begin(), m.end()), "name");
        if (pack_name.empty())
          pack_name = d.filename().string();
        for (auto &c : pack_name)
          if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
            c = '_';
        for (fs::directory_iterator it(d / "media" / "music", ec), end; !ec && it != end; it.increment(ec)) {
          const fs::path to = mods.parent_path() / "music" / fs::u8path(pack_name + " " + it->path().filename().string());
          std::error_code e2;
          if (!fs::exists(to, e2) || fs::file_size(to, e2) != it->file_size(e2)) {
            fs::create_directories(to.parent_path(), e2);
            fs::copy_file(it->path(), to, fs::copy_options::overwrite_existing, e2);
            REXLOG_INFO("[superstar mods] media mod {}: song {} added to the music folder", d.filename().string(),
                        rex::path_to_utf8(to.filename()));
          }
        }
        ec.clear();
      }
      for (fs::directory_iterator it(d / "media" / "portraits", ec), end; !ec && it != end; it.increment(ec)) {
        superstar_pack::MediaPortraits mp;
        mp.key = d.filename().string();
        mp.superstar = atoi(it->path().filename().string().c_str());
        if (mp.superstar < 50 || mp.superstar >= 373)
          continue;
        bool any = false;
        for (int hd = 0; hd < 2; ++hd)
          for (int k = 0; k < 3; ++k)
            any |= ReadHost(it->path() / (std::string("SSF") + char('A' + k) + (hd ? "_HD.dds" : "_SD.dds")), mp.dds[hd][k]);
        if (any) {
          REXLOG_INFO("[superstar mods] media mod {}: portraits of superstar {}", mp.key, mp.superstar);
          media.push_back(std::move(mp));
        }
      }
      ec.clear();
    }
  }
  // The signs mods (signs/NNN_K.dds).
  std::vector<superstar_pack::Sign> signs;
  {
    std::vector<fs::path> sdirs;
    for (fs::directory_iterator it(mods / ".svrmods" / "Signs", ec), end; !ec && it != end; it.increment(ec))
      if (it->is_directory(ec))
        sdirs.push_back(it->path());
    ec.clear();
    std::sort(sdirs.begin(), sdirs.end());
    for (const auto &d : sdirs) {
      if (fs::exists(d / "disabled", ec))
        continue;
      int n = 0;
      for (fs::directory_iterator it(d / "signs", ec), end; !ec && it != end; it.increment(ec)) {
        const std::string f = it->path().filename().string();
        const size_t us = f.find('_');
        superstar_pack::Sign sg;
        sg.key = d.filename().string();
        if (it->path().extension() != ".dds" || !ReadHost(it->path(), sg.dds))
          continue;
        // NNN_K.dds: superstar NNN's sign K; <id>.dds: any sign by its number (the crowd's general ones: 4001-5151)
        const int superstar = atoi(f.c_str()), k = us == std::string::npos ? 0 : atoi(f.c_str() + us + 1);
        sg.id = us == std::string::npos ? superstar : superstar * 10 + k;
        if (us != std::string::npos && (superstar < 50 || superstar >= 373 || k < 1 || k > 4))
          continue;
        if (sg.id < 500 || sg.id > 9999)
          continue;
        signs.push_back(std::move(sg));
        ++n;
      }
      ec.clear();
      if (n)
        REXLOG_INFO("[superstar mods] signs mod {}: {} sign(s)", d.filename().string(), n);
    }
  }
  // The main menu backgrounds mods (backgrounds/NN/menu.dds, title.dds).
  std::vector<superstar_pack::Background> backgrounds;
  {
    std::vector<fs::path> bdirs;
    for (fs::directory_iterator it(mods / ".svrmods" / "Backgrounds", ec), end; !ec && it != end; it.increment(ec))
      if (it->is_directory(ec))
        bdirs.push_back(it->path());
    ec.clear();
    std::sort(bdirs.begin(), bdirs.end());
    for (const auto &d : bdirs) {
      if (fs::exists(d / "disabled", ec))
        continue;
      std::vector<fs::path> one;
      for (fs::directory_iterator it(d / "backgrounds", ec), end; !ec && it != end; it.increment(ec))
        if (it->is_directory(ec))
          one.push_back(it->path());
      ec.clear();
      std::sort(one.begin(), one.end());
      for (const auto &b : one) {
        superstar_pack::Background bg;
        bg.key = d.filename().string();
        if (!ReadHost(b / "menu.dds", bg.menu))
          continue;
        ReadHost(b / "title.dds", bg.title);
        backgrounds.push_back(std::move(bg));
      }
      if (!one.empty())
        REXLOG_INFO("[superstar mods] backgrounds mod {}: {} background(s)", d.filename().string(), one.size());
    }
  }
  // One background drawn for this start (or the game's own): MnLayHD / MnLaySD.pac dressed, served from
  // Documents\wwe12\menu_background ahead of the mods folder (Wwe12MountMenuBackground).
  g_menu_dir.clear();
  if (!backgrounds.empty()) {
    const uint32_t seed = uint32_t(std::chrono::steady_clock::now().time_since_epoch().count() / 1000);
    const size_t pick = (seed * 2654435761u >> 8) % (backgrounds.size() + 1);
    const fs::path dir = user_root / "menu_background";
    fs::remove_all(dir, ec);
    if (pick > 0) {
      int dressed = 0;
      for (const char *rel : {"pac/menu/MnLayHD.pac", "pac/menu/MnLaySD.pac"}) {
        Bytes f;
        if (ReadGame(vfs, rel, f) && superstar_pack::DressMenu(f, backgrounds[pick - 1]) && WriteHost(dir / fs::path(rel), f))
          ++dressed;
      }
      if (dressed)
        g_menu_dir = dir;
      REXLOG_INFO("[superstar mods] main menu background {} of {} ({}): {} file(s)", pick, backgrounds.size(),
                  backgrounds[pick - 1].key, dressed);
    }
  }
  MenuPictures(vfs, user_root, mods);
  if (list.empty() && attires.empty() && media.empty() && signs.empty() && movesets.empty()) {
    AddRestores(GameChEtc(vfs, content));
    RemovePack(content, headers);
    return;
  }
  // The installed packs (not ours): their numbers, catalogs and folders, newest first.
  superstar_pack::Inputs in;
  std::vector<std::pair<int, fs::path>> packs;
  for (fs::directory_iterator it(content, ec), end; !ec && it != end; it.increment(ec)) {
    if (!it->is_directory(ec) || it->path().filename() == kPackName)
      continue;
    Bytes cat;
    if (!ReadHost(it->path() / "info" / "catalog.dlc", cat) || cat.size() < 12)
      continue;
    const int number = cat[8] | cat[9] << 8;
    in.catalogs.push_back({number, cat});
    packs.push_back({number, it->path()});
  }
  std::sort(packs.begin(), packs.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
  auto latest = [&](const std::string &rel, Bytes &out) {
    for (const auto &[n, dir] : packs)
      if (ReadHost(dir / fs::path(rel), out))
        return true;
    return ReadGame(vfs, rel, out);
  };
  if (!latest("pac/ch/chEtc.pac", in.chetc) || !latest("pac/DLC.pac", in.dlc)) {
    REXLOG_WARN("[superstar mods] chEtc.pac or DLC.pac unreadable: no added superstars");
    return;
  }
  // Portraits: the packs' DLC_HD/SD_dlcNN.pac, the disc's DLC_HD/SD.pac and menu\menuHD/SD.pac.
  for (const auto &[n, dir] : packs)
    for (fs::directory_iterator it(dir / "pac", ec), end; !ec && it != end; it.increment(ec)) {
      const std::string f = it->path().filename().string();
      Bytes b;
      if (f.rfind("DLC_HD_", 0) == 0 && ReadHost(it->path(), b))
        in.portraits_hd.push_back(std::move(b));
      else if (f.rfind("DLC_SD_", 0) == 0 && ReadHost(it->path(), b))
        in.portraits_sd.push_back(std::move(b));
    }
  for (const auto &[rel, hd] : std::vector<std::pair<std::string, bool>>{
           {"pac/DLC_HD.pac", true}, {"pac/menu/menuHD.pac", true}, {"pac/DLC_SD.pac", false}, {"pac/menu/menuSD.pac", false}}) {
    Bytes b;
    if (ReadGame(vfs, rel, b))
      (hd ? in.portraits_hd : in.portraits_sd).push_back(std::move(b));
  }
  // Slots: each mod keeps its number; new ones take the first free slots.
  std::map<std::string, int> kept;
  {
    Bytes s;
    if (ReadHost(user_root / "superstar_slots.txt", s)) {
      const std::string text(s.begin(), s.end());
      size_t pos = 0;
      while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos)
          end = text.size();
        std::string line = text.substr(pos, end - pos);
        if (!line.empty() && line.back() == '\r')
          line.pop_back();
        const size_t eq = line.rfind('=');
        if (eq != std::string::npos)
          kept[line.substr(0, eq)] = atoi(line.c_str() + eq + 1);
        pos = end + 1;
      }
    }
  }
  const std::vector<int> free_slots = superstar_pack::FreeSlots(in);
  std::vector<int> taken;
  std::vector<superstar_pack::Star> stars;
  for (const auto &mod : list) {
    superstar_pack::Star st;
    st.key = mod.key;
    st.name = mod.name;
    st.base = mod.base;
    st.attributes = mod.attributes;
    st.record = mod.record;
    st.moves = mod.moves;
    st.entrance = mod.entrance;
    st.own_entrance = mod.own_entrance;
    st.new_moves = mod.new_moves;
    if (!mod.theme.empty())
      ReadHost(mod.theme, st.theme);
    const auto k = kept.find(mod.key);
    if (k != kept.end() && std::find(free_slots.begin(), free_slots.end(), k->second) != free_slots.end() &&
        std::find(taken.begin(), taken.end(), k->second) == taken.end())
      st.slot = k->second;
    stars.push_back(std::move(st));
    if (stars.back().slot)
      taken.push_back(stars.back().slot);
  }
  for (auto &st : stars) {
    if (st.slot)
      continue;
    for (int s : free_slots)
      if (std::find(taken.begin(), taken.end(), s) == taken.end()) {
        st.slot = s;
        taken.push_back(s);
        break;
      }
  }
  // Models: the mod's, else the base's (the disc's chNNN.pac, or a pack's chNN_dlcXX.pac).
  for (size_t i = 0; i < stars.size(); ++i) {
    auto &st = stars[i];
    if (!st.slot) {
      REXLOG_WARN("[superstar mods] {}: no free superstar slot left", st.key);
      continue;
    }
    // Its own portraits, if any (superstar\SSFA_HD.dds ... SSFC_SD.dds).
    for (int hd = 0; hd < 2; ++hd)
      for (int k = 0; k < 3; ++k)
        ReadHost(list[i].model.parent_path() / (std::string("SSF") + char('A' + k) + (hd ? "_HD.dds" : "_SD.dds")),
                 st.portraits[hd][k]);
    if (ReadHost(list[i].model, st.model)) {
      if (REXCVAR_GET(wwe12_belt_donor)) {
        static const std::set<int> divas = Divas(in.chetc);
        FitBelt(vfs, user_root, st, divas);
      }
      continue;
    }
    if (!ReadGame(vfs, "pac/ch/ch" + std::to_string(st.base) + ".pac", st.model))
      for (const auto &[n, dir] : packs)
        for (fs::directory_iterator it(dir / "pac" / "ch", ec), end; !ec && it != end && st.model.empty(); it.increment(ec))
          if (it->path().filename().string().rfind("ch" + std::to_string(st.base) + "_dlc", 0) == 0)
            ReadHost(it->path(), st.model);
    if (st.model.empty())
      REXLOG_WARN("[superstar mods] {}: no model (superstar\\ch.pac) and none for base {}", st.key, st.base);
  }
  // Entrance videos: a number that no video of the disc or of the installed packs has (the slot's if free; the
  // game treats 500-550 and 902-904 apart).
  {
    std::vector<int> used;
    auto taken_number = [&](int n) {
      if (std::find(used.begin(), used.end(), n) != used.end())
        return true;
      char rel[40];
      snprintf(rel, sizeof(rel), "movies/titantron/%03d.bik", n);
      std::string game = std::string("game:/") + rel;
      std::replace(game.begin(), game.end(), '/', char(92));
      if (vfs && vfs->ResolvePath(game))
        return true;
      for (const auto &[p, dir] : packs)
        if (fs::exists(dir / fs::path(rel), ec))
          return true;
      return false;
    };
    for (size_t i = 0; i < stars.size(); ++i) {
      auto &st = stars[i];
      if (!st.slot || st.model.empty() || !ReadHost(list[i].movie, st.movie))
        continue;
      // (a movie whose biggest frame is over 120 KB, or whose frames average over 32 KB, froze the loading before the
      // entrance: Seth Rollins' 2K16 titantron, 2026-11-04; the game's own stay under 108 KB / 26.7 KB) left out
      if (st.movie.size() >= 16 && !memcmp(st.movie.data(), "BIK", 3)) {
        const uint32_t largest = uint32_t(st.movie[12]) | uint32_t(st.movie[13]) << 8 | uint32_t(st.movie[14]) << 16 |
                                 uint32_t(st.movie[15]) << 24;
        const uint32_t frames = uint32_t(st.movie[8]) | uint32_t(st.movie[9]) << 8 | uint32_t(st.movie[10]) << 16 |
                                uint32_t(st.movie[11]) << 24;
        const size_t average = frames ? st.movie.size() / frames : 0;
        // (the rate too: the game's own average at most ~26.7 KB a frame; Rollins' remade one, 53 KB a frame with
        // every frame under 100 KB, froze the same)
        if (largest > 120000 || average > 32000) {
          REXLOG_WARN("[superstar mods] {}: entrance video left out, frames of {} bytes at most, {} on average (the game"
                      " freezes over ~120 KB / ~32 KB); make it again with the Mod Maker",
                      st.key, largest, average);
          st.movie.clear();
          continue;
        }
      }
      std::vector<int> candidates = {st.slot};
      for (const auto &[from, to] : std::vector<std::pair<int, int>>{{373, 399}, {428, 499}, {551, 615}, {726, 899}})
        for (int n = from; n <= to; ++n)
          candidates.push_back(n);
      for (int n : candidates)
        if (!taken_number(n)) {
          st.movie_number = n;
          used.push_back(n);
          break;
        }
      if (!st.movie_number) {
        REXLOG_WARN("[superstar mods] {}: no free video number", st.key);
        st.movie.clear();
      }
    }
  }
  stars.erase(std::remove_if(stars.begin(), stars.end(), [](const auto &s) { return !s.slot || s.model.empty(); }),
              stars.end());
  g_movies.clear();
  g_mod_movies.clear();
  g_own_screens.clear();
  g_themes.clear();
  std::map<std::string, fs::path> star_dirs;  // (its superstar\ folder)
  for (const auto &m : list)
    star_dirs[m.key] = m.movie.parent_path();
  for (const auto &st : stars) {
    if (st.movie_number) {
      g_movies[uint32_t(st.slot)] = uint32_t(st.movie_number);
      g_mod_movies[st.key] = uint32_t(st.movie_number);
      std::error_code e1;
      for (const auto &[kind, file] : {std::pair<uint32_t, const char *>{1, "ribbon.bik"}, {2, "wall.bik"}})
        if (OwnScreenFits(star_dirs[st.key] / file))
          g_own_screens.insert({kind, uint32_t(st.movie_number)});  // (else left out, wwe12_arena_pages.cpp)
    }
    if (!st.theme.empty())
      g_themes.push_back({uint32_t(superstar_pack::detail::MusicNumber(st)), st.name});
  }
  for (const auto &mv : movesets)
    if (!mv.theme.empty())
      g_themes.push_back({uint32_t(superstar_pack::detail::MusicNumber(mv)), mv.name.empty() ? mv.key : mv.name});
  // A superstar of the pack whose mod is gone: the pack stays as it is (a save may use him), with a warning.
  {
    std::vector<std::string> gone;
    for (const auto &[key, slot] : PackStars(content / kPackName))
      if (std::none_of(stars.begin(), stars.end(), [&](const auto &st) { return st.key == key; }))
        gone.push_back(key + " (" + std::to_string(slot) + ")");
    std::error_code e2;
    if (!gone.empty()) {
      std::string list;
      for (const auto &g : gone)
        list += (list.empty() ? "" : ", ") + g;
      REXLOG_WARN("[superstar mods] superstar mod(s) gone but still in the pack: {}. Pack {} kept as it was (a save "
                  "may use them); take them out with the Mod Maker's CHECK MODS once no save uses them",
                  list, kPackName);
      const std::string text = "missing=" + list + "\n";
      WriteHost(user_root / "mods_warning.txt", Bytes(text.begin(), text.end()));
      g_records.clear();  // (the kept pack's records are the save's already; its videos keep their numbers)
      g_moves.clear();
      return;
    }
    fs::remove(user_root / "mods_warning.txt", e2);
  }
  if (stars.empty() && attires.empty() && media.empty() && signs.empty() && movesets.empty()) {
    g_moves.clear();
    AddRestores(in.chetc);
    RemovePack(content, headers);
    return;
  }
  std::vector<superstar_pack::File> files;
  std::string why;
  latest("pac/audience.pac", in.audience);
  // (attire lines of new attires, attires' own names: into copies of misc.pac and string.pac)
  if (std::any_of(attires.begin(), attires.end(), [](const auto &a) { return a.added || !a.label.empty(); }))
    latest("pac/misc.pac", in.misc);
  // (their names: text numbers from 60000, one per name, answered by the text lookup hook below)
  g_attire_texts.clear();
  {
    std::map<std::string, uint32_t> number;
    for (auto &at : attires)
      if (!at.label.empty()) {
        auto [it, fresh] = number.emplace(at.label, kAttireText + uint32_t(number.size()));
        at.label_id = it->second;
        g_attire_texts[it->second] = at.label;
      }
  }
  if (!stars.empty()) {
    if (in.misc.empty())
      latest("pac/misc.pac", in.misc);  // (moves to add, attire lines: they go into a copy of it)
    // Superstar Threads: the newest pack's table (pac\edit\edit_dlcNN.pac, the number is the pack's), else the disc's.
    for (const auto &[n, dir] : packs) {
      for (fs::directory_iterator it(dir / "pac" / "edit", ec), end; !ec && it != end; it.increment(ec))
        if (it->path().filename().string().rfind("edit_dlc", 0) == 0 && ReadHost(it->path(), in.edit))
          break;
      if (!in.edit.empty())
        break;
    }
    if (in.edit.empty())
      ReadGame(vfs, "pac/edit/edit.pac", in.edit);
  }
  // (the moves mods' added moves: into a copy of misc.pac too)
  if (in.misc.empty() && std::any_of(movesets.begin(), movesets.end(), [](const auto &mv) { return !mv.new_moves.empty(); }))
    latest("pac/misc.pac", in.misc);
  // Themes: the game's sound\Music.pck (read in parts) and the newest pack's pac\sound_dlc_NN.pac.
  if (std::any_of(stars.begin(), stars.end(), [](const auto &s) { return !s.theme.empty(); }) ||
      std::any_of(movesets.begin(), movesets.end(), [](const auto &mv) { return !mv.theme.empty(); })) {
    in.music.Open([vfs](uint64_t at, uint32_t size, Bytes &out) { return ReadGamePart(vfs, "sound/Music.pck", at, size, out); });
    for (const auto &[n, dir] : packs) {
      for (fs::directory_iterator it(dir / "pac", ec), end; !ec && it != end; it.increment(ec))
        if (it->path().filename().string().rfind("sound_dlc_", 0) == 0 && ReadHost(it->path(), in.sound_pac))
          break;
      if (!in.sound_pac.empty())
        break;
    }
  }
  if (!superstar_pack::Build(in, stars, files, &why, attires, media, signs, movesets)) {
    REXLOG_WARN("[superstar mods] pack not made: {}", why);
    return;
  }
  if (!why.empty())
    REXLOG_WARN("[superstar mods] {}", why);
  {  // (what the string.pac way left, 2026-11-02)
    std::error_code e2;
    fs::remove_all(user_root / "attire_names", e2);
  }
  // The added superstars' entrance videos served over the game's files too (Documents\wwe12\pack_movies\movies\
  // titantron\NNN.bik): the game looked for Mankind's 108 in no pack, only GAME:, though the pack lists it (Warrior's 104,
  // listed after it, played; 2026-11-02, cause not found). Written when missing or another size; others removed.
  {
    const fs::path dir = user_root / "pack_movies", mdir = dir / "movies" / "titantron";
    std::error_code e2;
    std::set<std::string> keep;
    for (const auto &st : stars) {
      if (st.movie.empty() || st.movie_number <= 0 || st.movie_number >= 1000)
        continue;
      char name[16];
      snprintf(name, sizeof(name), "%03d.bik", st.movie_number);
      keep.insert(name);
      const fs::path to = mdir / name;
      if (!fs::exists(to, e2) || fs::file_size(to, e2) != st.movie.size())
        WriteHost(to, st.movie);
    }
    for (fs::directory_iterator it(mdir, e2), end; !e2 && it != end; it.increment(e2))
      if (!keep.count(it->path().filename().string()))
        fs::remove(it->path(), e2);
    std::string game_device;
    if (!keep.empty() && vfs && vfs->FindSymbolicLink("game:", game_device)) {
      auto overlay = std::make_unique<rex::filesystem::HostPathDevice>(game_device, dir, true);
      if (overlay->Initialize() && vfs->RegisterOverlay(game_device, std::move(overlay)))
        REXLOG_INFO("[superstar mods] {} entrance video(s) served from {}", keep.size(), rex::path_to_utf8(dir));
    }
  }
  for (const auto &f : files)
    if (f.path.rfind("sound/", 0) == 0 || f.path.rfind("pac/sound_dlc_", 0) == 0)
      REXLOG_INFO("[superstar mods] themes: {} ({} bytes)", f.path, f.data.size());
  // Their records as the save keeps them (the select screen's roster, below): the pack's CHAR / DAT record without
  // its first 4 bytes, its 16-bit numbers big-endian (13 of them, found on 254 records of a save, 2026-10-26).
  g_records.clear();
  g_moves.clear();
  g_originals.clear();
  {
    svrfmt::Archive etc;
    if (svrfmt::ArchiveOpen(in.chetc, etc, nullptr))
      for (auto &k : etc.root.kids)
        if (k.group == "CHAR" && k.name == "PRO " && svrfmt::ArchiveExpand(k))
          for (auto &c : k.kids) {
            if (svrfmt::ArchiveData(c).size() != 1072)
              continue;
            if (c.id == 139)
              g_cena_moves = SavedMoves(svrfmt::ArchiveData(c));
            if (std::any_of(movesets.begin(), movesets.end(), [&](const auto &mv) { return uint32_t(mv.superstar) == c.id; }))
              g_originals[c.id] = SavedMoves(svrfmt::ArchiveData(c));
          }
  }
  for (const auto &f : files) {
    svrfmt::Archive etc;
    if (f.path != "pac/ch/chEtc.pac" || !svrfmt::ArchiveOpen(f.data, etc, nullptr))
      continue;
    for (auto &k : etc.root.kids)
      if (k.group == "CHAR" && k.name == "PRO " && svrfmt::ArchiveExpand(k))
        for (auto &c : k.kids) {
          const Bytes d = svrfmt::ArchiveData(c);
          if (d.size() == 1072 &&
              std::any_of(stars.begin(), stars.end(), [&](const auto &st) { return uint32_t(st.slot) == c.id; }))
            g_moves[c.id] = SavedMoves(d);
          // (a moves mod's superstar: the save's copy of his moves and entrance made again too, so a save made before
          // the mod gets them; it also undoes his moveset edited in the game while the mod is on)
          if (d.size() == 1072 &&
              std::any_of(movesets.begin(), movesets.end(), [&](const auto &mv) { return uint32_t(mv.superstar) == c.id; })) {
            g_moves[c.id] = SavedMoves(d);
            REXLOG_INFO("[superstar mods] superstar {}: his moves and entrance from a moves mod", c.id);
          }
        }
    for (auto &k : etc.root.kids)
      if (k.group == "CHAR" && k.name == "DAT " && svrfmt::ArchiveExpand(k))
        for (auto &c : k.kids) {
          const Bytes d = svrfmt::ArchiveData(c);
          if (d.size() != 290 || std::none_of(stars.begin(), stars.end(), [&](const auto &st) { return uint32_t(st.slot) == c.id; }))
            continue;
          Bytes r(d.begin() + 4, d.end());
          for (const size_t at : {36, 38, 40, 236, 238, 240, 242, 244, 252, 254, 268, 270, 272})
            std::swap(r[at], r[at + 1]);
          g_records[c.id] = std::move(r);
        }
  }
  AddRestores(in.chetc);
  // The numbers kept for next time.
  {
    std::string text;
    for (const auto &[key, slot] : kept)
      if (std::none_of(stars.begin(), stars.end(), [&](const auto &s) { return s.key == key; }))
        text += key + "=" + std::to_string(slot) + "\n";  // (a mod switched off keeps its number)
    for (const auto &st : stars)
      text += st.key + "=" + std::to_string(st.slot) + "\n";
    WriteHost(user_root / "superstar_slots.txt", Bytes(text.begin(), text.end()));
    // and their own video numbers (their ribbon and wall are served under it; wwe12_arena_pages.cpp mounts the
    // entrances before this runs, so it reads them from here, as the slots)
    std::string movies;
    for (const auto &[key, n] : g_mod_movies)
      movies += key + "=" + std::to_string(n) + "\n";
    WriteHost(user_root / "superstar_movies.txt", Bytes(movies.begin(), movies.end()));
  }
  // Written again only when it changed (the videos too: their numbers and contents are in the files).
  uint64_t h = 0xCBF29CE484222325ull;
  for (const auto &f : files) {
    h = Fnv(Bytes(f.path.begin(), f.path.end()), h);
    h = Fnv(f.data, h);
  }
  char stamp[32];
  snprintf(stamp, sizeof(stamp), "%016llx", static_cast<unsigned long long>(h));
  Bytes old;
  const fs::path stamp_file = content / kPackName / "wwe12pc_stamp.txt";
  Bytes old_header;  // (one made without the license mask, below, is made again)
  if (ReadHost(stamp_file, old) && std::string(old.begin(), old.end()) == stamp &&
      ReadHost(headers / (std::string(kPackName) + ".header"), old_header) && old_header.size() == 332 &&
      old_header[0x148] == 0xFF && old_header[0x14B] == 0xFF) {
    REXLOG_INFO("[superstar mods] {} added superstar(s), pack {} unchanged", stars.size(), kPackName);
    if (!fs::exists(content / kPackName / "wwe12pc_stars.txt", ec)) {  // (packs made before the list)
      std::string list;
      for (const auto &st : stars)
        list += st.key + "=" + std::to_string(st.slot) + "\n";
      WriteHost(content / kPackName / "wwe12pc_stars.txt", Bytes(list.begin(), list.end()));
    }
    return;
  }
  if (fs::exists(content / kPackName, ec))
    BackupSave(user_root, "before the mods pack (WWE12PCMODS) changed");
  fs::remove_all(content / kPackName, ec);
  for (const auto &f : files)
    if (!WriteHost(content / kPackName / fs::path(f.path), f.data)) {
      REXLOG_WARN("[superstar mods] could not write {}", f.path);
      return;
    }
  WriteHost(stamp_file, Bytes(stamp, stamp + 16));
  {
    std::string list;
    for (const auto &st : stars)
      list += st.key + "=" + std::to_string(st.slot) + "\n";
    WriteHost(content / kPackName / "wwe12pc_stars.txt", Bytes(list.begin(), list.end()));
  }
  // Its header, as an installed pack's (the first one found, else a blank one): device 1, type 2, display name
  // (UTF-16BE) at 8, file name at 0x108.
  Bytes header;
  for (fs::directory_iterator it(headers, ec), end; !ec && it != end && header.size() != 332; it.increment(ec))
    if (it->path().stem() != kPackName && !ReadHost(it->path(), header))
      header.clear();
  if (header.size() != 332) {
    header.assign(332, 0);
    header[3] = 1;
    header[7] = 2;
  }
  // The game's title (0x140, big-endian) and the license mask after the content data (0x148, all bits): a blank
  // header had none, the game took the pack as not owned (its content list state 1, not 2: sub_826B13D0) and its
  // superstars showed nowhere when no pack of the game's was installed (2026-10-28).
  const uint8_t title[4] = {0x54, 0x51, 0x08, 0x90};
  std::copy(title, title + 4, header.begin() + 0x140);
  std::fill(header.begin() + 0x148, header.begin() + 0x14C, uint8_t(0xFF));
  std::fill(header.begin() + 8, header.begin() + 0x108 + 42, uint8_t(0));
  const std::string display = "WWE '12 PC superstar mods";
  for (size_t i = 0; i < display.size(); ++i)
    header[9 + 2 * i] = uint8_t(display[i]);
  for (size_t i = 0; kPackName[i]; ++i)
    header[0x108 + i] = uint8_t(kPackName[i]);
  WriteHost(headers / (std::string(kPackName) + ".header"), header);
  // The game's cache is made from the packs present (wwe12_dlc.cpp): emptied for the new one.
  for (fs::directory_iterator it(user_root / "xbox_cache", ec), end; !ec && it != end; it.increment(ec))
    fs::remove_all(it->path(), ec);
  for (const auto &st : stars)
    REXLOG_INFO("[superstar mods] {} \"{}\" = superstar {} (base {}){}", st.key, st.name, st.slot, st.base,
                st.movie_number ? ", own entrance video " + std::to_string(st.movie_number) : std::string());
  REXLOG_INFO("[superstar mods] pack {} written ({} files), game cache emptied", kPackName, files.size());
}

// PLAY WITHOUT MODS (--wwe12_no_mods): the pack goes too, but a save that uses its superstars may be refused or
// rewritten without them, so this game's saving doesn't count. Before the game lists its packs, the save (and the
// achievements) is copied and the pack moved into Documents\wwe12\NoModsSession; the next start puts both back
// (before anything else reads them), whatever this game wrote. A step that fails leaves the session as it is, to be
// tried again at the next start.
namespace {

fs::path NoModsDir(const fs::path &user_root) { return user_root / "NoModsSession"; }

void EmptyGameCache(const fs::path &user_root) {
  std::error_code ec;
  for (fs::directory_iterator it(user_root / "xbox_cache", ec), end; !ec && it != end; it.increment(ec))
    fs::remove_all(it->path(), ec);
}

}  // namespace

void Wwe12NoModsRestore(const fs::path &user_root) {
  const fs::path session = NoModsDir(user_root);
  std::error_code ec;
  if (user_root.empty() || !fs::exists(session / "session.txt", ec))
    return;
  const fs::path content = user_root / "0000000000000000" / "54510890" / "00000002",
                 headers = user_root / "0000000000000000" / "54510890" / "Headers" / "00000002",
                 save = user_root / "B13EBABEBABEBABE" / "54510890",
                 ach = user_root / "achievements" / "54510890.toml";
  bool ok = true;
  auto put_back = [&](const fs::path &from, const fs::path &to) {
    std::error_code e;
    if (!fs::exists(from, e))
      return;
    fs::remove_all(to, e);
    fs::create_directories(to.parent_path(), e);
    fs::rename(from, to, e);
    if (e) {
      REXLOG_WARN("[no mods] could not put {} back: {}", rex::path_to_utf8(to), e.message());
      ok = false;
    }
  };
  put_back(session / "pack", content / kPackName);
  put_back(session / (std::string(kPackName) + ".header"), headers / (std::string(kPackName) + ".header"));
  if (fs::exists(session / "no_save", ec))
    fs::remove_all(save, ec);  // (there was none: the one this game made goes)
  put_back(session / "save", save);
  if (fs::exists(session / "no_achievements", ec))
    fs::remove(ach, ec);
  put_back(session / "achievements.toml", ach);
  if (!ok)
    return;
  EmptyGameCache(user_root);
  fs::remove_all(session, ec);
  REXLOG_INFO("[no mods] back from a game without mods: save and pack {} as before it", kPackName);
}

void Wwe12NoModsHide(const fs::path &user_root) {
  const fs::path session = NoModsDir(user_root);
  const fs::path content = user_root / "0000000000000000" / "54510890" / "00000002",
                 headers = user_root / "0000000000000000" / "54510890" / "Headers" / "00000002",
                 save = user_root / "B13EBABEBABEBABE" / "54510890",
                 ach = user_root / "achievements" / "54510890.toml";
  std::error_code ec;
  if (user_root.empty() || fs::exists(session, ec) || !fs::exists(content / kPackName, ec))
    return;  // (no pack: nothing to hide, saving stays as usual)
  fs::create_directories(session, ec);
  if (fs::exists(save, ec))
    fs::copy(save, session / "save", fs::copy_options::recursive, ec);
  else
    std::ofstream(session / "no_save") << "1\n";
  if (!ec && fs::exists(ach))
    fs::copy_file(ach, session / "achievements.toml", ec);
  else if (!ec)
    std::ofstream(session / "no_achievements") << "1\n";
  if (ec) {
    REXLOG_WARN("[no mods] the save could not be copied ({}): pack {} left in", ec.message(), kPackName);
    std::error_code ignore;
    fs::remove_all(session, ignore);
    return;
  }
  std::ofstream(session / "session.txt") << "a game without mods: its saving is undone at the next start\n";
  fs::rename(content / kPackName, session / "pack", ec);
  if (ec) {
    REXLOG_WARN("[no mods] pack {} could not be moved ({}): left in", kPackName, ec.message());
    Wwe12NoModsRestore(user_root);
    return;
  }
  fs::rename(headers / (std::string(kPackName) + ".header"), session / (std::string(kPackName) + ".header"), ec);
  EmptyGameCache(user_root);
  REXLOG_INFO("[no mods] pack {} put aside and the save copied: this game's saving is undone at the next start",
              kPackName);
}

REX_EXTERN(__imp__sub_82A316A0);  // the entrance video's path (n): movies\titantron\%03d.bik
REX_EXTERN(__imp__sub_828C21A8);  // entrance: the current event
REX_EXTERN(__imp__sub_828CEF58);  // ... its entrant {+8 set, +16 place}
REX_EXTERN(__imp__sub_823F8270);  // place -> ...
REX_EXTERN(__imp__sub_8267CD30);  // ... -> its Superstar (+16 number x 100 + attire)

namespace {

// The Superstar entering now (its number), 0 if none (outside an entrance).
uint32_t Entering(const PPCContext &ctx, uint8_t *base) {
  auto call = [&](void (*fn)(PPCContext &, uint8_t *), uint32_t a) {
    PPCContext x = ctx;
    x.r3.u64 = a;
    fn(x, base);
    return x.r3.u32;
  };
  if (!bd::mem::load<uint32_t>(0x8324DF2C))  // (the entrance manager sub_828C21A8 reads)
    return 0;
  const uint32_t event = call(__imp__sub_828C21A8, 0);
  if (!event)
    return 0;
  const uint32_t entrant = call(__imp__sub_828CEF58, event);
  if (!entrant || !bd::mem::load<uint8_t>(entrant + 8))
    return 0;
  const uint32_t superstar = call(__imp__sub_8267CD30, call(__imp__sub_823F8270, bd::mem::load<uint32_t>(entrant + 16)));
  return superstar ? bd::mem::load<uint32_t>(superstar + 16) / 100 : 0;  // (number x 100 + attire: 8702)
}

}  // namespace

// The superstar entering now (his number), 0 if none (wwe12_arena_pages.cpp: his waist belt).
uint32_t Wwe12EnteringSuperstar(const PPCContext &ctx, uint8_t *base) { return Entering(ctx, base); }


// (wwe12_arena_pages.cpp: whether a media mod has a ribbon / wall for that video number)
bool Wwe12ModScreen(uint32_t kind, uint32_t n);

// The added superstars' own themes (music number, name), for the jukebox (wwe12_jukebox.cpp).
std::vector<std::pair<uint32_t, std::string>> Wwe12AddedThemes() { return g_themes; }

// How many added superstars the pack has (wwe12_heap_probe.cpp: the heaps grown only for many).
size_t Wwe12AddedSuperstarCount() { return g_records.size(); }

// A lookup of "MVSB/M%03d" / "MVSW/M%03d" (an entrant's ribbon / wall) while an added superstar with its own is
// entering: the name it is served under ("MVSB/M<its video number>"), else "".
std::string Wwe12OwnScreen(const std::string &asked, const PPCContext &ctx, uint8_t *base) {
  if (g_own_screens.empty() || asked.size() < 7 || asked.compare(0, 3, "MVS") || (asked[3] != 'B' && asked[3] != 'W') ||
      asked.compare(4, 2, "/M"))
    return {};
  const auto movie = g_movies.find(Entering(ctx, base));
  if (movie == g_movies.end() || !g_own_screens.count({asked[3] == 'B' ? 1u : 2u, movie->second}))
    return {};
  char name[16];
  snprintf(name, sizeof(name), "MVS%c/M%03u", asked[3], movie->second);
  return name;
}

// (not the full-screen videos the same function opens: 500-520, 640 x 640, a title match's title graphic before the
// entrances, and 900-909 (900: 1024 x 512). The first entrant is already "entering" then: an added superstar going
// first put his titantron in the title graphic's place, Discord 2026-11-05, Jay. The entrance videos are 320 x 320.)
static bool EntranceVideoNumber(uint32_t n) { return !(n >= 500 && n < 600) && !(n >= 900 && n < 910); }

extern "C" REX_FUNC(sub_82A316A0) {
  if (!g_movies.empty() && EntranceVideoNumber(ctx.r3.u32)) {
    const uint32_t id = Entering(ctx, base);
    const auto it = g_movies.find(id);
    REXLOG_INFO("[superstar mods] entrance video {} (superstar entering: {}){}", ctx.r3.u32, id,
                it != g_movies.end() ? " -> its own, " + std::to_string(it->second) : std::string());
    if (it != g_movies.end())
      ctx.r3.u64 = it->second;
  }
  __imp__sub_82A316A0(ctx, base);
}

// Whether video number n's titantron has a minitron (sub_82A31538(n): a byte per number, 1 = yes; else the stage
// screen shows the superstar's wall, MVSW, in its place). Slater / Gabriel, Miz / Morrison have none: a VIDEOS mod's
// titantron for them (made with its own minitron below) showed its wall there, or nothing without one (Max Matus'
// "Created Trons Glitch", 2026-11-04). A number whose titantron a VIDEOS mod replaced has its minitron.
REX_EXTERN(__imp__sub_82A31538);
extern "C" REX_FUNC(sub_82A31538) {
  const uint32_t n = ctx.r3.u32;
  __imp__sub_82A31538(ctx, base);
  if (ctx.r3.u32 != 0 && ctx.r3.u32 != 1 && g_mod_minitrons.count(n)) {
    static std::set<uint32_t> told;
    if (told.insert(n).second)
      REXLOG_INFO("[superstar mods] video {}: the game has no minitron for it ({}); its VIDEOS mod's minitron shown", n,
                  ctx.r3.u32);
    ctx.r3.u64 = 1;
  }
}

// The arena's entrance screens (sub_828D7xxx, once per entrance): with the entrant's video number n, sub_82A31578(n,
// kind) says whether it has a movie of that kind in pac\evt\evtmov.pac: 2 its wall (MVSW/M%03d, 256 x 256), 1 its
// ribbon (MVSB/M%03d, 1024 x 32, its name around the arena); without them the arena's are shown (its wall, its ribbon
// MVAB, kind 3). An added superstar has its base's video number: it showed the base's ribbon and wall (Ultimate
// Warrior on Ted DiBiase's 261, 2026-11-01). It has the arena's, unless its mod has its own (superstar\ribbon.bik,
// wall.bik, made by the Mod Maker): served in evtmov.pac under its own video number (wwe12_arena_pages.cpp), the
// game's lookup of its base's name sent there (Wwe12OwnScreen).
REX_EXTERN(__imp__sub_82A31578);
extern "C" REX_FUNC(sub_82A31578) {
  const uint32_t n = ctx.r3.u32, kind = ctx.r4.u32;
  __imp__sub_82A31578(ctx, base);
  if (kind != 1 && kind != 2)
    return;
  if (!g_records.empty()) {
    const uint32_t id = Entering(ctx, base);
    if (g_records.count(id)) {
      const auto movie = g_movies.find(id);
      const bool own = movie != g_movies.end() && g_own_screens.count({kind, movie->second});
      REXLOG_INFO("[superstar mods] superstar {} entering: {} {}{}", id, own ? "its own" : "the arena's",
                  kind == 1 ? "ribbon" : "wall", own ? " (" + std::to_string(movie->second) + ")" : std::string());
      ctx.r3.u64 = own ? 1 : 0;
      return;
    }
  }
  // a game superstar's ribbon / wall from a media mod (VIDEOS page; one it hadn't: added)
  if (Wwe12ModScreen(kind, n))
    ctx.r3.u64 = 1;
}

// The game's texts by number: sub_827DB6D0(texts, table, number) -> the text (guest address) or 0, by a binary search
// of each loaded string.pac table ({position, length, number, 0} rows); sub_827DB610, the same search, -> a byte of
// the table it is in (255: none). The attires' own names (kAttireText...) are answered when no table has them.
REX_EXTERN(__imp__sub_827DB6D0);
extern "C" REX_FUNC(sub_827DB6D0) {
  const uint32_t number = ctx.r5.u32;
  __imp__sub_827DB6D0(ctx, base);
  if (ctx.r3.u32 || number < kAttireText)
    return;
  const auto it = g_attire_texts.find(number);
  if (it == g_attire_texts.end())
    return;
  uint32_t &at = g_attire_text_at[number];
  if (!at) {
    at = REX_KERNEL_MEMORY()->SystemHeapAlloc(uint32_t(it->second.size() + 1), 4);
    if (at)
      memcpy(bd::mem::at<char>(at), it->second.c_str(), it->second.size() + 1);
  }
  ctx.r3.u64 = at;
}
REX_EXTERN(__imp__sub_827DB610);
extern "C" REX_FUNC(sub_827DB610) {
  const uint32_t texts = ctx.r3.u32, table = ctx.r4.u32, number = ctx.r5.u32;
  __imp__sub_827DB610(ctx, base);
  if (ctx.r3.u32 != 255 || number < kAttireText || !g_attire_texts.count(number))
    return;
  ctx.r3.u64 = texts, ctx.r4.u64 = table, ctx.r5.u64 = 9;  // (as "ORIGINAL ATTIRE"'s)
  __imp__sub_827DB610(ctx, base);
}

// The select screen's candidates (sub_828ADFB8: numbers 50..372 that the save says are unlocked). The save keeps its
// own copy of every superstar's record (the roster: [0x8317FA08] + index * 286, index = u16 table 0x8307B5E0[number];
// sub_82684BF8 / sub_82684910 read it), made when it first saw the superstar: its name, ratings and "unlocked" (byte
// 247) are shown from there. A number another mod had before kept that one's copy (FBX TEST at 86, where an old
// test's superstar was: hidden as locked, then shown as "DLC 37" with OVERALL 0 and a black screen at the match,
// 2026-10-26, wwe12_roster_probe): an added superstar's copy is made again from its record when it differs, unlocked.
void Wwe12UniverseForgetRelations(const PPCContext &in, uint8_t *base, uint32_t id);  // (wwe12_universe.cpp)
REXCVAR_DEFINE_BOOL(wwe12_added_relations_reset, true, "WWE12",
                    "Added superstars start the Universe with no allies or rivals (once per superstar and mod).");
// Its Universe allies and rivals: those of the number's former owner (Damien Sandow at 116 had Eve as ally and four
// Divas as rivals, 2026-11-05): emptied once per added superstar and mod (relations_cleared.txt: "number=hash" of
// its record, its brand and "unlocked" left out), by the Universe's Wwe12UniverseForgetRelations; the ones the
// player makes afterwards stay.
// Called by the select screen (below) and by the Universe menu (wwe12_universe.cpp, sub_82545A70): a player going
// straight to the Universe sees them empty too; the second call does nothing.
void Wwe12ForgetAddedRelations(const PPCContext &ctx, uint8_t *base) {
  if (bd::mem::load<uint32_t>(0x8317FA08) && !g_relations_file.empty() &&
      REXCVAR_GET(wwe12_added_relations_reset)) {
    static std::set<std::string> done;
    static bool read = false;
    if (!read) {
      read = true;
      Bytes f;
      if (ReadHost(g_relations_file, f)) {
        std::string s(f.begin(), f.end()), line;
        for (char c : s + "\n")
          if (c == '\n' || c == '\r') {
            if (!line.empty())
              done.insert(line);
            line.clear();
          } else {
            line += c;
          }
      }
    }
    bool wrote = false;
    for (const auto &[id, rec] : g_records) {
      uint32_t h = 2166136261u;
      for (size_t k = 0; k < rec.size(); ++k)
        if (k != 247 && (k < 24 || k >= 28))
          h = (h ^ rec[k]) * 16777619u;
      const std::string key = std::to_string(id) + "=" + std::to_string(h);
      if (done.count(key))
        continue;
      Wwe12UniverseForgetRelations(ctx, base, id);
      done.insert(key);
      wrote = true;
    }
    if (wrote) {
      std::string s;
      for (const auto &k : done)
        s += k + "\n";
      WriteHost(g_relations_file, Bytes(s.begin(), s.end()));
    }
  }
}

REXCVAR_DEFINE_BOOL(wwe12_superstar_roster_fix, true, "WWE12",
                    "Added superstars: the save's copy of their record made again when it differs (unlocked).");
REX_EXTERN(__imp__sub_828ADFB8);
extern "C" REX_FUNC(sub_828ADFB8) {
  {  // (once: the video numbers without a minitron, sub_82A31538's table, for the log)
    static bool told = false;
    const uint32_t table = bd::mem::load<uint32_t>(0x8324E254), count = bd::mem::load<uint32_t>(0x8324E264);
    if (!told && table && count) {
      told = true;
      std::string other;
      for (uint32_t n = 0; n < count && n < 1000; ++n)
        if (const uint8_t v = bd::mem::load<uint8_t>(table + n); v > 1)
          other += " " + std::to_string(n) + ":" + std::to_string(v);
      REXLOG_INFO("[superstar mods] video numbers without a minitron (of {}):{}", count, other);
    }
  }
  const uint32_t roster = REXCVAR_GET(wwe12_superstar_roster_fix) ? bd::mem::load<uint32_t>(0x8317FA08) : 0;
  for (const auto &[id, rec] : g_records) {
    const uint32_t index = id < 373 ? bd::mem::load<uint16_t>(0x8307B5E0 + id * 2) : 0xFFFF;
    if (!roster || index >= 373)
      continue;
    const uint32_t at = roster + index * 286;
    // (its brand, +24..+27 - the Universe reads +27, sub_826D2B50; EDIT BRAND and the transfers write there - is the
    // player's: compared, it made the copy again with the pack's brand at the next exhibition, Discord 2026-11-05,
    // ChrisX's Goldberg always back on SmackDown; found by the Universe conversation)
    bool same = true;
    for (uint32_t k = 0; k < 286 && same; ++k)
      same = k == 247 || (k >= 24 && k < 28) || bd::mem::load<uint8_t>(at + k) == rec[k];
    if (same && bd::mem::load<uint8_t>(at + 247))
      continue;
    for (uint32_t k = 0; k < 286; ++k)
      if (k != 247)
        bd::mem::store<uint8_t>(at + k, rec[k]);
    bd::mem::store<uint8_t>(at + 247, 1);
    REXLOG_INFO("[superstar mods] superstar {}: the save's copy of its record made again{}", id,
                same ? " (it was locked)" : "");
  }
  Wwe12ForgetAddedRelations(ctx, base);  // (their Universe allies and rivals, once)
  // Its moves and entrance too: the save keeps them after the records (SaveData.Dat: the records at 0x1C, the moves at
  // 324792, 1068 bytes each, same order). A disc's empty record's were empty: no entrance (FBX TEST at 108, 2026-10-26).
  // Only once that place holds John Cena's as they should be: his bytes past his moves and entrance blocks (from 486;
  // and before them, 0-5), which neither a moves mod nor a moveset edited in the game changes (2026-11-03; all 1068
  // before, and a moves mod for Cena made it fail).
  uint32_t moves = roster ? roster - 0x1C + 324792 : 0;
  const auto index_of = [](uint32_t id) { return id < 373 ? uint32_t(bd::mem::load<uint16_t>(0x8307B5E0 + id * 2)) : 0xFFFFu; };
  // (a few bytes may differ: something the save keeps about him changes as one plays - byte 658 was 01 for 00 in the
  // user's save, and every added superstar went without his moves and entrance, 2026-11-07; the right place matches
  // hundreds of bytes, so 8 at most may differ)
  const auto cena_at = [&](uint32_t base_moves) {
    if (!base_moves || g_cena_moves.size() != 1068 || index_of(139) >= 373)
      return false;
    int off = 0;
    for (uint32_t k = 0; k < 1068 && off <= 8; k = k == 5 ? 486 : k + 1)
      off += bd::mem::load<uint8_t>(base_moves + index_of(139) * 1068 + k) != g_cena_moves[k];
    return off <= 8;
  };
  bool found = cena_at(moves);
  // (not there: John Cena's record looked for in the save's memory, from the roster on - the moves' place taken from
  // where his is, if his index puts it on a whole record. SvR 2007 Lita, added, had no entrance, the game went straight
  // to her opponent's: "not found where expected", 2026-11-07; what was there is logged, to see why)
  if (moves && !found && !g_moves.empty() && g_cena_moves.size() == 1068 && index_of(139) < 373) {
    static bool told = false;
    const uint32_t expected = moves + index_of(139) * 1068;
    uint32_t first_diff = 0xFFFFFFFF;
    for (uint32_t k = 0; k < 1068 && first_diff == 0xFFFFFFFF; k = k == 5 ? 486 : k + 1)
      if (bd::mem::load<uint8_t>(expected + k) != g_cena_moves[k])
        first_diff = k;
    const uint8_t *from = bd::mem::at<uint8_t>(roster), *to = from + 8 * 1024 * 1024;
    const uint8_t *hit = std::search(from, to, g_cena_moves.begin() + 486, g_cena_moves.end());
    uint32_t moved = 0;
    if (hit != to) {
      const uint32_t rec = roster + uint32_t(hit - from) - 486;
      if (rec >= index_of(139) * 1068 && cena_at(rec - index_of(139) * 1068))
        moved = rec - index_of(139) * 1068;
    }
    if (!told)
      REXLOG_WARN("[superstar mods] the save's moves not where expected ({:08X}, Cena index {} at {:08X}, first byte off: "
                  "{}, there {:02X} wanted {:02X}); Cena's record found {}; moves place {}",
                  moves, index_of(139), expected, first_diff, first_diff < 1068 ? bd::mem::load<uint8_t>(expected + first_diff) : 0,
                  first_diff < 1068 ? g_cena_moves[first_diff] : 0,
                  hit != to ? fmt::format("at {:08X} (roster + {})", roster + uint32_t(hit - from) - 486, uint32_t(hit - from) - 486) : "nowhere",
                  moved ? fmt::format("{:08X} (expected + {})", moved, int64_t(moved) - int64_t(moves)) : "not found: added superstars' left as they are");
    told = true;
    if (moved) {
      moves = moved;
      found = true;
    }
  }
  std::vector<uint32_t> restored;
  for (const auto &[id, mv] : g_moves) {
    if (!found || index_of(id) >= 373)
      continue;
    if (g_moves_restore.count(id))
      restored.push_back(id);  // (put back once: then forgotten)
    const uint32_t at = moves + index_of(id) * 1068;
    bool same = true;
    for (uint32_t k = 0; k < 1068 && same; ++k)
      same = bd::mem::load<uint8_t>(at + k) == mv[k];
    if (same)
      continue;
    const uint64_t h = Fnv(mv, 0xCBF29CE484222325ull);
    Bytes now(1068);
    for (uint32_t k = 0; k < 1068; ++k)
      now[k] = bd::mem::load<uint8_t>(at + k);
    const uint64_t now_h = Fnv(now, 0xCBF29CE484222325ull);
    if (!g_moves_restore.count(id)) {
      const auto applied = g_applied.find(id);
      bool foreign = false;
      for (uint32_t k = 0; k < 1068 && !foreign; k = k == 5 ? 486 : k + 1)
        foreign = bd::mem::load<uint8_t>(at + k) != mv[k];
      bool original = false;
      if (const auto o = g_originals.find(id); o != g_originals.end()) {
        original = true;
        for (uint32_t k = 0; k < 1068 && original; ++k)
          original = bd::mem::load<uint8_t>(at + k) == o->second[k];
      }
      const auto before = g_applied_before.find(id);
      const bool unsaved = before != g_applied_before.end() && before->second == now_h;
      if (applied != g_applied.end() && applied->second == h && !foreign && !original && !unsaved)
        continue;  // (the mod's, edited in the game since: kept)
    }
    for (uint32_t k = 0; k < 1068; ++k)
      bd::mem::store<uint8_t>(at + k, mv[k]);
    REXLOG_INFO("[superstar mods] superstar {}: the save's copy of its moves and entrance made again", id);
    if (g_moves_restore.count(id)) {
      g_applied.erase(id);
      g_applied_before.erase(id);
    } else {
      // (what it held before: kept from the first time while the game hasn't saved since)
      if (!(g_applied_before.count(id) && g_applied_before[id] == now_h))
        g_applied_before[id] = now_h;
      g_applied[id] = h;
    }
    WriteAppliedFile();
  }
  for (uint32_t id : restored) {
    g_moves.erase(id);
    g_moves_restore.erase(id);
  }
  if (!restored.empty())
    WriteMovesFile();
  __imp__sub_828ADFB8(ctx, base);
}


// The dressed menu files (a main menu background drawn for this start), served ahead of the mods folder.
void Wwe12MountMenuBackground(rex::filesystem::VirtualFileSystem *vfs, const std::string &game_device) {
  if (g_menu_dir.empty())
    return;
  auto overlay = std::make_unique<rex::filesystem::HostPathDevice>(game_device, g_menu_dir, true);
  if (!overlay->Initialize() || !vfs->RegisterOverlay(game_device, std::move(overlay)))
    REXLOG_WARN("[superstar mods] could not serve {}", rex::path_to_utf8(g_menu_dir));
}
