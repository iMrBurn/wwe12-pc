// A game's own icon (the title image the Xbox 360 dashboard shows, a 64x64 PNG), read from the player's disc at run
// time (2026-11-09, for the game lists): nothing of it is shipped.
//
// Where it is (the XEX2 format as xenia reads it): default.xex's header lists optional headers {u32 key, u32 value}:
// 0x3FF the file format (u16 encryption, u16 compression - every WWE disc from SvR 2007 to 2K17: 1 / 1, AES and
// "basic": blocks {u32 data size, u32 zeros after}), 0x2FF the resources {name[8], u32 address, u32 size}, 0x10201
// the image base. The image is AES-128-CBC (IV 0) with the session key at security info + 0x150, itself AES-128-ECB
// with the retail key (else the devkit's, all zeros) - checked: the image starts with "MZ". The resource named by the
// title id is an XDBF table: "XDBF", u32 version, u32 entries, u32 used, u32 free, u32 used free; entries
// {u16 namespace, u64 id, u32 offset, u32 size} (namespace 2 = images, id 0x8000 = the title image), then the free
// list (8 bytes each), then the data. All big-endian.
#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace xex_icon {

using Bytes = std::vector<uint8_t>;

namespace detail {

inline uint32_t B32(const Bytes &d, size_t o) {
  return o + 4 <= d.size() ? uint32_t(d[o]) << 24 | d[o + 1] << 16 | d[o + 2] << 8 | d[o + 3] : 0;
}
inline uint16_t B16(const Bytes &d, size_t o) { return o + 2 <= d.size() ? uint16_t(d[o] << 8 | d[o + 1]) : 0; }

// AES-128, decryption only (FIPS-197's inverse cipher), the tables computed once
struct Aes {
  uint8_t sbox[256], inv[256];
  static uint8_t Mul(uint8_t a, uint8_t b) {
    uint8_t p = 0;
    for (int i = 0; i < 8; ++i) {
      if (b & 1)
        p ^= a;
      const bool hi = a & 0x80;
      a = uint8_t(a << 1);
      if (hi)
        a ^= 0x1B;
      b >>= 1;
    }
    return p;
  }
  Aes() {
    for (int i = 0; i < 256; ++i) {
      uint8_t x = 0;  // (the multiplicative inverse: i^254)
      if (i) {
        uint8_t r = 1, b = uint8_t(i);
        for (int e = 254; e; e >>= 1, b = Mul(b, b))
          if (e & 1)
            r = Mul(r, b);
        x = r;
      }
      uint8_t s = x;
      for (int k = 1; k <= 4; ++k)
        s ^= uint8_t((x << k) | (x >> (8 - k)));
      s ^= 0x63;
      sbox[i] = s;
      inv[s] = uint8_t(i);
    }
  }
  static const Aes &Get() {
    static const Aes a;
    return a;
  }
  // the 11 round keys
  std::array<uint8_t, 176> Expand(const uint8_t key[16]) const {
    std::array<uint8_t, 176> w{};
    memcpy(w.data(), key, 16);
    uint8_t rcon = 1;
    for (int i = 4; i < 44; ++i) {
      uint8_t t[4] = {w[4 * (i - 1)], w[4 * (i - 1) + 1], w[4 * (i - 1) + 2], w[4 * (i - 1) + 3]};
      if (i % 4 == 0) {
        const uint8_t t0 = t[0];
        t[0] = uint8_t(sbox[t[1]] ^ rcon), t[1] = sbox[t[2]], t[2] = sbox[t[3]], t[3] = sbox[t0];
        rcon = Mul(rcon, 2);
      }
      for (int j = 0; j < 4; ++j)
        w[size_t(4 * i + j)] = uint8_t(w[size_t(4 * (i - 4) + j)] ^ t[j]);
    }
    return w;
  }
  void DecryptBlock(const std::array<uint8_t, 176> &w, uint8_t s[16]) const {
    auto add = [&](int round) {
      for (int i = 0; i < 16; ++i)
        s[i] ^= w[size_t(16 * round + i)];
    };
    auto inv_shift_sub = [&]() {
      uint8_t t[16];
      for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
          t[4 * ((c + r) % 4) + r] = inv[s[4 * c + r]];
      memcpy(s, t, 16);
    };
    auto inv_mix = [&]() {
      for (int c = 0; c < 4; ++c) {
        const uint8_t a0 = s[4 * c], a1 = s[4 * c + 1], a2 = s[4 * c + 2], a3 = s[4 * c + 3];
        s[4 * c] = uint8_t(Mul(a0, 14) ^ Mul(a1, 11) ^ Mul(a2, 13) ^ Mul(a3, 9));
        s[4 * c + 1] = uint8_t(Mul(a0, 9) ^ Mul(a1, 14) ^ Mul(a2, 11) ^ Mul(a3, 13));
        s[4 * c + 2] = uint8_t(Mul(a0, 13) ^ Mul(a1, 9) ^ Mul(a2, 14) ^ Mul(a3, 11));
        s[4 * c + 3] = uint8_t(Mul(a0, 11) ^ Mul(a1, 13) ^ Mul(a2, 9) ^ Mul(a3, 14));
      }
    };
    add(10);
    for (int round = 9; round >= 1; --round) {
      inv_shift_sub();
      add(round);
      inv_mix();
    }
    inv_shift_sub();
    add(0);
  }
};

}  // namespace detail

