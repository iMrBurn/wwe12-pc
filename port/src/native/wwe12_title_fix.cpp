// WWE '12: the championship title swinging out of the hand in 60 fps entrances.
//
// The title is its own small skinned model with its own animation clip, hung on the wrestler's
// hand. Entrance clips have a key for every 1/60 s, but the console plays entrances at 30 fps and
// advances the clip two frames per update, so it only ever samples the even keys. Some odd keys of
// the title's translation are bad (the plate jumps off the path by up to ~20 cm). With wwe12_60fps
// every key is shown and the plate zigzags around its path every other frame (belt probe,
// 2026-10-18: deviation from the neighbours' midpoint 2.7 at 60 vs 1.2 at 30, alternating sign).
// Same bug and same cure as the WWE 2K14 recomp (its author described the method; the code here is
// our own, written for WWE '12's sampler).
//
// The translation track samplers, sub_8248B898 (16-bit delta keys) and sub_8248B458 (8-bit twin),
// take r3 = motion (time at +52, per-track cached positions at [+120] + track * 16), r4 = output
// vec4, r5 = cached key (s16 index, s16 time), r6 = track, r7 = track index, f1 = time in 1/60 s
// frames. While an entrance plays at 60 instead of the console's 30, a time between two even
// frames is also sampled at both even frames (with the caches put back, so the real call is
// unchanged); if the result is off that segment by more than 0.5 cm and more than a quarter of the
// segment's length, the point on the segment is used instead. Even frames never change, real
// jumps (on the segment) and fast motion (long segments) are kept, and matches (60 on the console
// too) are not touched.

#include <atomic>
#include <chrono>
#include <cmath>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_title_fix, true, "WWE12",
                    "60 fps entrances: keep championship titles on the path the console shows "
                    "(their odd animation keys are bad).");
REXCVAR_DEFINE_BOOL(wwe12_title_fix_log, false, "WWE12",
                    "Dev: log how many positions wwe12_title_fix corrects.");

bool Wwe12EntranceAt60();  // wwe12_fps.cpp

REX_EXTERN(__imp__sub_8248B898);
REX_EXTERN(__imp__sub_8248B458);

namespace {

using Sampler = void (*)(PPCContext&, uint8_t*);

struct Vec3 {
  float x, y, z;
};

Vec3 LoadVec3(u32 va) {
  return {bd::mem::load<float>(va), bd::mem::load<float>(va + 4), bd::mem::load<float>(va + 8)};
}

std::atomic<u32> g_fixed{0};
std::atomic<u32> g_checked{0};
std::atomic<u32> g_worst_mm{0};

void Log(float dist) {
  g_checked.fetch_add(1, std::memory_order_relaxed);
  if (dist > 0) {
    g_fixed.fetch_add(1, std::memory_order_relaxed);
    const u32 mm = u32(dist * 10);
    u32 worst = g_worst_mm.load(std::memory_order_relaxed);
    while (mm > worst && !g_worst_mm.compare_exchange_weak(worst, mm)) {
    }
  }
  static std::atomic<i64> s_last{0};
  const i64 now = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now().time_since_epoch())
                      .count();
  i64 last = s_last.load(std::memory_order_relaxed);
  if (now - last < 1000 || !s_last.compare_exchange_strong(last, now))
    return;
  const u32 fixed = g_fixed.exchange(0), checked = g_checked.exchange(0);
  const u32 worst_mm = g_worst_mm.exchange(0);
  if (fixed)
    BD_INFO("[title] {} of {} in-between positions put back on the path (worst {:.1f} cm)", fixed,
            checked, worst_mm / 10.0);
}

void Sample(PPCContext& ctx, uint8_t* base, Sampler orig) {
  const double t = ctx.f1.f64;
  const double even = 2.0 * std::floor(t / 2.0);
  const double frac = (t - even) / 2.0;
  const u32 motion = ctx.r3.u32, out = ctx.r4.u32, key = ctx.r5.u32;
  // The 8-bit sampler returns without output when [motion+80] is 0.
  if (!REXCVAR_GET(wwe12_title_fix) || frac < 1e-4 || t < 2.0 || !Wwe12EntranceAt60() ||
      bd::mem::load<u32>(motion + 80) == 0) {
    orig(ctx, base);
    return;
  }
  // The sampler moves the cached key and the track's cached position: save them, sample both
  // even frames into a scratch area below the caller's frame, put the caches back.
  const u32 slot = bd::mem::load<u32>(motion + 120) + (ctx.r7.u32 << 4);
  const u32 saved_key = bd::mem::load<u32>(key);
  const u64 saved_slot0 = bd::mem::load<u64>(slot), saved_slot1 = bd::mem::load<u64>(slot + 8);
  const u32 scratch = (ctx.r1.u32 - 512) & ~15u;
  Vec3 ends[2];
  for (int i = 0; i < 2; ++i) {
    PPCContext c = ctx;
    c.r1.u64 = scratch;
    c.r4.u64 = scratch + 256;
    c.f1.f64 = even + 2.0 * i;
    orig(c, base);
    ends[i] = LoadVec3(scratch + 256);
    bd::mem::store<u32>(key, saved_key);
    bd::mem::store<u64>(slot, saved_slot0);
    bd::mem::store<u64>(slot + 8, saved_slot1);
  }
  orig(ctx, base);

  const Vec3 p = LoadVec3(out), a = ends[0], b = ends[1];
  const float sx = b.x - a.x, sy = b.y - a.y, sz = b.z - a.z;
  const float len2 = sx * sx + sy * sy + sz * sz;
  // Distance from the segment, so that a real jump (a cut: the point sits at one end) is kept.
  float u = len2 > 0 ? ((p.x - a.x) * sx + (p.y - a.y) * sy + (p.z - a.z) * sz) / len2 : 0.0f;
  u = u < 0 ? 0 : (u > 1 ? 1 : u);
  const float dx = p.x - (a.x + sx * u), dy = p.y - (a.y + sy * u), dz = p.z - (a.z + sz * u);
  const float dist = std::sqrt(dx * dx + dy * dy + dz * dz);
  const bool fix = dist > 0.5f && dist * dist > len2 / 16.0f;
  if (fix) {
    const float f = float(frac);
    bd::mem::store<float>(out, a.x + sx * f);
    bd::mem::store<float>(out + 4, a.y + sy * f);
    bd::mem::store<float>(out + 8, a.z + sz * f);
  }
  if (REXCVAR_GET(wwe12_title_fix_log))
    Log(fix ? dist : 0.0f);
}

}  // namespace

extern "C" REX_FUNC(sub_8248B898) { Sample(ctx, base, __imp__sub_8248B898); }

extern "C" REX_FUNC(sub_8248B458) { Sample(ctx, base, __imp__sub_8248B458); }
