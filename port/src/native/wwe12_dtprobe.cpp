// Dev-only experiment: compiled only with -DWWE12_DEV_PROBES (off by default, so releases carry no hooks).
#if defined(WWE12_DEV_PROBES)

// Probe for the belt-at-60-fps bug: counts calls of the few functions that read the game's
// frame-rate time-step fields (struct 0x8324CE28, fields +28, +36, +64), and logs them every 2 s
// while wwe12_dtprobe is on. Functions called during Cena's run are the suspects.

#include <atomic>
#include <chrono>
#include <mutex>

#include <rex/cvar.h>

#include "core/logging.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_dtprobe, false, "WWE12",
                    "Debug: log how often the functions reading the frame-rate time-step fields run.");

namespace {
struct Probe { const char *name; const char *field; std::atomic<u32> n; };
Probe g_probes[] = {
  {"sub_821E1C08", "f28", {0}},
  {"sub_823B2808", "f28", {0}},
  {"sub_8254F608", "f28", {0}},
  {"sub_821C7130", "f36", {0}},
  {"sub_8237BFC8", "f36", {0}},
  {"sub_82698448", "f36", {0}},
  {"sub_829F4718", "f36", {0}},
  {"sub_821BAE38", "f64", {0}},
  {"sub_821BD498", "f64", {0}},
  {"sub_821BE370", "f64", {0}},
  {"sub_8222D9F8", "f64", {0}},
  {"sub_82260A68", "f64", {0}},
  {"sub_822624E0", "f64", {0}},
  {"sub_823213F0", "f64", {0}},
  {"sub_8235E760", "f64", {0}},
  {"sub_823685F0", "f64", {0}},
  {"sub_8247DF78", "f64", {0}},
  {"sub_824BDD50", "f64", {0}},
  {"sub_824F9F70", "f64", {0}},
  {"sub_824FA120", "f64", {0}},
  {"sub_8256B790", "f64", {0}},
  {"sub_82597A90", "f64", {0}},
  {"sub_8269B580", "f64", {0}},
  {"sub_826A0D88", "f64", {0}},
  {"sub_826A18C0", "f64", {0}},
};

void Hit(size_t i) {
  if (!REXCVAR_GET(wwe12_dtprobe))
    return;
  g_probes[i].n.fetch_add(1, std::memory_order_relaxed);
  static std::atomic<i64> s_last{0};
  const i64 now = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now().time_since_epoch()).count();
  i64 last = s_last.load(std::memory_order_relaxed);
  if (now - last < 2000 || !s_last.compare_exchange_strong(last, now))
    return;
  std::string line;
  for (auto &p : g_probes) {
    const u32 c = p.n.exchange(0, std::memory_order_relaxed);
    if (c)
      line += fmt::format(" {}[{}]={}", p.name, p.field, c);
  }
  if (!line.empty())
    BD_INFO("[dtprobe]{}", line);
}
}  // namespace

REX_EXTERN(__imp__sub_821E1C08);
REX_EXTERN(__imp__sub_823B2808);
REX_EXTERN(__imp__sub_8254F608);
REX_EXTERN(__imp__sub_821C7130);
REX_EXTERN(__imp__sub_8237BFC8);
REX_EXTERN(__imp__sub_82698448);
REX_EXTERN(__imp__sub_829F4718);
REX_EXTERN(__imp__sub_821BAE38);
REX_EXTERN(__imp__sub_821BD498);
REX_EXTERN(__imp__sub_821BE370);
REX_EXTERN(__imp__sub_8222D9F8);
REX_EXTERN(__imp__sub_82260A68);
REX_EXTERN(__imp__sub_822624E0);
REX_EXTERN(__imp__sub_823213F0);
REX_EXTERN(__imp__sub_8235E760);
REX_EXTERN(__imp__sub_823685F0);
REX_EXTERN(__imp__sub_8247DF78);
REX_EXTERN(__imp__sub_824BDD50);
REX_EXTERN(__imp__sub_824F9F70);
REX_EXTERN(__imp__sub_824FA120);
REX_EXTERN(__imp__sub_8256B790);
REX_EXTERN(__imp__sub_82597A90);
REX_EXTERN(__imp__sub_8269B580);
REX_EXTERN(__imp__sub_826A0D88);
REX_EXTERN(__imp__sub_826A18C0);

