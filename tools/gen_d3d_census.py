#!/usr/bin/env python3
"""Génère port/src/d3d_census.cpp : un crochet de comptage sur chaque fonction d'une plage
d'adresses (par défaut la bibliothèque Direct3D de la XDK dans WWE '12), pour identifier les
fonctions par leur comportement en jeu (« census » D3D, comme SvR2011).

Chaque crochet compte les appels, retient les arguments r3-r6 du dernier appel, et note si
r3 a toujours la même valeur (r3 = l'objet device pour les méthodes D3DDevice_*), puis
appelle la fonction d'origine. Un fil écrit d3d_census.csv toutes les 2 s.

    python tools/gen_d3d_census.py                      # plage 0x82A70000-0x82AC8000
    cmake ... -DWWE12_D3D_CENSUS=ON                     # compile le module
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

HEADER = r"""// Généré par tools/gen_d3d_census.py — ne pas modifier à la main.
// Recensement des appels aux fonctions 0x{lo:08X}-0x{hi:08X} (bibliothèque D3D de la XDK),
// compilé seulement avec -DWWE12_D3D_CENSUS=ON. Écrit d3d_census.csv toutes les 2 s :
// adresse, appels totaux, appels sur les 2 dernières secondes, r3 constant ?, r3-r6 du dernier
// appel.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <rex/ppc/context.h>
#include <rex/ppc/func.h>

