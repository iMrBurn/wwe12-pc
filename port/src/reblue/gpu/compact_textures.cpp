/**
 * @file    gpu/compact_textures.cpp
 * @brief   Compact texture tables (iPhone / MoltenVK): the shaders see a small table filled for the
 *          textures each draw names, instead of the 65536-entry bindless one.
 *
 * Why: MoltenVK turns the big table into Metal argument buffers; on the iPhone that shows black or
 * scrambled HUD textures, GPU page faults and lost devices (the SvR 2011 iOS port had the same
 * symptoms until it moved to a small table filled for each draw).
 *
 * How: the slot allocator is unchanged (a slot is still what the guest texture records), but a slot
 * is only an entry of a CPU-side registry (VideoState::bindless_registry, written by
 * SetBindlessTexture). Before each draw the texture indices of the shared constants are read, the
 * distinct slots go to positions 3.. of a descriptor set of kCompactTextureCount entries (0..2 are
 * the null textures), and the indices are rewritten to those positions. Sets come from a pool per
 * frame slot and are shared by the draws of a frame that name the same textures. The same layout as
 * before is used (only its size changed), so pipelines, shaders and push constants are untouched.
 *
 * Only built with BD_COMPACT_TEXTURES (Vulkan; WWE12_IOS or WWE12_COMPACT_TEXTURES).
 */
#include "gpu/bindless_allocator.h"

#if BD_COMPACT_TEXTURES

#include <cstring>
#include <memory>
#include <unordered_map>
#include <vector>

#include <plume_render_interface.h>

#include "core/logging.h"
#include "gpu/constant_buffers.h"
#include "gpu/device.h"

