"""Dump every text table of string.pac to a TSV (table, id, text).

Usage: python -I tools/dev/strpac.py <string.pac> <out.tsv>
Reads the player's own game file; the output is game text, never commit it.
Tables are zlib blocks, little-endian: {0, count, entries {off, len, id, 0}, texts}.
"""
import struct
import sys
import zlib


def tables(data):
    i = 0
    while True:
        i = data.find(b"\x78", i)
        if i < 0:
            return
        if i + 1 < len(data) and data[i + 1] in (0x01, 0x5E, 0x9C, 0xDA):
            try:
                z = zlib.decompressobj()
                block = z.decompress(data[i:])
            except zlib.error:
                block = b""
            if len(block) > 16:
                _, count = struct.unpack_from("<II", block, 0)
                if 0 < count < 100000 and 8 + count * 16 <= len(block):
                    yield block, count
                    i += len(data) - i - len(z.unused_data)
                    continue
        i += 1


def text(raw):
    if len(raw) > 1 and raw[1] == 0:
        return raw.decode("utf-16-le", "replace").rstrip("\0")
    return raw.decode("latin1").rstrip("\0")


def main():
    data = open(sys.argv[1], "rb").read()
    n = 0
    with open(sys.argv[2], "w", encoding="utf-8") as out:
        for block, count in tables(data):
            for k in range(count):
                off, ln, sid, _ = struct.unpack_from("<IIII", block, 8 + k * 16)
                out.write(f"{n}\t{sid}\t{text(block[off:off + ln])!r}\n")
            n += 1
    print("tables", n)


if __name__ == "__main__":
    main()
