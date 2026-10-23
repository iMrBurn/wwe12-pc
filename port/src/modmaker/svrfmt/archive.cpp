// WWE12-PC: taken from Kaiko's WWE SmackDown vs. Raw 2011 PC port (with his permission, credit given),
// https://github.com/KaikoClanworth1/wwe-svr2011-pc, port/modmaker/svrfmt/archive.cpp at b2521fe.
#include "archive.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <mutex>

// (jboy.h: not needed for reading)
#include "texture.h"
#include "xcompress.h"

namespace svrfmt {

namespace {

std::mutex g_hash_mutex;
std::map<uint32_t, uint64_t> g_shdc_hash;  // child id -> its index hash (learned from every SHDC read)

ArcWrap WrapOf(const Bytes& stored) {
  if (IsBpe(stored)) return ArcWrap::Bpe;
  if (stored.size() >= 16 && !std::memcmp(stored.data(), "ZLIB", 4)) return ArcWrap::Zlib;
  return ArcWrap::None;
}

Bytes Wrap(const Bytes& raw, ArcWrap w) {
  if (w == ArcWrap::Bpe) return BpeEncode(raw);
  if (w == ArcWrap::Zlib) return ZlibStore(raw);
  return raw;
}

bool Is(const Bytes& d, const char* magic) { return d.size() >= 4 && !std::memcmp(d.data(), magic, 4); }

size_t Align(size_t v, size_t a) { return (v + a - 1) / a * a; }

std::string Hex(uint64_t v, const char* fmt = "0x%llX") {
  char b[32];
  std::snprintf(b, sizeof b, fmt, static_cast<unsigned long long>(v));
  return b;
}

ArcNode Child(const Bytes& stored) {
  ArcNode c;
  c.stored = stored;
  c.wrap = WrapOf(stored);
  return c;
}

// ---- readers (raw: the container's unwrapped bytes)

bool ReadEpac(const Bytes& raw, ArcNode& n) {
  if (!EpacRead(raw, n.epac)) return false;
  n.kind = ArcKind::Epac;
  n.kids.clear();
  for (size_t gi = 0; gi < n.epac.groups.size(); ++gi) {
    auto& g = n.epac.groups[gi];
    for (auto& e : g.entries) {
      ArcNode c = Child(e.data);
      c.name = e.name;
      c.group = g.type;
      c.gi = int(gi);
      c.tail = e.tail;
      n.kids.push_back(std::move(c));
      e.data.clear();  // (the kids hold it)
    }
  }
  return true;
}

bool ReadHspc(const Bytes& raw, ArcNode& n) {
  if (raw.size() < 0x1000 || !Is(raw, "HSPC")) return false;
  const uint32_t ib = Le32(&raw[0x18]), lb = Le32(&raw[0x20]), cnt = Le32(&raw[0x38]), dbytes = Le32(&raw[0x3C]);
  const size_t loc = 0x800 + Align(ib, 0x800), data = loc + Align(lb, 0x800);
  if (size_t(cnt) * 20 > ib || size_t(cnt) * 12 > lb || data > raw.size()) {
    n.note = "HSPC table out of range";
    return false;
  }
  n.kind = ArcKind::Hspc;
  n.head.assign(raw.begin(), raw.begin() + 0x800);
  n.kids.clear();
  for (uint32_t i = 0; i < cnt; ++i) {
    const uint8_t* r = &raw[0x800 + 20 * size_t(i)];
    const uint64_t hash = uint64_t(Le32(r)) | uint64_t(Le32(r + 4)) << 32;
    const uint32_t lo = Le32(r + 12);
    if (loc + lo + 12 > raw.size()) return false;
    const uint32_t sec = Le32(&raw[loc + lo]), sz = Le32(&raw[loc + lo + 4]), fl = Le32(&raw[loc + lo + 8]);
    const size_t off = size_t(sec) * 0x800, len = size_t(sz) * 0x100;
    if (sz && off + len > raw.size()) return false;
    ArcNode c = Child(sz ? Bytes(raw.begin() + ptrdiff_t(off), raw.begin() + ptrdiff_t(off + len)) : Bytes());
    c.hash = hash;
    c.flags = fl;
    c.order = sz ? sec : 0;
    c.name = Hex(hash, "#%016llx");
    n.kids.push_back(std::move(c));
  }
  const size_t tr = data + dbytes;
  if (tr <= raw.size()) n.trailer.assign(raw.begin() + ptrdiff_t(tr), raw.end());
  return true;
}

bool ReadShdc(const Bytes& raw, ArcNode& n) {
  if (raw.size() < 0x40 || !Is(raw, "SHDC")) return false;
  const uint32_t ix = Le32(&raw[0x14]), t = Le32(&raw[0x1C]), cnt = Le32(&raw[0x38]);
  if (t + size_t(cnt) * 16 > raw.size() || ix + size_t(cnt) * 20 > raw.size()) {
    n.note = "SHDC table out of range";
    return false;
  }
  n.kind = ArcKind::Shdc;
  n.head.assign(raw.begin(), raw.begin() + 0x40);
  n.kids.clear();
  n.trailer.clear();
  const uint32_t sec = Le32(&raw[0x24]);
  if (sec && sec <= raw.size()) n.trailer.assign(raw.begin() + sec, raw.end());
  std::lock_guard<std::mutex> lock(g_hash_mutex);
  for (uint32_t i = 0; i < cnt; ++i) {
    const uint8_t* r = &raw[t + 16 * size_t(i)];
    const uint32_t id = Le32(r), off = Le32(r + 4), size = Le32(r + 8);
    if (size_t(off) + size > raw.size()) return false;
    ArcNode c = Child(Bytes(raw.begin() + off, raw.begin() + off + size));
    c.id = id;
    const uint8_t* h = &raw[ix + 20 * size_t(i)];
    c.hash = uint64_t(Le32(h)) | uint64_t(Le32(h + 4)) << 32;
    c.name = Hex(id);
    g_shdc_hash.emplace(id, c.hash);
    n.kids.push_back(std::move(c));
  }
  return true;
}

bool ReadPach(const Bytes& raw, ArcNode& n) {
  std::vector<PachEntry> es;
  if (!PachRead(raw, es)) return false;
  n.kind = ArcKind::Pach;
  n.kids.clear();
  // the children's alignment in the file: the largest power of two (up to 0x800) all their offsets keep
  const size_t base = 8 + 12 * es.size();
  n.align = 0x800;
  for (size_t i = 0; i < es.size(); ++i)
    if (!es[i].data.empty())
      while (n.align > 4 && (base + Le32(&raw[8 + 12 * i + 4])) % n.align) n.align >>= 1;
  if (n.align == 0x800 && es.size() < 2) n.align = 4;   // (nothing to tell it from)
  // bytes after the last child that aren't padding (SvR 2008 menu scripts): kept after the children
  size_t end = base;
  for (size_t i = 0; i < es.size(); ++i) end = std::max<size_t>(end, base + Le32(&raw[8 + 12 * i + 4]) + es[i].data.size());
  n.trailer.clear();
  if (std::any_of(raw.begin() + ptrdiff_t(end), raw.end(), [](uint8_t b) { return b != 0; }))
    n.trailer.assign(raw.begin() + ptrdiff_t(end), raw.end());
  for (auto& e : es) {
    ArcNode c = Child(e.data);
    c.id = e.id;
    c.name = Hex(e.id);
    n.kids.push_back(std::move(c));
  }
  return true;
}

bool ReadContainer(const Bytes& raw, ArcNode& n) {
  if (Is(raw, "PACH")) return ReadPach(raw, n);
  if (Is(raw, "SHDC")) return ReadShdc(raw, n);
  if (Is(raw, "HSPC")) return ReadHspc(raw, n);
  if ((Is(raw, "EPAC") || Is(raw, "EPK8")) && raw.size() >= 0x4000) return ReadEpac(raw, n);
  return false;
}

// ---- writers (from the kids' stored bytes)

Bytes Serialize(ArcNode& n);

Bytes BuildPach(ArcNode& n) {
  const size_t cnt = n.kids.size(), base = 8 + 12 * cnt, a = std::max<size_t>(4, n.align);
  Bytes out = {'P', 'A', 'C', 'H'};
  AppLe32(out, uint32_t(cnt));
  out.resize(base, 0);
  for (size_t i = 0; i < cnt; ++i) {
    const Bytes s = Serialize(n.kids[i]);
    out.resize(Align(out.size(), s.empty() ? 4 : a), 0);   // (offsets are from the table's end; alignment is in the file)
    PutLe32(&out[8 + 12 * i], n.kids[i].id);
    PutLe32(&out[8 + 12 * i + 4], uint32_t(out.size() - base));
    PutLe32(&out[8 + 12 * i + 8], uint32_t(s.size()));
    App(out, s);
  }
  if (!n.trailer.empty()) App(out, n.trailer);
  else Pad(out, 4);
  return out;
}

Bytes BuildShdc(ArcNode& n) {
  const uint32_t cnt = uint32_t(n.kids.size());
  const size_t ix = 0x40, t = ix + 20 * size_t(cnt), first = t + 16 * size_t(cnt);
  Bytes out(n.head);
  out.resize(first, 0);
  PutLe32(&out[0x14], uint32_t(ix));
  PutLe32(&out[0x18], 20 * cnt);
  PutLe32(&out[0x1C], uint32_t(t));
  PutLe32(&out[0x20], 16 * cnt);
  PutLe32(&out[0x38], cnt);
  for (uint32_t i = 0; i < cnt; ++i) {
    ArcNode& c = n.kids[i];
    const Bytes s = Serialize(c);
    Pad(out, 4);
    uint8_t* h = &out[ix + 20 * size_t(i)];
    PutLe32(h, uint32_t(c.hash));
    PutLe32(h + 4, uint32_t(c.hash >> 32));
    PutLe32(h + 8, 0);
    PutLe32(h + 12, 16 * i);
    PutLe32(h + 16, 0xFFFFFFFFu);
    uint8_t* r = &out[t + 16 * size_t(i)];
    PutLe32(r, c.id);
    PutLe32(r + 4, uint32_t(out.size()));
    PutLe32(r + 8, uint32_t(s.size()));
    PutLe32(r + 12, 0);
    App(out, s);
  }
  PutLe32(&out[0x3C], uint32_t(Align(out.size() - first, 4)));
  if (!n.trailer.empty()) {   // the two sections after the data, moved with it
    Pad(out, 4);
    const uint32_t was = Le32(&n.head[0x24]), now = uint32_t(out.size()), second = Le32(&n.head[0x2C]);
    PutLe32(&out[0x24], now);
    if (second) PutLe32(&out[0x2C], second - was + now);
    App(out, n.trailer);
  }
  return out;
}

Bytes BuildEpac(ArcNode& n) {
  Epac e = n.epac;
  for (auto& g : e.groups) g.entries.clear();
  for (auto& c : n.kids) {
    auto g = c.gi >= 0 && size_t(c.gi) < e.groups.size() && e.groups[size_t(c.gi)].type == c.group
                 ? e.groups.begin() + c.gi
                 : std::find_if(e.groups.begin(), e.groups.end(), [&](const EpacGroup& x) { return x.type == c.group; });
    if (g == e.groups.end()) {   // (a new group)
      e.groups.push_back({});
      g = e.groups.end() - 1;
      g->type = c.group;
    }
    g->entries.push_back({c.name, Serialize(c), c.tail});
  }
  return EpacWrite(e);
}

Bytes BuildHspc(ArcNode& n) {
  const uint32_t cnt = uint32_t(n.kids.size());
  const size_t loc = 0x800 + Align(20 * size_t(cnt), 0x800), data = loc + Align(12 * size_t(cnt), 0x800);
  Bytes out(n.head);
  out.resize(data, 0);
  std::vector<Bytes> s(cnt);
  for (uint32_t i = 0; i < cnt; ++i) s[i] = Serialize(n.kids[i]);
  // data in the original order (new entries last), each on 0x800
  std::vector<uint32_t> order(cnt);
  for (uint32_t i = 0; i < cnt; ++i) order[i] = i;
  std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) {
    const uint64_t oa = n.kids[a].order ? n.kids[a].order : ~0ull, ob = n.kids[b].order ? n.kids[b].order : ~0ull;
    return oa < ob;
  });
  std::vector<uint32_t> sector(cnt, 0);
  for (const uint32_t i : order) {
    if (s[i].empty()) continue;
    Pad(out, 0x800);
    sector[i] = uint32_t(out.size() / 0x800);
    App(out, s[i]);
  }
  Pad(out, 0x800);
  const size_t dbytes = out.size() - data;
  for (uint32_t i = 0; i < cnt; ++i) {
    uint8_t* h = &out[0x800 + 20 * size_t(i)];
    PutLe32(h, uint32_t(n.kids[i].hash));
    PutLe32(h + 4, uint32_t(n.kids[i].hash >> 32));
    PutLe32(h + 8, 0);
    PutLe32(h + 12, 12 * i);
    PutLe32(h + 16, 0xFFFFFFFFu);
    uint8_t* l = &out[loc + 12 * size_t(i)];
    PutLe32(l, sector[i]);
    PutLe32(l + 4, uint32_t((s[i].size() + 0xFF) / 0x100));
    PutLe32(l + 8, n.kids[i].flags);
  }
  App(out, n.trailer);
  Pad(out, 0x800);
  const uint32_t sectors = uint32_t(out.size() / 0x800);
  PutLe32(&out[0x18], 20 * cnt);
  PutLe32(&out[0x20], 12 * cnt);
  PutLe32(&out[0x24], sectors - 1);
  PutLe32(&out[0x2C], sectors);
  PutLe32(&out[0x38], cnt);
  PutLe32(&out[0x3C], uint32_t(dbytes));
  return out;
}

