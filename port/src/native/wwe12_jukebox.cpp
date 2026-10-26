// WWE '12 PC: menu jukebox (1.0, work in progress: probe only for now).
//
// Found so far (2026-10-26):
//   - The music manager M = [[0x83231754] + 52] (sub_8270AC50, sub_8270B008) has players in slots
//     ([M + 4 + 4 * slot]): slot 0 = menu music. Its update sub_8271BFD0(M) (called through its
//     vtable) restarts the menu music when slot 0 has stopped and [M+32] is set, taking the next
//     number of a list of 19 at 0x83074908 (11 entrance themes 104 ... 321 and the menu themes
//     801-807) into [M+40]. That number does NOT choose the song: sub_8271C9B0(desc, number, flags)
//     always names "@System-Menu_Music", a sound-engine container that picks its own (test 1 of
//     2026-10-26: list set to 139, a menu theme played).
//   - Entrance themes use the same numbers: sub_8274A180 replaces %ENTSSMUSICNO% in an entrance
//     script with the wrestler's number and asks sub_8271CA28(number) first. A theme's name is made
//     by sub_8271F080(desc, number, 0, 0) = "@Entrance_Music-MUS_%04d_0_0". Giving that name to the
//     menu player does not work (test 2: the play fails at once, every frame): the theme has to be
//     prepared first.
//   - Create an Entrance's theme preview (sub_829EBBE8, a state machine) does: stop slots 0, 1 and 2
//     (sub_8271BB60(M, slot, 0); slot 0 also clears [M+32], so the menus stop restarting their
//     music), wait until they are idle (sub_8271BB00(M, slot, &state): state 0, bit 0x1 or 0x20),
//     sub_8271BC90(M, 2, desc) to load the theme, wait for sub_8271BF58(M, 2) (state bit 0x4 =
//     ready), then sub_8271BD28(M, 2, 0) to play; sub_8271BEE0(M, 2) (bit 0x8) = still playing.
//
// Probe (wwe12_jukebox_test): every entrance theme number seen in a match is logged; when the
// menus next pick a song, the probe plays the last entrance theme the way Create an Entrance does
// (each step logged, "[jukebox] ...").

#include <atomic>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_jukebox_test, false, "WWE12",
                    "Probe: back in the menus, play the last entrance theme heard in a match.");

REX_EXTERN(__imp__sub_8271BFD0);
REX_EXTERN(__imp__sub_8271CA28);
REX_EXTERN(__imp__sub_8271F080);
REX_EXTERN(__imp__sub_8271BB60);
REX_EXTERN(__imp__sub_8271BB00);
REX_EXTERN(__imp__sub_8271BC90);
REX_EXTERN(__imp__sub_8271BF58);
REX_EXTERN(__imp__sub_8271BD28);
REX_EXTERN(__imp__sub_8271BEE0);

