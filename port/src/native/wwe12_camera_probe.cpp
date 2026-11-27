// Dev probe (wwe12_camera_probe, off by default): find the game's match camera (2026-11-27, Rendu; plan in
// docs/etude_camera_libre.md, step A). Three steps, all logged as "[camprobe]":
//
// 1. View matrix -> eye. Every 2 s one frame is "armed": for each draw of that frame the renderer hands us the
//    guest device's 256 vertex shader float constants (D3DDevice + 0x700, big-endian). Every group of 4 registers
//    whose 3x3 part is a rotation (unit rows, at right angles) and whose 4th row or column is (0, 0, 0, 1) is a
//    view or world-view matrix; its eye is -R^T t. The eyes are grouped; the one found in the most draws is the
//    camera (static world geometry is drawn with world = identity). The shadow pass gives another one, the light,
//    which does not move: step 2 keeps only addresses whose value moves.
// 2. Eye -> guest memory. On the first sample, the guest heap is scanned for 3 big-endian floats equal to each of
//    the 2 most frequent eyes (within 2 % of its size, at least 0.5). Each later sample keeps the addresses that still hold one of
//    the new eyes; an address whose value has moved is marked. After 6 samples, up to 16 moving survivors are
//    logged with the 32 floats around them (the target and the field of view are probably next to the eye).
// 3. Survivor -> code. A hardware data breakpoint (debug register DR3 only, 4-byte writes) is put on each moving
//    survivor in turn, 5 s each (up to 8): every distinct host instruction that writes it is logged with its call
//    chain "wwe12.exe+X" (translate with wwe12.map: the generated sub_ that updates the camera).
// The pin trap (wwe12_pin_trap) uses DR0-DR2: this probe touches only DR3. Windows, native renderer only.
// Launcher: tools/dev/lancer_test_camera.bat. Move a superstar around the ring for a minute (or let the benchmark
// match run): the camera has to move for step 2.

#include <rex/cvar.h>

#include <atomic>

#include "svr_hook.h"

REXCVAR_DEFINE_BOOL(wwe12_camera_probe, false, "WWE12",
                    "Dev: find the match camera (view matrix -> eye -> guest memory -> the code that writes it), "
                    "'[camprobe]' lines.");

std::atomic<bool> g_wwe12_camprobe_armed{false};  // read by draw.cpp before each draw's constant upload

#if !defined(_WIN32) || !defined(SVR_NATIVE_RENDERER)
void Wwe12CameraProbeDraw(u32) {}
void Wwe12CameraProbePresent() {}
#else

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <fmt/format.h>

#include "core/logging.h"
#include "core/memory_helpers.h"