// A changed node below n (opened containers only: an unopened one can't have changed).
bool AnyDirtyBelow(const ArcNode& n) {
  if (!n.parsed) return false;
  for (const auto& c : n.kids)
    if (c.dirty || AnyDirtyBelow(c)) return true;
  return false;
}

// A node's stored bytes, rebuilt when it or a descendant changed.
Bytes Serialize(ArcNode& n) {
  if (!n.dirty && !AnyDirtyBelow(n)) return n.stored;
  if (n.kind == ArcKind::Leaf || !n.parsed) return n.stored;
  Bytes raw;
  switch (n.kind) {
    case ArcKind::Pach: raw = BuildPach(n); break;
    case ArcKind::Shdc: raw = BuildShdc(n); break;
    case ArcKind::Epac: raw = BuildEpac(n); break;
    case ArcKind::Hspc: raw = BuildHspc(n); break;
    default: break;
  }
  // the same bytes as before (only touched): keep the stored form - BpeEncode isn't the game's packer.
  // (An EPAC entry holds its data rounded to 0x100: the zeros past a rebuilt container's end count as the same.)
  const Bytes was = ArchiveData(n);
  if (raw.size() <= was.size() && std::equal(raw.begin(), raw.end(), was.begin()) &&
      std::all_of(was.begin() + ptrdiff_t(raw.size()), was.end(), [](uint8_t b) { return b == 0; }))
    return n.stored;
  n.stored = Wrap(raw, n.wrap);
  n.dirty = true;
  return n.stored;
}

}  // namespace

