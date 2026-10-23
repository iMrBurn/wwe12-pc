// WWE '12 texture dumps and texture packs (see wwe12_texture_packs.h). Adapted from Kaiko's SvR 2011
// port (texture_packs.cpp): the PNG writer (fixed-Huffman deflate), the hash-from-file-name rule and the
// PNG/DDS readers are his, credited in docs/mod_maker.md.
#include "wwe12_texture_packs.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#define XXH_STATIC_LINKING_ONLY  // XXH3_state_t (streaming hash of the rows)
#include <xxhash.h>

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

#include "core/logging.h"

REXCVAR_DEFINE_BOOL(wwe12_texture_dump, false, "WWE12",
                    "For texture pack makers: every texture shown is written once to texture_dumps (beside "
                    "the exe) as a PNG named after its content hash, which packs match on");
REXCVAR_DEFINE_BOOL(wwe12_texture_packs, true, "WWE12",
                    "Texture packs: a .png or .dds under the textures folder (beside the exe) whose name ends "
                    "with _<hash> replaces that texture");

namespace wwe12::texture_packs {

namespace fs = std::filesystem;

namespace {

struct DumpItem {
  uint64_t hash;
  uint32_t width, height, blocks_w, blocks_h, swizzle;
  Format format;
  std::vector<uint8_t> blocks;  // tightly packed
};

std::once_flag g_start_once;
std::atomic<bool> g_active{false}, g_dumping{false};
fs::path g_dump_dir;

std::mutex g_mutex;
std::condition_variable g_dump_cv;
std::unordered_map<uint64_t, fs::path> g_files;  // hash -> pack file
std::unordered_map<uint64_t, std::shared_ptr<const Replacement>> g_loaded;
std::unordered_set<uint64_t> g_failed, g_dumped;
std::deque<DumpItem> g_dump_queue;
size_t g_dump_queue_bytes = 0;

constexpr size_t kDumpQueueLimit = size_t(256) << 20;
constexpr uint32_t kMaxSide = 8192;

bool ReadAll(const fs::path &p, std::vector<uint8_t> &out) {
  std::ifstream f(p, std::ios::binary);
  if (!f)
    return false;
  f.seekg(0, std::ios::end);
  const std::streamoff n = f.tellg();
  if (n <= 0)
    return false;
  f.seekg(0, std::ios::beg);
  out.resize(size_t(n));
  return bool(f.read(reinterpret_cast<char *>(out.data()), n));
}

bool WriteAll(const fs::path &p, const std::vector<uint8_t> &data) {
  std::error_code ec;
  fs::create_directories(p.parent_path(), ec);
  std::ofstream f(p, std::ios::binary | std::ios::trunc);
  return f && f.write(reinterpret_cast<const char *>(data.data()), std::streamsize(data.size()));
}

uint32_t Le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24; }

std::string Hex16(uint64_t h) {
  char b[17];
  std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(h));
  return b;
}

// A file name's hash: <anything>_<16 hex digits> or just the 16 digits (Kaiko's rule).
bool HashOfStem(const std::string &stem, uint64_t *hash) {
  if (stem.size() < 16)
    return false;
  const size_t at = stem.size() - 16;
  if (at > 0 && stem[at - 1] != '_')
    return false;
  uint64_t h = 0;
  for (size_t i = at; i < stem.size(); ++i) {
    const char c = char(std::tolower(static_cast<unsigned char>(stem[i])));
    if (c >= '0' && c <= '9')
      h = h << 4 | uint64_t(c - '0');
    else if (c >= 'a' && c <= 'f')
      h = h << 4 | uint64_t(c - 'a' + 10);
    else
      return false;
  }
  *hash = h;
  return true;
}

