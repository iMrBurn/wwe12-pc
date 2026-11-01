// WWE '12 PC: names for the preset entrances and move-sets (docs/noms_entrees_movesets.md).
//
// CREATE AN ENTRANCE's preset entrances and the preset move-sets are labelled "SUPERSTAR 01", "DIVA 03", "LEGEND 12"...
// (string ids 8200-8822, the same text in every language but Japanese). The string manager hook (sub_821A4400, in
// wwe12_universe.cpp) asks here: a name known for the id, or for the id asked from a given place in the code (when the
// two lists share an id), is served instead of the game's text.
//
// Names: the table below, then preset_names.txt next to the exe, lines "8215=KURT ANGLE" or "8215@82ABCDEF=CHRIS
// MASTERS" (that caller only), for trying names without a new build.
// Probe (wwe12_preset_names_probe): every id from 8000 to 8899 and 10350 to 10379 shows "#id" and its caller goes to the log once
// ("[preset names] ..."), to see which list reads which ids.
// Off: wwe12_preset_names = false.

#include "wwe12_preset_names.h"

#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <string>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/runtime.h>

#include "core/logging.h"
#include "core/memory_helpers.h"

REXCVAR_DEFINE_BOOL(wwe12_preset_names, true, "WWE12",
                    "Preset entrances and move-sets named after their superstars instead of \"SUPERSTAR 01\".");
REXCVAR_DEFINE_BOOL(wwe12_preset_names_probe, false, "WWE12",
                    "Dev: preset texts (ids 8000-8899, 10350-10379) show \"#id\" and their callers go to the log.");

