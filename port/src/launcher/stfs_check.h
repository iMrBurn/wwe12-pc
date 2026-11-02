// Looks at the first bytes of a file and says whether it is an Xbox 360 STFS package ("CON ", "LIVE" or
// "PIRS") of a given content type for WWE '12 (title 54510890), and if not, why. Used by the SAVES page (type 1,
// saved game) and the DLC button (type 2, marketplace content). Nothing is read past the header: the game
// itself unpacks the files (wwe12_saves.cpp, wwe12_dlc.cpp).
#pragma once

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace stfs {

constexpr uint32_t kWweTitle = 0x54510890;
constexpr uint32_t kTypeSavedGame = 1;
constexpr uint32_t kTypeMarketplace = 2;

enum class Verdict {
  Ok,
  Unreadable,  // could not be opened
  TooSmall,    // shorter than an STFS header
  Archive,     // zip / rar / 7z: has to be unpacked first
  DiskImage,   // a whole console storage image (FATX), not one package
  NotStfs,     // anything else
  WrongType,   // an STFS package, but not the wanted content type
  WrongGame    // an STFS package of the right type, for another game
};

struct Result {
  Verdict verdict = Verdict::NotStfs;
  uint32_t type = 0;   // content type found (WrongType / WrongGame / Ok)
  uint32_t title = 0;  // title id found
};

inline uint32_t ReadU32BE(const unsigned char *p) {
  return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
}

inline Result Check(const std::filesystem::path &file, uint32_t wanted_type) {
  Result r;
  std::ifstream f(file, std::ios::binary);
  if (!f) {
    r.verdict = Verdict::Unreadable;
    return r;
  }
  unsigned char h[0x364] = {};
  f.read(reinterpret_cast<char *>(h), sizeof(h));
  const std::streamsize got = f.gcount();
  if (got >= 4) {
    if (!memcmp(h, "PK\x03\x04", 4) || !memcmp(h, "Rar!", 4) || !memcmp(h, "7z\xBC\xAF", 4)) {
      r.verdict = Verdict::Archive;
      return r;
    }
    if (!memcmp(h, "XTAF", 4)) {
      r.verdict = Verdict::DiskImage;
      return r;
    }
  }
  if (got < static_cast<std::streamsize>(sizeof(h))) {
    // Short files that do not even start like a package are "not a package" rather than "too small".
    const bool stfs_magic = got >= 4 && (!memcmp(h, "CON ", 4) || !memcmp(h, "LIVE", 4) || !memcmp(h, "PIRS", 4));
    r.verdict = stfs_magic ? Verdict::TooSmall : Verdict::NotStfs;
    return r;
  }
  if (memcmp(h, "CON ", 4) && memcmp(h, "LIVE", 4) && memcmp(h, "PIRS", 4)) {
    r.verdict = Verdict::NotStfs;
    return r;
  }
  r.type = ReadU32BE(h + 0x344);
  r.title = ReadU32BE(h + 0x360);
  if (r.title != kWweTitle)
    r.verdict = Verdict::WrongGame;
  else if (r.type != wanted_type)
    r.verdict = Verdict::WrongType;
  else
    r.verdict = Verdict::Ok;
  return r;
}

}  // namespace stfs
