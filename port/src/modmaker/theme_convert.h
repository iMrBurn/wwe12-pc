// Mod Maker: a WWE '13 entrance theme made playable by WWE '12. WWE '13's themes are Wwise Vorbis of a newer Wwise
// than WWE '12's: the same "fmt ", but WWE '12's decoder crashed on them (2026-11-01). They are decoded by vgmstream
// (vgmstream-cli, a tool the player has: Kaiko's Mod Maker Tools ship it, as his wwise.py uses it; nothing of it ships
// with the port) and written again as WWE '12's own themes are (1.3: Wwise Vorbis 32 kHz stereo, wwise_vorbis.h, the
// whole theme), or as Wwise PCM (wwise::PcmStream, 100 s at most) if that can't be done.
#pragma once

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "wwise.h"
#include "wwise_vorbis.h"

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
      // (then raised to the other themes' level, its peak at 32000, at most x 4: Lashley's came out at -27 dBFS where
      // the others are at -18, unheard under the crowd, 2026-11-02)
      const size_t n = size / 2 / 6;
      std::vector<double> mix(n * 2);
      const int16_t *in = reinterpret_cast<const int16_t *>(&wav[data]);
      double peak = 1;
      for (size_t i = 0; i < n; ++i) {
        const int16_t *x = in + i * 6;
        const double mid = 0.707 * x[2] + 0.5 * x[3];
        mix[2 * i] = x[0] + mid + x[4];
        mix[2 * i + 1] = x[1] + mid + x[5];
        peak = std::max({peak, std::abs(mix[2 * i]), std::abs(mix[2 * i + 1])});
      }
      const double gain = std::clamp(32000.0 / peak, 1.0, 4.0);
      samples.resize(n * 2);
      for (size_t i = 0; i < mix.size(); ++i)
        samples[i] = int16_t(std::clamp(mix[i] * gain, -32768.0, 32767.0));
      channels = 2, rate = int(hz);
    }
  }
  std::error_code ec;
  fs::remove(in, ec);
  fs::remove(out, ec);
  return !samples.empty();
}

// WWE '12's themes are all 48 kHz; a 2K16 one at 32 kHz (Seth Rollins', Play_MUS_0369) froze the loading before
// his entrance (2026-11-04). Any other rate made 48 kHz (linear resampling).
inline void To48k(std::vector<int16_t> &s, int channels, int &rate) {
  if (rate == 48000 || rate <= 0 || channels <= 0 || s.empty())
    return;
  const size_t n = s.size() / size_t(channels), m = size_t(double(n) * 48000.0 / rate);
  std::vector<int16_t> out(m * size_t(channels));
  for (size_t i = 0; i < m; ++i) {
    const double at = double(i) * rate / 48000.0;
    const size_t a = std::min(size_t(at), n - 1), b = std::min(a + 1, n - 1);
    const double f = at - double(a);
    for (int c = 0; c < channels; ++c)
      out[i * size_t(channels) + size_t(c)] =
          int16_t(std::lround(s[a * size_t(channels) + size_t(c)] * (1 - f) + s[b * size_t(channels) + size_t(c)] * f));
  }
  s.swap(out);
  rate = 48000;
}

// At most 100 s (18.3 MiB of 48 kHz stereo PCM), its last 3 s faded out. Ported themes over about 20 MiB never
// started in an entrance (and a triple threat waited for one forever, black screen): 2K14 Goldberg's 20.57 MB played,
// Hollywood Hogan's 21.55 MB, Hulk Hogan's, Cesaro's, SvR 2011 MVP's (56 MB) didn't (players' reports and the user's
// test, 2026-11-07). The game's own themes are at most 3.2 MB (Vorbis); an entrance lasts well under 100 s.
inline void Cap(std::vector<int16_t> &s, int channels, int rate, double seconds = 100, double fade = 3) {
  if (channels <= 0 || rate <= 0)
    return;
  const size_t max = size_t(seconds * rate), n = s.size() / size_t(channels);
  if (n <= max)
    return;
  s.resize(max * size_t(channels));
  const size_t f = std::min(max, size_t(fade * rate));
  for (size_t i = 0; i < f; ++i) {
    const double g = double(f - i) / double(f);
    for (int c = 0; c < channels; ++c) {
      int16_t &v = s[(max - f + i) * size_t(channels) + size_t(c)];
      v = int16_t(std::lround(v * g));
    }
  }
}

