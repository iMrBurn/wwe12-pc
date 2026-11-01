// Wwise sound packages (.pck, big-endian Xbox 360) of WWE '12 and the games after it: an entrance theme read out of
// one (WWE '13's sound\Music.pck: ThemeStream) and the package a mods pack brings for its superstars' themes
// (MusicPackage). The reading follows Kaiko's wwise.py (his SvR 2011 port, port/tools), with his permission and credit.
//
// AKPK: header size, version, the sizes of the language map, the bank table and the stream table (WWE '12, '13: and an
// externals table), then those tables (entries {id, block size, [u32 0,] size, start block, language}: 24 bytes in
// WWE '12, 20 in WWE '13). A bank: chunks BKHD (version: 56 in WWE '12, 65 in WWE '13), DIDX / DATA (prefetched
// media), HIRC (objects {u8 type, u32 size, u32 id, body}), STID (its name). Ids are FNV-1 hashes of the lower-case
// names: an entrance theme is the event Play_MUS_<number>_0_0 (its victory Play_MUS_<number>_0_1), its play action
// (type 3) targets a random container (5) of sounds (2) whose source is a stream of the package (Wwise Vorbis, the
// same RIFX "fmt " in both games).
//
// The game (sub_82748120's loop) looks for <pack>:\sound\DLC_<NN>.pck in each DLC pack and loads its bank
// Music_DLC_<NN>; the entrance music it plays is Play_MUS_%ENTSSMUSICNO%_0_0 (sound.pac GEMD), the profile's music
// number. The latest sound_dlc_<NN>.pac also holds each DLC package's header (YAFH/PCKH, child 100 + NN: the name in
// 32 bytes, {1, size, header bytes, 0x800, 1, 0, 0}, then the package's bytes 8.. up to its header's end + 8).
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace wwise {

using Bytes = std::vector<uint8_t>;
using Reader = std::function<bool(uint64_t at, uint32_t size, Bytes &out)>;

inline uint32_t Fnv(const std::string &s) {
  uint32_t h = 2166136261u;
  for (char c : s) {
    h *= 16777619u;
    h ^= uint8_t(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  }
  return h;
}

inline uint32_t Be32(const Bytes &d, size_t o) {
  return o + 4 <= d.size() ? uint32_t(d[o]) << 24 | uint32_t(d[o + 1]) << 16 | uint32_t(d[o + 2]) << 8 | d[o + 3] : 0;
}
inline uint16_t Be16(const Bytes &d, size_t o) { return o + 2 <= d.size() ? uint16_t(d[o] << 8 | d[o + 1]) : 0; }
inline void Put32(Bytes &d, size_t o, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    d[o + size_t(i)] = uint8_t(v >> (24 - 8 * i));
}
inline void App32(Bytes &d, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    d.push_back(uint8_t(v >> (24 - 8 * i)));
}

struct Entry {
  uint32_t id = 0, block = 0, size = 0, start = 0, lang = 0;
};

struct Package {
  std::vector<Entry> banks;
  std::map<uint32_t, Entry> streams;
  Reader read;

  bool Open(Reader r) {
    read = std::move(r);
    Bytes h;
    if (!read(0, 0x20, h) || h.size() < 0x20 || memcmp(h.data(), "AKPK", 4))
      return false;
    const uint32_t hs = Be32(h, 4), lm = Be32(h, 12), bl = Be32(h, 16), sl = Be32(h, 20), ext = Be32(h, 24);
    if (hs > (64u << 20) || !read(0, hs + 8, h) || h.size() < hs + 8)
      return false;
    size_t p = (0x1C + lm + bl + sl + ext == hs + 8 ? 0x1C : 0x18) + lm;
    auto table = [&](size_t at, uint32_t size, auto &&add) {
      const uint32_t n = Be32(h, at);
      if (!n)
        return;
      const size_t w = size > 20 * n + 4 ? 24 : 20;
      for (uint32_t i = 0; i < n && at + 4 + w * (i + 1) <= h.size(); ++i) {
        const size_t e = at + 4 + w * i;
        Entry x;
        x.id = Be32(h, e), x.block = Be32(h, e + 4);
        x.size = Be32(h, e + (w == 24 ? 12 : 8)), x.start = Be32(h, e + (w == 24 ? 16 : 12));
        x.lang = Be32(h, e + (w == 24 ? 20 : 16));
        add(x);
      }
    };
    table(p, bl, [&](const Entry &e) { banks.push_back(e); });
    table(p + bl, sl, [&](const Entry &e) { streams[e.id] = e; });
    return true;
  }
  bool Data(const Entry &e, Bytes &out) const { return read(uint64_t(e.start) * e.block, e.size, out) && out.size() == e.size; }
};

struct Object {
  uint8_t type = 0;
  Bytes body;
};

// A bank's objects, by id; its version (BKHD).
inline std::map<uint32_t, Object> Objects(const Bytes &bank, uint32_t *version = nullptr) {
  std::map<uint32_t, Object> out;
  uint32_t ver = 46;
  for (size_t p = 0; p + 8 <= bank.size();) {
    const uint32_t len = Be32(bank, p + 4);
    if (!memcmp(&bank[p], "BKHD", 4))
      ver = Be32(bank, p + 8);
    else if (!memcmp(&bank[p], "HIRC", 4)) {
      const uint32_t n = Be32(bank, p + 8);
      size_t q = p + 12;
      for (uint32_t i = 0; i < n && q + 9 <= bank.size(); ++i) {
        Object o;
        uint32_t sz, id;
        size_t body;
        if (ver >= 56)
          o.type = bank[q], sz = Be32(bank, q + 1), id = Be32(bank, q + 5), body = q + 9, q += 5 + sz;
        else
          o.type = uint8_t(Be32(bank, q)), sz = Be32(bank, q + 4), id = Be32(bank, q + 8), body = q + 12, q += 8 + sz;
        if (q > bank.size() || sz < 4)
          break;
        o.body.assign(bank.begin() + long(body), bank.begin() + long(q));
        out[id] = std::move(o);
      }
    }
    p += 8 + len;
  }
  if (version)
    *version = ver;
  return out;
}

// All the package's banks' objects.
inline std::map<uint32_t, Object> AllObjects(const Package &pk, uint32_t *version = nullptr) {
  std::map<uint32_t, Object> all;
  for (const auto &b : pk.banks) {
    Bytes d;
    if (!pk.Data(b, d))
      continue;
    auto o = Objects(d, version);
    all.insert(o.begin(), o.end());
  }
  return all;
}

// The stream an event plays: its play action's target, then down the containers (not up: mixers and buses list every
// child of theirs) to a sound whose source is one of the package's streams. 0 if none.
inline uint32_t EventStream(const std::map<uint32_t, Object> &objs, const Package &pk, const std::string &event) {
  const auto ev = objs.find(Fnv(event));
  if (ev == objs.end() || ev->second.type != 4)
    return 0;
  const Bytes &e = ev->second.body;
  const uint32_t n = Be32(e, 0) <= 1000 ? Be32(e, 0) : e[0];
  const size_t first = Be32(e, 0) <= 1000 ? 4 : 1;
  for (uint32_t k = 0; k < n; ++k) {
    const auto a = objs.find(Be32(e, first + 4 * k));
    if (a == objs.end() || a->second.type != 3)
      continue;
    std::vector<uint32_t> stack;
    for (size_t o = 0; o + 4 <= std::min<size_t>(a->second.body.size(), 16) && stack.empty(); ++o)
      if (objs.count(Be32(a->second.body, o)))
        stack.push_back(Be32(a->second.body, o));
    std::set<uint32_t> seen;
    while (!stack.empty()) {
      const uint32_t id = stack.back();
      stack.pop_back();
      const auto it = objs.find(id);
      if (it == objs.end() || !seen.insert(id).second)
        continue;
      const Bytes &b = it->second.body;
      if (it->second.type == 2) {
        for (size_t off : {8, 20, 16, 12, 24, 5})
          if (pk.streams.count(Be32(b, off)))
            return Be32(b, off);
      } else if (it->second.type == 5 || it->second.type == 6 || it->second.type == 9) {  // containers
        std::vector<uint32_t> kids;
        for (size_t q = 0; q + 4 <= b.size(); ++q) {
          const uint32_t c = Be32(b, q);
          const auto k2 = objs.find(c);
          if (c != id && k2 != objs.end() && (k2->second.type == 2 || k2->second.type == 5))
            kids.push_back(c);
        }
        for (auto r = kids.rbegin(); r != kids.rend(); ++r)
          stack.push_back(*r);
      }
    }
  }
  return 0;
}

// The theme of entrance music number `number` (its stream, a Wwise Vorbis RIFX), from a package such as WWE '13's
// sound\Music.pck. Empty if it hasn't one.
inline Bytes ThemeStream(const Package &pk, int number, int victory = 0) {
  const auto objs = AllObjects(pk);
  char ev[40];
  snprintf(ev, sizeof(ev), "Play_MUS_%04d_0_%d", number, victory);
  const uint32_t sid = EventStream(objs, pk, ev);
  Bytes out;
  if (!sid || !pk.Data(pk.streams.at(sid), out) || out.size() < 12 || memcmp(out.data(), "RIFX", 4))
    out.clear();
  return out;
}

// ---- the mods pack's package

struct Theme {
  int number = 0;  // its music number (the profile's, +0x1D4)
  Bytes stream;    // Wwise Vorbis RIFX
};

// Package DLC_<pack>.pck for `themes`: one bank, Music_DLC_<pack>, events Play_MUS_<n>_0_0 and _0_1 each playing its
// stream, made after a theme of the game's own (WWE '12's sound\Music.pck: `game`, Cena's 139: its play action, random
// container, sound, their mixer and its parent) the way the game's DLC packages are made (DLC 4: Music_DLC_04). The
// sounds are streamed (type 1, as some of the game's): no prefetched media. `pckh`: the package's header record for
// sound_dlc_<pack>.pac. False (and why) if the game's theme isn't there to copy.
inline bool MusicPackage(const Package &game, int pack, const std::vector<Theme> &themes, Bytes &pck, Bytes &pckh,
                         std::string *why) {
  auto fail = [&](const char *w) {
    if (why)
      *why = w;
    return false;
  };
  uint32_t ver = 0;
  const auto objs = AllObjects(game, &ver);
  if (ver != 56)
    return fail("not WWE '12's sound bank version");
  const auto find = [&](uint32_t id, uint8_t type) -> const Object * {
    const auto it = objs.find(id);
    return it != objs.end() && it->second.type == type ? &it->second : nullptr;
  };
  // the game's chain: event -> action -> random container -> sound; the container's mixer and the mixer's parent
  const Object *ev = find(Fnv("Play_MUS_0139_0_0"), 4);
  if (!ev || Be32(ev->body, 0) != 1)
    return fail("no theme 139 to copy");
  const uint32_t act_id = Be32(ev->body, 4);
  const Object *act = find(act_id, 3);
  if (!act || act->body.size() < 12)
    return fail("no play action");
  const uint32_t rs_id = Be32(act->body, 4);
  const Object *rs = find(rs_id, 5);
  if (!rs)
    return fail("no random container");
  uint32_t snd_id = 0, mix_id = 0;
  for (size_t q = 0; q + 4 <= rs->body.size(); ++q) {
    const uint32_t c = Be32(rs->body, q);
    if (!snd_id && find(c, 2))
      snd_id = c;
    if (!mix_id && find(c, 7))
      mix_id = c;
  }
  const Object *snd = snd_id ? find(snd_id, 2) : nullptr, *mix = mix_id ? find(mix_id, 7) : nullptr;
  if (!snd || !mix || snd->body.size() < 32 || Be32(snd->body, 4) != 2)
    return fail("no sound or mixer");
  // the mixer's children list: the u32 count that ends the body with its ids
  size_t kids_at = 0;
  for (size_t q = 0; q + 4 <= mix->body.size(); ++q)
    if (q + 4 + 4 * size_t(Be32(mix->body, q)) == mix->body.size() && Be32(mix->body, q) > 0) {
      kids_at = q;
      break;
    }
  if (!kids_at)
    return fail("no mixer children list");
  uint32_t parent_id = 0;
  for (size_t q = 0; q + 4 <= kids_at && !parent_id; ++q)
    if (find(Be32(mix->body, q), 19))
      parent_id = Be32(mix->body, q);
  const Object *parent = parent_id ? find(parent_id, 19) : nullptr;

  char name[32];
  snprintf(name, sizeof(name), "Music_DLC_%02d", pack);
  const uint32_t bank_id = Fnv(name);
  auto replace = [](Bytes b, uint32_t from, uint32_t to) {
    for (size_t q = 0; q + 4 <= b.size(); ++q)
      if (Be32(b, q) == from)
        Put32(b, q, to);
    return b;
  };
  struct Obj {
    uint8_t type;
    uint32_t id;
    Bytes body;
  };
  std::vector<Obj> list, tail;
  std::vector<uint32_t> containers;
  std::vector<std::pair<uint32_t, const Bytes *>> streams;
  for (const auto &t : themes) {
    if (t.stream.empty() || t.number <= 0 || t.number > 9999)
      continue;
    char s[48];
    snprintf(s, sizeof(s), "wwe12pc_mus_%04d", t.number);
    const uint32_t sid = Fnv(s);
    snprintf(s, sizeof(s), "wwe12pc_mus_%04d_sound", t.number);
    const uint32_t new_snd = Fnv(s);
    snprintf(s, sizeof(s), "wwe12pc_mus_%04d_container", t.number);
    const uint32_t new_rs = Fnv(s);
    streams.push_back({sid, &t.stream});
    // the sound: streamed (type 1: no file offset nor in-memory size), its source the stream, its parent the container
    Bytes sb(snd->body.begin(), snd->body.begin() + 16);
    Put32(sb, 4, 1);
    Put32(sb, 8, sid);
    Put32(sb, 12, sid);
    sb.insert(sb.end(), snd->body.begin() + 24, snd->body.end());
    list.push_back({2, new_snd, replace(sb, rs_id, new_rs)});
    list.push_back({5, new_rs, replace(rs->body, snd_id, new_snd)});
    containers.push_back(new_rs);
    for (int v = 0; v < 2; ++v) {
      snprintf(s, sizeof(s), "wwe12pc_mus_%04d_action_%d", t.number, v);
      const uint32_t new_act = Fnv(s);
      Bytes ab = replace(act->body, rs_id, new_rs);
      Put32(ab, ab.size() - 4, bank_id);  // (its bank: the target's)
      tail.push_back({3, new_act, ab});
      snprintf(s, sizeof(s), "Play_MUS_%04d_0_%d", t.number, v);
      Bytes eb;
      App32(eb, 1);
      App32(eb, new_act);
      tail.push_back({4, Fnv(s), eb});
    }
  }
  if (streams.empty())
    return fail("no theme");
  std::sort(containers.begin(), containers.end());
  Bytes mb(mix->body.begin(), mix->body.begin() + long(kids_at));
  App32(mb, uint32_t(containers.size()));
  for (uint32_t c : containers)
    App32(mb, c);
  list.push_back({7, mix_id, mb});
  if (parent)
    list.push_back({19, parent_id, parent->body});
  list.insert(list.end(), tail.begin(), tail.end());

  // the bank
  Bytes bank = {'B', 'K', 'H', 'D'};
  App32(bank, 20);
  App32(bank, 56);
  App32(bank, bank_id);
  App32(bank, 0);
  App32(bank, 0);
  App32(bank, 0);
  Bytes hirc;
  App32(hirc, uint32_t(list.size()));
  for (const auto &o : list) {
    hirc.push_back(o.type);
    App32(hirc, uint32_t(o.body.size() + 4));
    App32(hirc, o.id);
    hirc.insert(hirc.end(), o.body.begin(), o.body.end());
  }
  bank.insert(bank.end(), {'H', 'I', 'R', 'C'});
  App32(bank, uint32_t(hirc.size()));
  bank.insert(bank.end(), hirc.begin(), hirc.end());
  bank.insert(bank.end(), {'S', 'T', 'I', 'D'});
  const size_t nl = strlen(name);
  App32(bank, uint32_t(13 + nl));
  App32(bank, 1);
  App32(bank, 1);
  App32(bank, bank_id);
  bank.push_back(uint8_t(nl));
  bank.insert(bank.end(), name, name + nl);

  // the package: header, then the bank and the streams at 2 KB blocks
  std::sort(streams.begin(), streams.end());
  Bytes lang;
  App32(lang, 2);
  App32(lang, 0x14);
  App32(lang, 1);
  App32(lang, 0x20);
  App32(lang, 0);
  for (const char *s : {"english(us)", "sfx"})
    lang.insert(lang.end(), s, s + strlen(s) + 1);
  const uint32_t lm = uint32_t(lang.size()), bl = 4 + 24, sl = uint32_t(4 + 24 * streams.size());
  const uint32_t hs = 0x10 + lm + bl + sl;
  auto blocks = [](size_t n) { return uint32_t((n + 0x7FF) / 0x800); };
  uint32_t at = blocks(hs + 16);
  pck = {'A', 'K', 'P', 'K'};
  App32(pck, hs);
  App32(pck, 1);
  App32(pck, lm);
  App32(pck, bl);
  App32(pck, sl);
  pck.insert(pck.end(), lang.begin(), lang.end());
  auto entry = [&](uint32_t id, size_t size) {
    App32(pck, id);
    App32(pck, 0x800);
    App32(pck, 0);
    App32(pck, uint32_t(size));
    App32(pck, at);
    App32(pck, 0);
    at += blocks(size);
  };
  App32(pck, 1);
  entry(bank_id, bank.size());
  App32(pck, uint32_t(streams.size()));
  for (const auto &[id, data] : streams)
    entry(id, data->size());
  pck.resize(size_t(blocks(hs + 16)) * 0x800, 0);
  auto put = [&](const Bytes &d) {
    pck.insert(pck.end(), d.begin(), d.end());
    pck.resize(size_t(blocks(pck.size())) * 0x800, 0);
  };
  put(bank);
  for (const auto &[id, data] : streams)
    put(*data);
  // its header record
  const uint32_t len = hs + 8;
  pckh.assign(32, 0);
  snprintf(reinterpret_cast<char *>(pckh.data()), 32, "DLC_%02d.pck", pack);
  for (uint32_t v : {1u, 0x3Cu + len, len, 0x800u, 1u, 0u, 0u})
    App32(pckh, v);
  pckh.insert(pckh.end(), pck.begin() + 8, pck.begin() + 8 + long(len));
  return true;
}

}  // namespace wwise
