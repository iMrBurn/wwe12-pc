#!/usr/bin/env python3
"""Trouve les fonctions que le scanner de ReXGlue risque de rater dans un default.xex.

Catégories recherchées (toutes situées hors des fonctions déclarées dans .pdata) :
  * pointeurs en données : vtables, callbacks, entrées de threads (.rdata/.data) ;
  * adresses construites dans le code : lis rX,hi + addi/ori rY,rX,lo ;
  * fonctions « trous » atteintes seulement par un saut b/bl (tail-calls).
Bonus : candidats setjmp/longjmp (sauvegarde/restauration de f14-f31 via r3).

Entrées acceptées :
  * XEX2 non compressé ou en compression « basic », chiffré ou non.
    Le déchiffrement AES demande `pip install cryptography` (ou pycryptodome).
  * Image PE déjà extraite (« basefile », disposition mémoire), commençant par MZ.
Les XEX en compression « normal » (LZX) ne sont pas gérés : décompresse-les d'abord,
par ex. avec XexTool (`xextool -c u -e u -o default_u.xex default.xex`).

Sortie : lignes TOML à coller dans la table [functions] de port/wwe12_config.toml.
"""
from __future__ import annotations

import argparse
import bisect
import struct
import sys
import tomllib
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path

# Clés AES publiques des XEX2 (identiques à celles de Xenia).
RETAIL_KEY = bytes.fromhex("20B185A59D28FDC340583FBB0896BF91")
DEVKIT_KEY = bytes(16)

OPT_FILE_FORMAT = 0x000003FF
OPT_IMAGE_BASE = 0x00010201

INSN_BLR = 0x4E800020
INSN_BCTR = 0x4E800420
SCN_CNT_CODE = 0x00000020
SCN_MEM_EXECUTE = 0x20000000


# --------------------------------------------------------------------------- XEX2

def _aes_decrypt(key: bytes, data: bytes, cbc: bool) -> bytes:
    try:
        from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
        mode = modes.CBC(bytes(16)) if cbc else modes.ECB()
        d = Cipher(algorithms.AES(key), mode).decryptor()
        return d.update(data) + d.finalize()
    except ImportError:
        pass
    try:
        from Crypto.Cipher import AES
    except ImportError:
        sys.exit("XEX chiffré : installe `cryptography` (pip install cryptography)")
    c = AES.new(key, AES.MODE_CBC, iv=bytes(16)) if cbc else AES.new(key, AES.MODE_ECB)
    return c.decrypt(data)