std::string Lower(std::string s) {
  for (char &c : s)
    c = char(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

// ---------------------------------------------------------------------------------------------
// PNG writer (Kaiko's): zlib with fixed-Huffman deflate (LZ77, hash chains), Sub filter

struct Bits {
  std::vector<uint8_t> &out;
  uint32_t acc = 0, n = 0;
  void Put(uint32_t bits, uint32_t count) {
    acc |= bits << n;
    n += count;
    while (n >= 8) {
      out.push_back(uint8_t(acc));
      acc >>= 8;
      n -= 8;
    }
  }
  void Flush() {
    if (n)
      out.push_back(uint8_t(acc));
    acc = n = 0;
  }
};

uint32_t Reverse(uint32_t code, uint32_t len) {
  uint32_t r = 0;
  for (uint32_t i = 0; i < len; ++i, code >>= 1)
    r = r << 1 | (code & 1);
  return r;
}

void PutSymbol(Bits &w, uint32_t v) {  // literal / length symbol 0..287, fixed codes
  if (v <= 143)
    w.Put(Reverse(0x30 + v, 8), 8);
  else if (v <= 255)
    w.Put(Reverse(0x190 + v - 144, 9), 9);
  else if (v <= 279)
    w.Put(Reverse(v - 256, 7), 7);
  else
    w.Put(Reverse(0xC0 + v - 280, 8), 8);
}

constexpr uint16_t kLenBase[29] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                   31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr uint8_t kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                   2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr uint16_t kDistBase[30] = {1,    2,    3,    4,    5,    7,     9,     13,    17,  25,
                                    33,   49,   65,   97,   129,  193,   257,   385,   513, 769,
                                    1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr uint8_t kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6,
                                    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

void PutMatch(Bits &w, uint32_t len, uint32_t dist) {
  int i = 28;
  while (kLenBase[i] > len)
    --i;
  PutSymbol(w, 257 + i);
  w.Put(len - kLenBase[i], kLenExtra[i]);
  int d = 29;
  while (kDistBase[d] > dist)
    --d;
  w.Put(Reverse(uint32_t(d), 5), 5);
  w.Put(dist - kDistBase[d], kDistExtra[d]);
}

void Deflate(const uint8_t *data, size_t n, std::vector<uint8_t> &out) {
  Bits w{out};
  w.Put(1, 1);  // final block
  w.Put(1, 2);  // fixed Huffman
  constexpr uint32_t kWindow = 32768, kHashBits = 15, kChain = 48;
  std::vector<int32_t> head(size_t(1) << kHashBits, -1), prev(kWindow, -1);
  auto hash3 = [&](size_t i) {
    return ((uint32_t(data[i]) << 16 | uint32_t(data[i + 1]) << 8 | data[i + 2]) * 2654435761u) >>
           (32 - kHashBits);
  };
  auto insert = [&](size_t i) {
    if (i + 2 >= n)
      return;
    const uint32_t h = hash3(i);
    prev[i & (kWindow - 1)] = head[h];
    head[h] = int32_t(i);
  };
  size_t i = 0;
  while (i < n) {
    uint32_t best = 0, best_dist = 0;
    if (i + 2 < n) {
      int32_t cand = head[hash3(i)];
      const size_t max_len = std::min<size_t>(258, n - i);
      for (uint32_t chain = 0; cand >= 0 && chain < kChain; ++chain) {
        const size_t c = size_t(cand);
        if (c >= i || i - c > kWindow - 1)
          break;
        if (data[c + best] == data[i + best]) {
          size_t l = 0;
          while (l < max_len && data[c + l] == data[i + l])
            ++l;
          if (l > best) {
            best = uint32_t(l);
            best_dist = uint32_t(i - c);
            if (l == max_len)
              break;
          }
        }
        const int32_t next = prev[c & (kWindow - 1)];
        if (next >= cand)
          break;  // (a stale slot from beyond the window)
        cand = next;
      }
    }
    if (best >= 3) {
      PutMatch(w, best, best_dist);
      for (size_t k = 0; k < best; ++k)
        insert(i + k);
      i += best;
    } else {
      PutSymbol(w, data[i]);
      insert(i);
      ++i;
    }
  }
  PutSymbol(w, 256);
  w.Flush();
}

uint32_t Crc32(const uint8_t *p, size_t n) {
  static uint32_t table[256];
  static std::once_flag once;
  std::call_once(once, [] {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k)
        c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      table[i] = c;
    }
  });
  uint32_t crc = ~0u;
  for (size_t i = 0; i < n; ++i)
    crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
  return ~crc;
}

void PutBe32(std::vector<uint8_t> &o, uint32_t v) {
  o.push_back(uint8_t(v >> 24));
  o.push_back(uint8_t(v >> 16));
  o.push_back(uint8_t(v >> 8));
  o.push_back(uint8_t(v));
}

void Chunk(std::vector<uint8_t> &png, const char *type, const std::vector<uint8_t> &data) {
  PutBe32(png, uint32_t(data.size()));
  const size_t at = png.size();
  png.insert(png.end(), type, type + 4);
  png.insert(png.end(), data.begin(), data.end());
  PutBe32(png, Crc32(&png[at], png.size() - at));
}

std::vector<uint8_t> EncodePng(const uint8_t *rgba, uint32_t width, uint32_t height) {
  // Rows with the Sub filter, then zlib (deflate + Adler-32).
  std::vector<uint8_t> raw;
  raw.reserve(size_t(height) * (size_t(width) * 4 + 1));
  for (uint32_t y = 0; y < height; ++y) {
    raw.push_back(1);
    const uint8_t *row = rgba + size_t(y) * width * 4;
    for (uint32_t x = 0; x < width * 4; ++x)
      raw.push_back(uint8_t(row[x] - (x >= 4 ? row[x - 4] : 0)));
  }
  std::vector<uint8_t> z = {0x78, 0x01};
  Deflate(raw.data(), raw.size(), z);
  uint32_t a = 1, b = 0;
  for (uint8_t v : raw) {
    a = (a + v) % 65521;
    b = (b + a) % 65521;
  }
  PutBe32(z, b << 16 | a);
  std::vector<uint8_t> ihdr;
  PutBe32(ihdr, width);
  PutBe32(ihdr, height);
  ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});  // 8-bit RGBA
  std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  Chunk(png, "IHDR", ihdr);
  Chunk(png, "IDAT", z);
  Chunk(png, "IEND", {});
  return png;
}

