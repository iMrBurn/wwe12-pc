// WWE '12: Universe mode, RESET UNIVERSE (as in WWE '13). See docs/univers.md.
//
// The Universe menu's EDIT SUPERSTAR entry is labelled EDIT UNIVERSE. Choosing it opens, as in
// WWE '13, a sub-menu in the game's own choice box: EDIT SUPERSTARS (the Superstar editor, as
// before), EDIT TITLES (the game's own TITLE MANAGEMENT screen of OPTIONS > TITLE SETTINGS, here on
// the Universe champions) and RESET UNIVERSE (asks once more, backs the save up in the launcher's
// format, SAVES page, then resets the Universe to the one a new save starts with; the reset lives in
// memory until the game next saves, after a show, and the messages say so). On by default
// (wwe12_universe_edit = false turns it off).
//
// The choice box: the message box (*0x8324DEF0) has a list mode (request kind 4: text id, then at
// +24 the number of choices and at +28 an array of string pointers), opened by sub_8260DD80(-, text
// id, choices, count, 0, 0) as the game's own "Do Not Review / Remind Me Later" box does; once closed,
// the answer (sub_8254E8C8) is the chosen line, from 0.
//
// TITLE MANAGEMENT (menu.pac record "TITLE SETTINGS", link 0x52E): the main menu's scene for it
// (sub_825477E8) makes a 2 560-byte object (operator new sub_821A0000) with sub_82564548(object,
// pad), which registers itself as a layout screen (updated and drawn on its own); it is finished when
// the word at object+228+16 is 5, then destroyed with sub_82565A30(object, 1). Its rows ({holder,
// title, place, ...}, 24 bytes from object+376) come from sub_82A39FC8(title, place, 1), which reads
// the Universe champions table in the Universe; on leaving it asks to keep the changes, writes them
// with sub_82A3A060 and saves the game to disk. Its X button (VACATE TITLE, vtable slot sub_82565390)
// keeps our limit in the Universe: no VACANT for a title a weekly show uses.
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
#include <cmath>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <map>
#include <random>
#include <string>

#include <fmt/format.h>
#include <vector>

#include <rex/cvar.h>
#include <rex/runtime.h>

#include "core/logging.h"
#include "core/memory_helpers.h"
#include "svr_hook.h"
#include "wwe12_graphics_menu.h"

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
REX_EXTERN(__imp__sub_8260DD80);  // message box: choice list (-, text id, choices, count, 0, 0)
REX_EXTERN(__imp__sub_8253E240);  // main menu manager: active pad
REX_EXTERN(__imp__sub_821A0000);  // operator new (size)
REX_EXTERN(__imp__sub_82564548);  // TITLE MANAGEMENT: constructor (object, pad)
REX_EXTERN(__imp__sub_82565A30);  // TITLE MANAGEMENT: destructor (object, 1 = free)
REX_EXTERN(__imp__sub_828942C0);  // button bar: prompt set (9 = A SELECT, B BACK)

