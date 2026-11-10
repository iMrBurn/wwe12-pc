// Mod Maker: what WWE '12 can take of a superstar of another game (My game discs): his moves in WWE '12's slots, the
// moves WWE '12 hasn't (WWE '13's), his entrance and victory scene made WWE '12's, their objects, his theme. Used by
// NEW SUPERSTAR (new_star_page.h: an added superstar) and MOVES & ENTRANCES (moves_page.h: put on a superstar the game
// has, 2026-11-03); the pipeline was NEW SUPERSTAR's own (its OwnMoves), moved here as it was.
//  - moves: moveset_port.h (SvR 2011, WWE '13, the 2K games through WWE '13's names; SvR 2007-2010's slots aren't
//    2011's: not read yet), the WWE '13 moves WWE '12 hasn't added (newmoves13.h);
//  - entrance: WWE '13 / 2K14-2K17 entrance13.h, SvR 2007-2011 svr_scenes.h (2011's numbered as the superstar, the older
//    games' through 2011's format, svr_old_entrance.h), served under a number WWE '12 and the installed mods haven't
//    (499 down to 100; it is the theme's music number too);
//  - victory scene: 6000 + his number (entrance13::Victory, svr_scenes::Victory);
//  - objects: those of both WWE '12 hasn't (from WWE '13 / 2K: evtobj.pac; SvR's props are another format: hidden);
//  - theme: theme_source.h, made Wwise Vorbis like the game's themes (else PCM) with the player's vgmstream (theme_convert.h).
#pragma once

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "entrance13.h"
#include "game_disc.h"
#include "game_discs.h"
#include "imgui.h"
#include "moveset_port.h"
#include "model_moves.h"
#include "newmoves13.h"
#include "svr_moves.h"
#include "strings2k.h"
#include "svr_old_entrance.h"
#include "svr_scenes.h"
#include "theme_convert.h"
#include "theme_source.h"
#include "wwise.h"

