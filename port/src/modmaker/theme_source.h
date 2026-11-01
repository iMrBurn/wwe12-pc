// Mod Maker: a superstar's entrance theme in another WWE game (its stream, for theme_convert.h to make PCM). After
// Kaiko's superstar_port.py (his SvR 2011 port, port/tools: GameW13 / GameW2K14-2K17 theme_events, Game09
// music_records, svr10_superstar.yaf_table), with his permission and credit:
//  - WWE '13, 2K14: Wwise, the event Play_MUS_<music>_0_0 (WWE '13 sound\Music.pck; 2K14 sound\Game_Sound_E_S.pck,
//    its streams in sound\Game_Sound_M.pck); music = the profile's entrance block (CHAR / PRO +576, u16);
//  - 2K15: +576 u32 = music id x 100 + variant: Play_MUS_<id>_<variant>_0 (sounds as 2K14);
//  - 2K16, 2K17: +588 u32 = music id x 1000 + variant: Play_MUS_<id>_<variant, 2 digits>_0_0 (lower-case pcks); their
//    PRO records by index, the superstar's number at CHAR / DAT +4 (216-byte records);
//  - SvR 2008-2010: sound\music*.yaf records (a table at 0x18 {count, align, version, record size, table offset},
//    records {size, offset, id}), id = music x 100, a RIFF XMA stream; music = PRO +0xE4 (2009, u16), +0x178 (2010),
//    the superstar's number (2008);
//  - the superstar's own number is tried as well (Kaiko's fallback).
//  - SvR 2011: Wwise (bank version 46), sound\Music.pck, Play_MUS_<number>_0_0;
//  - SvR 2007: XACT (Kaiko's xact.py): pac\sound.pac XACT/MUSI is a PACH of the sound bank (0x3E8, "KBDS",
//    big-endian) and its C header (0x3E9: XACT_SOUNDBANK_..._MUSIC_<music>_<part> = <cue>); a simple cue (5 bytes at
//    +0x1A, count +0x09: u8 flags, u32 sound) -> its sound's u16 wave at +9; the wave in music\DVD_Music.xwb ("DNBW":
//    5 segments, entries {u32 flags | samples << 4, u32 format (tag, channels, rate), u32 offset, u32 length}),
//    the longest of the superstar's cues, made a RIFF XMA2 for vgmstream.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "game_disc.h"
#include "svrfmt/archive.h"
#include "svrfmt/pac.h"
#include "svrfmt/xcompress.h"
#include "wwise.h"

