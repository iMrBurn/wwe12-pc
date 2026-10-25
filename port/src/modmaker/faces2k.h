// Mod Maker: the WWE 2K games' superstar portraits (read from the player's own discs, My game discs).
//  - 2K15 / 2K16: pac\menu\menuHD2.pac, SFAG / SFBG / SFCG "NNNN", uncompressed TGA (Tga below).
//  - 2K17: pac\menu\SSFaceHD.pac, an HSPC whose names are hashes of an unknown name: 351 TGA 512 x 512 portraits,
//    stored in the superstars' number order (ArcNode::order) but several for some superstars (attires), and an SHDC
//    holding an SHDC of 64 x 64 face icons (DDS) by superstar number. Each icon is matched with the portraits by
//    looks (the icon is the portrait's face, about x 168, y 88, 144 x 144 of 512) and by order (a number's portrait
//    comes after the smaller numbers'): Match2K17.
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <vector>
#include "svrfmt/archive.h"
#include "svrfmt/texture.h"

namespace faces2k {

using Bytes = std::vector<uint8_t>;

// An uncompressed 32-bit TGA (type 2: BGRA, bottom row first unless the descriptor's bit 5 says top first).
inline bool Tga(const Bytes &d, svrfmt::Image &out) {
  if (d.size() < 18 || d[2] != 2 || d[16] != 32)
    return false;
  const int w = d[12] | d[13] << 8, h = d[14] | d[15] << 8;
  const size_t at = 18 + d[0];
  if (w <= 0 || h <= 0 || d.size() < at + size_t(w) * h * 4)
    return false;
  const bool top_first = d[17] & 0x20;
  out.w = w;
  out.h = h;
  out.rgba.resize(size_t(w) * h * 4);
  for (int y = 0; y < h; ++y) {
    const uint8_t *row = &d[at + size_t(top_first ? y : h - 1 - y) * w * 4];
    for (int x = 0; x < w; ++x) {
      uint8_t *o = &out.rgba[(size_t(y) * w + x) * 4];
      o[0] = row[x * 4 + 2];
      o[1] = row[x * 4 + 1];
      o[2] = row[x * 4 + 0];
      o[3] = row[x * 4 + 3];
    }
  }
  return true;
}

constexpr int kCell = 32;  // the pictures are compared at 32 x 32, in grey
using Grey = std::array<float, kCell * kCell>;

// A square of an image (x, y, size) in grey, over black (alpha), averaged down to 32 x 32.
inline Grey Square(const svrfmt::Image &img, int x0, int y0, int size, bool over_black) {
  Grey g{};
  std::array<int, kCell * kCell> n{};
  for (int y = std::max(0, y0); y < std::min(img.h, y0 + size); ++y)
    for (int x = std::max(0, x0); x < std::min(img.w, x0 + size); ++x) {
      const uint8_t *p = &img.rgba[(size_t(y) * img.w + x) * 4];
      const float a = over_black ? p[3] / 255.0f : 1.0f;
      const int cell = (y - y0) * kCell / size * kCell + (x - x0) * kCell / size;
      g[size_t(cell)] += (0.299f * p[0] + 0.587f * p[1] + 0.114f * p[2]) * a;
      ++n[size_t(cell)];
    }
  for (size_t i = 0; i < g.size(); ++i)
    g[i] = n[i] ? g[i] / float(n[i]) : 0;
  return g;
}

inline float Diff(const Grey &a, const Grey &b) {
  float d = 0;
  for (size_t i = 0; i < a.size(); ++i)
    d += a[i] > b[i] ? a[i] - b[i] : b[i] - a[i];
  return d / float(a.size());
}

// WWE 2K17's SSFaceHD.pac: its portraits in their stored order (the superstars' number order).
inline std::vector<const svrfmt::ArcNode *> Portraits2K17(const svrfmt::Archive &a) {
  std::vector<const svrfmt::ArcNode *> v;
  for (const auto &k : a.root.kids)
    if (svrfmt::ArchiveKindOf(k) != "SHDC")
      v.push_back(&k);
  std::sort(v.begin(), v.end(), [](const auto *x, const auto *y) { return x->order < y->order; });
  return v;
}

// Which portrait (index in Portraits2K17) is each superstar's, by number: only the icons matched well.
inline std::map<int, size_t> Match2K17(svrfmt::Archive &a, float kSkip = 34, float kGood = 30) {
  // the icons: the SHDC's SHDC, children by superstar number
  std::map<int, Grey> icons;
  for (auto &k : a.root.kids)
    if (k.kind == svrfmt::ArcKind::Shdc || svrfmt::ArchiveKindOf(k) == "SHDC") {
      if (!svrfmt::ArchiveExpand(k))
        continue;
      for (auto &inner : k.kids)
        if (svrfmt::ArchiveExpand(inner))
          for (auto &c : inner.kids) {
            svrfmt::Image img;
            if (svrfmt::DdsDecode(svrfmt::ArchiveData(c), img) && img.w > 0)
              icons[int(c.id)] = Square(img, 0, 0, img.w, false);
          }
    }
  const auto portraits = Portraits2K17(a);
  if (icons.empty() || portraits.empty())
    return {};
  // each portrait's face, a little around the usual place
  std::vector<std::vector<Grey>> faces(portraits.size());
  for (size_t i = 0; i < portraits.size(); ++i) {
    svrfmt::Image img;
    if (!Tga(svrfmt::ArchiveData(*portraits[i]), img) || img.w != 512)
      continue;
    for (int dx : {-16, 0, 16})
      for (int dy : {-16, 0, 16})
        for (int s : {128, 144, 160})
          faces[i].push_back(Square(img, 168 + dx, 88 + dy, s, true));
  }
  // the best assignment keeping the order: dp over the icons (by number), each on a later portrait than the one
  // before, or skipped (a cost of kSkip)

  const size_t P = portraits.size();
  std::vector<int> numbers;
  std::vector<std::vector<float>> cost;
  for (const auto &[n, g] : icons) {
    numbers.push_back(n);
    std::vector<float> c(P, 1e9f);
    for (size_t i = 0; i < P; ++i)
      for (const auto &f : faces[i])
        c[i] = std::min(c[i], Diff(f, g));
    cost.push_back(std::move(c));
  }
  const size_t K = numbers.size();
  // state[j + 1]: the least total with the last portrait used j (-1: none)
  std::vector<float> state(P + 1, 0);
  std::vector<std::vector<int>> from(K, std::vector<int>(P + 1, -2));  // -2: skipped; else the previous last + 1
  for (size_t k = 0; k < K; ++k) {
    std::vector<float> next(P + 1);
    for (size_t j = 0; j <= P; ++j)
      next[j] = state[j] + kSkip;  // (skipped: the last stays)
    float best = state[0];
    int best_at = 0;
    for (size_t i = 0; i < P; ++i) {  // assigned to portrait i: after any last < i
      if (state[i] < best) {
        best = state[i];
        best_at = int(i);
      }
      if (best + cost[k][i] < next[i + 1]) {
        next[i + 1] = best + cost[k][i];
        from[k][i + 1] = best_at;
      }
    }
    state = std::move(next);
  }
  std::map<int, size_t> out;
  size_t j = size_t(std::min_element(state.begin(), state.end()) - state.begin());
  for (size_t k = K; k-- > 0;) {
    const int f = from[k][j];
    if (f == -2)
      continue;  // (skipped)
    if (cost[k][j - 1] < kGood)
      out[numbers[k]] = j - 1;
    j = size_t(f);
  }
  return out;
}

}  // namespace faces2k
