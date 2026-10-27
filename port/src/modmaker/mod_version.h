// The version a mod's manifest says (version=, the launcher's Version column): the Version field next to Author on
// each page that makes a mod, "1.0" when left empty.
#pragma once

#include <string>

#include "imgui.h"

namespace mod_version {

// The field, on the same line as what comes before it.
inline void Input(const char *id, char *buf, size_t size) {
  ImGui::SameLine();
  ImGui::SetNextItemWidth(70);
  ImGui::InputTextWithHint(id, "1.0", buf, size);
}

// For the manifest: one line, "1.0" when empty.
inline std::string Of(const char *v) {
  std::string s;
  for (const char *p = v; *p; ++p)
    if (*p != '\n' && *p != '\r')
      s += *p;
  while (!s.empty() && s.back() == ' ')
    s.pop_back();
  while (!s.empty() && s.front() == ' ')
    s.erase(s.begin());
  return s.empty() ? "1.0" : s;
}

}  // namespace mod_version