// ---------------------------------------------------------------------------------------------
// BC1/2/3 -> RGBA for the dumps

void Rgb565(uint16_t c, uint8_t *o) {
  o[0] = uint8_t(((c >> 11) & 31) * 255 / 31);
  o[1] = uint8_t(((c >> 5) & 63) * 255 / 63);
  o[2] = uint8_t((c & 31) * 255 / 31);
  o[3] = 255;
}

// One 4x4 block into out (pitch in pixels).
void DecodeBlock(Format f, const uint8_t *b, uint8_t *out, uint32_t pitch) {
  const uint8_t *color = f == Format::kBC1 ? b : b + 8;
  const uint16_t c0 = uint16_t(color[0] | color[1] << 8), c1 = uint16_t(color[2] | color[3] << 8);
  uint8_t pal[4][4];
  Rgb565(c0, pal[0]);
  Rgb565(c1, pal[1]);
  const bool three = f == Format::kBC1 && c0 <= c1;
  for (int c = 0; c < 3; ++c) {
    if (three) {
      pal[2][c] = uint8_t((pal[0][c] + pal[1][c]) / 2);
      pal[3][c] = 0;
    } else {
      pal[2][c] = uint8_t((2 * pal[0][c] + pal[1][c]) / 3);
      pal[3][c] = uint8_t((pal[0][c] + 2 * pal[1][c]) / 3);
    }
  }
  pal[2][3] = 255;
  pal[3][3] = three ? 0 : 255;
  uint8_t alpha[16];
  if (f == Format::kBC2) {
    for (int i = 0; i < 16; ++i)
      alpha[i] = uint8_t(((b[i / 2] >> ((i & 1) * 4)) & 15) * 17);
  } else if (f == Format::kBC3) {
    uint8_t a[8] = {b[0], b[1]};
    for (int i = 2; i < 8; ++i)
      a[i] = a[0] > a[1] ? uint8_t(((8 - i) * a[0] + (i - 1) * a[1]) / 7)
             : i < 6     ? uint8_t(((6 - i) * a[0] + (i - 1) * a[1]) / 5)
                         : uint8_t(i == 6 ? 0 : 255);
    uint64_t bits = 0;
    for (int i = 0; i < 6; ++i)
      bits |= uint64_t(b[2 + i]) << (8 * i);
    for (int i = 0; i < 16; ++i)
      alpha[i] = a[(bits >> (3 * i)) & 7];
  }
  const uint32_t idx = Le32(color + 4);
  for (int i = 0; i < 16; ++i) {
    uint8_t *p = out + (size_t(i / 4) * pitch + (i % 4)) * 4;
    std::memcpy(p, pal[(idx >> (2 * i)) & 3], 4);
    if (f != Format::kBC1)
      p[3] = alpha[i];
  }
}

