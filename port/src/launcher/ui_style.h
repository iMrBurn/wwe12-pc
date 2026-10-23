// The launcher's look (also the Mod Maker's): colours, fonts, the game menus' slanted brush strokes.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <filesystem>

#include "imgui.h"

namespace ui {

inline constexpr ImU32 kGold = IM_COL32(240, 190, 60, 255);
inline constexpr ImU32 kRed = IM_COL32(200, 24, 34, 255);
inline ImFont *g_font_body = nullptr;
inline ImFont *g_font_title = nullptr;

inline void SetupStyle() {
  ImGuiStyle &st = ImGui::GetStyle();
  st.WindowRounding = 0;
  st.FrameRounding = 4;
  st.GrabRounding = 4;
  st.FramePadding = ImVec2(10, 7);
  st.ItemSpacing = ImVec2(10, 10);
  st.WindowBorderSize = 0;
  ImVec4 *c = st.Colors;
  c[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.055f, 0.063f, 1.0f);
  c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
  c[ImGuiCol_Text] = ImVec4(0.95f, 0.94f, 0.92f, 1.0f);
  c[ImGuiCol_TextDisabled] = ImVec4(0.6f, 0.58f, 0.55f, 1.0f);
  c[ImGuiCol_FrameBg] = ImVec4(0.13f, 0.13f, 0.15f, 1.0f);
  c[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.18f, 0.12f, 1.0f);
  c[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.22f, 0.10f, 1.0f);
  c[ImGuiCol_Button] = ImVec4(0.16f, 0.16f, 0.18f, 1.0f);
  c[ImGuiCol_ButtonHovered] = ImVec4(0.78f, 0.09f, 0.13f, 1.0f);
  c[ImGuiCol_ButtonActive] = ImVec4(0.94f, 0.75f, 0.24f, 1.0f);
  c[ImGuiCol_CheckMark] = ImVec4(0.94f, 0.75f, 0.24f, 1.0f);
  c[ImGuiCol_Header] = ImVec4(0.78f, 0.09f, 0.13f, 0.6f);
  c[ImGuiCol_HeaderHovered] = ImVec4(0.78f, 0.09f, 0.13f, 0.8f);
  c[ImGuiCol_HeaderActive] = ImVec4(0.94f, 0.75f, 0.24f, 0.8f);
  c[ImGuiCol_PopupBg] = ImVec4(0.09f, 0.09f, 0.10f, 0.98f);
  c[ImGuiCol_Separator] = ImVec4(0.94f, 0.75f, 0.24f, 0.35f);
  c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0);
}

inline void LoadFonts() {
  ImGuiIO &io = ImGui::GetIO();
  wchar_t windir[MAX_PATH];
  GetWindowsDirectoryW(windir, MAX_PATH);
  const std::filesystem::path fonts = std::filesystem::path(windir) / "Fonts";
  std::error_code ec;
  auto try_font = [&](const char *file, float size) -> ImFont * {
    const std::filesystem::path p = fonts / file;
    if (!std::filesystem::exists(p, ec))
      return nullptr;
    return io.Fonts->AddFontFromFileTTF(p.string().c_str(), size);
  };
  g_font_body = try_font("segoeui.ttf", 19.0f);
  if (!g_font_body)
    g_font_body = io.Fonts->AddFontDefault();
  g_font_title = try_font("impact.ttf", 40.0f);
  if (!g_font_title)
    g_font_title = try_font("segoeuib.ttf", 34.0f);
  if (!g_font_title)
    g_font_title = g_font_body;
}

// A slanted brush stroke, the game menus' look.
inline void Stroke(ImDrawList *dl, ImVec2 a, float w, float h, ImU32 col, float slant = 18.0f) {
  dl->AddQuadFilled(ImVec2(a.x + slant, a.y), ImVec2(a.x + w + slant, a.y),
                    ImVec2(a.x + w, a.y + h), ImVec2(a.x, a.y + h), col);
}

inline bool BigButton(const char *label, ImVec2 size, ImU32 fill, ImU32 text_col) {
  ImGui::PushID(label);
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const bool clicked = ImGui::InvisibleButton("##b", size);
  const bool hovered = ImGui::IsItemHovered();
  ImDrawList *dl = ImGui::GetWindowDrawList();
  Stroke(dl, p, size.x - 18, size.y, hovered ? kRed : fill);
  ImGui::PushFont(g_font_title, size.y >= 80 ? 46.0f : 30.0f);
  const ImVec2 ts = ImGui::CalcTextSize(label);
  dl->AddText(ImVec2(p.x + (size.x - ts.x) * 0.5f, p.y + (size.y - ts.y) * 0.5f),
              hovered ? IM_COL32_WHITE : text_col, label);
  ImGui::PopFont();
  ImGui::PopID();
  return clicked;
}


inline void Title(const char *text) {
  ImGui::PushFont(g_font_title, 44.0f);
  const ImVec2 p = ImGui::GetCursorScreenPos();
  Stroke(ImGui::GetWindowDrawList(), ImVec2(p.x - 10, p.y + ImGui::GetTextLineHeight() - 4), 300,
         10, kRed, 6);
  ImGui::TextColored(ImVec4(0.94f, 0.75f, 0.24f, 1.0f), "%s", text);
  ImGui::PopFont();
  ImGui::Dummy(ImVec2(0, 6));
}

}  // namespace ui
