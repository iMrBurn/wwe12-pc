// Dev-only experiment: compiled only with -DWWE12_DEV_PROBES (off by default, so releases carry no hooks).
#if defined(WWE12_DEV_PROBES)

// Probe 2 for the belt-at-60-fps bug: counts calls of the functions that read the frame-rate
// fields +60 (60/fps) and +32 (fps as float), logged every 2 s while wwe12_dtprobe2 is on.

#include <atomic>
#include <chrono>
#include <string>
#include <vector>
#include <sstream>
#include <mutex>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_dtprobe2, false, "WWE12",
                    "Debug: log how often the functions reading frame-rate fields +60/+32 run.");

REXCVAR_DEFINE_STRING(wwe12_spoof30, "", "WWE12",
                      "Debug: comma-separated sub_ names of dtprobe2 whose calls see the 30 fps frame-rate values.");

REXCVAR_DEFINE_DOUBLE(wwe12_spoof_f60, 0.0, "WWE12",
                      "Debug: if > 0, wwe12_spoof30 functions only see this value in field +60 (60/fps).");

namespace {
struct Probe { const char *name; const char *field; std::atomic<u32> n; };
Probe g_probes[] = {
  {"sub_821AD2B8", "32", {0}},
  {"sub_821B8630", "60", {0}},
  {"sub_821CE5D0", "32", {0}},
  {"sub_821CE930", "32", {0}},
  {"sub_821D0C20", "60+32", {0}},
  {"sub_821D4B78", "32", {0}},
  {"sub_821E2238", "32", {0}},
  {"sub_821ED6D8", "32", {0}},
  {"sub_821EE1B8", "32", {0}},
  {"sub_822273B8", "32", {0}},
  {"sub_82227A58", "32", {0}},
  {"sub_82229DE8", "60", {0}},
  {"sub_82230B50", "32", {0}},
  {"sub_82232190", "32", {0}},
  {"sub_82237AE8", "32", {0}},
  {"sub_82237E38", "60", {0}},
  {"sub_82238BB8", "32", {0}},
  {"sub_8223D9B0", "32", {0}},
  {"sub_8223DA10", "32", {0}},
  {"sub_8223F878", "32", {0}},
  {"sub_82247C08", "32", {0}},
  {"sub_82247F38", "32", {0}},
  {"sub_822482E0", "32", {0}},
  {"sub_82248368", "32", {0}},
  {"sub_82249EA8", "32", {0}},
  {"sub_82249FE0", "32", {0}},
  {"sub_8224ABF0", "32", {0}},
  {"sub_8224AE18", "32", {0}},
  {"sub_8224BB58", "60", {0}},
  {"sub_82256388", "60+32", {0}},
  {"sub_82256BF8", "60+32", {0}},
  {"sub_8225A808", "32", {0}},
  {"sub_8225E5F0", "32", {0}},
  {"sub_8225E828", "32", {0}},
  {"sub_8225EA60", "32", {0}},
  {"sub_82264690", "60", {0}},
  {"sub_82266188", "32", {0}},
  {"sub_82267AE8", "32", {0}},
  {"sub_8226B3E0", "60", {0}},
  {"sub_82271CD8", "60", {0}},
  {"sub_82272578", "60", {0}},
  {"sub_82273758", "60", {0}},
  {"sub_82276D30", "32", {0}},
  {"sub_8227AD40", "32", {0}},
  {"sub_8227B5F8", "60", {0}},
  {"sub_8227C1B0", "32", {0}},
  {"sub_8227CB40", "32", {0}},
  {"sub_8227CBC8", "32", {0}},
  {"sub_822A1290", "32", {0}},
  {"sub_822AB020", "60", {0}},
  {"sub_822B2E60", "32", {0}},
  {"sub_822C24E0", "60", {0}},
  {"sub_822D4F80", "60", {0}},
  {"sub_822D6120", "60", {0}},
  {"sub_822EACA0", "32", {0}},
  {"sub_822F2998", "60", {0}},
  {"sub_822F4340", "60", {0}},
  {"sub_822F7A90", "60", {0}},
  {"sub_822F8CC8", "60", {0}},
  {"sub_822FB0C8", "60", {0}},
  {"sub_822FBD80", "60", {0}},
  {"sub_822FC398", "60", {0}},
  {"sub_822FD708", "60", {0}},
  {"sub_822FE3B0", "60", {0}},
  {"sub_823008F0", "60", {0}},
  {"sub_82306950", "60", {0}},
  {"sub_8230F718", "32", {0}},
  {"sub_8230FDF8", "60", {0}},
  {"sub_82313FF8", "60", {0}},
  {"sub_82314BB8", "60", {0}},
  {"sub_82317CA8", "32", {0}},
  {"sub_82318768", "32", {0}},
  {"sub_8231B3F8", "60", {0}},
  {"sub_82321990", "60", {0}},
  {"sub_82321F80", "60", {0}},
  {"sub_82322FB0", "60", {0}},
  {"sub_823260C8", "32", {0}},
  {"sub_823266F0", "60", {0}},
  {"sub_82326B38", "32", {0}},
  {"sub_82329710", "32", {0}},
  {"sub_8232A650", "60", {0}},
  {"sub_8232D2D8", "32", {0}},
  {"sub_8232D380", "60", {0}},
  {"sub_8232E7F8", "32", {0}},
  {"sub_82330C70", "60", {0}},
  {"sub_82333FB0", "32", {0}},
  {"sub_82335080", "60", {0}},
  {"sub_823358C8", "60+32", {0}},
  {"sub_82336DC8", "32", {0}},
  {"sub_8233D250", "32", {0}},
  {"sub_823409E0", "32", {0}},
  {"sub_82342258", "32", {0}},
  {"sub_82342748", "60", {0}},
  {"sub_823451D8", "60", {0}},
  {"sub_82345650", "60", {0}},
  {"sub_82345D80", "60", {0}},
  {"sub_82346498", "60", {0}},
  {"sub_823467C8", "32", {0}},
  {"sub_82346A60", "60", {0}},
  {"sub_82346EA8", "60", {0}},
  {"sub_82347178", "32", {0}},
  {"sub_823474D8", "60", {0}},
  {"sub_823476F0", "60", {0}},
  {"sub_82347970", "60", {0}},
  {"sub_82347DD0", "60", {0}},
  {"sub_82348860", "60", {0}},
  {"sub_8234DD70", "60", {0}},
  {"sub_8234FD30", "32", {0}},
  {"sub_82354460", "60", {0}},
  {"sub_82356E98", "60", {0}},
  {"sub_82356EF8", "60", {0}},
  {"sub_82356F60", "60", {0}},
  {"sub_823604F8", "60", {0}},
  {"sub_82360780", "60", {0}},
  {"sub_823609B8", "60", {0}},
  {"sub_82360C10", "60", {0}},
  {"sub_823657B8", "60", {0}},
  {"sub_82366468", "32", {0}},
  {"sub_823670C0", "60", {0}},
  {"sub_8236AE90", "60", {0}},
  {"sub_8236B528", "60", {0}},
  {"sub_8236C038", "60", {0}},
  {"sub_8236C7D0", "60", {0}},
  {"sub_8236D930", "60", {0}},
  {"sub_82378C10", "32", {0}},
  {"sub_8237F630", "60", {0}},
  {"sub_823841B0", "32", {0}},
  {"sub_823A3D20", "60", {0}},
  {"sub_823A4E48", "60", {0}},
  {"sub_823ADE60", "60", {0}},
  {"sub_823AF558", "60", {0}},
  {"sub_823AFFA0", "60", {0}},
  {"sub_823B2368", "60", {0}},
  {"sub_823B5340", "60", {0}},
  {"sub_823C5B40", "60", {0}},
  {"sub_823C97B0", "32", {0}},
  {"sub_823CA3C8", "32", {0}},
  {"sub_823CC7A0", "60", {0}},
  {"sub_823CF848", "60", {0}},
  {"sub_823CFD38", "60", {0}},
  {"sub_823CFE18", "60", {0}},
  {"sub_823D8388", "60", {0}},
  {"sub_823D8470", "60", {0}},
  {"sub_82400CD0", "32", {0}},
  {"sub_824025A8", "32", {0}},
  {"sub_82402CF8", "32", {0}},
  {"sub_82403018", "32", {0}},
  {"sub_82405068", "60", {0}},
  {"sub_82406168", "60", {0}},
  {"sub_8240D460", "60", {0}},
  {"sub_82411810", "60", {0}},
  {"sub_82415740", "60", {0}},
  {"sub_82418110", "60", {0}},
  {"sub_8241DB00", "32", {0}},
  {"sub_8241FB20", "32", {0}},
  {"sub_8242B748", "32", {0}},
  {"sub_8242BA80", "32", {0}},
  {"sub_8242CF78", "32", {0}},
  {"sub_8245AA28", "60", {0}},
  {"sub_82465E30", "60", {0}},
  {"sub_824665B0", "60", {0}},
  {"sub_8246A020", "60", {0}},
  {"sub_82479990", "60", {0}},
  {"sub_82479C48", "60", {0}},
  {"sub_82481320", "60", {0}},
  {"sub_82481C78", "60", {0}},
  {"sub_82486080", "60", {0}},
  {"sub_824919B0", "60", {0}},
  {"sub_82493D98", "60", {0}},
  {"sub_82494628", "60", {0}},
  {"sub_82496C28", "60", {0}},
  {"sub_824999F8", "60", {0}},
  {"sub_824A5118", "60", {0}},
  {"sub_824B6270", "60", {0}},
  {"sub_824B9F80", "60", {0}},
  {"sub_824BAAD8", "60", {0}},
  {"sub_824C3180", "60", {0}},
  {"sub_824C3438", "60", {0}},
  {"sub_824C3578", "60", {0}},
  {"sub_824C3FB0", "60", {0}},
  {"sub_824CACB8", "60", {0}},
  {"sub_824D6568", "60", {0}},
  {"sub_824E9DB8", "32", {0}},
  {"sub_824F2E70", "32", {0}},
  {"sub_824F4E90", "32", {0}},
  {"sub_824F4EE0", "32", {0}},
  {"sub_824F5358", "32", {0}},
  {"sub_824F7878", "32", {0}},
  {"sub_824FC158", "32", {0}},
  {"sub_824FD7B0", "32", {0}},
  {"sub_82501CC0", "32", {0}},
  {"sub_82508BE8", "32", {0}},
  {"sub_8250E7F8", "60", {0}},
  {"sub_8250F8B8", "32", {0}},
  {"sub_82516F50", "32", {0}},
  {"sub_8251B9C0", "60", {0}},
  {"sub_8251BAA0", "60", {0}},
  {"sub_8251C6C0", "32", {0}},
  {"sub_8251C718", "32", {0}},
  {"sub_8251D230", "60", {0}},
  {"sub_8251F5C8", "32", {0}},
  {"sub_825257C0", "32", {0}},
  {"sub_825261E0", "60", {0}},
  {"sub_8252BEC8", "32", {0}},
  {"sub_8253C3B0", "60", {0}},
  {"sub_8253C3D0", "60", {0}},
  {"sub_8253C6F8", "60", {0}},
  {"sub_8253FDC8", "60", {0}},
  {"sub_82540260", "60", {0}},
  {"sub_82540F80", "60", {0}},
  {"sub_8254C230", "32", {0}},
  {"sub_825649F0", "60", {0}},
  {"sub_8256A300", "60", {0}},
  {"sub_8259AE08", "60", {0}},
  {"sub_8259AFC0", "60", {0}},
  {"sub_825A53D0", "60", {0}},
  {"sub_8266E6B0", "60", {0}},
  {"sub_8266EA90", "60", {0}},
  {"sub_8266F708", "60", {0}},
  {"sub_82675080", "60", {0}},
  {"sub_82675300", "60", {0}},
  {"sub_8267AB50", "60", {0}},
  {"sub_826A9570", "32", {0}},
  {"sub_826BCA38", "60", {0}},
  {"sub_82762698", "60", {0}},
  {"sub_82777420", "60", {0}},
  {"sub_827CF0A0", "32", {0}},
  {"sub_82884078", "32", {0}},
  {"sub_828B3EF8", "60", {0}},
  {"sub_828D42D8", "60", {0}},
  {"sub_828D9BA0", "60", {0}},
  {"sub_828DAAF0", "60", {0}},
  {"sub_828DAE50", "60", {0}},
  {"sub_828DAF98", "60", {0}},
  {"sub_828DB080", "60", {0}},
  {"sub_828DB1A0", "60", {0}},
  {"sub_828DB480", "60", {0}},
  {"sub_828DC570", "60", {0}},
  {"sub_828ED1C8", "32", {0}},
  {"sub_828ED408", "32", {0}},
  {"sub_828EFC40", "60", {0}},
  {"sub_828F11B8", "60", {0}},
  {"sub_828FAD88", "60", {0}},
  {"sub_8296E500", "60", {0}},
  {"sub_8296E578", "60", {0}},
  {"sub_829840D0", "60", {0}},
  {"sub_829841A0", "60", {0}},
  {"sub_829848B8", "60", {0}},
  {"sub_8298BCC0", "60", {0}},
  {"sub_8298DAD8", "60", {0}},
  {"sub_829987D8", "60", {0}},
  {"sub_82998988", "60", {0}},
  {"sub_82998BF8", "60", {0}},
  {"sub_829A1B48", "60", {0}},
  {"sub_829A1CF8", "60", {0}},
  {"sub_829A1F50", "60", {0}},
  {"sub_829A3CE0", "60", {0}},
  {"sub_829A3E28", "60", {0}},
  {"sub_829A3F60", "60", {0}},
  {"sub_829A4090", "60", {0}},
  {"sub_829A4230", "60", {0}},
  {"sub_829A43B8", "60", {0}},
  {"sub_829A9278", "60", {0}},
  {"sub_829B3258", "60", {0}},
  {"sub_829B3378", "60", {0}},
  {"sub_829B68B0", "60", {0}},
  {"sub_829EECA0", "60", {0}},
  {"sub_82A01E18", "60", {0}},
  {"sub_82A01F08", "60", {0}},
  {"sub_82A111C0", "60", {0}},
  {"sub_82A180B0", "60", {0}},
};

constexpr u32 kFps = 0x8324CE28;
bool IsSpoofed(size_t i) {
  static std::once_flag once;
  static std::vector<char> on;
  std::call_once(once, [] {
    on.assign(sizeof(g_probes) / sizeof(g_probes[0]), 0);
    std::stringstream ss(REXCVAR_GET(wwe12_spoof30));
    std::string tok;
    while (std::getline(ss, tok, ',')) {
      for (size_t k = 0; k < on.size(); ++k)
        if (tok == g_probes[k].name) {
          on[k] = 1;
          BD_INFO("[spoof30] armed {}", tok);
        }
    }
  });
  return on[i] != 0;
}

std::atomic<u32> g_spoofed_calls{0};
void ReportSpoof() {
  static std::atomic<i64> s_last{0};
  const i64 now = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now().time_since_epoch()).count();
  i64 last = s_last.load(std::memory_order_relaxed);
  if (now - last < 2000 || !s_last.compare_exchange_strong(last, now))
    return;
  const u32 c = g_spoofed_calls.exchange(0, std::memory_order_relaxed);
  if (c)
    BD_INFO("[spoof30] {} spoofed calls in the last 2 s", c);
}

