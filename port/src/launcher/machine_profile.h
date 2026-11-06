// "Steam Deck / small PC" detection and the settings it proposes. The launcher offers the profile once, at
// start-up, never applies it by itself (launcher.cpp, DrawProfileOffer), and the player can undo it from
// SETTINGS like the low-end preset (one logic: ApplyPreset there).
//
// What is looked at: the SteamDeck environment variable (Steam sets it on the Deck and Proton passes it on, as
// the game's own SvrOnSteamDeck does), the graphics card's name (the Deck's chip is "AMD Custom GPU 0405" /
// "... 0932", RADV calls it VANGOGH), Wine / Proton (ntdll exports wine_get_version), integrated or small
// graphics memory, physical cores, RAM and a battery (laptop). The processor limits this game more than the
// graphics card, so few cores and little RAM count as much as a weak card.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dxgi.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

namespace machine {

struct Facts {
  bool deck_env = false;  // SteamDeck=1
  bool wine = false;      // Wine / Proton (not Windows)
  bool battery = false;   // a laptop or handheld
  int cores = 0;          // physical cores
  int threads = 0;
  unsigned ram_gb = 0;
  // Every adapter DXGI lists: name and dedicated video memory. Used only for the card the game will use, found
  // by its name (the memory of "any adapter" is not reliable on laptops with two cards).
  struct Adapter {
    std::string name;
    unsigned vram_mb = 0;
  };
  std::vector<Adapter> adapters;
};

inline Facts Detect() {
  Facts f;
  const char *sd = std::getenv("SteamDeck");
  f.deck_env = sd && sd[0] == '1';
  const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  f.wine = ntdll && GetProcAddress(ntdll, "wine_get_version") != nullptr;
  SYSTEM_POWER_STATUS ps{};
  // BatteryFlag 128 = no battery, 255 = unknown.
  f.battery = GetSystemPowerStatus(&ps) && ps.BatteryFlag != 128 && ps.BatteryFlag != 255;
  f.threads = int(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
  DWORD len = 0;
  GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &len);
  if (len) {
    std::vector<char> buf(len);
    if (GetLogicalProcessorInformationEx(RelationProcessorCore,
                                         reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(buf.data()),
                                         &len)) {
      for (DWORD off = 0; off < len;) {
        const auto *p = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *>(buf.data() + off);
        if (p->Size == 0)
          break;
        ++f.cores;
        off += p->Size;
      }
    }
  }
  if (f.cores == 0)
    f.cores = f.threads;
  MEMORYSTATUSEX m{sizeof(m)};
  if (GlobalMemoryStatusEx(&m))
    f.ram_gb = unsigned((m.ullTotalPhys + (512ull << 20)) >> 30);
  IDXGIFactory1 *factory = nullptr;
  if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void **>(&factory)))) {
    IDXGIAdapter1 *adapter = nullptr;
    for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
      DXGI_ADAPTER_DESC1 d{};
      adapter->GetDesc1(&d);
      if (!(d.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
        char name[256] = {};
        WideCharToMultiByte(CP_UTF8, 0, d.Description, -1, name, int(sizeof(name)) - 1, nullptr, nullptr);
        f.adapters.push_back({name, unsigned(d.DedicatedVideoMemory >> 20)});
      }
      adapter->Release();
    }
    factory->Release();
  }
  return f;
}

enum class Kind { None, Deck, Small };

struct Profile {
  Kind kind = Kind::None;
  int scale = 0;            // svr_render_scale (a height; 0 = automatic)
  bool crowd = true;
  bool shadows = true;
  bool dof = true;
  bool widescreen = true;   // wwe12_widescreen (the 16:10 fill)
  int api = -1;             // gpu_backend as the launcher numbers it (1 Vulkan); -1 = leave as is
  int score = 0;            // how weak the machine looks (Small only)
};

inline bool Contains(std::string haystack, const char *needle) {
  std::transform(haystack.begin(), haystack.end(), haystack.begin(), [](unsigned char c) { return char(std::toupper(c)); });
  std::string n = needle;
  std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return char(std::toupper(c)); });
  return haystack.find(n) != std::string::npos;
}

// gpu_name / gpu_kind: the card the game will use, as the launcher shows it (kind: 1 dedicated, 2 integrated,
// 0 unknown or none).
inline Profile Choose(const Facts &f, const std::string &gpu_name, int gpu_kind) {
  Profile p;
  const bool deck = f.deck_env || Contains(gpu_name, "CUSTOM GPU 0405") || Contains(gpu_name, "CUSTOM GPU 0932") ||
                    Contains(gpu_name, "VANGOGH") || Contains(gpu_name, "VAN GOGH") || Contains(gpu_name, "STEAM DECK");
  if (deck) {
    // 800p 16:10 screen, 4 cores at about 15 W: 720p is what the game picks by itself there; the rest is for speed.
    p.kind = Kind::Deck;
    p.scale = 720;
    p.crowd = p.shadows = p.dof = false;
    p.widescreen = true;
    p.api = 1;  // Vulkan goes straight to the driver; DirectX goes through a translation layer
    return p;
  }
  unsigned vram_mb = 0;  // of the card the game will use; 0 = not found by name, then not counted
  for (const Facts::Adapter &a : f.adapters)
    if (!gpu_name.empty() && !a.name.empty() && (Contains(a.name, gpu_name.c_str()) || Contains(gpu_name, a.name.c_str())))
      vram_mb = std::max(vram_mb, a.vram_mb);
  int score = 0;
  if (gpu_kind == 2)
    score += 2;  // graphics built into the processor
  if (gpu_kind == 1 && vram_mb && vram_mb < 3584)
    score += 2;  // a small dedicated card (Rendu's own limit)
  if (f.cores && f.cores <= 4)
    ++score;
  if (f.ram_gb && f.ram_gb <= 8)
    ++score;
  if (score < 2)
    return p;  // a normal PC: nothing to offer
  p.kind = Kind::Small;
  p.score = score;
  p.scale = (score >= 5 || (f.ram_gb && f.ram_gb <= 4)) ? 480 : score >= 3 ? 540 : 720;  // one weak point: 720p (the game's own pick for integrated graphics)
  p.crowd = score < 3;  // the crowd is kept only on machines that are weak on one side
  p.shadows = false;
  p.dof = false;
  p.widescreen = true;
  p.api = f.wine ? 1 : -1;  // under Wine / Proton: Vulkan (DirectX would be translated again)
  return p;
}

}  // namespace machine
