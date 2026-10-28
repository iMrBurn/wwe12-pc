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
//  - its moves WWE '12 hasn't (a WWE '13 superstar's, the Mod Maker's newmoves13.h), added as the game's own packs 3
//    and 4 add moves: their records in a copy of misc.pac (MOVS/WAZE: 160-byte records in name order after a 0x430
//    header whose u16 at +4 counts them and at 0x08 + 2k counts those with category bit k set), their motions in
//    m_dlcNN.pac ZMOT/CA(NN + 6, at least CA11; the game loads every CA00..CA19) (CA09 and CA10 in packs 3 and 4: a top id, then pairs of children, light and full
//    records, by motion number); moves the latest misc.pac has already (an installed DLC's) are left out;
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
#include "svrfmt/pac.h"
#include "svrfmt/texture.h"

namespace superstar_pack {

using svrfmt::ArcNode;
using svrfmt::Bytes;

// A move to add (newmoves13.h): its number (as CHAR / PRO holds it), its WAZE record, its motion's two records.
struct NewMove {
  int number = 0;
  Bytes waze, low, high;
  Bytes magg;  // its two RGRP/MAGG records (children 10 and 11 of DLC.pac's, 104 bytes each), else empty
};

struct Star {
  std::string key;   // the mod's folder name
  std::string name;  // shown in the game (Latin-1 is written; UTF-8 is converted)
  int base = 0;      // the superstar it starts from (record, moves, portraits)
  int slot = 0;      // its number in the game
  Bytes model;       // a chNNN.pac (EPK8, "EMD " entries "000NNNAA"): the mod's or the base's
  Bytes movie;       // its own entrance video (Bink 320 x 320: titantron on top, minitron below), else empty
  int movie_number = 0;  // ... as movies\titantron\<number>.bik (a number no other video has)
  // Its own attributes (0-100, the record's +4..+15, again at +16..+27: STRIKE POWER, GRAPPLE POWER, SUBMISSION,
  // STRIKE DEFENSE, GRAPPLE DEFENSE, SPEED, AGILITY, ADRENALINE, RECOVERY, DURABILITY, CHARISMA, TAG TEAM, the game's
  // own names in that order), else empty (the base's).
  std::vector<uint8_t> attributes;
  // Its own moves over its base's (CHAR / PRO byte offset, move number, big-endian there: the Mod Maker's
  // moveset_port.h), else empty.
  std::vector<std::pair<int, int>> moves;
  // Another superstar's entrance (CHAR / PRO +0x1D4..+0x1DE, again at +0x1E0..+0x1EA: music, video and entrance
  // numbers, two more, 16-bit little-endian), else 0: the base's.
  int entrance = 0;
  // Its own entrance (a WWE '13 one the game serves under that number: wwe12_arena_pages.cpp), else 0: CHAR / PRO
  // +0x1D8 and +0x1E4, the entrance number of both blocks (music, video, entrance).
  int own_entrance = 0;
  // Its moves WWE '12 hasn't (added to the pack: misc.pac records, m_dlcNN.pac motions), else empty.
  std::vector<NewMove> new_moves;
  // Its own portraits, else empty (the base's): [0 SD / 1 HD][SSFA, SSFB, SSFC], DDS as the game's (DXT5; HD 512 x 512,
  // 512 x 512, 256 x 256; SD 512 x 256, 512 x 256, 256 x 256: WWE '13's are the same).
  Bytes portraits[2][3];
};

// An attire mod's attire (attire_mod.h): put into our pack as the game's superstar's own entry, so the pack's
// directory (newer than the disc's) gives it: an attire replaced, or one more (2026-10-26: a superstar file of the
// disc rebuilt and served whole kept the disc's directory: an added attire wasn't offered).
struct Attire {
  std::string key;    // the mod's folder name
  int superstar = 0;  // a superstar of the game (his file pac/ch/chNNN.pac)
  std::string entry;  // "000NNNAK"
  Bytes file;         // attire/attire.pac: a superstar file holding only that attire
  // One the superstar hasn't (a new attire): listed as a DLC attire too (the select screen offers a superstar's attires
  // from those lists, not from his file: an added one in the file alone wasn't offered, 2026-10-26). The lists' rows
  // (DLC.pac IGST/VOL, info/catalog.dlc) of the game's DLC attires (The Miz, Hawk, Animal): kind 2, the superstar,
  // the attire's index (0-3).
  bool added = false;
};

// A media mod's portraits for a superstar the game has (type=media, media/portraits/NNN/SSFA_HD.dds ...): in our pack's
// DLC_HD / DLC_SD pacs under his own number, found before the disc's (menu\menuHD.pac, DLC_HD.pac).
struct MediaPortraits {
  std::string key;    // the mod's folder name
  int superstar = 0;  // a superstar of the game
  Bytes dds[2][3];    // [0 SD / 1 HD][SSFA, SSFB, SSFC], DDS as the game's; empty = the game's own
};

// A crowd sign (type=signs, signs/NNN_K.dds: superstar NNN, sign K 1-4): the crowd holds up a superstar's signs while
// he's in the match. The game keeps them in pac\audience.pac AUDE/BORD, a PACH of texture bundles (one 128 x 64 DXT1
// picture "no01" each) whose child ids are superstar x 10 + K (1611-1614: Randy Orton's). Our pack brings that entry
// again with the signs replaced or added (its directory comes before the disc's), and the added superstars get their
// bases' signs.
struct Sign {
  std::string key;  // the mod's folder name
  int id = 0;       // superstar x 10 + K
  Bytes dds;        // 128 x 64 DXT1, as the game's
};

// A main menu background (type=background, backgrounds/NN/menu.dds, title.dds): the main menu shows MnLayHD.pac MLYE/EM00
// (EL00 in some regions), a layout whose pictures are the superstar beside the menu (ortontex02, 1024 x 1024, cut out)
// and on the title screen (ortontex01, 512 x 1024). The menu layouts are read whole at startup, not through the packs'
// directories (EM01 in our pack: a black screen, 2026-10-26): one background is drawn at each start and EM00 / EL00's
// pictures replaced in a copy of MnLayHD.pac / MnLaySD.pac, in their own format and size, so every entry keeps its
// place (src/native/wwe12_superstar_mods.cpp serves it).
struct Background {
  std::string key;  // the mod's folder name
  Bytes menu;       // DDS for ortontex02 (the main menu)
  Bytes title;      // DDS for ortontex01 (the title screen), else empty (the game's)
};

// MnLayHD.pac / MnLaySD.pac with EM00 / EL00 wearing this background, false if the file couldn't keep its size.
inline bool DressMenu(Bytes &mnlay, const Background &bg) {
  svrfmt::Archive m;
  if (!svrfmt::ArchiveOpen(mnlay, m, nullptr))
    return false;
  bool any = false;
  for (auto &k : m.root.kids) {
    if (k.group != "MLYE" || (k.name != "EM00" && k.name != "EL00") || !svrfmt::ArchiveExpand(k))
      continue;
    for (auto &c : k.kids) {
      std::vector<svrfmt::BundleTexture> t;
      if (c.id != 7001 || !svrfmt::BundleRead(svrfmt::ArchiveData(c), t))
        continue;
      for (auto &x : t) {
        const Bytes &mine = x.name == "ortontex02" ? bg.menu : x.name == "ortontex01" ? bg.title : Bytes();
        svrfmt::Image img;
        svrfmt::DdsInfo want;
        if (mine.empty() || !svrfmt::DdsInfoOf(x.data, want) || !svrfmt::DdsDecode(mine, img))
          continue;
        if (img.w != want.w || img.h != want.h)
          img = svrfmt::Resize(img, want.w, want.h);
        x.data = svrfmt::DdsEncode(img, want.format, want.mips > 1);
      }
      svrfmt::ArchiveReplace(c, svrfmt::BundleWrite(t));
      any = true;
    }
    svrfmt::ArchiveTouch(k);
  }
  if (!any)
    return false;
  svrfmt::ArchiveTouch(m.root);
  const Bytes out = svrfmt::ArchiveSave(m);
  if (out.size() != mnlay.size())
    return false;
  mnlay = out;
  return true;
}

struct Inputs {
  Bytes chetc, dlc;                                // the game's latest pac\ch\chEtc.pac and pac\DLC.pac
  Bytes audience;                                  // ... and pac\audience.pac (the crowd's signs)
  Bytes misc;                                      // ... and pac\misc.pac (the move records), when moves are added
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
  // The disc's empty records first: the save has them ready (unlocked, set up). A DLC slot is set up by the game only
  // when a pack it hasn't seen brings it: one added to our pack later stayed locked, and set unlocked by hand it went
  // to a black screen at the match (FBX TEST at 86, 90, 91, 2026-10-26; at 104, CHRIS JERICHO played at once).
  for (int id = 100; id < 373; ++id)
    if (blank(id))
      out.push_back(id);
  for (int id = 85; id < 100; ++id)
    if (blank(id))
      out.push_back(id);
  for (int id = 50; id < 85; ++id)
    if (blank(id))
      out.push_back(id);
  return out;
}

// The pack's number: after the installed packs' (the game reads IGST/VOL0..VOL9), and never one of the game's four
// packs: the disc's DLC.pac lists VOL1 and VOL2 already, and with no pack installed ours was 1, its list in place of
// VOL1, and its superstars showed nowhere (the select screen's DLC tile empty, 2026-10-28).
inline int PackNumber(const Inputs &in) {
  int n = 0;
  for (const auto &[number, cat] : in.catalogs)
    n = std::max(n, number);
  return std::max(5, n + 1);
}

// The pack's files for `stars` (slots set). False (and why) when it can't be made.
inline bool Build(const Inputs &in, const std::vector<Star> &stars, std::vector<File> &files, std::string *why,
                  const std::vector<Attire> &attires = {}, const std::vector<MediaPortraits> &media = {},
                  const std::vector<Sign> &signs = {}) {
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
  // The rows of the lists: the added superstars (kind 1), then the new attires (kind 2).
  struct Row {
    int kind, id, sub;
    std::string name;
  };
  std::vector<Row> rows;
  for (const auto &st : stars)
    rows.push_back({1, st.slot, 0, st.name.empty() ? st.key : st.name});
  {
    std::set<std::string> listed;
    for (const auto &at : attires)
      if (at.added && listed.insert(at.entry).second)
        rows.push_back({2, at.superstar, at.entry[6] - '0', "ATTIRE " + at.entry});
  }
  if (free_codes.size() < 2 * rows.size()) {
    free_codes.clear();
    for (char second = 'U'; second <= 'Z' && free_codes.size() < 2 * rows.size(); ++second)
      for (char first = 'A'; first <= 'Z' && free_codes.size() < 2 * rows.size(); ++first) {
        const std::string c{first, second};
        if (!codes.count(c))
          free_codes.push_back(c);
      }
  }
  if (free_codes.size() < 2 * rows.size())
    return fail("no codes left");
  char vol_name[8];
  snprintf(vol_name, sizeof(vol_name), "VOL%d", pack);
  if (ArcNode *old = Find(dlc.root, "IGST", vol_name))
    old->name = "XXXX";  // (a stale one of ours: replaced below)
  ArcNode *vol = svrfmt::ArchiveAdd(dlc.root, "IGST", vol_name, 0, model_vol->stored, &e);
  if (!vol || !svrfmt::ArchiveExpand(*vol) || vol->kids.empty())
    return fail("can't add " + std::string(vol_name) + ": " + e);
  Bytes list(4 + 52 * rows.size(), 0);
  Le32(list, 0, uint32_t(rows.size()));
  for (size_t i = 0; i < rows.size(); ++i) {
    const size_t o = 4 + 52 * i;
    Le32(list, o, 1);
    Le32(list, o + 4, uint32_t(rows[i].kind));
    Le32(list, o + 8, uint32_t(rows[i].id));
    Le32(list, o + 12, uint32_t(rows[i].sub));
    Le32(list, o + 16, uint32_t(serial + 1 + int(i)));
    memcpy(&list[o + 20], free_codes[2 * i].data(), 2);
    memcpy(&list[o + 24], free_codes[2 * i + 1].data(), 2);
    Le32(list, o + 28, 1);
    Le32(list, o + 32, 2);
    char key[17];
    if (rows[i].kind == 1)
      snprintf(key, sizeof(key), "WWE12PCMOD%06d", rows[i].id);
    else
      snprintf(key, sizeof(key), "WWE12PCATT%04d%d%d", rows[i].id % 10000, rows[i].sub, 2);
    memcpy(&list[o + 36], key, 16);
  }
  svrfmt::ArchiveReplace(vol->kids[0], list);
  svrfmt::ArchiveTouch(*vol);
  // The added moves' RGRP/MAGG records (children 10 and 11: 104-byte records in motion number order, u16 motion
  // little-endian at +0, name at +8; the game's pack 4 added one per move), those it hasn't.
  for (auto &k : dlc.root.kids) {
    if (k.group != "RGRP")
      continue;
    std::vector<svrfmt::PachEntry> kids;
    if (!svrfmt::PachRead(svrfmt::Unpack(svrfmt::ArchiveData(k)), kids))
      break;
    bool changed = false;
    for (auto &kid : kids) {
      if (kid.id != 10 && kid.id != 11)
        continue;
      Bytes d = svrfmt::Unpack(kid.data);
      const size_t which = kid.id == 10 ? 0 : 104;
      for (const auto &st : stars)
        for (const auto &m : st.new_moves) {
          if (m.magg.size() != 208)
            continue;
          const int motion = m.magg[which] | m.magg[which + 1] << 8;
          size_t at = 0;
          bool have = false;
          for (; at + 104 <= d.size(); at += 104) {
            const int x = d[at] | d[at + 1] << 8;
            have = x == motion;
            if (x >= motion)
              break;
          }
          if (have)
            continue;
          d.insert(d.begin() + long(std::min(at, d.size())), m.magg.begin() + long(which), m.magg.begin() + long(which) + 104);
          changed = true;
        }
      if (changed)
        kid.data = svrfmt::IsBpe(kid.data) ? svrfmt::BpeEncode(d) : d;
    }
    if (changed) {
      svrfmt::ArchiveReplace(k, svrfmt::PachWrite(kids));
      svrfmt::ArchiveTouch(k);
    }
    break;
  }
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
    if (st.attributes.size() == 12)
      for (size_t i = 0; i < 12; ++i)
        d[4 + i] = d[16 + i] = uint8_t(std::min<int>(st.attributes[i], 100));
    memcpy(&d[0xF0], &own[0xF0], 8);
    d[0x11F] = 1;  // a DLC superstar (the select screen's DLC tile; a disc superstar's copy had 0 and showed nowhere)
    while (person > 0 && person_used[size_t(person)])
      --person;
    person_used[size_t(person)] = true;
    Le16(d, 0x100, uint16_t(person));
    svrfmt::ArchiveReplace(*s, d);
    Bytes moves = svrfmt::ArchiveData(*bp);
    for (const auto &[at, id] : st.moves)
      if (at >= 0x00A && at < 0x1C2 && (at & 1) == 0 && size_t(at) + 1 < moves.size()) {  // (the moves, not the entrance)
        moves[size_t(at)] = uint8_t(id >> 8);
        moves[size_t(at) + 1] = uint8_t(id);
      }
    // The entrance block is there twice, +0x1D4 and +0x1E0 (equal on the disc's records); the game plays the second
    // (the first changed alone: the base's entrance, music and video, 2026-10-28). Both written.
    if (ArcNode *ep = st.entrance > 0 ? Child(*pro, uint32_t(st.entrance)) : nullptr) {
      const Bytes e = svrfmt::ArchiveData(*ep);
      for (const size_t at : {size_t(0x1D4), size_t(0x1E0)})
        if (e.size() >= at + 10 && moves.size() >= at + 10)
          std::copy(e.begin() + long(at), e.begin() + long(at) + 10, moves.begin() + long(at));
    }
    if (st.own_entrance > 0)
      for (const size_t at : {size_t(0x1D8), size_t(0x1E4)})
        if (moves.size() >= at + 2) {
          moves[at] = uint8_t(st.own_entrance);
          moves[at + 1] = uint8_t(st.own_entrance >> 8);
        }
    svrfmt::ArchiveReplace(*sp, moves);
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
  // 3b. The attire mods' attires, one file per superstar (the first mod giving an attire wins).
  std::map<int, Bytes> attire_files;
  {
    std::map<int, svrfmt::Archive> by_star;
    std::set<std::string> given;
    for (const auto &at : attires) {
      if (!given.insert(at.entry).second)
        continue;
      svrfmt::Archive one;
      if (!svrfmt::ArchiveOpen(at.file, one, &e) || one.root.kids.empty())
        return fail(at.key + ": attire unreadable: " + e);
      auto it = by_star.find(at.superstar);
      if (it == by_star.end()) {
        one.root.kids.resize(1);
        if (!svrfmt::ArchiveRename(one.root, 0, at.entry, &e))
          return fail(at.key + ": " + e);
        by_star.emplace(at.superstar, std::move(one));
      } else if (!svrfmt::ArchiveAdd(it->second.root, "EMD ", at.entry, 0, svrfmt::ArchiveData(one.root.kids[0]), &e)) {
        return fail(at.key + ": " + e);
      }
    }
    for (auto &[superstar, a] : by_star) {
      svrfmt::ArchiveTouch(a.root);
      attire_files[superstar] = svrfmt::ArchiveSave(a);
    }
  }
  // 3c. The crowd's signs: the signs mods' and the added superstars' (their bases').
  Bytes audience_out;
  if (!in.audience.empty() && (!signs.empty() || !stars.empty())) {
    svrfmt::Archive aud;
    ArcNode *bord = nullptr;
    if (svrfmt::ArchiveOpen(in.audience, aud, nullptr))
      bord = Find(aud.root, "AUDE", "BORD");
    ArcNode *pack_node = bord && svrfmt::ArchiveExpand(*bord) && !bord->kids.empty() ? &bord->kids[0] : nullptr;
    if (pack_node && svrfmt::ArchiveExpand(*pack_node)) {
      auto child = [&](uint32_t id) -> ArcNode * {
        for (auto &k : pack_node->kids)
          if (k.id == id)
            return &k;
        return nullptr;
      };
      bool changed = false;
      // a sign's bundle: the one of that id (its picture replaced), else a copy of another one with that picture
      auto put = [&](uint32_t id, const Bytes &dds) {
        std::vector<svrfmt::BundleTexture> t;
        ArcNode *k = child(id);
        ArcNode *like = k ? k : pack_node->kids.empty() ? nullptr : &pack_node->kids[0];
        if (!like || !svrfmt::BundleRead(svrfmt::ArchiveData(*like), t) || t.empty())
          return;
        t[0].data = dds;
        const Bytes raw = svrfmt::BundleWrite(t);
        if (k)
          svrfmt::ArchiveReplace(*k, raw);
        else if (!svrfmt::ArchiveAdd(*pack_node, "", "", id, raw, nullptr))
          return;
        changed = true;
      };
      std::set<uint32_t> given;
      for (const auto &sg : signs)
        if (sg.id > 0 && given.insert(uint32_t(sg.id)).second)
          put(uint32_t(sg.id), sg.dds);
      for (const auto &st : stars)
        for (uint32_t k = 1; k <= 4; ++k) {
          const uint32_t mine = uint32_t(st.slot) * 10 + k, theirs = uint32_t(st.base) * 10 + k;
          ArcNode *b = child(theirs);
          std::vector<svrfmt::BundleTexture> t;
          if (given.count(mine) || child(mine) || !b || !svrfmt::BundleRead(svrfmt::ArchiveData(*b), t) || t.empty())
            continue;
          put(mine, t[0].data);
        }
      if (changed) {
        svrfmt::ArchiveTouch(*pack_node);
        svrfmt::ArchiveTouch(*bord);
        svrfmt::ArchiveTouch(aud.root);
        audience_out = svrfmt::ArchiveSave(aud);
      }
    }
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
    // The media mods' portraits of the game's superstars (the first mod giving one wins).
    std::set<std::string> media_given;
    for (const auto &mp : media) {
      if (!have_template) {
        svrfmt::Archive p;
        if (sources.empty() || !svrfmt::ArchiveOpen(sources.front(), p, nullptr))
          break;
        out_pac = p;
        for (size_t i = out_pac.root.kids.size(); i-- > 0;)
          svrfmt::ArchiveRemove(out_pac.root, i);
        have_template = true;
      }
      char name[8];
      snprintf(name, sizeof(name), "%04d", mp.superstar);
      for (int k = 0; k < 3; ++k) {
        const std::string group = std::string("SSF") + char('A' + k);
        if (mp.dds[hd][k].empty() || !media_given.insert(group + name).second || Find(out_pac.root, group.c_str(), name))
          continue;
        if (!svrfmt::ArchiveAdd(out_pac.root, group, name, 0, svrfmt::BpeEncode(mp.dds[hd][k]), &e))
          return fail(mp.key + ": portrait: " + e);
      }
    }
    if (have_template && !out_pac.root.kids.empty()) {
      svrfmt::ArchiveTouch(out_pac.root);
      portraits[hd] = svrfmt::ArchiveSave(out_pac);
    }
  }
  // 4b. Moves to add: the records into a copy of misc.pac, the motions into m_dlcNN.pac.
  Bytes misc_out, moves_out;
  {
    std::vector<const NewMove *> adds;
    std::set<int> taken;
    for (const auto &st : stars)
      for (const auto &m : st.new_moves)
        if (m.waze.size() == 160 && !m.low.empty() && !m.high.empty() && taken.insert(m.number).second)
          adds.push_back(&m);
    svrfmt::Archive misc;
    ArcNode *waze = nullptr;
    if (!adds.empty() && svrfmt::ArchiveOpen(in.misc, misc, nullptr))
      for (auto &k : misc.root.kids)
        if (k.group == "MOVS" && k.name.rfind("WAZE", 0) == 0)
          waze = &k;
    if (waze) {
      Bytes t = svrfmt::ArchiveData(*waze);
      // the records it has (by number) and where the named ones are
      std::set<int> have;
      for (size_t o = 0x430; o + 160 <= t.size(); o += 160)
        if (t[o + 0x10])
          have.insert(t[o + 0x90] << 8 | t[o + 0x91]);
      auto name_at = [&](size_t o) { return std::string(reinterpret_cast<const char *>(&t[o + 0x10]), strnlen(reinterpret_cast<const char *>(&t[o + 0x10]), 0x40)); };
      auto lower = [](std::string x) {
        for (auto &c : x)
          c = char(std::tolower(static_cast<unsigned char>(c)));
        return x;
      };
      std::vector<const NewMove *> added;
      for (const NewMove *m : adds) {
        if (have.count(m->number) || t.size() < 0x430)
          continue;  // (the game has it: an installed DLC's move)
        const std::string name = lower(std::string(reinterpret_cast<const char *>(&m->waze[0x10]), strnlen(reinterpret_cast<const char *>(&m->waze[0x10]), 0x40)));
        size_t at = 0x430;  // (in name order, as the game's packs insert theirs)
        while (at + 160 <= t.size() && t[at + 0x10] && lower(name_at(at)) < name)
          at += 160;
        t.insert(t.begin() + long(at), m->waze.begin(), m->waze.end());
        Le16(t, 4, uint16_t((t[4] | t[5] << 8) + 1));
        for (int bit = 0; bit < 128; ++bit)
          if ((m->waze[size_t(bit / 8)] >> (bit % 8)) & 1) {
            const size_t c = 0x08 + 2 * size_t(bit);
            Le16(t, c, uint16_t((t[c] | t[c + 1] << 8) + 1));
          }
        have.insert(m->number);
        added.push_back(m);
      }
      if (!added.empty()) {
        svrfmt::ArchiveReplace(*waze, t);
        svrfmt::ArchiveTouch(misc.root);
        misc_out = svrfmt::ArchiveSave(misc);
        // m_dlcNN.pac: ZMOT/CA(NN + 6), {top: {2i: {motion: light}, 2i + 1: {motion: full}}}, in chEtc.pac's kind of pac
        std::vector<svrfmt::PachEntry> pairs;
        for (const NewMove *m : added) {
          const uint32_t motion = uint32_t(((m->number & 0xFF) << 8) | (m->number >> 8));
          pairs.push_back({uint32_t(pairs.size()), svrfmt::PachWrite({{motion, m->low}})});
          pairs.push_back({uint32_t(pairs.size()), svrfmt::PachWrite({{motion, m->high}})});
        }
        const Bytes tree = svrfmt::PachWrite({{uint32_t(180 + pack), svrfmt::PachWrite(pairs)}});
        svrfmt::Epac like, out;
        char bank[8];
        // (the game loads every /ZMOT/CA00..CA19 that exists, wherever it is (sub_822B65xx, the loop at 0x822B665C):
        // the disc has CA01-CA08, the game's packs 3 and 4 CA09 and CA10; ours never below CA11, whatever its number)
        snprintf(bank, sizeof(bank), "CA%02d", std::min(19, std::max(pack, 5) + 6));
        if (svrfmt::EpacRead(in.chetc, like) && !like.groups.empty()) {
          out.header = like.header;
          out.epk8 = like.epk8;
          out.wide = like.wide;
          out.toc_extra = like.toc_extra;
          svrfmt::EpacGroup g;
          g.type = "ZMOT";
          g.number = like.groups[0].number;
          g.extra = 0;
          g.entries.push_back({bank, tree, 0});
          out.groups.push_back(std::move(g));
          moves_out = svrfmt::EpacWrite(out);
        }
        if (moves_out.empty())
          misc_out.clear();  // (records without their motions: neither)
      }
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
  for (const auto &[superstar, file] : attire_files) {
    entries.push_back({"pac/ch/ch" + std::to_string(superstar) + "_dlc" + tag + ".pac", next, next, &file});
    ++next;
  }
  if (!audience_out.empty()) {
    entries.push_back({"pac/audience_dlc" + std::string(tag) + ".pac", next, next, &audience_out});
    ++next;
  }
  if (!moves_out.empty()) {
    entries.push_back({"pac/m_dlc" + std::string(tag) + ".pac", next, next, &moves_out});
    ++next;
    entries.push_back({"pac/misc.pac", next, next, &misc_out});
    ++next;
  }
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
  Bytes cat(0x18 + 0x40 * rows.size(), 0);
  memcpy(cat.data(), "DLCC", 4);
  Le16(cat, 4, 2);
  Le16(cat, 6, 0x18);
  Le32(cat, 8, uint32_t(pack));
  Le16(cat, 12, uint16_t(rows.size()));
  Le16(cat, 14, uint16_t(entries.size()));
  for (size_t i = 0; i < rows.size(); ++i) {
    const size_t r = 0x18 + 0x40 * i;
    Le16(cat, r, 0x40);
    Le16(cat, r + 2, uint16_t(i + 1));
    Le16(cat, r + 4, uint16_t(rows[i].kind));
    Le16(cat, r + 6, uint16_t(pack));
    Le16(cat, r + 8, uint16_t(i));
    Le16(cat, r + 10, uint16_t(i));
    Le32(cat, r + 0x10, uint32_t(rows[i].id));
    Le32(cat, r + 0x14, uint32_t(rows[i].sub));
    const std::string name = Latin1(rows[i].name).substr(0, 31);
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
