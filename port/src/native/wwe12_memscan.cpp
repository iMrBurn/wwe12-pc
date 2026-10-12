// Dev-only: compiled only with the WWE12_DEV_PROBES environment variable at configure time.
// F7 in game scans guest memory for the string ids of the GAMEPLAY > IN-GAME OPTIONS page rows
// (DIFFICULTY 41233, ADRENALINE 41235, REVERSAL DAMAGE 41505, BLOOD 41506), stored big- or
// little-endian as u32 or u16, logs where they sit close together and dumps 16 KB around each
// cluster to Documents\wwe12\logs\optscan_<n>.bin (first 8 bytes: guest address, then the bytes).
// Used to find how the game describes an option page, to build a native GRAPHICS page.
#if defined(WWE12_DEV_PROBES)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <thread>
#include <vector>

#include "core/logging.h"
#include "core/memory_helpers.h"

namespace {

constexpr uint32_t kIds[] = {41233, 41235, 41505, 41506};

void Scan(int run) {
  u8 *const base = bd::mem::at<u8>(0x40000000) - 0x40000000;
  struct Hit { u32 va; int id; const char *fmt; };
  std::vector<Hit> hits;
  for (const auto [lo, hi] : {std::pair<u32, u32>{0x00010000u, 0x9FFF0000u},
                              std::pair<u32, u32>{0xA0000000u, 0xC0000000u}}) {
    u8 *p = base + lo;
    u8 *const end = base + hi;
    while (p < end) {
      MEMORY_BASIC_INFORMATION mbi;
      if (!VirtualQuery(p, &mbi, sizeof(mbi)))
        break;
      u8 *rs = static_cast<u8 *>(mbi.BaseAddress);
      u8 *re = std::min(rs + mbi.RegionSize, end);
      const bool readable = mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD));
      if (readable) {
        for (u8 *q = std::max(p, rs); q + 4 <= re; q += 2) {
          const u16 be16 = u16(q[0] << 8 | q[1]), le16 = u16(q[1] << 8 | q[0]);
          for (int i = 0; i < 4; ++i) {
            const u32 id = kIds[i];
            if (be16 == id || le16 == id)
              hits.push_back({u32(q - base), i, be16 == id ? "be16" : "le16"});
          }
        }
      }
      p = re;
    }
  }
  // Clusters: at least 3 different ids within 0x800 bytes.
  int dumped = 0;
  for (size_t a = 0; a < hits.size() && dumped < 12; ++a) {
    bool seen[4] = {};
    int kinds = 0;
    size_t b = a;
    for (; b < hits.size() && hits[b].va - hits[a].va < 0x800; ++b)
      if (!seen[hits[b].id]) { seen[hits[b].id] = true; ++kinds; }
    if (kinds < 3)
      continue;
    std::string line;
    for (size_t c = a; c < b; ++c)
      line += fmt::format(" {:08X}:{}({})", hits[c].va, kIds[hits[c].id], hits[c].fmt);
    BD_INFO("[optscan] run {} cluster {}:{}", run, dumped, line);
    PWSTR docs = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs))) {
      const auto path = std::filesystem::path(docs) / "wwe12" / "logs" /
                        fmt::format("optscan_{}_{}.bin", run, dumped);
      CoTaskMemFree(docs);
      if (FILE *f = _wfopen(path.c_str(), L"wb")) {
        const u32 start = (hits[a].va & ~0xFFFu) - 0x2000u;
        std::fwrite(&start, 4, 1, f);
        for (u32 off = 0; off < 0x4000; ++off) {
          const u8 *bptr = base + start + off;
          MEMORY_BASIC_INFORMATION m2;
          u8 v = 0;
          if (VirtualQuery(bptr, &m2, sizeof(m2)) && m2.State == MEM_COMMIT &&
              !(m2.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
            v = *bptr;
          std::fwrite(&v, 1, 1, f);
        }
        std::fclose(f);
      }
    }
    ++dumped;
    a = b;
  }
  BD_INFO("[optscan] run {}: {} raw hits, {} clusters dumped", run, hits.size(), dumped);
}

struct Starter {
  Starter() {
    std::thread([] {
      int run = 0;
      bool was = false;
      for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const bool down = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
        if (down && !was) {
          BD_INFO("[optscan] F7: scanning guest memory");
          Scan(++run);
        }
        was = down;
      }
    }).detach();
  }
} g_starter;

}  // namespace

#endif  // WWE12_DEV_PROBES
