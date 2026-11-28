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
std::map<u32, long long> g_last_screens;  // layout screens (sub_824D3458) and when they ticked last

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

// v2 (2026-11-28: sub_8254E920 = *(*0x8324DEF4 + 84) gave nothing in the main menu): every layout the layout holder
// *0x8324DEF4 keeps (+64..+124, as the manager at +96: +12 groups {u32 count, u32 elements}, +28 group count, elements
// of 240 bytes), named elements only; and the words of the menu object and of the layout screens ticking now.
void DumpElements(std::ofstream &f, u32 at, u32 e, u32 g) {
  const u32 text = bd::mem::load<u32>(at + 200);
  f << fmt::format("  g{} e{} @{:08X} kind={:X} x={} y={} sx={} sy={} name={} parent={:08X} hidden={:08X} "
                   "colour={:08X} {:08X} {:08X} flags={:08X} text={}\n",
                   g, e, at, bd::mem::load<u32>(at), bd::mem::load<float>(at + 4), bd::mem::load<float>(at + 8),
                   bd::mem::load<float>(at + 12), bd::mem::load<float>(at + 16), Str(at + 96, 32),
                   bd::mem::load<u32>(at + 176), bd::mem::load<u32>(at + 180), bd::mem::load<u32>(at + 184),
                   bd::mem::load<u32>(at + 188), bd::mem::load<u32>(at + 192), bd::mem::load<u32>(at + 228),
                   text && bd::mem::try_at<u8>(text) ? Str(text, 40) : std::string("-"));
}

void DumpWords(std::ofstream &f, const char *what, u32 at, u32 size) {
  if (!at || !bd::mem::try_at<u8>(at + size))
    return;
  f << fmt::format("{} {:08X} (vtbl {:08X}):", what, at, bd::mem::load<u32>(at));
  for (u32 o = 0; o < size; o += 4)
    f << fmt::format("{}{:08X}", o % 64 ? " " : fmt::format("\n  +{:03X}: ", o), bd::mem::load<u32>(at + o));
  f << "\n";
}

void DumpLayout(const PPCContext &in, u8 *base) {
  (void)in, (void)base;
  auto *state = REX_KERNEL_STATE();
  const u32 holder = bd::mem::try_load<u32>(0x8324DEF4);
  if (!holder || !state || !state->emulator())
    return;
  const int n = ++g_dumps;
  const auto dir = state->emulator()->user_data_root() / "logs";
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
  std::ofstream f(dir / fmt::format("mouse_layout_{:02}.txt", n));
  f << MenuWords(g_menu) << "\n";
  DumpWords(f, "menu", g_menu, 0x200);
  for (auto &[obj, when] : g_last_screens)
    if (NowMs() - when < 200)
      DumpWords(f, "screen", obj, 0x400);
  DumpWords(f, "holder", holder, 0x100);
  u32 named = 0;
  for (u32 o = 64; o <= 124; o += 4) {
    const u32 m = bd::mem::try_load<u32>(holder + o);
    if (!m || !bd::mem::try_at<u8>(m + 32))
      continue;
    const u32 groups = bd::mem::load<u32>(m + 12), count = bd::mem::load<u32>(m + 28);
    if (!groups || count == 0 || count > 4096 || !bd::mem::try_at<u8>(groups + count * 16))
      continue;
    f << fmt::format("holder+{} manager {:08X}: {} groups at {:08X}\n", o, m, count, groups);
    for (u32 g = 0; g < count; ++g) {
      const u32 ec2 = bd::mem::load<u32>(groups + g * 16), els = bd::mem::load<u32>(groups + g * 16 + 4);
      if (!els || ec2 > 4096 || !bd::mem::try_at<u8>(els + ec2 * 240))
        continue;
      for (u32 e = 0; e < ec2; ++e) {
        const u32 at = els + e * 240;
        if (!bd::mem::try_load<u8>(at + 96))
          continue;
        DumpElements(f, at, e, g);
        ++named;
      }
    }
  }
  Wwe12RemoteShot(fmt::format("mouse_{:02}", n));
  BD_INFO("[mouse probe] dump {:02}: {} named elements + menu / screens / holder words + frame", n, named);
}

}  // namespace

// From the menus' hooks (here and in wwe12_universe.cpp, wwe12_quit.cpp, wwe12_arena_pages.cpp).
void Wwe12MouseProbeTick(int kind, const PPCContext &ctx, u8 *base) {
  if (!REXCVAR_GET(wwe12_mouse_probe) || kind <= 0 || kind >= kKindCount)
    return;
  std::lock_guard lock(g_mu);
  ++g_seen[kind][ctx.r3.u32];
  if (kind == 1)
    g_last_screens[ctx.r3.u32] = NowMs();
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

// From wwe12_intros.cpp's XInputGetState hook (player 1, a pad there): D-pad (or stick) / A / B presses start a dump.
void Wwe12MouseProbePad(u16 buttons, int16_t lx, int16_t ly) {
  if (!REXCVAR_GET(wwe12_mouse_probe))
    return;
  std::lock_guard lock(g_mu);
  // the left stick pushed (keyboard arrows too, as the SDK maps them) counts as the D-pad
  u16 now_b = buttons;
  if (ly > 16000) now_b |= 0x0001;
  if (ly < -16000) now_b |= 0x0002;
  if (lx < -16000) now_b |= 0x0004;
  if (lx > 16000) now_b |= 0x0008;
  const u16 newly = u16(now_b & ~g_last_buttons & 0x300F);  // D-pad, A, B
  g_last_buttons = now_b;
  if (newly) {
    g_dump_at = NowMs() + 400;
    BD_INFO("[mouse probe] press {:04X} | {}", newly, MenuWords(g_menu));
  }
}

extern "C" REX_FUNC(sub_8253F9A0) {
  Wwe12MouseProbeTick(3, ctx, base);
  __imp__sub_8253F9A0(ctx, base);
}

