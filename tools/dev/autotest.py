"""Runs one game test with nobody at the PC, then sums it up.

Copies a data folder to a fresh one (so the menus' cursors start where the template left them),
starts the game on it with the remote pad (wwe12_remote_pad), hands it a script once it has booted,
waits for the script's "quit" (or the time limit: then it stops the game it started, never another
one), and prints the [perf] figures between the script's marks, the pipelines compiled on the render
thread and the shots taken. The words of a script are in src/native/wwe12_remote_pad.cpp.

    python -I tools/dev/autotest.py --template C:/Users/melfo/Documents/wwe12_test ^
        --script "START wait4000 A wait2500 ... untilmatch:240 markmatch wait30000 markend shotmatch quit"

A script can also come from a file (--script-file). Nothing is sent anywhere; everything stays in
work/autotest/<name>.
"""
import argparse
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
BUILD = REPO / "port" / "out" / "build" / "win-amd64-release"

PERF = re.compile(r"\[perf\] (\d+) frames: interval ([\d.]+) ms \(max ([\d.]+)\) \| work ([\d.]+) ms "
                  r"\(max ([\d.]+)\) \| gpu wait ([\d.]+) ms")
CPU = re.compile(r"\[perf\] cpu per frame: render thread ([\d.]+) ms \(work [\d.]+ ms\) \| whole game ([\d.]+) ms")
LOAD = re.compile(r"\[load\].*pipelines (\d+) \((\d+) known, (\d+) ms\)")
MARK = re.compile(r"\[remote\] mark (\S+)")


def summary(log_path):
    lines = log_path.read_text(encoding="utf-8", errors="replace").splitlines()
    section, sections = "start", {}
    for line in lines:
        if m := MARK.search(line):
            section = m.group(1).lower()
            continue
        s = sections.setdefault(section, {"perf": [], "cpu": [], "pso": [0, 0, 0]})
        if m := PERF.search(line):
            s["perf"].append(tuple(float(x) for x in m.groups()))
        elif m := CPU.search(line):
            s["cpu"].append((float(m.group(1)), float(m.group(2))))
        elif m := LOAD.search(line):
            for i in range(3):
                s["pso"][i] += int(m.group(i + 1))
    for name, s in sections.items():
        print(f"== after mark '{name}'")
        if s["perf"]:
            frames = sum(p[0] for p in s["perf"])
            avg = lambda i: sum(p[i] * p[0] for p in s["perf"]) / frames
            print(f"   {int(frames)} frames: interval {avg(1):.1f} ms (worst {max(p[2] for p in s['perf']):.1f}), "
                  f"work {avg(3):.1f} ms, gpu wait {avg(5):.1f} ms")
        if s["cpu"]:
            n = len(s["cpu"])
            print(f"   cpu per frame: render thread {sum(c[0] for c in s['cpu']) / n:.1f} ms, "
                  f"whole game {sum(c[1] for c in s['cpu']) / n:.1f} ms")
        print(f"   pipelines compiled on the render thread: {s['pso'][0]} ({s['pso'][1]} known, {s['pso'][2]} ms)")
    for line in lines:
        if "[remote] timeout" in line or "[remote] shot" in line or "[remote] state" in line:
            print("   " + line[line.find("[remote]"):])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--name", default="run")
    ap.add_argument("--template", required=True, help="data folder copied for the run (never used itself)")
    ap.add_argument("--script", default="")
    ap.add_argument("--script-file")
    ap.add_argument("--exe", default="wwe12.exe")
    ap.add_argument("--boot", type=int, default=25, help="seconds before the script is handed over")
    ap.add_argument("--limit", type=int, default=600, help="seconds before the game is stopped")
    ap.add_argument("--keep-caches", action="store_true", help="keep the template's pipelines.bin / declarations.bin")
    ap.add_argument("cvars", nargs="*", help="extra game options, e.g. --svr_pso_boot_objects=false")
    args = ap.parse_args()

    script = Path(args.script_file).read_text() if args.script_file else args.script
    out = REPO / "work" / "autotest" / args.name
    data = out / "data"
    if out.exists():
        shutil.rmtree(out)
    shutil.copytree(args.template, data)
    if not args.keep_caches:
        for f in ("pipelines.bin", "declarations.bin"):
            (data / "cache" / f).unlink(missing_ok=True)
    log = out / "game.log"
    pad = BUILD / "remote_pad.txt"
    pad.unlink(missing_ok=True)
    cmd = [str(BUILD / args.exe), "--user_data_root", str(data), "--log_file", str(log),
           "--log_flush_interval", "1", "--wwe12_remote_pad=true", *args.cvars]
    print("starting", " ".join(cmd))
    game = subprocess.Popen(cmd, cwd=BUILD)
    start = time.time()
    try:
        time.sleep(args.boot)
        pad.write_text(script, encoding="ascii")
        while game.poll() is None and time.time() - start < args.limit:
            time.sleep(1)
        if game.poll() is None:
            print(f"time limit ({args.limit} s): stopping the game started here (pid {game.pid})")
            game.kill()
    finally:
        pad.unlink(missing_ok=True)
    time.sleep(1)
    print(f"log: {log}")
    if log.exists():
        summary(log)
    shots = sorted((data / "logs" / "shots").glob("*.bmp")) if (data / "logs" / "shots").exists() else []
    for s in shots:
        print(f"shot: {s}")


if __name__ == "__main__":
    sys.exit(main())
