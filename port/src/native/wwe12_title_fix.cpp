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

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <fmt/format.h>
#include <vector>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_title_fix, true, "WWE12",
                    "60 fps entrances: keep championship titles on the path the console shows "
                    "(their odd animation keys are bad).");
REXCVAR_DEFINE_BOOL(wwe12_title_fix_all, false, "WWE12",
                    "Test: wwe12_title_fix puts every in-between position on the path, not only the "
                    "ones far off it.");
REXCVAR_DEFINE_BOOL(wwe12_title_fix_pose, false, "WWE12",
                    "Test: 60 fps entrances blend each animated pose between the two console "
                    "frames around it (position and rotation of every bone).");
REXCVAR_DEFINE_BOOL(wwe12_title_fix_pose_hold, false, "WWE12",
                    "Dev: with wwe12_title_fix_pose, hold the even frame's pose instead of blending.");
REXCVAR_DEFINE_BOOL(wwe12_arm_trap, false, "WWE12",
                    "Dev: in 60 fps entrances, find the code that changes bone records after "
                    "sub_82490530 (logs [armtrap]).");
REXCVAR_DEFINE_BOOL(wwe12_title_fix_log, false, "WWE12",
                    "Dev: log how many positions wwe12_title_fix corrects.");

bool Wwe12EntranceAt60();  // wwe12_fps.cpp

REX_EXTERN(__imp__sub_8248B898);
REX_EXTERN(__imp__sub_8248B458);
REX_EXTERN(__imp__sub_82490530);
REX_EXTERN(__imp__sub_82490268);

namespace bd::gpu {
u64 SvrFrameIndex();
}

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
  const bool fix =
      REXCVAR_GET(wwe12_title_fix_all) || (dist > 0.5f && dist * dist > len2 / 16.0f);
  if (fix) {
    const float f = float(frac);
    bd::mem::store<float>(out, a.x + sx * f);
    bd::mem::store<float>(out + 4, a.y + sy * f);
    bd::mem::store<float>(out + 8, a.z + sz * f);
  }
  if (REXCVAR_GET(wwe12_title_fix_log))
    Log(fix ? dist : 0.0f);
}

// sub_82490530(motion) evaluates every bone of a motion at its time [motion+52] (1/60 s frames):
// sub_8248EF80 per bone i < [motion+124] writes the bone's record at [motion+92] + i * 80
// (translation at +16, rotation quaternion at +48, flags at +64), using the key caches at
// [motion+100] (3 per bone) and [motion+120] (16 bytes per bone). For a time between two even
// frames, the motion is also evaluated at both even frames (state put back after each) and every
// bone gets the blend of the two console poses.
struct Saved {
  u32 va;
  std::vector<u8> bytes;
};

void Save(std::vector<Saved> &out, u32 va, u32 size) {
  if (!va || !size)
    return;
  Saved s{va, std::vector<u8>(size)};
  std::memcpy(s.bytes.data(), bd::mem::at<u8>(va), size);
  out.push_back(std::move(s));
}

void Restore(const std::vector<Saved> &saved) {
  for (const auto &s : saved)
    std::memcpy(bd::mem::at<u8>(s.va), s.bytes.data(), s.bytes.size());
}

u64 SvrFrame() { return bd::gpu::SvrFrameIndex(); }

// wwe12_title_fix_log: how far each motion's clock moves between two displayed frames.
void LogClock(u32 motion, float t) {
  struct Last {
    u64 frame;
    float t;
  };
  static std::mutex mu;
  static std::unordered_map<u32, Last> last;
  static std::map<int, u32> hist[2];
  static u64 report = 0;
  const u64 frame = SvrFrame();
  std::lock_guard lock(mu);
  auto &l = last[motion];
  if (l.frame + 1 == frame) {
    const int step = int(std::lround((t - l.t) * 100));
    if (step > -500 && step < 500)
      ++hist[frame & 1][step];
  }
  if (l.frame != frame)
    l = {frame, t};
  if (frame >= report + 120) {
    report = frame;
    for (int p = 0; p < 2; ++p) {
      std::string line;
      std::vector<std::pair<u32, int>> top;
      for (auto &[k, v] : hist[p])
        top.push_back({v, k});
      std::sort(top.rbegin(), top.rend());
      for (size_t i = 0; i < top.size() && i < 6; ++i)
        line += fmt::format(" {:.2f}x{}", top[i].second / 100.0, top[i].first);
      if (!line.empty())
        BD_INFO("[clock] {} frames: clock steps{}", p ? "odd" : "even", line);
      hist[p].clear();
    }
  }
}

