#!/usr/bin/env python3
"""
xplkit.py -- the asset-authoring kit for xtc prim lists (.xpl).

Merged from the two libraries the first artist cohort grew independently
(assets/xplbuild.py, architecture; assets/vehkit.py, vehicles).  Those
files stay with their generators as the frozen source of the first nine
assets; THIS is the kit new work should use.

The engine draws prim lists with NO lighting -- vertex colours are the
whole look -- so the kit's job is baking light into colours while the
geometry is emitted:

    from xplkit import *
    b = Build(DAYLIGHT)                  # or STUDIO, or your own Rig
    box(b, (0,0,0), (0.5,0.5,0.5), (180,60,50))
    tube(b, (0,0,0.5), (0,0,1.2), 0.1, 0.05, 12, (200,190,60))
    b.fit(1.45)                          # centre + scale into the camera orbit
    b.save('assets/thing.xpl')

Conventions: +z up, colours 0..255 RGB, model centred on the origin
inside radius ~1.5.  Everything is tristrips under the hood; flat-shaded
work gets one strip per facet (hard colour edges), smooth work one strip
per grid row with central-difference normals.

Sections: vectors / transforms / noise / Rig (baked light) / Build /
primitives (box, bevelbox, prism, plate, beam, tube, cylinder, loft,
revolve, sphere, torus, ledge, ring, rect_ring) / v2: curves & sweeps
(catmull, ptframes, sweep, rod, loop_rod, arc), coherent noise (vnoise2,
fbm2, vnoise3), colour (hsv), fields (radial_ao, ao_spheres), surfaces
(grid_from, at, blob, disc, flat_ring, poly_ring, bead, upnorm),
polyhedra (icosahedron, geodesic, goldberg, goldberg_faces).

The v2 section was written by the first artist cohort inside their
generators and absorbed here afterwards; the originals remain in
assets/gen_garden.py, assets/gen_vine.py, assets/rlkit.py.

Budgeting: Build.strip costs 2 extra join vertices per strip after the
first (the tristrip restart duplicates); a grid of R x C points emitted
smooth is about 2*C*(R-1) + 2*(R-2) vertices; a rod segment at seg=3
is 6 vertices plus joins.  Check b.count as you go.

Rig notes: for an open, uncull-able surface, a second key from exactly
opposite (fill_dir = -key_dir) keeps the far side alive.  With
`exposure` set, the filmic curve clips channels together: near-white
bases above full brightness lose their hue and go dead white -- keep
saturated bases out of the top stop.
"""

import math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xpl                                                  # noqa: E402

TAU = math.pi*2

# ---- vectors --------------------------------------------------------------

def add(a, b):    return (a[0]+b[0], a[1]+b[1], a[2]+b[2])
def sub(a, b):    return (a[0]-b[0], a[1]-b[1], a[2]-b[2])
def mul(a, s):    return (a[0]*s, a[1]*s, a[2]*s)
def dot(a, b):    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]
def length(a):    return math.sqrt(dot(a, a))

def cross(a, b):
    return (a[1]*b[2] - a[2]*b[1],
            a[2]*b[0] - a[0]*b[2],
            a[0]*b[1] - a[1]*b[0])

def norm(a):
    l = length(a)
    return (0.0, 0.0, 1.0) if l < 1e-12 else (a[0]/l, a[1]/l, a[2]/l)

def lerp(a, b, t):
    return a + (b - a)*t

def lerp3(a, b, t):
    return (lerp(a[0], b[0], t), lerp(a[1], b[1], t), lerp(a[2], b[2], t))

def clamp(v, lo=0.0, hi=1.0):
    return lo if v < lo else (hi if v > hi else v)

def smoothstep(e0, e1, x):
    t = clamp((x - e0)/(e1 - e0) if e1 != e0 else 0.0)
    return t*t*(3.0 - 2.0*t)

def newell(pts):
    """Robust polygon normal (fine with near-degenerate corners)."""
    n = [0.0, 0.0, 0.0]
    for i, p in enumerate(pts):
        q = pts[(i + 1) % len(pts)]
        n[0] += (p[1] - q[1])*(p[2] + q[2])
        n[1] += (p[2] - q[2])*(p[0] + q[0])
        n[2] += (p[0] - q[0])*(p[1] + q[1])
    return norm(n)

def basis(d):
    """Orthonormal frame (u, v, w) with w along d."""
    w = norm(d)
    up = (0.0, 0.0, 1.0) if abs(w[2]) < 0.9 else (1.0, 0.0, 0.0)
    u = norm(cross(up, w))
    return u, cross(w, u), w

# ---- transforms (point -> point) ------------------------------------------

def rotate_about(p, axis, ang):
    a = norm(axis)
    c, s = math.cos(ang), math.sin(ang)
    return add(add(mul(p, c), mul(cross(a, p), s)), mul(a, dot(a, p)*(1.0 - c)))

def rotx(p, a):
    c, s = math.cos(a), math.sin(a)
    return (p[0], p[1]*c - p[2]*s, p[1]*s + p[2]*c)

def roty(p, a):
    c, s = math.cos(a), math.sin(a)
    return (p[0]*c + p[2]*s, p[1], -p[0]*s + p[2]*c)

def rotz(p, a):
    c, s = math.cos(a), math.sin(a)
    return (p[0]*c - p[1]*s, p[0]*s + p[1]*c, p[2])

