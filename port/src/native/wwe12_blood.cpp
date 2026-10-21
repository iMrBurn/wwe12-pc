// WWE '12 PC: blood for female wrestlers (work in progress).
//
// Probe (wwe12_blood_probe): the game's settings live in a fixed block, sub_82684BE8() =
// 0x831CEEA8 plus 0x57720 (bytes written by UpdateInGameOptions, sub_82DEC528), and in the object
// of sub_826838F0() = [0x8317FA08] + 0x3BB4C (words +8, +12, +20, +28). A host thread reads both
// every 200 ms and logs each byte that changes, to find which one is the BLOOD option.

#include <array>
#include <atomic>
#include <chrono>
#include <string>
#include <thread>

#include <fmt/format.h>
#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"

namespace {

REXCVAR_DEFINE_BOOL(wwe12_blood_probe, false, "WWE12",
                    "Probe: log the game's settings bytes that change (to find BLOOD).");

constexpr u32 kSettingsBlock = 0x831CEEA8u + 0x57720u;
constexpr u32 kBlockSize = 0x100;
constexpr u32 kObjectSize = 0x40;

[[nodiscard]] u32 SettingsObject() {
  const u32 holder = bd::mem::try_field<u32>(0x8317FA08u, 0);
  return holder ? holder + 0x40000u - 17588u : 0;
}

void Probe() {
  std::array<u8, kBlockSize> block{};
  std::array<u8, kObjectSize> object{};
  bool first = true;
  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    if (!REXCVAR_GET(wwe12_blood_probe) || !bd::mem::ready()) continue;
    const u32 obj = SettingsObject();
    std::string changes;
    for (u32 i = 0; i < kBlockSize; ++i) {
      const u8 v = bd::mem::try_field<u8>(kSettingsBlock, i);
      if (!first && v != block[i]) changes += fmt::format(" bloc+{}:{}->{}", i, block[i], v);
      block[i] = v;
    }
    for (u32 i = 0; obj && i < kObjectSize; ++i) {
      const u8 v = bd::mem::try_field<u8>(obj, i);
      if (!first && v != object[i]) changes += fmt::format(" objet+{}:{}->{}", i, object[i], v);
      object[i] = v;
    }
    if (first) {
      std::string dump;
      for (u32 i = 0; i < 0x60; ++i) dump += fmt::format(" {:02X}", block[i]);
      BD_INFO("[sang] sonde prete : bloc {:08X} objet {:08X} ; bloc+0..0x5F :{}", kSettingsBlock,
              obj, dump);
    }
    first = false;
    if (!changes.empty()) BD_INFO("[sang] reglages changes :{}", changes);
  }
}

struct ProbeStarter {
  ProbeStarter() { std::thread(&Probe).detach(); }
} g_probe_starter;

}  // namespace
