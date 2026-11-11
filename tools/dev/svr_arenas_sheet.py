"""Planche de contrôle des arènes de SvR 2008 / 2009, lue sur le disque du joueur, sans rien porter (2026-11-11).

Une ligne par arène : sa bannière (photo de l'écran de choix, bande 2:1 comme le Mod Maker), son écran VS recomposé
(fond mh_<show>_bg ; 2008 : la plaque mh_<show>_fr en bas, pleine largeur, comme le Mod Maker la met dans la plaque des
noms ; 2009 : la bande posée en bas du fond), la plus grande image de sa plaque d'entrée (_EvtGdHD.pac ET2D|ETEX/00NN,
entrée 0) et une image du milieu de sa vidéo de logo (MOV /M0NN, si ffmpeg est là). Pour comparer d'un coup d'œil avec
Xenia. Aucune donnée du jeu dans le dépôt : la planche est écrite dans work/svr_check.

    python tools/dev/svr_arenas_sheet.py "<iso SvR 2008 ou 2009>" 2008
"""
from __future__ import annotations

import io
import os
import shutil
import struct
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import yukes_bpe  # noqa: E402
from PIL import Image, ImageDraw  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))


class Iso:
    """XDVDFS (partitions à 0 / 0x2080000 / 0xFD90000 / 0x18300000)."""

    def __init__(self, path):
        self.f = open(path, "rb")
        for base in (0, 0x2080000, 0xFD90000, 0x18300000):
            self.f.seek(base + 32 * 2048)
            h = self.f.read(28)
            if h[:20] == b"MICROSOFT*XBOX*MEDIA":
                self.base, self.root = base, struct.unpack_from("<II", h, 20)
                return
        raise ValueError("pas un disque Xbox 360")

    def _rd(self, sec, size):
        self.f.seek(self.base + sec * 2048)
        return self.f.read(size)

    def _dir(self, sec, size):
        d, out = self._rd(sec, size), {}

        def walk(o):
            if o * 4 + 14 > len(d):
                return
            l, r, es, esz, _, nl = struct.unpack_from("<HHIIBB", d, o * 4)
            if l == 0xFFFF:
                return
            out[d[o * 4 + 14:o * 4 + 14 + nl].decode("latin1").lower()] = (es, esz)
            if l:
                walk(l)
            if r:
                walk(r)

        walk(0)
        return out

    def read(self, path):
        sec, size = self.root
        for part in path.strip("/").split("/"):
            e = self._dir(sec, size).get(part.lower())
            if not e:
                return None
            sec, size = e
        return self._rd(sec, size)


def unbpe(b):
    if b[:4] == b"BPE ":
        r = yukes_bpe.decompress_at(b, 0)
        return r if r else b
    return b