// ---- encoders

Bytes ZlibStore(const Bytes& raw) {
  Bytes out;
  App(out, reinterpret_cast<const uint8_t*>("ZLIB"), 4);
  AppLe32(out, 0x1230);
  AppLe32(out, uint32_t(raw.size()));   // (packed == size: stored, the raw bytes follow)
  AppLe32(out, uint32_t(raw.size()));
  App(out, raw);
  return out;
}

Bytes XcompressStore(const Bytes& plain, const Bytes& like) {
  // header: magic, u32 1, u32 hash, u32 flags (bits 4-5 chunk = 32 KB << n, 6-21 chunk count, 22-23 table
  // width 20 / 32 bits) - big-endian; then the chunks' unpacked sizes, MSB-first bit-packed; chunk 0 after
  // the table, chunk i (i > 0) at file offset i * chunk. A stored chunk is u16 0 + its bytes, so each holds
  // what fits its slot: chunk - 2 bytes (chunk 0: chunk - table end - 2).
  uint8_t hdr[16] = {0x0F, 0xF5, 0x12, 0xED, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0};
  if (like.size() >= 16) std::memcpy(hdr, like.data(), 16);
  uint32_t fl = Be32(&hdr[12]);
  const uint32_t shift = (fl >> 4) & 3, bits = (fl & 0xC00000) ? 32 : 20;
  const size_t chunk = size_t(0x8000) << shift;
  // the chunk count decides the table's size, which decides chunk 0's room: settle it
  uint32_t n = 1;
  for (int it = 0; it < 8; ++it) {
    const size_t table = ((size_t(bits) * n + 31) >> 5) * 4, room0 = chunk - 16 - table - 2;
    const size_t rest = plain.size() > room0 ? plain.size() - room0 : 0;
    const uint32_t need = 1 + uint32_t((rest + (chunk - 2) - 1) / (chunk - 2));
    if (need == n) break;
    n = need;
  }
  const size_t table = ((size_t(bits) * n + 31) >> 5) * 4, data = 16 + table;
  fl = (fl & ~(0xFFFFu << 6)) | (n << 6);
  PutBe32(&hdr[12], fl);
  std::vector<uint32_t> sizes(n);
  size_t left = plain.size();
  for (uint32_t i = 0; i < n; ++i) {
    const size_t room = i ? chunk - 2 : chunk - data - 2;
    sizes[i] = uint32_t(std::min(left, room));
    left -= sizes[i];
  }
  Bytes out(hdr, hdr + 16);
  out.resize(data, 0);
  for (uint32_t i = 0; i < n; ++i)
    for (uint32_t b = 0; b < bits; ++b) {
      const uint64_t q = uint64_t(i) * bits + b;
      if ((sizes[i] >> (bits - 1 - b)) & 1) out[16 + q / 8] |= uint8_t(0x80 >> (q % 8));
    }
  size_t p = 0;
  for (uint32_t i = 0; i < n; ++i) {
    if (i) out.resize(size_t(i) * chunk, 0);
    out.push_back(0), out.push_back(0);
    App(out, plain.data() + p, sizes[i]);
    p += sizes[i];
  }
  return out;
}

