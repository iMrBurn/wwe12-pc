// wwe12_d3d12.exe: whether DXC (dxcompiler.dll, delay-loaded) is next to the exe. The shipped exe
// has none: its spec-constant shaders come pre-linked (wwe12_prelink), and a variant missing from
// that cache is not drawn instead of being linked (guest_shaders.cpp). Dev builds have it.

#include <windows.h>

bool Wwe12DxcAvailable() {
  static const bool available =
      LoadLibraryExW(L"dxcompiler.dll", nullptr,
                     LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32) != nullptr;
  return available;
}
