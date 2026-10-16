// Minimal line-based editor for wwe12.toml: reads "key = value" lines and changes values in place,
// so the file's comments and layout survive the launcher's edits. Keys it does not find are
// appended at the end. Only the flat key = value form the game's settings use is handled.
// Saving never leaves a half-written file: the text goes to wwe12.toml.new, which then replaces
// the file; the previous file is copied to wwe12.toml.bak once per run.
#pragma once

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

class TomlFile {
 public:
  bool Load(const std::filesystem::path &path) {
    path_ = path;
    lines_.clear();
    std::ifstream f(path, std::ios::binary);
    if (!f)
      return false;
    std::string line;
    while (std::getline(f, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      if (lines_.empty() && line.compare(0, 3, "\xEF\xBB\xBF") == 0)
        line.erase(0, 3);  // UTF-8 BOM some editors add: it would hide the first key
      lines_.push_back(line);
    }
    return true;
  }

  // false when the file cannot be written (folder read-only, disk full...); the old file is then
  // left as it was. error() says why.
  bool Save() {
    namespace fs = std::filesystem;
    error_.clear();
    std::error_code ec;
    if (!backed_up_ && fs::exists(path_, ec)) {
      fs::path bak = path_;
      bak += ".bak";
      fs::copy_file(path_, bak, fs::copy_options::overwrite_existing, ec);  // best effort
      backed_up_ = true;
    }
    fs::path tmp = path_;
    tmp += ".new";
    {
      std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
      for (const auto &l : lines_)
        f << l << "\r\n";
      f.flush();
      if (!f) {
        error_ = "cannot write " + tmp.filename().string();
        f.close();
        fs::remove(tmp, ec);
        return false;
      }
    }
    fs::rename(tmp, path_, ec);  // replaces the old file in one step
    if (ec) {
      error_ = ec.message();
      std::error_code ignore;
      fs::remove(tmp, ignore);
      return false;
    }
    return true;
  }
  const std::string &error() const { return error_; }

  // Raw value text (quotes kept for strings), or nothing.
  std::optional<std::string> Raw(const std::string &key) const {
    for (const auto &l : lines_) {
      std::string k, v;
      if (Split(l, k, v) && k == key)
        return v;
    }
    return std::nullopt;
  }

  bool GetBool(const std::string &key, bool def) const {
    auto v = Raw(key);
    return v ? *v == "true" : def;
  }
  int GetInt(const std::string &key, int def) const {
    auto v = Raw(key);
    if (!v)
      return def;
    try {
      return std::stoi(*v);
    } catch (...) {
      return def;
    }
  }
  std::string GetString(const std::string &key, const std::string &def) const {
    auto v = Raw(key);
    if (!v || v->size() < 2 || v->front() != '"' || v->back() != '"')
      return def;
    return Unescape(v->substr(1, v->size() - 2));
  }

  void SetRaw(const std::string &key, const std::string &value) {
    for (auto &l : lines_) {
      std::string k, v;
      if (Split(l, k, v) && k == key) {
        l = key + " = " + value;
        return;
      }
    }
    lines_.push_back(key + " = " + value);
  }
  void SetBool(const std::string &key, bool v) { SetRaw(key, v ? "true" : "false"); }
  void SetInt(const std::string &key, int v) { SetRaw(key, std::to_string(v)); }
  void SetString(const std::string &key, const std::string &v) {
    std::string e;
    for (char c : v) {
      if (c == '"' || c == '\\')
        e += '\\';
      e += c;
    }
    SetRaw(key, "\"" + e + "\"");
  }

 private:
  static bool Split(const std::string &line, std::string &key, std::string &value) {
    const size_t first = line.find_first_not_of(" \t");
    if (first == std::string::npos || line[first] == '#' || line[first] == '[')
      return false;
    const size_t eq = line.find('=', first);
    if (eq == std::string::npos)
      return false;
    key = Trim(line.substr(first, eq - first));
    value = Trim(line.substr(eq + 1));
    // Drop a trailing comment outside quotes.
    bool in_str = false;
    for (size_t i = 0; i < value.size(); ++i) {
      if (value[i] == '"' && (i == 0 || value[i - 1] != '\\'))
        in_str = !in_str;
      else if (value[i] == '#' && !in_str) {
        value = Trim(value.substr(0, i));
        break;
      }
    }
    return !key.empty();
  }
  static std::string Trim(const std::string &s) {
    const size_t a = s.find_first_not_of(" \t");
    if (a == std::string::npos)
      return {};
    const size_t b = s.find_last_not_of(" \t");
    return s.substr(a, b - a + 1);
  }
  static std::string Unescape(const std::string &s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
      if (s[i] == '\\' && i + 1 < s.size())
        ++i;
      o += s[i];
    }
    return o;
  }

  std::filesystem::path path_;
  std::vector<std::string> lines_;
  std::string error_;
  bool backed_up_ = false;
};
