// I QUIT match (1.4, session Gameplay): the "ask him" mini-game of WWE '13, redone for WWE '12.
//
// WWE '13 (its InGameLua.pac script, read locally to understand it; nothing of it is copied): in an I Quit match,
// next to an opponent lying down, B calls the referee (StateIQuitMiniGameCallReferee, CheckIQuitAskHim); he
// kneels with a microphone and the downed superstar gets a gauge: a cursor to stop in the red zone ("NON") before
// the time runs out, while "I QUIT" grows whiter; if he fails, he gives up and the match ends "by I QUIT".
//
// Here: the match is the SUBMISSION one marked I Quit (wwe12_match_modes.cpp: its own ONE ON ONE row; no pinfall,
// no count-out, no rope breaks). In it:
// - a human next to a superstar lying down (his animation 0 face up or 1 face down) presses B (the pin button,
//   which does nothing in this match); a computer-controlled one asks when the human has been down 3 s;
// - the referee plays his "asking" animation (203: what he does when he asks a superstar in a submission,
//   probe 2026-11-14) through sub_8247AD18;
// - the gauge is drawn over the game with ImGui, in the look of the Royal Rumble's bottom rope elimination
//   gauge: a black bar, a red zone, a cursor going left and right, "I QUIT" whiter and whiter, "NON" under the
//   zone. A human stops the cursor with A; the computer resists with a chance that falls with its health;
// - failed (or the time out): he gives up, as the game's own give-up does it (sub_82265180, probe 2026-11-14):
//   victim +999 = 7, +1000 = who made him give up, +2784 = 0, sub_8227B208(victim), +2094 = 1. The match's
//   judge (sub_82263440) then ends the match with the give-up reason; its texts read "I QUIT".

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(wwe12_iquit_ask, true, "WWE12",
                    "I Quit match: B next to a downed opponent calls the referee and starts the I QUIT gauge (as WWE '13).");
REXCVAR_DEFINE_INT32(wwe12_iquit_ref_anim, 203, "WWE12",
                     "I Quit match: the referee's animation when he asks (203 = WWE '12's; a WWE '13 microphone motion imported "
                     "by the Mod Maker from the player's own disc can be set here).");

#include <algorithm>
#include <cmath>
#include <atomic>
#include <chrono>
#include <cstring>
#include <functional>
#include <random>

#include <imgui.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/imgui_drawer.h>

#include "svr_hook.h"
#include "core/logging.h"
#include "core/memory_helpers.h"

bool Wwe12IQuitMatch();  // wwe12_match_modes.cpp: an I Quit match is set up

REX_EXTERN(__imp__sub_8247AD18);  // play an animation on an actor
REX_EXTERN(__imp__sub_8227B208);  // (a superstar who gives up)

