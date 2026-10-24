// XMemCompress'd files (WWE '13 and later: "0x0FF512ED", 32-128 KB chunks of LZX frames). Kaiko's archive.cpp
// reads them through this (his wwe13.h); the decoder is xcompress.cpp, over libmspack's LZX reader (Mod Maker
// only: the game build doesn't include it, its own files are never xcompressed).
#pragma once
#include <string>

#include "bytes.h"

namespace svrfmt {
inline bool IsXcompress(const Bytes &b) { return b.size() >= 16 && Be32(b.data()) == 0x0FF512EDu; }
#ifdef SVRFMT_NO_XCOMPRESS
inline bool XcompressDecode(const Bytes &, Bytes &, std::string *error = nullptr) {
  if (error)
    *error = "xcompressed file (WWE '13 and later): not read here";
  return false;
}
#else
bool XcompressDecode(const Bytes &in, Bytes &out, std::string *error = nullptr);
#endif
}  // namespace svrfmt
