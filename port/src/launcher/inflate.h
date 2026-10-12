// Small raw-DEFLATE decoder (RFC 1951) for the updater's zip files: stored, fixed and dynamic
// Huffman blocks. Written for this project; slow-ish but plenty for a 35 MB download.
#pragma once

#include <cstdint>
#include <vector>

class Inflater {
 public:
  // Decompresses src[0..size) into out (appended). false on malformed data.
  static bool Inflate(const uint8_t *src, size_t size, std::vector<uint8_t> &out) {
    Inflater z(src, size, out);
    return z.Run();
  }

 private:
  struct Huffman {
    uint16_t counts[16] = {};
    uint16_t symbols[320] = {};
  };

  Inflater(const uint8_t *s, size_t n, std::vector<uint8_t> &o) : src_(s), size_(n), out_(o) {}

  int Bits(int need) {
    uint32_t val = bitbuf_;
    while (bitcnt_ < need) {
      if (pos_ >= size_) {
        error_ = true;
        return 0;
      }
      val |= uint32_t(src_[pos_++]) << bitcnt_;
      bitcnt_ += 8;
    }
    bitbuf_ = val >> need;
    bitcnt_ -= need;
    return int(val & ((1u << need) - 1u));
  }

  static void Build(Huffman &h, const uint8_t *lengths, int n) {
    for (int i = 0; i < 16; ++i)
      h.counts[i] = 0;
    for (int i = 0; i < n; ++i)
      h.counts[lengths[i]]++;
    h.counts[0] = 0;
    uint16_t offs[16];
    offs[1] = 0;
    for (int i = 1; i < 15; ++i)
      offs[i + 1] = uint16_t(offs[i] + h.counts[i]);
    for (int i = 0; i < n; ++i)
      if (lengths[i])
        h.symbols[offs[lengths[i]]++] = uint16_t(i);
  }

  int Decode(const Huffman &h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; ++len) {
      code |= Bits(1);
      if (error_)
        return -1;
      const int count = h.counts[len];
      if (code - count < first)
        return h.symbols[index + (code - first)];
      index += count;
      first += count;
      first <<= 1;
      code <<= 1;
    }
    error_ = true;
    return -1;
  }

  bool Codes(const Huffman &lit, const Huffman &dist) {
    static const uint16_t kLenBase[] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                        31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const uint8_t kLenExtra[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                        2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const uint16_t kDistBase[] = {1,   2,   3,   4,   5,   7,    9,    13,   17,   25,
                                         33,  49,  65,  97,  129, 193,  257,  385,  513,  769,
                                         1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const uint8_t kDistExtra[] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                         6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    for (;;) {
      int sym = Decode(lit);
      if (error_ || sym < 0)
        return false;
      if (sym < 256) {
        out_.push_back(uint8_t(sym));
      } else if (sym == 256) {
        return true;
      } else {
        sym -= 257;
        if (sym >= 29)
          return false;
        const int len = kLenBase[sym] + Bits(kLenExtra[sym]);
        const int ds = Decode(dist);
        if (error_ || ds < 0 || ds >= 30)
          return false;
        const size_t d = size_t(kDistBase[ds]) + size_t(Bits(kDistExtra[ds]));
        if (error_ || d > out_.size())
          return false;
        const size_t from = out_.size() - d;
        for (int i = 0; i < len; ++i)
          out_.push_back(out_[from + size_t(i)]);
      }
    }
  }

  bool Stored() {
    bitbuf_ = 0;
    bitcnt_ = 0;
    if (pos_ + 4 > size_)
      return false;
    const unsigned len = src_[pos_] | (src_[pos_ + 1] << 8);
    pos_ += 4;
    if (pos_ + len > size_)
      return false;
    out_.insert(out_.end(), src_ + pos_, src_ + pos_ + len);
    pos_ += len;
    return true;
  }

  bool Fixed() {
    static Huffman lit, dist;
    static bool built = false;
    if (!built) {
      uint8_t l[288];
      int i = 0;
      for (; i < 144; ++i) l[i] = 8;
      for (; i < 256; ++i) l[i] = 9;
      for (; i < 280; ++i) l[i] = 7;
      for (; i < 288; ++i) l[i] = 8;
      Build(lit, l, 288);
      for (i = 0; i < 30; ++i) l[i] = 5;
      Build(dist, l, 30);
      built = true;
    }
    return Codes(lit, dist);
  }

  bool Dynamic() {
    static const uint8_t kOrder[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    const int nlen = Bits(5) + 257, ndist = Bits(5) + 1, ncode = Bits(4) + 4;
    if (error_ || nlen > 286 || ndist > 30)
      return false;
    uint8_t lengths[320] = {};
    for (int i = 0; i < ncode; ++i)
      lengths[kOrder[i]] = uint8_t(Bits(3));
    Huffman lencode;
    Build(lencode, lengths, 19);
    int index = 0;
    while (index < nlen + ndist) {
      int sym = Decode(lencode);
      if (error_ || sym < 0)
        return false;
      if (sym < 16) {
        lengths[index++] = uint8_t(sym);
      } else {
        int len = 0, rep;
        if (sym == 16) {
          if (index == 0)
            return false;
          len = lengths[index - 1];
          rep = 3 + Bits(2);
        } else if (sym == 17) {
          rep = 3 + Bits(3);
        } else {
          rep = 11 + Bits(7);
        }
        if (index + rep > nlen + ndist)
          return false;
        while (rep--)
          lengths[index++] = uint8_t(len);
      }
    }
    Huffman lit, dist;
    Build(lit, lengths, nlen);
    Build(dist, lengths + nlen, ndist);
    return Codes(lit, dist);
  }

  bool Run() {
    int last;
    do {
      last = Bits(1);
      const int type = Bits(2);
      if (error_)
        return false;
      bool ok = false;
      if (type == 0)
        ok = Stored();
      else if (type == 1)
        ok = Fixed();
      else if (type == 2)
        ok = Dynamic();
      if (!ok)
        return false;
    } while (!last);
    return true;
  }

  const uint8_t *src_;
  size_t size_;
  size_t pos_ = 0;
  std::vector<uint8_t> &out_;
  uint32_t bitbuf_ = 0;
  int bitcnt_ = 0;
  bool error_ = false;
};
