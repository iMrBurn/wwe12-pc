// SvR 2011 Universe scene (pac\evt\wunimot.pac UNVS/NNNN, its YMKs bank) -> a WWE '12 Universe act (a pac\m.pac
// ZMOT/UN00 record, unpacked), with the Mod Maker's SvR 2011 entrance converter (svr11_entrance.h: BodyBlock,
// CameraBlock; after LA Max's SVR11_to_WWE12_Tool and Kaiko's tools, credit in docs/mod_maker.md).
// docs/univers.md section 44: an act = one record, one block per track named %04d%04d (x, y): y 0-59 a role's body
// (y = the role), 60 / 61 / 62 the cameras (eye "Camera<y>", target, fov: the entrance camera block's tracks with the
// block's number in their names). SvR 2011's scenes have cameras 60, 61 and 66: 66 is written as 62.
//
// svr_scene_convert <ymks bank> <template record, or Nyujyo0.pac> <act number> <out record> [act for the header]
// The template gives the record header and the body template (a WWE '12 act's first role block).
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
// a camera track's hash with its last two characters "01" (the entrance's) made the camera's number
static uint32_t Renumber(uint32_t h01, int n) {
  uint32_t x = h01 ^ uint32_t('1');
  x = Rotr7(x) ^ uint32_t('0');
  x = Rotr7(x);
  x = Rotl7(x) ^ uint32_t('0' + n / 10);
  x = Rotl7(x) ^ uint32_t('0' + n % 10);
  return x;
}

// the other way: a camera track's hash with its number n made "01"
static uint32_t Unnumber(uint32_t hn, int n) {
  uint32_t x = hn ^ uint32_t('0' + n % 10);
  x = Rotr7(x) ^ uint32_t('0' + n / 10);
  x = Rotr7(x);
  x = Rotl7(x) ^ uint32_t('0');
  x = Rotl7(x) ^ uint32_t('1');
  return x;
}

