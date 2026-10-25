// Added superstars: the DLC pack the game makes of the superstar mods at startup (docs/mod_maker.md, "Ajouter des
// catcheurs"; src/native/wwe12_superstar_mods.cpp). The game already adds superstars through its DLC packs, each
// filling one of its 373 superstar records (pac\ch\chEtc.pac: 0-49 created, 50-99 DLC slots, 100-372 disc): a pack
// with a higher number than the installed ones brings its own copy of chEtc.pac and DLC.pac, the superstars' models
// and portraits, a list of its files (info/catalog.dlc) and directories of their entries (info/catalog_hd/sd.arc).
// Each added superstar takes a free slot (a "DLC 36" or blank "0" record) and gets:
//  - its record: its base superstar's (ratings, style...) with its own number (+0x2C, +0xF8, +0x102) and name (+0x2E,
//    +0x72, +0xB6), the slot's own four codes (+0xF0, = CHAR/DSBD), a "person" number nobody uses (+0x100: the
//    select screen shows one tile per person, alternate costumes share their superstar's: copies of Batista hid
//    each other) and the DLC flag (+0x11F: the select screen's DLC tile, whatever the base);
//  - its base's moves (CHAR/PRO);
//  - its model (a chNNN.pac, its attires "000NNNAA" renamed to the slot) and its portraits (SSFA/B/C: its own, else
//    the base's);
//  - its own entrance video if it has one (movies\titantron\NNN.bik, listed in the catalog without a directory, as
//    the game's packs do; the game plays it in place of its base's: wwe12_superstar_mods.cpp);
//  - a line in the pack's catalog and in its superstar list (DLC.pac IGST/VOL<pack>: codes "lettre + U" that follow
//    the installed packs', the next serial numbers).
// Nothing of the game's goes into a mod: the pack is made from the player's own files.
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "svrfmt/archive.h"

