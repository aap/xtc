#!/bin/sh
# import.sh -- the Spyro levels from the game into local/spyro/, which is
# gitignored: nothing of the game goes into the repo, the pipeline does.
#
# One host tool does the whole import.  tools/spyroconv.c reads the
# game's archive -- WAD.WAD on the disc, aap's spyro1.WAD is the same
# 110 MB file -- and writes world.xm, sky.xm and the tex_NNN.png the
# materials name.  No librw, no spyroview, no DFFs, no VRAM dump: the
# texture page the old texconv.c wanted is itself a file in the archive.
#
# demos/spyro is a viewer of all 35 levels, so all 35 are converted, into
# $OUT/levels/levelNN/.
#
#	$WAD	the archive.  Tried in order: $WAD, $SPYRO/spyro1.WAD,
#		/mnt/wad.wad (a mounted disc), ./spyro1.WAD
#	$LEVEL	just this one, 0..34, instead of all of them
#	$OUT	where to put the result (default local/spyro)
#
# then `make chunks` turns the 70 .xm files into build/chk/spyro/
# levelNN_{world,sky}.chk, which demos/spyro loads.  That is 70 runs of
# the stripper, lua, dvp-as and ld, so `make -j8 chunks' if you can.
# Both steps run from here.
set -e
cd "$(dirname "$0")/../.."
SPYRO=${SPYRO:-/u/aap/src/spyro}
OUT=${OUT:-local/spyro}

if [ -z "$WAD" ]; then
	for f in "$SPYRO/spyro1.WAD" /mnt/wad.wad ./spyro1.WAD; do
		[ -s "$f" ] && { WAD=$f; break; }
	done
fi
[ -n "$WAD" ] && [ -s "$WAD" ] || {
	echo "import: no Spyro WAD; set \$WAD or \$SPYRO" >&2; exit 1; }

# 1. the converter, on the host
make build/host/spyroconv

# 2. the levels: world.xm, sky.xm and the textures, one directory each
mkdir -p "$OUT/levels"
if [ -n "$LEVEL" ]; then
	D=$(printf '%s/levels/level%02d' "$OUT" "$LEVEL")
	mkdir -p "$D"
	rm -f "$D"/tex_[0-9][0-9][0-9].png
	build/host/spyroconv -l "$LEVEL" ${MERGE--m} -o "$D" "$WAD"
else
	rm -rf "$OUT/levels"
	# -m: one mesh per texture, the sectors baked in.  A level is 800
	# draws of a dozen triangles otherwise, which the EE pays for and
	# the GS does not notice; MERGE= keeps the sectors
	build/host/spyroconv -a ${MERGE--m} -o "$OUT/levels" "$WAD"
fi
du -sh "$OUT/levels"

# 3. the chunks demos/spyro loads
make -j8 chunks
