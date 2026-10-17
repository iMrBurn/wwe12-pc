"""Builds port/res/ps3_buttons.bin: the PlayStation button icons of WWE '12 (PS3), to show in place
of the Xbox ones (native/wwe12_buttons.cpp). The user decided to ship the real PS3 icons (2026-10-17).

usage: python tools/extract_ps3_buttons.py <Xbox game folder with pac\\> <PS3 USRDIR folder with pac\\>

Both versions store their textures as DDS (DXT5, level 0 only) in BPE blocks (tools/yukes_bpe.py),
in the same order. Two kinds of differences hold the buttons:
  - the fonts (menu\\fontHD.pac, fontSD.pac): one icon sheet per font, 512x512 or 256x512, same layout
    on both consoles for the button cells (all rows above y = 256, plus the L3 cell at x 64-127,
    y 256-303); below, THQ and the F/R logos sit elsewhere on PS3, so only the button cells are
    copied, as DXT5 blocks, and the fonts' glyph tables stay valid; four PS3 icons are wider than
    their Xbox glyph rectangles (SELECT, START, L2, R2: the game converts its glyph tables at load,
    they could not be widened), so they are narrowed to the Xbox width and re-encoded (Pillow);
  - the in-match prompts (game2dHD.pac, game2dSD.pac): small textures replaced whole, paired by
    their position in the file (the identical textures around them anchor the pairing).

File format (little-endian): "W12B", u32 version (1), u32 record count, then per record:
  u32 width, u32 height, u64 FNV-1a of the Xbox texture's level 0, u32 kind (0 = whole level 0,
  1 = blocks), u32 payload size, payload (kind 0: the PS3 level 0; kind 1: u32 block index + 16 bytes,
  repeated).
"""
from __future__ import annotations

import collections
import difflib
import io
import os
import struct
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(__file__))
import yukes_bpe  # noqa: E402

FONT_PACS = ("menu/fontHD.pac", "menu/fontSD.pac")
PROMPT_PACS = ("game2dHD.pac", "game2dSD.pac")
ATLAS_SIZES = ((512, 512), (256, 512))
NOT_BUTTONS = ((512, 128),)  # the BREAKINGPOINT logo also differs; it is not a button
# Icon glyphs wider on PS3 than their Xbox rectangle (same in every font): x, y, Xbox width,
# PS3 width, height. U+E000 Back/SELECT, U+E001 Start/START, U+E00C LT/L2, U+E00F RT/R2.
WIDE_GLYPHS = ((152, 220, 48, 92, 40), (152, 176, 48, 104, 40), (0, 220, 34, 56, 40),
               (76, 220, 34, 56, 40))


def fnv1a64(data: bytes) -> int:
    h = 0xCBF29CE484222325
    for b in data:
        h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def dds_header(width: int, height: int) -> bytes:
    """A DDS header for a DXT5 texture without mipmaps."""
    header = bytearray(128)
    header[0:4] = b"DDS "
    struct.pack_into("<IIIII", header, 4, 124, 0x81007, height, width, width * height)
    struct.pack_into("<II", header, 76, 32, 4)
    header[84:88] = b"DXT5"
    struct.pack_into("<I", header, 108, 0x1000)
    return bytes(header)


def decode(width: int, height: int, level0: bytes) -> Image.Image:
    return Image.open(io.BytesIO(dds_header(width, height) + level0)).convert("RGBA")


def encode(image: Image.Image) -> bytes:
    out = io.BytesIO()
    image.save(out, "DDS", pixel_format="DXT5")
    return out.getvalue()[128:]


