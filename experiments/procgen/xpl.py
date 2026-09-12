#!/usr/bin/env python3
"""
xpl.py -- offline generator/inspector for xtc prim list files (.xpl).

A .xpl file is a 16-byte header followed by a position-independent PS2
DMA chain, exactly what xtcpBuildList records on the console: per batch
one DMAcnt tag (DMAret on the last) whose payload interleave-UNPACKs
the vertex attributes into VU memory and kicks the microprogram.  The
runtime loader (loadPrimList in scenes.c) mmaps this behind an
xtcPrimList and xtcPrimListDraw runs it unmodified.

Only the nolight pipeline is generated for now: one vertex = position
(4 floats, w=0), texcoord (2 floats), color (4 bytes RGBA 0-255).
No transforms, no hierarchy, no materials -- think of it as a binary
.obj with vertex colors.

Writer mirrors xtc's immediate mode:

    import xpl
    m = xpl.PrimList('tristrip')        # or 'trilist'
    m.color(255, 160, 0)                # sticky, alpha defaults 255
    m.uv(0.0, 0.0)                      # sticky, optional (unused untextured)
    m.vertex(x, y, z)
    m.restart()                         # tristrip only: start a new strip
    m.save('assets/thing.xpl')

CLI:
    xpl.py info file.xpl                # header + batch structure
    xpl.py diff a.xpl b.xpl             # structural + epsilon compare
    xpl.py preview file.xpl out.png     # software-rendered turntable
"""

import math, struct, sys

# ---- the wire format ------------------------------------------------------

XPL_IDENT = 0x304C5058          # "XPL0"

PIPE_TWOD, PIPE_NOLIGHT, PIPE_DEFAULT = 0, 1, 2

POINTS, LINELIST, LINESTRIP, TRILIST, TRISTRIP = range(5)
PRIMNAME = ['points', 'linelist', 'linestrip', 'trilist', 'tristrip']
PRIMSIZE   = [1, 2, 2, 3, 3]
PRIMREPEAT = [0, 0, 1, 0, 2]

