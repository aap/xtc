# tools

What every build uses.  The asset recipes are in `Makefile.assets` at
the top; the things that were experiments live under `experiments/`.

Assets (the chunk path, `.xm`/`.xan` text to what a demo loads):

* `xm2dsm.lua` -- an `.xm` model as dvp-as source: the xModel structs, the
  prim lists as ref chains, `-strips`, `-nogeo`, `-quant`; `-link` for an
  object linked straight into the ELF (DSMNOTES: Linked into the ELF).
* `xan2dsm.lua` -- an `.xan` animation list the same way.
* `xancut.py` -- cut an `.xan` down to named clips; `xanchkdiff.py` checks
  an animation chunk against its text.
* `xstrip.cpp` -- tri strips for `.xm` models (`common/tristrip.cpp`), and
  its test bench.
* `chunk.inc`, `chk.ld` -- the assembler side of the chunk format: header,
  `chkref`, globals; the linker script that makes the file.
* `DSMNOTES.md` -- the dvp-as syntax as verified, the chain shapes, the
  gotchas of the chunk and microcode work.

Microcode:

* `vudiff.sh`, `vudiff.lua` -- which VU instructions change when an
  equate changes (assemble twice, diff the code, not the data).
* `vumap.lua` -- the VU memory map a microcode's equates describe.

Running:

* `pcsx2run.sh` -- run an ELF in PCSX2 for a bounded time, boot into a
  scene, take a screenshot, grep the log.
