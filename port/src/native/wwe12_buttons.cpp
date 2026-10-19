// WWE '12: PlayStation button icons when a PlayStation controller is used.
//
// The game shows its buttons from textures: one icon sheet per font (menu\fontHD.pac /
// fontSD.pac, 512x512 or 256x512 DXT5; the texts reach the icons through private-use characters
// U+E000-U+E02A) and small in-match prompts (game2dHD.pac / game2dSD.pac: "REACH! HOLD! PULL!",
// LB + Y...). The PS3 version of the game has the same textures with PlayStation buttons. The user
// decided to use those real icons (2026-10-17): tools/extract_ps3_buttons.py builds
// port/res/ps3_buttons.bin from both versions' files, embedded in wwe12.exe as resource 101.
// Each record names the Xbox texture it replaces by the FNV-1a of its level 0, and holds either the
// whole PS3 level 0 (prompts) or only the button cells' DXT5 blocks (icon sheets: the PS3 sheet puts
// THQ and the F/R logos elsewhere, so the rest of the Xbox sheet and the fonts' glyph tables stay).
// The fonts' glyph rectangles stay the Xbox ones (the game converts its glyph tables at load; they
// were not found in memory), so the four PS3 icons wider than their Xbox cells (SELECT, START, L2,
// R2) are narrowed by the tool to fit those cells.
//
// sub_82AC59C8(info, dds, size) is the game's DDS loader (it checks the "DDS " magic): the hook
// recognises an Xbox texture of the list and copies the PS3 data over it in guest memory before the
// game converts it. Fonts load at startup, prompts at each match: with wwe12_button_icons = auto,
// plug the PlayStation controller in before starting the game (the pad list comes from the SDK's
// input system, Wwe12ButtonsSetInput).

#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <rex/input/input_system.h>


#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_STRING(wwe12_button_icons, "auto", "WWE12",
                      "Button icons: auto (PlayStation when a PlayStation controller is plugged in), "
                      "xbox or playstation.");