std::vector<uint8_t> DumpPixels(const DumpItem &it) {
  const uint32_t w = it.width, h = it.height;
  std::vector<uint8_t> rgba(size_t(w) * h * 4);
  if (it.format == Format::kRGBA8) {
    for (uint32_t y = 0; y < h; ++y)
      std::memcpy(&rgba[size_t(y) * w * 4], &it.blocks[size_t(y) * it.blocks_w * 4], size_t(w) * 4);
  } else {
    const uint32_t bs = it.format == Format::kBC1 ? 8 : 16, pw = it.blocks_w * 4;
    std::vector<uint8_t> full(size_t(pw) * it.blocks_h * 4 * 4);
    for (uint32_t by = 0; by < it.blocks_h; ++by)
      for (uint32_t bx = 0; bx < it.blocks_w; ++bx)
        DecodeBlock(it.format, &it.blocks[(size_t(by) * it.blocks_w + bx) * bs],
                    &full[(size_t(by) * 4 * pw + bx * 4) * 4], pw);
    for (uint32_t y = 0; y < h; ++y)
      std::memcpy(&rgba[size_t(y) * w * 4], &full[size_t(y) * pw * 4], size_t(w) * 4);
  }
  // The fetch constant's swizzle, 3 bits per result channel (0-3 = source R/G/B/A, 4 = 0, 5 = 1).
  if (it.swizzle != 0x688) {
    for (size_t i = 0; i < rgba.size(); i += 4) {
      uint8_t src[4];
      std::memcpy(src, &rgba[i], 4);
      for (int c = 0; c < 4; ++c) {
        const uint32_t s = (it.swizzle >> (3 * c)) & 7;
        rgba[i + c] = s < 4 ? src[s] : s == 4 ? 0 : 255;
      }
    }
  }
  return rgba;
}

const char *FormatTag(Format f) {
  switch (f) {
  case Format::kBC1: return "dxt1";
  case Format::kBC2: return "dxt3";
  case Format::kBC3: return "dxt5";
  default: return "rgba";
  }
}

void DumpThread() {
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
  for (;;) {
    DumpItem it;
    {
      std::unique_lock lock(g_mutex);
      g_dump_cv.wait(lock, [] { return !g_dump_queue.empty(); });
      it = std::move(g_dump_queue.front());
      g_dump_queue.pop_front();
      g_dump_queue_bytes -= it.blocks.size();
    }
    const std::vector<uint8_t> rgba = DumpPixels(it);
    const fs::path path = g_dump_dir / (std::to_string(it.width) + "x" + std::to_string(it.height) + "_" +
                                        FormatTag(it.format) + "_" + Hex16(it.hash) + ".png");
    if (!WriteAll(path, EncodePng(rgba.data(), it.width, it.height)))
      BD_WARN("Textures : écriture impossible de {}", path.string());
  }
}

// ---------------------------------------------------------------------------------------------
// replacement files (Kaiko's readers)

std::vector<uint8_t> Downsample(const std::vector<uint8_t> &src, uint32_t w, uint32_t h, uint32_t nw,
                                uint32_t nh) {
  std::vector<uint8_t> dst(size_t(nw) * nh * 4);
  for (uint32_t y = 0; y < nh; ++y) {
    for (uint32_t x = 0; x < nw; ++x) {
      const uint32_t x0 = std::min(2 * x, w - 1), x1 = std::min(2 * x + 1, w - 1);
      const uint32_t y0 = std::min(2 * y, h - 1), y1 = std::min(2 * y + 1, h - 1);
      for (int c = 0; c < 4; ++c) {
        const uint32_t s = src[(size_t(y0) * w + x0) * 4 + c] + src[(size_t(y0) * w + x1) * 4 + c] +
                           src[(size_t(y1) * w + x0) * 4 + c] + src[(size_t(y1) * w + x1) * 4 + c];
        dst[(size_t(y) * nw + x) * 4 + c] = uint8_t((s + 2) / 4);
      }
    }
  }
  return dst;
}

