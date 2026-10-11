#!/usr/bin/env python3
"""Convertit les shaders Xbox 360 extraits (find_shaders.py --extract) en HLSL avec
XenosRecomp, puis (étape suivante) en DXIL/SPIR-V avec DXC.

    python tools/convert_shaders.py work/shaders/xsc work/shaders/hlsl

Écrit un rapport (report.txt) : nombre de réussites et, pour chaque échec, la cause.
"""
from __future__ import annotations

import argparse
import concurrent.futures as cf
import os
import subprocess
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
XENOSRECOMP = ROOT / "recomp/XenosRecomp/build/XenosRecomp/XenosRecomp.exe"
HEADER = ROOT / "recomp/XenosRecomp/XenosRecomp/shader_common.h"


def convert(xsc: Path, out_dir: Path, timeout: int) -> tuple[str, str]:
    """(statut, détail) ; statut ∈ ok / erreur / plantage / délai."""
    out = out_dir / (xsc.stem + ".hlsl")
    try:
        r = subprocess.run([str(XENOSRECOMP), str(xsc), str(out), str(HEADER)],
                           capture_output=True, text=True, timeout=timeout,
                           encoding="utf-8", errors="replace")
    except subprocess.TimeoutExpired:
        return "délai", f"plus de {timeout} s"
    text = (r.stdout + r.stderr).strip()
    if r.returncode < 0 or r.returncode > 255 or r.returncode in (3, 0xC0000005):
        return "plantage", f"code {r.returncode:#x}"
    if r.returncode != 0 or not out.exists() or out.stat().st_size == 0:
        first = next((l for l in text.splitlines() if l.strip()), f"code {r.returncode}")
        return "erreur", first[:200]
    return "ok", ""


def find_dxc() -> Path | None:
    kits = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Windows Kits/10/bin"
    found = sorted(kits.glob("10.*/x64/dxc.exe"))
    return found[-1] if found else None


def compile_dxc(dxc: Path, hlsl: Path, out: Path, spirv: bool, timeout: int) -> tuple[str, str]:
    """Compile un shader HLSL de XenosRecomp en DXIL (D3D12) ou SPIR-V (Vulkan)."""
    stage = "vs" if hlsl.name.endswith(".vs.hlsl") else "ps"
    cmd = [str(dxc), "-T", f"{stage}_6_0", "-E", "main", "-HV", "2021", "-O3",
           "-I", str(HEADER.parent), "-Fo", str(out), str(hlsl)]
    if spirv:
        cmd[1:1] = ["-spirv", "-fspv-target-env=vulkan1.1"]
        cmd[1:1] = ["-fvk-invert-y"] if stage == "vs" else ["-fvk-use-dx-position-w"]
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout,
                           encoding="utf-8", errors="replace")
    except subprocess.TimeoutExpired:
        return "délai", f"plus de {timeout} s"
    if r.returncode != 0 or not out.exists():
        err = next((l for l in r.stderr.splitlines() if "error" in l), r.stderr.strip()[:200])
        # Retire le chemin du fichier pour regrouper les causes identiques.
        return "erreur", err.split(": error: ", 1)[-1][:200]
    return "ok", ""


def main() -> None:
    for stream in (sys.stdout, sys.stderr):
        stream.reconfigure(encoding="utf-8")
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("xsc_dir", type=Path)
    ap.add_argument("hlsl_dir", type=Path)
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 8)
    ap.add_argument("--timeout", type=int, default=30)
    ap.add_argument("--compile", choices=["dxil", "spirv"], action="append", default=[],
                    help="compiler aussi le HLSL obtenu (répétable) ; sortie à côté de hlsl_dir")
    args = ap.parse_args()
    if not XENOSRECOMP.exists():
        sys.exit(f"XenosRecomp introuvable : {XENOSRECOMP}")

    args.hlsl_dir.mkdir(parents=True, exist_ok=True)
    files = sorted(args.xsc_dir.glob("*.xsc"))
    results: dict[str, tuple[str, str]] = {}
    with cf.ThreadPoolExecutor(args.jobs) as pool:
        futures = {pool.submit(convert, f, args.hlsl_dir, args.timeout): f for f in files}
        for fut in cf.as_completed(futures):
            results[futures[fut].name] = fut.result()

    status = Counter(s for s, _ in results.values())
    reasons = Counter(d for s, d in results.values() if s != "ok")
    lines = [f"{len(files)} shaders : " + ", ".join(f"{k} {v}" for k, v in status.most_common())]
    if reasons:
        lines.append("\nCauses d'échec les plus fréquentes :")
        lines += [f"  {n:4}x  {d}" for d, n in reasons.most_common(15)]
        lines.append("\nDétail :")
        lines += [f"  {name}: {s} {d}" for name, (s, d) in sorted(results.items()) if s != "ok"]
    report = "\n".join(lines) + "\n"
    (args.hlsl_dir / "report.txt").write_text(report, encoding="utf-8")
    print("\n".join(lines[: 2 + min(15, len(reasons)) + 1]))

    if args.compile:
        dxc = find_dxc()
        if dxc is None:
            sys.exit("dxc.exe introuvable (Windows SDK)")
        hlsl_files = [args.hlsl_dir / (n[:-4] + ".hlsl") for n, (s, _) in results.items() if s == "ok"]
        for target in args.compile:
            out_dir = args.hlsl_dir.parent / target
            out_dir.mkdir(parents=True, exist_ok=True)
            ext = ".dxil" if target == "dxil" else ".spv"
            with cf.ThreadPoolExecutor(args.jobs) as pool:
                futs = {pool.submit(compile_dxc, dxc, h, out_dir / (h.name[:-5] + ext),
                                    target == "spirv", args.timeout): h for h in hlsl_files}
                res = {futs[f].name: f.result() for f in cf.as_completed(futs)}
            st = Counter(s for s, _ in res.values())
            why = Counter(d for s, d in res.values() if s != "ok")
            out = [f"\n{target} : " + ", ".join(f"{k} {v}" for k, v in st.most_common())]
            out += [f"  {n:4}x  {d}" for d, n in why.most_common(10)]
            out += [f"  {name}: {d}" for name, (s, d) in sorted(res.items()) if s != "ok"]
            (out_dir / "report.txt").write_text("\n".join(out) + "\n", encoding="utf-8")
            print("\n".join(out[:12]))


if __name__ == "__main__":
    main()
