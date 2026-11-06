// Dev probe (off by default): the arena's big screens and their movies, 2026-11-06.
//
// Request (Mod Maker, the user's go): in a match, show the arena's movie on the stage's big screen
// (model 800, texture "titantron_movie") instead of the live picture (base arenas) or black
// (ported arenas, whose screen is 808). RenderDoc shows the live picture as model 800 drawn again
// with a copy of the match frame; where the game switches it is not known yet.
//
// With wwe12_screen_probe on, the screen-movie manager's calls are logged as "[screen]", with the
// remote pad's state (menu / loading / scene / match) and the caller (lr):
//   sub_82696160(mgr, desc, stream) - start a movie: desc+0 its number, +44 != 0 = from memory
//                                     (sub_82695798, an arena movie in bgNN.pac), else streamed
//                                     from movies\titantron\NNN.bik (path: sub_82A316A0);
//   sub_82695798(mgr, desc)         - the from-memory start;
//   sub_82695DE0(mgr, op, desc, ..) - a command to a channel (sub_82696328 sends 3, sub_82696448 2);
//   sub_826959C8(mgr, channel)      - stops a channel (sub_82696518 stops all 4).
// Read the log around the end of the entrances: which movie stops, who stops it, and what starts.

#include <string>

#include <fmt/format.h>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_screen_probe, false, "WWE12",
                    "Probe: log the arena screens' movie starts, commands and stops ([screen]).");

const char *Wwe12RemoteState();  // wwe12_remote_pad.cpp

namespace {

std::string Desc(u32 desc) {
  if (!desc)
    return "desc 0";
  return fmt::format("desc {:08X}: no {} +4 {} +20 {} +28 {} +32 {} +36 {} +40 {} +44 {}", desc,
                     bd::mem::try_load<u32>(desc), bd::mem::try_load<u32>(desc + 4),
                     bd::mem::try_load<u32>(desc + 20), bd::mem::try_load<u32>(desc + 28),
                     bd::mem::try_load<u32>(desc + 32), bd::mem::try_load<u32>(desc + 36),
                     bd::mem::try_load<u32>(desc + 40), bd::mem::try_load<u32>(desc + 44));
}

}  // namespace

REX_EXTERN(__imp__sub_82696160);
extern "C" REX_FUNC(sub_82696160) {
  if (!REXCVAR_GET(wwe12_screen_probe)) {
    __imp__sub_82696160(ctx, base);
    return;
  }
  const u32 mgr = ctx.r3.u32, desc = ctx.r4.u32, stream = ctx.r5.u32, lr = u32(ctx.lr);
  const std::string d = Desc(desc);
  __imp__sub_82696160(ctx, base);
  BD_INFO("[screen] {} start mgr {:08X} stream {} {} -> {} (lr {:08X})", Wwe12RemoteState(), mgr, stream, d,
          ctx.r3.u32, lr);
}

REX_EXTERN(__imp__sub_82695798);
extern "C" REX_FUNC(sub_82695798) {
  if (REXCVAR_GET(wwe12_screen_probe))
    BD_INFO("[screen] {} memory start mgr {:08X} {} (lr {:08X})", Wwe12RemoteState(), ctx.r3.u32,
            Desc(ctx.r4.u32), u32(ctx.lr));
  __imp__sub_82695798(ctx, base);
}

REX_EXTERN(__imp__sub_82695DE0);
extern "C" REX_FUNC(sub_82695DE0) {
  if (REXCVAR_GET(wwe12_screen_probe))
    BD_INFO("[screen] {} command mgr {:08X} op {} {} (lr {:08X})", Wwe12RemoteState(), ctx.r3.u32, ctx.r4.u32,
            Desc(ctx.r5.u32), u32(ctx.lr));
  __imp__sub_82695DE0(ctx, base);
}

REX_EXTERN(__imp__sub_826959C8);
extern "C" REX_FUNC(sub_826959C8) {
  if (REXCVAR_GET(wwe12_screen_probe)) {
    const u32 mgr = ctx.r3.u32, ch = ctx.r4.u32;
    // Channel records at mgr+24, 60 bytes each (sub_826959C8; sub_82696160 fills +0 state, +4, +12, +16, +28).
    const u32 rec = mgr + 24 + ch * 60;
    BD_INFO("[screen] {} stop mgr {:08X} channel {} (state {} +12 {} +28 {}) (lr {:08X})", Wwe12RemoteState(), mgr,
            ch, bd::mem::try_load<u32>(rec), bd::mem::try_load<u32>(rec + 12), bd::mem::try_load<u32>(rec + 28),
            u32(ctx.lr));
  }
  __imp__sub_826959C8(ctx, base);
}
