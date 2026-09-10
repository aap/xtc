#!/usr/bin/env python3
"""Compare an animation chunk against the .xan text it came from.

    tools/xanchkdiff.py samples/fox/fox.xan build/chk/fox_anim.chk [Clip ...]

Reads the chunk the way common/chunk.c does -- 32 byte header, then a
memory image whose pointer fields hold file offsets because tools/chk.ld
links everything at address zero -- walks the xAnimList in it and checks
every name, id, duration and key against the text, as the float the C
loader would have made of it.  Names the clips to compare only those, in
that order; with none, the chunk's clips have to be exactly the text's.

This is the offline half of verifying tools/xan2dsm.lua: it catches a
float the assembler parsed differently, a wrong offset, a dropped key.
The other half is drawing the same frame on the PS2 from both.
"""
import struct, sys

def f32(x):
    return struct.unpack("<f", struct.pack("<f", x))[0]

def read_text(path):
    anims = []
    a = c = None
    for line in open(path):
        t = line.split()
        if not t:
            continue
        if t[0] == "anim":
            name = line.split('"')[1]
            a = {"name": name, "duration": f32(float(t[-2])), "chans": []}
            anims.append(a)
        elif t[0] == "channel":
            name = line.split('"')[1]
            r = line.split('"')[2].split()
            c = {"name": name, "id": int(r[0]), "rot": [], "trans": [], "scale": []}
            a["chans"].append(c)
        elif t[0] in ("rot", "trans", "scale"):
            c[t[0]].append(tuple(f32(float(x)) for x in t[1:]))
    return anims

class Chunk:
    def __init__(self, path):
        self.d = open(path, "rb").read()
        h = struct.unpack("<8I", self.d[:32])
        (self.ident, self.shrink, self.fileEnd, self.dataEnd,
         self.relocTab, self.numRelocs, self.globalTab, self.globalEnd) = h
        assert self.ident == 0x41424344, "not a chunk: %08X" % self.ident
        assert self.fileEnd == len(self.d), "fileEnd %d, file %d" % (self.fileEnd, len(self.d))
        self.relocs = set(struct.unpack("<%dI" % self.numRelocs,
                          self.d[self.relocTab:self.relocTab + 4*self.numRelocs]))

    def u32(self, off):
        return struct.unpack("<I", self.d[off:off+4])[0]

    def i32(self, off):
        return struct.unpack("<i", self.d[off:off+4])[0]

    def f32(self, off):
        return struct.unpack("<f", self.d[off:off+4])[0]

    def ptr(self, off):
        """a pointer field: its value is a file offset, and the loader
        must have been told about the field"""
        v = self.u32(off)
        if v and off not in self.relocs:
            raise AssertionError("pointer at %#x not in the fixup table" % off)
        return v

    def string(self, off):
        end = self.d.index(b"\0", off)
        return self.d[off:end].decode()

def main():
    text, chunk = sys.argv[1], sys.argv[2]
    want = sys.argv[3:]
    anims = {a["name"]: a for a in read_text(text)}
    order = want or [a["name"] for a in read_text(text)]

    ck = Chunk(chunk)
    base = 32                           # the xAnimList is first in .data
    n = ck.i32(base)
    lst = ck.ptr(base + 4)
    if n != len(order):
        sys.exit("chunk has %d clips, expected %d" % (n, len(order)))

    bad = 0
    keys = 0
    for i, name in enumerate(order):
        a = anims[name]
        off = lst + 16*i
        cname = ck.string(ck.ptr(off))
        dur = ck.f32(off + 4)
        nch = ck.i32(off + 8)
        chans = ck.ptr(off + 12)
        if cname != name:
            print("clip %d: name %r, expected %r" % (i, cname, name)); bad += 1
        if dur != a["duration"]:
            print("%s: duration %r, expected %r" % (name, dur, a["duration"])); bad += 1
        if nch != len(a["chans"]):
            print("%s: %d channels, expected %d" % (name, nch, len(a["chans"]))); bad += 1
            continue
        for j, tc in enumerate(a["chans"]):
            co = chans + 32*j
            n2 = ck.string(ck.ptr(co))
            cid = ck.i32(co + 4)
            counts = [ck.i32(co + 8), ck.i32(co + 12), ck.i32(co + 16)]
            ptrs = [ck.ptr(co + 20), ck.ptr(co + 24), ck.ptr(co + 28)]
            if n2 != tc["name"] or cid != tc["id"]:
                print("%s/%d: %r id %d, expected %r id %d" %
                      (name, j, n2, cid, tc["name"], tc["id"])); bad += 1
            for k, (kind, sz) in enumerate((("rot", 20), ("trans", 16), ("scale", 16))):
                tk = tc[kind]
                if counts[k] != len(tk):
                    print("%s/%s/%s: %d keys, expected %d" %
                          (name, tc["name"], kind, counts[k], len(tk))); bad += 1
                    continue
                if len(tk) == 0:
                    if ptrs[k] != 0:
                        print("%s/%s/%s: empty but not nil" % (name, tc["name"], kind)); bad += 1
                    continue
                if ptrs[k] % 16:
                    print("%s/%s/%s: array at %#x is not qword aligned" %
                          (name, tc["name"], kind, ptrs[k])); bad += 1
                nf = 5 if kind == "rot" else 4
                for m, key in enumerate(tk):
                    o = ptrs[k] + sz*m
                    got = tuple(ck.f32(o + 4*x) for x in range(nf))
                    keys += 1
                    if got != key:
                        print("%s/%s/%s[%d]: %r, expected %r" %
                              (name, tc["name"], kind, m, got, key)); bad += 1
                        if bad > 20:
                            sys.exit("too many differences")
    print("%s: %d clips, %d keys, %d fixups -- %s" %
          (chunk, n, keys, ck.numRelocs, "%d differences" % bad if bad else "identical to " + text))
    sys.exit(1 if bad else 0)

main()
