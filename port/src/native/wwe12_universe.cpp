// WWE '12: Universe mode, RESET UNIVERSE (as in WWE '13). See docs/univers.md.
//
// The Universe menu's EDIT SUPERSTAR entry is labelled EDIT UNIVERSE. Choosing it asks, with the
// game's own yes/no box, whether to reset the Universe; "no" opens the Superstar editor as before,
// "yes" asks once more, backs the save up (launcher format, SAVES page), then resets the Universe
// to the one a new save starts with. The reset lives in memory until the game next saves (after a
// show); the messages say so. On by default (wwe12_universe_edit = false turns it off).
//
// Also VACANT, as in SvR 2011: on a Superstar's page, choosing again the belt he already holds
// offers to vacate it, for a title no weekly show uses (see the VACANT section below).
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
// of an id; 27045 is EDIT SUPERSTAR; 27026-27029 are unused in WWE '12 and carry our messages.
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
#include <ctime>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

#include <fmt/format.h>
#include <vector>

#include <rex/cvar.h>
#include <rex/runtime.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"

REXCVAR_DECLARE(uint32_t, user_language);

REXCVAR_DEFINE_BOOL(wwe12_universe_probe, false, "WWE12",
                    "Dev: log the screen, its vtable, action and first words on every A button of a "
                    "layout menu, and the call chain of each Decision sound (to find a screen's code).");