bool DecodePng(const std::vector<uint8_t> &file, Replacement &r) {
  int w = 0, h = 0, n = 0;
  stbi_uc *px = stbi_load_from_memory(file.data(), int(file.size()), &w, &h, &n, 4);
  if (!px)
    return false;
  if (w <= 0 || h <= 0 || uint32_t(w) > kMaxSide || uint32_t(h) > kMaxSide) {
    stbi_image_free(px);
    return false;
  }
  r.width = uint32_t(w);
  r.height = uint32_t(h);
  r.format = Format::kRGBA8;
  r.levels.emplace_back(px, px + size_t(w) * h * 4);
  stbi_image_free(px);
  uint32_t lw = r.width, lh = r.height;
  while (lw > 1 || lh > 1) {
    const uint32_t nw = std::max(lw / 2, 1u), nh = std::max(lh / 2, 1u);
    r.levels.push_back(Downsample(r.levels.back(), lw, lh, nw, nh));
    lw = nw;
    lh = nh;
  }
  return true;
}

uint32_t MaskShift(uint32_t mask) {
  uint32_t s = 0;
  while (mask && !(mask & 1))
    mask >>= 1, ++s;
  return s;
}

bool DecodeDds(const std::vector<uint8_t> &d, Replacement &r) {
  if (d.size() < 128 || std::memcmp(d.data(), "DDS ", 4))
    return false;
  const uint32_t h = Le32(&d[12]), w = Le32(&d[16]);
  if (!w || !h || w > kMaxSide || h > kMaxSide)
    return false;
  const uint32_t mips = (Le32(&d[8]) & 0x20000) ? std::max(1u, Le32(&d[28])) : 1u;
  const uint32_t pf_flags = Le32(&d[80]);
  size_t at = 128;
  r.width = w;
  r.height = h;
  if (pf_flags & 4) {  // FourCC
    uint32_t bs;
    if (!std::memcmp(&d[84], "DXT1", 4))
      r.format = Format::kBC1, bs = 8;
    else if (!std::memcmp(&d[84], "DXT3", 4))
      r.format = Format::kBC2, bs = 16;
    else if (!std::memcmp(&d[84], "DXT5", 4))
      r.format = Format::kBC3, bs = 16;
    else
      return false;
    for (uint32_t l = 0; l < mips; ++l) {
      const uint32_t lw = std::max(w >> l, 1u), lh = std::max(h >> l, 1u);
      const size_t n = size_t(std::max((lw + 3) / 4, 1u)) * std::max((lh + 3) / 4, 1u) * bs;
      if (at + n > d.size())
        break;
      r.levels.emplace_back(d.begin() + at, d.begin() + at + n);
      at += n;
    }
  } else if (Le32(&d[88]) == 32) {  // uncompressed 32-bit, by its channel masks
    const uint32_t masks[4] = {Le32(&d[92]), Le32(&d[96]), Le32(&d[100]), (pf_flags & 1) ? Le32(&d[104]) : 0};
    r.format = Format::kRGBA8;
    for (uint32_t l = 0; l < mips; ++l) {
      const uint32_t lw = std::max(w >> l, 1u), lh = std::max(h >> l, 1u);
      const size_t n = size_t(lw) * lh * 4;
      if (at + n > d.size())
        break;
      std::vector<uint8_t> px(n);
      for (size_t i = 0; i < size_t(lw) * lh; ++i) {
        const uint32_t v = Le32(&d[at + i * 4]);
        for (int c = 0; c < 4; ++c)
          px[i * 4 + c] = masks[c] ? uint8_t((v & masks[c]) >> MaskShift(masks[c])) : 255;
      }
      r.levels.push_back(std::move(px));
      at += n;
    }
  } else {
    return false;
  }
  return !r.levels.empty();
}

