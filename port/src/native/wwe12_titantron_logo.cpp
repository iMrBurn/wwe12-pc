// The show's logo on the stage's big screen during a match (2026-11-27, Rendu; iMrBurn: "l'image du titantron",
// like SvR 2011's RAW logo on its titantron in a match).
//
// In a match the game draws the stage screen (model 800, its picture "titantron_movie") with a copy of the match
// frame (the "mirror" iMrBurn hated; hooks/draw.cpp, cvar wwe12_live_screen). Every WWE '12 arena still carries its
// show's screen picture, "<show>_titan" (sd_titan, raw_titan, noc_titan...), in the arena's main picture bundle
// (bgNN.pac, child 400) - most of them used by no model (offline study of bg00-bg23, 2026-11-27). When the game loads
// an arena (sub_823BEED0, wwe12_arena_pages.cpp's hook calls Wwe12TitantronLogoStage), that picture is read from the
// file as the game gets it (mods, added arenas served in the slot), decoded, and made a host texture with its mips;
// hooks/draw.cpp gives it to the screen's draw instead of the frame copy. RAW's 800 shows only u, v 0.185-0.815 of
// its picture (the middle of the logo).

#include <rex/cvar.h>
#include <rex/system/kernel_state.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <span>
#include <atomic>
#include <chrono>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include <rex/filesystem/entry.h>
#include <rex/filesystem/file.h>
#include <rex/filesystem/vfs.h>

#include <fmt/format.h>

#include "core/logging.h"
#include "gpu/device.h"
#include "svr_hook.h"

#include "../modmaker/svrfmt/archive.h"
#include "../modmaker/svrfmt/texture.h"

REXCVAR_DEFINE_BOOL(wwe12_titantron_logo, true, "WWE12",
                    "In a match, the stage's big screen shows the show's logo (when the arena has one) instead of "
                    "nothing (with wwe12_live_screen off).");

namespace bd::gpu {
GuestTexture *SvrCreateHostTexture(u32 width, u32 height, u32 depth, u32 levels, u32 usage, u32 format,
                                   u32 d3d_type);  // hooks/resource.cpp
}

// hooks/draw.cpp: the logo of the arena loaded last (null: none, the screen draw is skipped).
std::atomic<bd::gpu::GuestTexture *> g_wwe12_titan_logo{nullptr};