int main(int argc, char **argv) {
  if (argc < 5) {
    std::fprintf(stderr, "svr_scene_convert <ymks> <template act> <act> <out>\n");
    return 2;
  }
  const Bytes ymks = ReadFile(argv[1]);
  Bytes tpl_act = ReadFile(argv[2]);
  // a .pac: WWE '12's Nyujyo0.pac, the Rock's entrance (EVZ 100) record 0, a body block that isn't templated (the
  // acts' are)
  if (std::string(argv[2]).size() > 4 && std::string(argv[2]).substr(std::string(argv[2]).size() - 4) == ".pac") {
    svrfmt::Epac e;
    if (!svrfmt::EpacRead(tpl_act, e))
      return std::fprintf(stderr, "not an EPAC\n"), 1;
    tpl_act.clear();
    for (auto &g : e.groups)
      if (g.type.rfind("EVZ", 0) == 0)
        for (auto &en : g.entries)
          if (std::atoi(en.name.c_str()) == 100) {
            std::vector<svrfmt::PachEntry> leaves;
            if (svrfmt::PachRead(svrfmt::Unpack(en.data), leaves))
              for (auto &l : leaves)
                if (l.id == 0)
                  tpl_act = svrfmt::Unpack(l.data);
          }
    if (tpl_act.empty())
      return std::fprintf(stderr, "no EVZ 100 record 0\n"), 1;
  }
  if (tpl_act.size() < 0x70)
    return std::fprintf(stderr, "template too small\n"), 1;
  Bytes tpl_head(tpl_act.begin(), tpl_act.begin() + 0x70);
  if (argc > 5) {  // the act whose header the record takes (a WWE '12 act record)
    const Bytes h = ReadFile(argv[5]);
    if (h.size() >= 0x70)
      std::copy(h.begin(), h.begin() + 0x70, tpl_head.begin());
  }
  const int act = std::atoi(argv[3]);
  Ctx ctx;
  std::vector<Motion> bank;
  Span T;
  if (!ReadBank(ymks, bank, T))
    return std::fprintf(stderr, "not a YMKs bank\n"), 1;
  std::vector<TemplateTrack> tpl;
  if (!BodyTemplate(tpl_act, tpl, ctx))
    return std::fprintf(stderr, "template: %s\n", ctx.why.c_str()), 1;
  std::map<std::pair<int, int>, const Motion *> tracks;  // (y, x)
  for (auto &m : bank)
    if (m.id == act && !m.data.Empty())
      tracks[{m.y, m.x}] = &m;
  if (tracks.empty())
    return std::fprintf(stderr, "no track of act %d\n", act), 1;
  // the first role's pelvis by frame (the camera's target distance)
  std::map<int, V3> pel;
  for (auto &[yx, m] : tracks)
    if (yx.first == 0) {
      std::vector<BodyKey> bk;
      if (!BodyKeys(m->data, T, bk, ctx))
        return std::fprintf(stderr, "role 0: %s\n", ctx.why.c_str()), 1;
      for (auto &k : bk)
        pel[k.frame] = {10 * k.root[0] * kRootUnit, 10 * k.root[1] * kRootUnit, 10 * k.root[2] * kRootUnit};
      break;
    }
  std::vector<BlockOut> blocks;
  for (auto &[yx, m] : tracks) {
    const int y = yx.first, x = yx.second;
    BlockOut b;
    char name[16];
    if (y < 60) {
      std::vector<BodyKey> keys;
      if (!BodyKeys(m->data, T, keys, ctx))
        return std::fprintf(stderr, "role %d: %s\n", y, ctx.why.c_str()), 1;
      std::map<int, std::vector<Span>> fe;
      for (auto &k : keys)
        for (auto &ev : k.events)
          fe[k.frame].push_back(ev);
      if (!BodyBlock(tpl, keys, 2 * int(m->frames) + 1, {}, b.raw, ctx))
        return std::fprintf(stderr, "role %d body: %s\n", y, ctx.why.c_str()), 1;
      b.events = ConvertEvents(fe, kChar);
      std::snprintf(name, sizeof(name), "%04d%04d", x, y);
    } else {
      const int cam = y == 66 ? 62 : y;
      std::vector<CamKey> keys;
      std::map<int, std::vector<Span>> fe;
      if (!CamKeys(m->data, T, keys, fe, ctx))
        return std::fprintf(stderr, "track %d (camera?): %s\n", y, ctx.why.c_str()), 1;
      if (keys.empty())
        continue;
      if (!CameraBlock(keys, 2 * int(m->frames) + 1, pel, b.raw, ctx))
        return std::fprintf(stderr, "camera %d: %s\n", y, ctx.why.c_str()), 1;
      for (auto &t : b.raw.tracks)
        t.hash = Renumber(t.hash, cam);
      b.events = ConvertEvents(fe, kCamera);
      b.events.push_back({2 * int(m->frames), {{254, 0, {}}}});
      std::snprintf(name, sizeof(name), "%04d%04d", x, cam);
    }
    b.name = name;
    blocks.push_back(std::move(b));
  }
  // WWE '12's acts all have the cameras 60, 61 and 62 (SvR 2011's scenes 60, 61, 66 or only 66): a missing one is
  // a copy of another (60 first), renamed
  {
    const BlockOut *src = nullptr;
    for (const char *want : {"60", "61", "62"})
      for (auto &bo : blocks)
        if (!src && bo.name.size() == 8 && bo.name.substr(6) == want)
          src = &bo;
    if (src) {
      const BlockOut copy = *src;
      const int from = std::atoi(copy.name.substr(6).c_str());
      for (int cam : {60, 61, 62}) {
        char nm[16];
        std::snprintf(nm, sizeof(nm), "%s%04d", copy.name.substr(0, 4).c_str(), cam);
        bool have = false;
        for (auto &bo : blocks)
          have = have || bo.name == nm;
        if (have)
          continue;
        BlockOut c = copy;
        c.name = nm;
        for (auto &t : c.raw.tracks)  // back to "01", then to cam
          t.hash = Renumber(Unnumber(t.hash, from), cam);
        blocks.push_back(std::move(c));
      }
    }
  }
  // blocks in the game's order (by name)
  std::sort(blocks.begin(), blocks.end(), [](const BlockOut &a, const BlockOut &b) { return a.name < b.name; });
  Bytes rec = BuildRecord(tpl_head, blocks, ctx);
  if (!ctx.ok)
    return std::fprintf(stderr, "record: %s\n", ctx.why.c_str()), 1;
  // the act's number as text at +0x4C (as the game's)
  char num[16] = {};
  std::snprintf(num, sizeof(num), "%d", act);
  std::memcpy(&rec[0x4C], num, 16);
  std::ofstream(argv[4], std::ios::binary).write((const char *)rec.data(), std::streamsize(rec.size()));
  std::printf("act %d: %zu blocks, %zu bytes\n", act, blocks.size(), rec.size());
  return 0;
}
