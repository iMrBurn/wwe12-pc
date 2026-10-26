// Mod Maker: the hair's collision capsules (attire entry 61: "bone x0 y0 z0 r0 x1 y1 z1 r1 k" lines on WWE '12's
// Biped bones) for a ported superstar whose base has none: a long-haired superstar's, read from the player's game
// files (none written in the Mod Maker). Read once.
#pragma once

#include <string>

#include "game_disc.h"
#include "svrfmt/archive.h"

namespace hair_donor {

inline const svrfmt::Bytes &Collision(const disc::GameDisc &disc) {
  static svrfmt::Bytes found;
  static bool tried = false;
  if (tried)
    return found;
  tried = true;
  // (superstars with long hair on the disc: the first that has the entry)
  for (int id : {117, 102, 103, 175, 179, 176, 110, 164}) {
    svrfmt::Bytes pac;
    svrfmt::Archive a;
    if (!disc.Read("pac/ch/ch" + std::to_string(id) + ".pac", pac) || !svrfmt::ArchiveOpen(pac, a, nullptr) ||
        a.root.kids.empty() || !svrfmt::ArchiveExpand(a.root.kids[0]))
      continue;
    for (const auto &k : a.root.kids[0].kids)
      if (k.id == 61)
        found = svrfmt::ArchiveData(k);
    if (!found.empty())
      break;
  }
  return found;
}

}  // namespace hair_donor