REXCVAR_DEFINE_BOOL(wwe12_universe_edit, true, "WWE12",
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
constexpr u32 kAskText = 27026, kConfirmText = 27027, kDoneText = 27028, kHintText = 27029;
constexpr u32 kVacateAskText = 27030, kVacateDoneText = 27031;  // the last of our texts

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

// The entry's label says what it is (the user worried players would not find the reset under a
// bare "UNIVERS"); it fits the menu (LISTE DES MATCHS is 16 characters).
const char *Text(u32 id) {
  static const char *const kTexts[][5] = {
      // EDIT SUPERSTAR's new label
      {"EDIT UNIVERSE", "MODIFIER L'UNIVERS", "WELT BEARBEITEN", "EDITAR UNIVERSO",
       "MODIFICA UNIVERSE"},
      // the question
      {"Reset the WWE Universe?\nChoose No to edit your Superstars.",
       "Remettre l'Univers WWE \xC3\xA0 z\xC3\xA9ro ?\nChoisissez Non pour modifier les Superstars.",
       "WWE-Welt zur\xC3\xBC" "cksetzen?\nW\xC3\xA4hle Nein, um die Superstars zu bearbeiten.",
       "\xC2\xBFReiniciar el Universo WWE?\nElige No para editar las Superstars.",
       "Azzerare il WWE Universe?\nScegli No per modificare le Superstar."},
      // the confirmation
      {"All WWE Universe progress will be lost (calendar, rankings, champions, rivalries).\nA "
       "backup of your save is made first (launcher, SAVES page).\nContinue?",
       "Toute la progression de l'Univers WWE sera perdue (calendrier, classements, champions, "
       "rivalit\xC3\xA9s).\nUne copie de votre sauvegarde est faite avant (launcher, page "
       "SAUVEGARDES).\nContinuer ?",
       "Der gesamte Fortschritt der WWE-Welt geht verloren (Kalender, Ranglisten, Champions, "
       "Rivalit\xC3\xA4ten).\nVorher wird eine Kopie deines Spielstands erstellt (Launcher, Seite "
       "SAVES).\nFortfahren?",
       "Se perder\xC3\xA1 todo el progreso del Universo WWE (calendario, clasificaciones, campeones, "
       "rivalidades).\nAntes se hace una copia de tu partida (launcher, p\xC3\xA1gina "
       "SAVES).\n\xC2\xBF" "Continuar?",
       "Tutti i progressi del WWE Universe andranno persi (calendario, classifiche, campioni, "
       "rivalit\xC3\xA0).\nPrima viene fatta una copia del tuo salvataggio (launcher, pagina "
       "SAVES).\nContinuare?"},
      // done
      {"The WWE Universe has been reset. A backup of your save was made first.\nLeave WWE "
       "Universe and come back to see it. The reset is saved the next time the game saves "
       "(after your next show).",
       "L'Univers WWE a \xC3\xA9t\xC3\xA9 remis \xC3\xA0 z\xC3\xA9ro. Une copie de votre "
       "sauvegarde a \xC3\xA9t\xC3\xA9 faite avant.\nQuittez l'Univers WWE et revenez-y pour le "
       "voir. La remise \xC3\xA0 z\xC3\xA9ro sera enregistr\xC3\xA9" "e \xC3\xA0 la prochaine "
       "sauvegarde du jeu (apr\xC3\xA8s votre prochaine \xC3\xA9mission).",
       "Die WWE-Welt wurde zur\xC3\xBC" "ckgesetzt. Vorher wurde eine Kopie deines Spielstands "
       "erstellt.\nVerlasse die WWE-Welt und kehre zur\xC3\xBC" "ck, um sie zu sehen. Das "
       "Zur\xC3\xBC" "cksetzen wird beim n\xC3\xA4" "chsten Speichern des Spiels gesichert (nach "
       "deiner n\xC3\xA4" "chsten Show).",
       "El Universo WWE se ha reiniciado. Antes se hizo una copia de tu partida.\nSal del "
       "Universo WWE y vuelve a entrar para verlo. El reinicio se guardar\xC3\xA1 la pr\xC3\xB3xima "
       "vez que el juego guarde (tras tu pr\xC3\xB3ximo show).",
       "Il WWE Universe \xC3\xA8 stato azzerato. Prima \xC3\xA8 stata fatta una copia del tuo "
       "salvataggio.\nEsci dal WWE Universe e rientra per vederlo. L'azzeramento sar\xC3\xA0 "
       "salvato al prossimo salvataggio del gioco (dopo il tuo prossimo show)."},
      // the one-time "new" hint (shown on the Universe menu once, see below)
      {"New: you can now reset your WWE Universe.\nChoose EDIT UNIVERSE in this menu (a backup "
       "of your save is made first).",
       "Nouveau : vous pouvez remettre votre Univers WWE \xC3\xA0 z\xC3\xA9ro.\nChoisissez "
       "MODIFIER L'UNIVERS dans ce menu (une copie de votre sauvegarde est faite avant).",
       "Neu: Du kannst deine WWE-Welt jetzt zur\xC3\xBC" "cksetzen.\nW\xC3\xA4hle WELT BEARBEITEN "
       "in diesem Men\xC3\xBC (vorher wird eine Kopie deines Spielstands erstellt).",
       "Novedad: ahora puedes reiniciar tu Universo WWE.\nElige EDITAR UNIVERSO en este "
       "men\xC3\xBA (antes se hace una copia de tu partida).",
       "Novit\xC3\xA0: ora puoi azzerare il tuo WWE Universe.\nScegli MODIFICA UNIVERSE in questo "
       "menu (prima viene fatta una copia del tuo salvataggio)."},
      // VACANT: the question (a title no show uses, chosen again for its own champion)
      {"Vacate this title?\nNo show uses it, so it stays vacant until you give it to a Superstar.",
       "Rendre ce titre vacant ?\nAucune \xC3\xA9mission ne l'utilise : il restera vacant jusqu'\xC3\xA0 "
       "ce que vous le donniez \xC3\xA0 un Superstar.",
       "Diesen Titel vakant machen?\nKeine Show nutzt ihn: Er bleibt vakant, bis du ihn einem "
       "Superstar gibst.",
       "\xC2\xBF" "Dejar este t\xC3\xADtulo vacante?\nNing\xC3\xBAn show lo usa: quedar\xC3\xA1 vacante "
       "hasta que se lo des a una Superstar.",
       "Rendere vacante questo titolo?\nNessuno show lo usa: rester\xC3\xA0 vacante finch\xC3\xA9 non "
       "lo darai a una Superstar."},
      // VACANT: done
      {"The title is now vacant.", "Le titre est maintenant vacant.", "Der Titel ist jetzt vakant.",
       "El t\xC3\xADtulo ahora est\xC3\xA1 vacante.", "Il titolo ora \xC3\xA8 vacante."},
  };
  size_t row;
  switch (id) {
  case kEditSuperstarText: row = 0; break;
  case kAskText: row = 1; break;
  case kConfirmText: row = 2; break;
  case kDoneText: row = 3; break;
  case kHintText: row = 4; break;
  case kVacateAskText: row = 5; break;
  case kVacateDoneText: row = 6; break;
  default: return nullptr;
  }
  return kTexts[row][CurrentLang()];
}

// Our texts as guest strings (the game draws the text the string manager returns), one 512-byte
// slot per text, rewritten when the language changes.
u32 GuestText(u32 id) {
  constexpr u32 kSlot = 512;
  const char *text = Text(id);
  if (!text)
    return 0;
  static u32 s_block = 0;
  static const char *s_for[7] = {};
  if (!s_block)
    s_block = REX_KERNEL_MEMORY()->SystemHeapAlloc(7 * kSlot, 0x20);
  if (!s_block)
    return 0;
  const u32 slot = id == kEditSuperstarText ? 0 : id - kAskText + 1;
  const u32 at = s_block + slot * kSlot;
  if (s_for[slot] != text) {
    std::memset(bd::mem::at<char>(at), 0, kSlot);
    std::memcpy(bd::mem::at<char>(at), text, std::min<size_t>(std::strlen(text), kSlot - 1));
    s_for[slot] = text;
  }
  return at;
}

// ---- backup ---------------------------------------------------------------------------------

// Before a reset, the save on disk is backed up exactly as the launcher's SAVES page does it
// (port/src/launcher/saves.h): Documents\wwe12\SaveBackups\<date>\ mirrors the save package
// folder (B13EBABEBABEBABE\54510890) and the achievements file, with backup.txt = "auto" then a
// label; only the latest 10 automatic backups are kept. It appears on the SAVES page and restores
// from there.
bool BackupSave() {
  namespace fs = std::filesystem;
  auto *state = REX_KERNEL_STATE();
  if (!state || !state->emulator())
    return false;
  const fs::path root = state->emulator()->user_data_root();
  if (root.empty())
    return false;
  const fs::path save_dir = root / "B13EBABEBABEBABE" / "54510890";
  const fs::path ach_file = root / "achievements" / "54510890.toml";
  const fs::path backups = root / "SaveBackups";
  std::error_code ec;
  const bool has_save = fs::exists(save_dir, ec), has_ach = fs::exists(ach_file, ec);
  if (!has_save && !has_ach)
    return true;  // nothing on disk yet: nothing to lose
  const std::time_t now = std::time(nullptr);
  std::tm tm{};
  localtime_s(&tm, &now);
  char stamp[32];
  std::strftime(stamp, sizeof(stamp), "%Y-%m-%d_%H-%M-%S", &tm);
  std::string name = stamp;
  for (int n = 2; fs::exists(backups / name, ec); ++n)
    name = std::string(stamp) + "_" + std::to_string(n);
  const fs::path dest = backups / name;
  fs::create_directories(dest, ec);
  if (!ec && has_save) {
    fs::create_directories(dest / "B13EBABEBABEBABE", ec);
    if (!ec)
      fs::copy(save_dir, dest / "B13EBABEBABEBABE" / "54510890",
               fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
  }
  if (!ec && has_ach) {
    fs::create_directories(dest / "achievements", ec);
    if (!ec)
      fs::copy_file(ach_file, dest / "achievements" / ach_file.filename(),
                    fs::copy_options::overwrite_existing, ec);
  }
  if (ec) {
    BD_INFO("[univers] backup failed: {}", ec.message());
    std::error_code ignore;
    fs::remove_all(dest, ignore);
    return false;
  }
  std::ofstream(dest / "backup.txt") << "auto\nBefore RESET UNIVERSE\n";
  // Keep the latest 10 automatic backups (names sort by date), as the launcher does.
  std::vector<std::string> automatic;
  for (const auto &e : fs::directory_iterator(backups, ec)) {
    std::string kind;
    std::ifstream info(e.path() / "backup.txt");
    if (e.is_directory() && std::getline(info, kind) && kind == "auto")
      automatic.push_back(e.path().filename().string());
  }
  std::sort(automatic.rbegin(), automatic.rend());
  for (size_t i = 10; i < automatic.size(); ++i)
    fs::remove_all(backups / automatic[i], ec);
  BD_INFO("[univers] save backed up to {}", dest.string());
  return true;
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

enum class Step { kIdle, kAsk, kConfirm, kDone, kHint, kVacateAsk, kVacateDone };
Step g_step = Step::kIdle;
u32 g_screen = 0;  // the Universe menu screen the dialogue belongs to
u32 g_answer = 0;  // guest word for the message box's answer
u32 g_vacate_title = 0;  // the title the VACANT question is about

// The one-time hint: the first time the Universe menu shows (2 s in, no other message box open),
// a message says the reset exists and where. Documents\wwe12\universe_reset_hint.txt remembers
// it was shown.
std::filesystem::path HintFile() {
  auto *state = REX_KERNEL_STATE();
  if (!state || !state->emulator() || state->emulator()->user_data_root().empty())
    return {};
  return state->emulator()->user_data_root() / "universe_reset_hint.txt";
}

int g_hint_pending = -1;  // -1: not checked yet

bool HintPending() {
  if (g_hint_pending < 0) {
    const std::filesystem::path file = HintFile();
    std::error_code ec;
    g_hint_pending = !file.empty() && !std::filesystem::exists(file, ec);
  }
  return g_hint_pending == 1;
}

void HintShown() {
  g_hint_pending = 0;
  const std::filesystem::path file = HintFile();
  if (!file.empty())
    std::ofstream(file) << "The RESET UNIVERSE hint was shown.\n";
}

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
  if (Enabled() && (id == kEditSuperstarText || (id >= kAskText && id <= kVacateDoneText)))
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

// ---- VACANT (a title no show uses) --------------------------------------------------------
//
// A Superstar's page has a grid of belts (screen vtable 0x82029BB8). Its action, sub_825B47B0
// (grid = r3, its owner = grid - 28): the chosen cell is owner+(118 + grid[+640])*4 (cells:
// owner+656 count), a cell = {title, place}; the edited Superstar is grid+256. It reads the
// cell's holder (sub_82DEA428 = sub_82A39FC8(title, place, 1)); when that is already the edited
// Superstar it does nothing, otherwise it gives the title (sub_82DEA418 = sub_82A3A060), or asks
// for a partner first for a tag title. As in SvR 2011, choosing the belt a Superstar already holds
// now offers to vacate it, for a title no weekly show uses (each show's record, U + 27360 +
// show * 984, has at +904 one byte per title: 0 = unused, 1-4 = MAJOR / MINOR / TAG / DIVA, as
// sub_826CC180 reads it). Universe only (*0x8324E51C == 1: the Universe champions table).
namespace {

constexpr u32 kBeltGridVtbl = 0x82029BB8;
constexpr u32 kInUniverseFlag = 0x8324E51C;

bool TitleUsedByAShow(const PPCContext &in, u8 *base, u32 title) {
  const u32 universe = Call(in, base, __imp__sub_82683A30, 0);
  if (!universe)
    return true;  // unknown: leave it alone
  for (u32 show = 0; show < 10; ++show)
    if (bd::mem::load<u8>(universe + 27360 + show * 984 + 904 + title))
      return true;
  return false;
}

}  // namespace

REX_EXTERN(__imp__sub_825B47B0);  // the belt grid's action
REX_EXTERN(__imp__sub_82A39FC8);  // holder (title, place, universe check)
REX_EXTERN(__imp__sub_82A3A060);  // set holder (title, holder, place)
extern "C" REX_FUNC(sub_825B47B0) {
  const u32 grid = ctx.r3.u32;
  if (Enabled() && g_step == Step::kIdle && bd::mem::try_load<u32>(grid) == kBeltGridVtbl &&
      bd::mem::load<u32>(kInUniverseFlag) == 1) {
    const u32 owner = grid - 28;
    const int k = int(bd::mem::load<u32>(grid + 640));
    const int count = int(bd::mem::load<u32>(owner + 656));
    const u32 cell = (k >= 0 && k < count) ? bd::mem::load<u32>(owner + (118 + k) * 4) : 0;
    if (cell) {
      const u32 title = bd::mem::load<u32>(cell), place = bd::mem::load<u32>(cell + 4);
      const u32 edited = bd::mem::load<u32>(grid + 256);
      if (title < kTitleCount && Call(ctx, base, __imp__sub_82A39FC8, title, place, 1) == edited &&
          !TitleUsedByAShow(ctx, base, title)) {
        g_screen = grid;
        g_vacate_title = title;
        g_step = Step::kVacateAsk;
        Call(ctx, base, __imp__sub_8254E7E8, kVacateAskText);
        return;
      }
    }
  }
  if (g_step != Step::kIdle && grid == g_screen)
    return;  // our question is open
  __imp__sub_825B47B0(ctx, base);
}

namespace {

void VacateTitle(const PPCContext &in, u8 *base, u32 title) {
  Call(in, base, __imp__sub_82A3A060, title, kNobody, 0);
  Call(in, base, __imp__sub_82A3A060, title, kNobody, 1);
  BD_INFO("[univers] title {} vacated (no show uses it)", title);
}

}  // namespace

// Every layout menu screen's per-frame update: drives the dialogue while it is open (the screen
// itself keeps updating; its A button waits, see above).
REX_EXTERN(__imp__sub_824D3458);
REX_EXTERN(__imp__sub_8254E7D0);  // message box: idle (none open)?
extern "C" REX_FUNC(sub_824D3458) {
  // The one-time hint, 120 frames (2 s) after the Universe menu shows.
  if (Enabled() && g_step == Step::kIdle && HintPending() &&
      bd::mem::try_load<u32>(ctx.r3.u32) == kUniverseMenuVtbl) {
    static u32 s_screen = 0, s_frames = 0;
    if (s_screen != ctx.r3.u32) {
      s_screen = ctx.r3.u32;
      s_frames = 0;
    }
    if (++s_frames >= 120 && Call(ctx, base, __imp__sub_8254E7D0, 0)) {
      g_screen = ctx.r3.u32;
      g_step = Step::kHint;
      Call(ctx, base, __imp__sub_8254E680, kHintText);
      HintShown();
    }
  }
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
      case Step::kVacateAsk:
        if (yes) {
          VacateTitle(ctx, base, g_vacate_title);
          g_step = Step::kVacateDone;
          Call(ctx, base, __imp__sub_8254E680, kVacateDoneText);
        } else {
          g_step = Step::kIdle;
        }
        break;
      case Step::kConfirm:
        if (yes && BackupSave() && ResetUniverse(ctx, base)) {
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

// ---- dev probe (wwe12_universe_probe) -------------------------------------------------------

namespace {

// Return addresses up the guest stack (checked reads: the walk ends at the top of the stack).
void LogChain(const char *what, const PPCContext &ctx) {
  u32 chain[8] = {u32(ctx.lr)};
  u32 frame = ctx.r1.u32;
  for (int i = 1; i < 8 && frame; ++i) {
    const u32 back = bd::mem::try_load<u32>(frame);
    if (!back || back <= frame || back - frame > 0x10000)
      break;
    chain[i] = bd::mem::try_load<u32>(back - 8);
    if (!chain[i])
      break;
    frame = back;
  }
  BD_INFO("[univers] {} from {:08X} <- {:08X} <- {:08X} <- {:08X} <- {:08X} <- {:08X} <- {:08X} <- "
          "{:08X}",
          what, chain[0], chain[1], chain[2], chain[3], chain[4], chain[5], chain[6], chain[7]);
}

}  // namespace

// The A button of the layout menus: the screen, its vtable, its action and first words.
REX_EXTERN(__imp__sub_824D1FB0);
extern "C" REX_FUNC(sub_824D1FB0) {
  if (REXCVAR_GET(wwe12_universe_probe)) {
    const u32 screen = ctx.r3.u32;
    const u32 vtbl = bd::mem::try_load<u32>(screen);
    std::string words;
    for (u32 i = 0; i < 96; ++i)
      words += fmt::format(" {:08X}", bd::mem::try_load<u32>(screen + i * 4));
    BD_INFO("[univers] A: screen 0x{:08X} vtbl 0x{:08X} action 0x{:08X} | words:{}", screen, vtbl,
            bd::mem::try_load<u32>(vtbl + 72), words);
  }
  __imp__sub_824D1FB0(ctx, base);
}

// A menu sound's name is printed into its object (sub_8270EEB0(object, format, ...), name at
// object+8): "Decision_8000_0_0" when an entry is chosen.
REX_EXTERN(__imp__sub_8270EEB0);
extern "C" REX_FUNC(sub_8270EEB0) {
  const u32 object = ctx.r3.u32;
  const PPCContext in = ctx;
  __imp__sub_8270EEB0(ctx, base);
  if (!REXCVAR_GET(wwe12_universe_probe) || !object)
    return;
  const char *name = bd::mem::try_at<const char>(object + 8);
  if (name && (std::strncmp(name, "Decision", 8) == 0 || std::strncmp(name, "Cancel", 6) == 0))
    LogChain(name[0] == 'D' ? "cue Decision" : "cue Cancel", in);
}