namespace {

struct Vec3 {
  float x, y, z;
};

float LoadBeF32(const u8 *p) {
  u32 v;
  std::memcpy(&v, p, 4);
  v = _byteswap_ulong(v);
  float f;
  std::memcpy(&f, &v, 4);
  return f;
}

// Memory search tolerance: the game has moved on by a frame or two when the scan reads its memory (the eye comes
// from the frame the renderer drew), so 2 % of the eye's size (at least 0.5).
float Tol(const Vec3 &e) {
  const float m = std::max({std::fabs(e.x), std::fabs(e.y), std::fabs(e.z)});
  return std::max(0.5f, m * 0.02f);
}

// Grouping of the eyes of one frame (same matrix in several draws).
float GroupTol(const Vec3 &e) {
  const float m = std::max({std::fabs(e.x), std::fabs(e.y), std::fabs(e.z)});
  return std::max(0.01f, m * 1e-4f);
}

bool Near(const Vec3 &a, const Vec3 &b, float tol) {
  return std::fabs(a.x - b.x) <= tol && std::fabs(a.y - b.y) <= tol && std::fabs(a.z - b.z) <= tol;
}

// ---- step 1 (render thread) ----
struct Cluster {
  Vec3 eye;
  int count;
  int reg;
  char conv;  // 'R': translation in the 4th register (row vectors), 'C': translation in .w (column vectors)
};
std::vector<Cluster> g_clusters;  // this armed frame's eyes (render thread only)
int g_draws = 0;

void AddEye(const Vec3 &e, int reg, char conv) {
  if (!std::isfinite(e.x) || !std::isfinite(e.y) || !std::isfinite(e.z))
    return;
  for (Cluster &c : g_clusters)
    if (Near(c.eye, e, GroupTol(e))) {
      ++c.count;
      return;
    }
  if (g_clusters.size() < 64)
    g_clusters.push_back({e, 1, reg, conv});
}

bool Unit(float x, float y, float z) { return std::fabs(x * x + y * y + z * z - 1.0f) < 2e-3f; }
bool Ortho(const float *a, const float *b) { return std::fabs(a[0] * b[0] + a[1] * b[1] + a[2] * b[2]) < 2e-3f; }

// ---- step 2/3 (probe thread) ----
std::mutex g_mutex;
std::vector<Vec3> g_sample;  // the last armed frame's top eyes (most frequent first)
bool g_sample_new = false;
int g_sample_id = 0;

struct Survivor {
  u32 va;
  Vec3 last;
  bool moved;
};

u8 *GuestBase() { return bd::mem::at<u8>(0x40000000) - 0x40000000; }

// Readable host bytes (no __try: the SDK's exception handler sees a bad access before any __try and ends the
// game, see d3d_census.cpp).
bool Readable(const u8 *p, size_t n) {
  MEMORY_BASIC_INFORMATION mbi;
  if (!VirtualQuery(p, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
    return false;
  return p + n <= static_cast<const u8 *>(mbi.BaseAddress) + mbi.RegionSize;
}

// Scans [p, end) (one committed region) for 3 floats near `e`, 4-byte steps.
void ScanRegion(u8 *base, u8 *p, u8 *end, const Vec3 &e, std::vector<Survivor> &out, size_t cap) {
  const float tol = Tol(e);
  for (; p + 12 <= end && out.size() < cap; p += 4) {
    if (std::fabs(LoadBeF32(p) - e.x) > tol)
      continue;
    if (std::fabs(LoadBeF32(p + 4) - e.y) > tol || std::fabs(LoadBeF32(p + 8) - e.z) > tol)
      continue;
    out.push_back({u32(p - base), e, false});
  }
}

void FullScan(const Vec3 &e, std::vector<Survivor> &out) {
  u8 *const base = GuestBase();
  constexpr size_t kCap = 8192;
  for (const auto [lo, hi] : {std::pair<u32, u32>{0x00010000u, 0x9FFF0000u},
                              std::pair<u32, u32>{0xA0000000u, 0xC0000000u}}) {
    u8 *p = base + lo;
    u8 *const end = base + hi;
    while (p < end && out.size() < kCap) {
      MEMORY_BASIC_INFORMATION mbi;
      if (!VirtualQuery(p, &mbi, sizeof(mbi)))
        break;
      u8 *rs = static_cast<u8 *>(mbi.BaseAddress);
      u8 *re = std::min(rs + mbi.RegionSize, end);
      if (mbi.State == MEM_COMMIT && !(mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) &&
          (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                          PAGE_EXECUTE_READWRITE)))
        ScanRegion(base, std::max(p, rs), re, e, out, kCap);
      p = re;
    }
  }
}

bool ReadVec(u32 va, Vec3 &v) {
  const u8 *p = GuestBase() + va;
  if (!Readable(p, 12))
    return false;
  v = {LoadBeF32(p), LoadBeF32(p + 4), LoadBeF32(p + 8)};
  return true;
}

// Hardware breakpoint on DR3 (4-byte writes), on every thread but this one; 0 = off.
u64 g_exe = 0;
std::atomic<u64> g_watch_host{0};
constexpr int kMaxHits = 64;
std::atomic<int> g_hits{0};
u64 g_hit_rip[kMaxHits];
u64 g_hit_chain[kMaxHits][8];
u32 g_hit_value[kMaxHits];

void Chain(const CONTEXT *in, u64 *out) {
  CONTEXT c = *in;
  for (int f = 0; f < 8; ++f) {
    out[f] = c.Rip ? c.Rip - g_exe : 0;
    DWORD64 image = 0;
    PRUNTIME_FUNCTION fn = RtlLookupFunctionEntry(c.Rip, &image, nullptr);
    if (!fn) {
      for (int k = f + 1; k < 8; ++k)
        out[k] = 0;
      return;
    }
    void *data = nullptr;
    DWORD64 frame = 0;
    RtlVirtualUnwind(UNW_FLAG_NHANDLER, image, c.Rip, fn, &c, &data, &frame, nullptr);
  }
}

LONG CALLBACK Handler(EXCEPTION_POINTERS *ep) {
  if (ep->ExceptionRecord->ExceptionCode != STATUS_SINGLE_STEP)
    return EXCEPTION_CONTINUE_SEARCH;
  CONTEXT *c = ep->ContextRecord;
  if (!(c->Dr6 & 8))
    return EXCEPTION_CONTINUE_SEARCH;  // not DR3 (the pin trap's DR0-DR2, or a real single step)
  const u64 rip = c->Rip - g_exe;
  const u64 h = g_watch_host.load();
  const u32 value = h ? _byteswap_ulong(*reinterpret_cast<volatile u32 *>(h)) : 0;
  const int n = std::min(g_hits.load(), kMaxHits);
  bool seen = false;
  for (int i = 0; i < n && !seen; ++i)
    seen = g_hit_rip[i] == rip;
  if (!seen) {
    const int i = g_hits.fetch_add(1);
    if (i < kMaxHits) {
      g_hit_rip[i] = rip;
      g_hit_value[i] = value;
      Chain(c, g_hit_chain[i]);
    }
  }
  c->Dr6 &= ~u64(8);
  return EXCEPTION_CONTINUE_EXECUTION;
}

void SetDr3(u64 addr) {
  const DWORD self = GetCurrentThreadId(), pid = GetCurrentProcessId();
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  if (snap == INVALID_HANDLE_VALUE)
    return;
  THREADENTRY32 te{};
  te.dwSize = sizeof te;
  for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
    if (te.th32OwnerProcessID != pid || te.th32ThreadID == self)
      continue;
    HANDLE t = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
    if (!t)
      continue;
    if (SuspendThread(t) != DWORD(-1)) {
      CONTEXT c{};
      c.ContextFlags = CONTEXT_DEBUG_REGISTERS;
      if (GetThreadContext(t, &c)) {
        c.Dr3 = addr;
        u64 dr7 = c.Dr7 & ~(u64(0xC0) | (u64(0xF) << 28));  // L3/G3 and RW3/LEN3 cleared
        if (addr)
          dr7 |= 0x40 | (u64(0b01) << 28) | (u64(0b11) << 30);  // L3, write, 4 bytes
        c.Dr7 = dr7;
        SetThreadContext(t, &c);
      }
      ResumeThread(t);
    }
    CloseHandle(t);
  }
  CloseHandle(snap);
}

void LogContext(u32 va) {
  std::string line;
  for (int i = -16; i < 16; ++i) {
    const u8 *p = GuestBase() + u32(va + u32(i * 4));
    float f = 0;
    if (Readable(p, 4))
      f = LoadBeF32(p);
    line += fmt::format("{}{:.3f}", i == -16 ? "" : (i == 0 ? " [" : " "), f);
    if (i == 2)
      line += "]";
  }
  BD_INFO("[camprobe]   {:08X} (32 floats from -64 bytes, the eye in []): {}", va, line);
}

void FlushHits(u32 va) {
  const int n = std::min(g_hits.exchange(0), kMaxHits);
  if (!n)
    BD_INFO("[camprobe]   {:08X}: no write in 5 s", va);
  for (int i = 0; i < n; ++i) {
    std::string chain;
    for (u64 o : g_hit_chain[i])
      if (o)
        chain += fmt::format(" {:X}", o);
    BD_INFO("[camprobe]   {:08X} written by wwe12.exe+{:X} (value {:08X}), stack:{}", va, g_hit_rip[i],
            g_hit_value[i], chain);
  }
}

void ProbeThread() {
  std::vector<Survivor> survivors;
  int samples = 0, last_id = 0;
  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    std::vector<Vec3> eyes;
    {
      std::lock_guard lock(g_mutex);
      if (!g_sample_new || g_sample_id == last_id)
        continue;
      g_sample_new = false;
      last_id = g_sample_id;
      eyes = g_sample;
    }
    if (eyes.empty())
      continue;
    if (samples == 0) {
      for (size_t k = 0; k < eyes.size() && k < 2; ++k) {
        const size_t before = survivors.size();
        FullScan(eyes[k], survivors);
        BD_INFO("[camprobe] step 2: eye {} ({:.3f}, {:.3f}, {:.3f}) found at {} guest addresses", k + 1,
                eyes[k].x, eyes[k].y, eyes[k].z, survivors.size() - before);
      }
      if (survivors.empty())
        continue;  // try again at the next sample
      ++samples;
      continue;
    }
    size_t moved = 0;
    std::erase_if(survivors, [&](Survivor &s) {
      Vec3 v;
      if (!ReadVec(s.va, v))
        return true;
      for (const Vec3 &e : eyes)
        if (Near(v, e, Tol(e))) {
          if (!Near(v, s.last, Tol(e) * 0.5f))
            s.moved = true;
          s.last = v;
          return false;
        }
      return true;
    });
    for (const Survivor &s : survivors)
      moved += s.moved;
    ++samples;
    BD_INFO("[camprobe] step 2: sample {}: {} addresses still hold the camera eye, {} of them have moved", samples,
            survivors.size(), moved);
    if (samples < 6)
      continue;
    if (!moved) {
      if (samples % 5 == 0)
        BD_INFO("[camprobe] the eye has not moved yet: move a superstar around the ring");
      continue;
    }
    // Step 3.
    std::vector<u32> list;
    for (const Survivor &s : survivors)
      if (s.moved && list.size() < 16)
        list.push_back(s.va);
    BD_INFO("[camprobe] step 2 done: {} moving address(es) hold the camera eye:", list.size());
    for (u32 va : list)
      LogContext(va);
    g_exe = u64(GetModuleHandleW(nullptr));
    static PVOID handler = AddVectoredExceptionHandler(1, &Handler);
    (void)handler;
    for (size_t i = 0; i < list.size() && i < 8; ++i) {
      u8 *host = GuestBase() + list[i];
      if (!Readable(host, 4))
        continue;
      g_hits = 0;
      g_watch_host = u64(host);
      BD_INFO("[camprobe] step 3: watching the writes of {:08X} for 5 s", list[i]);
      for (int t = 0; t < 5; ++t) {  // again each second: threads created since
        SetDr3(u64(host));
        std::this_thread::sleep_for(std::chrono::seconds(1));
      }
      SetDr3(0);
      g_watch_host = 0;
      FlushHits(list[i]);
    }
    BD_INFO("[camprobe] done: translate the wwe12.exe+X offsets with wwe12.map; the game can be closed");
    g_wwe12_camprobe_armed = false;
    return;
  }
}

