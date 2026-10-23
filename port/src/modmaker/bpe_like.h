// BPE re-compression that keeps Yuke's own blocks (C++ of tools/yukes_bpe.py compress_like): an entry's new data
// is cut where the old entry's sub-blocks were; a sub-block whose decoded bytes didn't change is copied as Yuke's
// coded it (~2.5 % smaller than Kaiko's BpeEncode, ~16 % than ours), only the changed parts are packed again. An
// arena must never outgrow its shipped file (the game loads it into a fixed buffer), so a texture replaced at the
// same size and format costs a few hundred bytes instead of the whole package's growth.
#pragma once

#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

#include "svrfmt/pac.h"

namespace bpe_like {

using svrfmt::Bytes;

// The sub-blocks of a "BPE " entry: (coded bytes: pair table + size + data, decoded length). Empty if unreadable.
inline std::vector<std::pair<Bytes, size_t>> Split(const Bytes &entry) {
  std::vector<std::pair<Bytes, size_t>> out;
  if (!svrfmt::IsBpe(entry))
    return out;
  const uint32_t packed = svrfmt::Le32(&entry[8]), unpacked = svrfmt::Le32(&entry[12]);
  if (16 + size_t(packed) > entry.size())
    return out;
  const uint8_t *src = entry.data() + 16;
  size_t pos = 0, total = 0;
  while (pos < packed && total < unpacked) {
    const size_t start = pos;
    uint8_t left[256], right[256] = {};
    for (int i = 0; i < 256; ++i)
      left[i] = uint8_t(i);
    for (int c = 0; c < 256;) {
      if (pos >= packed)
        return {};
      int count = src[pos++];
      if (count > 127) {
        c += count - 127;
        count = 0;
      }
      if (c == 256)
        break;
      for (int k = 0; k <= count && c < 256; ++k, ++c) {
        if (pos >= packed)
          return {};
        left[c] = src[pos++];
        if (left[c] != c) {
          if (pos >= packed)
            return {};
          right[c] = src[pos++];
        }
      }
    }
    if (pos + 2 > packed)
      return {};
    const size_t size = src[pos] | (src[pos + 1] << 8);
    pos += 2;
    if (pos + size > packed)
      return {};
    size_t decoded = 0;  // expand, counting only
    for (size_t k = 0; k < size; ++k) {
      uint8_t stack[256];
      int sp = 0;
      stack[sp++] = src[pos + k];
      while (sp) {
        const uint8_t b = stack[--sp];
        if (left[b] == b) {
          ++decoded;
        } else {
          if (sp > 254)
            return {};
          stack[sp++] = right[b];
          stack[sp++] = left[b];
        }
      }
    }
    pos += size;
    out.push_back({Bytes(src + start, src + pos), decoded});
    total += decoded;
  }
  return total == unpacked ? out : std::vector<std::pair<Bytes, size_t>>{};
}

// `raw` packed as a "BPE " entry, reusing `old`'s sub-blocks wherever their bytes are found again: at the same
// place (before a change) or moved by the size difference (after it: a texture made bigger or smaller shifts
// everything behind it). The bytes in between are packed again.
inline Bytes EncodeLike(const Bytes &raw, const Bytes &old) {
  Bytes body;
  auto pack = [&](size_t at, size_t n) {
    if (!n)
      return;
    const Bytes e = svrfmt::BpeEncode(Bytes(raw.begin() + ptrdiff_t(at), raw.begin() + ptrdiff_t(at + n)));
    body.insert(body.end(), e.begin() + 16, e.end());
  };
  struct Block {
    Bytes coded, decoded;
    size_t at;  // in the old data
  };
  std::vector<Block> blocks;
  size_t old_size = 0;
  for (auto &[coded, n] : Split(old)) {
    Bytes one = {'B', 'P', 'E', ' ', 0, 1, 0, 0}, was;
    svrfmt::AppLe32(one, uint32_t(coded.size()));
    svrfmt::AppLe32(one, uint32_t(n));
    svrfmt::App(one, coded);
    if (!svrfmt::BpeDecode(one, was) || was.size() != n)
      return svrfmt::BpeEncode(raw);
    blocks.push_back({std::move(coded), std::move(was), old_size});
    old_size += n;
  }
  const ptrdiff_t delta = ptrdiff_t(raw.size()) - ptrdiff_t(old_size);
  auto found_at = [&](const Block &b, ptrdiff_t at) {
    return at >= 0 && size_t(at) + b.decoded.size() <= raw.size() &&
           std::equal(b.decoded.begin(), b.decoded.end(), raw.begin() + at);
  };
  size_t pos = 0;
  for (const Block &b : blocks) {
    // The first place this block can go from the cursor on: unchanged, or moved by delta.
    ptrdiff_t at = -1;
    for (ptrdiff_t cand : {ptrdiff_t(b.at), ptrdiff_t(b.at) + delta})
      if (cand >= ptrdiff_t(pos) && found_at(b, cand) && (at < 0 || cand < at))
        at = cand;
    if (at < 0)
      continue;
    pack(pos, size_t(at) - pos);
    svrfmt::App(body, b.coded);
    pos = size_t(at) + b.decoded.size();
  }
  pack(pos, raw.size() - pos);
  Bytes out = {'B', 'P', 'E', ' ', 0, 1, 0, 0};
  svrfmt::AppLe32(out, uint32_t(body.size()));
  svrfmt::AppLe32(out, uint32_t(raw.size()));
  svrfmt::App(out, body);
  return out;
}

}  // namespace bpe_like
