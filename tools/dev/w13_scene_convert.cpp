// WWE '13 Universe scene (pac\evt\Universe00-03.pac: EVP NNNN the script, EVZ NNNN a PACH of track records, one per
// script object) -> a WWE '12 Universe act (pac\m.pac ZMOT/UN00 record, unpacked), read only, for the "more scenes"
// of wwe12_universe.cpp (docs/univers.md section 44). With the Mod Maker's readers / writers (svr11_entrance.h: ParseEvp,
// ParseRaw, Serialize, Quant, BuildRecord; yanm_ik.h BakeBlock: the IK legs made FK, as entrance13.h does).
//  - the characters (script objects of type 0x01000000), in order, are the act's roles 0-5 (block 0000000r);
//  - the camera's blocks c = 0, 1, 2 are the act's cameras 60, 61, 62 (tracks renamed "Camera6c" ...);
//  - a scene's shots (blocks %04d%04d: shot, sub) are put end to end (an act plays one): times shifted, rotations kept,
//    positions (deltas) made absolute and quantized again; props, ropes, the global object and events left out (the
//    game loads the objects of ITS act), but the camera's end marker (event 254).
// w13_scene_convert <EVP> <EVZ (PACH, unpacked)> <template act record> <act> <out>
#include <cstdio>
#include <fstream>
#include <iterator>

#include "../../port/src/modmaker/svr11_entrance.h"
#include "../../port/src/modmaker/svrfmt/pac.h"

using namespace svr11_entrance::detail;
using Bytes = std::vector<uint8_t>;

static Bytes ReadFile(const char *p) {
  std::ifstream f(p, std::ios::binary);
  return Bytes(std::istreambuf_iterator<char>(f), {});
}
static uint32_t Rotr7(uint32_t x) { return (x >> 7) | (x << 25); }
static uint32_t Rotl7(uint32_t x) { return (x << 7) | (x >> 25); }
static uint32_t NameHash(const std::string &s) {
  uint32_t h = 0;
  for (unsigned char c : s)
    h = Rotl7(h) ^ c;
  return h;
}
// a hash whose name ends with the two digits of `from` made to end with those of `to`
static uint32_t Renumber(uint32_t h, int from, int to) {
  uint32_t x = h ^ uint32_t('0' + from % 10);
  x = Rotr7(x) ^ uint32_t('0' + from / 10);
  x = Rotr7(x);
  x = Rotl7(x) ^ uint32_t('0' + to / 10);
  return Rotl7(x) ^ uint32_t('0' + to % 10);
}

static int LastTime(const Comp &c, int kind) {
  if (c.keys.empty())
    return 0;
  if (kind == kScale)
    return c.keys.back().dt;
  int t = 0;
  for (auto &k : c.keys)
    t += k.dt;
  return t;
}

struct Acc {  // one output block being put together
  Raw raw;
  std::map<uint32_t, std::vector<PosKey>> pos;  // by track: absolute positions (pos8 / pos16)
  std::map<uint32_t, int> pos_kind;
};

// Appends a shot's raw tracks at `offset`
static void Append(Acc &acc, const Raw &seg, int offset) {
  acc.raw.ver = seg.ver;
  for (const RawTrack &t : seg.tracks) {
    RawTrack *dst = nullptr;
    for (auto &x : acc.raw.tracks)
      if (x.hash == t.hash)
        dst = &x;
    if (!dst) {
      acc.raw.tracks.push_back({});
      dst = &acc.raw.tracks.back();
      dst->hash = t.hash;
      dst->flags = t.flags;
    } else if (dst->flags != t.flags)
      continue;  // (another layout in this shot: the track holds its last pose)
    for (int k = 0; k < kKinds; ++k) {
      const Comp &c = t.c[size_t(k)];
      if (!c.on || c.keys.empty())
        continue;
      Comp &d = dst->c[size_t(k)];
      if (IsPos(k)) {  // deltas -> absolute (acc x scale + bias)
        acc.pos_kind[t.hash] = k;
        int time = 0;
        double a[3] = {0, 0, 0};
        for (auto &key : c.keys) {
          time += key.dt;
          PosKey p{offset + time, {}};
          for (int i = 0; i < 3; ++i)
            a[i] += key.v[i], p.v[size_t(i)] = a[i] * c.scale + c.bias[size_t(i)];
          acc.pos[t.hash].push_back(p);
        }
        d.on = true;
        continue;
      }
      const bool was = d.on && !d.keys.empty();
      d.on = true, d.axis = c.axis;
      if (k == kScale) {
        for (auto key : c.keys) {
          key.dt += offset;
          if (!was || key.dt > d.keys.back().dt)
            d.keys.push_back(key);
        }
        continue;
      }
      const int maxdt = Is8(k) ? 255 : 65535;
      int gap = offset + c.keys[0].dt - (was ? LastTime(d, k) : 0);
      if (was)
        while (gap > maxdt) {  // a hold of the last pose
          Key h = d.keys.back();
          h.dt = maxdt, d.keys.push_back(h), gap -= maxdt;
        }
      Key first = c.keys[0];
      first.dt = std::max(0, std::min(gap, maxdt));
      if (!was && gap > maxdt) {  // (a track starting late: its first pose from 0)
        first.dt = 0;
        d.keys.push_back(first);
        int g = gap;
        while (g > maxdt) {
          Key h = first;
          h.dt = maxdt, d.keys.push_back(h), g -= maxdt;
        }
        first.dt = g;
      }
      d.keys.push_back(first);
      for (size_t i = 1; i < c.keys.size(); ++i)
        d.keys.push_back(c.keys[i]);
    }
  }
}

