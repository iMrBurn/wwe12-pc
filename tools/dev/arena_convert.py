"""Convert a SvR 2011 arena (pac/bg/bgNN.pac) for WWE '12 by completing it with entries of a WWE '12 arena.

Usage: python -I tools/dev/arena_convert.py <svr_arena.pac> <wwe12_donor.pac> <out.pac>
                                            [--add 980,30002,800,804,809] [--replace 995,996,997,999]
                                            [--stage NN]

Both files are the player's own game data: the output is a derived game file, never commit or ship it.

Layout (docs/limit_breaking.md, "Arènes"): EPAC header (+8 = PACH size rounded up to 0x800), at 0x800 a
"STG " record (+0x0C = 4-digit stage number in ASCII, +0x14 = PACH size in 256-byte units, rounded up),
then at 0x4000 a PACH: "PACH", count, {id, offset, size} (little endian), entry data from the end of the
table, each entry 4-byte aligned (most are Yuke's BPE blocks; they are copied as they are). The file ends with
a 0x800-byte block after the rounded PACH holding the packer's signature.

--add copies entries the SvR file lacks (980 ring ropes, 30002, 800/804/809 video screens by default);
--replace takes the donor's version of entries present in both (none by default); --remove drops entries.
Ring (bg15 test): 951 posts, 952/964 turnbuckles and covers, 953 mat, 956/960 ropes, 973/974 apron, 980 WWE '12
ropes; SvR has four copies of some (957-959, 961-963, 965-967).
--models converts every SvR 2011 JBOY model (top-level entries and those inside sub-PACHs such as 20000/20010)
to the WWE '12 layout (tools/dev/jboy.py) and recompresses the changed entries (yukes_bpe.compress). The game
loads an arena into a fixed buffer: the output must not be bigger than the shipped file of the arena it replaces
(bg15: 10,063,872 bytes; 10,074,112 already crashed). The texture package keeps Yuke's compressed blocks
(yukes_bpe.compress_like) to stay under it.
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import jboy  # noqa: E402
import yukes_bpe  # noqa: E402

PACH_AT = 0x4000


def read_pach(data):
    if data[PACH_AT:PACH_AT + 4] != b"PACH":
        raise SystemExit("no PACH at 0x4000")
    count = struct.unpack_from("<I", data, PACH_AT + 4)[0]
    base = PACH_AT + 8 + count * 12
    entries = {}
    for i in range(count):
        eid, off, size = struct.unpack_from("<III", data, PACH_AT + 8 + i * 12)
        entries[eid] = data[base + off:base + off + size]
    return entries


SKIPPED = []
SCALE = None  # --scale: SvR 2011 counts in 10 cm, WWE '12 in cm


LIGHT_CHANNELS_SCALED = {10, 11, 12, 13}


def scale_lights(data, factor, channels=LIGHT_CHANNELS_SCALED):
    """Arena light banks (YMKs 30000/31010, same format in both games): u32 track count at 0x110, 16-byte
    entries {y, x, id, offset, frames, 0} at 0x114, then the tracks. A frame is 26 bytes: 0x79, a repeat
    count, 3 floats, 12 more bytes (colour, strength...); event bytes may sit between frames.
    Channel y = 10-13 holds positions and aimed points (world units: SvR 2011 counts in 10 cm). Channels 0
    and 1 are not: the beam motions 101-120 (objects m(101)... in 50001) keep (0, +-1000, 0) in both games
    (scaled, the beams grew 10 times longer, v15/v16); 50-55 are equal byte for byte. Unscaled, the beams'
    aimed points (e.g. 4677, -2677, -11054) sat 10 times too close and every beam lit the ring (v13-v17)."""
    out = bytearray(data)
    count = struct.unpack_from("<I", data, 0x110)[0]
    base = 0x114 + count * 16
    ents = [struct.unpack_from("<BBHIII", data, 0x114 + i * 16) for i in range(count)]
    offsets = sorted(e[3] for e in ents)
    nxt = dict(zip(offsets, offsets[1:] + [len(data) - base]))
    for start, end in sorted((e[3], nxt[e[3]]) for e in ents if e[0] in channels):
        pos, end = base + start, base + end
        while pos + 26 <= end:
            if out[pos] != 0x79:
                pos += 1
                continue
            for k in range(3):
                at = pos + 2 + 4 * k
                struct.pack_into("<f", out, at, struct.unpack_from("<f", out, at)[0] * factor)
            pos += 26
    return bytes(out)


def read_tracks(data):
    """Light bank tracks: {(id, x, y): (frames, runtime, bytes)}; a track runs to the next one's offset."""
    count = struct.unpack_from("<I", data, 0x110)[0]
    base = 0x114 + count * 16
    ents = [struct.unpack_from("<BBHIII", data, 0x114 + i * 16) for i in range(count)]
    starts = sorted(e[3] for e in ents) + [len(data) - base]
    nxt = dict(zip(starts, starts[1:]))
    return {(i, x, y): (frames, rt, data[base + off:base + nxt[off]]) for y, x, i, off, frames, rt in ents}