void Start() {
  const fs::path exe = rex::filesystem::GetExecutableFolder();
  if (REXCVAR_GET(wwe12_texture_packs)) {
    std::error_code ec;
    std::vector<fs::path> found;
    for (fs::recursive_directory_iterator it(exe / "textures", ec), end; !ec && it != end; it.increment(ec)) {
      if (!it->is_regular_file(ec))
        continue;
      const std::string ext = Lower(it->path().extension().string());
      if (ext == ".png" || ext == ".dds")
        found.push_back(it->path());
    }
    std::sort(found.begin(), found.end());  // the first one by path wins
    for (const auto &p : found) {
      uint64_t h = 0;
      if (HashOfStem(p.stem().string(), &h))
        g_files.emplace(h, p);
    }
    if (!g_files.empty())
      BD_INFO("Textures : {} remplacement(s) dans le dossier textures", g_files.size());
  }
  if (REXCVAR_GET(wwe12_texture_dump)) {
    g_dump_dir = exe / "texture_dumps";
    std::error_code ec;
    for (fs::directory_iterator it(g_dump_dir, ec), end; !ec && it != end; it.increment(ec)) {
      uint64_t h = 0;
      if (HashOfStem(it->path().stem().string(), &h))
        g_dumped.insert(h);  // dumped by an earlier run
    }
    g_dumping = true;
    std::thread(DumpThread).detach();
    BD_INFO("Textures : dump actif dans {} ({} déjà là)", g_dump_dir.string(), g_dumped.size());
  }
  g_active = g_dumping || !g_files.empty();
}

}  // namespace

bool Active() {
  std::call_once(g_start_once, Start);
  return g_active;
}

uint64_t HashBlocks(const uint8_t *rows, uint32_t row_bytes, uint32_t blocks_w, uint32_t blocks_h,
                    uint32_t bytes_per_block) {
  XXH3_state_t state;
  XXH3_64bits_reset(&state);
  const size_t tight = size_t(blocks_w) * bytes_per_block;
  for (uint32_t y = 0; y < blocks_h; ++y)
    XXH3_64bits_update(&state, rows + size_t(y) * row_bytes, tight);
  return XXH3_64bits_digest(&state);
}

void Dump(uint64_t hash, uint32_t width, uint32_t height, Format format, const uint8_t *rows,
          uint32_t row_bytes, uint32_t swizzle) {
  if (!g_dumping)
    return;
  const uint32_t edge = format == Format::kRGBA8 ? 1 : 4;
  const uint32_t bs = format == Format::kRGBA8 ? 4 : format == Format::kBC1 ? 8 : 16;
  DumpItem it{hash, width, height, (width + edge - 1) / edge, (height + edge - 1) / edge, swizzle, format, {}};
  std::lock_guard lock(g_mutex);
  if (!g_dumped.insert(hash).second || g_dump_queue_bytes > kDumpQueueLimit)
    return;
  const size_t tight = size_t(it.blocks_w) * bs;
  it.blocks.resize(tight * it.blocks_h);
  for (uint32_t y = 0; y < it.blocks_h; ++y)
    std::memcpy(&it.blocks[y * tight], rows + size_t(y) * row_bytes, tight);
  g_dump_queue_bytes += it.blocks.size();
  g_dump_queue.push_back(std::move(it));
  g_dump_cv.notify_one();
}

std::shared_ptr<const Replacement> Find(uint64_t hash) {
  fs::path file;
  {
    std::lock_guard lock(g_mutex);
    const auto loaded = g_loaded.find(hash);
    if (loaded != g_loaded.end())
      return loaded->second;
    const auto f = g_files.find(hash);
    if (f == g_files.end() || g_failed.count(hash))
      return nullptr;
    file = f->second;
  }
  auto r = std::make_shared<Replacement>();
  std::vector<uint8_t> data;
  const std::string ext = Lower(file.extension().string());
  const bool ok = ReadAll(file, data) && (ext == ".dds" ? DecodeDds(data, *r) : DecodePng(data, *r));
  std::lock_guard lock(g_mutex);
  if (!ok) {
    g_failed.insert(hash);
    BD_WARN("Textures : {} illisible (PNG, ou DDS DXT1/3/5 ou 32 bits)", file.string());
    return nullptr;
  }
  BD_INFO("Textures : {} ({}x{}) utilisé", file.filename().string(), r->width, r->height);
  return g_loaded[hash] = std::move(r);
}

}  // namespace wwe12::texture_packs