void EvaluatePose(PPCContext &ctx, uint8_t *base) {
  const u32 motion = ctx.r3.u32;
  const float t = bd::mem::load<float>(motion + 52);
  if (REXCVAR_GET(wwe12_title_fix_log) && Wwe12EntranceAt60())
    LogClock(motion, t);
  const float even = 2.0f * std::floor(t / 2.0f);
  const float frac = (t - even) / 2.0f;
  const u32 bones = bd::mem::load<u32>(motion + 124);
  if (!REXCVAR_GET(wwe12_title_fix_pose) || frac < 1e-4f || t < 2.0f || bones == 0 ||
      bones > 512 || !Wwe12EntranceAt60()) {
    __imp__sub_82490530(ctx, base);
    return;
  }
  const u32 records = bd::mem::load<u32>(motion + 92);
  std::vector<Saved> saved;
  Save(saved, motion, 0x120);
  Save(saved, records, bones * 80);
  Save(saved, bd::mem::load<u32>(motion + 100), bones * 3 * 4);
  Save(saved, bd::mem::load<u32>(motion + 120), bones * 16);
  std::vector<float> pose[2];
  for (int k = 0; k < 2; ++k) {
    PPCContext c = ctx;
    bd::mem::store<float>(motion + 52, even + 2.0f * k);
    __imp__sub_82490530(c, base);
    pose[k].resize(bones * 7);
    for (u32 i = 0; i < bones; ++i) {
      const u32 r = records + i * 80;
      for (int j = 0; j < 3; ++j)
        pose[k][i * 7 + j] = bd::mem::load<float>(r + 16 + 4 * j);
      for (int j = 0; j < 4; ++j)
        pose[k][i * 7 + 3 + j] = bd::mem::load<float>(r + 48 + 4 * j);
    }
    Restore(saved);
  }
  __imp__sub_82490530(ctx, base);
  static std::atomic<u32> s_blends{0};
  if (REXCVAR_GET(wwe12_title_fix_log) && (s_blends.fetch_add(1) % 2000) == 0)
    BD_INFO("[pose] blend #{} motion {:08X} bones {} t {:.2f}", s_blends.load(), motion, bones, t);
  const float w = REXCVAR_GET(wwe12_title_fix_pose_hold) ? 0.0f : frac;
  for (u32 i = 0; i < bones; ++i) {
    if (bd::mem::load<int32_t>(bd::mem::load<u32>(motion + 76) + 4 * i) == -1)
      continue;
    const u32 r = records + i * 80;
    const float *a = &pose[0][i * 7], *b = &pose[1][i * 7];
    for (int j = 0; j < 3; ++j)
      bd::mem::store<float>(r + 16 + 4 * j, a[j] + (b[j] - a[j]) * w);
    float dot = 0, qa2 = 0, qb2 = 0;
    for (int j = 0; j < 4; ++j) {
      dot += a[3 + j] * b[3 + j];
      qa2 += a[3 + j] * a[3 + j];
      qb2 += b[3 + j] * b[3 + j];
    }
    // Only unit quaternions are blended (anything else in that slot is left as computed).
    if (std::fabs(qa2 - 1.0f) > 0.05f || std::fabs(qb2 - 1.0f) > 0.05f)
      continue;
    const float sign = dot < 0 ? -1.0f : 1.0f;
    float q[4], n = 0;
    for (int j = 0; j < 4; ++j) {
      q[j] = a[3 + j] + (sign * b[3 + j] - a[3 + j]) * w;
      n += q[j] * q[j];
    }
    n = 1.0f / std::sqrt(n);
    for (int j = 0; j < 4; ++j)
      bd::mem::store<float>(r + 48 + 4 * j, q[j] * n);
  }
}