def merge_lights(svr, donor, ids):
    """The SvR light bank with the donor's tracks for the light sets `ids` (e.g. 10-14, the match lights:
    SvR 2011's flood the ring with a white glare in WWE '12). Same 0x110-byte header in both games."""
    tracks = read_tracks(svr)
    for key, track in read_tracks(donor).items():
        if key[0] in ids:
            tracks[key] = track
    for key in [k for k in tracks if k[0] in ids and k not in read_tracks(donor)]:
        del tracks[key]
    keys = sorted(tracks, key=lambda k: (k[0] << 16) | (k[1] << 8) | k[2])
    table, body = bytearray(), bytearray()
    for i, x, y in keys:
        frames, rt, blob = tracks[(i, x, y)]
        table += struct.pack("<BBHIII", y, x, i, len(body), frames, rt)
        body += blob
    return svr[:0x110] + struct.pack("<I", len(keys)) + bytes(table) + bytes(body)


def entrance_only(data, objects):
    """50001 (per-object flags, explained in Japanese at the end of the file): add `e` ("shown during events
    only", i.e. entrances) to the given objects. SvR 2011's beam cones (a z m(101...) b) flood the ring with
    white light during WWE '12 matches; WWE '12's own cones are flagged `w` (match: move camera only)."""
    lines = data.split(b"\r\n")
    for i, line in enumerate(lines):
        words = line.split()
        if words and words[0].isdigit() and int(words[0]) in objects and b"e" not in words[1:]:
            lines[i] = line.rstrip() + b" e"
    return b"\r\n".join(lines)


def scale_text(eid, data, factor):
    """Placement texts in world units: 50003 (light zones: first 4 numbers of each data line) and 50020
    (visibility boxes: the numbers between braces). Other entries are returned unchanged."""
    if eid not in (50003, 50020):
        return data
    def fmt(x):
        # Always a decimal point and no exponent, as the game's files ("0.0", "960.0"): a bare integer
        # in 50020 made the game's reader lose its place (crash on loading the v10).
        text = ("%.6f" % (float(x) * factor)).rstrip("0")
        return (text + "0" if text.endswith(".") else text).encode()
    lines = data.split(b"\r\n")
    inside = False
    for i, line in enumerate(lines):
        if eid == 50020:
            if line.strip() == b"{":
                inside = True
            elif line.strip() == b"}":
                inside = False
            elif inside:
                lines[i] = b"\t" + b" ".join(fmt(x) for x in line.split())
        elif line.strip() and not line.lstrip().startswith((b"//", b";")):
            body, sep, comment = line.partition(b";")
            parts = body.split()
            if len(parts) >= 4:
                lines[i] = b"  ".join([fmt(x) for x in parts[:4]] + parts[4:]) + b"     " + sep + comment
    return b"\r\n".join(lines)


def read_sub_pach(data):
    count = struct.unpack_from("<I", data, 4)[0]
    base = 8 + count * 12
    return {eid: data[base + off:base + off + size]
            for eid, off, size in (struct.unpack_from("<III", data, 8 + i * 12) for i in range(count))}


def convert_models(data):
    """Return (new data, models converted) for a decompressed entry, or (None, 0) when unchanged."""
    if data[:4] == b"JBOY" and b"POF0" in data:
        try:
            new = jboy.svr_to_w12(data)
            return (jboy.scale(new, SCALE) if SCALE else new), 1
        except ValueError as err:
            SKIPPED.append((jboy.u32(data, 0x18), jboy.u32(data, 0x1C), str(err)))
            return None, 0
    if data[:4] == b"PACH" and b"JBOY" in data:
        subs = read_sub_pach(data)
        done = 0
        for eid, sub in subs.items():
            new, n = convert_models(sub)
            if new is not None:
                subs[eid], done = new, done + n
        return (write_pach(subs), done) if done else (None, 0)
    return None, 0


def write_pach(entries):
    ids = sorted(entries)
    table = bytearray(b"PACH" + struct.pack("<I", len(ids)))
    body = bytearray()
    for eid in ids:
        while len(body) % 4:
            body.append(0)
        table += struct.pack("<III", eid, len(body), len(entries[eid]))
        body += entries[eid]
    return bytes(table + body)


def read_textures(data):
    """Texture package (entries 400-402): count, 0x100, 0, 0x10, then 32-byte records {name[16], "dds", size,
    offset, 0}, offsets from the start of the package, data aligned on 16."""
    count = struct.unpack_from("<I", data, 0)[0]
    out = {}
    for i in range(count):
        rec = 16 + i * 32
        name = data[rec:rec + 16].rstrip(b"\0")
        ext = data[rec + 16:rec + 20]
        size, off = struct.unpack_from("<II", data, rec + 20)
        out[name] = (ext, data[off:off + size])
    return data[4:16], out


