"""Hottest functions of a SVR_SAMPLE_STACKS profile (Documents\\wwe12\\logs\\samples.txt).

The game's profiler (crash_trace.cpp, SVR_SAMPLE_STACKS=<ms>) writes one line per thread per
sample: "<ms> <tid> <thread name> module+0xRVA;module+0xRVA;..." (innermost frame first). This
names the frames from the exe's linker map (dist/symbols-v<ver>/wwe12.map, or the build's) and
prints, per thread, the functions where the samples land ("self": innermost frame of the exe)
and the functions on the stack ("total": counted once per sample). Recompiled game functions
keep their Xbox 360 address in their name (sub_82AA7040), so the list is the start of a
"rewrite it natively" plan.

    python -I tools/samples_top.py samples.txt wwe12.map [--thread NAME] [--top 10] [--from MS] [--to MS]
"""
import argparse
import bisect
import collections
import re
import sys

MAP_LINE = re.compile(r"^\s*[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{16})\s")


def load_map(path):
    base = 0x140000000
    symbols = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if "Preferred load address is" in line:
                base = int(line.split()[-1], 16)
                continue
            m = MAP_LINE.match(line)
            if m and m.group(1)[0] not in "?$":
                symbols.append((int(m.group(2), 16) - base, m.group(1)))
    symbols.sort()
    return [s[0] for s in symbols], [s[1] for s in symbols]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("samples")
    ap.add_argument("map")
    ap.add_argument("--exe", default="wwe12", help="module name prefix of the exe (wwe12, wwe12_d3d12...)")
    ap.add_argument("--thread", help="only threads whose name contains this")
    ap.add_argument("--top", type=int, default=10)
    ap.add_argument("--from", dest="start", type=int, default=0, help="first ms kept")
    ap.add_argument("--to", type=int, default=1 << 62, help="last ms kept")
    args = ap.parse_args()

    rvas, names = load_map(args.map)

    def name_of(rva):
        i = bisect.bisect_right(rvas, rva) - 1
        return names[i].removeprefix("__imp__") if i >= 0 else "?"

    self_counts = collections.defaultdict(collections.Counter)
    total_counts = collections.defaultdict(collections.Counter)
    samples = collections.Counter()
    with open(args.samples, encoding="utf-8", errors="replace") as f:
        for line in f:
            parts = line.split(" ", 3)
            if len(parts) < 4 or not parts[0].isdigit():
                continue
            ms, thread, frames = int(parts[0]), parts[2], parts[3].strip()
            if not (args.start <= ms <= args.to) or (args.thread and args.thread not in thread):
                continue
            samples[thread] += 1
            seen = set()
            first = True
            for frame in frames.split(";"):
                module, _, off = frame.partition("+0x")
                if not module.lower().startswith(args.exe.lower()) or not off:
                    continue
                fn = name_of(int(off, 16))
                if first:
                    self_counts[thread][fn] += 1
                    first = False
                if fn not in seen:
                    seen.add(fn)
                    total_counts[thread][fn] += 1

    for thread, n in samples.most_common():
        print(f"== {thread}: {n} samples")
        print("   self (where the samples land)")
        for fn, c in self_counts[thread].most_common(args.top):
            print(f"   {100.0 * c / n:5.1f} %  {fn}")
        print("   total (on the stack)")
        for fn, c in total_counts[thread].most_common(args.top):
            print(f"   {100.0 * c / n:5.1f} %  {fn}")


if __name__ == "__main__":
    sys.exit(main())
