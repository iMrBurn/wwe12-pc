// WWE '12 PC: type with the physical keyboard while the game's own on-screen letter grid is
// open (Create a Story text, CAW names, arena names...), no controller needed.
//
// How it works (all offsets deduced from the translated sources, verified against the guest
// memory the game actually touches during a typing session):
//   - The per-frame text-widget update is sub_824C9228 (recomp.194.cpp:7772), called for the
//     active widget from the screen state machine sub_824CAF58 (recomp.76.cpp:7878). While this
//     screen is open it reads the widget's fields every frame, so wrapping this function both
//     tells us a letter grid is up and hands us the widget pointer (r4).
//   - Widget layout (guest offsets from the widget base): +0x4F property tag byte (0x36, string),
//     +0x50 work text buffer (NUL-terminated, capacity ~1024), +0xC8 pointer back to +0x50,
//     +0xD5 caret column byte, +0xD8 int gate the caller checks (==1).
//   - The game rebuilds everything it shows from that work buffer every frame, so a hook only
//     needs to write the work buffer and nudge +0xD5: pad-typed letters and keyboard-typed
//     letters stay consistent, and the caret box lands after our text.
//   - While the grid is up we pause mnk_mode (the keyboard-as-controller cvar) and restore it
//     when the widget goes away, so physical keys type instead of firing bound buttons.
// Disable with wwe12_kbd = false in wwe12.toml (wwe12_kbd_max caps the text length, 84 as in
// the story editor's "LEFT 84").

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <string>

#include <fmt/format.h>
#include <rex/cvar.h>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shlobj.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

namespace {

REXCVAR_DEFINE_BOOL(wwe12_kbd, true, "WWE12",
                    "Type with the physical keyboard while the letter grid is open.");
REXCVAR_DEFINE_INT32(wwe12_kbd_max, 84, "WWE12",
                     "Maximum text length typed with the keyboard (story editor is 84).");

// Le widget actif, capturé par le hook; expire quand l'écran ferme.
std::atomic<u32> g_widget{0};
std::atomic<long long> g_last_seen{0};   // ms monotone
std::atomic<size_t> g_len{0};
std::atomic<bool> g_mnk_on_hold{false};  // mnk_mode suspendu pendant la grille
bool g_mnk_prev = false;

[[nodiscard]] long long NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

// Journal de diagnostic du module : Documents\wwe12\logs\kbd_module_log.txt.
// Écrit au plus une ligne toutes les 500 ms (ou quand l'état change), pour
// voir sur place pourquoi un widget est accepté ou refusé.
void Diag(const std::string &what) {
  static FILE *f = nullptr;
  static std::string last;
  static long long last_ms = 0;
  const long long now = NowMs();
  auto open = [&] {
    PWSTR docs = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_Documents, 0, nullptr, &docs))) return;
    std::wstring dir = std::wstring(docs) + L"\\wwe12\\logs";
    CreateDirectoryW((std::wstring(docs) + L"\\wwe12").c_str(), nullptr);
    CreateDirectoryW(dir.c_str(), nullptr);
    CoTaskMemFree(docs);
    f = _wfopen((dir + L"\\kbd_module_log.txt").c_str(), L"ab");
  };
  if (!f) open();
  if (!f) return;
  if (what != last || now - last_ms >= 500) {
    std::fprintf(f, "%lld %s\n", now, what.c_str());
    std::fflush(f);
    last = what;
    last_ms = now;
  }
}

// -------------------- le hook sur la mise à jour du widget --------------------