namespace star_port {

namespace fs = std::filesystem;
using Bytes = std::vector<uint8_t>;

inline const char *GameName(uint32_t title) {
  return title == 0x545408B2   ? "WWE 2K14"
         : title == 0x545408B6 ? "WWE 2K15"
         : title == 0x315A07D2 ? "WWE 2K16"
         : title == 0x545408BA ? "WWE 2K17"
         : title == 0x5451085D ? "SvR 2011"
         : title == 0x54510844 ? "SvR 2010"
         : title == 0x54510826 ? "SvR 2009"
         : title == 0x5451080B ? "SvR 2008"
         : title == 0x545107E0 ? "SvR 2007"
                               : "WWE '13";
}
// SvR 2007-2010 (their moves aren't read yet; entrance and victory through SvR 2011's format)
inline bool SvrOld(uint32_t t) { return t == 0x54510844 || t == 0x54510826 || t == 0x5451080B || t == 0x545107E0; }
inline bool Svr(uint32_t t) { return t == 0x5451085D || SvrOld(t); }
// WWE '13 and the 2K games (entrance13.h: WWE '12's format)
inline bool Scenes(uint32_t t) {
  return t == 0x545108B4 || t == 0x545408B2 || t == 0x545408B6 || t == 0x315A07D2 || t == 0x545408BA;
}

// The superstars of a game of My game discs: its pac/ch/chNNN.pac files, named from its strings (strings2k.h), else
// "chNNN".
struct GameList {
  uint32_t title = 0;
  fs::path path;
  disc::GameDisc disc;                          // the game's disc, open
  std::vector<std::pair<int, std::string>> list;  // (number, name), by name
  std::map<int, std::string> files;               // the game's superstar files by number ("ch065.pac", "ch65_dlc02.pac")
};

// A game's superstar names by number (strings2k::SuperstarNames).
inline std::map<int, std::string> NamesOf(const disc::GameDisc &g) { return strings2k::SuperstarNames(g); }

// The superstars of `title` in My game discs (w12: WWE '12, its names a guess for the 2K games' unnamed numbers), into
// `disc` (opened) and its list (number, name: by name) and files by number.
inline void ListGame(const game_discs::Discs *discs, const disc::GameDisc *w12, uint32_t title, fs::path &path,
                     disc::GameDisc &disc, std::vector<std::pair<int, std::string>> &list, std::map<int, std::string> &files) {
  list.clear();
  files.clear();
  path = discs ? discs->PathOf(title) : fs::path();
  if (path.empty() || !disc.OpenPath(path))
    return;
  std::map<int, std::string> names = NamesOf(disc), guess;
  // The 2K games' numbers without a name in their strings (Names2K): the superstar numbers are the same as in WWE '13
  // and WWE '12, whose names are shown with a "?" (most of the time right).
  if (title != 0x5451085D && title != 0x54510844 && title != 0x545108B4) {  // (the 2K games' unnamed numbers)
    disc::GameDisc w13;
    if (discs && w13.OpenPath(discs->PathOf(0x545108B4)))
      guess = NamesOf(w13);
    if (w12)
      for (const auto &[n, nm] : NamesOf(*w12))
        guess.emplace(n, nm);
  }
  // (chNNN.pac; SvR 2008: chNN.pac; a DLC's: chNN.pac, chNN_dlcMM.pac; the disc's listed first, kept)
  for (const auto &f : disc.List("pac/ch")) {
    size_t k = 2;
    while (k < f.size() && isdigit(static_cast<unsigned char>(f[k])))
      ++k;
    if (k == 2 || k > 5 || _strnicmp(f.c_str(), "ch", 2) != 0 || f.size() < 4 ||
        _stricmp(f.c_str() + f.size() - 4, ".pac") != 0 ||
        (k != f.size() - 4 && _strnicmp(f.c_str() + k, "_dlc", 4) != 0))
      continue;
    const int n = atoi(f.c_str() + 2);
    if (!files.emplace(n, f).second)
      continue;
    const auto it = names.find(n), g = guess.find(n);
    const bool named = it != names.end() && it->second.size() > 1;  // (some records hold "0": no name)
    list.push_back({n, named ? it->second
                       : g != guess.end() && g->second.size() > 1 ? g->second + " ?"
                                                                  : f.substr(0, f.size() - 4)});
  }
  std::sort(list.begin(), list.end(), [](const auto &x, const auto &y) { return x.second < y.second; });
}
inline void ListGame(const game_discs::Discs *discs, const disc::GameDisc *w12, uint32_t title, GameList &out) {
  out.title = title;
  ListGame(discs, w12, title, out.path, out.disc, out.list, out.files);
}

// Whether WWE '12 has a theme numbered `n` (sound\Music.pck's event Play_MUS_<n>_0_0; read once).
class Music12 {
 public:
  bool Has(const disc::GameDisc &w12, int n) {
    if (!read_) {
      read_ = true;
      wwise::Package pk;
      const disc::GameDisc *g = &w12;
      if (pk.Open([g](uint64_t at, uint32_t size, Bytes &o) { return g->ReadPart("sound/Music.pck", at, size, o); }))
        for (const auto &[id, o] : wwise::AllObjects(pk))
          events_.insert(id);
    }
    char e[32];
    snprintf(e, sizeof(e), "Play_MUS_%04d_0_0", n);
    return events_.count(wwise::Fnv(e)) != 0;
  }

