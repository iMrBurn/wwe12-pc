"""Test de find_fptrs.py sur un XEX2 synthétique (aucune donnée de jeu nécessaire).

    python tools/test_find_fptrs.py
"""
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes

TOOL = Path(__file__).with_name("find_fptrs.py")
BASE = 0x82000000
RDATA, PDATA, TEXT = 0x1000, 0x2000, 0x3000
RETAIL_KEY = bytes.fromhex("20B185A59D28FDC340583FBB0896BF91")


def be(*words):
    return b"".join(struct.pack(">I", w) for w in words)


def build_image() -> bytes:
    img = bytearray(0x4000)
    # En-tête PE minimal (PPC big-endian, 3 sections)
    img[0:2] = b"MZ"
    struct.pack_into("<I", img, 0x3C, 0x80)
    img[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HH", img, 0x84, 0x1F2, 3)
    struct.pack_into("<H", img, 0x84 + 16, 0xE0)
    struct.pack_into("<I", img, 0x98 + 28, BASE)
    for i, (name, rva, size, chars) in enumerate([
        (b".rdata", RDATA, 0x1000, 0x40000040),
        (b".pdata", PDATA, 0x1000, 0x40000040),
        (b".text", TEXT, 0x1000, 0x60000020),
    ]):
        sh = 0x98 + 0xE0 + i * 40
        img[sh:sh + 8] = name.ljust(8, b"\0")
        struct.pack_into("<IIII", img, sh + 8, size, rva, size, rva)
        struct.pack_into("<I", img, sh + 36, chars)

    # .text
    code = bytearray(0x1000)
    # A @0x3000 (dans .pdata) : lis r11,0x8200 ; addi r3,r11,0x3060 ; nop ; b G3 ; ... ; blr
    a = be(0x3D608200, (14 << 26) | (3 << 21) | (11 << 16) | 0x3060, 0x60000000,
           (18 << 26) | (0x3080 - 0x300C)) + be(0x60000000) * 11 + be(0x4E800020)
    code[0x00:0x40] = a
    code[0x40:0x48] = be(0x38600001, 0x4E800020)  # G1 : pointé depuis .rdata
    code[0x60:0x68] = be(0x38600002, 0x4E800020)  # G2 : lis + addi
    code[0x80:0x88] = be(0x38600003, 0x4E800020)  # G3 : b (tail-call)
    # S @0x30A0 : setjmp (stfd f14..f31, n(r3))
    s = be(*[(54 << 26) | (n << 21) | (3 << 16) | ((n - 14) * 8) for n in range(14, 32)])
    code[0xA0:0xA0 + len(s) + 8] = s + be(0x38600000, 0x4E800020)
    img[TEXT:TEXT + 0x1000] = code

    # .pdata : A (16 instr.) et S (20 instr.)
    img[PDATA:PDATA + 16] = be(BASE + 0x3000, 16 << 8, BASE + 0x30A0, 20 << 8)
    # .rdata : G1 (2 réf.), A (connue), A+8 (étiquette interne), valeur sans rapport
    img[RDATA:RDATA + 20] = be(BASE + 0x3040, BASE + 0x3040, BASE + 0x3000,
                               BASE + 0x3008, 0x12345678)
    return bytes(img)


def build_xex(image: bytes, encrypt: bool) -> bytes:
    data_len = 0x3100  # le reste de l'image devient un bloc de zéros
    payload = image[:data_len]
    file_key = bytes(16)
    if encrypt:
        session = bytes(range(16))
        file_key = Cipher(algorithms.AES(RETAIL_KEY), modes.ECB()).encryptor().update(session)
        enc = Cipher(algorithms.AES(session), modes.CBC(bytes(16))).encryptor()
        payload = enc.update(payload) + enc.finalize()

    ff_off, sec_off, pe_off = 0x40, 0x80, 0x400
    hdr = bytearray(pe_off)
    struct.pack_into(">4sIIIII", hdr, 0, b"XEX2", 0, pe_off, 0, sec_off, 2)
    struct.pack_into(">IIII", hdr, 24, 0x3FF, ff_off, 0x10201, BASE)
    struct.pack_into(">IHHII", hdr, ff_off, 16, int(encrypt), 1, data_len, len(image) - data_len)
    struct.pack_into(">I", hdr, sec_off + 4, len(image))
    struct.pack_into(">I", hdr, sec_off + 0x110, BASE)
    hdr[sec_off + 0x150:sec_off + 0x160] = file_key
    return bytes(hdr) + payload


def run(path: Path, *extra) -> str:
    r = subprocess.run([sys.executable, str(TOOL), str(path), *extra],
                       capture_output=True, text=True, encoding="utf-8", check=True)
    return r.stdout


def check(out: str, label: str) -> None:
    expect_in = ["0x82003040 = {}  # 2 ref(s)", "0x82003060 = {}", "0x82003080 = {}",
                 "setjmp_address = 0x820030A0"]
    expect_out = ["0x82003000 =", "0x82003008 =", "0x12345678"]
    for e in expect_in:
        assert e in out, f"[{label}] manquant : {e}\n{out}"
    for e in expect_out:
        assert e not in out, f"[{label}] faux positif : {e}\n{out}"
    print(f"OK {label}")


def main() -> None:
    image = build_image()
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        (tmp / "image.exe").write_bytes(image)
        check(run(tmp / "image.exe"), "image PE")
        for enc in (False, True):
            p = tmp / f"test_{enc}.xex"
            p.write_bytes(build_xex(image, enc))
            check(run(p), f"XEX basic, chiffré={enc}")
        # --known : les adresses déjà déclarées disparaissent
        (tmp / "cfg.toml").write_text('[functions]\n0x82003040 = {}\n')
        out = run(tmp / "image.exe", "--known", str(tmp / "cfg.toml"))
        assert "0x82003040 =" not in out and "0x82003060 =" in out, out
        print("OK --known")


if __name__ == "__main__":
    main()
