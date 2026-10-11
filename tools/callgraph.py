#!/usr/bin/env python3
"""Graphe d'appels du code PowerPC de default.xex (appels directs bl), pour identifier des
fonctions par leurs relations : qui appelle qui, dans quel ordre, avec quels imports.

    python tools/callgraph.py image.bin port/generated/default/wwe12_register.cpp 0x82AA8898
    python tools/callgraph.py image.bin port/generated/default/wwe12_register.cpp 0x82AA8898 --callers

Les bornes des fonctions viennent des débuts enregistrés par ReXGlue (+ .pdata) : une
fonction va de son début au début suivant. Les imports du noyau sont nommés d'après
port/generated/default/wwe12_init.cpp.
"""
from __future__ import annotations

import argparse
import bisect
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import find_fptrs  # noqa: E402


class CallGraph:
    def __init__(self, image_path: Path, register_cpp: Path):
        self.img = find_fptrs.parse_pe(image_path.read_bytes(), None)
        text = register_cpp.read_text(encoding="utf-8", errors="replace")
        self.starts = sorted(set(int(m, 16) for m in re.findall(r"SetFunction\((0x[0-9A-F]+)", text))
                             | {b for b, _ in self.img.pdata})
        init = register_cpp.with_name(register_cpp.name.replace("_register.cpp", "_init.cpp"))
        self.imports = {int(a, 16): n for a, n in re.findall(
            r"\{ (0x[0-9A-F]+), __imp__(\w+) \}", init.read_text(encoding="utf-8", errors="replace"))
            if not n.startswith("sub_")}
        self.calls: dict[int, list[tuple[int, int]]] = defaultdict(list)    # fn -> [(site, cible)]
        self.callers: dict[int, list[tuple[int, int]]] = defaultdict(list)  # cible -> [(site, fn)]
        for k, lo in enumerate(self.starts):
            hi = self.starts[k + 1] if k + 1 < len(self.starts) else lo + 4
            if hi - lo > 0x20000 or self.img.code_section(lo) is None:
                continue
            for site in range(lo, hi, 4):
                w = self.img.word(site)
                if w is None or w >> 26 != 18 or not w & 1 or w & 2:  # bl seulement
                    continue
                tgt = site + find_fptrs.sext(w & 0x03FFFFFC, 26)
                self.calls[lo].append((site, tgt))
                self.callers[tgt].append((site, lo))

    def function_of(self, addr: int) -> int:
        return self.starts[bisect.bisect_right(self.starts, addr) - 1]

    def end_of(self, fn: int) -> int:
        i = bisect.bisect_right(self.starts, fn)
        return self.starts[i] if i < len(self.starts) else fn + 4

    def name(self, addr: int) -> str:
        return self.imports.get(addr, f"sub_{addr:08X}")


def main() -> None:
    for stream in (sys.stdout, sys.stderr):
        stream.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image", type=Path)
    ap.add_argument("register_cpp", type=Path)
    ap.add_argument("functions", nargs="+", help="adresses (0x…)")
    ap.add_argument("--callers", action="store_true", help="afficher les appelants")
    args = ap.parse_args()
    g = CallGraph(args.image, args.register_cpp)
    for a in args.functions:
        fn = g.function_of(int(a, 0))
        print(f"== {g.name(fn)}  ({fn:08X}-{g.end_of(fn):08X}, {g.end_of(fn) - fn} octets)")
        if args.callers:
            for site, caller in g.callers.get(fn, []):
                print(f"   appelée depuis {site:08X} dans {g.name(caller)}")
        else:
            for site, tgt in g.calls.get(fn, []):
                print(f"   {site:08X}: bl {g.name(tgt)}")


if __name__ == "__main__":
    main()
