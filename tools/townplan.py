#!/usr/bin/env python3
"""
townplan.py -- the layout program for the Greek town.

This is the first "separate program" of the scene pipeline: it doesn't
make geometry (the building and vegetation .xpl sets have their own
generators), it makes the SCENE -- assets/ground.xpl for the terrain
and town.scene, the textual scene description the PS2 town scene and
tools/scenepreview.py both read.

Run from the repo root:  python3 tools/townplan.py
Preview:                 python3 tools/scenepreview.py town.scene out.png

The composition follows the client's concept image
(ref/c5133cda-dfd2-41a5-85d4-0717c0955afb.png): the temple crowning a
stepped terrace at the north with cypress flanks and a broad stair down
to the plaza; fountain house and votive statue holding the centre with
the stoa angled beside them; and a ring of whitewash-walled house
compounds -- the image's signature texture -- spreading downhill south,
stitched together with cypresses, olives and doorstep shrubs.
"""

import math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from xplkit import *

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), os.pardir)

# ---- the ground -----------------------------------------------------------
# Dead flat (the buildings assume it), sunk 3cm so building slabs never
# z-fight; a golden hill in colour: paved plaza, dust paths, dry grass.

PLAZA_R = 8.5
ROAD_HW = 2.0

def ground_col(p):
    x, y = p[0], p[1]
    r = math.hypot(x, y)
    dust  = (176, 160, 130)
    paved = (188, 178, 158)
    grass = (150, 140, 94)
    road = abs(x) < ROAD_HW and y < 2.0
    east = abs(y - x*0.15) < 1.5 and 2.0 < x < 30.0
    up   = abs(x) < 2.6 and 8.0 < y < 15.0        # to the temple stair
    t = smoothstep(PLAZA_R - 2.0, PLAZA_R + 3.0, r)
    c = mix(paved, dust, t)
    c = mix(c, grass, 0.55*smoothstep(0.45, 0.75, fbm2(x*0.18, y*0.18, 3, 5))
            + 0.35*smoothstep(24.0, 36.0, r))
    if road or east or up:
        c = mix(c, dust, 0.8)
    n = fbm2(x*0.35, y*0.35, 3, 7)
    c = tone(c, 0.9 + 0.2*n)
    if r < PLAZA_R:
        jx = abs(math.sin(x*math.pi/2.6))
        jy = abs(math.sin(y*math.pi/2.6))
        if min(jx, jy) < 0.09:
            c = tone(c, 0.93)
    return c

def make_ground(path):
    # modest triangles: huge polys trip xtc's clip microcode at some
    # view angles (the clipVertLimit TODO in im3d.dsm)
    b = Build(DAYLIGHT)
    rings, secs = 26, 72
    P = []
    for i in range(rings + 1):
        rr = 40.0*(i/rings)**1.15
        row = []
        for j in range(secs):
            a = j*TAU/secs
            wob = 1.0 + 0.05*fbm2(math.cos(a)*3.0, math.sin(a)*3.0, 2, 11)
            row.append((rr*wob*math.cos(a), rr*wob*math.sin(a), -0.03))
        P.append(row)
    b.grid(P, at(P, lambda p, i, j: ground_col(p)), closed_v=True)
    n = b.save(path)
    print('ground: %d verts -> %s' % (n, path))

# ---- the plan -------------------------------------------------------------
# rows: (file, x, y, z, rotz degrees, scale)
# buildings face -y; rotz 90 turns the front to +x, 180 to +y.

PLAN = []

def inst(name, x, y, rot=0.0, s=1.0, z=0.0):
    PLAN.append((name, x, y, z, rot, s))

def compound(cx, cy, nx, ny, rot=0.0, gate=None):
    """A walled yard: nx x ny wall segments (4m pitch), posts on the
    corners, a gate (one dropped segment flanked by posts) on the local
    south side at segment index `gate`."""
    a = math.radians(rot)
    ca, sa = math.cos(a), math.sin(a)
    def place(lx, ly, lrot):
        inst('gwall', cx + lx*ca - ly*sa, cy + lx*sa + ly*ca, rot + lrot)
    def post(lx, ly):
        inst('gwallpost', cx + lx*ca - ly*sa, cy + lx*sa + ly*ca, rot)
    dx, dy = nx*2.0, ny*2.0
    for i in range(nx):
        lx = (i + 0.5)*4.0 - dx
        if gate is not None and i == gate:
            post(lx - 2.0, -dy)
            post(lx + 2.0, -dy)
            continue
        place(lx, -dy, 0)
        place(lx, dy, 0)
    for j in range(ny):
        ly = (j + 0.5)*4.0 - dy
        place(dx, ly, 90)
        place(-dx, ly, 90)
    for sx in (-dx, dx):
        for sy in (-dy, dy):
            post(sx, sy)

