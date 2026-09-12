#!/usr/bin/env python3
"""
primdsm.py -- prim lists as dvp-as source.

Writes the DMA chain xtcpBuildList records on the console as text for
ee-dvp-as, so prim lists can be authored offline and linked into the
ELF like the microcode is.  Two shapes:

  inline  one DMAcnt per batch with the vertex data in the tag's own
          transfer: byte for byte what the runtime records (and what
          tools/xpl.py writes for the nolight pipe).  Position
          independent, no relocations.

  ref     the vertex attributes are contiguous arrays, one per
          attribute for the whole mesh, and the chain is one DMAref per
          attribute per batch pointing into them, plus a DMAcnt with
          the ITOP/MSCAL.  The arrays know nothing about the pipeline;
          only the small chain depends on the batch size and VU layout,
          so the same arrays can serve several pipelines.  Needs the
          batch slices qword aligned, see refBatchSize().

The pipeline (batch size per prim type, attribute order, VU offsets,
unpack formats, stride) is read from the microcode's own .dsm: the .equ
lines and the xtcCode footer.  See tools/DSMNOTES.md for the dvp-as
syntax this relies on.

Library:

    import primdsm
    pipe = primdsm.Pipeline('src/vu1/stdPipe.dsm')
    m = primdsm.PrimList(pipe, 'trilist')
    m.color(255, 160, 0); m.normal(0, 0, 1); m.uv(0, 0)   # sticky
    m.vertex(x, y, z)
    text = m.dsm('monkey_std')            # or m.dsm('monkey_std', ref=True)

CLI (OBJ, y-up, fanned to a trilist, colours from the v x y z r g b
extension if present):

    primdsm.py -pipe src/vu1/stdPipe.dsm [-ref] [-name SYM] [-scale S]
               [-zup] model.obj > src/data/model.dsm
"""

import math, os, re, sys

# ---- what the microcode footers say ---------------------------------------

POINTS, LINELIST, LINESTRIP, TRILIST, TRISTRIP = range(5)
PRIMNAME = ['points', 'linelist', 'linestrip', 'trilist', 'tristrip']
PRIMSIZE = [1, 2, 2, 3, 3]
PRIMREPEAT = [0, 0, 1, 0, 2]

# XTCP_* usage codes (defines.inc) and the attribute each one wants
USAGE = {1: 'pos', 2: 'uv', 3: 'rgba', 4: 'normal', 5: 'skin'}

# VIF UNPACK: cmd byte, dvp-as name, bytes per vertex, data directive
UNPACK_USN = 0x4000
UNPACK_DBLBUF = 0x8000
FORMATS = {
    0x6C: ('V4_32', 16, '.float'),
    0x68: ('V3_32', 12, '.float'),
    0x64: ('V2_32', 8, '.float'),
    0x6D: ('V4_16', 8, '.short'),
    0x69: ('V3_16', 6, '.short'),
    0x6E: ('V4_8', 4, '.byte'),
    0x6A: ('V3_8', 3, '.byte'),
}