void Report() {
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
    BD_INFO("[dtprobe2]{}", line);
}

// Counts the call and, for functions named in wwe12_spoof30, shows them the 30 fps values of the
// frame-rate struct (+8 fps, +28 dt, +32 fps float, +36 ms, +40 ms int, +60 60/fps, +64 fps/60)
// for the duration of the call.
struct Guard {
  bool spoofed = false;
  u32 i8 = 0, i40 = 0;
  float f28 = 0, f32 = 0, f36 = 0, f60 = 0, f64 = 0;
  explicit Guard(size_t i) {
    if (REXCVAR_GET(wwe12_dtprobe2)) {
      g_probes[i].n.fetch_add(1, std::memory_order_relaxed);
      Report();
    }
    if (REXCVAR_GET(wwe12_spoof30).empty() || !IsSpoofed(i))
      return;
    spoofed = true;
    i8 = bd::mem::load<u32>(kFps + 8); i40 = bd::mem::load<u32>(kFps + 40);
    f28 = bd::mem::load<float>(kFps + 28); f32 = bd::mem::load<float>(kFps + 32);
    f36 = bd::mem::load<float>(kFps + 36); f60 = bd::mem::load<float>(kFps + 60);
    f64 = bd::mem::load<float>(kFps + 64);
    if (i8 != 60)
      return;  // only meaningful in the 60 fps mode
    g_spoofed_calls.fetch_add(1, std::memory_order_relaxed);
    ReportSpoof();
    if (REXCVAR_GET(wwe12_spoof_f60) > 0.0) {
      bd::mem::store<float>(kFps + 60, float(REXCVAR_GET(wwe12_spoof_f60)));
      return;
    }
    bd::mem::store<u32>(kFps + 8, 30u); bd::mem::store<u32>(kFps + 40, 33u);
    bd::mem::store<float>(kFps + 28, 1.0f / 30.0f); bd::mem::store<float>(kFps + 32, 30.0f);
    bd::mem::store<float>(kFps + 36, 1000.0f / 30.0f); bd::mem::store<float>(kFps + 60, 2.0f);
    bd::mem::store<float>(kFps + 64, 0.5f);
  }
  ~Guard() {
    if (!spoofed || i8 != 60)
      return;
    if (REXCVAR_GET(wwe12_spoof_f60) > 0.0) {
      bd::mem::store<float>(kFps + 60, f60);
      return;
    }
    bd::mem::store<u32>(kFps + 8, i8); bd::mem::store<u32>(kFps + 40, i40);
    bd::mem::store<float>(kFps + 28, f28); bd::mem::store<float>(kFps + 32, f32);
    bd::mem::store<float>(kFps + 36, f36); bd::mem::store<float>(kFps + 60, f60);
    bd::mem::store<float>(kFps + 64, f64);
  }
};
}  // namespace

