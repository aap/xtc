# XTC

XTC aims to be a graphics library for the Playstation 2,
similar in spirit to traditional OpenGL.

## Features

* Standard OpenGL render states (as far as possible)
* VU1 rendering, including clipping
* Immediate rendering & simple display lists
* Lighting
* Textures

![image](https://github.com/aap/xtc/assets/1521437/2a864453-38d2-4035-acd4-76fffa109627)

## Architecture

A basic layer for dealing with DMA chains and packets
as well as graphics context initialization is provided by MDMA.
MDMA also implements a GS register cache, which can be flushed as needed.
On top of that sits XTC, which implements an OpenGL-like interface.

Where exactly the line between the two should be drawn
is not entirely clear to me yet.

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

There are currently three pipelines implemented:

* 2d (`twodPipeline`). Simple 2d transformations, no clipping
* 3d, no lighting (`nolightPipeline`)
* 3d, lighting (`defaultPipeline`). Clipping only for triangles

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

Render states are handled similarly to OpenGL.
GS register changes are cached so redundant state changes
should not cause terrible overhead.

### Lighting & Materials

Only ambient and directional lights are supported so far.
There is no support for specular lighting yet.

The material and lighting model used right now
is that of RenderWare.
This means a material consists of a color,
an intensity for ambient, diffuse and specular lighting each,
and a shininess.
Because there is specular lighting yet
the specular intensity and the shininess are currently unused.

Full OpenGL-compatibility would be nice
but is not very efficient
because of all the ways colors and be specified
and the lighting complexity.

### Textures

Currently all texture uploads are synchronous over PATH2.
This is of course inefficient and will be improved in the future.

A texture (`xtcRaster`) can currently be loaded from a PNG file
and used for rendering with `xtcBindTexture`.
How exactly multi-pass rendering and multi-texturing will work
is not clear yet.
Generally textures are pretty bare bones so far.
Expect the API to change. 

## To-do

XTC is still very bare bones.
It has most of the fundamental features but
still requires a lot of work on the details.

* General
	* figure out division between MDMA and XTC properly
	* some debugging/profiling functionality
	* better chain handling and buffer flipping
	* store/load resources from files
	* improve display lists

* Geometry
	* figure out vertex format or perhaps better: get rid of it

* Textures
	* PATH3 texture uploads
	* GS texture cache
	* swizzled textures
	* mipmapping

* Rendering
	* orthogonal projection
	* more fine grained clipping. clip/cull switch
	* backface culling
	* more render pipelines
		* some multi-pass effects (env mapping)
		* specular light
		* skinning
		* morphing?
	* try for more accurate OpenGL lighting model
	* material model should be per pipeline
	* don't re-upload matrix and lights all the time

* Toolchain
	* currently uses sony SDK, would be nice to support open source ps2sdk
		* inline assembly might be problematic

* Examples
	* examples to demonstrate the various features
	* simple skeleton with camera and scene-graph functionality
		* load models with assimp?

## Credits

I'm using the awesome lodepng library.
