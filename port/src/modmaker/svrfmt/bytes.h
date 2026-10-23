// WWE12-PC: taken from Kaiko's WWE SmackDown vs. Raw 2011 PC port (with his permission, credit given),
// https://github.com/KaikoClanworth1/wwe-svr2011-pc, port/modmaker/svrfmt/bytes.h at b2521fe.
// Byte helpers for the game's file formats (arenas branch, Mod Maker).
#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace svrfmt {

using Bytes = std::vector<uint8_t>;

inline uint32_t Le32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | uint32_t(p[3]) << 24; }
inline uint16_t Le16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
inline uint32_t Be32(const uint8_t* p) { return uint32_t(p[0]) << 24 | p[1] << 16 | p[2] << 8 | p[3]; }
inline uint16_t Be16(const uint8_t* p) { return uint16_t(p[0] << 8 | p[1]); }
inline float BeF(const uint8_t* p) { uint32_t v = Be32(p); float f; std::memcpy(&f, &v, 4); return f; }
inline float LeF(const uint8_t* p) { uint32_t v = Le32(p); float f; std::memcpy(&f, &v, 4); return f; }

inline void PutLe32(uint8_t* p, uint32_t v) { p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); p[2] = uint8_t(v >> 16); p[3] = uint8_t(v >> 24); }
inline void PutBe32(uint8_t* p, uint32_t v) { p[0] = uint8_t(v >> 24); p[1] = uint8_t(v >> 16); p[2] = uint8_t(v >> 8); p[3] = uint8_t(v); }
inline void PutBe16(uint8_t* p, uint16_t v) { p[0] = uint8_t(v >> 8); p[1] = uint8_t(v); }
inline void PutBeF(uint8_t* p, float f) { uint32_t v; std::memcpy(&v, &f, 4); PutBe32(p, v); }

inline void AppLe32(Bytes& b, uint32_t v) { size_t o = b.size(); b.resize(o + 4); PutLe32(&b[o], v); }
inline void AppBe32(Bytes& b, uint32_t v) { size_t o = b.size(); b.resize(o + 4); PutBe32(&b[o], v); }
inline void AppBe16(Bytes& b, uint16_t v) { size_t o = b.size(); b.resize(o + 2); PutBe16(&b[o], v); }
inline void AppBeF(Bytes& b, float f) { size_t o = b.size(); b.resize(o + 4); PutBeF(&b[o], f); }
inline void App(Bytes& b, const uint8_t* p, size_t n) { b.insert(b.end(), p, p + n); }
inline void App(Bytes& b, const Bytes& s) { b.insert(b.end(), s.begin(), s.end()); }
inline void Pad(Bytes& b, size_t align, size_t base = 0) {
  while ((b.size() - base) % align) b.push_back(0);
}

// A 16-byte name field: bytes up to the first NUL (names may be Shift-JIS,
// so they are kept as raw bytes in a std::string).
inline std::string Name16(const uint8_t* p, size_t n = 16) {
  size_t l = 0;
  while (l < n && p[l]) ++l;
  return std::string(reinterpret_cast<const char*>(p), l);
}
inline void AppName(Bytes& b, const std::string& s, size_t n = 16) {
  size_t o = b.size();
  b.resize(o + n, 0);
  std::memcpy(&b[o], s.data(), s.size() < n ? s.size() : n);
}

bool ReadFile(const std::string& path, Bytes& out);
bool WriteFile(const std::string& path, const Bytes& data);

}  // namespace svrfmt
