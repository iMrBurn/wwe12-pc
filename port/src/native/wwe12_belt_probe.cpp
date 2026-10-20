// Dev probe: the championship title's bone matrices per frame (60 fps entrance bug).
//
// The title is its own small skinned model drawn with vertex shader FC173F716C3D2065 (found in a
// RenderDoc capture of Ezekiel Jackson's entrance, 2026-10-18); like every skinned shader of the
// game, its bones are 4x3 matrices from vertex constant c76 (three float4 rows each, the
// translation in the rows' w). wwe12_belt_probe logs, for each draw with that shader, the frame,
// the index count and the translation of its first bones, to see whether they zigzag every other
// frame at 60 fps (bad odd animation keys, as in the WWE 2K14 recomp) or move smoothly.

#include <cstddef>
#include <cstring>
#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdlib>

#include <fmt/format.h>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "gpu/d3d.h"

REXCVAR_DEFINE_BOOL(wwe12_belt_probe, false, "WWE12",
                    "Dev: log the championship title's bone positions each frame.");
REXCVAR_DEFINE_INT32(wwe12_belt_hide_idx, 0, "WWE12",
                     "Dev: skip the draws of vertex shader FC173F716C3D2065 with this index count "
                     "(to see which model it is). 0 = none.");

REXCVAR_DEFINE_STRING(wwe12_belt_hide_models, "", "WWE12",
                      "Dev: skip the draws of these wwe12_belt_scan models (hex keys, comma "
                      "separated), to see which model a key is.");
REXCVAR_DEFINE_BOOL(wwe12_belt_scan, false, "WWE12",
                    "Dev: find skinned models that move at 30 Hz or zigzag every other frame.");

namespace bd::gpu {
u64 SvrFrameIndex();
}

namespace {

// wwe12_belt_scan: per (vertex shader, index count), the first bones' translations each frame.
struct ScanStat {
  u64 last_frame = 0;
  float last[4][12] = {};
  bool have_last = false, have_prev = false;
  float prev[4][12] = {};
  u32 n = 0, same = 0;
  double dev[2] = {0, 0};
  u32 devn[2] = {0, 0};
};
std::unordered_map<u64, ScanStat> g_scan;
u64 g_scan_report = 0;

void Scan(u32 device_guest, u64 vs_hash, u32 count) {
  const u64 frame = bd::gpu::SvrFrameIndex();
  const u32 c76 = device_guest + u32(offsetof(bd::gpu::D3DDevice, vsFloatConstants)) + 76 * 16;
  const auto f = [](u32 va) {
    const u32 bits = bd::mem::load<u32>(va);
    float v;
    std::memcpy(&v, &bits, 4);
    return v;
  };
  float cur[4][12];
  for (u32 b = 0; b < 4; ++b)
    for (u32 a = 0; a < 12; ++a)
      cur[b][a] = f(c76 + (3 * b + a / 4) * 16 + (a % 4) * 4) * ((a % 4) == 3 ? 1.0f : 30.0f);
  ScanStat &st = g_scan[vs_hash ^ (u64(count) << 1)];
  if (st.last_frame == frame)
    return;  // first draw of this model in the frame only
  if (st.have_last && st.last_frame + 1 == frame) {
    ++st.n;
    if (std::memcmp(cur, st.last, sizeof cur) == 0)
      ++st.same;
    if (st.have_prev) {
      // Bone 0..3: distance of last frame from the midpoint of its neighbours.
      double d = 0;
      for (u32 b = 0; b < 4; ++b)
        for (u32 a = 0; a < 12; ++a) {
          const double m = st.last[b][a] - (st.prev[b][a] + cur[b][a]) / 2;
          d += m * m;
        }
      st.dev[st.last_frame & 1] += std::sqrt(d);
      ++st.devn[st.last_frame & 1];
    }
    std::memcpy(st.prev, st.last, sizeof cur);
    st.have_prev = true;
  } else {
    st.have_prev = false;
  }
  std::memcpy(st.last, cur, sizeof cur);
  st.have_last = true;
  st.last_frame = frame;
  if (frame >= g_scan_report + 300) {
    g_scan_report = frame;
    for (auto &[key, s] : g_scan) {
      if (s.n >= 100) {
        const double e = s.devn[0] ? s.dev[0] / s.devn[0] : 0,
                     o = s.devn[1] ? s.dev[1] / s.devn[1] : 0;
        const double samef = double(s.same) / s.n;
        if ((samef > 0.25 && samef < 0.75) || (std::max(e, o) > 0.3 && std::max(e, o) > 2 * std::min(e, o)))
          BD_INFO("[scan] model {:016X} n={} same={:.0f}% dev even={:.2f} odd={:.2f}", key, s.n,
                  100 * samef, e, o);
      }
      s.n = s.same = 0;
      s.dev[0] = s.dev[1] = 0;
      s.devn[0] = s.devn[1] = 0;
    }
  }
}

}  // namespace

// Returns true when the draw is to be skipped (wwe12_belt_hide_idx).
bool Wwe12BeltProbe(u32 device_guest, u64 vs_hash, u32 count, u32 start) {
  {
    static std::string s_text;
    static std::vector<u64> s_keys;
    const std::string text = REXCVAR_GET(wwe12_belt_hide_models);
    if (text != s_text) {
      s_text = text;
      s_keys.clear();
      size_t p = 0;
      while (p < text.size()) {
        size_t e = text.find(',', p);
        if (e == std::string::npos)
          e = text.size();
        s_keys.push_back(std::strtoull(text.substr(p, e - p).c_str(), nullptr, 16));
        p = e + 1;
      }
    }
    const u64 key = vs_hash ^ (u64(count) << 1);
    for (u64 k : s_keys)
      if (k == key)
        return true;
  }
  if (vs_hash == 0xFC173F716C3D2065ull && REXCVAR_GET(wwe12_belt_hide_idx) > 0 &&
      count == u32(REXCVAR_GET(wwe12_belt_hide_idx)))
    return true;
  if (REXCVAR_GET(wwe12_belt_scan))
    Scan(device_guest, vs_hash, count);
  if (!REXCVAR_GET(wwe12_belt_probe))
    return false;
  static bool s_announced = false;
  if (!s_announced) {
    s_announced = true;
    BD_INFO("[belt] probe on (first draw: vs 0x{:016X})", vs_hash);
  }
  if (vs_hash != 0xFC173F716C3D2065ull)
    return false;
  const u32 c76 = device_guest + u32(offsetof(bd::gpu::D3DDevice, vsFloatConstants)) + 76 * 16;
  const auto f = [](u32 va) {
    const u32 bits = bd::mem::load<u32>(va);
    float v;
    std::memcpy(&v, &bits, 4);
    return v;
  };
  // Bone b: rows at c76 + 3b, translation = (row0.w, row1.w, row2.w).
  const auto t = [&](u32 b, u32 axis) { return f(c76 + (3 * b + axis) * 16 + 12); };
  // All 11 bones the title's shader can use (c76..c108).
  std::string line;
  for (u32 b = 0; b < 11; ++b)
    line += fmt::format(" b{}=({:.3f},{:.3f},{:.3f})", b, t(b, 0), t(b, 1), t(b, 2));
  BD_INFO("[belt] f={} idx={} start={}{}", bd::gpu::SvrFrameIndex(), count, start, line);
  return false;
}
