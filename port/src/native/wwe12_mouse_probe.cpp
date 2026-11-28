// Dev probe (off by default): the mouse in the menus, stage 0 (docs/etude_souris_menus.md, 2026-11-28, Rendu).
//
// With wwe12_mouse_probe on, in the menus:
// - every second, which menu hooks ran (and on which objects): sub_824D3458 / sub_824D20A0 (the layout screens,
//   wwe12_universe.cpp), sub_8253F9A0 (menu.pac's menus, hooked here; sub_8253A028 can't be: the recompiled one is final), sub_8253C3F0 (a menu.pac
//   choice, wwe12_quit.cpp), sub_825A6A98 (the arena select, wwe12_arena_pages.cpp), with the menu.pac menu's
//   words +360 / +372 (the row index?) and its selected entry (sub_8253A028);
// - 0.4 s after each press of player 1's D-pad, A or B (wwe12_intros.cpp's XInputGetState hook): the same, plus the
//   layout of the moment (sub_8254E920(0): every element's name, x, y, scale, parent, hidden, colour, text) written
//   to logs\mouse_layout_NN.txt and the frame saved as logs\shots\mouse_NN.bmp (wwe12_remote_pad.cpp's shot),
//   to match each row's place with the picture.
// Lines "[mouse probe]" in the log. Nothing in the game changes.

#include <rex/cvar.h>
#include <rex/runtime.h>
#include <rex/system/kernel_state.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <string>

#include <fmt/format.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_mouse_probe, false, "WWE12",
                    "Dev probe: the menus' hooks, cursors and layouts for the mouse in the menus ([mouse probe], "
                    "logs\\mouse_layout_NN.txt).");

REX_EXTERN(__imp__sub_8253F9A0);
REX_EXTERN(__imp__sub_8254E920);

bool Wwe12LoadingScreenUp();               // wwe12_load_turbo.cpp
bool Wwe12SceneMode();                     // wwe12_fps.cpp
void Wwe12RemoteShot(const std::string &name);  // wwe12_remote_pad.cpp: the next presented frame saved