namespace {{

struct Census {{
  uint32_t address;
  std::atomic<uint64_t> calls{{0}};
  std::atomic<uint32_t> r3_first{{0}}, r3{{0}}, r4{{0}}, r5{{0}}, r6{{0}};
  std::atomic<bool> r3_varies{{false}};
  // Nombre d'appels où r4 / r5 pointe vers un conteneur de shader Xbox 360 (0x102A11xx) :
  // signe des fonctions Create*Shader.
  std::atomic<uint32_t> r4_shader{{0}}, r5_shader{{0}};
  // Pour r4, r5, r6 : histogramme du quartet de poids fort du premier mot pointé (champ
  // D3DResource::Common, dont les bits 28-31 donnent le type d'objet), 16 cases.
  std::atomic<uint32_t> ptr_type[3][16]{{}};
  // Plage de r4 quand c'est un petit entier (index de sampler, de flux, de cible…).
  std::atomic<uint32_t> r4_small_min{{0xFFFFFFFF}}, r4_small_max{{0}};
  // Dernier premier mot complet lu derrière r4 / r5 / r6 (champ Common d'un objet D3D).
  std::atomic<uint32_t> ptr_word[3]{{}};
  // Arguments suivants du dernier appel (Clear, Resolve, SetStreamSource… en ont plus de 3).
  std::atomic<uint32_t> r7{{0}}, r8{{0}}, r9{{0}}, r10{{0}};
  // Adresse de retour (code du jeu qui appelle) et premier argument flottant (Clear : Z).
  std::atomic<uint32_t> lr{{0}};
  std::atomic<double> f1{{0}};
  // --dump-words : 16 premiers mots de l'objet pointé par r4 au dernier appel (structure des
  // objets D3D du jeu : où sont le microcode d'un shader, les éléments d'une déclaration…).
  bool dump_words = false;
  std::atomic<uint32_t> words[16]{{}};
  uint64_t calls_at_last_dump = 0;
}};

Census g_census[{count}];

// Lit le mot big-endian à une adresse invitée sans risquer de planter : on ne lit que si la
// page hôte est engagée et lisible (le gestionnaire d'exceptions de ReXGlue intercepterait un
// accès invalide avant tout __try et terminerait le jeu). Hors registres MMIO (0x7Fxxxxxx).
bool ReadGuestWord(const uint8_t* base, uint32_t address, uint32_t& out) {{
  if (address < 0x10000 || address > 0xFFFFFFFC || (address >> 24) == 0x7F) return false;
  const uint8_t* host = base + address;
  MEMORY_BASIC_INFORMATION mbi;
  if (!VirtualQuery(host, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
      (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) ||
      !(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                       PAGE_EXECUTE_READWRITE))) {{
    return false;
  }}
  out = _byteswap_ulong(*reinterpret_cast<const uint32_t*>(host));
  return true;
}}

inline void Record(Census& c, const PPCContext& ctx, const uint8_t* base) {{
  // Les fonctions de création sont rares : inspecter les 200 premiers appels suffit et évite
  // le coût de VirtualQuery sur les fonctions appelées des millions de fois.
  if (c.calls.load(std::memory_order_relaxed) < 200) {{
    uint32_t word;
    if (ReadGuestWord(base, ctx.r4.u32, word) && (word & 0xFFFFFF00) == 0x102A1100) {{
      c.r4_shader.fetch_add(1, std::memory_order_relaxed);
    }}
    if (ReadGuestWord(base, ctx.r5.u32, word) && (word & 0xFFFFFF00) == 0x102A1100) {{
      c.r5_shader.fetch_add(1, std::memory_order_relaxed);
    }}
    const uint32_t args[3] = {{ctx.r4.u32, ctx.r5.u32, ctx.r6.u32}};
    for (int i = 0; i < 3; i++) {{
      if (args[i] >= 0x40000000 && ReadGuestWord(base, args[i], word)) {{
        c.ptr_type[i][word >> 28].fetch_add(1, std::memory_order_relaxed);
        c.ptr_word[i].store(word, std::memory_order_relaxed);
      }}
    }}
  }}
  if (c.dump_words) {{
    uint32_t w;
    for (int i = 0; i < 16; i++) {{
      if (!ReadGuestWord(base, ctx.r4.u32 + 4 * i, w)) break;
      c.words[i].store(w, std::memory_order_relaxed);
    }}
  }}
  if (ctx.r4.u32 < 0x10000) {{
    uint32_t v = ctx.r4.u32;
    uint32_t cur = c.r4_small_min.load(std::memory_order_relaxed);
    while (v < cur && !c.r4_small_min.compare_exchange_weak(cur, v)) {{}}
    cur = c.r4_small_max.load(std::memory_order_relaxed);
    while (v > cur && !c.r4_small_max.compare_exchange_weak(cur, v)) {{}}
  }}
  uint32_t r3 = ctx.r3.u32;
  if (c.calls.fetch_add(1, std::memory_order_relaxed) == 0) {{
    c.r3_first.store(r3, std::memory_order_relaxed);
  }} else if (r3 != c.r3_first.load(std::memory_order_relaxed)) {{
    c.r3_varies.store(true, std::memory_order_relaxed);
  }}
  c.r3.store(r3, std::memory_order_relaxed);
  c.r4.store(ctx.r4.u32, std::memory_order_relaxed);
  c.r5.store(ctx.r5.u32, std::memory_order_relaxed);
  c.r6.store(ctx.r6.u32, std::memory_order_relaxed);
  c.r7.store(ctx.r7.u32, std::memory_order_relaxed);
  c.r8.store(ctx.r8.u32, std::memory_order_relaxed);
  c.r9.store(ctx.r9.u32, std::memory_order_relaxed);
  c.r10.store(ctx.r10.u32, std::memory_order_relaxed);
  c.lr.store(static_cast<uint32_t>(ctx.lr), std::memory_order_relaxed);
  c.f1.store(ctx.f1.f64, std::memory_order_relaxed);
}}

void DumpLoop() {{
  for (;;) {{
    std::this_thread::sleep_for(std::chrono::seconds(2));
    FILE* f = std::fopen("d3d_census.csv.tmp", "w");
    if (!f) continue;
    std::fprintf(f, "address,calls,calls_2s,r3_constant,r3,r4,r5,r6,r4_shader,r5_shader,"
                    "r4_types,r5_types,r6_types,r4_small_min,r4_small_max,"
                    "r4_word,r5_word,r6_word,r7,r8,r9,r10,lr,f1,r4_object\n");
    for (Census& c : g_census) {{
      uint64_t calls = c.calls.load(std::memory_order_relaxed);
      if (!calls) continue;
      std::fprintf(f, "%08X,%llu,%llu,%d,%08X,%08X,%08X,%08X,%u,%u", c.address,
                   (unsigned long long)calls, (unsigned long long)(calls - c.calls_at_last_dump),
                   c.r3_varies.load() ? 0 : 1, c.r3.load(), c.r4.load(), c.r5.load(), c.r6.load(),
                   c.r4_shader.load(), c.r5_shader.load());
      // Histogrammes « type:nombre » séparés par des espaces (ex. « 3:120 4:2 »).
      for (int i = 0; i < 3; i++) {{
        std::fputc(',', f);
        for (int t = 0; t < 16; t++) {{
          uint32_t n = c.ptr_type[i][t].load(std::memory_order_relaxed);
          if (n) std::fprintf(f, "%X:%u ", t, n);
        }}
      }}
      uint32_t lo = c.r4_small_min.load(), hi = c.r4_small_max.load();
      if (lo <= hi) std::fprintf(f, ",%u,%u", lo, hi);
      else std::fprintf(f, ",,");
      std::fprintf(f, ",%08X,%08X,%08X,%08X,%08X,%08X,%08X,%08X,%g,", c.ptr_word[0].load(),
                   c.ptr_word[1].load(), c.ptr_word[2].load(), c.r7.load(), c.r8.load(),
                   c.r9.load(), c.r10.load(), c.lr.load(), c.f1.load());
      if (c.dump_words) {{
        for (int i = 0; i < 16; i++) std::fprintf(f, "%08X ", c.words[i].load());
      }}
      std::fputc('\n', f);
      c.calls_at_last_dump = calls;
    }}
    std::fclose(f);
    std::remove("d3d_census.csv");
    std::rename("d3d_census.csv.tmp", "d3d_census.csv");
  }}
}}

struct Starter {{
  Starter() {{
{init}
    std::thread(DumpLoop).detach();
  }}
}} g_starter;

}}  // namespace