def epac(d):
    p, out = 0x800, []
    while p + 12 <= 0x4000:
        t = d[p:p + 4]
        if not t.strip(b"\0"):
            break
        cw = struct.unpack_from("<I", d, p + 4)[0]
        p += 12
        for _ in range((cw & 0xFFFF) // 3):
            n = d[p:p + 4].decode("latin1")
            sec, sz = struct.unpack_from("<II", d, p + 4)
            p += 12
            out.append((t.decode("latin1"), n, d[0x4000 + sec * 0x800:0x4000 + sec * 0x800 + sz * 0x100]))
    return out


def pach(b):
    if b[:4] != b"PACH":
        return []
    n = struct.unpack_from("<I", b, 4)[0]
    base = 8 + 12 * n
    return [(i, b[base + o:base + o + s]) for i, o, s in (struct.unpack_from("<III", b, 8 + 12 * k) for k in range(n))]


def bundle(b):
    """Paquet d'images (compte, puis 32 octets : nom[16], ext[4], taille, place) -> {nom: Image}."""
    out = {}
    if len(b) < 16:
        return out
    c = struct.unpack_from("<I", b, 0)[0]
    if not 0 < c < 4096 or len(b) < 16 + 32 * c:
        return out
    for k in range(c):
        r = b[16 + 32 * k:48 + 32 * k]
        nm = r[:16].split(b"\0")[0].decode("latin1")
        sz, off = struct.unpack_from("<II", r, 20)
        try:
            out[nm] = Image.open(io.BytesIO(b[off:off + sz])).convert("RGBA")
        except Exception:
            pass
    return out


def dds_all(b, depth=0):
    """Toutes les images DDS d'une entrée (PACH imbriqués)."""
    b = unbpe(b)
    if b[:4] == b"DDS ":
        try:
            return [Image.open(io.BytesIO(b)).convert("RGBA")]
        except Exception:
            return []
    if b[:4] == b"PACH" and depth < 4:
        return [im for _, x in pach(b) for im in dds_all(x, depth + 1)]
    bb = bundle(b)
    return list(bb.values())


def fit(im, w, h):
    im = im.copy()
    im.thumbnail((w, h))
    return im


def main():
    iso_path, year = sys.argv[1], int(sys.argv[2]) if len(sys.argv) > 2 else 2008
    iso = Iso(iso_path)
    match = epac(iso.read("pac/menu/MatchHD.pac"))
    vs = {n: bundle(unbpe(dict(pach(unbpe(b))).get(7001, b""))) for g, n, b in match if g == "MENU" and n.endswith("I")}
    # bannières
    banners = {}
    if year == 2008:
        for g, n, b in epac(iso.read("pac/menu/menuHD.pac")):
            if g == "MENU" and n == "STTX":
                inner = dict(pach(unbpe(b))).get(0)
                for i, x in pach(unbpe(inner or b"")):
                    try:
                        banners[i] = Image.open(io.BytesIO(unbpe(x))).convert("RGBA")
                    except Exception:
                        pass
    else:
        names = ["arena_smack", "arena_raw", "arena_ecw", "arena_troop", "arena_summer", "arena_unfor", "arena_nomercy",
                 "arena_survive", "arena_armage", "arena_rumble", "arena_noway", "arena_WM24", "arena_backlash",
                 "arena_judge", "arena_ven", "arena_GAB", "arena_SNME", "arena_ONS", "arena_cybers"]
        for g, n, b in epac(iso.read("pac/menu/ScrLayHD.pac")):
            if n == "LSSE":
                pics = bundle(unbpe(dict(pach(unbpe(b))).get(7001, b"")))
                for i, nm in enumerate(names):
                    if nm in pics:
                        banners[i] = pics[nm]
    # plaques d'entrée
    plates = {}
    evt = iso.read("pac/evt/_EvtGdHD.pac")
    if evt:
        for g, n, b in epac(evt):
            if g in ("ET2D", "ETEX") and n.isdigit():
                ims = []
                for i, x in pach(unbpe(b)):
                    if i < 10000:
                        ims += dds_all(x)
                if ims:
                    plates[int(n)] = max(ims, key=lambda im: im.width * im.height)
    # vidéos de logo
    movies = {}
    mov = iso.read("pac/movie.pac" if year == 2008 else "pac/evt/_EvtMov.pac")
    ffmpeg = shutil.which("ffmpeg")
    tmp = tempfile.mkdtemp()
    if mov and ffmpeg:
        for g, n, b in epac(mov):
            if g == "MOV " and n[1:].isdigit():
                src = os.path.join(tmp, n + ".bik")
                open(src, "wb").write(b)
                dst = os.path.join(tmp, n + ".png")
                subprocess.run([ffmpeg, "-v", "error", "-y", "-i", src, "-vf", "select=eq(n\\,90)", "-frames:v", "1", dst],
                               check=False)
                if os.path.exists(dst):
                    movies[int(n[1:])] = Image.open(dst).convert("RGBA")
    arenas = sorted(int(n[1:3]) for n in vs if n[1:3].isdigit())
    W, H = 1700, 200
    sheet = Image.new("RGBA", (W, H * len(arenas)), (24, 24, 24, 255))
    dr = ImageDraw.Draw(sheet)
    for row, a in enumerate(arenas):
        y = row * H
        pics = vs.get("M%02dI" % a, {})
        bg = next((im for nm, im in pics.items() if nm.startswith("mh_") and nm.endswith("_bg")), None)
        fr = next((im for nm, im in pics.items() if nm.startswith("mh_") and nm.endswith("_fr")), None)
        show = next((nm[3:-3] for nm in pics if nm.startswith("mh_") and nm.endswith("_bg")), "?")
        dr.text((6, y + 4), "%02d %s" % (a, show), fill=(255, 255, 0, 255))
        if a in banners:
            im = banners[a]
            ch = min(im.height, im.width // 2)
            y0 = (im.height - ch) // 3 if year == 2008 else im.width // 32 + (im.height - im.width // 16 - ch) // 3
            band = im.crop((0, y0, im.width, y0 + ch)).resize((256, 128))
            sheet.alpha_composite(band, (6, y + 24))
        if bg:
            card = bg.resize((352, 198)).copy()
            if fr:
                if year == 2008:
                    p = fr.resize((352, 50))
                    card.alpha_composite(p, (0, 198 - 50 - 20))
                else:
                    fh = 198 * fr.height // bg.height
                    card.alpha_composite(fr.resize((352, fh)), (0, 198 - fh))
            sheet.alpha_composite(card, (270, y + 1))
        if a in plates:
            sheet.alpha_composite(fit(plates[a], 700, 190), (630, y + 5))
        if a in movies:
            sheet.alpha_composite(fit(movies[a], 190, 190), (1340, y + 5))
    out_dir = os.path.join(ROOT, "work", "svr_check")
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, "svr%d_arenas.png" % year)
    sheet.convert("RGB").save(out)
    shutil.rmtree(tmp, ignore_errors=True)
    print(out, len(arenas), "arènes")


if __name__ == "__main__":
    main()