def xform(t=(0, 0, 0), rx=0.0, ry=0.0, rz=0.0, s=1.0):
    """Scale, then rotate x->y->z, then translate; returns a callable."""
    def f(p):
        p = (p[0]*s, p[1]*s, p[2]*s)
        if rx: p = rotx(p, rx)
        if ry: p = roty(p, ry)
        if rz: p = rotz(p, rz)
        return (p[0]+t[0], p[1]+t[1], p[2]+t[2])
    return f

def mirror_y(f=None):
    """Mirror through the x-z plane, optionally after another transform."""
    def g(p):
        if f: p = f(p)
        return (p[0], -p[1], p[2])
    return g

# ---- deterministic noise --------------------------------------------------

def hash01(*key):
    """Stable float in [0,1) from any tuple of ints/floats.  Key noise by
    POSITION, not vertex index, so strip-join duplicates match exactly."""
    h = 0x9E3779B9
    for k in key:
        h ^= int(k*1013.0) & 0xFFFFFFFF if isinstance(k, float) else (k & 0xFFFFFFFF)
        h = (h*0x85EBCA6B) & 0xFFFFFFFF
        h ^= h >> 13
        h = (h*0xC2B2AE35) & 0xFFFFFFFF
        h ^= h >> 16
    return h/4294967296.0

def jitter(*key, amount=0.1):
    """Symmetric multiplier around 1.0, e.g. per-stone tint variation."""
    return 1.0 + (hash01(*key) - 0.5)*2.0*amount

def clamp255(v):
    return 0 if v < 0 else (255 if v > 255 else int(v))

def mix(a, b, t):
    return tuple(clamp255(a[i] + (b[i]-a[i])*t) for i in range(3))

def tone(c, f):
    return tuple(clamp255(c[i]*f) for i in range(3))

# ---- the baked-light rig --------------------------------------------------

class Rig:
    """Turns normal + base colour (0..255) into a lit vertex colour.

    key_dir/key_col/key   warm directional key light
    fill_dir/fill_col/fill  optional cool fill from the other side
    sky_col/ground_col      hemispheric ambient: sky above, bounce below
    ambient                 flat floor so nothing goes pure black
    exposure                if set, a soft filmic curve (outdoor look);
                            if None, linear with clamp (studio look)

    shade() extras: ao (occlusion multiplier), spec (tight white highlight
    on key-facing surfaces), bounce (warm kick onto down-facing surfaces),
    emit (a colour that bypasses the rig entirely).
    """

    def __init__(self, key_dir, key_col, key, sky_col, ground_col, ambient,
                 fill_dir=None, fill_col=(1, 1, 1), fill=0.0, exposure=None):
        self.key_dir = norm(key_dir)
        self.key_col, self.key = key_col, key
        self.fill_dir = norm(fill_dir) if fill_dir else None
        self.fill_col, self.fill = fill_col, fill
        self.sky_col, self.ground_col = sky_col, ground_col
        self.ambient, self.exposure = ambient, exposure

    def light(self, n):
        """Incident light for normal n, linear RGB triple around 1.0."""
        k = self.key*max(0.0, dot(n, self.key_dir))
        f = self.fill*max(0.0, dot(n, self.fill_dir)) if self.fill_dir else 0.0
        hemi = 0.5 + 0.5*n[2]
        return tuple(self.ambient
                     + lerp(self.ground_col[i], self.sky_col[i], hemi)
                     + self.key_col[i]*k + self.fill_col[i]*f
                     for i in range(3))

    def shade(self, n, base, ao=1.0, spec=0.0, bounce=0.0, emit=None):
        if emit is not None:
            return tuple(clamp255(c) for c in emit[:3])
        n = norm(n)
        L = self.light(n)
        b = max(0.0, -n[2])*bounce
        k = max(0.0, dot(n, self.key_dir))
        out = []
        for i in range(3):
            v = base[i]/255.0*(L[i] + b)*ao
            v += spec*k**10
            if self.exposure is not None:
                v = max(0.0, v)*self.exposure
                v = clamp(v/(1.0 + v*0.55)*1.55)
            out.append(clamp255(v*255.0))
        return tuple(out)

# an outdoor sun (from the architecture kit): warm key, cool fill,
# sky/ground hemisphere, soft filmic rolloff
DAYLIGHT = Rig(key_dir=(0.56, -0.44, 0.70), key_col=(1.12, 1.00, 0.80),
               key=1.25, sky_col=(0.40, 0.46, 0.58),
               ground_col=(0.30, 0.23, 0.17), ambient=0.0,
               fill_dir=(-0.68, 0.52, 0.22), fill_col=(0.67, 0.71, 0.86),
               fill=0.42, exposure=1.05)

# a product-shot studio (from the vehicle kit): linear, wide ambient floor
STUDIO = Rig(key_dir=(0.46, -0.70, 0.55), key_col=(1.14, 1.03, 0.84),
             key=0.46, sky_col=(0.29, 0.32, 0.39),
             ground_col=(0.0, 0.0, 0.0), ambient=0.42,
             exposure=None)

# ---- builder --------------------------------------------------------------

def convex_strip(seq):
    """Zig-zag ordering turning a convex polygon into one tristrip."""
    out = []
    lo, hi = 0, len(seq) - 1
    while lo <= hi:
        out.append(seq[lo]); lo += 1
        if lo <= hi:
            out.append(seq[hi]); hi -= 1
    return out

def _fn(v):
    """Broadcast a value to a callable of (i, j)."""
    return v if callable(v) else (lambda i, j: v)

