#!/usr/bin/env python3
"""fn.py ADDR [ADDR...] : compact PPC listing of a recompiled function (asm comments of port/generated).
   fn.py --callers ADDR : functions that call ADDR (direct bl / b)."""
import sys, os, re, pickle, glob
GEN = r"C:\Users\melfo\Documents\WWE12-PC\wwe12-pc\port\generated\default"
IDX = os.path.join(os.environ.get("TEMP", "."), "wwe12_fn_idx.pkl")
def build():
    idx = {}
    for f in glob.glob(os.path.join(GEN, "wwe12_recomp.*.cpp")):
        with open(f, "rb") as fh:
            off = 0
            for line in fh:
                if line.startswith(b"DEFINE_REX_FUNC(sub_"):
                    idx[int(line[20:28], 16)] = (os.path.basename(f), off)
                off += len(line)
    pickle.dump(idx, open(IDX, "wb"))
    return idx
idx = pickle.load(open(IDX, "rb")) if os.path.exists(IDX) else build()
def listing(addr):
    f, off = idx[addr]
    out = []
    with open(os.path.join(GEN, f), "rb") as fh:
        fh.seek(off); fh.readline()
        for line in fh:
            if line.startswith(b"DEFINE_REX_FUNC(") or line.startswith(b"}"):
                if line.startswith(b"}"): break
            s = line.decode("latin-1").rstrip()
            t = s.strip()
            if t.startswith("// "): out.append("    " + t[3:])
            elif t.startswith("loc_") and t.endswith(":"): out.append(t)
            elif "ctx.lr = 0x" in t: out[-1] += "    ; ret " + t.split("0x")[1].rstrip(";")
    return out
def callers(addr):
    pat = re.compile(rb"// bl? 0x%08x\b" % addr)
    res = []
    for f in sorted(glob.glob(os.path.join(GEN, "wwe12_recomp.*.cpp"))):
        data = open(f, "rb").read()
        if not pat.search(data): continue
        cur = None
        for line in data.split(b"\n"):
            if line.startswith(b"DEFINE_REX_FUNC(sub_"): cur = line[16:28].decode()
            elif pat.search(line): res.append(cur)
    return res
if sys.argv[1] == "--callers":
    for a in sys.argv[2:]:
        c = callers(int(a, 16)); print(a, "<-", len(c), sorted(set(c)))
else:
    for a in sys.argv[1:]:
        a = int(a, 16); print("==== sub_%08X" % a); print("\n".join(listing(a)))
