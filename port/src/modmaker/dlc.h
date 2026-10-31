// Mod Maker: the player's DLC of the other WWE games, from one folder (My game discs): every content package under it
// (stfs.h: "LIVE" / "CON " files, as the console keeps them in <title id>\00000002\) and every package already
// extracted (a folder holding info\catalog.dlc, as an emulator installs them: ...\<title id>\00000002\<package>\).
// Each game's packages are added over its disc (disc::GameDisc layers): its pages then list and read the DLC's
// superstars, arenas, names, portraits and movies as the disc's. A DLC names its files after itself (WWE '13
// "pac/ch/ch65_dlc02.pac", 2K16 "pac/ch/ch124.pac", "pac/ch/chEtc_dlc20.pac"): the pages read those names.
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include "game_disc.h"
#include "stfs.h"

namespace dlc {

namespace fs = std::filesystem;

inline std::string Lower(std::string s) {
  for (auto &c : s)
    c = c == '\\' ? '/' : char(tolower(static_cast<unsigned char>(c)));
  return s;
}

// One game's packages, as files over its disc.
class Game : public disc::GameDisc::Layer {
 public:
  struct Pack {
    std::string name;  // its title ("WWE 2K16 Legends Pack"), else its file name
    fs::path path;
    std::unique_ptr<stfs::Package> stfs;  // (null: an extracted folder)
    int superstars = 0, arenas = 0;
  };

  void AddPackage(const fs::path &p, std::unique_ptr<stfs::Package> pkg) {
    Pack k;
    k.path = p;
    k.name = DisplayName(p);
    k.stfs = std::move(pkg);
    for (const auto &f : k.stfs->Files())
      Index(f.path, packs_.size(), k);
    packs_.push_back(std::move(k));
  }