class Build:
    """Wrapper over xpl.PrimList that shades through a Rig as it emits."""

    def __init__(self, rig=DAYLIGHT):
        self.rig = rig
        self.m = xpl.PrimList('tristrip')
        self._started = False

    # -- raw ---------------------------------------------------------------
    def strip(self, pts, cols):
        """One tristrip.  Every strip after the first costs 2 extra join
        vertices (the restart duplicates) -- budget with b.count."""
        if len(pts) < 3:
            return
        if self._started:
            self.m.restart()
        self._started = True
        for p, c in zip(pts, cols):
            self.m.color(c[0], c[1], c[2])
            self.m.vertex(p[0], p[1], p[2])

    # -- flat faces --------------------------------------------------------
    def poly(self, pts, base, ao=1.0, n=None, spec=0.0, bounce=0.0,
             emit=None, flip=False):
        """Flat convex polygon: one normal, one strip.  ao may be a list
        (per vertex) for baked gradients across the face."""
        if len(pts) < 3:
            return
        if n is None:
            n = newell(pts)
        if flip:
            n = mul(n, -1.0)
        aos = ao if isinstance(ao, (list, tuple)) else [ao]*len(pts)
        cols = [self.rig.shade(n, base, aos[i], spec, bounce, emit)
                for i in range(len(pts))]
        idx = convex_strip(list(range(len(pts))))
        self.strip([pts[i] for i in idx], [cols[i] for i in idx])

    face = poly                         # the vehicle kit's name for it

    def quad(self, a, b, c, d, base, **kw):
        self.poly([a, b, c, d], base, **kw)

    # -- grids -------------------------------------------------------------
    def grid(self, P, base, ao=1.0, spec=0.0, bounce=0.0, emit=None,
             smooth=True, closed_u=False, closed_v=False, flip=False):
        """P[i][j] point mesh -> tristrips; rows (i) become strips.

        base/ao/emit may be callables f(i, j).  smooth shades per vertex
        from central-difference normals; otherwise each quad is its own
        flat-shaded strip (hard colour edges).  closed_u/closed_v wrap
        the respective axis.
        """
        nu, nv = len(P), len(P[0])
        base_f, ao_f, emit_f = _fn(base), _fn(ao), _fn(emit)

        if not smooth:
            for i in range(nu if closed_u else nu - 1):
                i2 = (i + 1) % nu
                for j in range(nv if closed_v else nv - 1):
                    j2 = (j + 1) % nv
                    self.poly([P[i][j], P[i2][j], P[i2][j2], P[i][j2]],
                              base_f(i, j), ao_f(i, j), spec=spec,
                              bounce=bounce, emit=emit_f(i, j), flip=flip)
            return

        N = grid_normals(P, closed_u, closed_v)
        sgn = -1.0 if flip else 1.0
        C = [[self.rig.shade(mul(N[i][j], sgn), base_f(i, j), ao_f(i, j),
                             spec, bounce, emit_f(i, j))
              for j in range(nv)] for i in range(nu)]
        for i in range(nu if closed_u else nu - 1):
            i2 = (i + 1) % nu
            pts, cols = [], []
            for j in list(range(nv)) + ([0] if closed_v else []):
                pts.append(P[i][j]);  cols.append(C[i][j])
                pts.append(P[i2][j]); cols.append(C[i2][j])
            self.strip(pts, cols)

    # -- output ------------------------------------------------------------
    @property
    def count(self):
        return len(self.m.verts)

    def bounds(self):
        vs = self.m.verts
        return [(min(v[k] for v in vs), max(v[k] for v in vs))
                for k in range(3)]

    def fit(self, radius=1.45, center=(True, True, True)):
        """Centre on the bbox and scale to fit inside `radius`."""
        vs = self.m.verts
        bb = self.bounds()
        c = [((lo + hi)*0.5 if center[k] else 0.0)
             for k, (lo, hi) in enumerate(bb)]
        r = max(math.sqrt(sum((v[k] - c[k])**2 for k in range(3)))
                for v in vs)
        s = radius/r if r > 1e-9 else 1.0
        for i, v in enumerate(vs):
            vs[i] = ((v[0]-c[0])*s, (v[1]-c[1])*s, (v[2]-c[2])*s) + v[3:]
        return s

    def save(self, path):
        self.m.save(path)
        return self.count

def grid_normals(P, closed_u=False, closed_v=False):
    nu, nv = len(P), len(P[0])
    N = [[(0.0, 0.0, 1.0)]*nv for _ in range(nu)]
    for i in range(nu):
        for j in range(nv):
            ip = i+1 if i+1 < nu else (0 if closed_u else i)
            im = i-1 if i > 0 else (nu-1 if closed_u else i)
            jp = j+1 if j+1 < nv else (0 if closed_v else j)
            jm = j-1 if j > 0 else (nv-1 if closed_v else j)
            du = sub(P[ip][j], P[im][j])
            dv = sub(P[i][jp], P[i][jm])
            n = cross(du, dv)
            if length(n) < 1e-12:               # degenerate pole: look around
                for dj in range(1, nv):
                    dv = sub(P[i][(j+dj) % nv], P[i][j])
                    n = cross(du, dv)
                    if length(n) > 1e-12:
                        break
            N[i][j] = norm(n)
    return N

# ---- primitives -----------------------------------------------------------
# All take the Build first and colours as 0..255 RGB; base/ao follow
# Build.grid's callable convention where noted.

