// WWE12-PC: taken from Kaiko's WWE SmackDown vs. Raw 2011 PC port (with his permission, credit given),
// https://github.com/KaikoClanworth1/wwe-svr2011-pc, port/modmaker/svrfmt/pac.cpp at b2521fe.
#define _CRT_SECURE_NO_WARNINGS
#include "pac.h"

// (zlib: the launcher's inflate, linked into the Mod Maker and svrmod only -
// SVRFMT_INFLATE; the game, which builds this file too, reads no ZLIB entries)
#ifdef SVRFMT_INFLATE
extern "C" {
#include "unzip.h"
}
#endif

#include <algorithm>
#include <cstdio>
#include <functional>

namespace svrfmt {

bool ReadFile(const std::string& path, Bytes& out) {
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  std::fseek(f, 0, SEEK_END);
  long n = std::ftell(f);
  std::fseek(f, 0, SEEK_SET);
  out.resize(n > 0 ? size_t(n) : 0);
  bool ok = out.empty() || std::fread(out.data(), 1, out.size(), f) == out.size();
  std::fclose(f);
  return ok;
}

bool WriteFile(const std::string& path, const Bytes& data) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return false;
  bool ok = data.empty() || std::fwrite(data.data(), 1, data.size(), f) == data.size();
  return std::fclose(f) == 0 && ok;
}

// ------------------------------------------------------------------ BPE

// The game decodes into a 4000-byte block buffer (every original block
// unpacks to at most 4000 bytes); larger blocks hang the arena load.
constexpr size_t kBpeBlock = 4000;

bool BpeDecode(const Bytes& in, Bytes& out) {
  if (!IsBpe(in)) return false;
  const uint32_t packed = Le32(&in[8]), unpacked = Le32(&in[12]);
  if (16 + size_t(packed) > in.size()) return false;
  out.clear();
  out.reserve(unpacked);
  size_t pos = 16;
  const size_t end = 16 + packed;
  uint8_t left[256], right[256], stack[256];
  while (pos < end) {
    for (int i = 0; i < 256; ++i) left[i] = uint8_t(i);
    int c = 0;
    int count = in[pos++];
    for (;;) {
      if (count > 127) { c += count - 127; count = 0; }
      if (c == 256) break;
      for (int i = 0; i <= count; ++i, ++c) {
        if (pos >= end) return false;
        left[c] = in[pos++];
        if (c != left[c]) right[c] = in[pos++];
      }
      if (c == 256) break;
      count = in[pos++];
    }
    if (pos + 2 > end) return false;
    size_t size = in[pos] | in[pos + 1] << 8;
    pos += 2;
    if (pos + size > end) return false;
    for (size_t k = 0; k < size; ++k) {
      int sp = 0;
      stack[sp++] = in[pos + k];
      while (sp) {
        uint8_t s = stack[--sp];
        if (s == left[s]) {
          out.push_back(s);
        } else {
          if (sp > 254) return false;
          stack[sp++] = right[s];
          stack[sp++] = left[s];
        }
      }
    }
    pos += size;
  }
  return out.size() == unpacked;
}

Bytes BpeEncodeStored(const Bytes& raw) {
  Bytes body;
  for (size_t at = 0; at < raw.size(); at += kBpeBlock) {
    size_t n = raw.size() - at < kBpeBlock ? raw.size() - at : kBpeBlock;
    body.push_back(255);  // skip 128 identity codes,
    body.push_back(128);  // code 128 is itself,
    body.push_back(254);  // skip the remaining 127
    body.push_back(uint8_t(n));
    body.push_back(uint8_t(n >> 8));
    App(body, &raw[at], n);
  }
  Bytes out = {'B', 'P', 'E', ' '};
  AppLe32(out, 0x100);
  AppLe32(out, uint32_t(body.size()));
  AppLe32(out, uint32_t(raw.size()));
  App(out, body);
  return out;
}