REX_EXTERN(__imp__sub_821AD2B8);
REX_EXTERN(__imp__sub_821B8630);
REX_EXTERN(__imp__sub_821CE5D0);
REX_EXTERN(__imp__sub_821CE930);
REX_EXTERN(__imp__sub_821D0C20);
REX_EXTERN(__imp__sub_821D4B78);
REX_EXTERN(__imp__sub_821E2238);
REX_EXTERN(__imp__sub_821ED6D8);
REX_EXTERN(__imp__sub_821EE1B8);
REX_EXTERN(__imp__sub_822273B8);
REX_EXTERN(__imp__sub_82227A58);
REX_EXTERN(__imp__sub_82229DE8);
REX_EXTERN(__imp__sub_82230B50);
REX_EXTERN(__imp__sub_82232190);
REX_EXTERN(__imp__sub_82237AE8);
REX_EXTERN(__imp__sub_82237E38);
REX_EXTERN(__imp__sub_82238BB8);
REX_EXTERN(__imp__sub_8223D9B0);
REX_EXTERN(__imp__sub_8223DA10);
REX_EXTERN(__imp__sub_8223F878);
REX_EXTERN(__imp__sub_82247C08);
REX_EXTERN(__imp__sub_82247F38);
REX_EXTERN(__imp__sub_822482E0);
REX_EXTERN(__imp__sub_82248368);
REX_EXTERN(__imp__sub_82249EA8);
REX_EXTERN(__imp__sub_82249FE0);
REX_EXTERN(__imp__sub_8224ABF0);
REX_EXTERN(__imp__sub_8224AE18);
REX_EXTERN(__imp__sub_8224BB58);
REX_EXTERN(__imp__sub_82256388);
REX_EXTERN(__imp__sub_82256BF8);
REX_EXTERN(__imp__sub_8225A808);
REX_EXTERN(__imp__sub_8225E5F0);
REX_EXTERN(__imp__sub_8225E828);
REX_EXTERN(__imp__sub_8225EA60);
REX_EXTERN(__imp__sub_82264690);
REX_EXTERN(__imp__sub_82266188);
REX_EXTERN(__imp__sub_82267AE8);
REX_EXTERN(__imp__sub_8226B3E0);
REX_EXTERN(__imp__sub_82271CD8);
REX_EXTERN(__imp__sub_82272578);
REX_EXTERN(__imp__sub_82273758);
REX_EXTERN(__imp__sub_82276D30);
REX_EXTERN(__imp__sub_8227AD40);
REX_EXTERN(__imp__sub_8227B5F8);
REX_EXTERN(__imp__sub_8227C1B0);
REX_EXTERN(__imp__sub_8227CB40);
REX_EXTERN(__imp__sub_8227CBC8);
REX_EXTERN(__imp__sub_822A1290);
REX_EXTERN(__imp__sub_822AB020);
REX_EXTERN(__imp__sub_822B2E60);
REX_EXTERN(__imp__sub_822C24E0);
REX_EXTERN(__imp__sub_822D4F80);
REX_EXTERN(__imp__sub_822D6120);
REX_EXTERN(__imp__sub_822EACA0);
REX_EXTERN(__imp__sub_822F2998);
REX_EXTERN(__imp__sub_822F4340);
REX_EXTERN(__imp__sub_822F7A90);
REX_EXTERN(__imp__sub_822F8CC8);
REX_EXTERN(__imp__sub_822FB0C8);
REX_EXTERN(__imp__sub_822FBD80);
REX_EXTERN(__imp__sub_822FC398);
REX_EXTERN(__imp__sub_822FD708);
REX_EXTERN(__imp__sub_822FE3B0);
REX_EXTERN(__imp__sub_823008F0);
REX_EXTERN(__imp__sub_82306950);
REX_EXTERN(__imp__sub_8230F718);
REX_EXTERN(__imp__sub_8230FDF8);
REX_EXTERN(__imp__sub_82313FF8);
REX_EXTERN(__imp__sub_82314BB8);
REX_EXTERN(__imp__sub_82317CA8);
REX_EXTERN(__imp__sub_82318768);
REX_EXTERN(__imp__sub_8231B3F8);
REX_EXTERN(__imp__sub_82321990);
REX_EXTERN(__imp__sub_82321F80);
REX_EXTERN(__imp__sub_82322FB0);
REX_EXTERN(__imp__sub_823260C8);
REX_EXTERN(__imp__sub_823266F0);
REX_EXTERN(__imp__sub_82326B38);
REX_EXTERN(__imp__sub_82329710);
REX_EXTERN(__imp__sub_8232A650);
REX_EXTERN(__imp__sub_8232D2D8);
REX_EXTERN(__imp__sub_8232D380);
REX_EXTERN(__imp__sub_8232E7F8);
REX_EXTERN(__imp__sub_82330C70);
REX_EXTERN(__imp__sub_82333FB0);
REX_EXTERN(__imp__sub_82335080);
REX_EXTERN(__imp__sub_823358C8);
REX_EXTERN(__imp__sub_82336DC8);
REX_EXTERN(__imp__sub_8233D250);
REX_EXTERN(__imp__sub_823409E0);
REX_EXTERN(__imp__sub_82342258);
REX_EXTERN(__imp__sub_82342748);
REX_EXTERN(__imp__sub_823451D8);
REX_EXTERN(__imp__sub_82345650);
REX_EXTERN(__imp__sub_82345D80);
REX_EXTERN(__imp__sub_82346498);
REX_EXTERN(__imp__sub_823467C8);
REX_EXTERN(__imp__sub_82346A60);
REX_EXTERN(__imp__sub_82346EA8);
REX_EXTERN(__imp__sub_82347178);
REX_EXTERN(__imp__sub_823474D8);
REX_EXTERN(__imp__sub_823476F0);
REX_EXTERN(__imp__sub_82347970);
REX_EXTERN(__imp__sub_82347DD0);
REX_EXTERN(__imp__sub_82348860);
REX_EXTERN(__imp__sub_8234DD70);
REX_EXTERN(__imp__sub_8234FD30);
REX_EXTERN(__imp__sub_82354460);
REX_EXTERN(__imp__sub_82356E98);
REX_EXTERN(__imp__sub_82356EF8);
REX_EXTERN(__imp__sub_82356F60);
REX_EXTERN(__imp__sub_823604F8);
REX_EXTERN(__imp__sub_82360780);
REX_EXTERN(__imp__sub_823609B8);
REX_EXTERN(__imp__sub_82360C10);
REX_EXTERN(__imp__sub_823657B8);
REX_EXTERN(__imp__sub_82366468);
REX_EXTERN(__imp__sub_823670C0);
REX_EXTERN(__imp__sub_8236AE90);
REX_EXTERN(__imp__sub_8236B528);
REX_EXTERN(__imp__sub_8236C038);
REX_EXTERN(__imp__sub_8236C7D0);
REX_EXTERN(__imp__sub_8236D930);
REX_EXTERN(__imp__sub_82378C10);
REX_EXTERN(__imp__sub_8237F630);
REX_EXTERN(__imp__sub_823841B0);
REX_EXTERN(__imp__sub_823A3D20);
REX_EXTERN(__imp__sub_823A4E48);
REX_EXTERN(__imp__sub_823ADE60);
REX_EXTERN(__imp__sub_823AF558);
REX_EXTERN(__imp__sub_823AFFA0);
REX_EXTERN(__imp__sub_823B2368);
REX_EXTERN(__imp__sub_823B5340);
REX_EXTERN(__imp__sub_823C5B40);
REX_EXTERN(__imp__sub_823C97B0);
REX_EXTERN(__imp__sub_823CA3C8);
REX_EXTERN(__imp__sub_823CC7A0);
REX_EXTERN(__imp__sub_823CF848);
REX_EXTERN(__imp__sub_823CFD38);
REX_EXTERN(__imp__sub_823CFE18);
REX_EXTERN(__imp__sub_823D8388);
REX_EXTERN(__imp__sub_823D8470);
REX_EXTERN(__imp__sub_82400CD0);
REX_EXTERN(__imp__sub_824025A8);
REX_EXTERN(__imp__sub_82402CF8);
REX_EXTERN(__imp__sub_82403018);
REX_EXTERN(__imp__sub_82405068);
REX_EXTERN(__imp__sub_82406168);
REX_EXTERN(__imp__sub_8240D460);
REX_EXTERN(__imp__sub_82411810);
REX_EXTERN(__imp__sub_82415740);
REX_EXTERN(__imp__sub_82418110);
REX_EXTERN(__imp__sub_8241DB00);
REX_EXTERN(__imp__sub_8241FB20);
REX_EXTERN(__imp__sub_8242B748);
REX_EXTERN(__imp__sub_8242BA80);
REX_EXTERN(__imp__sub_8242CF78);
REX_EXTERN(__imp__sub_8245AA28);
REX_EXTERN(__imp__sub_82465E30);
REX_EXTERN(__imp__sub_824665B0);
REX_EXTERN(__imp__sub_8246A020);
REX_EXTERN(__imp__sub_82479990);
REX_EXTERN(__imp__sub_82479C48);
REX_EXTERN(__imp__sub_82481320);
REX_EXTERN(__imp__sub_82481C78);
REX_EXTERN(__imp__sub_82486080);
REX_EXTERN(__imp__sub_824919B0);
REX_EXTERN(__imp__sub_82493D98);
REX_EXTERN(__imp__sub_82494628);
REX_EXTERN(__imp__sub_82496C28);
REX_EXTERN(__imp__sub_824999F8);
REX_EXTERN(__imp__sub_824A5118);
REX_EXTERN(__imp__sub_824B6270);
REX_EXTERN(__imp__sub_824B9F80);
REX_EXTERN(__imp__sub_824BAAD8);
REX_EXTERN(__imp__sub_824C3180);
REX_EXTERN(__imp__sub_824C3438);
REX_EXTERN(__imp__sub_824C3578);
REX_EXTERN(__imp__sub_824C3FB0);
REX_EXTERN(__imp__sub_824CACB8);
REX_EXTERN(__imp__sub_824D6568);
REX_EXTERN(__imp__sub_824E9DB8);
REX_EXTERN(__imp__sub_824F2E70);
REX_EXTERN(__imp__sub_824F4E90);
REX_EXTERN(__imp__sub_824F4EE0);
REX_EXTERN(__imp__sub_824F5358);
REX_EXTERN(__imp__sub_824F7878);
REX_EXTERN(__imp__sub_824FC158);
REX_EXTERN(__imp__sub_824FD7B0);
REX_EXTERN(__imp__sub_82501CC0);
REX_EXTERN(__imp__sub_82508BE8);
REX_EXTERN(__imp__sub_8250E7F8);
REX_EXTERN(__imp__sub_8250F8B8);
REX_EXTERN(__imp__sub_82516F50);
REX_EXTERN(__imp__sub_8251B9C0);
REX_EXTERN(__imp__sub_8251BAA0);
REX_EXTERN(__imp__sub_8251C6C0);
REX_EXTERN(__imp__sub_8251C718);
REX_EXTERN(__imp__sub_8251D230);
REX_EXTERN(__imp__sub_8251F5C8);
REX_EXTERN(__imp__sub_825257C0);
REX_EXTERN(__imp__sub_825261E0);
REX_EXTERN(__imp__sub_8252BEC8);
REX_EXTERN(__imp__sub_8253C3B0);
REX_EXTERN(__imp__sub_8253C3D0);
REX_EXTERN(__imp__sub_8253C6F8);
REX_EXTERN(__imp__sub_8253FDC8);
REX_EXTERN(__imp__sub_82540260);
REX_EXTERN(__imp__sub_82540F80);
REX_EXTERN(__imp__sub_8254C230);
REX_EXTERN(__imp__sub_825649F0);
REX_EXTERN(__imp__sub_8256A300);
REX_EXTERN(__imp__sub_8259AE08);
REX_EXTERN(__imp__sub_8259AFC0);
REX_EXTERN(__imp__sub_825A53D0);
REX_EXTERN(__imp__sub_8266E6B0);
REX_EXTERN(__imp__sub_8266EA90);
REX_EXTERN(__imp__sub_8266F708);
REX_EXTERN(__imp__sub_82675080);
REX_EXTERN(__imp__sub_82675300);
REX_EXTERN(__imp__sub_8267AB50);
REX_EXTERN(__imp__sub_826A9570);
REX_EXTERN(__imp__sub_826BCA38);
REX_EXTERN(__imp__sub_82762698);
REX_EXTERN(__imp__sub_82777420);
REX_EXTERN(__imp__sub_827CF0A0);
REX_EXTERN(__imp__sub_82884078);
REX_EXTERN(__imp__sub_828B3EF8);
REX_EXTERN(__imp__sub_828D42D8);
REX_EXTERN(__imp__sub_828D9BA0);
REX_EXTERN(__imp__sub_828DAAF0);
REX_EXTERN(__imp__sub_828DAE50);
REX_EXTERN(__imp__sub_828DAF98);
REX_EXTERN(__imp__sub_828DB080);
REX_EXTERN(__imp__sub_828DB1A0);
REX_EXTERN(__imp__sub_828DB480);
REX_EXTERN(__imp__sub_828DC570);
REX_EXTERN(__imp__sub_828ED1C8);
REX_EXTERN(__imp__sub_828ED408);
REX_EXTERN(__imp__sub_828EFC40);
REX_EXTERN(__imp__sub_828F11B8);
REX_EXTERN(__imp__sub_828FAD88);
REX_EXTERN(__imp__sub_8296E500);
REX_EXTERN(__imp__sub_8296E578);
REX_EXTERN(__imp__sub_829840D0);
REX_EXTERN(__imp__sub_829841A0);
REX_EXTERN(__imp__sub_829848B8);
REX_EXTERN(__imp__sub_8298BCC0);
REX_EXTERN(__imp__sub_8298DAD8);
REX_EXTERN(__imp__sub_829987D8);
REX_EXTERN(__imp__sub_82998988);
REX_EXTERN(__imp__sub_82998BF8);
REX_EXTERN(__imp__sub_829A1B48);
REX_EXTERN(__imp__sub_829A1CF8);
REX_EXTERN(__imp__sub_829A1F50);
REX_EXTERN(__imp__sub_829A3CE0);
REX_EXTERN(__imp__sub_829A3E28);
REX_EXTERN(__imp__sub_829A3F60);
REX_EXTERN(__imp__sub_829A4090);
REX_EXTERN(__imp__sub_829A4230);
REX_EXTERN(__imp__sub_829A43B8);
REX_EXTERN(__imp__sub_829A9278);
REX_EXTERN(__imp__sub_829B3258);
REX_EXTERN(__imp__sub_829B3378);
REX_EXTERN(__imp__sub_829B68B0);
REX_EXTERN(__imp__sub_829EECA0);
REX_EXTERN(__imp__sub_82A01E18);
REX_EXTERN(__imp__sub_82A01F08);
REX_EXTERN(__imp__sub_82A111C0);
REX_EXTERN(__imp__sub_82A180B0);

