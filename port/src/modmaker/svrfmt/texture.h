// WWE12-PC: taken from Kaiko's WWE SmackDown vs. Raw 2011 PC port (with his permission, credit given),
// https://github.com/KaikoClanworth1/wwe-svr2011-pc, port/modmaker/svrfmt/texture.h at b2521fe.
// Arena texture bundles and DDS (DXT1/3/5, A8R8G8B8): untiled PC DDS inside
// a bundle of {name[16], "dds", size, offset} records. Mirrors
// tools/arena_tool.py (all 97 arena bundles round-trip byte-identically).
#pragma once

#include <string>
#include <vector>

#include "bytes.h"

namespace svrfmt {

struct BundleTexture {
  std::string name;  // without extension
  std::string ext;   // "dds"
  Bytes data;
};
bool IsTextureBundle(const Bytes& raw);
bool BundleRead(const Bytes& raw, std::vector<BundleTexture>& out);
Bytes BundleWrite(const std::vector<BundleTexture>& texs);

struct Image {
  int w = 0, h = 0;
  std::vector<uint8_t> rgba;  // w*h*4
};

enum class DxtFormat { kDxt1, kDxt3, kDxt5, kArgb, kUnknown };
struct DdsInfo {
  int w = 0, h = 0, mips = 1;
  DxtFormat format = DxtFormat::kUnknown;
};
bool DdsInfoOf(const Bytes& dds, DdsInfo& info);
// Top mip level -> RGBA.
bool DdsDecode(const Bytes& dds, Image& out);
// RGBA -> DDS with a full mip chain (box filter). w and h are rounded up to
// multiples of 4 by edge padding.
Bytes DdsEncode(const Image& img, DxtFormat format, bool mips = true);

Image Resize(const Image& img, int w, int h);  // bilinear

}  // namespace svrfmt
