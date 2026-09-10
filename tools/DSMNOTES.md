# Prim lists as dvp-as source

Notes on writing xtc DMA chains as `.dsm` text for `ee-dvp-as`, so prim
lists (and anything else that is "just a DMA list") can be made offline
and linked into the ELF like the microcode.  Everything here was checked
by assembling small files and reading the bytes back with
`ee-objdump -s -j .vudata` (2026-09-08); there is no SCE manual for the
DMA/VIF pseudo-ops, `man ee-dvp-as` only lists the generic options and
`sample/graphics/clip_vu1/torus.dsm` only ever uses `4,4,V4_32`.

`tools/primdsm.py` writes these files from geometry; `src/data/*.dsm`
are its output and get assembled by the normal `.dsm` rule (`joinvu`,
`cpp`, `ee-dvp-as`), so `src/data` is in `SRCDIRS`.  Scene `dsm` in
`src/scenes.c` draws two of them.

## Chain shapes

**Inline** (what `xtcpBuildList` records, what `tools/xpl.py` writes):
one `DMAcnt` per batch with the vertices in the tag's own transfer, the
last one a `DMAret`.  Per batch:

```
DMAcnt *                    ; qwc filled in by .EndDmaData
vifnop
stcycl 1, 4                 ; wl 1, cl 4 = one qword per vertex slot, stride 4
unpack[r] V4_32, 0, *       ; position -> TOPS + 0 + 4*i
.float 1.0, 2.0, 3.0, 0.0
...
.EndUnpack
unpack[r] V2_32, 1, *       ; texcoord
...
unpack[ru] V4_8, 2, *       ; colour, unsigned
.byte 255, 128, 64, 255
...
unpack[r] V3_8, 3, *        ; normal, signed (int)(n*127)
.byte 127, 0, -127
...
.EndUnpack
itop 60                     ; vertex count for the microcode
mscalf 0                    ; first batch; mscnt afterwards
vifnop                      ; flush on the last batch
.EndDmaData
```

Position independent, byte for byte the runtime's chain (verified with
`xpl.py` on a 1638 vertex sphere: identical 47920 bytes).

**Ref**: the attributes are contiguous arrays for the whole mesh and
the chain refs into them, one `DMAref` per attribute per batch, then a
`DMAcnt` carrying only the ITOP and the kick:

```
DMAref 48, monkey_pos + 768         ; qwc, address (linker relocates)
stcycl 1, 4                         ; only 2 VIF codes fit in a ref tag
unpackref 0x6C, 0x8000, 48          ; V4_32 at 0, TOPS-relative, 48 vertices
DMAref 24, monkey_uv + 384
vifnop
unpackref 0x64, 0x8001, 48
...
DMAcnt *
itop 48
mscnt
.EndDmaData
```

Only the chain depends on the pipeline (batch size, VU offsets,
stride); the arrays are plain data any pipeline can ref, and a skin
pipeline with a different VU layout only needs another small chain.

## Syntax, as verified

- **DMA tags**: `DMAcnt *`, `DMAret *`, `DMAref qwc, addr`, `DMAref *,
  label` (auto count, needs the target to be a `.DmaData label` ...
  `.EndDmaData` block), `DMAnext *, label`, `DMAcall *, label`, `DMAend`.
  A tag opens a DmaData block; `.EndDmaData` closes it, computes the
  qwc for `*` and pads to a quadword.  The next tag always starts on a
  quadword.
- **The two VIF codes in the tag**: whatever follows a tag goes into the
  upper 64 bits of the tag quadword (we run VIF1 with TTE), so a `cnt`
  tag's payload starts 8 bytes in and a `ref` tag has room for exactly
  two codes.  Nothing stops you writing a third one after a `DMAref`;
  it lands in the next quadword and the DMAC will read it as a tag.
- **`vifnop`** is the VIF NOP (`nop` is a VU instruction and fails
  outside `.vu`).
- **`stcycl wl, cl`**: WL first.  `stcycl 1, 4` gives immediate 0x0104,
  the same as the runtime's `SCE_VIF1_SET_STCYCL(1, stride, 0)` (that
  macro is `(wl, cl, irq)` too, `eestruct.h:84`).