def box(b, cen, half, base, yaw=0.0, top=True, bottom=False, sides=True,
        tilt=None, **kw):
    """Chunky box; yaw about z, tilt an optional (axis, angle) after it."""
    cy, sy = math.cos(yaw), math.sin(yaw)
    corners = []
    for sz in (-1, 1):
        for sx, sxy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
            lx, ly, lz = sx*half[0], sxy*half[1], sz*half[2]
            p = (lx*cy - ly*sy, lx*sy + ly*cy, lz)
            if tilt is not None:
                p = rotate_about(p, tilt[0], tilt[1])
            corners.append(add(cen, p))
    lo, hi = corners[0:4], corners[4:8]
    if sides:
        for i in range(4):
            j = (i + 1) % 4
            b.quad(lo[i], lo[j], hi[j], hi[i], base, **kw)
    if top:
        b.poly(hi, base, **kw)
    if bottom:
        b.poly(lo[::-1], base, **kw)

def chamfer_ring(hy, hz, c, oy=0.0, oz=0.0):
    """Octagonal cross-section in the y-z plane, for lofted hulls."""
    c = min(c, hy*0.98, hz*0.98)
    return [(0.0, oy+hy, oz+hz-c), (0.0, oy+hy-c, oz+hz),
            (0.0, oy-hy+c, oz+hz), (0.0, oy-hy, oz+hz-c),
            (0.0, oy-hy, oz-hz+c), (0.0, oy-hy+c, oz-hz),
            (0.0, oy+hy-c, oz-hz), (0.0, oy+hy, oz-hz+c)]

def loft(b, stations, base, ao=1.0, caps=True, smooth=False, xf=None, **kw):
    """stations: [(x, ring)] with equal-length y-z rings -> swept hull.
    base/ao may be callables f(i, j) over the (station, point) grid."""
    P = [[(x, p[1], p[2]) for p in ring] for x, ring in stations]
    if xf:
        P = [[xf(p) for p in row] for row in P]
    b.grid(P, base, ao, closed_v=True, smooth=smooth, **kw)
    if caps:
        last = len(P) - 1
        base_f, ao_f = _fn(base), _fn(ao)
        kw = {k: v for k, v in kw.items() if k != 'emit'}
        b.poly(list(reversed(P[0])), base_f(0, 0), ao_f(0, 0), **kw)
        b.poly(P[-1], base_f(last, 0), ao_f(last, 0), **kw)

def bevelbox(b, center, size, bevel, base, ao=1.0, xf=None, axis='x', **kw):
    """Chamfered box, the chunky building block."""
    cx, cy, cz = center
    sx, sy, sz = size[0]*0.5, size[1]*0.5, size[2]*0.5
    bv = min(bevel, sx*0.9, sy*0.9, sz*0.9)
    stations = [(x, chamfer_ring(sy - inset, sz - inset, bv))
                for x, inset in ((-sx, bv), (-sx+bv, 0.0),
                                 (sx-bv, 0.0), (sx, bv))]
    def place(p):
        if axis == 'x':   q = p
        elif axis == 'y': q = (p[1], p[0], p[2])
        else:             q = (p[2], p[1], p[0])
        q = (q[0]+cx, q[1]+cy, q[2]+cz)
        return xf(q) if xf else q
    loft(b, stations, base, ao, caps=True, smooth=False, xf=place, **kw)

def prism(b, poly2d, z0, z1, base, ao=1.0, top=True, bottom=False,
          closed=True, **kw):
    """Extrude a 2-D polygon [(x, y), ...] between two heights.
    base may be a callable of the side-face index."""
    lo = [(p[0], p[1], z0) for p in poly2d]
    hi = [(p[0], p[1], z1) for p in poly2d]
    n = len(poly2d)
    for i in range(n if closed else n - 1):
        j = (i + 1) % n
        alb = base(i) if callable(base) else base
        b.quad(lo[i], lo[j], hi[j], hi[i], alb, ao=ao, **kw)
    alb = base(0) if callable(base) else base
    if top:
        b.poly(hi, alb, ao, n=(0.0, 0.0, 1.0), **kw)
    if bottom:
        b.poly(lo[::-1], alb, ao, n=(0.0, 0.0, -1.0), **kw)

def plate(b, pts, thickness, base, ao=1.0, up=(0, 0, 1), **kw):
    """Extrude a convex polygon into a slab (both faces + rim).
    ao may be a scalar or a per-vertex list."""
    def scale_ao(a, f):
        return [x*f for x in a] if isinstance(a, (list, tuple)) else a*f
    h = mul(norm(up), thickness*0.5)
    top = [add(p, h) for p in pts]
    bot = [sub(p, h) for p in pts]
    b.poly(top, base, ao, **kw)
    b.poly(list(reversed(bot)), base, scale_ao(ao, 0.7), **kw)
    rim = scale_ao(ao, 0.88)
    rim = rim if not isinstance(rim, (list, tuple)) else sum(rim)/len(rim)
    for i in range(len(pts)):
        j = (i + 1) % len(pts)
        b.quad(bot[i], bot[j], top[j], top[i], base, ao=rim, **kw)