namespace {

using Clock = std::chrono::steady_clock;
constexpr u32 kActors = 0x8317B770;
constexpr int kReferee = 6;
constexpr u16 kPadA = 0x1000, kPadB = 0x2000, kPadDpad = 0x000F;  // (the D-pad: the taunts)
constexpr double kTime = 5.0;  // seconds to stop the cursor

std::atomic<u16> g_pad[4];
std::atomic<u16> g_pressed[4];  // new presses since the last tick (cleared there)

// the mini-game (written on the game thread, read by the overlay)
std::atomic<bool> g_on{false};
std::atomic<float> g_cursor{0.f}, g_zone0{0.4f}, g_zone1{0.55f}, g_white{0.f};
std::atomic<int> g_result{0};  // 0 running, 1 resisted ("NON"), 2 gave up (shown 1.5 s)
std::atomic<int> g_ask{0};     // a human's B next to a downed opponent: his place + 1
std::atomic<int> g_taunt{0};   // a human's taunt next to a downed opponent: his place + 1
std::atomic<bool> g_sub{false};  // a submission of the game itself in the I Quit match: "I QUIT" shown over it
int g_pressure = 0;            // threats since the victim went down (WWE '13's MakeAThreat): a harder gauge
int g_threat_by = -1;          // a threat is made: the referee asks again at g_threat_at
Clock::time_point g_threat_at, g_refused_at, g_sub_start;
bool g_we_asked = false;       // the referee's asking animation is ours (not a submission of the game)
int g_attacker = -1, g_victim = -1;
Clock::time_point g_start, g_end_shown, g_cooldown, g_down_since[6], g_cpu_next_ask;
float g_speed = 1.0f;
bool g_cpu_victim = false;
double g_cpu_stop_at = 0;
bool g_cpu_resists = false;
std::mt19937 g_rng{std::random_device{}()};

u32 Actor(int i) { return bd::mem::try_load<u32>(kActors + u32(i) * 4); }

u16 Anim(u32 actor) {
  const u32 st = actor ? bd::mem::try_load<u32>(actor + 680) : 0;
  return st ? bd::mem::try_load<u16>(st) : 0xFFFF;
}

// Lying down or hurting on the ground (animations seen in the probes of 2026-11-14: 0 / 1 lying face up / down,
// 20 / 21 getting up, 440-480 the ground pain and the slow getting up after a big move).
bool Down(u32 actor) {
  const u16 a = Anim(actor);
  return a <= 30 || (a >= 440 && a <= 480);
}

// The pad of a place (the select object's places: +44, 255 = the computer).
// (in a match the select object no longer says it: user's test of 2026-11-14, both superstars read as the computer.
// Then, as in a 1 on 1 exhibition against the computer, the first superstar is the first pad's.)
int PadOf(int place) {
  const u32 m = bd::mem::try_load<u32>(0x8324DEFC);
  int pads[2] = {-1, -1};
  if (m) {
    const u32 sel = m + bd::mem::try_load<u32>(m + 15960) * 1596 + 124;
    for (int k = 0; k < 2; ++k) {
      const u8 pad = bd::mem::try_load<u8>(sel + u32(k) * 244 + 44);
      pads[k] = pad < 4 ? int(pad) : -1;
    }
  }
  if (pads[0] < 0 && pads[1] < 0)
    pads[0] = 0;
  return place < 2 ? pads[place] : -1;
}

float HealthRatio(u32 actor) {
  const u32 dmg = bd::mem::try_load<u32>(actor + 3364);
  if (!dmg)
    return 1.f;
  float sum = 0;
  for (u32 k = 0; k < 4; ++k) {
    const int16_t v = int16_t(bd::mem::try_load<u16>(dmg + 56 + k * 2));
    sum += std::clamp(float(v) / 120.f, 0.f, 1.f);
  }
  return sum / 4.f;
}

void PlayAnim(const PPCContext &in, u8 *base, u32 actor, u32 anim) {
  PPCContext c = in;
  c.r1.u64 = (in.r1.u32 - 512) & ~15u;
  c.r3.u64 = actor;
  c.r4.u64 = anim;
  c.r5.u64 = 0;
  c.r6.u64 = 0;
  c.r8.u64 = 1;
  c.r9.u64 = 0;
  const u32 bits = bd::mem::try_load<u32>(0x82000AD8);  // (the float the game passes, lfs f1,2776 of 0x82000000)
  float f;
  std::memcpy(&f, &bits, 4);
  c.f1.f64 = f;
  __imp__sub_8247AD18(c, base);
}

void GiveUp(const PPCContext &in, u8 *base) {
  const u32 victim = Actor(g_victim);
  if (!victim)
    return;
  if (bd::mem::try_load<u8>(victim + 999) == 21)
    return;
  bd::mem::store<u8>(victim + 1000, u8(g_attacker));
  bd::mem::store<u8>(victim + 999, 7);
  bd::mem::store<u32>(victim + 2784, 0);
  PPCContext c = in;
  c.r1.u64 = (in.r1.u32 - 512) & ~15u;
  c.r3.u64 = victim;
  __imp__sub_8227B208(c, base);
  bd::mem::store<u8>(victim + 2094, 1);
  BD_INFO("[iquit] catcheur {} abandonne : I QUIT (gagnant {})", g_victim, g_attacker);
}

void ShowGauge();  // below

void Start(const PPCContext &in, u8 *base, int attacker, int victim) {
  g_attacker = attacker;
  g_victim = victim;
  g_start = Clock::now();
  const float health = HealthRatio(Actor(victim));
  // a wide zone and a slow cursor when he is fresh, small and fast when he is worn out
  const float width = std::max(0.05f, 0.08f + 0.22f * health - 0.04f * float(g_pressure));
  const float z0 = std::uniform_real_distribution<float>(0.15f, 0.85f - width)(g_rng);
  g_zone0 = z0;
  g_zone1 = z0 + width;
  g_speed = 0.7f + 1.1f * (1.f - health) + 0.25f * float(g_pressure);
  g_cursor = 0.f;
  g_white = 0.f;
  g_result = 0;
  g_cpu_victim = PadOf(victim) < 0;
  g_cpu_resists = std::uniform_real_distribution<float>(0.f, 1.f)(g_rng) < std::clamp(health * std::sqrt(health) - 0.15f * float(g_pressure), 0.05f, 0.9f);  // (63 % health: resists half the time)
  g_cpu_stop_at = std::uniform_real_distribution<double>(1.0, kTime - 0.5)(g_rng);
  g_on = true;
  ShowGauge();
  if (const u32 ref = Actor(kReferee)) {
    PlayAnim(in, base, ref, u32(REXCVAR_GET(wwe12_iquit_ref_anim)));
    g_we_asked = true;
  }
  BD_INFO("[iquit] l'arbitre demande au catcheur {} (vie {:.0f} %, zone {:.2f}-{:.2f}, menaces {}{})", victim,
          health * 100, z0, z0 + width, g_pressure, g_cpu_victim ? ", ordinateur" : "");
}

}  // namespace

