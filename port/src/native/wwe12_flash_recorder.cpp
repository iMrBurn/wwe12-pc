// Dev (1.3): a recorder for the one-frame black picture before the bell, which nobody can pause on.
// wwe12_flash_recorder = true: every presented frame's picture (the front buffer, before the present
// pass) is copied on the GPU into a ring of kRing textures (cheap: one copy per frame, no wait). F11
// in game saves the ring, oldest first, as logs\flash\<n>_<frame>.bmp (quarter size) with one log line
// per picture: frame, front buffer address, average brightness ("[flash]"). The player presses F11
// right after seeing the flash; the black picture is among the ~0.7 s before.

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/runtime.h>

#include "core/logging.h"
#include "gpu/device.h"
#include "gpu/host_resource_heap.h"

REXCVAR_DEFINE_BOOL(wwe12_flash_recorder, false, "WWE12",
                    "Dev: keep the last 40 presented pictures; F11 saves them (logs\\flash).");

namespace bd::gpu {
GuestTexture *SvrCreateHostTexture(u32 width, u32 height, u32 depth, u32 levels, u32 usage, u32 format,
                                   u32 d3d_type);  // hooks/resource.cpp
}

namespace {
constexpr u32 kRing = 40;
constexpr u32 kGuestRgba8 = 0xC0000000u | 6u;  // k_8_8_8_8, as svr_resources.cpp tags it
struct Slot {
  bd::gpu::GuestTexture *tex = nullptr;
  u64 frame = 0;
  u32 front_va = 0;
  bool filled = false;
};
std::array<Slot, kRing> g_ring;
u32 g_next = 0;
u64 g_frame = 0;

void SaveBmp(const std::filesystem::path &file, const std::vector<u8> &pixels, u32 row_pitch, u32 w, u32 h,
             bool rgba, double &average) {
  constexpr u32 kStep = 4;  // quarter size
  const u32 ow = w / kStep, oh = h / kStep, bmp_row = (ow * 3 + 3) & ~3u;
  std::vector<u8> bmp(54 + size_t(bmp_row) * oh, 0);
  const auto put32 = [&bmp](size_t at, u32 v) { std::memcpy(&bmp[at], &v, 4); };
  bmp[0] = 'B', bmp[1] = 'M';
  put32(2, u32(bmp.size()));
  put32(10, 54);
  put32(14, 40);
  put32(18, ow);
  put32(22, oh);
  bmp[26] = 1, bmp[28] = 24;
  put32(34, u32(bmp.size() - 54));
  double sum = 0;
  for (u32 y = 0; y < oh; ++y) {
    u8 *dst = bmp.data() + 54 + size_t(oh - 1 - y) * bmp_row;
    for (u32 x = 0; x < ow; ++x, dst += 3) {
      const u8 *src = pixels.data() + size_t(y * kStep) * row_pitch + size_t(x * kStep) * 4;
      dst[0] = rgba ? src[2] : src[0];
      dst[1] = src[1];
      dst[2] = rgba ? src[0] : src[2];
      sum += (src[0] + src[1] + src[2]) / 3.0;
    }
  }
  average = ow && oh ? sum / (double(ow) * oh) : 0;
  std::ofstream(file, std::ios::binary).write(reinterpret_cast<const char *>(bmp.data()), std::streamsize(bmp.size()));
}

void SaveRing() {
  std::filesystem::path dir;
  if (auto *rt = rex::Runtime::instance())
    dir = rt->user_data_root();
  dir = (dir.empty() ? rex::filesystem::GetExecutableFolder() : dir) / "logs" / "flash";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  BD_INFO("[flash] F11: saving the last {} pictures to {}", kRing, rex::path_to_utf8(dir));
  for (u32 i = 0; i < kRing; ++i) {
    const Slot &s = g_ring[(g_next + i) % kRing];
    if (!s.filled || !s.tex)
      continue;
    std::vector<u8> pixels;
    u32 row_pitch = 0;
    if (!bd::gpu::Video::ReadbackTextureLevel(s.tex, 0, 0, pixels, row_pitch))
      continue;
    char name[64];
    std::snprintf(name, sizeof(name), "%02u_%llu.bmp", i, static_cast<unsigned long long>(s.frame));
    double average = 0;
    SaveBmp(dir / name, pixels, row_pitch, s.tex->width, s.tex->height,
            s.tex->format != plume::RenderFormat::B8G8R8A8_UNORM, average);
    BD_INFO("[flash] {:02} frame {} front buffer 0x{:08X}: average brightness {:.1f}", i, s.frame, s.front_va,
            average);
  }
}
}  // namespace

// Each presented frame (wwe12_remote_pad.cpp's Wwe12RemoteOnPresent, before the renderer's lock).
void Wwe12FlashRecorderFrame(bd::gpu::GuestTexture *front_buffer) {
  if (!REXCVAR_GET(wwe12_flash_recorder) || !front_buffer)
    return;
  ++g_frame;
  if (GetAsyncKeyState(VK_F11) & 1) {
    SaveRing();
    return;
  }
  Slot &s = g_ring[g_next];
  if (s.tex && (s.tex->width != front_buffer->width || s.tex->height != front_buffer->height))
    s.tex = nullptr;  // (a new size: a new texture; the old one is left, dev only)
  if (!s.tex)
    s.tex = bd::gpu::SvrCreateHostTexture(front_buffer->width, front_buffer->height, 1, 1, 0, kGuestRgba8, 0);
  if (!s.tex || !bd::gpu::Video::SeedTextureBaseLevel(s.tex, front_buffer)) {
    s.filled = false;
    return;
  }
  s.frame = g_frame;
  s.front_va = bd::gpu::HostResourceHeap::ToGuest(front_buffer);
  s.filled = true;
  g_next = (g_next + 1) % kRing;
}
