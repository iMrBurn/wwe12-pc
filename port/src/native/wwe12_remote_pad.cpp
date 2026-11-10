// Dev: a remote control for player 1's pad, so tests can drive the menus without anyone at the PC
// (reproducing the 4-thread turbo loading hang over many loads).
//
// With wwe12_remote_pad on, remote_pad.txt next to the exe is read every 100 ms, then deleted. Each
// word is a button (A B X Y START BACK UP DOWN LEFT RIGHT LB RB), pressed 120 ms then released
// 120 ms, in order, or "wait<ms>" (wait2000). The buttons are ORed into player 1's
// XInputGetState in wwe12_intros.cpp, as for the skipped start-up videos.
// "scan<hex>" (scan7c92b392) logs every guest address where those bytes are ("[remote] scan"), to find which copies
// of a record the game keeps (an added superstar's moves, 2026-10-28).
//
// Scripts that run without anyone at the PC (2026-11-05). The game writes what it is doing to
// remote_state.txt next to the exe at each frame change of state ("state=match since_ms=... loads=..
// scenes=.. frame=.."), and these words wait for it or act:
//   until<state>[:<s>]  wait until the state is menu / loading / scene / match (timeout in s, 120 by
//                       default; on a timeout the rest of the script still runs, "[remote] timeout")
//   shot<name>          the frame the game presents next, read back from the GPU (not the desktop,
//                       black when the screen is off), saved as logs\shots\<name>.bmp
//   mark<name>          a "[remote] mark <name>" log line (tools/dev/autotest.py cuts [perf] there)
//   quit                ends the game (after the log is flushed)
// States, from the game's own switches: loading = a loading screen is up (wwe12_load_turbo.cpp);
// scene = the 30 fps mode of entrances and cutscenes (wwe12_fps.cpp); match = neither, once a scene
// has played, for 3 s after a loading screen that followed it (the end of the entrances) or 10 s
// otherwise (the gap between two entrances lasts a few seconds); it stays "match" in the menus
// after a match: scripts wait for it once;
// menu = before any scene.

#include <algorithm>
#include <cstdlib>
#include <atomic>
#include <cctype>
#include <cstring>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <windows.h>

#include <rex/cvar.h>
#include <rex/filesystem.h>

#include <rex/logging.h>
#include <rex/runtime.h>

#include <plume_render_interface.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "gpu/device.h"
#include "gpu/resources.h"

bool Wwe12LoadingScreenUp();  // wwe12_load_turbo.cpp
bool Wwe12SceneMode();        // wwe12_fps.cpp
// wwe12_benchmark.cpp: the built-in benchmark drives the same steps.
bool Wwe12BenchmarkActive();
std::string Wwe12BenchmarkTakeScript();
void Wwe12BenchmarkMark(const std::string &name);

REXCVAR_DEFINE_BOOL(wwe12_remote_pad, false, "WWE12",
                    "Dev: press player 1's buttons listed in remote_pad.txt next to the exe.");

