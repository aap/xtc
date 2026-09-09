#!/bin/sh
# Run an ELF in PCSX2 (flatpak) for a bounded time and kill it reliably.
#
#   pcsx2run.sh [-t secs] [-s scene] [-shot out.png] [-g regex] elf
#
#   -t secs      how long to let it run (default 15)
#   -s scene     boot into this scene (passed to the ELF as argv[1])
#   -shot f.png  grab the GS window right before shutdown
#   -g regex     print matching log lines afterwards (default: last 25)
#
# PCSX2 cannot run headless (its GS layer knows no offscreen platform,
# and there is no Xvfb here), so a window does appear -- but it goes
# away by itself and never piles up: everything is killed through
# flatpak, which earlier pkill attempts orphaned.
#
# PCSX2 must get an ABSOLUTE elf path or its IopHLE denies every host:
# open ("ELF directory: '.'").  host: paths resolve next to the ELF.

TIME=15
SCENE=
SHOT=
GREP=

while [ $# -gt 1 ]; do
	case "$1" in
	-t)    TIME=$2; shift 2;;
	-s)    SCENE=$2; shift 2;;
	-shot) SHOT=$2; shift 2;;
	-g)    GREP=$2; shift 2;;
	*)     echo "unknown option $1" >&2; exit 1;;
	esac
done
[ -z "$1" ] && { echo "usage: $0 [-t secs] [-s scene] [-shot out.png] [-g regex] elf" >&2; exit 1; }

ELF=$(realpath "$1")
LOG=$HOME/.cache/pcsx2run.log	# somewhere the flatpak can write
rm -f "$LOG"

cleanup() {
	flatpak kill net.pcsx2.PCSX2 2>/dev/null
}
trap cleanup EXIT INT TERM

if [ -n "$SCENE" ]; then
	flatpak run net.pcsx2.PCSX2 -batch -nogui -logfile "$LOG" \
		-gameargs "$SCENE" -- "$ELF" >/dev/null 2>&1 &
else
	flatpak run net.pcsx2.PCSX2 -batch -nogui -logfile "$LOG" \
		-- "$ELF" >/dev/null 2>&1 &
fi

sleep "$TIME"

if [ -n "$SHOT" ]; then
	# the GS window is titled after the elf, not "PCSX2"
	WID=$(xdotool search --name "$(basename "$ELF" .elf)" 2>/dev/null | tail -1)
	if [ -n "$WID" ]; then
		import -window "$WID" "$SHOT" 2>/dev/null && echo "shot: $SHOT"
	else
		echo "shot: no PCSX2 window found" >&2
	fi
fi

cleanup
sleep 1
trap - EXIT

if [ -n "$GREP" ]; then
	grep -aE "$GREP" "$LOG"
else
	tail -25 "$LOG"
fi
