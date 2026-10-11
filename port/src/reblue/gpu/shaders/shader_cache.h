/**
 * @file    gpu/shaders/shader_cache.h
 * @brief   Compiled shader cache symbols emitted by XenosRecomp into
 *          generated/shader_cache.cpp. Referenced by unqualified name from
 *          the generated cpp, so this must stay in the global namespace with
 *          the emitter's exact field order and types. The build-time
 *          reblue_prelink compiles it without the rex SDK on its include
 *          path, hence <cstdint> rather than the rex aliases.
 * @copyright Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *            All rights reserved.
 * @license   BSD 3-Clause License
 *            See LICENSE file in the project root for full license text.
 */
#pragma once

#include <cstddef>
#include <cstdint>

namespace bd::gpu {
struct GuestShader;
}

struct ShaderCacheEntry {
  const uint64_t hash;
  const uint32_t dxilOffset;
  const uint32_t dxilSize;
  const uint32_t spirvOffset;
  const uint32_t spirvSize;
  const uint32_t specConstantsMask;
  bd::gpu::GuestShader *guestShader;
};

extern ShaderCacheEntry g_shaderCacheEntries[];
extern const size_t g_shaderCacheEntryCount;

extern const uint8_t g_compressedDxilCache[];
extern const size_t g_dxilCacheCompressedSize;
extern const size_t g_dxilCacheDecompressedSize;

extern const uint8_t g_compressedSpirvCache[];
extern const size_t g_spirvCacheCompressedSize;
extern const size_t g_spirvCacheDecompressedSize;