namespace {

enum class StepKind { kPad, kUntil, kTapUntil, kShot, kMark, kQuit };

struct Step {
  u16 buttons;  // 0 = a pause
  long long ms;
  StepKind kind = StepKind::kPad;
  std::string arg;  // until: the state; shot / mark: the name
};

std::mutex g_mu;
std::deque<Step> g_steps;
long long g_step_end = 0;  // steady ms when the current step (front) ends
long long g_next_poll = 0;

long long NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// The game's state as the scripts see it (updated at each presented frame).
std::atomic<const char *> g_state{"menu"};
long long g_state_since = 0;
long long g_quiet_since = 0;  // neither loading nor scene since then (0 = not quiet)
bool g_saw_scene = false;
bool g_loaded_after_scene = false;  // a loading screen came after the last scene
u32 g_loads = 0, g_scenes = 0;
u64 g_frames = 0;
std::mutex g_shot_mu;
std::string g_shot_name;  // the next presented frame is saved under this name

void WriteStateFile() {
  const std::filesystem::path file = rex::filesystem::GetExecutableFolder() / "remote_state.txt";
  std::ofstream out(file, std::ios::trunc);
  out << "state=" << g_state.load() << " since_ms=" << (NowMs() - g_state_since)
      << " loads=" << g_loads << " scenes=" << g_scenes << " frame=" << g_frames << "\n";
}

// The guest address space walked by its host regions (VirtualQuery), committed readable ones only, all the patterns
// in one pass, on a thread of its own: page by page on the pad's thread it froze the game (2026-10-28).
void Scan(std::vector<std::string> hexes) {
  if (hexes.empty() || !bd::mem::ready())
    return;
  std::thread([hexes = std::move(hexes)] {
    std::vector<std::vector<uint8_t>> pats;
    for (const auto &hex : hexes) {
      std::vector<uint8_t> pat;
      for (size_t i = 0; i + 1 < hex.size(); i += 2)
        pat.push_back(uint8_t(std::stoul(hex.substr(i, 2), nullptr, 16)));
      pats.push_back(std::move(pat));
    }
    const uint8_t *host0 = bd::mem::at<uint8_t>(0x40000000);
    if (!host0)
      return;
    const uint8_t *base = host0 - 0x40000000;
    std::vector<int> found(pats.size(), 0);
    for (const uint8_t *h = host0; h < base + 0x100000000ull;) {
      MEMORY_BASIC_INFORMATION mbi{};
      if (VirtualQuery(h, &mbi, sizeof(mbi)) != sizeof(mbi))
        break;
      const uint8_t *start = static_cast<const uint8_t *>(mbi.BaseAddress);
      const uint8_t *end = std::min(start + mbi.RegionSize, base + 0x100000000ull);
      if (mbi.State == MEM_COMMIT && mbi.Protect && !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)))
        for (size_t k = 0; k < pats.size(); ++k)
          for (const uint8_t *q = std::max(start, h); found[k] < 64;) {
            q = std::search(q, end, pats[k].begin(), pats[k].end());
            if (q == end)
              break;
            BD_INFO("[remote] scan {} at {:08X}", hexes[k], u32(q - base));
            ++found[k];
            ++q;
          }
      h = end;
    }
    for (size_t k = 0; k < pats.size(); ++k)
      BD_INFO("[remote] scan {}: {} place(s)", hexes[k], found[k]);
  }).detach();
}

u16 ButtonBit(const std::string &w) {
  static const struct {
    const char *name;
    u16 bit;
  } kButtons[] = {{"UP", 0x0001},    {"DOWN", 0x0002}, {"LEFT", 0x0004}, {"RIGHT", 0x0008},
                  {"START", 0x0010}, {"BACK", 0x0020}, {"LB", 0x0100},   {"RB", 0x0200},
                  {"A", 0x1000},     {"B", 0x2000},    {"X", 0x4000},    {"Y", 0x8000}};
  for (const auto &b : kButtons)
    if (w == b.name)
      return b.bit;
  return 0;
}

void Queue(const std::string &text);

void Poll(long long now) {
  g_next_poll = now + 100;
  const std::filesystem::path file = rex::filesystem::GetExecutableFolder() / "remote_pad.txt";
  std::error_code ec;
  if (!std::filesystem::exists(file, ec))
    return;
  std::string text;
  {
    std::ifstream in(file);
    std::stringstream ss;
    ss << in.rdbuf();
    text = ss.str();
  }
  std::filesystem::remove(file, ec);
  Queue(text);
}

