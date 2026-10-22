// Windows twin of the Android benchmark: gives the reference score of a PC that runs the game.
// Build: clang++ -O2 -std=c++17 bench_pc.cpp -o bench_pc.exe
#include <cstdio>
#include <thread>

#include "bench.h"

int main() {
  int threads = static_cast<int>(std::thread::hardware_concurrency());
  diag::BenchResult b = diag::RunBench(threads);
  std::printf("single-thread: %.0f   all %d threads: %.0f\n", b.single, b.threads, b.multi);
  return 0;
}
