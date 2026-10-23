// The player's game files, read by the launcher: from the extracted folders (assets, game, or the project
// assets in a build folder) or from the disc image (.iso in game\ or next to the exe). Read only.
// The ISO is the Xbox 360 XDVDFS: the game partition at 0 (extracted), 0x2080000 (XGD3), 0xFD90000 (XGD2)
// or 0x18300000 (XGD1); its volume descriptor 32 sectors in ("MICROSOFT*XBOX*MEDIA", root sector, root size);
// directory entries {left u16, right u16, sector u32, size u32, attributes u8, name length u8, name}, 4-byte
// aligned, 0xFF padding to the end of a sector.
// Also the Yuke's BPE blocks of the .pac files (tools/yukes_bpe.py) and the superstars' names (chEtc.pac).
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace disc {

namespace fs = std::filesystem;

class GameDisc {
 public:
  // Finds the game files next to the launcher (same places as the game looks).
  void Open(const fs::path &exe_dir) {
    folder_.clear();
    iso_.clear();
    std::error_code ec;
    for (const auto &d : {exe_dir / "assets", exe_dir / "game", exe_dir / ".." / ".." / ".." / "assets"})
      if (fs::exists(d / "default.xex", ec)) {
        folder_ = d;
        return;
      }
    for (const auto &d : {exe_dir / "game", exe_dir})
      for (fs::directory_iterator it(d, ec), end; !ec && it != end; it.increment(ec)) {
        std::wstring ext = it->path().extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
        if (ext == L".iso" && OpenIso(it->path()))
          return;
      }
  }

  bool Found() const { return !folder_.empty() || !iso_.empty(); }

  // A file by its disc path ("pac/ch/chEtc.pac").
  bool Read(const std::string &path, std::vector<uint8_t> &out) const {
    out.clear();
    if (!folder_.empty()) {
      std::ifstream f(folder_ / path, std::ios::binary);
      if (!f)
        return false;
      out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
      return true;
    }
    Item it;
    if (iso_.empty() || !Find(path, it) || it.dir)
      return false;
    return ReadAt(uint64_t(it.sector) * 2048, it.size, out);
  }

  // The file names in a disc folder ("movies/titantron").
  std::vector<std::string> List(const std::string &dir) const {
    std::vector<std::string> names;
    if (!folder_.empty()) {
      std::error_code ec;
      for (fs::directory_iterator it(folder_ / dir, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_regular_file(ec))
          names.push_back(it->path().filename().string());
      return names;
    }
    Item d;
    if (iso_.empty() || !Find(dir, d) || !d.dir)
      return names;
    for (const auto &e : ReadDir(d))
      if (!e.dir)
        names.push_back(e.name);
    return names;
  }

  // The sub folders of a disc folder ("" = the root).
  std::vector<std::string> ListDirs(const std::string &dir) const {
    std::vector<std::string> names;
    if (!folder_.empty()) {
      std::error_code ec;
      for (fs::directory_iterator it(folder_ / dir, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_directory(ec))
          names.push_back(it->path().filename().string());
      return names;
    }
    Item d;
    if (iso_.empty() || !Find(dir, d) || !d.dir)
      return names;
    for (const auto &e : ReadDir(d))
      if (e.dir)
        names.push_back(e.name);
    return names;
  }

 private:
  struct Item {
    std::string name;
    uint32_t sector = 0, size = 0;
    bool dir = false;
  };

  bool ReadAt(uint64_t at, uint32_t size, std::vector<uint8_t> &out) const {
    std::ifstream f(iso_, std::ios::binary);
    f.seekg(std::streamoff(partition_ + at));
    out.resize(size);
    f.read(reinterpret_cast<char *>(out.data()), size);
    return bool(f);
  }

  bool OpenIso(const fs::path &iso) {
    std::ifstream f(iso, std::ios::binary);
    for (uint64_t part : {0ull, 0x2080000ull, 0xFD90000ull, 0x18300000ull}) {
      char vd[28] = {};
      f.clear();
      f.seekg(std::streamoff(part + 32 * 2048));
      if (f.read(vd, sizeof(vd)) && memcmp(vd, "MICROSOFT*XBOX*MEDIA", 20) == 0) {
        iso_ = iso;
        partition_ = part;
        memcpy(&root_.sector, vd + 20, 4);
        memcpy(&root_.size, vd + 24, 4);
        root_.dir = true;
        return true;
      }
    }
    return false;
  }

  std::vector<Item> ReadDir(const Item &d) const {
    std::vector<Item> items;
    std::vector<uint8_t> b;
    if (d.size > (64u << 20) || !ReadAt(uint64_t(d.sector) * 2048, d.size, b))
      return items;
    size_t o = 0;
    while (o + 14 <= b.size()) {
      if (b[o] == 0xFF && b[o + 1] == 0xFF) {  // padding: the rest of this sector is empty
        o = (o / 2048 + 1) * 2048;
        continue;
      }
      const uint8_t nlen = b[o + 13];
      if (nlen == 0 || o + 14 + nlen > b.size())
        break;
      Item it;
      memcpy(&it.sector, &b[o + 4], 4);
      memcpy(&it.size, &b[o + 8], 4);
      it.dir = (b[o + 12] & 0x10) != 0;
      it.name.assign(reinterpret_cast<const char *>(&b[o + 14]), nlen);
      items.push_back(it);
      o += (14 + nlen + 3) & ~size_t(3);
    }
    return items;
  }

  static bool SameName(const std::string &a, const std::string &b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), [](char x, char y) {
             return std::tolower(static_cast<unsigned char>(x)) == std::tolower(static_cast<unsigned char>(y));
           });
  }

  bool Find(const std::string &path, Item &out) const {
    Item cur = root_;
    size_t p = 0;
    while (p < path.size()) {
      size_t q = path.find_first_of("/\\", p);
      if (q == std::string::npos)
        q = path.size();
      const std::string part = path.substr(p, q - p);
      p = q + 1;
      if (part.empty())
        continue;
      if (!cur.dir)
        return false;
      bool found = false;
      for (const auto &e : ReadDir(cur))
        if (SameName(e.name, part)) {
          cur = e;
          found = true;
          break;
        }
      if (!found)
        return false;
    }
    out = cur;
    return true;
  }

  fs::path folder_, iso_;
  uint64_t partition_ = 0;
  Item root_;
};

// ---- Yuke's BPE ("BPE ", version 0x100, packed size, unpacked size, then Philip Gage's byte pair blocks;
// WWE '12 writes the blocks' sizes little-endian, older games big-endian).
inline bool BpeExpand(const uint8_t *src, size_t n, size_t expected, bool big_endian, std::vector<uint8_t> &out) {
  out.clear();
  uint8_t left[256], right[256] = {}, stack[1024];
  size_t pos = 0;
  while (pos < n && out.size() < expected) {
    for (int c = 0; c < 256; ++c)
      left[c] = uint8_t(c);
    for (int c = 0;;) {
      if (pos >= n)
        return false;
      int count = src[pos++];
      if (count > 127) {
        c += count - 127;
        count = 0;
      }
      if (c == 256)
        break;
      for (int k = 0; k <= count; ++k) {
        if (pos >= n || c > 255)
          return false;
        left[c] = src[pos++];
        if (left[c] != c) {
          if (pos >= n)
            return false;
          right[c] = src[pos++];
        }
        ++c;
      }
      if (c == 256)
        break;
    }
    if (pos + 2 > n)
      return false;
    const size_t size = big_endian ? (src[pos] << 8) | src[pos + 1] : src[pos] | (src[pos + 1] << 8);
    pos += 2;
    for (size_t i = 0; i < size; ++i) {
      if (pos >= n)
        return false;
      int sp = 0;
      stack[sp++] = src[pos++];
      while (sp) {
        const uint8_t b = stack[--sp];
        if (left[b] == b) {
          out.push_back(b);
          if (out.size() > expected)
            return false;
        } else {
          if (sp > 1020)
            return false;  // a cyclic table: not BPE after all
          stack[sp++] = right[b];
          stack[sp++] = left[b];
        }
      }
    }
  }
  return out.size() == expected;
}

// An entry as stored: BPE blocks are unpacked, anything else is returned as it is.
inline std::vector<uint8_t> Unpack(const uint8_t *p, size_t n) {
  if (n >= 16 && memcmp(p, "BPE ", 4) == 0) {
    uint32_t packed, unpacked;
    memcpy(&packed, p + 8, 4);
    memcpy(&unpacked, p + 12, 4);
    std::vector<uint8_t> out;
    if (16 + size_t(packed) <= n && unpacked < (64u << 20))
      for (bool be : {false, true})
        if (BpeExpand(p + 16, packed, unpacked, be, out))
          return out;
    return {};
  }
  return std::vector<uint8_t>(p, p + n);
}

// The superstars' names by id, from pac/ch/chEtc.pac (EPAC, PACH at 0x4000 = the CHAR/DAT records:
// 290 bytes, the id (u16) at +44 and the name at +46). The id is also the number of the superstar's
// entrance movie, movies/titantron/<id>.bik.
inline std::map<int, std::string> SuperstarNames(const GameDisc &d) {
  std::map<int, std::string> names;
  std::vector<uint8_t> pac;
  if (!d.Read("pac/ch/chEtc.pac", pac) || pac.size() < 0x4008 || memcmp(&pac[0x4000], "PACH", 4) != 0)
    return names;
  uint32_t count;
  memcpy(&count, &pac[0x4004], 4);
  const size_t base = 0x4008 + size_t(count) * 12;
  for (uint32_t i = 0; i < count && base <= pac.size(); ++i) {
    uint32_t e[3];
    memcpy(e, &pac[0x4008 + i * 12], 12);
    if (base + e[1] + e[2] > pac.size())
      break;
    const std::vector<uint8_t> r = Unpack(&pac[base + e[1]], e[2]);
    if (r.size() < 80)
      continue;
    const int id = r[44] | (r[45] << 8);
    std::string name;  // Latin-1 in the game, UTF-8 for ImGui
    for (size_t k = 46; k < 80 && r[k]; ++k) {
      if (r[k] < 0x80) {
        name += char(r[k]);
      } else {
        name += char(0xC0 | (r[k] >> 6));
        name += char(0x80 | (r[k] & 0x3F));
      }
    }
    if (!name.empty())
      names[id] = name;
  }
  return names;
}

}  // namespace disc
