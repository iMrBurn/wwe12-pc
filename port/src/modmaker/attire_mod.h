// Attire mods (type=attire): one attire of a superstar the game has, replaced or added, carried alone (docs/mod_maker.md,
// "Tenues"). A superstar's file (pac/ch/chNNN.pac) is an EPK8 of "EMD " entries named "000NNNAK": superstar NNN,
// attire A (0-3), kind K (2: the wrestler; 4: the same attire wearing its entrance gear). The mod gives
// attire/attire.pac, that file with only its entry, and says which (superstar=NNN, attire=<entry name>); the game puts
// it into its own DLC pack at startup (superstar_pack.h, src/native/wwe12_superstar_mods.cpp), so a mod carries none of the game's
// other attires and several attire mods of one superstar go together. Shared by the Mod Maker and the game.
#pragma once

#include <cstdio>
#include <string>

#include "svrfmt/archive.h"

namespace attire_mod {

using svrfmt::Bytes;

// "000NNNAK" for superstar NNN, attire 1-4, kind K.
inline std::string EntryName(int superstar, int attire, int kind = 2) {
  char b[16];
  snprintf(b, sizeof(b), "%06d%d%d", superstar, attire - 1, kind);
  return b;
}

// The first free attire (1-4) of a superstar file for a new one, 0 if it has four.
inline int FreeAttire(const Bytes &pac, int superstar) {
  svrfmt::Archive a;
  if (!svrfmt::ArchiveOpen(pac, a, nullptr))
    return 0;
  for (int n = 1; n <= 4; ++n) {
    bool used = false;
    for (const auto &k : a.root.kids)
      used |= k.name == EntryName(superstar, n);
    if (!used)
      return n;
  }
  return 0;
}

// One attire of a superstar file (entry `entry`) as a file of its own: the same pac with only that entry.
inline bool OneAttire(const Bytes &pac, const std::string &entry, Bytes &out, std::string *why) {
  svrfmt::Archive a;
  if (!svrfmt::ArchiveOpen(pac, a, why))
    return false;
  bool found = false;
  for (size_t i = a.root.kids.size(); i-- > 0;) {
    if (a.root.kids[i].name == entry && !found)
      found = true;
    else
      svrfmt::ArchiveRemove(a.root, i);
  }
  if (!found) {
    if (why)
      *why = "no attire " + entry;
    return false;
  }
  svrfmt::ArchiveTouch(a.root);
  out = svrfmt::ArchiveSave(a);
  return true;
}

// The attire file's entry into a superstar file as `entry`: the one of that name replaced, else added.
inline bool PutAttire(Bytes &pac, const Bytes &attire_file, const std::string &entry, std::string *why) {
  svrfmt::Archive a, t;
  if (!svrfmt::ArchiveOpen(pac, a, why) || !svrfmt::ArchiveOpen(attire_file, t, why))
    return false;
  if (t.root.kids.empty() || t.root.kids[0].group != "EMD ") {
    if (why)
      *why = "the attire file has no attire";
    return false;
  }
  const svrfmt::ArcNode &src = t.root.kids[0];
  bool replaced = false;
  for (auto &k : a.root.kids)
    if (k.name == entry && k.group == "EMD ") {
      const std::string name = k.name, group = k.group;
      const int gi = k.gi;
      k = src;  // (its data as stored, untouched)
      k.name = name, k.group = group, k.gi = gi;
      replaced = true;
    }
  if (!replaced && !svrfmt::ArchiveAdd(a.root, "EMD ", entry, 0, svrfmt::ArchiveData(src), why))
    return false;
  svrfmt::ArchiveTouch(a.root);
  pac = svrfmt::ArchiveSave(a);
  return true;
}

}  // namespace attire_mod