// Stereo at 32 kHz (WWE '12's themes' Wwise Vorbis): mono copied to both sides, then a windowed-sinc resampling (16
// taps each side, Blackman window, cut a little under the lower Nyquist: no aliasing from 48 kHz).
inline std::vector<int16_t> To32kStereo(const std::vector<int16_t> &s, int channels, int rate) {
  const size_t n = channels > 0 ? s.size() / size_t(channels) : 0;
  std::vector<float> in(n * 2);
  for (size_t i = 0; i < n; ++i)
    for (int c = 0; c < 2; ++c)
      in[2 * i + size_t(c)] = s[i * size_t(channels) + size_t(channels == 1 ? 0 : c)];
  if (rate == wwise_vorbis::kRate || rate <= 0 || n == 0) {
    std::vector<int16_t> out(in.size());
    for (size_t i = 0; i < in.size(); ++i)
      out[i] = int16_t(in[i]);
    return out;
  }
  constexpr int kTaps = 16;
  constexpr double kPi = 3.14159265358979323846;
  const double step = double(rate) / wwise_vorbis::kRate, cut = std::min(1.0, 1.0 / step) * 0.95;
  const size_t m = size_t(double(n) / step);
  std::vector<int16_t> out(m * 2);
  for (size_t i = 0; i < m; ++i) {
    const double at = double(i) * step;
    const long base = long(std::floor(at));
    double l = 0, r = 0, wsum = 0;
    for (long k = base - kTaps + 1; k <= base + kTaps; ++k) {
      const double x = at - double(k), t = x / kTaps;
      if (std::abs(t) >= 1)
        continue;
      const double win = 0.42 + 0.5 * std::cos(kPi * t) + 0.08 * std::cos(2 * kPi * t);
      const double sinc = x == 0 ? 1.0 : std::sin(kPi * cut * x) / (kPi * cut * x);
      const double wgt = cut * sinc * win;
      wsum += wgt;
      if (k >= 0 && size_t(k) < n)
        l += wgt * in[2 * size_t(k)], r += wgt * in[2 * size_t(k) + 1];
    }
    if (wsum > 0)  // (gain 1 whatever the phase)
      l /= wsum, r /= wsum;
    out[2 * i] = int16_t(std::clamp(std::lround(l), -32768L, 32767L));
    out[2 * i + 1] = int16_t(std::clamp(std::lround(r), -32768L, 32767L));
  }
  return out;
}

// The samples as the mod's theme: Wwise Vorbis like the game's themes (up to 10 minutes), else Wwise PCM at 48 kHz cut
// at 100 s (theme sizes over ~20 MiB never started). Empty if neither.
inline Bytes Finish(std::vector<int16_t> &s, int channels, int rate) {
  if (channels <= 0 || rate <= 0 || s.empty())
    return {};
  {
    std::vector<int16_t> v = To32kStereo(s, channels, rate);
    const size_t max = size_t(600) * wwise_vorbis::kRate;
    if (v.size() / 2 > max)
      Cap(v, 2, wwise_vorbis::kRate, 600);
    Bytes wem = wwise_vorbis::Encode(v.data(), v.size() / 2);
    if (!wem.empty())
      return wem;
  }
  To48k(s, channels, rate);
  Cap(s, channels, rate);
  return wwise::PcmStream(s.data(), s.size() / size_t(channels), channels, rate);
}

// A stream decoded by vgmstream and written for the mod (Finish). Empty if it can't.
inline Bytes ToPcm(const fs::path &vgmstream, const Bytes &wem, const std::wstring &ext = L".wem") {
  std::vector<int16_t> s;
  int channels = 0, rate = 0;
  if (!Decode(vgmstream, wem, ext, s, channels, rate))
    return {};
  return Finish(s, channels, rate);
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
  return Finish(out, channels, rate);
}

}  // namespace theme_convert