"""


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--lo", type=lambda s: int(s, 0), default=0x82A70000)
    ap.add_argument("--hi", type=lambda s: int(s, 0), default=0x82AC8000)
    ap.add_argument("--register", type=Path,
                    default=ROOT / "port/generated/default/wwe12_register.cpp")
    ap.add_argument("--out", type=Path, default=ROOT / "port/src/d3d_census.cpp")
    ap.add_argument("--dump-words", default="",
                    help="adresses (séparées par des virgules) dont on relève les 16 premiers "
                         "mots de l'objet pointé par r4")
    args = ap.parse_args()
    dump = {int(a, 0) for a in args.dump_words.split(",") if a}

    text = args.register.read_text(encoding="utf-8", errors="replace")
    funcs = sorted(int(a, 16) for a in re.findall(r"SetFunction\((0x[0-9A-F]+), sub_", text)
                   if args.lo <= int(a, 16) < args.hi)
    init = "\n".join(f"    g_census[{i}].address = 0x{a:08X};"
                     + (f" g_census[{i}].dump_words = true;" if a in dump else "")
                     for i, a in enumerate(funcs))
    hooks = []
    for i, a in enumerate(funcs):
        hooks.append(f"REX_EXTERN(__imp__sub_{a:08X});\n"
                     f"extern \"C\" REX_FUNC(sub_{a:08X}) {{\n"
                     f"  Record(g_census[{i}], ctx, base);\n"
                     f"  __imp__sub_{a:08X}(ctx, base);\n"
                     f"}}\n")
    src = HEADER.format(lo=args.lo, hi=args.hi, count=len(funcs), init=init) + "\n".join(hooks)
    args.out.write_text(src, encoding="utf-8")
    print(f"{len(funcs)} crochets -> {args.out}")


if __name__ == "__main__":
    main()
