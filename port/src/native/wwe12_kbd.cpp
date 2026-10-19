// WWE '12 PC: type with the physical keyboard while the game's own on-screen letter grid is
// open (Create a Story text, CAW names, arena names...), no controller needed.
//
// How it works (found 2026-10-19 with a write trap on the grid's lines, see CLAUDE.md):
//   - The grid's lines are text widgets named "_write21", "_write22"... (name at +0x60), drawn
//     every frame by sub_824C9228(screen r3, widget r4). Their text is only a display copy: the
//     grid edits its own "editor" object and copies the result into the line.
//   - The screen the lines are drawn with (r3) is a member of the grid's "editor" object, at
//     +0x740; the grid's buttons (thunks sub_828A7D30, sub_828A6988, sub_828A6998) act on it.
//   - sub_828A47D0(editor, const char* s, 1, 0) inserts s at the cursor: the letter cells and
//     SPACE (sub_828A62F0, s = " ") go through it, and it handles the length limit ("LEFT 84"),
//     the next line and the grid's SHIFT mode. sub_828A6158(editor) is BACK.
//   - Keys come from the game window (OnKeyChar = the text Windows produces, so AZERTY, Shift,
//     AltGr and key repeat all work). They are queued and replayed through those two functions
//     by the game's own thread, inside the hook, exactly as if the grid's buttons were pressed.
//   - While the grid is up, the typing keys (letters, digits, space, punctuation, Backspace)
//     are not passed to mnk_mode (the keyboard-as-controller), so Z or Backspace type instead
//     of moving or going back. Enter, Escape and the arrows still reach the game.
// Disable with wwe12_kbd = false in wwe12.toml.

#include <atomic>
#include <chrono>
#include <cstring>
#include <deque>
#include <mutex>
#include <string>

#include <fmt/format.h>
#include <rex/cvar.h>
#include <rex/ui/window.h>
#include <rex/ui/window_listener.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REX_EXTERN(__imp__sub_828A47D0);
REX_EXTERN(__imp__sub_828A6158);

namespace {

REXCVAR_DEFINE_BOOL(wwe12_kbd, true, "WWE12",
                    "Type with the physical keyboard while the letter grid is open.");
REXCVAR_DEFINE_BOOL(wwe12_kbd_log, false, "WWE12",
                    "Log the letter grid's editor and every key typed (keyboard typing probe).");

constexpr char kBackspace = '\b';
constexpr long long kGridTimeoutMs = 400;  // grid seen less than this long ago = still open

rex::ui::Window* g_window = nullptr;
std::atomic<u32> g_editor{0};
std::atomic<long long> g_last_seen{0};  // ms, steady clock
std::atomic<u32> g_screen{0};           // screen the grid lines were last drawn with
std::atomic<u32> g_learned_editor{0};   // editor of the grid's own last insert, on g_screen
std::atomic<u32> g_learned_screen{0};

std::mutex g_queue_mutex;
std::deque<char> g_queue;  // typed characters (or kBackspace) waiting for the game thread

[[nodiscard]] long long NowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

[[nodiscard]] bool GridOpen() {
  return REXCVAR_GET(wwe12_kbd) && g_editor.load() != 0 &&
         NowMs() - g_last_seen.load() < kGridTimeoutMs;
}

// The game's font is ASCII: accented letters typed on a French keyboard become their plain
// letter (é -> e), anything else outside ASCII is dropped.
[[nodiscard]] char ToGameChar(uint32_t cp) {
  if (cp >= 0x20 && cp < 0x7F) return char(cp);
  switch (cp) {
    case 0xE0: case 0xE1: case 0xE2: case 0xE4: return 'a';
    case 0xC0: case 0xC1: case 0xC2: case 0xC4: return 'A';
    case 0xE8: case 0xE9: case 0xEA: case 0xEB: return 'e';
    case 0xC8: case 0xC9: case 0xCA: case 0xCB: return 'E';
    case 0xEC: case 0xED: case 0xEE: case 0xEF: return 'i';
    case 0xCC: case 0xCD: case 0xCE: case 0xCF: return 'I';
    case 0xF2: case 0xF3: case 0xF4: case 0xF6: return 'o';
    case 0xD2: case 0xD3: case 0xD4: case 0xD6: return 'O';
    case 0xF9: case 0xFA: case 0xFB: case 0xFC: return 'u';
    case 0xD9: case 0xDA: case 0xDB: case 0xDC: return 'U';
    case 0xE7: return 'c';
    case 0xC7: return 'C';
    case 0xF1: return 'n';
    case 0xD1: return 'N';
    default: return 0;
  }
}

// Keys that type while the grid is open (kept away from mnk_mode). Win32 virtual-key codes.
[[nodiscard]] bool IsTypingKey(uint16_t vk) {
  return vk == 0x08 /*Backspace*/ || vk == 0x20 /*Space*/ || (vk >= '0' && vk <= '9') ||
         (vk >= 'A' && vk <= 'Z') || (vk >= 0x60 && vk <= 0x6F) /*numpad*/ ||
         (vk >= 0xBA && vk <= 0xC0) || (vk >= 0xDB && vk <= 0xDF) || vk == 0xE2 /*OEM*/;
}

class KbdListener final : public rex::ui::WindowInputListener {
 public:
  void OnKeyDown(rex::ui::KeyEvent& e) override {
    if (!GridOpen() || e.is_ctrl_pressed()) return;
    const auto vk = static_cast<uint16_t>(e.virtual_key());
    if (!IsTypingKey(vk)) return;
    if (vk == 0x08) Push(kBackspace);
    e.set_handled(true);
  }

