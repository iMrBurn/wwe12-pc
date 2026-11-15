"""Packs WWE '13's I Quit motions and HUD images into port/res/iquit13.bin (embedded in wwe12.exe, resource 102).

Usage: python -I tools/pack_iquit13.py <motions mod folder> [out]

The folder is a motions mod made from the user's own WWE '13 (mods/.svrmods/Motions/iquit_wwe13_all): <number>.high
and <number>.m3 (WWE '13's 132NN as 131(NN+50)) and hud/<en|fr|de|it|es>/<name>.dds (its game2dHD.pac, G?2D child 16).
THE OUTPUT IS WWE '13 GAME DATA. It is in the repository and in the exe by the user's own decision of 2026-11-15
(he takes the DMCA risk, as for the PS3 button icons); nothing else of a game goes there.

Format, little-endian: "IQ13", u32 count, then per file: u16 name length, name (forward slashes), u32 size, bytes.
"""
import struct
import sys
from pathlib import Path


def main():
    src = Path(sys.argv[1])
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else Path(__file__).resolve().parents[1] / "port" / "res" / "iquit13.bin"
    files = sorted(p for p in src.rglob("*") if p.is_file() and p.suffix in (".high", ".m3", ".dds"))
    blob = bytearray(b"IQ13" + struct.pack("<I", len(files)))
    for p in files:
        name = p.relative_to(src).as_posix().encode()
        data = p.read_bytes()
        blob += struct.pack("<H", len(name)) + name + struct.pack("<I", len(data)) + data
    out.write_bytes(blob)
    print(f"{len(files)} files, {len(blob)} bytes -> {out}")


if __name__ == "__main__":
    main()
