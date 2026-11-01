// Mod Maker: a WWE '13 entrance theme made playable by WWE '12. WWE '13's themes are Wwise Vorbis of a newer Wwise
// than WWE '12's: the same "fmt ", but WWE '12's decoder crashed on them (2026-11-01). They are decoded by vgmstream
// (vgmstream-cli, a tool the player has: Kaiko's Mod Maker Tools ship it, as his wwise.py uses it; nothing of it ships
// with the port) and written again as Wwise PCM (wwise::PcmStream), which WWE '12 played.
#pragma once

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "wwise.h"

namespace theme_convert {

namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;

// vgmstream-cli: next to the Mod Maker (vgmstream\), in Kaiko's Mod Maker Tools beside his SvR 2011 (`svr2011`: that
// game's folder from My game discs), or on the PATH. Empty if none.
inline fs::path FindVgmstream(const fs::path &svr2011 = {}) {
  std::error_code ec;
  wchar_t me[MAX_PATH];
  GetModuleFileNameW(nullptr, me, MAX_PATH);
  std::vector<fs::path> cands = {fs::path(me).parent_path() / L"vgmstream" / L"vgmstream-cli.exe"};
  if (!svr2011.empty())
    for (fs::path p = svr2011; !p.empty() && p != p.parent_path(); p = p.parent_path())
      cands.push_back(p / L"Mod Maker Tools" / L"vgmstream" / L"vgmstream-cli.exe");
  for (const auto &c : cands)
    if (fs::is_regular_file(c, ec))
      return c;
  wchar_t found[MAX_PATH];
  if (SearchPathW(nullptr, L"vgmstream-cli.exe", nullptr, MAX_PATH, found, nullptr))
    return found;
  return {};
}

// A stream decoded by vgmstream (once through, no loop; `ext` tells it what the bytes are: ".wem" Wwise, ".xma" a
// RIFF XMA) and written as Wwise PCM. Empty if it can't.
inline Bytes ToPcm(const fs::path &vgmstream, const Bytes &wem, const std::wstring &ext = L".wem") {
  if (vgmstream.empty() || wem.empty())
    return {};
  wchar_t tmp[MAX_PATH];
  GetTempPathW(MAX_PATH, tmp);
  const std::wstring stamp = std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount());
  const fs::path in = fs::path(tmp) / (L"wwe12_theme_" + stamp + ext), out = fs::path(tmp) / (L"wwe12_theme_" + stamp + L".wav");
  std::ofstream(in, std::ios::binary).write(reinterpret_cast<const char *>(wem.data()), std::streamsize(wem.size()));
  std::wstring line = L"\"" + vgmstream.wstring() + L"\" -i -o \"" + out.wstring() + L"\" \"" + in.wstring() + L"\"";
  STARTUPINFOW si{sizeof(si)};
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  PROCESS_INFORMATION pi{};
  Bytes pcm;
  if (CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    if (WaitForSingleObject(pi.hProcess, 120000) == WAIT_TIMEOUT)
      TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    std::ifstream f(out, std::ios::binary);
    const Bytes wav((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    // RIFF WAVE, 16-bit PCM: its "fmt " and "data"
    uint16_t channels = 0, bits = 0;
    uint32_t rate = 0;
    size_t data = 0, size = 0;
    auto le32 = [&](size_t o) { return uint32_t(wav[o]) | uint32_t(wav[o + 1]) << 8 | uint32_t(wav[o + 2]) << 16 | uint32_t(wav[o + 3]) << 24; };
    auto le16 = [&](size_t o) { return uint16_t(wav[o] | wav[o + 1] << 8); };
    if (wav.size() >= 12 && !memcmp(wav.data(), "RIFF", 4))
      for (size_t q = 12; q + 8 <= wav.size();) {
        const uint32_t len = le32(q + 4);
        if (!memcmp(&wav[q], "fmt ", 4) && len >= 16)
          channels = le16(q + 10), rate = le32(q + 12), bits = le16(q + 22);
        if (!memcmp(&wav[q], "data", 4)) {
          data = q + 8, size = std::min<size_t>(len, wav.size() - q - 8);
          break;
        }
        q += 8 + len + (len & 1);
      }
    if (data && bits == 16 && channels >= 1 && channels <= 2 && rate)
      pcm = wwise::PcmStream(reinterpret_cast<const int16_t *>(&wav[data]), size / 2 / channels, channels, int(rate));
  }
  std::error_code ec;
  fs::remove(in, ec);
  fs::remove(out, ec);
  return pcm;
}

}  // namespace theme_convert
