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
#include "core/memory_helpers.h"
#include "svr_hook.h"
#include "gpu/device.h"

REXCVAR_DEFINE_BOOL(wwe12_benchmark, false, "WWE12",
                    "Dev / opt-in: play the built-in benchmark (default exhibition match twice, then "
                    "a summary in logs\\benchmark_*.txt). Use benchmark.bat: it gives it a fresh data "
                    "folder.");
REXCVAR_DEFINE_INT32(wwe12_benchmark_pass, 1, "WWE12", "Benchmark pass (2 = warm caches, set by pass 1).")
    .range(1, 2);
REXCVAR_DEFINE_INT32(wwe12_benchmark_match_s, 30, "WWE12", "Seconds of match measured in each pass.")
    .range(5, 600);
REXCVAR_DEFINE_INT32(wwe12_benchmark_passes, 2, "WWE12",
                     "Benchmark passes: 1, or 2 (the game starts itself again with warm caches).")
    .range(1, 2);
REXCVAR_DEFINE_INT32(wwe12_benchmark_weak_mask, 0, "WWE12",
                     "Benchmark 'weak' condition: keep the game on these cores (5 = two physical "
                     "cores) with one busy thread beside it, a small PC stand-in (0 = off).");
REXCVAR_DEFINE_STRING(wwe12_benchmark_scenario, "player_match", "WWE12",
                      "player_match: the player picks an exhibition match with entrances. hiac6: the "
                      "benchmark drives the menus itself (wwe12_benchmark_route) to an Armageddon Hell "
                      "in a Cell with 6 superstars, and checks it is that match.");
// 1.3: the fixed match goes through the menus like a player (Kaiko's way for his SvR 2011 PC port's
// test match: a fixed route, then a check; the direct call of the game's match start was dropped,
// 9-15 h and risky). On a fresh data folder nothing is remembered, so the same buttons always lead
// to the same match. The route is recorded once by hand (wwe12_benchmark_record_route) and set
// here; the remote pad's words (buttons, wait<ms>, until<state>). The check at the match start
// (sub_822B51A0) makes a wrong route "failed: wrong match" instead of a measure of something else.
// Recorded on 2026-11-10 (fresh data folder, version 1.3 in progress): the first-start prompts, PLAY, the
// Armageddon Hell in a Cell, 6 superstars and their attires, the default arena, entrances off.
// Its first START comes 8 s after the title screen shows (10 s more than the route's own 2 s): pressed
// 2 s after it, the game missed it (its title animation) and the whole route went astray (test of
// 2026-11-10; the player pressed it 13 s after). The first loading screen only comes after the
// first-start prompts, so "tap until loading" would press START into them.
REXCVAR_DEFINE_STRING(wwe12_benchmark_route,
                      "wait8000 START wait10000 A wait1500 LEFT wait1500 A wait5000 B wait1500 A wait1500 DOWN wait1500 DOWN wait1500 DOWN wait1500 DOWN wait1500 A wait1500 DOWN wait1500 A wait1533 DOWN wait1500 DOWN wait1500 DOWN wait1500 A wait1500 A wait1500 A wait1500 A wait1500 A wait1500 A wait1500 A wait1500 A wait1500 A wait1500 A wait1500 A wait1500 A wait1500 A wait4500 DOWN wait1500 A",
                      "WWE12",
                      "Benchmark hiac6: the remote pad's words from the title screen to the match "
                      "loading (recorded with wwe12_benchmark_record_route).");
// What the route must lead to (checked at the match start) and whether its match has entrances (measured
// first, as the WWE 2K14 recomp's benchmark does: entrances, then the match). Logged by the recorder.
REXCVAR_DEFINE_INT32(wwe12_benchmark_expect_type, 53, "WWE12",
                     "Benchmark fixed match: the match type the route must lead to (53 = Armageddon Hell in a Cell).");
REXCVAR_DEFINE_INT32(wwe12_benchmark_expect_superstars, 6, "WWE12",
                     "Benchmark fixed match: how many superstars the route's match has.");
REXCVAR_DEFINE_BOOL(wwe12_benchmark_entrances, false, "WWE12",
                    "Benchmark fixed match: the route's match has entrances (measured before the match).");
REXCVAR_DEFINE_BOOL(wwe12_benchmark_record_route, false, "WWE12",
                    "Dev: log player 1's buttons as a remote pad route ('[route]' lines), from the "
                    "title screen to the first match loading.");

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
std::string g_failure;                     // why no match was measured (fixed scenario)
std::atomic<bool> g_fixed_started{false};  // the fixed match's setup said it started the load
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

