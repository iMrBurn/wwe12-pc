"""Yuke's JBOY models (arena pieces in pac/bg/bgNN.pac): read them and convert SvR 2011 models to WWE '12.

Library + small CLI:  python -I tools/dev/jboy.py info <file>   (a raw JBOY, already BPE-expanded)

Layout (big endian; docs/limit_breaking.md, "Comparaison JBOY"):
  +0x00 "JBOY", +0x04 data size (= start of the 8-byte trailer, then the "POF0" chunk), +0x0C same size,
  +0x18 mesh count, +0x1C mesh table offset (always 0x40), +0x28/+0x2C/+0x30 section offsets.
  Mesh records are contiguous at 0x40: 180 bytes in SvR 2011, 184 bytes in WWE '12, which inserts one word at
  +124 (a pointer to a lightmap block, or flags such as 0xB0020000). Everything after moves by 4.
  "POF0" + size + list of pointer locations: each step is one, two or four bytes (top two bits 01, 10, 11)
  holding a word count; positions are counted from offset 8 of the model. Every listed word holds an offset
  into the model, so moving data only needs these words and the list updated.
"""
import bisect
import struct
import sys

SVR_MESH = 180
W12_MESH = 184
MESH_TABLE = 0x40
INSERT_AT = 124
POF_BASE = 8


def u32(data, off):
    return struct.unpack_from(">I", data, off)[0]


def read_pof0(data):
    """Return (offset of "POF0", absolute pointer locations)."""
    at = data.rfind(b"POF0")
    if at < 0:
        raise ValueError("no POF0")
    size = u32(data, at + 4)
    table = data[at + 8:at + 8 + size]
    locs, pos, k = [], 0, 0
    while k < len(table):
        b = table[k]
        kind = b >> 6
        if kind == 0:
            break
        if kind == 1:
            step, k = b & 0x3F, k + 1
        elif kind == 2:
            step, k = ((b & 0x3F) << 8) | table[k + 1], k + 2
        else:
            step = ((b & 0x3F) << 24) | (table[k + 1] << 16) | (table[k + 2] << 8) | table[k + 3]
            k += 4
        pos += step * 4
        locs.append(pos + POF_BASE)
    return at, locs


def encode_pof0(locs):
    out = bytearray()
    prev = 0
    for loc in sorted(locs):
        step = (loc - POF_BASE - prev) // 4
        prev = loc - POF_BASE
        if step < 0x40:
            out.append(0x40 | step)
        elif step < 0x4000:
            out += bytes((0x80 | (step >> 8), step & 0xFF))
        else:
            out += bytes((0xC0 | (step >> 24), (step >> 16) & 0xFF, (step >> 8) & 0xFF, step & 0xFF))
    while len(out) % 4:
        out.append(0)
    return b"POF0" + struct.pack(">I", len(out)) + bytes(out)


def info(data):
    at, locs = read_pof0(data)
    return {
        "size": u32(data, 4),
        "meshes": u32(data, 0x18),
        "mesh_table": u32(data, 0x1C),
        "pof0_at": at,
        "pointers": len(locs),
    }


def svr_to_w12(data, inserted_value=0):
    """Convert one SvR 2011 JBOY to the WWE '12 layout (mesh records 180 -> 184 bytes)."""
    if data[:4] != b"JBOY":
        raise ValueError("not a JBOY")
    count = u32(data, 0x18)
    if u32(data, 0x1C) != MESH_TABLE:
        raise ValueError("mesh table not at 0x40")
    size = u32(data, 4)
    pof_at, locs = read_pof0(data)
    # Insertion points (old offsets): 4 bytes at +124 of each record, then padding after the table so that
    # everything behind it keeps its 16-byte alignment.
    table_end = MESH_TABLE + count * SVR_MESH
    inserts = [(MESH_TABLE + i * SVR_MESH + INSERT_AT, 4) for i in range(count)]
    pad = (-4 * count) % 16
    if pad:
        inserts.append((table_end, pad))
    points = [p for p, _ in inserts]
    sums = []
    total = 0
    for _, n in inserts:
        total += n
        sums.append(total)

    def moved(off):
        i = bisect.bisect_right(points, off)  # data at an insertion point moves too
        return off + (sums[i - 1] if i else 0)

    out = bytearray()
    prev = 0
    for (p, n), value in zip(inserts, [inserted_value] * count + [0]):
        out += data[prev:p]
        out += struct.pack(">I", value) if n == 4 and p != table_end else bytes(n)
        prev = p
    out += data[prev:size]
    tail = data[size:pof_at]  # 8-byte trailer before POF0
    new_size = len(out)
    new_locs = []
    for loc in locs:
        new_loc = moved(loc)
        struct.pack_into(">I", out, new_loc, moved(u32(data, loc)))
        new_locs.append(new_loc)
    struct.pack_into(">I", out, 4, new_size)
    struct.pack_into(">I", out, 0x0C, new_size)
    out += tail + encode_pof0(new_locs)
    end = pof_at + 8 + u32(data, pof_at + 4)
    out += data[end:]  # whatever follows POF0 (end marker, padding)
    return bytes(out)


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "info":
        print(info(open(sys.argv[2], "rb").read()))
    else:
        print(__doc__)


if __name__ == "__main__":
    main()
