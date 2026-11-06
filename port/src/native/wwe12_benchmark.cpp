// WWE '12 PC: built-in benchmark (2026-11-06), the idea of Jarrod's WWE 2K14 recomp: comparable
// figures from players' PCs (small laptops, Steam Deck).
//
// wwe12_benchmark = true (tools/dev/lancer_benchmark.bat starts it on a fresh data folder, so the
// player's own save is never touched) follows the remote pad's states (wwe12_remote_pad.cpp): the
// player goes to an exhibition match with entrances, then the benchmark measures the entrances and
// wwe12_benchmark_match_s seconds of the match, hands off the pad. Pass 1 has cold caches (a fresh
// data folder: no pipelines.bin / declarations.bin of its own); the game then starts itself again
// with wwe12_benchmark_pass = 2 on the same folder (warm caches) and the player picks the same match. The summary goes to logs\benchmark_<date>_<time>.txt in the
// data folder: version, machine, settings, frames per second and frame times (average, 1 % worst,
// worst), "work" (CPU time of a frame) and "gpu wait", CPU time per frame (render thread and the
// whole game), pipelines compiled while drawing, loading times, and a verdict. Nothing is sent
// anywhere: the player pastes the file. No game data in it (no names, only the numbers above).

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <windows.h>
#if defined(_M_X64) || defined(__x86_64__)
#include <intrin.h>
#endif

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/runtime.h>

#include "core/logging.h"
#include "gpu/device.h"

REXCVAR_DEFINE_BOOL(wwe12_benchmark, false, "WWE12",
                    "Dev / opt-in: play the built-in benchmark (default exhibition match twice, then "
                    "a summary in logs\\benchmark_*.txt). Use benchmark.bat: it gives it a fresh data "
                    "folder.");
REXCVAR_DEFINE_INT32(wwe12_benchmark_pass, 1, "WWE12", "Benchmark pass (2 = warm caches, set by pass 1).")
    .range(1, 2);
REXCVAR_DEFINE_INT32(wwe12_benchmark_match_s, 30, "WWE12", "Seconds of match measured in each pass.")
    .range(5, 600);

REXCVAR_DECLARE(int32_t, svr_render_scale);
REXCVAR_DECLARE(bool, wwe12_60fps);
REXCVAR_DECLARE(bool, wwe12_widescreen);

const char *Wwe12RemoteState();  // wwe12_remote_pad.cpp
namespace bd::gpu {
extern std::atomic<u64> g_render_thread_pso_compiles;  // pso_recorder.cpp
}

