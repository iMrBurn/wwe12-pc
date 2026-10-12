// Release notes in the launcher: reads the Markdown notes of a GitHub release (JSON "body") or the
// CHANGELOG.md shipped in the zip, cleans them for display (no emoji: the launcher's fonts lack
// them) and draws headings, bullets and paragraphs with ImGui.
#pragma once

#include <cstdint>
#include <string>

#include "imgui.h"

namespace changelog {

inline void AppendUtf8(std::string &o, uint32_t cp) {
  if (cp < 0x80) {
    o += char(cp);
  } else if (cp < 0x800) {
    o += char(0xC0 | (cp >> 6));
    o += char(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    o += char(0xE0 | (cp >> 12));
    o += char(0x80 | ((cp >> 6) & 0x3F));
    o += char(0x80 | (cp & 0x3F));
  } else {
    o += char(0xF0 | (cp >> 18));
    o += char(0x80 | ((cp >> 12) & 0x3F));
    o += char(0x80 | ((cp >> 6) & 0x3F));
    o += char(0x80 | (cp & 0x3F));
  }
}

// Emoji and symbols the fonts cannot draw (they would show as "?").
inline bool Drawable(uint32_t cp) {
  if (cp == 0xFE0F || cp == 0x200D || cp >= 0x1F000)
    return false;
  return !(cp >= 0x2600 && cp <= 0x27BF);
}

// Copies UTF-8 text, dropping what the fonts cannot draw.
inline std::string FilterUtf8(const std::string &s) {
  std::string o;
  for (size_t i = 0; i < s.size();) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    const int len = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : (c >> 3) == 30 ? 4 : 0;
    if (len == 0 || i + len > s.size()) {
      ++i;  // not valid UTF-8: skip the byte
      continue;
    }
    uint32_t cp = len == 1 ? c : c & (0xFF >> (len + 1));
    for (int k = 1; k < len; ++k)
      cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
    if (Drawable(cp))
      o.append(s, i, size_t(len));
    i += size_t(len);
  }
  return o;
}

// The string value of a JSON key (escapes decoded, \uXXXX pairs joined), or "" when the key is
// missing or null. Searches from `from`, stops at `to`.
inline std::string JsonStringValue(const std::string &json, const char *key, size_t from = 0,
                                   size_t to = std::string::npos) {
  const size_t k = json.find(key, from);
  if (k == std::string::npos || (to != std::string::npos && k >= to))
    return {};
  size_t i = json.find(':', k);
  if (i == std::string::npos)
    return {};
  i = json.find_first_not_of(" \t\r\n", i + 1);
  if (i == std::string::npos || json[i] != '"')
    return {};
  auto hex4 = [&](size_t at, uint32_t &v) {
    v = 0;
    if (at + 4 > json.size())
      return false;
    for (size_t j = at; j < at + 4; ++j) {
      const char h = json[j];
      v = v * 16 + uint32_t(h >= '0' && h <= '9' ? h - '0' : (h | 0x20) >= 'a' && (h | 0x20) <= 'f' ? (h | 0x20) - 'a' + 10 : 0);
    }
    return true;
  };
  std::string o;
  for (++i; i < json.size() && json[i] != '"'; ++i) {
    if (json[i] != '\\') {
      o += json[i];
      continue;
    }
    if (++i >= json.size())
      break;
    switch (json[i]) {
    case 'n': o += '\n'; break;
    case 'r': break;
    case 't': o += ' '; break;
    case 'u': {
      uint32_t cp, lo;
      if (!hex4(i + 1, cp))
        return o;
      i += 4;
      if (cp >= 0xD800 && cp < 0xDC00 && i + 6 < json.size() && json[i + 1] == '\\' &&
          json[i + 2] == 'u' && hex4(i + 3, lo) && lo >= 0xDC00 && lo < 0xE000) {
        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
        i += 6;
      }
      AppendUtf8(o, cp);
      break;
    }
    default: o += json[i]; break;  // \" \\ \/
    }
  }
  return o;
}

inline std::string Trim(const std::string &s) {
  const size_t a = s.find_first_not_of(" \t\r");
  if (a == std::string::npos)
    return {};
  return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
}

// Markdown -> plain lines: "#" headings and "- " bullets stay (Draw styles them); bold, code,
// quote marks, italics-only lines and [text](link) are reduced to their text.
inline std::string Prepare(const std::string &markdown) {
  std::string text = FilterUtf8(markdown);
  if (text.compare(0, 3, "\xEF\xBB\xBF") == 0)
    text.erase(0, 3);  // BOM
  for (const char *mark : {"**", "`"})
    for (size_t p; (p = text.find(mark)) != std::string::npos;)
      text.erase(p, std::char_traits<char>::length(mark));
  for (size_t p = 0; (p = text.find("](", p)) != std::string::npos;) {  // [text](url) -> text
    const size_t open = text.rfind('[', p), close = text.find(')', p);
    if (open == std::string::npos || close == std::string::npos || text.find('\n', open) < p) {
      p += 2;
      continue;
    }
    text.erase(p, close - p + 1);
    text.erase(open, 1);
  }
  std::string out;
  bool skipping = false;
  for (size_t i = 0; i <= text.size();) {
    size_t e = text.find('\n', i);
    if (e == std::string::npos)
      e = text.size();
    std::string line = Trim(text.substr(i, e - i));
    i = e + 1;
    // The "Installation" section tells how to download the zip: useless inside the launcher.
    if (line[0] == '#' && line.find("Installation") != std::string::npos) {
      skipping = true;
      continue;
    }
    if (skipping && line[0] != '#' && line != "---")
      continue;
    skipping = false;
    if (!line.empty() && line[0] == '>')
      line = Trim(line.substr(1));
    if (line.size() >= 2 && line.front() == '*' && line.back() == '*')
      line = Trim(line.substr(1, line.size() - 2));
    if (line.compare(0, 2, "* ") == 0)
      line = "- " + line.substr(2);
    out += line + "\n";
  }
  return Trim(out);
}

// Draws text from Prepare(). Call inside a window or child (it wraps at the right edge).
inline void Draw(const std::string &text) {
  bool blank_before = true;
  for (size_t i = 0; i < text.size();) {
    size_t e = text.find('\n', i);
    if (e == std::string::npos)
      e = text.size();
    const std::string line = text.substr(i, e - i);
    i = e + 1;
    if (line.empty()) {
      if (!blank_before)
        ImGui::Dummy(ImVec2(0, 4));
      blank_before = true;
      continue;
    }
    blank_before = false;
    if (line == "---") {
      ImGui::Separator();
    } else if (line[0] == '#') {
      const size_t t = line.find_first_not_of("# ");
      ImGui::TextColored(ImVec4(0.94f, 0.75f, 0.24f, 1.0f), "%s",
                         t == std::string::npos ? "" : line.c_str() + t);
    } else if (line.compare(0, 2, "- ") == 0) {
      ImGui::Bullet();
      ImGui::SameLine();
      ImGui::TextWrapped("%s", line.c_str() + 2);
    } else {
      ImGui::TextWrapped("%s", line.c_str());
    }
  }
}

}  // namespace changelog