inst('ground', 0, 0)

# -- the acropolis: terrace, temple, cypress flanks, hedge at the stair
inst('terrace', 0, 25)
inst('gtemple', 0, 24.5, 0, 1.0, z=1.2)
for yy in (19.0, 24.0, 29.0):
    inst('cypress', -8.2, yy, (yy*57) % 360, 0.95, z=1.2)
    inst('cypress',  8.2, yy, (yy*91) % 360, 1.0, z=1.2)
inst('hedge', -8.4, 13.8); inst('hedge', -6.4, 13.8); inst('hedge', -4.4, 13.8)

# -- the plaza: fountain house, votive statue, the stoa angled NE
inst('fountain', 0, 0.5, 0)
inst('shrine', 5.0, -2.0, -115)
inst('stoa', 12.5, 6.5, -35)
inst('figpot', 7.1, 5.8); inst('figpot', 10.4, 3.5); inst('figpot', 13.7, 1.2)

# -- the house ring: (variant, x, y, rot, scale, walled yard)
HOUSES = [
    ('house1', -16.0, 14.0, 125, 1.0,  (3, 3, 1)),
    ('house2', -18.0,  4.0,  95, 1.0,  None),
    ('house1', -20.0, -14.0, 55, 0.97, (3, 3, 1)),
    ('house2',  -8.0, -17.5, 205, 1.0, None),
    ('house1',   0.5, -20.5, 175, 1.04, (4, 3, 2)),
    ('house3',   9.5, -16.5, 220, 1.0, None),
    ('house2',  15.0, -11.0, 250, 0.95, None),
    ('house1',  29.0, -2.0, 265, 1.0,  (3, 3, 1)),
    ('house3',  21.0, 13.5, 305, 1.0,  None),
    ('house2',  15.0, 23.0, 190, 1.0,  None),
    ('house1', -16.5, 23.0, 140, 0.85, None),
    ('house1',  24.0, -17.0, 230, 0.92, None),
]
for name, x, y, rot, s, walls in HOUSES:
    inst(name, x, y, rot, s)
    if walls:
        compound(x, y, walls[0], walls[1], rot, gate=walls[2])

# -- planting between the compounds
for x, y, r, s in [(-19, 12, 30, 1.0), (-9, -12, 140, 0.9),
                   (4, -13.5, 220, 1.05), (13.5, -13.5, 80, 0.95),
                   (23.5, 6, 300, 1.0), (20, 15.5, 10, 0.9),
                   (-22.5, -7, 190, 1.05), (8, 12.5, 250, 0.92)]:
    inst('cypress', x, y, r, s)
for x, y, r, s in [(-24, 2, 40, 1.0), (26, -7, 160, 0.95),
                   (-4, -26, 280, 1.05), (17, 25, 90, 1.0),
                   (-25, 18, 210, 0.9)]:
    inst('olive' if (x + y) % 2 else 'olive2', x, y, r, s)
for x, y, r in [(-12, 13.5, 0), (0.5, -17.5, 70), (18.5, -6.5, 140)]:
    inst('laurel', x, y, r)
for x, y, r in [(-16.5, 8.5, 90), (20, 3.5, 200), (6.5, -18.5, 320)]:
    inst('oleander', x, y, r)

def main():
    make_ground(os.path.join(ROOT, 'assets', 'ground.xpl'))
    out = os.path.join(ROOT, 'town.scene')
    with open(out, 'w') as f:
        f.write('# generated by tools/townplan.py -- edit the plan there\n')
        f.write('cam 58 -1.25 0.6\n')
        for name, x, y, z, rot, s in PLAN:
            f.write('inst assets/%s.xpl %g %g %g %g %g\n'
                    % (name, x, y, z, rot, s))
    print('%d instances -> %s' % (len(PLAN), out))

if __name__ == '__main__':
    main()
