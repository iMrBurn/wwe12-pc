// CPU time of each thread, per frame, in the [perf] lines (2026-11-20, Rendu; docs/rendu_petit_pc.md on branch
// 2.0-natif). The stack sampler (SVR_SAMPLE_STACKS) shares the cores it measures: on a 2-core PC it ran every
// ~67 ms instead of 5 and mostly caught threads while they waited. GetThreadTimes has no such bias: the kernel
// counts every thread's own time. Called by present.cpp with the [perf] lines (every 10 s): one line with the
// busiest threads (CPU ms per frame, by thread name) and the total of the unnamed ones (driver, OS pools).
// Guest threads are named "XThread<id>_(F80000xx)" by the SDK (F800002C = the game's main thread,
// F800005C = the game's render thread with our renderer, F8000074 = the superstars' CPU skinning in the
// 2026-11-20 runs); SDK threads keep their own names (GPU_Commands, XMA_Decoder, Audio_Worker...).
// Windows only (players' logs come from PCs; Android has its own perf zones).

#include <algorithm>
#include <climits>
#include <string>
#include <unordered_map>
#include <vector>

#include <rex/cvar.h>

#include "core/logging.h"
#include "svr_hook.h"

// ---- Thread priorities on small PCs (2026-11-22, Rendu; 2.0 "petits PC", docs/rendu_petit_pc.md piste 3) ----
// On 2 cores, the game's main, render and skinning threads meet at the frame barrier sub_827DEA20 every frame:
// when the one that limits the frame is preempted by a less useful thread, the whole frame waits. The SDK even
// runs its "GPU Commands" thread (the null GPU reading the guest's PM4 ring, which our renderer does not need for
// its pictures) ABOVE normal priority (gpu_thread_above_normal_priority, for the Xenia-style GPU path).
// With wwe12_few_cores_priorities and at most wwe12_few_cores_max_threads usable logical processors (process
// affinity, read every 10 s since --wwe12_dev_affinity applies late): the 3 game threads go above normal and
// GPU Commands back to normal; the previous priorities come back if the PC has more threads again. No affinity is
// forced (the 0xF affinity trap of the turbo). Applied when each game thread starts and again every 10 s with the
// [perf] lines (the game or the SDK may set a priority later).
REXCVAR_DEFINE_BOOL(wwe12_few_cores_priorities, true, "WWE12",
                    "On PCs with few CPU threads (wwe12_few_cores_max_threads), run the game's main, render and "
                    "skinning threads above normal priority and the null GPU thread at normal.");
REXCVAR_DEFINE_INT32(wwe12_few_cores_max_threads, 4, "WWE12",
                     "Most usable logical processors for wwe12_few_cores_priorities.")
    .range(1, 256);

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>

#include <mutex>

// Guest threads all start in sub_821B3F58 (r3 = the thread's function, r4 = its argument; it calls r3, then
// ExitThread). Recording the function per host thread names each thread by what it runs, which stays the same from
// one launch to the next (the XThread numbers do not).
namespace {
std::mutex g_roots_mutex;
std::unordered_map<DWORD, u32> g_roots;  // host tid -> guest thread function

const char *RootLabel(u32 fn) {
  switch (fn) {
    case 0x821AD2B8: return "main";
    case 0x82826A38: return "render";
    case 0x82827698: return "skinning";
    default: return nullptr;
  }
}
}  // namespace

namespace {
u32 UsableThreadCount() {
  DWORD_PTR process = 0, system = 0;
  if (GetProcessAffinityMask(GetCurrentProcess(), &process, &system) && process) {
    u32 n = 0;
    for (DWORD_PTR m = process; m; m &= m - 1)
      ++n;
    return n;
  }
  SYSTEM_INFO si{};
  GetSystemInfo(&si);
  return si.dwNumberOfProcessors ? si.dwNumberOfProcessors : 1;
}

bool FewCores() {
  return REXCVAR_GET(wwe12_few_cores_priorities) &&
         UsableThreadCount() <= u32(REXCVAR_GET(wwe12_few_cores_max_threads));
}

std::unordered_map<DWORD, int> g_saved_priority;  // host tid -> priority before ours (under g_roots_mutex)

// Sets `wanted` on the thread (first saving its priority), or restores the saved one when wanted is INT_MIN.
// Returns 1 if the priority was changed, 0 otherwise. g_roots_mutex must be held.
int SetPriorityLocked(DWORD tid, HANDLE h, int wanted) {
  const int now = GetThreadPriority(h);
  if (now == THREAD_PRIORITY_ERROR_RETURN)
    return 0;
  if (wanted == INT_MIN) {
    const auto it = g_saved_priority.find(tid);
    if (it == g_saved_priority.end())
      return 0;
    const int old = it->second;
    g_saved_priority.erase(it);
    return now != old && SetThreadPriority(h, old) ? 1 : 0;
  }
  if (now == wanted)
    return 0;
  g_saved_priority.try_emplace(tid, now);
  return SetThreadPriority(h, wanted) ? 1 : 0;
}

// Every 10 s (and once at the first report): the whole process's threads.
void ApplyFewCoresPriorities() {
  const bool few = FewCores();
  static int s_logged = -1;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snap == INVALID_HANDLE_VALUE)
    return;
  const DWORD pid = GetCurrentProcessId();
  int changed = 0, raised = 0, gpu = 0;
  THREADENTRY32 te{};
  te.dwSize = sizeof(te);
  std::lock_guard lock(g_roots_mutex);
  for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
    if (te.th32OwnerProcessID != pid)
      continue;
    const DWORD tid = te.th32ThreadID;
    const auto root = g_roots.find(tid);
    const bool game = root != g_roots.end() && RootLabel(root->second);
    if (!game && !few && g_saved_priority.find(tid) == g_saved_priority.end())
      continue;  // nothing to set or restore
    HANDLE h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION | THREAD_SET_LIMITED_INFORMATION, FALSE, tid);
    if (!h)
      continue;
    bool gpu_thread = false;
    if (!game) {
      PWSTR wname = nullptr;
      if (SUCCEEDED(GetThreadDescription(h, &wname)) && wname) {
        gpu_thread = wcscmp(wname, L"GPU Commands") == 0;
        LocalFree(wname);
      }
    }
    if (game || gpu_thread) {
      const int wanted = !few ? INT_MIN : game ? THREAD_PRIORITY_ABOVE_NORMAL : THREAD_PRIORITY_NORMAL;
      changed += SetPriorityLocked(tid, h, wanted);
      if (few)
        (game ? raised : gpu) += 1;
    }
    CloseHandle(h);
  }
  CloseHandle(snap);
  if (s_logged != (few ? 1 : 0)) {
    s_logged = few ? 1 : 0;
    if (few)
      BD_INFO("[prio] {} usable logical processors: game main / render / skinning threads above normal ({}), "
              "GPU Commands at normal ({})",
              UsableThreadCount(), raised, gpu);
    else if (changed)
      BD_INFO("[prio] {} usable logical processors: usual thread priorities back ({} threads)",
              UsableThreadCount(), changed);
  } else if (few && changed) {
    BD_INFO("[prio] {} thread priorities had been changed again: set back", changed);
  }
}
}  // namespace

