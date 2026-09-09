#!/usr/bin/env python3
"""Cut an .xan animation list down to the named clips, in that order:

    xancut.py fox/fox.xan fox/fox_ps2.xan A1_Stand_Idle_02 Loco_Walk Run

The format: a "numAnimations N" line, then one block per clip starting
with 'anim "Name" ...' and running to the next such line.
"""
import re, sys

def main():
    if len(sys.argv) < 4:
        sys.exit(__doc__)
    src, dst, names = sys.argv[1], sys.argv[2], sys.argv[3:]
    blocks = {}
    cur = None
    for line in open(src):
        m = re.match(r'anim "([^"]*)"', line)
        if m:
            cur = m.group(1)
            blocks[cur] = []
        if cur is not None:
            blocks[cur].append(line)
    missing = [n for n in names if n not in blocks]
    if missing:
        sys.exit("not in %s: %s\navailable: %s" % (src, ' '.join(missing), ' '.join(blocks)))
    with open(dst, 'w') as f:
        f.write("numAnimations %d\n" % len(names))
        for n in names:
            f.writelines(blocks[n])
    print("%s: %d of %d clips" % (dst, len(names), len(blocks)))

main()