// The match phase in figures (the [results] block and the verdict).
struct MatchFigures {
  bool measured = false;
  double fps = 0, avg_ms = 0, median_ms = 0, p1_ms = 0, worst_ms = 0, work_ms = 0, gpu_wait_ms = 0;
  size_t over20 = 0, over51 = 0;  // frames over 20 ms, and over 51 ms (3 vblanks: a visible stutter)
  double cpu_render_ms = 0, cpu_game_ms = 0;
  u64 pso = 0;
};

MatchFigures Figures(const Phase &p) {
  MatchFigures f;
  if (p.intervals.empty())
    return f;
  std::vector<float> sorted = p.intervals;
  std::sort(sorted.begin(), sorted.end());
  const size_t n = sorted.size();
  double sum = 0;
  for (float v : sorted)
    sum += v;
  f.measured = true;
  f.avg_ms = sum / n;
  f.fps = 1000.0 / f.avg_ms;
  f.median_ms = sorted[n / 2];
  f.p1_ms = sorted[std::min(n - 1, size_t(n * 0.99))];
  for (float v : sorted) {
    f.over20 += v > 20.0f;
    f.over51 += v > 51.0f;
  }
  f.worst_ms = sorted.back();
  f.work_ms = p.work / n;
  f.gpu_wait_ms = p.gpu_wait / n;
  f.cpu_render_ms = p.render_cpu_ms / n;
  f.cpu_game_ms = p.game_cpu_ms / n;
  f.pso = p.pso;
  return f;
}

std::string PhaseLines(const char *title, const Phase &p, int target_fps) {
  const MatchFigures f = Figures(p);
  if (!f.measured)
    return std::string("  ") + title + ": not reached\n";
  char line[640];
  std::snprintf(line, sizeof(line),
                "  %s: %.0f fps average (target %d) | frame %.1f ms average, %.1f ms median, %.1f ms 1%% "
                "worst, %.1f ms worst, %zu over 20 ms, %zu over 51 ms | work %.1f ms, gpu wait %.1f ms | "
                "CPU per frame: render thread %.1f ms, whole game %.1f ms | pipelines compiled while "
                "drawing: %llu\n",
                title, f.fps, target_fps, f.avg_ms, f.median_ms, f.p1_ms, f.worst_ms, f.over20, f.over51,
                f.work_ms, f.gpu_wait_ms, f.cpu_render_ms, f.cpu_game_ms,
                static_cast<unsigned long long>(f.pso));
  return line;
}

// ok: 60 in matches; cpu: the frame's own work fills it (our [perf] "work" counts the game's and
// our CPU time); gpu: the rest (GPU or display); failed: no match measured.
const char *VerdictCode(const MatchFigures &f) {
  if (!f.measured)
    return "failed";
  if (f.fps >= 57.0)
    return "ok";
  return f.work_ms >= 0.85 * f.avg_ms ? "cpu" : "gpu";
}

std::string Verdict(const MatchFigures &f, const std::string &failure) {
  const std::string code = VerdictCode(f);
  char text[256];
  if (code == "failed")
    std::snprintf(text, sizeof(text), "could not measure a match (%s)",
                  failure.empty() ? "no match was reached: see the log" : failure.c_str());
  else if (code == "ok")
    std::snprintf(text, sizeof(text), "OK: 60 fps in matches");
  else if (code == "cpu")
    std::snprintf(text, sizeof(text), "limited by the processor: %.0f fps in matches", f.fps);
  else
    std::snprintf(text, sizeof(text), "limited by the graphics card or the display: %.0f fps in matches", f.fps);
  return text;
}

