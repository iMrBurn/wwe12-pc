import sys, re, io, contextlib
sys.argv, args = [sys.argv[0], "--callers"], sys.argv[1:]
with contextlib.redirect_stdout(io.StringIO()):
    import fn
pat = re.compile(args[0]); after = int(args[1]); before = int(args[2])
for a in args[3:]:
    L = fn.listing(int(a, 16)); print("==== sub_%s (%d lines)" % (a, len(L)))
    shown = set()
    for i, s in enumerate(L):
        if pat.search(s):
            for j in range(max(0, i - before), min(len(L), i + after + 1)):
                if j not in shown: shown.add(j); print(("%4d " % j) + L[j][:64])
            print("     ...")
