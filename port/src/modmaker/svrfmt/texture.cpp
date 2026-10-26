// WWE12-PC: taken from Kaiko's WWE SmackDown vs. Raw 2011 PC port (with his permission, credit given),
// https://github.com/KaikoClanworth1/wwe-svr2011-pc, port/modmaker/svrfmt/texture.cpp at b2521fe.
#include "texture.h"

#include <algorithm>
#include <cmath>

namespace svrfmt {

// ------------------------------------------------------------ bundle

bool IsTextureBundle(const Bytes& raw) {
  if (raw.size() < 48) return false;
  const uint32_t n = Le32(&raw[0]);
  return n > 0 && n < 4096 && (!std::memcmp(&raw[32], "dds", 4) || !std::memcmp(&raw[32], "DDS", 4));
}

bool BundleRead(const Bytes& raw, std::vector<BundleTexture>& out) {
  if (!IsTextureBundle(raw)) return false;
  const uint32_t n = Le32(&raw[0]);
  if (16 + size_t(n) * 32 > raw.size()) return false;
  out.clear();
  for (uint32_t i = 0; i < n; ++i) {
    const uint8_t* r = &raw[16 + i * 32];
    const size_t size = Le32(r + 20), off = Le32(r + 24);
    if (off + size > raw.size()) return false;
    out.push_back({Name16(r), Name16(r + 16, 4), Bytes(raw.begin() + off, raw.begin() + off + size)});
  }
  return true;
}

Bytes BundleWrite(const std::vector<BundleTexture>& texs) {
  Bytes out;
  AppLe32(out, uint32_t(texs.size()));
  AppLe32(out, 0x100);
  AppLe32(out, 0);
  AppLe32(out, 0x10);
  out.resize(16 + texs.size() * 32, 0);
  for (size_t i = 0; i < texs.size(); ++i) {
    Pad(out, 16);
    uint8_t* r = &out[16 + i * 32];
    std::memset(r, 0, 32);
    std::memcpy(r, texs[i].name.data(), std::min<size_t>(16, texs[i].name.size()));
    std::memcpy(r + 16, texs[i].ext.data(), std::min<size_t>(4, texs[i].ext.size()));
    PutLe32(r + 20, uint32_t(texs[i].data.size()));
    PutLe32(r + 24, uint32_t(out.size()));
    App(out, texs[i].data);
  }
  Pad(out, 16);
  return out;
}

// ------------------------------------------------------------ DDS

bool DdsInfoOf(const Bytes& dds, DdsInfo& info) {
  if (dds.size() < 128 || std::memcmp(dds.data(), "DDS ", 4)) return false;
  info.h = int(Le32(&dds[12]));
  info.w = int(Le32(&dds[16]));
  info.mips = std::max(1, int(Le32(&dds[28])));
  const uint32_t pf_flags = Le32(&dds[80]);
  if (pf_flags & 4) {
    if (!std::memcmp(&dds[84], "DXT1", 4)) info.format = DxtFormat::kDxt1;
    else if (!std::memcmp(&dds[84], "DXT3", 4)) info.format = DxtFormat::kDxt3;
    else if (!std::memcmp(&dds[84], "DXT5", 4)) info.format = DxtFormat::kDxt5;
  } else if (Le32(&dds[88]) == 32) {
    info.format = DxtFormat::kArgb;
  } else if (Le32(&dds[88]) == 16 && Le32(&dds[92]) == 0x0F00 && Le32(&dds[96]) == 0x00F0 &&
             Le32(&dds[100]) == 0x000F && Le32(&dds[104]) == 0xF000) {
    info.format = DxtFormat::kArgb4444;
  }
  return info.format != DxtFormat::kUnknown;
}

namespace {

void Rgb565(uint16_t c, uint8_t* o) {
  o[0] = uint8_t((c >> 11 & 31) * 255 / 31);
  o[1] = uint8_t((c >> 5 & 63) * 255 / 63);
  o[2] = uint8_t((c & 31) * 255 / 31);
}

void DecodeColorBlock(const uint8_t* b, uint8_t px[16][4], bool dxt1) {
  const uint16_t c0 = Le16(b), c1 = Le16(b + 2);
  uint8_t pal[4][4];
  Rgb565(c0, pal[0]);
  Rgb565(c1, pal[1]);
  pal[0][3] = pal[1][3] = 255;
  if (c0 > c1 || !dxt1) {
    for (int k = 0; k < 3; ++k) {
      pal[2][k] = uint8_t((2 * pal[0][k] + pal[1][k]) / 3);
      pal[3][k] = uint8_t((pal[0][k] + 2 * pal[1][k]) / 3);
    }
    pal[2][3] = pal[3][3] = 255;
  } else {
    for (int k = 0; k < 3; ++k) pal[2][k] = uint8_t((pal[0][k] + pal[1][k]) / 2);
    pal[2][3] = 255;
    pal[3][0] = pal[3][1] = pal[3][2] = pal[3][3] = 0;
  }
  const uint32_t idx = Le32(b + 4);
  for (int i = 0; i < 16; ++i) std::memcpy(px[i], pal[idx >> (2 * i) & 3], 4);
}

}  // namespace

bool DdsDecode(const Bytes& dds, Image& out) {
  DdsInfo info;
  if (!DdsInfoOf(dds, info)) return false;
  out.w = info.w;
  out.h = info.h;
  out.rgba.assign(size_t(info.w) * info.h * 4, 0);
  const uint8_t* p = &dds[128];
  const uint8_t* end = dds.data() + dds.size();
  if (info.format == DxtFormat::kArgb) {
    if (p + size_t(info.w) * info.h * 4 > end) return false;
    for (size_t i = 0; i < size_t(info.w) * info.h; ++i) {  // BGRA in memory
      out.rgba[i * 4 + 0] = p[i * 4 + 2];
      out.rgba[i * 4 + 1] = p[i * 4 + 1];
      out.rgba[i * 4 + 2] = p[i * 4 + 0];
      out.rgba[i * 4 + 3] = p[i * 4 + 3];
    }
    return true;
  }
  if (info.format == DxtFormat::kArgb4444) {
    if (p + size_t(info.w) * info.h * 2 > end) return false;
    for (size_t i = 0; i < size_t(info.w) * info.h; ++i) {
      const uint16_t v = Le16(p + i * 2);
      out.rgba[i * 4 + 0] = uint8_t((v >> 8 & 15) * 17);
      out.rgba[i * 4 + 1] = uint8_t((v >> 4 & 15) * 17);
      out.rgba[i * 4 + 2] = uint8_t((v & 15) * 17);
      out.rgba[i * 4 + 3] = uint8_t((v >> 12 & 15) * 17);
    }
    return true;
  }
  const int bs = info.format == DxtFormat::kDxt1 ? 8 : 16;
  const int bw = std::max(1, (info.w + 3) / 4), bh = std::max(1, (info.h + 3) / 4);
  if (p + size_t(bw) * bh * bs > end) return false;
  uint8_t px[16][4];
  for (int by = 0; by < bh; ++by) {
    for (int bx = 0; bx < bw; ++bx, p += bs) {
      const uint8_t* color = bs == 16 ? p + 8 : p;
      DecodeColorBlock(color, px, info.format == DxtFormat::kDxt1);
      if (info.format == DxtFormat::kDxt3) {
        for (int i = 0; i < 16; ++i) px[i][3] = uint8_t((p[i / 2] >> (4 * (i & 1)) & 15) * 17);
      } else if (info.format == DxtFormat::kDxt5) {
        uint8_t a[8] = {p[0], p[1]};
        if (a[0] > a[1]) for (int k = 1; k < 7; ++k) a[k + 1] = uint8_t(((7 - k) * a[0] + k * a[1]) / 7);
        else {
          for (int k = 1; k < 5; ++k) a[k + 1] = uint8_t(((5 - k) * a[0] + k * a[1]) / 5);
          a[6] = 0;
          a[7] = 255;
        }
        uint64_t bits = 0;
        for (int k = 0; k < 6; ++k) bits |= uint64_t(p[2 + k]) << (8 * k);
        for (int i = 0; i < 16; ++i) px[i][3] = a[bits >> (3 * i) & 7];
      }
      for (int i = 0; i < 16; ++i) {
        const int x = bx * 4 + (i & 3), y = by * 4 + (i >> 2);
        if (x < info.w && y < info.h) std::memcpy(&out.rgba[(size_t(y) * info.w + x) * 4], px[i], 4);
      }
    }
  }
  return true;
}

// ------------------------------------------------------------ encode

namespace {

uint16_t To565(const float* c) {
  auto q = [](float v, int m) { return int(std::lround(std::clamp(v, 0.f, 255.f) * m / 255.f)); };
  return uint16_t(q(c[0], 31) << 11 | q(c[1], 63) << 5 | q(c[2], 31));
}

// Colour block: endpoints from the block's principal axis (inset a little),
// then the nearest of the 4 palette entries per pixel.
void EncodeColorBlock(const uint8_t px[16][4], uint8_t* out, bool dxt1_alpha) {
  float mean[3] = {0, 0, 0};
  int n = 0;
  bool any_transparent = false;
  for (int i = 0; i < 16; ++i) {
    if (dxt1_alpha && px[i][3] < 128) { any_transparent = true; continue; }
    for (int k = 0; k < 3; ++k) mean[k] += px[i][k];
    ++n;
  }
  if (!n) {  // fully transparent DXT1 block
    PutLe32(out, 0);
    PutLe32(out + 4, 0xFFFFFFFF);
    return;
  }
  for (float& m : mean) m /= float(n);
  float cov[6] = {0, 0, 0, 0, 0, 0};
  for (int i = 0; i < 16; ++i) {
    if (dxt1_alpha && px[i][3] < 128) continue;
    const float d[3] = {px[i][0] - mean[0], px[i][1] - mean[1], px[i][2] - mean[2]};
    cov[0] += d[0] * d[0]; cov[1] += d[0] * d[1]; cov[2] += d[0] * d[2];
    cov[3] += d[1] * d[1]; cov[4] += d[1] * d[2]; cov[5] += d[2] * d[2];
  }
  float axis[3] = {1, 1, 1};
  for (int it = 0; it < 8; ++it) {
    const float x = cov[0] * axis[0] + cov[1] * axis[1] + cov[2] * axis[2];
    const float y = cov[1] * axis[0] + cov[3] * axis[1] + cov[4] * axis[2];
    const float z = cov[2] * axis[0] + cov[4] * axis[1] + cov[5] * axis[2];
    const float l = std::sqrt(x * x + y * y + z * z);
    if (l < 1e-6f) break;
    axis[0] = x / l; axis[1] = y / l; axis[2] = z / l;
  }
  float lo = 1e9f, hi = -1e9f;
  for (int i = 0; i < 16; ++i) {
    if (dxt1_alpha && px[i][3] < 128) continue;
    const float t = (px[i][0] - mean[0]) * axis[0] + (px[i][1] - mean[1]) * axis[1] + (px[i][2] - mean[2]) * axis[2];
    lo = std::min(lo, t);
    hi = std::max(hi, t);
  }
  const float inset = (hi - lo) / 32.f;
  lo += inset;
  hi -= inset;
  float e0[3], e1[3];
  for (int k = 0; k < 3; ++k) { e0[k] = mean[k] + axis[k] * hi; e1[k] = mean[k] + axis[k] * lo; }
  uint16_t c0 = To565(e0), c1 = To565(e1);
  const bool three = any_transparent;  // DXT1 punch-through needs c0 <= c1
  if (three ? c0 > c1 : c0 < c1) std::swap(c0, c1);
  if (!three && c0 == c1) {
    if (c1 > 0) --c1; else ++c0;
  }
  uint8_t pal[4][4];
  Rgb565(c0, pal[0]);
  Rgb565(c1, pal[1]);
  const int colors = three ? 3 : 4;
  if (!three) {
    for (int k = 0; k < 3; ++k) {
      pal[2][k] = uint8_t((2 * pal[0][k] + pal[1][k]) / 3);
      pal[3][k] = uint8_t((pal[0][k] + 2 * pal[1][k]) / 3);
    }
  } else {
    for (int k = 0; k < 3; ++k) pal[2][k] = uint8_t((pal[0][k] + pal[1][k]) / 2);
  }
  uint32_t idx = 0;
  for (int i = 0; i < 16; ++i) {
    int best = 0;
    if (three && px[i][3] < 128) best = 3;
    else {
      int bd = 1 << 30;
      for (int c = 0; c < colors; ++c) {
        int d = 0;
        for (int k = 0; k < 3; ++k) d += (px[i][k] - pal[c][k]) * (px[i][k] - pal[c][k]);
        if (d < bd) { bd = d; best = c; }
      }
    }
    idx |= uint32_t(best) << (2 * i);
  }
  out[0] = uint8_t(c0); out[1] = uint8_t(c0 >> 8);
  out[2] = uint8_t(c1); out[3] = uint8_t(c1 >> 8);
  PutLe32(out + 4, idx);
}

void EncodeAlphaDxt5(const uint8_t px[16][4], uint8_t* out) {
  uint8_t lo = 255, hi = 0;
  for (int i = 0; i < 16; ++i) { lo = std::min(lo, px[i][3]); hi = std::max(hi, px[i][3]); }
  uint8_t a[8];
  a[0] = hi; a[1] = lo;
  if (hi == lo) { out[0] = hi; out[1] = lo; std::memset(out + 2, 0, 6); return; }
  for (int k = 1; k < 7; ++k) a[k + 1] = uint8_t(((7 - k) * a[0] + k * a[1]) / 7);
  uint64_t bits = 0;
  for (int i = 0; i < 16; ++i) {
    int best = 0, bd = 1 << 30;
    for (int c = 0; c < 8; ++c) {
      const int d = std::abs(int(px[i][3]) - a[c]);
      if (d < bd) { bd = d; best = c; }
    }
    bits |= uint64_t(best) << (3 * i);
  }
  out[0] = a[0]; out[1] = a[1];
  for (int k = 0; k < 6; ++k) out[2 + k] = uint8_t(bits >> (8 * k));
}

Image Half(const Image& s) {
  Image d;
  d.w = std::max(1, s.w / 2);
  d.h = std::max(1, s.h / 2);
  d.rgba.resize(size_t(d.w) * d.h * 4);
  for (int y = 0; y < d.h; ++y)
    for (int x = 0; x < d.w; ++x)
      for (int k = 0; k < 4; ++k) {
        int sum = 0;
        for (int dy = 0; dy < 2; ++dy)
          for (int dx = 0; dx < 2; ++dx) {
            const int sx = std::min(s.w - 1, x * 2 + dx), sy = std::min(s.h - 1, y * 2 + dy);
            sum += s.rgba[(size_t(sy) * s.w + sx) * 4 + k];
          }
        d.rgba[(size_t(y) * d.w + x) * 4 + k] = uint8_t((sum + 2) / 4);
      }
  return d;
}

void EncodeLevel(const Image& img, DxtFormat f, Bytes& out) {
  const int bw = std::max(1, (img.w + 3) / 4), bh = std::max(1, (img.h + 3) / 4);
  uint8_t px[16][4];
  for (int by = 0; by < bh; ++by)
    for (int bx = 0; bx < bw; ++bx) {
      for (int i = 0; i < 16; ++i) {
        const int x = std::min(img.w - 1, bx * 4 + (i & 3)), y = std::min(img.h - 1, by * 4 + (i >> 2));
        std::memcpy(px[i], &img.rgba[(size_t(y) * img.w + x) * 4], 4);
      }
      uint8_t blk[16];
      if (f == DxtFormat::kDxt1) {
        EncodeColorBlock(px, blk, true);
        App(out, blk, 8);
      } else {
        if (f == DxtFormat::kDxt5) {
          EncodeAlphaDxt5(px, blk);
        } else {
          for (int i = 0; i < 8; ++i) blk[i] = uint8_t((px[2 * i][3] * 15 + 127) / 255 | ((px[2 * i + 1][3] * 15 + 127) / 255) << 4);
        }
        EncodeColorBlock(px, blk + 8, false);
        App(out, blk, 16);
      }
    }
}

}  // namespace

Image Resize(const Image& img, int w, int h) {
  Image d;
  d.w = w;
  d.h = h;
  d.rgba.resize(size_t(w) * h * 4);
  for (int y = 0; y < h; ++y)
    for (int x = 0; x < w; ++x) {
      const float fx = (x + 0.5f) * img.w / w - 0.5f, fy = (y + 0.5f) * img.h / h - 0.5f;
      const int x0 = std::clamp(int(std::floor(fx)), 0, img.w - 1), y0 = std::clamp(int(std::floor(fy)), 0, img.h - 1);
      const int x1 = std::min(img.w - 1, x0 + 1), y1 = std::min(img.h - 1, y0 + 1);
      const float tx = std::clamp(fx - x0, 0.f, 1.f), ty = std::clamp(fy - y0, 0.f, 1.f);
      for (int k = 0; k < 4; ++k) {
        auto at = [&](int xx, int yy) { return float(img.rgba[(size_t(yy) * img.w + xx) * 4 + k]); };
        const float v = (at(x0, y0) * (1 - tx) + at(x1, y0) * tx) * (1 - ty) + (at(x0, y1) * (1 - tx) + at(x1, y1) * tx) * ty;
        d.rgba[(size_t(y) * w + x) * 4 + k] = uint8_t(std::lround(v));
      }
    }
  return d;
}

Bytes DdsEncode(const Image& img, DxtFormat format, bool mips) {
  int levels = 1;
  if (mips) for (int s = std::max(img.w, img.h); s > 1; s >>= 1) ++levels;
  Bytes out(128, 0);
  std::memcpy(out.data(), "DDS ", 4);
  PutLe32(&out[4], 124);
  uint32_t flags = 0x1 | 0x2 | 0x4 | 0x1000 | (levels > 1 ? 0x20000 : 0);
  flags |= format == DxtFormat::kArgb ? 0x8 : 0x80000;  // pitch, else linear size (A4R4G4B4 too, as the game's)
  PutLe32(&out[8], flags);
  PutLe32(&out[12], uint32_t(img.h));
  PutLe32(&out[16], uint32_t(img.w));
  const int bs = format == DxtFormat::kDxt1 ? 8 : 16;
  PutLe32(&out[20], format == DxtFormat::kArgb       ? uint32_t(img.w * 4)
                    : format == DxtFormat::kArgb4444 ? uint32_t(img.w * img.h * 2)
                                                     : uint32_t(std::max(1, (img.w + 3) / 4) * std::max(1, (img.h + 3) / 4) * bs));
  PutLe32(&out[28], uint32_t(levels));
  PutLe32(&out[76], 32);
  if (format == DxtFormat::kArgb) {
    PutLe32(&out[80], 0x41);
    PutLe32(&out[88], 32);
    PutLe32(&out[92], 0x00FF0000);
    PutLe32(&out[96], 0x0000FF00);
    PutLe32(&out[100], 0x000000FF);
    PutLe32(&out[104], 0xFF000000);
  } else if (format == DxtFormat::kArgb4444) {
    PutLe32(&out[80], 0x41);
    PutLe32(&out[88], 16);
    PutLe32(&out[92], 0x0F00);
    PutLe32(&out[96], 0x00F0);
    PutLe32(&out[100], 0x000F);
    PutLe32(&out[104], 0xF000);
  } else {
    PutLe32(&out[80], 0x4);
    std::memcpy(&out[84], format == DxtFormat::kDxt1 ? "DXT1" : format == DxtFormat::kDxt3 ? "DXT3" : "DXT5", 4);
  }
  PutLe32(&out[108], 0x1000 | (levels > 1 ? 0x400008 : 0));
  Image cur = img;
  for (int l = 0; l < levels; ++l) {
    if (format == DxtFormat::kArgb) {
      for (size_t i = 0; i < size_t(cur.w) * cur.h; ++i) {
        out.push_back(cur.rgba[i * 4 + 2]);
        out.push_back(cur.rgba[i * 4 + 1]);
        out.push_back(cur.rgba[i * 4 + 0]);
        out.push_back(cur.rgba[i * 4 + 3]);
      }
    } else if (format == DxtFormat::kArgb4444) {
      auto q = [&](size_t i, int k) { return uint16_t((cur.rgba[i * 4 + k] * 15 + 127) / 255); };
      for (size_t i = 0; i < size_t(cur.w) * cur.h; ++i) {
        const uint16_t v = uint16_t(q(i, 3) << 12 | q(i, 0) << 8 | q(i, 1) << 4 | q(i, 2));
        out.push_back(uint8_t(v));
        out.push_back(uint8_t(v >> 8));
      }
    } else {
      EncodeLevel(cur, format, out);
    }
    if (l + 1 < levels) cur = Half(cur);
  }
  return out;
}

}  // namespace svrfmt
