#!/usr/bin/env python3
"""
scenepreview.py -- software-render a .scene file (the town format).

Reads the same textual scene description the PS2 town scene loads:

    # comment
    cam <dist>                                  (ignored here)
    inst <file> <x> <y> <z> <rotz deg> <scale>

and renders the composed scene with the same painter's-algorithm
renderer xpl.py uses, so layout can be iterated without booting PCSX2.

    scenepreview.py town.scene out.png [views] [elev]

Paths in the scene file are relative to the scene file's directory.
"""

import math, os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import xpl


def load_scene(path):
    base = os.path.dirname(os.path.abspath(path))
    insts = []
    cache = {}
    for line in open(path):
        t = line.split('#')[0].split()
        if not t:
            continue
        if t[0] == 'inst':
            name = t[1]
            x, y, z, rz, s = (float(v) for v in t[2:7])
            if name not in cache:
                cache[name] = xpl.vertices(os.path.join(base, name))
            insts.append((name, x, y, z, math.radians(rz), s))
    return insts, cache


def scene_tris(insts, cache):
    tris = []
    for name, x, y, z, rz, s in insts:
        prim, verts = cache[name]
        c, sn = math.cos(rz), math.sin(rz)
        tv = []
        for v in verts:
            px = (v[0]*c - v[1]*sn)*s + x
            py = (v[0]*sn + v[1]*c)*s + y
            pz = v[2]*s + z
            tv.append((px, py, pz) + v[3:])
        tris += [t for t in xpl._triangles(prim, tv)
                 if t[0][:3] != t[1][:3] and t[1][:3] != t[2][:3]
                 and t[0][:3] != t[2][:3]]
    return tris


def render(tris, out, size=700, views=2, elev=0.42, shade=True):
    from PIL import Image, ImageDraw
    xs = [p[0] for t in tris for p in t]
    ys = [p[1] for t in tris for p in t]
    zs = [p[2] for t in tris for p in t]
    cx, cy = (min(xs) + max(xs))/2, (min(ys) + max(ys))/2
    cz = (min(zs) + max(zs))/2
    r = max(math.sqrt((p[0]-cx)**2 + (p[1]-cy)**2 + (p[2]-cz)**2)
            for t in tris for p in t) or 1.0
    img = Image.new('RGB', (size*views, size), (54, 60, 72))
    drw = ImageDraw.Draw(img)
    for view in range(views):
        theta = 0.7 + view*2*math.pi/max(views, 1)
        ct, st = math.cos(theta), math.sin(theta)
        ce, se = math.cos(elev), math.sin(elev)
        light = (ct*ce, st*ce, se)
        def project(v):
            x0, y0, z0 = v[0]-cx, v[1]-cy, v[2]-cz
            x = x0*ct + y0*st
            y = -x0*st + y0*ct
            sx = view*size + size/2 + y/r*size*0.46
            sy = size/2 - (z0*ce - x*se)/r*size*0.46
            return sx, sy, x*ce + z0*se
        polys = []
        for a, b, c in tris:
            pa, pb, pc = project(a), project(b), project(c)
            u = [b[i]-a[i] for i in range(3)]
            w = [c[i]-a[i] for i in range(3)]
            n = (u[1]*w[2]-u[2]*w[1], u[2]*w[0]-u[0]*w[2], u[0]*w[1]-u[1]*w[0])
            ln = math.sqrt(n[0]**2 + n[1]**2 + n[2]**2)
            if ln < 1e-12:
                continue
            f = 1.0
            if shade:
                d = abs(sum(n[i]*light[i] for i in range(3)))/ln
                f = 0.5 + 0.5*d
            col = tuple(min(255, int((a[5+i]+b[5+i]+c[5+i])/3*f))
                        for i in range(3))
            polys.append(((pa[2]+pb[2]+pc[2])/3, [pa[:2], pb[:2], pc[:2]], col))
        polys.sort(key=lambda p: p[0])
        for _, pts, col in polys:
            drw.polygon(pts, fill=col)
    img.save(out)


if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.exit(__doc__.strip())
    insts, cache = load_scene(sys.argv[1])
    tris = scene_tris(insts, cache)
    views = int(sys.argv[3]) if len(sys.argv) > 3 else 2
    elev = float(sys.argv[4]) if len(sys.argv) > 4 else 0.42
    render(tris, sys.argv[2], views=views, elev=elev)
    print('%d instances, %d triangles -> %s'
          % (len(insts), len(tris), sys.argv[2]))
