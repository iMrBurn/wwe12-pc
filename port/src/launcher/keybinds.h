// Keyboard bindings: the exact key names the game understands, so what the launcher writes in
// wwe12.toml always works.
//
// The game (rexglue-sdk, src/ui/keybinds.cpp and src/input/mnk/mnk_input_driver.cpp) reads each
// keybind_* setting as comma-separated alternatives, each "Key" or "Shift+Key" (also Ctrl+, Alt+).
// Names are case-sensitive and come from a fixed table: "j", "Enter" or "space" are silently
// ignored. A modifier alone ("Shift", "Ctrl", "Alt") is a key of its own (issue #18,
// rexglue-sdk-mnk-modifiers.patch): then it no longer works as a prefix anywhere ("Shift+Up" stays
// silent), which ModifierConflicts reports. This header keeps the same table (kept in sync by
// hand), corrects what was typed (j -> J, Enter -> Return) and flags the rest.
#pragma once

#include <algorithm>
#include <cctype>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include "imgui.h"

namespace keys {

struct Def {
  const char *name;  // the game's name
  ImGuiKey key;      // the same key as ImGui sees it (to capture a key press), or None
};

inline const std::vector<Def> &Table() {
  static const std::vector<Def> table = [] {
    std::vector<Def> t;
    static std::deque<std::string> storage;  // names built in loops: stable addresses
    auto add_built = [&](const std::string &name, ImGuiKey key) {
      storage.push_back(name);
      t.push_back({storage.back().c_str(), key});
    };
    for (int i = 0; i < 24; ++i)
      add_built("F" + std::to_string(i + 1), ImGuiKey(int(ImGuiKey_F1) + i));
    for (int i = 0; i < 26; ++i)
      add_built(std::string(1, char('A' + i)), ImGuiKey(int(ImGuiKey_A) + i));
    for (int i = 0; i < 10; ++i)
      add_built(std::string(1, char('0' + i)), ImGuiKey(int(ImGuiKey_0) + i));
    for (int i = 0; i < 10; ++i)
      add_built("Numpad" + std::to_string(i), ImGuiKey(int(ImGuiKey_Keypad0) + i));
    const Def fixed[] = {
        {"Backtick", ImGuiKey_GraveAccent}, {"Minus", ImGuiKey_Minus},       {"Plus", ImGuiKey_Equal},
        {"Comma", ImGuiKey_Comma},          {"Period", ImGuiKey_Period},     {"Semicolon", ImGuiKey_Semicolon},
        {"Slash", ImGuiKey_Slash},          {"Backslash", ImGuiKey_Backslash},
        {"LBracket", ImGuiKey_LeftBracket}, {"RBracket", ImGuiKey_RightBracket}, {"Quote", ImGuiKey_Apostrophe},
        {"Escape", ImGuiKey_Escape},        {"Return", ImGuiKey_Enter},      {"Space", ImGuiKey_Space},
        {"Tab", ImGuiKey_Tab},              {"Backspace", ImGuiKey_Backspace}, {"Delete", ImGuiKey_Delete},
        {"Insert", ImGuiKey_Insert},        {"Home", ImGuiKey_Home},         {"End", ImGuiKey_End},
        {"PageUp", ImGuiKey_PageUp},        {"PageDown", ImGuiKey_PageDown},
        {"Left", ImGuiKey_LeftArrow},       {"Right", ImGuiKey_RightArrow},  {"Up", ImGuiKey_UpArrow},
        {"Down", ImGuiKey_DownArrow},
        {"NumpadEnter", ImGuiKey_KeypadEnter}, {"NumpadPlus", ImGuiKey_KeypadAdd},
        {"NumpadMinus", ImGuiKey_KeypadSubtract}, {"NumpadStar", ImGuiKey_KeypadMultiply},
        {"NumpadSlash", ImGuiKey_KeypadDivide},
        {"PrintScreen", ImGuiKey_PrintScreen}, {"Pause", ImGuiKey_Pause}, {"CapsLock", ImGuiKey_CapsLock},
        {"NumLock", ImGuiKey_NumLock},      {"ScrollLock", ImGuiKey_ScrollLock},
        {"LMB", ImGuiKey_None},             {"RMB", ImGuiKey_None},          {"MMB", ImGuiKey_None},
    };
    for (const Def &d : fixed)
      t.push_back(d);
    return t;
  }();
  return table;
}

inline std::string Lower(std::string s) {
  for (char &c : s)
    c = char(tolower(static_cast<unsigned char>(c)));
  return s;
}

inline std::string Trim(const std::string &s) {
  const size_t a = s.find_first_not_of(" \t");
  if (a == std::string::npos)
    return {};
  return s.substr(a, s.find_last_not_of(" \t") - a + 1);
}

// The game's name for what the player typed (any case, usual aliases and the key's own symbol),
// or nullptr when the game would not know it.
inline const char *Canonical(const std::string &typed) {
  static const std::unordered_map<std::string, const char *> map = [] {
    std::unordered_map<std::string, const char *> m;
    for (const Def &d : Table())
      m[Lower(d.name)] = d.name;
    const std::pair<const char *, const char *> aliases[] = {
        {"enter", "Return"},    {"esc", "Escape"},      {"spacebar", "Space"},   {"del", "Delete"},
        {"ins", "Insert"},      {"pgup", "PageUp"},     {"pgdn", "PageDown"},    {"bksp", "Backspace"},
        {"arrowleft", "Left"},  {"arrowright", "Right"}, {"arrowup", "Up"},     {"arrowdown", "Down"},
        {";", "Semicolon"},     {".", "Period"},        {"/", "Slash"},          {"\\", "Backslash"},
        {"[", "LBracket"},      {"]", "RBracket"},      {"'", "Quote"},          {"`", "Backtick"},
        {"-", "Minus"},         {"=", "Plus"},          {"num0", "Numpad0"},     {"num1", "Numpad1"},
        {"num2", "Numpad2"},    {"num3", "Numpad3"},    {"num4", "Numpad4"},     {"num5", "Numpad5"},
        {"num6", "Numpad6"},    {"num7", "Numpad7"},    {"num8", "Numpad8"},     {"num9", "Numpad9"},
    };
    for (const auto &a : aliases)
      m[a.first] = a.second;
    return m;
  }();
  const auto it = map.find(Lower(typed));
  return it == map.end() ? nullptr : it->second;
}

struct Result {
  std::string text;              // corrected bindings, "Z,W" / "Shift+Up"
  std::vector<std::string> bad;  // pieces the game would ignore
};

inline Result Normalize(const std::string &typed) {
  Result r;
  size_t from = 0;
  while (from <= typed.size()) {
    size_t comma = typed.find(',', from);
    if (comma == std::string::npos)
      comma = typed.size();
    const std::string piece = Trim(typed.substr(from, comma - from));
    from = comma + 1;
    if (piece.empty())
      continue;
    // "A+B+Key": every part but the last is a modifier. A lone "+" is the plus key itself.
    std::vector<std::string> parts;
    if (piece == "+") {
      parts.push_back("Plus");
    } else {
      size_t p = 0;
      for (;;) {
        const size_t plus = piece.find('+', p);
        if (plus == std::string::npos || plus + 1 == piece.size()) {  // trailing '+': the plus key
          const std::string head = Trim(piece.substr(p, plus == std::string::npos ? plus : plus - p));
          if (!head.empty())
            parts.push_back(head);
          if (plus != std::string::npos)
            parts.push_back("Plus");
          break;
        }
        parts.push_back(Trim(piece.substr(p, plus - p)));
        p = plus + 1;
      }
    }
    bool shift = false, ctrl = false, alt = false, ok = !parts.empty();
    for (size_t i = 0; ok && i + 1 < parts.size(); ++i) {
      const std::string m = Lower(parts[i]);
      if (m == "shift")
        shift = true;
      else if (m == "ctrl" || m == "control")
        ctrl = true;
      else if (m == "alt")
        alt = true;
      else
        ok = false;
    }
    const std::string lower_key = ok ? Lower(parts.back()) : std::string();
    // A modifier on its own is a key of its own; it cannot also prefix itself.
    const char *modifier = lower_key == "shift"                           ? "Shift"
                           : lower_key == "ctrl" || lower_key == "control" ? "Ctrl"
                           : lower_key == "alt"                             ? "Alt"
                                                                            : nullptr;
    const char *key = !ok ? nullptr : modifier ? (parts.size() == 1 ? modifier : nullptr) : Canonical(parts.back());
    if (!key) {
      r.bad.push_back(piece);
      continue;
    }
    std::string out;
    if (shift)
      out += "Shift+";
    if (ctrl)
      out += "Ctrl+";
    if (alt)
      out += "Alt+";
    out += key;
    if (!r.text.empty())
      r.text += ",";
    r.text += out;
  }
  return r;
}

// Call every frame while waiting for the player to press a key. Returns true once, with the
// game's name for the key (with "Shift+", "Ctrl+", "Alt+" if held). A modifier pressed and
// released on its own gives "Shift", "Ctrl" or "Alt".
inline bool Capture(std::string &out) {
  const ImGuiIO &io = ImGui::GetIO();
  static const char *alone = nullptr;  // a modifier held with no other key yet
  const struct {
    ImGuiKey left, right;
    const char *name;
  } mods[] = {{ImGuiKey_LeftShift, ImGuiKey_RightShift, "Shift"},
              {ImGuiKey_LeftCtrl, ImGuiKey_RightCtrl, "Ctrl"},
              {ImGuiKey_LeftAlt, ImGuiKey_RightAlt, "Alt"}};
  for (const auto &m : mods) {
    if (ImGui::IsKeyPressed(m.left, false) || ImGui::IsKeyPressed(m.right, false))
      alone = m.name;
    if (alone == m.name && (ImGui::IsKeyReleased(m.left) || ImGui::IsKeyReleased(m.right))) {
      alone = nullptr;
      out = m.name;
      return true;
    }
  }
  for (const Def &d : Table()) {
    if (d.key == ImGuiKey_None || !ImGui::IsKeyPressed(d.key, false))
      continue;
    alone = nullptr;
    out.clear();
    if (io.KeyShift)
      out += "Shift+";
    if (io.KeyCtrl)
      out += "Ctrl+";
    if (io.KeyAlt)
      out += "Alt+";
    out += d.name;
    return true;
  }
  return false;
}

// Modifiers bound on their own whose combinations ("Shift+Up") will then do nothing, as
// "Shift (Shift+Up)": the game treats a modifier bound alone as a key of its own.
inline std::string ModifierConflicts(const std::vector<std::string> &binds) {
  std::vector<std::string> alone, combos;
  for (const std::string &b : binds) {
    size_t from = 0;
    while (from <= b.size()) {
      size_t comma = b.find(',', from);
      if (comma == std::string::npos)
        comma = b.size();
      const std::string piece = Trim(b.substr(from, comma - from));
      from = comma + 1;
      if (piece == "Shift" || piece == "Ctrl" || piece == "Alt")
        alone.push_back(piece);
      else if (piece.find('+') != std::string::npos && piece != "+")
        combos.push_back(piece);
    }
  }
  std::string out;
  for (const std::string &m : alone)
    for (const std::string &c : combos)
      if (c.rfind(m + "+", 0) == 0 || c.find("+" + m + "+") != std::string::npos)
        out += (out.empty() ? "" : ", ") + m + " (" + c + ")";
  return out;
}

}  // namespace keys