std::string PassText(int pass, const Pass &p) {
  const int entrance_target = REXCVAR_GET(wwe12_60fps) ? 60 : 30;
  std::ostringstream o;
  char line[256];
  const bool fixed = std::string(REXCVAR_GET(wwe12_benchmark_scenario)) != "player_match";
  std::snprintf(line, sizeof(line), "Pass %d (%s caches): start to %s %.1f s (%s), loading screens %.1f s\n",
                pass, pass == 1 ? "cold" : "warm", fixed ? "match start" : "first entrance", p.to_entrance_s,
                fixed ? "menus driven by the benchmark" : "menus by hand", p.load_s);
  o << line;
  if (!fixed || REXCVAR_GET(wwe12_benchmark_entrances))
    o << PhaseLines("Entrances", p.entrance, entrance_target);
  o << PhaseLines("Match", p.match, 60);
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
  bool failed;
  {
    std::lock_guard lock(g_mu);
    failed = !g_failure.empty();
  }
  if (pass < REXCVAR_GET(wwe12_benchmark_passes) && !failed) {
    std::ofstream(partial, std::ios::trunc) << text;
    // The fixed match's route was recorded on a fresh data folder: pass 1 made a save, which changes
    // the first menus. Everything but the caches and the logs goes, so pass 2 finds the same menus
    // with warm caches. Only in the benchmark's own folder (the launchers' Documents\wwe12_benchmark).
    if (std::string(REXCVAR_GET(wwe12_benchmark_scenario)) != "player_match") {
      const std::filesystem::path root = logs.parent_path();
      if (root.filename() == "wwe12_benchmark") {
        for (const auto &entry : std::filesystem::directory_iterator(root, ec)) {
          const auto name = entry.path().filename();
          if (name != "cache" && name != "logs")
            std::filesystem::remove_all(entry.path(), ec);
        }
        BD_INFO("[benchmark] pass 2 on the same menus: {} emptied but its caches", rex::path_to_utf8(root));
      } else {
        BD_WARN("[benchmark] data folder {} is not the benchmark's own: kept as is", rex::path_to_utf8(root));
      }
    }
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
    if (pass > 1) {
      std::ifstream in(partial);
      std::stringstream ss;
      ss << in.rdbuf();
      first = ss.str();
    }
    std::filesystem::remove(partial, ec);
    const std::string scenario = REXCVAR_GET(wwe12_benchmark_scenario);
    const bool fixed = scenario != "player_match";
    MatchFigures f;
    double load_s;
    std::string failure;
    {
      std::lock_guard lock(g_mu);
      f = Figures(g_pass.match);
      load_s = g_pass.load_s;
      failure = g_failure;
    }
    const std::string render_scale =
        REXCVAR_GET(svr_render_scale) ? std::to_string(REXCVAR_GET(svr_render_scale)) : std::string("auto");
    const char *frame_mode = REXCVAR_GET(wwe12_60fps) ? "60_entrances" : "original";
    char condition[32] = "normal";
    if (REXCVAR_GET(wwe12_benchmark_weak_mask))
      std::snprintf(condition, sizeof(condition), "weak_0x%X", unsigned(REXCVAR_GET(wwe12_benchmark_weak_mask)));
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
      << " | render scale " << render_scale << " | entrances at 60 fps: "
      << (REXCVAR_GET(wwe12_60fps) ? "on" : "off")
      << " | wide screen fill: " << (REXCVAR_GET(wwe12_widescreen) ? "on" : "off") << "\n"
      << (fixed ? "Fixed match (" + scenario + ")" : std::string("Exhibition match with entrances, picked by the player"))
      << ", fresh data folder, " << REXCVAR_GET(wwe12_benchmark_match_s) << " s of match per pass.\n\n"
      << first << text << "\n"
      << "Verdict: " << Verdict(f, failure) << "\n\n";
    // For the launcher's result panel: one key=value per line, nothing to parse in free text.
    char num[64];
    const auto fmt = [&num](double v) {
      std::snprintf(num, sizeof(num), "%.1f", v);
      return std::string(num);
    };
    o << "[results]\n"
      << "version=" << WWE12_VERSION << "\n"
      << "scenario=" << scenario << "\n"
      << "condition=" << condition << "\n"
      << "cpu=" << CpuName() << "\n"
      << "threads=" << std::thread::hardware_concurrency() << "\n"
      << "gpu=" << bd::gpu::Video::GetDeviceName() << "\n"
      << "api=" << bd::gpu::Video::BackendName() << "\n"
      << "platform=" << Platform() << "\n"
      << "output=" << bd::gpu::Video::OutputWidth() << "x" << bd::gpu::Video::OutputHeight() << "\n"
      << "render_scale=" << render_scale << "\n"
      << "frame_mode=" << frame_mode << "\n"
      << "match_fps=" << fmt(f.fps) << "\n"
      << "match_target_fps=60\n"
      << "match_median_ms=" << fmt(f.median_ms) << "\n"
      << "match_p1_ms=" << fmt(f.p1_ms) << "\n"
      << "match_frames_over_20ms=" << f.over20 << "\n"
      << "match_frames_over_51ms=" << f.over51 << "\n"
      << "match_worst_ms=" << fmt(f.worst_ms) << "\n"
      << "match_work_ms=" << fmt(f.work_ms) << "\n"
      << "match_gpu_wait_ms=" << fmt(f.gpu_wait_ms) << "\n"
      << "cpu_render_ms=" << fmt(f.cpu_render_ms) << "\n"
      << "cpu_game_ms=" << fmt(f.cpu_game_ms) << "\n"
      << "pipelines_while_drawing=" << f.pso << "\n"
      << "load_s=" << fmt(load_s) << "\n"
      << "verdict=" << VerdictCode(f) << "\n";
    const auto file = logs / (std::string("benchmark_") + stamp + ".txt");
    std::ofstream(file, std::ios::trunc) << o.str();
    // One line per run next to the exe (the data folder is fresh each time), to compare builds and
    // settings before / after a change, as Kaiko's runs\opt_results.md.
    {
      const auto history = rex::filesystem::GetExecutableFolder() / "benchmark_history.csv";
      const bool fresh = !std::filesystem::exists(history, ec);
      std::ofstream h(history, std::ios::app);
      if (fresh)
        h << "date,version,scenario,condition,api,output,render_scale,match_fps,median_ms,p1_ms,"
             "over_20ms,over_51ms,worst_ms,work_ms,gpu_wait_ms,cpu_render_ms,cpu_game_ms,"
             "pipelines,load_s,verdict\n";
      h << when << ',' << WWE12_VERSION << ',' << scenario << ',' << condition << ','
        << bd::gpu::Video::BackendName() << ',' << bd::gpu::Video::OutputWidth() << 'x'
        << bd::gpu::Video::OutputHeight() << ',' << render_scale << ',' << fmt(f.fps) << ','
        << fmt(f.median_ms) << ',' << fmt(f.p1_ms) << ',' << f.over20 << ',' << f.over51 << ','
        << fmt(f.worst_ms) << ',' << fmt(f.work_ms) << ',' << fmt(f.gpu_wait_ms) << ','
        << fmt(f.cpu_render_ms) << ',' << fmt(f.cpu_game_ms) << ',' << f.pso << ',' << fmt(load_s)
        << ',' << VerdictCode(f) << '\n';
    }
    BD_INFO("[benchmark] summary written to {}", rex::path_to_utf8(file));
    (void)exe;
  }
  rex::FlushLogging();
  TerminateProcess(GetCurrentProcess(), 0);
}

}  // namespace

