// Android: the PC's built-in benchmark (native/wwe12_benchmark.cpp: Windows timers, cpuid, relaunch of
// the exe) is not built here; these empty versions keep the remote pad and the frame statistics linking.
#include <string>

bool Wwe12BenchmarkActive() { return false; }
std::string Wwe12BenchmarkTakeScript() { return {}; }
void Wwe12BenchmarkMark(const std::string &) {}
void Wwe12BenchmarkFrame(double, double, double) {}
void Wwe12BenchmarkRecordPad(unsigned short, short, short, bool) {}