// Observateur par frame (arguments capturés AVANT l'appel du jeu).
//
// Structure vérifiée sur la mémoire réelle (dump radar) :
//   - r4 = widget de l'écran (tableau à pas de 240 octets) ;
//   - [r4+200] = pointeur vers le TEXTE DE TRAVAIL (p) ;
//   - le texte vit INLINE dans l'objet « entrée » : entry = p - 0x50, avec
//     [entry+0x4F] = 0x36 (tag propriété string) et [entry+0x30] = entry
//     (auto-pointeur, saut de vérif sans faille) ;
//   - les labels de menu (r4 vus en séance : A3EDF170.., texte de tables)
//     n'ont PAS ce motif : refusés d'office.
void ObserveTextWidget(PPCContext &args, uint8_t *base) {
  const u32 widget = args.r4.u32;
  if (!bd::mem::ready() || !widget) {
    Diag("hook r4=0 (noyau pas pret)");
    return;
  }
  const u32 text_va = bd::mem::try_field<u32>(widget, 200);
  const u32 entry = text_va - 0x50;
  bool ok = false;
  size_t len = 0;
  if (const auto *t = bd::mem::try_at<char>(text_va)) {
    len = std::strlen(t);
    ok = len <= 0x300 && bd::mem::try_field<u32>(entry, 0x30) == entry &&
         bd::mem::try_field<u8>(entry, 0x4F) == 0x36;
    if (ok) {
      for (size_t i = 0; i < len; ++i) {
        const u8 c = u8(t[i]);
        if (c < 0x20 || (c >= 0x7F && c < 0xA0)) {
          ok = false;
          break;
        }
      }
    }
  }
  if (ok) {
    if (g_widget.exchange(widget) != widget) {
      // Nouveau widget : une photo complète de ses champs pour l'analyse.
      std::string h = "accepte " + fmt::format("{:08X} text={:08X} len={}", widget, text_va, len) +
                      " widget[+0..0x120]:";
      for (int off = 0; off < 0x120; off += 4) {
        h += fmt::format(" {:08X}", bd::mem::try_field<u32>(widget, off));
      }
      Diag(h);
      std::string e = "entry " + fmt::format("{:08X}", entry) + " [+0x00..0x60]:";
      for (int off = 0; off < 0x60; off += 4) {
        e += fmt::format(" {:08X}", bd::mem::try_field<u32>(entry, off));
      }
      Diag(e);
    }
    g_widget = widget;
    g_len = len;
    g_last_seen = NowMs();
  } else {
    Diag("hook r4=" + fmt::format("{:08X}", widget) + " text=" + fmt::format("{:08X}", text_va) +
         " refus");
  }
}

}  // namespace

// Le hook lui-même : le jeu tourne, puis on observe avec les arguments d'entrée.
// (Remplacement à l'édition de liens, comme les autres hooks du port.)
REX_EXTERN(__imp__sub_824C9228);
extern "C" REX_FUNC(sub_824C9228) {
  PPCContext args;
  ::svr::CopyArgs(args, ctx);
  __imp__sub_824C9228(ctx, base);
  if (REXCVAR_GET(wwe12_kbd)) {
    ObserveTextWidget(args, base);
  }
}

