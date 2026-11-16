import sys, os, re, glob, collections
GEN = r"C:\Users\melfo\Documents\WWE12-PC\wwe12-pc\port\generated\default"
off = sys.argv[1].encode()
res = collections.defaultdict(list)
for f in sorted(glob.glob(os.path.join(GEN, "wwe12_recomp.*.cpp"))):
    data = open(f, "rb").read()
    if (b"," + off + b"(r") not in data: continue
    cur = None; pend = 0; reg = None
    for line in data.split(b"\n"):
        if line.startswith(b"DEFINE_REX_FUNC(sub_"): cur = line[16:28].decode(); pend = 0
        elif line.startswith(b"\t// "):
            s = line[4:].decode("latin-1").strip()
            m = re.match(r"lwz (r\d+),%s\(r\d+\)" % off.decode(), s)
            if m: pend = 4; reg = m.group(1); continue
            if pend:
                pend -= 1
                m = re.match(r"cmpl?wi cr\d,%s,(-?\d+)" % reg, s)
                if m: res[cur].append(int(m.group(1))); pend = 0
for k in sorted(res): print(k, res[k])
