// Mod Maker: an added superstar's own arena screens during its entrance, besides its titantron (docs/plan_movesets.md):
// WWE '12 shows, for the 85 superstars that have them, pac\evt\evtmov.pac's MVSB/M<video number> (the ribbon around
// the arena, its name: Bink 1, 1024 x 32) and MVSW/M<video number> (the wall, a 256 x 256 sheet the arena cuts up);
// the others, the arena's. A mod's superstar\ribbon.bik and wall.bik are served by the game under its own video number
// (wwe12_superstar_mods.cpp, wwe12_arena_pages.cpp).
// Made here from: its game's (Versions: the 2K games' banner, 256 x 128, laid along the ribbon again and again at the
// ribbon's height; their wall, a sheet of their own, as it is), or a picture of the player's (a still movie).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <filesystem>
#include <string>
#include <vector>

#include "game_disc.h"
#include "svrfmt/archive.h"
#include "svrfmt/pac.h"
#include "svrfmt/xcompress.h"
#include "wic_image.h"

extern "C" {
#include "video/bink2_decode.h"
#include "video/bink_decode.h"
#include "video/bink_encode.h"
}

namespace entrance_screens {

namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;

constexpr int kRibbonW = 1024, kRibbonH = 32, kWall = 256;

// An archive's entries (xcompressed or not) as {group, name, data}, names and groups trimmed.
struct Entry {
  std::string group, name;
  Bytes data;
};
inline std::vector<Entry> Entries(Bytes b) {
  if (svrfmt::IsXcompress(b)) {
    Bytes o;
    if (!svrfmt::XcompressDecode(b, o))
      return {};
    b.swap(o);
  }
  svrfmt::Archive a;
  if (!svrfmt::ArchiveOpen(b, a, nullptr))
    return {};
  std::vector<Entry> out;
  for (const auto &k : a.root.kids) {
    Entry e{k.group, k.name, {}};
    for (auto *t : {&e.group, &e.name})
      while (!t->empty() && (t->back() == ' ' || !t->back()))
        t->pop_back();
    e.data = svrfmt::Unpack(svrfmt::ArchiveData(k));
    out.push_back(std::move(e));
  }
  return out;
}

// A version of a superstar's ribbon or wall in its game: its label ("M139", "014600") and the movie as stored (Bink 1
// or 2).
struct Version {
  std::string label;
  Bytes movie;
};

// Every version of superstar `number`'s ribbon (MVSB: its banner) or wall (MVSW) in game `title`:
//  - WWE '13 and 2K14: pac\evt\evtmov.pac MVSB / MVSW "M%03d" (WWE '13's are Bink 2 "KB2d");
//  - 2K15: pac\evt\evtmov_ss_mini.pac (MVSB) / evtmov_ss_wall.pac (MVSW), "M%04d%02d" (number, version);
//  - 2K16 / 2K17: pac\mov\ss_mini\ / ss_wall\%04d%02d.pac, one movie each;
//  - SvR 2011 has none.
inline std::vector<Version> Versions(const disc::GameDisc &g, uint32_t title, bool ribbon, int number) {
  std::vector<Version> out;
  const char *group = ribbon ? "MVSB" : "MVSW";
  Bytes b;
  if (title == 0x545108B4 || title == 0x545408B2) {
    char want[8];
    snprintf(want, sizeof(want), "M%03d", number % 1000);
    if (g.Read("pac/evt/evtmov.pac", b))
      for (auto &e : Entries(std::move(b)))
        if (e.group == group && e.name == want)
          out.push_back({e.name, std::move(e.data)});
  } else if (title == 0x545408B6) {
    char want[8];
    snprintf(want, sizeof(want), "M%04d", number % 10000);
    if (g.Read(ribbon ? "pac/evt/evtmov_ss_mini.pac" : "pac/evt/evtmov_ss_wall.pac", b))
      for (auto &e : Entries(std::move(b)))
        if (e.group == group && e.name.size() == 7 && e.name.compare(0, 5, want) == 0)
          out.push_back({e.name, std::move(e.data)});
  } else if (title == 0x315A07D2 || title == 0x545408BA) {
    const std::string dir = ribbon ? "pac/mov/ss_mini" : "pac/mov/ss_wall";
    char want[8];
    snprintf(want, sizeof(want), "%04d", number % 10000);
    std::vector<std::string> files = g.List(dir);
    std::sort(files.begin(), files.end());
    for (const auto &f : files)
      if (f.size() == 10 && f.compare(0, 4, want) == 0 && g.Read(dir + "/" + f, b))
        for (auto &e : Entries(b))
          if (e.data.size() > 4) {
            out.push_back({f.substr(0, 6), std::move(e.data)});
            break;
          }
  }
  // (only the movies this Mod Maker reads: Bink 2, Bink 1)
  out.erase(std::remove_if(out.begin(), out.end(), [](const Version &v) {
              return v.movie.size() < 4 || !(memcmp(v.movie.data(), "BIK", 3) == 0 ||
                                             memcmp(v.movie.data(), "KB2", 3) == 0);
            }), out.end());
  return out;
}

namespace detail {

// src (sw x sh BGRA) onto dst (W x H): stretched, or (tile) scaled to H rows and laid side by side. Bilinear.
inline void Place(const uint8_t *src, int sw, int sh, uint8_t *dst, int W, int H, bool tile) {
  const float tw = tile ? std::max(1.0f, float(sw) * H / sh) : float(W);
  for (int y = 0; y < H; ++y)
    for (int x = 0; x < W; ++x) {
      const float u = std::fmod(float(x), tw) / tw * sw - 0.5f, v = (y + 0.5f) / H * sh - 0.5f;
      const int x0 = std::max(0, std::min(sw - 1, int(std::floor(u)))), y0 = std::max(0, std::min(sh - 1, int(std::floor(v))));
      const int x1 = std::min(sw - 1, x0 + 1), y1 = std::min(sh - 1, y0 + 1);
      const float fx = std::max(0.0f, std::min(1.0f, u - x0)), fy = std::max(0.0f, std::min(1.0f, v - y0));
      for (int c = 0; c < 4; ++c) {
        const float a = src[(size_t(y0) * sw + x0) * 4 + c] * (1 - fx) + src[(size_t(y0) * sw + x1) * 4 + c] * fx;
        const float b = src[(size_t(y1) * sw + x0) * 4 + c] * (1 - fx) + src[(size_t(y1) * sw + x1) * 4 + c] * fx;
        dst[(size_t(y) * W + x) * 4 + c] = uint8_t(a * (1 - fy) + b * fy + 0.5f);
      }
    }
}

// frames (each W x H BGRA, from `next`, which returns false at the end) as a Bink 1 movie (30 fps) in `out`.
template <class Next>
inline bool Encode(int frames, int W, int H, Next next, Bytes &out) {
  wchar_t tmp[MAX_PATH];
  GetTempPathW(MAX_PATH, tmp);
  const fs::path file = fs::path(tmp) / L"wwe12_screen.bik";
  FILE *f = _wfopen(file.c_str(), L"wb");
  if (!f)
    return false;
  BinkWriter *w = bink_writer_open_size(f, frames, W, H);
  if (!w) {
    fclose(f);
    return false;
  }
  std::vector<uint8_t> px(size_t(W) * H * 4);
  bool ok = true;
  for (int i = 0; i < frames && ok; ++i)
    ok = next(px.data()) && bink_writer_frame(w, px.data()) == 1;
  ok = bink_writer_close(w) && ok;
  fclose(f);
  out.clear();
  if (ok) {
    std::ifstream in(file, std::ios::binary);
    out.assign(std::istreambuf_iterator<char>(in), {});
  }
  std::error_code ec;
  fs::remove(file, ec);
  return ok && !out.empty();
}

}  // namespace detail

// A ribbon (1024 x 32, the source laid along it) or wall (256 x 256, stretched) from a Bink 2 movie (its game's).
inline bool FromBink2(const Bytes &bk2, bool ribbon, Bytes &out) {
  WCHAR err[256] = L"";
  Bink2Reader *r = bink2_open_memory(bk2.data(), bk2.size(), err, 256);
  if (!r)
    return false;
  const int sw = bink2_width(r), sh = bink2_height(r), frames = bink2_frames(r);
  const int W = ribbon ? kRibbonW : kWall, H = ribbon ? kRibbonH : kWall;
  std::vector<uint8_t> src(size_t(sw) * sh * 4);
  const bool ok = frames > 0 && detail::Encode(frames, W, H, [&](uint8_t *dst) {
    if (!bink2_next(r))
      return false;
    bink2_bgra(r, src.data());
    detail::Place(src.data(), sw, sh, dst, W, H, ribbon);
    return true;
  }, out);
  bink2_close(r);
  return ok;
}

// A picture of what the game will show (BGRA at the screen's size: 1024 x 32 or 256 x 256), for a preview: frame `at`
// of a Bink 2 movie (its game's), else the player's picture.
inline bool Frame(const Bytes &bk2, const fs::path &picture, bool ribbon, int at, std::vector<uint8_t> &out) {
  const int W = ribbon ? kRibbonW : kWall, H = ribbon ? kRibbonH : kWall;
  out.assign(size_t(W) * H * 4, 0);
  if (bk2.size() > 4 && memcmp(bk2.data(), "KB2", 3) == 0) {
    WCHAR err[256] = L"";
    Bink2Reader *r = bink2_open_memory(bk2.data(), bk2.size(), err, 256);
    if (!r)
      return false;
    const int sw = bink2_width(r), sh = bink2_height(r);
    bool ok = false;
    for (int i = 0; i <= std::min(at, bink2_frames(r) - 1); ++i)
      ok = bink2_next(r) != 0;
    if (ok) {
      std::vector<uint8_t> src(size_t(sw) * sh * 4);
      bink2_bgra(r, src.data());
      detail::Place(src.data(), sw, sh, out.data(), W, H, ribbon);
    }
    bink2_close(r);
    return ok;
  }
  if (bk2.size() > 4 && memcmp(bk2.data(), "BIK", 3) == 0) {  // (WWE '12's own: Bink 1, read from a file)
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    const fs::path file = fs::path(tmp) / L"wwe12_screen_in.bik";
    {
      std::ofstream o(file, std::ios::binary);
      o.write(reinterpret_cast<const char *>(bk2.data()), std::streamsize(bk2.size()));
    }
    WCHAR err[256] = L"";
    BinkReader *r = bink_open(file.c_str(), err, 256);
    bool ok = false;
    if (r) {
      const int sw = bink_width(r), sh = bink_height(r);
      for (int i = 0; i <= std::min(at, bink_frames(r) - 1); ++i)
        ok = bink_next(r) != 0;
      if (ok) {
        std::vector<uint8_t> src(size_t(sw) * sh * 4);
        bink_bgra(r, src.data());
        detail::Place(src.data(), sw, sh, out.data(), W, H, ribbon);
      }
      bink_close(r);
    }
    std::error_code ec;
    fs::remove(file, ec);
    return ok;
  }
  svrfmt::Image img;
  if (picture.empty() || !wic_image::Load(picture, img) || img.w <= 0 || img.h <= 0)
    return false;
  std::vector<uint8_t> bgra(img.rgba.size());
  for (size_t i = 0; i + 3 < img.rgba.size(); i += 4)
    bgra[i] = img.rgba[i + 2], bgra[i + 1] = img.rgba[i + 1], bgra[i + 2] = img.rgba[i], bgra[i + 3] = 255;
  detail::Place(bgra.data(), img.w, img.h, out.data(), W, H, ribbon);
  return true;
}

// The same from a picture of the player's: a still movie of 6 seconds.
inline bool FromPicture(const fs::path &picture, bool ribbon, Bytes &out) {
  svrfmt::Image img;
  if (!wic_image::Load(picture, img) || img.w <= 0 || img.h <= 0)
    return false;
  std::vector<uint8_t> bgra(img.rgba.size());
  for (size_t i = 0; i + 3 < img.rgba.size(); i += 4)
    bgra[i] = img.rgba[i + 2], bgra[i + 1] = img.rgba[i + 1], bgra[i + 2] = img.rgba[i], bgra[i + 3] = 255;
  const int W = ribbon ? kRibbonW : kWall, H = ribbon ? kRibbonH : kWall;
  std::vector<uint8_t> frame(size_t(W) * H * 4);
  detail::Place(bgra.data(), img.w, img.h, frame.data(), W, H, ribbon);
  return detail::Encode(180, W, H, [&](uint8_t *dst) {
    memcpy(dst, frame.data(), frame.size());
    return true;
  }, out);
}

}  // namespace entrance_screens