std::atomic<bool> g_done{false};

}  // namespace

// Render thread, before each draw's vertex constants upload, only in an armed frame.
void Wwe12CameraProbeDraw(u32 device_guest) {
  if (!device_guest)
    return;
  const u8 *c = bd::mem::try_at<u8>(device_guest + 0x700);
  if (!c)
    return;
  ++g_draws;
  float r[256][4];
  for (int i = 0; i < 256; ++i)
    for (int j = 0; j < 4; ++j)
      r[i][j] = LoadBeF32(c + (i * 4 + j) * 4);
  for (int i = 0; i + 4 <= 256; ++i) {
    const float *a = r[i], *b = r[i + 1], *d = r[i + 2], *t = r[i + 3];
    if (!Unit(a[0], a[1], a[2]) || !Unit(b[0], b[1], b[2]) || !Unit(d[0], d[1], d[2]))
      continue;
    if (!Ortho(a, b) || !Ortho(a, d) || !Ortho(b, d))
      continue;
    if (std::fabs(t[3] - 1.0f) > 1e-4f)
      continue;
    if (std::fabs(a[3]) < 1e-6f && std::fabs(b[3]) < 1e-6f && std::fabs(d[3]) < 1e-6f) {
      // Row vectors: v' = v R + t, t = r[i+3].xyz -> eye = -t R^T.
      if (std::fabs(t[0]) + std::fabs(t[1]) + std::fabs(t[2]) < 1e-6f)
        continue;  // identity-like: not a camera
      AddEye({-(t[0] * a[0] + t[1] * a[1] + t[2] * a[2]), -(t[0] * b[0] + t[1] * b[1] + t[2] * b[2]),
              -(t[0] * d[0] + t[1] * d[1] + t[2] * d[2])},
             i, 'R');
    } else if (std::fabs(t[0]) < 1e-6f && std::fabs(t[1]) < 1e-6f && std::fabs(t[2]) < 1e-6f) {
      // Column vectors: v' = R v + t, t = (r[i].w, r[i+1].w, r[i+2].w) -> eye = -R^T t.
      AddEye({-(a[0] * a[3] + b[0] * b[3] + d[0] * d[3]), -(a[1] * a[3] + b[1] * b[3] + d[1] * d[3]),
              -(a[2] * a[3] + b[2] * b[3] + d[2] * d[3])},
             i, 'C');
    }
  }
}

