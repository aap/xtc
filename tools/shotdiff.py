#!/usr/bin/env python3
"""shotdiff.py -- how different are two screenshots?

	tools/shotdiff.py a.png b.png [more.png ...]

Crops the black side borders pcsx2run.sh's capture has, then counts the
pixels that differ at all and those that differ by more than 40 in some
channel: the first number is rounding noise between two builds of the
same thing (a few thousand for a quantized model, a few hundred for the
"Paused" text), the second is a real difference.  With more than two
files every one is compared against the first.
"""
import sys
from PIL import Image
import numpy as np

def crop(path):
	im = Image.open(path).convert("RGB")
	a = np.asarray(im)
	cols = np.where(a.max(axis=(0, 2)) > 0)[0]
	if len(cols):
		a = a[:, cols.min():cols.max()+1]
	return a.astype(int)

if len(sys.argv) < 3:
	sys.exit(__doc__)
ref = crop(sys.argv[1])
for path in sys.argv[2:]:
	b = crop(path)
	if b.shape != ref.shape:
		print("%s: size %s, %s is %s" % (path, b.shape[1::-1], sys.argv[1], ref.shape[1::-1]))
		continue
	d = np.abs(b - ref).max(axis=2)
	print("%-40s %7d px differ, %6d by more than 40" % (path, (d > 0).sum(), (d > 40).sum()))
