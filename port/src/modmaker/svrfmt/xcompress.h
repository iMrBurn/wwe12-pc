// WWE12-PC: Kaiko's archive.cpp reads xcompressed files (LZX, WWE '13 to 2K17) with wwe13.h. WWE '12's own
// files are never xcompressed, so the decoder isn't taken (yet): such a file is refused with a clear message.
#pragma once
#include <string>

#include "bytes.h"

namespace svrfmt {
inline bool IsXcompress(const Bytes &b) { return b.size() >= 16 && Be32(b.data()) == 0x0FF512EDu; }
inline bool XcompressDecode(const Bytes &, Bytes &, std::string *error = nullptr) {
  if (error)
    *error = "xcompressed file (WWE '13 and later): not supported here yet";
  return false;
}
}  // namespace svrfmt