def beam(b, p0, p1, w, h, base, ends=True, up=(0.0, 0.0, 1.0), **kw):
    """Rectangular-section bar from p0 to p1, w across, h in `up` sense."""
    d = norm(sub(p1, p0))
    s = norm(cross(d, up))
    u = norm(cross(s, d))
    def corner(p, sw, sh):
        return add(p, add(mul(s, sw*w*0.5), mul(u, sh*h*0.5)))
    A = [corner(p0, -1, -1), corner(p0, 1, -1),
         corner(p0, 1, 1), corner(p0, -1, 1)]
    B = [corner(p1, -1, -1), corner(p1, 1, -1),
         corner(p1, 1, 1), corner(p1, -1, 1)]
    for i in range(4):
        j = (i + 1) % 4
        b.quad(A[i], A[j], B[j], B[i], base, **kw)
    if ends:
        b.poly([A[3], A[2], A[1], A[0]], base, **kw)
        b.poly(B, base, **kw)
    return A, B

def tube(b, p0, p1, r0, r1, seg, base, ao=1.0, caps=True, smooth=True,
         nlev=2, rfun=None, phase=0.0, twist=0.0, **kw):
    """Cylinder/cone between two points, optionally with intermediate
    levels and a radius modulator rfun(t, theta) (flutes, wobble)."""
    u, v, w = basis(sub(p1, p0))
    P = []
    for iv in range(nlev):
        t = iv/(nlev - 1.0)
        c = lerp3(p0, p1, t)
        rr = lerp(r0, r1, t)
        row = []
        for j in range(seg):
            a = phase + twist*t + j*TAU/seg
            k = rfun(t, a) if rfun else 1.0
            row.append(add(c, add(mul(u, rr*k*math.cos(a)),
                                  mul(v, rr*k*math.sin(a)))))
        P.append(row)
    b.grid(P, base, ao, closed_v=True, smooth=smooth, **kw)
    if caps:
        base_f, ao_f = _fn(base), _fn(ao)
        kw = {k: v for k, v in kw.items() if k != 'emit'}
        if r0 > 1e-6:
            b.poly(list(reversed(P[0])), base_f(0, 0), ao_f(0, 0), **kw)
        if r1 > 1e-6:
            b.poly(P[-1], base_f(nlev-1, 0), ao_f(nlev-1, 0), **kw)
    return P

cylinder = tube

def revolve(b, profile, seg, base, ao=1.0, axis='z', xf=None, smooth=True,
            closed_u=False, **kw):
    """profile: [(r, h)] swept around the axis."""
    P = []
    for r, h in profile:
        ring_ = []
        for j in range(seg):
            a = j*TAU/seg
            c, s = math.cos(a)*r, math.sin(a)*r
            if axis == 'z':   p = (c, s, h)
            elif axis == 'x': p = (h, c, s)
            else:             p = (c, h, s)
            ring_.append(xf(p) if xf else p)
        P.append(ring_)
    b.grid(P, base, ao, closed_v=True, closed_u=closed_u, smooth=smooth, **kw)

def sphere(b, center, radii, useg, vseg, base, ao=1.0, xf=None, **kw):
    rx, ry, rz = radii
    P = []
    for i in range(vseg + 1):
        t = math.pi*i/vseg
        s, c = math.sin(t), math.cos(t)
        ring_ = []
        for j in range(useg):
            a = j*TAU/useg
            p = (center[0] + math.cos(a)*s*rx,
                 center[1] + math.sin(a)*s*ry,
                 center[2] + c*rz)
            ring_.append(xf(p) if xf else p)
        P.append(ring_)
    b.grid(P, base, ao, closed_v=True, **kw)

def torus(b, center, R, r, useg, vseg, base, ao=1.0, xf=None, plane='xy', **kw):
    P = []
    for i in range(useg):
        a = i*TAU/useg
        ca, sa = math.cos(a), math.sin(a)
        ring_ = []
        for j in range(vseg):
            t = j*TAU/vseg
            rr, h = R + math.cos(t)*r, math.sin(t)*r
            if plane == 'xy':   p = (ca*rr, sa*rr, h)
            elif plane == 'xz': p = (ca*rr, h, sa*rr)
            else:               p = (h, ca*rr, sa*rr)
            ring_.append(xf(add(center, p)) if xf else add(center, p))
        P.append(ring_)
    b.grid(P, base, ao, closed_u=True, closed_v=True, **kw)

def ledge(b, z, outer, inner, base, **kw):
    """Flat rectangular ring at height z (a step or ledge top face);
    outer/inner are (hx, hy) half extents."""
    ox, oy = outer; ix, iy = inner
    O = [(-ox, -oy, z), (ox, -oy, z), (ox, oy, z), (-ox, oy, z)]
    I = [(-ix, -iy, z), (ix, -iy, z), (ix, iy, z), (-ix, iy, z)]
    for i in range(4):
        j = (i + 1) % 4
        b.quad(O[i], O[j], I[j], I[i], base, n=(0.0, 0.0, 1.0), **kw)

def ring(cx, cy, z, n, rfun, phase=0.0):
    """n points around (cx, cy) at height z; rfun(theta) -> radius."""
    out = []
    for i in range(n):
        t = phase + TAU*i/n
        r = rfun(t) if callable(rfun) else rfun
        out.append((cx + r*math.cos(t), cy + r*math.sin(t), z))
    return out

def rect_ring(hx, hy, z, nx, ny):
    """Points around a rectangle perimeter, nx per long edge, ny per short."""
    pts = []
    for i in range(nx):
        pts.append((lerp(-hx, hx, i/nx), -hy, z))
    for i in range(ny):
        pts.append((hx, lerp(-hy, hy, i/ny), z))
    for i in range(nx):
        pts.append((lerp(hx, -hx, i/nx), hy, z))
    for i in range(ny):
        pts.append((-hx, lerp(hy, -hy, i/ny), z))
    return pts