  void OnKeyChar(rex::ui::KeyEvent& e) override {
    if (!GridOpen()) return;
    if (const char ch = ToGameChar(static_cast<uint32_t>(e.virtual_key()))) Push(ch);
    e.set_handled(true);
  }

 private:
  static void Push(char ch) {
    std::lock_guard lock(g_queue_mutex);
    if (g_queue.size() < 256) g_queue.push_back(ch);
  }
};

KbdListener g_listener;

// A grid line: its widget is named "_write..." (at +0x60, after the font name).
[[nodiscard]] bool IsGridLine(u32 widget) {
  return bd::mem::try_field<u8>(widget, 0x60) == '_' &&
         bd::mem::try_field<u8>(widget, 0x61) == 'w' &&
         bd::mem::try_field<u8>(widget, 0x62) == 'r' &&
         bd::mem::try_field<u8>(widget, 0x63) == 'i' && bd::mem::try_field<u8>(widget, 0x64) == 't';
}

// The grid's editor: the screen the lines are drawn with is a member of it, 0x740 bytes in
// (seen 2026-10-19: editor A3DEEE50, screen A3DEF590). 0 when it does not look like one.
constexpr u32 kScreenInEditor = 0x740;

[[nodiscard]] u32 EditorOf(u32 screen) {
  if (screen < kScreenInEditor) return 0;
  const u32 editor = screen - kScreenInEditor;
  if (!bd::mem::try_field<u32>(editor, 44)) return 0;          // insert reads [editor+44]
  if (bd::mem::try_field<u32>(editor, 3676) >= 8) return 0;    // current line index
  return editor;
}

// Game thread: replay the queued keys through the grid's own functions.
void ApplyQueued(const PPCContext& ctx, uint8_t* base, u32 editor) {
  std::deque<char> keys;
  {
    std::lock_guard lock(g_queue_mutex);
    keys.swap(g_queue);
  }
  for (const char ch : keys) {
    PPCContext c = ctx;
    // A scratch area below the caller's frame: the string at +256, the callee's frames below.
    c.r1.u64 = (ctx.r1.u32 - 512) & ~15u;
    c.r3.u64 = editor;
    if (ch == kBackspace) {
      __imp__sub_828A6158(c, base);
    } else {
      const u32 str = c.r1.u32 + 256;
      bd::mem::store<u8>(str, u8(ch));
      bd::mem::store<u8>(str + 1, 0);
      c.r4.u64 = str;
      c.r5.u64 = 1;
      c.r6.u64 = 0;
      __imp__sub_828A47D0(c, base);
    }
  }
  if (!keys.empty() && REXCVAR_GET(wwe12_kbd_log)) {
    BD_INFO("[kbd] {} touche(s) rejouee(s) dans l'editeur {:08X}", keys.size(), editor);
  }
}

void KeepTextInputOn() {
  // SDL only sends typed text while text input is on; the SDK's ImGui turns it off when its
  // own text boxes close, so it is asked again whenever the grid is seen with it off.
  rex::ui::Window* window = g_window;
  if (!window || window->IsTextInputActive()) return;
  window->app_context().CallInUIThread([window] { window->SetTextInputActive(true); });
}

}  // namespace

