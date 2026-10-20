// WWE '12: Universe mode, RESET UNIVERSE (as in WWE '13). See docs/univers.md.
//
// The Universe menu's EDIT SUPERSTAR entry is labelled EDIT UNIVERSE. Choosing it asks, with the
// game's own yes/no box, whether to reset the Universe; "no" opens the Superstar editor as before,
// "yes" asks once more, then resets the Universe to the one a new save starts with. On while
// wwe12_universe_edit = true.
//
// What a new save does to the Universe, and so the reset:
//   1. sub_826D5938(U): clears the Universe block (U = sub_82683A30()).
//   2. sub_826BE3C0(start data, size): applies the Universe's starting data (superstars, show
//      cards, PPVs, brands, relationships); it is the new-save callback of BATL/SAVE's entry 4.
//   3. The Universe championship table (0x83226508, 23 titles x 2 u16 holders, 512 = nobody) gets
//      the default holders of the table at 0x8324E3A8 (16-byte records: +0 holder, +4 partner,
//      +12 kind, 2 = tag), as sub_82A39E08 does, but for the Universe table only.
// Clearing alone crashes on entering the Universe (tested 2026-10-18).
//
// The start data: on the way into the Universe the game reads BATL/SAVE; sub_826EC008(this, pack)
// is that read's "end" callback and gets the opened pack (a reader handle). Its entry with id 4 is
// the start data, BPE-compressed; the game's reader expands it: sub_827D8E30(pack, id) = index,
// sub_827D9528(pack, index) = a new copy (freed with sub_827D2EA0(copy, 0)), a 6-entry "PACH" pack
// of 6 756 bytes. A copy of our own is kept for the reset.
//
// The Universe menu (found 2026-10-20): a layout menu screen, vtable 0x82021F98. Its A button
// goes through sub_824D1FB0, which calls the screen's action, vtbl[18] = sub_825453D8 (it starts
// the transition to the chosen entry's screen); the chosen entry is the screen's word +28 (0 MATCH
// CARDS ... 4 EDIT SUPERSTAR, 5 EDIT TEAM, 6 EDIT BRAND). vtbl[1] = sub_824D3458 runs every frame
// (all layout menus share it). Texts: the string manager's sub_821A4400(this, id) returns the text
// of an id; 27045 is EDIT SUPERSTAR; 27026-27028 are unused in WWE '12 and carry our questions.
// The game's message box (object *0x8324DEF0): sub_8254E7E8(id) asks yes/no, sub_8254E680(id)
// informs, sub_8254E8C8(&answer) returns non-zero once closed (answer 1 = yes).

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstring>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DECLARE(uint32_t, user_language);

REXCVAR_DEFINE_BOOL(wwe12_universe_edit, false, "WWE12",
                    "Universe menu: EDIT SUPERSTAR becomes EDIT UNIVERSE, which can reset the "
                    "Universe (as in WWE '13).");

REX_EXTERN(__imp__sub_827D8E30);  // pack: index of an entry id (pack, id), -1 if none
REX_EXTERN(__imp__sub_827D9528);  // pack: new expanded copy of an entry (pack, index)
REX_EXTERN(__imp__sub_827D2EA0);  // free (data, 0)
REX_EXTERN(__imp__sub_82683A30);  // Universe block
REX_EXTERN(__imp__sub_826D5938);  // Universe block clear
REX_EXTERN(__imp__sub_826BE3C0);  // Universe new-save init (data, size)
REX_EXTERN(__imp__sub_8254E7E8);  // message box: yes/no question (text id)
REX_EXTERN(__imp__sub_8254E680);  // message box: information (text id)
REX_EXTERN(__imp__sub_8254E8C8);  // message box: closed? (&answer)