namespace {

constexpr u32 kUniverseMenuVtbl = 0x82021F98;
constexpr u32 kEditSuperstarEntry = 4;
constexpr u32 kEditSuperstarText = 27045;
constexpr u32 kAskText = 27026, kConfirmText = 27027, kDoneText = 27028, kHintText = 27029;
constexpr u32 kVacateAskText = 27030, kVacateDoneText = 27031;
constexpr u32 kTitleUsedText = 27032;
// The sub-menu's lines (EDIT SUPERSTARS, EDIT TITLES, RESET UNIVERSE), 27033-27035: the last of
// our texts (27026-27040 are unused in WWE '12).
constexpr u32 kChoiceText = 27033, kChoiceCount = 3;
// The attire box (X on a Superstar's page): its question (with the current choice), "more" for a
// second page, and the message for a Superstar with a single attire.
constexpr u32 kAttireText = 27036, kAttireMoreText = 27037, kAttireOnlyText = 27038;
// The Superstar page's button bar: its "DEFAULT" (text 45087) followed by "Y ATTIRE" (U+E007 = the
// Y button's icon, drawn by the game as Y or triangle depending on the pad, as in its own
// "Y COPY").
constexpr u32 kAttirePromptText = 27039;
constexpr u32 kDefaultPromptText = 45087;
constexpr u32 kLastText = kAttirePromptText;
enum Choice : u32 { kEditSuperstars, kEditTitles, kResetUniverse };

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

// The attire box's question, written when the box opens (the current choice in it).
std::string g_attire_question;
// The Superstar page's last button bar label ("DEFAULT   Y ATTIRE"), written when the page shows.
std::string g_attire_prompt;

// The entry's label says what it is (the user worried players would not find the reset under a
// bare "UNIVERS"); it fits the menu (LISTE DES MATCHS is 16 characters).
const char *Text(u32 id) {
  static const char *const kTexts[][5] = {
      // EDIT SUPERSTAR's new label
      {"EDIT UNIVERSE", "MODIFIER L'UNIVERS", "WELT BEARBEITEN", "EDITAR UNIVERSO",
       "MODIFICA UNIVERSE"},
      // the sub-menu's question
      {"EDIT UNIVERSE\nWhat do you want to edit?", "MODIFIER L'UNIVERS\nQue voulez-vous modifier ?",
       "WELT BEARBEITEN\nWas m\xC3\xB6" "chtest du bearbeiten?",
       "EDITAR UNIVERSO\n\xC2\xBF" "Qu\xC3\xA9 quieres editar?",
       "MODIFICA UNIVERSE\nCosa vuoi modificare?"},
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
      // TITLE MANAGEMENT in the Universe: X on a title a show uses
      {"A weekly show uses this title, so it cannot be vacant.\nChange the show's titles first "
       "(SCHEDULE, X on the show).",
       "Une \xC3\xA9mission utilise ce titre : il ne peut pas \xC3\xAAtre vacant.\nChangez d'abord "
       "les titres de l'\xC3\xA9mission (CALENDRIER, X sur l'\xC3\xA9mission).",
       "Eine Show nutzt diesen Titel, daher kann er nicht vakant werden.\n\xC3\x84ndere zuerst die "
       "Titel der Show (KALENDER, X auf der Show).",
       "Un show usa este t\xC3\xADtulo, as\xC3\xAD que no puede quedar vacante.\nCambia antes los "
       "t\xC3\xADtulos del show (CALENDARIO, X sobre el show).",
       "Uno show usa questo titolo, quindi non pu\xC3\xB2 restare vacante.\nCambia prima i titoli "
       "dello show (CALENDARIO, X sullo show)."},
      // the sub-menu's lines
      {"EDIT SUPERSTARS", "MODIFIER LES SUPERSTARS", "SUPERSTARS BEARBEITEN", "EDITAR SUPERSTARS",
       "MODIFICA SUPERSTAR"},
      {"EDIT TITLES", "MODIFIER LES TITRES", "TITEL BEARBEITEN", "EDITAR T\xC3\x8DTULOS",
       "MODIFICA TITOLI"},
      {"RESET UNIVERSE", "R\xC3\x89INITIALISER L'UNIVERS", "WELT ZUR\xC3\x9C" "CKSETZEN",
       "REINICIAR UNIVERSO", "AZZERA UNIVERSE"},
      // the attire box's question: "%s" = the current choice
      {"UNIVERSE ATTIRE\nCurrent: %s", "TENUE DANS L'UNIVERS\nActuelle : %s",
       "OUTFIT IN DER WWE-WELT\nAktuell: %s", "ATUENDO EN EL UNIVERSO\nActual: %s",
       "COSTUME NEL WWE UNIVERSE\nAttuale: %s"},
      // the attire box's next page
      {"MORE...", "PLUS...", "MEHR...", "M\xC3\x81S...", "ALTRO..."},
      // a Superstar with one attire
      {"This Superstar has a single attire.", "Ce Superstar n'a qu'une tenue.",
       "Dieser Superstar hat nur ein Outfit.", "Esta Superstar solo tiene un atuendo.",
       "Questa Superstar ha un solo costume."},
      // the Superstar page's button bar, after the game's "DEFAULT"
      {"%s   \xEE\x80\x87 ATTIRE", "%s   \xEE\x80\x87 TENUE", "%s   \xEE\x80\x87 OUTFIT",
       "%s   \xEE\x80\x87 ATUENDO", "%s   \xEE\x80\x87 COSTUME"},
  };
  if (id == kAttireText && !g_attire_question.empty())
    return g_attire_question.c_str();
  if (id == kAttirePromptText && !g_attire_prompt.empty())
    return g_attire_prompt.c_str();
  size_t row;
  if (id == kEditSuperstarText)
    row = 0;
  else if (id >= kAskText && id <= kLastText)
    row = id - kAskText + 1;
  else
    return nullptr;
  return kTexts[row][CurrentLang()];
}

// Our texts as guest strings (the game draws the text the string manager returns), one 512-byte
// slot per text, rewritten when the language changes.
u32 GuestText(u32 id) {
  constexpr u32 kSlot = 512;
  constexpr u32 kSlots = 1 + kLastText - kAskText + 1;
  const char *text = Text(id);
  if (!text)
    return 0;
  static u32 s_block = 0;
  static const char *s_for[kSlots] = {};
  if (!s_block)
    s_block = REX_KERNEL_MEMORY()->SystemHeapAlloc(kSlots * kSlot, 0x20);
  if (!s_block)
    return 0;
  const u32 slot = id == kEditSuperstarText ? 0 : id - kAskText + 1;
  const u32 at = s_block + slot * kSlot;
  if (s_for[slot] != text || id == kAttireText || id == kAttirePromptText) {
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

// Calls a guest function with up to six arguments, on a copy of the hooked call's context.
template <typename Fn>
u32 Call(const PPCContext &in, u8 *base, Fn fn, u32 a, u32 b = 0, u32 c = 0, u32 d = 0, u32 e = 0,
         u32 f = 0) {
  PPCContext x = in;
  x.r3.u64 = a;
  x.r4.u64 = b;
  x.r5.u64 = c;
  x.r6.u64 = d;
  x.r7.u64 = e;
  x.r8.u64 = f;
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

enum class Step { kIdle, kMenu, kConfirm, kDone, kHint, kVacateAsk, kVacateDone, kTitles, kAttire };
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

// The message box closed? (answer: 1 = yes for a question, the chosen line for a choice list)
bool BoxClosed(const PPCContext &in, u8 *base, u32 &answer) {
  if (!g_answer)
    g_answer = REX_KERNEL_MEMORY()->SystemHeapAlloc(16, 0x10);
  answer = 0xFFFFFFFFu;
  if (!g_answer)
    return true;
  bd::mem::store<u32>(g_answer, 0xFFFFFFFFu);
  if (!Call(in, base, __imp__sub_8254E8C8, g_answer))
    return false;
  answer = bd::mem::load<u32>(g_answer);
  return true;
}

// The sub-menu: the game's choice box with our three lines (guest array of string pointers).
void AskMenu(const PPCContext &in, u8 *base) {
  static u32 s_choices = 0;
  if (!s_choices)
    s_choices = REX_KERNEL_MEMORY()->SystemHeapAlloc(kChoiceCount * 4, 0x10);
  if (!s_choices)
    return;
  for (u32 i = 0; i < kChoiceCount; ++i)
    bd::mem::store<u32>(s_choices + i * 4, GuestText(kChoiceText + i));
  Call(in, base, __imp__sub_8260DD80, 0, kAskText, s_choices, kChoiceCount, 0, 0);
}

// ---- EDIT TITLES: the game's TITLE MANAGEMENT screen ------------------------------------------

constexpr u32 kTitleScreenSize = 2560;
constexpr u32 kMainMenuManager = 0x8317C594;  // the main menu's manager (its active pad)
u32 g_titles = 0;                             // the TITLE MANAGEMENT object while it is open
u32 g_menu_frames = 0;                        // frames since the sub-menu opened
u32 g_box_lines = 0;    // lines of the choice box shown (3: the third line is placed by hand)

bool OpenTitles(const PPCContext &in, u8 *base) {
  const u32 manager = bd::mem::load<u32>(kMainMenuManager);
  const u32 pad = manager ? Call(in, base, __imp__sub_8253E240, manager) : 0;
  const u32 object = Call(in, base, __imp__sub_821A0000, kTitleScreenSize);
  if (!object) {
    BD_INFO("[univers] titles: no memory for TITLE MANAGEMENT");
    return false;
  }
  Call(in, base, __imp__sub_82564548, object, pad);
  g_titles = object;
  BD_INFO("[univers] titles: TITLE MANAGEMENT opened (0x{:08X}, pad {})", object, pad);
  return true;
}

// Done once its state (object+228+16) is 5, as the main menu's scene checks it.
bool TitlesFinished() { return bd::mem::load<u32>(g_titles + 228 + 16) == 5; }

void CloseTitles(const PPCContext &in, u8 *base) {
  Call(in, base, __imp__sub_82565A30, g_titles, 1);
  // The button bar keeps TITLE MANAGEMENT's prompts (X VACATE TITLE): back to the Universe
  // menu's set, 9 (A SELECT, B BACK), as the menu itself sets it (sub_825463E8).
  Call(in, base, __imp__sub_828942C0, 9);
  BD_INFO("[univers] titles: TITLE MANAGEMENT closed");
  g_titles = 0;
}

// ---- a third line in the choice box -----------------------------------------------------------
//
// The box's layout (layout = box+1952; elements of 240 bytes in an array at +8, count at +24, as
// sub_824C5458 reads them) was read with wwe12_universe_probe on 2026-10-20. Element fields: +0
// kind, +4 x, +8 y (in the parent's units), +12/+16 scale, +80 font name, +176 parent, +180
// hidden, +184..+196 colour (RGBA floats; the cursor's line is red), +200 text.
// - 0xD2 / 0xD3: the two choice lines (font001, y -8 / +8);
// - 0xD6 / 0xD7: their red underlines (red_line01, y -2 / +14), shown for the cursor's line;
// - a third choice goes to 0xD4, a smaller text of another group (font000, scale 0.7) drawn over
//   the first line, and its "underline" is 0xD8, a textless element: nothing shows.
// While our sub-menu is open, 0xD4 becomes a copy of 0xD3 one step lower (its text and colour
// stay the game's), and the three lines with their underlines move up 3/4 of a step into the
// empty line under the question, so that the third stays inside the box. The cursor code
// (sub_8288F0A0) shows underline 0xD6 + cursor, so 0xD8 (textless) for the third line: there, the
// second line's underline 0xD7 is moved under the third line instead (an underline of another
// group, such as the yes/no boxes' 0xDC, stays hidden: tested). The elements are put back as they
// were when the box closes.

constexpr u32 kMessageBox = 0x8324DEF0;
constexpr u32 kLine1 = 0xD2, kLine2 = 0xD3, kLine3 = 0xD4;
constexpr u32 kUnder1 = 0xD6, kUnder2 = 0xD7, kUnder3State = 0xD8;

u32 BoxElement(u32 id) {
  const u32 box = bd::mem::load<u32>(kMessageBox);
  const u32 layout = box ? bd::mem::load<u32>(box + 1952) : 0;
  const u32 array = layout ? bd::mem::load<u32>(layout + 8) : 0;
  if (!array || id >= bd::mem::load<u32>(layout + 24))
    return 0;
  return array + id * 240;
}

struct SavedElement {
  u32 id = 0, at = 0;
  u8 bytes[240] = {};
};
SavedElement g_saved[6];
bool g_saved_ok = false;

const SavedElement &Saved(u32 id) {
  for (const SavedElement &e : g_saved)
    if (e.id == id)
      return e;
  return g_saved[0];
}
float SavedFloat(u32 id, u32 o) {
  u32 v;
  std::memcpy(&v, Saved(id).bytes + o, 4);
  v = _byteswap_ulong(v);
  float f;
  std::memcpy(&f, &v, 4);
  return f;
}

// An element made a copy of a saved one, except the fields listed (kept).
void CopyElement(u32 to, u32 from_id, std::initializer_list<u32> keep) {
  const u8 *src = Saved(from_id).bytes;
  for (u32 o = 0; o < 240; o += 4) {
    if (std::find(keep.begin(), keep.end(), o) != keep.end())
      continue;
    std::memcpy(bd::mem::at<u8>(to + o), src + o, 4);
  }
}

void PlaceThirdChoice() {
  if (!g_saved_ok) {
    // Once the box has laid its lines out (the second line has its text).
    // 0xD8 too: it is the yes/no boxes' YES, and the cursor code leaves its transparency there.
    const u32 ids[6] = {kLine1, kLine2, kLine3, kUnder1, kUnder2, kUnder3State};
    const u32 line2 = BoxElement(kLine2);
    if (!line2 || !bd::mem::load<u32>(line2 + 200))
      return;
    for (int i = 0; i < 6; ++i) {
      g_saved[i].id = ids[i];
      g_saved[i].at = BoxElement(ids[i]);
      if (!g_saved[i].at)
        return;
      std::memcpy(g_saved[i].bytes, bd::mem::at<u8>(g_saved[i].at), 240);
    }
    g_saved_ok = true;
  }
  const float step = SavedFloat(kLine2, 8) - SavedFloat(kLine1, 8);
  const float up = step * 0.75f;
  const u32 line1 = BoxElement(kLine1), line2 = BoxElement(kLine2), line3 = BoxElement(kLine3);
  const u32 under1 = BoxElement(kUnder1), under2 = BoxElement(kUnder2);
  const u32 under3_state = BoxElement(kUnder3State);
  if (!line1 || !line2 || !line3 || !under1 || !under2 || !under3_state)
    return;
  CopyElement(line3, kLine2, {180, 184, 188, 192, 196, 200});
  bd::mem::store<float>(line1 + 8, SavedFloat(kLine1, 8) - up);
  bd::mem::store<float>(line2 + 8, SavedFloat(kLine2, 8) - up);
  bd::mem::store<float>(line3 + 8, SavedFloat(kLine2, 8) + step - up);
  bd::mem::store<float>(under1 + 8, SavedFloat(kUnder1, 8) - up);
  const u32 box = bd::mem::load<u32>(kMessageBox);
  if (box && bd::mem::load<u32>(box + 56) == 2) {  // the cursor on the third line
    bd::mem::store<u32>(under2 + 180, bd::mem::load<u32>(under3_state + 180));
    bd::mem::store<u32>(under2 + 196, bd::mem::load<u32>(under3_state + 196));
    bd::mem::store<float>(under2 + 8, SavedFloat(kUnder2, 8) + step - up);
  } else {
    bd::mem::store<float>(under2 + 8, SavedFloat(kUnder2, 8) - up);
  }
}

void RestoreChoiceElements() {
  if (g_saved_ok)
    for (const SavedElement &e : g_saved)
      std::memcpy(bd::mem::at<u8>(e.at), e.bytes, 240);
  g_saved_ok = false;
}

// Dev (wwe12_universe_probe): the choice elements, to see what the box's layout holds.
void DumpChoiceElements(const char *when) {
  for (u32 id = 0xC8; id <= 0xE8; ++id) {
    const u32 e = BoxElement(id);
    if (!e)
      continue;
    std::string words;
    for (u32 o = 0; o < 240; o += 4)
      words += fmt::format(" {:08X}", bd::mem::load<u32>(e + o));
    BD_INFO("[univers] box {} element 0x{:02X} @0x{:08X}:{}", when, id, e, words);
  }
}

}  // namespace

// The string manager's "text of id": EDIT SUPERSTAR's new label and our questions.
REX_EXTERN(__imp__sub_821A4400);
extern "C" REX_FUNC(sub_821A4400) {
  const u32 id = ctx.r4.u32;
  __imp__sub_821A4400(ctx, base);
  if (Enabled() && (id == kEditSuperstarText || (id >= kAskText && id <= kLastText))) {
    if (const u32 text = GuestText(id))
      ctx.r3.u64 = text;
  } else if (const u32 text = Wwe12NativeGraphicsGuestText(id)) {
    ctx.r3.u64 = text;  // OPTIONS > GRAPHICS in the game's menus (wwe12_graphics_menu.cpp)
  }
}

// BATL/SAVE read end (this, pack): keep the Universe's start data.
REX_EXTERN(__imp__sub_826EC008);
extern "C" REX_FUNC(sub_826EC008) {
  const PPCContext in = ctx;
  __imp__sub_826EC008(ctx, base);
  KeepStartData(in, base, in.r4.u32);
}

// The Universe menu's action (A on an entry): EDIT UNIVERSE opens the sub-menu first.
REX_EXTERN(__imp__sub_825453D8);
extern "C" REX_FUNC(sub_825453D8) {
  const u32 screen = ctx.r3.u32;
  if (Enabled() && g_step == Step::kIdle && bd::mem::try_load<u32>(screen) == kUniverseMenuVtbl &&
      bd::mem::try_load<u32>(screen + 28) == kEditSuperstarEntry) {
    g_screen = screen;
    g_step = Step::kMenu;
    g_box_lines = kChoiceCount;
    g_menu_frames = 0;
    g_saved_ok = false;  // the box lays its lines out again: take them before moving them
    AskMenu(ctx, base);
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

// TITLE MANAGEMENT's X button (VACATE TITLE, this = the screen object): the chosen row is
// [+28] + [+2520] (cursor + scroll), its title at +380 + row * 24. In the Universe, a title a
// weekly show uses stays: the game's message box says why.
REX_EXTERN(__imp__sub_82565390);
extern "C" REX_FUNC(sub_82565390) {
  const u32 screen = ctx.r3.u32;
  if (Enabled() && bd::mem::load<u32>(kInUniverseFlag) == 1) {
    const u32 row = bd::mem::load<u32>(screen + 28) + bd::mem::load<u32>(screen + 2520);
    const u32 title = row < 64 ? bd::mem::load<u32>(screen + 380 + row * 24) : kTitleCount;
    if (title < kTitleCount && TitleUsedByAShow(ctx, base, title)) {
      BD_INFO("[univers] titles: VACATE refused, a show uses title {}", title);
      Call(ctx, base, __imp__sub_8254E680, kTitleUsedText);
      return;
    }
  }
  __imp__sub_82565390(ctx, base);
}

// ---- Universe attires (as in WWE 2K14) --------------------------------------------------------
//
// A Universe match always starts with every Superstar in attire 0: sub_826BE900(this, M, match, 1)
// fills the match-select object M (*0x8324DEFC, shared with the character select) from the card,
// and its setter sub_828A9E50 resets each place's attire. Places: M + [M+15960] * 1596 + 124 + i *
// 244 (+0 id, +16 attire, +20 Superstar Threads slot or -1, +44 controller). After the card is
// loaded, each Superstar gets the attire chosen for him here (or one of his attires at random),
// set the way the character select does it (sub_825AE158): sub_828AA050(M, place, attire) and
// sub_828AA070(M, place, threads slot). The player can still change it in EDIT PARTICIPANTS.
//
// His attires, listed as the character select lists them (sub_825ADD48):
// - ORIGINAL (string 6571): attire 0, slot -1;
// - the game's attires: table T = *0x8324E524, [T+3200] entries of 40 bytes {id, count, 4 x {name
//   string id, unlock}}; sub_82A41D50(T, id, 0) = count (attire 0 included), sub_82A41C70(T, id, k)
//   = {name, unlock} of attire k, sub_82A3FE18(id, k) = unlocked; attire k, slot -1;
// - his Superstar Threads attires: 3 records of 108 bytes at sub_82685CD8(id) + 744 + j * 108:
//   +0 the attire it is made from (-1 = empty slot), +40 its name (text); attire [+0], slot j.
// Created Superstars (their attires are elsewhere) are left alone.
//
// The choice: Y on a Superstar's page in the Universe (screen vtable 0x82028C88; Y = button 0x40,
// handled for every layout screen by sub_824D1AF0, empty on that page, whose X is DEFAULT; the
// page's Superstar is sub_82DEA3A8()) opens the game's choice box with ORIGINAL, his attires and
// RANDOM (string 278); more than three lines go on pages ("MORE..."). Kept in
// Documents\wwe12\universe_attires.txt ("id attire slot" lines; random = "id -1 -1"), not in the
// save.

REX_EXTERN(__imp__sub_826BE900);  // Universe card -> match select (this, M, match, 1)
REX_EXTERN(__imp__sub_828AA050);  // match select: a place's attire (M, place, attire)
REX_EXTERN(__imp__sub_828AA070);  // match select: a place's Superstar Threads slot (M, place, slot)
REX_EXTERN(__imp__sub_82A41D50);  // attires: count (T, id, unlocked only)
REX_EXTERN(__imp__sub_82A41C70);  // attires: {name, unlock} of attire k (T, id, k)
REX_EXTERN(__imp__sub_82A3FE18);  // attires: attire k unlocked? (id, k)
REX_EXTERN(__imp__sub_82685CD8);  // a Superstar's record in save block 2 (Superstar Threads at +744)
REX_EXTERN(__imp__sub_82DEA3A8);  // Superstar page: the Superstar shown
REX_EXTERN(__imp__sub_824D1AF0);  // layout screens: button 0x40 (Y)
REX_EXTERN(__imp__sub_8254E920);  // the Universe screens' shared layout

namespace {

constexpr u32 kSuperstarPageVtbl = 0x82028C88;
constexpr u32 kAttireTable = 0x8324E524;
constexpr u32 kStringManager = 0x8317B86C;
constexpr u32 kOriginalText = 6571, kRandomText = 278;
constexpr u32 kFirstCreated = 50;  // Superstar ids 0-49 are created Superstars

// One line of the attire box / one attire a match can use.
struct Attire {
  int attire = 0;  // -1 = random, -2 = "more..."
  int slot = -1;   // Superstar Threads slot, or -1
  bool operator==(const Attire &o) const { return attire == o.attire && slot == o.slot; }
};
const Attire kRandom{-1, -1}, kMore{-2, -1};

std::map<u32, Attire> g_attires;  // Superstar id -> chosen attire
bool g_attires_loaded = false;

std::filesystem::path AttiresFile() {
  auto *state = REX_KERNEL_STATE();
  if (!state || !state->emulator() || state->emulator()->user_data_root().empty())
    return {};
  return state->emulator()->user_data_root() / "universe_attires.txt";
}

void LoadAttires() {
  if (g_attires_loaded)
    return;
  g_attires_loaded = true;
  std::ifstream f(AttiresFile());
  std::string line;
  while (std::getline(f, line)) {
    u32 id;
    int attire, slot;
    if (std::sscanf(line.c_str(), "%u %d %d", &id, &attire, &slot) == 3)
      g_attires[id] = {attire, slot};
  }
}

void SaveAttires() {
  const std::filesystem::path file = AttiresFile();
  if (file.empty())
    return;
  std::ofstream f(file);
  f << "# Universe attires chosen in the game (Y on a Superstar's page): Superstar id, attire, "
       "Superstar Threads slot (-1 = none). \"id -1 -1\" = random.\n";
  for (const auto &[id, a] : g_attires)
    f << id << ' ' << a.attire << ' ' << a.slot << '\n';
}

// The game's text of an id (guest address), in the player's language.
u32 GameText(const PPCContext &in, u8 *base, u32 id) {
  return Call(in, base, __imp__sub_821A4400, bd::mem::load<u32>(kStringManager), id);
}

// Every attire of a Superstar, as the character select lists them, with their names (guest texts).
std::vector<std::pair<Attire, u32>> SuperstarAttires(const PPCContext &in, u8 *base, u32 id) {
  std::vector<std::pair<Attire, u32>> out;
  out.push_back({{0, -1}, GameText(in, base, kOriginalText)});
  if (id < kFirstCreated)
    return out;
  const u32 table = bd::mem::load<u32>(kAttireTable);
  const u32 count = table ? Call(in, base, __imp__sub_82A41D50, table, id, 0) : 0;
  for (u32 k = 1; k < count && k < 8; ++k) {
    if (!Call(in, base, __imp__sub_82A3FE18, id, k))
      continue;  // locked
    const u32 info = Call(in, base, __imp__sub_82A41C70, table, id, k);
    const u32 name = info ? bd::mem::load<u32>(info) : 0;
    out.push_back({{int(k), -1}, name ? GameText(in, base, name) : 0});
  }
  if (const u32 record = Call(in, base, __imp__sub_82685CD8, id))
    for (int j = 0; j < 3; ++j) {
      const u32 threads = record + 744 + u32(j) * 108;
      const int from = int(bd::mem::load<u32>(threads));
      if (from >= 0)
        out.push_back({{from, j}, threads + 40});
    }
  return out;
}

// The attire box being shown.
u32 g_attire_id = 0;
std::vector<std::pair<Attire, u32>> g_attire_options;  // lines: attires then RANDOM
std::vector<std::pair<Attire, u32>> g_attire_page;     // the lines of the page shown
size_t g_attire_first = 0;                             // first option of the page shown

void ShowAttirePage(const PPCContext &in, u8 *base, size_t first) {
  g_attire_first = first;
  g_attire_page.clear();
  const size_t left = g_attire_options.size() - first;
  if (left <= 3) {
    g_attire_page.assign(g_attire_options.begin() + first, g_attire_options.end());
  } else {
    g_attire_page = {g_attire_options[first], g_attire_options[first + 1],
                     {kMore, GuestText(kAttireMoreText)}};
  }
  static u32 s_choices = 0;
  if (!s_choices)
    s_choices = REX_KERNEL_MEMORY()->SystemHeapAlloc(4 * 4, 0x10);
  if (!s_choices)
    return;
  for (size_t i = 0; i < g_attire_page.size(); ++i)
    bd::mem::store<u32>(s_choices + u32(i) * 4, g_attire_page[i].second);
  // The question, with the current choice.
  LoadAttires();
  const auto it = g_attires.find(g_attire_id);
  const Attire current = it == g_attires.end() ? Attire{0, -1} : it->second;
  u32 current_text = GameText(in, base, kOriginalText);
  for (const auto &[a, text] : g_attire_options)
    if (a == current)
      current_text = text;
  const char *name = current_text ? bd::mem::try_at<const char>(current_text) : nullptr;
  g_attire_question.clear();  // Text(kAttireText) gives the language's format again
  char buffer[480];
  std::snprintf(buffer, sizeof(buffer), Text(kAttireText), name ? name : "");
  g_attire_question = buffer;
  g_box_lines = u32(g_attire_page.size());
  g_menu_frames = 0;
  g_saved_ok = false;
  Call(in, base, __imp__sub_8260DD80, 0, kAttireText, s_choices, g_box_lines, 0, 0);
}

// After the box closed with an answer.
void AttireChosen(const PPCContext &in, u8 *base, u32 answer) {
  if (answer >= g_attire_page.size()) {
    g_step = Step::kIdle;
    return;
  }
  const Attire choice = g_attire_page[answer].first;
  if (choice == kMore) {
    ShowAttirePage(in, base, g_attire_first + 2);  // g_step stays kAttire
    return;
  }
  LoadAttires();
  if (choice == Attire{0, -1})
    g_attires.erase(g_attire_id);
  else
    g_attires[g_attire_id] = choice;
  SaveAttires();
  BD_INFO("[univers] attire of Superstar {}: {} (Superstar Threads slot {})", g_attire_id,
          choice.attire, choice.slot);
  g_step = Step::kIdle;
}

// The attire for a Superstar in a match: his choice if he still has it, one at random, or none.
bool MatchAttire(const PPCContext &in, u8 *base, u32 id, Attire &out) {
  LoadAttires();
  const auto it = g_attires.find(id);
  if (it == g_attires.end())
    return false;
  const auto attires = SuperstarAttires(in, base, id);
  if (it->second == kRandom) {
    if (attires.size() < 2)
      return false;
    static std::mt19937 s_random{std::random_device{}()};
    out = attires[std::uniform_int_distribution<size_t>(0, attires.size() - 1)(s_random)].first;
    return true;
  }
  for (const auto &[a, text] : attires)
    if (a == it->second) {
      out = a;
      return true;
    }
  return false;  // no longer his (Superstar Threads slot emptied, attire locked)
}

}  // namespace

namespace {

// Dev (wwe12_universe_probe): every element and group of the Universe screens' layout
// (sub_8254E920(): groups of 144 bytes at +4, count +20, each {+76 count, +112 its elements};
// elements of 240 bytes at +8, count +24), written once to Documents\wwe12\logs\umsi_layout.txt
// the first time a Superstar's page shows.
void DumpPageLayout(const PPCContext &in, u8 *base) {
  static bool s_done = false;
  if (s_done)
    return;
  s_done = true;
  const u32 layout = Call(in, base, __imp__sub_8254E920, 0);
  auto *state = REX_KERNEL_STATE();
  if (!layout || !state || !state->emulator())
    return;
  std::ofstream f(state->emulator()->user_data_root() / "logs" / "umsi_layout.txt");
  const auto str = [](u32 at, u32 max) {
    std::string out;
    for (u32 i = 0; i < max; ++i) {
      const u8 c = bd::mem::try_load<u8>(at + i);
      if (!c)
        break;
      out += (c >= 32 && c < 127) ? char(c) : '?';
    }
    return out;
  };
  const u32 groups = bd::mem::load<u32>(layout + 4), group_count = bd::mem::load<u32>(layout + 20);
  const u32 elements = bd::mem::load<u32>(layout + 8), element_count = bd::mem::load<u32>(layout + 24);
  f << fmt::format("layout 0x{:08X}: {} groups at 0x{:08X}, {} elements at 0x{:08X}\n", layout,
                   group_count, groups, element_count, elements);
  for (u32 g = 0; g < group_count && g < 4096; ++g) {
    const u32 at = groups + g * 144;
    std::string words;
    for (u32 o = 0; o < 144; o += 4)
      words += fmt::format(" {:08X}", bd::mem::load<u32>(at + o));
    f << fmt::format("G{} @{:08X} n={} first={:08X}:{}\n", g, at, bd::mem::load<u32>(at + 76),
                     bd::mem::load<u32>(at + 112), words);
  }
  for (u32 e = 0; e < element_count && e < 8192; ++e) {
    const u32 at = elements + e * 240;
    const u32 text = bd::mem::load<u32>(at + 200);
    f << fmt::format("E{} @{:08X} kind={:X} x={} y={} sx={} sy={} font={} name={} parent={:08X} "
                     "hidden={} text={}\n",
                     e, at, bd::mem::load<u32>(at), bd::mem::load<float>(at + 4),
                     bd::mem::load<float>(at + 8), bd::mem::load<float>(at + 12),
                     bd::mem::load<float>(at + 16), str(at + 80, 16), str(at + 96, 16),
                     bd::mem::load<u32>(at + 176), bd::mem::load<u32>(at + 180),
                     text ? str(text, 40) : std::string("-"));
  }
  BD_INFO("[univers] probe: Universe layout dumped ({} groups, {} elements)", group_count,
          element_count);
}

}  // namespace

namespace {

// The button bar's sets: help data H = *0x8324DF18, records of 20 bytes at [H+48], count [H+52]:
// three places of two u16 text ids {button, label}, +12 u16 set id (sub_828BB8C0 looks a set up,
// sub_828BE488 shows it; three places at most). A Superstar's page shows set 505 (its vtable slot
// 26, sub_825AAD38, calls sub_828942C0(505)): {45000 SELECT}, {45001 BACK}, {45003 DEFAULT}, seen
// 2026-10-21. No place is free: in the Universe, the third label becomes our text, the game's
// DEFAULT followed by "Y ATTIRE" (button icons can sit inside a text); outside, it is put back.
constexpr u32 kHelpData = 0x8324DF18;
constexpr u32 kSuperstarPageHelpSet = 505;

void SetAttirePrompt(const PPCContext &in, u8 *base, bool on) {
  const u32 help = bd::mem::load<u32>(kHelpData);
  const u32 records = help ? bd::mem::load<u32>(help + 48) : 0;
  const u32 count = help ? bd::mem::load<u32>(help + 52) : 0;
  for (u32 r = 0; records && r < count && r < 4096; ++r) {
    const u32 at = records + r * 20;
    if (bd::mem::load<u16>(at + 12) != kSuperstarPageHelpSet)
      continue;
    const u16 label = bd::mem::load<u16>(at + 10);
    if (on && label == kDefaultPromptText) {
      const u32 text = GameText(in, base, kDefaultPromptText);
      const char *word = text ? bd::mem::try_at<const char>(text) : nullptr;
      g_attire_prompt.clear();  // Text(kAttirePromptText) gives the language's format again
      char buffer[256];
      std::snprintf(buffer, sizeof(buffer), Text(kAttirePromptText), word ? word : "DEFAULT");
      g_attire_prompt = buffer;
      bd::mem::store<u16>(at + 10, u16(kAttirePromptText));
    } else if (!on && label == kAttirePromptText) {
      bd::mem::store<u16>(at + 10, u16(kDefaultPromptText));
    }
    return;
  }
}

}  // namespace

// A Superstar's page shows its button bar (set 505): "Y ATTIRE" in it in the Universe.
REX_EXTERN(__imp__sub_825AAD38);
extern "C" REX_FUNC(sub_825AAD38) {
  SetAttirePrompt(ctx, base, Enabled() && bd::mem::load<u32>(kInUniverseFlag) == 1);
  __imp__sub_825AAD38(ctx, base);
}

// Y on a layout screen (button 0x40; X, 0x80, is DEFAULT on a Superstar's page): on a
// Superstar's page in the Universe, the attire box.
extern "C" REX_FUNC(sub_824D1AF0) {
  const u32 screen = ctx.r3.u32;
  if (Enabled() && g_step == Step::kIdle && bd::mem::try_load<u32>(screen) == kSuperstarPageVtbl &&
      bd::mem::load<u32>(kInUniverseFlag) == 1) {
    const u32 id = Call(ctx, base, __imp__sub_82DEA3A8, 0);
    g_attire_id = id;
    g_attire_options = SuperstarAttires(ctx, base, id);
    g_screen = screen;
    BD_INFO("[univers] attire box: Superstar {}, {} attire(s)", id, g_attire_options.size());
    if (g_attire_options.size() < 2) {
      g_step = Step::kHint;  // an information box, closed by the per-frame driver
      Call(ctx, base, __imp__sub_8254E680, kAttireOnlyText);
      return;
    }
    g_attire_options.push_back({kRandom, GameText(ctx, base, kRandomText)});
    g_step = Step::kAttire;
    ShowAttirePage(ctx, base, 0);
    return;
  }
  __imp__sub_824D1AF0(ctx, base);
}

// A Universe card loaded into the match select: the attires chosen for its Superstars.
extern "C" REX_FUNC(sub_826BE900) {
  const PPCContext in = ctx;
  __imp__sub_826BE900(ctx, base);
  if (!Enabled())
    return;
  const u32 m = in.r4.u32;
  if (!m)
    return;
  const u32 places = m + bd::mem::load<u32>(m + 15960) * 1596 + 124;
  for (u32 i = 0; i < 6; ++i) {
    const u32 id = bd::mem::load<u32>(places + i * 244) & 0x7FFFFFFF;
    Attire a;
    if (id >= 512 || !MatchAttire(in, base, id, a))
      continue;
    Call(in, base, __imp__sub_828AA050, m, i, u32(a.attire));
    Call(in, base, __imp__sub_828AA070, m, i, u32(a.slot));
    BD_INFO("[univers] match place {}: Superstar {} in attire {} (Superstar Threads slot {})", i,
            id, a.attire, a.slot);
  }
}

// Every layout menu screen's per-frame update: drives the dialogue while it is open (the screen
// itself keeps updating; its A button waits, see above).
REX_EXTERN(__imp__sub_824D3458);
REX_EXTERN(__imp__sub_8254E7D0);  // message box: idle (none open)?
extern "C" REX_FUNC(sub_824D3458) {
  if (REXCVAR_GET(wwe12_universe_probe) && bd::mem::try_load<u32>(ctx.r3.u32) == kSuperstarPageVtbl)
    DumpPageLayout(ctx, base);
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
  // TITLE MANAGEMENT open: the Universe menu takes no input meanwhile (its word +224 = 0, as
  // the layout screens' update reads it; 1 gives it back from the next frame). The screen is
  // closed from another screen's update, never from its own.
  if (g_step == Step::kTitles) {
    if (ctx.r3.u32 == g_screen)
      bd::mem::store<u32>(g_screen + 224, 0);
    if (g_titles && ctx.r3.u32 != g_titles && TitlesFinished()) {
      CloseTitles(ctx, base);
      bd::mem::store<u32>(g_screen + 224, 1);
      g_step = Step::kIdle;
    }
  } else if (g_step != Step::kIdle && ctx.r3.u32 == g_screen) {
    if (g_step == Step::kMenu || g_step == Step::kAttire) {
      const u32 frame = g_menu_frames++;
      if (!REXCVAR_GET(wwe12_universe_probe)) {
        if (g_box_lines == 3)
          PlaceThirdChoice();
      } else {
        // Dev: the box as the game draws it, logged at frame 20 and at each cursor move.
        static u32 s_cursor = 0xFFFFFFFFu;
        const u32 box = bd::mem::load<u32>(kMessageBox);
        const u32 cursor = box ? bd::mem::load<u32>(box + 56) : 0;
        if (frame == 0)
          s_cursor = 0xFFFFFFFFu;
        if (frame == 20 || (frame > 20 && cursor != s_cursor)) {
          s_cursor = cursor;
          DumpChoiceElements(fmt::format("frame {} cursor {}", frame, cursor).c_str());
        }
      }
    }
    u32 answer = 0;
    if (BoxClosed(ctx, base, answer)) {
      const bool yes = answer == 1;
      switch (g_step) {
      case Step::kAttire:
        RestoreChoiceElements();
        AttireChosen(ctx, base, answer);
        break;
      case Step::kMenu:
        RestoreChoiceElements();
        BD_INFO("[univers] EDIT UNIVERSE: choice {}", answer);
        g_step = Step::kIdle;
        if (answer == kEditSuperstars) {
          Call(ctx, base, __imp__sub_825453D8, g_screen);  // the Superstar editor, as before
        } else if (answer == kEditTitles) {
          if (OpenTitles(ctx, base))
            g_step = Step::kTitles;
        } else if (answer == kResetUniverse) {
          g_step = Step::kConfirm;
          Call(ctx, base, __imp__sub_8254E7E8, kConfirmText);
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

// The message box's cursor update (underline 0xD6 + i shown for the cursor's line, hidden for the
// others; text 0xD2 + i red or white). It runs after the box has hidden its unused elements for
// the frame, so the third line's underline is placed again here, last.
REX_EXTERN(__imp__sub_8288F0A0);
extern "C" REX_FUNC(sub_8288F0A0) {
  __imp__sub_8288F0A0(ctx, base);
  if (Enabled() && (g_step == Step::kMenu || g_step == Step::kAttire) && g_box_lines == 3 &&
      !REXCVAR_GET(wwe12_universe_probe))
    PlaceThirdChoice();
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
