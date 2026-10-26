// WWE '12: added superstars (docs/mod_maker.md, "Ajouter des catcheurs"). At startup, after the DLC packs are
// installed and before the game lists them, the superstar mods (mods\.svrmods\Superstars\<id>, type=superstar,
// not disabled: manifest name=, base=NNN; optional in superstar\: ch.pac, titantron.bik, its portraits SSFA_HD.dds ...
// SSFC_SD.dds) become one more DLC pack, WWE12PCMODS, made by
// modmaker/superstar_pack.h from the player's own files: the latest installed pack's (else the disc's) chEtc.pac,
// DLC.pac and portraits, the base superstars' models when a mod has none. Each mod keeps its number from one start
// to the next (Documents\wwe12\superstar_slots.txt: "<mod>=<number>"), so a save's records never move. The pack is
// written again only when its contents change (then the game's cache, made from the packs present, is emptied as
// for a new DLC: wwe12_dlc.cpp); without superstar mods it is removed.
//
// Entrance videos. An added superstar plays its base's entrance, whose script names the video to show (titantron
// on top, minitron below): sub_828F2410 reads that number (command + 172, from sub_828D83B8; 999 = the superstar's own,
// for created ones), and sub_82A316A0(n) opens movies/titantron/%03d.bik (the packs first).
// A mod's superstar/titantron.bik goes into the pack under a number no other video has, and when an entrance video
// is opened for that superstar (the one entering: sub_828CEF58(sub_828C21A8()) -> {+8 set, +16 place}, its Superstar
// sub_8267CD30(sub_823F8270(place)) + 16 = number x 100 + attire), the path is made with that number instead.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
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

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

#include "../modmaker/superstar_pack.h"

REXCVAR_DEFINE_BOOL(wwe12_superstar_mods, true, "WWE12",
                    "Added superstars (superstar mods) as a DLC pack made at startup.");

namespace {

namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;

constexpr const char *kPackName = "WWE12PCMODS";

std::map<uint32_t, uint32_t> g_movies;  // added superstar -> the number of its own entrance video
fs::path g_menu_dir;                    // the dressed menu files of this start, "" = the game's
std::map<uint32_t, Bytes> g_records;    // added superstar -> its record as the save keeps it (286 bytes)
std::map<uint32_t, Bytes> g_moves;      // ... its moves and entrance (CHAR / PRO) as the save keeps them (1068 bytes)
Bytes g_cena_moves;                     // John Cena's (139), as the save keeps them: where they are is checked on his

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

uint64_t Fnv(const Bytes &b, uint64_t h) {
  for (uint8_t c : b)
    h = (h ^ c) * 0x100000001B3ull;
  return h;
}

void RemovePack(const fs::path &content, const fs::path &headers) {
  std::error_code ec;
  if (fs::exists(content / kPackName, ec)) {
    fs::remove_all(content / kPackName, ec);
    fs::remove(headers / (std::string(kPackName) + ".header"), ec);
    REXLOG_INFO("[superstar mods] no superstar mods: pack {} removed", kPackName);
  }
}

}  // namespace

