// Small CPU benchmark shared by the Android diagnostic app and its Windows twin (bench_pc.cpp),
// so a phone score can be compared with a PC that runs the game. The loop mimics recompiled PPC
// code: big-endian loads (byte swaps), integer rotates, float multiply-adds and branches.
#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <thread>
#include <vector>

namespace diag {

inline uint32_t bswap32(uint32_t v) { return __builtin_bswap32(v); }

// One unit of work over a 64 KB buffer (fits in L2 on every target).
inline uint64_t BenchUnit(uint32_t *buf, size_t n, float *f, uint64_t seed) {
  uint64_t acc = seed;
  for (size_t i = 0; i < n; ++i) {
    uint32_t v = bswap32(buf[i]);
    v = (v << 7) | (v >> 25);
    acc += v ^ (acc >> 3);
    float x = f[i & 255];
    x = x * 1.0001f + static_cast<float>(v & 0xFF) * 0.5f;
    f[i & 255] = (x > 1e6f) ? x * 1e-6f : x;
    if (v & 1) buf[i] = bswap32(v + static_cast<uint32_t>(acc));
  }
  return acc;
}

// Units per second on one thread, measured over `ms` milliseconds.
inline double BenchThread(int ms) {
  constexpr size_t kN = 16384;
  std::vector<uint32_t> buf(kN);
  std::vector<float> f(256, 1.0f);
  for (size_t i = 0; i < kN; ++i) buf[i] = static_cast<uint32_t>(i * 2654435761u);
  uint64_t acc = 0, units = 0;
  auto start = std::chrono::steady_clock::now();
  auto end = start + std::chrono::milliseconds(ms);
  auto now = start;
  while (now < end) {
    for (int k = 0; k < 8; ++k) acc = BenchUnit(buf.data(), kN, f.data(), acc);
    units += 8;
    now = std::chrono::steady_clock::now();
  }
  asm volatile("" : : "r"(acc));  // keep the work from being optimized away
  double secs = std::chrono::duration<double>(now - start).count();
  return units / secs;
}

// Score = units/s / 100, single thread and all threads together.
struct BenchResult {
  double single = 0;
  double multi = 0;
  int threads = 0;
};

inline BenchResult RunBench(int threads, int ms = 1500) {
  BenchResult r;
  r.threads = threads;
  BenchThread(200);  // warm up / let the governor raise clocks
  r.single = BenchThread(ms) / 100.0;
  std::vector<std::thread> pool;
  std::vector<double> scores(threads);
  for (int t = 0; t < threads; ++t)
    pool.emplace_back([&, t] { scores[t] = BenchThread(ms); });
  for (auto &th : pool) th.join();
  for (double s : scores) r.multi += s / 100.0;
  return r;
}

}  // namespace diag