namespace theme_source {

using Bytes = std::vector<uint8_t>;

struct Theme {
  Bytes stream;     // the theme's bytes
  std::string ext;  // what vgmstream should take it as: ".wem" (Wwise), ".xma" (RIFF XMA)
  std::string what; // where it came from ("Play_MUS_0246_0_0", "music1.yaf record 12300")
};

namespace detail {

inline uint32_t Le32(const Bytes &b, size_t o) {
  return o + 4 <= b.size() ? uint32_t(b[o]) | uint32_t(b[o + 1]) << 8 | uint32_t(b[o + 2]) << 16 | uint32_t(b[o + 3]) << 24 : 0;
}
inline uint16_t Le16(const Bytes &b, size_t o) { return o + 2 <= b.size() ? uint16_t(b[o] | b[o + 1] << 8) : 0; }

inline bool ReadPac(const disc::GameDisc &g, const std::string &path, Bytes &out) {
  if (!g.Read(path, out))
    return false;
  if (svrfmt::IsXcompress(out)) {
    Bytes o;
    if (!svrfmt::XcompressDecode(out, o))
      return false;
    out.swap(o);
  }
  return true;
}

// chEtc.pac's CHAR / <name> records by id (the 2K games: a PACH the archive may not open).
inline std::map<uint32_t, Bytes> Records(const disc::GameDisc &g, const char *name) {
  std::map<uint32_t, Bytes> out;
  Bytes c;
  svrfmt::Archive a;
  if (!ReadPac(g, "pac/ch/chEtc.pac", c) || !svrfmt::ArchiveOpen(c, a, nullptr))
    return out;
  for (auto &k : a.root.kids) {
    std::string g4 = k.group, n4 = k.name;
    while (!n4.empty() && (n4.back() == ' ' || n4.back() == '\0'))
      n4.pop_back();
    if (g4 != "CHAR" || n4 != name)
      continue;
    if (svrfmt::ArchiveExpand(k) && !k.kids.empty()) {
      for (auto &c2 : k.kids)
        out[c2.id] = svrfmt::Unpack(svrfmt::ArchiveData(c2));
    } else {
      std::vector<svrfmt::PachEntry> kids;
      if (svrfmt::PachRead(svrfmt::Unpack(svrfmt::ArchiveData(k)), kids))
        for (auto &e : kids)
          out[e.id] = svrfmt::Unpack(e.data);
    }
  }
  return out;
}

// A Wwise theme: the first of `events` the package plays (2K14-2K17: events in one package, streams in another).
inline bool Wwise(const disc::GameDisc &g, const std::string &events_pck, const std::string &streams_pck,
                  const std::vector<std::string> &events, Theme &out) {
  wwise::Package ev, st;
  if (!ev.Open([&g, events_pck](uint64_t at, uint32_t size, Bytes &o) { return g.ReadPart(events_pck, at, size, o); }))
    return false;
  const bool two = !streams_pck.empty() && streams_pck != events_pck;
  if (two && !st.Open([&g, streams_pck](uint64_t at, uint32_t size, Bytes &o) { return g.ReadPart(streams_pck, at, size, o); }))
    return false;
  const wwise::Package &streams = two ? st : ev;
  const auto objs = wwise::AllObjects(ev);
  for (const auto &e : events) {
    const uint32_t sid = wwise::EventStream(objs, streams, e);
    Bytes b;
    if (sid && streams.Data(streams.streams.at(sid), b) && b.size() > 12) {
      out = {std::move(b), ".wem", e};
      return true;
    }
  }
  return false;
}

// A SvR 2008-2010 theme: sound\music*.yaf record `id`.
inline bool Yaf(const disc::GameDisc &g, const std::vector<int> &ids, Theme &out) {
  for (const auto &f : g.List("sound")) {
    std::string low = f;
    for (auto &c : low)
      c = char(tolower(static_cast<unsigned char>(c)));
    if (low.rfind("music", 0) != 0 || low.size() < 9 || low.compare(low.size() - 4, 4, ".yaf"))
      continue;
    const std::string path = "sound/" + f;
    Bytes h;
    if (!g.ReadPart(path, 0, 0x40, h) || h.size() < 0x2C)
      continue;
    const uint32_t n = Le32(h, 0x18), rs = Le32(h, 0x24), to = Le32(h, 0x28);
    if (!n || n > 100000 || rs < 12 || rs > 64)
      continue;
    Bytes t;
    if (!g.ReadPart(path, to, n * rs, t))
      continue;
    for (int id : ids)
      for (uint32_t i = 0; i < n; ++i) {
        const size_t r = size_t(i) * rs;
        if (Le32(t, r + 8) != uint32_t(id))
          continue;
        Bytes b;
        if (g.ReadPart(path, Le32(t, r + 4), Le32(t, r), b) && b.size() > 12) {  // ({size, offset, id})
          out = {std::move(b), ".xma", f + " record " + std::to_string(id)};
          return true;
        }
      }
  }
  return false;
}

inline uint32_t Be32(const Bytes &b, size_t o) {
  return o + 4 <= b.size() ? uint32_t(b[o]) << 24 | uint32_t(b[o + 1]) << 16 | uint32_t(b[o + 2]) << 8 | b[o + 3] : 0;
}
inline uint16_t Be16(const Bytes &b, size_t o) { return o + 2 <= b.size() ? uint16_t(b[o] << 8 | b[o + 1]) : 0; }
inline void PutLe(Bytes &b, uint32_t v, int n) {
  for (int i = 0; i < n; ++i)
    b.push_back(uint8_t(v >> (8 * i)));
}

// A SvR 2007 theme (XACT): the longest wave of the cues MUSIC_<number>_<part>.
inline bool Xact(const disc::GameDisc &g, int number, Theme &out) {
  Bytes pac;
  svrfmt::Archive a;
  if (!ReadPac(g, "pac/sound.pac", pac) || !svrfmt::ArchiveOpen(pac, a, nullptr))
    return false;
  Bytes xsb, hdr;
  for (auto &k : a.root.kids) {
    std::string n4 = k.name;
    while (!n4.empty() && (n4.back() == ' ' || n4.back() == '\0'))
      n4.pop_back();
    if (k.group != "XACT" || n4 != "MUSI")
      continue;
    std::vector<svrfmt::PachEntry> kids;
    if (svrfmt::PachRead(svrfmt::Unpack(svrfmt::ArchiveData(k)), kids))
      for (auto &e : kids)
        (e.id == 0x3E8 ? xsb : e.id == 0x3E9 ? hdr : e.data) = svrfmt::Unpack(e.data);
  }
  if (xsb.size() < 0x20 || memcmp(xsb.data(), "KBDS", 4) || hdr.empty())
    return false;
  // the cue numbers of MUSIC_<number>_<part> in the header
  char want[24];
  snprintf(want, sizeof(want), "_MUSIC_%04d_", number);
  std::vector<uint32_t> cues;
  const std::string text(hdr.begin(), hdr.end());
  for (size_t at = text.find(want); at != std::string::npos; at = text.find(want, at + 1)) {
    const size_t eq = text.find('=', at), nl = text.find('\n', at);
    if (eq != std::string::npos && (nl == std::string::npos || eq < nl))
      cues.push_back(uint32_t(atoi(text.c_str() + eq + 1)));
  }
  const uint16_t nsimple = Be16(xsb, 0x09);
  const uint32_t simple = Be32(xsb, 0x1A);
  std::vector<uint16_t> waves;
  for (uint32_t c : cues) {
    if (c >= nsimple)
      continue;
    const uint32_t so = Be32(xsb, simple + 5 * size_t(c) + 1);
    if (so + 12 <= xsb.size() && !(xsb[so] & 1))
      waves.push_back(Be16(xsb, so + 9));
  }
  // the wave bank
  const std::string xwb = "music/DVD_Music.xwb";
  Bytes h;
  if (waves.empty() || !g.ReadPart(xwb, 0, 0x40, h) || h.size() < 0x30 || memcmp(h.data(), "DNBW", 4))
    return false;
  uint32_t seg[5][2];
  for (int i = 0; i < 5; ++i)
    seg[i][0] = Be32(h, 8 + 8 * size_t(i)), seg[i][1] = Be32(h, 12 + 8 * size_t(i));
  Bytes bd;
  if (!g.ReadPart(xwb, seg[0][0], 76, bd))
    return false;
  const uint32_t esize = Be32(bd, 72);
  uint32_t best_samples = 0;
  Bytes best_e;
  for (uint16_t w : waves) {
    Bytes e;
    if (!g.ReadPart(xwb, seg[1][0] + uint64_t(w) * esize, 16, e) || (Be32(e, 4) & 3) != 1)
      continue;
    if ((Be32(e, 0) >> 4) > best_samples)
      best_samples = Be32(e, 0) >> 4, best_e = e;
  }
  if (best_e.empty())
    return false;
  const uint32_t fmt = Be32(best_e, 4), ch = (fmt >> 2) & 7, rate = (fmt >> 5) & 0x3FFFF;
  Bytes data;
  if (!g.ReadPart(xwb, uint64_t(seg[4][0]) + Be32(best_e, 8), Be32(best_e, 12), data))
    return false;
  // RIFF WAVE, XMA2 format (0x166), as xact.py writes it
  const uint32_t block = 0x8000;
  Bytes f;
  PutLe(f, 0x166, 2), PutLe(f, ch, 2), PutLe(f, rate, 4), PutLe(f, rate * ch * 2, 4), PutLe(f, 4, 2), PutLe(f, 16, 2);
  PutLe(f, 34, 2);
  PutLe(f, (ch + 1) / 2, 2), PutLe(f, ch == 2 ? 3 : 4, 4), PutLe(f, best_samples, 4), PutLe(f, block, 4), PutLe(f, 0, 4);
  PutLe(f, best_samples, 4), PutLe(f, 0, 4), PutLe(f, 0, 4), PutLe(f, 0, 1), PutLe(f, 4, 1);
  PutLe(f, uint32_t((data.size() + block - 1) / block), 2);
  Bytes r = {'R', 'I', 'F', 'F'};
  PutLe(r, uint32_t(4 + 8 + f.size() + 8 + data.size()), 4);
  r.insert(r.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
  PutLe(r, uint32_t(f.size()), 4);
  r.insert(r.end(), f.begin(), f.end());
  r.insert(r.end(), {'d', 'a', 't', 'a'});
  PutLe(r, uint32_t(data.size()), 4);
  r.insert(r.end(), data.begin(), data.end());
  out = {std::move(r), ".xma", "XACT MUSIC_" + std::to_string(number)};
  return true;
}

}  // namespace detail

// The theme of superstar `number` in game `title` (its disc `g`). False if not found (or a game not read yet).
inline bool Find(const disc::GameDisc &g, uint32_t title, int number, Theme &out) {
  using namespace detail;
  char e[48];
  std::vector<std::string> events;
  auto add = [&](const char *fmt, int a, int b = 0) {
    snprintf(e, sizeof(e), fmt, a, b);
    events.push_back(e);
  };
  switch (title) {
    case 0x545108B4:  // WWE '13
    case 0x545408B2: {  // WWE 2K14
      const auto pro = Records(g, "PRO");
      const auto it = pro.find(uint32_t(number));
      const int music = it != pro.end() ? Le16(it->second, 576) : 0;
      if (music)
        add("Play_MUS_%04d_0_0", music);
      add("Play_MUS_%04d_0_0", number);
      return title == 0x545108B4 ? Wwise(g, "sound/Music.pck", "", events, out)
                                 : Wwise(g, "sound/Game_Sound_E_S.pck", "sound/Game_Sound_M.pck", events, out);
    }
    case 0x545408B6: {  // WWE 2K15
      const auto pro = Records(g, "PRO");
      const auto it = pro.find(uint32_t(number));
      const uint32_t music = it != pro.end() ? Le32(it->second, 576) : 0;
      if (music >= 100 && music < 100000)
        add("Play_MUS_%04d_%d_0", int(music / 100), int(music % 100));
      add("Play_MUS_%04d_0_0", number);
      return Wwise(g, "sound/Game_Sound_E_S.pck", "sound/Game_Sound_M.pck", events, out);
    }
    case 0x315A07D2:    // WWE 2K16
    case 0x545408BA: {  // WWE 2K17
      // PRO by index; the superstar's number at DAT +4 of the same index
      const auto dat = Records(g, "DAT"), pro = Records(g, "PRO");
      uint32_t music = 0;
      for (const auto &[i, d] : dat)
        if (Le16(d, 4) == number && pro.count(i))
          music = Le32(pro.at(i), 588);
      if (music >= 1000 && music < 1000000)
        add("Play_MUS_%04d_%02d_0_0", int(music / 1000), int(music % 1000));
      add("Play_MUS_%04d_00_0_0", number);
      return Wwise(g, "sound/game_sound_e_s.pck", "sound/game_sound_m.pck", events, out);
    }
    case 0x5451080B:    // SvR 2008
    case 0x54510826:    // SvR 2009
    case 0x54510844: {  // SvR 2010
      std::vector<int> ids;
      if (title != 0x5451080B) {
        const auto pro = Records(g, "PRO");
        const auto it = pro.find(uint32_t(number));
        const int music = it != pro.end() ? Le16(it->second, title == 0x54510826 ? 0xE4 : 0x178) : 0;
        if (music)
          ids.push_back(music * 100);
      }
      ids.push_back(number * 100);
      return Yaf(g, ids, out);
    }
    case 0x5451085D:  // SvR 2011
      add("Play_MUS_%04d_0_0", number);
      return Wwise(g, "sound/Music.pck", "", events, out);
    case 0x545107E0:  // SvR 2007
      return Xact(g, number, out);
    default:
      return false;
  }
}

}  // namespace theme_source