namespace bd::gpu {

namespace {

// Distinct sets one frame slot may use (each is a plume descriptor set: a layout and a pool).
constexpr u32 kMaxSetsPerFrame = 1024;
constexpr u32 kNullBase = kNullTextureDescriptorCount;

struct Entry {
  std::unique_ptr<plume::RenderDescriptorSet> set;
  std::vector<u32> slots; // the slot at position kNullBase + i
  u64 writes = 0;         // VideoState::texture_set_writes when it was filled
};

struct FramePool {
  std::vector<Entry> entries;
  u32 used = 0;
  std::unordered_multimap<u64, u32> index; // content hash -> entries[]
};

FramePool g_frames[kNumFrames];
const plume::RenderDescriptorSet *g_bound = nullptr;
u32 g_exhausted = 0;

std::unique_ptr<plume::RenderDescriptorSet> CreateSet(VideoState &s) {
  plume::RenderDescriptorSetBuilder b;
  b.begin();
  b.addTexture(0, kCompactTextureCount);
  b.end(true, kCompactTextureCount);
  auto set = b.create(s.device.get());
  if (!set)
    return nullptr;
  for (u32 i = 0; i < kNullBase; ++i) {
    set->setTexture(i, s.null_textures[i].get(), plume::RenderTextureLayout::SHADER_READ,
                    s.null_texture_views[i].get());
  }
  return set;
}

void Fill(VideoState &s, Entry &e, const u32 *slots, const u8 *kinds, u32 n) {
  for (u32 i = 0; i < n; ++i) {
    const VideoState::BindlessEntry r =
        slots[i] < s.bindless_registry.size() ? s.bindless_registry[slots[i]] : VideoState::BindlessEntry{};
    if (r.texture) {
      e.set->setTexture(kNullBase + i, r.texture, plume::RenderTextureLayout::SHADER_READ, r.view);
    } else {
      e.set->setTexture(kNullBase + i, s.null_textures[kinds[i]].get(),
                        plume::RenderTextureLayout::SHADER_READ, s.null_texture_views[kinds[i]].get());
    }
  }
  // What the set held before and the new content does not cover points at textures that may be freed
  // while the set stays bound: put the null texture there.
  for (u32 i = n; i < static_cast<u32>(e.slots.size()); ++i) {
    e.set->setTexture(kNullBase + i, s.null_textures[0].get(), plume::RenderTextureLayout::SHADER_READ,
                      s.null_texture_views[0].get());
  }
  e.slots.assign(slots, slots + n);
}

u64 Hash(const u32 *slots, u32 n, u64 writes) {
  u64 h = 1469598103934665603ull ^ (writes * 0x9E3779B97F4A7C15ull);
  for (u32 i = 0; i < n; ++i) {
    h ^= slots[i];
    h *= 1099511628211ull;
  }
  return h ^ n;
}

// The set holding exactly these textures at kNullBase.., or null when the frame's pool is used up.
const plume::RenderDescriptorSet *Acquire(VideoState &s, const u32 *slots, const u8 *kinds, u32 n) {
  FramePool &fp = g_frames[s.frame.load(std::memory_order_relaxed) % kNumFrames];
  const u64 writes = s.texture_set_writes.load(std::memory_order_relaxed);
  const u64 h = Hash(slots, n, writes);
  const auto range = fp.index.equal_range(h);
  for (auto it = range.first; it != range.second; ++it) {
    Entry &e = fp.entries[it->second];
    if (e.writes == writes && e.slots.size() == n && std::memcmp(e.slots.data(), slots, n * sizeof(u32)) == 0) {
      return e.set.get();
    }
  }
  if (fp.used >= kMaxSetsPerFrame)
    return nullptr;
  if (fp.used == fp.entries.size()) {
    Entry fresh;
    fresh.set = CreateSet(s);
    if (!fresh.set)
      return nullptr;
    fp.entries.push_back(std::move(fresh));
  }
  Entry &e = fp.entries[fp.used];
  Fill(s, e, slots, kinds, n);
  e.writes = writes;
  fp.index.emplace(h, fp.used);
  ++fp.used;
  return e.set.get();
}

void BindMain(VideoState &s, plume::RenderCommandList *list, const plume::RenderDescriptorSet *set) {
  auto *p = const_cast<plume::RenderDescriptorSet *>(set);
  list->setGraphicsDescriptorSet(p, 0);
  list->setGraphicsDescriptorSet(p, 1);
  list->setGraphicsDescriptorSet(p, 2);
  (void)s;
}

} // namespace

void CompactResetFrame(u32 slot) {
  FramePool &fp = g_frames[slot % kNumFrames];
  fp.used = 0;
  fp.index.clear();
}

void CompactForgetBinding() { g_bound = nullptr; }

void CompactBindDraw(SharedConstants &c) {
  auto &s = state();
  if (!s.command_list || !s.texture_descriptor_set)
    return;
  u32 *arrays[6] = {c.texture2DIndices,   c.texture3DIndices,   c.textureCubeIndices,
                    c.texture2DIndicesHi, c.texture3DIndicesHi, c.textureCubeIndicesHi};
  static constexpr u8 kKind[6] = {0, 1, 2, 0, 1, 2};
  u32 slots[96];
  u8 kinds[96];
  u32 n = 0;
  for (u32 a = 0; a < 6; ++a) {
    for (u32 i = 0; i < 16; ++i) {
      const u32 idx = arrays[a][i];
      if (idx < kNullBase)
        continue; // already one of the null textures (same positions in the compact table)
      if (idx >= kBindlessTextureCount) {
        arrays[a][i] = kKind[a];
        continue;
      }
      u32 j = 0;
      while (j < n && slots[j] != idx)
        ++j;
      if (j == n) {
        slots[n] = idx;
        kinds[n] = kKind[a];
        ++n;
      }
      arrays[a][i] = kNullBase + j;
    }
  }
  const plume::RenderDescriptorSet *set = n ? Acquire(s, slots, kinds, n) : s.texture_descriptor_set.get();
  if (!set) {
    // Pool used up this frame: the draw gets the null textures (black, never a stale texture).
    for (u32 a = 0; a < 6; ++a) {
      for (u32 i = 0; i < 16; ++i) {
        if (arrays[a][i] >= kNullBase)
          arrays[a][i] = kKind[a];
      }
    }
    set = s.texture_descriptor_set.get();
    if (g_exhausted++ < 8)
      BD_WARN("compact textures: more than {} distinct texture sets in one frame, draws get null textures",
              kMaxSetsPerFrame);
  }
  if (set != g_bound) {
    BindMain(s, s.command_list, set);
    g_bound = set;
  }
}

u32 CompactBindHost(plume::RenderCommandList *list, u32 slot, bool main_layout) {
  auto &s = state();
  g_bound = nullptr; // the pass changes what is bound
  if (!list || !s.texture_descriptor_set)
    return 0;
  const u8 kind = 0;
  const bool valid = slot >= kNullBase && slot < kBindlessTextureCount;
  const plume::RenderDescriptorSet *set = valid ? Acquire(s, &slot, &kind, 1) : nullptr;
  auto *p = const_cast<plume::RenderDescriptorSet *>(set ? set : s.texture_descriptor_set.get());
  list->setGraphicsDescriptorSet(p, 0);
  if (main_layout) {
    list->setGraphicsDescriptorSet(p, 1);
    list->setGraphicsDescriptorSet(p, 2);
  }
  return set ? kNullBase : (slot < kNullBase ? slot : 0);
}

} // namespace bd::gpu

#else

namespace bd::gpu {
void CompactResetFrame(unsigned) {}
void CompactForgetBinding() {}
} // namespace bd::gpu

#endif
