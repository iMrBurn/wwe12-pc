// Mod Maker: an entrance theme written as WWE '12's own themes are (sound\Music.pck: Wwise Vorbis, 32 kHz stereo), so a
// ported theme plays whole: as Wwise PCM (theme_convert.h) one over about 20 MiB never started, hence the 100 s cap.
// Measured on the disc's 131 streams (2026-11-10, work notes docs/mod_maker.md "Themes in Wwise Vorbis"):
//
// - its 125 stereo 32 kHz themes share one setup packet (201 bytes), and aoTuV beta6.03 at quality 0.2 (32 kHz stereo)
//   gives exactly it once written in Wwise's compact form: codebooks by number (10 bits) in the game's own codebook
//   table (599 entries at 0x8310D1D8 in default.xex, not ww2ogg's order), no time-domain list, floor / residue /
//   mapping types shortened, no window / transform types. The numbers below are those of aoTuV's 38 books there; the
//   result is checked against the disc setup's hash (no game data here);
// - audio packets "modified": without the packet-type bit and, in a long block, the two window flags; each {u16 size, bytes};
// - seek table: a point at the first packet whose end position is at least 2048 samples after the previous point,
//   {u16 samples, u16 bytes} from the previous one, bytes counted from the setup's start;
// - the "fmt " vorb fields (0x18..): samples, setup + 2, setup + 2 + audio, extra samples of the last packets
//   (packets' samples - samples), seek table size, its + setup + 2, (largest packet << 16 | extra), the decoder sizes
//   0x37EC / 0x39AC, the codebooks' hash 0x24E8B2D4, block exponents 8 / 11 (as every stereo theme of the disc).
// Checked out of the game: Cena's theme encoded again is read by vgmstream (exact length; 21.5 dB against the input,
// as a plain Ogg Vorbis of the same quality).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <vorbis/vorbisenc.h>