namespace {
constexpr u32 kDescVtable = 0x82002EC4u;  // the sound description the callers build on the stack
constexpr int kThemeSlot = 2;              // Create an Entrance's preview slot

std::atomic<u32> g_last_theme{0};  // last entrance theme number seen (0 = none yet)

enum class Step { Idle, Stop, WaitStop, Prepare, WaitReady, Play, Playing, Done };
Step g_step = Step::Idle;  // game thread only
int g_frames = 0;          // updates spent in the current step
u32 g_playing = 0;         // theme being played

using GuestFn = void (*)(PPCContext&, u8*);

// Calls a guest function with r3-r6, on a stack below the caller's; `scratch` (out-parameters, the
// sound description) is 512 bytes of that stack, above the callee's frame.
u32 Call(GuestFn fn, PPCContext& ctx, u8* base, u32 r3, u32 r4, u32 r5 = 0, u32 r6 = 0) {
  PPCContext c = ctx;
  c.r1.u64 = (ctx.r1.u32 - 1024) & ~15u;
  c.r3.u64 = r3;
  c.r4.u64 = r4;
  c.r5.u64 = r5;
  c.r6.u64 = r6;
  fn(c, base);
  return c.r3.u32;
}
u32 Scratch(const PPCContext& ctx) { return ((ctx.r1.u32 - 1024) & ~15u) + 256; }

const char* StepName(Step s) {
  switch (s) {
    case Step::Stop: return "stop";
    case Step::WaitStop: return "wait stop";
    case Step::Prepare: return "prepare";
    case Step::WaitReady: return "wait ready";
    case Step::Play: return "play";
    case Step::Playing: return "playing";
    default: return "idle";
  }
}

void Go(Step s) {
  g_step = s;
  g_frames = 0;
}

u32 SlotState(PPCContext& ctx, u8* base, u32 m, int slot) {
  const u32 out = Scratch(ctx);
  bd::mem::store<u32>(out, 0);
  Call(__imp__sub_8271BB00, ctx, base, m, slot, out);
  return bd::mem::try_load<u32>(out);
}

// One step of the theme player per music update (game thread, ~30 per second).
void Run(PPCContext& ctx, u8* base, u32 m) {
  ++g_frames;
  switch (g_step) {
    case Step::Stop:
      for (int slot = 0; slot <= 2; ++slot) Call(__imp__sub_8271BB60, ctx, base, m, slot, 0);
      BD_INFO("[jukebox] menu music stopped (slots 0-2)");
      Go(Step::WaitStop);
      break;
    case Step::WaitStop: {
      bool idle = true;
      u32 states[3];
      for (int slot = 0; slot <= 2; ++slot) {
        states[slot] = SlotState(ctx, base, m, slot);
        if (states[slot] != 0 && !(states[slot] & 0x21)) idle = false;
      }
      if (idle || g_frames > 120) {
        BD_INFO("[jukebox] slots {}: states {:X} {:X} {:X} after {} updates", idle ? "idle" : "NOT idle (timeout)",
                states[0], states[1], states[2], g_frames);
        Go(Step::Prepare);
      }
      break;
    }
    case Step::Prepare: {
      g_playing = g_last_theme.load();
      const u32 desc = Scratch(ctx) + 16;
      for (u32 i = 0; i < 160; i += 4) bd::mem::store<u32>(desc + i, 0);
      bd::mem::store<u32>(desc, kDescVtable);
      bd::mem::store<u32>(desc + 136, 1);
      Call(__imp__sub_8271F080, ctx, base, desc, g_playing, 0, 0);
      const char* name = bd::mem::str(desc + 8);
      const u32 ok = Call(__imp__sub_8271BC90, ctx, base, m, kThemeSlot, desc);
      BD_INFO("[jukebox] theme {} prepared as '{}' (result {})", g_playing, name ? name : "?", ok);
      Go(Step::WaitReady);
      break;
    }
    case Step::WaitReady:
      if (Call(__imp__sub_8271BF58, ctx, base, m, kThemeSlot)) {
        BD_INFO("[jukebox] theme {} ready after {} updates", g_playing, g_frames);
        Go(Step::Play);
      } else if (g_frames > 600) {
        BD_INFO("[jukebox] theme {} never ready (state {:X}): given up", g_playing,
                SlotState(ctx, base, m, kThemeSlot));
        Go(Step::Done);
      }
      break;
    case Step::Play: {
      const u32 ok = Call(__imp__sub_8271BD28, ctx, base, m, kThemeSlot, 0);
      BD_INFO("[jukebox] theme {} play (result {}, state {:X})", g_playing, ok, SlotState(ctx, base, m, kThemeSlot));
      Go(Step::Playing);
      break;
    }
    case Step::Playing:
      if (g_frames == 60) {
        BD_INFO("[jukebox] theme {} after 2 s: playing {}, state {:X}", g_playing,
                Call(__imp__sub_8271BEE0, ctx, base, m, kThemeSlot), SlotState(ctx, base, m, kThemeSlot));
      }
      if (g_frames > 60 && !Call(__imp__sub_8271BEE0, ctx, base, m, kThemeSlot)) {
        BD_INFO("[jukebox] theme {} ended after {} updates", g_playing, g_frames);
        Go(Step::Done);
      }
      break;
    default:
      break;
  }
}
}  // namespace

// sub_8271CA28(number): the name of a music number that has one (only 428), else 0. Called by the
// entrance scripts' %ENTSSMUSICNO% with the wrestler's theme number.
extern "C" REX_FUNC(sub_8271CA28) {
  const u32 number = ctx.r3.u32;
  __imp__sub_8271CA28(ctx, base);
  if (!REXCVAR_GET(wwe12_jukebox_test)) return;
  if (number == 0 || number >= 800) return;
  if (g_last_theme.exchange(number) != number) BD_INFO("[jukebox] entrance theme {}", number);
}

extern "C" REX_FUNC(sub_8271BFD0) {
  if (!REXCVAR_GET(wwe12_jukebox_test)) {
    __imp__sub_8271BFD0(ctx, base);
    return;
  }
  PPCContext saved = ctx;
  const u32 m = ctx.r3.u32;
  const u32 before = bd::mem::try_field<u32>(m, 40);
  __imp__sub_8271BFD0(ctx, base);
  const u32 now = bd::mem::try_field<u32>(m, 40);
  if (now != before) {
    BD_INFO("[jukebox] menus picked song {} (item {}, manager {:08X})", now, bd::mem::try_field<u32>(m, 36), m);
    // Back in the menus after a match: play the last entrance theme instead.
    if (g_last_theme.load() && (g_step == Step::Idle || g_step == Step::Done)) Go(Step::Stop);
  }
  const Step was = g_step;
  if (g_step != Step::Idle && g_step != Step::Done) Run(saved, base, m);
  if (g_step != was && g_step == Step::Done) BD_INFO("[jukebox] done ({})", StepName(was));
}
