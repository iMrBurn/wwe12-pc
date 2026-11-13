/**
 * @file    gpu/bindless_allocator.h
 * @brief   Linear free slot scan shared by the texture and sampler bindless
 *          descriptor heaps.
 *
 * @copyright Copyright (c) 2026 Tom Clay <tomc@tctechstuff.com>
 *            All rights reserved.
 * @license   BSD 3-Clause License
 *            See LICENSE file in the project root for full license text.
 */
#pragma once

#include <rex/types.h>
#include <vector>

namespace bd::gpu {

// Heap capacities. Every CreateTexture registers a texture slot, so that heap
// must be large.
constexpr u32 kBindlessTextureCount = 65536;

// Compact texture tables (Vulkan only; iPhone, or WWE12_COMPACT_TEXTURES to try it elsewhere):
// MoltenVK turns the 65536-entry texture arrays into Metal argument buffers and shows black or
// scrambled HUD textures with them (the SvR 2011 iOS port had the same symptoms until it moved to a
// small table filled for each draw). Here the slots stay a CPU-side registry (slot -> texture), the
// shader-visible table has kCompactTextureCount entries, and gpu/compact_textures.cpp fills one for
// the textures each draw names and rewrites the indices of the shared constants to match.
#if (defined(WWE12_IOS) || defined(WWE12_COMPACT_TEXTURES)) && !defined(REBLUE_D3D11) && \
    !defined(REBLUE_D3D12)
#define BD_COMPACT_TEXTURES 1
#else
#define BD_COMPACT_TEXTURES 0
#endif
// 3 null entries + at most 3 x 32 textures named by one draw (16 slots in each of 3 kinds, pixel and
// vertex halves), rounded up.
constexpr u32 kCompactTextureCount = 128;
constexpr u32 kBindlessSamplerCount = 1024;

// First free slot in used[start_index..end), marked used, returned. on_full
// when none free. Caller holds the heap's mutex.
inline u32 BindlessAllocateSlot(std::vector<bool> &used, u32 start_index,
                                u32 on_full) {
  for (size_t i = start_index; i < used.size(); ++i) {
    if (!used[i]) {
      used[i] = true;
      return static_cast<u32>(i);
    }
  }
  return on_full;
}

// Marks used[slot] free. No-op if slot < start_index or out of range.
inline void BindlessFreeSlot(std::vector<bool> &used, u32 slot,
                             u32 start_index) {
  if (slot < start_index || slot >= static_cast<u32>(used.size()))
    return;
  used[slot] = false;
}

} // namespace bd::gpu