namespace {

// Pair table as Gage writes it: a byte > 127 skips (byte - 127) codes that
// stand for themselves and is followed by one entry; a byte <= 127 is
// followed by byte + 1 entries; an entry is left[c] (+ right[c] if a pair).
void AppendPairTable(Bytes& out, const uint8_t* left, const uint8_t* right) {
  int c = 0;
  while (c < 256) {
    int skip = 0;
    while (c + skip < 256 && left[c + skip] == c + skip && skip < 128) ++skip;
    if (skip) {
      out.push_back(uint8_t(127 + skip));
      c += skip;
      if (c == 256) break;
      out.push_back(left[c]);
      if (left[c] != c) out.push_back(right[c]);
      ++c;
      continue;
    }
    int run = 0;
    while (c + run < 256 && left[c + run] != c + run && run < 128) ++run;
    out.push_back(uint8_t(run - 1));
    for (int k = c; k < c + run; ++k) {
      out.push_back(left[k]);
      out.push_back(right[k]);
    }
    c += run;
  }
}

void CompressBlock(const uint8_t* src, size_t n, Bytes& out) {
  std::vector<uint8_t> buf(src, src + n), next;
  next.reserve(n);
  uint8_t left[256], right[256];
  bool used[256] = {};
  for (int i = 0; i < 256; ++i) { left[i] = uint8_t(i); right[i] = 0; }
  for (uint8_t b : buf) used[b] = true;
  std::vector<uint16_t> counts(65536);
  for (;;) {
    int code = -1;
    for (int k = 0; k < 256; ++k)
      if (!used[k]) { code = k; break; }
    if (code < 0 || buf.size() < 2) break;
    int best = 0, best_pair = 0;
    for (size_t i = 0; i + 1 < buf.size(); ++i) {
      const int p = buf[i] << 8 | buf[i + 1];
      if (++counts[p] > best) { best = counts[p]; best_pair = p; }
    }
    for (size_t i = 0; i + 1 < buf.size(); ++i) counts[buf[i] << 8 | buf[i + 1]] = 0;  // (only the pairs seen)
    if (best < 3) break;  // Gage: a pair must occur 3+ times to pay for its table entry
    const uint8_t a = uint8_t(best_pair >> 8), b = uint8_t(best_pair);
    next.clear();
    for (size_t i = 0; i < buf.size(); ++i) {
      if (i + 1 < buf.size() && buf[i] == a && buf[i + 1] == b) { next.push_back(uint8_t(code)); ++i; }
      else next.push_back(buf[i]);
    }
    buf.swap(next);
    left[code] = a;
    right[code] = b;
    used[code] = true;
  }
  AppendPairTable(out, left, right);
  out.push_back(uint8_t(buf.size()));
  out.push_back(uint8_t(buf.size() >> 8));
  App(out, buf.data(), buf.size());
}

}  // namespace

Bytes BpeEncode(const Bytes& raw) {
  Bytes body;
  for (size_t at = 0; at < raw.size(); at += kBpeBlock) {
    const size_t n = raw.size() - at < kBpeBlock ? raw.size() - at : kBpeBlock;
    CompressBlock(&raw[at], n, body);
  }
  Bytes out = {'B', 'P', 'E', ' '};
  AppLe32(out, 0x100);
  AppLe32(out, uint32_t(body.size()));
  AppLe32(out, uint32_t(raw.size()));
  App(out, body);
  return out;
}

// WWE 2K14 entries: "ZLIB", u32 version (0x1230), u32 packed size, u32
// unpacked size, then a zlib stream (2-byte header, deflate); stored as is
// when the sizes are equal. A bare zlib stream (0x78 0x01/9C/DA) also occurs.
static bool ZlibUnpack(const Bytes& in, size_t at, size_t packed, size_t usize, Bytes& out) {
#ifdef SVRFMT_INFLATE
  if (at + packed > in.size() || packed < 2) return false;
  out.assign(usize, 0);
  return usize && unzip_inflate(&in[at + 2], packed - 2, out.data(), usize);
#else
  (void)in, (void)at, (void)packed, (void)usize, (void)out;
  return false;
#endif
}

Bytes Unpack(const Bytes& entry) {
  if (entry.size() >= 16 && !std::memcmp(entry.data(), "ZLIB", 4)) {
    const uint32_t packed = Le32(&entry[8]), usize = Le32(&entry[12]);
    if (packed == usize && 16 + size_t(usize) <= entry.size()) return Bytes(entry.begin() + 16, entry.begin() + 16 + usize);
    Bytes out;
    return ZlibUnpack(entry, 16, packed, usize, out) ? out : Bytes{};
  }
  if (!IsBpe(entry)) return entry;
  Bytes out;
  if (!BpeDecode(entry, out)) return {};
  return out;
}