namespace wwise_vorbis {

using Bytes = std::vector<uint8_t>;

constexpr int kRate = 32000;

namespace detail {

struct BitReader {
  const uint8_t *b;
  size_t n, p = 0;  // (in bits)
  bool bad = false;
  uint32_t operator()(int bits) {
    uint32_t v = 0;
    for (int i = 0; i < bits; ++i, ++p) {
      if ((p >> 3) >= n) {
        bad = true;
        return 0;
      }
      v |= uint32_t((b[p >> 3] >> (p & 7)) & 1) << i;
    }
    return v;
  }
};

struct BitWriter {
  Bytes out;
  size_t p = 0;
  void operator()(uint32_t v, int bits) {
    for (int i = 0; i < bits; ++i, ++p) {
      if ((p >> 3) >= out.size())
        out.push_back(0);
      out[p >> 3] |= uint8_t(((v >> i) & 1) << (p & 7));
    }
  }
};

inline int Ilog(uint32_t v) {
  int n = 0;
  while (v)
    ++n, v >>= 1;
  return n;
}

inline long QuantVals(long entries, long dims) {  // (libvorbis' _book_maptype1_quantvals)
  long vals = long(std::floor(std::pow(double(entries), 1.0 / double(dims))));
  for (;;) {
    long acc = 1, acc1 = 1;
    for (long i = 0; i < dims; ++i)
      acc *= vals, acc1 *= vals + 1;
    if (acc <= entries && acc1 > entries)
      return vals;
    vals += acc > entries ? -1 : 1;
  }
}

// Skips one full codebook of a Vorbis setup.
inline void SkipBook(BitReader &r) {
  if (r(24) != 0x564342) {
    r.bad = true;
    return;
  }
  const uint32_t dims = r(16), entries = r(24);
  if (r(1)) {
    r(5);
    for (uint32_t cur = 0; cur < entries && !r.bad;)
      cur += r(Ilog(entries - cur));
  } else {
    const uint32_t sparse = r(1);
    for (uint32_t i = 0; i < entries && !r.bad; ++i)
      if (!sparse || r(1))
        r(5);
  }
  const uint32_t lookup = r(4);
  if (lookup == 1 || lookup == 2) {
    r(32), r(32);
    const int len = int(r(4)) + 1;
    r(1);
    const long q = lookup == 1 ? QuantVals(long(entries), long(dims)) : long(entries) * long(dims);
    for (long i = 0; i < q && !r.bad; ++i)
      r(len);
  } else if (lookup) {
    r.bad = true;
  }
}

inline uint64_t Fnv64(const Bytes &d) {
  uint64_t h = 0xCBF29CE484222325ull;
  for (uint8_t c : d)
    h = (h ^ c) * 0x100000001B3ull;
  return h;
}

// aoTuV's 38 codebooks (quality 0.2, 32 kHz stereo) in the game's codebook table, in its setup's order.
constexpr uint16_t kGameBooks[38] = {81,  59,  361, 97,  551, 104, 79,  521, 82,  284, 528, 354, 197,
                                     142, 582, 461, 115, 496, 210, 73,  151, 360, 57,  196, 156, 314,
                                     395, 282, 542, 550, 331, 526, 236, 18,  12,  290, 321, 319};
constexpr uint64_t kDiscSetupHash = 0xB3743D90DE26477Eull;  // (FNV-1a 64 of the disc themes' 201-byte setup)

// A Vorbis setup header (packet 3) in Wwise's compact form. Empty if it isn't the one expected.
inline Bytes WwiseSetup(const ogg_packet &setup, int channels) {
  BitReader r{setup.packet, size_t(setup.bytes)};
  BitWriter w;
  if (setup.bytes < 8 || setup.packet[0] != 5 || memcmp(setup.packet + 1, "vorbis", 6))
    return {};
  r.p = 7 * 8;
  const uint32_t books = r(8) + 1;
  if (books != 38)
    return {};
  w(books - 1, 8);
  for (uint32_t i = 0; i < books; ++i) {
    SkipBook(r);
    w(kGameBooks[i], 10);
  }
  for (uint32_t i = 0, n = r(6) + 1; i < n; ++i)
    if (r(16))
      return {};
  const uint32_t floors = r(6) + 1;
  w(floors - 1, 6);
  for (uint32_t f = 0; f < floors && !r.bad; ++f) {
    if (r(16) != 1)
      return {};
    const uint32_t parts = r(5);
    w(parts, 5);
    std::vector<uint32_t> cls(parts);
    uint32_t top = 0;
    for (auto &c : cls)
      c = r(4), w(c, 4), top = std::max(top, c + 1);
    std::vector<uint32_t> dims(top);
    for (uint32_t c = 0; c < top; ++c) {
      dims[c] = r(3) + 1;
      w(dims[c] - 1, 3);
      const uint32_t sub = r(2);
      w(sub, 2);
      if (sub)
        w(r(8), 8);
      for (uint32_t k = 0; k < (1u << sub); ++k)
        w(r(8), 8);
    }
    w(r(2), 2);
    const int range = int(r(4));
    w(uint32_t(range), 4);
    for (uint32_t c : cls)
      for (uint32_t k = 0; k < dims[c]; ++k)
        w(r(range), range);
  }
  const uint32_t residues = r(6) + 1;
  w(residues - 1, 6);
  for (uint32_t i = 0; i < residues && !r.bad; ++i) {
    const uint32_t type = r(16);
    if (type > 2)
      return {};
    w(type, 2);
    for (int k = 0; k < 3; ++k)
      w(r(24), 24);
    const uint32_t classes = r(6) + 1;
    w(classes - 1, 6);
    w(r(8), 8);
    std::vector<uint32_t> cascade(classes);
    for (auto &c : cascade) {
      const uint32_t lo = r(3), flag = r(1);
      w(lo, 3), w(flag, 1);
      uint32_t hi = 0;
      if (flag)
        hi = r(5), w(hi, 5);
      c = hi * 8 + lo;
    }
    for (uint32_t c : cascade)
      for (int k = 0; k < 8; ++k)
        if (c & (1u << k))
          w(r(8), 8);
  }
  const uint32_t mappings = r(6) + 1;
  w(mappings - 1, 6);
  for (uint32_t i = 0; i < mappings && !r.bad; ++i) {
    if (r(16))
      return {};
    const uint32_t flag = r(1);
    w(flag, 1);
    uint32_t subs = 1;
    if (flag)
      subs = r(4) + 1, w(subs - 1, 4);
    const uint32_t polar = r(1);
    w(polar, 1);
    if (polar) {
      const uint32_t steps = r(8) + 1;
      w(steps - 1, 8);
      const int bits = Ilog(uint32_t(channels - 1));
      for (uint32_t s = 0; s < steps; ++s)
        w(r(bits), bits), w(r(bits), bits);
    }
    if (r(2))
      return {};
    w(0, 2);
    if (subs > 1)
      for (int c = 0; c < channels; ++c)
        w(r(4), 4);
    for (uint32_t s = 0; s < subs; ++s)
      w(r(8), 8), w(r(8), 8), w(r(8), 8);
  }
  const uint32_t modes = r(6) + 1;
  w(modes - 1, 6);
  for (uint32_t i = 0; i < modes && !r.bad; ++i) {
    w(r(1), 1);
    if (r(16) || r(16))
      return {};
    w(r(8), 8);
  }
  if (r.bad || r(1) != 1)
    return {};
  return w.out;
}

// A Vorbis audio packet in Wwise's "modified" form: without bit 0 (packet type) and, in a long block, bits 2-3 (the
// window flags; bit 1 is the mode: 2 modes). (No byte cut at the end: a zero read as "end of packet" changes the sound.)
inline Bytes Modified(const ogg_packet &op, bool long_block) {
  Bytes out(size_t(op.bytes), 0);
  const size_t bits = size_t(op.bytes) * 8;
  size_t o = 0;
  for (size_t i = 1; i < bits; ++i) {
    if (long_block && (i == 2 || i == 3))
      continue;
    if ((op.packet[i >> 3] >> (i & 7)) & 1)
      out[o >> 3] |= uint8_t(1 << (o & 7));
    ++o;
  }
  out.resize((o + 7) / 8);
  return out;
}

inline void A16(Bytes &b, uint32_t v) { b.push_back(uint8_t(v >> 8)), b.push_back(uint8_t(v)); }
inline void A32(Bytes &b, uint32_t v) {
  for (int i = 0; i < 4; ++i)
    b.push_back(uint8_t(v >> (24 - 8 * i)));
}

}  // namespace detail

// 32 kHz stereo 16-bit samples (interleaved) as a Wwise Vorbis stream (RIFX) like the disc's themes. Empty (and why)
// if it can't.
inline Bytes Encode(const int16_t *samples, size_t frames, std::string *why = nullptr) {
  using namespace detail;
  auto fail = [&](const char *w) {
    if (why)
      *why = w;
    return Bytes{};
  };
  if (!samples || frames < 4096 || frames > 0x7FFFFFFF)
    return fail("no sound");
  vorbis_info vi;
  vorbis_info_init(&vi);
  if (vorbis_encode_init_vbr(&vi, 2, kRate, 0.2f)) {
    vorbis_info_clear(&vi);
    return fail("encoder setup failed");
  }
  vorbis_comment vc;
  vorbis_dsp_state vd;
  vorbis_block vb;
  vorbis_comment_init(&vc);
  vorbis_analysis_init(&vd, &vi);
  vorbis_block_init(&vd, &vb);
  ogg_packet h0, h1, h2;
  vorbis_analysis_headerout(&vd, &vc, &h0, &h1, &h2);
  const Bytes setup = WwiseSetup(h2, 2);
  const bool blocks_ok = vorbis_info_blocksize(&vi, 0) == 256 && vorbis_info_blocksize(&vi, 1) == 2048;

  Bytes audio;
  std::vector<uint32_t> offset;  // (of each packet in `audio`)
  std::vector<uint64_t> end;     // (its end position in samples)
  uint64_t pos = 0;
  int prev = 0;
  uint32_t largest = 0;
  bool ok = !setup.empty() && Fnv64(setup) == kDiscSetupHash && blocks_ok;
  size_t done = 0;
  bool wrote_end = false, eos = false;
  while (ok && !eos) {
    if (done < frames) {
      const size_t n = std::min<size_t>(frames - done, 1024);
      float **buf = vorbis_analysis_buffer(&vd, int(n));
      for (size_t i = 0; i < n; ++i) {
        buf[0][i] = samples[2 * (done + i)] / 32768.f;
        buf[1][i] = samples[2 * (done + i) + 1] / 32768.f;
      }
      vorbis_analysis_wrote(&vd, int(n));
      done += n;
    } else if (!wrote_end) {
      vorbis_analysis_wrote(&vd, 0);
      wrote_end = true;
    }
    while (ok && vorbis_analysis_blockout(&vd, &vb) == 1) {
      vorbis_analysis(&vb, nullptr);
      vorbis_bitrate_addblock(&vb);
      ogg_packet op;
      while (vorbis_bitrate_flushpacket(&vd, &op)) {
        const int bs = int(vorbis_packet_blocksize(&vi, &op));
        const Bytes m = Modified(op, bs == 2048);
        if (m.size() > 0xFFFF || bs <= 0) {
          ok = false;
          break;
        }
        if (!offset.empty())
          pos += uint64_t(prev / 4 + bs / 4);
        prev = bs;
        offset.push_back(uint32_t(audio.size()));
        end.push_back(pos);
        A16(audio, uint32_t(m.size()));
        audio.insert(audio.end(), m.begin(), m.end());
        largest = std::max(largest, uint32_t(m.size()));
        if (op.e_o_s)
          eos = true;
      }
    }
  }
  vorbis_block_clear(&vb);
  vorbis_dsp_clear(&vd);
  vorbis_comment_clear(&vc);
  vorbis_info_clear(&vi);
  if (setup.empty() || Fnv64(setup) != kDiscSetupHash || !blocks_ok)
    return fail("the encoder's setup isn't the disc themes'");
  if (!ok || pos < frames)
    return fail("encoding failed");

  Bytes seek;
  uint64_t last_pos = 0;
  uint32_t last_off = 0;
  for (size_t p = 0; p < offset.size(); ++p)
    if (end[p] >= last_pos + 2048) {
      const uint32_t at = uint32_t(setup.size() + 2 + offset[p]);
      if (end[p] - last_pos > 0xFFFF || at - last_off > 0xFFFF)
        return fail("seek table overflow");
      A16(seek, uint32_t(end[p] - last_pos));
      A16(seek, at - last_off);
      last_pos = end[p], last_off = at;
    }
  const uint32_t extra = uint32_t(pos - frames);
  const uint32_t data_size = uint32_t(seek.size() + 2 + setup.size() + audio.size());
  Bytes out = {'R', 'I', 'F', 'X'};
  A32(out, 4 + 8 + 66 + 8 + data_size);
  out.insert(out.end(), {'W', 'A', 'V', 'E', 'f', 'm', 't', ' '});
  A32(out, 66);
  A16(out, 0xFFFF);
  A16(out, 2);
  A32(out, kRate);
  A32(out, uint32_t(double(audio.size()) * kRate / double(frames) + 0.5));  // (average bytes per second, audio only)
  A16(out, 0), A16(out, 0), A16(out, 0x30), A16(out, 0x10);
  A32(out, 3);  // (channel mask FL FR)
  A32(out, uint32_t(frames));
  A32(out, uint32_t(setup.size() + 2));
  A32(out, uint32_t(setup.size() + 2 + audio.size()));
  A32(out, extra);
  A32(out, uint32_t(seek.size()));
  A32(out, uint32_t(seek.size() + setup.size() + 2));
  A32(out, largest << 16 | (extra & 0xFFFF));
  A32(out, 0x37EC), A32(out, 0x39AC), A32(out, 0x24E8B2D4);
  out.push_back(8), out.push_back(11);
  out.insert(out.end(), {'d', 'a', 't', 'a'});
  A32(out, data_size);
  out.insert(out.end(), seek.begin(), seek.end());
  A16(out, uint32_t(setup.size()));
  out.insert(out.end(), setup.begin(), setup.end());
  out.insert(out.end(), audio.begin(), audio.end());
  return out;
}

// Whether `wem` is a stream this encoder wrote (WWE '12's stereo theme format: Wwise Vorbis, 32 kHz stereo, the
// codebooks' hash 0x24E8B2D4 and the disc themes' setup).
inline bool IsOurs(const Bytes &wem) {
  if (wem.size() < 12 + 8 + 66 + 8 || memcmp(wem.data(), "RIFX", 4) || memcmp(&wem[12], "fmt ", 4) ||
      memcmp(&wem[12 + 8 + 66], "data", 4))
    return false;
  auto be32 = [&](size_t o) { return uint32_t(wem[o]) << 24 | uint32_t(wem[o + 1]) << 16 | uint32_t(wem[o + 2]) << 8 | wem[o + 3]; };
  const size_t fmt = 20, data = 12 + 8 + 66 + 8;
  if (be32(fmt) != 0xFFFF0002u || be32(fmt + 4) != kRate || be32(fmt + 0x18 + 0x24) != 0x24E8B2D4u)
    return false;
  const size_t so = data + be32(fmt + 0x18 + 0x10);
  if (so + 2 > wem.size())
    return false;
  const size_t n = size_t(wem[so]) << 8 | wem[so + 1];
  return so + 2 + n <= wem.size() && detail::Fnv64(Bytes(wem.begin() + long(so + 2), wem.begin() + long(so + 2 + n))) == detail::kDiscSetupHash;
}

}  // namespace wwise_vorbis