// The title image (PNG bytes) of a default.xex, else empty.
inline Bytes TitleImage(const Bytes &x) {
  using namespace detail;
  if (x.size() < 0x18 || memcmp(x.data(), "XEX2", 4))
    return {};
  const uint32_t data_at = B32(x, 8), sec = B32(x, 0x10), n = B32(x, 0x14);
  uint32_t format = 0, res = 0, base = 0x82000000;
  for (uint32_t i = 0; i < n && 0x20 + 8 * size_t(i) <= x.size(); ++i) {
    const uint32_t k = B32(x, 0x18 + 8 * i), v = B32(x, 0x1C + 8 * i);
    if (k == 0x3FF)
      format = v;
    else if (k == 0x2FF)
      res = v;
    else if (k == 0x10201)
      base = v;
  }
  if (!format || !res || B16(x, format + 6) != 1 || data_at >= x.size() || sec + 0x160 > x.size())
    return {};
  const bool encrypted = B16(x, format + 4) == 1;
  // the payload decrypted (CBC, IV 0), with each key until the image reads as one
  Bytes plain;
  static const uint8_t kRetail[16] = {0x20, 0xB1, 0x85, 0xA5, 0x9D, 0x28, 0xFD, 0xC3,
                                      0x40, 0x58, 0x3F, 0xBB, 0x08, 0x96, 0xBF, 0x91};
  static const uint8_t kDevkit[16] = {};
  const size_t len = (x.size() - data_at) & ~size_t(15);
  auto image_of = [&](const Bytes &payload) {
    Bytes img;
    const uint32_t fsize = B32(x, format);
    size_t at = 0;
    for (uint32_t o = format + 8; o + 8 <= format + fsize && o + 8 <= x.size(); o += 8) {
      const uint32_t data = B32(x, o), zeros = B32(x, o + 4);
      if (at + data > payload.size())
        return Bytes{};
      img.insert(img.end(), payload.begin() + long(at), payload.begin() + long(at + data));
      img.insert(img.end(), zeros, 0);
      at += data;
    }
    return img;
  };
  Bytes img;
  for (const uint8_t *k : {kRetail, kDevkit}) {
    plain.assign(x.begin() + long(data_at), x.begin() + long(data_at + len));
    if (encrypted) {
      const auto &aes = Aes::Get();
      uint8_t session[16];
      memcpy(session, &x[sec + 0x150], 16);
      aes.DecryptBlock(aes.Expand(k), session);
      const auto w = aes.Expand(session);
      uint8_t prev[16] = {};
      for (size_t o = 0; o + 16 <= plain.size(); o += 16) {
        uint8_t cur[16];
        memcpy(cur, &plain[o], 16);
        aes.DecryptBlock(w, &plain[o]);
        for (int i = 0; i < 16; ++i)
          plain[o + size_t(i)] ^= prev[i];
        memcpy(prev, cur, 16);
      }
    }
    img = image_of(plain);
    if (img.size() >= 2 && img[0] == 'M' && img[1] == 'Z')
      break;
    img.clear();
    if (!encrypted)
      break;
  }
  if (img.empty())
    return {};
  // the resource: an XDBF table, its title image (namespace 2, id 0x8000)
  const uint32_t rsize = B32(x, res);
  for (uint32_t o = res + 4; o + 16 <= res + rsize && o + 16 <= x.size(); o += 16) {
    const uint32_t addr = B32(x, o + 8), size = B32(x, o + 12);
    if (addr < base || size_t(addr - base) + size > img.size())
      continue;
    const Bytes r(img.begin() + long(addr - base), img.begin() + long(addr - base + size));
    if (r.size() < 0x18 || memcmp(r.data(), "XDBF", 4))
      continue;
    const uint32_t entries = B32(r, 8), frees = B32(r, 0x10);
    const size_t data = 0x18 + 18 * size_t(entries) + 8 * size_t(frees);
    for (uint32_t i = 0; i < entries; ++i) {
      const size_t e = 0x18 + 18 * size_t(i);
      const uint16_t ns = B16(r, e);
      const uint64_t id = uint64_t(B32(r, e + 2)) << 32 | B32(r, e + 6);
      const uint32_t off = B32(r, e + 10), sz = B32(r, e + 14);
      if (ns == 2 && id == 0x8000 && data + off + sz <= r.size())
        return Bytes(r.begin() + long(data + off), r.begin() + long(data + off + sz));
    }
  }
  return {};
}

}  // namespace xex_icon
