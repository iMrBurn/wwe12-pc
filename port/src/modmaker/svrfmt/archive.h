// WWE12-PC: taken from Kaiko's WWE SmackDown vs. Raw 2011 PC port (with his permission, credit given),
// https://github.com/KaikoClanworth1/wwe-svr2011-pc, port/modmaker/svrfmt/archive.h at b2521fe.
// Every Yuke's / Visual Concepts pac layout from SvR 2007 to WWE 2K17 as one tree, for the
// Mod Maker's pac editor (checklist D16, 2.1.2 "full EPAC support"): open, browse nested
// containers, change / add / remove entries and save in the file's own layout.
//
//   file wrapping   xcompress (WWE '13 - 2K17: LZX chunks; written back as stored chunks -
//                   the decoder here and svrmod read them, nothing re-compresses LZX)
//   archives        EPAC / EPK8 with 12-, 16- or 20-byte entries (pac.h; 20: WWE 2K16)
//                   HSPC (WWE 2K17): header 0x40 bytes in sector 0 {'HSPC', 0x7DA, 0x40, 0x800,
//                   hash, 1, index bytes, 2, location bytes, last sector, 0x70, sectors, ?, 0,
//                   count, data bytes}; at 0x800 the index {u64 name hash, u32 0, u32 location
//                   offset, i32 -1}; after it (0x800-aligned) the locations {u32 sector, u32
//                   size / 0x100, u32 flags}; entries on 0x800 sectors in index order; a last
//                   sector (a "ZLIB" note) kept. Names are hashes (the hash isn't known).
//   containers      PACH {id: data} (BPE- or ZLIB-wrapped or bare)
//                   SHDC (WWE 2K17, PACH's place): 0x40-byte header {'SHDC', 0x7DA, 0x40, 1,
//                   hash, 0x40 index offset, index bytes, table offset, table bytes, 0 x5, count,
//                   data bytes rounded to 4}; index {u64 hash of the child id, u32 0, u32 16 i,
//                   i32 -1}; table {u32 id, u32 offset from the SHDC, u32 size, u32 0}; children
//                   from the table's end, each on 4 bytes; then (if +0x24 isn't 0) two sections
//                   {+0x24 offset, +0x28 size, +0x2C offset, +0x30 size}: a "ZLIB" note + one more
//   entry wrapping  BPE (pac.h), "ZLIB" {u32 0x1230, u32 packed, u32 size} - written back
//                   stored (packed == size: the raw bytes, which every reader takes)
//
// Containers are parsed when opened (Expand); a node nothing changed keeps its original bytes,
// so an open + save with no change gives the same file, and a rebuilt container (Touch) with
// unchanged children gives the same bytes too (--test-archive checks both).
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "pac.h"

namespace svrfmt {

enum class ArcKind { Leaf, Epac, Hspc, Shdc, Pach };
enum class ArcWrap { None, Bpe, Zlib };

struct ArcNode {
  ArcKind kind = ArcKind::Leaf;
  ArcWrap wrap = ArcWrap::None;
  std::string name;    // EPAC entry name; "0x1A" child id; "#3f8a..." hash (HSPC)
  std::string group;   // (EPAC entries) the group's type
  uint32_t id = 0;     // PACH / SHDC child id
  uint64_t hash = 0;   // HSPC / SHDC index hash
  uint32_t tail = 0;   // (2K16 entries) the entry's last u32
  uint32_t flags = 0;  // (HSPC entries) the location's flags
  int gi = -1;         // (EPAC entries) the group's index (a pac can have two groups of one type); -1: by type
  uint32_t align = 4;  // (PACH) its children's alignment in the file (some menus: 32)
  uint64_t order = 0;  // (HSPC entries) where its data was (the rebuild keeps that order; new ones last)
  Bytes stored;        // as stored (wrapped); a container's rebuilt bytes after Save
  bool parsed = false; // a container's children are read (Expand)
  bool dirty = false;  // changed (or a child is): rebuilt on Save
  std::vector<ArcNode> kids;
  // the container's own bytes kept for a rebuild
  Epac epac;           // (Epac) header / groups' numbers / trailer; entries' data come from kids
  Bytes head;          // (Hspc) bytes 0..0x800, (Shdc) the 0x40-byte header
  Bytes trailer;       // (Hspc) the last sector; (Shdc) the two sections after the data (header +0x24 / +0x2C)
  std::string note;    // why it can't be parsed / written, if so
};

struct Archive {
  ArcNode root;            // kind Epac or Hspc (or a bare Pach / Shdc file)
  bool xcompress = false;  // the file was xcompressed (written back with stored chunks)
  Bytes xheader;           // its 16-byte header (hash, chunk size kept)
  Bytes original;          // the file as it was
  std::string format;      // "EPAC (12-byte entries)", "HSPC", ...
};

// The file -> its tree (the top level read; containers on Expand).
bool ArchiveOpen(const Bytes& file, Archive& a, std::string* error);
// Reads a node's children if it is a container (PACH / SHDC / EPAC / HSPC inside an entry). False if it isn't one.
bool ArchiveExpand(ArcNode& n);
// The node's data unwrapped (BPE / ZLIB undone).
Bytes ArchiveData(const ArcNode& n);
// What the data is: "PACH", "SHDC", "EPAC", "HSPC", "DDS", "textures", "model", "motions", "text", "data".
std::string ArchiveKindOf(const ArcNode& n);
// New data for a leaf (wrapped like it was: BPE / ZLIB stored / none).
void ArchiveReplace(ArcNode& n, const Bytes& raw);
// Why a container can't take a new entry ("" = it can). HSPC: never (its names are hashes of a function nobody
// knows); SHDC: any id (its hash is known, ShdcHashFor).
std::string ArchiveCantAdd(const ArcNode& parent, uint32_t id = 0);
// A new entry holding raw (wrapped like its siblings): EPAC group + name (4 characters, 8 in 16-byte pacs; a new
// group type adds a group), PACH / SHDC id (in id order). Null (and why) when it can't be added.
ArcNode* ArchiveAdd(ArcNode& parent, const std::string& group, const std::string& name, uint32_t id, const Bytes& raw,
                    std::string* why);
void ArchiveRemove(ArcNode& parent, size_t i);
// An EPAC entry's new name (unique in its group, the pac's name length). False (and why) when it can't.
bool ArchiveRename(ArcNode& parent, size_t i, const std::string& name, std::string* why);
// The longest entry name the EPAC takes (4 or 8).
size_t ArchiveNameMax(const ArcNode& epac);
// Marks a node to be rebuilt from its children on Save (the path down to it must be marked too: Save does).
void ArchiveTouch(ArcNode& n);
// The file in its own layout (xcompressed again if it was). Untouched nodes keep their bytes.
Bytes ArchiveSave(Archive& a);
// Every container node parsed and marked dirty, so Save rebuilds the whole tree (round-trip test).
void ArchiveTouchAll(ArcNode& n, int depth = 8);

// xcompress with stored chunks (no LZX): read back by XcompressDecode / svrmod xdec.
Bytes XcompressStore(const Bytes& plain, const Bytes& like_header);
// "ZLIB" entry, stored form.
Bytes ZlibStore(const Bytes& raw);

// A new SHDC child id's index hash: MurmurHash64A of the id in decimal, seed 0x12345678 (the same id has the same
// hash in every file; one seen in an SHDC read is used first).
uint64_t ShdcHashFor(uint32_t id);

}  // namespace svrfmt