extern "C" REX_FUNC(sub_821AD2B8) {
  Guard g(0);
  __imp__sub_821AD2B8(ctx, base);
}

extern "C" REX_FUNC(sub_821B8630) {
  Guard g(1);
  __imp__sub_821B8630(ctx, base);
}

extern "C" REX_FUNC(sub_821CE5D0) {
  Guard g(2);
  __imp__sub_821CE5D0(ctx, base);
}

extern "C" REX_FUNC(sub_821CE930) {
  Guard g(3);
  __imp__sub_821CE930(ctx, base);
}

extern "C" REX_FUNC(sub_821D0C20) {
  Guard g(4);
  __imp__sub_821D0C20(ctx, base);
}

extern "C" REX_FUNC(sub_821D4B78) {
  Guard g(5);
  __imp__sub_821D4B78(ctx, base);
}

extern "C" REX_FUNC(sub_821E2238) {
  Guard g(6);
  __imp__sub_821E2238(ctx, base);
}

extern "C" REX_FUNC(sub_821ED6D8) {
  Guard g(7);
  __imp__sub_821ED6D8(ctx, base);
}

extern "C" REX_FUNC(sub_821EE1B8) {
  Guard g(8);
  __imp__sub_821EE1B8(ctx, base);
}

extern "C" REX_FUNC(sub_822273B8) {
  Guard g(9);
  __imp__sub_822273B8(ctx, base);
}

extern "C" REX_FUNC(sub_82227A58) {
  Guard g(10);
  __imp__sub_82227A58(ctx, base);
}

extern "C" REX_FUNC(sub_82229DE8) {
  Guard g(11);
  __imp__sub_82229DE8(ctx, base);
}

extern "C" REX_FUNC(sub_82230B50) {
  Guard g(12);
  __imp__sub_82230B50(ctx, base);
}