 private:
  bool read_ = false;
  std::set<uint32_t> events_;
};

// The entrances the installed mods have (their manifest's entrance_own=), beside the Mod Maker: the superstar mods'
// and the moves mods' (moves_page.h).
inline std::set<int> TakenEntrances() {
  std::set<int> out;
  wchar_t exe[MAX_PATH] = {};
  GetModuleFileNameW(nullptr, exe, MAX_PATH);
  std::error_code ec;
  for (const char *kind : {"Superstars", "Moves"}) {
    for (fs::directory_iterator it(fs::path(exe).parent_path() / "mods" / ".svrmods" / kind, ec), end;
         !ec && it != end; it.increment(ec)) {
      std::ifstream f(it->path() / "manifest.txt");
      std::string line;
      while (std::getline(f, line))
        if (line.rfind("entrance_own=", 0) == 0)
          out.insert(atoi(line.c_str() + 13));
    }
    ec.clear();
  }
  return out;
}

// What WWE '12 can take of one superstar of another game.
struct Parts {
  uint32_t game = 0;       // the game they come from
  bool opened = false;     // its game was read (else nothing below was looked for)
  moveset_port::Result moves;  // his moves in WWE '12's slots (moveset_port.h)
  std::string moves_why;       // ... why none
  std::vector<newmoves13::Move> new_moves;  // his WWE '13 moves WWE '12 hasn't (newmoves13.h)
  std::vector<entrance13::Part> entrance;   // his entrance as WWE '12's, else empty
  int entrance_number = 0;                  // ... the number it is served under (its theme's music number too)
  bool one_take = false;                    // ... a 2K15 "one-take" (entrance13::OneTake)
  std::vector<entrance13::Part> victory;    // his victory scene (6000 + his number), else empty
  std::vector<entrance13::Part> objects;    // the objects of both WWE '12 hasn't (entrance13::Objects)
  int hidden_people = 0;  // people of its entrance other than him (2K17 Goldberg's security: 7) WWE '12 can't show
};

// Superstar `number` of `game` (in My game discs): his moves, entrance, victory scene and their objects. `name`: his
// name in that game's list (ListGame; the 2K16 / 2K17 entrance is found by it); `base12`: a WWE '12 superstar whose
// effects (PCE6) stand in for 2K16 / 2K17's newer ones, -1 for none.
inline void Collect(const disc::GameDisc &w12, const game_discs::Discs &discs, uint32_t game, int number,
                    const std::string &name, int base12, Music12 &music, Parts &out) {
  out = {};
  out.game = game;
  // (SvR 2007-2010: their move slots through Kaiko's maps, moveset_port.h FromOld; their entrance and victory,
  // svr_old_entrance.h)
  const bool svr_old = SvrOld(game);
  disc::GameDisc src;
  const fs::path p = discs.PathOf(game);
  if (p.empty() || !src.OpenPath(p)) {
    out.moves_why = "its game isn't in My game discs";
    return;
  }
  out.opened = true;
  if (svr_old)  // (their slots from Kaiko's maps through SvR 2011's)
    moveset_port::FromOld(w12, src, game, number, out.moves, &out.moves_why);
  else
    moveset_port::FromGame(w12, src, number, out.moves, &out.moves_why);
  // Its moves WWE '12 hasn't, from WWE '13 and the 2K games (their move records and motions are WWE '12's formats:
  // 2026-11-08, after the WWE '13 ones): added (newmoves13.h; the game puts them in the added superstars' pack).
  // SvR 2011's (2026-11-08, the Moveset update): its own animations converted, a WWE '12 model of its kind for the rest
  // (svr_moves.h); a slot kept because WWE '12 has no move of that name is one to add.
  const bool from13 =
      game == 0x545108B4 || game == 0x545408B2 || game == 0x545408B6 || game == 0x315A07D2 || game == 0x545408BA;
  // (SvR 2011, and 2007-2010 the same way, 2026-11-09: 2010 kept 2011's formats, its tables in misc.pac BATS/INIT;
  // 2007-2009 with 144-byte records and their banks without their header - svr_moves.h)
  const bool from11 = game == 0x5451085D || svr_old;
  if (from13 || from11) {
    std::map<int, std::map<int, int>> model_hundreds;  // (WWE '13 / 2K moves on a model: its hundred, model_moves.h)
    std::set<int> want;
    for (auto &r : out.moves.rows) {
      if (from11 && r.to.empty() && r.kept_why == 2 && r.from_id > 0)
        r.new_move = true;
      if (r.new_move)
        want.insert(r.from_id);
    }
    if (from11) {
      out.new_moves = svr_moves::Collect(w12, src, want);
    } else {
      out.new_moves = newmoves13::Collect(src, want);
      // (only the moves whose own motion parameters we could read: without m3.pac rows the game doesn't play an added
      // move - the wrestlers freeze. Every game's m3.pac is read since 2026-11-08 (2K17: 1 436 of its superstars' 1 444
      // added moves have rows; the others - JBL 5, Emma 1 and 2, Emma Lock, Konnor - none): those slots keep the base's)
      out.new_moves.erase(
          std::remove_if(out.new_moves.begin(), out.new_moves.end(), [](const auto &m) { return m.m3.empty(); }),
          out.new_moves.end());
      newmoves13::FitCategories(w12, src, out.new_moves);  // (their category bits WWE '12's: else "None" in the moveset)
      // (and rebuilt on a WWE '12 model of their kind, as SvR's: model_moves.h - those without one keep the base's)
      std::map<int, int> slot_of;
      for (const auto &r : out.moves.rows)
        if (r.new_move && !slot_of.count(r.from_id))
          slot_of[r.from_id] = r.off;
      std::vector<model_moves::Note> notes;
      model_moves::OnModels(w12, out.new_moves, slot_of, &notes);
      model_hundreds = model_moves::ModelHundreds(notes);
    }
    {  // (their events of kinds WWE '12 never uses left out: m3_params::DropUnknownEvents; the 2K games' impact frame,
       // only its low byte in their rows, set from the motion: svr_moves::SetImpact)
      svrfmt::Bytes m3;
      const auto known = w12.Read("pac/m3.pac", m3) ? m3_params::KnownEvents(m3) : std::set<int>{};
      for (auto &m : out.new_moves) {
        m3_params::Rows rows;
        if (m.m3.empty() || !m3_params::Read(m.m3, rows))
          continue;
        m3_params::DropUnknownEvents(rows, known);
        if (from13)  // (and the victim's down value as WWE '12 reads it: 3 of 2K16's moves disagree, k2check)
          svr_moves::SetImpact(m.high, rows), svr_moves::SetDown(m.high, rows);
        m.m3 = m3_params::Write(rows);
      }
    }
    // (renumbered into a kind of move WWE '12 knows: a motion in a hundred it hasn't froze the wrestlers; the moveset's
    // slots take the new numbers)
    std::map<int, std::map<int, int>> prefer;  // move number -> the hundreds WWE '12's rosters use in its slot (counts)
    moveset_port::detail::Game g12;
    moveset_port::detail::Load(w12, g12);
    std::map<int, std::set<int>> in_slot;  // slot -> move numbers WWE '12's rosters put there
    std::map<int, std::map<int, int>> slot_counts;  // slot -> move number -> how many of WWE '12's rosters
    {
      std::map<int, std::map<int, int>> by_slot;  // slot -> hundred -> count
      for (const auto &[id, p] : g12.pro)
        for (size_t o = 0x0A; o + 2 <= p.size() && o < 0x1C4; o += 2)
          if (const int v = moveset_port::detail::U16(p, o); v > 0) {
            by_slot[int(o)][newmoves13::MotionOf(v) / 100]++;
            in_slot[int(o)].insert(v);
            slot_counts[int(o)][v]++;
          }
      for (const auto &r : out.moves.rows)
        if (r.new_move && !prefer.count(r.from_id))
          prefer[r.from_id] = by_slot[r.off];
      for (const auto &[num, hundreds] : model_hundreds)  // (a move on a model: its model's hundred, model_moves.h)
        prefer[num] = hundreds;
    }
    const auto renamed = newmoves13::Renumber(w12, out.new_moves, prefer);
    for (auto &r : out.moves.rows)
      if (r.new_move && renamed.count(r.from_id))
        r.from_id = renamed.at(r.from_id);
    // (and their category bits: those of a WWE '12 move used in the same slot whose bits are closest - bits valid for
    // that slot's Create a Moveset list; the donors' bits alone are WWE '12's for 62 % of the moves only, and a 2K14
    // Cesaro's Neutralizer was in no list, 2026-11-08 - and of the same kind of move: its motion's hundred, as given
    // after the renumbering. The bits tell the game how the move starts (running, from the top rope...): 2K14 Hogan's
    // Running Leg Drop (333xx, the running moves at a downed opponent: People's Elbow, Running Senton) took a diving
    // finisher's bits (560xx: Diving Fist Drop 4) and froze as his finisher, 2026-11-10. Order: a move of the slot of
    // that hundred, else any WWE '12 move of that hundred, else a move of the slot.)
    for (auto &m : out.new_moves) {
      int slot = -1;
      for (const auto &r : out.moves.rows)
        if (r.new_move && r.from_id == m.number) {
          slot = r.off;
          break;
        }
      if (slot < 0 || m.waze.size() < 16)
        continue;
      const int hundred = newmoves13::MotionOf(m.number) / 100;
      const svrfmt::Bytes *best = nullptr;
      int best_d = 1 << 30;
      auto consider = [&](int v, bool same_kind) {
        const auto c = g12.cats.find(v);
        if (c == g12.cats.end() || c->second.size() < 16 || (same_kind && newmoves13::MotionOf(v) / 100 != hundred))
          return;
        bool any = false;  // (a record without bits - an unnamed motion - gives none)
        for (int i = 0; i < 16; ++i)
          any |= c->second[size_t(i)] != 0;
        if (!any)
          return;
        int d = 0;
        for (int i = 0; i < 16; ++i)
          d += __builtin_popcount(unsigned(c->second[size_t(i)] ^ m.waze[size_t(i)]));
        if (d < best_d)
          best_d = d, best = &c->second;
      };
      for (int v : in_slot[slot])
        consider(v, true);
      if (!best)
        for (const auto &[v, c] : g12.cats)
          consider(v, true);
      if (!best)
        for (int v : in_slot[slot])
          consider(v, false);
      if (best)
        std::copy(best->begin(), best->begin() + 16, m.waze.begin());
    }
    std::set<int> got;
    for (const auto &m : out.new_moves)
      got.insert(m.number);
    for (auto &r : out.moves.rows)
      if (r.new_move && got.count(r.from_id)) {
        r.to = r.from;
        r.added = true;
        out.moves.moves.push_back({r.off, r.from_id});
        --out.moves.kept;
        ++out.moves.exact;
      }
    // (a move that couldn't be added - no WWE '12 model of its kind, 2026-11-10: 2K14 Hogan's Running Leg Drop gave him
    // his base Ted DiBiase's finisher -: in its slot the WWE '12 move of that slot closest by name instead, one WWE '12's
    // rosters put there (so it plays there), its last word the same - "Drop", "Kick" -, then the most words alike, then
    // the most used; none so named: the base's, as before)
    for (auto &r : out.moves.rows) {
      if (!r.new_move || r.added || !r.to.empty())
        continue;
      if (const int pick = moveset_port::StandIn(r.from, slot_counts[r.off], g12.names); pick > 0) {
        r.to = g12.names.at(pick);
        r.variant = true;
        out.moves.moves.push_back({r.off, pick});
        --out.moves.kept;
        ++out.moves.variant;
      }
    }
  }
  // Its WWE '13 / 2K14-2K17 entrance (entrance13.h: WWE '12's format; 2K15-2K17's made so), when WWE '12 has none with
  // that number.
  const bool scenes = Scenes(game);
  // (its entrance's number: 2K14 and 2K15 name it in its profile, CHAR / PRO u16 little-endian at +580 / +584, and
  // one superstar's may be another's: 2K15 Sami Zayn 429 enters with 478; elsewhere its own number)
  int entrance = number;
  if (game == 0x315A07D2 || game == 0x545408BA) {  // (2K16 / 2K17: PRO by index, +596)
    const auto dat = theme_source::detail::Records(src, "DAT"), pro = theme_source::detail::Records(src, "PRO");
    for (const auto &[i, d] : dat)
      if (d.size() > 5 && (d[4] | d[5] << 8) == number && pro.count(i) && pro.at(i).size() > 597) {
        const int e = pro.at(i)[596] | pro.at(i)[597] << 8;
        if (e > 0 && e < 1000)
          entrance = e;
      }
  }
  // (2K16 / 2K17 without a profile entrance, or none of that number: the script named as the superstar)
  if ((game == 0x315A07D2 || game == 0x545408BA) && entrance == number) {
    char f[32];
    snprintf(f, sizeof(f), "%05d.pac", number);
    const auto files = src.List("pac/evt");
    const bool has = std::find(files.begin(), files.end(), std::string(f)) != files.end();
    if ((game == 0x545408BA || !has) && !name.empty())  // (its name in its game)
      if (const int e = entrance13::EntranceByName(src, name); e > 0)
        entrance = e;
  }
  if (game == 0x545408B2 || game == 0x545408B6) {
    const auto pro = theme_source::detail::Records(src, "PRO");
    const size_t at = game == 0x545408B2 ? 580 : 584;
    if (const auto it = pro.find(uint32_t(number)); it != pro.end() && it->second.size() > at + 1) {
      const int e = it->second[at] | it->second[at + 1] << 8;
      if (e > 0 && e < 1000)
        entrance = e;
    }
  }
  // (SvR 2007-2011: its own entrance and victory converted, svr_scenes.h: 2011's numbered as the superstar, the older
  // games' through 2011's format, svr_old_entrance.h, the number the profile names; then WWE '12's number it's served
  // as, a free one if the game or a mod has it)
  const bool svr = Svr(game);
  disc::GameDisc disc11;
  const disc::GameDisc *with11 =
      svr_old && !discs.PathOf(0x5451085D).empty() && disc11.OpenPath(discs.PathOf(0x5451085D)) ? &disc11 : nullptr;
  if (svr)
    entrance = svr_old_entrance::EntranceOf(src, game, number);
  if ((scenes && entrance13::Entrance(src, entrance, out.entrance, nullptr)) ||
      (svr && entrance > 0 && svr_scenes::Entrance(src, w12, entrance, out.entrance, nullptr, game, with11))) {
    if (svr_old)
      entrance = out.entrance.front().number;
    out.entrance_number = entrance;
    // (a 2K15 "one-take", the whole entrance in segment 0: said at step 2; WWE '12 plays it without the crowd)
    out.one_take = entrance13::OneTake(out.entrance, entrance);
    // (2K16 / 2K17 effects are a newer PCE6, each emitter 2340 bytes where 2K15's are 2020 and WWE '13's 1780: the
    // game crashed on them, 2K16 Kevin Owens, 2026-11-02. The same entrance's from an older game of the player's,
    // else its base's in WWE '12, else none)
    if (game == 0x315A07D2 || game == 0x545408BA) {
      Bytes older;
      for (uint32_t t : {0x545408B6u, 0x545408B2u, 0x545108B4u}) {
        disc::GameDisc g;
        if (older.empty() && !discs.PathOf(t).empty() && g.OpenPath(discs.PathOf(t)))
          older = entrance13::Pce6Of(g, entrance);
      }
      if (older.empty() && base12 >= 0)
        older = entrance13::Pce6Of(w12, base12);
      for (size_t i = out.entrance.size(); i-- > 0;)
        if (out.entrance[i].kind == "PCE6") {
          if (older.empty())
            out.entrance.erase(out.entrance.begin() + long(i));
          else
            out.entrance[i].data = older;
        }
    }
    // (its effects are asked for twice, pac\prtcls.pac PCE6 and LPE6: a game without LPE6 (2K14...) gives its PCE6 for
    // both, as Kaiko's entrance_port.py; with PCE6 alone no pyro showed, 2026-11-02)
    if (std::none_of(out.entrance.begin(), out.entrance.end(), [](const auto &q) { return q.kind == "LPE6"; }))
      for (size_t i = 0, n = out.entrance.size(); i < n; ++i)
        if (out.entrance[i].kind == "PCE6") {
          entrance13::Part l = out.entrance[i];
          l.kind = "LPE6";
          out.entrance.push_back(std::move(l));
          break;
        }
    // (a number WWE '12 or another installed mod has already: a free one, 499 down to 100; 854 crashed AJ Styles. The
    // number is its theme's too, MusicNumber: one the game has a theme of played that one, none heard: SvR 2010 Jeff
    // Hardy as 111, 2026-11-02)
    const std::set<int> taken = TakenEntrances();
    if (entrance13::Exists12(w12, entrance) || taken.count(entrance) || music.Has(w12, entrance)) {
      int to = 0;
      for (int n = 499; n >= 100 && !to; --n)
        if (!taken.count(n) && !entrance13::Group12(n).empty() && !entrance13::Exists12(w12, n) && !music.Has(w12, n))
          to = n;
      if (to) {
        entrance13::Renumber(out.entrance, entrance, to);
        out.entrance_number = to;
      } else {
        out.entrance.clear();
        out.entrance_number = 0;
      }
    }
  }
  // its victory scene (served under 6000 + its slot)
  if (scenes)
    entrance13::Victory(src, number, out.victory);
  else if (svr)
    svr_scenes::Victory(src, w12, number, out.victory, game, with11);
  // the objects of both that WWE '12 hasn't (Cena's chain, Kane's mask...): from its game's evtobj.pac
  if (scenes || svr) {
    std::vector<entrance13::Part> both = out.entrance;
    both.insert(both.end(), out.victory.begin(), out.victory.end());
    // (SvR 2007-2011's props are another format: none brought, the ones WWE '12 hasn't hidden)
    if (scenes)
      out.objects = entrance13::Objects(src, w12, both);
    // (the props neither WWE '12 nor the mod has: an existing one, hidden)
    entrance13::HideMissing(out.entrance, w12, out.objects);
    entrance13::HideMissing(out.victory, w12, out.objects);
    // (the people of its entrance other than him: objects of type 0x01000000 after object 0 with another model (person
    // NNNAA = superstar NNN, attire AA, of its own game). 2K16 / 2K17 place them with their game, WWE '12 can't: they
    // take WWE '12's people's spots and are not shown (2K17 Goldberg's 7 security men, model 19202, a player's report,
    // 2026-11-04); counted for the page to say so)
    for (const auto &p : out.entrance) {
      if (p.kind != "EVP" || p.number != out.entrance_number || p.data.size() < 0x50)
        continue;
      using entrance13::detail::Be32;
      const uint32_t n = Be32(p.data, 0x40) & 0xFFFF, t = Be32(p.data, 0x48) + 8;
      const uint32_t self = t + 32 <= p.data.size() ? Be32(p.data, t + 4) : 0;
      for (uint32_t i = 1; i < n && t + 32 * size_t(i + 1) <= p.data.size(); ++i) {
        const size_t q = t + 32 * size_t(i);
        const uint32_t model = Be32(p.data, q + 4);
        if (Be32(p.data, q + 8) == 0x01000000 && model && model != self && (game == 0x315A07D2 || game == 0x545408BA))
          ++out.hidden_people;
      }
    }
    // (2K16 / 2K17's object 5009 and superstars waiting in the ring, made for their skeleton and placement: WWE '12's)
    for (auto *parts : {&out.entrance, &out.victory}) {
      entrance13::Announcer12(*parts, w12);
      entrance13::Waiting12(*parts, w12);
      entrance13::WaitingFirst12(*parts);
      entrance13::People12(*parts, w12);
    }
  }
}

// His theme in his game (theme_source.h; `all`: every one found, his profile's first), false if none.
inline bool FindTheme(const GameList &g, int number, theme_source::Theme &out, std::vector<theme_source::Theme> *all) {
  out = {};
  if (all)
    all->clear();
  return theme_source::Find(g.disc, g.title, number, out, all);
}

// The player's vgmstream (the Mod Maker Tools of Kaiko, or a vgmstream folder beside the Mod Maker).
inline fs::path Vgmstream(const game_discs::Discs *discs) {
  return theme_convert::FindVgmstream(discs ? discs->PathOf(0x5451085D) : fs::path());
}

// The theme made for the mod (Wwise Vorbis like the game's themes, else PCM: theme_convert::Finish) (superstar/theme.wem), empty if vgmstream couldn't. `entrance`: the entrance
// going into the mod with it (else null): one whose video plays full screen first (2K15 Bray Wyatt's) gets that video's
// sound before the song (`video`: the video, a 2K .bk2; empty: the song alone), and then its script's EVP in `entries`
// (the mod's files so far) plays the music from frame 100.
inline Bytes ThemeWem(const fs::path &vgmstream, const theme_source::Theme &theme,
                      const std::vector<entrance13::Part> *entrance, int entrance_number, const Bytes &video,
                      std::vector<std::pair<std::string, Bytes>> &entries) {
  const std::wstring ext(theme.ext.begin(), theme.ext.end());
  Bytes pcm;
  // (an entrance whose video plays full screen first, 2K15 Bray Wyatt's: that video's sound before the song, the
  // music from frame 100 as usual and the sound from there; the theme alone has only the song. From frame 0, the
  // video showed neither full screen nor on the titantron, 2026-11-02)
  uint32_t fullscreen = 0;
  if (entrance)
    for (const auto &part : *entrance)
      if (part.kind == "EVP" && part.number == entrance_number)
        fullscreen = entrance13::FullScreenOf(part.data);
  if (fullscreen && !video.empty())
    pcm = theme_convert::ToPcmWithIntro(vgmstream, theme.stream, ext, video, L".bk2", fullscreen / 60.0, 100 / 60.0);
  if (!pcm.empty()) {
    const std::string evp = entrance13::ModPath({entrance_number, "EVP", {}});
    for (auto &[name, data] : entries)
      if (name == evp)
        entrance13::SetMusicFrame(data, 100);
  } else {
    pcm = theme_convert::ToPcm(vgmstream, theme.stream, ext);
  }
  return pcm;
}

// The moves the mod writes (moves=): the added ones (WWE '13's moves WWE '12 hasn't, newmoves13.h) only when the
// player ticked "Added moves (experimental)" (2026-11-05: off by default; their slots then keep the base's move).
inline std::vector<std::pair<int, int>> MovesToWrite(const moveset_port::Result &moves, bool with_added) {
  std::set<int> added;
  for (const auto &r : moves.rows)
    if (r.added)
      added.insert(r.off);
  std::vector<std::pair<int, int>> out;
  for (const auto &m : moves.moves)
    if (with_added || !added.count(m.first))
      out.push_back(m);
  return out;
}

// The "Added moves (experimental)" box, under the moves when there are some (`count`: how many).
inline void DrawAddedMovesChoice(bool fr, bool &use, size_t count) {
  if (!count)
    return;
  char label[200];
  snprintf(label, sizeof(label),
           fr ? "Prises ajoutées (expérimental) : %zu prise(s) que WWE '12 n'a pas, avec leurs animations"
              : "Added moves (experimental): %zu move(s) WWE '12 hasn't, with their animations",
           count);
  ImGui::Checkbox(label, &use);
  ImGui::TextDisabled("%s", fr ? "(décochée : ces cases gardent la prise de la base ; si une prise ajoutée bloque le match, "
                                 "décochez puis refaites le mod)"
                               : "(unticked: those slots keep the base's move; if an added move stops the match, untick "
                                 "and make the mod again)");
}

// His moves, slot by slot (a tree node: the other game's move, WWE '12's it became), under a summary: how many go,
// how many keep the base's move and why (2026-11-05). with_added: the added moves go (DrawAddedMovesChoice).
inline void DrawMovesTable(bool fr, const moveset_port::Result &moves, uint32_t game, const char *kept_en,
                           const char *kept_fr, bool with_added = true) {
  int go = 0, added = 0, why[4] = {};
  for (const auto &r : moves.rows)
    if (!r.to.empty() && (!r.added || with_added))
      ++go;
    else if (r.added)
      ++added;
    else
      ++why[r.kept_why & 3];
  const int all = int(moves.rows.size());
  ImGui::Text(fr ? "%d prises sur %d passent (%d %%)" : "%d moves of %d go (%d %%)", go, all, all ? go * 100 / all : 0);
  std::string kept;
  char b[96];
  auto add = [&](int n, const char *en, const char *f) {
    if (!n)
      return;
    snprintf(b, sizeof(b), "%s%d %s", kept.empty() ? "" : ", ", n, fr ? f : en);
    kept += b;
  };
  add(why[2], "not in WWE '12", "absentes de WWE '12");
  add(why[3], "of another kind in WWE '12", "d'un autre genre dans WWE '12");
  add(why[1] + why[0], "without a name", "sans nom");
  add(added, "added moves left out", "ajoutées laissées de côté");
  if (!kept.empty()) {
    ImGui::SameLine();
    ImGui::TextDisabled(fr ? "; gardent celle de la base : %s" : "; keep the base's: %s", kept.c_str());
  }
  if (moves.slots > all)
    ImGui::TextDisabled(fr ? "(%d autres cases de WWE '12 n'ont pas d'équivalent dans %s : celles de la base)"
                           : "(%d other WWE '12 slots have no match in %s: the base's)",
                        moves.slots - all, GameName(game));
  if (!ImGui::TreeNode(fr ? "Détail des prises" : "The moves, slot by slot"))
    return;
  if (ImGui::BeginTable("own_moves", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                        ImVec2(0, 220))) {
    ImGui::TableSetupColumn(fr ? "Case" : "Slot", ImGuiTableColumnFlags_WidthFixed);
    ImGui::TableSetupColumn(GameName(game));
    ImGui::TableSetupColumn("WWE '12");
    ImGui::TableHeadersRow();
    for (const auto &r : moves.rows) {
      ImGui::TableNextRow();
      ImGui::TableNextColumn();
      ImGui::Text("%03X", r.off);
      ImGui::TableNextColumn();
      ImGui::TextUnformatted(r.from.empty() ? "?" : r.from.c_str());
      ImGui::TableNextColumn();
      if (r.to.empty() || (r.added && !with_added)) {
        const char *w = r.added           ? (fr ? "ajoutée laissée de côté" : "added move left out")
                        : r.kept_why == 2 ? (fr ? "absente de WWE '12" : "not in WWE '12")
                        : r.kept_why == 3 ? (fr ? "d'un autre genre dans WWE '12" : "another kind in WWE '12")
                                          : (fr ? "sans nom" : "no name");
        ImGui::TextDisabled("%s : %s", fr ? kept_fr : kept_en, w);
      } else if (r.added)
        ImGui::Text("%s  %s", r.to.c_str(), fr ? "(ajoutée depuis WWE '13 ; essai)" : "(added from WWE '13; a test)");
      else if (r.variant)
        ImGui::Text("%s  %s", r.to.c_str(), fr ? "(autre version)" : "(another version)");
      else
        ImGui::TextUnformatted(r.to.c_str());
    }
    ImGui::EndTable();
  }
  ImGui::TreePop();
}

}  // namespace star_port