bool Wwe12BenchmarkActive() { return REXCVAR_GET(wwe12_benchmark); }

void Wwe12BenchmarkFixedMatchFailed(const char *why);

// The steps of one pass, handed once to the remote pad.
bool Wwe12FastBootDone();  // wwe12_load_turbo.cpp: the title screen is shown

std::string Wwe12BenchmarkTakeScript() {
  // (after the start-up: the route starts on the title screen, as it was recorded)
  if (!REXCVAR_GET(wwe12_benchmark) || !Wwe12FastBootDone() || g_script_taken.exchange(true))
    return {};
  g_start = Clock::now();
  if (const u64 mask = u64(REXCVAR_GET(wwe12_benchmark_weak_mask))) {
    // "Weak" condition, Kaiko's opt_bench.ps1 idea (SvR 2011 PC port): the game kept on a few cores
    // (0x5 = two physical cores) with one busy thread of normal priority beside it, to stand in for
    // a small PC. The runtime gives every core back now and then, so the mask is set again every 3 s.
    std::thread([mask] {
      for (;;) {
        SetProcessAffinityMask(GetCurrentProcess(), DWORD_PTR(mask));
        std::this_thread::sleep_for(std::chrono::seconds(3));
      }
    }).detach();
    std::thread([mask] {
      SetThreadAffinityMask(GetCurrentThread(), DWORD_PTR(mask));
      volatile u64 spin = 0;
      for (;;)
        spin = spin + 1;
    }).detach();
    BD_INFO("[benchmark] weak condition: cores mask 0x{:X} and one busy thread", mask);
  }
  BD_INFO("[benchmark] pass {} starts", REXCVAR_GET(wwe12_benchmark_pass));
  const std::string measure =
      " markmatch wait" + std::to_string(REXCVAR_GET(wwe12_benchmark_match_s) * 1000) + " markend";
  if (std::string(REXCVAR_GET(wwe12_benchmark_scenario)) != "player_match") {
    // The fixed match: the recorded route through the menus, then entrances and match as usual.
    const std::string route = REXCVAR_GET(wwe12_benchmark_route);
    if (route.empty()) {
      Wwe12BenchmarkFixedMatchFailed("no route set (wwe12_benchmark_route)");
      return "wait1000";
    }
    BD_INFO("[benchmark] fixed match {}: route {}", std::string(REXCVAR_GET(wwe12_benchmark_scenario)),
            route);
    if (REXCVAR_GET(wwe12_benchmark_entrances))  // (as the player's match: entrances, then the match)
      return "wait2000 " + route + " untilscene:900 markentrance untilmatch:600" + measure;
    // (no entrances in the recorded match: the check at the match start tells the remote pad that the
    // next loading screen ends in the match; its own loading screen first: the remote pad says "match"
    // as soon as the check passed, and the first 4 s measured were that loading screen, 2026-11-10)
    return "wait2000 " + route + " untilloading:120 untilmatch:600" + measure;
  }
  // The player picks the match (pressing A by itself got lost in the first-start menus,
  // 2026-11-06): the benchmark measures from the first entrance.
  return "untilscene:900 markentrance untilmatch:600" + measure;
}