# ===========================================================================
# v2 -- helpers the first artist cohort wrote inside their generators,
# absorbed from assets/gen_garden.py, assets/gen_vine.py, assets/rlkit.py.
# ===========================================================================

# ---- colour ---------------------------------------------------------------

def hsv(h, s, v):
    """h in turns (0..1 wraps), s/v 0..1 -> 0..255 RGB triple."""
    import colorsys
    r, g, b = colorsys.hsv_to_rgb(h % 1.0, clamp(s), clamp(v))
    return (clamp255(r*255.0), clamp255(g*255.0), clamp255(b*255.0))

# ---- coherent noise -------------------------------------------------------
# Pure functions of position: two vertices sharing a position always
# share a value, so strip-join duplicates can never tear.

def vnoise2(x, y, salt=0):
    """Smooth value noise on a lattice."""
    i, j = math.floor(x), math.floor(y)
    fx, fy = x - i, y - j
    ux = fx*fx*(3.0 - 2.0*fx)
    uy = fy*fy*(3.0 - 2.0*fy)
    def h(a, b):
        return hash01(int(a), int(b), salt)
    return lerp(lerp(h(i, j),   h(i+1, j),   ux),
                lerp(h(i, j+1), h(i+1, j+1), ux), uy)

def fbm2(x, y, oct=3, salt=0):
    v, a, f = 0.0, 0.5, 1.0
    for k in range(oct):
        v += a*vnoise2(x*f, y*f, salt + k)
        a *= 0.5
        f *= 2.0
    return v

def vnoise3(x, y, z, salt=0):
    """Two lattice slices in z, smoothly blended."""
    k = math.floor(z)
    fz = z - k
    fz = fz*fz*(3.0 - 2.0*fz)
    return lerp(vnoise2(x, y, salt + 17*int(k)),
                vnoise2(x, y, salt + 17*int(k+1)), fz)

# ---- occlusion fields -----------------------------------------------------

def radial_ao(r, rmax, floor=0.16, gamma=1.5):
    """Bake nesting: structures near the core dark, the outer rim bright."""
    t = clamp(r/rmax if rmax > 1e-9 else 0.0)
    return floor + (1.0 - floor)*t**gamma

def ao_spheres(occluders, floor=0.30, zbias=0.04, zsquash=0.62):
    """Soft shadow field from [(center, radius, strength)] spheres;
    returns ao(p).  Occluders below the sample point don't shadow it."""
    def ao(p, k=1.0):
        a = 1.0
        for c, r, s in occluders:
            if c[2] < p[2] - zbias:
                continue
            d2 = (p[0]-c[0])**2 + (p[1]-c[1])**2 + ((p[2]-c[2])*zsquash)**2
            a *= 1.0 - s*math.exp(-d2/(r*r))
        return clamp(1.0 - (1.0 - a)*k, floor, 1.0)
    return ao

# ---- parametric surfaces --------------------------------------------------

def grid_from(f, nu, nv, u0=0.0, u1=1.0, v0=0.0, v1=1.0):
    """Sample f(u, v) -> point over a parameter rectangle into a P[i][j]
    grid ((nu+1) x (nv+1) points) ready for Build.grid."""
    return [[f(lerp(u0, u1, i/nu), lerp(v0, v1, j/nv))
             for j in range(nv + 1)] for i in range(nu + 1)]

def at(P, f):
    """Adapt a position-taking callable f(p, i, j) to Build.grid's
    (i, j) convention by closing over the point grid P."""
    return lambda i, j: f(P[i][j], i, j)

def upnorm(n):
    """Force a normal into the upper hemisphere -- thin single-sided
    foliage shows both faces (no culling) but bakes from one normal,
    and the underside one bakes near-black."""
    return n if n[2] >= 0.0 else mul(n, -1.0)

# ---- curves & sweeps ------------------------------------------------------

def catmull(cps, n):
    """n points along a Catmull-Rom spline through the control points."""
    P = [cps[0]] + list(cps) + [cps[-1]]
    out = []
    m = len(cps) - 1
    for s in range(n):
        t = s/(n - 1.0)*m
        i = min(int(t), m - 1)
        f = t - i
        p0, p1, p2, p3 = P[i], P[i+1], P[i+2], P[i+3]
        f2, f3 = f*f, f*f*f
        out.append(tuple(
            0.5*(2*p1[k] + (-p0[k] + p2[k])*f
                 + (2*p0[k] - 5*p1[k] + 4*p2[k] - p3[k])*f2
                 + (-p0[k] + 3*p1[k] - 3*p2[k] + p3[k])*f3)
            for k in range(3)))
    return out

def arc(p0, p1, bulge, n):
    """n+1 points from p0 to p1 bowed radially away from the origin."""
    out = []
    for i in range(n + 1):
        t = i/n
        p = lerp3(p0, p1, t)
        out.append(add(p, mul(norm(p), math.sin(math.pi*t)*bulge)))
    return out

