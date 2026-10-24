// WWE '12: added superstars (docs/mod_maker.md, "Ajouter des catcheurs"). At startup, after the DLC packs are
// installed and before the game lists them, the superstar mods (mods\.svrmods\Superstars\<id>, type=superstar,
// not disabled: manifest name=, base=NNN, superstar\ch.pac optional) become one more DLC pack, WWE12PCMODS, made by
// modmaker/superstar_pack.h from the player's own files: the latest installed pack's (else the disc's) chEtc.pac,
// DLC.pac and portraits, the base superstars' models when a mod has none. Each mod keeps its number from one start
// to the next (Documents\wwe12\superstar_slots.txt: "<mod>=<number>"), so a save's records never move. The pack is
// written again only when its contents change (then the game's cache, made from the packs present, is emptied as
// for a new DLC: wwe12_dlc.cpp); without superstar mods it is removed.

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <span>
#include <string>
#include <vector>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/filesystem/entry.h>
#include <rex/filesystem/file.h>
#include <rex/filesystem/vfs.h>

#include "core/logging.h"

#include "../modmaker/superstar_pack.h"

REXCVAR_DEFINE_BOOL(wwe12_superstar_mods, true, "WWE12",
                    "Added superstars (superstar mods) as a DLC pack made at startup.");

namespace {

namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;

constexpr const char *kPackName = "WWE12PCMODS";

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
    fs::path model;
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
    mod.model = d / "superstar" / "ch.pac";
    if (mod.base < 50 || mod.base >= 373) {
      REXLOG_WARN("[superstar mods] {}: base={} is not a superstar of the game", mod.key, Value(text, "base"));
      continue;
    }
    list.push_back(mod);
  }
  if (list.empty()) {
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
  stars.erase(std::remove_if(stars.begin(), stars.end(), [](const auto &s) { return !s.slot || s.model.empty(); }),
              stars.end());
  if (stars.empty()) {
    RemovePack(content, headers);
    return;
  }
  std::vector<superstar_pack::File> files;
  std::string why;
  if (!superstar_pack::Build(in, stars, files, &why)) {
    REXLOG_WARN("[superstar mods] pack not made: {}", why);
    return;
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
  // Written again only when it changed.
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
    REXLOG_INFO("[superstar mods] {} \"{}\" = superstar {} (base {})", st.key, st.name, st.slot, st.base);
  REXLOG_INFO("[superstar mods] pack {} written ({} files), game cache emptied", kPackName, files.size());
}
