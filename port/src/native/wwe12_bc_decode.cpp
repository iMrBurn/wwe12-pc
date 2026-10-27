// BC1/BC2/BC3 (DXT1, DXT3, DXT5) to RGBA8 on the CPU, for GPUs that cannot sample BC textures:
// Mali (most MediaTek and Exynos phones, Android #34, #36) refuses every BC image, so the whole
// game drew black and white. texture_upload.cpp decodes when the first BC texture cannot be made,
// or always with wwe12_bc_decode = true (to test on a GPU that has BC). Costs 4x (BC2/BC3) to 8x
// (BC1) the texture memory, and CPU time while textures load.

#include <algorithm>
#include <cstring>

#include <rex/cvar.h>

#include "wwe12_bc_decode.h"

REXCVAR_DEFINE_BOOL(wwe12_bc_decode, false, "WWE12",
                    "Decode BC (DXT) textures to RGBA8 on the CPU even when the GPU supports them "
                    "(for tests; GPUs without BC, like Mali, get it on their own).");

namespace {

inline u32 Rgb565(u16 c) {
  const u32 r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
  return ((r << 3) | (r >> 2)) | (((g << 2) | (g >> 4)) << 8) | (((b << 3) | (b >> 2)) << 16);
}

inline u32 Mix(u32 a, u32 b, u32 wa, u32 wb, u32 div) {
  u32 out = 0;
  for (int sh = 0; sh < 24; sh += 8)
    out |= ((((a >> sh) & 255) * wa + ((b >> sh) & 255) * wb) / div) << sh;
  return out;
}

inline u16 Le16(const u8 *p) { return u16(p[0] | (p[1] << 8)); }
inline u32 Le32(const u8 *p) { return u32(p[0] | (p[1] << 8) | (p[2] << 16) | (u32(p[3]) << 24)); }

// Colour block (8 bytes) into 16 RGBA texels; three_colour: BC1's 1-bit alpha mode allowed.
void ColourBlock(const u8 *b, bool three_colour, u32 out[16]) {
  const u16 c0 = Le16(b), c1 = Le16(b + 2);
  u32 pal[4];
  pal[0] = Rgb565(c0) | 0xFF000000u;
  pal[1] = Rgb565(c1) | 0xFF000000u;
  if (c0 > c1 || !three_colour) {
    pal[2] = Mix(pal[0], pal[1], 2, 1, 3) | 0xFF000000u;
    pal[3] = Mix(pal[0], pal[1], 1, 2, 3) | 0xFF000000u;
  } else {
    pal[2] = Mix(pal[0], pal[1], 1, 1, 2) | 0xFF000000u;
    pal[3] = 0;  // transparent black
  }
  const u32 idx = Le32(b + 4);
  for (int i = 0; i < 16; ++i)
    out[i] = pal[(idx >> (2 * i)) & 3];
}

// BC3 / BC4 style alpha block (8 bytes) into 16 alpha values.
void AlphaBlock(const u8 *b, u8 out[16]) {
  const u32 a0 = b[0], a1 = b[1];
  u32 pal[8] = {a0, a1};
  if (a0 > a1) {
    for (u32 i = 1; i < 7; ++i)
      pal[i + 1] = ((7 - i) * a0 + i * a1) / 7;
  } else {
    for (u32 i = 1; i < 5; ++i)
      pal[i + 1] = ((5 - i) * a0 + i * a1) / 5;
    pal[6] = 0;
    pal[7] = 255;
  }
  u64 bits = 0;
  for (int i = 0; i < 6; ++i)
    bits |= u64(b[2 + i]) << (8 * i);
  for (int i = 0; i < 16; ++i)
    out[i] = u8(pal[(bits >> (3 * i)) & 7]);
}

}  // namespace

bool Wwe12ForceBCDecode() { return REXCVAR_GET(wwe12_bc_decode); }

bool Wwe12DecodeBCSlice(Wwe12BCKind kind, const u8 *src, u32 row_blocks, u32 block_rows, u32 width,
                        u32 height, u8 *dst, u32 dst_row_texels) {
  const u32 block_bytes = kind == Wwe12BCKind::BC1 ? 8 : 16;
  u32 texels[16];
  u8 alpha[16];
  const u32 bw = std::min(row_blocks, (width + 3) / 4), bh = std::min(block_rows, (height + 3) / 4);
  for (u32 by = 0; by < bh; ++by) {
    const u8 *row = src + size_t(by) * row_blocks * block_bytes;
    for (u32 bx = 0; bx < bw; ++bx) {
      const u8 *blk = row + size_t(bx) * block_bytes;
      switch (kind) {
      case Wwe12BCKind::BC1:
        ColourBlock(blk, true, texels);
        break;
      case Wwe12BCKind::BC2:
        ColourBlock(blk + 8, false, texels);
        for (int i = 0; i < 16; ++i) {
          const u32 a4 = (blk[i / 2] >> (4 * (i & 1))) & 15;
          texels[i] = (texels[i] & 0x00FFFFFFu) | ((a4 * 17) << 24);
        }
        break;
      case Wwe12BCKind::BC3:
        AlphaBlock(blk, alpha);
        ColourBlock(blk + 8, false, texels);
        for (int i = 0; i < 16; ++i)
          texels[i] = (texels[i] & 0x00FFFFFFu) | (u32(alpha[i]) << 24);
        break;
      }
      for (u32 y = 0; y < 4; ++y) {
        const u32 ty = by * 4 + y;
        if (ty >= height)
          break;
        for (u32 x = 0; x < 4; ++x) {
          const u32 tx = bx * 4 + x;
          if (tx >= width)
            break;
          std::memcpy(dst + (size_t(ty) * dst_row_texels + tx) * 4, &texels[y * 4 + x], 4);
        }
      }
    }
  }
  return true;
}