def ptframes(path):
    """Parallel-transported (rotation-minimising) frames along a
    polyline -> (T, U, V) lists.  A fixed up-vector spins the frame
    wherever the curve passes vertical (a helix does, constantly);
    transport doesn't."""
    n = len(path)
    T = []
    for i in range(n):
        if i == 0:
            d = sub(path[1], path[0])
        elif i == n - 1:
            d = sub(path[-1], path[-2])
        else:
            d = sub(path[i+1], path[i-1])
        T.append(norm(d))
    U = [basis(T[0])[0]]
    for i in range(1, n):
        ax = cross(T[i-1], T[i])
        s = length(ax)
        u = U[-1]
        if s > 1e-9:
            ang = math.asin(min(1.0, s))
            if dot(T[i-1], T[i]) < 0.0:
                ang = math.pi - ang
            u = rotate_about(u, ax, ang)
        u = norm(sub(u, mul(T[i], dot(u, T[i]))))
        U.append(u)
    V = [cross(T[i], U[i]) for i in range(n)]
    return T, U, V

def sweep(b, path, radii, seg, colf, aof=None, smooth=True, rfun=None,
          phase=0.0, **kw):
    """Tube swept along a curved path with per-station radius -- arms,
    branches, kelp, anything alive.  colf/aof take (world point, i, j);
    rfun(t, theta) modulates the radius.  Returns (P, T, U, V)."""
    T, U, V = ptframes(path)
    if not isinstance(radii, (list, tuple)):
        radii = [radii]*len(path)
    P = []
    for i, p in enumerate(path):
        t = i/(len(path) - 1.0)
        r = radii[i]
        row = []
        for j in range(seg):
            a = phase + j*TAU/seg
            k = rfun(t, a) if rfun else 1.0
            row.append(add(p, add(mul(U[i], r*k*math.cos(a)),
                                  mul(V[i], r*k*math.sin(a)))))
        P.append(row)
    cf = colf if not callable(colf) else at(P, colf)
    af = aof if not callable(aof) else at(P, aof)
    b.grid(P, cf, 1.0 if af is None else af, closed_v=True, smooth=smooth,
           **kw)
    return P, T, U, V

def rod(b, pts, radii, base, ao=1.0, seg=3, phase=0.0, **kw):
    """Cheap swept strut along a polyline: base/ao take (i, j) like
    Build.grid.  At seg=3 a segment is 6 vertices and still reads round
    at PS2 scale from the baked shading -- the lattice workhorse."""
    if len(pts) < 2:
        return
    if not isinstance(radii, (list, tuple)):
        radii = [radii]*len(pts)
    T, U, V = ptframes(pts)
    P = []
    for i, p in enumerate(pts):
        r = max(radii[i], 1.2e-3)
        P.append([add(p, add(mul(U[i], r*math.cos(phase + j*TAU/seg)),
                             mul(V[i], r*math.sin(phase + j*TAU/seg))))
                  for j in range(seg)])
    b.grid(P, base, ao, closed_v=True, smooth=True, **kw)

def loop_rod(b, pts, radii, uv, base, ao=1.0, seg=3, phase=0.0, **kw):
    """A CLOSED tube through pts.  Parallel transport cannot close a
    loop without a twist seam, so the caller supplies the cross-section
    frame per point as uv[i] = (u, v) -- a surface normal and meridian
    tangent, say -- and gets a seamless ring."""
    n = len(pts)
    if not isinstance(radii, (list, tuple)):
        radii = [radii]*n
    P = []
    for i, p in enumerate(pts):
        u, v = uv[i]
        r = max(radii[i], 1.2e-3)
        P.append([add(p, add(mul(u, r*math.cos(phase + j*TAU/seg)),
                             mul(v, r*math.sin(phase + j*TAU/seg))))
                  for j in range(seg)])
    b.grid(P, base, ao, closed_u=True, closed_v=True, smooth=True, **kw)

# ---- organic & perforated surfaces ----------------------------------------

def blob(b, cen, radii, useg, vseg, colf, aof=None, warp=0.30, salt=0,
         squash=None, smooth=False, **kw):
    """A lumpy mass: a sphere with position-keyed radial displacement.
    Flat-shaded gives chunky PS2 rock facets; smooth gives flesh.
    colf/aof take (world point, i, j)."""
    P = []
    for i in range(vseg + 1):
        th = math.pi*i/vseg
        st, ct = math.sin(th), math.cos(th)
        row = []
        for j in range(useg):
            a = j*TAU/useg
            d = (math.cos(a)*st, math.sin(a)*st, ct)
            g = 1.0 + (vnoise2(math.cos(a)*st*2.4 + i*0.11,
                               math.sin(a)*st*2.4, salt) - 0.5)*2.0*warp
            p = (cen[0] + d[0]*radii[0]*g,
                 cen[1] + d[1]*radii[1]*g,
                 cen[2] + d[2]*radii[2]*g)
            row.append(squash(p) if squash else p)
        P.append(row)
    cf = colf if not callable(colf) else at(P, colf)
    af = aof if not callable(aof) else at(P, aof)
    b.grid(P, cf, 1.0 if af is None else af, closed_v=True, smooth=smooth,
           **kw)
    return P

def disc(b, cen, n, u, r, sides, base, ao=1.0, **kw):
    """Flat n-gon facing n, with u fixing the first vertex direction."""
    w = norm(n)
    u = norm(sub(u, mul(w, dot(u, w))))
    v = cross(w, u)
    pts = [add(cen, add(mul(u, r*math.cos(a)), mul(v, r*math.sin(a))))
           for a in (k*TAU/sides for k in range(sides))]
    b.poly(pts, base, ao, n=w, **kw)
    return pts

