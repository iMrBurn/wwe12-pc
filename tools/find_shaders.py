#!/usr/bin/env python3
"""Cherche les conteneurs de shaders Xbox 360 (format compilé de Microsoft, « .xsc »)
dans des fichiers bruts : données du disque, image du XEX, dumps mémoire.

En-tête d'un conteneur (big-endian) :
  +0x00  flags      0x102A11xx (bit 0 : 1 = vertex shader, 0 = pixel shader)
  +0x04  taille de la partie « vertex »   } la somme = taille totale du conteneur
  +0x08  taille de la partie « pixel »    }
  +0x1C  0
  +0x20  0

    python tools/find_shaders.py port/assets image.bin            # inventaire
    python tools/find_shaders.py port/assets --extract shaders/xsc  # extraction dédupliquée

Les fichiers extraits sont nommés <sha1 sur 16 caractères>.<vs|ps>.xsc.
"""
from __future__ import annotations

import argparse
import hashlib
import os
import struct
import sys
from collections import Counter
from pathlib import Path

MAGIC_PREFIX = b"\x10\x2A\x11"
HEADER_SIZE = 0x24
MAX_SIZE = 0x100000


def scan(data: bytes):
    """Renvoie [(offset, taille, est_vertex)] des conteneurs plausibles."""
    hits = []
    i = data.find(MAGIC_PREFIX)
    while i != -1:
        if i % 4 == 0 and i + HEADER_SIZE <= len(data):
            flags, part_a, part_b = struct.unpack_from(">III", data, i)
            zero1, zero2 = struct.unpack_from(">II", data, i + 0x1C)
            size = part_a + part_b
            if zero1 == 0 and zero2 == 0 and HEADER_SIZE < size <= min(len(data) - i, MAX_SIZE):
                hits.append((i, size, bool(flags & 1)))
        i = data.find(MAGIC_PREFIX, i + 1)
    return hits


def iter_payloads(data: bytes, depth: int = 0):
    """`data` puis le contenu de ses blocs BPE de Yuke's, récursivement (archives imbriquées)."""
    yield data
    if depth >= 4:
        return
    import yukes_bpe
    for _, blob in yukes_bpe.iter_blocks(data):
        yield from iter_payloads(blob, depth + 1)


def iter_files(paths):
    for p in paths:
        p = Path(p)
        if p.is_dir():
            for root, _, files in os.walk(p):
                for f in files:
                    yield Path(root) / f
        elif p.is_file():
            yield p


def main() -> None:
    for stream in (sys.stdout, sys.stderr):
        stream.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("paths", nargs="+", help="fichiers ou dossiers à parcourir")
    ap.add_argument("--extract", type=Path, help="dossier où écrire les conteneurs uniques")
    ap.add_argument("--bpe", action="store_true",
                    help="décompresser aussi les blocs « BPE » des archives de Yuke's")
    args = ap.parse_args()
    sys.path.insert(0, str(Path(__file__).parent))

    if args.extract:
        args.extract.mkdir(parents=True, exist_ok=True)
    seen: set[str] = set()
    kinds: Counter = Counter()
    total = 0
    for path in iter_files(args.paths):
        try:
            data = path.read_bytes()
        except OSError as e:
            print(f"illisible : {path} ({e})", file=sys.stderr)
            continue
        payloads = iter_payloads(data) if args.bpe else [data]
        hits = [(payload, off, size, is_vs) for payload in payloads
                for off, size, is_vs in scan(payload)]
        if not hits:
            continue
        total += len(hits)
        new = 0
        for payload, off, size, is_vs in hits:
            blob = payload[off: off + size]
            digest = hashlib.sha1(blob).hexdigest()[:16]
            if digest in seen:
                continue
            seen.add(digest)
            new += 1
            kinds["vs" if is_vs else "ps"] += 1
            if args.extract:
                (args.extract / f"{digest}.{'vs' if is_vs else 'ps'}.xsc").write_bytes(blob)
        print(f"{len(hits):6} conteneurs ({new} nouveaux)  {path}", flush=True)
    print(f"total : {total} conteneurs, {len(seen)} uniques "
          f"({kinds['vs']} vertex, {kinds['ps']} pixel)")


if __name__ == "__main__":
    main()
