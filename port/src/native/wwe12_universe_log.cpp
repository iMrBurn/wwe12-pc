// WWE '12: the WWE Universe mode's own debug log (dev aid).
//
// The Universe code prints its decisions through sub_826D4228 (a printf into a stack buffer that
// the retail build throws away): the card of each show ("1: JOHN CENA: 901[ 2], ..."), rankings,
// roster setup, "Draft Event Success" / "Draft Event Cancel"... With wwe12_universe_log = true
// (tools/dev/lancer_univers.bat) the hook formats the same text into our log as "[uni] ...".
//
// The WWE Draft (Universe, after WrestleMania) works: DRAFT EVENT at the end of the draft match
// plays the lottery and the drafted Superstar's entrance (checked 2026-10-16).

#include <cstring>
#include <mutex>
#include <string>

#include <fmt/format.h>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_universe_log, false, "WWE12",
                    "Dev: write the WWE Universe mode's debug messages (draft, rosters) to the log.");

namespace {

// Variadic argument i (0 = the one after the format): r4..r10, then the caller's stack (the PPC
// parameter save area: 8-byte slots from r1+24, the r4 slot first, value in the low word).
u64 VarArg(const PPCContext &ctx, u32 i) {
  const u64 regs[7] = {ctx.r4.u64, ctx.r5.u64, ctx.r6.u64, ctx.r7.u64,
                       ctx.r8.u64, ctx.r9.u64, ctx.r10.u64};
  if (i < 7)
    return regs[i];
  return bd::mem::load<u64>(ctx.r1.u32 + 24 + i * 8);
}

// The game's format strings use %d %u %x %s %c with optional flags/width; floats are shown as "?".
std::string FormatGuest(const PPCContext &ctx, const char *f) {
  std::string out;
  u32 arg = 0;
  for (; *f; ++f) {
    if (*f != '%') {
      out += *f;
      continue;
    }
    const char *spec = f++;
    while (*f && std::strchr("-+ #0123456789.lhz", *f))
      ++f;
    if (!*f)
      break;
    const std::string flags(spec, f);  // "%-20" etc. without the conversion
    char conv = *f;
    if (conv == '%') {
      out += '%';
      continue;
    }
    const u64 v = VarArg(ctx, arg++);
    char buf[512];
    switch (conv) {
    case 'd': case 'i':
      std::snprintf(buf, sizeof(buf), (flags + "d").c_str(), int(u32(v)));
      break;
    case 'u': case 'x': case 'X': case 'c':
      std::snprintf(buf, sizeof(buf), (flags + conv).c_str(), unsigned(u32(v)));
      break;
    case 's': {
      std::string s = bd::mem::str(u32(v));
      if (s.size() > 200)
        s.resize(200);
      std::snprintf(buf, sizeof(buf), (flags + "s").c_str(), s.c_str());
      break;
    }
    default:
      std::snprintf(buf, sizeof(buf), "?");
    }
    out += buf;
  }
  while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
    out.pop_back();
  return out;
}

}  // namespace

REX_EXTERN(__imp__sub_826D4228);
extern "C" REX_FUNC(sub_826D4228) {
  if (REXCVAR_GET(wwe12_universe_log) && ctx.r3.u32) {
    static std::mutex s_mu;
    static std::string s_last;
    static u32 s_count = 0;
    const std::string line = FormatGuest(ctx, bd::mem::str(ctx.r3.u32));
    std::lock_guard lock(s_mu);
    if (line != s_last && s_count < 20000) {
      ++s_count;
      BD_INFO("[uni] {}", line);
      s_last = line;
    }
  }
  __imp__sub_826D4228(ctx, base);
}
