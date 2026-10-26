// WWE '12 PC: OPTIONS > JUKEBOX, the menus play entrance themes (wwe12_jukebox.cpp).
#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>

// Text id of the OPTIONS entry (CREDITS relabelled, wwe12_menu_patch.cpp) and of the page's title.
constexpr uint32_t kWwe12JukeboxText = 43103;

// Option page number next to the GRAPHICS pages' 0-2 (wwe12_graphics_menu.cpp runs the page).
constexpr int kWwe12JukeboxPage = 3;

// Called once by the app: the wwe12.toml the page saves to.
void Wwe12JukeboxInit(std::filesystem::path toml_path);

// True when OPTIONS has the JUKEBOX entry (wwe12_jukebox_menu).
bool Wwe12JukeboxMenuEnabled();

// The page script (compiled Lua 5.1, tools/graphics_pages.py) and its number of rows.
const uint8_t *Wwe12JukeboxPageScript(size_t *size);
int Wwe12JukeboxPageRows();

// yOption.InitInGameOptions / UpdateInGameOptions for the page: the rows' values in the page's
// 32-bit array (value i, enabled N+i, maximum 2N+i), and the chosen ones applied and saved.
void Wwe12JukeboxPageInit(uint32_t array);
void Wwe12JukeboxPageUpdate(uint32_t array);

// The text of one of the page's text ids in the player's language, or empty.
std::string Wwe12JukeboxText(uint32_t id);

// The page's subtitle, in place of the reused page's "IN-GAME OPTIONS" (text 41142) while it is open.
constexpr uint32_t kWwe12JukeboxSubtitleText = 41142;
std::string Wwe12JukeboxSubtitle();