def write_textures(head, textures):
    table = bytearray(struct.pack("<I", len(textures)) + head)
    body = bytearray()
    start = 16 + len(textures) * 32
    for name, (ext, blob) in textures.items():
        table += name.ljust(16, b"\0") + ext + struct.pack("<III", len(blob), start + len(body), 0)
        body += blob + bytes(-len(blob) % 16)
    return bytes(table + body)


def add_textures(svr_pkg, donor_pkg, names, unused):
    """Copy donor textures into the SvR package (WWE '12 ring: mat_noize...); same name = replaced.
    The table keeps its size and the old data its place, so that yukes_bpe.compress_like() can keep Yuke's own
    compressed blocks (ours are ~16 % bigger, and an arena must not outgrow its shipped file): a new name takes
    the record of a texture no model uses any more (`unused`), the data goes at the end of the package."""
    donor = read_textures(donor_pkg)[1]
    count = struct.unpack_from("<I", svr_pkg, 0)[0]
    records = {svr_pkg[16 + i * 32:16 + i * 32 + 16].rstrip(b"\0"): 16 + i * 32 for i in range(count)}
    # Old SvR ring textures first; never a light map, titantron or movie (the game may look those up itself).
    risky = (b"light", b"titan", b"mov")
    spare = [records[n] for n in sorted(unused, key=lambda n: not n.startswith(b"rin_"))
             if n in records and n not in names and not any(w in n for w in risky)]
    out = bytearray(svr_pkg)
    done = []
    for name in names:
        if name not in donor:
            continue
        rec = records.get(name) or (spare.pop(0) if spare else None)
        if rec is None:
            raise SystemExit(f"no free texture record for {name.decode()}: give more unused textures")
        ext, blob = donor[name]
        out += bytes(-len(out) % 16)
        out[rec:rec + 32] = name.ljust(16, b"\0") + ext + struct.pack("<III", len(blob), len(out), 0)
        out += blob
        done.append(name)
    return bytes(out), done


def used_names(entries):
    """Every JBOY (also inside sub-PACHs) as one blob, to test which texture names models still use."""
    blobs = []
    for data in entries.values():
        data = yukes_bpe.decompress_at(data, 0) if data[:4] == b"BPE " else data
        if data is None:
            continue
        if data[:4] == b"JBOY":
            blobs.append(data)
        elif data[:4] == b"PACH" and b"JBOY" in data:
            blobs += [d for d in read_sub_pach(data).values() if d[:4] == b"JBOY"]
    return b"\0".join(blobs)