extern "C" REX_FUNC(sub_821E1C08) {
  Hit(0);
  __imp__sub_821E1C08(ctx, base);
}

extern "C" REX_FUNC(sub_823B2808) {
  Hit(1);
  __imp__sub_823B2808(ctx, base);
}

extern "C" REX_FUNC(sub_8254F608) {
  Hit(2);
  __imp__sub_8254F608(ctx, base);
}

extern "C" REX_FUNC(sub_821C7130) {
  Hit(3);
  __imp__sub_821C7130(ctx, base);
}

extern "C" REX_FUNC(sub_8237BFC8) {
  Hit(4);
  __imp__sub_8237BFC8(ctx, base);
}

extern "C" REX_FUNC(sub_82698448) {
  Hit(5);
  __imp__sub_82698448(ctx, base);
}

extern "C" REX_FUNC(sub_829F4718) {
  Hit(6);
  __imp__sub_829F4718(ctx, base);
}

extern "C" REX_FUNC(sub_821BAE38) {
  Hit(7);
  __imp__sub_821BAE38(ctx, base);
}

extern "C" REX_FUNC(sub_821BD498) {
  Hit(8);
  __imp__sub_821BD498(ctx, base);
}

extern "C" REX_FUNC(sub_821BE370) {
  Hit(9);
  __imp__sub_821BE370(ctx, base);
}

extern "C" REX_FUNC(sub_8222D9F8) {
  Hit(10);
  __imp__sub_8222D9F8(ctx, base);
}

extern "C" REX_FUNC(sub_82260A68) {
  Hit(11);
  __imp__sub_82260A68(ctx, base);
}

extern "C" REX_FUNC(sub_822624E0) {
  Hit(12);
  __imp__sub_822624E0(ctx, base);
}

extern "C" REX_FUNC(sub_823213F0) {
  Hit(13);
  __imp__sub_823213F0(ctx, base);
}

extern "C" REX_FUNC(sub_8235E760) {
  Hit(14);
  __imp__sub_8235E760(ctx, base);
}

extern "C" REX_FUNC(sub_823685F0) {
  Hit(15);
  __imp__sub_823685F0(ctx, base);
}

extern "C" REX_FUNC(sub_8247DF78) {
  Hit(16);
  __imp__sub_8247DF78(ctx, base);
}

extern "C" REX_FUNC(sub_824BDD50) {
  Hit(17);
  __imp__sub_824BDD50(ctx, base);
}

extern "C" REX_FUNC(sub_824F9F70) {
  Hit(18);
  __imp__sub_824F9F70(ctx, base);
}

extern "C" REX_FUNC(sub_824FA120) {
  Hit(19);
  __imp__sub_824FA120(ctx, base);
}

extern "C" REX_FUNC(sub_8256B790) {
  Hit(20);
  __imp__sub_8256B790(ctx, base);
}

extern "C" REX_FUNC(sub_82597A90) {
  Hit(21);
  __imp__sub_82597A90(ctx, base);
}

extern "C" REX_FUNC(sub_8269B580) {
  Hit(22);
  __imp__sub_8269B580(ctx, base);
}

extern "C" REX_FUNC(sub_826A0D88) {
  Hit(23);
  __imp__sub_826A0D88(ctx, base);
}

extern "C" REX_FUNC(sub_826A18C0) {
  Hit(24);
  __imp__sub_826A18C0(ctx, base);
}

#endif  // WWE12_DEV_PROBES