# nolight microcode (im3d.dsm): vertexTop 0x3f0, 3 in / 3 out attribs,
# double buffered -> vertCount = (0x3f0-2)//(3*2+3*2) = 83
_VC = (0x3F0 - 2)//(3*2 + 3*2)
NOLIGHT_NUMVERTS = [
    _VC & ~3,                           # points / non-ref strips
    ((_VC//2) & ~3)*2,                  # line list
    (((_VC & ~3) - 1) & ~3) + 1,        # line strip
    ((_VC//3) & ~3)*3,                  # tri list
    (((_VC & ~3) - 2) & ~3) + 2,        # tri strip
]

# attrib: (name, vu offset, unpack cmd byte, usn, bytes/vert)
NOLIGHT_ATTRIBS = [
    ('pos', 0, 0x6C, 0, 16),            # V4_32
    ('uv',  1, 0x64, 0, 8),             # V2_32
    ('rgba', 2, 0x6E, 1, 4),            # V4_8 | USN
]
NOLIGHT_STRIDE = 3                      # qwords per vertex in VU memory

DMAcnt, DMAret = 0x10000000, 0x60000000
UNPACK_USN, UNPACK_DBLBUF = 0x4000, 0x8000

def VIF(imm, num, cmd): return imm | num << 16 | cmd << 24
def NOP():              return 0
def STCYCL(cl, wl):     return VIF(cl | wl << 8, 0, 0x01)
def ITOP(n):            return VIF(n, 0, 0x04)
def MSCALF(a):          return VIF(a, 0, 0x15)
def MSCNT():            return VIF(0, 0, 0x17)
def FLUSH():            return VIF(0, 0, 0x11)

# ---- writer ---------------------------------------------------------------

class PrimList:
    def __init__(self, prim='tristrip'):
        if prim not in ('tristrip', 'trilist'):
            raise ValueError('prim must be tristrip or trilist')
        self.prim = PRIMNAME.index(prim)
        self.verts = []                 # (x,y,z, s,t, r,g,b,a)
        self._uv = (0.0, 0.0)
        self._col = (255, 255, 255, 255)
        self._pos = (0.0, 0.0, 0.0)
        self._restart = False

    def color(self, r, g, b, a=255):
        self._col = (int(r) & 0xFF, int(g) & 0xFF, int(b) & 0xFF, int(a) & 0xFF)

    def uv(self, s, t):
        self._uv = (float(s), float(t))

    def _kick(self):
        self.verts.append(self._pos + self._uv + self._col)

    def vertex(self, x, y, z):
        self._pos = (float(x), float(y), float(z))
        self._kick()
        if self._restart:               # doubled first vertex of the new strip
            self._kick()
            self._restart = False

    def restart(self):
        # exactly xtcRestartStrip: re-emit the last vertex, then the
        # next vertex comes twice -- two degenerate join vertices
        if self.prim != TRISTRIP:
            raise ValueError('restart only works on tristrips')
        self._kick()
        self._restart = True

    # xtcpGetBatchInfo
    def _batchinfo(self):
        n = len(self.verts)
        repeat = PRIMREPEAT[self.prim]
        primsz = PRIMSIZE[self.prim]
        batchSize = NOLIGHT_NUMVERTS[self.prim]
        if self.prim & 1:               # list
            numPrims = n//primsz
            numPrimsBatch = batchSize//primsz
        else:                           # strip
            numPrims = n - repeat
            numPrimsBatch = batchSize - repeat
        if numPrims < 1:
            raise ValueError('nothing to draw (%d verts)' % n)
        numBatches = (numPrims + numPrimsBatch - 1)//numPrimsBatch
        lastBatchSize = n - (numBatches - 1)*(batchSize - repeat)
        return batchSize, lastBatchSize, numBatches, repeat

    def chain(self):
        batchSize, lastBatchSize, numBatches, repeat = self._batchinfo()
        out = bytearray()
        call = MSCALF(0)
        wait = NOP()
        base = 0
        for i in range(numBatches):
            last = i == numBatches - 1
            count = lastBatchSize if last else batchSize
            if last:
                wait = FLUSH()
            batch = bytearray()
            batch += struct.pack('<IIII', 0, 0, NOP(), STCYCL(NOLIGHT_STRIDE, 1))
            vs = self.verts[base:base + count]
            for name, off, cmd, usn, nbytes in NOLIGHT_ATTRIBS:
                imm = UNPACK_DBLBUF | (UNPACK_USN if usn else 0) | off
                batch += struct.pack('<I', VIF(imm, count & 0xFF, cmd))
                if name == 'pos':
                    for v in vs:
                        batch += struct.pack('<4f', v[0], v[1], v[2], 0.0)
                elif name == 'uv':
                    for v in vs:
                        batch += struct.pack('<2f', v[3], v[4])
                else:
                    for v in vs:
                        batch += bytes(v[5:9])
            batch += struct.pack('<III', ITOP(count), call, wait)
            while len(batch) & 0xF:
                batch += struct.pack('<I', NOP())
            qwc = len(batch)//16
            tag = (DMAret if last else DMAcnt) + qwc - 1
            struct.pack_into('<I', batch, 0, tag)
            out += batch
            call = MSCNT()
            base += count - repeat
        return bytes(out)

    def save(self, path):
        chain = self.chain()
        with open(path, 'wb') as f:
            f.write(struct.pack('<IIII', XPL_IDENT, PIPE_NOLIGHT,
                                self.prim, len(chain)))
            f.write(chain)
        return len(chain) + 16

# ---- reader ---------------------------------------------------------------

class Batch:
    __slots__ = ('tagid', 'qwc', 'itop', 'kick', 'wait', 'verts')

def parse(path):
    """-> (pipe, primtype, [Batch]); verts as writer tuples."""
    data = open(path, 'rb').read()
    ident, pipe, prim, size = struct.unpack_from('<IIII', data, 0)
    if ident != XPL_IDENT:
        raise ValueError('%s: not an xpl file' % path)
    if len(data) != 16 + size:
        raise ValueError('%s: size mismatch (header %d, file %d)'
                         % (path, size, len(data) - 16))
    if pipe != PIPE_NOLIGHT:
        raise ValueError('%s: can only parse nolight files' % path)

    batches = []
    p = 16
    end = 16 + size
    while p < end:
        b = Batch()
        tag, _, nop, stcycl = struct.unpack_from('<IIII', data, p)
        b.tagid = tag & 0x70000000
        b.qwc = tag & 0xFFFF
        if b.tagid not in (DMAcnt, DMAret):
            raise ValueError('bad dma tag %08X at %d' % (tag, p))
        if stcycl != STCYCL(NOLIGHT_STRIDE, 1):
            raise ValueError('bad stcycl %08X at %d' % (stcycl, p))
        q = p + 16
        verts = None
        for name, off, cmd, usn, nbytes in NOLIGHT_ATTRIBS:
            (code,) = struct.unpack_from('<I', data, q)
            if code >> 24 != cmd:
                raise ValueError('expected %s unpack at %d, got %08X'
                                 % (name, q, code))
            num = code >> 16 & 0xFF
            imm = code & 0xFFFF
            want = UNPACK_DBLBUF | (UNPACK_USN if usn else 0) | off
            if imm != want:
                raise ValueError('bad %s unpack imm %04X at %d' % (name, imm, q))
            q += 4
            n = num if num else 256
            if verts is None:
                verts = [[] for _ in range(n)]
            if name == 'pos':
                for i in range(n):
                    verts[i] += struct.unpack_from('<3f', data, q + 16*i)
                q += 16*n
            elif name == 'uv':
                for i in range(n):
                    verts[i] += struct.unpack_from('<2f', data, q + 8*i)
                q += 8*n
            else:
                for i in range(n):
                    verts[i] += list(data[q + 4*i:q + 4*i + 4])
                q += 4*n
        b.itop, b.kick, b.wait = struct.unpack_from('<III', data, q)
        if b.itop != ITOP(len(verts)):
            raise ValueError('itop %08X vs %d verts at %d'
                             % (b.itop, len(verts), p))
        b.verts = [tuple(v) for v in verts]
        batches.append(b)
        p += 16 + b.qwc*16
    if p != end:
        raise ValueError('chain overruns file')
    if batches[-1].tagid != DMAret:
        raise ValueError('last tag is not ret')
    return pipe, prim, batches

def vertices(path):
    """-> (primtype, flat vertex list with the batch overlap removed)"""
    pipe, prim, batches = parse(path)
    repeat = PRIMREPEAT[prim]
    verts = list(batches[0].verts)
    for b in batches[1:]:
        assert b.verts[:repeat] == verts[-repeat:] if repeat else True
        verts += b.verts[repeat:]
    return prim, verts

# ---- inspection -----------------------------------------------------------

def info(path):
    pipe, prim, batches = parse(path)
    prim2, verts = vertices(path)
    total = 16 + sum(16 + b.qwc*16 for b in batches)
    print('%s: nolight %s, %d vertices, %d batches, %d bytes'
          % (path, PRIMNAME[prim], len(verts), len(batches), total))
    for b in batches:
        print('  %s qwc %3d  %3d verts  %s' %
              ('ret' if b.tagid == DMAret else 'cnt', b.qwc, len(b.verts),
               'mscalf' if b.kick == MSCALF(0) else 'mscnt'))
    xs = [v[0] for v in verts]; ys = [v[1] for v in verts]; zs = [v[2] for v in verts]
    print('  bounds x [%.3f %.3f] y [%.3f %.3f] z [%.3f %.3f]'
          % (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs)))

def diff(patha, pathb, eps=1e-5):
    """Structural equality, float payloads within eps.  -> True if same."""
    pa, prima, ba = parse(patha)
    pb, primb, bb = parse(pathb)
    if prima != primb or len(ba) != len(bb):
        print('differ: prim %d/%d, %d/%d batches'
              % (prima, primb, len(ba), len(bb)))
        return False
    for i, (x, y) in enumerate(zip(ba, bb)):
        for f in ('tagid', 'qwc', 'itop', 'kick', 'wait'):
            if getattr(x, f) != getattr(y, f):
                print('batch %d: %s differs' % (i, f))
                return False
        for j, (v, w) in enumerate(zip(x.verts, y.verts)):
            # color bytes get +-1: they are truncated from floats upstream
            if any(abs(a - b) > eps for a, b in zip(v[:5], w[:5])) \
               or any(abs(a - b) > 1 for a, b in zip(v[5:], w[5:])):
                print('batch %d vert %d: %s vs %s' % (i, j, v, w))
                return False
    print('same (%d batches, eps %g)' % (len(ba), eps))
    return True

# ---- preview renderer -----------------------------------------------------

def _triangles(prim, verts):
    tris = []
    if prim == TRILIST:
        for i in range(0, len(verts) - 2, 3):
            tris.append((verts[i], verts[i+1], verts[i+2]))
    elif prim == TRISTRIP:
        for i in range(len(verts) - 2):
            tris.append((verts[i], verts[i+1], verts[i+2]))
    return tris

def preview(path, out, size=400, views=4, shade=True):
    """Software-render `views` turntable angles side by side into a PNG.
    Vertex-colored, painter's algorithm; optional headlight shading so
    unlit geometry still reads (the PS2 shows plain gouraud instead).
    Up is +z, matching the xtc scenes."""
    from PIL import Image, ImageDraw
    prim, verts = vertices(path)
    tris = [t for t in _triangles(prim, verts)
            if t[0][:3] != t[1][:3] and t[1][:3] != t[2][:3] and t[0][:3] != t[2][:3]]
    r = max(math.sqrt(v[0]**2 + v[1]**2 + v[2]**2) for v in verts) or 1.0
    img = Image.new('RGB', (size*views, size), (38, 38, 46))
    drw = ImageDraw.Draw(img)
    elev = 0.42                          # a bit above the horizon
    for view in range(views):
        theta = 0.7 + view*2*math.pi/max(views, 1)
        ct, st = math.cos(theta), math.sin(theta)
        ce, se = math.cos(elev), math.sin(elev)
        def project(v):
            x = v[0]*ct + v[1]*st
            y = -v[0]*st + v[1]*ct
            # camera looks along -x'; screen: y' right, z up
            sx = view*size + size/2 + y/r*size*0.42
            sy = size/2 - (v[2]*ce - x*se)/r*size*0.42
            depth = x*ce + v[2]*se
            return sx, sy, depth
        light = (ct*ce, st*ce, se)       # headlight
        polys = []
        for a, b, c in tris:
            pa, pb, pc = project(a), project(b), project(c)
            u = [b[i] - a[i] for i in range(3)]
            w = [c[i] - a[i] for i in range(3)]
            n = (u[1]*w[2] - u[2]*w[1], u[2]*w[0] - u[0]*w[2], u[0]*w[1] - u[1]*w[0])
            ln = math.sqrt(n[0]**2 + n[1]**2 + n[2]**2)
            if ln < 1e-12:
                continue
            f = 1.0
            if shade:
                d = abs(sum(n[i]*light[i] for i in range(3)))/ln
                f = 0.5 + 0.5*d
            col = tuple(min(255, int((a[5+i] + b[5+i] + c[5+i])/3*f))
                        for i in range(3))
            polys.append(((pa[2] + pb[2] + pc[2])/3,
                          [pa[:2], pb[:2], pc[:2]], col))
        polys.sort(key=lambda p: p[0])
        for _, pts, col in polys:
            drw.polygon(pts, fill=col)
    img.save(out)
    return out

# ---- cli ------------------------------------------------------------------

if __name__ == '__main__':
    argv = sys.argv[1:]
    if len(argv) >= 2 and argv[0] == 'info':
        for p in argv[1:]:
            info(p)
    elif len(argv) == 3 and argv[0] == 'diff':
        sys.exit(0 if diff(argv[1], argv[2]) else 1)
    elif len(argv) >= 3 and argv[0] == 'preview':
        preview(argv[1], argv[2], views=int(argv[3]) if len(argv) > 3 else 4)
        print('wrote', argv[2])
    else:
        sys.exit(__doc__.strip())
