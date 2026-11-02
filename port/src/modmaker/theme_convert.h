// Mod Maker: a WWE '13 entrance theme made playable by WWE '12. WWE '13's themes are Wwise Vorbis of a newer Wwise
// than WWE '12's: the same "fmt ", but WWE '12's decoder crashed on them (2026-11-01). They are decoded by vgmstream
// (vgmstream-cli, a tool the player has: Kaiko's Mod Maker Tools ship it, as his wwise.py uses it; nothing of it ships
// with the port) and written again as Wwise PCM (wwise::PcmStream), which WWE '12 played.
#pragma once

#include <windows.h>

#include <algorithm>
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
// RIFF XMA, ".bk2" a Bink video's sound): its 16-bit samples (interleaved), channels (1 or 2) and rate. False if it can't.
inline bool Decode(const fs::path &vgmstream, const Bytes &in_bytes, const std::wstring &ext, std::vector<int16_t> &samples,
                   int &channels, int &rate) {
  samples.clear();
  if (vgmstream.empty() || in_bytes.empty())
    return false;
  wchar_t tmp[MAX_PATH];
  GetTempPathW(MAX_PATH, tmp);
  const std::wstring stamp = std::to_wstring(GetCurrentProcessId()) + L"_" + std::to_wstring(GetTickCount());
  const fs::path in = fs::path(tmp) / (L"wwe12_theme_" + stamp + ext), out = fs::path(tmp) / (L"wwe12_theme_" + stamp + L".wav");
  std::ofstream(in, std::ios::binary).write(reinterpret_cast<const char *>(in_bytes.data()), std::streamsize(in_bytes.size()));
  std::wstring line = L"\"" + vgmstream.wstring() + L"\" -i -o \"" + out.wstring() + L"\" \"" + in.wstring() + L"\"";
  STARTUPINFOW si{sizeof(si)};
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE;
  PROCESS_INFORMATION pi{};
  if (CreateProcessW(nullptr, line.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
    if (WaitForSingleObject(pi.hProcess, 120000) == WAIT_TIMEOUT)
      TerminateProcess(pi.hProcess, 1);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    std::ifstream f(out, std::ios::binary);
    const Bytes wav((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    // RIFF WAVE, 16-bit PCM: its "fmt " and "data"
    uint16_t ch = 0, bits = 0;
    uint32_t hz = 0;
    size_t data = 0, size = 0;
    auto le32 = [&](size_t o) { return uint32_t(wav[o]) | uint32_t(wav[o + 1]) << 8 | uint32_t(wav[o + 2]) << 16 | uint32_t(wav[o + 3]) << 24; };
    auto le16 = [&](size_t o) { return uint16_t(wav[o] | wav[o + 1] << 8); };
    if (wav.size() >= 12 && !memcmp(wav.data(), "RIFF", 4))
      for (size_t q = 12; q + 8 <= wav.size();) {
        const uint32_t len = le32(q + 4);
        if (!memcmp(&wav[q], "fmt ", 4) && len >= 16)
          ch = le16(q + 10), hz = le32(q + 12), bits = le16(q + 22);
        if (!memcmp(&wav[q], "data", 4)) {
          data = q + 8, size = std::min<size_t>(len, wav.size() - q - 8);
          break;
        }
        q += 8 + len + (len & 1);
      }
    if (data && bits == 16 && ch >= 1 && ch <= 2 && hz) {
      samples.resize(size / 2 / ch * ch);
      memcpy(samples.data(), &wav[data], samples.size() * 2);
      channels = ch, rate = int(hz);
    } else if (data && bits == 16 && ch == 6 && hz) {
      // 5.1 (FL FR FC LFE BL BR: SvR 2007's themes, the music in the centre, LFE and rear) made stereo as a player does:
      // L = FL + 0.707 FC + 0.5 LFE + BL, R the same on the right
      const size_t n = size / 2 / 6;
      samples.resize(n * 2);
      const int16_t *in = reinterpret_cast<const int16_t *>(&wav[data]);
      for (size_t i = 0; i < n; ++i) {
        const int16_t *x = in + i * 6;
        const double mid = 0.707 * x[2] + 0.5 * x[3];
        samples[2 * i] = int16_t(std::clamp(x[0] + mid + x[4], -32768.0, 32767.0));
        samples[2 * i + 1] = int16_t(std::clamp(x[1] + mid + x[5], -32768.0, 32767.0));
      }
      channels = 2, rate = int(hz);
    }
  }
  std::error_code ec;
  fs::remove(in, ec);
  fs::remove(out, ec);
  return !samples.empty();
}

// A stream decoded by vgmstream and written as Wwise PCM. Empty if it can't.
inline Bytes ToPcm(const fs::path &vgmstream, const Bytes &wem, const std::wstring &ext = L".wem") {
  std::vector<int16_t> s;
  int channels = 0, rate = 0;
  if (!Decode(vgmstream, wem, ext, s, channels, rate))
    return {};
  return wwise::PcmStream(s.data(), s.size() / size_t(channels), channels, rate);
}

// The same with the sound of a video before it (from `from` to `seconds`, at the theme's rate and channels): a 2K15
// entrance whose video plays full screen first (Bray Wyatt's, 20 s of its own sound before his song, which the game's
// theme alone doesn't have, 2026-11-02). Empty if either can't be decoded.
inline Bytes ToPcmWithIntro(const fs::path &vgmstream, const Bytes &wem, const std::wstring &ext, const Bytes &video,
                            const std::wstring &video_ext, double seconds, double from = 0) {
  std::vector<int16_t> s, v;
  int channels = 0, rate = 0, vch = 0, vrate = 0;
  if (!Decode(vgmstream, wem, ext, s, channels, rate) || !Decode(vgmstream, video, video_ext, v, vch, vrate))
    return {};
  const size_t frames = size_t(std::max(0.0, seconds - from) * rate), vframes = v.size() / size_t(vch);
  std::vector<int16_t> out;
  out.reserve(frames * size_t(channels) + s.size());
  for (size_t i = 0; i < frames; ++i) {  // (linear resampling; a missing channel copied, two made one by their mean)
    const double at = (double(i) / rate + from) * vrate;
    const size_t a = size_t(at), b = std::min(a + 1, vframes ? vframes - 1 : 0);
    const double t = at - double(a);
    for (int c = 0; c < channels; ++c) {
      auto sample = [&](size_t k) -> double {
        if (k >= vframes)
          return 0.0;
        if (vch == channels)
          return v[k * size_t(vch) + size_t(c)];
        if (vch == 1)
          return v[k];
        return (double(v[k * 2]) + v[k * 2 + 1]) / 2;
      };
      out.push_back(int16_t(std::clamp(sample(a) * (1 - t) + sample(b) * t, -32768.0, 32767.0)));
    }
  }
  out.insert(out.end(), s.begin(), s.end());
  return wwise::PcmStream(out.data(), out.size() / size_t(channels), channels, rate);
}

}  // namespace theme_convert
