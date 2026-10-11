#!/usr/bin/env python3
"""Génère port/generated/native/shader_keys.cpp : clés secondaires des shaders précompilés.

La cache de shaders (shader_cache.cpp) est indexée par XXH3 du conteneur .xsc entier, tel
qu'extrait des archives du jeu. En mémoire, WWE '12 modifie ce conteneur une fois chargé
(microcode copié en mémoire physique, champs remplis), si bien que l'empreinte calculée en
jeu ne correspond plus. Ce script produit, pour chaque conteneur, deux clés qui survivent à
ces modifications :

  - XXH3 du microcode seul (partie « physique », les physicalSize derniers octets) ;
  - XXH3 de la partie « virtuelle » (virtualSize premiers octets) avec les champs
    0x1C / 0x20 (remplis au chargement) mis à zéro.

Le rendu natif (svr_resources.cpp) s'en sert quand l'empreinte complète ne trouve rien.

Usage : python tools/gen_shader_keys.py work/shaders/xsc port/generated/native/shader_keys.cpp
"""
import pathlib
import struct
import sys

import xxhash


def keys(data: bytes):
    flags, vsize, psize = struct.unpack_from(">III", data, 0)
    if (flags & 0xFFFFFF00) != 0x102A1100 or vsize + psize > len(data):
        return None
    full = xxhash.xxh3_64_intdigest(data[: vsize + psize])
    virt = bytearray(data[:vsize])
    virt[0x1C:0x24] = bytes(8)
    phys = data[vsize : vsize + psize]
    return full, xxhash.xxh3_64_intdigest(bytes(virt)), xxhash.xxh3_64_intdigest(phys), flags & 1


def main():
    src, out = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
    rows = []
    for f in sorted(src.glob("*.xsc")):
        k = keys(f.read_bytes())
        if k:
            rows.append(k)
    lines = [
        "// Généré par tools/gen_shader_keys.py : ne pas modifier.",
        "#include <cstddef>",
        "#include <cstdint>",
        "",
        "struct WWE12ShaderKey { uint64_t full, virt, phys; uint32_t vertex; };",
        "extern const WWE12ShaderKey g_wwe12_shader_keys[];",
        "extern const size_t g_wwe12_shader_key_count;",
        "const WWE12ShaderKey g_wwe12_shader_keys[] = {",
    ]
    lines += [f"  {{0x{a:016X}ull, 0x{b:016X}ull, 0x{c:016X}ull, {d}}}," for a, b, c, d in rows]
    lines += ["};", f"const size_t g_wwe12_shader_key_count = {len(rows)};", ""]
    out.write_text("\n".join(lines), encoding="utf-8")
    print(f"{len(rows)} clés -> {out}")


if __name__ == "__main__":
    main()
