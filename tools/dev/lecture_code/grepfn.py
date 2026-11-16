#!/usr/bin/env python3
"""grepfn.py REGEX : functions whose asm comments match (prints fn, line)."""
import sys, os, re, glob
GEN = r"C:\Users\melfo\Documents\WWE12-PC\wwe12-pc\port\generated\default"
pat = re.compile(sys.argv[1].encode())
mx = int(sys.argv[2]) if len(sys.argv) > 2 else 200
n = 0
for f in sorted(glob.glob(os.path.join(GEN, "wwe12_recomp.*.cpp"))):
    data = open(f, "rb").read()
    if not pat.search(data): continue
    cur = None
    for line in data.split(b"\n"):
        if line.startswith(b"DEFINE_REX_FUNC(sub_"): cur = line[16:28].decode()
        elif line.startswith(b"\t// ") and pat.search(line):
            print(cur, line.decode("latin-1").strip()[3:]); n += 1
            if n >= mx: sys.exit()