// ------------------------------------------------------------------ PACH

bool PachRead(const Bytes& in, std::vector<PachEntry>& out) {
  if (!IsPach(in)) return false;
  const uint32_t n = Le32(&in[4]);
  const size_t base = 8 + size_t(n) * 12;
  if (base > in.size()) return false;
  out.clear();
  for (uint32_t i = 0; i < n; ++i) {
    const uint8_t* e = &in[8 + i * 12];
    const size_t off = base + Le32(e + 4), size = Le32(e + 8);
    if (off + size > in.size()) return false;
    out.push_back({Le32(e), Bytes(in.begin() + off, in.begin() + off + size)});
  }
  return true;
}

Bytes PachWrite(const std::vector<PachEntry>& entries) {
  Bytes table = {'P', 'A', 'C', 'H'};
  AppLe32(table, uint32_t(entries.size()));
  Bytes body;
  for (const auto& e : entries) {
    AppLe32(table, e.id);
    AppLe32(table, uint32_t(body.size()));
    AppLe32(table, uint32_t(e.data.size()));
    App(body, e.data);
    Pad(body, 4);
  }
  App(table, body);
  return table;
}

// ------------------------------------------------------------------ EPAC

namespace {

bool IsArchive(const Bytes& in) {
  return in.size() >= 0x4000 && (!std::memcmp(in.data(), "EPAC", 4) || !std::memcmp(in.data(), "EPK8", 4));
}

// The group table read with one entry size; false when it doesn't fit (a count
// that isn't a whole number of entries, a table past 0x4000, an entry past the
// file's end, a name that isn't text).
struct RawGroup {
  std::string type;
  uint16_t number = 0;
  uint32_t extra = 0;
  std::vector<std::pair<std::string, std::pair<uint32_t, uint32_t>>> entries;  // name, (sector, size / 0x100)
  std::vector<uint32_t> tails;  // (20-byte entries) each entry's last u32
};
// esz: 12, 16 (8-char names) or 20 (WWE 2K16: name[4], sector, size, the size again, tail; count = entries x 4)
bool ReadTable(const Bytes& in, size_t esz, uint64_t file_size, std::vector<RawGroup>& out, uint32_t* used) {
  out.clear();
  const size_t nlen = esz == 16 ? 8 : 4, words = esz == 12 ? 3 : 4;
  size_t p = 0x800;
  *used = 0;
  while (p + 12 <= 0x4000) {
    if (!Le32(&in[p])) break;
    RawGroup g;
    g.type.assign(reinterpret_cast<const char*>(&in[p]), 4);
    const uint32_t w = Le16(&in[p + 4]);
    if (w % words) return false;
    g.number = Le16(&in[p + 6]);
    g.extra = Le32(&in[p + 8]);
    p += 12;
    const uint32_t cnt = w / uint32_t(words);
    if (p + esz * cnt > 0x4000) return false;
    for (uint32_t k = 0; k < cnt; ++k, p += esz) {
      const uint8_t* e = &in[p];
      for (size_t c = 0; c < nlen && e[c]; ++c)
        if (e[c] < 0x20) return false;  // (names may be Shift-JIS: only control bytes rule a layout out)
      const uint32_t sec = Le32(e + nlen), sz = Le32(e + nlen + 4);
      if (sec >= 0x200000) return false;  // (past 4 GB: name bytes read as a sector - the wrong entry size)
      if (esz == 20 && Le32(e + 12) != sz) return false;  // (2K16 gives the size twice)
      if (file_size && 0x4000 + uint64_t(sec) * 0x800 + uint64_t(sz) * 0x100 > file_size + 0x800) return false;
      g.entries.push_back({Name16(e, nlen), {sec, sz}});
      g.tails.push_back(esz == 20 ? Le32(e + 16) : 0);
    }
    *used += 12 + uint32_t(esz) * cnt;
    out.push_back(std::move(g));
  }
  return true;
}

// The consistent layout: the flag's (bit 0 at group +6), then the magic's, then the other, then 2K16's 20 bytes.
bool ReadAnyTable(const Bytes& in, uint64_t file_size, std::vector<RawGroup>& out, size_t& esz, uint32_t& used) {
  if (!IsArchive(in)) return false;
  const bool magic16 = !std::memcmp(in.data(), "EPK8", 4);
  const bool flag16 = Le32(&in[0x800]) && (Le16(&in[0x806]) & 1);
  for (size_t e : {size_t(flag16 ? 16 : 12), size_t(magic16 ? 16 : 12), size_t(magic16 ? 12 : 16), size_t(20)})
    if (ReadTable(in, e, file_size, out, &used)) {
      esz = e;
      return true;
    }
  return false;
}

}  // namespace