extern "C" REX_FUNC(sub_82232190) {
  Guard g(13);
  __imp__sub_82232190(ctx, base);
}

extern "C" REX_FUNC(sub_82237AE8) {
  Guard g(14);
  __imp__sub_82237AE8(ctx, base);
}

extern "C" REX_FUNC(sub_82237E38) {
  Guard g(15);
  __imp__sub_82237E38(ctx, base);
}

extern "C" REX_FUNC(sub_82238BB8) {
  Guard g(16);
  __imp__sub_82238BB8(ctx, base);
}

extern "C" REX_FUNC(sub_8223D9B0) {
  Guard g(17);
  __imp__sub_8223D9B0(ctx, base);
}

extern "C" REX_FUNC(sub_8223DA10) {
  Guard g(18);
  __imp__sub_8223DA10(ctx, base);
}

extern "C" REX_FUNC(sub_8223F878) {
  Guard g(19);
  __imp__sub_8223F878(ctx, base);
}

extern "C" REX_FUNC(sub_82247C08) {
  Guard g(20);
  __imp__sub_82247C08(ctx, base);
}

extern "C" REX_FUNC(sub_82247F38) {
  Guard g(21);
  __imp__sub_82247F38(ctx, base);
}

extern "C" REX_FUNC(sub_822482E0) {
  Guard g(22);
  __imp__sub_822482E0(ctx, base);
}

extern "C" REX_FUNC(sub_82248368) {
  Guard g(23);
  __imp__sub_82248368(ctx, base);
}

extern "C" REX_FUNC(sub_82249EA8) {
  Guard g(24);
  __imp__sub_82249EA8(ctx, base);
}

extern "C" REX_FUNC(sub_82249FE0) {
  Guard g(25);
  __imp__sub_82249FE0(ctx, base);
}

extern "C" REX_FUNC(sub_8224ABF0) {
  Guard g(26);
  __imp__sub_8224ABF0(ctx, base);
}

extern "C" REX_FUNC(sub_8224AE18) {
  Guard g(27);
  __imp__sub_8224AE18(ctx, base);
}

extern "C" REX_FUNC(sub_8224BB58) {
  Guard g(28);
  __imp__sub_8224BB58(ctx, base);
}

extern "C" REX_FUNC(sub_82256388) {
  Guard g(29);
  __imp__sub_82256388(ctx, base);
}

extern "C" REX_FUNC(sub_82256BF8) {
  Guard g(30);
  __imp__sub_82256BF8(ctx, base);
}

extern "C" REX_FUNC(sub_8225A808) {
  Guard g(31);
  __imp__sub_8225A808(ctx, base);
}

extern "C" REX_FUNC(sub_8225E5F0) {
  Guard g(32);
  __imp__sub_8225E5F0(ctx, base);
}

extern "C" REX_FUNC(sub_8225E828) {
  Guard g(33);
  __imp__sub_8225E828(ctx, base);
}

extern "C" REX_FUNC(sub_8225EA60) {
  Guard g(34);
  __imp__sub_8225EA60(ctx, base);
}

extern "C" REX_FUNC(sub_82264690) {
  Guard g(35);
  __imp__sub_82264690(ctx, base);
}

extern "C" REX_FUNC(sub_82266188) {
  Guard g(36);
  __imp__sub_82266188(ctx, base);
}

extern "C" REX_FUNC(sub_82267AE8) {
  Guard g(37);
  __imp__sub_82267AE8(ctx, base);
}

extern "C" REX_FUNC(sub_8226B3E0) {
  Guard g(38);
  __imp__sub_8226B3E0(ctx, base);
}

extern "C" REX_FUNC(sub_82271CD8) {
  Guard g(39);
  __imp__sub_82271CD8(ctx, base);
}

extern "C" REX_FUNC(sub_82272578) {
  Guard g(40);
  __imp__sub_82272578(ctx, base);
}

extern "C" REX_FUNC(sub_82273758) {
  Guard g(41);
  __imp__sub_82273758(ctx, base);
}

extern "C" REX_FUNC(sub_82276D30) {
  Guard g(42);
  __imp__sub_82276D30(ctx, base);
}

extern "C" REX_FUNC(sub_8227AD40) {
  Guard g(43);
  __imp__sub_8227AD40(ctx, base);
}

extern "C" REX_FUNC(sub_8227B5F8) {
  Guard g(44);
  __imp__sub_8227B5F8(ctx, base);
}

extern "C" REX_FUNC(sub_8227C1B0) {
  Guard g(45);
  __imp__sub_8227C1B0(ctx, base);
}

extern "C" REX_FUNC(sub_8227CB40) {
  Guard g(46);
  __imp__sub_8227CB40(ctx, base);
}

extern "C" REX_FUNC(sub_8227CBC8) {
  Guard g(47);
  __imp__sub_8227CBC8(ctx, base);
}

extern "C" REX_FUNC(sub_822A1290) {
  Guard g(48);
  __imp__sub_822A1290(ctx, base);
}

extern "C" REX_FUNC(sub_822AB020) {
  Guard g(49);
  __imp__sub_822AB020(ctx, base);
}

extern "C" REX_FUNC(sub_822B2E60) {
  Guard g(50);
  __imp__sub_822B2E60(ctx, base);
}

extern "C" REX_FUNC(sub_822C24E0) {
  Guard g(51);
  __imp__sub_822C24E0(ctx, base);
}

extern "C" REX_FUNC(sub_822D4F80) {
  Guard g(52);
  __imp__sub_822D4F80(ctx, base);
}

extern "C" REX_FUNC(sub_822D6120) {
  Guard g(53);
  __imp__sub_822D6120(ctx, base);
}

extern "C" REX_FUNC(sub_822EACA0) {
  Guard g(54);
  __imp__sub_822EACA0(ctx, base);
}

extern "C" REX_FUNC(sub_822F2998) {
  Guard g(55);
  __imp__sub_822F2998(ctx, base);
}

extern "C" REX_FUNC(sub_822F4340) {
  Guard g(56);
  __imp__sub_822F4340(ctx, base);
}

extern "C" REX_FUNC(sub_822F7A90) {
  Guard g(57);
  __imp__sub_822F7A90(ctx, base);
}

extern "C" REX_FUNC(sub_822F8CC8) {
  Guard g(58);
  __imp__sub_822F8CC8(ctx, base);
}

extern "C" REX_FUNC(sub_822FB0C8) {
  Guard g(59);
  __imp__sub_822FB0C8(ctx, base);
}

extern "C" REX_FUNC(sub_822FBD80) {
  Guard g(60);
  __imp__sub_822FBD80(ctx, base);
}

extern "C" REX_FUNC(sub_822FC398) {
  Guard g(61);
  __imp__sub_822FC398(ctx, base);
}

extern "C" REX_FUNC(sub_822FD708) {
  Guard g(62);
  __imp__sub_822FD708(ctx, base);
}

extern "C" REX_FUNC(sub_822FE3B0) {
  Guard g(63);
  __imp__sub_822FE3B0(ctx, base);
}

extern "C" REX_FUNC(sub_823008F0) {
  Guard g(64);
  __imp__sub_823008F0(ctx, base);
}

extern "C" REX_FUNC(sub_82306950) {
  Guard g(65);
  __imp__sub_82306950(ctx, base);
}

extern "C" REX_FUNC(sub_8230F718) {
  Guard g(66);
  __imp__sub_8230F718(ctx, base);
}

extern "C" REX_FUNC(sub_8230FDF8) {
  Guard g(67);
  __imp__sub_8230FDF8(ctx, base);
}

extern "C" REX_FUNC(sub_82313FF8) {
  Guard g(68);
  __imp__sub_82313FF8(ctx, base);
}

extern "C" REX_FUNC(sub_82314BB8) {
  Guard g(69);
  __imp__sub_82314BB8(ctx, base);
}

extern "C" REX_FUNC(sub_82317CA8) {
  Guard g(70);
  __imp__sub_82317CA8(ctx, base);
}

extern "C" REX_FUNC(sub_82318768) {
  Guard g(71);
  __imp__sub_82318768(ctx, base);
}

extern "C" REX_FUNC(sub_8231B3F8) {
  Guard g(72);
  __imp__sub_8231B3F8(ctx, base);
}

extern "C" REX_FUNC(sub_82321990) {
  Guard g(73);
  __imp__sub_82321990(ctx, base);
}

extern "C" REX_FUNC(sub_82321F80) {
  Guard g(74);
  __imp__sub_82321F80(ctx, base);
}

extern "C" REX_FUNC(sub_82322FB0) {
  Guard g(75);
  __imp__sub_82322FB0(ctx, base);
}

extern "C" REX_FUNC(sub_823260C8) {
  Guard g(76);
  __imp__sub_823260C8(ctx, base);
}

extern "C" REX_FUNC(sub_823266F0) {
  Guard g(77);
  __imp__sub_823266F0(ctx, base);
}