// wwe12_arm_trap. Each bone record written by sub_82490530 is compared, at the motion's next
// evaluation, with what sub_82490530 left: a change means some later step (IK, attach) rewrote
// the record. The pages of the changed records are then guarded: every write into those records
// logs the writing instruction (host address, symbolized offline with wwe12.map).
namespace trap {

struct Range {
  u32 lo, hi;
};
constexpr int kMaxRanges = 64;
Range g_ranges[kMaxRanges];
std::atomic<int> g_nranges{0};
std::atomic<bool> g_armed{false};
u8 *g_guest = nullptr;

constexpr int kSlots = 512;
struct Hit {
  std::atomic<u64> rip{0};  // key: writing instruction ^ caller chain
  std::atomic<u32> count{0};
  std::atomic<u32> offmask{0};  // bit = record offset / 4
  u64 where[5] = {};            // writing instruction, then return addresses
};
Hit g_hits[kSlots];
thread_local uintptr_t t_pending = 0;

bool InRange(u32 va) {
  const int n = g_nranges.load(std::memory_order_acquire);
  for (int i = 0; i < n; ++i)
    if (va >= g_ranges[i].lo && va < g_ranges[i].hi)
      return true;
  return false;
}

u32 RangeOffset(u32 va) {
  const int n = g_nranges.load(std::memory_order_acquire);
  for (int i = 0; i < n; ++i)
    if (va >= g_ranges[i].lo && va < g_ranges[i].hi)
      return (va - g_ranges[i].lo) % 80;
  return 0xFFFF;
}

bool OurPage(uintptr_t host) {
  if (!g_guest || host < uintptr_t(g_guest) || host >= uintptr_t(g_guest) + 0x100000000ull)
    return false;
  const u32 page = u32(host - uintptr_t(g_guest)) & ~0xFFFu;
  const int n = g_nranges.load(std::memory_order_acquire);
  for (int i = 0; i < n; ++i)
    if (page + 0x1000 > g_ranges[i].lo && page < g_ranges[i].hi)
      return true;
  return false;
}

void Guard(u32 lo, u32 hi) {
  for (u32 p = lo & ~0xFFFu; p < hi; p += 0x1000) {
    DWORD old;
    VirtualProtect(g_guest + p, 0x1000, PAGE_READWRITE | PAGE_GUARD, &old);
  }
}

LONG CALLBACK Handler(EXCEPTION_POINTERS *ep) {
  const DWORD code = ep->ExceptionRecord->ExceptionCode;
  if (code == 0x80000001u) {  // STATUS_GUARD_PAGE_VIOLATION
    if (ep->ExceptionRecord->NumberParameters < 2)
      return EXCEPTION_CONTINUE_SEARCH;
    const uintptr_t addr = ep->ExceptionRecord->ExceptionInformation[1];
    if (!OurPage(addr))
      return EXCEPTION_CONTINUE_SEARCH;
    const u32 va = u32(addr - uintptr_t(g_guest));
    if (ep->ExceptionRecord->ExceptionInformation[0] == 1 && InRange(va)) {
      const u64 rip = ep->ContextRecord->Rip;
      // Callers: walk the faulting frame's stack with the unwind data.
      u64 where[5] = {rip, 0, 0, 0, 0};
      {
        CONTEXT c = *ep->ContextRecord;
        for (int d = 1; d < 5; ++d) {
          DWORD64 image = 0;
          PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(c.Rip, &image, nullptr);
          if (!fn)
            break;
          void *handler_data;
          DWORD64 frame;
          RtlVirtualUnwind(UNW_FLAG_NHANDLER, image, c.Rip, fn, &c, &handler_data, &frame,
                           nullptr);
          if (!c.Rip)
            break;
          where[d] = c.Rip;
        }
      }
      const u64 key = rip ^ (where[1] * 31) ^ (where[2] * 1009);
      u32 h = u32((key >> 2) * 2654435761u) % kSlots;
      for (int k = 0; k < kSlots; ++k, h = (h + 1) % kSlots) {
        u64 cur = g_hits[h].rip.load();
        if (cur == 0 && g_hits[h].rip.compare_exchange_strong(cur, key)) {
          std::memcpy(g_hits[h].where, where, sizeof where);
          cur = key;
        }
        if (cur == key) {
          g_hits[h].count.fetch_add(1);
          g_hits[h].offmask.fetch_or(1u << (RangeOffset(va) / 4 & 31));
          break;
        }
      }
    }
    if (g_armed.load()) {
      t_pending = addr & ~uintptr_t(0xFFF);
      ep->ContextRecord->EFlags |= 0x100;  // re-guard after this instruction
    }
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  if (code == 0x80000004u && t_pending) {  // STATUS_SINGLE_STEP
    if (g_armed.load()) {
      DWORD old;
      VirtualProtect(reinterpret_cast<void *>(t_pending), 0x1000, PAGE_READWRITE | PAGE_GUARD,
                     &old);
    }
    t_pending = 0;
    return EXCEPTION_CONTINUE_EXECUTION;
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

struct Snap {
  u32 records = 0, bones = 0;
  u64 frame = 0;
  std::vector<u8> data;
};
std::mutex g_mu;
std::unordered_map<u32, Snap> g_snaps;
u64 g_report = 0, g_first = 0;
u32 g_changed_bones = 0, g_motions_changed = 0;

void Before(u32 motion) {
  if (!REXCVAR_GET(wwe12_arm_trap) || !Wwe12EntranceAt60())
    return;
  static bool s_init = false;
  if (!s_init) {
    s_init = true;
    g_guest = bd::mem::at<u8>(0x40000000) - 0x40000000;
    AddVectoredExceptionHandler(1, &Handler);
  }
  const u64 frame = SvrFrame();
  std::lock_guard lock(g_mu);
  if (!g_first)
    g_first = frame;
  auto it = g_snaps.find(motion);
  if (it == g_snaps.end() || it->second.frame + 1 != frame)
    return;
  const Snap &sn = it->second;
  const u8 *now = bd::mem::at<u8>(sn.records);
  u32 changed = 0;
  for (u32 i = 0; i < sn.bones; ++i)
    if (std::memcmp(now + i * 80 + 16, sn.data.data() + i * 80 + 16, 12) != 0 ||
        std::memcmp(now + i * 80 + 48, sn.data.data() + i * 80 + 48, 16) != 0)
      ++changed;
  if (!changed)
    return;
  g_changed_bones += changed;
  ++g_motions_changed;
  // Guard this motion's records (once), after the 1st second of the entrance.
  if (frame > g_first + 60 && !InRange(sn.records)) {
    const int n = g_nranges.load();
    if (n < kMaxRanges) {
      g_ranges[n] = {sn.records, sn.records + sn.bones * 80};
      g_nranges.store(n + 1, std::memory_order_release);
      g_armed = true;
      Guard(sn.records, sn.records + sn.bones * 80);
      BD_INFO("[armtrap] guarding motion {:08X} records {:08X} bones {} ({} changed)", motion,
              sn.records, sn.bones, changed);
    }
  }
}

void After(u32 motion) {
  if (!REXCVAR_GET(wwe12_arm_trap) || !Wwe12EntranceAt60())
    return;
  const u32 bones = bd::mem::load<u32>(motion + 124);
  const u32 records = bd::mem::load<u32>(motion + 92);
  if (!records || !bones || bones > 512)
    return;
  const u64 frame = SvrFrame();
  std::lock_guard lock(g_mu);
  // Our own copy must not trip the guard.
  const bool guarded = InRange(records);
  if (guarded) {
    g_armed = false;
    for (u32 p = records & ~0xFFFu; p < records + bones * 80; p += 0x1000) {
      DWORD old;
      VirtualProtect(g_guest + p, 0x1000, PAGE_READWRITE, &old);
    }
  }
  Snap &sn = g_snaps[motion];
  sn.records = records;
  sn.bones = bones;
  sn.frame = frame;
  sn.data.assign(bd::mem::at<u8>(records), bd::mem::at<u8>(records) + bones * 80);
  if (guarded) {
    g_armed = true;
    Guard(records, records + bones * 80);
  }
  if (frame >= g_report + 120) {
    g_report = frame;
    BD_INFO("[armtrap] {} motions / {} bones rewritten after sub_82490530 in 2 s",
            g_motions_changed, g_changed_bones);
    g_motions_changed = g_changed_bones = 0;
    const uintptr_t exe = uintptr_t(GetModuleHandleW(nullptr));
    for (auto &h : g_hits) {
      const u32 c = h.count.exchange(0);
      if (!h.rip.load() || !c)
        continue;
      std::string chain;
      for (u64 w : h.where)
        if (w)
          chain += fmt::format(" {:X}", w >= exe && w < exe + 0x20000000 ? w - exe : w);
      std::string offs;
      const u32 m = h.offmask.exchange(0);
      for (int b = 0; b < 20; ++b)
        if (m & (1u << b))
          offs += fmt::format(" +{}", b * 4);
      BD_INFO("[armtrap] writes x{} offsets{} chain(exe+){}", c, offs, chain);
    }
  }
}

}  // namespace trap

}  // namespace

extern "C" REX_FUNC(sub_82490530) {
  const u32 motion = ctx.r3.u32;
  trap::Before(motion);
  EvaluatePose(ctx, base);
  trap::After(motion);
}

extern "C" REX_FUNC(sub_8248B898) { Sample(ctx, base, __imp__sub_8248B898); }

extern "C" REX_FUNC(sub_8248B458) { Sample(ctx, base, __imp__sub_8248B458); }

// wwe12_arm_trap: sub_82490268(motion r3, clip r4/r5, ..., time f1) restarts a motion at a given
// time; log the times it is given per motion, frame after frame.
extern "C" REX_FUNC(sub_82490268) {
  if (REXCVAR_GET(wwe12_arm_trap) && Wwe12EntranceAt60()) {
    static std::mutex mu;
    static std::unordered_map<u32, std::string> lines;
    static u64 last_report = 0;
    const u64 frame = bd::gpu::SvrFrameIndex();
    std::lock_guard lock(mu);
    auto &l = lines[ctx.r3.u32];
    l += fmt::format(" {}:{:.2f}", frame % 1000, ctx.f1.f64);
    if (l.size() > 600) {
      BD_INFO("[restart] motion {:08X} bones {} lr {:08X} r6 {:08X} r7 {:08X}:{}", ctx.r3.u32,
              bd::mem::load<u32>(ctx.r3.u32 + 124), u32(ctx.lr), ctx.r6.u32, ctx.r7.u32, l);
      l.clear();
    }
    (void)last_report;
  }
  __imp__sub_82490268(ctx, base);
}
