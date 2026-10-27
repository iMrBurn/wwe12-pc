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
// id, choices, count, cancel, 0) as the game's own "Do Not Review / Remind Me Later" box does (cancel
// = request +12: 1 lets B close the box, as the game's sub_82623608 asks); once closed, the answer
// (sub_8254E8C8) is the chosen line, from 0 (anything else: cancelled).
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
#include <functional>
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
// After a reset saved to disk at once (the last of our texts: 27026-27040 are unused in WWE '12).
constexpr u32 kResetSavedText = 27040;
constexpr u32 kLastText = kResetSavedText;
// The contender settings' question (27048: unused in WWE '12, FEUD MANAGER in WWE '13).
constexpr u32 kContenderText = 27048;
// A Superstar's statistics (27049: unused in WWE '12, STATISTICS in WWE '13), written each time.
constexpr u32 kStatsText = 27049;
std::string g_stats_text;
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
// Our paged choice boxes' question (27051: empty in WWE '12) and information messages.
constexpr u32 kChooseText = 27051;
std::string g_choose_question;
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
      // the Superstar page's button bar, after the game's "DEFAULT" (U+E00E = RB, as in the game's
      // "RB EDIT")
      {"%s   \xEE\x80\x87 ATTIRE   \xEE\x80\x8E MORE", "%s   \xEE\x80\x87 TENUE   \xEE\x80\x8E PLUS",
       "%s   \xEE\x80\x87 OUTFIT   \xEE\x80\x8E MEHR", "%s   \xEE\x80\x87 ATUENDO   \xEE\x80\x8E M\xC3\x81S",
       "%s   \xEE\x80\x87 COSTUME   \xEE\x80\x8E ALTRO"},
      // done, and saved at once
      {"The WWE Universe has been reset and saved. A backup of your save was made first.\nLeave WWE "
       "Universe and come back to see it.",
       "L'Univers WWE a \xC3\xA9t\xC3\xA9 remis \xC3\xA0 z\xC3\xA9ro et enregistr\xC3\xA9. Une copie "
       "de votre sauvegarde a \xC3\xA9t\xC3\xA9 faite avant.\nQuittez l'Univers WWE et revenez-y pour "
       "le voir.",
       "Die WWE-Welt wurde zur\xC3\xBC" "ckgesetzt und gespeichert. Vorher wurde eine Kopie deines "
       "Spielstands erstellt.\nVerlasse die WWE-Welt und kehre zur\xC3\xBC" "ck, um sie zu sehen.",
       "El Universo WWE se ha reiniciado y guardado. Antes se hizo una copia de tu partida.\nSal del "
       "Universo WWE y vuelve a entrar para verlo.",
       "Il WWE Universe \xC3\xA8 stato azzerato e salvato. Prima \xC3\xA8 stata fatta una copia del tuo "
       "salvataggio.\nEsci dal WWE Universe e rientra per vederlo."},
  };
  if (id == kAttireText && !g_attire_question.empty())
    return g_attire_question.c_str();
  if (id == kAttirePromptText && !g_attire_prompt.empty())
    return g_attire_prompt.c_str();
  if (id == kStatsText)
    return g_stats_text.c_str();
  if (id == kChooseText)
    return g_choose_question.c_str();
  if (id == kContenderText) {
    static const char *const kContender[5] = {
        "CONTENDER SETTINGS\nThe titles this Superstar may be ranked for (A changes a line).",
        "R\xC3\x89GLAGES D'ASPIRANT\nLes titres pour lesquels ce Superstar peut \xC3\xAAtre "
        "class\xC3\xA9 (A change une ligne).",
        "HERAUSFORDERER\nDie Titel, f\xC3\xBCr die dieser Superstar gewertet werden kann (A "
        "\xC3\xA4ndert eine Zeile).",
        "AJUSTES DE ASPIRANTE\nLos t\xC3\xADtulos por los que este Superstar puede clasificarse "
        "(A cambia una l\xC3\xADnea).",
        "IMPOSTAZIONI SFIDANTE\nI titoli per cui questa Superstar pu\xC3\xB2 essere in classifica "
        "(A cambia una riga)."};
    return kContender[CurrentLang()];
  }
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
  constexpr u32 kSlots = 1 + kLastText - kAskText + 1 + 3;  // + contender, statistics, chooser
  const char *text = Text(id);
  if (!text)
    return 0;
  static u32 s_block = 0;
  static const char *s_for[kSlots] = {};
  if (!s_block)
    s_block = REX_KERNEL_MEMORY()->SystemHeapAlloc(kSlots * kSlot, 0x20);
  if (!s_block)
    return 0;
  const u32 slot = id == kEditSuperstarText ? 0
                   : id == kContenderText   ? kSlots - 3
                   : id == kStatsText       ? kSlots - 2
                   : id == kChooseText      ? kSlots - 1
                                            : id - kAskText + 1;
  const u32 at = s_block + slot * kSlot;
  if (s_for[slot] != text || id == kAttireText || id == kAttirePromptText || id == kStatsText ||
      id == kChooseText) {
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
bool BackupSave(const char *label = "Before RESET UNIVERSE") {
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
  std::ofstream(dest / "backup.txt") << "auto\n" << label << "\n";
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

// ---- stat tracking (as WWE 2K14's STATISTICS): the title history ------------------------------
//
// WWE '12 keeps no history. Every change of a Universe title's holder goes through the game's
// sub_82A3A060(title, holder, place) (match results, TITLE MANAGEMENT, ...): in the Universe
// (*0x8324E51C == 1), each real change is written to universe_stats.txt next to the save folder,
// one line "title <title> <place> <new holder> <old holder> <day>", the day being the Universe's
// own counter (U + 0xDDC4: month = day / 28 % 12 by sub_826D2890, week = day / 7). Reigns and weeks
// held are counted from it. A
// reset of the Universe moves the file aside (universe_stats_before_reset.txt).

constexpr u32 kUniverseDay = 0xDDC4;  // in the Universe block: days since its start

std::filesystem::path StatsFile() {
  auto *state = REX_KERNEL_STATE();
  if (!state || !state->emulator() || state->emulator()->user_data_root().empty())
    return {};
  return state->emulator()->user_data_root() / "universe_stats.txt";
}

struct Reign {
  u32 title, place, holder, from_day, to_day;  // to_day = ~0u while it goes on
};

// The title history, from the file: every reign, in order.
std::vector<Reign> LoadReigns() {
  std::vector<Reign> reigns;
  std::ifstream f(StatsFile());
  std::string line;
  while (std::getline(f, line)) {
    u32 title, place, holder, old_holder, day;
    if (std::sscanf(line.c_str(), "title %u %u %u %u %u", &title, &place, &holder, &old_holder,
                    &day) != 5)
      continue;
    for (Reign &r : reigns)
      if (r.title == title && r.place == place && r.to_day == ~0u)
        r.to_day = day;
    if (holder != kNobody)
      reigns.push_back({title, place, holder, day, ~0u});
  }
  return reigns;
}

// The match lines of universe_stats.txt (written after each Universe match, see the match results
// hook): "rumble <id> <won> <day>", "mania <id> <main event> <won> <day>", "mitb <id> <day>"
// (won the briefcase), "cashin <id> <won> <day>".
struct MatchStats {
  u32 rumble_wins = 0, rumble_entries = 0, mania_matches = 0, mania_main_events = 0,
      mania_wins = 0, mitb_wins = 0, cash_ins = 0, cash_in_wins = 0;
};

MatchStats LoadMatchStats(u32 id) {
  MatchStats m;
  std::ifstream f(StatsFile());
  std::string line;
  while (std::getline(f, line)) {
    u32 who, a, b, day;
    if (std::sscanf(line.c_str(), "rumble %u %u %u", &who, &a, &day) == 3 && who == id) {
      ++m.rumble_entries;
      m.rumble_wins += a ? 1 : 0;
    } else if (std::sscanf(line.c_str(), "mania %u %u %u %u", &who, &a, &b, &day) == 4 &&
               who == id) {
      ++m.mania_matches;
      m.mania_main_events += a ? 1 : 0;
      m.mania_wins += b ? 1 : 0;
    } else if (std::sscanf(line.c_str(), "mitb %u %u", &who, &day) == 2 && who == id) {
      ++m.mitb_wins;
    } else if (std::sscanf(line.c_str(), "cashin %u %u %u", &who, &a, &day) == 3 && who == id) {
      ++m.cash_ins;
      m.cash_in_wins += a ? 1 : 0;
    }
  }
  return m;
}

void StatsReset() {
  const std::filesystem::path file = StatsFile();
  std::error_code ec;
  if (!file.empty() && std::filesystem::exists(file, ec))
    std::filesystem::rename(file, file.parent_path() / "universe_stats_before_reset.txt", ec);
}

// ---- the Superstars' original brands ------------------------------------------------------------
//
// A Superstar's brand (RAW 0, SmackDown 1, ...) is in his own record (286 bytes, sub_82684910(id),
// +24..+27, the Universe reads +27 by sub_826D2B50), not in the Universe block: a reset left the
// brands as they were. The original records come from misc.pac's group /CHAR/DAT (class at 0x820ECC78),
// read by sub_82DCAA58(this, data, size): for each roster place k < 323 (sub_82683878), its Superstar
// sub_82A33440(k), entry of 4 + 286 bytes (u16 version 1, u16 size + 4), into the record k
// (sub_82683940) or, when this+48 is set, into the buffer there. The data is kept when the game reads
// it; a reset reads it again into a buffer of ours and puts back bytes +24..+27 of every record.
// Created Superstars (ids < 50, sub_82A330D8) have no original record: their brand is kept.
REX_EXTERN(__imp__sub_82DCAA58);  // /CHAR/DAT reader (this, data, size)
REX_EXTERN(__imp__sub_82A33418);  // Superstar -> roster place (512: none)
REX_EXTERN(__imp__sub_82A330D8);  // a created Superstar?
REX_EXTERN(__imp__sub_82684910);  // a Superstar's record
REX_EXTERN(__imp__sub_82A33458);  // a Superstar in the game's data (the reader skips the others)

constexpr u32 kRosterPlaces = 323, kRecordSize = 286;
u32 g_char_data = 0, g_char_size = 0;

extern "C" REX_FUNC(sub_82DCAA58) {
  const u32 data = ctx.r4.u32, size = ctx.r5.u32;
  if (!bd::mem::load<u32>(ctx.r3.u32 + 48) && data && size && size < 0x400000) {
    auto *memory = REX_KERNEL_MEMORY();
    if (g_char_data && g_char_size < size) {
      memory->SystemHeapFree(g_char_data);
      g_char_data = 0;
    }
    if (!g_char_data)
      g_char_data = memory->SystemHeapAlloc(size, 0x20);
    if (g_char_data) {
      std::memcpy(bd::mem::at<u8>(g_char_data), bd::mem::at<u8>(data), size);
      g_char_size = size;
    }
    BD_INFO("[univers] /CHAR/DAT read: {} bytes kept", g_char_size);
  }
  __imp__sub_82DCAA58(ctx, base);
}

void RestoreBrands(const PPCContext &ctx, u8 *base) {
  PPCContext in = ctx;  // the reader needs ~3 KB of stack
  in.r1.u32 = (in.r1.u32 - 0x1000) & ~0xFu;
  if (!g_char_data) {
    BD_INFO("[univers] reset: /CHAR/DAT not read this session, brands kept");
    return;
  }
  auto *memory = REX_KERNEL_MEMORY();
  const u32 reader = memory->SystemHeapAlloc(64, 0x10);
  const u32 records = memory->SystemHeapAlloc(kRosterPlaces * kRecordSize, 0x10);
  if (reader && records) {
    std::memset(bd::mem::at<u8>(reader), 0, 64);
    std::memset(bd::mem::at<u8>(records), 0, kRosterPlaces * kRecordSize);
    bd::mem::store<u32>(reader + 48, records);
    Call(in, base, __imp__sub_82DCAA58, reader, g_char_data, g_char_size);
    u32 changed = 0, read = 0;
    for (u32 k = 0; k < kRosterPlaces; ++k)
      for (u32 o = 0; o < kRecordSize; o += 4)
        if (bd::mem::load<u32>(records + k * kRecordSize + o)) {
          ++read;
          break;
        }
    BD_INFO("[univers] reset: {} original records read, reader now at +{}", read,
            bd::mem::load<u32>(reader + 48) - records);
    for (u32 id = 50; id < 512 && REXCVAR_GET(wwe12_universe_probe); ++id) {
      const u32 place = Call(in, base, __imp__sub_82A33418, id);
      const u32 to = Call(in, base, __imp__sub_82684910, id);
      if (place >= kRosterPlaces || !to)
        continue;
      const u32 from = records + place * kRecordSize;
      BD_INFO("[univers] brands: {} place {} in data {} now {}/{}/{}/{} original {}/{}/{}/{}", id,
              place, Call(in, base, __imp__sub_82A33458, id), bd::mem::load<u8>(to + 24),
              bd::mem::load<u8>(to + 25), bd::mem::load<u8>(to + 26), bd::mem::load<u8>(to + 27),
              bd::mem::load<u8>(from + 24), bd::mem::load<u8>(from + 25),
              bd::mem::load<u8>(from + 26), bd::mem::load<u8>(from + 27));
    }
    for (u32 id = 50; id < 512; ++id) {
      if (Call(in, base, __imp__sub_82A330D8, id))
        continue;
      const u32 place = Call(in, base, __imp__sub_82A33418, id);
      if (place >= kRosterPlaces)
        continue;
      const u32 from = records + place * kRecordSize;
      const u32 to = Call(in, base, __imp__sub_82684910, id);
      if (!to || !Call(in, base, __imp__sub_82A33458, id))  // no original record for him
        continue;
      for (u32 k = 24; k < 28; ++k)
        if (bd::mem::load<u8>(to + k) != bd::mem::load<u8>(from + k)) {
          bd::mem::store<u8>(to + k, bd::mem::load<u8>(from + k));
          ++changed;
        }
    }
    BD_INFO("[univers] reset: original brands put back ({} bytes changed)", changed);
  }
  if (reader)
    memory->SystemHeapFree(reader);
  if (records)
    memory->SystemHeapFree(records);
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
  RestoreBrands(in, base);
  StatsReset();
  BD_INFO("[univers] reset: done (block 0x{:08X} cleared, start data applied, default champions)",
          universe);
  return true;
}

// ---- the EDIT UNIVERSE dialogue -------------------------------------------------------------

enum class Step {
  kIdle, kMenu, kConfirm, kDone, kHint, kVacateAsk, kVacateDone, kTitles, kAttire, kSaving, kChoose
};
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
  // A choice box closed with B gives the line under the cursor too: the box's word +20 tells them
  // apart (1 after A, sub_824D1A20; 0 after B, sub_824D1A50). B is only allowed in our choice boxes.
  const u32 box = bd::mem::load<u32>(0x8324DEF0);
  if (box && bd::mem::load<u32>(box + 20) == 0 &&
      (g_step == Step::kMenu || g_step == Step::kAttire || g_step == Step::kChoose))
    answer = 0xFFFFFFFFu;
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
  Call(in, base, __imp__sub_8260DD80, 0, kAskText, s_choices, kChoiceCount, 1, 0);  // 1: B cancels
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

namespace {

// While a Universe match's edit menu is built: the label of the row that becomes SAVE (see
// sub_82574BD8 below), given the game's "SAVE" instead.
u32 g_save_label = 0;
constexpr u32 kGameSaveText = 238;

}  // namespace

// The string manager's "text of id": EDIT SUPERSTAR's new label and our questions.
REX_EXTERN(__imp__sub_821A4400);
extern "C" REX_FUNC(sub_821A4400) {
  if (g_save_label && ctx.r4.u32 == g_save_label)
    ctx.r4.u64 = kGameSaveText;
  const u32 id = ctx.r4.u32;
  __imp__sub_821A4400(ctx, base);
  if (Enabled() && (id == kEditSuperstarText || id == kContenderText || id == kStatsText ||
                    id == kChooseText ||
                    (id >= kAskText && id <= kLastText))) {
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

namespace {
void OpenUniverseMenu(const PPCContext &in, u8 *base);
void Chosen(const PPCContext &in, u8 *base, u32 answer);
}  // namespace

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
    OpenUniverseMenu(ctx, base);
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
  Call(in, base, __imp__sub_8260DD80, 0, kAttireText, s_choices, g_box_lines, 1, 0);  // 1: B cancels
}

// ---- paged choice boxes (the game's box, 2 lines + MORE... a page) -------------------------------
//
// The game's choice box shows 3 lines at most: longer lists go by pages of two lines and "MORE...",
// as the attire box. The answer goes to a function of ours (B closes the box).

struct Chooser {
  std::string question;
  std::vector<std::string> options;
  std::function<void(const PPCContext &, u8 *, size_t)> done;
  size_t first = 0;
  std::vector<size_t> page;  // the options on the box (npos: MORE...)
};
Chooser g_chooser;

void ShowChoosePage(const PPCContext &in, u8 *base, size_t first) {
  Chooser &c = g_chooser;
  c.first = first < c.options.size() ? first : 0;
  c.page.clear();
  const size_t left = c.options.size() - c.first;
  if (left <= 3) {
    for (size_t i = c.first; i < c.options.size(); ++i)
      c.page.push_back(i);
  } else {
    c.page = {c.first, c.first + 1, std::string::npos};
  }
  static u32 s_lines = 0, s_choices = 0;
  if (!s_lines)
    s_lines = REX_KERNEL_MEMORY()->SystemHeapAlloc(3 * 96, 0x10);
  if (!s_choices)
    s_choices = REX_KERNEL_MEMORY()->SystemHeapAlloc(4 * 4, 0x10);
  if (!s_lines || !s_choices)
    return;
  for (size_t i = 0; i < c.page.size(); ++i) {
    const u32 at = s_lines + u32(i) * 96;
    const std::string &t =
        c.page[i] == std::string::npos ? std::string(Text(kAttireMoreText)) : c.options[c.page[i]];
    std::memset(bd::mem::at<char>(at), 0, 96);
    std::memcpy(bd::mem::at<char>(at), t.data(), std::min<size_t>(t.size(), 95));
    bd::mem::store<u32>(s_choices + u32(i) * 4, at);
  }
  // the box lays a one-line question over its first line: a title line above it (EDIT UNIVERSE,
  // the first line of our menu's question)
  g_choose_question = c.question;
  if (g_choose_question.find('\n') == std::string::npos) {
    const std::string ask = Text(kAskText);
    g_choose_question = ask.substr(0, ask.find('\n')) + "\n" + g_choose_question;
  }
  g_step = Step::kChoose;
  g_box_lines = u32(c.page.size());
  g_menu_frames = 0;
  g_saved_ok = false;
  Call(in, base, __imp__sub_8260DD80, 0, kChooseText, s_choices, g_box_lines, 1, 0);  // 1: B cancels
}

void Choose(const PPCContext &in, u8 *base, std::string question, std::vector<std::string> options,
            std::function<void(const PPCContext &, u8 *, size_t)> done, size_t first = 0) {
  g_chooser.question = std::move(question);
  g_chooser.options = std::move(options);
  g_chooser.done = std::move(done);
  ShowChoosePage(in, base, first);
}

// An information box (closed by the per-frame driver).
void Inform(const PPCContext &in, u8 *base, const std::string &text) {
  g_stats_text = text;
  g_step = Step::kHint;
  Call(in, base, __imp__sub_8254E680, kStatsText);
}

void Chosen(const PPCContext &in, u8 *base, u32 answer) {
  g_step = Step::kIdle;
  Chooser &c = g_chooser;
  if (answer >= c.page.size())
    return;  // B
  if (c.page[answer] == std::string::npos) {
    ShowChoosePage(in, base, c.first + 2);
    return;
  }
  const auto done = c.done;  // it may open another box
  done(in, base, c.page[answer]);
}

// ---- the Universe's schedule (as WWE 2K14's EDIT CALENDAR) -----------------------------------------
//
// U + 0xDE00: 7 bytes, a weekday each (Monday first) = the weekly show airing then (0 RAW, 1
// SmackDown, 2 Superstars, 0xFF none); Sunday is never read there (PPVs). Only shows 0-2 have a
// booker (sub_826EB218), so only they can be scheduled. U + 0xDDD0: 48 bytes, month * 4 + week = the
// PPV (10-22) or 0xFF. Show records: U + 27360 + k * 984 (+832 name; +929 + b: uses brand b,
// 0 RAW, 1 SmackDown). WWE '12's own SHOW EDIT only flips a show between Monday and Friday (and its
// label follows the show, not the table).

constexpr u32 kWeekTable = 0xDE00, kPpvTable = 0xDDD0, kShowRecords = 27360, kShowRecordSize = 984;
constexpr u32 kNoShow = 0xFF;

struct CalendarWords {
  const char *calendar, *weekly, *ppvs, *days[6], *no_show, *move, *remove, *host, *raw, *smackdown,
      *both, *week, *airs_on, *pick_show, *pick_day, *pick_ppv, *pick_month, *pick_week, *what,
      *done_day, *done_none, *done_ppv, *done_ppv_none, *done_host, *matches, *pick_count,
      *default_count, *all_ppvs, *done_matches, *bumped, *menu;
};
const CalendarWords &CWords() {
  static const CalendarWords kWords[5] = {
      {"EDIT CALENDAR", "WEEKLY SHOWS", "PPVs", {"MONDAY", "TUESDAY", "WEDNESDAY", "THURSDAY",
       "FRIDAY", "SATURDAY"}, "NOT ON THE CALENDAR", "MOVE", "REMOVE FROM THE CALENDAR",
       "HOST SHOW", "RAW", "SMACKDOWN", "RAW AND SMACKDOWN", "WEEK", "on %s", "Which show?",
       "%s airs %s.\nWhich day?", "Which PPV?", "%s: which month?", "%s: which week of %s?",
       "%s (%s).\nWhat do you want to change?", "%s now airs on %s.", "%s is no longer on the calendar.",
       "%s now takes place in week %u of %s.", "%s is no longer on the calendar.",
       "%s is now hosted by %s.", "MATCHES PER SHOW", "%s: how many matches? (now %s)",
       "GAME DEFAULT", "ALL PPVs", "%s: %s.\nThe next cards will follow it.",
       "\n%s no longer airs (it was on that day).", "EDIT UNIVERSE"},
      {"MODIFIER LE CALENDRIER", "SHOWS HEBDOMADAIRES", "PPV", {"LUNDI", "MARDI", "MERCREDI",
       "JEUDI", "VENDREDI", "SAMEDI"}, "PAS AU CALENDRIER", "D\xC3\x89PLACER",
       "RETIRER DU CALENDRIER", "SHOW ORGANISATEUR", "RAW", "SMACKDOWN", "RAW ET SMACKDOWN",
       "SEMAINE", "le %s", "Quel show ?", "%s passe %s.\nQuel jour ?", "Quel PPV ?",
       "%s : quel mois ?", "%s : quelle semaine de %s ?", "%s (%s).\nQue voulez-vous changer ?",
       "%s passe maintenant le %s.", "%s n'est plus au calendrier.",
       "%s a maintenant lieu la semaine %u de %s.", "%s n'est plus au calendrier.",
       "%s est maintenant organis\xC3\xA9 par %s.", "MATCHS PAR SHOW",
       "%s : combien de matchs ? (actuellement %s)", "R\xC3\x89GLAGE DU JEU", "TOUS LES PPV",
       "%s : %s.\nLes prochaines cartes le suivront.",
       "\n%s ne passe plus (il \xC3\xA9tait ce jour-l\xC3\xA0).", "MODIFIER L'UNIVERS"},
      {"KALENDER BEARBEITEN", "WOCHENSHOWS", "PPVs", {"MONTAG", "DIENSTAG", "MITTWOCH",
       "DONNERSTAG", "FREITAG", "SAMSTAG"}, "NICHT IM KALENDER", "VERSCHIEBEN",
       "AUS DEM KALENDER NEHMEN", "AUSRICHTENDE SHOW", "RAW", "SMACKDOWN", "RAW UND SMACKDOWN",
       "WOCHE", "am %s", "Welche Show?", "%s l\xC3\xA4uft %s.\nWelcher Tag?", "Welcher PPV?",
       "%s: welcher Monat?", "%s: welche Woche im %s?", "%s (%s).\nWas m\xC3\xB6" "chtest du \xC3\xA4ndern?",
       "%s l\xC3\xA4uft jetzt am %s.", "%s ist nicht mehr im Kalender.",
       "%s findet jetzt in Woche %u im %s statt.", "%s ist nicht mehr im Kalender.",
       "%s wird jetzt von %s ausgerichtet.", "MATCHES PRO SHOW",
       "%s: wie viele Matches? (jetzt %s)", "SPIELSTANDARD", "ALLE PPVs",
       "%s: %s.\nDie n\xC3\xA4" "chsten Matchkarten folgen dem.",
       "\n%s l\xC3\xA4uft nicht mehr (sie war an dem Tag).", "WWE-WELT BEARBEITEN"},
      {"EDITAR CALENDARIO", "SHOWS SEMANALES", "PPV", {"LUNES", "MARTES", "MI\xC3\x89RCOLES", "JUEVES",
       "VIERNES", "S\xC3\x81" "BADO"}, "FUERA DEL CALENDARIO", "MOVER", "QUITAR DEL CALENDARIO",
       "SHOW ORGANIZADOR", "RAW", "SMACKDOWN", "RAW Y SMACKDOWN", "SEMANA", "el %s",
       "\xC2\xBF" "Qu\xC3\xA9 show?", "%s se emite %s.\n\xC2\xBF" "Qu\xC3\xA9 d\xC3\xAD" "a?",
       "\xC2\xBF" "Qu\xC3\xA9 PPV?", "%s: \xC2\xBF" "qu\xC3\xA9 mes?", "%s: \xC2\xBF" "qu\xC3\xA9 semana de %s?",
       "%s (%s).\n\xC2\xBF" "Qu\xC3\xA9 quieres cambiar?", "%s se emite ahora el %s.",
       "%s ya no est\xC3\xA1 en el calendario.", "%s es ahora la semana %u de %s.",
       "%s ya no est\xC3\xA1 en el calendario.", "%s lo organiza ahora %s.",
       "COMBATES POR SHOW", "%s: \xC2\xBF" "cu\xC3\xA1ntos combates? (ahora %s)", "VALOR DEL JUEGO",
       "TODOS LOS PPV", "%s: %s.\nLas pr\xC3\xB3ximas carteleras lo seguir\xC3\xA1n.",
       "\n%s ya no se emite (era ese d\xC3\xAD" "a).", "EDITAR UNIVERSO"},
      {"MODIFICA CALENDARIO", "SHOW SETTIMANALI", "PPV", {"LUNED\xC3\x8C", "MARTED\xC3\x8C",
       "MERCOLED\xC3\x8C", "GIOVED\xC3\x8C", "VENERD\xC3\x8C", "SABATO"}, "FUORI CALENDARIO", "SPOSTA",
       "TOGLI DAL CALENDARIO", "SHOW ORGANIZZATORE", "RAW", "SMACKDOWN", "RAW E SMACKDOWN",
       "SETTIMANA", "il %s", "Quale show?", "%s va in onda %s.\nQuale giorno?", "Quale PPV?",
       "%s: quale mese?", "%s: quale settimana di %s?", "%s (%s).\nCosa vuoi cambiare?",
       "%s ora va in onda il %s.", "%s non \xC3\xA8 pi\xC3\xB9 in calendario.",
       "%s ora si tiene nella settimana %u di %s.", "%s non \xC3\xA8 pi\xC3\xB9 in calendario.",
       "%s ora \xC3\xA8 organizzato da %s.", "INCONTRI PER SHOW",
       "%s: quanti incontri? (ora %s)", "VALORE DEL GIOCO", "TUTTI I PPV",
       "%s: %s.\nLe prossime card lo seguiranno.",
       "\n%s non va pi\xC3\xB9 in onda (era quel giorno).", "MODIFICA UNIVERSO"}};
  return kWords[CurrentLang()];
}

std::string Format(const char *format, const std::string &a, const std::string &b = {}) {
  char buffer[512];
  std::snprintf(buffer, sizeof(buffer), format, a.c_str(), b.c_str());
  return buffer;
}

u32 UniverseBlock(const PPCContext &in, u8 *base) { return Call(in, base, __imp__sub_82683A30, 0); }

std::string ShowName(u32 u, u32 show) {
  const char *n = bd::mem::try_at<const char>(u + kShowRecords + show * kShowRecordSize + 832);
  std::string name = n ? std::string(n, strnlen(n, 63)) : std::string();
  return name.empty() ? "?" : name;
}

std::string MonthName(const PPCContext &in, u8 *base, u32 month) {
  const u32 t = GameText(in, base, 27250 + month);  // JANUARY ... DECEMBER
  const char *n = t ? bd::mem::try_at<const char>(t) : nullptr;
  return n ? n : std::to_string(month + 1);
}

std::string ShowDay(u32 u, u32 show) {
  for (u32 d = 0; d < 6; ++d)
    if (bd::mem::load<u8>(u + kWeekTable + d) == show)
      return Format(CWords().airs_on, CWords().days[d]);
  return CWords().no_show;
}

void PickWeeklyShow(const PPCContext &in, u8 *base) {
  const CalendarWords &w = CWords();
  const u32 u = UniverseBlock(in, base);
  if (!u)
    return;
  std::vector<std::string> shows;
  for (u32 k = 0; k < 3; ++k)
    shows.push_back(ShowName(u, k));
  Choose(in, base, w.pick_show, shows, [](const PPCContext &in, u8 *base, size_t show) {
    const CalendarWords &w = CWords();
    const u32 u = UniverseBlock(in, base);
    std::vector<std::string> days(w.days, w.days + 6);
    days.push_back(w.no_show);
    Choose(in, base, Format(w.pick_day, ShowName(u, u32(show)), ShowDay(u, u32(show))), days,
           [show](const PPCContext &in, u8 *base, size_t day) {
             const CalendarWords &w = CWords();
             const u32 u = UniverseBlock(in, base);
             if (!u)
               return;
             std::string bumped;
             for (u32 d = 0; d < 7; ++d)
               if (bd::mem::load<u8>(u + kWeekTable + d) == show)
                 bd::mem::store<u8>(u + kWeekTable + d, kNoShow);
             if (day < 6) {
               const u8 there = bd::mem::load<u8>(u + kWeekTable + u32(day));
               if (there != kNoShow && there != show)
                 bumped = Format(w.bumped, ShowName(u, there));
               bd::mem::store<u8>(u + kWeekTable + u32(day), u8(show));
             }
             BD_INFO("[univers] calendar: show {} on day {} (week table {:02X} {:02X} {:02X} {:02X} "
                     "{:02X} {:02X} {:02X})", show, day, bd::mem::load<u8>(u + kWeekTable),
                     bd::mem::load<u8>(u + kWeekTable + 1), bd::mem::load<u8>(u + kWeekTable + 2),
                     bd::mem::load<u8>(u + kWeekTable + 3), bd::mem::load<u8>(u + kWeekTable + 4),
                     bd::mem::load<u8>(u + kWeekTable + 5), bd::mem::load<u8>(u + kWeekTable + 6));
             Inform(in, base,
                    (day < 6 ? Format(w.done_day, ShowName(u, u32(show)), w.days[day])
                             : Format(w.done_none, ShowName(u, u32(show)))) + bumped);
           });
  });
}

// Where a PPV is (month * 4 + week), or 48.
u32 PpvSlot(u32 u, u32 ppv) {
  for (u32 i = 0; i < 48; ++i)
    if (bd::mem::load<u8>(u + kPpvTable + i) == ppv)
      return i;
  return 48;
}

std::string PpvWhen(const PPCContext &in, u8 *base, u32 u, u32 ppv) {
  const u32 slot = PpvSlot(u, ppv);
  if (slot >= 48)
    return CWords().no_show;
  return MonthName(in, base, slot / 4) + " - " + CWords().week + " " + std::to_string(slot % 4 + 1);
}

void MovePpv(const PPCContext &in, u8 *base, u32 ppv, u32 month) {
  const CalendarWords &w = CWords();
  const u32 u = UniverseBlock(in, base);
  const std::string name = ShowName(u, ppv), month_name = MonthName(in, base, month);
  std::vector<std::string> weeks;
  for (u32 k = 1; k <= 4; ++k) {
    const u8 there = bd::mem::load<u8>(u + kPpvTable + month * 4 + k - 1);
    weeks.push_back(std::string(w.week) + " " + std::to_string(k) +
                    (there != kNoShow && there != ppv ? "  (" + ShowName(u, there) + ")" : ""));
  }
  char question[256];
  std::snprintf(question, sizeof(question), w.pick_week, name.c_str(), month_name.c_str());
  Choose(in, base, question, weeks, [ppv, month](const PPCContext &in, u8 *base, size_t week) {
    const CalendarWords &w = CWords();
    const u32 u = UniverseBlock(in, base);
    if (!u)
      return;
    const u32 slot = month * 4 + u32(week), old = PpvSlot(u, ppv);
    const u8 there = bd::mem::load<u8>(u + kPpvTable + slot);
    for (u32 i = 0; i < 48; ++i)  // the PPV leaves its date(s)
      if (bd::mem::load<u8>(u + kPpvTable + i) == ppv)
        bd::mem::store<u8>(u + kPpvTable + i, kNoShow);
    if (there != kNoShow && there != ppv && old < 48)
      bd::mem::store<u8>(u + kPpvTable + old, there);  // the one there takes its old date
    bd::mem::store<u8>(u + kPpvTable + slot, u8(ppv));
    BD_INFO("[univers] calendar: PPV {} in month {} week {} (was slot {}, {} moved)", ppv, month,
            week + 1, old, there);
    char text[256];
    std::snprintf(text, sizeof(text), w.done_ppv, ShowName(u, ppv).c_str(), u32(week + 1),
                  MonthName(in, base, month).c_str());
    std::string message = text;
    if (there != kNoShow && there != ppv)
      message += "\n" + ShowName(u, there) + ": " + PpvWhen(in, base, u, there);
    Inform(in, base, message);
  });
}

void EditPpv(const PPCContext &in, u8 *base, u32 ppv) {
  const CalendarWords &w = CWords();
  const u32 u = UniverseBlock(in, base);
  Choose(in, base, Format(w.what, ShowName(u, ppv), PpvWhen(in, base, u, ppv)),
         {w.move, w.host, w.remove}, [ppv](const PPCContext &in, u8 *base, size_t what) {
           const CalendarWords &w = CWords();
           const u32 u = UniverseBlock(in, base);
           if (!u)
             return;
           const u32 record = u + kShowRecords + ppv * kShowRecordSize;
           if (what == 0) {
             std::vector<std::string> months;
             for (u32 m = 0; m < 12; ++m)
               months.push_back(MonthName(in, base, m));
             Choose(in, base, Format(w.pick_month, ShowName(u, ppv)), months,
                    [ppv](const PPCContext &in, u8 *base, size_t month) {
                      MovePpv(in, base, ppv, u32(month));
                    });
           } else if (what == 1) {
             Choose(in, base, ShowName(u, ppv), {w.raw, w.smackdown, w.both},
                    [ppv](const PPCContext &in, u8 *base, size_t host) {
                      const CalendarWords &w = CWords();
                      const u32 u = UniverseBlock(in, base);
                      if (!u)
                        return;
                      const u32 record = u + kShowRecords + ppv * kShowRecordSize;
                      bd::mem::store<u8>(record + 929, host != 1);  // RAW
                      bd::mem::store<u8>(record + 930, host != 0);  // SmackDown
                      BD_INFO("[univers] calendar: PPV {} hosted by {}", ppv, host);
                      Inform(in, base,
                             Format(w.done_host, ShowName(u, ppv),
                                    host == 0 ? w.raw : host == 1 ? w.smackdown : w.both));
                    });
           } else {
             for (u32 i = 0; i < 48; ++i)
               if (bd::mem::load<u8>(u + kPpvTable + i) == ppv)
                 bd::mem::store<u8>(u + kPpvTable + i, kNoShow);
             BD_INFO("[univers] calendar: PPV {} removed", ppv);
             Inform(in, base, Format(w.done_ppv_none, ShowName(u, ppv)));
           }
           (void)record;
         });
}

void PickPpv(const PPCContext &in, u8 *base) {
  const u32 u = UniverseBlock(in, base);
  if (!u)
    return;
  std::vector<std::string> ppvs;
  for (u32 k = 10; k < 23; ++k)
    ppvs.push_back(ShowName(u, k));
  Choose(in, base, CWords().pick_ppv, ppvs, [](const PPCContext &in, u8 *base, size_t k) {
    EditPpv(in, base, 10 + u32(k));
  });
}

void EditCalendar(const PPCContext &in, u8 *base) {
  const CalendarWords &w = CWords();
  Choose(in, base, w.calendar, {w.weekly, w.ppvs}, [](const PPCContext &in, u8 *base, size_t i) {
    if (i == 0)
      PickWeeklyShow(in, base);
    else
      PickPpv(in, base);
  });
}

// ---- matches per show (as WWE 2K14's show setting MATCHES) ---------------------------------------
//
// Each kind of show has a booker class (sub_826EB218 picks it from the current show): its virtual
// slot 3 is the number of matches of the card, read when the card is booked (sub_826EB510, a job of
// its own thread) and when it is written (sub_826C8120): RAW / SmackDown sub_826F19E0 (4, vtables
// 0x8203EB08 / 0x8203EB1C), Superstars sub_82E05C90 (3, a "return 3" shared with other classes,
// vtable 0x8203EB30), PPVs sub_826F1EF8 (6). A card has 8 records at most; the match templates of a
// show give 6 at most (the last match is the title / main event one). Our setting replaces the
// number (universe_shows.txt: "matches <0 RAW / 1 SmackDown / 2 Superstars / 10 PPVs> <n>").

constexpr u32 kBookerRaw = 0x8203EB08, kBookerSmackDown = 0x8203EB1C, kBookerSuperstars = 0x8203EB30;
std::atomic<u32> g_matches[4] = {0, 0, 0, 0};  // RAW, SmackDown, Superstars, PPVs (0: the game's)
std::atomic<bool> g_matches_loaded{false};

std::filesystem::path ShowsFile() {
  auto *state = REX_KERNEL_STATE();
  if (!state || !state->emulator() || state->emulator()->user_data_root().empty())
    return {};
  return state->emulator()->user_data_root() / "universe_shows.txt";
}

void LoadMatchCounts() {
  if (g_matches_loaded.exchange(true))
    return;
  std::ifstream f(ShowsFile());
  std::string line;
  while (std::getline(f, line)) {
    u32 key = 0, n = 0;
    if (std::sscanf(line.c_str(), "matches %u %u", &key, &n) == 2 && n <= 8)
      g_matches[key >= 10 ? 3 : std::min<u32>(key, 2)] = n;
  }
}

void SaveMatchCounts() {
  const std::filesystem::path file = ShowsFile();
  if (file.empty())
    return;
  std::ofstream f(file);
  const u32 keys[4] = {0, 1, 2, 10};
  for (u32 i = 0; i < 4; ++i)
    if (g_matches[i])
      f << "matches " << keys[i] << ' ' << g_matches[i] << '\n';
}

void EditMatches(const PPCContext &in, u8 *base) {
  const CalendarWords &w = CWords();
  const u32 u = UniverseBlock(in, base);
  if (!u)
    return;
  LoadMatchCounts();
  std::vector<std::string> shows = {ShowName(u, 0), ShowName(u, 1), ShowName(u, 2), w.all_ppvs};
  Choose(in, base, w.matches, shows, [shows](const PPCContext &in, u8 *base, size_t which) {
    const CalendarWords &w = CWords();
    static const u32 kDefault[4] = {4, 4, 3, 6};
    const auto now = [&](u32 n) {
      return n ? std::to_string(n) : std::string(w.default_count) + " " + std::to_string(kDefault[which]);
    };
    std::vector<std::string> counts = {std::string(w.default_count) + " (" +
                                       std::to_string(kDefault[which]) + ")"};
    for (u32 n = 1; n <= 6; ++n)
      counts.push_back(std::to_string(n));
    Choose(in, base, Format(w.pick_count, shows[which], now(g_matches[which])), counts,
           [shows, which, now](const PPCContext &in, u8 *base, size_t pick) {
             const CalendarWords &w = CWords();
             g_matches[which] = u32(pick);  // 0: the game's
             SaveMatchCounts();
             BD_INFO("[univers] matches per show: {} -> {}", which, pick);
             Inform(in, base, Format(w.done_matches, shows[which], now(u32(pick))));
           });
  });
}

// ---- EDIT UNIVERSE (as WWE 2K14's list) -------------------------------------------------------

void OpenUniverseMenu(const PPCContext &in, u8 *base) {
  const CalendarWords &w = CWords();
  Choose(in, base, Text(kAskText),
         {Text(kChoiceText + kEditSuperstars), Text(kChoiceText + kEditTitles), w.calendar,
          w.matches, Text(kChoiceText + kResetUniverse)},
         [](const PPCContext &in, u8 *base, size_t i) {
           BD_INFO("[univers] EDIT UNIVERSE: choice {}", i);
           if (i == 0) {
             Call(in, base, __imp__sub_825453D8, g_screen);  // the Superstar editor, as before
           } else if (i == 1) {
             if (OpenTitles(in, base))
               g_step = Step::kTitles;
           } else if (i == 2) {
             EditCalendar(in, base);
           } else if (i == 3) {
             EditMatches(in, base);
           } else {
             g_step = Step::kConfirm;
             Call(in, base, __imp__sub_8254E7E8, kConfirmText);
           }
         });
}


// ---- contender settings (as WWE 2K14's "contender settings") ----------------------------------
//
// Per Superstar, the kinds of title he may be ranked for: a show's titles are, in order, its MAJOR,
// MINOR, TAG and DIVA (sub_826CC180(show) gives the four, as the RANKINGS screen reads them). A
// Superstar kept out of a kind is never ranked for those titles, so never #1 contender, so never in
// their matches: the game's "may he contend for this title" (sub_82A39EF8(id, title, 1), which only
// looks at men / women) answers no, but only when asked by the Universe's rankings (sub_826CFDA8
// singles, sub_826D18E8 tag teams) and booking filter (sub_826E1380), known by their return
// addresses; TITLE MANAGEMENT, MATCH CREATOR and matches still ask the game. Chosen from the
// attire box (Y on a Superstar's page, its last line), kept in universe_contenders.txt next to
// universe_attires.txt ("id mask", bit k = kind k kept out), not in the save.

REX_EXTERN(__imp__sub_82A39EF8);  // may this Superstar contend for this title (id, title, 1)
REX_EXTERN(__imp__sub_826CDC48);  // Universe view: open (428 bytes)
REX_EXTERN(__imp__sub_826CE098);  // Universe view: close
REX_EXTERN(__imp__sub_826CCB88);  // Universe view: show k
REX_EXTERN(__imp__sub_826CC180);  // show: its four titles (show, out[4], 23 = none)

constexpr Attire kContender{-3, -1};  // the attire box's "CONTENDERS..." line
constexpr Attire kStats{-4, -1};      // the attire box's "STATISTICS..." line
constexpr u32 kKinds = 4;             // MAJOR, MINOR, TAG, DIVA
constexpr u32 kKindText = 27606;      // the game's MAJOR, MINOR, TAG, DIVA
constexpr u32 kOnText = 27613, kOffText = 27614;
constexpr u32 kContenderReturns[] = {0x826CFF0C, 0x826D1AE0, 0x826E13A8};

std::map<u32, u32> g_contenders;  // Superstar id -> kinds kept out (bits)
bool g_contenders_loaded = false;
bool g_contender_box = false;     // the box shows the contender settings
u32 g_contender_page = 0;         // 0: MAJOR, MINOR, MORE...; 1: TAG, DIVA

std::filesystem::path ContendersFile() {
  const std::filesystem::path attires = AttiresFile();
  return attires.empty() ? attires : attires.parent_path() / "universe_contenders.txt";
}

void LoadContenders() {
  if (g_contenders_loaded)
    return;
  g_contenders_loaded = true;
  std::ifstream f(ContendersFile());
  std::string line;
  while (std::getline(f, line)) {
    u32 id, mask;
    if (std::sscanf(line.c_str(), "%u %u", &id, &mask) == 2 && mask)
      g_contenders[id] = mask & 0xF;
  }
}

void SaveContenders() {
  const std::filesystem::path file = ContendersFile();
  if (file.empty())
    return;
  std::ofstream f(file);
  f << "# Universe contender settings (Y on a Superstar's page): Superstar id, then the kinds of "
       "title he is kept out of (1 major, 2 minor, 4 tag, 8 diva, added up).\n";
  for (const auto &[id, mask] : g_contenders)
    f << id << ' ' << mask << '\n';
}

// The kind of a title (0-3), from the show that has it; -1 if no show has it. Kept a second.
int TitleKind(const PPCContext &in, u8 *base, u32 title) {
  static int s_kind[24];
  static long long s_until = 0;
  const long long now = GetTickCount64();
  if (now >= s_until) {
    std::fill(std::begin(s_kind), std::end(s_kind), -1);
    PPCContext x = in;
    x.r1.u32 = (x.r1.u32 - 0x1000) & ~0xFu;  // our view and table below the caller's stack
    const u32 view = x.r1.u32 + 0x800, titles = view + 0x200;
    Call(x, base, __imp__sub_826CDC48, view);
    for (u32 k = 0; k < 13; ++k) {
      const u32 show = Call(x, base, __imp__sub_826CCB88, view, k);
      if (!show || !bd::mem::load<u32>(show))
        continue;
      for (u32 i = 0; i < kKinds; ++i)
        bd::mem::store<u32>(titles + i * 4, 23);
      Call(x, base, __imp__sub_826CC180, show, titles);
      for (u32 i = 0; i < kKinds; ++i) {
        const u32 t = bd::mem::load<u32>(titles + i * 4);
        if (t < 23 && s_kind[t] < 0)
          s_kind[t] = int(i);
      }
    }
    Call(x, base, __imp__sub_826CE098, view);
    s_until = now + 1000;
  }
  return title < 23 ? s_kind[title] : -1;
}

REX_EXTERN(__imp__sub_826D2C30);  // a title's name (title)

// The attire box's "STATISTICS..." line, as a guest string.
u32 StatsLine() {
  static const char *const kLine[5] = {"STATISTICS...", "STATISTIQUES...", "STATISTIKEN...",
                                       "ESTAD\xC3\x8DSTICAS...", "STATISTICHE..."};
  static u32 s_at = 0;
  if (!s_at)
    s_at = REX_KERNEL_MEMORY()->SystemHeapAlloc(64, 0x10);
  if (s_at) {
    std::memset(bd::mem::at<char>(s_at), 0, 64);
    std::strncpy(bd::mem::at<char>(s_at), kLine[CurrentLang()], 63);
  }
  return s_at;
}

// A Superstar's title history, as WWE 2K14's STATISTICS page shows it: per title, the reigns and
// the weeks held (a reign still going counts up to this week). From universe_stats.txt.
void ShowStats(const PPCContext &in, u8 *base, u32 id) {
  static const char *const kHead[5] = {"TITLE HISTORY", "HISTORIQUE DES TITRES", "TITELHISTORIE",
                                       "HISTORIAL DE T\xC3\x8DTULOS", "STORIA DEI TITOLI"};
  static const char *const kLine[5] = {"%s: %u reign(s), %u week(s)",
                                       "%s : %u r\xC3\xA8gne(s), %u semaine(s)",
                                       "%s: %u Regentschaft(en), %u Woche(n)",
                                       "%s: %u reinado(s), %u semana(s)",
                                       "%s: %u regno/i, %u settimana/e"};
  static const char *const kNone[5] = {
      "No title won since statistics began (version 1.0).",
      "Aucun titre gagn\xC3\xA9 depuis le d\xC3\xA9" "but des statistiques (version 1.0).",
      "Kein Titel gewonnen, seit die Statistik l\xC3\xA4uft (Version 1.0).",
      "Ning\xC3\xBAn t\xC3\xADtulo ganado desde el inicio de las estad\xC3\xADsticas (versi\xC3\xB3n 1.0).",
      "Nessun titolo vinto dall'inizio delle statistiche (versione 1.0)."};
  const u32 lang = CurrentLang();
  const u32 u = Call(in, base, __imp__sub_82683A30, 0);
  const u32 now = u ? bd::mem::load<u32>(u + kUniverseDay) : 0;
  u32 reigns[23] = {}, weeks[23] = {};
  for (const Reign &r : LoadReigns()) {
    if (r.holder != id || r.title >= 23)
      continue;
    ++reigns[r.title];
    const u32 to = r.to_day == ~0u ? now : r.to_day;
    weeks[r.title] += (to > r.from_day ? to - r.from_day : 0) / 7;
  }
  std::string text = kHead[lang];
  bool any = false;
  for (u32 t = 0; t < 23; ++t) {
    if (!reigns[t])
      continue;
    const u32 name = Call(in, base, __imp__sub_826D2C30, t);
    const char *n = name ? bd::mem::try_at<const char>(name) : nullptr;
    char buffer[160];
    std::snprintf(buffer, sizeof(buffer), kLine[lang], n ? n : "?", reigns[t], weeks[t]);
    text += '\n';
    text += buffer;
    any = true;
  }
  if (!any) {
    text += '\n';
    text += kNone[lang];
  }
  g_stats_text = text;
  g_step = Step::kHint;  // an information box, closed by the per-frame driver
  Call(in, base, __imp__sub_8254E680, kStatsText);
}

// The attire box's last line, as a guest string.
u32 ContenderLine() {
  static const char *const kLine[5] = {"CONTENDER SETTINGS...", "R\xC3\x89GLAGES D'ASPIRANT...",
                                       "HERAUSFORDERER...", "AJUSTES DE ASPIRANTE...",
                                       "IMPOSTAZIONI SFIDANTE..."};
  static u32 s_at = 0;
  if (!s_at)
    s_at = REX_KERNEL_MEMORY()->SystemHeapAlloc(64, 0x10);
  if (s_at) {
    std::memset(bd::mem::at<char>(s_at), 0, 64);
    std::strncpy(bd::mem::at<char>(s_at), kLine[CurrentLang()], 63);
  }
  return s_at;
}

void ShowContenderPage(const PPCContext &in, u8 *base, u32 page) {
  g_contender_box = true;
  g_contender_page = page;
  LoadContenders();
  const u32 mask = g_contenders.count(g_attire_id) ? g_contenders[g_attire_id] : 0;
  static u32 s_lines = 0, s_choices = 0;
  if (!s_lines)
    s_lines = REX_KERNEL_MEMORY()->SystemHeapAlloc(kKinds * 64, 0x10);
  if (!s_choices)
    s_choices = REX_KERNEL_MEMORY()->SystemHeapAlloc(4 * 4, 0x10);
  if (!s_lines || !s_choices)
    return;
  const auto text = [&](u32 id) {
    const u32 t = GameText(in, base, id);
    const char *s = t ? bd::mem::try_at<const char>(t) : nullptr;
    return std::string(s ? s : "");
  };
  u32 n = 0;
  for (u32 k = page * 2; k < page * 2 + 2; ++k) {
    const std::string line =
        text(kKindText + k) + "   " + text((mask >> k) & 1 ? kOffText : kOnText);
    const u32 at = s_lines + k * 64;
    std::memset(bd::mem::at<char>(at), 0, 64);
    std::memcpy(bd::mem::at<char>(at), line.data(), std::min<size_t>(line.size(), 63));
    bd::mem::store<u32>(s_choices + n++ * 4, at);
  }
  if (page == 0)
    bd::mem::store<u32>(s_choices + n++ * 4, GuestText(kAttireMoreText));
  g_box_lines = n;
  g_menu_frames = 0;
  g_saved_ok = false;
  Call(in, base, __imp__sub_8260DD80, 0, kContenderText, s_choices, n, 1, 0);  // 1: B cancels
}

void ContenderChosen(const PPCContext &in, u8 *base, u32 answer) {
  if (answer >= g_box_lines) {
    g_contender_box = false;
    g_step = Step::kIdle;
    return;
  }
  if (g_contender_page == 0 && answer == 2) {
    ShowContenderPage(in, base, 1);
    return;
  }
  const u32 kind = g_contender_page * 2 + answer;
  LoadContenders();
  u32 &mask = g_contenders[g_attire_id];
  mask ^= 1u << kind;
  if (!mask)
    g_contenders.erase(g_attire_id);
  SaveContenders();
  BD_INFO("[univers] contenders: Superstar {} kept out of kinds 0x{:X}", g_attire_id,
          g_contenders.count(g_attire_id) ? g_contenders[g_attire_id] : 0);
  ShowContenderPage(in, base, g_contender_page);  // the line shows its new value
}

}  // namespace

extern "C" REX_FUNC(sub_82A39EF8) {
  const u32 from = u32(ctx.lr), id = ctx.r3.u32, title = ctx.r4.u32;
  const PPCContext in = ctx;
  __imp__sub_82A39EF8(ctx, base);
  if (!ctx.r3.u32 || !Enabled() ||
      std::find(std::begin(kContenderReturns), std::end(kContenderReturns), from) ==
          std::end(kContenderReturns))
    return;
  LoadContenders();
  const auto it = g_contenders.find(id);
  if (it == g_contenders.end())
    return;
  const int kind = TitleKind(in, base, title);
  if (kind >= 0 && (it->second >> kind) & 1)
    ctx.r3.u64 = 0;  // kept out of this kind of title
}

namespace {

// After the box closed with an answer.
void AttireChosen(const PPCContext &in, u8 *base, u32 answer) {
  if (answer >= g_attire_page.size()) {
    g_step = Step::kIdle;
    return;
  }
  const Attire choice = g_attire_page[answer].first;
  if (choice == kContender) {
    ShowContenderPage(in, base, 0);  // g_step stays kAttire
    return;
  }
  if (choice == kStats) {
    ShowStats(in, base, g_attire_id);
    return;
  }
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
void DumpPageLayout(const PPCContext &in, u8 *base, const char *name = "umsi_layout.txt",
                    bool once = true, u32 layout = 0) {
  static bool s_done = false;
  if (once && s_done)
    return;
  s_done = s_done || once;
  if (!layout)
    layout = Call(in, base, __imp__sub_8254E920, 0);
  auto *state = REX_KERNEL_STATE();
  if (!layout || !state || !state->emulator())
    return;
  std::error_code ec;
  std::filesystem::create_directories(state->emulator()->user_data_root() / "logs", ec);
  std::ofstream f(state->emulator()->user_data_root() / "logs" / name);
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
    if (g_attire_options.size() < 2) {  // statistics and titles: the RB / LB pages
      g_step = Step::kHint;  // an information box, closed by the per-frame driver
      Call(ctx, base, __imp__sub_8254E680, kAttireOnlyText);
      return;
    }
    g_attire_options.push_back({kRandom, GameText(ctx, base, kRandomText)});
    g_contender_box = false;
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

// ---- every title's full ranking (RANKINGS) -----------------------------------------------------
//
// A title's ranking screen (opened by sub_82553770, the screen's base at this - 28) is set up by
// sub_825535E0: on entry its +56 is the title's place in the show (0-3: major, minor, tag team,
// Divas), then +56..+68 become the page state {page, pages, 0, scrolling}. The ranking itself
// (sub_825530A0, up to 180 Superstars or teams of the show) exists for every title, and pages =
// 2 + (count - 6) / 12 when count > 5, but the game turns scrolling off (+68 = 0, arrows 900/901
// hidden) unless the title is the show's first. In the Universe, every title scrolls like the
// major one: +68 = 1 and the down arrow (901) shown when there is more than one page.

REX_EXTERN(__imp__sub_825535E0);  // a title's ranking screen: set up for its title
REX_EXTERN(__imp__sub_824C5458);  // layout: element by id (layout, id)

namespace {
constexpr u32 kRankingDownArrow = 901;
}  // namespace

extern "C" REX_FUNC(sub_825535E0) {
  const PPCContext in = ctx;
  const u32 self = ctx.r3.u32;
  const u32 place = bd::mem::load<u32>(self + 56);
  __imp__sub_825535E0(ctx, base);
  if (!Enabled() || bd::mem::load<u32>(kInUniverseFlag) != 1 || place == 0 ||
      bd::mem::load<u32>(self + 60) < 2)
    return;
  bd::mem::store<u32>(self + 68, 1);
  if (const u32 arrow = Call(in, base, __imp__sub_824C5458, bd::mem::load<u32>(self + 296),
                             kRankingDownArrow))
    bd::mem::store<u32>(arrow + 180, 0);
  BD_INFO("[univers] ranking: title place {}, {} ranked, {} pages, scrolling on", place,
          bd::mem::load<u32>(self + 8852), bd::mem::load<u32>(self + 60));
}

// ---- a ranking's order changed by hand (as WWE '13's X EDIT) -----------------------------------
//
// On a title's ranking (layout screen S, vtable kRankingVtbl; the ranking object R = S - 28), X
// starts editing: a red cursor on a Superstar of the list, moved with up/down (the pages follow);
// A takes him (gold) and up/down then move him one place at a time; A again puts him down; B or X
// ends. The champion stays on top.
//
// The list (R+2368, 32-byte entries, +0 Superstar, +24 this week's points; [R+8852] entries,
// entry 0 = the champion, 1 = the #1 contender) is sorted by those points, high first
// (sub_82552A00); sub_826CE180 gives 1000 to the champion and 999 to the #1 contender (Universe
// table U+0xC750, written with sub_826D5118(table, title, Superstar, 0)), the others their Universe
// record's points for the title (U + sub_826D3320(id) * 112 + title * 2, u16). A move rewrites the
// #1 contender and these points, strictly decreasing in the new order (the same values, raised
// where needed), then sub_825530A0(R) rebuilds the list and page 0. A copy of the save is made
// before the first change. Singles titles only (tag team rankings list teams).
//
// Pages: page 0 = the champion and #1..#4, page p = list entries 5 + (p - 1) * 12 ... + 11 (rows
// drawn every frame from S+28 = the page by sub_82552A78). The layout screen's pressed buttons
// for the frame are S+64 (16 A, 32 B, 128 X; up / down by the list's direction S+20, masks at
// 0x8201AD80), handled by sub_824D20A0: while editing, they are ours.

REX_EXTERN(__imp__sub_824D20A0);  // layout screens: the frame's buttons
REX_EXTERN(__imp__sub_823D35B8);  // layout screens: the list moved (screen, 1 up / 2 down)
REX_EXTERN(__imp__sub_82552CE8);  // ranking screen: show the page's rows
REX_EXTERN(__imp__sub_825530A0);  // ranking: build the list
REX_EXTERN(__imp__sub_82A39CE8);  // title: tag team?
REX_EXTERN(__imp__sub_826D3320);  // Superstar: Universe record index, -1 if none
REX_EXTERN(__imp__sub_826D5118);  // #1 contenders: set (table, title, Superstar, place)

namespace {

constexpr u32 kRankingVtbl = 0x82023240;
constexpr u32 kContenders = 0xC750;  // in the Universe block
constexpr u32 kRankingHelpSet = 515;
constexpr u32 kButtonX = 45003, kEditLabel = 45409;  // button bar: X icon, the game's "EDIT"
constexpr u32 kUp = 1, kDown = 2, kA = 16, kB = 32, kY = 64, kX = 128;
constexpr u32 kContenderName = 948, kRowName = 1020;  // layout elements
constexpr u32 kListMasks = 0x8201AD80;  // per list direction: {previous, -, next, -}

struct RankEdit {
  u32 screen = 0;     // the layout screen being edited, 0 = not editing
  u32 cursor = 1;     // list entry
  bool held = false;  // the Superstar under the cursor moves with it
  u32 lit = 0;        // the element coloured, and its colour before
  u32 lit_rgba[4] = {};
  bool backed_up = false;
};
RankEdit g_rank;
u32 g_universe_menu = 0;  // the Universe menu object (sub_82545A70)

u32 RankObject(u32 screen) { return screen - 28; }
u32 RankCount(u32 r) { return std::min<u32>(bd::mem::load<u32>(r + 8852), 180); }
u32 RankId(u32 r, u32 i) { return bd::mem::load<u32>(r + 2368 + i * 32); }
u32 RankPage(u32 i) { return i <= 4 ? 0 : 1 + (i - 5) / 12; }

void Unlight() {
  if (g_rank.lit)
    for (u32 k = 0; k < 4; ++k)
      bd::mem::store<u32>(g_rank.lit + 184 + k * 4, g_rank.lit_rgba[k]);
  g_rank.lit = 0;
}

// The cursor's name in red (gold while held).
void Light(u32 r) {
  // The name's element: page 0's #1..#4 are elements 948..951, a list page's rows 1020..1031
  // (sub_82552A78; other screens of the layout show the same names, so not found by text).
  const u32 i = g_rank.cursor;
  const u32 id = i <= 4 ? kContenderName + i - 1 : kRowName + (i - 5) % 12;
  const u32 layout = bd::mem::load<u32>(r + 296);
  const u32 array = layout ? bd::mem::load<u32>(layout + 8) : 0;
  const u32 count = layout ? bd::mem::load<u32>(layout + 24) : 0;
  const u32 found = array && id < count ? array + id * 240 : 0;
  const u32 text = found ? bd::mem::load<u32>(found + 200) : 0;
  const char *want = text ? bd::mem::try_at<const char>(text) : nullptr;
  static u32 s_logged = ~0u;
  if (g_rank.cursor != s_logged) {
    s_logged = g_rank.cursor;
    BD_INFO("[univers] ranking edit: cursor {} ({}) -> element 0x{:08X}", g_rank.cursor,
            want ? want : "?", found);
  }
  if (found != g_rank.lit) {
    Unlight();
    if (found) {
      for (u32 k = 0; k < 4; ++k)
        g_rank.lit_rgba[k] = bd::mem::load<u32>(found + 184 + k * 4);
      g_rank.lit = found;
    }
  }
  if (g_rank.lit) {
    // the game's red for a menu's chosen line (0.80, 0.08, 0.08), gold while held
    const float rgba[4] = {g_rank.held ? 1.0f : 0.8039f, g_rank.held ? 0.75f : 0.0784f,
                           g_rank.held ? 0.1f : 0.0784f, 1.0f};
    for (u32 k = 0; k < 4; ++k)
      bd::mem::store<float>(g_rank.lit + 184 + k * 4, rgba[k]);
  }
}

// Swaps list entries a and b (>= 1) in the saved ranking, then rebuilds the list.
void RankSwap(const PPCContext &in, u8 *base, u32 r, u32 a, u32 b) {
  const u32 n = RankCount(r), title = bd::mem::load<u32>(r + 8848);
  const u32 u = Call(in, base, __imp__sub_82683A30, 0);
  if (!u || a == 0 || b == 0 || a >= n || b >= n)
    return;
  if (!g_rank.backed_up) {
    g_rank.backed_up = BackupSave("Before RANKINGS EDIT");
    if (!g_rank.backed_up) {
      BD_WARN("[univers] ranking edit: backup failed, nothing changed");
      return;
    }
  }
  std::vector<u32> order;
  for (u32 i = 1; i < n; ++i)
    order.push_back(RankId(r, i));
  std::swap(order[a - 1], order[b - 1]);
  if (order[0] != RankId(r, 1))
    Call(in, base, __imp__sub_826D5118, u + kContenders, title, order[0], 0);
  // The others' points: their values, high first, kept strictly decreasing from the bottom up.
  std::vector<u32> records, points;
  for (u32 i = 1; i < order.size(); ++i) {
    const int32_t index = int32_t(Call(in, base, __imp__sub_826D3320, order[i]));
    const u32 rec = index < 0 ? 0 : u + u32(index) * 112;
    records.push_back(rec);
    points.push_back(rec ? bd::mem::load<u16>(rec + title * 2) : 0);
  }
  std::sort(points.begin(), points.end(), std::greater<u32>());
  for (size_t i = points.size(); i-- > 0;)
    if (i + 1 < points.size() && points[i] <= points[i + 1])
      points[i] = points[i + 1] + 1;
  for (size_t i = 0; i < records.size(); ++i)
    if (records[i])
      bd::mem::store<u16>(records[i] + title * 2, u16(std::min<u32>(points[i], 998)));
  Call(in, base, __imp__sub_825530A0, r);
  BD_INFO("[univers] ranking edit: title {}, places {} and {} swapped", title, a, b);
}

void RankEditEnd() {
  Unlight();
  g_rank.screen = 0;
  g_rank.held = false;
}

// The ranking's button bar: X EDIT in a free place (logged once).
void RankEditPrompt() {
  static bool s_done = false;
  const u32 help = bd::mem::load<u32>(kHelpData);
  const u32 records = help ? bd::mem::load<u32>(help + 48) : 0;
  const u32 count = help ? bd::mem::load<u32>(help + 52) : 0;
  for (u32 i = 0; records && i < count && i < 4096; ++i) {
    const u32 at = records + i * 20;
    if (bd::mem::load<u16>(at + 12) != kRankingHelpSet)
      continue;
    if (!s_done)
      BD_INFO("[univers] ranking: button bar set {}: {:04X} {:04X} {:04X} {:04X} {:04X} {:04X}",
              kRankingHelpSet, bd::mem::load<u16>(at), bd::mem::load<u16>(at + 2),
              bd::mem::load<u16>(at + 4), bd::mem::load<u16>(at + 6), bd::mem::load<u16>(at + 8),
              bd::mem::load<u16>(at + 10));
    s_done = true;
    for (u32 k = 0; k < 3; ++k)
      if (bd::mem::load<u16>(at + k * 4) == kButtonX)
        return;
    for (u32 k = 0; k < 3; ++k) {
      if (bd::mem::load<u16>(at + k * 4) != 0xFFFF)
        continue;
      bd::mem::store<u16>(at + k * 4, u16(kButtonX));
      bd::mem::store<u16>(at + k * 4 + 2, u16(kEditLabel));
      BD_INFO("[univers] ranking: X EDIT in the button bar (place {})", k);
      return;
    }
    return;
  }
  if (!s_done)
    BD_INFO("[univers] ranking: button bar set {} not found", kRankingHelpSet);
  s_done = true;
}

// Dev: the ranking layout's shown texts, when editing starts (logs/rank_layout.txt).
void DumpRankLayout(u32 r) {
  auto *state = REX_KERNEL_STATE();
  const u32 layout = bd::mem::load<u32>(r + 296);
  if (!layout || !state || !state->emulator())
    return;
  std::ofstream f(state->emulator()->user_data_root() / "logs" / "rank_layout.txt");
  const u32 array = bd::mem::load<u32>(layout + 8), count = bd::mem::load<u32>(layout + 24);
  for (u32 e = 0; e < count && e < 8192; ++e) {
    const u32 at = array + e * 240, text = bd::mem::load<u32>(at + 200);
    const char *t = text ? bd::mem::try_at<const char>(text) : nullptr;
    f << fmt::format("E{} hidden={} rgba={} {} {} {} num={} flags={:X} text={}\n", e,
                     bd::mem::load<u32>(at + 180), bd::mem::load<float>(at + 184),
                     bd::mem::load<float>(at + 188), bd::mem::load<float>(at + 192),
                     bd::mem::load<float>(at + 196), bd::mem::load<u32>(at + 220),
                     bd::mem::load<u32>(at + 228), t ? std::string(t, strnlen(t, 40)) : "-");
  }
}

}  // namespace

// The Universe menu's screens are made by sub_82545A70(menu): +260 MATCH CARDS, +264 RANKINGS (the
// ranking object R, vtable 0x820232C8, its layout screen at R+28), +268 ... (one object each).
REX_EXTERN(__imp__sub_82545A70);
REX_EXTERN(__imp__sub_82A341A0);  // a Superstar's brand byte k (record +24 + k; 3 = the Universe's)
REX_EXTERN(__imp__sub_826D3218);  // a Superstar in use (id, 1)
REX_EXTERN(__imp__sub_82A32618);  // Superstar: name

// Dev (wwe12_universe_probe): every Superstar's four brand bytes, to find the original brands.
void DumpBrands(const PPCContext &in, u8 *base) {
  auto *state = REX_KERNEL_STATE();
  if (!state || !state->emulator())
    return;
  std::error_code ec;
  std::filesystem::create_directories(state->emulator()->user_data_root() / "logs", ec);
  std::ofstream f(state->emulator()->user_data_root() / "logs" / "brands.txt");
  for (u32 id = 0; id < 512; ++id) {
    if (!Call(in, base, __imp__sub_826D3218, id, 1))
      continue;
    const u32 name = Call(in, base, __imp__sub_82A32618, id);
    const char *n = name ? bd::mem::try_at<const char>(name) : nullptr;
    f << fmt::format("{} {} {} {} {} {}\n", id, Call(in, base, __imp__sub_82A341A0, id, 0),
                     Call(in, base, __imp__sub_82A341A0, id, 1),
                     Call(in, base, __imp__sub_82A341A0, id, 2),
                     Call(in, base, __imp__sub_82A341A0, id, 3), n ? n : "?");
  }
}

extern "C" REX_FUNC(sub_82545A70) {
  g_universe_menu = ctx.r3.u32;
  if (const u32 u = Enabled() ? Call(ctx, base, __imp__sub_82683A30, 0) : 0) {
    std::string week, ppv;
    for (u32 d = 0; d < 7; ++d)
      week += fmt::format(" {:02X}", bd::mem::load<u8>(u + 0xDE00 + d));
    for (u32 i = 0; i < 48; ++i)
      ppv += fmt::format("{}{:02X}", i % 4 ? "" : " ", bd::mem::load<u8>(u + 0xDDD0 + i));
    BD_INFO("[univers] calendar: weekdays{} | PPVs by month{}", week, ppv);
  }
  if (REXCVAR_GET(wwe12_universe_probe))
    DumpBrands(ctx, base);
  __imp__sub_82545A70(ctx, base);
}

// ---- a Superstar's page: CONTENDERS and STATISTICS pages (as WWE 2K14's, RB / LB) -------------
//
// The Universe menu M shows one panel at a time: M+288 = the panel shown, chosen from its list
// (sub_82544EE8: 0 MATCH CARDS +260, 1 SCHEDULE +276, 2 RANKINGS +264, 3 NEWS +280, 4 EDIT UNIVERSE
// +268, 5 EDIT TEAM +272, 6 EDIT BRAND +284); changing it = the old one's vtable +92 (hide,
// sub_82545070 with its chain at +256), M+288 = the new one, its vtable +88 (show, sub_82545008). On a
// Superstar's page (EDIT UNIVERSE's panel), RB / LB go to our pages and back: they show the RANKINGS
// panel (R = M+264), the four titles of the Superstar's show (R+2352 = show 0 / 1, refreshed as its own
// LB / RB do: sub_82552768, sub_82551CB8, sub_82551F48, sub_82552048), while the Superstar's page keeps
// the buttons (ours here). Each title's champion line (elements 842/854/866/881, their red copies
// 843/855/867/882 hidden) shows instead:
// - CONTENDERS: his place in that title's ranking (sub_826D2508), CHAMPION, or OFF when kept out
//   (§21); up / down choose a title, X turns it on / off;
// - STATISTICS: his reigns and weeks held (universe_stats.txt).
// The header (830, the show's name) shows the Superstar and the page. B goes back to his page.

REX_EXTERN(__imp__sub_825ACEC0);  // a Superstar's page: show it (page object; its vtable +88)
REX_EXTERN(__imp__sub_8258D240);  // the Superstar list: its row changed
REX_EXTERN(__imp__sub_82545070);  // Universe menu: hide a panel
REX_EXTERN(__imp__sub_82545008);  // Universe menu: show a panel
REX_EXTERN(__imp__sub_82552768);  // rankings: refresh (as its LB / RB)
REX_EXTERN(__imp__sub_82551CB8);
REX_EXTERN(__imp__sub_82551F48);  // rankings: the show's four title names
REX_EXTERN(__imp__sub_82552048);  // rankings: the champions
REX_EXTERN(__imp__sub_826D2508);  // a title's ranking (view, title, out, max): count
REX_EXTERN(__imp__sub_826CC0B8);  // show: is this Superstar on it (show, id)

namespace {

constexpr u32 kPanelShown = 288, kRankingsPanel = 264;
constexpr u32 kTitleRowName[4] = {842, 854, 866, 881}, kTitleRowRed[4] = {843, 855, 867, 882};
constexpr u32 kHeaderName = 830;
// The pages' top, as WWE 2K14's: LB / RB (U+E00B / U+E00E; U+E00D is the left stick) and the
// page's name.
constexpr const char *kPageKeys = "\xEE\x80\x8B \xEE\x80\x8E   ";
constexpr u32 kLB = 0x100, kRB = 0x200;

struct SuperstarPages {
  u32 page = 0;    // 0 the game's page, 1 CONTENDERS, 2 STATISTICS
  u32 screen = 0;  // the Superstar's page
  u32 old_panel = 0;
  u32 id = 0, show = 0, row = 0;
  u32 titles[4] = {23, 23, 23, 23};
  std::string lines[4], header, name;
  u32 page_input = 2, menu_input = 0;  // the +224 of his page and of the Universe menu, before
  u32 list_row = 0, list_top = 0;  // the Superstar list's row and first row shown
  u32 menu_state = 0;              // the Universe menu's +296 (2: a panel's screen is open)
  struct Row {
    std::string label, value;
    bool section = false;  // a section's title (red, as in 2K14)
  };
  std::vector<Row> table;  // the STATISTICS page's rows
  u32 table_page = 1;      // its page of 12 rows (the rankings list's pages 1, 2)
  std::string top, subtitle;  // the table's top line (897) and, after LB / RB, the page's (898)
  bool global = false;        // the Universe's statistics (RANKINGS, Y), not a Superstar's
  u32 section = 0;            // their page: titles, champions, Money in the Bank, Royal Rumble
  u32 global_place = 0;       // RANKINGS' title place (R+56) before they opened
};
SuperstarPages g_pages;

u32 PageText(u32 slot, const std::string &text) {  // guest strings of ours, 96 bytes each
  static u32 s_block = 0;
  if (!s_block)
    s_block = REX_KERNEL_MEMORY()->SystemHeapAlloc(8 * 96, 0x10);
  if (!s_block)
    return 0;
  const u32 at = s_block + slot * 96;
  std::memset(bd::mem::at<char>(at), 0, 96);
  std::memcpy(bd::mem::at<char>(at), text.data(), std::min<size_t>(text.size(), 95));
  return at;
}

// Changing the panel shown, as the Universe menu does.
void SetPanel(const PPCContext &in, u8 *base, u32 panel) {
  const u32 m = g_universe_menu, old = bd::mem::load<u32>(m + kPanelShown);
  if (old == panel || !panel)
    return;
  if (old)
    Call(in, base, __imp__sub_82545070, old);
  bd::mem::store<u32>(m + kPanelShown, panel);
  Call(in, base, __imp__sub_82545008, panel);
}

// EDIT UNIVERSE's Superstar list (screen, vtable 0x82027970) is its panel's +412 (its screen +28,
// then +384). Its rows scroll with an object at +1760 ({+0 row in the window, +4 first row shown,
// +8 rows shown, +12 rows, +16 wrap}, sub_8254DE48 moves it); showing the panel again starts it at
// the top: its row and first row are kept and put back, then its "row changed" (vtable +80,
// sub_8258D240, nothing moved: +88 = 0) makes that row's Superstar the current one again
// (sub_82DEA390) and draws the header.
constexpr u32 kSuperstarList = 412, kListScroll = 1760;
// The Universe menu's +296: 2 while a panel's own screen is open (here the Superstar list): the
// menu takes the buttons back when it closes. Showing a panel sets it to 0: it is put back.
constexpr u32 kMenuState = 296;

// The Superstar's show (0 / 1, the RANKINGS panel's two) and its four titles; his place in each.
void ReadSuperstar(const PPCContext &in, u8 *base) {
  PPCContext x = in;
  x.r1.u32 = (x.r1.u32 - 0x1000) & ~0xFu;
  const u32 view = x.r1.u32 + 0x800, titles = view + 0x200;
  Call(x, base, __imp__sub_826CDC48, view);
  g_pages.show = 0;
  for (u32 k = 0; k < 2; ++k) {
    const u32 show = Call(x, base, __imp__sub_826CCB88, view, k);
    if (show && bd::mem::load<u32>(show) && Call(x, base, __imp__sub_826CC0B8, show, g_pages.id)) {
      g_pages.show = k;
      break;
    }
  }
  const u32 show = Call(x, base, __imp__sub_826CCB88, view, g_pages.show);
  for (u32 i = 0; i < 4; ++i)
    bd::mem::store<u32>(titles + i * 4, 23);
  if (show && bd::mem::load<u32>(show))
    Call(x, base, __imp__sub_826CC180, show, titles);
  for (u32 i = 0; i < 4; ++i)
    g_pages.titles[i] = bd::mem::load<u32>(titles + i * 4);
  Call(x, base, __imp__sub_826CE098, view);
}

// His place in a title's ranking: 0 champion, n, or -1 not in it.
int RankOf(const PPCContext &in, u8 *base, u32 title) {
  static u32 s_out = 0;
  if (!s_out)
    s_out = REX_KERNEL_MEMORY()->SystemHeapAlloc(180 * 32, 0x10);
  if (!s_out || title >= 23)
    return -1;
  PPCContext x = in;
  x.r1.u32 = (x.r1.u32 - 0x1000) & ~0xFu;
  const u32 view = x.r1.u32 + 0x800;
  Call(x, base, __imp__sub_826CDC48, view);
  const u32 n = std::min<u32>(Call(x, base, __imp__sub_826D2508, view, title, s_out, 180), 180);
  Call(x, base, __imp__sub_826CE098, view);
  for (u32 i = 0; i < n; ++i)
    for (u32 k = 0; k < 5; ++k)  // a team's members are +0..+16
      if (bd::mem::load<u32>(s_out + i * 32 + k * 4) == g_pages.id)
        return int(i);
  return -1;
}

void BuildLines(const PPCContext &in, u8 *base) {
  static const char *const kPage[2][5] = {
      {"CHAMPIONSHIPS", "CHAMPIONNATS", "TITEL", "CAMPEONATOS", "TITOLI"},
      {"STATISTICS", "STATISTIQUES", "STATISTIKEN", "ESTAD\xC3\x8DSTICAS", "STATISTICHE"}};
  static const char *const kRank[5] = {"RANK %d", "RANG %d", "RANG %d", "PUESTO %d", "POSIZIONE %d"};
  static const char *const kChampion[5] = {"CHAMPION", "CHAMPION", "CHAMPION", "CAMPE\xC3\x93N",
                                           "CAMPIONE"};
  static const char *const kHeld[5] = {"HELD %u - %u WEEK(S)", "D\xC3\x89TENU %u - %u SEMAINE(S)",
                                       "GEHALTEN %u - %u WOCHE(N)", "%u VECES - %u SEMANA(S)",
                                       "DETENUTO %u - %u SETTIMANA/E"};
  const u32 lang = CurrentLang();
  const u32 name = Call(in, base, __imp__sub_82A32618, g_pages.id);
  const char *n = name ? bd::mem::try_at<const char>(name) : nullptr;
  g_pages.name = n ? n : "";
  g_pages.header = std::string(kPageKeys) + kPage[g_pages.page - 1][lang];
  LoadContenders();
  const u32 mask = g_contenders.count(g_pages.id) ? g_contenders[g_pages.id] : 0;
  u32 reigns[23] = {}, weeks[23] = {};
  if (g_pages.page == 2) {
    const u32 u = Call(in, base, __imp__sub_82683A30, 0);
    const u32 now = u ? bd::mem::load<u32>(u + kUniverseDay) : 0;
    for (const Reign &r : LoadReigns())
      if (r.holder == g_pages.id && r.title < 23) {
        ++reigns[r.title];
        const u32 to = r.to_day == ~0u ? now : r.to_day;
        weeks[r.title] += (to > r.from_day ? to - r.from_day : 0) / 7;
      }
  }
  for (u32 i = 0; i < 4; ++i) {
    u32 t = g_pages.titles[i];
    if (t < 23 && !Call(in, base, __imp__sub_82A39EF8, g_pages.id, t, 1))
      t = 23;  // not his kind of title (a Divas title for a man): "-", as in 2K14
    char buffer[96] = "-";
    if (t < 23 && g_pages.page == 1) {
      if ((mask >> i) & 1) {
        const u32 off = GameText(in, base, kOffText);
        const char *o = off ? bd::mem::try_at<const char>(off) : nullptr;
        std::snprintf(buffer, sizeof(buffer), "%s", o ? o : "OFF");
      } else {
        const int rank = RankOf(in, base, t);
        if (rank == 0)
          std::snprintf(buffer, sizeof(buffer), "%s", kChampion[lang]);
        else if (rank > 0)
          std::snprintf(buffer, sizeof(buffer), kRank[lang], rank);
      }
    } else if (t < 23 && g_pages.page == 2) {
      std::snprintf(buffer, sizeof(buffer), kHeld[lang], reigns[t], weeks[t]);
    }
    g_pages.lines[i] = buffer;
  }
}

// Our texts on the RANKINGS panel, every frame (the game may draw it again).
void ApplyPage(u32 r) {
  const u32 layout = r ? bd::mem::load<u32>(r + 296) : 0;
  const u32 array = layout ? bd::mem::load<u32>(layout + 8) : 0;
  if (!array || bd::mem::load<u32>(layout + 24) <= 887)
    return;
  const auto set = [&](u32 e, u32 text) {
    const u32 at = array + e * 240;
    bd::mem::store<u32>(at + 200, text);
    bd::mem::store<u32>(at + 228, bd::mem::load<u32>(at + 228) & ~0x10u);  // a text, not a number
  };
  set(kHeaderName, PageText(4, g_pages.header));
  for (u32 i = 0; i < 4; ++i) {
    const u32 text = PageText(i, g_pages.lines[i]);
    const bool chosen = g_pages.page == 1 && i == g_pages.row;
    const float rgba[4] = {chosen ? 0.8039f : 1.0f, chosen ? 0.0784f : 1.0f,
                           chosen ? 0.0784f : 1.0f, 1.0f};
    for (u32 e : {kTitleRowName[i], kTitleRowRed[i]}) {  // the line and its copy (the one seen)
      set(e, text);
      for (u32 k = 0; k < 4; ++k)
        bd::mem::store<float>(array + e * 240 + 184 + k * 4, rgba[k]);
    }
  }
}

// ---- the STATISTICS page: a table on the rankings' list page -----------------------------------
//
// A title's ranking (RANKINGS, then A) is opened by its screen's sub_82553770 (R+28; R+56 = the
// title's place) and closed by sub_82552D40 (its B); its list pages (R+56 = page >= 1, R+60 =
// pages) draw 12 rows (sub_82552CE8): name 1020 + k, number 1032 + k, last week's 1044 + k (numbers
// in +220 with flag 0x10 of +228; texts in +200 without it). The page shows our rows instead, as
// WWE 2K14's STATISTICS: sections (TITLES, ROYAL RUMBLE, WRESTLEMANIA, MONEY IN THE BANK) and
// their lines, two columns. Its top: 897 the page's name, 898 / 899 the columns' names (cleared),
// 911 / 912 the champion's label and name (the Superstar's), 913-915 his portrait (hidden).

REX_EXTERN(__imp__sub_82553770);  // a title's ranking: open (its screen)
REX_EXTERN(__imp__sub_82552D40);  // a title's ranking: close (its screen)

constexpr u32 kTableRows = 12, kRowLabel = 1020, kRowHeld = 1032, kRowTotal = 1044;

struct StatWords {
  const char *titles, *held, *weeks, *total, *rumble, *singles, *wins, *entries, *mania, *matches,
      *main_events, *win_rate, *mitb, *cash_rate, *page, *kinds[4];
};
const StatWords &Words() {
  static const StatWords kWords[5] = {
      {"TITLES", "HELD", "WEEKS", "TOTAL", "ROYAL RUMBLE", "WINS", "WINS", "ENTRIES", "WRESTLEMANIA",
       "MATCHES", "MAIN EVENTS", "WIN RATE", "MONEY IN THE BANK", "WIN RATE AFTER CASH-IN",
       "STATISTICS", {"MAJOR", "MINOR", "TAG", "DIVA"}},
      {"TITRES", "D\xC3\x89TENU", "SEMAINES", "TOTAL", "ROYAL RUMBLE", "VICTOIRES", "VICTOIRES",
       "PARTICIPATIONS", "WRESTLEMANIA", "PARTICIPATIONS", "MAIN EVENT", "TAUX DE VICTOIRE",
       "MONEY IN THE BANK", "TAUX DE VICTOIRE APR\xC3\x88S CASH-IN", "STATISTIQUES",
       {"MAJEUR", "MINEUR", "TAG", "DIVA"}},
      {"TITEL", "GEHALTEN", "WOCHEN", "GESAMT", "ROYAL RUMBLE", "SIEGE", "SIEGE", "TEILNAHMEN",
       "WRESTLEMANIA", "MATCHES", "MAIN EVENTS", "SIEGQUOTE", "MONEY IN THE BANK",
       "SIEGQUOTE NACH CASH-IN", "STATISTIKEN", {"MAJOR", "MINOR", "TAG", "DIVA"}},
      {"T\xC3\x8DTULOS", "VECES", "SEMANAS", "TOTAL", "ROYAL RUMBLE", "VICTORIAS", "VICTORIAS",
       "PARTICIPACIONES", "WRESTLEMANIA", "COMBATES", "EVENTO PRINCIPAL", "\xC3\x89XITO",
       "MONEY IN THE BANK", "\xC3\x89XITO TRAS CANJEAR", "ESTAD\xC3\x8DSTICAS",
       {"MAYOR", "MENOR", "PAREJAS", "DIVA"}},
      {"TITOLI", "DETENUTO", "SETTIMANE", "TOTALE", "ROYAL RUMBLE", "VITTORIE", "VITTORIE",
       "PARTECIPAZIONI", "WRESTLEMANIA", "INCONTRI", "MAIN EVENT", "PERCENTUALE VITTORIE",
       "MONEY IN THE BANK", "VITTORIE DOPO IL CASH-IN", "STATISTICHE",
       {"MAGGIORE", "MINORE", "TAG", "DIVA"}}};
  return kWords[CurrentLang()];
}

// The Superstar's lines, from universe_stats.txt.
void BuildTable(const PPCContext &in, u8 *base) {
  const StatWords &w = Words();
  const u32 u = Call(in, base, __imp__sub_82683A30, 0);
  const u32 now = u ? bd::mem::load<u32>(u + kUniverseDay) : 0;
  u32 reigns[23] = {}, weeks[23] = {};
  for (const Reign &r : LoadReigns())
    if (r.holder == g_pages.id && r.title < 23) {
      ++reigns[r.title];
      const u32 to = r.to_day == ~0u ? now : r.to_day;
      weeks[r.title] += (to > r.from_day ? to - r.from_day : 0) / 7;
    }
  const MatchStats m = LoadMatchStats(g_pages.id);
  const auto num = [](u32 v) { return std::to_string(v); };
  const auto rate = [](u32 won, u32 of) {
    return of ? std::to_string(won * 100 / of) + "%" : std::string("0%");
  };
  auto &t = g_pages.table;
  t.clear();
  const auto two = [](const std::string &a, const std::string &b) { return a + "  -  " + b; };
  t.push_back({w.titles, two(w.held, w.weeks), true});
  u32 all_reigns = 0, all_weeks = 0;
  for (u32 i = 0; i < 4; ++i) {
    const u32 title = g_pages.titles[i];
    std::string name = w.kinds[i];
    if (title < 23) {
      const u32 n = Call(in, base, __imp__sub_826D2C30, title);
      const char *s = n ? bd::mem::try_at<const char>(n) : nullptr;
      name += std::string("  ") + (s ? s : "");
    }
    const bool his = title < 23 && Call(in, base, __imp__sub_82A39EF8, g_pages.id, title, 1);
    t.push_back({name, his ? two(num(reigns[title]), num(weeks[title])) : "--"});
  }
  for (u32 title = 0; title < 23; ++title) {
    all_reigns += reigns[title];
    all_weeks += weeks[title];
  }
  t.push_back({w.total, two(num(all_reigns), num(all_weeks))});
  t.push_back({w.rumble, w.total, true});
  t.push_back({w.wins, num(m.rumble_wins)});
  t.push_back({w.entries, num(m.rumble_entries)});
  t.push_back({w.mania, w.total, true});
  t.push_back({w.matches, num(m.mania_matches)});
  t.push_back({w.main_events, num(m.mania_main_events)});
  t.push_back({w.wins, num(m.mania_wins)});
  t.push_back({w.win_rate, rate(m.mania_wins, m.mania_matches)});
  t.push_back({w.mitb, w.total, true});
  t.push_back({w.wins, num(m.mitb_wins)});
  t.push_back({w.cash_rate, rate(m.cash_in_wins, m.cash_ins)});
  g_pages.top = g_pages.name;
  g_pages.subtitle = w.page;
}

// ---- the Universe's statistics, as WWE 2K14's WWE.com > STATISTICS (RANKINGS, then Y) ------------
//
// The same table as a Superstar's STATISTICS (a title's ranking page with our rows), four pages with
// LB / RB: TITLES (per title: most reigns, most weeks held), CHAMPIONS (per title: the champion and
// the former ones, weeks held), MONEY IN THE BANK (the briefcase's holder, the former winners and
// their cash-in), ROYAL RUMBLE (the winners and their WrestleMania match). From universe_stats.txt:
// what happened since the statistics are kept.

struct GlobalWords {
  const char *champions, *most_reigns, *most_weeks, *champion, *former, *holder, *winners, *won,
      *lost, *wm_rate, *nobody, *weeks;
};
const GlobalWords &GWords() {
  static const GlobalWords kWords[5] = {
      {"CHAMPIONS", "MOST REIGNS", "MOST WEEKS HELD", "CHAMPION", "FORMER CHAMPIONS", "HOLDER",
       "PREVIOUS WINNERS", "WON", "LOST", "WRESTLEMANIA WIN RATE", "NOBODY YET", "WEEKS"},
      {"CHAMPIONS", "NB DE FOIS REMPORT\xC3\x89", "NB DE SEMAINES D\xC3\x89TENU", "CHAMPION",
       "ANCIENS CHAMPIONS", "D\xC3\x89TENTEUR", "VAINQUEURS PR\xC3\x89" "C\xC3\x89" "DENTS", "GAGN\xC3\x89",
       "PERDU", "TAUX DE VICTOIRE WRESTLEMANIA", "PERSONNE POUR L'INSTANT", "SEMAINES"},
      {"CHAMPIONS", "MEISTE REGENTSCHAFTEN", "MEISTE WOCHEN", "CHAMPION", "FR\xC3\x9CHERE CHAMPIONS",
       "INHABER", "FR\xC3\x9CHERE SIEGER", "GEWONNEN", "VERLOREN", "SIEGQUOTE WRESTLEMANIA",
       "NOCH NIEMAND", "WOCHEN"},
      {"CAMPEONES", "M\xC3\x81S REINADOS", "M\xC3\x81S SEMANAS", "CAMPE\xC3\x93N",
       "CAMPEONES ANTERIORES", "PORTADOR", "GANADORES ANTERIORES", "GAN\xC3\x93", "PERDI\xC3\x93",
       "\xC3\x89XITO EN WRESTLEMANIA", "NADIE A\xC3\x9AN", "SEMANAS"},
      {"CAMPIONI", "PI\xC3\x99 REGNI", "PI\xC3\x99 SETTIMANE", "CAMPIONE", "EX CAMPIONI", "DETENTORE",
       "VINCITORI PRECEDENTI", "VINTO", "PERSO", "VITTORIE A WRESTLEMANIA", "ANCORA NESSUNO",
       "SETTIMANE"}};
  return kWords[CurrentLang()];
}

std::string NameOf(const PPCContext &in, u8 *base, u32 id) {
  if (id >= 512)
    return "--";
  const u32 n = Call(in, base, __imp__sub_82A32618, id);
  const char *t = n ? bd::mem::try_at<const char>(n) : nullptr;
  return t ? t : "?";
}

std::string TitleName(const PPCContext &in, u8 *base, u32 title) {
  const u32 n = Call(in, base, __imp__sub_826D2C30, title);
  const char *t = n ? bd::mem::try_at<const char>(n) : nullptr;
  return t ? t : "?";
}

struct Event {
  std::string kind;
  u32 id, a, b, day;
};
std::vector<Event> LoadEvents() {
  std::vector<Event> events;
  std::ifstream f(StatsFile());
  std::string line;
  while (std::getline(f, line)) {
    char kind[16] = {};
    u32 v[4] = {};
    const int n = std::sscanf(line.c_str(), "%15s %u %u %u %u", kind, &v[0], &v[1], &v[2], &v[3]);
    if (n < 3)
      continue;
    const std::string k = kind;
    if (k == "rumble" && n >= 4)
      events.push_back({k, v[0], v[1], 0, v[2]});
    else if (k == "mania" && n >= 5)
      events.push_back({k, v[0], v[1], v[2], v[3]});
    else if (k == "mitb")
      events.push_back({k, v[0], 0, 0, v[1]});
    else if (k == "cashin" && n >= 4)
      events.push_back({k, v[0], v[1], 0, v[2]});
  }
  return events;
}

void BuildGlobal(const PPCContext &ctx, u8 *base) {
  PPCContext in = ctx;
  in.r1.u32 = (in.r1.u32 - 0x1000) & ~0xFu;
  const StatWords &w = Words();
  const GlobalWords &g = GWords();
  const u32 u = Call(in, base, __imp__sub_82683A30, 0);
  const u32 now = u ? bd::mem::load<u32>(u + kUniverseDay) : 0;
  const auto num = [](u32 v) { return std::to_string(v); };
  const auto weeks_of = [&](const Reign &r) {
    const u32 to = r.to_day == ~0u ? now : r.to_day;
    return (to > r.from_day ? to - r.from_day : 0) / 7;
  };
  // the shows' titles, in the shows' order, then any other title with a history
  std::vector<u32> titles;
  {
    const u32 view = in.r1.u32 + 0x800, out = view + 0x200;
    Call(in, base, __imp__sub_826CDC48, view);
    for (u32 k = 0; k < 2; ++k) {
      const u32 show = Call(in, base, __imp__sub_826CCB88, view, k);
      if (!show || !bd::mem::load<u32>(show))
        continue;
      for (u32 i = 0; i < 4; ++i)
        bd::mem::store<u32>(out + i * 4, 23);
      Call(in, base, __imp__sub_826CC180, show, out);
      for (u32 i = 0; i < 4; ++i) {
        const u32 t = bd::mem::load<u32>(out + i * 4);
        if (t < 23 && std::find(titles.begin(), titles.end(), t) == titles.end())
          titles.push_back(t);
      }
    }
    Call(in, base, __imp__sub_826CE098, view);
  }
  const std::vector<Reign> reigns = LoadReigns();
  for (const Reign &r : reigns)
    if (r.title < 23 && std::find(titles.begin(), titles.end(), r.title) == titles.end())
      titles.push_back(r.title);
  const std::vector<Event> events = LoadEvents();
  auto &t = g_pages.table;
  t.clear();
  g_pages.top = w.page;
  switch (g_pages.section) {
  case 0:  // TITLES: the records
    g_pages.subtitle = w.titles;
    for (u32 title : titles) {
      std::map<u32, u32> count, held;
      for (const Reign &r : reigns)
        if (r.title == title) {
          ++count[r.holder];
          held[r.holder] += weeks_of(r);
        }
      const auto best = [](const std::map<u32, u32> &m) {
        std::pair<u32, u32> b{512, 0};
        for (const auto &[id, v] : m)
          if (v > b.second)
            b = {id, v};
        return b;
      };
      const auto [most, times] = best(count);
      const auto [longest, wk] = best(held);
      t.push_back({TitleName(in, base, title), "", true});
      t.push_back({std::string(g.most_reigns) + ":  " + (times ? NameOf(in, base, most) : "--"),
                   times ? num(times) : "--"});
      t.push_back({std::string(g.most_weeks) + ":  " + (wk ? NameOf(in, base, longest) : "--"),
                   wk ? num(wk) : "--"});
    }
    break;
  case 1:  // CHAMPIONS: the champion, then the former ones (latest first), weeks held
    g_pages.subtitle = g.champions;
    for (u32 title : titles) {
      t.push_back({TitleName(in, base, title), g.weeks, true});
      std::string champ;
      for (u32 place = 0; place < 2; ++place) {
        const u32 id = Call(in, base, __imp__sub_82A39FC8, title, place, 1) & 0xFFFF;
        if (id < 512)
          champ += (champ.empty() ? "" : " & ") + NameOf(in, base, id);
      }
      u32 champ_weeks = 0;
      for (const Reign &r : reigns)
        if (r.title == title && r.to_day == ~0u && r.place == 0)
          champ_weeks = weeks_of(r);
      t.push_back({std::string(g.champion) + ":  " + (champ.empty() ? "--" : champ),
                   champ.empty() ? "--" : num(champ_weeks)});
      u32 shown = 0;
      for (auto r = reigns.rbegin(); r != reigns.rend() && shown < 5; ++r)
        if (r->title == title && r->to_day != ~0u) {
          t.push_back({std::string(shown ? "" : g.former) + (shown ? "      " : ":  ") +
                           NameOf(in, base, r->holder),
                       num(weeks_of(*r))});
          ++shown;
        }
    }
    break;
  case 2: {  // MONEY IN THE BANK
    g_pages.subtitle = w.mitb;
    const u32 holder = u ? bd::mem::load<u16>(u + 51004) : 512;
    t.push_back({w.mitb, "", true});
    t.push_back({std::string(g.holder) + ":  " + (holder < 512 ? NameOf(in, base, holder) : "--"),
                 ""});
    u32 cash_ins = 0, cash_wins = 0;
    for (const Event &e : events)
      if (e.kind == "cashin") {
        ++cash_ins;
        cash_wins += e.a ? 1 : 0;
      }
    t.push_back({w.cash_rate, cash_ins ? num(cash_wins * 100 / cash_ins) + "%" : "0%"});
    t.push_back({g.winners, "CASH-IN", true});
    bool any = false;
    for (auto e = events.rbegin(); e != events.rend(); ++e) {
      if (e->kind != "mitb")
        continue;
      std::string result = "--";
      for (const Event &c : events)
        if (c.kind == "cashin" && c.id == e->id && c.day >= e->day) {
          result = c.a ? g.won : g.lost;
          break;
        }
      t.push_back({NameOf(in, base, e->id), result});
      any = true;
    }
    if (!any)
      t.push_back({g.nobody, ""});
    break;
  }
  default: {  // ROYAL RUMBLE: the winners and their next WrestleMania match
    g_pages.subtitle = w.rumble;
    u32 wm = 0, wm_wins = 0;
    std::vector<SuperstarPages::Row> rows;
    for (auto e = events.rbegin(); e != events.rend(); ++e) {
      if (e->kind != "rumble" || !e->a)
        continue;
      std::string result = "--";
      for (const Event &m : events)
        if (m.kind == "mania" && m.id == e->id && m.day > e->day) {
          result = m.b ? g.won : g.lost;
          ++wm;
          wm_wins += m.b ? 1 : 0;
          break;
        }
      rows.push_back({NameOf(in, base, e->id), result});
    }
    t.push_back({w.rumble, "", true});
    t.push_back({g.wm_rate, wm ? num(wm_wins * 100 / wm) + "%" : "0%"});
    t.push_back({g.winners, "WM", true});
    if (rows.empty())
      t.push_back({g.nobody, ""});
    t.insert(t.end(), rows.begin(), rows.end());
    break;
  }
  }
}

u32 TableText(u32 slot, const std::string &text) {  // guest strings of ours, 64 bytes each
  static u32 s_block = 0;
  constexpr u32 kSlots = 3 * kTableRows + 8;
  if (!s_block)
    s_block = REX_KERNEL_MEMORY()->SystemHeapAlloc(kSlots * 64, 0x10);
  if (!s_block || slot >= kSlots)
    return 0;
  const u32 at = s_block + slot * 64;
  std::memset(bd::mem::at<char>(at), 0, 64);
  std::memcpy(bd::mem::at<char>(at), text.data(), std::min<size_t>(text.size(), 63));
  return at;
}

// The table's own layout, as WWE 2K14's: the champion's banner (907-912, its copy 916-921) hidden,
// the Superstar's name on top (897) with LB / RB and the page's name under it (898), the 12 rows
// bigger and spread over the banner's place: stripes 996 + k / 1008 + k, labels 1020 + k, values in
// the rank's column 1032 + k (white; the last-week column 1044 + k is grey and hidden), arrows 900 /
// 901. Element: +4 x, +8 y, +12 / +16 scale, +180 hidden. The game's values are kept and put back
// when the table closes (the RANKINGS' own page uses the same layout).
struct Placed {
  u32 e;
  float x, y, sx, sy;
  u32 hidden;
};
std::vector<Placed> g_table_placed;
u32 g_table_array = 0;

constexpr float kStripe0 = 92, kStripeStep = 17;

void Place(u32 array, u32 e, float x, float y, float scale, bool hidden) {
  const u32 at = array + e * 240;
  if (g_table_array != array) {
    g_table_placed.clear();
    g_table_array = array;
  }
  if (std::none_of(g_table_placed.begin(), g_table_placed.end(),
                   [&](const Placed &p) { return p.e == e; }))
    g_table_placed.push_back({e, bd::mem::load<float>(at + 4), bd::mem::load<float>(at + 8),
                              bd::mem::load<float>(at + 12), bd::mem::load<float>(at + 16),
                              bd::mem::load<u32>(at + 180)});
  const Placed &old = *std::find_if(g_table_placed.begin(), g_table_placed.end(),
                                    [&](const Placed &p) { return p.e == e; });
  bd::mem::store<float>(at + 4, x < 0 ? old.x : x);
  bd::mem::store<float>(at + 8, y < 0 ? old.y : y);
  bd::mem::store<float>(at + 12, scale < 0 ? old.sx : scale);
  bd::mem::store<float>(at + 16, scale < 0 ? old.sy : scale);
  bd::mem::store<u32>(at + 180, hidden);
}

void RestoreTableLayout(u32 r) {
  const u32 layout = r ? bd::mem::load<u32>(r + 296) : 0;
  const u32 array = layout ? bd::mem::load<u32>(layout + 8) : 0;
  if (array && array == g_table_array)
    for (const Placed &p : g_table_placed) {
      const u32 at = array + p.e * 240;
      bd::mem::store<float>(at + 4, p.x);
      bd::mem::store<float>(at + 8, p.y);
      bd::mem::store<float>(at + 12, p.sx);
      bd::mem::store<float>(at + 16, p.sy);
      bd::mem::store<u32>(at + 180, p.hidden);
    }
  g_table_placed.clear();
  g_table_array = 0;
}

// Our rows on the list page, every frame.
void ApplyTable(u32 r) {
  const u32 layout = r ? bd::mem::load<u32>(r + 296) : 0;
  const u32 array = layout ? bd::mem::load<u32>(layout + 8) : 0;
  if (!array || bd::mem::load<u32>(layout + 24) < kRowTotal + kTableRows)  // up to element 1055
    return;
  const auto element = [&](u32 e) { return array + e * 240; };
  const auto text = [&](u32 e, u32 t) {
    bd::mem::store<u32>(element(e) + 200, t);
    bd::mem::store<u32>(element(e) + 228, bd::mem::load<u32>(element(e) + 228) & ~0x10u);
  };
  text(897, TableText(3 * kTableRows, g_pages.top));
  text(898, TableText(3 * kTableRows + 1, std::string(kPageKeys) + g_pages.subtitle));
  text(899, TableText(3 * kTableRows + 2, ""));
  Place(array, 898, 96, 55, 0.7f, false);
  for (u32 e = 907; e <= 921; ++e)  // the champion's banner and its copy
    Place(array, e, -1, -1, -1, true);
  const float last = kStripe0 + kStripeStep * (kTableRows - 1);
  Place(array, 900, -1, kStripe0 - 10, -1, false);
  Place(array, 901, -1, last - 10, -1, false);
  for (u32 k = 0; k < kTableRows; ++k) {
    const size_t i = (g_pages.table_page - 1) * kTableRows + k;
    const bool shown = i < g_pages.table.size();
    const float y = kStripe0 + kStripeStep * k;
    Place(array, 996 + k, -1, y, -1, false);
    Place(array, 1008 + k, -1, y, -1, false);
    Place(array, kRowLabel + k, -1, y - 18, 0.8f, !shown);
    Place(array, kRowHeld + k, 392, y - 11, 0.85f, !shown);  // the values, white
    Place(array, kRowTotal + k, -1, -1, -1, true);
    if (!shown)
      continue;
    const auto &row = g_pages.table[i];
    text(kRowLabel + k, TableText(k * 3, row.label));
    text(kRowHeld + k, TableText(k * 3 + 1, row.value));
    const float red[4] = {0.8039f, 0.0784f, 0.0784f, 1.0f}, white[4] = {1, 1, 1, 1};
    for (u32 c = 0; c < 4; ++c)
      bd::mem::store<float>(element(kRowLabel + k) + 184 + c * 4, row.section ? red[c] : white[c]);
    for (u32 c = 0; c < 4; ++c)
      bd::mem::store<float>(element(kRowHeld + k) + 184 + c * 4, row.section ? red[c] : white[c]);
  }
}

void ShowTablePage(const PPCContext &in, u8 *base, u32 r, u32 page) {
  g_pages.table_page = page;
  bd::mem::store<u32>(r + 56, page);  // the list's page
  bd::mem::store<u32>(r + 60, std::max<u32>(bd::mem::load<u32>(r + 60), std::max<u32>(3, page + 1)));
  Call(in, base, __imp__sub_82552CE8, r + 28);
  ApplyTable(r);
}

void OpenTable(const PPCContext &in, u8 *base, u32 r) {
  bd::mem::store<u32>(r + 56, 0);  // the show's first title: its ranking has rows enough
  Call(in, base, __imp__sub_82553770, r + 28);
  ShowTablePage(in, base, r, 1);
}

void CloseTable(const PPCContext &in, u8 *base, u32 r) {
  RestoreTableLayout(r);
  if (bd::mem::load<u32>(r + 2360) == 1)
    Call(in, base, __imp__sub_82552D40, r + 28);
}

void KeepButtons();

void OpenPage(const PPCContext &in, u8 *base, u32 page) {
  const u32 m = g_universe_menu;
  const u32 r = m ? bd::mem::load<u32>(m + kRankingsPanel) : 0;
  if (!r || (g_pages.page == 0 && bd::mem::load<u32>(r + 2360) != 0))
    return;
  if (g_pages.page == 2 && page != 2)
    CloseTable(in, base, r);
  if (g_pages.page == 0) {
    g_pages.old_panel = bd::mem::load<u32>(m + kPanelShown);
    g_pages.page_input = bd::mem::load<u32>(g_pages.screen + 224);
    g_pages.menu_input = bd::mem::load<u32>(m + 28 + 224);
    BD_INFO("[univers] pages: buttons of his page {}, of the menu {}", g_pages.page_input,
            g_pages.menu_input);
    g_pages.id = Call(in, base, __imp__sub_82DEA3A8, 0);
    g_pages.row = 0;
    g_pages.menu_state = bd::mem::load<u32>(m + kMenuState);
    g_pages.list_row = bd::mem::load<u32>(g_pages.old_panel + kSuperstarList + kListScroll);
    g_pages.list_top = bd::mem::load<u32>(g_pages.old_panel + kSuperstarList + kListScroll + 4);
    ReadSuperstar(in, base);
    SetPanel(in, base, r);
  }
  g_pages.page = page;
  KeepButtons();
  bd::mem::store<u32>(r + 2352, g_pages.show);
  Call(in, base, __imp__sub_82552768, r);
  Call(in, base, __imp__sub_82551CB8, r);
  Call(in, base, __imp__sub_82551F48, r);
  Call(in, base, __imp__sub_82552048, r);
  BuildLines(in, base);
  if (page == 2) {
    BuildTable(in, base);
    OpenTable(in, base, r);
  }
  BD_INFO("[univers] Superstar {} page {} (show {}, titles {} {} {} {})", g_pages.id, page,
          g_pages.show, g_pages.titles[0], g_pages.titles[1], g_pages.titles[2],
          g_pages.titles[3]);
}

// Hiding EDIT UNIVERSE's panel hands the buttons to the Universe menu (its screen S = M+28):
// while our pages show, the Superstar's page keeps them (layout screens' +224: 0 none, 1 from the
// next frame, 2 now) and S has none.
void KeepButtons() {
  if (!g_pages.page || !g_universe_menu)
    return;
  bd::mem::store<u32>(g_pages.screen + 224, 2);
  bd::mem::store<u32>(g_universe_menu + 28 + 224, 0);
  if (const u32 r = bd::mem::load<u32>(g_universe_menu + kRankingsPanel))
    bd::mem::store<u32>(r + 28 + 224, 0);  // a title's ranking (the table) takes none either
}

void ClosePages(const PPCContext &in, u8 *base) {
  if (g_pages.page == 2 && g_universe_menu)
    CloseTable(in, base, bd::mem::load<u32>(g_universe_menu + kRankingsPanel));
  if (g_pages.page && g_universe_menu && g_pages.old_panel) {
    SetPanel(in, base, g_pages.old_panel);
    const u32 list = g_pages.old_panel + kSuperstarList;
    bd::mem::store<u32>(list + kListScroll, g_pages.list_row);
    bd::mem::store<u32>(list + kListScroll + 4, g_pages.list_top);
    bd::mem::store<u32>(list + 88, 0);
    Call(in, base, __imp__sub_8258D240, list);                // his row again, and the header
    Call(in, base, __imp__sub_825ACEC0, g_pages.screen - 28);  // his page drawn again
    bd::mem::store<u32>(g_universe_menu + kMenuState, g_pages.menu_state);
    bd::mem::store<u32>(g_pages.screen + 224, g_pages.page_input);  // as before
    bd::mem::store<u32>(g_universe_menu + 28 + 224, g_pages.menu_input);
    Call(in, base, sub_825AAD38, g_pages.screen - 28);  // its button bar again
  }
  g_pages.page = 0;
}

// The Superstar's page buttons while our pages show (all of them ours). False: not ours.
bool SuperstarPageButtons(const PPCContext &in, u8 *base, u32 screen) {
  if (!Enabled() || !g_universe_menu || bd::mem::load<u32>(kInUniverseFlag) != 1)
    return false;
  const u32 pressed = bd::mem::load<u32>(screen + 64);
  if (pressed && REXCVAR_GET(wwe12_universe_probe))
    BD_INFO("[univers] probe: pages {} buttons 0x{:X}; menu +224 {}, rankings screen +224 {}",
            g_pages.page, pressed, bd::mem::load<u32>(g_universe_menu + 28 + 224),
            bd::mem::load<u32>(bd::mem::load<u32>(g_universe_menu + kRankingsPanel) + 28 + 224));
  if (g_pages.page == 0) {
    if (!(pressed & (kRB | kLB)))
      return false;
    g_pages.screen = screen;
    OpenPage(in, base, pressed & kRB ? 1 : 2);
  } else if (pressed & kB) {
    ClosePages(in, base);
  } else if (pressed & (kRB | kLB)) {
    const u32 next = (g_pages.page + (pressed & kRB ? 1 : 2)) % 3;
    if (next == 0)
      ClosePages(in, base);
    else
      OpenPage(in, base, next);
  } else if (g_pages.page == 2) {
    const u32 dir = std::min<u32>(bd::mem::load<u32>(screen + 20), 3);
    const u32 up = bd::mem::load<u32>(kListMasks + dir * 16) | kUp;
    const u32 down = bd::mem::load<u32>(kListMasks + dir * 16 + 8) | kDown;
    const u32 r = bd::mem::load<u32>(g_universe_menu + kRankingsPanel);
    const u32 pages = u32((g_pages.table.size() + kTableRows - 1) / kTableRows);
    if ((pressed & up) && g_pages.table_page > 1)
      ShowTablePage(in, base, r, g_pages.table_page - 1);
    else if ((pressed & down) && g_pages.table_page < pages)
      ShowTablePage(in, base, r, g_pages.table_page + 1);
  } else if (g_pages.page == 1) {
    const u32 dir = std::min<u32>(bd::mem::load<u32>(screen + 20), 3);
    const u32 up = bd::mem::load<u32>(kListMasks + dir * 16) | kUp;
    const u32 down = bd::mem::load<u32>(kListMasks + dir * 16 + 8) | kDown;
    if ((pressed & up) && g_pages.row > 0)
      --g_pages.row;
    else if ((pressed & down) && g_pages.row < 3)
      ++g_pages.row;
    if ((pressed & (kA | kX)) && g_pages.titles[g_pages.row] < 23) {  // on / off
      LoadContenders();
      u32 &mask = g_contenders[g_pages.id];
      mask ^= 1u << g_pages.row;
      if (!mask)
        g_contenders.erase(g_pages.id);
      SaveContenders();
      BuildLines(in, base);
    }
  }
  bd::mem::store<u32>(screen + 64, 0);
  if (g_pages.page) {
    KeepButtons();
    const u32 r = bd::mem::load<u32>(g_universe_menu + kRankingsPanel);
    if (g_pages.page == 1)
      ApplyPage(r);
    else
      ApplyTable(r);
  }
  return true;
}

// RANKINGS' buttons for the Universe's statistics: Y on its first screen opens them; then LB / RB
// their pages, up / down the table's pages, B back. True: the frame's buttons were ours.
bool GlobalStatsButtons(const PPCContext &in, u8 *base, u32 screen) {
  const u32 r = RankObject(screen), pressed = bd::mem::load<u32>(screen + 64);
  const bool detail = bd::mem::load<u32>(screen + 2332) == 1;
  if (!g_pages.global) {
    if (!(pressed & kY) || detail || g_pages.page)
      return false;
    g_pages.global = true;
    g_pages.section = 0;
    g_pages.global_place = bd::mem::load<u32>(r + 56);
    BuildGlobal(in, base);
    OpenTable(in, base, r);
    BD_INFO("[univers] statistics: open ({} rows)", g_pages.table.size());
  } else if (!detail) {  // closed meanwhile
    RestoreTableLayout(r);
    g_pages.global = false;
    return false;
  } else if (pressed & kB) {
    bd::mem::store<u32>(r + 56, g_pages.global_place);
    CloseTable(in, base, r);
    g_pages.global = false;
    BD_INFO("[univers] statistics: closed");
  } else if (pressed & (kLB | kRB)) {
    g_pages.section = (g_pages.section + (pressed & kRB ? 1 : 3)) % 4;
    BuildGlobal(in, base);
    ShowTablePage(in, base, r, 1);
  } else {
    const u32 dir = std::min<u32>(bd::mem::load<u32>(screen + 20), 3);
    const u32 up = bd::mem::load<u32>(kListMasks + dir * 16) | kUp;
    const u32 down = bd::mem::load<u32>(kListMasks + dir * 16 + 8) | kDown;
    const u32 pages = u32((g_pages.table.size() + kTableRows - 1) / kTableRows);
    if ((pressed & up) && g_pages.table_page > 1)
      ShowTablePage(in, base, r, g_pages.table_page - 1);
    else if ((pressed & down) && g_pages.table_page < pages)
      ShowTablePage(in, base, r, g_pages.table_page + 1);
  }
  bd::mem::store<u32>(screen + 64, 0);
  if (g_pages.global)
    ApplyTable(r);
  return true;
}

}  // namespace

// The rankings' list rows and its top block are drawn again by the game (sub_82552A78,
// sub_82552240, R in r3): while our STATISTICS page shows, our texts go back on after them.
REX_EXTERN(__imp__sub_82552A78);
extern "C" REX_FUNC(sub_82552A78) {
  const u32 r = ctx.r3.u32;
  __imp__sub_82552A78(ctx, base);
  if (g_pages.page == 2 || g_pages.global)
    ApplyTable(r);
}

REX_EXTERN(__imp__sub_82552240);
extern "C" REX_FUNC(sub_82552240) {
  const u32 r = ctx.r3.u32;
  __imp__sub_82552240(ctx, base);
  if (g_pages.page == 2 || g_pages.global)
    ApplyTable(r);
}

extern "C" REX_FUNC(sub_824D20A0) {
  const u32 screen = ctx.r3.u32;
  if (bd::mem::try_load<u32>(screen) == kSuperstarPageVtbl) {
    if (SuperstarPageButtons(ctx, base, screen))
      return;
  } else if (g_pages.page && bd::mem::try_load<u32>(g_pages.screen) != kSuperstarPageVtbl) {
    ClosePages(ctx, base);  // his page closed meanwhile
  }
  // Dev (wwe12_universe_probe): the buttons of a Superstar's page and of RANKINGS, and the shared
  // layout as each shows it (LB / RB pressed: logs/umsi_<screen>_<n>.txt), for the pages to come.
  if (REXCVAR_GET(wwe12_universe_probe) && bd::mem::load<u32>(kInUniverseFlag) == 1) {
    const u32 vtbl = bd::mem::try_load<u32>(screen), pressed = bd::mem::load<u32>(screen + 64);
    if (pressed)
      BD_INFO("[univers] probe: screen {:08X} (vtable {:08X}) buttons 0x{:X}, +224 {}", screen,
              vtbl, pressed, bd::mem::load<u32>(screen + 224));
    if (pressed && (vtbl == kSuperstarPageVtbl || vtbl == kRankingVtbl)) {
      static u32 s_dumps = 0;
      if ((pressed & 0x300) && s_dumps < 8) {
        const char *what = vtbl == kSuperstarPageVtbl ? "superstar" : "rankings";
        DumpPageLayout(ctx, base, fmt::format("umsi_{}_{}.txt", what, s_dumps).c_str(), false);
        // the rankings screen's own layout (R+296, R = the Universe menu's +264)
        const u32 r = g_universe_menu ? bd::mem::load<u32>(g_universe_menu + 264) : 0;
        const u32 layout = r ? bd::mem::load<u32>(r + 296) : 0;
        BD_INFO("[univers] probe: Universe menu {:08X}, rankings {:08X}, its layout {:08X}",
                g_universe_menu, r, layout);
        if (layout)
          DumpPageLayout(ctx, base, fmt::format("rlay_{}_{}.txt", what, s_dumps).c_str(), false,
                         layout);
        ++s_dumps;
      }
    }
  }
  if (!Enabled() || bd::mem::try_load<u32>(screen) != kRankingVtbl ||
      bd::mem::load<u32>(kInUniverseFlag) != 1) {
    __imp__sub_824D20A0(ctx, base);
    return;
  }
  if (g_rank.screen != screen && GlobalStatsButtons(ctx, base, screen))
    return;
  RankEditPrompt();
  const u32 r = RankObject(screen), pressed = bd::mem::load<u32>(screen + 64);
  const bool detail = bd::mem::load<u32>(screen + 2332) == 1;  // a title's ranking is open
  if (g_rank.screen != screen) {
    if (g_rank.screen)
      RankEditEnd();
    if (!(pressed & kX) || !detail || RankCount(r) < 3 ||
        Call(ctx, base, __imp__sub_82A39CE8, bd::mem::load<u32>(r + 8848)) != 0) {
      __imp__sub_824D20A0(ctx, base);
      return;
    }
    g_rank.screen = screen;
    g_rank.held = false;
    const u32 page = bd::mem::load<u32>(screen + 28);
    g_rank.cursor = page == 0 ? 1 : std::min(RankCount(r) - 1, 5 + (page - 1) * 12);
    BD_INFO("[univers] ranking edit: on (title {}, {} ranked)", bd::mem::load<u32>(r + 8848),
            RankCount(r));
    if (REXCVAR_GET(wwe12_universe_probe))
      DumpRankLayout(r);
  } else if (!detail || (pressed & (kB | kX))) {
    BD_INFO("[univers] ranking edit: off");
    RankEditEnd();
  } else {
    // up / down as the screen's list reads them: masks by its direction S+20 (sub_824D2148)
    const u32 dir = std::min<u32>(bd::mem::load<u32>(screen + 20), 3);
    const u32 up = bd::mem::load<u32>(kListMasks + dir * 16) | kUp;
    const u32 down = bd::mem::load<u32>(kListMasks + dir * 16 + 8) | kDown;
    if (pressed && REXCVAR_GET(wwe12_universe_probe))
      BD_INFO("[univers] ranking edit: buttons 0x{:X} (list direction {})", pressed, dir);
    const u32 n = RankCount(r);
    u32 next = g_rank.cursor;
    if ((pressed & up) && g_rank.cursor > 1)
      next = g_rank.cursor - 1;
    else if ((pressed & down) && g_rank.cursor + 1 < n)
      next = g_rank.cursor + 1;
    const bool swap = next != g_rank.cursor && g_rank.held;
    if (swap)
      RankSwap(ctx, base, r, g_rank.cursor, next);
    g_rank.cursor = next;
    if (pressed & kA)
      g_rank.held = !g_rank.held;
    const u32 page = RankPage(g_rank.cursor);
    if (page != bd::mem::load<u32>(screen + 28)) {
      // as the screen's own list does (sub_824D2148): the page and its "moved" (vtable slot 6);
      // then the rows (vtable slot 20, sub_82552A78 inside)
      const u32 down = page > bd::mem::load<u32>(screen + 28);
      bd::mem::store<u32>(screen + 28, page);
      Call(ctx, base, __imp__sub_823D35B8, screen, down ? 2u : 1u);
      Call(ctx, base, __imp__sub_82552CE8, screen);  // the rows of the new page
    } else if (swap) {
      Call(ctx, base, __imp__sub_82552CE8, screen);  // the page's rows in their new order
    }
  }
  bd::mem::store<u32>(screen + 64, 0);  // the frame's buttons were ours
  if (g_rank.screen == screen)
    Light(r);
}


// ---- a show's minimum roster (15 in the game) --------------------------------------------------
//
// The game wants 15 Superstars per show, tested in three places, each with its own 15:
// - sub_826D3B58(id) "can leave his show" (a transfer to another brand, or back to the default:
//   messages 51274 / 51275; also sub_8258A088): his show's roster sub_826D0598(show, 0, 0, 1)
//   must be > 15;
// - sub_82599DB0, a contract (message 51411): counts the show's Superstars with
//   sub_826CC0B8(show, id, ...) over all 373, needs 15;
// - sub_82583B78, a show's brands chosen in EDIT BRAND (message 51413): counts the Superstars of
//   the chosen brands (S + (brand + 66) * 4 = 1), needs 15, else S+428 = {0, 51413, 1}.
// With wwe12_universe_min_roster below 15: in the first two, the count is raised by 15 - minimum
// (only for the call made there, known by its return address); the third is redone after the
// game refused: our own count, and if it is enough, the game's own "accepted" end (the 13 shows'
// brands kept with sub_826CC0E0, the screen's done flag, sub_82550538). A copy of the save is made
// the first time a roster smaller than the game's minimum is let through.

REXCVAR_DEFINE_INT32(wwe12_universe_min_roster, 15, "WWE12",
                     "Universe: the fewest Superstars a show may have (the game's own is 15).");

REX_EXTERN(__imp__sub_826D3B58);  // roster: may this Superstar leave his show?
REX_EXTERN(__imp__sub_826D0598);  // roster: a show's size (show, 0, 0, 1)
REX_EXTERN(__imp__sub_826CC0B8);  // roster: is this Superstar on the show? (show, id)
REX_EXTERN(__imp__sub_82583B78);  // EDIT BRAND: the show's brands chosen
REX_EXTERN(__imp__sub_82A32DB8);  // Superstar: a woman?
REX_EXTERN(__imp__sub_826D2B50);  // Superstar: his brand
REX_EXTERN(__imp__sub_826CC0E0);  // show: set a brand (shows, brand, on)
REX_EXTERN(__imp__sub_82550538);  // EDIT BRAND: done

namespace {

constexpr u32 kGameMinRoster = 15;
constexpr u32 kLeaveCountReturn = 0x826D3C84;     // sub_826D3B58's call to sub_826D0598
constexpr u32 kContractCountReturn = 0x8259A028;  // sub_82599DB0's call to sub_826CC0B8
constexpr u32 kShowRosterText = 51413;

u32 MinRoster() {
  const int32_t min = REXCVAR_GET(wwe12_universe_min_roster);
  return u32(std::clamp<int32_t>(min, 1, int32_t(kGameMinRoster)));
}

bool SmallRosters() { return MinRoster() < kGameMinRoster; }

// The first time a roster under the game's minimum gets through: a copy of the save.
void SmallRosterBackup() {
  static bool s_done = false;
  if (!s_done)
    s_done = BackupSave("Before a show under 15 Superstars");
}

u32 g_contract_extra = 0;  // the contract count's remaining raise

}  // namespace

extern "C" REX_FUNC(sub_826D0598) {
  const u32 from = u32(ctx.lr);
  __imp__sub_826D0598(ctx, base);
  if (from == kLeaveCountReturn && SmallRosters()) {
    const u32 size = ctx.r3.u32;
    ctx.r3.u64 = size + (kGameMinRoster - MinRoster());
    static u32 s_logged = 0;
    if (size <= kGameMinRoster && ctx.r3.u32 > kGameMinRoster) {
      SmallRosterBackup();
      if (size != s_logged)  // asked every frame while a Superstar is chosen
        BD_INFO("[univers] roster: {} on the show, may leave (minimum {})", size, MinRoster());
      s_logged = size;
    }
  }
}

extern "C" REX_FUNC(sub_826CC0B8) {
  const u32 from = u32(ctx.lr);
  __imp__sub_826CC0B8(ctx, base);
  if (from == kContractCountReturn && ctx.r3.u32 == 0 && g_contract_extra) {
    --g_contract_extra;  // counted as on the show: the count reaches 15 with minimum - 15 fewer
    ctx.r3.u64 = 1;
  }
}

REX_EXTERN(__imp__sub_82599DB0);
extern "C" REX_FUNC(sub_82599DB0) {
  g_contract_extra = SmallRosters() ? kGameMinRoster - MinRoster() : 0;
  __imp__sub_82599DB0(ctx, base);
  g_contract_extra = 0;
}

extern "C" REX_FUNC(sub_82583B78) {
  const PPCContext in = ctx;
  const u32 s = ctx.r3.u32;
  const u32 before[3] = {bd::mem::load<u32>(s + 428), bd::mem::load<u32>(s + 432),
                         bd::mem::load<u32>(s + 436)};
  __imp__sub_82583B78(ctx, base);
  if (!SmallRosters() || before[1] == kShowRosterText || bd::mem::load<u32>(s + 432) != kShowRosterText)
    return;
  u32 count = 0;
  for (u32 id = 0; id < 373; ++id) {
    if (!Call(in, base, __imp__sub_826D3218, id, 1) || Call(in, base, __imp__sub_82A32DB8, id) ||
        Call(in, base, __imp__sub_82A330D8, id) == 1)
      continue;
    const int32_t brand = int32_t(Call(in, base, __imp__sub_826D2B50, id));
    if (brand >= 0 && brand < 13 && bd::mem::load<u32>(s + (u32(brand) + 66) * 4) == 1)
      ++count;
  }
  BD_INFO("[univers] roster: show of {} Superstars (minimum {})", count, MinRoster());
  if (count < MinRoster())
    return;
  // Accepted after all: the refusal undone, then what the game does when there are 15.
  for (u32 k = 0; k < 3; ++k)
    bd::mem::store<u32>(s + 428 + k * 4, before[k]);
  SmallRosterBackup();
  for (u32 k = 0; k < 13; ++k)
    Call(in, base, __imp__sub_826CC0E0, bd::mem::load<u32>(s + 420), k,
         bd::mem::load<u32>(s + 264 + k * 4));
  bd::mem::store<u32>(bd::mem::load<u32>(s + 444), 1);
  Call(in, base, __imp__sub_82550538, s - 28);
}

// The title history: each change of a Universe title's holder, with the week (see StatsFile).
REX_EXTERN(__imp__sub_82A39FC8);  // a title's holder (title, place, 1)
extern "C" REX_FUNC(sub_82A3A060) {
  const PPCContext in = ctx;
  const u32 title = ctx.r3.u32, holder = ctx.r4.u32 & 0xFFFF, place = ctx.r5.u32;
  const bool universe = Enabled() && bd::mem::load<u32>(kInUniverseFlag) == 1 && title < 23 &&
                        place < 2;
  const u32 old_holder = universe ? Call(in, base, __imp__sub_82A39FC8, title, place, 1) & 0xFFFF
                                  : 0;
  __imp__sub_82A3A060(ctx, base);
  if (!universe || old_holder == holder)
    return;
  const u32 u = Call(in, base, __imp__sub_82683A30, 0);
  const u32 day = u ? bd::mem::load<u32>(u + kUniverseDay) : 0;
  const std::filesystem::path file = StatsFile();
  if (!file.empty())
    std::ofstream(file, std::ios::app)
        << "title " << title << ' ' << place << ' ' << holder << ' ' << old_holder << ' ' << day
        << '\n';
  BD_INFO("[univers] stats: title {} place {}: {} -> {} (day {}, from 0x{:08X})", title, place,
          old_holder, holder, day, u32(in.lr));
}

// The results of the Universe's matches (played or simulated): all go through
// sub_826CEF28(view, show, -, match, winner's place, loser's place, 1). The show: 0-9 the weekly
// shows, 10-22 the PPVs in the game's order (10 Royal Rumble, 11 Elimination Chamber, 12
// WrestleMania, ...); its matches: sub_826CCC38(view, show) +4 (104-byte records), +8 how many; a
// record's +0 = its match type (sub_82A2FA18(type) == 1: the Royal Rumble, whose result the game
// keeps with sub_826CCA20), +3 = 2 once played; its places: sub_826D4958(record, out, place), out
// +0 the Superstar (u16), +3 his side. The winners: the winner's side. Written to universe_stats.txt:
// "rumble <id> <won> <day>" for everyone in a Royal Rumble, "mania <id> <main event> <won> <day>" for
// everyone at WrestleMania (main event = the card's last match).
REX_EXTERN(__imp__sub_826CEF28);  // a Universe match's result
REX_EXTERN(__imp__sub_826CCC38);  // Universe view: a show's matches
REX_EXTERN(__imp__sub_826D4958);  // a match record: a place (record, out, place)
REX_EXTERN(__imp__sub_826D4F90);  // a match record: its places
REX_EXTERN(__imp__sub_82A2FA18);  // a match type: 1 = Royal Rumble

constexpr u32 kShowWrestleMania = 12;

// Money in the Bank: the briefcase's holder is U + 51004 (u16, 512 = nobody; the Royal Rumble's
// winner is U + 51000), read by sub_826CC718(view), set by sub_826CCA90(view, id, flag) right after a
// Money in the Bank match (type 76, sub_82A2F998) for its winner, cleared by sub_826CCAB8(view) when
// he cashes in: the cash-in match is added at the end of the card and the briefcase is cleared just
// after its result (seen 2026-10-27), or before the match on another path (sub_826C95E0). So the
// last result's Superstars are kept: at the clear, the holder's result in it is the cash-in's; if he
// was not in it, his next result is. Lines: "mitb <id> <day>" when he wins the briefcase,
// "cashin <id> <won> <day>" for the cash-in.
REX_EXTERN(__imp__sub_826CCA90);  // Money in the Bank: the briefcase's new holder
REX_EXTERN(__imp__sub_826CCAB8);  // Money in the Bank: the briefcase cashed in (holder cleared)

constexpr u32 kBriefcaseHolder = 51004;
u32 g_cashed_holder = 512;               // the holder cleared before his cash-in's result
std::vector<std::pair<u32, bool>> g_last_result;  // the last match's Superstars, won or not

void AppendStats(const std::vector<std::string> &lines) {
  const std::filesystem::path file = StatsFile();
  if (lines.empty() || file.empty())
    return;
  std::ofstream f(file, std::ios::app);
  for (const std::string &l : lines)
    f << l << '\n';
}

u32 UniverseDay(const PPCContext &in, u8 *base) {
  const u32 u = Call(in, base, __imp__sub_82683A30, 0);
  return u ? bd::mem::load<u32>(u + kUniverseDay) : 0;
}

extern "C" REX_FUNC(sub_826CCA90) {
  const u32 id = ctx.r4.u32;
  if (Enabled() && id < 512) {
    BD_INFO("[univers] Money in the Bank: {} holds the briefcase", id);
    AppendStats({fmt::format("mitb {} {}", id, UniverseDay(ctx, base))});
  }
  __imp__sub_826CCA90(ctx, base);
}

extern "C" REX_FUNC(sub_826CCAB8) {
  const u32 u = bd::mem::load<u32>(ctx.r3.u32 + 4);
  const u32 holder = u ? bd::mem::load<u16>(u + kBriefcaseHolder) : 512;
  if (Enabled() && holder < 512) {
    const auto it = std::find_if(g_last_result.begin(), g_last_result.end(),
                                 [&](const auto &r) { return r.first == holder; });
    if (it != g_last_result.end()) {
      AppendStats({fmt::format("cashin {} {} {}", holder, it->second ? 1 : 0,
                               UniverseDay(ctx, base))});
      BD_INFO("[univers] Money in the Bank: {} cashed in, {}", holder, it->second ? "won" : "lost");
    } else {
      g_cashed_holder = holder;
      BD_INFO("[univers] Money in the Bank: {} cashes in (result to come)", holder);
    }
  }
  __imp__sub_826CCAB8(ctx, base);
}

extern "C" REX_FUNC(sub_826CEF28) {
  const PPCContext in = ctx;
  const u32 view = ctx.r3.u32, show = ctx.r4.u32, match = ctx.r6.u32, winner = ctx.r7.u32;
  std::vector<std::string> lines;
  if (Enabled() && bd::mem::load<u32>(kInUniverseFlag) == 1) {
    PPCContext x = in;
    x.r1.u32 = (x.r1.u32 - 0x400) & ~0xFu;
    const u32 out = x.r1.u32 + 0x200;
    const u32 entry = Call(x, base, __imp__sub_826CCC38, view, show);
    const u32 list = entry ? bd::mem::load<u32>(entry + 4) : 0;
    // the card: 8 records, the unused ones without places (entry +8 is not a count)
    u32 count = 0;
    for (u32 k = 0; list && k < 8; ++k)
      if (const u8 n = bd::mem::load<u8>(list + k * 104 + 2); n && n <= 40)
        count = k + 1;
    const u32 record = list && match < count ? list + match * 104 : 0;
    if (record && bd::mem::load<u8>(record + 3) != 2) {
      const bool rumble = Call(x, base, __imp__sub_82A2FA18, bd::mem::load<u8>(record)) == 1;
      const bool mania = show == kShowWrestleMania;
      const u32 places = std::min<u32>(Call(x, base, __imp__sub_826D4F90, record), 40);
      Call(x, base, __imp__sub_826D4958, record, out, winner);
      const u32 winner_id = bd::mem::load<u16>(out), winner_side = bd::mem::load<u8>(out + 3);
      const u32 u = Call(x, base, __imp__sub_82683A30, 0);
      const u32 day = u ? bd::mem::load<u32>(u + kUniverseDay) : 0;
      // this match's Superstars (a cash-in is known when the briefcase is cleared)
      g_last_result.clear();
      for (u32 p = 0; !rumble && p < places; ++p) {
        if (!Call(x, base, __imp__sub_826D4958, record, out, p) || bd::mem::load<u16>(out) >= 512)
          continue;
        const u32 id = bd::mem::load<u16>(out);
        const bool won = winner < 6 && bd::mem::load<u8>(out + 3) == winner_side;
        g_last_result.push_back({id, won});
        if (id == g_cashed_holder) {  // the briefcase was cleared before this match
          lines.push_back(fmt::format("cashin {} {} {}", id, won ? 1 : 0, day));
          BD_INFO("[univers] Money in the Bank: {} cashed in, {}", id, won ? "won" : "lost");
          g_cashed_holder = 512;
        }
      }
      for (u32 p = 0; (rumble || mania) && p < places; ++p) {
        if (!Call(x, base, __imp__sub_826D4958, record, out, p))
          continue;
        const u32 id = bd::mem::load<u16>(out);
        if (id >= 512)
          continue;
        // a place's +3: its team (the winner's place 6 = no winner, a draw)
        const bool won = rumble ? winner < 40 && id == winner_id
                                : winner < 6 && bd::mem::load<u8>(out + 3) == winner_side;
        if (rumble)
          lines.push_back(fmt::format("rumble {} {} {}", id, won ? 1 : 0, day));
        else
          lines.push_back(fmt::format("mania {} {} {} {}", id, match + 1 == count ? 1 : 0,
                                      won ? 1 : 0, day));
      }
      BD_INFO("[univers] result: show {} match {}/{} ({} places), winner {} (side {}){}{}", show,
              match, count, places, winner_id, winner_side, rumble ? ", Royal Rumble" : "",
              mania ? ", WrestleMania" : "");
    }
  }
  __imp__sub_826CEF28(ctx, base);
  AppendStats(lines);
}

// ---- SAVE on an edited match of the card (as in WWE '13) ---------------------------------------
//
// EDIT on a card match (the card screen's state machine, sub_8254A778 state 2 = sub_82548F80)
// opens the match's edit screen (2976 bytes, sub_82575688(flags, cursor)), whose menu is built by
// sub_82574BD8: rows by bit of the flags, 1 PLAY (text 284), 2 MATCH CREATOR (286), 4 SELECT ARENA
// (282), 8 CHANGE PARTICIPANTS (514), 16 SIMULATE (487); per shown row j: screen+276+4j = its bit
// (the result, sub_82571F88 reads the cursor's, screen+72), screen+296+4j = enabled, [screen+2964]
// + 20 + 4j = its text. The Universe never shows both 4 and 8, so a free one becomes SAVE (the
// game's text 238) with our own result, kSaveResult.
//
// When the screen closes with it, the card is written as SIMULATE does before simulating
// (sub_826C63D8(-, match, M): the match select M back into the card's match record), then the
// screen's A becomes a B ([screen+20] = 0): the game's own way back to the card, nothing played.

REX_EXTERN(__imp__sub_82574BD8);  // match edit screen: its menu (screen, flags, cursor's bit)
REX_EXTERN(__imp__sub_82575AA8);  // match edit screen: closing
REX_EXTERN(__imp__sub_82548F80);  // card screens: the match edit state
REX_EXTERN(__imp__sub_826C63D8);  // card: match select -> a card match (-, match, M)
REX_EXTERN(__imp__sub_826BAF28);  // card: the next match to play (card context)

namespace {

constexpr u32 kSaveResult = 32;
constexpr u32 kSelectArena = 4, kChangeParticipants = 8;
constexpr u32 kSelectArenaText = 282, kChangeParticipantsText = 514;
constexpr u32 kMatchSelect = 0x8324DEFC;  // M
constexpr u32 kCardContext = 0x83231548;  // +20 = 1: play the show in order, +32 = match loaded
constexpr u32 kCardOwner = 0x8323154C;

u32 EditResult(u32 screen) {
  return bd::mem::load<u32>(screen + 276 + 4 * bd::mem::load<u32>(screen + 72));
}

}  // namespace

extern "C" REX_FUNC(sub_82574BD8) {
  const u32 screen = ctx.r3.u32, flags = ctx.r4.u32;
  u32 bit = 0;
  if (Enabled() && bd::mem::load<u32>(kInUniverseFlag) == 1)
    bit = !(flags & kChangeParticipants) ? kChangeParticipants
          : !(flags & kSelectArena)      ? kSelectArena
                                         : 0;
  if (!bit) {
    __imp__sub_82574BD8(ctx, base);
    return;
  }
  ctx.r4.u64 = flags | bit;
  g_save_label = bit == kSelectArena ? kSelectArenaText : kChangeParticipantsText;
  __imp__sub_82574BD8(ctx, base);
  g_save_label = 0;
  for (u32 j = 0; j < 5; ++j) {
    if (bd::mem::load<u32>(screen + 276 + 4 * j) != bit)
      continue;
    bd::mem::store<u32>(screen + 276 + 4 * j, kSaveResult);
    bd::mem::store<u32>(screen + 296 + 4 * j, 1);
    BD_INFO("[univers] match edit menu: flags 0x{:X}, SAVE on row {}", flags, j);
    break;
  }
}

// Closing with SAVE: as SELECT ARENA closes (it skips what PLAY needs, sub_82893B30(0)).
extern "C" REX_FUNC(sub_82575AA8) {
  const u32 screen = ctx.r3.u32;
  const u32 at = screen + 276 + 4 * bd::mem::load<u32>(screen + 72);
  const bool save = bd::mem::load<u32>(at) == kSaveResult;
  if (save)
    bd::mem::store<u32>(at, kSelectArena);
  __imp__sub_82575AA8(ctx, base);
  if (save)
    bd::mem::store<u32>(at, kSaveResult);
}

extern "C" REX_FUNC(sub_82548F80) {
  const u32 self = ctx.r3.u32;
  const u32 screen = bd::mem::load<u32>(self);
  if (bd::mem::load<u32>(self + 44) == 2 && screen && bd::mem::load<u32>(screen + 16) == 5 &&
      bd::mem::load<u32>(screen + 20) != 0 && EditResult(screen) == kSaveResult) {
    const u32 m = bd::mem::load<u32>(kMatchSelect);
    const u32 card = bd::mem::load<u32>(kCardContext);
    if (m && card) {
      const u32 match = bd::mem::load<u32>(card + 20) == 1
                            ? Call(ctx, base, __imp__sub_826BAF28, card)
                            : bd::mem::load<u8>(card + 32);
      Call(ctx, base, __imp__sub_826C63D8, bd::mem::load<u32>(kCardOwner), match, m);
      BD_INFO("[univers] match edit: SAVE, card match {} written", match);
    }
    bd::mem::store<u32>(screen + 20, 0);  // back to the card, as with B
  }
  __imp__sub_82548F80(ctx, base);
}

// ---- saving to disk at once (after a reset) ----------------------------------------------------
//
// As TITLE MANAGEMENT does it when it leaves with changes (sub_82564DD0): if a profile can save
// (sub_82A27208(sub_8267CD10()) != 0) and the save exists ([sub_82684BE8()] != 0), make the game's
// saver object (33 720 bytes, sub_828955A0) in the game's heap, raise save block 2's flag +0x576BC,
// and start it with sub_825391F0(saver), or sub_82539130(saver, 0) when block 2's byte +0x57667 is
// 0. The saver works on its own (its own "SAVING..." messages); its word +32 becomes non-zero when
// it is done, then it is destroyed with sub_82895740(saver, 1).

REX_EXTERN(__imp__sub_8267CD10);  // the active profile
REX_EXTERN(__imp__sub_82A27208);  // profile: can save
REX_EXTERN(__imp__sub_82684BE8);  // save block 2
REX_EXTERN(__imp__sub_827D1C68);  // heaps: the game's heap
REX_EXTERN(__imp__sub_827D1F18);  // heaps: this thread's heap
REX_EXTERN(__imp__sub_827D28D0);  // heaps: set this thread's heap
REX_EXTERN(__imp__sub_827D2950);  // heaps: state
REX_EXTERN(__imp__sub_827D1F98);  // heaps: set this thread's flag
REX_EXTERN(__imp__sub_828955A0);  // saver: constructor
REX_EXTERN(__imp__sub_825391F0);  // saver: start
REX_EXTERN(__imp__sub_82539130);  // saver: start (first save)
REX_EXTERN(__imp__sub_82895740);  // saver: destructor (saver, 1 = free)

namespace {

u32 g_saver = 0;

u32 StartSave(const PPCContext &in, u8 *base) {
  const u32 profile = Call(in, base, __imp__sub_8267CD10, 0);
  if (!profile || !Call(in, base, __imp__sub_82A27208, profile)) {
    BD_INFO("[univers] save: no profile that can save");
    return 0;
  }
  const u32 block2 = Call(in, base, __imp__sub_82684BE8, 0);
  if (!block2 || !bd::mem::load<u32>(block2)) {
    BD_INFO("[univers] save: no save yet");
    return 0;
  }
  const u32 heap = Call(in, base, __imp__sub_827D1C68, 0);
  const u32 previous = Call(in, base, __imp__sub_827D1F18, 0);
  Call(in, base, __imp__sub_827D28D0, heap);
  const u32 flag = Call(in, base, __imp__sub_827D2950, 0) == 0 ? 1 : 0;
  Call(in, base, __imp__sub_827D1F18, 0);
  Call(in, base, __imp__sub_827D1F98, 1);
  const u32 saver = Call(in, base, __imp__sub_821A0000, 33720);
  if (saver) {
    Call(in, base, __imp__sub_828955A0, saver);
    bd::mem::store<u32>(block2 + 0x576BC, 1);
    if (bd::mem::load<u8>(block2 + 0x57667))
      Call(in, base, __imp__sub_825391F0, saver);
    else
      Call(in, base, __imp__sub_82539130, saver, 0);
  }
  Call(in, base, __imp__sub_827D1F98, flag);
  Call(in, base, __imp__sub_827D28D0, previous);
  BD_INFO("[univers] save: started (saver 0x{:08X})", saver);
  return saver;
}

bool SaveFinished() { return bd::mem::load<u32>(g_saver + 32) != 0; }

void EndSave(const PPCContext &in, u8 *base) {
  Call(in, base, __imp__sub_82895740, g_saver, 1);
  BD_INFO("[univers] save: done");
  g_saver = 0;
}

}  // namespace

// Every layout menu screen's per-frame update: drives the dialogue while it is open (the screen
// itself keeps updating; its A button waits, see above).
REX_EXTERN(__imp__sub_824D3458);
REX_EXTERN(__imp__sub_8254E7D0);  // message box: idle (none open)?
extern "C" REX_FUNC(sub_824D3458) {
  if (REXCVAR_GET(wwe12_universe_probe) && bd::mem::try_load<u32>(ctx.r3.u32) == kSuperstarPageVtbl)
    DumpPageLayout(ctx, base);
  if (g_pages.page && ctx.r3.u32 == g_pages.screen) {
    if (bd::mem::try_load<u32>(g_pages.screen) == kSuperstarPageVtbl)
      KeepButtons();  // before the frame's buttons are read
    else
      g_pages.page = 0;
  }
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
  // Saving after a reset: the saver works on its own; the Universe menu waits, then says it is done.
  if (g_step == Step::kSaving) {
    if (ctx.r3.u32 == g_screen && (!g_saver || SaveFinished())) {
      if (g_saver)
        EndSave(ctx, base);
      g_step = Step::kDone;
      Call(ctx, base, __imp__sub_8254E680, kResetSavedText);
    }
  } else if (g_step == Step::kTitles) {
    if (ctx.r3.u32 == g_screen)
      bd::mem::store<u32>(g_screen + 224, 0);
    if (g_titles && ctx.r3.u32 != g_titles && TitlesFinished()) {
      CloseTitles(ctx, base);
      bd::mem::store<u32>(g_screen + 224, 1);
      g_step = Step::kIdle;
    }
  } else if (g_step != Step::kIdle && ctx.r3.u32 == g_screen) {
    if (g_step == Step::kMenu || g_step == Step::kAttire || g_step == Step::kChoose) {
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
        BD_INFO("[univers] attire box: answer {}", answer);
        if (g_contender_box)
          ContenderChosen(ctx, base, answer);
        else
          AttireChosen(ctx, base, answer);
        break;
      case Step::kChoose:
        RestoreChoiceElements();
        Chosen(ctx, base, answer);
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
          // Saved to disk at once when the game can save; otherwise at its next save, as before.
          g_saver = StartSave(ctx, base);
          if (g_saver) {
            g_step = Step::kSaving;
          } else {
            g_step = Step::kDone;
            Call(ctx, base, __imp__sub_8254E680, kDoneText);
          }
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
  if (Enabled() &&
      (g_step == Step::kMenu || g_step == Step::kAttire || g_step == Step::kChoose) &&
      g_box_lines == 3 &&
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

// ---- matches per show: the bookers' "number of matches" ------------------------------------------

// A weekly show's match templates (sub_826DC6E8(table, group, i): table +4 groups, group at
// table + 8 + g * 508, its +2 the templates, NULL past them) are fewer than our number: the booker
// (sub_826F15E8, the call returning to 0x826F1690) skips a slot without one, but a card rule then
// reads it (crash seen with RAW at 6, 2026-10-27). So, with our number, the last match takes the
// last template (the main event / title one, as the game's 4th of 4) and the others cycle through
// the other templates. PPVs stay at 6 at most (the game's own number).
thread_local u32 t_weekly_matches = 0;  // our number for the card being booked on this thread

namespace {
u32 MatchCount(u32 slot, u32 game) {
  LoadMatchCounts();
  const u32 n = g_matches[slot];
  return n ? std::min<u32>(n, slot == 3 ? 6 : 8) : game;
}
}  // namespace

REX_EXTERN(__imp__sub_826DC6E8);
extern "C" REX_FUNC(sub_826DC6E8) {
  const u32 table = ctx.r3.u32, group = ctx.r4.u32 & 0xFFFF, i = ctx.r5.u32 & 0xFFFF;
  const u32 n = t_weekly_matches;
  if (n && ctx.lr == 0x826F1690 && table && group < bd::mem::load<u32>(table + 4)) {
    const u32 count = bd::mem::load<u16>(table + 8 + group * 508 + 2);
    if (count >= 2) {
      const u32 to = i + 1 == n ? count - 1 : i >= count - 1 ? (i - (count - 1)) % (count - 1) : i;
      if (to != i)
        ctx.r5.u64 = to;
    }
  }
  __imp__sub_826DC6E8(ctx, base);
}

REX_EXTERN(__imp__sub_826F19E0);
extern "C" REX_FUNC(sub_826F19E0) {  // RAW / SmackDown: 4
  const u32 vtbl = bd::mem::load<u32>(ctx.r3.u32);
  __imp__sub_826F19E0(ctx, base);
  if (Enabled() && (vtbl == kBookerRaw || vtbl == kBookerSmackDown)) {
    const u32 slot = vtbl == kBookerRaw ? 0 : 1;
    ctx.r3.u64 = MatchCount(slot, ctx.r3.u32);
    t_weekly_matches = g_matches[slot] ? ctx.r3.u32 : 0;
  }
}

REX_EXTERN(__imp__sub_82E05C90);
extern "C" REX_FUNC(sub_82E05C90) {  // Superstars: 3 (shared with other classes)
  const u32 vtbl = bd::mem::try_load<u32>(ctx.r3.u32);
  __imp__sub_82E05C90(ctx, base);
  if (Enabled() && vtbl == kBookerSuperstars) {
    ctx.r3.u64 = MatchCount(2, ctx.r3.u32);
    t_weekly_matches = g_matches[2] ? ctx.r3.u32 : 0;
  }
}

REX_EXTERN(__imp__sub_826F1EF8);
extern "C" REX_FUNC(sub_826F1EF8) {  // PPVs: 6
  __imp__sub_826F1EF8(ctx, base);
  if (Enabled())
    ctx.r3.u64 = MatchCount(3, ctx.r3.u32);
}
