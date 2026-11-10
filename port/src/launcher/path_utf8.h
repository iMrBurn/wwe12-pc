// A file name or path as UTF-8 text, without ever throwing.
// std::filesystem::path::string() converts through the ANSI code page and THROWS on a name it can't hold
// (Japanese, Korean, emoji...), which closed the launcher when such a file was in the music / mods / saves folders.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <filesystem>
#include <string>

namespace pathutil {

inline std::string Utf8(const std::filesystem::path &p) {
  try {
    const std::wstring &w = p.native();
    if (w.empty())
      return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), int(w.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0)
      return {};
    std::string s(size_t(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), int(w.size()), s.data(), n, nullptr, nullptr);
    return s;
  } catch (...) {
    return {};
  }
}

}  // namespace pathutil