uint64_t ShdcHashFor(uint32_t id) {
  std::lock_guard<std::mutex> lock(g_hash_mutex);
  auto it = g_shdc_hash.find(id);
  return it == g_shdc_hash.end() ? 0 : it->second;
}

// ---- the tree

bool ArchiveOpen(const Bytes& file, Archive& a, std::string* error) {
  auto fail = [&](const std::string& why) { if (error) *error = why; return false; };
  a = {};
  a.original = file;
  Bytes plain;
  const Bytes* d = &file;
  if (IsXcompress(file)) {
    if (!XcompressDecode(file, plain, error)) return false;
    a.xcompress = true;
    a.xheader.assign(file.begin(), file.begin() + 16);
    d = &plain;
  }
  a.root.stored = *d;
  if (!ReadContainer(*d, a.root)) return fail(a.root.note.empty() ? "not an EPAC / EPK8 / HSPC / PACH / SHDC file" : a.root.note);
  a.root.parsed = true;
  if (a.root.kind == ArcKind::Hspc) a.format = "HSPC (WWE 2K17)";
  else if (a.root.kind == ArcKind::Pach) a.format = "PACH";
  else if (a.root.kind == ArcKind::Shdc) a.format = "SHDC";
  else a.format = std::string(a.root.epac.epk8 ? "EPK8" : "EPAC") + (a.root.epac.e20 ? " (20-byte entries, WWE 2K16)"
                  : a.root.epac.wide ? " (16-byte entries)" : " (12-byte entries)");
  if (a.xcompress) a.format += ", xcompressed";
  return true;
}