extern "C" REX_FUNC(sub_82326B38) {
  Guard g(78);
  __imp__sub_82326B38(ctx, base);
}

extern "C" REX_FUNC(sub_82329710) {
  Guard g(79);
  __imp__sub_82329710(ctx, base);
}

extern "C" REX_FUNC(sub_8232A650) {
  Guard g(80);
  __imp__sub_8232A650(ctx, base);
}

extern "C" REX_FUNC(sub_8232D2D8) {
  Guard g(81);
  __imp__sub_8232D2D8(ctx, base);
}

extern "C" REX_FUNC(sub_8232D380) {
  Guard g(82);
  __imp__sub_8232D380(ctx, base);
}

extern "C" REX_FUNC(sub_8232E7F8) {
  Guard g(83);
  __imp__sub_8232E7F8(ctx, base);
}

extern "C" REX_FUNC(sub_82330C70) {
  Guard g(84);
  __imp__sub_82330C70(ctx, base);
}

extern "C" REX_FUNC(sub_82333FB0) {
  Guard g(85);
  __imp__sub_82333FB0(ctx, base);
}

extern "C" REX_FUNC(sub_82335080) {
  Guard g(86);
  __imp__sub_82335080(ctx, base);
}

extern "C" REX_FUNC(sub_823358C8) {
  Guard g(87);
  __imp__sub_823358C8(ctx, base);
}

extern "C" REX_FUNC(sub_82336DC8) {
  Guard g(88);
  __imp__sub_82336DC8(ctx, base);
}

extern "C" REX_FUNC(sub_8233D250) {
  Guard g(89);
  __imp__sub_8233D250(ctx, base);
}

extern "C" REX_FUNC(sub_823409E0) {
  Guard g(90);
  __imp__sub_823409E0(ctx, base);
}

extern "C" REX_FUNC(sub_82342258) {
  Guard g(91);
  __imp__sub_82342258(ctx, base);
}

extern "C" REX_FUNC(sub_82342748) {
  Guard g(92);
  __imp__sub_82342748(ctx, base);
}

extern "C" REX_FUNC(sub_823451D8) {
  Guard g(93);
  __imp__sub_823451D8(ctx, base);
}

extern "C" REX_FUNC(sub_82345650) {
  Guard g(94);
  __imp__sub_82345650(ctx, base);
}

extern "C" REX_FUNC(sub_82345D80) {
  Guard g(95);
  __imp__sub_82345D80(ctx, base);
}

extern "C" REX_FUNC(sub_82346498) {
  Guard g(96);
  __imp__sub_82346498(ctx, base);
}

extern "C" REX_FUNC(sub_823467C8) {
  Guard g(97);
  __imp__sub_823467C8(ctx, base);
}

extern "C" REX_FUNC(sub_82346A60) {
  Guard g(98);
  __imp__sub_82346A60(ctx, base);
}

extern "C" REX_FUNC(sub_82346EA8) {
  Guard g(99);
  __imp__sub_82346EA8(ctx, base);
}

extern "C" REX_FUNC(sub_82347178) {
  Guard g(100);
  __imp__sub_82347178(ctx, base);
}

extern "C" REX_FUNC(sub_823474D8) {
  Guard g(101);
  __imp__sub_823474D8(ctx, base);
}

extern "C" REX_FUNC(sub_823476F0) {
  Guard g(102);
  __imp__sub_823476F0(ctx, base);
}

extern "C" REX_FUNC(sub_82347970) {
  Guard g(103);
  __imp__sub_82347970(ctx, base);
}

extern "C" REX_FUNC(sub_82347DD0) {
  Guard g(104);
  __imp__sub_82347DD0(ctx, base);
}

extern "C" REX_FUNC(sub_82348860) {
  Guard g(105);
  __imp__sub_82348860(ctx, base);
}

extern "C" REX_FUNC(sub_8234DD70) {
  Guard g(106);
  __imp__sub_8234DD70(ctx, base);
}

extern "C" REX_FUNC(sub_8234FD30) {
  Guard g(107);
  __imp__sub_8234FD30(ctx, base);
}

extern "C" REX_FUNC(sub_82354460) {
  Guard g(108);
  __imp__sub_82354460(ctx, base);
}

extern "C" REX_FUNC(sub_82356E98) {
  Guard g(109);
  __imp__sub_82356E98(ctx, base);
}

extern "C" REX_FUNC(sub_82356EF8) {
  Guard g(110);
  __imp__sub_82356EF8(ctx, base);
}

extern "C" REX_FUNC(sub_82356F60) {
  Guard g(111);
  __imp__sub_82356F60(ctx, base);
}

extern "C" REX_FUNC(sub_823604F8) {
  Guard g(112);
  __imp__sub_823604F8(ctx, base);
}

extern "C" REX_FUNC(sub_82360780) {
  Guard g(113);
  __imp__sub_82360780(ctx, base);
}

extern "C" REX_FUNC(sub_823609B8) {
  Guard g(114);
  __imp__sub_823609B8(ctx, base);
}

extern "C" REX_FUNC(sub_82360C10) {
  Guard g(115);
  __imp__sub_82360C10(ctx, base);
}

extern "C" REX_FUNC(sub_823657B8) {
  Guard g(116);
  __imp__sub_823657B8(ctx, base);
}

extern "C" REX_FUNC(sub_82366468) {
  Guard g(117);
  __imp__sub_82366468(ctx, base);
}

extern "C" REX_FUNC(sub_823670C0) {
  Guard g(118);
  __imp__sub_823670C0(ctx, base);
}

extern "C" REX_FUNC(sub_8236AE90) {
  Guard g(119);
  __imp__sub_8236AE90(ctx, base);
}

extern "C" REX_FUNC(sub_8236B528) {
  Guard g(120);
  __imp__sub_8236B528(ctx, base);
}

extern "C" REX_FUNC(sub_8236C038) {
  Guard g(121);
  __imp__sub_8236C038(ctx, base);
}

extern "C" REX_FUNC(sub_8236C7D0) {
  Guard g(122);
  __imp__sub_8236C7D0(ctx, base);
}

extern "C" REX_FUNC(sub_8236D930) {
  Guard g(123);
  __imp__sub_8236D930(ctx, base);
}

extern "C" REX_FUNC(sub_82378C10) {
  Guard g(124);
  __imp__sub_82378C10(ctx, base);
}

extern "C" REX_FUNC(sub_8237F630) {
  Guard g(125);
  __imp__sub_8237F630(ctx, base);
}

extern "C" REX_FUNC(sub_823841B0) {
  Guard g(126);
  __imp__sub_823841B0(ctx, base);
}

extern "C" REX_FUNC(sub_823A3D20) {
  Guard g(127);
  __imp__sub_823A3D20(ctx, base);
}

extern "C" REX_FUNC(sub_823A4E48) {
  Guard g(128);
  __imp__sub_823A4E48(ctx, base);
}

extern "C" REX_FUNC(sub_823ADE60) {
  Guard g(129);
  __imp__sub_823ADE60(ctx, base);
}

extern "C" REX_FUNC(sub_823AF558) {
  Guard g(130);
  __imp__sub_823AF558(ctx, base);
}

extern "C" REX_FUNC(sub_823AFFA0) {
  Guard g(131);
  __imp__sub_823AFFA0(ctx, base);
}

extern "C" REX_FUNC(sub_823B2368) {
  Guard g(132);
  __imp__sub_823B2368(ctx, base);
}

extern "C" REX_FUNC(sub_823B5340) {
  Guard g(133);
  __imp__sub_823B5340(ctx, base);
}

extern "C" REX_FUNC(sub_823C5B40) {
  Guard g(134);
  __imp__sub_823C5B40(ctx, base);
}

extern "C" REX_FUNC(sub_823C97B0) {
  Guard g(135);
  __imp__sub_823C97B0(ctx, base);
}

extern "C" REX_FUNC(sub_823CA3C8) {
  Guard g(136);
  __imp__sub_823CA3C8(ctx, base);
}

extern "C" REX_FUNC(sub_823CC7A0) {
  Guard g(137);
  __imp__sub_823CC7A0(ctx, base);
}

extern "C" REX_FUNC(sub_823CF848) {
  Guard g(138);
  __imp__sub_823CF848(ctx, base);
}

extern "C" REX_FUNC(sub_823CFD38) {
  Guard g(139);
  __imp__sub_823CFD38(ctx, base);
}

extern "C" REX_FUNC(sub_823CFE18) {
  Guard g(140);
  __imp__sub_823CFE18(ctx, base);
}

extern "C" REX_FUNC(sub_823D8388) {
  Guard g(141);
  __imp__sub_823D8388(ctx, base);
}

extern "C" REX_FUNC(sub_823D8470) {
  Guard g(142);
  __imp__sub_823D8470(ctx, base);
}