// From the XInputGetState hook (through wwe12_match_modes.cpp): every pad's buttons. Returns the buttons the game
// must not see: in an I Quit match, B next to a downed opponent is ours (the game would start a submission: user's
// test of 2026-11-14), and A / B during the gauge.
u16 Wwe12IQuitPad(u32 user, u16 buttons) {
  if (user >= 4)
    return 0;
  const u16 before = g_pad[user].exchange(buttons);
  const u16 newly = u16(buttons & ~before);
  if (newly)
    g_pressed[user].fetch_or(newly);
  if (!REXCVAR_GET(wwe12_iquit_ask) || !Wwe12IQuitMatch())
    return 0;
  if (g_on.load())
    return u16(kPadA | kPadB);
  for (int a = 0; a < 2; ++a)
    if (PadOf(a) == int(user) && !Down(Actor(a)) && Down(Actor(1 - a))) {
      if (newly & kPadDpad)
        g_taunt = a + 1;  // (the game plays the taunt itself; a threat if the referee has just asked)
      if (newly & kPadB)
        g_ask = a + 1;  // (asked now, next to him: the tick starts the mini-game)
      return kPadB;
    }
  return 0;
}

// Once per game frame during a match (wwe12_match_modes.cpp, from the gauge hook of the first superstar).
void Wwe12IQuitTick(const PPCContext &ctx, u8 *base) {
  if (!REXCVAR_GET(wwe12_iquit_ask) || !Wwe12IQuitMatch()) {
    g_on = false;
    g_sub = false;
    g_pressure = 0;
    g_threat_by = -1;
    return;
  }
  u16 pressed[4];
  for (int p = 0; p < 4; ++p)
    pressed[p] = g_pressed[p].exchange(0);
  const auto now = Clock::now();
  if (g_on) {
    if (g_result != 0) {
      if (now > g_end_shown) {
        g_on = false;
        if (const u32 ref = Actor(kReferee))
          if (Anim(ref) == u16(REXCVAR_GET(wwe12_iquit_ref_anim)))
            PlayAnim(ctx, base, ref, 20152);  // (the referee stands up again: his idle, seen in the probes)
      }
      return;
    }
    const double t = std::chrono::duration<double>(now - g_start).count();
    const double phase = t * g_speed;  // the cursor goes left and right
    const double frac = phase - std::floor(phase);
    const float cursor = float((int(phase) % 2) ? 1.0 - frac : frac);
    g_cursor = cursor;
    g_white = float(std::min(1.0, t / kTime));
    bool stopped = false;
    if (g_cpu_victim) {
      if (t >= g_cpu_stop_at) {
        stopped = true;
        if (g_cpu_resists)  // (the computer stops it where it wants)
          g_cursor = (g_zone0.load() + g_zone1.load()) / 2;
      }
    } else {
      const int pad = PadOf(g_victim);
      stopped = pad >= 0 && (pressed[pad] & kPadA);
    }
    if (stopped || t >= kTime) {
      const float c = g_cursor.load();
      const bool resisted = stopped && c >= g_zone0.load() && c <= g_zone1.load();
      g_result = resisted ? 1 : 2;
      g_end_shown = now + std::chrono::milliseconds(1500);
      g_cooldown = now + std::chrono::seconds(4);
      if (resisted) {
        BD_INFO("[iquit] le catcheur {} resiste (NON)", g_victim);
        g_refused_at = now;
        // a computer facing a human who said NO threatens him (WWE '13: a taunt, then the referee asks again)
        if (PadOf(g_attacker) < 0 && !g_cpu_victim && std::uniform_int_distribution<int>(0, 1)(g_rng)) {
          g_threat_by = g_attacker;
          g_threat_at = now + std::chrono::milliseconds(4000);
          g_pressure = std::min(3, g_pressure + 1);
          BD_INFO("[iquit] l'ordinateur menace le catcheur {} (menaces {})", g_victim, g_pressure);
        }
      } else {
        GiveUp(ctx, base);
      }
    }
    return;
  }
  // a submission of the game itself (the referee asks with his own animation, not ours): "I QUIT" over it
  {
    const u32 ref = Actor(kReferee);
    const bool asking = ref && Anim(ref) == 203;
    if (!asking)
      g_we_asked = false;
    if (asking && !g_we_asked) {
      if (!g_sub.exchange(true)) {
        g_sub_start = now;
        ShowGauge();
      }
      g_white = float(std::min(1.0, std::chrono::duration<double>(now - g_sub_start).count() / 8.0));
    } else {
      g_sub = false;
    }
  }
  // a taunt next to the downed opponent just after he said NO: a threat, the referee asks again, harder
  const int taunt = g_taunt.exchange(0);
  if (taunt && g_threat_by < 0 && now - g_refused_at < std::chrono::seconds(8)) {
    g_threat_by = taunt - 1;
    g_threat_at = now + std::chrono::milliseconds(2500);  // (the taunt plays first)
    g_pressure = std::min(3, g_pressure + 1);
    BD_INFO("[iquit] le catcheur {} menace son adversaire (menaces {})", g_threat_by, g_pressure);
  }
  if (g_threat_by >= 0 && now >= g_threat_at) {
    const int a = g_threat_by, v = 1 - a;
    g_threat_by = -1;
    if (Actor(a) && Actor(v) && !Down(Actor(a)) && Down(Actor(v))) {
      g_ask = 0;
      Start(ctx, base, a, v);
      return;
    }
  }
  if (now < g_cooldown) {
    g_ask = 0;
    return;
  }
  // who can ask whom: two superstars, one lying down
  for (int a = 0; a < 2; ++a) {
    const int v = 1 - a;
    const u32 att = Actor(a), vic = Actor(v);
    if (!att || !vic || Down(att))
      continue;
    if (!Down(vic)) {
      g_down_since[v] = now;
      if (g_threat_by < 0)
        g_pressure = 0;  // (back on his feet: the threats start again from nothing)
      continue;
    }
    const int pad = PadOf(a);
    const bool human_asks = pad >= 0 && g_ask.load() == a + 1;
    // the computer asks a worn-out opponent (under 70 % health) who has been down 3 s, at most every 15 s
    const bool cpu_asks = pad < 0 && now - g_down_since[v] > std::chrono::seconds(3) && now > g_cpu_next_ask &&
                          HealthRatio(vic) < 0.7f;
    if (human_asks || cpu_asks) {
      g_ask = 0;
      if (cpu_asks)
        g_cpu_next_ask = now + std::chrono::seconds(15);
      Start(ctx, base, a, v);
      return;
    }
  }
  g_ask = 0;  // (a B the game did not see but that asked no one now)
}

