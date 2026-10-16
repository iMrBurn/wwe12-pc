// Launcher updates from the project's GitHub releases: checks the newest release of the chosen
// channel (stable only, or stable + betas), downloads its zip and installs it over the install,
// keeping the player's settings (wwe12.toml), the game folder and the mods folder.
//
// Installing is all-or-nothing: the whole zip is unpacked in memory and every file's CRC checked
// before anything on disk is touched; the new files are then written next to the old ones
// (name.new), and swapped in with renames (old -> name.old, new -> name). If anything fails the
// renames are undone and the install is left as it was. Files in use (the launcher itself) are
// renamed rather than overwritten; the *.old files are deleted at the next start.
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

#include "changelog.h"
#include "game_process.h"
#include "imgui.h"
#include "inflate.h"

struct UpdaterTexts {
  const char *checking, *up_to_date, *available, *install, *failed, *check_now, *check_at_start,
      *downloading, *update_failed, *include_beta, *game_running;
};

class Updater {
 public:
  enum class State { Idle, Checking, UpToDate, Available, Downloading, Failed, Done };

  void Start(const std::filesystem::path &dir) {
    dir_ = dir;
    CleanOld();
    LoadPrefs();
    if (check_at_start_)
      Check();
  }

  bool RestartRequested() const { return restart_; }
  // A download is running: starting the game now would end the launcher (and the update).
  bool Busy() const { return state_ == State::Downloading; }
  // Called when the launcher closes: lets an install that is swapping files finish, so closing
  // the window can never leave a half-updated install.
  void Shutdown() {
    if (applying_ && worker_.joinable())
      worker_.join();
  }