Bytes ArchiveData(const ArcNode& n) { return n.wrap == ArcWrap::None ? n.stored : Unpack(n.stored); }

bool ArchiveExpand(ArcNode& n) {
  if (n.parsed) return n.kind != ArcKind::Leaf;
  const Bytes raw = ArchiveData(n);
  ArcNode probe;
  if (!ReadContainer(raw, probe)) return false;
  n.kind = probe.kind;
  n.kids = std::move(probe.kids);
  n.epac = std::move(probe.epac);
  n.head = std::move(probe.head);
  n.trailer = std::move(probe.trailer);
  n.align = probe.align;
  n.parsed = true;
  return true;
}

std::string ArchiveKindOf(const ArcNode& n) {
  if (n.parsed && n.kind != ArcKind::Leaf) {
    switch (n.kind) {
      case ArcKind::Pach: return "PACH";
      case ArcKind::Shdc: return "SHDC";
      case ArcKind::Epac: return "EPAC";
      case ArcKind::Hspc: return "HSPC";
      default: break;
    }
  }
  const Bytes d = ArchiveData(n);
  for (const char* m : {"PACH", "SHDC", "EPAC", "EPK8", "HSPC"})
    if (Is(d, m)) return m;
  if (Is(d, "DDS ")) return "DDS";
  if (IsTextureBundle(d)) return "textures";
  if (Is(d, "JBOY")) return "model";
  if (Is(d, "YMBs") || Is(d, "YMKs") || Is(d, "YANM")) return "motions";
  if (!d.empty()) {
    size_t text = 0;
    const size_t m = std::min<size_t>(d.size(), 256);
    for (size_t i = 0; i < m; ++i) text += (d[i] >= 0x20 && d[i] < 0x7F) || d[i] == '\r' || d[i] == '\n' || d[i] == '\t';
    if (text * 10 >= m * 9) return "text";
  }
  return "data";
}

void ArchiveReplace(ArcNode& n, const Bytes& raw) {
  n.stored = Wrap(raw, n.wrap);
  n.kind = ArcKind::Leaf;
  n.kids.clear();
  n.parsed = false;
  n.dirty = true;
}

void ArchiveTouch(ArcNode& n) { n.dirty = true; }

