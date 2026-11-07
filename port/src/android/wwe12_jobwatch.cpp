// Android: watches the game's job thread (sub_821D9428), the one that writes the instant replay's
// highlights into cache:\Spectacular.rps. In the frozen runs (issues #22-#31, about 1 in 2 on the
// emulator) that thread opens the file, reads 16 KB, then waits forever and the match never ends.
// Layout read from the recompiled loop (r31 = job system): +28 critical section, +64 event the
// thread waits on, +68 event set when a job is done, +80 two 48-byte entries (type at +28, file
// manager at +4), +180 first entry, +184 number of entries queued. The loop takes ONE entry per
// wake-up of the +64 event.
//
// wwe12_jobwatch: a host thread logs the queue on each change ("[jobwatch]") and when entries stay
// queued for 2 s while nothing is taken: then the answer to "lost wake-up (entries waiting) or
// never queued (queue empty)" is in the log. wwe12_jobwatch_heal also sets the +64 event again in
// that case (a lost wake-up of an auto-reset event, which merges two Sets into one, is cured by it).

#include <atomic>
#include <chrono>
#include <thread>

#include <rex/cvar.h>
#include <rex/kernel/xboxkrnl/threading.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "../native/svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_jobwatch, true, "WWE12",
                    "Android: log the game's replay-writer job queue (to find the post-match freeze).");
REXCVAR_DEFINE_BOOL(wwe12_jobwatch_heal, false, "WWE12",
                    "Android: set the job thread's event again when entries wait in its queue (lost "
                    "wake-up cure, to test).");

namespace {
void Watch(u32 jobs) {
  using clock = std::chrono::steady_clock;
  u32 last_count = ~0u, last_head = ~0u, last_types = ~0u;
  auto changed = clock::now();
  bool stuck_logged = false;
  u32 heals = 0;
  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(250));
    const u32 count = bd::mem::load<u32>(jobs + 184), head = bd::mem::load<u32>(jobs + 180);
    const u32 t0 = bd::mem::load<u32>(jobs + 80 + 28), t1 = bd::mem::load<u32>(jobs + 80 + 48 + 28);
    const u32 types = (t0 & 0xFFFF) << 16 | (t1 & 0xFFFF);
    const auto now = clock::now();
    if (count != last_count || head != last_head || types != last_types) {
      BD_INFO("[jobwatch] queued {} first {} | entry types {} {} | events 0x{:X} 0x{:X}", count, head,
              t0, t1, bd::mem::load<u32>(jobs + 64), bd::mem::load<u32>(jobs + 68));
      last_count = count, last_head = head, last_types = types;
      changed = now;
      stuck_logged = false;
      continue;
    }
    if (count == 0 || count > 2)
      continue;
    const double waited = std::chrono::duration<double>(now - changed).count();
    if (waited >= 2.0 && !stuck_logged) {
      stuck_logged = true;
      BD_INFO("[jobwatch] {} entries have waited {:.1f} s and nothing was taken (first {}, types {} {})",
              count, waited, head, t0, t1);
    }
    if (waited >= 2.0 && REXCVAR_GET(wwe12_jobwatch_heal) && heals < 20) {
      ++heals;
      changed = now;  // again in 2 s if it did not help
      const u32 status = rex::kernel::xboxkrnl::xeNtSetEvent(bd::mem::load<u32>(jobs + 64), nullptr);
      BD_INFO("[jobwatch] heal #{}: event set again (status 0x{:X})", heals, status);
    }
  }
}
}  // namespace

REX_EXTERN(__imp__sub_821D9428);
extern "C" REX_FUNC(sub_821D9428) {
  static std::atomic<bool> s_started{false};
  const u32 jobs = ctx.r3.u32;
  if (REXCVAR_GET(wwe12_jobwatch) && jobs && !s_started.exchange(true)) {
    BD_INFO("[jobwatch] job thread started, job system 0x{:08X}", jobs);
    std::thread(Watch, jobs).detach();
  }
  __imp__sub_821D9428(ctx, base);
}
