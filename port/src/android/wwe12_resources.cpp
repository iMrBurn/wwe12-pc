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
#if defined(WWE12_IOS)
    // 103 / 104 (iPhone only): the pipelines and vertex declarations a developer's PC has already
    // met (pso_recorder.cpp, svr_resources.cpp). The PC zip and the APK ship them as files beside
    // the game, so the first session prepares them in the background instead of freezing half a
    // second on each new scene; an app bundle is read-only and the workflow copies nothing in, so
    // the executable carries them and wwe12_ios.cpp writes them into Documents, the game folder on
    // iOS. 53 KB of shader fingerprints and render states, no game data.
    ".balign 16\n"
    ".global " WWE12_RES_SYM("wwe12_res103_begin") "\n"
    WWE12_RES_SYM("wwe12_res103_begin") ":\n"
    ".incbin \"" WWE12_RES_DIR "/pipelines_seed.w12p\"\n"
    ".global " WWE12_RES_SYM("wwe12_res103_end") "\n"
    WWE12_RES_SYM("wwe12_res103_end") ":\n"
    ".balign 16\n"
    ".global " WWE12_RES_SYM("wwe12_res104_begin") "\n"
    WWE12_RES_SYM("wwe12_res104_begin") ":\n"
    ".incbin \"" WWE12_RES_DIR "/declarations_seed.w12d\"\n"
    ".global " WWE12_RES_SYM("wwe12_res104_end") "\n"
    WWE12_RES_SYM("wwe12_res104_end") ":\n"
#endif
    ".text\n");

extern "C" const uint8_t wwe12_res101_begin[], wwe12_res101_end[], wwe12_res102_begin[], wwe12_res102_end[];
#if defined(WWE12_IOS)
extern "C" const uint8_t wwe12_res103_begin[], wwe12_res103_end[], wwe12_res104_begin[], wwe12_res104_end[];
#endif

const Wwe12Resource *Wwe12AndroidResource(int id) {
  static const Wwe12Resource r101{wwe12_res101_begin, DWORD(wwe12_res101_end - wwe12_res101_begin)};
  static const Wwe12Resource r102{wwe12_res102_begin, DWORD(wwe12_res102_end - wwe12_res102_begin)};
  if (id == 101)
    return &r101;
  if (id == 102)
    return &r102;
#if defined(WWE12_IOS)
  static const Wwe12Resource r103{wwe12_res103_begin, DWORD(wwe12_res103_end - wwe12_res103_begin)};
  static const Wwe12Resource r104{wwe12_res104_begin, DWORD(wwe12_res104_end - wwe12_res104_begin)};
  if (id == 103)
    return &r103;
  if (id == 104)
    return &r104;
#endif
  return nullptr;  // 1 = the exe's icon: none here
}
