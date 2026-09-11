#!/bin/sh
# vudiff.sh -- which VU instructions change when a define changes?
#
#	tools/vudiff.sh src/vu1/stdPipe.dsm vertexTop=0x2d0 [NAME=VALUE ...]
#	tools/vudiff.sh src/vu1/stdPipe.dsm -DFOO=1
#
# Assembles the microcode twice, as the Makefile does (joinvu, cpp,
# ee-dvp-as), once as it is and once with the given .equ values (or cpp
# defines) replaced, disassembles both with ee-objdump and prints every
# instruction whose encoding differs, with the source line it came
# from.  Nothing printed means the change reached no instruction: the
# equate is data (footer, switch table) only.  Working files under
# build/vudiff/.
set -e
[ $# -ge 2 ] || { echo "usage: $0 file.dsm NAME=VALUE|-DNAME=VALUE ..." >&2; exit 1; }
cd "$(dirname "$0")/.."
SRC=$1; shift
D=build/vudiff
mkdir -p $D
INC="-Isrc -Isrc/vu1 -Isrc/data -Iskeleton -Icommon"

# the variant: cpp defines pass through, equates are rewritten by a
# sed script (one command per line, so values may hold spaces)
: > $D/vars.sed
CPPDEF=""
for a in "$@"; do
	case "$a" in
	-D*)	CPPDEF="$CPPDEF $a";;
	*=*)	n=${a%%=*}; v=${a#*=}
		echo "s|^\\.equ[[:space:]]*$n,.*|.equ $n, $v|" >> $D/vars.sed;;
	*)	echo "vudiff: what is $a?" >&2; exit 1;;
	esac
done

PATH=$PATH:$(pwd) joinvu "$SRC" | cpp $INC | grep -v '^#' > $D/a.dsm_x
PATH=$PATH:$(pwd) joinvu "$SRC" | cpp $INC $CPPDEF | grep -v '^#' | sed -f $D/vars.sed > $D/b.dsm_x
ee-dvp-as -alm=$D/a.lst $INC $D/a.dsm_x -o $D/a.o
ee-dvp-as -alm=$D/b.lst $INC $D/b.dsm_x -o $D/b.o
ee-objdump -d -m dvp:vu -j .vutext $D/a.o > $D/a.dis
ee-objdump -d -m dvp:vu -j .vutext $D/b.o > $D/b.dis
lua tools/vudiff.lua $D/a.dis $D/b.dis $D/a.lst $D/b.lst
