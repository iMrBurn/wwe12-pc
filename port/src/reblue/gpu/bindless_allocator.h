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
#if defined(WWE12_IOS)
// MoltenVK turns each of the three 65536-entry texture arrays into a Metal argument buffer; the
// iPhone shows scrambled / black HUD textures with that table (the SvR 2011 iOS port had the same
// symptoms until it moved to a small per-draw table). A smaller table is the cheap first test.
constexpr u32 kBindlessTextureCount = 16384;
#else
constexpr u32 kBindlessTextureCount = 65536;
#endif
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
