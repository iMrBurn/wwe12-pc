"""Convert a SvR 2011 arena (pac/bg/bgNN.pac) for WWE '12 by completing it with entries of a WWE '12 arena.

Usage: python -I tools/dev/arena_convert.py <svr_arena.pac> <wwe12_donor.pac> <out.pac>
                                            [--add 980,30002,800,804,809] [--replace 995,996,997,999]
                                            [--stage NN]

Both files are the player's own game data: the output is a derived game file, never commit or ship it.

Layout (docs/limit_breaking.md, "Arènes"): EPAC header (+8 = PACH size rounded up to 0x800), at 0x800 a
"STG " record (+0x0C = 4-digit stage number in ASCII, +0x14 = PACH size in 256-byte units, rounded up),
then at 0x4000 a PACH: "PACH", count, {id, offset, size} (little endian), entry data from the end of the
table, each entry 4-byte aligned (most are Yuke's BPE blocks; they are copied as they are). The file ends with
a 0x800-byte block after the rounded PACH holding the packer's signature.

--add copies entries the SvR file lacks (980 ring ropes, 30002, 800/804/809 video screens by default);
--replace takes the donor's version of entries present in both (none by default).
--models converts every SvR 2011 JBOY model (top-level entries and those inside sub-PACHs such as 20000/20010)
to the WWE '12 layout (tools/dev/jboy.py) and recompresses the changed entries (yukes_bpe.compress). The game
loads an arena into a fixed buffer: a 16.7 MB file crashed and an 11.8 MB one hung (originals are at most ~10.9 MB).
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import jboy  # noqa: E402
import yukes_bpe  # noqa: E402

PACH_AT = 0x4000


def read_pach(data):
    if data[PACH_AT:PACH_AT + 4] != b"PACH":
        raise SystemExit("no PACH at 0x4000")
    count = struct.unpack_from("<I", data, PACH_AT + 4)[0]
    base = PACH_AT + 8 + count * 12
    entries = {}
    for i in range(count):
        eid, off, size = struct.unpack_from("<III", data, PACH_AT + 8 + i * 12)
        entries[eid] = data[base + off:base + off + size]
    return entries


SKIPPED = []


def read_sub_pach(data):
    count = struct.unpack_from("<I", data, 4)[0]
    base = 8 + count * 12
    return {eid: data[base + off:base + off + size]
            for eid, off, size in (struct.unpack_from("<III", data, 8 + i * 12) for i in range(count))}


def convert_models(data):
    """Return (new data, models converted) for a decompressed entry, or (None, 0) when unchanged."""
    if data[:4] == b"JBOY" and b"POF0" in data:
        try:
            return jboy.svr_to_w12(data), 1
        except ValueError as err:
            SKIPPED.append((jboy.u32(data, 0x18), jboy.u32(data, 0x1C), str(err)))
            return None, 0
    if data[:4] == b"PACH" and b"JBOY" in data:
        subs = read_sub_pach(data)
        done = 0
        for eid, sub in subs.items():
            new, n = convert_models(sub)
            if new is not None:
                subs[eid], done = new, done + n
        return (write_pach(subs), done) if done else (None, 0)
    return None, 0


def write_pach(entries):
    ids = sorted(entries)
    table = bytearray(b"PACH" + struct.pack("<I", len(ids)))
    body = bytearray()
    for eid in ids:
        while len(body) % 4:
            body.append(0)
        table += struct.pack("<III", eid, len(body), len(entries[eid]))
        body += entries[eid]
    return bytes(table + body)


def ids(text):
    return [int(x) for x in text.split(",") if x.strip()] if text else []


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("svr")
    ap.add_argument("donor")
    ap.add_argument("out")
    ap.add_argument("--add", default="980,30002,800,804,809")
    ap.add_argument("--replace", default="")
    ap.add_argument("--stage", type=int, default=None, help="stage number written in the STG record")
    ap.add_argument("--models", action="store_true", help="convert the SvR JBOY models to WWE '12")
    args = ap.parse_args()

    svr = open(args.svr, "rb").read()
    donor = open(args.donor, "rb").read()
    entries = read_pach(svr)
    donor_entries = read_pach(donor)

    if args.models:
        total = 0
        for eid, packed in entries.items():
            data = yukes_bpe.decompress_at(packed, 0) if packed[:4] == b"BPE " else packed
            if data is None:
                continue
            new, n = convert_models(data)
            if new is not None:
                entries[eid] = yukes_bpe.compress(new)
                total += n
        print(f"models converted: {total}, left as they are: {len(SKIPPED)} {sorted(set(SKIPPED))[:8]}")

    for eid in ids(args.add):
        if eid in entries:
            print(f"{eid}: already in the SvR arena, kept")
        elif eid in donor_entries:
            entries[eid] = donor_entries[eid]
            print(f"{eid}: added from the donor ({len(donor_entries[eid])} bytes)")
        else:
            print(f"{eid}: not in the donor, skipped")
    for eid in ids(args.replace):
        if eid in donor_entries:
            entries[eid] = donor_entries[eid]
            print(f"{eid}: replaced by the donor's")

    pach = write_pach(entries)
    rounded = (len(pach) + 0x7FF) & ~0x7FF
    header = bytearray(svr[:PACH_AT])
    struct.pack_into("<I", header, 8, rounded)
    struct.pack_into("<I", header, 0x800 + 0x14, (len(pach) + 0xFF) // 0x100)
    if args.stage is not None:
        header[0x800 + 0x0C:0x800 + 0x10] = b"%04d" % args.stage
    # Last 0x800 bytes: the packer's signature ("EOP5/plugin version 1.25", WWE '12; 1.26, SvR 2011).
    out = bytes(header) + pach + bytes(rounded - len(pach)) + svr[-0x800:]
    open(args.out, "wb").write(out)
    print(f"{args.out}: {len(entries)} entries, {len(out)} bytes")


if __name__ == "__main__":
    main()
