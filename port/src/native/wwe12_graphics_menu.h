// WWE '12 PC: graphics settings screen opened from the game's OPTIONS menu (wwe12_graphics_menu.cpp).
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>

namespace rex::input {
class InputSystem;
}

struct ImFontAtlas;

namespace rex::ui {
class ImGuiDrawer;
}

// Called once by the app, on the UI thread: where to draw, how to run code on the UI thread, and
// the wwe12.toml the screen saves to.
void Wwe12GraphicsMenuInit(rex::ui::ImGuiDrawer *drawer,
                           std::function<void(std::function<void()>)> run_in_ui_thread,
                           std::filesystem::path toml_path);

// Adds the screen's font (Impact) to ImGui's atlas; called from the app's font setup.
void Wwe12GraphicsMenuConfigureFonts(ImFontAtlas *atlas);

// The input system the screen reads the pads from (it works even with the game's input off).
void Wwe12GraphicsMenuSetInput(rex::input::InputSystem *input);

// True while the screen is open: the game gets no controller or keyboard input meanwhile.
bool Wwe12GraphicsMenuIsOpen();

// The OPTIONS entry was chosen (game thread): opens the screen.
void Wwe12GraphicsMenuRequest();

// String id of the OPTIONS entry that opens the screen ("IN-GAME MANUAL", relabelled).
constexpr uint32_t kWwe12GraphicsStringId = 41184;

// Rewrites the entry's text in a writable guest memory region holding a loaded string table.
// Returns the number of texts rewritten.
uint32_t Wwe12RelabelGraphicsEntry(uint8_t *region_start, uint8_t *region_end);

// ---- The native pages (wwe12_graphics_native): GRAPHICS in the game's own menus. -------------

// Text ids of the GRAPHICS sub-menu's three entries (DISPLAY / GRAPHICS / ADVANCED).
constexpr uint32_t kWwe12GraphicsNativeTabText = 43100;

bool Wwe12GraphicsNativeEnabled();

// A menu entry was chosen (decide hook, game thread): one of ours opens its page next.
void Wwe12GraphicsNativeDecide(uint32_t label);

// The guest text of one of the native pages' ids, or 0 (string manager hook, game thread).
uint32_t Wwe12NativeGraphicsGuestText(uint32_t id);