// For the fixed scenario's match setup (Gameplay): true once, when the benchmark wants its match.
bool Wwe12BenchmarkWantsFixedMatch() {
  return REXCVAR_GET(wwe12_benchmark) &&
         std::string(REXCVAR_GET(wwe12_benchmark_scenario)) != "player_match" &&
         !g_fixed_started.load(std::memory_order_relaxed);
}

void Wwe12RemoteExpectMatch();  // wwe12_remote_pad.cpp

// The setup asked the game to load the match: the next loading screen ends in it (no entrance
// needed for the "match" state), and the time to the match is measured from here.
void Wwe12BenchmarkFixedMatchStarted() {
  g_fixed_started.store(true, std::memory_order_relaxed);
  Wwe12RemoteExpectMatch();
  std::lock_guard lock(g_mu);
  g_pass.to_entrance_s = std::chrono::duration<double>(Clock::now() - g_start).count();
  BD_INFO("[benchmark] fixed match started by the game's match setup");
}

// The setup could not start it (the summary says why; verdict=failed).
void Wwe12BenchmarkFixedMatchFailed(const char *why) {
  g_fixed_started.store(true, std::memory_order_relaxed);
  {
    std::lock_guard lock(g_mu);
    g_failure = why ? why : "the fixed match could not be set up";
    g_end_requested = true;
  }
  BD_WARN("[benchmark] fixed match failed: {}", why ? why : "?");
}

