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
#include <string>
#include <unordered_map>
#include <vector>

#include "core/logging.h"
#include "svr_hook.h"

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

REX_EXTERN(__imp__sub_821B3F58);
extern "C" REX_FUNC(sub_821B3F58) {
  {
    std::lock_guard lock(g_roots_mutex);
    g_roots[GetCurrentThreadId()] = ctx.r3.u32;
  }
  __imp__sub_821B3F58(ctx, base);
}

void Wwe12ThreadCpuReport(double frames) {
  static std::unordered_map<DWORD, unsigned long long> s_last;  // tid -> kernel + user, 100 ns
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