static bool Finish(Acc &acc, Ctx &ctx) {
  for (auto &t : acc.raw.tracks) {
    t.flags &= 0x8000 | 0x4000 | 0x2000 | 0x1000 | 0x0800 | 0x0400 | 0x0200 | 0x0100 | 0x0010 | 0x0008;
    if (acc.pos.count(t.hash)) {
      Comp q = Quant(acc.pos[t.hash], 0.02, 1e-7, true, ctx);
      if (!ctx.ok)
        return false;
      t.c[kPos8] = Comp{}, t.c[kPos16] = q;
      t.flags = uint16_t((t.flags & ~0x8000) | 0x0100);
    }
    for (int k = 0; k < kKinds; ++k)
      if ((t.flags & kBits[k]) && (!t.c[size_t(k)].on || t.c[size_t(k)].keys.empty()))
        t.flags = uint16_t(t.flags & ~kBits[k]);
  }
  return true;
}

int main(int argc, char **argv) {
  if (argc < 6)
    return std::fprintf(stderr, "w13_scene_convert <EVP> <EVZ> <template act> <act> <out>\n"), 2;
  const Bytes evp = ReadFile(argv[1]), evz = ReadFile(argv[2]), tpl = ReadFile(argv[3]);
  const int act = std::atoi(argv[4]);
  Ctx ctx;
  Evp e;
  if (!ParseEvp(evp, e))
    return std::fprintf(stderr, "not a script\n"), 1;
  std::vector<svrfmt::PachEntry> leaves;
  if (!svrfmt::PachRead(evz, leaves))
    return std::fprintf(stderr, "tracks: not a PACH\n"), 1;
  std::map<uint32_t, Bytes> rec;
  for (auto &l : leaves)
    rec[l.id] = svrfmt::Unpack(l.data);
  std::vector<int> chars;
  int camera = -1;
  for (size_t i = 0; i < e.objs.size(); ++i) {
    if (IsType(e.objs[i], 0x01000000, 0))
      chars.push_back(int(i));
    else if (IsType(e.objs[i], 0x10000, 0) && camera < 0)
      camera = int(i);
  }
  if (chars.empty() || chars.size() > 6 || camera < 0)
    return std::fprintf(stderr, "%zu characters, camera %d: not taken\n", chars.size(), camera), 1;
  // the blocks of an object's record by (shot, sub), as raw tracks (the characters' legs baked)
  auto blocks_of = [&](int obj, bool bake, std::map<std::pair<int, int>, Raw> &out) {
    const Bytes &r = rec[uint32_t(obj)];
    int kind;
    std::vector<BlockRef> bl;
    if (!Header(r, kind, bl))
      return false;
    std::vector<size_t> starts;
    for (auto &b : bl)
      starts.push_back(b.offset);
    std::sort(starts.begin(), starts.end());
    for (auto &b : bl) {
      const auto nx = std::upper_bound(starts.begin(), starts.end(), b.offset);
      const size_t end = nx == starts.end() ? r.size() : *nx;
      Bytes blk(r.begin() + long(b.offset), r.begin() + long(end));
      if (bake) {
        Bytes baked;
        int frames = 0;
        if (yanm::ik::detail::BakeBlock(blk, baked, &frames))
          blk.swap(baked);
      }
      Raw raw;
      if (!ParseRaw(blk, 0, raw, ctx))
        return false;
      out[{std::atoi(b.name.substr(0, 4).c_str()), std::atoi(b.name.substr(4).c_str())}] = std::move(raw);
    }
    return true;
  };
  std::vector<std::map<std::pair<int, int>, Raw>> role(chars.size());
  for (size_t r = 0; r < chars.size(); ++r)
    if (!blocks_of(chars[r], true, role[r]))
      return std::fprintf(stderr, "character %zu: %s\n", r, ctx.why.c_str()), 1;
  std::map<std::pair<int, int>, Raw> cam;
  if (!blocks_of(camera, false, cam))
    return std::fprintf(stderr, "camera: %s\n", ctx.why.c_str()), 1;
  // the shots and their lengths (the longest character / camera block)
  std::map<int, int> shot_len;
  auto measure = [&](const std::map<std::pair<int, int>, Raw> &m) {
    for (auto &[key, raw] : m)
      for (auto &t : raw.tracks)
        for (int k = 0; k < kKinds; ++k)
          if (t.c[size_t(k)].on)
            shot_len[key.first] = std::max(shot_len[key.first], LastTime(t.c[size_t(k)], k));
  };
  for (auto &m : role)
    measure(m);
  measure(cam);
  std::map<int, int> offset;
  int total = 0;
  for (auto &[s, len] : shot_len)
    offset[s] = total, total += len + 1;
  std::vector<BlockOut> out;
  for (size_t r = 0; r < role.size(); ++r) {
    Acc acc;
    for (auto &[key, raw] : role[r])
      if (key.second == 0)
        Append(acc, raw, offset[key.first]);
    if (!Finish(acc, ctx))
      return std::fprintf(stderr, "role %zu: %s\n", r, ctx.why.c_str()), 1;
    char nm[16];
    std::snprintf(nm, sizeof(nm), "0000%04zu", r);
    out.push_back({nm, acc.raw, {}});
  }
  for (int c = 0; c < 3; ++c) {
    Acc acc;
    for (auto &[key, raw] : cam)
      if (key.second == c)
        Append(acc, raw, offset[key.first]);
    if (acc.raw.tracks.empty())
      continue;
    if (!Finish(acc, ctx))
      return std::fprintf(stderr, "camera %d: %s\n", c, ctx.why.c_str()), 1;
    // the tracks' names end with the camera's number (Camera<nn>, ...): found from the eye's, made 60 + c
    int from = -1;
    for (auto &t : acc.raw.tracks)
      for (int n = 0; n < 100 && from < 0; ++n) {
        char b[16];
        std::snprintf(b, sizeof(b), "Camera%02d", n);
        if (NameHash(b) == t.hash)
          from = n;
      }
    if (from < 0)
      return std::fprintf(stderr, "camera %d: no Camera<nn> track\n", c), 1;
    for (auto &t : acc.raw.tracks)
      t.hash = Renumber(t.hash, from, 60 + c);
    char nm[16];
    std::snprintf(nm, sizeof(nm), "0000%04d", 60 + c);
    out.push_back({nm, acc.raw, {{total - 1, {{254, 0, {}}}}}});
  }
  // cameras missing (WWE '12's acts have 60, 61 and 62): copies of the first
  for (int c = 0; c < 3; ++c) {
    char nm[16];
    std::snprintf(nm, sizeof(nm), "0000%04d", 60 + c);
    bool have = false;
    const BlockOut *first = nullptr;
    for (auto &b : out) {
      have = have || b.name == nm;
      if (!first && b.name.substr(4, 2) == "00" && std::atoi(b.name.substr(4).c_str()) >= 60)
        first = &b;
    }
    if (have || !first)
      continue;
    BlockOut copy = *first;
    const int from = std::atoi(first->name.substr(4).c_str());
    for (auto &t : copy.raw.tracks)
      t.hash = Renumber(t.hash, from, 60 + c);
    copy.name = nm;
    out.push_back(std::move(copy));
  }
  std::sort(out.begin(), out.end(), [](const BlockOut &a, const BlockOut &b) { return a.name < b.name; });
  if (tpl.size() < 0x70)
    return std::fprintf(stderr, "template act too small\n"), 1;
  Bytes record = BuildRecord(Bytes(tpl.begin(), tpl.begin() + 0x70), out, ctx);
  if (!ctx.ok)
    return std::fprintf(stderr, "record: %s\n", ctx.why.c_str()), 1;
  char num[16] = {};
  std::snprintf(num, sizeof(num), "%d", act);
  std::memcpy(&record[0x4C], num, 16);
  std::ofstream(argv[5], std::ios::binary).write((const char *)record.data(), std::streamsize(record.size()));
  std::printf("%d: %zu roles, %zu shots, %d frames, %zu bytes\n", act, chars.size(), shot_len.size(), total,
              record.size());
  return 0;
}
