// WWE '12 PC: names for the preset entrances and move-sets (wwe12_preset_names.cpp).
#pragma once

#include <cstdint>

// The guest text to show for string `id` asked by code at `caller` (the string manager hook's return address), or 0
// to keep the game's (string manager hook, game thread).
uint32_t Wwe12PresetNameText(uint32_t id, uint32_t caller);
