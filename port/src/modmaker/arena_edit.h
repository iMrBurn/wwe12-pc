// An arena file (pac/bg/bgNN.pac) opened for its textures: EPAC -> "STG " entry -> PACH whose texture packages
// (ids 400-402, 32010...: {count, ..., records {name[16], "dds", size, offset, 0}}, Kaiko's BundleRead / BundleWrite)
// hold the arena's pictures as PC DDS. Pictures are replaced in place; the file is rebuilt with Kaiko's archive
// writer, the changed packages packed again keeping Yuke's own blocks (bpe_like.h).
// The game loads an arena into fixed memory sized for the arena it replaces: the file must never be bigger than
// the shipped bgNN.pac of its slot (docs/limit_breaking.md, "Arènes": 10 KB over already crashed). Fit() makes room
// by dropping the top level of the biggest textures nobody changed (their smaller levels are kept as they are:
// nothing is encoded again, the picture is only half as sharp).
#pragma once

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "bpe_like.h"
#include "svrfmt/archive.h"
#include "svrfmt/texture.h"

namespace arena_edit {

using svrfmt::Bytes;

// A DDS without its top mip level (half the size, the same smaller levels); false if it has only one level.
inline bool DropTopMip(Bytes &dds) {
  svrfmt::DdsInfo info;
  if (!svrfmt::DdsInfoOf(dds, info) || info.mips < 2 || dds.size() < 128)
    return false;
  const size_t bw = std::max(1, (info.w + 3) / 4), bh = std::max(1, (info.h + 3) / 4);
  size_t top = 0;
  switch (info.format) {
  case svrfmt::DxtFormat::kDxt1: top = bw * bh * 8; break;
  case svrfmt::DxtFormat::kDxt3:
  case svrfmt::DxtFormat::kDxt5: top = bw * bh * 16; break;
  case svrfmt::DxtFormat::kArgb: top = size_t(info.w) * info.h * 4; break;
  default: return false;
  }
  if (128 + top >= dds.size())
    return false;
  Bytes out(dds.begin(), dds.begin() + 128);
  const uint32_t w = std::max(1, info.w / 2), h = std::max(1, info.h / 2);
  svrfmt::PutLe32(&out[12], h);
  svrfmt::PutLe32(&out[16], w);
  const uint32_t flags = svrfmt::Le32(&out[8]);
  const size_t nbw = std::max<uint32_t>(1, (w + 3) / 4), nbh = std::max<uint32_t>(1, (h + 3) / 4);
  const size_t next = info.format == svrfmt::DxtFormat::kArgb      ? size_t(w) * 4
                      : info.format == svrfmt::DxtFormat::kDxt1    ? nbw * nbh * 8
                                                                   : nbw * nbh * 16;
  svrfmt::PutLe32(&out[20], uint32_t(next));  // pitch (ARGB) or linear size (DXT), as the flags say
  (void)flags;
  svrfmt::PutLe32(&out[28], uint32_t(info.mips - 1));
  out.insert(out.end(), dds.begin() + ptrdiff_t(128 + top), dds.end());
  dds = std::move(out);
  return true;
}

struct Texture {
  size_t package = 0;  // index in Arena::packages
  size_t index = 0;    // in its package
  bool replaced = false, reduced = false;
};

struct Package {
  svrfmt::ArcNode *node = nullptr;
  std::vector<svrfmt::BundleTexture> textures, original;
  Bytes shipped;  // the package as stored in the shipped file (the blocks EncodeLike keeps)
  bool changed = false;
};

class Arena {
 public:
  bool Load(Bytes file, std::string *error) {
    packages_.clear();
    textures_.clear();
    archive_ = svrfmt::Archive();
    if (!svrfmt::ArchiveOpen(file, archive_, error))
      return false;
    if (archive_.root.kind != svrfmt::ArcKind::Epac || archive_.root.kids.empty() ||
        archive_.root.kids[0].group != "STG ") {
      if (error)
        *error = "not an arena (no STG section)";
      return false;
    }
    svrfmt::ArcNode &stg = archive_.root.kids[0];
    if (!svrfmt::ArchiveExpand(stg) || stg.kind != svrfmt::ArcKind::Pach) {
      if (error)
        *error = "the arena's list can't be read";
      return false;
    }
    for (auto &k : stg.kids) {
      if (svrfmt::ArchiveKindOf(k) != "textures")
        continue;
      Package p;
      p.node = &k;
      if (!svrfmt::BundleRead(svrfmt::ArchiveData(k), p.textures))
        continue;
      p.original = p.textures;
      p.shipped = k.stored;
      packages_.push_back(std::move(p));
    }
    for (size_t i = 0; i < packages_.size(); ++i)
      for (size_t j = 0; j < packages_[i].textures.size(); ++j)
        textures_.push_back({i, j});
    return true;
  }

