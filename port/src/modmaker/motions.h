// Mod Maker: the game's motion banks (pac\m.pac). Group ZMVM = the move-set's banks by kind (STAT stances, BASE
// taunts and poses, FINI finishers...): each entry is a PACH tree whose leaves are motion records (yanm.h), the leaf's
// id = the motion's number. Read one bank at a time (the file is 270 MB).
#pragma once

#include <string>
#include <vector>

#include "game_disc.h"
#include "svrfmt/pac.h"
#include "yanm.h"

namespace motions {

using svrfmt::Bytes;

struct Bank {
  const char *entry, *en, *fr;
};
inline const std::vector<Bank> &Banks() {
  static const std::vector<Bank> b = {
      {"STAT", "Stances and idles", "Postures et attentes"},
      {"BASE", "Taunts and poses", "Provocations et poses"},
      {"FINI", "Finishers", "Prises de finition"},
      {"SPE1", "Signatures and specials (1)", "Prises spéciales (1)"},
      {"SPE2", "Signatures and specials (2)", "Prises spéciales (2)"},
      {"STN1", "Standing grapples (1)", "Prises debout (1)"},
      {"STN2", "Standing grapples (2)", "Prises debout (2)"},
      {"GRND", "Ground moves", "Prises au sol"},
      {"CORN", "Corner moves", "Prises dans le coin"},
      {"CETB", "Turnbuckle moves", "Prises sur le poteau"},
      {"ROPE", "Rope moves", "Prises aux cordes"},
      {"APRO", "Apron moves (1)", "Prises sur le tablier (1)"},
      {"APR2", "Apron moves (2)", "Prises sur le tablier (2)"},
      {"DIVI", "Dives", "Plongeons"},
      {"RUN ", "Running moves", "Prises en courant"},
      {"TEAM", "Tag team moves", "Prises en équipe"},
      {"WEAP", "Weapon moves", "Prises avec objet"},
      {"ANAS", "ANAS motions (1)", "Mouvements ANAS (1)"},
      {"ANA2", "ANAS motions (2)", "Mouvements ANAS (2)"},
      {"ABIL", "Abilities", "Capacités"},
      {"RUMB", "Royal Rumble", "Royal Rumble"},
      {"LADR", "Ladder", "Échelle"},
      {"CELL", "Hell in a Cell", "Hell in a Cell"},
  };
  return b;
}

struct Motion {
  int number = 0;
  Bytes record;
};

// Every record of m.pac ZMVM/<entry>, in the file's order.
inline bool ReadBank(const disc::GameDisc &disc, const std::string &entry, std::vector<Motion> &out, std::string *why) {
  out.clear();
  const std::string file = "pac/m.pac";
  Bytes head;
  const uint64_t size = disc.SizeOf(file);
  if (!size || !disc.ReadPart(file, 0, 0x4000, head)) {
    if (why)
      *why = "pac/m.pac not found";
    return false;
  }
  std::vector<svrfmt::EpacRow> rows;
  if (!svrfmt::EpacTable(head, size, rows)) {
    if (why)
      *why = "pac/m.pac: no table";
    return false;
  }
  for (const auto &r : rows) {
    if (r.group != "ZMVM" || r.name != entry)
      continue;
    Bytes raw;
    if (!disc.ReadPart(file, r.offset, r.size, raw)) {
      if (why)
        *why = "pac/m.pac: read error";
      return false;
    }
    struct W {
      static void Walk(const Bytes &d, int id, std::vector<Motion> &o, int depth) {
        if (svrfmt::IsPach(d)) {
          std::vector<svrfmt::PachEntry> kids;
          if (depth < 6 && svrfmt::PachRead(d, kids))
            for (auto &k : kids)
              Walk(svrfmt::Unpack(k.data), int(k.id), o, depth + 1);
        } else if (yanm::IsRecord(d)) {
          o.push_back({id, d});
        }
      }
    };
    W::Walk(svrfmt::Unpack(raw), 0, out, 0);
    if (out.empty() && why)
      *why = "no motions in that bank";
    return !out.empty();
  }
  if (why)
    *why = "pac/m.pac has no ZMVM/" + entry;
  return false;
}

}  // namespace motions