namespace {

using Clock = std::chrono::steady_clock;

// One phase of a pass: entrances (state "scene") or the match (from the "match" mark to "end").
struct Phase {
  std::vector<float> intervals;
  double work = 0, gpu_wait = 0;
  double render_cpu_ms = 0, game_cpu_ms = 0;  // CPU time used during the phase
  u64 pso = 0;                                 // pipelines compiled while drawing
};

struct Pass {
  Phase entrance, match;
  double load_s = 0;       // seconds spent on loading screens
  double to_entrance_s = 0;  // from the first tap to the first entrance
  bool reached_match = false;
};

std::mutex g_mu;
std::atomic<bool> g_script_taken{false};
Pass g_pass;
Phase *g_phase = nullptr;  // where frames go now
bool g_in_match = false;
bool g_end_requested = false, g_finished = false;
Clock::time_point g_start, g_loading_since;
bool g_loading = false;
double g_render_cpu0 = 0, g_game_cpu0 = 0;
u64 g_pso0 = 0;

double ThreadCpuMs(bool whole_process) {
  FILETIME a, b, kernel, user;
  const BOOL ok = whole_process ? GetProcessTimes(GetCurrentProcess(), &a, &b, &kernel, &user)
                                : GetThreadTimes(GetCurrentThread(), &a, &b, &kernel, &user);
  if (!ok)
    return 0;
  const auto ticks = [](const FILETIME &t) { return (u64(t.dwHighDateTime) << 32) | t.dwLowDateTime; };
  return double(ticks(kernel) + ticks(user)) / 1e4;
}

// Called on the render thread with the phase lock held.
void EnterPhase(Phase *phase) {
  const double render = ThreadCpuMs(false), game = ThreadCpuMs(true);
  const u64 pso = bd::gpu::g_render_thread_pso_compiles.load(std::memory_order_relaxed);
  if (g_phase) {
    g_phase->render_cpu_ms += render - g_render_cpu0;
    g_phase->game_cpu_ms += game - g_game_cpu0;
    g_phase->pso += pso - g_pso0;
  }
  g_phase = phase;
  g_render_cpu0 = render, g_game_cpu0 = game, g_pso0 = pso;
}

std::filesystem::path LogsFolder() {
  std::filesystem::path root;
  if (auto *rt = rex::Runtime::instance())
    root = rt->user_data_root();
  return (root.empty() ? rex::filesystem::GetExecutableFolder() : root) / "logs";
}

std::string CpuName() {
#if defined(_M_X64) || defined(__x86_64__)
  int regs[12] = {};
  __cpuid(regs, 0x80000002);
  __cpuid(regs + 4, 0x80000003);
  __cpuid(regs + 8, 0x80000004);
  std::string name(reinterpret_cast<const char *>(regs), sizeof(regs));
  name.resize(std::strlen(name.c_str()));
  while (!name.empty() && name.front() == ' ')
    name.erase(name.begin());
  return name;
#else
  return "unknown";
#endif
}

// Wine / Proton (Steam Deck, Linux): ntdll exports wine_get_version.
std::string Platform() {
  if (HMODULE ntdll = GetModuleHandleA("ntdll.dll")) {
    using Fn = const char *(*)();
    if (auto fn = reinterpret_cast<Fn>(GetProcAddress(ntdll, "wine_get_version")))
      return std::string("Wine / Proton ") + fn();
  }
  return "Windows";
}

std::string PhaseLines(const char *title, const Phase &p, int target_fps) {
  std::ostringstream o;
  if (p.intervals.empty()) {
    o << "  " << title << ": not reached\n";
    return o.str();
  }
  std::vector<float> sorted = p.intervals;
  std::sort(sorted.begin(), sorted.end());
  const size_t n = sorted.size();
  double sum = 0;
  for (float v : sorted)
    sum += v;
  const double avg = sum / n, p99 = sorted[std::min(n - 1, size_t(n * 0.99))], worst = sorted.back();
  char line[512];
  std::snprintf(line, sizeof(line),
                "  %s: %.0f fps average (target %d) | frame %.1f ms average, %.1f ms 1%% worst, %.1f ms "
                "worst | work %.1f ms, gpu wait %.1f ms | CPU per frame: render thread %.1f ms, whole "
                "game %.1f ms | pipelines compiled while drawing: %llu\n",
                title, 1000.0 / avg, target_fps, avg, p99, worst, p.work / n, p.gpu_wait / n,
                p.render_cpu_ms / n, p.game_cpu_ms / n, static_cast<unsigned long long>(p.pso));
  o << line;
  return o.str();
}

std::string Verdict(const Phase &match) {
  if (match.intervals.empty())
    return "could not measure a match (the menus did not lead to one: see the log)";
  double sum = 0;
  for (float v : match.intervals)
    sum += v;
  const double avg = sum / match.intervals.size(), fps = 1000.0 / avg;
  const double work = match.work / match.intervals.size();
  char text[200];
  if (fps >= 57.0)
    std::snprintf(text, sizeof(text), "OK: 60 fps in matches");
  else if (work >= 0.85 * avg)
    std::snprintf(text, sizeof(text), "limited by the processor: %.0f fps in matches", fps);
  else
    std::snprintf(text, sizeof(text), "limited by the graphics card or the display: %.0f fps in matches", fps);
  return text;
}

std::string PassText(int pass, const Pass &p) {
  const int entrance_target = REXCVAR_GET(wwe12_60fps) ? 60 : 30;
  std::ostringstream o;
  char line[256];
  std::snprintf(line, sizeof(line), "Pass %d (%s caches): start to first entrance %.1f s (menus by hand), loading screens %.1f s\n",
                pass, pass == 1 ? "cold" : "warm", p.to_entrance_s, p.load_s);
  o << line << PhaseLines("Entrances", p.entrance, entrance_target) << PhaseLines("Match", p.match, 60);
  return o.str();
}

void FinishPass() {
  const int pass = REXCVAR_GET(wwe12_benchmark_pass);
  const std::filesystem::path logs = LogsFolder();
  std::error_code ec;
  std::filesystem::create_directories(logs, ec);
  const std::filesystem::path partial = logs / "benchmark_pass1.tmp";
  std::string text;
  {
    std::lock_guard lock(g_mu);
    text = PassText(pass, g_pass);
  }
  BD_INFO("[benchmark] pass {} done:\n{}", pass, text);
  if (pass == 1) {
    std::ofstream(partial, std::ios::trunc) << text;
    // Pass 2: the same command line on the same data folder (warm caches).
    std::wstring cmd = GetCommandLineW();
    cmd += L" --wwe12_benchmark_pass=2";
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
      CloseHandle(pi.hThread);
      CloseHandle(pi.hProcess);
    } else {
      BD_WARN("[benchmark] could not start pass 2 (error {})", GetLastError());
    }
  } else {
    std::string first;
    {
      std::ifstream in(partial);
      std::stringstream ss;
      ss << in.rdbuf();
      first = ss.str();
    }
    std::filesystem::remove(partial, ec);
    const std::time_t now = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &now);
    char stamp[32], when[64];
    std::strftime(stamp, sizeof(stamp), "%Y%m%d_%H%M", &tm);
    std::strftime(when, sizeof(when), "%Y-%m-%d %H:%M", &tm);
    const auto exe = rex::filesystem::GetExecutableFolder();
    std::ostringstream o;
    o << "WWE '12 PC benchmark - version " << WWE12_VERSION << " - " << when << "\n"
      << "Machine: " << CpuName() << ", " << std::thread::hardware_concurrency() << " threads | "
      << bd::gpu::Video::GetDeviceName() << " | " << bd::gpu::Video::BackendName() << " | " << Platform()
      << "\n"
      << "Output " << bd::gpu::Video::OutputWidth() << "x" << bd::gpu::Video::OutputHeight()
      << " | render scale "
      << (REXCVAR_GET(svr_render_scale) ? std::to_string(REXCVAR_GET(svr_render_scale)) : std::string("auto"))
      << " | entrances at 60 fps: "
      << (REXCVAR_GET(wwe12_60fps) ? "on" : "off")
      << " | wide screen fill: " << (REXCVAR_GET(wwe12_widescreen) ? "on" : "off") << "\n"
      << "Exhibition match with entrances, picked by the player, fresh data folder, "
      << REXCVAR_GET(wwe12_benchmark_match_s) << " s of match per pass.\n\n"
      << first << text << "\n"
      << "Verdict (pass 2): " << Verdict(g_pass.match) << "\n";
    const auto file = logs / (std::string("benchmark_") + stamp + ".txt");
    std::ofstream(file, std::ios::trunc) << o.str();
    BD_INFO("[benchmark] summary written to {}", rex::path_to_utf8(file));
    (void)exe;
  }
  rex::FlushLogging();
  TerminateProcess(GetCurrentProcess(), 0);
}

}  // namespace

