# XTC

XTC aims to be a graphics library for the Playstation 2,
similar in spirit to traditional OpenGL.

## Features

* Standard OpenGL render states (as far as possible)
* VU1 rendering, including clipping
* Immediate rendering & simple display lists
* Lighting, RenderWare style and GL/PSP style
* Skinning
* Textures
* A portable model, skeleton and animation layer with text file formats
* An OpenGL implementation of the same API for prototyping and tooling

![image](https://github.com/aap/xtc/assets/1521437/2a864453-38d2-4035-acd4-76fffa109627)

## Layout

* `common/` -- the public API (`xtc.h`), the math (`xmath.h`) and the
  platform independent model, skeleton, animation and drawing code
  (`xmodel.*`, `xanim.cpp`, `xdraw.cpp`, the text and chunk formats).
  Built into both backends.
* `src/` -- the PS2 backend, the library and nothing else: the GS layer,
  VU1 pipelines and their microcode (`vu1/`), textures, host files
  (`xfile.c`), offline prim lists (`data/`).
* `skeleton/` -- the program layer both PS2 demos share, in librw's
  sense: the pad (`joy.c`), the boot ROM's FILEIO client (`fio.c`),
  the debug allocator (`mem.c`) and the graphics context, VIF list and
  frame loop (`skel.c`).
* `demos/` -- one directory per PS2 program, each linked into its own
  ELF in the repo root with both toolchains: `xtcdemo/` (the example
  and test scenes), `fox/` (the fox idling through a random walk of
  poses, model and animation as chunks) and `spyro/` (a Spyro the
  Dragon level; the assets are the game's, so they are not here:
  `demos/spyro/import.sh` makes them from the ripped game).
* `src_gl/` -- the OpenGL backend and sketch: the same API on GLFW/glad,
  plus assimp import, a Lua/Fennel driven viewer and demo scripts.
* `tools/` -- offline tooling: chunks and prim lists as dvp-as source
  (`chunk.inc`, `chk.ld`, `xm2dsm.lua`, `primdsm.py`, `DSMNOTES.md`),
  the tri stripper's test bench (`xstrip.cpp`), the xpl asset kit,
  OBJ helpers, animation cutting, the PCSX2 runner.
* `samples/` -- the sample assets, a submodule of
  [xtc-assets](https://github.com/aap/xtc-assets):
  the fox, the skinning test model (38 bones, 69 clips).
* `local/` -- gitignored, not in the repo: converted assets that may not
  be published. A demo loads `local/NAME/` over `host:` and the chunk
  rules build `build/chk/*.chk` from any `.xm` found there.

## Architecture

A basic layer for dealing with DMA chains and packets
as well as graphics context initialization is provided by MDMA.
MDMA also implements a GS register cache, which can be flushed as needed.
On top of that sits XTC, which implements an OpenGL-like interface.

Where exactly the line between the two should be drawn
is not entirely clear to me yet.

### The shared API

`common/xtc.h` is the one public header.
Each backend provides `xtcplat.h` (integer types) ahead of it and keeps
its internals in its own `xtci.h`.
The header fixes the conventions both backends follow:
column-major `Mat4` with the GL camera convention,
float colours as `Vec4` in 0..1,
vertex colours as bytes,
texture coordinates with row 0 at the top of the image,
and 8 light slots in world space.
The PS2 backend adapts where its hardware differs
(matrices are combined before the VU sees them,
colours are scaled to 0..255 at upload).

### Microcode & Pipelines

The VU1 microcode is very much inspired by RenderWare and
essentially works the same way.
The `xtcMicrocode` struct is defined in the assembly of each microcode
and holds things like buffer layout and code switch addresses.

The `xtcPipeline` struct defines a render pipeline to be selected by the application.
It is linked to a specific microcode and has a function that
does the necessary state changes and VIF uploads and renders the geometry.
Its batch descriptor (`xtcpBatchDesc`) describes the layout of the input buffer.
(TODO: this probably belongs to the microcode logically)

Pipelines are globals with the same names on both backends:

* `twodPipeline`. Simple 2d transformations, no clipping
* `nolightPipeline`. 3d, no lighting
* `defaultPipeline`. 3d, RenderWare style lighting, `xtcRwMaterial`
* `stdPipeline`. 3d, GL/PSP style lighting, `xtcStdMaterial`
* `skinPipeline`. `stdPipeline` plus skinning, up to 64 bones

The std pipe packs the enabled directional lights into matrices
in object space, so the diffuse term per vertex is one matrix multiply,
a clamp and a second matrix multiply per four lights.
Its lighting routine is picked per draw through a table of code addresses,
by light count and by which material terms take the vertex colour.
The skin pipe unpacks a fifth attribute (four weights with the matrix
offset in the low byte of each), skins vertices and normals in place,
compacts the vertex to the std layout and shares the rest of the code.

### Rendering

All rendering is handled by some pipeline,
which is selected by `xtcSetPipeline`.
After that normal `xtcBegin`/`xtcEnd` immediate mode style rendering can be done.

To reduce memory and cpu overhead geometry can be compiled
into a simple type of display list (`xtcPrimList`)
by wrapping the immediate draw commands in `xtcStartList` and `xtcEndList`.
Such a primlist can then be drawn with `xtcPrimListDraw`.
It is important to note that a primlist is tied to a specific
pipeline because of the input buffer layout.

Prim lists can also be made offline:
`tools/primdsm.py` writes the same DMA chain as dvp-as source,
either with the vertices inline (byte for byte what the console records)
or as a ref chain into per-attribute arrays that no pipeline layout touches.
`ee-dvp-as` assembles it and the linker puts it into the ELF
(`src/data/`, see `tools/DSMNOTES.md` for the syntax that was verified).

Render states are handled similarly to OpenGL.
GS register changes are cached so redundant state changes
should not cause terrible overhead.
The same goes for what the VU holds: the setters bump generation
counters for the transforms, the lights and the material,
and the std pipe only recomputes and re-uploads what changed since its
last draw, so a model with many materials pays for one matrix and one
light block.
`skeleton/skel.c` keeps three counters per frame, the EE building the
list, the DMA and GS running it, and the vsync wait, which is how the
Spyro level went from 20 to 60 frames per second: the hardware was idle
and the EE redid the same uploads 838 times.

### Lighting & Materials

Ambient and directional lights are supported;
the std pipe takes up to 8 directionals, the RW pipe as many as fit.
There is no specular lighting yet.

Materials are the constants of a pipeline, so there are two kinds:
`xtcRwMaterial` (a colour and ambient/diffuse/specular intensities)
for the default pipe and `xtcStdMaterial`
(emissive, ambient, diffuse and specular colours, the power in specular alpha)
for the std and skin pipes.
Which terms take the vertex colour instead is render state,
`xtcSetColorMaterial`, like `glColorMaterial`.

### Textures

Currently all texture uploads are synchronous over PATH2.
This is of course inefficient and will be improved in the future.

A texture (`xtcTexture`) can be loaded from a PNG with `xtcTextureReadPNG`
(4 and 8 bit palettes, 24 and 32 bit)
and used for rendering with `xtcSetTexture`.
How exactly multi-pass rendering and multi-texturing will work
is not clear yet.

### Models & Animation

`common/xmodel.h` has the portable data:
`xModel` with nodes, meshes, materials, geometry,
`xSkeleton` with RenderWare style push/pop bone flags,
`xSkin` with four weights and bone indices per vertex,
`xAnimation` with rotation, translation and scale key tracks,
and `xAnimPlayer` to drive a model with a clip.

Two file formats:
`.xm`/`.xan` are text and portable, written by the GL sketch
(from anything assimp reads, or a RenderWare DFF) and loaded on both backends;
`.chk` is a memory image with a fixup table, one read and one pass to load,
and per target.
`loadXModel` tells them apart by the first bytes.
`buildXModel` turns the geometry into prim lists through the std or skin pipe,
unless the file brought them,
and `xModelDraw` draws a model with its skeleton.
The PS2 reads the files over `host:` (`src/xfile.c`).

### Chunks from the assembler

The PS2's chunks are made by the toolchain, not by a converter of its own:
`tools/xm2dsm.lua` writes the model's structures as dvp-as source,
every pointer through the `ptr` macro of `tools/chunk.inc`,
which records the field's address in a fixup section,
and each mesh's prim list as a ref chain into per-attribute arrays
batched for the std or skin microcode.
`tools/chk.ld` links that at address 0 into the file the loader expects:
header, data, fixup table, global table.
Pointers to things outside the file, the pipeline of a prim list
and the texture of a material, are `global` entries, class and name,
that the loader hands to a resolver (`xModelResolve` in `common/xmodel.cpp`).
The PC writer (`writeXModelChunk`) produces the same layout,
with the textures as globals too.
`make chunks` builds `build/chk/fox.chk` from `samples/fox/fox.xm`,
which is what the fox scene loads;
`-nogeo` in `CHKFLAGS` leaves the geometry out and keeps only the chains.

### Tri strips

`common/tristrip.cpp` turns a triangle list into one stitched strip:
greedy paths through the dual graph, then Stewart's tunnel operator
joins them (a path alternating non-strip and strip edges between two
strip ends is complemented; a cycle is walked for and undone).
`xTriStripVerify` checks the strip against the source triangles,
`tools/xstrip` runs both over `.xm` files with vertices welded on all
attributes, and writes the strips the chunk converter takes.
The fox goes from 19956 list vertices to 9160, the chunk from 1.3 MB to 0.8.

### The GL sketch

`src_gl/` is where things get tried first.
`./xtc model` views a model and plays its clips
(`-save` writes the `.xm`/`.xan` files),
`./xtc -script lights.fnl` and `skin.fnl` are the lighting and skinning demos.
The screenshot mode (`-shot`) makes the renders reproducible,
so the PS2 has a picture to aim for.

## Building

The PS2 build has two flavours:
`make` with the Sony SDK toolchain and `make freesce` with freesce's,
which is binutils 2.9 and gcc 2.95 and compiles the C as C++.
Both expect MDMA as a sibling checkout,
and the sample scenes need the assets submodule (`git submodule update --init`).
`make` finds `ee-gcc` on the PATH, so the SDK's `ee/gcc/bin` has to come first.
`tools/pcsx2run.sh -s scene` runs a scene in PCSX2 for a bounded time
and can take a screenshot;
the scene files load from the directory of the ELF.

The GL sketch builds with `make` in `src_gl/`
and wants GLFW, assimp, Lua, Dear ImGui and librw.

## To-do

XTC has most of the fundamental features but
still requires a lot of work on the details.

* General
	* figure out division between MDMA and XTC properly
	* some debugging/profiling functionality
	* better chain handling and buffer flipping
	* animations as chunks too; chunks written by the console itself
	* a resident VU1 library with per-pipeline front ends
	* Lua on the PS2

* Textures
	* PATH3 texture uploads
	* a real GS texture cache (today: resident until the memory is full, then start over)
	* swizzled textures
	* mipmapping

* Rendering
	* orthogonal projection
	* more fine grained clipping. clip/cull switch
	* backface culling
	* more render pipelines
		* some multi-pass effects (env mapping)
		* specular light
		* sprites/particles from points
		* morphing?
	* the material/pass structure of the model layer
	* sort draws by texture and material

* Toolchain
	* currently uses (free!) sony SDK, would be nice to support open source ps2sdk
		* inline assembly might be problematic

## Credits

I'm using the awesome lodepng library.
