// Launcher updates from the project's GitHub releases: checks the newest release (pre-releases
// included), downloads its zip and unpacks it over the install, keeping the player's settings
// (wwe12.toml), the game folder and the mods folder. Files in use (the launcher itself) are
// renamed to *.old first, and deleted at the next start.
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "imgui.h"
#include "inflate.h"

class Updater {
 public:
  enum class State { Idle, Checking, UpToDate, Available, Downloading, Failed, Done };

  void Start(const std::filesystem::path &dir) {
    dir_ = dir;
    CleanOld();
    check_at_start_ = ReadPref();
    if (check_at_start_)
      Check();
  }

  bool RestartRequested() const { return restart_; }

  void Draw(const char *checking, const char *up_to_date, const char *available,
            const char *install, const char *failed, const char *check_now,
            const char *check_at_start, const char *downloading, const char *update_failed) {
    if (auto_install_.exchange(false))
      Install();
    std::string latest, error;
    {
      std::lock_guard lock(mutex_);
      latest = latest_;
      error = error_;
    }
    switch (state_.load()) {
    case State::Idle:
      break;
    case State::Checking:
      ImGui::TextDisabled("%s", checking);
      break;
    case State::UpToDate:
      ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.45f, 1.0f), "%s", up_to_date);
      break;
    case State::Available:
      ImGui::TextColored(ImVec4(0.94f, 0.75f, 0.24f, 1.0f), "%s %s", available, latest.c_str());
      if (ImGui::Button(install))
        Install();
      break;
    case State::Downloading:
      ImGui::Text("%s", downloading);
      ImGui::ProgressBar(progress_.load(), ImVec2(320, 0));
      break;
    case State::Failed:
      ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", error.empty() ? failed : update_failed);
      if (!error.empty())
        ImGui::TextDisabled("%s", error.c_str());
      break;
    case State::Done:
      break;
    }
    if (state_ != State::Checking && state_ != State::Downloading) {
      if (ImGui::Button(check_now))
        Check();
      ImGui::SameLine();
    }
    if (ImGui::Checkbox(check_at_start, &check_at_start_))
      WritePref(check_at_start_);
  }

 private:
  static constexpr const wchar_t *kHost = L"api.github.com";
  static constexpr const wchar_t *kPath = L"/repos/iMrBurn/wwe12-pc/releases?per_page=10";

  void Check() {
    if (worker_.joinable())
      worker_.join();
    state_ = State::Checking;
    worker_ = std::thread([this] {
      std::vector<uint8_t> body;
      if (!HttpGet(L"https://api.github.com/repos/iMrBurn/wwe12-pc/releases?per_page=10", body,
                   nullptr)) {
        SetFailed(LastHttpError());
        return;
      }
      const std::string json(body.begin(), body.end());
      // The first release listed is the newest; its tag and its .zip asset.
      const std::string tag = JsonString(json, "\"tag_name\"", 0);
      // Its first downloadable .zip (the release's own zip, listed before older releases').
      std::string url;
      for (size_t key = json.find("\"browser_download_url\""); key != std::string::npos;
           key = json.find("\"browser_download_url\"", key + 1)) {
        const std::string u = JsonString(json, "\"browser_download_url\"", key);
        if (u.size() > 4 && u.compare(u.size() - 4, 4, ".zip") == 0) {
          url = u;
          break;
        }
      }
      if (tag.empty() || url.empty()) {
        // Keep what GitHub sent, to see why it was not understood.
        std::ofstream(dir_ / "launcher_update_reply.txt", std::ios::binary)
            .write(json.data(), std::streamsize(json.size()));
        SetFailed("reply not understood (" + std::to_string(json.size()) + " bytes)");
        return;
      }
      std::lock_guard lock(mutex_);
      latest_ = tag;
      url_ = url;
      // Dev aids: WWE12_LAUNCHER_FAKE_VERSION pretends to be another version,
      // WWE12_LAUNCHER_AUTO_INSTALL installs an update without the click.
      const char *fake = std::getenv("WWE12_LAUNCHER_FAKE_VERSION");
      state_ = Newer(tag, fake ? fake : WWE12_VERSION) ? State::Available : State::UpToDate;
      auto_install_ = state_ == State::Available && std::getenv("WWE12_LAUNCHER_AUTO_INSTALL");
    });
  }

  void Install() {
    if (worker_.joinable())
      worker_.join();
    state_ = State::Downloading;
    progress_ = 0;
    std::string url;
    {
      std::lock_guard lock(mutex_);
      url = url_;
    }
    worker_ = std::thread([this, url] {
      std::vector<uint8_t> zip;
      const std::wstring wurl(url.begin(), url.end());
      if (!HttpGet(wurl, zip, &progress_)) {
        SetFailed("download " + LastHttpError());
        return;
      }
      std::string err;
      if (!Apply(zip, err)) {
        SetFailed(err);
        return;
      }
      // Start the new launcher, then this one closes. The dev aids must not carry over (the new
      // launcher would install again, in a loop).
      SetEnvironmentVariableW(L"WWE12_LAUNCHER_AUTO_INSTALL", nullptr);
      SetEnvironmentVariableW(L"WWE12_LAUNCHER_FAKE_VERSION", nullptr);
      const std::wstring exe = (dir_ / L"WWE12 Launcher.exe").wstring();
      std::wstring cmd = L"\"" + exe + L"\"";
      STARTUPINFOW si{sizeof(si)};
      PROCESS_INFORMATION pi{};
      if (CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr,
                         dir_.wstring().c_str(), &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
      }
      state_ = State::Done;
      restart_ = true;
    });
  }

  void SetFailed(const std::string &e) {
    std::lock_guard lock(mutex_);
    error_ = e;
    state_ = State::Failed;
  }

  // "v0.5.0-beta" > "0.4.1-beta": numeric major.minor.patch.
  static bool Newer(const std::string &tag, const std::string &current) {
    auto parse = [](const std::string &s, int v[3]) {
      size_t i = 0;
      while (i < s.size() && !isdigit(static_cast<unsigned char>(s[i])))
        ++i;
      for (int k = 0; k < 3; ++k) {
        v[k] = 0;
        while (i < s.size() && isdigit(static_cast<unsigned char>(s[i])))
          v[k] = v[k] * 10 + (s[i++] - '0');
        if (i < s.size() && s[i] == '.')
          ++i;
      }
    };
    int a[3], b[3];
    parse(tag, a);
    parse(current, b);
    for (int k = 0; k < 3; ++k)
      if (a[k] != b[k])
        return a[k] > b[k];
    return false;
  }

  static std::string JsonString(const std::string &json, const char *key, size_t from) {
    size_t k = json.find(key, from);
    if (k == std::string::npos)
      return {};
    k = json.find('"', json.find(':', k) + 1);
    if (k == std::string::npos)
      return {};
    const size_t e = json.find('"', k + 1);
    return e == std::string::npos ? std::string() : json.substr(k + 1, e - k - 1);
  }

  // HTTPS GET following redirects (GitHub release assets redirect to their CDN).
  static std::string &HttpErrorSlot() {
    thread_local std::string e;
    return e;
  }
  static std::string LastHttpError() { return HttpErrorSlot(); }

  static bool HttpGet(const std::wstring &url, std::vector<uint8_t> &out,
                      std::atomic<float> *progress) {
    HttpErrorSlot().clear();
    auto fail = [](const char *step) {
      HttpErrorSlot() = std::string(step) + " " + std::to_string(GetLastError());
      return false;
    };
    URL_COMPONENTS uc{sizeof(uc)};
    wchar_t host[256], path[2048];
    uc.lpszHostName = host;
    uc.dwHostNameLength = 256;
    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc))
      return fail("url");
    HINTERNET session = WinHttpOpen(L"WWE12-PC-Launcher", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                    WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session)
      return fail("open");
    bool ok = false;
    HINTERNET conn = WinHttpConnect(session, host, uc.nPort, 0);
    HINTERNET req = conn ? WinHttpOpenRequest(conn, L"GET", path, nullptr, WINHTTP_NO_REFERER,
                                              WINHTTP_DEFAULT_ACCEPT_TYPES,
                                              uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0)
                         : nullptr;
    if (req && WinHttpSendRequest(req, L"Accept: application/vnd.github+json\r\n", DWORD(-1),
                                  WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(req, nullptr)) {
      DWORD status = 0, sz = sizeof(status);
      WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr,
                          &status, &sz, nullptr);
      wchar_t lenbuf[32];
      DWORD lensz = sizeof(lenbuf);
      uint64_t total = 0;
      if (WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH, nullptr, lenbuf, &lensz, nullptr))
        total = _wcstoui64(lenbuf, nullptr, 10);
      if (status != 200)
        HttpErrorSlot() = "HTTP " + std::to_string(status);
      if (status == 200) {
        ok = true;
        for (;;) {
          DWORD avail = 0;
          if (!WinHttpQueryDataAvailable(req, &avail)) {
            ok = false;
            break;
          }
          if (!avail)
            break;
          const size_t at = out.size();
          out.resize(at + avail);
          DWORD read = 0;
          if (!WinHttpReadData(req, out.data() + at, avail, &read)) {
            ok = false;
            break;
          }
          out.resize(at + read);
          if (progress && total)
            *progress = float(double(out.size()) / double(total));
        }
      }
    }
    if (!ok && HttpErrorSlot().empty())
      fail(req ? "request" : "connect");
    if (req)
      WinHttpCloseHandle(req);
    if (conn)
      WinHttpCloseHandle(conn);
    WinHttpCloseHandle(session);
    return ok;
  }

  static uint32_t U32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24); }
  static uint16_t U16(const uint8_t *p) { return uint16_t(p[0] | (p[1] << 8)); }

  // Unpacks the release zip (entries under "WWE12-PC/") over the install folder.
  bool Apply(const std::vector<uint8_t> &zip, std::string &err) {
    namespace fs = std::filesystem;
    // End of central directory.
    if (zip.size() < 22) {
      err = "zip";
      return false;
    }
    size_t eocd = zip.size() - 22;
    while (eocd > 0 && U32(&zip[eocd]) != 0x06054b50)
      --eocd;
    if (U32(&zip[eocd]) != 0x06054b50) {
      err = "zip";
      return false;
    }
    const uint16_t entries = U16(&zip[eocd + 10]);
    size_t cd = U32(&zip[eocd + 16]);
    for (uint16_t i = 0; i < entries; ++i) {
      if (cd + 46 > zip.size() || U32(&zip[cd]) != 0x02014b50) {
        err = "zip";
        return false;
      }
      const uint16_t method = U16(&zip[cd + 10]);
      const uint32_t csize = U32(&zip[cd + 20]);
      const uint32_t usize = U32(&zip[cd + 24]);
      const uint16_t nlen = U16(&zip[cd + 28]), xlen = U16(&zip[cd + 30]), clen = U16(&zip[cd + 32]);
      const uint32_t local = U32(&zip[cd + 42]);
      std::string name(reinterpret_cast<const char *>(&zip[cd + 46]), nlen);
      cd += 46 + nlen + xlen + clen;
      for (auto &c : name)
        if (c == '\\')
          c = '/';
      const std::string prefix = "WWE12-PC/";
      if (name.rfind(prefix, 0) != 0 || name.back() == '/')
        continue;
      const std::string rel = name.substr(prefix.size());
      // The player's own files stay as they are.
      if (rel == "wwe12.toml" || rel.rfind("game/", 0) == 0 || rel.rfind("mods/", 0) == 0 ||
          rel.find("..") != std::string::npos)
        continue;
      const size_t data = local + 30 + U16(&zip[local + 26]) + U16(&zip[local + 28]);
      if (data + csize > zip.size()) {
        err = "zip";
        return false;
      }
      std::vector<uint8_t> bytes;
      if (method == 0) {
        bytes.assign(zip.begin() + data, zip.begin() + data + csize);
      } else if (method == 8) {
        bytes.reserve(usize);
        if (!Inflater::Inflate(&zip[data], csize, bytes) || bytes.size() != usize) {
          err = "inflate " + rel;
          return false;
        }
      } else {
        err = "zip method";
        return false;
      }
      const fs::path target = dir_ / fs::path(std::u8string(rel.begin(), rel.end()));
      std::error_code ec;
      fs::create_directories(target.parent_path(), ec);
      // A running file (this launcher) cannot be overwritten, but it can be renamed.
      if (fs::exists(target, ec)) {
        fs::path old = target;
        old += ".old";
        fs::remove(old, ec);
        fs::rename(target, old, ec);
      }
      std::ofstream f(target, std::ios::binary | std::ios::trunc);
      f.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
      if (!f) {
        err = "write " + rel;
        return false;
      }
    }
    return true;
  }

  void CleanOld() {
    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator(dir_, ec);
         it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
      if (it->is_directory(ec) && (it->path().filename() == "game" || it->path().filename() == "mods")) {
        it.disable_recursion_pending();
        continue;
      }
      if (it->path().extension() == ".old")
        std::filesystem::remove(it->path(), ec);
      if (it.depth() > 2)
        it.disable_recursion_pending();
    }
  }

  bool ReadPref() const {
    std::ifstream f(dir_ / "launcher.ini");
    std::string line;
    while (std::getline(f, line))
      if (line.rfind("check_updates=", 0) == 0)
        return line.substr(14) != "0";
    return true;
  }
  void WritePref(bool on) const {
    std::ofstream f(dir_ / "launcher.ini", std::ios::trunc);
    f << "check_updates=" << (on ? 1 : 0) << "\n";
  }

  std::filesystem::path dir_;
  std::atomic<State> state_{State::Idle};
  std::atomic<float> progress_{0};
  std::mutex mutex_;
  std::string latest_, url_, error_;
  bool check_at_start_ = true;
  std::atomic<bool> restart_{false};
  std::atomic<bool> auto_install_{false};
  std::thread worker_;

 public:
  ~Updater() {
    if (worker_.joinable())
      worker_.detach();
  }
};