void Wwe12BenchmarkMark(const std::string &name) {
  if (!REXCVAR_GET(wwe12_benchmark))
    return;
  if (name == "ENTRANCE") {
    std::lock_guard lock(g_mu);
    g_pass.to_entrance_s = std::chrono::duration<double>(Clock::now() - g_start).count();
  } else if (name == "MATCH") {
    std::lock_guard lock(g_mu);
    if (std::string_view(Wwe12RemoteState()) != "match") {
      // (the wait timed out: measuring the menus would give a false result)
      if (g_failure.empty())
        g_failure = std::string("no match was reached (state ") + Wwe12RemoteState() + ")";
      g_end_requested = true;
      return;
    }
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

// ---- The fixed match's check (1.3): the game's own match start, sub_822B51A0 (match record object
// +12456 = 2, then the CGameMainSequence task: docs/perf_natif_steam_deck.md, step 0). There the
// match record (0x8317C850: +0 the type) and the chosen superstars (48-byte places at the start of
// heap 0, superstar x 100 + attire at +16; probe of 2026-11-10) must be the scenario's.
std::atomic<bool> g_route_match_started{false};  // the route recorder's end: the game's match start

namespace {
// The match being started: its type and how many superstars sit in the chosen places.
u32 CountChosen(u8 &type, std::string &ids) {
  constexpr u32 kMatchRecord = 0x8317C850, kPlaces = 0xAEDDFEA4, kPlaceSize = 48;
  type = bd::mem::load<u8>(kMatchRecord);
  u32 superstars = 0;
  for (u32 i = 0; i < 6; ++i) {
    const u32 id = bd::mem::load<u32>(kPlaces + i * kPlaceSize + 16);
    ids += (i ? "," : "") + std::to_string(id);
    superstars += id >= 100 && id < 100000;
  }
  return superstars;
}
}  // namespace

REX_EXTERN(__imp__sub_822B51A0);
extern "C" REX_FUNC(sub_822B51A0) {
  if (REXCVAR_GET(wwe12_benchmark_record_route) && !g_route_match_started.exchange(true)) {
    u8 type = 0;
    std::string ids;
    const u32 superstars = CountChosen(type, ids);
    BD_INFO("[route] the game starts the match: --wwe12_benchmark_expect_type={} "
            "--wwe12_benchmark_expect_superstars={} ({})",
            type, superstars, ids);
  }
  if (REXCVAR_GET(wwe12_benchmark) && std::string(REXCVAR_GET(wwe12_benchmark_scenario)) != "player_match" &&
      !g_fixed_started.load(std::memory_order_relaxed)) {
    u8 type = 0;
    std::string ids;
    const u32 superstars = CountChosen(type, ids);
    const u32 want_type = u32(REXCVAR_GET(wwe12_benchmark_expect_type));
    const u32 want_superstars = u32(REXCVAR_GET(wwe12_benchmark_expect_superstars));
    BD_INFO("[benchmark] match start: type {} superstars {} ({})", type, superstars, ids);
    if (type == want_type && superstars == want_superstars) {
      if (REXCVAR_GET(wwe12_benchmark_entrances))
        g_fixed_started.store(true, std::memory_order_relaxed);  // (entrances follow, measured as usual)
      else
        Wwe12BenchmarkFixedMatchStarted();
    } else {
      static char why[160];
      std::snprintf(why, sizeof(why), "wrong match: type %u (want %u), %u superstars (want %u)", unsigned(type),
                    want_type, superstars, want_superstars);
      Wwe12BenchmarkFixedMatchFailed(why);
    }
  }
  __imp__sub_822B51A0(ctx, base);
}

// ---- Route recorder (dev): player 1's button presses (and the left stick as the d-pad) logged as
// remote pad words with the waits between them, from the title screen (first press) to the loading
// screen of the match (after the game's match start, sub_822B51A0: loading screens also come between
// menus, the first try of 2026-11-10 stopped at the main menu's). The whole route is logged again in one line at the end
// ("[route] full: ..."): paste it into wwe12_benchmark_route. Waits are kept at least 1.5 s
// (slower PCs), and each press is a remote pad press (120 ms).
void Wwe12BenchmarkRecordPad(u16 buttons, int16_t stick_x, int16_t stick_y, bool loading) {
  if (!REXCVAR_GET(wwe12_benchmark_record_route))
    return;
  static u16 s_previous = 0;
  static Clock::time_point s_last{};
  static std::string s_route;
  static bool s_done = false;
  if (s_done)
    return;
  constexpr int16_t kDeflect = 16000;
  u16 now_buttons = buttons;
  if (stick_x > kDeflect) now_buttons |= 0x0008;
  if (stick_x < -kDeflect) now_buttons |= 0x0004;
  if (stick_y > kDeflect) now_buttons |= 0x0001;
  if (stick_y < -kDeflect) now_buttons |= 0x0002;
  if (loading && !s_route.empty() && g_route_match_started.load()) {
    s_done = true;
    BD_INFO("[route] full: {}", s_route);
    return;
  }
  const u16 pressed = u16(now_buttons & ~s_previous);
  s_previous = now_buttons;
  if (!pressed)
    return;
  static const struct {
    const char *name;
    u16 bit;
  } kButtons[] = {{"UP", 0x0001},    {"DOWN", 0x0002}, {"LEFT", 0x0004}, {"RIGHT", 0x0008},
                  {"START", 0x0010}, {"BACK", 0x0020}, {"LB", 0x0100},   {"RB", 0x0200},
                  {"A", 0x1000},     {"B", 0x2000},    {"X", 0x4000},    {"Y", 0x8000}};
  const auto t = Clock::now();
  for (const auto &b : kButtons) {
    if (!(pressed & b.bit))
      continue;
    if (!s_route.empty()) {
      const long long gap = std::chrono::duration_cast<std::chrono::milliseconds>(t - s_last).count();
      s_route += " wait" + std::to_string(std::max(1500ll, gap)) + " ";
    }
    s_route += b.name;
    s_last = t;
    BD_INFO("[route] {} (state {})", b.name, Wwe12RemoteState());
  }
}