  void Draw(const UpdaterTexts &t) {
    if (auto_install_.exchange(false))
      Install();
    std::string latest, error, notes;
    {
      std::lock_guard lock(mutex_);
      latest = latest_;
      error = error_;
      notes = notes_;
    }
    switch (state_.load()) {
    case State::Idle:
      break;
    case State::Checking:
      ImGui::TextDisabled("%s", t.checking);
      break;
    case State::UpToDate:
      ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.45f, 1.0f), "%s", t.up_to_date);
      break;
    case State::Available:
      ImGui::TextColored(ImVec4(0.94f, 0.75f, 0.24f, 1.0f), "%s %s", t.available, latest.c_str());
      if (!notes.empty()) {
        ImGui::BeginChild("##release_notes", ImVec2(0, ImGui::GetTextLineHeight() * 11),
                          ImGuiChildFlags_Borders);
        changelog::Draw(notes);
        ImGui::EndChild();
      }
      if (ImGui::Button(t.install))
        Install();
      break;
    case State::Downloading:
      ImGui::Text("%s", t.downloading);
      ImGui::ProgressBar(progress_.load(), ImVec2(320, 0));
      break;
    case State::Failed:
      if (game_running_fail_) {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", t.game_running);
      } else {
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", error.empty() ? t.failed : t.update_failed);
        if (!error.empty())
          ImGui::TextWrapped("%s", error.c_str());
      }
      break;
    case State::Done:
      break;
    }
    const bool busy = state_ == State::Checking || state_ == State::Downloading;
    if (!busy) {
      if (ImGui::Button(t.check_now))
        Check();
      ImGui::SameLine();
    }
    if (ImGui::Checkbox(t.check_at_start, &check_at_start_))
      SavePrefs();
    if (ImGui::Checkbox(t.include_beta, &include_beta_)) {
      SavePrefs();
      if (!busy)
        Check();
    }
  }

  // "v0.6.0-beta" vs "0.6.0": numeric major.minor.patch first, then alpha < beta < rc < stable, then
  // the number after the word (rc2 > rc1). Public for tests.
  static bool Newer(const std::string &tag, const std::string &current) {
    return Parse(tag) > Parse(current);
  }

 private:
  struct Version {
    int n[3] = {0, 0, 0};
    int rank = 3;  // alpha 0, beta 1, rc 2, no suffix (stable) 3
    int pre = 0;
    bool operator>(const Version &o) const {
      for (int k = 0; k < 3; ++k)
        if (n[k] != o.n[k])
          return n[k] > o.n[k];
      if (rank != o.rank)
        return rank > o.rank;
      return pre > o.pre;
    }
  };

  static Version Parse(const std::string &s) {
    Version v;
    size_t i = 0;
    while (i < s.size() && !isdigit(static_cast<unsigned char>(s[i])))
      ++i;
    for (int k = 0; k < 3; ++k) {
      while (i < s.size() && isdigit(static_cast<unsigned char>(s[i])))
        v.n[k] = v.n[k] * 10 + (s[i++] - '0');
      if (i < s.size() && s[i] == '.' && k < 2)
        ++i;
    }
    while (i < s.size() && (s[i] == '-' || s[i] == '.' || s[i] == '_'))
      ++i;
    std::string word;
    while (i < s.size() && isalpha(static_cast<unsigned char>(s[i])))
      word += char(tolower(static_cast<unsigned char>(s[i++])));
    if (!word.empty()) {
      v.rank = word == "alpha" || word == "a" ? 0 : word == "rc" || word == "pre" ? 2 : 1;
      while (i < s.size() && !isdigit(static_cast<unsigned char>(s[i])))
        ++i;
      while (i < s.size() && isdigit(static_cast<unsigned char>(s[i])))
        v.pre = v.pre * 10 + (s[i++] - '0');
    }
    return v;
  }

  void Check() {
    if (worker_.joinable())
      worker_.join();
    state_ = State::Checking;
    game_running_fail_ = false;
    const bool include_pre = include_beta_;
    worker_ = std::thread([this, include_pre] {
      std::vector<uint8_t> body;
      if (!HttpGet(L"https://api.github.com/repos/iMrBurn/wwe12-pc/releases?per_page=15", body,
                   nullptr)) {
        SetFailed(LastHttpError());
        return;
      }
      const std::string json(body.begin(), body.end());
      // Each release is read in its own part of the reply (from its "tag_name" to the next one):
      // tag, "prerelease" flag, first .zip asset and notes. A release still without its zip is
      // not offered, and an older release's zip is never taken for a newer tag.
      std::vector<size_t> starts;
      for (size_t k = json.find("\"tag_name\""); k != std::string::npos; k = json.find("\"tag_name\"", k + 1))
        starts.push_back(k);
      if (starts.empty()) {
        // Keep what GitHub sent, to see why it was not understood.
        std::ofstream(dir_ / "launcher_update_reply.txt", std::ios::binary)
            .write(json.data(), std::streamsize(json.size()));
        SetFailed("reply not understood (" + std::to_string(json.size()) + " bytes)");
        return;
      }
      std::string best_tag, best_url, best_notes;
      for (size_t r = 0; r < starts.size(); ++r) {
        const size_t end = r + 1 < starts.size() ? starts[r + 1] : std::string::npos;
        const std::string part = json.substr(starts[r], end == std::string::npos ? end : end - starts[r]);
        const std::string tag = JsonString(part, "\"tag_name\"", 0);
        if (tag.empty() || (!include_pre && IsPrerelease(part)))
          continue;
        std::string url;
        for (size_t key = part.find("\"browser_download_url\""); key != std::string::npos;
             key = part.find("\"browser_download_url\"", key + 1)) {
          const std::string u = JsonString(part, "\"browser_download_url\"", key);
          if (u.size() > 4 && u.compare(u.size() - 4, 4, ".zip") == 0) {
            url = u;
            break;
          }
        }
        // Only files of this project's own releases are ever downloaded.
        if (url.rfind("https://github.com/iMrBurn/wwe12-pc/releases/download/", 0) != 0)
          url.clear();
        if (url.empty() || (!best_tag.empty() && !Newer(tag, best_tag)))
          continue;
        best_tag = tag;
        best_url = url;
        best_notes = changelog::Prepare(changelog::JsonStringValue(part, "\"body\""));
      }
      std::lock_guard lock(mutex_);
      if (best_tag.empty()) {  // nothing in this channel
        latest_.clear();
        url_.clear();
        notes_.clear();
        state_ = State::UpToDate;
        return;
      }
      latest_ = best_tag;
      url_ = best_url;
      notes_ = best_notes;
      // Dev aids: WWE12_LAUNCHER_FAKE_VERSION pretends to be another version,
      // WWE12_LAUNCHER_AUTO_INSTALL installs an update without the click.
      const char *fake = std::getenv("WWE12_LAUNCHER_FAKE_VERSION");
      state_ = Newer(best_tag, fake ? fake : WWE12_VERSION) ? State::Available : State::UpToDate;
      auto_install_ = state_ == State::Available && std::getenv("WWE12_LAUNCHER_AUTO_INSTALL");
    });
  }

  void Install() {
    game_running_fail_ = game::IsRunning();  // wwe12.exe is locked while the game runs
    if (game_running_fail_) {
      state_ = State::Failed;
      return;
    }
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
      applying_ = true;
      const bool applied = Apply(zip, err);
      applying_ = false;
      if (!applied) {
        SetFailed(err + " (nothing was changed)");
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

  static bool IsPrerelease(const std::string &part) {
    const size_t k = part.find("\"prerelease\"");
    if (k == std::string::npos)
      return false;
    const size_t v = part.find_first_not_of(" \t\r\n:", k + 12);
    return v != std::string::npos && part.compare(v, 4, "true") == 0;
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
    WinHttpSetTimeouts(session, 15000, 15000, 30000, 60000);  // resolve, connect, send, receive (ms)
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
        // A download cut short must not be unpacked.
        if (ok && total && out.size() != total) {
          ok = false;
          HttpErrorSlot() = "incomplete (" + std::to_string(out.size()) + " of " + std::to_string(total) + " bytes)";
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

  static uint32_t Crc32(const uint8_t *data, size_t size) {
    static const auto table = [] {
      std::vector<uint32_t> t(256);
      for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k)
          c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        t[i] = c;
      }
      return t;
    }();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; ++i)
      crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
  }

  struct StagedFile {
    std::string rel;
    std::vector<uint8_t> bytes;
  };

  // Unpacks the release zip (entries under "WWE12-PC/") in memory, checking each file's size and
  // CRC. Nothing is written to disk here.
  static bool Unpack(const std::vector<uint8_t> &zip, std::vector<StagedFile> &files, std::string &err) {
    if (zip.size() < 22) {
      err = "zip too small";
      return false;
    }
    size_t eocd = zip.size() - 22;
    while (eocd > 0 && U32(&zip[eocd]) != 0x06054b50)
      --eocd;
    if (U32(&zip[eocd]) != 0x06054b50) {
      err = "zip directory not found";
      return false;
    }
    const uint16_t entries = U16(&zip[eocd + 10]);
    size_t cd = U32(&zip[eocd + 16]);
    for (uint16_t i = 0; i < entries; ++i) {
      if (cd + 46 > zip.size() || U32(&zip[cd]) != 0x02014b50) {
        err = "zip directory damaged";
        return false;
      }
      const uint16_t method = U16(&zip[cd + 10]);
      const uint32_t crc = U32(&zip[cd + 16]);
      const uint32_t csize = U32(&zip[cd + 20]);
      const uint32_t usize = U32(&zip[cd + 24]);
      const uint16_t nlen = U16(&zip[cd + 28]), xlen = U16(&zip[cd + 30]), clen = U16(&zip[cd + 32]);
      const uint32_t local = U32(&zip[cd + 42]);
      if (cd + 46 + nlen > zip.size()) {
        err = "zip directory damaged";
        return false;
      }
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
      if (rel == "wwe12.toml" || rel.rfind("game/", 0) == 0 || rel.rfind("mods/", 0) == 0)
        continue;
      // Only plain relative paths: no "..", no drive letters, no absolute paths.
      if (rel.empty() || rel[0] == '/' || rel.find("..") != std::string::npos || rel.find(':') != std::string::npos) {
        err = "unsafe path in zip: " + rel;
        return false;
      }
      if (csize == 0xFFFFFFFFu || usize == 0xFFFFFFFFu || local + 30 > zip.size()) {
        err = "zip entry not supported: " + rel;
        return false;
      }
      const size_t data = local + 30 + U16(&zip[local + 26]) + U16(&zip[local + 28]);
      if (data + csize > zip.size()) {
        err = "zip truncated: " + rel;
        return false;
      }
      StagedFile f;
      f.rel = rel;
      if (method == 0) {
        f.bytes.assign(zip.begin() + data, zip.begin() + data + csize);
      } else if (method == 8) {
        f.bytes.reserve(usize);
        if (!Inflater::Inflate(&zip[data], csize, f.bytes)) {
          err = "cannot unpack " + rel;
          return false;
        }
      } else {
        err = "zip method not supported: " + rel;
        return false;
      }
      if (f.bytes.size() != usize || Crc32(f.bytes.data(), f.bytes.size()) != crc) {
        err = "download damaged (checksum): " + rel;
        return false;
      }
      files.push_back(std::move(f));
    }
    if (files.empty()) {
      err = "no files in zip";
      return false;
    }
    return true;
  }

  // Installs the release zip over dir_: checked in memory, written as name.new, swapped in with
  // renames. On any failure the renames are undone.
  bool Apply(const std::vector<uint8_t> &zip, std::string &err) {
    namespace fs = std::filesystem;
    std::vector<StagedFile> files;
    if (!Unpack(zip, files, err))
      return false;

    auto target_of = [&](const std::string &rel) {
      return dir_ / fs::path(std::u8string(rel.begin(), rel.end()));
    };
    auto with_ext = [](fs::path p, const char *ext) {
      p += ext;
      return p;
    };

    // 1. Write every new file next to its target. Nothing of the install is touched yet.
    std::error_code ec;
    std::vector<fs::path> written;
    auto discard_written = [&] {
      std::error_code e;
      for (const auto &p : written)
        fs::remove(p, e);
    };
    for (const auto &f : files) {
      const fs::path target = target_of(f.rel);
      fs::create_directories(target.parent_path(), ec);
      const fs::path tmp = with_ext(target, ".new");
      std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
      written.push_back(tmp);
      out.write(reinterpret_cast<const char *>(f.bytes.data()), std::streamsize(f.bytes.size()));
      out.close();
      if (!out) {
        err = "cannot write " + f.rel + " (disk full?)";
        discard_written();
        return false;
      }
    }

    // 2. Swap: old -> name.old, new -> name. Undone in reverse order if one step fails.
    struct Swapped {
      fs::path target, old, tmp;
      bool had_old;
    };
    std::vector<Swapped> done;
    auto undo = [&] {
      std::error_code e;
      for (auto it = done.rbegin(); it != done.rend(); ++it) {
        fs::remove(it->target, e);
        if (it->had_old)
          fs::rename(it->old, it->target, e);
      }
      discard_written();
    };
    for (const auto &f : files) {
      Swapped s{target_of(f.rel), {}, {}, false};
      s.old = with_ext(s.target, ".old");
      s.tmp = with_ext(s.target, ".new");
      s.had_old = fs::exists(s.target, ec);
      if (s.had_old) {
        fs::remove(s.old, ec);
        fs::rename(s.target, s.old, ec);
        if (ec) {
          err = "cannot replace " + f.rel + " (" + ec.message() + ")";
          undo();
          return false;
        }
      }
      fs::rename(s.tmp, s.target, ec);
      if (ec) {
        err = "cannot install " + f.rel + " (" + ec.message() + ")";
        if (s.had_old) {
          std::error_code e;
          fs::rename(s.old, s.target, e);
        }
        undo();
        return false;
      }
      done.push_back(std::move(s));
    }
    return true;
  }

  // Leftovers of earlier updates: *.old (replaced files, the old launcher) and *.new (a crash
  // between writing and swapping). The player's game, mods and dlc folders are not looked at.
  void CleanOld() {
    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator(dir_, ec);
         it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
      if (it->is_directory(ec) && (it->path().filename() == "game" || it->path().filename() == "mods" ||
                                   it->path().filename() == "dlc")) {
        it.disable_recursion_pending();
        continue;
      }
      const auto ext = it->path().extension();
      if (ext == ".old" || ext == ".new")
        std::filesystem::remove(it->path(), ec);
      if (it.depth() > 2)
        it.disable_recursion_pending();
    }
  }

  // launcher.ini: check_updates=0|1, channel=stable|beta. Without a channel, a beta install
  // follows the betas and a stable install follows the stable releases.
  void LoadPrefs() {
    check_at_start_ = true;
    include_beta_ = Parse(WWE12_VERSION).rank < 3;
    std::ifstream f(dir_ / "launcher.ini");
    std::string line;
    while (std::getline(f, line)) {
      if (line.rfind("check_updates=", 0) == 0)
        check_at_start_ = line.substr(14) != "0";
      else if (line.rfind("channel=", 0) == 0)
        include_beta_ = line.substr(8).rfind("beta", 0) == 0;
    }
  }
  void SavePrefs() const {
    const std::filesystem::path path = dir_ / "launcher.ini", tmp = dir_ / "launcher.ini.new";
    {
      std::ofstream f(tmp, std::ios::trunc);
      f << "check_updates=" << (check_at_start_ ? 1 : 0) << "\n";
      f << "channel=" << (include_beta_ ? "beta" : "stable") << "\n";
    }
    std::error_code ec;
    std::filesystem::rename(tmp, path, ec);
  }

  std::filesystem::path dir_;
  std::atomic<State> state_{State::Idle};
  std::atomic<float> progress_{0};
  std::mutex mutex_;
  std::string latest_, url_, error_, notes_;
  bool check_at_start_ = true;
  bool include_beta_ = true;
  std::atomic<bool> game_running_fail_{false};
  std::atomic<bool> restart_{false};
  std::atomic<bool> applying_{false};  // files are being swapped: do not kill the process
  std::atomic<bool> auto_install_{false};
  std::thread worker_;

 public:
  ~Updater() {
    if (worker_.joinable())
      worker_.detach();
  }
};
