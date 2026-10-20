// WWE '12 PC: OPTIONS > GRAPHICS in the game's own menus (wwe12_graphics_menu.cpp).
#pragma once

#include <cstdint>
#include <filesystem>

// Called once by the app: the wwe12.toml the pages save to.
void Wwe12GraphicsMenuInit(std::filesystem::path toml_path);

// String id of the OPTIONS entry that leads to the pages ("IN-GAME MANUAL", relabelled).
constexpr uint32_t kWwe12GraphicsStringId = 41184;

// Rewrites the entry's text in a writable guest memory region holding a loaded string table.
// Returns the number of texts rewritten.
uint32_t Wwe12RelabelGraphicsEntry(uint8_t *region_start, uint8_t *region_end);

// Text ids of the entry's sub-menu (DISPLAY / GRAPHICS / ADVANCED).
constexpr uint32_t kWwe12GraphicsNativeTabText = 43100;

// True when the pages replace the manual (wwe12_graphics_menu).
bool Wwe12GraphicsNativeEnabled();

// A menu entry was chosen (decide hook, game thread): one of ours opens its page next.
void Wwe12GraphicsNativeDecide(uint32_t label);

// The guest text of one of the pages' text ids, or 0 (string manager hook, game thread).
uint32_t Wwe12NativeGraphicsGuestText(uint32_t id);