def load_xex(raw: bytes) -> tuple[bytes, int]:
    """Renvoie (image PE en disposition mémoire, adresse de chargement)."""
    _magic, _flags, pe_off, _res, sec_off, n_opt = struct.unpack_from(">4sIIIII", raw, 0)
    opts = {}
    for i in range(n_opt):
        key, value = struct.unpack_from(">II", raw, 24 + i * 8)
        opts[key] = value

    image_size = struct.unpack_from(">I", raw, sec_off + 0x4)[0]
    load_addr = struct.unpack_from(">I", raw, sec_off + 0x110)[0]
    file_key = raw[sec_off + 0x150: sec_off + 0x160]
    if OPT_IMAGE_BASE in opts:
        load_addr = opts[OPT_IMAGE_BASE]

    if OPT_FILE_FORMAT not in opts:
        sys.exit("XEX sans en-tête de format de fichier (0x3FF)")
    ff = opts[OPT_FILE_FORMAT]
    info_size, enc_type, comp_type = struct.unpack_from(">IHH", raw, ff)

    payload = raw[pe_off:]
    if comp_type == 2:
        sys.exit("XEX en compression LZX : décompresse-le d'abord (voir l'aide du script)")
    if comp_type not in (0, 1):
        sys.exit(f"type de compression XEX non géré : {comp_type}")

    candidates = [payload]
    if enc_type == 1:
        payload = payload[: len(payload) // 16 * 16]
        candidates = [
            _aes_decrypt(_aes_decrypt(k, file_key, cbc=False), payload, cbc=True)
            for k in (RETAIL_KEY, DEVKIT_KEY)
        ]

    for data in candidates:
        if comp_type == 0:
            image = data[:image_size]
        else:
            parts, pos = [], 0
            for b in range((info_size - 8) // 8):
                size, zeros = struct.unpack_from(">II", raw, ff + 8 + b * 8)
                parts += [data[pos: pos + size], bytes(zeros)]
                pos += size
            image = b"".join(parts)
        if image[:2] == b"MZ":
            return image.ljust(image_size, b"\0"), load_addr
    sys.exit("échec du déchiffrement : l'image ne commence pas par MZ")


# --------------------------------------------------------------------------- PE

@dataclass
class Section:
    name: str
    start: int  # adresse virtuelle absolue
    data: bytes
    executable: bool

    @property
    def end(self) -> int:
        return self.start + len(self.data)


@dataclass
class Image:
    base: int
    sections: list[Section]
    pdata: list[tuple[int, int]]  # (début, fin) triés

    def code_section(self, addr: int) -> Section | None:
        for s in self.sections:
            if s.executable and s.start <= addr < s.end:
                return s
        return None

    def word(self, addr: int) -> int | None:
        for s in self.sections:
            if s.start <= addr and addr + 4 <= s.end:
                return struct.unpack_from(">I", s.data, addr - s.start)[0]
        return None


def parse_pe(image: bytes, base: int | None) -> Image:
    pe = struct.unpack_from("<I", image, 0x3C)[0]
    if image[pe: pe + 4] != b"PE\0\0":
        sys.exit("signature PE introuvable")
    n_sections, = struct.unpack_from("<H", image, pe + 6)
    opt_size, = struct.unpack_from("<H", image, pe + 20)
    opt = pe + 24
    if base is None:
        base = struct.unpack_from("<I", image, opt + 28)[0]

    sections = []
    for i in range(n_sections):
        sh = opt + opt_size + i * 40
        name = image[sh: sh + 8].rstrip(b"\0").decode("ascii", "replace")
        vsize, vaddr, raw_size = struct.unpack_from("<III", image, sh + 8)
        chars, = struct.unpack_from("<I", image, sh + 36)
        size = max(vsize, raw_size)
        data = image[vaddr: vaddr + size]
        sections.append(Section(name, base + vaddr, data,
                                bool(chars & (SCN_MEM_EXECUTE | SCN_CNT_CODE))))

    pdata = []
    for s in sections:
        if s.name == ".pdata":
            for begin, info in struct.iter_unpack(">II", s.data[: len(s.data) // 8 * 8]):
                length = ((info >> 8) & 0x3FFFFF) * 4
                if begin and length:
                    pdata.append((begin, begin + length))
    pdata.sort()
    return Image(base, sections, pdata)


# --------------------------------------------------------------------------- analyse

def sext(value: int, bits: int) -> int:
    return value - (1 << bits) if value & (1 << (bits - 1)) else value


class Ranges:
    """Recherche rapide « adresse dans une fonction .pdata ? »."""

    def __init__(self, pdata: list[tuple[int, int]]):
        self.starts = [b for b, _ in pdata]
        self.ends = [e for _, e in pdata]
        self.start_set = set(self.starts)

    def containing(self, addr: int) -> tuple[int, int] | None:
        i = bisect.bisect_right(self.starts, addr) - 1
        if i >= 0 and addr < self.ends[i]:
            return self.starts[i], self.ends[i]
        return None


def is_import_thunk(img: Image, addr: int) -> bool:
    w = [img.word(addr + 4 * i) for i in range(4)]
    return (None not in w and w[0] >> 16 == 0x3D60 and w[1] >> 16 == 0x816B
            and w[2] == 0x7D6903A6 and w[3] == INSN_BCTR)


def looks_like_start(img: Image, addr: int) -> bool:
    sec = img.code_section(addr)
    if sec is None or addr % 4:
        return False
    insn = img.word(addr)
    if not insn:
        return False
    if addr == sec.start:
        return True
    prev = img.word(addr - 4)
    # précédé d'un retour, d'un saut inconditionnel ou de remplissage
    return prev in (0, INSN_BLR, INSN_BCTR) or (prev >> 26 == 18 and prev & 1 == 0)


def accept(img: Image, ranges: Ranges, target: int, loose: bool) -> bool:
    if img.code_section(target) is None or target % 4:
        return False
    if target in ranges.start_set or ranges.containing(target):
        return False  # déjà connu, ou étiquette interne (table de saut…)
    if is_import_thunk(img, target):
        return False
    return loose or looks_like_start(img, target)


def scan_data_pointers(img, ranges, loose):
    refs = defaultdict(list)
    for s in img.sections:
        if s.executable or s.name in (".pdata", ".reloc"):
            continue
        n = len(s.data) // 4
        for i, (v,) in enumerate(struct.iter_unpack(">I", s.data[: n * 4])):
            if accept(img, ranges, v, loose):
                refs[v].append(s.start + i * 4)
    return refs


def scan_code(img, ranges, loose):
    built = defaultdict(list)    # lis + addi/ori
    branched = defaultdict(list)  # b (tail-call) vers un trou
    for s in img.sections:
        if not s.executable:
            continue
        words = [w for (w,) in struct.iter_unpack(">I", s.data[: len(s.data) // 4 * 4])]
        for i, insn in enumerate(words):
            addr = s.start + i * 4
            op = insn >> 26
            if op == 18:
                # b seulement : ReXGlue suit déjà les bl (sur WWE '12, ~11 000 cibles bl
                # hors .pdata, toutes trouvées seules), mais pas les tail-calls.
                if insn & 3:  # AA ou LK
                    continue
                target = addr + sext(insn & 0x03FFFFFC, 26)
                if accept(img, ranges, target, True):
                    branched[target].append(addr)
        scan_built(words, s.start, img, ranges, loose, built)
    return built, branched


# Formes D qui écrivent dans le champ rD (bits 21-25) : addi, addis, mulli, subfic, addic,
# lwz, lbz, lhz, lha, ld (et leurs formes « update », qui écrivent aussi rA).
_WRITES_RD = {7, 8, 12, 13, 14, 15, 32, 33, 34, 35, 40, 41, 42, 43, 58}
_UPDATE_FORMS = {33, 35, 37, 39, 41, 43, 45, 49, 51, 53, 55}
# Formes qui écrivent dans rA : ori, oris, xori, xoris, andi., andis., rotations.
_WRITES_RA = {20, 21, 23, 24, 25, 26, 27, 28, 29, 30}
# Magasins X-form (op 31) : n'écrivent aucun GPR (hors formes « update »).
_STORE_X = {149, 151, 215, 407, 663, 695, 727, 759, 918, 662, 150, 214, 983}
_VOLATILE = set(range(0, 13))  # r0, r3-r12 (et r1/r2, sans importance ici)


def scan_built(words, base, img, ranges, loose, built):
    """Suit les `lis rX, hi` dans chaque bloc et capte les `addi/ori rY, rX, lo` qui
    suivent, même loin (sur WWE '12 : lis r30 … plusieurs lis/stw/bl … addi r9, r30, lo).
    Les registres non volatils (r13-r31) survivent aux appels bl."""
    his: dict[int, int] = {}
    for i, insn in enumerate(words):
        op = insn >> 26
        rd, ra = (insn >> 21) & 31, (insn >> 16) & 31
        # 1. utilisation d'un registre chargé par lis
        target = None
        if op == 14 and ra in his:                     # addi rY, rX, lo
            target = (his[ra] + sext(insn & 0xFFFF, 16)) & 0xFFFFFFFF
        elif op == 24 and rd in his:                   # ori rY, rX, lo
            target = his[rd] | (insn & 0xFFFF)
        if target is not None and accept(img, ranges, target, loose):
            built[target].append(base + i * 4)
        # 2. mise à jour de l'état des registres
        if op == 15 and ra == 0:                       # lis rD, hi
            his[rd] = (insn & 0xFFFF) << 16
            continue
        if op == 18:                                   # b / bl
            his = {r: v for r, v in his.items() if r not in _VOLATILE} if insn & 1 else {}
            continue
        if op == 19 and not insn & 1 and ((insn >> 1) & 0x3FF) in (16, 528):
            his = {}                                   # blr / bctr : fin du flot
            continue
        if op in (16, 19):                             # bc, bcctrl… : on garde l'état
            continue
        if op in _WRITES_RD:
            his.pop(rd, None)
        if op in _WRITES_RA:
            his.pop(ra, None)
        if op in _UPDATE_FORMS:
            his.pop(ra, None)
        if op == 31:
            xo = (insn >> 1) & 0x3FF
            if xo not in _STORE_X and xo not in (144, 467):  # mtcrf, mtspr
                his.pop(rd, None)
                his.pop(ra, None)


def crossing_branch(img: Image, starts: list[int], addr: int) -> int | None:
    """Renvoie l'adresse d'un saut b/bc qui traverse `addr` à l'intérieur de la fonction
    connue qui l'entoure, ou None. Un tel saut prouve que `addr` est une étiquette interne :
    la déclarer comme fonction couperait la vraie fonction en deux (« Unresolved branch »)."""
    i = bisect.bisect_left(starts, addr)
    lo = starts[i - 1] if i > 0 else addr
    j = i + 1 if i < len(starts) and starts[i] == addr else i
    hi = starts[j] if j < len(starts) else addr + 0x1000
    for src in range(lo, hi, 4):
        w = img.word(src)
        if w is None:
            continue
        op = w >> 26
        if op == 18 and not w & 3:
            tgt = src + sext(w & 0x03FFFFFC, 26)
        elif op == 16 and not w & 2:
            tgt = src + sext(w & 0xFFFC, 16)
        else:
            continue
        k = bisect.bisect_left(starts, tgt)
        if k < len(starts) and starts[k] == tgt:
            continue  # tail-call vers le début d'une fonction connue, pas un saut interne
        if lo <= tgt < hi and tgt != addr and min(src, tgt) < addr <= max(src, tgt):
            return src
    return None


def _ctr_source(img: Image, bctr: int) -> int | None:
    """Instruction qui a calculé la valeur passée à mtctr avant le bctr à `bctr`."""
    for c in range(bctr - 4, bctr - 40, -4):
        w = img.word(c)
        if w is None:
            return None
        if w >> 26 == 31 and (w >> 1) & 0x3FF == 467 and (w >> 11) & 0x3FF == 0x120:  # mtctr
            r = (w >> 21) & 31
            for d in range(c - 4, c - 48, -4):
                x = img.word(d)
                if x is not None and (x >> 21) & 31 == r and x >> 26 not in (36, 38, 44):
                    return x
            return None
    return None


def _is_switch_bctr(img: Image, bctr: int) -> bool:
    src = _ctr_source(img, bctr)
    return src is not None and src >> 26 == 31 and (src >> 1) & 0x3FF in (23, 266)  # lwzx/add


def find_merged(img: Image, starts: list[int]) -> list[int]:
    """Fonctions que ReXGlue a fusionnées avec celle qui les précède : du code juste après
    un blr/bctr (donc inatteignable depuis la fonction précédente) qu'aucun saut de la
    région ne vise ni ne traverse. Cas typique sur WWE '12 : des séries de thunks de
    vtable de 4-6 instructions (lwz / mtctr / bctr) appelées par pointeur.
    Exclus : les régions .pdata (bornes exactes), le code après un bctr de switch, toute
    région contenant un switch (ses « case » peuvent suivre un blr), et les mots qui
    ressemblent à une adresse (tables d'exceptions placées après le code)."""
    pdata_starts = {b for b, _ in img.pdata}
    lo_img = img.base
    hi_img = max(s.end for s in img.sections)
    out = []
    for k, lo in enumerate(starts[:-1]):
        hi = starts[k + 1]
        if lo in pdata_starts or hi - lo > 0x4000:
            continue
        ws = [img.word(a) for a in range(lo, hi, 4)]
        if None in ws:
            continue
        if any(w == INSN_BCTR and _is_switch_bctr(img, lo + 4 * j) for j, w in enumerate(ws)):
            continue
        targets = set()
        for j, w in enumerate(ws):
            a, op = lo + 4 * j, w >> 26
            if op == 18 and not w & 3:
                targets.add(a + sext(w & 0x03FFFFFC, 26))
            elif op == 16 and not w & 2:
                targets.add(a + sext(w & 0xFFFC, 16))
        for j in range(1, len(ws)):
            a, w = lo + 4 * j, ws[j]
            if ws[j - 1] not in (INSN_BLR, INSN_BCTR) or w in (0, 0x60000000) or a in targets:
                continue
            if lo_img <= w < hi_img:
                continue  # donnée (adresse), pas une instruction
            if ws[j - 1] == INSN_BCTR:
                src = _ctr_source(img, a - 4)
                if src is None or src >> 26 != 32:  # pas un thunk lwz/mtctr/bctr
                    continue
            if crossing_branch(img, starts, a) is None:
                out.append(a)
    return out


def find_setjmp(img: Image, ranges: Ranges):
    """Cherche dans tout le code (setjmp n'a souvent pas d'entrée .pdata) :
    setjmp  = stfd f14..f31 via r3 ;
    longjmp = lfd f14..f31 depuis un même registre autre que r12, avec un mtcrf.
    r12 est exclu : __restfpr_14 (restauration de fin de fonction) lit f14..f31 via r12."""
    out = {"setjmp": set(), "longjmp": set()}
    for s in img.sections:
        if not s.executable:
            continue
        w = [x for (x,) in struct.iter_unpack(">I", s.data[: len(s.data) // 4 * 4])]

        def start_of(i: int) -> int:
            fn = ranges.containing(s.start + i * 4)
            if fn:
                return fn[0]
            st = i
            while st > 0 and i - st < 64 and w[st - 1] not in (0, INSN_BLR, INSN_BCTR):
                st -= 1
            return s.start + st * 4

        for kind, opcode in (("setjmp", 54), ("longjmp", 50)):
            hits = [i for i, x in enumerate(w)
                    if x >> 26 == opcode and (x >> 21) & 31 >= 14
                    and ((x >> 16) & 31 == 3 if kind == "setjmp" else (x >> 16) & 31 != 12)]
            for n, i in enumerate(hits):
                window = [j for j in hits[n: n + 18] if j < i + 48]
                if len({(w[j] >> 21) & 31 for j in window}) < 18:
                    continue
                if len({(w[j] >> 16) & 31 for j in window}) != 1:
                    continue
                if kind == "longjmp":
                    fn = ranges.containing(s.start + i * 4)
                    lo, hi = ((fn[0] - s.start) // 4, (fn[1] - s.start) // 4) if fn else (i, i + 96)
                    if not any(x >> 26 == 31 and (x >> 1) & 0x3FF == 144 for x in w[lo:hi]):
                        continue  # pas de mtcrf : simple restauration de registres
                out[kind].add(start_of(i))
    return {k: sorted(v) for k, v in out.items()}


# --------------------------------------------------------------------------- sortie

def load_unresolved(path: Path) -> dict[int, list[int]]:
    """Lit les « UnresolvedCall » d'un journal de `rexglue codegen` : {cible: [sources]}."""
    import re
    out = defaultdict(list)
    for m in re.finditer(r"(0x[0-9A-Fa-f]{8}) from (0x[0-9A-Fa-f]{8}):",
                         path.read_text(encoding="utf-8", errors="replace")):
        out[int(m[1], 16)].append(int(m[2], 16))
    return out


def load_registered(path: Path) -> set[int]:
    """Fonctions déjà générées par ReXGlue (generated/default/<projet>_register.cpp)."""
    import re
    text = path.read_text(encoding="utf-8", errors="replace")
    return {int(m[1], 16) for m in re.finditer(r"SetFunction\((0x[0-9A-Fa-f]+)", text)}


def load_known(path: Path | None) -> set[int]:
    if not path:
        return set()
    with path.open("rb") as f:
        cfg = tomllib.load(f)
    return {int(k, 0) for k in cfg.get("functions", {})}


def emit(title, refs, known, min_refs, lines):
    items = sorted((a, r) for a, r in refs.items() if a not in known and len(r) >= min_refs)
    if not items:
        return 0
    lines.append(f"\n# --- {title} ({len(items)})")
    for addr, r in items:
        lines.append(f"0x{addr:08X} = {{}}  # {len(r)} ref(s), e.g. 0x{r[0]:08X}")
    return len(items)


def main() -> None:
    for stream in (sys.stdout, sys.stderr):
        stream.reconfigure(encoding="utf-8")  # console Windows en cp1252 par défaut
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", type=Path, help="default.xex ou image PE extraite")
    ap.add_argument("-o", "--output", type=Path, help="fichier TOML de sortie (sinon stdout)")
    ap.add_argument("--known", type=Path, help="config TOML existante : ignorer ses [functions]")
    ap.add_argument("--min-refs", type=int, default=1, help="références minimum (défaut 1)")
    ap.add_argument("--loose", action="store_true",
                    help="ne pas exiger qu'une cible ressemble à un début de fonction")
    ap.add_argument("--registered", type=Path,
                    help="generated/default/<projet>_register.cpp : ignorer les fonctions que "
                         "ReXGlue a déjà générées (ne restent que celles qu'il a ratées)")
    ap.add_argument("--codegen-log", type=Path,
                    help="journal de `rexglue codegen` : ne sortir que les fonctions qu'il "
                         "n'a pas résolues (+ setjmp/longjmp)")
    ap.add_argument("--merged", action="store_true",
                    help="avec --registered : lister aussi les fonctions que ReXGlue a "
                         "fusionnées avec la précédente (code après un blr/bctr)")
    ap.add_argument("--check-config", action="store_true",
                    help="avec --known et --registered : signaler les [functions] de la config "
                         "qui sont en fait des étiquettes internes (traversées par un saut)")
    ap.add_argument("--dump-image", type=Path,
                    help="écrire l'image PE extraite (pratique pour Ghidra/IDA)")
    args = ap.parse_args()
    if args.check_config and not (args.known and args.registered):
        ap.error("--check-config demande --known et --registered")
    if args.merged and not args.registered:
        ap.error("--merged demande --registered")

    raw = args.input.read_bytes()
    if raw[:4] == b"XEX2":
        image, base = load_xex(raw)
    elif raw[:2] == b"MZ":
        image, base = raw, None
    else:
        sys.exit("format inconnu (ni XEX2 ni PE)")
    if args.dump_image:
        args.dump_image.write_bytes(image)

    img = parse_pe(image, base)
    ranges = Ranges(img.pdata)
    configured = load_known(args.known)
    known = set(configured)
    starts: list[int] = []
    if args.registered:
        registered = load_registered(args.registered)
        known |= registered
        starts = sorted(registered | {b for b, _ in img.pdata})

    if args.check_config:
        bad = [(a, crossing_branch(img, starts, a)) for a in sorted(configured)]
        bad = [(a, src) for a, src in bad if src is not None]
        for a, src in bad:
            print(f"0x{a:08X} : étiquette interne (saut traversant depuis 0x{src:08X})")
        print(f"{len(bad)} entrée(s) suspecte(s) sur {len(configured)}", file=sys.stderr)
        return

    data_refs = scan_data_pointers(img, ranges, args.loose)
    built, branched = scan_code(img, ranges, args.loose)
    jmp = find_setjmp(img, ranges)
    if starts:  # écarter les étiquettes internes aux fonctions déjà générées
        for refs in (data_refs, built):
            for a in [a for a in refs if a not in known and crossing_branch(img, starts, a)]:
                del refs[a]

    lines = [f"# find_fptrs.py : {args.input.name}, base 0x{img.base:08X}, "
             f"{len(img.pdata)} fonctions .pdata",
             "# À relire avant de coller dans [functions] : ce sont des candidats."]
    if jmp["setjmp"] or jmp["longjmp"]:
        lines.append("\n# --- candidats setjmp/longjmp (vérifier dans Ghidra, puis renseigner)")
        for a in jmp["setjmp"]:
            lines.append(f"# setjmp_address = 0x{a:08X}")
        for a in jmp["longjmp"]:
            lines.append(f"# longjmp_address = 0x{a:08X}")
    if args.codegen_log:
        unresolved = load_unresolved(args.codegen_log)
        lines.append(f"\n# --- non résolues par le codegen ({len(unresolved)})")
        for addr in sorted(unresolved):
            if addr in known:
                continue
            notes = [f"b depuis {', '.join(f'0x{a:08X}' for a in unresolved[addr])}"]
            if addr in data_refs:
                notes.append(f"{len(data_refs[addr])} réf. en données")
            if addr in built:
                notes.append(f"{len(built[addr])} adresse(s) construite(s)")
            if img.code_section(addr) is None:
                notes.append("ATTENTION : hors code")
            lines.append(f"0x{addr:08X} = {{}}  # {' ; '.join(notes)}")
        text = "\n".join(lines) + "\n"
        if args.output:
            args.output.write_text(text, encoding="utf-8")
        else:
            sys.stdout.write(text)
        print(f"{len(unresolved)} fonction(s) non résolue(s)", file=sys.stderr)
        return

    counts = [
        emit("pointeurs en données (vtables, callbacks, threads)", data_refs, known,
             args.min_refs, lines),
        emit("adresses construites (lis + addi/ori)",
             {a: r for a, r in built.items() if a not in data_refs}, known, args.min_refs, lines),
        emit("fonctions « trous » atteintes par un tail-call b",
             {a: r for a, r in branched.items() if a not in data_refs and a not in built},
             known, args.min_refs, lines),
    ]
    merged = []
    if args.merged:
        merged = [a for a in find_merged(img, starts)
                  if a not in known and a not in data_refs and a not in built]
        if merged:
            lines.append(f"\n# --- fusionnées par ReXGlue avec la fonction précédente ({len(merged)})")
            for a in merged:
                kind = "fonction vide (blr)" if img.word(a) == INSN_BLR else \
                    "après un thunk bctr" if img.word(a - 4) == INSN_BCTR else "après un blr"
                lines.append(f"0x{a:08X} = {{}}  # {kind}")
    text = "\n".join(lines) + "\n"
    if args.output:
        args.output.write_text(text, encoding="utf-8")
    else:
        sys.stdout.write(text)
    print(f"{sum(counts) + len(merged)} candidats (données {counts[0]}, construits {counts[1]}, "
          f"trous {counts[2]}, fusionnées {len(merged)})", file=sys.stderr)


if __name__ == "__main__":
    main()
