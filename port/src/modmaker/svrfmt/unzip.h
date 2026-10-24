// WWE12-PC: Kaiko's svrfmt inflates zlib with his launcher's unzip_inflate; ours is launcher/inflate.h.
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "../../launcher/inflate.h"  // (relative: the game build uses these readers too)

inline int unzip_inflate(const unsigned char *in, size_t in_len, unsigned char *out, size_t out_len) {
  std::vector<uint8_t> v;
  v.reserve(out_len);
  if (!Inflater::Inflate(in, in_len, v) || v.size() != out_len)
    return 0;
  memcpy(out, v.data(), out_len);
  return 1;
}