void Wwe12MountSuperstarMods(rex::filesystem::VirtualFileSystem *vfs, const fs::path &user_root,
                             const fs::path &mods) {
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
    fs::path model, movie;
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
    mod.model = d / "superstar" / "ch.pac";
    mod.movie = d / "superstar" / "titantron.bik";
    if (mod.base < 50 || mod.base >= 373) {
      REXLOG_WARN("[superstar mods] {}: base={} is not a superstar of the game", mod.key, Value(text, "base"));
      continue;
    }
    list.push_back(mod);
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
      // A new attire: one his file (the game's, the latest pack's or the disc's) hasn't.
      {
        Bytes game;
        svrfmt::Archive g;
        at.added = ReadGame(vfs, "pac/ch/ch" + std::to_string(at.superstar) + ".pac", game) &&
                   svrfmt::ArchiveOpen(game, g, nullptr) &&
                   std::none_of(g.root.kids.begin(), g.root.kids.end(), [&](const auto &k) { return k.name == at.entry; });
      }
      REXLOG_INFO("[superstar mods] attire mod {}: superstar {}, attire {}{}", at.key, at.superstar, at.entry,
                  at.added ? " (a new one)" : "");
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
  if (list.empty() && attires.empty() && media.empty() && signs.empty()) {
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
    if (ReadHost(list[i].model, st.model))
      continue;
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
  for (const auto &st : stars)
    if (st.movie_number)
      g_movies[uint32_t(st.slot)] = uint32_t(st.movie_number);
  if (stars.empty() && attires.empty() && media.empty() && signs.empty()) {
    RemovePack(content, headers);
    return;
  }
  std::vector<superstar_pack::File> files;
  std::string why;
  latest("pac/audience.pac", in.audience);
  if (!superstar_pack::Build(in, stars, files, &why, attires, media, signs)) {
    REXLOG_WARN("[superstar mods] pack not made: {}", why);
    return;
  }
  // Their records as the save keeps them (the select screen's roster, below): the pack's CHAR / DAT record without
  // its first 4 bytes, its 16-bit numbers big-endian (13 of them, found on 254 records of a save, 2026-10-26).
  g_records.clear();
  g_moves.clear();
  {
    svrfmt::Archive etc;
    if (svrfmt::ArchiveOpen(in.chetc, etc, nullptr))
      for (auto &k : etc.root.kids)
        if (k.group == "CHAR" && k.name == "PRO " && svrfmt::ArchiveExpand(k))
          for (auto &c : k.kids)
            if (c.id == 139 && svrfmt::ArchiveData(c).size() == 1072)
              g_cena_moves = SavedMoves(svrfmt::ArchiveData(c));
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
  // The numbers kept for next time.
  {
    std::string text;
    for (const auto &[key, slot] : kept)
      if (std::none_of(stars.begin(), stars.end(), [&](const auto &s) { return s.key == key; }))
        text += key + "=" + std::to_string(slot) + "\n";  // (a mod switched off keeps its number)
    for (const auto &st : stars)
      text += st.key + "=" + std::to_string(st.slot) + "\n";
    WriteHost(user_root / "superstar_slots.txt", Bytes(text.begin(), text.end()));
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
  if (ReadHost(stamp_file, old) && std::string(old.begin(), old.end()) == stamp &&
      fs::exists(headers / (std::string(kPackName) + ".header"), ec)) {
    REXLOG_INFO("[superstar mods] {} added superstar(s), pack {} unchanged", stars.size(), kPackName);
    return;
  }
  fs::remove_all(content / kPackName, ec);
  for (const auto &f : files)
    if (!WriteHost(content / kPackName / fs::path(f.path), f.data)) {
      REXLOG_WARN("[superstar mods] could not write {}", f.path);
      return;
    }
  WriteHost(stamp_file, Bytes(stamp, stamp + 16));
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

extern "C" REX_FUNC(sub_82A316A0) {
  if (!g_movies.empty()) {
    const uint32_t id = Entering(ctx, base);
    const auto it = g_movies.find(id);
    REXLOG_INFO("[superstar mods] entrance video {} (superstar entering: {}){}", ctx.r3.u32, id,
                it != g_movies.end() ? " -> its own, " + std::to_string(it->second) : std::string());
    if (it != g_movies.end())
      ctx.r3.u64 = it->second;
  }
  __imp__sub_82A316A0(ctx, base);
}

// The select screen's candidates (sub_828ADFB8: numbers 50..372 that the save says are unlocked). The save keeps its
// own copy of every superstar's record (the roster: [0x8317FA08] + index * 286, index = u16 table 0x8307B5E0[number];
// sub_82684BF8 / sub_82684910 read it), made when it first saw the superstar: its name, ratings and "unlocked" (byte
// 247) are shown from there. A number another mod had before kept that one's copy (FBX TEST at 86, where an old
// test's superstar was: hidden as locked, then shown as "DLC 37" with OVERALL 0 and a black screen at the match,
// 2026-10-26, wwe12_roster_probe): an added superstar's copy is made again from its record when it differs, unlocked.
REXCVAR_DEFINE_BOOL(wwe12_superstar_roster_fix, true, "WWE12",
                    "Added superstars: the save's copy of their record made again when it differs (unlocked).");
REX_EXTERN(__imp__sub_828ADFB8);
extern "C" REX_FUNC(sub_828ADFB8) {
  const uint32_t roster = REXCVAR_GET(wwe12_superstar_roster_fix) ? bd::mem::load<uint32_t>(0x8317FA08) : 0;
  for (const auto &[id, rec] : g_records) {
    const uint32_t index = id < 373 ? bd::mem::load<uint16_t>(0x8307B5E0 + id * 2) : 0xFFFF;
    if (!roster || index >= 373)
      continue;
    const uint32_t at = roster + index * 286;
    bool same = true;
    for (uint32_t k = 0; k < 286 && same; ++k)
      same = k == 247 || bd::mem::load<uint8_t>(at + k) == rec[k];
    if (same && bd::mem::load<uint8_t>(at + 247))
      continue;
    for (uint32_t k = 0; k < 286; ++k)
      if (k != 247)
        bd::mem::store<uint8_t>(at + k, rec[k]);
    bd::mem::store<uint8_t>(at + 247, 1);
    REXLOG_INFO("[superstar mods] superstar {}: the save's copy of its record made again{}", id,
                same ? " (it was locked)" : "");
  }
  // Its moves and entrance too: the save keeps them after the records (SaveData.Dat: the records at 0x1C, the moves at
  // 324792, 1068 bytes each, same order). A disc's empty record's were empty: no entrance (FBX TEST at 108, 2026-10-26).
  // Only once that place holds John Cena's as they should be.
  const uint32_t moves = roster ? roster - 0x1C + 324792 : 0;
  const auto index_of = [](uint32_t id) { return id < 373 ? uint32_t(bd::mem::load<uint16_t>(0x8307B5E0 + id * 2)) : 0xFFFFu; };
  bool found = moves && g_cena_moves.size() == 1068 && index_of(139) < 373;
  for (uint32_t k = 0; found && k < 1068; ++k)
    found = bd::mem::load<uint8_t>(moves + index_of(139) * 1068 + k) == g_cena_moves[k];
  if (moves && !found && !g_moves.empty()) {
    static bool told = false;
    if (!told)
      REXLOG_WARN("[superstar mods] the save's moves not found where expected: added superstars' left as they are");
    told = true;
  }
  for (const auto &[id, mv] : g_moves) {
    if (!found || index_of(id) >= 373)
      continue;
    const uint32_t at = moves + index_of(id) * 1068;
    bool same = true;
    for (uint32_t k = 0; k < 1068 && same; ++k)
      same = bd::mem::load<uint8_t>(at + k) == mv[k];
    if (same)
      continue;
    for (uint32_t k = 0; k < 1068; ++k)
      bd::mem::store<uint8_t>(at + k, mv[k]);
    REXLOG_INFO("[superstar mods] superstar {}: the save's copy of its moves and entrance made again", id);
  }
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