size_t ArchiveNameMax(const ArcNode& epac) { return epac.epac.wide ? 8 : 4; }

std::string ArchiveCantAdd(const ArcNode& p, uint32_t id) {
  switch (p.kind) {
    case ArcKind::Hspc:
      return "WWE 2K17's HSPC pacs find entries by a hash of their name, and that hash isn't known: entries can be "
             "replaced and removed, not added.";
    case ArcKind::Shdc:
      if (!ShdcHashFor(id))
        return "SHDC lists find children by a hash of the id, and the hash for " + Hex(id) +
               " isn't known (it is learned from SHDCs that have that id: open one).";
      return "";
    case ArcKind::Epac:
    case ArcKind::Pach: return "";
    default: return "not a list of entries";
  }
}

ArcNode* ArchiveAdd(ArcNode& p, const std::string& group, const std::string& name, uint32_t id, const Bytes& raw,
                    std::string* why) {
  auto fail = [&](const std::string& w) -> ArcNode* { if (why) *why = w; return nullptr; };
  if (!p.parsed) return fail("not opened");
  const std::string cant = ArchiveCantAdd(p, id);
  if (!cant.empty()) return fail(cant);
  ArcNode c;
  c.wrap = p.kids.empty() ? ArcWrap::None : p.kids.front().wrap;  // (wrapped like the siblings)
  if (p.kind == ArcKind::Epac) {
    const size_t max = ArchiveNameMax(p);
    if (name.empty() || name.size() > max) return fail("names are 1 to " + std::to_string(max) + " characters");
    if (group.size() != 4) return fail("group types are 4 characters");
    for (const auto& k : p.kids)
      if (k.group == group && k.name == name) return fail(group + "/" + name + " is there already: replace it instead");
    c.name = name;
    c.group = group;
    c.wrap = ArcWrap::None;
    size_t at = p.kids.size();
    for (size_t i = 0; i < p.kids.size(); ++i)
      if (p.kids[i].group == group) at = i + 1, c.gi = p.kids[i].gi, c.wrap = p.kids[i].wrap;
    c.stored = Wrap(raw, c.wrap);
    c.dirty = true;
    p.kids.insert(p.kids.begin() + ptrdiff_t(at), std::move(c));
    p.dirty = true;
    return &p.kids[at];
  }
  for (const auto& k : p.kids)
    if (k.id == id) return fail(Hex(id) + " is there already: replace it instead");
  c.id = id;
  c.name = Hex(id);
  if (p.kind == ArcKind::Shdc) c.hash = ShdcHashFor(id);
  c.stored = Wrap(raw, c.wrap);
  c.dirty = true;
  size_t at = 0;
  while (at < p.kids.size() && p.kids[at].id < id) ++at;
  p.kids.insert(p.kids.begin() + ptrdiff_t(at), std::move(c));
  p.dirty = true;
  return &p.kids[at];
}

void ArchiveRemove(ArcNode& p, size_t i) {
  if (i >= p.kids.size()) return;
  p.kids.erase(p.kids.begin() + ptrdiff_t(i));
  p.dirty = true;
}

bool ArchiveRename(ArcNode& p, size_t i, const std::string& name, std::string* why) {
  auto fail = [&](const std::string& w) { if (why) *why = w; return false; };
  if (p.kind != ArcKind::Epac || i >= p.kids.size()) return fail("only EPAC entries have names (the others have ids or hashes)");
  const size_t max = ArchiveNameMax(p);
  if (name.empty() || name.size() > max) return fail("names are 1 to " + std::to_string(max) + " characters");
  for (size_t k = 0; k < p.kids.size(); ++k)
    if (k != i && p.kids[k].group == p.kids[i].group && p.kids[k].name == name) return fail("that name is taken in the group");
  p.kids[i].name = name;
  p.kids[i].dirty = true;
  p.dirty = true;
  return true;
}

void ArchiveTouchAll(ArcNode& n, int depth) {
  if (depth <= 0) return;
  if (!n.parsed && !ArchiveExpand(n)) return;
  if (n.kind == ArcKind::Leaf) return;
  n.dirty = true;
  for (auto& c : n.kids) ArchiveTouchAll(c, depth - 1);
}

Bytes ArchiveSave(Archive& a) {
  const Bytes plain = Serialize(a.root);
  if (!a.root.dirty) return a.original;   // (nothing changed: the file as it was)
  return a.xcompress ? XcompressStore(plain, a.xheader) : plain;
}

}  // namespace svrfmt
