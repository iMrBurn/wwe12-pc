// Android: the few MSVC C runtime functions the port's shared files use, for every file of the
// game (force-included by port/cmake/android.cmake), not only the ones that include <windows.h>.
#pragma once

#include <ctime>

inline int localtime_s(std::tm *out, const std::time_t *t) { return localtime_r(t, out) ? 0 : 1; }