namespace {

constexpr int kResourceId = 101;  // port/res/wwe12.rc.in

struct Record {
  uint32_t width = 0, height = 0;
  bool whole = false;              // true: payload = PS3 level 0
  const uint8_t *payload = nullptr;
  uint32_t payload_size = 0;
};

struct Table {
  std::unordered_map<uint64_t, Record> by_hash;
  std::unordered_set<uint64_t> sizes;  // (width << 32) | height of the listed textures
};

const Table &PlayStationTable() {
  static const Table table = [] {
    Table t;
    HRSRC res = FindResourceW(nullptr, MAKEINTRESOURCEW(kResourceId), MAKEINTRESOURCEW(10));  // 10 = RT_RCDATA
    HGLOBAL mem = res ? LoadResource(nullptr, res) : nullptr;
    const auto *data = mem ? static_cast<const uint8_t *>(LockResource(mem)) : nullptr;
    const uint32_t size = res ? SizeofResource(nullptr, res) : 0;
    auto u32 = [&](uint32_t at) {
      uint32_t v;
      std::memcpy(&v, data + at, 4);
      return v;
    };
    if (!data || size < 12 || std::memcmp(data, "W12B", 4) != 0 || u32(4) != 1) {
      BD_WARN("Boutons PlayStation : ressource absente ou invalide");
      return t;
    }
    const uint32_t count = u32(8);
    uint32_t at = 12;
    for (uint32_t i = 0; i < count && at + 28 <= size; ++i) {
      Record r;
      r.width = u32(at);
      r.height = u32(at + 4);
      uint64_t hash;
      std::memcpy(&hash, data + at + 8, 8);
      r.whole = u32(at + 16) == 0;
      r.payload_size = u32(at + 20);
      r.payload = data + at + 24;
      at += 24 + r.payload_size;
      if (at > size)
        break;
      t.by_hash.emplace(hash, r);
      t.sizes.insert((uint64_t(r.width) << 32) | r.height);
    }
    BD_INFO("Boutons PlayStation : {} textures prêtes", t.by_hash.size());
    return t;
  }();
  return table;
}

// Pads come from the SDK's input system: wwe12.exe has its own static copy of SDL, which never
// opened the gamepad subsystem and so sees no pad (the cause of auto mode showing Xbox icons with
// a DualSense plugged in, 0.8). A Sony pad has USB vendor 054C, which the SDL GUID string holds
// at characters 8-11 ("4c05", little-endian); the name is checked too.
rex::input::InputSystem *g_input = nullptr;

bool PlayStationControllerPluggedIn() {
  if (!g_input)
    return false;
  for (const rex::input::DeviceInfo &d : g_input->GetDevices()) {
    if (d.synthetic)
      continue;
    const bool sony_vendor = d.guid.size() >= 12 && d.guid.compare(8, 4, "4c05") == 0;
    const bool sony_name = d.name.find("DualSense") != std::string::npos ||
                           d.name.find("DualShock") != std::string::npos ||
                           d.name.find("PS3") != std::string::npos ||
                           d.name.find("PS4") != std::string::npos ||
                           d.name.find("PS5") != std::string::npos;
    if (sony_vendor || sony_name)
      return true;
  }
  return false;
}

bool WantPlayStationIcons() {
  const std::string mode = REXCVAR_GET(wwe12_button_icons);
  if (mode == "playstation")
    return true;
  if (mode == "xbox")
    return false;
  return PlayStationControllerPluggedIn();
}

uint64_t Fnv1a64(const uint8_t *p, size_t n) {
  uint64_t h = 0xCBF29CE484222325ull;
  for (size_t i = 0; i < n; ++i)
    h = (h ^ p[i]) * 0x100000001B3ull;
  return h;
}

// dds = guest address of "DDS " + 124-byte header + level 0 (as in the pac: little-endian DDS).
void MaybeSwapToPlayStation(uint32_t dds, uint32_t size) {
  if (size < 128)
    return;
  auto *bytes = bd::mem::at<uint8_t>(dds);
  if (!bytes || std::memcmp(bytes, "DDS ", 4) != 0 || std::memcmp(bytes + 84, "DXT5", 4) != 0)
    return;
  uint32_t height, width;
  std::memcpy(&height, bytes + 12, 4);
  std::memcpy(&width, bytes + 16, 4);
  const Table &table = PlayStationTable();
  if (!table.sizes.count((uint64_t(width) << 32) | height))
    return;
  const uint64_t level0_size = uint64_t(width) * height;  // DXT5: one byte per pixel
  if (128 + level0_size > size)
    return;
  uint8_t *level0 = bytes + 128;
  const auto it = table.by_hash.find(Fnv1a64(level0, level0_size));
  if (it == table.by_hash.end() || !WantPlayStationIcons())
    return;
  const Record &r = it->second;
  if (r.whole) {
    if (r.payload_size == level0_size)
      std::memcpy(level0, r.payload, level0_size);
  } else {
    for (uint32_t at = 0; at + 20 <= r.payload_size; at += 20) {
      uint32_t block;
      std::memcpy(&block, r.payload + at, 4);
      if (uint64_t(block) * 16 + 16 <= level0_size)
        std::memcpy(level0 + uint64_t(block) * 16, r.payload + at + 4, 16);
    }
  }
  BD_INFO("Boutons PlayStation : texture {}x{} remplacée", width, height);
}

}  // namespace

void Wwe12ButtonsSetInput(rex::input::InputSystem *input) { g_input = input; }

REX_EXTERN(__imp__sub_82AC59C8);
extern "C" REX_FUNC(sub_82AC59C8) {
  MaybeSwapToPlayStation(ctx.r4.u32, ctx.r5.u32);
  __imp__sub_82AC59C8(ctx, base);
}
