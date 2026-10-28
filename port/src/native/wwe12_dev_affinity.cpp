// Dev: run the game on some processors only, to stand in for a small PC (2 cores, 4 threads...).
// "start /affinity" doesn't do it: the SDK gives the process every processor back when its runtime
// starts (rex::thread::EnableAffinityConfiguration). So the mask is set again a few seconds after
// start, once that has run. Off by default; docs/tests_petit_pc.md. Windows only.

#if defined(_WIN32)

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <rex/cvar.h>

#include "core/logging.h"

REXCVAR_DEFINE_STRING(wwe12_dev_affinity, "", "WWE12",
                      "Dev: processors the game may use, as a hex mask set 3 s after start "
                      "(e.g. 30000 = two E-cores of an i7-14700HX). Empty = all.");

namespace {

struct DevAffinity {
  DevAffinity() {
    std::thread([] {
      std::this_thread::sleep_for(std::chrono::seconds(3));
      const std::string text = REXCVAR_GET(wwe12_dev_affinity);
      if (text.empty())
        return;
      const unsigned long long mask = std::strtoull(text.c_str(), nullptr, 16);
      if (!mask)
        return;
      if (SetProcessAffinityMask(GetCurrentProcess(), static_cast<DWORD_PTR>(mask)))
        BD_INFO("[dev] processor affinity set to 0x{:X}", mask);
      else
        BD_WARN("[dev] processor affinity 0x{:X} refused ({})", mask, GetLastError());
    }).detach();
  }
};
DevAffinity g_dev_affinity;

} // namespace

#endif