// Render thread, once per presented frame.
void Wwe12CameraProbePresent() {
  if (!REXCVAR_GET(wwe12_camera_probe) || g_done.load(std::memory_order_relaxed))
    return;
  static bool s_started = false;
  if (!s_started) {
    s_started = true;
    std::thread([] {
      ProbeThread();
      g_done = true;
    }).detach();
    BD_INFO("[camprobe] probe on: one frame looked at every 2 s");
  }
  using clock = std::chrono::steady_clock;
  static clock::time_point s_next = clock::now() + std::chrono::seconds(2);
  if (g_wwe12_camprobe_armed.load(std::memory_order_relaxed)) {
    // The armed frame just ended: its eyes, most frequent first.
    g_wwe12_camprobe_armed.store(false, std::memory_order_relaxed);
    std::sort(g_clusters.begin(), g_clusters.end(), [](const Cluster &a, const Cluster &b) { return a.count > b.count; });
    std::string line;
    for (size_t k = 0; k < g_clusters.size() && k < 3; ++k)
      line += fmt::format(" | ({:.3f}, {:.3f}, {:.3f}) in {} draws, registers c{}..c{} ({})", g_clusters[k].eye.x,
                          g_clusters[k].eye.y, g_clusters[k].eye.z, g_clusters[k].count, g_clusters[k].reg,
                          g_clusters[k].reg + 3, g_clusters[k].conv == 'R' ? "row vectors" : "column vectors");
    static int s_logged = 0;
    if (s_logged++ < 40)
      BD_INFO("[camprobe] step 1: {} draws, {} eye(s){}", g_draws, g_clusters.size(), line);
    {
      std::lock_guard lock(g_mutex);
      g_sample.clear();
      for (size_t k = 0; k < g_clusters.size() && k < 3; ++k)
        g_sample.push_back(g_clusters[k].eye);
      g_sample_new = true;
      ++g_sample_id;
    }
    g_clusters.clear();
    g_draws = 0;
    return;
  }
  if (clock::now() >= s_next) {
    s_next = clock::now() + std::chrono::seconds(2);
    g_clusters.clear();
    g_draws = 0;
    g_wwe12_camprobe_armed.store(true, std::memory_order_relaxed);  // the next frame's draws
  }
}

#endif
