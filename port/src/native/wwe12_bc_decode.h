// BC1/BC2/BC3 to RGBA8 on the CPU, for GPUs without BC textures (Mali). See wwe12_bc_decode.cpp.
#pragma once

#include <cstdint>

using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

enum class Wwe12BCKind { BC1, BC2, BC3 };

// wwe12_bc_decode: decode even when the GPU has BC (tests on the emulator / PC).
bool Wwe12ForceBCDecode();

// One slice: src = block_rows rows of row_blocks blocks; dst = height rows of dst_row_texels RGBA8
// texels (only width x height written).
bool Wwe12DecodeBCSlice(Wwe12BCKind kind, const u8 *src, u32 row_blocks, u32 block_rows, u32 width,
                        u32 height, u8 *dst, u32 dst_row_texels);