extern "C" REX_FUNC(sub_82400CD0) {
  Guard g(143);
  __imp__sub_82400CD0(ctx, base);
}

extern "C" REX_FUNC(sub_824025A8) {
  Guard g(144);
  __imp__sub_824025A8(ctx, base);
}

extern "C" REX_FUNC(sub_82402CF8) {
  Guard g(145);
  __imp__sub_82402CF8(ctx, base);
}

extern "C" REX_FUNC(sub_82403018) {
  Guard g(146);
  __imp__sub_82403018(ctx, base);
}

extern "C" REX_FUNC(sub_82405068) {
  Guard g(147);
  __imp__sub_82405068(ctx, base);
}

extern "C" REX_FUNC(sub_82406168) {
  Guard g(148);
  __imp__sub_82406168(ctx, base);
}

extern "C" REX_FUNC(sub_8240D460) {
  Guard g(149);
  __imp__sub_8240D460(ctx, base);
}

extern "C" REX_FUNC(sub_82411810) {
  Guard g(150);
  __imp__sub_82411810(ctx, base);
}

extern "C" REX_FUNC(sub_82415740) {
  Guard g(151);
  __imp__sub_82415740(ctx, base);
}

extern "C" REX_FUNC(sub_82418110) {
  Guard g(152);
  __imp__sub_82418110(ctx, base);
}

extern "C" REX_FUNC(sub_8241DB00) {
  Guard g(153);
  __imp__sub_8241DB00(ctx, base);
}

extern "C" REX_FUNC(sub_8241FB20) {
  Guard g(154);
  __imp__sub_8241FB20(ctx, base);
}

extern "C" REX_FUNC(sub_8242B748) {
  Guard g(155);
  __imp__sub_8242B748(ctx, base);
}

extern "C" REX_FUNC(sub_8242BA80) {
  Guard g(156);
  __imp__sub_8242BA80(ctx, base);
}

extern "C" REX_FUNC(sub_8242CF78) {
  Guard g(157);
  __imp__sub_8242CF78(ctx, base);
}

extern "C" REX_FUNC(sub_8245AA28) {
  Guard g(158);
  __imp__sub_8245AA28(ctx, base);
}

extern "C" REX_FUNC(sub_82465E30) {
  Guard g(159);
  __imp__sub_82465E30(ctx, base);
}

extern "C" REX_FUNC(sub_824665B0) {
  Guard g(160);
  __imp__sub_824665B0(ctx, base);
}

extern "C" REX_FUNC(sub_8246A020) {
  Guard g(161);
  __imp__sub_8246A020(ctx, base);
}

extern "C" REX_FUNC(sub_82479990) {
  Guard g(162);
  __imp__sub_82479990(ctx, base);
}

extern "C" REX_FUNC(sub_82479C48) {
  Guard g(163);
  __imp__sub_82479C48(ctx, base);
}

extern "C" REX_FUNC(sub_82481320) {
  Guard g(164);
  __imp__sub_82481320(ctx, base);
}

extern "C" REX_FUNC(sub_82481C78) {
  Guard g(165);
  __imp__sub_82481C78(ctx, base);
}

extern "C" REX_FUNC(sub_82486080) {
  Guard g(166);
  __imp__sub_82486080(ctx, base);
}

extern "C" REX_FUNC(sub_824919B0) {
  Guard g(167);
  __imp__sub_824919B0(ctx, base);
}

extern "C" REX_FUNC(sub_82493D98) {
  Guard g(168);
  __imp__sub_82493D98(ctx, base);
}

extern "C" REX_FUNC(sub_82494628) {
  Guard g(169);
  __imp__sub_82494628(ctx, base);
}

extern "C" REX_FUNC(sub_82496C28) {
  Guard g(170);
  __imp__sub_82496C28(ctx, base);
}

extern "C" REX_FUNC(sub_824999F8) {
  Guard g(171);
  __imp__sub_824999F8(ctx, base);
}

extern "C" REX_FUNC(sub_824A5118) {
  Guard g(172);
  __imp__sub_824A5118(ctx, base);
}

extern "C" REX_FUNC(sub_824B6270) {
  Guard g(173);
  __imp__sub_824B6270(ctx, base);
}

extern "C" REX_FUNC(sub_824B9F80) {
  Guard g(174);
  __imp__sub_824B9F80(ctx, base);
}

extern "C" REX_FUNC(sub_824BAAD8) {
  Guard g(175);
  __imp__sub_824BAAD8(ctx, base);
}

extern "C" REX_FUNC(sub_824C3180) {
  Guard g(176);
  __imp__sub_824C3180(ctx, base);
}

extern "C" REX_FUNC(sub_824C3438) {
  Guard g(177);
  __imp__sub_824C3438(ctx, base);
}

extern "C" REX_FUNC(sub_824C3578) {
  Guard g(178);
  __imp__sub_824C3578(ctx, base);
}

extern "C" REX_FUNC(sub_824C3FB0) {
  Guard g(179);
  __imp__sub_824C3FB0(ctx, base);
}

extern "C" REX_FUNC(sub_824CACB8) {
  Guard g(180);
  __imp__sub_824CACB8(ctx, base);
}

extern "C" REX_FUNC(sub_824D6568) {
  Guard g(181);
  __imp__sub_824D6568(ctx, base);
}

extern "C" REX_FUNC(sub_824E9DB8) {
  Guard g(182);
  __imp__sub_824E9DB8(ctx, base);
}

extern "C" REX_FUNC(sub_824F2E70) {
  Guard g(183);
  __imp__sub_824F2E70(ctx, base);
}

extern "C" REX_FUNC(sub_824F4E90) {
  Guard g(184);
  __imp__sub_824F4E90(ctx, base);
}

extern "C" REX_FUNC(sub_824F4EE0) {
  Guard g(185);
  __imp__sub_824F4EE0(ctx, base);
}

extern "C" REX_FUNC(sub_824F5358) {
  Guard g(186);
  __imp__sub_824F5358(ctx, base);
}

extern "C" REX_FUNC(sub_824F7878) {
  Guard g(187);
  __imp__sub_824F7878(ctx, base);
}

extern "C" REX_FUNC(sub_824FC158) {
  Guard g(188);
  __imp__sub_824FC158(ctx, base);
}

extern "C" REX_FUNC(sub_824FD7B0) {
  Guard g(189);
  __imp__sub_824FD7B0(ctx, base);
}

extern "C" REX_FUNC(sub_82501CC0) {
  Guard g(190);
  __imp__sub_82501CC0(ctx, base);
}

extern "C" REX_FUNC(sub_82508BE8) {
  Guard g(191);
  __imp__sub_82508BE8(ctx, base);
}

extern "C" REX_FUNC(sub_8250E7F8) {
  Guard g(192);
  __imp__sub_8250E7F8(ctx, base);
}

extern "C" REX_FUNC(sub_8250F8B8) {
  Guard g(193);
  __imp__sub_8250F8B8(ctx, base);
}

extern "C" REX_FUNC(sub_82516F50) {
  Guard g(194);
  __imp__sub_82516F50(ctx, base);
}

extern "C" REX_FUNC(sub_8251B9C0) {
  Guard g(195);
  __imp__sub_8251B9C0(ctx, base);
}

extern "C" REX_FUNC(sub_8251BAA0) {
  Guard g(196);
  __imp__sub_8251BAA0(ctx, base);
}

extern "C" REX_FUNC(sub_8251C6C0) {
  Guard g(197);
  __imp__sub_8251C6C0(ctx, base);
}

extern "C" REX_FUNC(sub_8251C718) {
  Guard g(198);
  __imp__sub_8251C718(ctx, base);
}

extern "C" REX_FUNC(sub_8251D230) {
  Guard g(199);
  __imp__sub_8251D230(ctx, base);
}

extern "C" REX_FUNC(sub_8251F5C8) {
  Guard g(200);
  __imp__sub_8251F5C8(ctx, base);
}

extern "C" REX_FUNC(sub_825257C0) {
  Guard g(201);
  __imp__sub_825257C0(ctx, base);
}

extern "C" REX_FUNC(sub_825261E0) {
  Guard g(202);
  __imp__sub_825261E0(ctx, base);
}

extern "C" REX_FUNC(sub_8252BEC8) {
  Guard g(203);
  __imp__sub_8252BEC8(ctx, base);
}

extern "C" REX_FUNC(sub_8253C3B0) {
  Guard g(204);
  __imp__sub_8253C3B0(ctx, base);
}

extern "C" REX_FUNC(sub_8253C3D0) {
  Guard g(205);
  __imp__sub_8253C3D0(ctx, base);
}

extern "C" REX_FUNC(sub_8253C6F8) {
  Guard g(206);
  __imp__sub_8253C6F8(ctx, base);
}