- **`unpack[flags] [wl, cl,] FMT, addr, num`**: `r` sets the
  TOPS-relative bit (0x8000, our double buffer), `u` the unsigned bit
  (0x4000), `m` masks.  Formats `V4_32 V3_32 V2_32 V4_16 V3_16 V2_16
  V4_8 V3_8 V2_8 S_32 S_16 S_8 V4_5`.  With the `wl, cl` prefix the
  assembler emits an extra `stcycl` word in front of the unpack (that is
  what `torus.dsm`'s `unpack[r] 4, 4, V4_32, 0, *` does), so leave it
  out when you already set the cycle.  `*` counts the data lines up to
  `.EndUnpack`; an explicit number with no data still assembles (the
  number wins) but warns "specified length value doesn't match computed
  value" every time, which is why `primdsm.py` emits ref'd unpacks as a
  raw `.int` through the `unpackref` macro.
- **Data**: `.float`, `.int`, `.short`, `.byte` as usual; `vumacros.h`
  from the SCE include dir only adds `fxyzw` style aliases for them.
  `.EndUnpack` pads to a word (4 bytes), not a quadword, so `V3_8` data
  is followed by the next code at the next word boundary, exactly like
  `packVertices`.
- **Sections and symbols**: everything lands in `.vudata` (the linker
  places it next to `.data`; the microcode lives there too).  `.global`
  works, C sees `extern uint128 monkey_std[];`.  A `DMAref` address is
  a `R_MIPS_DVP_27_S4` relocation, `DMAref *, label` adds a
  `R_MIPS_16 .dma.qwcount.label` one, so label arithmetic like
  `monkey_pos + 768` is fine.
- **Cache**: linked data is in cached memory; `mdmaKick` writes back the
  whole cache before every kick, so nothing extra is needed to DMA from
  it.

## The ref alignment rule

A ref'd slice must start on a quadword and the DMA moves whole
quadwords, so per batch every attribute slice has to be a whole number
of quadwords: the batch vertex count (minus the strip overlap) must be
a multiple of `16 / gcd(16, bytesPerVertex)` for every attribute.
That is 1 for `V4_32`, 2 for `V2_32`, 4 for `V4_8`/`V3_32`, 8 for
`V3_16` and **16 for `V3_8`**.  With the std pipe's `V3_8` normals a
tri list therefore runs in batches of 48 instead of the 60 the
microcode allows (`Pipeline.refBatchSize`).  Storing normals as `V4_8`
(one pad byte) would relax that to 4 and cost 6% more DMA; the
microcode would not notice, it only sees VU memory.  The trailing bytes
of a short last slice are read as VIF codes after the unpack, so the
arrays are padded with zeros (`vifnop`).

## What the runtime and the tools share

`xtcpGetBatchInfo` (batch sizes per prim type from the microcode
footer), `xtcpBuildList` (the inline layout), `xpl.py` (the same in
Python, nolight only) and `primdsm.py` (any pipeline, both shapes,
reading the sizes and the input descriptor straight from the
microcode's `.dsm`) all agree byte for byte.  The `xtcPrimList` the
scene fills in is just `{ pipe, primtype, size, list }` pointing at the
linked symbol, and `xtcPrimListDraw` calls into it like into a recorded
one.


## Chunks

`chunk.inc` and `chk.ld` turn a `.dsm` of data into an xtc chunk file
(the format `common/chunk.c` loads); `xm2dsm.lua` writes a model,
`xan2dsm.lua` an animation list.
What was verified with both 2.9 toolchains:

- A section that should reach the output needs the `"a"` flag:
  `.section .chkreloc, "a"`.  Without it the linker drops the section
  silently.
- `.previous` swaps the current and the previous section, so a macro
  can visit one other section and come back.  Two nested switches do
  not come back.
- `.data` in dvp-as is `.vudata`; the linker script collects both.
- `SIZEOF(.section)` works in the script, arithmetic on two symbols
  (`(a - b)/4`) is a parse error in this ld.
- Macro arguments are split at whitespace as well as commas, so an
  expression argument must have no spaces: `chkref 12, sym+192`.
- Quotes are stripped from macro arguments; a macro that wants a string
  puts them back: `.asciz "\cls"`.
- `DMAref` works inside a macro; `\@` numbers the labels.
- The address of a DMA ref tag is its second word, a plain byte address
  (bit 31 is SPR), so the loader's "add the base to this word" fixup
  serves tags and pointers alike.
- `.float` in dvp-as rounds decimal to the nearest single exactly the
  way C's `atof` plus an assignment does: `tools/xanchkdiff.py` checked
  all 58211 keys of the fox's animation chunk against the text the
  loader would otherwise parse and found no difference, and the PS2
  renders the same frame from either to the pixel.
