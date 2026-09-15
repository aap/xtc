#!/bin/sh
# toolrun.sh -- run an ELF on the TOOL (DTL-T10000) through dsedb for a
# bounded time and print what the program said.
#
#	tools/toolrun.sh [-t SECS] [-g PATTERN] [-k] elf [args...]
#
# The TOOL is $DSNETM (devtool).  dsedb wants a terminal, so it runs
# under script(1) with a pty, flushing into a log; after SECS (default
# 30) the log is printed, filtered by PATTERN if given.  The program
# keeps running on the TOOL and dsedb stays attached as its console
# unless -k kills dsedb (the program then runs on without a console).
# One dsedb at a time: a new run kills the previous one first.
#
# host: paths resolve against this directory, like PCSX2's, so run it
# from the repo root.  The SDK build (make fox.elf etc.) is the one to
# use: the freesce ELFs speak PCSX2's FILEIO and hang on the TOOL.
PATH=/usr/local/sce/bin:$PATH
TIME=30; GREP=; KILL=0
while [ $# -gt 0 ]; do
	case "$1" in
	-t) TIME=$2; shift 2;;
	-g) GREP=$2; shift 2;;
	-k) KILL=1; shift;;
	-*) echo "toolrun: unknown option $1" >&2; exit 1;;
	*) break;;
	esac
done
[ $# -ge 1 ] || { echo "usage: toolrun.sh [-t SECS] [-g PATTERN] [-k] elf [args...]" >&2; exit 1; }
ELF=$1; shift
[ -f "$ELF" ] || { echo "toolrun: no such ELF: $ELF" >&2; exit 1; }
LOG=${TMPDIR:-/tmp}/toolrun.$$.log
pkill -x dsedb 2>/dev/null; sleep 1
script -fqec "dsedb -nokbd -r run $ELF $*" "$LOG" >/dev/null 2>&1 &
sleep "$TIME"
if [ -n "$GREP" ]; then tr -d '\r' < "$LOG" | grep -E "$GREP"; else tr -d '\r' < "$LOG" | grep -v '^$' | tail -n +2; fi
[ $KILL = 1 ] && pkill -x dsedb 2>/dev/null
rm -f "$LOG"
exit 0
