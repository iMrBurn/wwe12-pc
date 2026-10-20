// Dev: a remote control for player 1's pad, so tests can drive the menus without anyone at the PC
// (reproducing the 4-thread turbo loading hang over many loads).
//
// With wwe12_remote_pad on, remote_pad.txt next to the exe is read every 100 ms, then deleted. Each
// word is a button (A B X Y START BACK UP DOWN LEFT RIGHT LB RB), pressed 120 ms then released
// 120 ms, in order, or "wait<ms>" (wait2000). The buttons are ORed into player 1's
// XInputGetState in wwe12_intros.cpp, as for the skipped start-up videos.

#include <atomic>
#include <cctype>
#include <chrono>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>

#include <rex/cvar.h>
#include <rex/filesystem.h>

#include "core/logging.h"

REXCVAR_DEFINE_BOOL(wwe12_remote_pad, false, "WWE12",
                    "Dev: press player 1's buttons listed in remote_pad.txt next to the exe.");

namespace {

struct Step {
  u16 buttons;  // 0 = a pause
  long long ms;
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
  std::istringstream words(text);
  std::string w;
  u32 n = 0;
  while (words >> w) {
    for (auto &c : w)
      c = char(std::toupper(static_cast<unsigned char>(c)));
    if (w.rfind("WAIT", 0) == 0) {
      g_steps.push_back({0, std::atoll(w.c_str() + 4)});
    } else if (const u16 bit = ButtonBit(w)) {
      g_steps.push_back({bit, 120});
      g_steps.push_back({0, 120});
    } else {
      BD_WARN("[remote] unknown word '{}'", w);
      continue;
    }
    ++n;
  }
  BD_INFO("[remote] {} step(s) queued", n);
}

}  // namespace

// Called from player 1's XInputGetState hook: the buttons to hold down now.
u16 Wwe12RemotePadButtons() {
  if (!REXCVAR_GET(wwe12_remote_pad))
    return 0;
  std::lock_guard lock(g_mu);
  const long long now = NowMs();
  if (now >= g_next_poll)
    Poll(now);
  while (!g_steps.empty()) {
    if (g_step_end == 0)
      g_step_end = now + g_steps.front().ms;
    if (now < g_step_end)
      return g_steps.front().buttons;
    g_steps.pop_front();
    g_step_end = 0;
  }
  return 0;
}