def ids(text):
    return [int(x) for x in text.split(",") if x.strip()] if text else []


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("svr")
    ap.add_argument("donor")
    ap.add_argument("out")
    ap.add_argument("--add", default="980,30002,800,804,809")
    ap.add_argument("--replace", default="")
    ap.add_argument("--remove", default="", help="entries to drop (e.g. SvR-only ring pieces 957-959,961-963,965-967)")
    ap.add_argument("--stage", type=int, default=None, help="stage number written in the STG record")
    ap.add_argument("--models", action="store_true", help="convert the SvR JBOY models to WWE '12")
    ap.add_argument("--textures", default="",
                    help="donor textures copied into package 400 (e.g. mat_noize,rin_ep01,rin_tb00_n,rin_tb00_s)")
    ap.add_argument("--scale", type=float, default=None,
                    help="with --models: scale geometry, placement texts and lights (10: SvR 10 cm -> WWE '12 cm)")
    ap.add_argument("--donor-lights", default="",
                    help="light sets taken from the donor's banks 30000/31010 (e.g. 10,11,12,13,14 = match lights)")
    ap.add_argument("--bg-shaders", default="",
                    help="converted models whose shaders become WWE '12 background ones (yDefault -> yBG_Default...)")
    ap.add_argument("--dim", default="",
                    help="FACTOR:ids, material colours of these models multiplied (e.g. 0.3:230,231 for glaring cones)")
    ap.add_argument("--entrance-only", default="",
                    help="objects shown during entrances only (flag e in 50001), e.g. the SvR beam cones")
    args = ap.parse_args()
    global SCALE
    SCALE = args.scale

    svr = open(args.svr, "rb").read()
    donor = open(args.donor, "rb").read()
    entries = read_pach(svr)
    donor_entries = read_pach(donor)

    if args.models:
        total = 0
        for eid, packed in entries.items():
            data = yukes_bpe.decompress_at(packed, 0) if packed[:4] == b"BPE " else packed
            if data is None:
                continue
            new, n = convert_models(data)
            if new is None and SCALE and scale_text(eid, data, SCALE) != data:
                new = scale_text(eid, data, SCALE)
                print(f"{eid}: placement text scaled x{SCALE:g}")
            if new is None and SCALE and eid in (30000, 31010) and data[:4] == b"YMKs":
                new = scale_lights(data, SCALE)
                print(f"{eid}: light positions scaled x{SCALE:g}")
            if new is not None:
                entries[eid] = yukes_bpe.compress(new)
                total += n
        print(f"models converted: {total}, left as they are: {len(SKIPPED)} {sorted(set(SKIPPED))[:8]}")

    for eid in ids(args.bg_shaders):
        data = yukes_bpe.decompress_at(entries[eid], 0) if entries[eid][:4] == b"BPE " else entries[eid]
        if data[:4] == b"JBOY":
            entries[eid] = yukes_bpe.compress(jboy.bg_shaders(data))
    if args.bg_shaders:
        print(f"background shaders on {len(ids(args.bg_shaders))} models")
    if args.dim:
        factor, _, which = args.dim.partition(":")
        for eid in ids(which):
            data = yukes_bpe.decompress_at(entries[eid], 0) if entries[eid][:4] == b"BPE " else entries[eid]
            entries[eid] = yukes_bpe.compress(jboy.dim(data, float(factor)))
        print(f"material colours x{factor} on {len(ids(which))} models")
    for eid in ids(args.add):
        if eid in entries:
            print(f"{eid}: already in the SvR arena, kept")
        elif eid in donor_entries:
            entries[eid] = donor_entries[eid]
            print(f"{eid}: added from the donor ({len(donor_entries[eid])} bytes)")
        else:
            print(f"{eid}: not in the donor, skipped")
    for eid in ids(args.remove):
        if entries.pop(eid, None) is not None:
            print(f"{eid}: removed")
    for eid in ids(args.replace):
        if eid in donor_entries:
            entries[eid] = donor_entries[eid]
            print(f"{eid}: replaced by the donor's")
    if args.entrance_only:
        objects = set(ids(args.entrance_only))
        old = yukes_bpe.decompress_at(entries[50001], 0) if entries[50001][:4] == b"BPE " else entries[50001]
        new = entrance_only(old, objects)
        entries[50001] = yukes_bpe.compress(new)
        print(f"50001: {new.count(b' e') - old.count(b' e')} objects set to entrances only")
    if args.donor_lights:
        sets = set(ids(args.donor_lights))
        unpack = lambda p: yukes_bpe.decompress_at(p, 0) if p[:4] == b"BPE " else p  # noqa: E731
        for eid in (30000, 31010):
            if eid in entries and eid in donor_entries:
                entries[eid] = yukes_bpe.compress(merge_lights(unpack(entries[eid]), unpack(donor_entries[eid]), sets))
                print(f"{eid}: light sets {sorted(sets)} from the donor")
    if args.textures:
        names = [n.strip().encode() for n in args.textures.split(",") if n.strip()]
        unpack = lambda p: yukes_bpe.decompress_at(p, 0) if p[:4] == b"BPE " else p  # noqa: E731
        svr_pkg = unpack(entries[400])
        models = used_names(entries)
        unused = [n for n in read_textures(svr_pkg)[1] if n + b"\0" not in models]
        pkg, done = add_textures(svr_pkg, unpack(donor_entries[400]), names, unused)
        entries[400] = yukes_bpe.compress_like(pkg, entries[400]) if entries[400][:4] == b"BPE " \
            else yukes_bpe.compress(pkg)
        print(f"400: textures from the donor: {[n.decode() for n in done]}, missing: "
              f"{[n.decode() for n in names if n not in done]}; unused before: {[n.decode() for n in unused]}; "
              f"Yuke's blocks kept: {getattr(yukes_bpe.compress_like, 'reused', 0)}")

    pach = write_pach(entries)
    rounded = (len(pach) + 0x7FF) & ~0x7FF
    header = bytearray(svr[:PACH_AT])
    struct.pack_into("<I", header, 8, rounded)
    struct.pack_into("<I", header, 0x800 + 0x14, (len(pach) + 0xFF) // 0x100)
    if args.stage is not None:
        header[0x800 + 0x0C:0x800 + 0x10] = b"%04d" % args.stage
    # Last 0x800 bytes: the packer's signature ("EOP5/plugin version 1.25", WWE '12; 1.26, SvR 2011).
    out = bytes(header) + pach + bytes(rounded - len(pach)) + svr[-0x800:]
    open(args.out, "wb").write(out)
    print(f"{args.out}: {len(entries)} entries, {len(out)} bytes")
    if len(out) > len(donor):
        print(f"WARNING: {len(out) - len(donor)} bytes over the donor arena's shipped size ({len(donor)}): "
              "the game will crash or hang")


if __name__ == "__main__":
    main()