extern "C" REX_FUNC(sub_8253FDC8) {
  Guard g(207);
  __imp__sub_8253FDC8(ctx, base);
}

extern "C" REX_FUNC(sub_82540260) {
  Guard g(208);
  __imp__sub_82540260(ctx, base);
}

extern "C" REX_FUNC(sub_82540F80) {
  Guard g(209);
  __imp__sub_82540F80(ctx, base);
}

extern "C" REX_FUNC(sub_8254C230) {
  Guard g(210);
  __imp__sub_8254C230(ctx, base);
}

extern "C" REX_FUNC(sub_825649F0) {
  Guard g(211);
  __imp__sub_825649F0(ctx, base);
}

extern "C" REX_FUNC(sub_8256A300) {
  Guard g(212);
  __imp__sub_8256A300(ctx, base);
}

extern "C" REX_FUNC(sub_8259AE08) {
  Guard g(213);
  __imp__sub_8259AE08(ctx, base);
}

extern "C" REX_FUNC(sub_8259AFC0) {
  Guard g(214);
  __imp__sub_8259AFC0(ctx, base);
}

extern "C" REX_FUNC(sub_825A53D0) {
  Guard g(215);
  __imp__sub_825A53D0(ctx, base);
}

extern "C" REX_FUNC(sub_8266E6B0) {
  Guard g(216);
  __imp__sub_8266E6B0(ctx, base);
}

extern "C" REX_FUNC(sub_8266EA90) {
  Guard g(217);
  __imp__sub_8266EA90(ctx, base);
}

extern "C" REX_FUNC(sub_8266F708) {
  Guard g(218);
  __imp__sub_8266F708(ctx, base);
}

extern "C" REX_FUNC(sub_82675080) {
  Guard g(219);
  __imp__sub_82675080(ctx, base);
}

extern "C" REX_FUNC(sub_82675300) {
  Guard g(220);
  __imp__sub_82675300(ctx, base);
}

extern "C" REX_FUNC(sub_8267AB50) {
  Guard g(221);
  __imp__sub_8267AB50(ctx, base);
}

extern "C" REX_FUNC(sub_826A9570) {
  Guard g(222);
  __imp__sub_826A9570(ctx, base);
}

extern "C" REX_FUNC(sub_826BCA38) {
  Guard g(223);
  __imp__sub_826BCA38(ctx, base);
}

extern "C" REX_FUNC(sub_82762698) {
  Guard g(224);
  __imp__sub_82762698(ctx, base);
}

extern "C" REX_FUNC(sub_82777420) {
  Guard g(225);
  __imp__sub_82777420(ctx, base);
}

extern "C" REX_FUNC(sub_827CF0A0) {
  Guard g(226);
  __imp__sub_827CF0A0(ctx, base);
}

extern "C" REX_FUNC(sub_82884078) {
  Guard g(227);
  __imp__sub_82884078(ctx, base);
}

extern "C" REX_FUNC(sub_828B3EF8) {
  Guard g(228);
  __imp__sub_828B3EF8(ctx, base);
}

extern "C" REX_FUNC(sub_828D42D8) {
  Guard g(229);
  __imp__sub_828D42D8(ctx, base);
}

extern "C" REX_FUNC(sub_828D9BA0) {
  Guard g(230);
  __imp__sub_828D9BA0(ctx, base);
}

extern "C" REX_FUNC(sub_828DAAF0) {
  Guard g(231);
  __imp__sub_828DAAF0(ctx, base);
}

extern "C" REX_FUNC(sub_828DAE50) {
  Guard g(232);
  __imp__sub_828DAE50(ctx, base);
}

extern "C" REX_FUNC(sub_828DAF98) {
  Guard g(233);
  __imp__sub_828DAF98(ctx, base);
}

extern "C" REX_FUNC(sub_828DB080) {
  Guard g(234);
  __imp__sub_828DB080(ctx, base);
}

extern "C" REX_FUNC(sub_828DB1A0) {
  Guard g(235);
  __imp__sub_828DB1A0(ctx, base);
}

extern "C" REX_FUNC(sub_828DB480) {
  Guard g(236);
  __imp__sub_828DB480(ctx, base);
}

extern "C" REX_FUNC(sub_828DC570) {
  Guard g(237);
  __imp__sub_828DC570(ctx, base);
}

extern "C" REX_FUNC(sub_828ED1C8) {
  Guard g(238);
  __imp__sub_828ED1C8(ctx, base);
}

extern "C" REX_FUNC(sub_828ED408) {
  Guard g(239);
  __imp__sub_828ED408(ctx, base);
}

extern "C" REX_FUNC(sub_828EFC40) {
  Guard g(240);
  __imp__sub_828EFC40(ctx, base);
}

extern "C" REX_FUNC(sub_828F11B8) {
  Guard g(241);
  __imp__sub_828F11B8(ctx, base);
}

extern "C" REX_FUNC(sub_828FAD88) {
  Guard g(242);
  __imp__sub_828FAD88(ctx, base);
}

extern "C" REX_FUNC(sub_8296E500) {
  Guard g(243);
  __imp__sub_8296E500(ctx, base);
}

extern "C" REX_FUNC(sub_8296E578) {
  Guard g(244);
  __imp__sub_8296E578(ctx, base);
}

extern "C" REX_FUNC(sub_829840D0) {
  Guard g(245);
  __imp__sub_829840D0(ctx, base);
}

extern "C" REX_FUNC(sub_829841A0) {
  Guard g(246);
  __imp__sub_829841A0(ctx, base);
}

extern "C" REX_FUNC(sub_829848B8) {
  Guard g(247);
  __imp__sub_829848B8(ctx, base);
}

extern "C" REX_FUNC(sub_8298BCC0) {
  Guard g(248);
  __imp__sub_8298BCC0(ctx, base);
}

extern "C" REX_FUNC(sub_8298DAD8) {
  Guard g(249);
  __imp__sub_8298DAD8(ctx, base);
}

extern "C" REX_FUNC(sub_829987D8) {
  Guard g(250);
  __imp__sub_829987D8(ctx, base);
}

extern "C" REX_FUNC(sub_82998988) {
  Guard g(251);
  __imp__sub_82998988(ctx, base);
}

extern "C" REX_FUNC(sub_82998BF8) {
  Guard g(252);
  __imp__sub_82998BF8(ctx, base);
}

extern "C" REX_FUNC(sub_829A1B48) {
  Guard g(253);
  __imp__sub_829A1B48(ctx, base);
}

extern "C" REX_FUNC(sub_829A1CF8) {
  Guard g(254);
  __imp__sub_829A1CF8(ctx, base);
}

extern "C" REX_FUNC(sub_829A1F50) {
  Guard g(255);
  __imp__sub_829A1F50(ctx, base);
}

extern "C" REX_FUNC(sub_829A3CE0) {
  Guard g(256);
  __imp__sub_829A3CE0(ctx, base);
}

extern "C" REX_FUNC(sub_829A3E28) {
  Guard g(257);
  __imp__sub_829A3E28(ctx, base);
}

extern "C" REX_FUNC(sub_829A3F60) {
  Guard g(258);
  __imp__sub_829A3F60(ctx, base);
}

extern "C" REX_FUNC(sub_829A4090) {
  Guard g(259);
  __imp__sub_829A4090(ctx, base);
}

extern "C" REX_FUNC(sub_829A4230) {
  Guard g(260);
  __imp__sub_829A4230(ctx, base);
}

extern "C" REX_FUNC(sub_829A43B8) {
  Guard g(261);
  __imp__sub_829A43B8(ctx, base);
}

extern "C" REX_FUNC(sub_829A9278) {
  Guard g(262);
  __imp__sub_829A9278(ctx, base);
}

extern "C" REX_FUNC(sub_829B3258) {
  Guard g(263);
  __imp__sub_829B3258(ctx, base);
}

extern "C" REX_FUNC(sub_829B3378) {
  Guard g(264);
  __imp__sub_829B3378(ctx, base);
}

extern "C" REX_FUNC(sub_829B68B0) {
  Guard g(265);
  __imp__sub_829B68B0(ctx, base);
}

extern "C" REX_FUNC(sub_829EECA0) {
  Guard g(266);
  __imp__sub_829EECA0(ctx, base);
}

extern "C" REX_FUNC(sub_82A01E18) {
  Guard g(267);
  __imp__sub_82A01E18(ctx, base);
}

extern "C" REX_FUNC(sub_82A01F08) {
  Guard g(268);
  __imp__sub_82A01F08(ctx, base);
}

extern "C" REX_FUNC(sub_82A111C0) {
  Guard g(269);
  __imp__sub_82A111C0(ctx, base);
}

extern "C" REX_FUNC(sub_82A180B0) {
  Guard g(270);
  __imp__sub_82A180B0(ctx, base);
}

#endif  // WWE12_DEV_PROBES
