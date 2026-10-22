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


def store(data: bytes, block: int = 2000) -> bytes:
    """Bloc « BPE » sans compression : chaque sous-bloc a une table de paires vide (FF 80 FE : tous les
    octets sont littéraux) suivie des octets bruts. Taille des sous-blocs gardée sous celle des fichiers
    du jeu (2 028 au plus dans bg15), au cas où le décodeur du jeu aurait un tampon fixe."""
    body = bytearray()
    for i in range(0, len(data), block):
        part = data[i:i + block]
        body += b"\xff\x80\xfe" + struct.pack("<H", len(part)) + part
    return MAGIC + struct.pack("<III", 0x100, len(body), len(data)) + bytes(body)


def _table(left: list[int], right: list[int]) -> bytes:
    """Table de paires au format lu par expand() : octet > 127 = sauter (n - 127) codes littéraux puis lire
    une entrée ; octet n <= 127 = lire n + 1 entrées (code littéral : 1 octet, paire : gauche + droite)."""
    out = bytearray()
    c = 0
    while c < 256:
        run = 0
        while c + run < 256 and left[c + run] == c + run and run < 128:
            run += 1
        if run:
            out.append(127 + run)
            c += run
            if c == 256:
                break
            out.append(left[c])
            if left[c] != c:
                out.append(right[c])
            c += 1
            continue
        n = 0
        while c + n < 256 and n < 128 and not (left[c + n] == c + n and c + n + 1 < 256
                                               and left[c + n + 1] == c + n + 1):
            n += 1
        out.append(n - 1)
        for k in range(c, c + n):
            out.append(left[k])
            if left[k] != k:
                out.append(right[k])
        c += n
    return bytes(out)


def compress(data: bytes, block: int = 4000, threshold: int = 3) -> bytes:
    """Compression BPE de Gage, sous-blocs de 4 000 octets comme les fichiers du jeu."""
    from collections import Counter
    body = bytearray()
    for i in range(0, len(data), block):
        buf = bytes(data[i:i + block])
        left = list(range(256))
        right = [0] * 256
        used = set(buf)
        free = [c for c in range(256) if c not in used]
        while free and len(buf) > 1:
            pairs = Counter(zip(buf, buf[1:]))
            (a, b), count = pairs.most_common(1)[0]
            if count < threshold:
                break
            code = free.pop()
            buf = buf.replace(bytes((a, b)), bytes((code,)))
            left[code], right[code] = a, b
        body += _table(left, right) + struct.pack("<H", len(buf)) + buf
    return MAGIC + struct.pack("<III", 0x100, len(body), len(data)) + bytes(body)
