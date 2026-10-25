// Mod Maker: Bink 2 movies (WWE '13's movies\titantron\NNN.bik2, the WWE 2K games' ent_NNNN_A_B.bk2: titantrons
// laid out like WWE '12's, 320 x 320) made into WWE '12's Bink 1, with a decoder the PLAYER installed (nothing of
// RAD's ships with the port), as Kaiko's SvR 2011 Mod Maker does (idea only, this code is ours):
//  - RAD Video Tools (free from RAD Game Tools): radvideo64.exe BinkConv <in> <out.avi> writes a raw AVI; it is run
//    on a desktop of its own, never shown, so its progress and "Done" windows don't reach the player's screen;
//  - Librempeg's ffmpeg (an FFmpeg fork with a Bink 2 decoder): raw frames through a pipe (mainline FFmpeg has no
//    Bink 2 decoder: an ffmpeg counts only when "-decoders" lists bink2).
// Never a game's own bink2w64.dll. The frames are encoded again by our Bink 1 encoder (video/bink_encode.h), 30 fps.
// What reads what (2026-10-25, RAD Video Tools 2024): WWE 2K15-2K17's KB2i yes; WWE '13's KB2d and WWE 2K14's KB2f
// no ("Error opening"). The 2K games' titantron (320 x 320) is one 16:9 picture squeezed into the square, no strip.
#pragma once

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>
#include "imgui.h"
#include "svrfmt/texture.h"

extern "C" {
#include "video/bink_encode.h"
}

namespace bink2 {

namespace fs = std::filesystem;

struct Info {
  char revision = 0;  // KB2<revision>: a-e the early (2012) format (WWE '13: d), i the 2K games'
  uint32_t frames = 0, width = 0, height = 0;
  double fps = 0;
};

inline bool ReadInfo(const fs::path &p, Info &out) {
  uint8_t h[44] = {};
  std::ifstream f(p, std::ios::binary);
  if (!f.read(reinterpret_cast<char *>(h), sizeof(h)) || memcmp(h, "KB2", 3) != 0)
    return false;
  uint32_t v[4];
  memcpy(&out.frames, h + 8, 4);
  memcpy(v, h + 20, 16);
  out.revision = char(h[3]);
  out.width = v[0];
  out.height = v[1];
  out.fps = v[3] ? double(v[2]) / v[3] : 0;
  return true;
}

enum class Kind { kNone, kRad, kLibrempeg };
struct Tool {
  Kind kind = Kind::kNone;
  fs::path exe;
};

// A program's standard output, run without a window (nullopt-like: false if it didn't start).
inline bool RunCapture(const std::wstring &cmd, std::string &out, DWORD timeout_ms) {
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  HANDLE rd = nullptr, wr = nullptr;
  if (!CreatePipe(&rd, &wr, &sa, 0))
    return false;
  SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
  STARTUPINFOW si{sizeof(si)};
  si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  si.hStdOutput = wr;
  si.hStdError = wr;
  PROCESS_INFORMATION pi{};
  std::wstring line = cmd;
  const BOOL ok = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr,
                                 &si, &pi);
  CloseHandle(wr);
  if (!ok) {
    CloseHandle(rd);
    return false;
  }
  char buf[4096];
  DWORD n = 0;
  const DWORD t0 = GetTickCount();
  while (ReadFile(rd, buf, sizeof(buf), &n, nullptr) && n) {
    out.append(buf, n);
    if (GetTickCount() - t0 > timeout_ms)
      break;
  }
  if (WaitForSingleObject(pi.hProcess, 2000) == WAIT_TIMEOUT)
    TerminateProcess(pi.hProcess, 1);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  CloseHandle(rd);
  return true;
}

inline std::wstring Quote(const fs::path &p) { return L"\"" + p.wstring() + L"\""; }

// What a program is: RAD Video Tools (radvideo*.exe), or an ffmpeg that has a Bink 2 decoder.
inline Kind KindOf(const fs::path &exe) {
  std::error_code ec;
  if (exe.empty() || !fs::is_regular_file(exe, ec))
    return Kind::kNone;
  std::wstring name = exe.filename().wstring();
  for (auto &c : name)
    c = towlower(c);
  if (name.rfind(L"radvideo", 0) == 0)
    return Kind::kRad;
  std::string out;
  if (RunCapture(Quote(exe) + L" -hide_banner -decoders", out, 30000) && out.find(" bink2 ") != std::string::npos)
    return Kind::kLibrempeg;
  return Kind::kNone;
}