// ---- the gauge, drawn over the game ----

namespace {
rex::ui::ImGuiDrawer *g_drawer = nullptr;
std::function<void(std::function<void()>)> g_post;  // runs a function on the UI thread
std::atomic<bool> g_shown{false};                    // the gauge's dialog exists (UI thread owns it)

class IQuitOverlay final : public rex::ui::ImGuiDialog {
 public:
  explicit IQuitOverlay(rex::ui::ImGuiDrawer *drawer) : ImGuiDialog(drawer) {}

 protected:
  void OnClose() override { g_shown = false; }
  void OnDraw(ImGuiIO &io) override {
    const bool sub_only = !g_on.load() && g_sub.load();
    if (!g_on.load() && !sub_only) {
      Close();  // (no dialog left: the drawer stops drawing ImGui every frame)
      return;
    }
    const float k = std::max(0.6f, io.DisplaySize.y / 1080.0f);
    ImDrawList *dl = ImGui::GetForegroundDrawList();
    ImFont *font = ImGui::GetFont();
    const float w = 400.f * k, h = 26.f * k;
    const float x = io.DisplaySize.x * 0.5f - w * 0.5f, y = io.DisplaySize.y * 0.58f;
    // "I QUIT", whiter and whiter
    const float white = g_white.load();
    const int g = int(90 + 165 * white);
    const char *title = "I QUIT";
    const float ts = 34.f * k;
    const ImVec2 tsz = font->CalcTextSizeA(ts, FLT_MAX, 0.f, title);
    dl->AddText(font, ts, ImVec2(x + w * 0.5f - tsz.x * 0.5f + 2 * k, y - tsz.y - 6 * k + 2 * k), IM_COL32(0, 0, 0, 200),
                title);
    dl->AddText(font, ts, ImVec2(x + w * 0.5f - tsz.x * 0.5f, y - tsz.y - 6 * k), IM_COL32(g, g, g, 255), title);
    if (sub_only)
      return;  // (a submission of the game: its own meter, our title over it)
    // the bar, its red zone and the cursor
    dl->AddRectFilled(ImVec2(x - 4 * k, y - 4 * k), ImVec2(x + w + 4 * k, y + h + 4 * k), IM_COL32(230, 230, 230, 255),
                      3 * k);
    dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), IM_COL32(20, 20, 20, 255));
    const float z0 = x + w * g_zone0.load(), z1 = x + w * g_zone1.load();
    dl->AddRectFilled(ImVec2(z0, y + 3 * k), ImVec2(z1, y + h - 3 * k), IM_COL32(210, 20, 20, 255));
    const float cx = x + w * g_cursor.load();
    dl->AddRectFilled(ImVec2(cx - 3 * k, y - 6 * k), ImVec2(cx + 3 * k, y + h + 6 * k), IM_COL32(255, 255, 255, 255));
    // "NON" under the zone, the result
    const char *non = "NON";
    const float ns = 26.f * k;
    const ImVec2 nsz = font->CalcTextSizeA(ns, FLT_MAX, 0.f, non);
    const float nx = (z0 + z1) * 0.5f - nsz.x * 0.5f;
    dl->AddText(font, ns, ImVec2(nx + 2 * k, y + h + 10 * k + 2 * k), IM_COL32(0, 0, 0, 200), non);
    dl->AddText(font, ns, ImVec2(nx, y + h + 10 * k), IM_COL32(255, 255, 255, 255), non);
    if (const int r = g_result.load()) {
      const char *msg = r == 1 ? "NON !" : "I QUIT !";
      const float ms = 48.f * k;
      const ImVec2 msz = font->CalcTextSizeA(ms, FLT_MAX, 0.f, msg);
      const ImU32 col = r == 1 ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 60, 60, 255);
      dl->AddText(font, ms, ImVec2(io.DisplaySize.x * 0.5f - msz.x * 0.5f + 3 * k, y + h + 50 * k + 3 * k),
                  IM_COL32(0, 0, 0, 220), msg);
      dl->AddText(font, ms, ImVec2(io.DisplaySize.x * 0.5f - msz.x * 0.5f, y + h + 50 * k), col, msg);
    }
  }
};
}  // namespace

// From the app's start-up (wwe12_app.h, next to Wwe12DisplayInit): the drawer and a way to the UI thread.
void Wwe12IQuitOverlayInit(rex::ui::ImGuiDrawer *drawer, std::function<void(std::function<void()>)> post) {
  g_drawer = drawer;
  g_post = std::move(post);
}

namespace {
void ShowGauge() {
  if (!g_drawer || !g_post || g_shown.exchange(true))
    return;
  g_post([] { new IQuitOverlay(g_drawer); });  // deletes itself when the mini-game is over
}
}  // namespace
