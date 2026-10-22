"""Print the menu.pac record tree with label names.

Usage: python -I tools/dev/menutree.py <menu.pac> <strings.tsv> [start_hex end_hex]
strings.tsv comes from tools/dev/strpac.py (first occurrence of each id is used).
Records are big-endian, 0x74 bytes, marked by 0x05F5E0FF at +0x1C (see CLAUDE.md, menu).
Default range = the copy the game uses (0x154800..0x15A900).
"""
import struct
import sys


def main():
    data = open(sys.argv[1], "rb").read()
    names = {}
    for line in open(sys.argv[2], encoding="utf-8"):
        parts = line.rstrip("\n").split("\t", 2)
        if len(parts) == 3:
            names.setdefault(int(parts[1]), parts[2][:40])
    start = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x154800
    end = int(sys.argv[4], 16) if len(sys.argv) > 4 else 0x15A900
    o = start
    while o + 0x74 <= min(end, len(data)):
        if struct.unpack_from(">I", data, o + 0x1C)[0] != 0x05F5E0FF:
            o += 4
            continue
        f = lambda k: struct.unpack_from(">I", data, o + k)[0]
        label = f(0x18)
        print(f"@{o:#x} page={f(0x30):3} id={f(0x34):3} parent={f(0x50):3} "
              f"kind={f(0x54)} flags={f(0x58):08x} link={f(0x6C):#5x} {label} {names.get(label, '')}")
        o += 0x74


if __name__ == "__main__":
    main()