bool EpacTable(const Bytes& head, uint64_t file_size, std::vector<EpacRow>& rows, bool* wide) {
  rows.clear();
  std::vector<RawGroup> gs;
  size_t esz = 12;
  uint32_t used = 0;
  if (!ReadAnyTable(head, file_size, gs, esz, used)) return false;
  for (const auto& g : gs)
    for (const auto& [name, at] : g.entries)
      rows.push_back({g.type, name, 0x4000 + uint64_t(at.first) * 0x800, at.second * 0x100});
  if (wide) *wide = esz == 16;
  return true;
}

bool EpacRead(const Bytes& in, Epac& out) {
  std::vector<RawGroup> gs;
  size_t esz = 12;
  uint32_t used = 0;
  if (!ReadAnyTable(in, in.size(), gs, esz, used)) return false;
  out = {};
  out.epk8 = !std::memcmp(in.data(), "EPK8", 4);
  out.wide = esz == 16;
  out.e20 = esz == 20;
  out.header.assign(in.begin(), in.begin() + 0x800);
  bool any_extra = false, sectors = true;
  for (const auto& g : gs) {
    EpacGroup eg;
    eg.type = g.type;
    eg.number = g.number;
    eg.extra = g.extra;
    if (g.extra) {
      any_extra = true;
      if (g.entries.empty() || g.entries.front().second.first != g.extra) sectors = false;
    }
    for (size_t k = 0; k < g.entries.size(); ++k) {
      const auto& [name, at] = g.entries[k];
      const size_t off = 0x4000 + size_t(at.first) * 0x800, size = size_t(at.second) * 0x100;
      if (off + size > in.size()) return false;
      eg.entries.push_back({name, Bytes(in.begin() + ptrdiff_t(off), in.begin() + ptrdiff_t(off + size)), g.tails[k]});
    }
    out.groups.push_back(std::move(eg));
  }
  out.group_sectors = any_extra && sectors;
  out.toc_extra = int32_t(Le32(&in[4]) - used);
  const size_t data_end = 0x4000 + size_t(Le32(&in[8]));
  if (data_end <= in.size()) out.trailer.assign(in.begin() + ptrdiff_t(data_end), in.end());
  return true;
}

Bytes EpacWrite(const Epac& e) {
  Bytes out(e.header);
  out.resize(0x800, 0);
  const size_t esz = e.e20 ? 20 : e.wide ? 16 : 12, nlen = e.wide ? 8 : 4, words = e.e20 ? 4 : esz / 4;
  Bytes toc, body;
  uint32_t used = 0;
  for (const auto& g : e.groups) {
    AppName(toc, g.type, 4);
    AppLe32(toc, uint32_t(g.entries.size() * words) | uint32_t(g.number) << 16);
    Pad(body, 0x800);
    AppLe32(toc, e.group_sectors ? uint32_t(body.size() / 0x800) : g.extra);
    for (const auto& en : g.entries) {
      Pad(body, 0x800);
      AppName(toc, en.name, nlen);
      AppLe32(toc, uint32_t(body.size() / 0x800));
      AppLe32(toc, uint32_t((en.data.size() + 0xFF) / 0x100));
      if (e.e20) {
        AppLe32(toc, uint32_t((en.data.size() + 0xFF) / 0x100));
        AppLe32(toc, en.tail);
      }
      App(body, en.data);
      Pad(body, 0x100);
    }
    used += 12 + uint32_t(esz * g.entries.size());
  }
  Pad(body, 0x800);
  toc.resize(0x3800, 0);
  App(out, toc);
  PutLe32(&out[4], uint32_t(int32_t(used) + e.toc_extra));
  PutLe32(&out[8], uint32_t(body.size()));
  App(out, body);
  if (!e.trailer.empty()) App(out, e.trailer);
  else out.resize(out.size() + 0x800, 0);
  return out;
}

}  // namespace svrfmt