class Attrib:
    def __init__(self, usage, offset, unpack):
        self.usage = USAGE[usage]
        self.offset = offset
        self.cmd = unpack & 0xFF
        self.usn = bool(unpack & UNPACK_USN)
        self.name, self.size, self.directive = FORMATS[self.cmd]
        # UNPACK immediate: address, TOPS-relative, unsigned
        self.imm = UNPACK_DBLBUF | (UNPACK_USN if self.usn else 0) | offset
        self.flags = 'r' + ('u' if self.usn else '')

    # 'unpack[ru] V4_8, 2' -- add ', *' or ', count'
    def mnemonic(self):
        return 'unpack[%s] %s, %d' % (self.flags, self.name, self.offset)

    # one vertex as a data line; v is the (pos, uv, rgba, normal) tuple
    def dataline(self, v):
        x = v[('pos', 'uv', 'rgba', 'normal').index(self.usage)]
        if self.directive == '.float':
            vals = list(x) + [0.0]*4
            return '.float ' + ', '.join(repr(float(f)) for f in vals[:self.size//4])
        n = self.size if self.directive == '.byte' else self.size//2
        return '%s %s' % (self.directive, ', '.join(str(int(i)) for i in x[:n]))

class Pipeline:
    """Everything primdsm needs from a microcode .dsm."""

    def __init__(self, path):
        self.path = path
        self.name = os.path.basename(path)
        eq = {}
        words = []
        attribs = []
        infoot = False
        for line in open(path):
            line = line.split(';')[0].strip()
            if not line:
                continue
            m = re.match(r'\.equ\s+(\w+)\s*,\s*(.*)$', line)
            if m:
                eq[m.group(1)] = self._eval(m.group(2), eq)
                continue
            if re.match(r'xtcCode\w+:', line):
                infoot = True
                continue
            if line == 'inputDesc:':
                infoot = False
                continue
            m = re.match(r'\.word\s+(.*)$', line)
            if not m:
                continue
            fields = [f.strip() for f in m.group(1).split(',')]
            if infoot:
                words.append(fields)
            elif fields[0].startswith('XTCP_'):
                usage = {'XTCP_POSITION': 1, 'XTCP_TEXCOORD': 2, 'XTCP_COLOR': 3,
                         'XTCP_NORMAL': 4, 'XTCP_SKINDATA': 5}[fields[0]]
                unpack = 0
                for tok in fields[2].split('|'):
                    tok = tok.strip()
                    unpack |= {'UNPACK_USN': UNPACK_USN, 'UNPACK_V4_32': 0x6C,
                               'UNPACK_V3_32': 0x68, 'UNPACK_V2_32': 0x64,
                               'UNPACK_V4_16': 0x6D, 'UNPACK_V3_16': 0x69,
                               'UNPACK_V4_8': 0x6E, 'UNPACK_V3_8': 0x6A}[tok]
                attribs.append(Attrib(usage, int(fields[1]), unpack))
            elif len(fields) == 2 and not attribs:
                self.stride, self.numAttribs = int(fields[0]), int(fields[1])
        # footer: code, vertexTop, vertCount, numInAttribs, offset,
        # inputDesc, numVerts x5, then the code switches
        flat = [w for ws in words for w in ws]
        self.vertCount = self._eval(flat[2], eq)
        self.numVerts = [self._eval(w, eq) for w in flat[6:11]]
        self.attribs = attribs
        assert len(attribs) == self.numAttribs, self.path
        self.equates = eq

    @staticmethod
    def _eval(expr, eq):
        # the footers use C operators; only / needs help
        e = re.sub(r'\b0x[0-9a-fA-F]+\b', lambda m: str(int(m.group(0), 16)), expr)
        e = e.replace('/', '//')
        e = re.sub(r'\b([A-Za-z_]\w*)\b', lambda m: str(eq[m.group(1)]), e)
        return int(eval(e, {'__builtins__': {}}, {}))

    # the smallest vertex count every attribute slice is a whole number
    # of qwords at, for ref'd uploads
    def refAlign(self):
        a = 1
        for at in self.attribs:
            a = a*(16//math.gcd(16, at.size)) // math.gcd(a, 16//math.gcd(16, at.size))
        return a

    # xtcpGetBatchInfo's batch size, or the largest ref-friendly one below it
    def refBatchSize(self, prim):
        n = self.numVerts[prim]
        repeat = PRIMREPEAT[prim]
        a = self.refAlign()
        if prim & 1:
            step = a*PRIMSIZE[prim] // math.gcd(a, PRIMSIZE[prim])
            return (n//step)*step
        return ((n - repeat)//a)*a + repeat

# ---- the writer -----------------------------------------------------------

class PrimList:
    def __init__(self, pipe, prim='trilist'):
        self.pipe = pipe
        self.prim = PRIMNAME.index(prim)
        self.verts = []
        self._uv = (0.0, 0.0)
        self._col = (255, 255, 255, 255)
        self._nrm = (0, 0, 127)
        self._restart = False

    def color(self, r, g, b, a=255):
        self._col = tuple(int(x) & 0xFF for x in (r, g, b, a))

    def uv(self, s, t):
        self._uv = (float(s), float(t))

    def normal(self, x, y, z):
        # xtcNormal stores (int)(n*127) as signed bytes, the VU scales by 1/127
        self._nrm = tuple(max(-127, min(127, int(c*127.0))) for c in (x, y, z))

    def _kick(self, pos):
        self.verts.append((pos, self._uv, self._col, self._nrm))

    def vertex(self, x, y, z):
        pos = (float(x), float(y), float(z), 0.0)
        self._kick(pos)
        if self._restart:
            self._kick(pos)
            self._restart = False

    def restart(self):
        # xtcRestartStrip: the last vertex again, then the next one twice
        if self.prim != TRISTRIP:
            raise ValueError('restart only works on tristrips')
        self._kick(self.verts[-1][0])
        self._restart = True

    # xtcpGetBatchInfo
    def batches(self, batchSize):
        n = len(self.verts)
        repeat = PRIMREPEAT[self.prim]
        primsz = PRIMSIZE[self.prim]
        if self.prim & 1:
            numPrims = n//primsz
            numPrimsBatch = batchSize//primsz
        else:
            numPrims = n - repeat
            numPrimsBatch = batchSize - repeat
        if numPrims < 1:
            raise ValueError('nothing to draw (%d verts)' % n)
        numBatches = (numPrims + numPrimsBatch - 1)//numPrimsBatch
        last = n - (numBatches - 1)*(batchSize - repeat)
        out = []
        base = 0
        for i in range(numBatches):
            count = last if i == numBatches - 1 else batchSize
            out.append((base, count))
            base += count - repeat
        return out

    def _header(self, sym, ref):
        p = self.pipe
        lines = [
            '; prim list for the %s pipeline, %s, %d vertices' %
            (p.name, PRIMNAME[self.prim], len(self.verts)),
            '; generated by tools/primdsm.py -- see tools/DSMNOTES.md',
            ';',
            '; VU input layout: stride %d, %s' % (p.stride, ', '.join(
                '%s %s @%d' % (a.usage, a.name + ('u' if a.usn else ''), a.offset)
                for a in p.attribs)),
            '',
            '.global %s' % sym,
            '.data',
            '.align 4',
        ]
        if ref:
            lines[1:1] = ['; ref style: the chain refs into the per-attribute arrays below']
            lines += [
                '',
                '; UNPACK with an explicit count and no inline data, for ref\'d payloads',
                '; (the assembler\'s own unpack wants the data in place)',
                '.macro unpackref cmd, imm, num',
                '.int ((\\cmd)<<24)|((\\num)<<16)|(\\imm)',
                '.endm',
            ]
        return lines

    def dsm(self, sym, ref=False):
        return self._ref(sym) if ref else self._inline(sym)

    def _inline(self, sym):
        p = self.pipe
        L = self._header(sym, False)
        L += ['', '%s:' % sym]
        bs = self.batches(p.numVerts[self.prim])
        for i, (base, count) in enumerate(bs):
            last = i == len(bs) - 1
            L.append('')
            L.append('; batch %d: vertices %d..%d' % (i, base, base + count - 1))
            L.append('DMAret *' if last else 'DMAcnt *')
            L.append('vifnop')
            L.append('stcycl 1, %d' % p.stride)
            for at in p.attribs:
                L.append('%s, *' % at.mnemonic())
                for v in self.verts[base:base + count]:
                    L.append(at.dataline(v))
                L.append('.EndUnpack')
            L.append('itop %d' % count)
            L.append('mscalf 0' if i == 0 else 'mscnt')
            L.append('flush' if last else 'vifnop')
            L.append('.EndDmaData')
        return '\n'.join(L) + '\n'

    def _ref(self, sym):
        p = self.pipe
        L = self._header(sym, True)
        batchSize = p.refBatchSize(self.prim)
        bs = self.batches(batchSize)
        L += ['', '; batches of %d (the pipeline allows %d, the slices must be qword sized)'
              % (batchSize, p.numVerts[self.prim]), '%s:' % sym]
        for i, (base, count) in enumerate(bs):
            last = i == len(bs) - 1
            L.append('')
            L.append('; batch %d: vertices %d..%d' % (i, base, base + count - 1))
            for j, at in enumerate(p.attribs):
                nbytes = at.size*count
                assert (base*at.size) % 16 == 0, 'slice start not qword aligned'
                qwc = (nbytes + 15)//16
                L.append('DMAref %d, %s_%s + %d' % (qwc, sym, at.usage, base*at.size))
                L.append('stcycl 1, %d' % p.stride if j == 0 else 'vifnop')
                L.append('unpackref 0x%02X, 0x%04X, %d\t; %s' %
                         (at.cmd, at.imm, count, at.mnemonic()))
            L.append('DMAret *' if last else 'DMAcnt *')
            L.append('itop %d' % count)
            L.append('mscalf 0' if i == 0 else 'mscnt')
            if last:
                L.append('flush')
            L.append('.EndDmaData')
        for at in p.attribs:
            L.append('')
            L.append('; %s, %s, %d bytes per vertex' % (at.usage, at.name, at.size))
            L.append('.align 4')
            L.append('%s_%s:' % (sym, at.usage))
            for v in self.verts:
                L.append(at.dataline(v))
            L.append('.align 4')
        return '\n'.join(L) + '\n'

# ---- OBJ front end ----------------------------------------------------------

def readobj(path):
    verts, colors, normals, faces = [], [], [], []
    for line in open(path):
        f = line.split()
        if not f or f[0].startswith('#'):
            continue
        if f[0] == 'v':
            verts.append(tuple(float(x) for x in f[1:4]))
            colors.append(tuple(float(x) for x in f[4:7]) if len(f) >= 7 else None)
        elif f[0] == 'vn':
            normals.append(tuple(float(x) for x in f[1:4]))
        elif f[0] == 'f':
            corners = []
            for c in f[1:]:
                idx = (c.split('/') + ['', ''])[:3]
                corners.append(tuple(int(i) if i else 0 for i in idx))
            for i in range(1, len(corners) - 1):
                faces.append((corners[0], corners[i], corners[i+1]))
    return verts, colors, normals, faces

def fromobj(pipe, path, scale=1.0, zup=False):
    verts, colors, normals, faces = readobj(path)
    m = PrimList(pipe, 'trilist')
    for face in faces:
        for vi, ti, ni in face:
            c = colors[vi-1]
            m.color(*(int(round(x*255)) for x in c)) if c else m.color(0, 0, 0)
            n = normals[ni-1] if ni else (0.0, 0.0, 1.0)
            v = verts[vi-1]
            if zup:
                # y-up to z-up, the way drawObj does it
                n = (n[0], -n[2], n[1])
                v = (v[0], -v[2], v[1])
            m.normal(*n)
            m.vertex(v[0]*scale, v[1]*scale, v[2]*scale)
    return m

def main():
    args = sys.argv[1:]
    pipe = 'src/vu1/stdPipe.dsm'
    ref = False
    name = None
    scale = 1.0
    zup = False
    while args and args[0].startswith('-'):
        a = args.pop(0)
        if a == '-pipe': pipe = args.pop(0)
        elif a == '-ref': ref = True
        elif a == '-name': name = args.pop(0)
        elif a == '-scale': scale = float(args.pop(0))
        elif a == '-zup': zup = True
        else: sys.exit('unknown option %s' % a)
    if len(args) != 1:
        sys.exit(__doc__)
    path = args[0]
    if name is None:
        name = re.sub(r'\W', '_', os.path.splitext(os.path.basename(path))[0])
    p = Pipeline(pipe)
    m = fromobj(p, path, scale, zup)
    sys.stdout.write(m.dsm(name, ref))

if __name__ == '__main__':
    main()
