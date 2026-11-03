// Android: the few MSVC C runtime functions the port's shared files use, for every file of the
// game (force-included by port/cmake/android.cmake), not only the ones that include <windows.h>.
#pragma once

#include <ctime>
#include <strings.h>

inline int localtime_s(std::tm *out, const std::time_t *t) { return localtime_r(t, out) ? 0 : 1; }
inline int _stricmp(const char *a, const char *b) { return strcasecmp(a, b); }
inline int _strnicmp(const char *a, const char *b, size_t n) { return strncasecmp(a, b, n); }
