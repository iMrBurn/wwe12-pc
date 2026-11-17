// Android and iPhone: the Windows exe's resources (RCDATA 101 = PlayStation button icons, 102 = WWE '13's
// I Quit / Special Guest Referee motions and HUD images; port/res/wwe12.rc.in) put in the library (Android)
// or the executable (iOS), so the phone has the same game as the PC. FindResourceW & co. of
// compat/windows.h read them from here.
#include <cstddef>
#include <cstdint>

#include "windows.h"

#ifndef WWE12_RES_DIR
#error "WWE12_RES_DIR is set by cmake/android.cmake or cmake/ios.cmake"
#endif

// Mach-O (iOS) names its read-only section differently and puts a '_' before every C symbol.
#if defined(__APPLE__)
#define WWE12_RES_SECTION ".section __TEXT,__const\n"
#define WWE12_RES_SYM(name) "_" name
#else
#define WWE12_RES_SECTION ".section .rodata\n"
#define WWE12_RES_SYM(name) name
#endif

__asm__(
    WWE12_RES_SECTION
    ".balign 16\n"
    ".global " WWE12_RES_SYM("wwe12_res101_begin") "\n"
    WWE12_RES_SYM("wwe12_res101_begin") ":\n"
    ".incbin \"" WWE12_RES_DIR "/ps3_buttons.bin\"\n"
    ".global " WWE12_RES_SYM("wwe12_res101_end") "\n"
    WWE12_RES_SYM("wwe12_res101_end") ":\n"
    ".balign 16\n"
    ".global " WWE12_RES_SYM("wwe12_res102_begin") "\n"
    WWE12_RES_SYM("wwe12_res102_begin") ":\n"
    ".incbin \"" WWE12_RES_DIR "/iquit13.bin\"\n"
    ".global " WWE12_RES_SYM("wwe12_res102_end") "\n"
    WWE12_RES_SYM("wwe12_res102_end") ":\n"
    ".text\n");

extern "C" const uint8_t wwe12_res101_begin[], wwe12_res101_end[], wwe12_res102_begin[], wwe12_res102_end[];

const Wwe12Resource *Wwe12AndroidResource(int id) {
  static const Wwe12Resource r101{wwe12_res101_begin, DWORD(wwe12_res101_end - wwe12_res101_begin)};
  static const Wwe12Resource r102{wwe12_res102_begin, DWORD(wwe12_res102_end - wwe12_res102_begin)};
  if (id == 101)
    return &r101;
  if (id == 102)
    return &r102;
  return nullptr;  // 1 = the exe's icon: none here
}