namespace {

using u32 = uint32_t;

constexpr u32 kFirst = 8000, kLast = 8899;          // the entrances' (probe range)
constexpr u32 kMoveSets = 10350, kMoveSetsEnd = 10379;  // the preset move-sets'

// Built-in names, after The SmackDown Hotel's "WWE '12 Preset Entrances List" and "WWE '12 Preset Movesets List"
// (thesmackdownhotel.com; the superstar each label stands for, facts, its numbering the label's): the entrances read
// ids 8200-8822 (all from one caller, seen with the probe 2026-11-01), the move-sets 10350-10379. Labels the site
// doesn't list (SUPERSTAR 46-55, DIVA 12-20...) keep the game's text; the generic ones get what they show.
const std::map<u32, const char *> &BuiltIn() {
  static const std::map<u32, const char *> names = {
      // SUPERSTAR 01-45
      {8215, "KURT ANGLE"},
      {8216, "KING BOOKER"},
      {8217, "CHRIS MASTERS"},
      {8218, "RVD"},
      {8219, "SABU"},
      {8220, "SANDMAN"},
      {8221, "GENERIC"},
      {8222, "KENNY DYKSTRA"},
      {8223, "BOBBY LASHLEY"},
      {8224, "GREGORY HELMS"},
      {8225, "MATT HARDY"},
      {8226, "BALLS MAHONEY"},
      {8227, "LANCE CADE"},
      {8228, "TREVOR MURDOCH"},
      {8229, "BIG DADDY V"},
      {8230, "SHELTON BENJAMIN"},
      {8231, "PAUL LONDON"},
      {8232, "CHUCK PALUMBO"},
      {8233, "TAZZ"},
      {8234, "TRIPLE H (DX)"},
      {8235, "SHAWN MICHAELS (DX)"},
      {8236, "CHRIS JERICHO (OLD)"},
      {8237, "RANDY ORTON (OLD)"},
      {8238, "SHANE McMAHON"},
      {8239, "TOMMY DREAMER"},
      {8240, "JTG"},
      {8241, "HARDCORE HOLLY"},
      {8242, "SUPER CRAZY"},
      {8243, "DOINK THE CLOWN"},
      {8244, "BOOGEYMAN"},
      {8245, "JIMMY WANG YANG"},
      {8246, "JEFF HARDY"},
      {8247, "JBL"},
      {8248, "UMAGA"},
      {8249, "CARLITO"},
      {8250, "MVP"},
      {8251, "HURRICANE"},
      {8252, "THE GREAT KHALI"},
      {8253, "R-TRUTH (OLD)"},
      {8254, "CHRIS JERICHO"},
      {8316, "BOXER"},
      {8317, "CELL PHONE"},
      {8318, "LADIES' MAN"},
      {8319, "CLUMSY"},
      {8320, "ZOMBIE"},
      // DIVA 01-11
      {8200, "MARIA"},
      {8201, "TORRIE WILSON"},
      {8202, "ASHLEY MASSARO"},
      {8203, "VICTORIA"},
      {8204, "CANDICE MICHELLE"},
      {8205, "MICKIE JAMES"},
      {8206, "MELINA"},
      {8207, "GAIL KIM"},
      {8208, "ARIEL"},
      {8209, "MARIA (OLD)"},
      {8210, "CARTWHEELING DIVA"},
      // LEGEND 01-12
      {8275, "HULK HOGAN"},
      {8276, "BOB ORTON"},
      {8277, "MR. PERFECT"},
      {8278, "DUSTY RHODES"},
      {8279, "JIM NEIDHART"},
      {8280, "JAKE \"THE SNAKE\" ROBERTS"},
      {8281, "TERRY FUNK"},
      {8282, "MICK FOLEY"},
      {8283, "RIC FLAIR"},
      {8284, "LEX LUGER"},
      {8285, "BRITISH BULLDOG"},
      {8286, "ULTIMATE WARRIOR"},
      // TAG 01-22
      {8295, "FACE TAG TEAM"},
      {8296, "RATED-RKO"},
      {8297, "ARMS RAISED & POINTING"},
      {8298, "STRETCHING"},
      {8299, "HAWKINS & RYDER"},
      {8300, "SHANE & VINCE McMAHON"},
      {8301, "CADE & MURDOCH"},
      {8302, "LONDON & KENDRICK"},
      {8303, "UMAGA & ARMANDO ESTRADA"},
      {8304, "JOHN CENA & TONY"},
      {8331, "D-GENERATION X"},
      {8332, "HARDY BOYZ"},
      {8333, "JOHN MORRISON & THE MIZ"},
      {8334, "CRYME TYME"},
      {8335, "EZEKIEL JACKSON & BRIAN KENDRICK"},
      {8336, "JESSE & FESTUS"},
      {8337, "THE COLONS"},
      {8338, "CODY RHODES & TED DiBIASE"},
      {8339, "JERISHOW"},
      {8340, "STRAIGHT EDGE SOCIETY"},
      {8560, "EDGE & CHRISTIAN (STAGE)"},
      {8561, "EDGE & CHRISTIAN (CROWD)"},
      // TAG LEGEND 01-03
      {8818, "THE ROAD WARRIORS"},
      {8819, "THE ROCKERS"},
      {8820, "THE BUSHWHACKERS"},
      // MIXED TAG 01-07
      {8290, "GLAMARELLA"},
      {8291, "JOHNNY NITRO & MELINA"},
      {8292, "EDGE & LITA"},
      {8293, "MIKE KNOX & KELLY KELLY"},
      {8294, "PAUL & KATIE LEA BURCHILL"},
      {8326, "CHARLIE HAAS & MISS JACKIE"},
      {8327, "HEEL MIXED TAG TEAM"},
      // TRIO 01-06
      {8305, "MNM"},
      {8306, "SPIRIT SQUAD"},
      {8307, "nWo"},
      {8308, "F.B.I."},
      {8309, "THE HART DYNASTY"},
      {8311, "LEGACY"},
      // MOVES-SET 01-30 (CREATE A SUPERSTAR > MOVE-SET > PRESET MOVE-SET)
      {10350, "CHRIS MASTERS"},
      {10351, "JTG"},
      {10352, "PRIMO"},
      {10353, "DAVID HART SMITH"},
      {10354, "FINLAY"},
      {10355, "CARLITO"},
      {10356, "UMAGA"},
      {10357, "THE BOOGEYMAN"},
      {10358, "PAUL LONDON"},
      {10359, "BRIAN KENDRICK"},
      {10360, "GREGORY HELMS"},
      {10361, "JAMIE NOBLE"},
      {10362, "JBL"},
      {10363, "MVP"},
      {10364, "SHELTON BENJAMIN"},
      {10365, "CHARLIE HAAS"},
      {10366, "GOLDBERG"},
      {10367, "GAIL KIM"},
      {10368, "MICKIE JAMES"},
      {10369, "KURT ANGLE"},
      {10370, "ROB VAN DAM"},
      {10371, "MR. ANDERSON"},
      {10372, "JEFF HARDY"},
      {10373, "MATT HARDY"},
      {10374, "ELIJAH BURKE"},
      {10375, "SAMOA JOE"},
      {10376, "AJ STYLES"},
      {10377, "RIC FLAIR"},
      {10378, "BRET HART"},
      {10379, "RODDY PIPER"},
  };
  return names;
}

struct Names {
  std::map<u32, std::string> by_id;
  std::map<std::pair<u32, u32>, std::string> by_caller;  // (id, caller)
};

const Names &Loaded() {
  static Names n;
  static std::once_flag once;
  std::call_once(once, [] {
    for (const auto &[id, name] : BuiltIn())
      n.by_id[id] = name;
    std::ifstream f(rex::filesystem::GetExecutableFolder() / "preset_names.txt");
    std::string line;
    int lines = 0;
    while (std::getline(f, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      const size_t eq = line.find('=');
      if (line.empty() || line[0] == '#' || eq == std::string::npos)
        continue;
      const std::string key = line.substr(0, eq), name = line.substr(eq + 1);
      const size_t at = key.find('@');
      const u32 id = u32(std::strtoul(key.c_str(), nullptr, 10));
      if (((id < kFirst || id > kLast) && (id < kMoveSets || id > kMoveSetsEnd)) || name.empty())
        continue;
      if (at == std::string::npos)
        n.by_id[id] = name;
      else
        n.by_caller[{id, u32(std::strtoul(key.c_str() + at + 1, nullptr, 16))}] = name;
      ++lines;
    }
    if (lines)
      BD_INFO("[preset names] {} name(s) from preset_names.txt", lines);
  });
  return n;
}

// One 64-byte guest slot per text, made once.
u32 GuestText(const std::string &text) {
  static std::mutex m;
  static std::map<std::string, u32> slots;
  std::lock_guard<std::mutex> lock(m);
  u32 &at = slots[text];
  if (!at) {
    at = REX_KERNEL_MEMORY()->SystemHeapAlloc(64, 0x10);
    if (at) {
      std::memset(bd::mem::at<char>(at), 0, 64);
      std::memcpy(bd::mem::at<char>(at), text.data(), std::min<size_t>(text.size(), 63));
    }
  }
  return at;
}

}  // namespace

uint32_t Wwe12PresetNameText(uint32_t id, uint32_t caller) {
  if (((id < kFirst || id > kLast) && (id < kMoveSets || id > kMoveSetsEnd)) || !REXCVAR_GET(wwe12_preset_names))
    return 0;
  if (REXCVAR_GET(wwe12_preset_names_probe)) {
    static std::mutex m;
    static std::set<std::pair<u32, u32>> seen;
    {
      std::lock_guard<std::mutex> lock(m);
      if (seen.insert({id, caller}).second)
        BD_INFO("[preset names] id {} asked by {:08X}", id, caller);
    }
    return GuestText("#" + std::to_string(id));
  }
  const Names &n = Loaded();
  if (const auto it = n.by_caller.find({id, caller}); it != n.by_caller.end())
    return GuestText(it->second);
  if (const auto it = n.by_id.find(id); it != n.by_id.end())
    return GuestText(it->second);
  return 0;
}
