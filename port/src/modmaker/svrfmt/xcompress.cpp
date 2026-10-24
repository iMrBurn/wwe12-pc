// WWE12-PC: taken from Kaiko's WWE SmackDown vs. Raw 2011 PC port (with his permission, credit given),
// https://github.com/KaikoClanworth1/wwe-svr2011-pc, port/modmaker/svrfmt/wwe13.cpp at b2521fe (its LZX part):
// XMemCompress'd files (WWE '13 and later), decoded with libmspack's LZX reader (LGPL 2.1, the copy in the ReXGlue
// SDK's thirdparty/libmspack).
#include "xcompress.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
#include "mspack.h"
#include "lzx.h"
}

namespace svrfmt {

namespace {

// ------------------------------------------------------------------ LZX (libmspack lzxd over memory)

struct MemFile {
  const uint8_t* buf;
  uint8_t* wbuf;
  size_t size, off;
};
mspack_file* MOpen(mspack_system*, const char*, int) { return nullptr; }
void MClose(mspack_file*) {}
int MRead(mspack_file* f, void* b, int n) {
  auto* m = reinterpret_cast<MemFile*>(f);
  const size_t r = std::min(size_t(n), m->size - std::min(m->size, m->off));
  std::memcpy(b, m->buf + m->off, r);
  m->off += r;
  return int(r);
}
int MWrite(mspack_file* f, void* b, int n) {
  auto* m = reinterpret_cast<MemFile*>(f);
  const size_t r = std::min(size_t(n), m->size - std::min(m->size, m->off));
  std::memcpy(m->wbuf + m->off, b, r);
  m->off += r;
  return n;
}
int MSeek(mspack_file*, off_t, int) { return 0; }
off_t MTell(mspack_file* f) { return off_t(reinterpret_cast<MemFile*>(f)->off); }
void MMsg(mspack_file*, const char*, ...) {}
void* MAlloc(mspack_system*, size_t n) { return std::calloc(n, 1); }
void MFree(void* p) { std::free(p); }
void MCopy(void* s, void* d, size_t n) { std::memmove(d, s, n); }

}  // namespace

bool XcompressDecode(const Bytes& d, Bytes& out, std::string* error) {
  auto fail = [&](const std::string& why) { if (error) *error = why; return false; };
  if (!IsXcompress(d)) return fail("not a 0x0FF512ED file");
  // header: magic, u32 1, u32 hash, u32 flags: bits 4-5 chunk = 32 KB << n,
  // bits 6-21 chunk count, bits 22-23 table entry width (20 or 32 bits)
  const uint32_t fl = Be32(&d[12]);
  const uint32_t n = (fl >> 6) & 0xFFFF, bits = (fl & 0xC00000) ? 32 : 20;
  const uint32_t shift = (fl >> 4) & 3;
  const size_t chunk = size_t(0x8000) << shift;
  const int window_bits = 15 + int(shift);
  const size_t table = ((size_t(bits) * n + 31) >> 5) * 4, data = 16 + table;
  if (data > d.size()) return fail("table out of range");
  std::vector<uint32_t> sizes(n);  // unpacked size of each chunk, MSB-first bit packed
  uint64_t total = 0;
  for (uint32_t i = 0; i < n; ++i) {
    uint64_t v = 0;
    for (uint32_t b = 0; b < bits; ++b) {
      const uint64_t q = uint64_t(i) * bits + b;
      v = v << 1 | ((d[16 + q / 8] >> (7 - q % 8)) & 1);
    }
    sizes[i] = uint32_t(v);
    total += v;
  }
  out.assign(size_t(total), 0);
  mspack_system sys = {MOpen, MClose, MRead, MWrite, MSeek, MTell, MMsg, MAlloc, MFree, MCopy, nullptr};
  // a chunk's frames from p: {u16 packed size} (32 KB unpacked) or {0xFF, u16 unpacked, u16 packed},
  // read until the chunk's unpacked size; *after = where the next byte is
  auto frames_at = [&](size_t p, size_t unpacked, Bytes& frames, size_t* after) {
    frames.clear();
    size_t got = 0;
    while (got < unpacked && p + 2 <= d.size()) {
      size_t c, u = 0x8000;
      if (d[p] == 0xFF) {
        if (p + 5 > d.size()) return false;
        u = size_t(d[p + 1]) << 8 | d[p + 2];
        c = size_t(d[p + 3]) << 8 | d[p + 4];
        p += 5;
      } else {
        c = size_t(d[p]) << 8 | d[p + 1];
        p += 2;
      }
      if (!c || p + c > d.size()) return false;
      frames.insert(frames.end(), d.begin() + ptrdiff_t(p), d.begin() + ptrdiff_t(p + c));
      if (c & 1) frames.push_back(0);  // the LZX reader works in 16-bit words
      p += c;
      got += u ? u : 0x8000;
    }
    *after = p;
    return got >= unpacked;
  };
  auto decode = [&](const Bytes& frames, uint8_t* dst, uint32_t size) {
    MemFile in{const_cast<uint8_t*>(frames.data()), nullptr, frames.size(), 0};
    MemFile o{nullptr, dst, size, 0};
    lzxd_stream* l = lzxd_init(&sys, reinterpret_cast<mspack_file*>(&in), reinterpret_cast<mspack_file*>(&o),
                               window_bits, 0, 0x8000, off_t(size), 0);
    if (!l) return -1;
    const int r = lzxd_decompress(l, off_t(size));
    lzxd_free(l);
    return r;
  };
  Bytes frames;
  size_t op = 0, next = data;  // (next: right after the previous chunk's frames)
  for (uint32_t i = 0; i < n; ++i) {
    // chunk i: usually at file offset i * chunk (chunk 0 after the header); some files pack them back to back
    const size_t slot = i ? size_t(i) * chunk : data;
    int r = -2;
    size_t after = 0;
    std::vector<size_t> starts{slot};
    if (next != slot) starts.push_back(next);
    for (const size_t start : starts) {
      // a stored chunk (2K14): u16 0, then its bytes as they are
      if (start + 2 + sizes[i] <= d.size() && d[start] == 0 && d[start + 1] == 0) {
        std::memcpy(out.data() + op, &d[start + 2], sizes[i]);
        after = start + 2 + sizes[i], r = 0;
        break;
      }
      if (!frames_at(start, sizes[i], frames, &after)) continue;
      r = decode(frames, out.data() + op, sizes[i]);
      if (!r) break;
    }
    if (r) return fail("LZX error " + std::to_string(r) + " in chunk " + std::to_string(i));
    next = after;
    op += sizes[i];
  }
  return true;
}

}  // namespace svrfmt