namespace {

// -------------------- frappe physique -> insertion --------------------

[[nodiscard]] char KeyToChar(WPARAM vk, bool shift) {
  if (vk >= 'A' && vk <= 'Z') {
    return char(shift ? vk : vk - 'A' + 'a');
  }
  if (vk >= '0' && vk <= '9' && !shift) {
    return char(vk);
  }
  if (!shift) {
    switch (vk) {
      case VK_SPACE: return ' ';
      case VK_OEM_MINUS: return '-';
      case VK_OEM_PLUS: return '=';
      case VK_OEM_COMMA: return ',';
      case VK_OEM_PERIOD: return '.';
      case VK_OEM_2: return '/';
      case VK_OEM_1: return ';';
      case VK_OEM_7: return '\'';
      case VK_OEM_4: return '[';
      case VK_OEM_6: return ']';
      default: return 0;
    }
  }
  switch (vk) {
    case VK_OEM_MINUS: return '_';
    case VK_OEM_PLUS: return '+';
    case VK_OEM_COMMA: return '<';
    case VK_OEM_PERIOD: return '>';
    case VK_OEM_2: return '?';
    case VK_OEM_1: return ':';
    case VK_OEM_7: return '"';
    case VK_OEM_4: return '{';
    case VK_OEM_6: return '}';
    default: return 0;
  }
}

// Position d'insertion : le jeu insère avant un éventuel marqueur 0x0A en fin
// de ligne, sinon à la fin (d'après sa routine de reconstruction). Ici on vise
// le marqueur s'il existe, sinon le NUL final.
[[nodiscard]] size_t InsertPos(const char *work, size_t &len) {
  len = std::strlen(work);
  for (size_t i = 0; i < len && i < 0x20; ++i) {
    if (work[i] == '\n') return i;  // au marqueur, sans le déplacer
  }
  return len;
}

bool InsertChar(WPARAM vk, bool shift) {
  const char ch = KeyToChar(vk, shift);
  if (!ch) return false;
  const u32 widget = g_widget.load();
  const u32 text_va = bd::mem::try_field<u32>(widget, 200);
  char *work = bd::mem::try_at<char>(text_va);
  if (!work) return false;
  size_t len = 0;
  const size_t pos = InsertPos(work, len);
  const int max = std::clamp(REXCVAR_GET(wwe12_kbd_max), 1, 1023);
  if (pos >= int(max) || len >= 0x300) return false;

  // Décale (y compris le NUL final) puis pose la lettre. Le texte de travail
  // est ce que le jeu relit chaque frame pour tout recalculer.
  std::memmove(work + pos + 1, work + pos, len - pos + 1);
  work[pos] = ch;
  g_len = len + 1;
  BD_INFO("[kbd] '{}' insere (pos {}, widget {:08X})", std::string(1, ch), pos, widget);
  return true;
}

bool Backchar() {
  const u32 widget = g_widget.load();
  const u32 text_va = bd::mem::try_field<u32>(widget, 200);
  char *work = bd::mem::try_at<char>(text_va);
  if (!work) return false;
  size_t len = 0;
  const size_t pos = InsertPos(work, len);
  if (pos == 0 || len == 0) return false;
  std::memmove(work + pos - 1, work + pos, len - pos + 1);  // emporte le NUL
  g_len = len - 1;
  return true;
}

// etat du clavier : une insertion par pression (avec repetition d'attente).
struct KeyState {
  bool was_down = false;
};

bool TakePress(KeyState &k, int vk) {
  const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
  const bool press = down && !k.was_down;
  k.was_down = down;
  return press;
}

void KeyThread() {
  static constexpr int kKeys[] = {
      'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
      'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
      '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
      VK_SPACE, VK_OEM_MINUS, VK_OEM_PLUS, VK_OEM_COMMA, VK_OEM_PERIOD,
      VK_OEM_1, VK_OEM_2, VK_OEM_4, VK_OEM_6, VK_OEM_7, VK_BACK,
  };
  static std::array<KeyState, std::size(kKeys)> states{};

  BD_INFO("[kbd] module clavier pret");
  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    if (!REXCVAR_GET(wwe12_kbd)) continue;

    const bool active = g_widget.load() != 0 && NowMs() - g_last_seen.load() < 600;

    // Suspend mnk (le clavier ne doit plus courir la manette pendant la saisie).
    // mnk_mode vit dans la DLL du SDK : accès par nom (pas la macro directe).
    if (active && !g_mnk_on_hold.load()) {
      g_mnk_prev = rex::cvar::Query<bool>("mnk_mode");
      rex::cvar::SetFlagByName("mnk_mode", "false");
      g_mnk_on_hold = true;
      BD_INFO("[kbd] grille de lettres ouverte: mnk suspend");
    } else if (!active && g_mnk_on_hold.load()) {
      rex::cvar::SetFlagByName("mnk_mode", g_mnk_prev ? "true" : "false");
      g_mnk_on_hold = false;
      BD_INFO("[kbd] grille fermee: mnk restaure");
    }
    if (!active) continue;

    const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    for (size_t i = 0; i < std::size(kKeys); ++i) {
      const int vk = kKeys[i];
      if (!TakePress(states[i], vk)) continue;
      if (vk == VK_BACK) {
        Backchar();
      } else {
        InsertChar(WPARAM(vk), shift);
      }
    }
  }
}

}  // namespace

// Le fil clavier démarre avec le module.
namespace {

struct KbdStarter {
  KbdStarter() {
    std::thread(&KeyThread).detach();
  }
} g_kbd_starter;

}  // namespace