void Wwe12KbdInit(rex::ui::Window* window) {
  g_window = window;
  if (window) window->AddInputListener(&g_listener, 1000);  // before mnk_mode's listener
}

// The hook: the game draws the widget; a grid line also hands us the screen, hence the editor.
REX_EXTERN(__imp__sub_824C9228);
extern "C" REX_FUNC(sub_824C9228) {
  PPCContext args;
  ::svr::CopyArgs(args, ctx);
  __imp__sub_824C9228(ctx, base);
  if (!REXCVAR_GET(wwe12_kbd) || !bd::mem::ready()) return;
  const u32 widget = args.r4.u32;
  if (!widget || !IsGridLine(widget)) return;
  g_screen = args.r3.u32;
  u32 editor = EditorOf(args.r3.u32);
  if (!editor && g_learned_screen.load() == args.r3.u32) editor = g_learned_editor.load();
  if (!editor) return;
  const bool was_open = GridOpen();
  if (!was_open || g_editor.exchange(editor) != editor) {
    g_editor = editor;
    BD_INFO("[kbd] grille de lettres ouverte (editeur {:08X})", editor);
    std::lock_guard lock(g_queue_mutex);
    g_queue.clear();  // keys typed before the grid opened are not for it
  }
  g_last_seen = NowMs();
  KeepTextInputOn();
  ApplyQueued(ctx, base, editor);
}

// Probes (wwe12_kbd_log): the grid's own insert and BACK, to check they reach the same editor.
extern "C" REX_FUNC(sub_828A47D0) {
  // The grid's own insert (pad letter, SPACE): remember its editor for the keyboard (fallback
  // for a grid whose editor is not at screen-0x740), and say so when it is not.
  const u32 editor = ctx.r3.u32;
  const u32 screen = g_screen.load();
  if (editor && screen && g_learned_editor.exchange(editor) != editor) {
    g_learned_screen = screen;
    BD_INFO("[kbd] editeur appris {:08X} (ecran {:08X})", editor, screen);
    if (EditorOf(screen) != editor) {
      BD_INFO("[kbd] editeur {:08X} pas a ecran-0x740 (ecran {:08X}) : grille d'un autre type", editor,
              screen);
    }
  }
  if (REXCVAR_GET(wwe12_kbd_log)) {
    const char* s = bd::mem::try_at<const char>(ctx.r4.u32);
    BD_INFO("[kbd] insertion du jeu : editeur {:08X} (vu {:08X}) texte '{}' appelant {:08X}",
            ctx.r3.u32, g_editor.load(), s ? std::string(s, strnlen(s, 4)) : std::string(),
            u32(ctx.lr));
  }
  __imp__sub_828A47D0(ctx, base);
}

extern "C" REX_FUNC(sub_828A6158) {
  if (REXCVAR_GET(wwe12_kbd_log)) {
    BD_INFO("[kbd] BACK du jeu : editeur {:08X} (vu {:08X}) appelant {:08X}", ctx.r3.u32,
            g_editor.load(), u32(ctx.lr));
  }
  __imp__sub_828A6158(ctx, base);
}