namespace {

constexpr u32 kUniverseMenuVtbl = 0x82021F98;
constexpr u32 kEditSuperstarEntry = 4;
constexpr u32 kEditSuperstarText = 27045;
constexpr u32 kAskText = 27026, kConfirmText = 27027, kDoneText = 27028;

constexpr u32 kUniverseTitles = 0x83226508;   // 23 titles x {holder, partner}, u16
constexpr u32 kDefaultChampions = 0x8324E3A8; // 16-byte records, loaded flag at +368
constexpr u32 kTitleCount = 23;
constexpr u16 kNobody = 512;

bool Enabled() { return REXCVAR_GET(wwe12_universe_edit); }

// ---- texts ----------------------------------------------------------------------------------

enum Lang { kEn, kFr, kDe, kEs, kIt };
Lang CurrentLang() {
  switch (REXCVAR_GET(user_language)) {
  case 3: return kDe;
  case 4: return kFr;
  case 5: return kEs;
  case 6: return kIt;
  default: return kEn;
  }
}

// In each language's menu style (French lists SUPERSTARS / ÉQUIPES / ÉCURIES).
const char *Text(u32 id) {
  static const char *const kTexts[][5] = {
      // EDIT SUPERSTAR's new label
      {"EDIT UNIVERSE", "UNIVERS", "WELT", "UNIVERSO", "UNIVERSE"},
      // the question
      {"Reset the WWE Universe?\nChoose No to edit your Superstars.",
       "Remettre l'Univers WWE \xC3\xA0 z\xC3\xA9ro ?\nChoisissez Non pour modifier les Superstars.",
       "WWE-Welt zur\xC3\xBC" "cksetzen?\nW\xC3\xA4hle Nein, um die Superstars zu bearbeiten.",
       "\xC2\xBFReiniciar el Universo WWE?\nElige No para editar las Superstars.",
       "Azzerare il WWE Universe?\nScegli No per modificare le Superstar."},
      // the confirmation
      {"All WWE Universe progress will be lost (calendar, rankings, champions, rivalries).\nContinue?",
       "Toute la progression de l'Univers WWE sera perdue (calendrier, classements, champions, "
       "rivalit\xC3\xA9s).\nContinuer ?",
       "Der gesamte Fortschritt der WWE-Welt geht verloren (Kalender, Ranglisten, Champions, "
       "Rivalit\xC3\xA4ten).\nFortfahren?",
       "Se perder\xC3\xA1 todo el progreso del Universo WWE (calendario, clasificaciones, campeones, "
       "rivalidades).\n\xC2\xBF" "Continuar?",
       "Tutti i progressi del WWE Universe andranno persi (calendario, classifiche, campioni, "
       "rivalit\xC3\xA0).\nContinuare?"},
      // done
      {"The WWE Universe has been reset.\nLeave WWE Universe and come back to see it.",
       "L'Univers WWE a \xC3\xA9t\xC3\xA9 remis \xC3\xA0 z\xC3\xA9ro.\nQuittez l'Univers WWE et "
       "revenez-y pour le voir.",
       "Die WWE-Welt wurde zur\xC3\xBC" "ckgesetzt.\nVerlasse die WWE-Welt und kehre zur\xC3\xBC" "ck, "
       "um sie zu sehen.",
       "El Universo WWE se ha reiniciado.\nSal del Universo WWE y vuelve a entrar para verlo.",
       "Il WWE Universe \xC3\xA8 stato azzerato.\nEsci dal WWE Universe e rientra per vederlo."},
  };
  size_t row;
  switch (id) {
  case kEditSuperstarText: row = 0; break;
  case kAskText: row = 1; break;
  case kConfirmText: row = 2; break;
  case kDoneText: row = 3; break;
  default: return nullptr;
  }
  return kTexts[row][CurrentLang()];
}

// Our texts as guest strings (the game draws the text the string manager returns), one 256-byte
// slot per text, rewritten when the language changes.
u32 GuestText(u32 id) {
  const char *text = Text(id);
  if (!text)
    return 0;
  static u32 s_block = 0;
  static const char *s_for[4] = {};
  if (!s_block)
    s_block = REX_KERNEL_MEMORY()->SystemHeapAlloc(4 * 256, 0x20);
  if (!s_block)
    return 0;
  const u32 slot = id == kEditSuperstarText ? 0 : id - kAskText + 1;
  const u32 at = s_block + slot * 256;
  if (s_for[slot] != text) {
    std::memset(bd::mem::at<char>(at), 0, 256);
    std::memcpy(bd::mem::at<char>(at), text, std::min<size_t>(std::strlen(text), 255));
    s_for[slot] = text;
  }
  return at;
}

// ---- reset ----------------------------------------------------------------------------------

// The Universe's start data, copied into guest memory of our own.
u32 g_start_data = 0;
u32 g_start_size = 0;

// Calls a guest function with up to three arguments, on a copy of the hooked call's context.
template <typename Fn> u32 Call(const PPCContext &in, u8 *base, Fn fn, u32 a, u32 b = 0, u32 c = 0) {
  PPCContext x = in;
  x.r3.u64 = a;
  x.r4.u64 = b;
  x.r5.u64 = c;
  fn(x, base);
  return x.r3.u32;
}

// Size of a "PACH" pack: "PACH", u32 count, then count x {id, offset, size} (little-endian),
// offsets from the end of that table.
u32 PackSize(u32 pack) {
  const u8 *p = bd::mem::try_at<const u8>(pack);
  if (!p || std::memcmp(p, "PACH", 4) != 0)
    return 0;
  const auto le32 = [p](u32 o) {
    return u32(p[o]) | u32(p[o + 1]) << 8 | u32(p[o + 2]) << 16 | u32(p[o + 3]) << 24;
  };
  const u32 count = le32(4);
  if (!count || count > 64)
    return 0;
  const u32 table_end = 8 + count * 12;
  u32 end = table_end;
  for (u32 i = 0; i < count; ++i)
    end = std::max(end, table_end + le32(8 + i * 12 + 4) + le32(8 + i * 12 + 8));
  return end < 0x100000 ? end : 0;
}

void KeepStartData(const PPCContext &in, u8 *base, u32 batl_save) {
  auto *memory = REX_KERNEL_MEMORY();
  const u32 index = Call(in, base, __imp__sub_827D8E30, batl_save, 4);
  if (index == 0xFFFFFFFFu) {
    BD_INFO("[univers] BATL/SAVE read: no entry 4 in pack 0x{:08X}", batl_save);
    return;
  }
  const u32 pack = Call(in, base, __imp__sub_827D9528, batl_save, index);
  const u32 size = pack ? PackSize(pack) : 0;
  if (size) {
    if (g_start_data && g_start_size < size) {
      memory->SystemHeapFree(g_start_data);
      g_start_data = 0;
    }
    if (!g_start_data)
      g_start_data = memory->SystemHeapAlloc(size, 0x20);
    if (g_start_data) {
      std::memcpy(bd::mem::at<u8>(g_start_data), bd::mem::at<u8>(pack), size);
      g_start_size = size;
    }
  }
  if (pack)
    Call(in, base, __imp__sub_827D2EA0, pack, 0);
  BD_INFO("[univers] BATL/SAVE read: Universe start data kept, {} bytes", g_start_size);
}

void ResetUniverseChampions() {
  if (!bd::mem::load<u32>(kDefaultChampions + 368)) {
    BD_INFO("[univers] reset: default champions not loaded, Universe champions kept");
    return;
  }
  for (u32 t = 0; t < kTitleCount; ++t) {
    const u32 rec = kDefaultChampions + t * 16;
    const u16 holder = u16(bd::mem::load<u32>(rec));
    const u16 partner = bd::mem::load<u8>(rec + 12) == 2 ? u16(bd::mem::load<u32>(rec + 4)) : kNobody;
    bd::mem::store<u16>(kUniverseTitles + t * 4, holder);
    bd::mem::store<u16>(kUniverseTitles + t * 4 + 2, partner);
  }
}

bool ResetUniverse(const PPCContext &in, u8 *base) {
  const u32 universe = Call(in, base, __imp__sub_82683A30, 0);
  if (!g_start_data || !g_start_size || !universe) {
    BD_INFO("[univers] reset: no start data (0x{:08X}, {} bytes), nothing done", g_start_data,
            g_start_size);
    return false;
  }
  Call(in, base, __imp__sub_826D5938, universe);
  Call(in, base, __imp__sub_826BE3C0, g_start_data, g_start_size);
  ResetUniverseChampions();
  BD_INFO("[univers] reset: done (block 0x{:08X} cleared, start data applied, default champions)",
          universe);
  return true;
}

// ---- the EDIT UNIVERSE dialogue -------------------------------------------------------------

enum class Step { kIdle, kAsk, kConfirm, kDone };
Step g_step = Step::kIdle;
u32 g_screen = 0;  // the Universe menu screen the dialogue belongs to
u32 g_answer = 0;  // guest word for the message box's answer

// The message box closed? (answer: 1 = yes)
bool BoxClosed(const PPCContext &in, u8 *base, bool &yes) {
  if (!g_answer)
    g_answer = REX_KERNEL_MEMORY()->SystemHeapAlloc(16, 0x10);
  if (!g_answer)
    return true;
  bd::mem::store<u32>(g_answer, 0);
  if (!Call(in, base, __imp__sub_8254E8C8, g_answer))
    return false;
  yes = bd::mem::load<u32>(g_answer) == 1;
  return true;
}

}  // namespace