  void AddFolder(const fs::path &dir) {
    Pack k;
    k.path = dir;
    k.name = dir.filename().string();
    std::error_code ec;
    for (fs::recursive_directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
      if (it->is_regular_file(ec)) {
        const auto rel = it->path().lexically_relative(dir).generic_u8string();
        Index(std::string(rel.begin(), rel.end()), packs_.size(), k);
      }
    packs_.push_back(std::move(k));
  }

  const std::vector<Pack> &Packs() const { return packs_; }
  int Superstars() const {
    int n = 0;
    for (const auto &k : packs_)
      n += k.superstars;
    return n;
  }
  int Arenas() const {
    int n = 0;
    for (const auto &k : packs_)
      n += k.arenas;
    return n;
  }

  bool Read(const std::string &path, std::vector<uint8_t> &out) const override {
    const auto it = files_.find(Lower(path));
    return it != files_.end() && ReadAt(it->second.front(), out);
  }

  void ReadAll(const std::string &path, std::vector<std::vector<uint8_t>> &out) const override {
    const auto it = files_.find(Lower(path));
    if (it == files_.end())
      return;
    // (the smaller first: a later roster's copy holds more, SvR 2011 Vol. 3's string.pac names Vol. 2's arena too)
    std::vector<std::vector<uint8_t>> mine;
    for (const auto &at : it->second) {
      std::vector<uint8_t> b;
      if (ReadAt(at, b))
        mine.push_back(std::move(b));
    }
    std::stable_sort(mine.begin(), mine.end(), [](const auto &x, const auto &y) { return x.size() < y.size(); });
    for (auto &b : mine)
      out.push_back(std::move(b));
  }

  std::vector<std::string> List(const std::string &dir, bool dirs) const override {
    std::string d = Lower(dir);
    while (!d.empty() && d.back() == '/')
      d.pop_back();
    if (!d.empty())
      d += '/';
    std::vector<std::string> names;
    for (const auto &[low, ats] : files_) {
      const At &at = ats.front();
      if (low.compare(0, d.size(), d) != 0)
        continue;
      const size_t slash = low.find('/', d.size());
      if ((slash == std::string::npos) == dirs)
        continue;
      // (the name as the package writes it)
      const std::string name = at.path.substr(d.size(), slash == std::string::npos ? std::string::npos : slash - d.size());
      if (std::none_of(names.begin(), names.end(), [&](const std::string &x) { return Lower(x) == Lower(name); }))
        names.push_back(name);
    }
    return names;
  }

 private:
  struct At {
    size_t pack;
    std::string path;  // in that package
  };

  bool ReadAt(const At &at, std::vector<uint8_t> &out) const {
    const Pack &k = packs_[at.pack];
    if (k.stfs)
      return k.stfs->Read(at.path, out);
    std::ifstream f(k.path / fs::path(std::u8string(at.path.begin(), at.path.end())), std::ios::binary);
    if (!f)
      return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
  }

  void Index(const std::string &path, size_t pack, Pack &k) {
    const std::string low = Lower(path);
    files_[low].push_back(At{pack, path});  // (Read: the first package's; ReadAll: every one's)
    if (low.rfind("pac/ch/ch", 0) == 0 && low.size() > 9 && isdigit(static_cast<unsigned char>(low[9])))
      ++k.superstars;
    if (low.rfind("pac/bg/bg", 0) == 0 && low.size() > 9 && isdigit(static_cast<unsigned char>(low[9])))
      ++k.arenas;
  }

  // A package's display name (its header's first title, UTF-16 big-endian at 0x411).
  static std::string DisplayName(const fs::path &p) {
    std::ifstream f(p, std::ios::binary);
    unsigned char b[0x80] = {};
    f.seekg(0x411);
    f.read(reinterpret_cast<char *>(b), sizeof(b));
    std::string s;
    for (size_t i = 0; i + 1 < sizeof(b); i += 2) {
      const unsigned c = unsigned(b[i]) << 8 | b[i + 1];
      if (!c)
        break;
      if (c < 0x80)
        s += char(c);
      else if (c < 0x800)
        s += {char(0xC0 | (c >> 6)), char(0x80 | (c & 0x3F))};
      else
        s += {char(0xE0 | (c >> 12)), char(0x80 | ((c >> 6) & 0x3F)), char(0x80 | (c & 0x3F))};
    }
    return s.empty() ? p.filename().string() : s;
  }

  std::vector<Pack> packs_;
  std::map<std::string, std::vector<At>> files_;  // lower-case path -> where
};

// Every game's packages under the DLC folder.
class Library {
 public:
  void Scan(const fs::path &root) {
    games_.clear();
    root_ = root;
    std::error_code ec;
    if (root.empty() || !fs::is_directory(root, ec))
      return;
    std::vector<fs::path> packages, folders;
    for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         !ec && it != end; it.increment(ec)) {
      if (it.depth() > 7) {
        it.disable_recursion_pending();
        continue;
      }
      if (it->is_directory(ec) && fs::exists(it->path() / "info" / "catalog.dlc", ec)) {
        folders.push_back(it->path());
        it.disable_recursion_pending();
      } else if (it->is_regular_file(ec) && it->file_size(ec) >= 0xB000) {
        packages.push_back(it->path());
      }
    }
    std::sort(packages.begin(), packages.end());
    std::sort(folders.begin(), folders.end());
    for (const auto &p : packages) {
      auto pkg = std::make_unique<stfs::Package>();
      if (!pkg->Open(p) || pkg->ContentType() != 2 || pkg->Files().empty())
        continue;
      const uint32_t title = pkg->Title();
      Of(title).AddPackage(p, std::move(pkg));
    }
    // (an extracted package: ...\<title id>\00000002\<package>\info\catalog.dlc)
    for (const auto &d : folders) {
      const std::string t = d.parent_path().parent_path().filename().string();
      if (t.size() != 8 || !std::all_of(t.begin(), t.end(), [](char c) { return isxdigit(static_cast<unsigned char>(c)); }))
        continue;
      Of(uint32_t(strtoul(t.c_str(), nullptr, 16))).AddFolder(d);
    }
  }

  const fs::path &Root() const { return root_; }
  // A game's packages (null: none).
  std::shared_ptr<const Game> For(uint32_t title) const {
    const auto it = games_.find(title);
    return it == games_.end() ? nullptr : it->second;
  }

 private:
  Game &Of(uint32_t title) {
    auto &g = games_[title];
    if (!g)
      g = std::make_shared<Game>();
    return *g;
  }

  fs::path root_;
  std::map<uint32_t, std::shared_ptr<Game>> games_;
};

}  // namespace dlc
