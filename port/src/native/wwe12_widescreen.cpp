// WWE '12 on screens that aren't 16:9 (wwe12_widescreen, svr_resources.cpp): the camera side.
//
// The game's cameras take their aspect ratio from one 16:9 constant (0x8200BC60; 4:3 at
// 0x8200BC5C for standard-definition consoles), read by three functions only:
// - sub_82824370, a camera's projection (vertical field of view at camera+512, near +500, far +504;
//   perspective when +480 is 0, through sub_82823CB0);
// - sub_823BE870, the current camera's view volume for culling (same camera fields);
// - sub_826907B0, a view volume built from another object's field of view (+104), most likely
//   the fit of the sun's shadows to what the camera sees.
// Writing the screen's ratio there gives a wider view on a wider screen, and the game culls and
// fits its shadows for it. Narrower than 16:9 (16:10), the vertical field of view is widened as
// well, for each call, so the view keeps 16:9's width and shows more above and below.
//
// Adapted from Kaiko's SvR 2011 port (native_widescreen: the same constant there, 0x82007724, and
// the field of view widened around its projection, sub_825818B8), with his permission
// (THIRD_PARTY.md).

#include <cmath>

#include <rex/cvar.h>
#include <rex/system/xmemory.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"
#include "svr_resources.h"

REX_EXTERN(__imp__sub_82824370);
REX_EXTERN(__imp__sub_823BE870);
REX_EXTERN(__imp__sub_826907B0);
REX_EXTERN(sub_82818B90);  // the calling thread's current camera (r3 ignored)

namespace {

constexpr u32 kAspectConstant = 0x8200BC60;
constexpr float kAspect16x9 = 16.0f / 9.0f;

// The constant is in read-only data: its page is made writable once (as Kaiko does).
bool AspectWritable() {
  static const bool writable = [] {
    auto *memory = REX_KERNEL_MEMORY();
    auto *heap = memory ? memory->LookupHeap(kAspectConstant) : nullptr;
    const bool ok = heap && heap->Protect(kAspectConstant & ~0xFFFu, 0x1000,
                                          rex::memory::kMemoryProtectRead |
                                              rex::memory::kMemoryProtectWrite);
    if (!ok)
      BD_WARN("[widescreen] camera aspect constant not writable: the camera stays 16:9");
    return ok;
  }();
  return writable;
}

// The screen's shape (1 and 1 outside 3D scenes), and the camera constant set to match.
float SceneShape(float &tall) {
  float wide = 1.0f;
  bd::gpu::SvrSceneShape(wide, tall);
  const float aspect = kAspect16x9 * wide / tall;
  if (bd::mem::load<float>(kAspectConstant) != aspect) {
    if (!AspectWritable()) {
      wide = tall = 1.0f;
      return wide;
    }
    bd::mem::store<float>(kAspectConstant, aspect);
    BD_INFO("[widescreen] camera aspect {:.4f}", aspect);
  }
  return wide;
}

// Widens the vertical field of view (radians) at va by tall for the call, restored on exit.
class WiderFov {
 public:
  WiderFov(u32 va, float tall) : va_(va) {
    if (tall <= 1.0f || !va)
      return;
    saved_ = bd::mem::load<float>(va);
    if (!(saved_ > 0.01f && saved_ < 3.1f))
      return;
    bd::mem::store<float>(va, 2.0f * std::atan(std::tan(saved_ * 0.5f) * tall));
    widened_ = true;
  }
  ~WiderFov() {
    if (widened_)
      bd::mem::store<float>(va_, saved_);
  }

 private:
  u32 va_;
  float saved_ = 0.0f;
  bool widened_ = false;
};

}  // namespace

extern "C" REX_FUNC(sub_82824370) {
  float tall = 1.0f;
  SceneShape(tall);
  WiderFov fov(ctx.r3.u32 ? ctx.r3.u32 + 512 : 0, tall);
  __imp__sub_82824370(ctx, base);
}

extern "C" REX_FUNC(sub_823BE870) {
  float tall = 1.0f;
  SceneShape(tall);
  u32 camera = 0;
  if (tall > 1.0f) {
    const u64 r3 = ctx.r3.u64;
    sub_82818B90(ctx, base);
    camera = ctx.r3.u32;
    ctx.r3.u64 = r3;
  }
  WiderFov fov(camera ? camera + 512 : 0, tall);
  __imp__sub_823BE870(ctx, base);
}

extern "C" REX_FUNC(sub_826907B0) {
  float tall = 1.0f;
  SceneShape(tall);
  WiderFov fov(ctx.r3.u32 ? ctx.r3.u32 + 104 : 0, tall);
  __imp__sub_826907B0(ctx, base);
}