REX_EXTERN(__imp__sub_821B3F58);
extern "C" REX_FUNC(sub_821B3F58) {
  {
    std::lock_guard lock(g_roots_mutex);
    g_roots[GetCurrentThreadId()] = ctx.r3.u32;
    if (RootLabel(ctx.r3.u32) && FewCores()) {
      HANDLE h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION | THREAD_SET_LIMITED_INFORMATION, FALSE,
                            GetCurrentThreadId());
      if (h) {
        SetPriorityLocked(GetCurrentThreadId(), h, THREAD_PRIORITY_ABOVE_NORMAL);
        CloseHandle(h);
      }
    }
  }
  __imp__sub_821B3F58(ctx, base);
}

void Wwe12ThreadCpuReport(double frames) {
  static std::unordered_map<DWORD, unsigned long long> s_last;  // tid -> kernel + user, 100 ns
  ApplyFewCoresPriorities();
  if (frames <= 0)
    return;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snap == INVALID_HANDLE_VALUE)
    return;
  const DWORD pid = GetCurrentProcessId();
  struct Row {
    std::string name;
    double ms;
  };
  std::vector<Row> rows;
  std::unordered_map<DWORD, unsigned long long> now_times;
  double unnamed_ms = 0, total_ms = 0;
  int unnamed = 0;
  THREADENTRY32 te{};
  te.dwSize = sizeof(te);
  for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
    if (te.th32OwnerProcessID != pid)
      continue;
    HANDLE h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID);
    if (!h)
      continue;
    FILETIME c, e, k, u;
    if (GetThreadTimes(h, &c, &e, &k, &u)) {
      const unsigned long long t = ((unsigned long long)(k.dwHighDateTime) << 32 | k.dwLowDateTime) +
                                   ((unsigned long long)(u.dwHighDateTime) << 32 | u.dwLowDateTime);
      now_times[te.th32ThreadID] = t;
      const auto it = s_last.find(te.th32ThreadID);
      const unsigned long long before = it != s_last.end() ? it->second : 0;
      if (it != s_last.end() && t > before) {
        const double ms = double(t - before) / 1e4 / frames;
        total_ms += ms;
        std::string name;
        PWSTR wname = nullptr;
        if (SUCCEEDED(GetThreadDescription(h, &wname)) && wname) {
          char buf[64] = "";
          if (*wname)
            WideCharToMultiByte(CP_UTF8, 0, wname, -1, buf, sizeof(buf), nullptr, nullptr);
          LocalFree(wname);
          name = buf;
        }
        {
          std::lock_guard lock(g_roots_mutex);
          const auto root = g_roots.find(te.th32ThreadID);
          if (root != g_roots.end()) {
            char label[48];
            const char *known = RootLabel(root->second);
            if (known)
              snprintf(label, sizeof(label), "%s(%08X)", known, root->second);
            else
              snprintf(label, sizeof(label), "game %08X", root->second);
            name = label;
          }
        }
        if (name.empty()) {
          unnamed_ms += ms;
          ++unnamed;
        } else {
          rows.push_back({name, ms});
        }
      }
    }
    CloseHandle(h);
  }
  CloseHandle(snap);
  const bool first = s_last.empty();
  s_last.swap(now_times);
  if (first)
    return;  // the first call only takes the starting times
  std::sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) { return a.ms > b.ms; });
  std::string line;
  for (size_t i = 0; i < rows.size() && i < 15; ++i) {
    if (rows[i].ms < 0.05)
      break;
    char part[128];
    snprintf(part, sizeof(part), "%s%s %.1f", i ? " | " : "", rows[i].name.c_str(), rows[i].ms);
    line += part;
  }
  BD_INFO("[perf] cpu per frame by thread (ms): {} | unnamed ({}) {:.1f} | all threads {:.1f}", line, unnamed,
          unnamed_ms, total_ms);
}

#else

void Wwe12ThreadCpuReport(double) {}

#endif