def narrow_wide_glyphs(width: int, height: int, sheet: bytearray, ps3: bytes) -> None:
    """Narrows the PS3 icons wider than their Xbox rectangle into it, in the sheet (in place)."""
    ps3_image = decode(width, height, ps3)
    current = decode(width, height, bytes(sheet))
    for x, y, xbox_w, ps3_w, h in WIDE_GLYPHS:
        icon = ps3_image.crop((x, y, x + ps3_w, y + h)).resize((xbox_w, h), Image.LANCZOS)
        area = current.crop((x, y, x + ps3_w, y + h))
        area.paste((0, 0, 0, 0), (0, 0, ps3_w, h))
        area.paste(icon, (0, 0))
        blocks = encode(area)  # (ps3_w / 4) x (h / 4) blocks, row by row
        per_row = ps3_w // 4
        for row in range(h // 4):
            for col in range(per_row):
                block = ((y // 4 + row) * (width // 4)) + x // 4 + col
                src = (row * per_row + col) * 16
                sheet[block * 16:block * 16 + 16] = blocks[src:src + 16]


def textures(path: str) -> list[tuple[int, int, bytes]]:
    """(width, height, level 0) of every DXT5 DDS in the BPE blocks of a pac, in file order."""
    data = open(path, "rb").read()
    out = []
    for _, blob in yukes_bpe.iter_blocks(data):
        i = blob.find(b"DDS ")
        while i != -1:
            header = blob[i:i + 128]
            if len(header) == 128 and header[84:88] == b"DXT5":
                height, width = struct.unpack_from("<II", header, 12)
                level0 = blob[i + 128:i + 128 + width * height]  # DXT5: 1 byte per pixel
                if len(level0) == width * height:
                    out.append((width, height, level0))
            i = blob.find(b"DDS ", i + 4)
    return out


def atlas_records(xbox: list, ps3: list) -> dict[int, bytes]:
    records = {}
    for size in ATLAS_SIZES:
        xs = {t[2] for t in xbox if t[:2] == size}
        ps = {t[2] for t in ps3 if t[:2] == size}
        only_x, only_p = list(xs - ps), list(ps - xs)
        if len(only_x) != 1 or len(only_p) != 1:
            raise SystemExit(f"icon sheet {size}: {len(only_x)} Xbox / {len(only_p)} PS3 candidates")
        x, p = only_x[0], only_p[0]
        width = size[0]
        sheet = bytearray(x)
        for block in range(len(x) // 16):
            bx, by = (block % (width // 4)) * 4, (block // (width // 4)) * 4
            if by < 256 or (64 <= bx < 128 and by < 304):
                sheet[block * 16:block * 16 + 16] = p[block * 16:block * 16 + 16]
        narrow_wide_glyphs(*size, sheet, p)
        payload = bytearray()
        for block in range(len(x) // 16):
            if sheet[block * 16:block * 16 + 16] != x[block * 16:block * 16 + 16]:
                payload += struct.pack("<I", block) + sheet[block * 16:block * 16 + 16]
        records[fnv1a64(x)] = struct.pack("<IIQII", *size, fnv1a64(x), 1, len(payload)) + payload
    return records


def prompt_records(xbox: list, ps3: list) -> dict[int, bytes]:
    votes: dict[bytes, collections.Counter] = collections.defaultdict(collections.Counter)
    matcher = difflib.SequenceMatcher(a=[t[2] for t in xbox], b=[t[2] for t in ps3], autojunk=False)
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag != "replace":
            continue
        xs = [t for t in xbox[i1:i2] if t[:2] not in NOT_BUTTONS]
        ps = [t for t in ps3[j1:j2] if t[:2] not in NOT_BUTTONS]
        # Pair same-size textures in order; leftovers (a texture only one version has) are skipped.
        by_size_p = collections.defaultdict(list)
        for t in ps:
            by_size_p[t[:2]].append(t)
        for t in xs:
            if by_size_p[t[:2]]:
                votes[t[2]][by_size_p[t[:2]].pop(0)[2]] += 1
    records = {}
    for x, counter in votes.items():
        p = counter.most_common(1)[0][0]
        size = next(t[:2] for t in xbox if t[2] == x)
        records[fnv1a64(x)] = struct.pack("<IIQII", *size, fnv1a64(x), 0, len(p)) + p
    return records


def main() -> None:
    xbox_root, ps3_root = sys.argv[1], sys.argv[2]
    out_path = os.path.join(os.path.dirname(__file__), "..", "port", "res", "ps3_buttons.bin")
    records: dict[int, bytes] = {}
    for rel in FONT_PACS:
        records.update(atlas_records(textures(os.path.join(xbox_root, "pac", rel)),
                                     textures(os.path.join(ps3_root, "pac", rel))))
    for rel in PROMPT_PACS:
        records.update(prompt_records(textures(os.path.join(xbox_root, "pac", rel)),
                                      textures(os.path.join(ps3_root, "pac", rel))))
    with open(out_path, "wb") as f:
        f.write(b"W12B" + struct.pack("<II", 1, len(records)))
        for key in sorted(records):
            f.write(records[key])
    print(f"{len(records)} textures, {os.path.getsize(out_path)} bytes -> {os.path.normpath(out_path)}")


if __name__ == "__main__":
    main()