namespace {

long long NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

const char *const kKinds[] = {"?", "layout tick (824D3458)", "layout buttons (824D20A0)", "menu.pac (8253F9A0)",
                              "menu.pac selected (8253A028)", "menu.pac choice (8253C3F0)", "arena select (825A6A98)"};
constexpr int kKindCount = 7;

std::recursive_mutex g_mu;  // (a dump calls the game: its hooks come back here)
std::map<u32, u32> g_seen[kKindCount];  // per kind: object -> calls since the last report
u32 g_menu = 0;                         // the menu.pac menu seen last
long long g_next_report = 0;
long long g_dump_at = 0;  // when to dump after a press (0: none)
u16 g_last_buttons = 0;
int g_dumps = 0;

std::string Str(u32 at, u32 max) {
  std::string out;
  for (u32 i = 0; i < max; ++i) {
    const u8 c = bd::mem::try_load<u8>(at + i);
    if (!c)
      break;
    out += (c >= 32 && c < 127) ? char(c) : '?';
  }
  return out;
}

std::string MenuWords(u32 m) {
  if (!m || !bd::mem::try_at<u8>(m + 7252))
    return "-";
  return fmt::format("menu {:08X} +360={} +364={} +368={} +372={} +376={} +7248={:08X}", m,
                     int(bd::mem::load<u32>(m + 360)), int(bd::mem::load<u32>(m + 364)),
                     int(bd::mem::load<u32>(m + 368)), int(bd::mem::load<u32>(m + 372)),
                     int(bd::mem::load<u32>(m + 376)), bd::mem::load<u32>(m + 7248));
}

void Report(const char *why) {
  std::string line;
  for (int k = 1; k < kKindCount; ++k) {
    if (g_seen[k].empty())
      continue;
    line += fmt::format(" | {}:", kKinds[k]);
    int n = 0;
    for (auto &[obj, calls] : g_seen[k]) {
      if (++n > 4)
        break;
      line += fmt::format(" {:08X} (vtbl {:08X}) x{}", obj, bd::mem::try_load<u32>(obj), calls);
    }
    g_seen[k].clear();
  }
  BD_INFO("[mouse probe] {}{} | {}", why, line.empty() ? std::string(" | no menu hook") : line, MenuWords(g_menu));
}

void DumpLayout(const PPCContext &in, u8 *base) {
  PPCContext x = in;
  x.r3.u64 = 0;
  __imp__sub_8254E920(x, base);
  const u32 layout = x.r3.u32;
  auto *state = REX_KERNEL_STATE();
  if (!layout || !bd::mem::try_at<u8>(layout + 28) || !state || !state->emulator())
    return;
  const int n = ++g_dumps;
  const auto dir = state->emulator()->user_data_root() / "logs";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  std::ofstream f(dir / fmt::format("mouse_layout_{:02}.txt", n));
  const u32 groups = bd::mem::load<u32>(layout + 4), group_count = bd::mem::load<u32>(layout + 20);
  const u32 elements = bd::mem::load<u32>(layout + 8), element_count = bd::mem::load<u32>(layout + 24);
  f << fmt::format("layout 0x{:08X}: {} groups at 0x{:08X}, {} elements at 0x{:08X} | {}\n", layout, group_count,
                   groups, element_count, elements, MenuWords(g_menu));
  for (u32 g = 0; g < group_count && g < 4096; ++g) {
    const u32 at = groups + g * 144;
    std::string words;
    for (u32 o = 0; o < 144; o += 4)
      words += fmt::format(" {:08X}", bd::mem::load<u32>(at + o));
    f << fmt::format("G{} @{:08X} n={} first={:08X}:{}\n", g, at, bd::mem::load<u32>(at + 76),
                     bd::mem::load<u32>(at + 112), words);
  }
  for (u32 e = 0; e < element_count && e < 8192; ++e) {
    const u32 at = elements + e * 240;
    const u32 text = bd::mem::load<u32>(at + 200);
    f << fmt::format("E{} @{:08X} kind={:X} x={} y={} sx={} sy={} font={} name={} parent={:08X} hidden={:08X} "
                     "colour={:08X} text={}\n",
                     e, at, bd::mem::load<u32>(at), bd::mem::load<float>(at + 4), bd::mem::load<float>(at + 8),
                     bd::mem::load<float>(at + 12), bd::mem::load<float>(at + 16), Str(at + 80, 16),
                     Str(at + 96, 16), bd::mem::load<u32>(at + 176), bd::mem::load<u32>(at + 180),
                     bd::mem::load<u32>(at + 184), text ? Str(text, 40) : std::string("-"));
  }
  Wwe12RemoteShot(fmt::format("mouse_{:02}", n));
  BD_INFO("[mouse probe] dump {:02}: layout {:08X} ({} groups, {} elements) + frame", n, layout, group_count,
          element_count);
}

}  // namespace

// From the menus' hooks (here and in wwe12_universe.cpp, wwe12_quit.cpp, wwe12_arena_pages.cpp).
void Wwe12MouseProbeTick(int kind, const PPCContext &ctx, u8 *base) {
  if (!REXCVAR_GET(wwe12_mouse_probe) || kind <= 0 || kind >= kKindCount)
    return;
  std::lock_guard lock(g_mu);
  ++g_seen[kind][ctx.r3.u32];
  if (kind == 3 || kind == 4 || kind == 5)
    g_menu = ctx.r3.u32;
  const long long now = NowMs();
  if (g_dump_at && now >= g_dump_at && g_dumps < 60) {
    g_dump_at = 0;
    Report("after a press");
    DumpLayout(ctx, base);
    g_next_report = now + 1000;
  } else if (now >= g_next_report) {
    g_next_report = now + 1000;
    if (!Wwe12LoadingScreenUp() && !Wwe12SceneMode())
      Report("1 s");
  }
}

// From wwe12_intros.cpp's XInputGetState hook (player 1, a pad there): D-pad / A / B presses start a dump.
void Wwe12MouseProbePad(u16 buttons) {
  if (!REXCVAR_GET(wwe12_mouse_probe))
    return;
  std::lock_guard lock(g_mu);
  const u16 newly = u16(buttons & ~g_last_buttons & 0x300F);  // D-pad, A, B
  g_last_buttons = buttons;
  if (newly) {
    g_dump_at = NowMs() + 400;
    BD_INFO("[mouse probe] press {:04X} | {}", newly, MenuWords(g_menu));
  }
}

extern "C" REX_FUNC(sub_8253F9A0) {
  Wwe12MouseProbeTick(3, ctx, base);
  __imp__sub_8253F9A0(ctx, base);
}

