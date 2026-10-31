// Xbox 360 content packages ("CON ", "LIVE", "PIRS": a game's DLC as the console keeps it, a file named by its hash in
// <profile>\<title id>\00000002\), read only: their files listed and read. The layout follows the SDK's reader
// (rexglue-sdk filesystem/devices/stfs_container_device.cpp, from Xenia, BSD): 4 KB blocks after the header, a hash
// table (or two, the active one flagged) before every 170 blocks, and above every 170 tables; the file table and the
// files' blocks chained through the hash entries. SVOD packages (games on demand) aren't read.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace stfs {

namespace fs = std::filesystem;

class Package {
 public:
  struct File {
    std::string path;  // "pac/ch/ch65_dlc02.pac" ('/' between folders)
    uint32_t start = 0, blocks = 0, size = 0;
    bool contiguous = false, dir = false;
  };

  ~Package() { Close(); }

  // False: not a content package (or an SVOD one).
  bool Open(const fs::path &p) {
    Close();
    f_ = _wfopen(p.c_str(), L"rb");
    if (!f_)
      return false;
    uint8_t h[0x3B0];
    if (fread(h, 1, sizeof(h), f_) != sizeof(h) ||
        (memcmp(h, "CON ", 4) && memcmp(h, "LIVE", 4) && memcmp(h, "PIRS", 4)) || Be32(h + 0x3A9) != 0) {
      Close();
      return false;
    }
    title_ = Be32(h + 0x360);
    content_type_ = Be32(h + 0x344);
    base_ = (uint64_t(Be32(h + 0x340)) + 0xFFF) & ~uint64_t(0xFFF);
    const uint8_t *vd = h + 0x379;
    flags_ = vd[2];
    per_table_ = (flags_ & 1) ? 1 : 2;  // (read-only format: one table per level, else two)
    step_[0] = 170 + per_table_;
    step_[1] = 28900 + 171 * per_table_;
    total_blocks_ = Be32(vd + 0x1C);
    const uint32_t table_count = uint32_t(vd[3]) | uint32_t(vd[4]) << 8;
    uint32_t block = Le24(vd + 5);
    std::vector<File> all;
    for (uint32_t n = 0; n < table_count && block != 0xFFFFFF; ++n) {
      uint8_t t[4096];
      if (!ReadBlock(block, t))
        break;
      for (int e = 0; e < 64; ++e) {
        const uint8_t *d = t + e * 0x40;
        if (!d[0])
          break;
        File x;
        std::string name(reinterpret_cast<const char *>(d), d[0x28] & 0x3F);
        x.contiguous = (d[0x28] & 0x40) != 0;
        x.dir = (d[0x28] & 0x80) != 0;
        x.blocks = Le24(d + 0x2C);
        x.start = Le24(d + 0x2F);
        x.size = Be32(d + 0x34);
        const uint16_t parent = uint16_t(d[0x32] << 8 | d[0x33]);
        x.path = parent != 0xFFFF && parent < all.size() ? all[parent].path + "/" + name : name;
        all.push_back(std::move(x));
      }
      block = Next(block);
    }
    for (auto &x : all)
      if (!x.dir)
        files_.push_back(x);
    return true;
  }

  void Close() {
    if (f_)
      fclose(f_);
    f_ = nullptr;
    files_.clear();
    tables_.clear();
  }

  uint32_t Title() const { return title_; }
  uint32_t ContentType() const { return content_type_; }  // (2: DLC)
  const std::vector<File> &Files() const { return files_; }

  // The file at that path (any case), whole.
  bool Read(const std::string &path, std::vector<uint8_t> &out) const {
    for (const auto &x : files_)
      if (Same(x.path, path))
        return Read(x, out);
    return false;
  }