namespace superstar_pack {

using svrfmt::ArcNode;
using svrfmt::Bytes;

struct Star {
  std::string key;   // the mod's folder name
  std::string name;  // shown in the game (Latin-1 is written; UTF-8 is converted)
  int base = 0;      // the superstar it starts from (record, moves, portraits)
  int slot = 0;      // its number in the game
  Bytes model;       // a chNNN.pac (EPK8, "EMD " entries "000NNNAA"): the mod's or the base's
  Bytes movie;       // its own entrance video (Bink 320 x 320: titantron on top, minitron below), else empty
  int movie_number = 0;  // ... as movies\titantron\<number>.bik (a number no other video has)
  // Its own portraits, else empty (the base's): [0 SD / 1 HD][SSFA, SSFB, SSFC], DDS as the game's (DXT5; HD 512 x 512,
  // 512 x 512, 256 x 256; SD 512 x 256, 512 x 256, 256 x 256: WWE '13's are the same).
  Bytes portraits[2][3];
};

struct Inputs {
  Bytes chetc, dlc;                                // the game's latest pac\ch\chEtc.pac and pac\DLC.pac
  std::vector<Bytes> portraits_hd, portraits_sd;   // pacs that may hold the bases' SSFA/SSFB/SSFC "%04d"
  std::vector<std::pair<int, Bytes>> catalogs;     // the installed packs (number, info/catalog.dlc), not ours
};

struct File {
  std::string path;  // in the pack ("pac/ch/chEtc.pac", "info/catalog.dlc"...)
  Bytes data;
};

namespace detail {

inline void Le16(Bytes &b, size_t o, uint16_t v) {
  b[o] = uint8_t(v);
  b[o + 1] = uint8_t(v >> 8);
}
inline void Le32(Bytes &b, size_t o, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    b[o + i] = uint8_t(v >> (8 * i));
}
inline uint32_t Rd32(const Bytes &b, size_t o) {
  return o + 4 <= b.size() ? uint32_t(b[o]) | uint32_t(b[o + 1]) << 8 | uint32_t(b[o + 2]) << 16 | uint32_t(b[o + 3]) << 24
                           : 0;
}
inline ArcNode *Find(ArcNode &r, const std::string &g, const std::string &n) {
  for (auto &k : r.kids)
    if (k.group == g && k.name == n)
      return &k;
  return nullptr;
}
inline ArcNode *Child(ArcNode &n, uint32_t id) {
  for (auto &k : n.kids)
    if (k.id == id)
      return &k;
  return nullptr;
}
// UTF-8 -> Latin-1 (the game's names), capitals, at most 31 characters.
inline std::string Latin1(const std::string &s) {
  std::string out;
  for (size_t i = 0; i < s.size() && out.size() < 31; ++i) {
    unsigned char c = s[i];
    if (c < 0x80) {
      out += char(std::toupper(c));
    } else if ((c & 0xE0) == 0xC0 && i + 1 < s.size()) {
      const unsigned v = (c & 0x1F) << 6 | (s[i + 1] & 0x3F);
      ++i;
      if (v < 0x100)
        out += char(v >= 0xE0 && v <= 0xFE && v != 0xF7 ? v - 0x20 : v);
    }
  }
  return out;
}
// A record's name (+0x2E), "" if unreadable.
inline std::string NameOf(const Bytes &rec) {
  return rec.size() > 0x2E ? std::string(reinterpret_cast<const char *>(&rec[0x2E]), strnlen(reinterpret_cast<const char *>(&rec[0x2E]), 40)) : "";
}
// The superstar ids a catalog lists (its lines: +0x10).
inline std::set<int> CatalogIds(const Bytes &cat) {
  std::set<int> ids;
  if (cat.size() < 0x18 || memcmp(cat.data(), "DLCC", 4) != 0)
    return ids;
  const int rows = cat[12] | cat[13] << 8;
  for (int i = 0; i < rows && 0x18 + 0x40 * size_t(i + 1) <= cat.size(); ++i)
    ids.insert(int(Rd32(cat, 0x18 + 0x40 * size_t(i) + 0x10)));
  return ids;
}
// The EPAC group table at 0x800 (little-endian) in the directory's form (big-endian), for file `index`.
inline void ArcAppend(Bytes &arc, const Bytes &f, int index) {
  if (index == 0) {
    const uint8_t m[4] = {0xFF, 0, 0, 0};
    arc.insert(arc.end(), m, m + 4);
  } else {
    arc.push_back(0xFF);
    arc.push_back(0xFF);
    arc.push_back(uint8_t(index >> 8));
    arc.push_back(uint8_t(index));
  }
  size_t o = 0x800;
  while (o + 12 <= f.size() && f[o]) {
    const uint16_t words = uint16_t(f[o + 4] | f[o + 5] << 8), wide = uint16_t(f[o + 6] | f[o + 7] << 8);
    arc.insert(arc.end(), f.begin() + long(o), f.begin() + long(o) + 4);
    const uint16_t a = uint16_t(words << 4), b = uint16_t(wide << 12);
    arc.push_back(uint8_t(a >> 8));
    arc.push_back(uint8_t(a));
    arc.push_back(uint8_t(b >> 8));
    arc.push_back(uint8_t(b));
    for (int i = 0; i < 4; ++i)
      arc.push_back(0);
    o += 12;
    const size_t name = wide ? 8 : 4, n = words * 4 / (name + 8);
    for (size_t e = 0; e < n && o + name + 8 <= f.size(); ++e, o += name + 8) {
      arc.insert(arc.end(), f.begin() + long(o), f.begin() + long(o + name));
      for (int q : {0, 4})
        for (int i = 3; i >= 0; --i)
          arc.push_back(f[o + name + size_t(q) + size_t(i)]);
    }
  }
}

}  // namespace detail

// Free slots, in the order they are given: the DLC ones (85-99, then the others), then the disc's blank records.
inline std::vector<int> FreeSlots(const Inputs &in) {
  std::vector<int> out;
  svrfmt::Archive etc;
  if (!svrfmt::ArchiveOpen(in.chetc, etc, nullptr))
    return out;
  ArcNode *dat = detail::Find(etc.root, "CHAR", "DAT ");
  if (!dat || !svrfmt::ArchiveExpand(*dat))
    return out;
  std::set<int> listed;
  for (const auto &[n, cat] : in.catalogs)
    for (int id : detail::CatalogIds(cat))
      listed.insert(id);
  auto blank = [&](int id) {
    ArcNode *c = detail::Child(*dat, uint32_t(id));
    if (!c || listed.count(id))
      return false;
    const std::string n = detail::NameOf(svrfmt::ArchiveData(*c));
    if (id >= 50 && id < 100)
      return n.rfind("DLC ", 0) == 0;
    return !n.empty() && std::all_of(n.begin(), n.end(), [](char ch) { return ch >= '0' && ch <= '9'; });
  };
  for (int id = 85; id < 100; ++id)
    if (blank(id))
      out.push_back(id);
  for (int id = 50; id < 85; ++id)
    if (blank(id))
      out.push_back(id);
  for (int id = 100; id < 373; ++id)
    if (blank(id))
      out.push_back(id);
  return out;
}

// The pack's number: after the installed packs' (the game reads IGST/VOL0..VOL9).
inline int PackNumber(const Inputs &in) {
  int n = 0;
  for (const auto &[number, cat] : in.catalogs)
    n = std::max(n, number);
  return n + 1;
}

// The pack's files for `stars` (slots set). False (and why) when it can't be made.
inline bool Build(const Inputs &in, const std::vector<Star> &stars, std::vector<File> &files, std::string *why) {
  using namespace detail;
  auto fail = [&](const std::string &w) {
    if (why)
      *why = w;
    return false;
  };
  const int pack = PackNumber(in);
  if (pack > 9)
    return fail("no pack number left (the game reads 10)");
  char tag[8];
  snprintf(tag, sizeof(tag), "%02d", pack);
  std::string e;
  // 1. DLC.pac: the latest one and its superstar list IGST/VOL<pack>.
  svrfmt::Archive dlc;
  if (!svrfmt::ArchiveOpen(in.dlc, dlc, &e))
    return fail("DLC.pac: " + e);
  int serial = 0;
  std::set<std::string> codes;
  ArcNode *model_vol = nullptr;
  for (auto &k : dlc.root.kids) {
    if (k.group != "IGST" || k.name.rfind("VOL", 0) != 0 || !svrfmt::ArchiveExpand(k) || k.kids.empty())
      continue;
    model_vol = &k;
    const Bytes v = svrfmt::ArchiveData(k.kids[0]);
    const uint32_t count = Rd32(v, 0);
    for (uint32_t i = 0; i < count && 4 + 52 * size_t(i + 1) <= v.size(); ++i) {
      const size_t o = 4 + 52 * size_t(i);
      serial = std::max(serial, int(Rd32(v, o + 16)));
      codes.insert(std::string(reinterpret_cast<const char *>(&v[o + 20]), 2));
      codes.insert(std::string(reinterpret_cast<const char *>(&v[o + 24]), 2));
    }
  }
  if (!model_vol)
    return fail("DLC.pac has no superstar list");
  // The next free codes, "lettre + U" first (as the packs: AU, BU... VU), then V, W...
  std::vector<std::string> free_codes;
  for (char second = 'U'; second <= 'Z' && free_codes.size() < 2 * stars.size(); ++second)
    for (char first = 'A'; first <= 'Z' && free_codes.size() < 2 * stars.size(); ++first) {
      const std::string c{first, second};
      if (!codes.count(c))
        free_codes.push_back(c);
    }
  if (free_codes.size() < 2 * stars.size())
    return fail("no codes left");
  char vol_name[8];
  snprintf(vol_name, sizeof(vol_name), "VOL%d", pack);
  if (ArcNode *old = Find(dlc.root, "IGST", vol_name))
    old->name = "XXXX";  // (a stale one of ours: replaced below)
  ArcNode *vol = svrfmt::ArchiveAdd(dlc.root, "IGST", vol_name, 0, model_vol->stored, &e);
  if (!vol || !svrfmt::ArchiveExpand(*vol) || vol->kids.empty())
    return fail("can't add " + std::string(vol_name) + ": " + e);
  Bytes list(4 + 52 * stars.size(), 0);
  Le32(list, 0, uint32_t(stars.size()));
  for (size_t i = 0; i < stars.size(); ++i) {
    const size_t o = 4 + 52 * i;
    Le32(list, o, 1);
    Le32(list, o + 4, 1);
    Le32(list, o + 8, uint32_t(stars[i].slot));
    Le32(list, o + 16, uint32_t(serial + 1 + int(i)));
    memcpy(&list[o + 20], free_codes[2 * i].data(), 2);
    memcpy(&list[o + 24], free_codes[2 * i + 1].data(), 2);
    Le32(list, o + 28, 1);
    Le32(list, o + 32, 2);
    char key[17];
    snprintf(key, sizeof(key), "WWE12PCMOD%06d", stars[i].slot);
    memcpy(&list[o + 36], key, 16);
  }
  svrfmt::ArchiveReplace(vol->kids[0], list);
  svrfmt::ArchiveTouch(*vol);
  svrfmt::ArchiveTouch(dlc.root);
  const Bytes dlc_out = svrfmt::ArchiveSave(dlc);
  // 2. chEtc.pac: the records, moves and codes.
  svrfmt::Archive etc;
  if (!svrfmt::ArchiveOpen(in.chetc, etc, &e))
    return fail("chEtc.pac: " + e);
  ArcNode *dat = Find(etc.root, "CHAR", "DAT "), *pro = Find(etc.root, "CHAR", "PRO ");
  if (!dat || !pro || !svrfmt::ArchiveExpand(*dat) || !svrfmt::ArchiveExpand(*pro))
    return fail("chEtc.pac has no superstar records");
  std::vector<bool> person_used(0x10000, false);
  for (auto &c : dat->kids) {
    const Bytes d = svrfmt::ArchiveData(c);
    if (d.size() > 0x101)
      person_used[d[0x100] | d[0x101] << 8] = true;
  }
  int person = 0x0433;
  for (const auto &st : stars) {
    ArcNode *b = Child(*dat, uint32_t(st.base)), *s = Child(*dat, uint32_t(st.slot));
    ArcNode *bp = Child(*pro, uint32_t(st.base)), *sp = Child(*pro, uint32_t(st.slot));
    if (!b || !s || !bp || !sp)
      return fail("no record for superstar " + std::to_string(st.base) + " or slot " + std::to_string(st.slot));
    Bytes d = svrfmt::ArchiveData(*b);
    const Bytes own = svrfmt::ArchiveData(*s);
    if (d.size() < 0x122 || own.size() < 0x122)
      return fail("short superstar record");
    Le16(d, 0x2C, uint16_t(st.slot));
    Le16(d, 0xF8, uint16_t(st.slot));
    Le16(d, 0x102, uint16_t(st.slot));
    const std::string name = Latin1(st.name.empty() ? st.key : st.name);
    for (size_t at : {size_t(0x2E), size_t(0x72), size_t(0xB6)}) {
      memset(&d[at], 0, 32);
      memcpy(&d[at], name.data(), name.size());
    }
    memcpy(&d[0xF0], &own[0xF0], 8);
    d[0x11F] = 1;  // a DLC superstar (the select screen's DLC tile; a disc superstar's copy had 0 and showed nowhere)
    while (person > 0 && person_used[size_t(person)])
      --person;
    person_used[size_t(person)] = true;
    Le16(d, 0x100, uint16_t(person));
    svrfmt::ArchiveReplace(*s, d);
    svrfmt::ArchiveReplace(*sp, svrfmt::ArchiveData(*bp));
  }
  svrfmt::ArchiveTouch(*dat);
  svrfmt::ArchiveTouch(*pro);
  svrfmt::ArchiveTouch(etc.root);
  const Bytes etc_out = svrfmt::ArchiveSave(etc);
  // 3. The models, their attires renamed to the slot.
  std::vector<Bytes> models;
  for (const auto &st : stars) {
    svrfmt::Archive ch;
    if (!svrfmt::ArchiveOpen(st.model, ch, &e))
      return fail(st.key + ": model unreadable: " + e);
    char num[8];
    snprintf(num, sizeof(num), "%03d", st.slot);
    for (size_t i = 0; i < ch.root.kids.size(); ++i) {
      std::string n = ch.root.kids[i].name;
      if (n.size() != 8)
        return fail(st.key + ": not a superstar file");
      n.replace(3, 3, num);
      if (!svrfmt::ArchiveRename(ch.root, i, n, &e))
        return fail(st.key + ": " + e);
    }
    svrfmt::ArchiveTouch(ch.root);
    models.push_back(svrfmt::ArchiveSave(ch));
  }
  // 4. Portraits: the bases' SSFA/SSFB/SSFC under the slots' numbers (taken from the first pac that has them).
  Bytes portraits[2];
  for (int hd = 0; hd < 2; ++hd) {
    const auto &sources = hd ? in.portraits_hd : in.portraits_sd;
    svrfmt::Archive out_pac;
    bool have_template = false;
    for (const auto &st : stars) {
      char base_name[8], slot_name[8];
      snprintf(base_name, sizeof(base_name), "%04d", st.base);
      snprintf(slot_name, sizeof(slot_name), "%04d", st.slot);
      for (const auto &src : sources) {
        svrfmt::Archive p;
        if (!svrfmt::ArchiveOpen(src, p, nullptr) || !Find(p.root, "SSFA", base_name))
          continue;
        std::vector<ArcNode> entries;
        for (auto &k : p.root.kids)
          if (k.group.rfind("SSF", 0) == 0 && k.name == base_name)
            entries.push_back(k);
        if (!have_template) {  // (a pac of the same kind, emptied)
          out_pac = p;
          for (size_t i = out_pac.root.kids.size(); i-- > 0;)
            svrfmt::ArchiveRemove(out_pac.root, i);
          have_template = true;
        }
        for (const auto &k : entries) {
          // Its own picture if it has one (a DDS, packed as the game's are), else the base's.
          const int which = k.group.size() == 4 ? k.group[3] - 'A' : -1;
          const Bytes &own = which >= 0 && which < 3 ? st.portraits[hd][which] : Bytes();
          if (!svrfmt::ArchiveAdd(out_pac.root, k.group, slot_name, 0, own.empty() ? k.stored : svrfmt::BpeEncode(own),
                                  &e))
            return fail(st.key + ": portrait: " + e);
        }
        break;
      }
    }
    if (have_template && !out_pac.root.kids.empty()) {
      svrfmt::ArchiveTouch(out_pac.root);
      portraits[hd] = svrfmt::ArchiveSave(out_pac);
    }
  }
  // 5. The files, their directories and the catalog.
  struct Entry {
    std::string path;
    int hd, sd;
    const Bytes *data;
  };
  // The videos first, as in the game's packs: listed, with no directory (no index).
  std::vector<Entry> entries;
  for (const auto &st : stars)
    if (!st.movie.empty() && st.movie_number > 0 && st.movie_number < 1000) {
      char movie[40];
      snprintf(movie, sizeof(movie), "movies/titantron/%03d.bik", st.movie_number);
      entries.push_back({movie, -1, -1, &st.movie});
    }
  entries.push_back({"pac/DLC.pac", 0, 0, &dlc_out});
  int next = 1;
  if (!portraits[1].empty())
    entries.push_back({"pac/DLC_HD_dlc" + std::string(tag) + ".pac", next, -1, &portraits[1]});
  if (!portraits[0].empty())
    entries.push_back({"pac/DLC_SD_dlc" + std::string(tag) + ".pac", -1, next, &portraits[0]});
  if (!portraits[0].empty() || !portraits[1].empty())
    ++next;
  for (size_t i = 0; i < stars.size(); ++i, ++next)
    entries.push_back({"pac/ch/ch" + std::to_string(stars[i].slot) + "_dlc" + tag + ".pac", next, next, &models[i]});
  entries.push_back({"pac/ch/chEtc.pac", next, next, &etc_out});
  for (int hd = 0; hd < 2; ++hd) {
    Bytes arc;
    for (const auto &en : entries) {
      const int index = hd ? en.hd : en.sd;
      if (index >= 0)
        ArcAppend(arc, *en.data, index);
    }
    for (int i = 0; i < 4; ++i)
      arc.push_back(0xFF);
    files.push_back({hd ? "info/catalog_hd.arc" : "info/catalog_sd.arc", std::move(arc)});
  }
  Bytes cat(0x18 + 0x40 * stars.size(), 0);
  memcpy(cat.data(), "DLCC", 4);
  Le16(cat, 4, 2);
  Le16(cat, 6, 0x18);
  Le32(cat, 8, uint32_t(pack));
  Le16(cat, 12, uint16_t(stars.size()));
  Le16(cat, 14, uint16_t(entries.size()));
  for (size_t i = 0; i < stars.size(); ++i) {
    const size_t r = 0x18 + 0x40 * i;
    Le16(cat, r, 0x40);
    Le16(cat, r + 2, uint16_t(i + 1));
    Le16(cat, r + 4, 1);
    Le16(cat, r + 6, uint16_t(pack));
    Le16(cat, r + 8, uint16_t(i));
    Le16(cat, r + 10, uint16_t(i));
    Le32(cat, r + 0x10, uint32_t(stars[i].slot));
    const std::string name = Latin1(stars[i].name.empty() ? stars[i].key : stars[i].name);
    memcpy(&cat[r + 0x20], name.data(), name.size());
  }
  for (const auto &en : entries) {
    std::string p = en.path;
    for (auto &c : p)
      if (c == '/')
        c = char(92);  // (the game's paths use backslashes)
    cat.push_back(uint8_t(7 + p.size()));
    cat.push_back(0);
    cat.push_back(uint8_t(en.hd));
    cat.push_back(uint8_t(en.hd >> 8));
    cat.push_back(uint8_t(en.sd));
    cat.push_back(uint8_t(en.sd >> 8));
    cat.push_back(uint8_t(p.size()));
    cat.insert(cat.end(), p.begin(), p.end());
  }
  files.push_back({"info/catalog.dlc", std::move(cat)});
  for (const auto &en : entries)
    files.push_back({en.path, *en.data});
  return true;
}

}  // namespace superstar_pack
