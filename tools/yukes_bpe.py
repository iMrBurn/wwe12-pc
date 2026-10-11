"""Décompression des blocs « BPE » des archives EPAC/EPK8 de Yuke's (WWE '12).

En-tête (little-endian), puis données au format « Byte Pair Encoding » de Philip Gage
(C Users Journal, 1994) : une suite de sous-blocs « table de paires + données ».

  +0x00  "BPE "
  +0x04  u32  version (0x100)
  +0x08  u32  taille compressée (sans l'en-tête)
  +0x0C  u32  taille décompressée
  +0x10  données
"""
from __future__ import annotations

import struct

MAGIC = b"BPE "
HEADER_SIZE = 0x10


def parse_header(data: bytes, offset: int = 0) -> tuple[int, int] | None:
    """(taille compressée, taille décompressée) si un en-tête BPE plausible est à offset."""
    if data[offset: offset + 4] != MAGIC or offset + HEADER_SIZE > len(data):
        return None
    version, packed, unpacked = struct.unpack_from("<III", data, offset + 4)
    if version != 0x100 or packed == 0 or unpacked == 0 or unpacked > 0x4000000:
        return None
    if offset + HEADER_SIZE + packed > len(data):
        return None
    return packed, unpacked


def expand(src: bytes, expected: int, size_big_endian: bool = True) -> bytes:
    """Décode une suite de sous-blocs BPE de Gage jusqu'à produire `expected` octets."""
    out = bytearray()
    pos = 0
    n = len(src)
    left = list(range(256))
    right = [0] * 256
    while pos < n and len(out) < expected:
        # Table de paires : left[c] == c signifie « c est un octet littéral ».
        for c in range(256):
            left[c] = c
        c = 0
        while True:
            count = src[pos]; pos += 1
            if count > 127:
                c += count - 127
                count = 0
            if c == 256:
                break
            for _ in range(count + 1):
                left[c] = src[pos]; pos += 1
                if left[c] != c:
                    right[c] = src[pos]; pos += 1
                c += 1
            if c == 256:
                break
        if size_big_endian:
            size = (src[pos] << 8) | src[pos + 1]
        else:
            size = src[pos] | (src[pos + 1] << 8)
        pos += 2
        # Données : chaque octet se développe récursivement via une pile.
        # Garde-fous : une table incohérente (mauvaise hypothèse de format) peut
        # boucler à l'infini ou produire bien plus que prévu.
        stack = []
        for _ in range(size):
            stack.append(src[pos]); pos += 1
            while stack:
                b = stack.pop()
                if left[b] == b:
                    out.append(b)
                    if len(out) > expected:
                        raise ValueError("sortie plus grande que prévu")
                else:
                    if len(stack) > 512:
                        raise ValueError("table de paires cyclique")
                    stack.append(right[b])
                    stack.append(left[b])
    return bytes(out)


def decompress_at(data: bytes, offset: int) -> bytes | None:
    hdr = parse_header(data, offset)
    if hdr is None:
        return None
    packed, unpacked = hdr
    src = data[offset + HEADER_SIZE: offset + HEADER_SIZE + packed]
    for big_endian in (False, True):  # WWE '12 : taille des sous-blocs en little-endian
        try:
            out = expand(src, unpacked, big_endian)
        except (IndexError, ValueError):
            continue
        if len(out) == unpacked:
            return out
    return None


def iter_blocks(data: bytes):
    """Renvoie (offset, données décompressées) pour chaque bloc BPE valide de `data`."""
    i = data.find(MAGIC)
    while i != -1:
        out = decompress_at(data, i)
        if out is not None:
            yield i, out
        i = data.find(MAGIC, i + 4)
