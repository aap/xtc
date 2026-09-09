/*
 * OpenGL backend internals.  The public API is common/xtc.h.
 */

#ifndef XTCI_H
#define XTCI_H

#include "xtc.h"
#include <vector>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

struct xtcTexture
{
	u32 tex;
	u32 width, height;
};

// more than one texture unit, for what the GS can't do anyway
void xtcSetTextureN(int n, xtcTexture *tex);

struct xtcPipeline
{
	void (*upload)(void);
};

/*
 * Immediate mode
 */

struct xtcImmVertex3D
{
	Vec3 pos;
	xtcRGBA color;
	Vec3 normal;
	Vec3 texcoord;	// s, t, q
	// for skinning - maybe skip this for most cases somehow?
	u32 indices;
	Vec4 weights;
};

struct ImmState
{
	xtcPrimType primType;
	xtcImmVertex3D vert;
	std::vector<xtcImmVertex3D> vertstore;
	u32 vao;
	u32 vbo;
	int restartstrip;
};

void xtcFlush(void);

/*
 * Retained mode
 */

struct xtcPrimList
{
	xtcPrimType primType;
	u32 numVertices;
	u32 vao;
	u32 vbo;
};

void xtcPrimListSetData(xtcPrimList *pl, xtcPrimType primType, u32 numVertices, xtcImmVertex3D *vertices);

void xtcInit(void);

#endif