void Queue(const std::string &text) {
  std::istringstream words(text);
  std::string w;
  u32 n = 0;
  std::vector<std::string> scans;
  while (words >> w) {
    for (auto &c : w)
      c = char(std::toupper(static_cast<unsigned char>(c)));
    if (w.rfind("SCAN", 0) == 0) {
      scans.push_back(w.substr(4));
      continue;
    }
    if (w.rfind("WAIT", 0) == 0) {
      g_steps.push_back({0, std::atoll(w.c_str() + 4)});
    } else if (w.rfind("TAP", 0) == 0 && w.find("UNTIL") != std::string::npos) {
      // tap<button>until<state>[:<s>]: the button every 2.5 s until the state (menus of any depth)
      const size_t until = w.find("UNTIL");
      const u16 bit = ButtonBit(w.substr(3, until - 3));
      std::string state = w.substr(until + 5);
      long long seconds = 120;
      if (const size_t colon = state.find(':'); colon != std::string::npos) {
        seconds = std::max(1ll, std::atoll(state.c_str() + colon + 1));
        state.resize(colon);
      }
      for (auto &c : state)
        c = char(std::tolower(static_cast<unsigned char>(c)));
      if (!bit) {
        BD_WARN("[remote] unknown word '{}'", w);
        continue;
      }
      g_steps.push_back({bit, seconds * 1000, StepKind::kTapUntil, state});
    } else if (w.rfind("UNTIL", 0) == 0) {
      std::string state = w.substr(5);
      long long seconds = 120;
      if (const size_t colon = state.find(':'); colon != std::string::npos) {
        seconds = std::max(1ll, std::atoll(state.c_str() + colon + 1));
        state.resize(colon);
      }
      for (auto &c : state)
        c = char(std::tolower(static_cast<unsigned char>(c)));
      g_steps.push_back({0, seconds * 1000, StepKind::kUntil, state});
    } else if (w.rfind("SHOT", 0) == 0 || w.rfind("MARK", 0) == 0) {
      // (names come upper-cased, like every word)
      g_steps.push_back({0, 0, w[0] == 'S' ? StepKind::kShot : StepKind::kMark, w.substr(4)});
    } else if (w == "QUIT") {
      g_steps.push_back({0, 0, StepKind::kQuit, {}});
    } else if (const u16 bit = ButtonBit(w)) {
      g_steps.push_back({bit, 120});
      g_steps.push_back({0, 120});
    } else {
      BD_WARN("[remote] unknown word '{}'", w);
      continue;
    }
    ++n;
  }
  Scan(std::move(scans));
  BD_INFO("[remote] {} step(s) queued", n);
}

}  // namespace

// Called from player 1's XInputGetState hook: the buttons to hold down now.
u16 Wwe12RemotePadButtons() {
  if (!REXCVAR_GET(wwe12_remote_pad) && !Wwe12BenchmarkActive())
    return 0;
  std::lock_guard lock(g_mu);
  const long long now = NowMs();
  if (const std::string script = Wwe12BenchmarkTakeScript(); !script.empty())
    Queue(script);
  if (now >= g_next_poll)
    Poll(now);
  while (!g_steps.empty()) {
    Step &step = g_steps.front();
    if (g_step_end == 0)
      g_step_end = now + step.ms;
    switch (step.kind) {
    case StepKind::kPad:
      if (now < g_step_end)
        return step.buttons;
      break;
    case StepKind::kUntil:
      if (step.arg != g_state.load()) {
        if (now < g_step_end)
          return 0;
        BD_WARN("[remote] timeout waiting for state {} (state {})", step.arg, g_state.load());
      } else {
        BD_INFO("[remote] state {} reached", step.arg);
      }
      break;
    case StepKind::kTapUntil:
      if (step.arg != g_state.load()) {
        if (now >= g_step_end) {
          BD_WARN("[remote] timeout tapping until state {} (state {})", step.arg, g_state.load());
          break;
        }
        return (now - (g_step_end - step.ms)) % 2500 < 120 ? step.buttons : 0;
      }
      BD_INFO("[remote] state {} reached", step.arg);
      break;
    case StepKind::kShot: {
      std::lock_guard shot_lock(g_shot_mu);
      g_shot_name = step.arg.empty() ? "shot" : step.arg;
      break;
    }
    case StepKind::kMark:
      BD_INFO("[remote] mark {}", step.arg);
      Wwe12BenchmarkMark(step.arg);
      break;
    case StepKind::kQuit:
      BD_INFO("[remote] quit");
      rex::FlushLogging();
#if defined(_WIN32)
      TerminateProcess(GetCurrentProcess(), 0);
#else
      std::_Exit(0);
#endif
      break;
    }
    g_steps.pop_front();
    g_step_end = 0;
  }
  return 0;
}