def flat_ring(b, outer, inner, base, ao_out=1.0, ao_in=1.0, flip=False,
              **kw):
    """Single-sided annulus between two same-length point rings -- the
    perforated-plate trick: no culling, one surface serves both sides."""
    m = len(outer)
    N = [(0.0, 0.0, 0.0)]*m
    for i in range(m):
        j, k = (i + 1) % m, (i - 1) % m
        nn = cross(sub(outer[j], outer[k]), sub(inner[i], outer[i]))
        N[i] = norm(mul(nn, -1.0 if flip else 1.0))
    pts, cols = [], []
    for i in list(range(m)) + [0]:
        ii = i % m
        pts.append(outer[ii])
        cols.append(b.rig.shade(N[ii], base, ao_out, **kw))
        pts.append(inner[ii])
        cols.append(b.rig.shade(N[ii], base, ao_in, **kw))
    b.strip(pts, cols)

def poly_ring(center, u, v, r, n, phase=0.0):
    """n points of a regular polygon in the (u, v) plane at center."""
    return [add(center, add(mul(u, r*math.cos(phase + k*TAU/n)),
                            mul(v, r*math.sin(phase + k*TAU/n))))
            for k in range(n)]

def bead(b, c, r, base, ao=1.0, **kw):
    """Octahedral node bump, 24 vertices."""
    sphere(b, c, (r, r, r), 4, 2, base, ao, **kw)

# ---- polyhedra ------------------------------------------------------------

PHI = (1.0 + 5.0**0.5)*0.5

def icosahedron():
    """-> (12 unit verts, 20 outward-wound faces)"""
    V = []
    for s1 in (-1.0, 1.0):
        for s2 in (-1.0, 1.0):
            V.append(norm((0.0, s1, s2*PHI)))
            V.append(norm((s1, s2*PHI, 0.0)))
            V.append(norm((s2*PHI, 0.0, s1)))
    edge = min(length(sub(V[i], V[j]))
               for i in range(12) for j in range(i + 1, 12))
    F = []
    for a in range(12):
        for b in range(a + 1, 12):
            if length(sub(V[a], V[b])) > edge*1.1:
                continue
            for c in range(b + 1, 12):
                if length(sub(V[a], V[c])) > edge*1.1:
                    continue
                if length(sub(V[b], V[c])) > edge*1.1:
                    continue
                tri = [a, b, c]
                n = cross(sub(V[b], V[a]), sub(V[c], V[a]))
                if dot(n, V[a]) < 0.0:
                    tri = [a, c, b]
                F.append(tuple(tri))
    return V, F

def geodesic(freq):
    """Class-I geodesic sphere -> (unit verts, outward faces)."""
    IV, IF = icosahedron()
    verts, index = [], {}

    def put(p):
        p = norm(p)
        k = (round(p[0], 5), round(p[1], 5), round(p[2], 5))
        if k not in index:
            index[k] = len(verts)
            verts.append(p)
        return index[k]

    faces = []
    for (ia, ib, ic) in IF:
        A, B, C = IV[ia], IV[ib], IV[ic]
        rows = []
        for i in range(freq + 1):
            row = []
            for j in range(freq - i + 1):
                k = freq - i - j
                p = add(add(mul(A, i/freq), mul(B, j/freq)), mul(C, k/freq))
                row.append(put(p))
            rows.append(row)
        for i in range(freq):
            for j in range(freq - i):
                faces.append((rows[i][j], rows[i][j + 1], rows[i + 1][j]))
                if j < freq - i - 1:
                    faces.append((rows[i][j + 1], rows[i + 1][j + 1],
                                  rows[i + 1][j]))
    fixed = []
    for f in faces:
        a, b, c = (verts[i] for i in f)
        if dot(cross(sub(b, a), sub(c, a)), a) < 0.0:
            f = (f[0], f[2], f[1])
        fixed.append(f)
    return verts, fixed

def edges_of(faces):
    """Unique undirected edges of a face list."""
    e = set()
    for f in faces:
        for k in range(len(f)):
            e.add(tuple(sorted((f[k], f[(k + 1) % len(f)]))))
    return sorted(e)

def goldberg(freq):
    """Dual of the geodesic: the pentagon/hexagon lattice.
    -> (unit node positions, [(i, j)] lattice edges)"""
    V, F = geodesic(freq)
    nodes = [norm(mul(add(add(V[f[0]], V[f[1]]), V[f[2]]), 1.0/3.0))
             for f in F]
    share = {}
    for fi, f in enumerate(F):
        for k in range(3):
            share.setdefault(tuple(sorted((f[k], f[(k + 1) % 3]))),
                             []).append(fi)
    links = sorted(tuple(sorted(v)) for v in share.values() if len(v) == 2)
    return nodes, links

def goldberg_faces(freq):
    """The pore faces of the Goldberg lattice: one per geodesic vertex,
    so 12 pentagons (on the icosahedral axes) and the rest hexagons.
    -> (node positions, [(geodesic vertex index, [node indices CCW])])"""
    V, F = geodesic(freq)
    nodes = [norm(mul(add(add(V[f[0]], V[f[1]]), V[f[2]]), 1.0/3.0))
             for f in F]
    inc = {}
    for fi, f in enumerate(F):
        for vi in f:
            inc.setdefault(vi, []).append(fi)
    faces = []
    for vi in sorted(inc):
        c = V[vi]
        u, v, _ = basis(c)                      # w == c, so +angle is CCW
        loop = sorted(inc[vi],
                      key=lambda fi: math.atan2(dot(nodes[fi], v),
                                                dot(nodes[fi], u)))
        faces.append((vi, loop))
    return nodes, faces
