// WWE12-PC: taken from Kaiko's WWE SmackDown vs. Raw 2011 PC port (with his permission, credit given),
// https://github.com/KaikoClanworth1/wwe-svr2011-pc, port/modmaker/svrfmt/pac.h at b2521fe.
// Game archives: EPAC (outer, little-endian), PACH (index), BPE (Yuke's
// byte-pair compression). Mirrors port/tools/svrfmt.py (tested on all arena,
// menu and crowd archives: repacks are byte-identical).
#pragma once

#include <string>
#include <vector>

#include "bytes.h"

namespace svrfmt {

// ---- BPE
bool BpeDecode(const Bytes& in, Bytes& out);
// "Stored" blocks (identity pair table + raw data): instant, any decoder reads it.
Bytes BpeEncodeStored(const Bytes& raw);
// Gage byte-pair compression in 4000-byte blocks (the game's block buffer).
Bytes BpeEncode(const Bytes& raw);
inline bool IsBpe(const Bytes& b) { return b.size() >= 16 && !std::memcmp(b.data(), "BPE ", 4); }
// An entry as stored in a PACH, unpacked if it is BPE.
Bytes Unpack(const Bytes& entry);

// ---- PACH
struct PachEntry {
  uint32_t id = 0;
  Bytes data;  // as stored (usually BPE)
};
inline bool IsPach(const Bytes& b) { return b.size() >= 8 && !std::memcmp(b.data(), "PACH", 4); }
bool PachRead(const Bytes& in, std::vector<PachEntry>& out);
Bytes PachWrite(const std::vector<PachEntry>& entries);

// ---- EPAC / EPK8
// Group headers {type 4, u16 count in 4-byte words, u16 flags, u32}, then
// entries {name, u32 sector, u32 size / 0x100}: 12-byte entries with 4-char
// names (count = entries x 3) or 16-byte ones with 8-char names (x 4). The
// magic doesn't decide which (2K14 "EPAC" pacs have 16-byte entries, WWE '13's
// chEtc "EPK8" 12-byte ones); bit 0 of the flags usually does, but SvR 2007
// has junk there - so the table is read the way that is consistent. Entries
// sit on 0x800 sectors from 0x4000. Header +4 = the table's size, +8 the data
// size. A group's u32 is 0, or (SvR 2007) the group's first data sector.
// WWE 2K16 model pacs: 20-byte entries {name[4], u32 sector, u32 size / 0x100,
// u32 size / 0x100 again, u32 tail ("0001" or 0)} - count = entries x 4 (e20).
struct EpacEntry {
  std::string name;  // 4 chars (wide: 8)
  Bytes data;
  uint32_t tail = 0;  // (e20) the entry's last u32, kept
};
struct EpacGroup {
  std::string type;  // 4 chars
  std::vector<EpacEntry> entries;
  uint16_t number = 0;  // the u16 after the count, kept
  uint32_t extra = 0;   // the u32 after that, kept (or the first sector: group_sectors)
};
struct Epac {
  Bytes header;   // bytes 0..0x800 (the magic included)
  std::vector<EpacGroup> groups;
  Bytes trailer;  // packer footer ("EOP5/plugin version ..."), kept on repack
  bool epk8 = false;           // the magic says EPK8
  bool wide = false;           // 16-byte entries, 8-char names
  bool e20 = false;            // 20-byte entries (WWE 2K16), 4-char names
  bool group_sectors = false;  // each group's u32 is its first data sector (SvR 2007)
  int32_t toc_extra = 0;       // header +4 minus the table's size (kept, so a repack is byte-identical)
};
// One table row (EpacTable): where an entry is, without reading the data.
struct EpacRow {
  std::string group, name;
  uint64_t offset = 0;
  uint32_t size = 0;
};
// The table from the first 0x4000 bytes; entries must end within file_size (0: unchecked).
bool EpacTable(const Bytes& head, uint64_t file_size, std::vector<EpacRow>& rows, bool* wide = nullptr);
bool EpacRead(const Bytes& in, Epac& out);  // EPAC or EPK8
Bytes EpacWrite(const Epac& e);

}  // namespace svrfmt