// The string manager's "text of id": EDIT SUPERSTAR's new label and our questions.
REX_EXTERN(__imp__sub_821A4400);
extern "C" REX_FUNC(sub_821A4400) {
  const u32 id = ctx.r4.u32;
  __imp__sub_821A4400(ctx, base);
  if (Enabled() && (id == kEditSuperstarText || (id >= kAskText && id <= kDoneText)))
    if (const u32 text = GuestText(id))
      ctx.r3.u64 = text;
}

// BATL/SAVE read end (this, pack): keep the Universe's start data.
REX_EXTERN(__imp__sub_826EC008);
extern "C" REX_FUNC(sub_826EC008) {
  const PPCContext in = ctx;
  __imp__sub_826EC008(ctx, base);
  KeepStartData(in, base, in.r4.u32);
}

// The Universe menu's action (A on an entry): EDIT UNIVERSE asks first.
REX_EXTERN(__imp__sub_825453D8);
extern "C" REX_FUNC(sub_825453D8) {
  const u32 screen = ctx.r3.u32;
  if (Enabled() && g_step == Step::kIdle && bd::mem::try_load<u32>(screen) == kUniverseMenuVtbl &&
      bd::mem::try_load<u32>(screen + 28) == kEditSuperstarEntry) {
    g_screen = screen;
    g_step = Step::kAsk;
    Call(ctx, base, __imp__sub_8254E7E8, kAskText);
    return;
  }
  if (g_step != Step::kIdle)
    return;  // a dialogue is open: no transition meanwhile
  __imp__sub_825453D8(ctx, base);
}

// Every layout menu screen's per-frame update: drives the dialogue while it is open (the screen
// itself keeps updating; its A button waits, see above).
REX_EXTERN(__imp__sub_824D3458);
extern "C" REX_FUNC(sub_824D3458) {
  if (g_step != Step::kIdle && ctx.r3.u32 == g_screen) {
    bool yes = false;
    if (BoxClosed(ctx, base, yes)) {
      switch (g_step) {
      case Step::kAsk:
        if (yes) {
          g_step = Step::kConfirm;
          Call(ctx, base, __imp__sub_8254E7E8, kConfirmText);
        } else {
          g_step = Step::kIdle;  // "no": the Superstar editor, as the entry always did
          Call(ctx, base, __imp__sub_825453D8, g_screen);
        }
        break;
      case Step::kConfirm:
        if (yes && ResetUniverse(ctx, base)) {
          g_step = Step::kDone;
          Call(ctx, base, __imp__sub_8254E680, kDoneText);
        } else {
          g_step = Step::kIdle;
        }
        break;
      default:
        g_step = Step::kIdle;
        break;
      }
    }
  }
  __imp__sub_824D3458(ctx, base);
}
