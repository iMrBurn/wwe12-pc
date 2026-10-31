// WWE '12 PC: the sound engine's lookup tables in the newest DLC's sound package, put back in the
// order the game's code expects (wwe12_comm_table_fix, on by default).
//
// With DLC packages installed the game drops the disc's sound tables (pac\sound.pac, /YAFH/DUPL)
// for those of the highest-numbered DLC (pac\sound_dlc_NN.pac, /YAFH/SDDC: sub_82747D48). Each
// table starts with {u32 type, u32 16 (header size), u32 data size, u32 count} (big-endian). The
// type 12 table is {u32 hash, u32 group} pairs: the disc's is sorted by hash, one DLC package's
// has the same pairs sorted by group, and with it Lawler's Universe replies stay silent (Cole asks,
// nobody answers). Sorting it by hash again changes nothing else: same pairs, nothing written to disk.
//
// The package's tables are "ZLIB"-wrapped, each on its own, and inflated through zlib's inflate
// (sub_827F57A8, two callers: sub_827E1248 whole buffer, sub_827E5F00 streamed). When a stream
// ends (inflate returns 1), its output starts at next_out (+12) - total_out (+20): a type 12 table
// there is sorted.

#include <algorithm>
#include <utility>
#include <vector>

#include <rex/cvar.h>

#include "core/logging.h"
#include "core/memory_helpers.h"

REXCVAR_DEFINE_BOOL(wwe12_comm_table_fix, true, "WWE12",
                    "Sort the DLC sound package's hash table the way the disc's is (Lawler's Universe commentary).");
REXCVAR_DEFINE_BOOL(wwe12_comm_table_probe, false, "WWE12",
                    "Probe: log each zlib stream's end (output address, size, first words).");

namespace {

u32 Be32(const u8 *p) { return u32(p[0]) << 24 | u32(p[1]) << 16 | u32(p[2]) << 8 | p[3]; }

void PutBe32(u8 *p, u32 v) {
  p[0] = u8(v >> 24);
  p[1] = u8(v >> 16);
  p[2] = u8(v >> 8);
  p[3] = u8(v);
}

constexpr u32 kHashTableType = 12, kHeaderSize = 16, kMaxEntries = 4096;

// Sorts the type 12 table at the start of [va, va + size) by hash; true when it was not sorted.
bool SortHashTable(u32 va, u32 size) {
  if (size < kHeaderSize + 8)
    return false;
  auto *p = static_cast<u8 *>(bd::mem::try_translate(va, 4));
  if (!p || Be32(p) != kHashTableType || Be32(p + 4) != kHeaderSize)
    return false;
  const u32 bytes = Be32(p + 8), count = Be32(p + 12);  // the pairs' size, header excluded
  if (count == 0 || count > kMaxEntries || bytes != count * 8 || kHeaderSize + bytes > size ||
      !bd::mem::try_translate(va + kHeaderSize + bytes - 1, 1))
    return false;
  std::vector<std::pair<u32, u32>> pairs(count);
  for (u32 i = 0; i < count; ++i)
    pairs[i] = {Be32(p + kHeaderSize + i * 8), Be32(p + kHeaderSize + i * 8 + 4)};
  const auto by_hash = [](const auto &a, const auto &b) { return a.first < b.first; };
  if (std::is_sorted(pairs.begin(), pairs.end(), by_hash))
    return false;
  std::stable_sort(pairs.begin(), pairs.end(), by_hash);
  for (u32 i = 0; i < count; ++i) {
    PutBe32(p + kHeaderSize + i * 8, pairs[i].first);
    PutBe32(p + kHeaderSize + i * 8 + 4, pairs[i].second);
  }
  return true;
}

}  // namespace

// Called by the inflate hook (wwe12_load_turbo.cpp) with the z_stream and inflate's result.
void Wwe12FixSoundTableAfterInflate(u32 stream, u32 result) {
  constexpr u32 kStreamEnd = 1;
  const u32 next_out = bd::mem::try_load<u32>(stream + 12), total_out = bd::mem::try_load<u32>(stream + 20);
  if (REXCVAR_GET(wwe12_comm_table_probe) && stream) {
    static int logged = 0;
    if (logged < 2000) {
      ++logged;
      const u32 out = next_out - total_out;
      BD_INFO("[sound] inflate result {} out 0x{:08X} total {} head {:08X} {:08X} {:08X} {:08X}", result, out,
              total_out, bd::mem::try_load<u32>(out), bd::mem::try_load<u32>(out + 4),
              bd::mem::try_load<u32>(out + 8), bd::mem::try_load<u32>(out + 12));
    }
  }
  if (result != kStreamEnd || !stream || !REXCVAR_GET(wwe12_comm_table_fix))
    return;
  if (!next_out || total_out < kHeaderSize + 8 || total_out > next_out)
    return;
  const u32 out = next_out - total_out;
  if (SortHashTable(out, total_out))
    BD_INFO("[sound] hash table at 0x{:08X} ({} entries) sorted by hash (wwe12_comm_table_fix)", out,
            bd::mem::try_load<u32>(out + 12));
}