// The tool: the one the player chose (`chosen`, if it still works), else RAD Video Tools in its usual folders or
// from its uninstall entry, else an ffmpeg / librempeg with Bink 2 on the PATH.
inline Tool Find(const fs::path &chosen) {
  if (const Kind k = KindOf(chosen); k != Kind::kNone)
    return {k, chosen};
  std::vector<fs::path> rad;
  for (const wchar_t *env : {L"ProgramFiles(x86)", L"ProgramFiles", L"ProgramW6432"}) {
    wchar_t v[MAX_PATH];
    if (GetEnvironmentVariableW(env, v, MAX_PATH))
      rad.push_back(fs::path(v) / L"RADVideo" / L"radvideo64.exe");
  }
  rad.push_back(L"C:\\RADVideo\\radvideo64.exe");
  for (HKEY root : {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER})
    for (const wchar_t *key : {L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
                               L"SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"}) {
      HKEY k;
      if (RegOpenKeyExW(root, key, 0, KEY_READ, &k) != ERROR_SUCCESS)
        continue;
      wchar_t sub[256];
      for (DWORD i = 0, n = 256; RegEnumKeyExW(k, i, sub, &n, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS;
           ++i, n = 256) {
        wchar_t name[256] = {}, where[MAX_PATH] = {};
        DWORD ns = sizeof(name), ws = sizeof(where);
        if (RegGetValueW(k, sub, L"DisplayName", RRF_RT_REG_SZ, nullptr, name, &ns) == ERROR_SUCCESS &&
            wcsstr(name, L"RAD Video Tools")) {
          if (RegGetValueW(k, sub, L"InstallLocation", RRF_RT_REG_SZ, nullptr, where, &ws) == ERROR_SUCCESS &&
              *where)
            rad.push_back(fs::path(where) / L"radvideo64.exe");
          ws = sizeof(where);
          if (RegGetValueW(k, sub, L"UninstallString", RRF_RT_REG_SZ, nullptr, where, &ws) == ERROR_SUCCESS) {
            std::wstring u = where;
            u.erase(std::remove(u.begin(), u.end(), L'"'), u.end());
            rad.push_back(fs::path(u).parent_path() / L"radvideo64.exe");
          }
        }
      }
      RegCloseKey(k);
    }
  std::error_code ec;
  for (const auto &p : rad)
    if (fs::is_regular_file(p, ec))
      return {Kind::kRad, p};
  for (const wchar_t *exe : {L"librempeg.exe", L"ffmpeg.exe"}) {
    wchar_t found[MAX_PATH];
    if (SearchPathW(nullptr, exe, nullptr, MAX_PATH, found, nullptr) && KindOf(found) == Kind::kLibrempeg)
      return {Kind::kLibrempeg, found};
  }
  return {};
}

// Runs a command on a desktop of its own (never shown): its windows don't reach the player's screen; a window that
// says "Done" is closed; ended after timeout_ms. Its exit code, or -1 if it didn't start / was ended.
inline int RunOnOwnDesktop(const std::wstring &cmd, DWORD timeout_ms, std::wstring *err) {
  wchar_t name[64];
  swprintf_s(name, L"wwe12_bink2_%lu_%lu", GetCurrentProcessId(), GetTickCount());
  HDESK desk = CreateDesktopW(name, nullptr, nullptr, 0, GENERIC_ALL, nullptr);
  if (!desk) {
    if (err)
      *err = L"no desktop of its own";
    return -1;
  }
  STARTUPINFOW si{sizeof(si)};
  si.lpDesktop = name;
  PROCESS_INFORMATION pi{};
  std::wstring line = cmd;
  if (!CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    CloseDesktop(desk);
    if (err)
      *err = L"RAD Video Tools didn't start";
    return -1;
  }
  struct Look {
    DWORD pid;
    bool done = false;
    std::wstring error;  // (a message box "Error ...")
  } look{pi.dwProcessId};
  const DWORD t0 = GetTickCount();
  int code = -1;
  for (;;) {
    if (WaitForSingleObject(pi.hProcess, 250) == WAIT_OBJECT_0) {
      DWORD c = 0;
      GetExitCodeProcess(pi.hProcess, &c);
      code = int(c);
      break;
    }
    // its "Done!" window (a message box): closed, the run is over
    EnumDesktopWindows(
        desk,
        [](HWND h, LPARAM lp) -> BOOL {
          auto *l = reinterpret_cast<Look *>(lp);
          DWORD pid = 0;
          GetWindowThreadProcessId(h, &pid);
          if (pid != l->pid || !IsWindowVisible(h))
            return TRUE;
          wchar_t t[256] = {};
          GetWindowTextW(h, t, 256);
          struct Seen {
            bool done;
            std::wstring error;
          } seen{wcsstr(t, L"Done") != nullptr, {}};
          EnumChildWindows(
              h,
              [](HWND c, LPARAM d) -> BOOL {
                auto *sn = reinterpret_cast<Seen *>(d);
                wchar_t s[512] = {};
                GetWindowTextW(c, s, 512);
                if (wcsstr(s, L"Done"))
                  sn->done = true;
                if (wcsncmp(s, L"Error", 5) == 0)
                  sn->error = s;
                return TRUE;
              },
              LPARAM(&seen));
          if (!seen.error.empty())
            l->error = seen.error;
          else if (seen.done) {
            l->done = true;
            PostMessageW(h, WM_CLOSE, 0, 0);
          }
          return TRUE;
        },
        LPARAM(&look));
    if (!look.error.empty()) {  // (RAD can't read it: "Error opening: <file>")
      TerminateProcess(pi.hProcess, 1);
      if (err)
        *err = L"RAD Video Tools: " + look.error.substr(0, look.error.find(L':'));
      break;
    }
    if (GetTickCount() - t0 > timeout_ms) {
      TerminateProcess(pi.hProcess, 1);
      if (err)
        *err = L"RAD Video Tools took too long";
      break;
    }
  }
  if (look.done && code < 0)
    code = 0;
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  CloseDesktop(desk);
  return code;
}

// The frames of an uncompressed AVI (24 or 32 bits, as RAD Video Tools writes them), each as BGRA top row first.
inline bool ReadAvi(const fs::path &p, const std::function<bool(const uint8_t *bgra, int w, int h)> &frame,
                    std::wstring *err) {
  std::ifstream f(p, std::ios::binary);
  if (!f) {
    if (err)
      *err = L"no AVI written";
    return false;
  }
  int w = 0, h = 0, bits = 0;
  bool bottom_up = true;
  std::vector<uint8_t> bgra;
  // walks RIFF / LIST chunks; frames are "##db" / "##dc" chunks
  const std::function<bool(std::streamoff, std::streamoff)> walk = [&](std::streamoff at, std::streamoff end) {
    while (at + 8 <= end) {
      char id[4];
      uint32_t size = 0;
      f.seekg(at);
      if (!f.read(id, 4) || !f.read(reinterpret_cast<char *>(&size), 4))
        return true;
      const std::streamoff body = at + 8;
      if (!memcmp(id, "RIFF", 4) || !memcmp(id, "LIST", 4)) {
        if (!walk(body + 4, (std::min<std::streamoff>)(end, body + size)))
          return false;
      } else if (!memcmp(id, "strf", 4) && !w && size >= 40) {
        uint8_t bi[40];
        f.read(reinterpret_cast<char *>(bi), 40);
        int32_t bw, bh;
        uint16_t bc;
        memcpy(&bw, bi + 4, 4);
        memcpy(&bh, bi + 8, 4);
        memcpy(&bc, bi + 14, 2);
        w = bw;
        h = bh < 0 ? -bh : bh;
        bottom_up = bh > 0;
        bits = bc;
      } else if ((id[2] == 'd' && (id[3] == 'b' || id[3] == 'c')) && w > 0 && (bits == 24 || bits == 32)) {
        const size_t stride = (size_t(w) * (bits / 8) + 3) & ~size_t(3);
        if (size >= stride * h) {
          std::vector<uint8_t> raw(stride * h);
          f.read(reinterpret_cast<char *>(raw.data()), std::streamsize(raw.size()));
          bgra.resize(size_t(w) * h * 4);
          for (int y = 0; y < h; ++y) {
            const uint8_t *src = &raw[stride * size_t(bottom_up ? h - 1 - y : y)];
            uint8_t *dst = &bgra[size_t(y) * w * 4];
            for (int x = 0; x < w; ++x) {
              dst[x * 4 + 0] = src[x * (bits / 8) + 0];
              dst[x * 4 + 1] = src[x * (bits / 8) + 1];
              dst[x * 4 + 2] = src[x * (bits / 8) + 2];
              dst[x * 4 + 3] = 255;
            }
          }
          if (!frame(bgra.data(), w, h))
            return false;
        }
      }
      at = body + size + (size & 1);
    }
    return true;
  };
  f.seekg(0, std::ios::end);
  const std::streamoff total = f.tellg();
  walk(0, total);
  if (!w && err)
    *err = L"an AVI not understood";
  return w > 0;
}

// in (Bink 2) -> out (Bink 1, WWE '12's titantron layout when the source is 320 x 320), with the tool; progress
// 0-100. The temporary files are removed.
inline bool ToBink1(const Tool &tool, const fs::path &in, const fs::path &out, std::atomic<int> *progress,
                    std::atomic<bool> *cancel, std::wstring *err, int out_w = 0, int out_h = 0) {
  Info info;
  if (!ReadInfo(in, info)) {
    if (err)
      *err = L"not a Bink 2 movie";
    return false;
  }
  if (tool.kind == Kind::kNone) {
    if (err)
      *err = L"no Bink 2 decoder installed";
    return false;
  }
  // the size: the source's, in 16s (up to 1024); 30 fps (29.97 movies keep their frames)
  const int W = (std::min)(1024, (std::max)(16, int(out_w ? out_w : info.width + 8) / 16 * 16));
  const int H = (std::min)(1024, (std::max)(16, int(out_h ? out_h : info.height + 8) / 16 * 16));
  const int frames = int(info.frames);
  FILE *f = _wfopen(out.c_str(), L"wb");
  if (!f) {
    if (err)
      *err = L"can't write " + out.wstring();
    return false;
  }
  BinkWriter *bw = bink_writer_open_size(f, frames, W, H);
  if (!bw) {
    fclose(f);
    if (err)
      *err = L"out of memory";
    return false;
  }
  int done = 0;
  std::vector<uint8_t> last;
  const auto put = [&](const uint8_t *bgra, int w, int h) {
    if (done >= frames || (cancel && *cancel))
      return false;
    if (w == W && h == H) {
      last.assign(bgra, bgra + size_t(W) * H * 4);
    } else {  // (a size not in 16s)
      svrfmt::Image img;
      img.w = w;
      img.h = h;
      img.rgba.resize(size_t(w) * h * 4);
      for (size_t i = 0; i < img.rgba.size(); i += 4) {  // BGRA <-> RGBA
        img.rgba[i] = bgra[i + 2];
        img.rgba[i + 1] = bgra[i + 1];
        img.rgba[i + 2] = bgra[i];
        img.rgba[i + 3] = 255;
      }
      const svrfmt::Image s = svrfmt::Resize(img, W, H);
      last.resize(s.rgba.size());
      for (size_t i = 0; i < s.rgba.size(); i += 4) {
        last[i] = s.rgba[i + 2];
        last[i + 1] = s.rgba[i + 1];
        last[i + 2] = s.rgba[i];
        last[i + 3] = 255;
      }
    }
    if (bink_writer_frame(bw, last.data()) != 1)
      return false;
    ++done;
    if (progress)
      *progress = 10 + 90 * done / (std::max)(1, frames);
    return true;
  };
  bool ok = false;
  if (tool.kind == Kind::kRad) {
    wchar_t tmp[MAX_PATH];
    GetTempPathW(MAX_PATH, tmp);
    const fs::path avi = fs::path(tmp) / (L"wwe12_bink2_" + std::to_wstring(GetTickCount()) + L".avi");
    if (progress)
      *progress = 2;
    const int code = RunOnOwnDesktop(Quote(tool.exe) + L" BinkConv " + Quote(fs::absolute(in)) + L" " + Quote(avi),
                                     120000 + DWORD(frames) * 200, err);
    std::error_code ec;
    if (fs::exists(avi, ec) && fs::file_size(avi, ec) > 0) {
      if (progress)
        *progress = 10;
      ok = ReadAvi(avi, put, err) && done > 0;
    } else if (err && err->empty()) {
      *err = L"RAD Video Tools couldn't decode it (code " + std::to_wstring(code) + L")";
    }
    fs::remove(avi, ec);
  } else {
    // raw BGRA frames on the pipe
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE rd = nullptr, wr = nullptr;
    if (CreatePipe(&rd, &wr, &sa, 1 << 20)) {
      SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
      STARTUPINFOW si{sizeof(si)};
      si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
      si.wShowWindow = SW_HIDE;
      si.hStdOutput = wr;
      si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
      PROCESS_INFORMATION pi{};
      std::wstring line = Quote(tool.exe) + L" -v error -nostdin -i " + Quote(fs::absolute(in)) + L" -vf scale=" +
                          std::to_wstring(W) + L":" + std::to_wstring(H) + L" -f rawvideo -pix_fmt bgra -";
      const BOOL started = CreateProcessW(nullptr, line.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                                          nullptr, &si, &pi);
      CloseHandle(wr);
      if (started) {
        std::vector<uint8_t> frame(size_t(W) * H * 4);
        size_t have = 0;
        DWORD n = 0;
        bool go = true;
        while (go && ReadFile(rd, frame.data() + have, DWORD(frame.size() - have), &n, nullptr) && n) {
          have += n;
          if (have == frame.size()) {
            go = put(frame.data(), W, H);
            have = 0;
          }
        }
        if (WaitForSingleObject(pi.hProcess, 5000) == WAIT_TIMEOUT)
          TerminateProcess(pi.hProcess, 1);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        ok = done > 0;
        if (!ok && err)
          *err = L"Librempeg couldn't decode it";
      } else if (err) {
        *err = L"Librempeg didn't start";
      }
      CloseHandle(rd);
    }
  }
  // (fewer frames than the header said: the last one again)
  while (ok && done < frames && !(cancel && *cancel)) {
    if (bink_writer_repeat(bw) != 1)
      break;
    ++done;
  }
  if (ok && !(cancel && *cancel)) {
    ok = bink_writer_close(bw) && !ferror(f);
  } else {
    bink_writer_free(bw);
    ok = false;
  }
  fclose(f);
  if (!ok) {
    std::error_code ec;
    fs::remove(out, ec);
    if (cancel && *cancel && err)
      *err = L"stopped";
  }
  if (progress)
    *progress = 100;
  return ok;
}

// The tool the player chose (modmaker_bink2.txt next to the Mod Maker), and the one found (looked for once).
inline fs::path SettingFile() {
  wchar_t me[MAX_PATH];
  GetModuleFileNameW(nullptr, me, MAX_PATH);
  return fs::path(me).parent_path() / L"modmaker_bink2.txt";
}
inline Tool &Found(bool again = false) {
  static Tool tool;
  static bool looked = false;
  if (!looked || again) {
    looked = true;
    std::wstring chosen;
    std::wifstream f(SettingFile());
    std::getline(f, chosen);
    tool = Find(chosen);
  }
  return tool;
}

inline std::string Narrow(const std::wstring &w) {
  std::string s(size_t(WideCharToMultiByte(CP_UTF8, 0, w.c_str(), int(w.size()), nullptr, 0, nullptr, nullptr)), 0);
  WideCharToMultiByte(CP_UTF8, 0, w.c_str(), int(w.size()), s.data(), int(s.size()), nullptr, nullptr);
  return s;
}

// The panel (My game discs page): which decoder, choose one, where to get RAD Video Tools.
inline void DrawSetting(bool fr) {
  ImGui::SeparatorText(fr ? "Vidéos Bink 2 (WWE 2K15 à 2K17)" : "Bink 2 movies (WWE 2K15 to 2K17)");
  ImGui::PushTextWrapPos(0);
  ImGui::TextDisabled("%s", fr ? "Les vidéos d'entrée des jeux 2K sont en Bink 2. Pour les reprendre, le Mod Maker utilise un "
                                 "outil que vous installez vous-même : RAD Video Tools (gratuit, de RAD Game Tools) ou "
                                 "le ffmpeg de Librempeg. Rien de RAD n'est fourni avec le portage."
                               : "The 2K games' entrance movies are Bink 2. To bring them, the Mod Maker uses a tool you "
                                 "install yourself: RAD Video Tools (free, from RAD Game Tools) or Librempeg's ffmpeg. "
                                 "Nothing of RAD's ships with the port.");
  ImGui::PopTextWrapPos();
  const Tool &t = Found();
  if (t.kind == Kind::kNone)
    ImGui::TextColored(ImVec4(0.94f, 0.75f, 0.24f, 1.0f), "%s", fr ? "Aucun outil trouvé." : "No tool found.");
  else
    ImGui::TextColored(ImVec4(0.55f, 0.85f, 0.45f, 1.0f), "%s : %s",
                       t.kind == Kind::kRad ? "RAD Video Tools" : "Librempeg", Narrow(t.exe.wstring()).c_str());
  if (ImGui::Button(fr ? "Choisir l'outil..." : "Choose the tool...")) {
    wchar_t file[MAX_PATH] = {};
    OPENFILENAMEW of{sizeof(of)};
    of.hwndOwner = GetActiveWindow();
    of.lpstrFilter = L"radvideo64.exe, ffmpeg.exe\0*.exe\0";
    of.lpstrFile = file;
    of.nMaxFile = MAX_PATH;
    of.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&of)) {
      std::wofstream(SettingFile()) << file;
      Found(true);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button(fr ? "Chercher à nouveau" : "Look again"))
    Found(true);
  ImGui::SameLine();
  if (ImGui::Button(fr ? "Page de RAD Video Tools" : "RAD Video Tools page"))
    ShellExecuteW(nullptr, L"open", L"https://www.radgametools.com/bnkdown.htm", nullptr, nullptr, SW_SHOWNORMAL);
}

}  // namespace bink2