// Each presented frame (present.cpp, before it takes the renderer's lock): the scripts' state,
// and a requested shot of the frame being presented.
void Wwe12RemoteOnPresent(bd::gpu::GuestTexture *front_buffer) {
  if (!REXCVAR_GET(wwe12_remote_pad) && !Wwe12BenchmarkActive())
    return;
  const long long now = NowMs();
  ++g_frames;
  const bool loading = Wwe12LoadingScreenUp(), scene = Wwe12SceneMode();
  const char *state;
  if (loading || scene) {
    g_quiet_since = 0;
    state = loading ? "loading" : "scene";
  } else {
    if (g_quiet_since == 0)
      g_quiet_since = now;
    if (!g_saw_scene)
      state = "menu";
    else if (now - g_quiet_since >= (g_loaded_after_scene ? 3000 : 10000))
      state = "match";  // (the short gaps between two entrances are not the match)
    else
      state = g_state.load();  // (3 s after a loading screen or a scene: still settling)
  }
  if (state != g_state.load()) {
    if (state == std::string_view("loading")) {
      ++g_loads;
      g_loaded_after_scene = g_saw_scene;
    }
    if (state == std::string_view("scene") && g_state.load() != std::string_view("scene")) {
      ++g_scenes;
      g_saw_scene = true;
      g_loaded_after_scene = false;
    }
    g_state.store(state);
    g_state_since = now;
    BD_INFO("[remote] state {} (loads {}, scenes {})", state, g_loads, g_scenes);
    WriteStateFile();
  }

  std::string name;
  {
    std::lock_guard lock(g_shot_mu);
    name.swap(g_shot_name);
  }
  if (name.empty() || !front_buffer)
    return;
  std::vector<u8> pixels;
  u32 row_pitch = 0;
  if (!bd::gpu::Video::ReadbackTextureLevel(front_buffer, 0, 0, pixels, row_pitch)) {
    BD_WARN("[remote] shot {}: the frame could not be read back", name);
    return;
  }
  const u32 w = front_buffer->width, h = front_buffer->height;
  const bool rgba = front_buffer->format != plume::RenderFormat::B8G8R8A8_UNORM;
  // 24-bit BMP, bottom-up rows of 3 bytes BGR padded to 4.
  const u32 bmp_row = (w * 3 + 3) & ~3u;
  std::vector<u8> bmp(54 + size_t(bmp_row) * h, 0);
  const auto put32 = [&bmp](size_t at, u32 v) { std::memcpy(&bmp[at], &v, 4); };
  bmp[0] = 'B', bmp[1] = 'M';
  put32(2, u32(bmp.size()));
  put32(10, 54);
  put32(14, 40);
  put32(18, w);
  put32(22, h);
  bmp[26] = 1, bmp[28] = 24;
  put32(34, u32(bmp.size() - 54));
  for (u32 y = 0; y < h; ++y) {
    const u8 *src = pixels.data() + size_t(y) * row_pitch;
    u8 *dst = bmp.data() + 54 + size_t(h - 1 - y) * bmp_row;
    for (u32 x = 0; x < w; ++x, src += 4, dst += 3) {
      dst[0] = rgba ? src[2] : src[0];
      dst[1] = src[1];
      dst[2] = rgba ? src[0] : src[2];
    }
  }
  std::filesystem::path dir;
  if (auto *rt = rex::Runtime::instance())
    dir = rt->user_data_root();
  dir = (dir.empty() ? rex::filesystem::GetExecutableFolder() : dir) / "logs" / "shots";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  std::ofstream(dir / (name + ".bmp"), std::ios::binary)
      .write(reinterpret_cast<const char *>(bmp.data()), std::streamsize(bmp.size()));
  BD_INFO("[remote] shot {} saved ({}x{}, {})", name, w, h, rex::path_to_utf8(dir / (name + ".bmp")));
}

// The scripts' state now (wwe12_benchmark.cpp).
const char *Wwe12RemoteState() { return g_state.load(); }

// A match is being loaded without an entrance before it (the benchmark's fixed match): the
// loading screen that ends it counts as the end of the entrances.
void Wwe12RemoteExpectMatch() {
  g_saw_scene = true;
  g_loaded_after_scene = false;
}