  // Sets the stage number (the STG entry's name, "%04d"): an arena works only under its own slot's number.
  bool SetStage(int number) {
    if (archive_.root.kids.empty() || Stage() == number)
      return true;
    char name[8];
    snprintf(name, sizeof(name), "%04d", number);
    std::string why;
    return svrfmt::ArchiveRename(archive_.root, 0, name, &why);
  }

  // The arena's stage number ("0015" -> 15), the STG entry's name.
  int Stage() const { return archive_.root.kids.empty() ? -1 : atoi(archive_.root.kids[0].name.c_str()); }

  std::vector<Texture> &Textures() { return textures_; }
  const svrfmt::BundleTexture &Get(const Texture &t) const { return packages_[t.package].textures[t.index]; }
  const svrfmt::BundleTexture &Original(const Texture &t) const { return packages_[t.package].original[t.index]; }

  void Replace(Texture &t, Bytes dds) {
    packages_[t.package].textures[t.index].data = std::move(dds);
    packages_[t.package].changed = true;
    t.replaced = true;
  }
  void Restore(Texture &t) {
    packages_[t.package].textures[t.index] = packages_[t.package].original[t.index];
    t.replaced = t.reduced = false;
    packages_[t.package].changed = true;  // (Build finds it unchanged again if it is)
  }

  // The rebuilt file (the original bytes if nothing changed).
  Bytes Build() {
    for (auto &p : packages_) {
      if (!p.changed)
        continue;
      const Bytes raw = svrfmt::BundleWrite(p.textures);
      p.node->stored = bpe_like::EncodeLike(raw, p.shipped);
      p.node->dirty = true;
    }
    return svrfmt::ArchiveSave(archive_);
  }

  // Makes the file fit `budget` bytes: the biggest textures not replaced lose their top level, one at a time.
  // The textures reduced (names); the file built last is in `out`.
  std::vector<std::string> Fit(size_t budget, Bytes &out) {
    std::vector<std::string> reduced;
    out = Build();
    std::set<size_t> tried;
    while (out.size() > budget) {
      size_t best = SIZE_MAX, best_size = 0;
      for (size_t i = 0; i < textures_.size(); ++i) {
        const Texture &t = textures_[i];
        if (t.replaced || tried.count(i) || Risky(Get(t).name))
          continue;
        const size_t s = Get(t).data.size();
        if (s > best_size) {
          best = i;
          best_size = s;
        }
      }
      if (best == SIZE_MAX)
        break;  // nothing left to make smaller
      tried.insert(best);
      Texture &t = textures_[best];
      Bytes dds = Get(t).data;
      if (!DropTopMip(dds))
        continue;
      packages_[t.package].textures[t.index].data = std::move(dds);
      packages_[t.package].changed = true;
      t.reduced = true;
      reduced.push_back(Get(t).name);
      out = Build();
    }
    return reduced;
  }

  // Screens the game may draw videos into, and light maps: never made smaller (their size may be expected).
  static bool Risky(const std::string &name) {
    std::string n = name;
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    for (const char *w : {"titan", "tron", "mov", "light", "lgt"})
      if (n.find(w) != std::string::npos)
        return true;
    return false;
  }

  // Undoes Fit's reductions (before replacing more pictures, so the next Fit starts from the shipped ones).
  void Unreduce() {
    for (auto &t : textures_)
      if (t.reduced && !t.replaced) {
        packages_[t.package].textures[t.index] = packages_[t.package].original[t.index];
        t.reduced = false;
      }
  }

 private:
  svrfmt::Archive archive_;
  std::vector<Package> packages_;
  std::vector<Texture> textures_;
};

}  // namespace arena_edit