  bool Read(const File &x, std::vector<uint8_t> &out) const {
    std::lock_guard<std::mutex> lock(mutex_);  // (one file handle, the ports' threads)
    out.resize(x.size);
    uint32_t block = x.start;
    for (uint32_t at = 0; at < x.size; at += 4096) {
      if (block == 0xFFFFFF)
        return false;
      const uint32_t n = std::min<uint32_t>(4096, x.size - at);
      if (_fseeki64(f_, int64_t(Offset(block)), SEEK_SET) || fread(out.data() + at, 1, n, f_) != n)
        return false;
      block = x.contiguous ? block + 1 : Next(block);
    }
    return true;
  }

 private:
  static uint32_t Be32(const uint8_t *p) { return uint32_t(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
  static uint32_t Le24(const uint8_t *p) { return uint32_t(p[0]) | p[1] << 8 | p[2] << 16; }
  static bool Same(const std::string &a, const std::string &b) {
    if (a.size() != b.size())
      return false;
    for (size_t i = 0; i < a.size(); ++i) {
      char x = char(tolower((unsigned char)a[i])), y = char(tolower((unsigned char)b[i]));
      if (x == '\\')
        x = '/';
      if (y == '\\')
        y = '/';
      if (x != y)
        return false;
    }
    return true;
  }

  // A data block's place in the file: the hash tables before it skipped.
  uint64_t Offset(uint64_t index) const {
    uint64_t base = 170, block = index;
    for (int i = 0; i < 3; ++i) {
      block += ((index + base) / base) * per_table_;
      if (index < base)
        break;
      base *= 170;
    }
    return base_ + (block << 12);
  }

  uint64_t TableOffset(uint32_t index, int level) const {
    uint64_t block = 0;
    if (level == 0) {
      if (index >= 170) {
        block = uint64_t(index / 170) * step_[0] + uint64_t(index / 28900 + 1) * per_table_;
        if (index >= 28900)
          block += per_table_;
      }
    } else if (level == 1) {
      block = index < 28900 ? step_[0] : uint64_t(index / 28900) * step_[1] + per_table_;
    } else {
      block = step_[1];
    }
    return base_ + (block << 12);
  }

  const std::vector<uint8_t> &Table(uint64_t at) const {
    auto it = tables_.find(at);
    if (it != tables_.end())
      return it->second;
    std::vector<uint8_t> t(4096);
    if (_fseeki64(f_, int64_t(at), SEEK_SET) || fread(t.data(), 1, 4096, f_) != 4096)
      t.assign(4096, 0xFF);
    return tables_[at] = std::move(t);
  }

  // The block after this one in its chain (level 0 hash entry), the active table of each level taken.
  uint32_t Next(uint32_t index) const {
    uint64_t second = (flags_ & 2) ? 4096 : 0;
    if (flags_ & 1) {
      second = 0;
    } else if (total_blocks_ > 170) {
      if (total_blocks_ > 28900) {
        const auto &t2 = Table(TableOffset(index, 2) + second);
        const uint8_t *e = t2.data() + ((index / 28900) % 170) * 0x18;
        second = (e[0x14] & 0x40) ? 4096 : 0;
      }
      const auto &t1 = Table(TableOffset(index, 1) + second);
      const uint8_t *e = t1.data() + ((index / 170) % 170) * 0x18;
      second = (e[0x14] & 0x40) ? 4096 : 0;
    }
    const auto &t0 = Table(TableOffset(index, 0) + second);
    const uint8_t *e = t0.data() + (index % 170) * 0x18;
    return uint32_t(e[0x15]) << 16 | uint32_t(e[0x16]) << 8 | e[0x17];
  }

  bool ReadBlock(uint32_t index, uint8_t *out) const {
    return !_fseeki64(f_, int64_t(Offset(index)), SEEK_SET) && fread(out, 1, 4096, f_) == 4096;
  }

  FILE *f_ = nullptr;
  uint32_t title_ = 0, content_type_ = 0, flags_ = 0, per_table_ = 2, total_blocks_ = 0;
  uint64_t base_ = 0, step_[2] = {};
  std::vector<File> files_;
  mutable std::map<uint64_t, std::vector<uint8_t>> tables_;
  mutable std::mutex mutex_;
};

}  // namespace stfs
