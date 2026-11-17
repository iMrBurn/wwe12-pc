#!/usr/bin/env python3
"""mem.py ADDR [LEN]      : hex dump of the game's static image at guest address (image.bin, base 0x82000000)
   mem.py -s ADDR         : C string at ADDR
   mem.py -w ADDR N       : N big-endian u32 words
   mem.py -f TEXT         : guest addresses where ASCII TEXT occurs
   mem.py -x ADDR         : places in the image holding the u32 ADDR (data pointers / vtables)"""
import sys, struct
IMG = open(r"C:\Users\melfo\Documents\WWE12-PC\wwe12-pc\image.bin", "rb").read(); BASE = 0x82000000
a = sys.argv
if a[1] == "-s":
    o = int(a[2], 16) - BASE; print(IMG[o:IMG.index(b"\0", o)].decode("latin-1"))
elif a[1] == "-w":
    o = int(a[2], 16) - BASE; n = int(a[3])
    for i in range(0, n, 8): print("%08X: " % (BASE + o + i * 4) + " ".join("%08X" % w for w in struct.unpack(">%dI" % min(8, n - i), IMG[o + i * 4:o + (i + min(8, n - i)) * 4])))
elif a[1] == "-f":
    t = a[2].encode(); o = -1
    while (o := IMG.find(t, o + 1)) >= 0: print("%08X" % (BASE + o), IMG[max(0,o-0):o + 48].split(b"\0")[0].decode("latin-1"))
elif a[1] == "-x":
    t = struct.pack(">I", int(a[2], 16)); o = -1
    while (o := IMG.find(t, o + 1)) >= 0:
        if o % 4 == 0: print("%08X" % (BASE + o))
else:
    o = int(a[1], 16) - BASE; n = int(a[2]) if len(a) > 2 else 64
    for i in range(0, n, 16): print("%08X: %s  %s" % (BASE + o + i, IMG[o + i:o + i + 16].hex(" "), "".join(chr(c) if 32 <= c < 127 else "." for c in IMG[o + i:o + i + 16])))
