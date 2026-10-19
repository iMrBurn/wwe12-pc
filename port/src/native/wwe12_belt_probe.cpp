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
#include <string>

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

namespace bd::gpu {
u64 SvrFrameIndex();
}

// Returns true when the draw is to be skipped (wwe12_belt_hide_idx).
bool Wwe12BeltProbe(u32 device_guest, u64 vs_hash, u32 count, u32 start) {
  if (vs_hash == 0xFC173F716C3D2065ull && REXCVAR_GET(wwe12_belt_hide_idx) > 0 &&
      count == u32(REXCVAR_GET(wwe12_belt_hide_idx)))
    return true;
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
