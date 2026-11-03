// WWE '12 PC: probe of the game's own waits (wwe12_wait_probe, off by default), to find the ones that
// slow the loads down (like the console's read throttle, wwe12_load_turbo.cpp).
//
// The game sleeps through SleepEx (sub_821B07E8(ms, alertable); Sleep, sub_821AEB18, jumps to it)
// and waits with a time limit through WaitForSingleObjectEx (sub_821B04D0(handle, ms, alertable)).
// Both are hooked: each call's real time is added to its caller (the link register, the wrappers
// jump rather than call) and requested time; once a second the callers that spent 20 ms or more are
// logged ("[wait] ..."), the longest first. Waits without a limit (-1) are left out: they wait for
// work, not for time.

#include <algorithm>
#include <chrono>
#include <map>
#include <mutex>
#include <tuple>
#include <vector>

#include <fmt/format.h>
#include <rex/cvar.h>

#include "core/logging.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_wait_probe, false, "WWE12",
                    "Probe: log once a second which game functions sleep or wait with a time limit, and how long.");

REX_EXTERN(__imp__sub_821B07E8);
REX_EXTERN(__imp__sub_821B04D0);

namespace {

using Clock = std::chrono::steady_clock;

struct Key {
  char kind;  // 's' sleep, 'w' wait
  u32 caller, ms;
  bool operator<(const Key &o) const { return std::tie(kind, caller, ms) < std::tie(o.kind, o.caller, o.ms); }
};
struct Total {
  u32 calls = 0;
  double ms = 0;
};

std::mutex g_mu;
std::map<Key, Total> g_totals;
Clock::time_point g_last = Clock::now();

void Add(char kind, u32 caller, u32 ms, Clock::time_point start) {
  const auto now = Clock::now();
  std::vector<std::pair<Key, Total>> report;
  {
    std::lock_guard lock(g_mu);
    auto &t = g_totals[{kind, caller, ms}];
    ++t.calls;
    t.ms += std::chrono::duration<double, std::milli>(now - start).count();
    if (now - g_last < std::chrono::seconds(1))
      return;
    g_last = now;
    for (const auto &[k, v] : g_totals)
      if (v.ms >= 20)
        report.emplace_back(k, v);
    g_totals.clear();
  }
  std::sort(report.begin(), report.end(), [](const auto &a, const auto &b) { return a.second.ms > b.second.ms; });
  std::string line;
  for (const auto &[k, v] : report)
    line += fmt::format(" | {} {:08X} ({} ms) x{} = {:.0f} ms", k.kind == 's' ? "sleep" : "wait", k.caller, int(k.ms),
                        v.calls, v.ms);
  if (!line.empty())
    BD_INFO("[wait] last second:{}", line);
}

}  // namespace

extern "C" REX_FUNC(sub_821B07E8) {
  if (!REXCVAR_GET(wwe12_wait_probe) || ctx.r3.u32 == 0xFFFFFFFFu) {
    __imp__sub_821B07E8(ctx, base);
    return;
  }
  const u32 caller = u32(ctx.lr), ms = ctx.r3.u32;
  const auto start = Clock::now();
  __imp__sub_821B07E8(ctx, base);
  Add('s', caller, ms, start);
}

extern "C" REX_FUNC(sub_821B04D0) {
  const u32 ms = ctx.r4.u32;
  if (!REXCVAR_GET(wwe12_wait_probe) || ms == 0xFFFFFFFFu) {
    __imp__sub_821B04D0(ctx, base);
    return;
  }
  const u32 caller = u32(ctx.lr);
  const auto start = Clock::now();
  __imp__sub_821B04D0(ctx, base);
  Add('w', caller, ms, start);
}