namespace {

constexpr u32 kGuestRgba8 = 0xC0000000u | 6u;  // k_8_8_8_8, as svr_resources.cpp tags it

// The game's arenas by stage (= bgNN.pac): the screen picture's name (the study's table; bg05's logo is the "_l"
// one, its "f4w_titan" a plain W; bg10 Bragging Rights keeps Over the Limit's name; bg20 and bg23 have no screen).
const char *KnownLogo(u32 stage) {
  static const char *const kNames[] = {"sd_titan",    "raw_titan",   "nxt_titan", "wss_titan", "otl_titan",
                                       "f4w_titan_l", "mib_titan",   "ss_titan",  "noc_titan", "hic_titan",
                                       "otl_titan",   "sv_titan",    "tlc_titan0", "rr_titan_03", "elc_titan4",
                                       "wm_titan",    "er_titan",    "wcw_titan0", "coc_titan", "stc_titan",
                                       nullptr,       "elc_titan4",  "rr_titan_03", nullptr};
  return stage < sizeof(kNames) / sizeof(kNames[0]) ? kNames[stage] : nullptr;
}

std::string Lower(std::string s) {
  for (auto &c : s)
    c = char(std::tolower(unsigned(c)));
  return s;
}

bool ReadGame(const std::string &rel, std::vector<uint8_t> &out) {
  auto *vfs = REX_KERNEL_FS();
  if (!vfs)
    return false;
  std::string path = "game:\\" + rel;
  for (auto &c : path)
    if (c == '/')
      c = '\\';
  rex::filesystem::Entry *entry = vfs->ResolvePath(path);
  rex::filesystem::File *file = nullptr;
  if (!entry || entry->Open(rex::filesystem::FileAccess::kFileReadData, &file) != 0 || !file)
    return false;
  out.resize(entry->size());
  size_t read = 0;
  const bool ok = file->ReadSync(std::span<uint8_t>(out.data(), out.size()), 0, &read) == 0 && read == out.size();
  file->Destroy();
  return ok;
}

// The texture bundle of child 400 (searched a few levels down), and from it the logo: the known name, else (an added
// arena) the largest "*_titan*" picture that isn't a "_l" strip.
bool FindLogo(svrfmt::ArcNode &n, const char *known, svrfmt::Image &out, std::string &name, int depth) {
  if (depth > 4)
    return false;
  if (n.kind != svrfmt::ArcKind::Leaf || !n.parsed)
    svrfmt::ArchiveExpand(n);
  for (auto &k : n.kids) {
    const std::string kind = svrfmt::ArchiveKindOf(k);
    if (k.id == 400 && kind == "textures") {
      std::vector<svrfmt::BundleTexture> texs;
      if (!svrfmt::BundleRead(svrfmt::ArchiveData(k), texs))
        continue;
      const svrfmt::BundleTexture *pick = nullptr;
      size_t pick_size = 0;
      for (auto &t : texs) {
        const std::string l = Lower(t.name);
        if (known && l == known) {
          pick = &t;
          break;
        }
        if (!known && l.find("_titan") != std::string::npos &&
            !(l.size() > 2 && l.compare(l.size() - 2, 2, "_l") == 0) && t.data.size() > pick_size) {
          pick = &t;
          pick_size = t.data.size();
        }
      }
      if (pick && svrfmt::DdsDecode(pick->data, out) && out.w > 0 && out.h > 0) {
        name = pick->name;
        return true;
      }
    } else if (kind == "PACH" || kind == "EPAC" || kind == "EPK8") {
      if (FindLogo(k, known, out, name, depth + 1))
        return true;
    }
  }
  return false;
}

// The host texture, with its mips (box filter), from RGBA rows.
bd::gpu::GuestTexture *MakeTexture(const svrfmt::Image &img) {
  u32 w = u32(img.w), h = u32(img.h), levels = 1;
  while ((w >> levels) >= 1 && (h >> levels) >= 1 && levels < 12)
    ++levels;
  auto *tex = bd::gpu::SvrCreateHostTexture(w, h, 1, levels, 0, kGuestRgba8, 0);
  if (!tex)
    return nullptr;
  std::vector<uint8_t> prev = img.rgba;
  u32 pw = w, ph = h;
  for (u32 level = 0; level < tex->mipLevels; ++level) {
    const u32 lw = std::max(1u, w >> level), lh = std::max(1u, h >> level);
    std::vector<uint8_t> cur(size_t(lw) * lh * 4);
    if (level == 0) {
      cur = prev;
    } else {
      for (u32 y = 0; y < lh; ++y)
        for (u32 x = 0; x < lw; ++x)
          for (int c = 0; c < 4; ++c) {
            const auto px = [&](u32 xx, u32 yy) {
              return u32(prev[(size_t(std::min(yy, ph - 1)) * pw + std::min(xx, pw - 1)) * 4 + c]);
            };
            cur[(size_t(y) * lw + x) * 4 + c] =
                uint8_t((px(2 * x, 2 * y) + px(2 * x + 1, 2 * y) + px(2 * x, 2 * y + 1) + px(2 * x + 1, 2 * y + 1) + 2) / 4);
          }
    }
    const u32 pitch = (lw * 4u + 255u) & ~255u;
    std::vector<uint8_t> rows(size_t(pitch) * lh);
    for (u32 y = 0; y < lh; ++y)
      std::memcpy(&rows[size_t(y) * pitch], &cur[size_t(y) * lw * 4], size_t(lw) * 4);
    if (!bd::gpu::Video::UploadTextureLevel(tex, level, rows.data(), pitch))
      break;
    prev = std::move(cur);
    pw = lw, ph = lh;
  }
  return tex;
}

// Logos already made, by stage and file size (an added arena plays on its base's stage with another file).
std::map<std::pair<u32, size_t>, bd::gpu::GuestTexture *> g_made;

}  // namespace

// From wwe12_arena_pages.cpp's sub_823BEED0 hook (the game thread, as an arena starts loading): the stage's logo.
void Wwe12TitantronLogoStage(u32 stage) {
  g_wwe12_titan_logo = nullptr;
  if (!REXCVAR_GET(wwe12_titantron_logo) || stage > 99)
    return;
  const auto t0 = std::chrono::steady_clock::now();
  char rel[32];
  std::snprintf(rel, sizeof rel, "pac/bg/bg%02u.pac", stage);
  std::vector<uint8_t> file;
  if (!ReadGame(rel, file)) {
    BD_INFO("[titantron] {}: not read", rel);
    return;
  }
  const auto key = std::make_pair(stage, file.size());
  if (auto it = g_made.find(key); it != g_made.end()) {
    g_wwe12_titan_logo = it->second;
    return;
  }
  svrfmt::Archive a;
  svrfmt::Image img;
  std::string name;
  const char *known = KnownLogo(stage);
  bool found = svrfmt::ArchiveOpen(file, a, nullptr) && FindLogo(a.root, known, img, name, 0);
  if (!found && known) {  // (an added arena on this stage: its own names)
    svrfmt::Archive b;
    found = svrfmt::ArchiveOpen(file, b, nullptr) && FindLogo(b.root, nullptr, img, name, 0);
  }
  bd::gpu::GuestTexture *tex = found ? MakeTexture(img) : nullptr;
  g_made[key] = tex;
  g_wwe12_titan_logo = tex;
  BD_INFO("[titantron] stage {}: {} ({} ms)", stage,
          tex ? fmt::format("logo \"{}\" {}x{}", name, img.w, img.h) : std::string("no logo"),
          std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count());
}