bool Wwe12BenchmarkActive() { return REXCVAR_GET(wwe12_benchmark); }

// The steps of one pass, handed once to the remote pad.
std::string Wwe12BenchmarkTakeScript() {
  if (!REXCVAR_GET(wwe12_benchmark) || g_script_taken.exchange(true))
    return {};
  g_start = Clock::now();
  BD_INFO("[benchmark] pass {} starts", REXCVAR_GET(wwe12_benchmark_pass));
  // The player picks the match (pressing A by itself got lost in the first-start menus,
  // 2026-11-06): the benchmark measures from the first entrance.
  return "untilscene:900 markentrance untilmatch:600 markmatch wait" +
         std::to_string(REXCVAR_GET(wwe12_benchmark_match_s) * 1000) + " markend";
}

void Wwe12BenchmarkMark(const std::string &name) {
  if (!REXCVAR_GET(wwe12_benchmark))
    return;
  if (name == "ENTRANCE") {
    std::lock_guard lock(g_mu);
    g_pass.to_entrance_s = std::chrono::duration<double>(Clock::now() - g_start).count();
  } else if (name == "MATCH") {
    std::lock_guard lock(g_mu);
    g_in_match = true;
    g_pass.reached_match = true;
  } else if (name == "END") {
    // The render thread closes the match phase at its next frame (its own CPU time), then the
    // summary is written off that thread.
    std::lock_guard lock(g_mu);
    g_end_requested = true;
  }
}

// Each presented frame (present.cpp), on the render thread.
void Wwe12BenchmarkFrame(double interval_ms, double work_ms, double gpu_wait_ms) {
  if (!REXCVAR_GET(wwe12_benchmark))
    return;
  std::lock_guard lock(g_mu);
  if (g_finished)
    return;
  if (g_end_requested) {
    EnterPhase(nullptr);
    g_finished = true;
    std::thread(FinishPass).detach();
    return;
  }
  const std::string_view state = Wwe12RemoteState();
  const bool loading = state == "loading";
  if (loading != g_loading) {
    if (!loading)
      g_pass.load_s += std::chrono::duration<double>(Clock::now() - g_loading_since).count();
    g_loading_since = Clock::now();
    g_loading = loading;
  }
  Phase *want = g_in_match ? &g_pass.match : state == "scene" ? &g_pass.entrance : nullptr;
  if (want != g_phase)
    EnterPhase(want);
  if (!g_phase)
    return;
  g_phase->intervals.push_back(float(interval_ms));
  g_phase->work += work_ms;
  g_phase->gpu_wait += gpu_wait_ms;
}
