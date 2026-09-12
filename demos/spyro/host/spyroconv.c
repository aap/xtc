/*
 * spyroconv -- Spyro the Dragon (PS1) level data straight into xtc assets.
 *
 * Everything comes out of the game's one big archive, WAD.WAD on the disc
 * (110'260'224 bytes, byte for byte the spyro1.WAD aap ripped).  No emulator
 * and no VRAM dump: the "VRAM" the old texconv.c wanted is itself a file in
 * the archive -- the level's texture page as the game DMAs it into VRAM.
 *
 * Layout, top down.  Every container in this game is the same thing: a table
 * of (u32 offset, u32 length) pairs from byte 0, terminated by a zero length,
 * offsets relative to the start of the container.
 *
 *	WAD.WAD				79 entries
 *	  file 0..9			menus, fonts, the dragons, ... (not us)
 *	  file 10, 12, 14, ... 78	one level each: 35 of them
 *	      sub 0			the texture page (raw 16 bit VRAM words)
 *	      sub 1			the level's "map" blocks (below)
 *	      sub 2..8			models, animations, collision -- not us
 *
 * A map block list is not a table but a chain: u32 total length (the length
 * word included), then length-4 bytes of payload, then the next one.  Six of
 * them, then a u32 count of alternative skies and that many (0x70 byte header
 * + one more chained block) after it.  What we want:
 *
 *	block 0		the texture descriptors
 *	block 1		the world: the level geometry, textured
 *	block 5		the sky: a coloured dome, no textures
 *
 * (block 2, 3 and 4 are small and not understood; the extra skies are the
 * same format as block 5 and this tool ignores them.)
 *
 * World and sky are both lists of little models the game calls sectors, each
 * with its own origin, 11:11:10 packed vertices and a palette of 32 bit
 * colours the polygons index.  A world sector carries two levels of detail;
 * we take the high one.  A world polygon is a quad (a triangle when its first
 * two corners are the same vertex) with a texture id and a rotation, and the
 * texture always covers the whole quad, so the texcoords are the four corners
 * of the image, turned by that rotation.
 *
 * Output, matching what the DFF detour produced (see demos/spyro/import.sh):
 *
 *	world.xm	the level, one xModel node per sector, one mesh per
 *			(sector, texture) pair, materials named tex_NNN
 *	sky.xm		the sky dome the same way, one mesh per sector
 *	tex_NNN.png	the level's textures, palettised PNGs
 *
 * Coordinates: as the DFF route had them.  z is up, the world is scaled by
 * 1/100 and shifted by (-50,-50,0), the sky by 1/10 with y and z negated.
 *
 * The .xm writer here is a transcription of writeXModel in common/xmodel.cpp
 * -- same order, same %f and %g -- rather than a call into it, because that
 * file wants a whole xtc backend behind it (xtcTexture, the chunk writer) and
 * this tool wants to be a host program with nothing but lodepng.
 *
 * Usage:
 *	spyroconv [-l N] [-f N] [-n NAME] [-o DIR] [-a] [-v] [-q] WAD
 *
 *	-l N	level index, 0..34 (default 0, the first level: Artisans)
 *	-f N	pick by raw WAD file index instead (10, 12, ... 78)
 *	-n NAME	write NAME.xm and NAME_sky.xm instead of world.xm and sky.xm
 *	-o DIR	where to write (default .); with -a, DIR/levelNN/
 *	-a	every level in the WAD
 *	-v	print the layout as it goes
 *	-q	no per-level summary
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <errno.h>

#include "lodepng.h"

typedef uint32_t u32;
typedef int32_t i32;
typedef uint16_t u16;
typedef int16_t i16;
typedef uint8_t u8;
typedef int8_t i8;

#define nil NULL
#define nelem(a) ((int)(sizeof(a)/sizeof(a[0])))

static int verbose;
static int merge;	/* -m: one mesh per texture for the whole level */
static int quiet;

static void*
emalloc(size_t sz)
{
	void *p = calloc(1, sz ? sz : 1);
	if(p == nil) {
		fprintf(stderr, "spyroconv: out of memory\n");
		exit(1);
	}
	return p;
}

/* ---- little endian reader over a memory block ---------------------------
 * Everything past the top level archive is in memory, and the point of a
 * reader with an error flag is the sweep over all 35 levels: a level whose
 * blocks do not parse must come back as a message, not a segfault.
 */

typedef struct Rd Rd;
struct Rd {
	const u8 *d;
	u32 len;
	u32 p;
	int err;
};

static Rd
rdinit(const u8 *d, u32 len)
{
	Rd r;
	r.d = d;
	r.len = len;
	r.p = 0;
	r.err = 0;
	return r;
}

static int
rdcheck(Rd *r, u32 n)
{
	if(r->err)
		return 0;
	if(r->p > r->len || n > r->len - r->p) {
		r->err = 1;
		return 0;
	}
	return 1;
}

static void
rdseek(Rd *r, u32 off)
{
	if(off > r->len) {
		r->err = 1;
		return;
	}
	r->p = off;
}

static void
rdskip(Rd *r, u32 n)
{
	if(!rdcheck(r, n))
		return;
	r->p += n;
}

static u32
rdu32(Rd *r)
{
	u32 v;
	if(!rdcheck(r, 4))
		return 0;
	v = (u32)r->d[r->p] | (u32)r->d[r->p+1]<<8 |
	    (u32)r->d[r->p+2]<<16 | (u32)r->d[r->p+3]<<24;
	r->p += 4;
	return v;
}

static i32 rdi32(Rd *r) { return (i32)rdu32(r); }

static u16
rdu16(Rd *r)
{
	u16 v;
	if(!rdcheck(r, 2))
		return 0;
	v = (u16)(r->d[r->p] | r->d[r->p+1]<<8);
	r->p += 2;
	return v;
}

static i16 rdi16(Rd *r) { return (i16)rdu16(r); }

/* ---- containers ---------------------------------------------------------- */

typedef struct Blob Blob;
struct Blob {
	const u8 *d;
	u32 len;
};

/*
 * Table of (offset, length) from byte 0, zero length ends it.  The first
 * entry's offset also bounds the table, which is how a container with no
 * terminator (the last table slot filled) still reads right.
 */
static int
tocCount(Blob b)
{
	u32 p, first;
	int n;

	first = b.len;
	for(n = 0, p = 0; p + 8 <= b.len; n++, p += 8) {
		u32 off = (u32)b.d[p] | (u32)b.d[p+1]<<8 | (u32)b.d[p+2]<<16 | (u32)b.d[p+3]<<24;
		u32 len = (u32)b.d[p+4] | (u32)b.d[p+5]<<8 | (u32)b.d[p+6]<<16 | (u32)b.d[p+7]<<24;
		if(len == 0)
			break;
		if(n == 0)
			first = off;
		if(p + 8 > first)
			break;
	}
	return n;
}

static int
tocEntry(Blob b, int i, Blob *out)
{
	u32 off, len, p;

	p = (u32)i*8;
	if(i < 0 || p + 8 > b.len)
		return 0;
	off = (u32)b.d[p] | (u32)b.d[p+1]<<8 | (u32)b.d[p+2]<<16 | (u32)b.d[p+3]<<24;
	len = (u32)b.d[p+4] | (u32)b.d[p+5]<<8 | (u32)b.d[p+6]<<16 | (u32)b.d[p+7]<<24;
	if(len == 0 || off > b.len || len > b.len - off)
		return 0;
	out->d = b.d + off;
	out->len = len;
	return 1;
}

/*
 * The chain of map blocks: u32 length (itself included), payload, repeat.
 * Six of them; we only ever want three, so read them all into an array.
 */
enum { NMAPBLOCK = 6 };

static int
mapBlocks(Blob c, Blob *blocks)
{
	u32 p;
	int i;

	for(i = 0, p = 0; i < NMAPBLOCK; i++) {
		u32 len;
		if(p + 4 > c.len)
			return i;
		len = (u32)c.d[p] | (u32)c.d[p+1]<<8 | (u32)c.d[p+2]<<16 | (u32)c.d[p+3]<<24;
		if(len < 4 || len > c.len - p)
			return i;
		blocks[i].d = c.d + p + 4;
		blocks[i].len = len - 4;
		p += len;
	}
	return i;
}

/* ---- the xModel we build ------------------------------------------------- */

typedef struct XVert XVert;
struct XVert {
	float vtx[3];
	float nrm[3];
	float tex[2];
	u8 col[4];
};

typedef struct XMesh XMesh;
struct XMesh {
	int mat;
	int numVerts, numTris;
	XVert *verts;
	int *idx;		/* 3*numTris */
};

typedef struct XMat XMat;
struct XMat {
	int hasTex;
	char tex[16];
};

typedef struct XNode XNode;
struct XNode {
	float pos[3];
	int firstMesh, numMeshes;
};

typedef struct XModel XModel;
struct XModel {
	XMat *mats;   int nmats,   matcap;
	XMesh *meshes; int nmeshes, meshcap;
	XNode *nodes;  int nnodes,  nodecap;
};

#define GROW(m, arr, n, cap) do { \
	if((m)->n == (m)->cap) { \
		(m)->cap = (m)->cap ? (m)->cap*2 : 64; \
		(m)->arr = realloc((m)->arr, (size_t)(m)->cap*sizeof(*(m)->arr)); \
		if((m)->arr == nil) { fprintf(stderr, "spyroconv: out of memory\n"); exit(1); } \
	} \
} while(0)

static XMat*
addMat(XModel *m)
{
	GROW(m, mats, nmats, matcap);
	memset(&m->mats[m->nmats], 0, sizeof(XMat));
	return &m->mats[m->nmats++];
}

static XMesh*
addMesh(XModel *m)
{
	GROW(m, meshes, nmeshes, meshcap);
	memset(&m->meshes[m->nmeshes], 0, sizeof(XMesh));
	return &m->meshes[m->nmeshes++];
}

static XNode*
addNode(XModel *m)
{
	GROW(m, nodes, nnodes, nodecap);
	memset(&m->nodes[m->nnodes], 0, sizeof(XNode));
	return &m->nodes[m->nnodes++];
}

static void
freeModel(XModel *m)
{
	int i;
	for(i = 0; i < m->nmeshes; i++) {
		free(m->meshes[i].verts);
		free(m->meshes[i].idx);
	}
	free(m->mats);
	free(m->meshes);
	free(m->nodes);
	memset(m, 0, sizeof(*m));
}

/* ---- the text writer -- writeXModel in common/xmodel.cpp ------------------ */

static const char*
ind(int level)
{
	static char spaces[64];
	int n = level*2;
	if(n > (int)sizeof(spaces)-1)
		n = (int)sizeof(spaces)-1;
	memset(spaces, ' ', (size_t)n);
	spaces[n] = '\0';
	return spaces;
}

static void
writeMaterial(FILE *f, XMat *mat, int n)
{
	fprintf(f, "material %d\n", n);
	/* the DFF route's materials are RenderWare defaults: white, with
	 * surface properties all 1, no shininess and no emissive */
	fprintf(f, "\tambient %g %g %g %g\n", 1.0, 1.0, 1.0, 1.0);
	fprintf(f, "\tdiffuse %g %g %g %g\n", 1.0, 1.0, 1.0, 1.0);
	fprintf(f, "\tspecular %g %g %g %g\n", 1.0, 1.0, 1.0, 0.0);
	fprintf(f, "\temissive %g %g %g %g\n", 0.0, 0.0, 0.0, 1.0);
	/* XTC_EMISSIVE: the vertex colour is the emissive term, which is how
	 * RenderWare draws a prelit unlit world */
	fprintf(f, "\tcolormaterial %u\n", 1u);
	if(mat->hasTex)
		fprintf(f, "\tdiffusetex \"%s\"\n", mat->tex);
	fprintf(f, "endmaterial\n");
}

static void
writeMesh(FILE *f, XMesh *mesh, int n)
{
	int i;

	fprintf(f, "mesh %d\n", n);
	fprintf(f, "\tmaterial %d\n", mesh->mat);
	fprintf(f, "\tnumVerts %d\n", mesh->numVerts);
	fprintf(f, "\tnumFaces %d\n", mesh->numTris);
	for(i = 0; i < mesh->numVerts; i++) {
		XVert *vx = &mesh->verts[i];
		fprintf(f, "\tv %f %f %f\n", vx->vtx[0], vx->vtx[1], vx->vtx[2]);
		fprintf(f, "\tn %f %f %f\n", vx->nrm[0], vx->nrm[1], vx->nrm[2]);
		fprintf(f, "\tt %f %f\n", vx->tex[0], vx->tex[1]);
		fprintf(f, "\tc %d %d %d %d\n", vx->col[0], vx->col[1], vx->col[2], vx->col[3]);
	}
	for(i = 0; i < mesh->numTris; i++)
		fprintf(f, "\tf %d %d %d\n", mesh->idx[i*3+0], mesh->idx[i*3+1], mesh->idx[i*3+2]);
	fprintf(f, "endmesh\n");
}

static void
writeXm(FILE *f, XModel *m)
{
	int i, j, level;

	fprintf(f, "numMaterials %d\n", m->nmats);
	fprintf(f, "numMeshes %d\n", m->nmeshes);
	for(i = 0; i < m->nmats; i++)
		writeMaterial(f, &m->mats[i], i);
	for(i = 0; i < m->nmeshes; i++)
		writeMesh(f, &m->meshes[i], i);

	/* the root, then one child node per sector */
	level = 0;
	fprintf(f, "%snode \"\"\n", ind(level));
	level++;
	fprintf(f, "%sxform %g %g %g  %g %g %g  %g %g %g  %g %g %g\n", ind(level),
		1.0, 0.0, 0.0,  0.0, 1.0, 0.0,  0.0, 0.0, 1.0,  0.0, 0.0, 0.0);
	for(i = 0; i < m->nnodes; i++) {
		XNode *n = &m->nodes[i];
		fprintf(f, "%snode \"\"\n", ind(level));
		level++;
		fprintf(f, "%sxform %g %g %g  %g %g %g  %g %g %g  %g %g %g\n", ind(level),
			1.0, 0.0, 0.0,  0.0, 1.0, 0.0,  0.0, 0.0, 1.0,
			n->pos[0], n->pos[1], n->pos[2]);
		if(n->numMeshes > 0)
			fprintf(f, "%snumMeshes %d\n", ind(level), n->numMeshes);
		for(j = 0; j < n->numMeshes; j++)
			fprintf(f, "%smesh %d\n", ind(level), n->firstMesh + j);
		level--;
		fprintf(f, "%sendnode\n", ind(level));
	}
	level--;
	fprintf(f, "%sendnode\n", ind(level));
}

/* ---- sector -> meshes ----------------------------------------------------
 * A sector's polygons index three parallel things: a position, a colour and
 * (in the world) one of the four corners of the texture.  A vertex is that
 * triple, so build the unique-triple list first, then the triangles, then cut
 * the result up by material the way the GL sketch's DFF importer does: one
 * mesh per material, its vertices numbered in the order the triangles first
 * reach them.
 */

typedef struct VertIdx VertIdx;
struct VertIdx {
	int pos, color, tex;
};

typedef struct Tri Tri;
struct Tri {
	int v[3];
	int mat;
};

typedef struct Sector Sector;
struct Sector {
	VertIdx *vidx;  int nvidx, vidxcap;
	Tri *tris;      int ntris, triscap;
	float (*pos)[3];		/* [npos] */
	u8 (*col)[4];			/* [ncol] */
	int npos, ncol;
};

static int
findVertIdx(Sector *s, int pos, int color, int tex)
{
	int i;
	for(i = 0; i < s->nvidx; i++)
		if(s->vidx[i].pos == pos && s->vidx[i].color == color &&
		   s->vidx[i].tex == tex)
			return i;
	return -1;
}

static int
getVertIdx(Sector *s, int pos, int color, int tex)
{
	int i = findVertIdx(s, pos, color, tex);
	if(i >= 0)
		return i;
	if(s->nvidx == s->vidxcap) {
		s->vidxcap = s->vidxcap ? s->vidxcap*2 : 64;
		s->vidx = realloc(s->vidx, (size_t)s->vidxcap*sizeof(VertIdx));
		if(s->vidx == nil) { fprintf(stderr, "spyroconv: out of memory\n"); exit(1); }
	}
	s->vidx[s->nvidx].pos = pos;
	s->vidx[s->nvidx].color = color;
	s->vidx[s->nvidx].tex = tex;
	return s->nvidx++;
}

static void
addTri(Sector *s, int a, int b, int c, int mat)
{
	if(s->ntris == s->triscap) {
		s->triscap = s->triscap ? s->triscap*2 : 64;
		s->tris = realloc(s->tris, (size_t)s->triscap*sizeof(Tri));
		if(s->tris == nil) { fprintf(stderr, "spyroconv: out of memory\n"); exit(1); }
	}
	s->tris[s->ntris].v[0] = a;
	s->tris[s->ntris].v[1] = b;
	s->tris[s->ntris].v[2] = c;
	s->tris[s->ntris].mat = mat;
	s->ntris++;
}

static void
freeSector(Sector *s)
{
	free(s->vidx);
	free(s->tris);
	free(s->pos);
	free(s->col);
	memset(s, 0, sizeof(*s));
}

/* the four corners of the image, in the order the polygon's corners go */
static const float corners[4][2] = {
	{ 0.0f, 1.0f },
	{ 1.0f, 1.0f },
	{ 1.0f, 0.0f },
	{ 0.0f, 0.0f },
};

/*
 * Split by material and append to the model, one mesh each, plus the node
 * that holds them.  texIDs nil means untextured: no diffusetex on the
 * materials, and the texcoords stay (0,0) the way an untextured RenderWare
 * geometry has none at all.
 */
static void
emitSector(XModel *m, Sector *s, int nmats, const int *texIDs, const float *pos)
{
	int mi, i, k;
	XNode *node;
	int *imap = emalloc((size_t)s->nvidx*sizeof(int) + 1);

	node = addNode(m);
	node->pos[0] = pos[0];
	node->pos[1] = pos[1];
	node->pos[2] = pos[2];
	node->firstMesh = m->nmeshes;
	node->numMeshes = nmats;

	for(mi = 0; mi < nmats; mi++) {
		XMesh *mesh;
		XMat *mat;
		int numVerts = 0, numTris = 0;

		mat = addMat(m);
		if(texIDs) {
			mat->hasTex = 1;
			snprintf(mat->tex, sizeof(mat->tex), "tex_%03d", texIDs[mi]);
		}

		for(i = 0; i < s->nvidx; i++)
			imap[i] = -1;
		for(i = 0; i < s->ntris; i++) {
			if(s->tris[i].mat != mi)
				continue;
			numTris++;
			for(k = 0; k < 3; k++)
				if(imap[s->tris[i].v[k]] < 0)
					imap[s->tris[i].v[k]] = numVerts++;
		}

		mesh = addMesh(m);
		mesh->mat = m->nmats-1;
		mesh->numVerts = numVerts;
		mesh->numTris = numTris;
		mesh->verts = emalloc((size_t)numVerts*sizeof(XVert) + 1);
		mesh->idx = emalloc((size_t)numTris*3*sizeof(int) + 1);

		for(i = 0; i < s->nvidx; i++) {
			VertIdx *vi = &s->vidx[i];
			XVert *vx;
			if(imap[i] < 0)
				continue;
			vx = &mesh->verts[imap[i]];
			vx->vtx[0] = s->pos[vi->pos][0];
			vx->vtx[1] = s->pos[vi->pos][1];
			vx->vtx[2] = s->pos[vi->pos][2];
			if(vi->tex >= 0) {
				vx->tex[0] = corners[vi->tex][0];
				vx->tex[1] = corners[vi->tex][1];
			}
			vx->col[0] = s->col[vi->color][0];
			vx->col[1] = s->col[vi->color][1];
			vx->col[2] = s->col[vi->color][2];
			vx->col[3] = s->col[vi->color][3];
		}
		numTris = 0;
		for(i = 0; i < s->ntris; i++) {
			if(s->tris[i].mat != mi)
				continue;
			for(k = 0; k < 3; k++)
				mesh->idx[numTris*3+k] = imap[s->tris[i].v[k]];
			numTris++;
		}
	}

	free(imap);
}

/*
 * -m: the level as one mesh per material.  The sectors are how the game
 * pages the level in, not something a renderer that draws all of it
 * wants: 800 draws of a dozen triangles cost the EE more than the
 * triangles cost the GS.  So bake every sector's offset into its
 * vertices and concatenate all meshes of the same texture, under one
 * node at the origin.  Materials are only a texture name here, so the
 * name is the key.
 */
static void
mergeModel(XModel *m)
{
	XModel out;
	XNode *root;
	int i, j, k, mi;
	int *matmap = emalloc((size_t)m->nmats*sizeof(int) + 1);

	memset(&out, 0, sizeof(out));
	root = addNode(&out);
	root->pos[0] = root->pos[1] = root->pos[2] = 0.0f;
	root->firstMesh = 0;

	/* one output material and mesh per distinct texture, in order of
	 * first use */
	for(i = 0; i < m->nmats; i++) {
		for(j = 0; j < i; j++)
			if(m->mats[j].hasTex == m->mats[i].hasTex &&
			   strcmp(m->mats[j].tex, m->mats[i].tex) == 0)
				break;
		if(j < i) {
			matmap[i] = matmap[j];
			continue;
		}
		matmap[i] = out.nmats;
		*addMat(&out) = m->mats[i];
		addMesh(&out);
		out.meshes[out.nmeshes-1].mat = out.nmats-1;
	}
	root->numMeshes = out.nmeshes;

	/* sizes first, then the data */
	for(k = 0; k < m->nnodes; k++) {
		XNode *n = &m->nodes[k];
		for(j = 0; j < n->numMeshes; j++) {
			XMesh *src = &m->meshes[n->firstMesh + j];
			XMesh *dst = &out.meshes[matmap[src->mat]];
			dst->numVerts += src->numVerts;
			dst->numTris += src->numTris;
		}
	}
	for(mi = 0; mi < out.nmeshes; mi++) {
		XMesh *dst = &out.meshes[mi];
		dst->verts = emalloc((size_t)dst->numVerts*sizeof(XVert) + 1);
		dst->idx = emalloc((size_t)dst->numTris*3*sizeof(int) + 1);
		dst->numVerts = 0;
		dst->numTris = 0;
	}
	for(k = 0; k < m->nnodes; k++) {
		XNode *n = &m->nodes[k];
		for(j = 0; j < n->numMeshes; j++) {
			XMesh *src = &m->meshes[n->firstMesh + j];
			XMesh *dst = &out.meshes[matmap[src->mat]];
			int base = dst->numVerts;
			for(i = 0; i < src->numVerts; i++) {
				XVert v = src->verts[i];
				v.vtx[0] += n->pos[0];
				v.vtx[1] += n->pos[1];
				v.vtx[2] += n->pos[2];
				dst->verts[dst->numVerts++] = v;
			}
			for(i = 0; i < src->numTris*3; i++)
				dst->idx[dst->numTris*3 + i] = src->idx[i] + base;
			dst->numTris += src->numTris;
		}
	}

	free(matmap);
	freeModel(m);
	*m = out;
}

/* ---- the world ----------------------------------------------------------
 * map block 1.  i32 sector count, then that many u32 offsets, then the
 * sectors.  A sector header is
 *
 *	4 x i16		a bounding sphere, going by the numbers
 *	i16 y, i16 x, i16 ?, i16 z	the sector's origin, 1/100 units
 *	u32 lo, u32 hi			counts for the two detail levels:
 *					verts, colours, polys in bytes 0,1,2
 *	i32 ?
 *
 * then the low detail arrays (verts, colours, polys of 2 words) and the high
 * detail ones (verts, colours, a second colour array the game lights with,
 * polys of 4 words).  We take the high level of detail and its first colour
 * array, which is what spyroview's ReadWorldTextured does.
 */
static int
convWorld(XModel *m, Blob b, int *nskipped)
{
	Rd r = rdinit(b.d, b.len);
	i32 nsec;
	u32 *offsets;
	int i, j;
	const float scale = 1.0f/100.0f;

	nsec = rdi32(&r);
	if(r.err || nsec < 0 || (u32)nsec > b.len/4)
		return -1;
	offsets = emalloc((size_t)nsec*sizeof(u32) + 1);
	for(i = 0; i < nsec; i++)
		offsets[i] = rdu32(&r);
	if(r.err) {
		free(offsets);
		return -1;
	}

	for(i = 0; i < nsec; i++) {
		Sector s;
		float pos[3];
		u32 lo, hi;
		int nverts, ncolors, npolys;
		int texIDs[128], nmats;

		memset(&s, 0, sizeof(s));
		rdseek(&r, offsets[i]);
		rdskip(&r, 8);			/* bounds */
		pos[1] = rdi16(&r)*scale;
		pos[0] = rdi16(&r)*scale;
		rdskip(&r, 2);
		pos[2] = rdi16(&r)*scale;
		lo = rdu32(&r);
		hi = rdu32(&r);
		rdskip(&r, 4);
		/* the level is built around the origin, the data is not */
		pos[0] -= 50.0f;
		pos[1] -= 50.0f;
		if(r.err) { free(offsets); return -1; }

		nverts  = lo & 0xFF;
		ncolors = (lo>>8) & 0xFF;
		npolys  = (lo>>16) & 0xFF;
		rdskip(&r, (u32)nverts*4);
		rdskip(&r, (u32)ncolors*4);
		rdskip(&r, (u32)npolys*8);

		nverts  = hi & 0xFF;
		ncolors = (hi>>8) & 0xFF;
		npolys  = (hi>>16) & 0xFF;
		if(r.err) { free(offsets); return -1; }
		if(npolys == 0) {
			/* an empty sector gets no node at all */
			if(nskipped) (*nskipped)++;
			continue;
		}

		s.npos = nverts;
		s.ncol = ncolors;
		s.pos = emalloc((size_t)nverts*sizeof(*s.pos) + 1);
		s.col = emalloc((size_t)ncolors*sizeof(*s.col) + 1);
		for(j = 0; j < nverts; j++) {
			u32 v = rdu32(&r);
			int z = v & 0x3FF;
			int y = (v>>10) & 0x7FF;
			int x = (v>>21) & 0x7FF;
			s.pos[j][0] = x*scale;
			s.pos[j][1] = y*scale;
			s.pos[j][2] = z*scale;
		}
		for(j = 0; j < ncolors; j++) {
			u32 c = rdu32(&r);
			s.col[j][0] = c & 0xFF;
			s.col[j][1] = (c>>8) & 0xFF;
			s.col[j][2] = (c>>16) & 0xFF;
			s.col[j][3] = 0xFF;
		}
		rdskip(&r, (u32)ncolors*4);	/* the lighting colours */
		if(r.err) { freeSector(&s); free(offsets); return -1; }

		for(j = 0; j < nelem(texIDs); j++)
			texIDs[j] = -1;
		nmats = 0;

		for(j = 0; j < npolys; j++) {
			u32 v = rdu32(&r);
			u32 c = rdu32(&r);
			u32 a = rdu32(&r);
			u32 bb = rdu32(&r);
			int rot, istri, vs[4], k, tid, mat;

			if(r.err) break;

			/* how far the texture is turned on the quad; a
			 * triangle never carries one */
			rot = (a>>8) & 3;
			istri = (v&0xFF) == ((v>>8)&0xFF);
			if(!istri)
				rot = 0;
			if(rot & 1)
				rot ^= 2;

			for(k = 0; k < 4; k++) {
				int p = (v >> (8*k)) & 0xFF;
				int cc = (c >> (8*k)) & 0xFF;
				if(p >= nverts || cc >= ncolors) { r.err = 1; break; }
				vs[k] = getVertIdx(&s, p, cc, (k + rot)%4);
			}
			if(r.err) break;

			tid = a & 0x7F;
			for(mat = 0; mat < nelem(texIDs); mat++) {
				if(texIDs[mat] == tid)
					break;
				if(texIDs[mat] == -1) {
					texIDs[mat] = tid;
					nmats++;
					break;
				}
			}
			if(mat == nelem(texIDs)) { r.err = 1; break; }

			/* the quad's winding flips with bit 1 of the last word */
			if(vs[0] != vs[1])
				addTri(&s, vs[0 ^ (bb&2)], vs[1], vs[2 ^ (bb&2)], mat);
			addTri(&s, vs[0 ^ (bb&2)], vs[2 ^ (bb&2)], vs[3], mat);
		}
		if(r.err) { freeSector(&s); free(offsets); return -1; }

		emitSector(m, &s, nmats, texIDs, pos);
		freeSector(&s);
	}

	free(offsets);
	return 0;
}

/* ---- the sky ------------------------------------------------------------
 * map block 5.  u32 background colour, i32 sector count, u32 offsets, then
 * the sectors:
 *
 *	4 x i16		bounds again
 *	i16 y, i16 z, u16 nverts, i16 x, u16 npolys, u16 ncolors
 *	i32 ?
 *
 * The sky's units are 1/10; y and z come out
 * negated.  Polygons are triangles, three 10 bit indices per word, and there
 * are no textures.
 */
static int
convSky(XModel *m, Blob b, u32 *bgcolor)
{
	Rd r = rdinit(b.d, b.len);
	i32 nsec;
	u32 *offsets;
	int i, j;

	if(bgcolor)
		*bgcolor = rdu32(&r);
	else
		rdu32(&r);
	nsec = rdi32(&r);
	if(r.err || nsec < 0 || (u32)nsec > b.len/4)
		return -1;
	offsets = emalloc((size_t)nsec*sizeof(u32) + 1);
	for(i = 0; i < nsec; i++)
		offsets[i] = rdu32(&r);
	if(r.err) { free(offsets); return -1; }

	for(i = 0; i < nsec; i++) {
		Sector s;
		float pos[3];
		int nverts, npolys, ncolors;

		memset(&s, 0, sizeof(s));
		rdseek(&r, offsets[i]);
		rdskip(&r, 8);			/* bounds */
		pos[1] = -rdi16(&r)/10.0f;
		pos[2] = -rdi16(&r)/10.0f;
		nverts = rdu16(&r);
		pos[0] = rdi16(&r)/10.0f;
		npolys = rdu16(&r);
		ncolors = rdu16(&r);
		rdskip(&r, 4);
		if(r.err) { free(offsets); return -1; }

		s.npos = nverts;
		s.ncol = ncolors;
		s.pos = emalloc((size_t)nverts*sizeof(*s.pos) + 1);
		s.col = emalloc((size_t)ncolors*sizeof(*s.col) + 1);
		for(j = 0; j < nverts; j++) {
			u32 v = rdu32(&r);
			/* unsigned offsets from the sector's origin, like the
			 * world's.  spyroview read them sign-magnitude, which no
			 * level below 512 in z ever contradicted; Town Square
			 * does, and tore */
			int z = v & 0x3FF;
			int y = (v>>10) & 0x7FF;
			int x = (v>>21) & 0x7FF;
			s.pos[j][0] = x/10.0f;
			s.pos[j][1] = y/10.0f;
			s.pos[j][2] = z/10.0f;
		}
		for(j = 0; j < ncolors; j++) {
			u32 c = rdu32(&r);
			s.col[j][0] = c & 0xFF;
			s.col[j][1] = (c>>8) & 0xFF;
			s.col[j][2] = (c>>16) & 0xFF;
			s.col[j][3] = 0xFF;
		}
		if(r.err) { freeSector(&s); free(offsets); return -1; }

		for(j = 0; j < npolys; j++) {
			u32 v = rdu32(&r);
			u32 c = rdu32(&r);
			int vs[3], k;

			if(r.err) break;
			v >>= 2;
			c >>= 2;
			for(k = 0; k < 3; k++) {
				int p = (v >> (10*k)) & 0x3FF;
				int cc = (c >> (10*k)) & 0x3FF;
				if(p >= nverts || cc >= ncolors) { r.err = 1; break; }
				vs[k] = getVertIdx(&s, p, cc, -1);
			}
			if(r.err) break;
			addTri(&s, vs[0], vs[1], vs[2], 0);
		}
		if(r.err) { freeSector(&s); free(offsets); return -1; }

		emitSector(m, &s, 1, nil, pos);
		freeSector(&s);
	}

	free(offsets);
	return 0;
}

/* ---- the textures --------------------------------------------------------
 * map block 0: i32 count, then 23 descriptors per texture -- two for the
 * texture itself (only the first is used) and 21 for its four and sixteen
 * piece mip levels, which the PS2 does not want.  A descriptor is two words:
 *
 *	a: [7:0] x in the page  [15:8] y  [20:16] clut x/16  [31:22] clut y
 *	b: [7:0] u  [15:8] v  [18:16] page x  [20] page y
 *	   [24:23] format: 0 = 4 bpp, 1 = 8 bpp, 2,3 = 16 bpp direct
 *	   [30:28] rotation  [31] 32x32 rather than 16x16
 *
 * x and the width are in 16 bit VRAM words, so a 4 bpp texture's x has to be
 * divided by four.  This is straight PlayStation texture-page addressing; the
 * only oddity is that the page stride here is 512 words, not VRAM's 1024.
 */

typedef struct TexOut TexOut;
struct TexOut {
	int w, h;
	int bpp;		/* 4, 8 or 16 */
	u8 *pix;		/* w*h indices, or w*h*3 rgb for 16 bpp */
	u8 pal[256*3];
	int npal;
};

/* 5 bits to 8, the way texconv.c did it: scale and truncate, so 31 -> 255 */
static int
c5to8(int v)
{
	return (int)(v * (255.0f/31.0f)) & 0xFF;
}

enum { VRAMSTRIDE = 512 };

static int
readTex(TexOut *t, const u16 *vram, u32 vramwords, u32 a, u32 b)
{
	int texx, texy, palx, paly;
	int sx, sy, fmt, sz, bpp, npx, nx;
	int x, y, i;
	u32 palbase, pixbase;

	memset(t, 0, sizeof(*t));

	texx = a & 0xFF;
	texy = (a>>8) & 0xFF;
	palx = (a>>16) & 0x1F;
	paly = (a>>22) & 0x3FF;

	sx = (b>>16) & 0x7;
	sy = (b>>20) & 0x1;
	fmt = (b>>23) & 3;
	sz = (b>>31) & 1 ? 32 : 16;

	bpp = fmt > 1 ? 16 : fmt == 0 ? 4 : 8;
	npx = 16/bpp;

	texx /= npx;
	texx += sx*64;
	texy += sy*256;

	palbase = (u32)paly*VRAMSTRIDE + (u32)palx*16;
	pixbase = (u32)texy*VRAMSTRIDE + (u32)texx;

	t->w = t->h = sz;
	t->bpp = bpp;
	nx = sz/npx;

	if(pixbase + (u32)(sz-1)*VRAMSTRIDE + (u32)nx > vramwords)
		return -1;

	if(bpp == 16) {
		t->pix = emalloc((size_t)sz*sz*3);
		for(y = 0; y < sz; y++)
			for(x = 0; x < sz; x++) {
				u16 c = vram[pixbase + (u32)y*VRAMSTRIDE + x];
				u8 *p = &t->pix[(y*sz + x)*3];
				p[0] = c5to8(c & 0x1F);
				p[1] = c5to8((c>>5) & 0x1F);
				p[2] = c5to8((c>>10) & 0x1F);
			}
		return 0;
	}

	t->npal = 1<<bpp;
	if(palbase + (u32)t->npal > vramwords)
		return -1;
	for(i = 0; i < t->npal; i++) {
		u16 c = vram[palbase + i];
		t->pal[i*3+0] = c5to8(c & 0x1F);
		t->pal[i*3+1] = c5to8((c>>5) & 0x1F);
		t->pal[i*3+2] = c5to8((c>>10) & 0x1F);
	}

	t->pix = emalloc((size_t)sz*sz);
	for(y = 0; y < sz; y++)
		for(x = 0; x < nx; x++) {
			u16 w = vram[pixbase + (u32)y*VRAMSTRIDE + x];
			for(i = 0; i < npx; i++)
				t->pix[y*sz + x*npx + i] = (w >> (bpp*i)) & (t->npal-1);
		}
	return 0;
}

static int
writeTexPNG(const char *path, TexOut *t)
{
	LodePNGState state;
	u8 *png = nil, *raw = t->pix, *packed = nil;
	size_t pngsize = 0;
	unsigned err;
	int i, bitdepth;

	lodepng_state_init(&state);
	if(t->bpp == 16) {
		state.info_raw.colortype = LCT_RGB;
		state.info_raw.bitdepth = 8;
		state.info_png.color.colortype = LCT_RGB;
		state.info_png.color.bitdepth = 8;
	} else {
		/* a 16 entry palette stays 4 bit in the file, which is what
		 * the GS wants anyway: xtcTextureReadPNG turns it into PSMT4.
		 * Raw and file format are kept identical so lodepng copies the
		 * indices through rather than going round by RGBA -- a PS1
		 * palette often has the same colour twice, and a round trip
		 * would quietly renumber those. */
		bitdepth = t->npal <= 16 ? 4 : 8;
		state.info_raw.colortype = LCT_PALETTE;
		state.info_raw.bitdepth = bitdepth;
		state.info_png.color.colortype = LCT_PALETTE;
		state.info_png.color.bitdepth = bitdepth;
		for(i = 0; i < t->npal; i++) {
			lodepng_palette_add(&state.info_raw,
				t->pal[i*3+0], t->pal[i*3+1], t->pal[i*3+2], 255);
			lodepng_palette_add(&state.info_png.color,
				t->pal[i*3+0], t->pal[i*3+1], t->pal[i*3+2], 255);
		}
		if(bitdepth == 4) {
			int n = t->w*t->h;
			packed = emalloc((size_t)(n+1)/2);
			for(i = 0; i < n; i++)
				packed[i/2] |= (u8)((t->pix[i] & 0xF) << (i&1 ? 0 : 4));
			raw = packed;
		}
	}
	state.encoder.auto_convert = 0;

	err = lodepng_encode(&png, &pngsize, raw, (unsigned)t->w, (unsigned)t->h, &state);
	if(err == 0)
		err = lodepng_save_file(png, pngsize, path);
	lodepng_state_cleanup(&state);
	free(png);
	free(packed);
	if(err) {
		fprintf(stderr, "spyroconv: %s: %s\n", path, lodepng_error_text(err));
		return -1;
	}
	return 0;
}

static int
convTextures(const char *dir, Blob desc, Blob page, int *ntexout, int *n16out)
{
	Rd r = rdinit(desc.d, desc.len);
	const u16 *vram = (const u16*)page.d;
	u32 vramwords = page.len/2;
	i32 ntex;
	int i, n16 = 0, bad = 0;

	ntex = rdi32(&r);
	if(r.err || ntex <= 0 || (u32)ntex > desc.len/8)
		return -1;
	/* the descriptors we need are the even ones of the first 2*ntex */
	if((u32)ntex*16 > desc.len - 4)
		return -1;

	for(i = 0; i < ntex; i++) {
		TexOut t;
		char path[1024];
		u32 a, b;

		rdseek(&r, 4 + (u32)i*16);
		a = rdu32(&r);
		b = rdu32(&r);
		if(r.err)
			return -1;
		if(readTex(&t, vram, vramwords, a, b) < 0) {
			if(verbose)
				fprintf(stderr, "spyroconv: texture %d out of the page\n", i);
			bad++;
			continue;
		}
		if(t.bpp == 16)
			n16++;
		snprintf(path, sizeof(path), "%s/tex_%03d.png", dir, i);
		if(writeTexPNG(path, &t) < 0)
			bad++;
		free(t.pix);
	}
	if(ntexout) *ntexout = ntex;
	if(n16out) *n16out = n16;
	return bad ? -2 : 0;
}

/* ---- driving ------------------------------------------------------------- */

/* read one entry of the top level archive without pulling the whole WAD in */
static u8*
readWadEntry(FILE *f, int i, u32 *size)
{
	u8 hdr[8];
	u32 off, len;
	u8 *d;

	if(fseek(f, (long)i*8, SEEK_SET) != 0)
		return nil;
	if(fread(hdr, 1, 8, f) != 8)
		return nil;
	off = (u32)hdr[0] | (u32)hdr[1]<<8 | (u32)hdr[2]<<16 | (u32)hdr[3]<<24;
	len = (u32)hdr[4] | (u32)hdr[5]<<8 | (u32)hdr[6]<<16 | (u32)hdr[7]<<24;
	if(len == 0)
		return nil;
	if(fseek(f, (long)off, SEEK_SET) != 0)
		return nil;
	d = emalloc(len);
	if(fread(d, 1, len, f) != len) {
		free(d);
		return nil;
	}
	*size = len;
	return d;
}

static int
countWadEntries(FILE *f)
{
	u8 hdr[8];
	u32 first = 0xFFFFFFFF;
	int n;

	for(n = 0;; n++) {
		u32 off, len;
		if(fseek(f, (long)n*8, SEEK_SET) != 0)
			break;
		if(fread(hdr, 1, 8, f) != 8)
			break;
		off = (u32)hdr[0] | (u32)hdr[1]<<8 | (u32)hdr[2]<<16 | (u32)hdr[3]<<24;
		len = (u32)hdr[4] | (u32)hdr[5]<<8 | (u32)hdr[6]<<16 | (u32)hdr[7]<<24;
		if(len == 0)
			break;
		if(n == 0)
			first = off;
		if((u32)(n+1)*8 > first)
			break;
	}
	return n;
}

static int
makedir(const char *path)
{
	if(mkdir(path, 0777) == 0 || errno == EEXIST)
		return 0;
	fprintf(stderr, "spyroconv: mkdir %s: %s\n", path, strerror(errno));
	return -1;
}

/* level N is WAD file 10 + 2*N: the levels sit at the even indices from 10 */
enum { FIRSTLEVEL = 10, LEVELSTEP = 2 };

static int
writeModel(const char *path, XModel *m)
{
	FILE *f = fopen(path, "wb");
	if(f == nil) {
		fprintf(stderr, "spyroconv: %s: %s\n", path, strerror(errno));
		return -1;
	}
	if(merge)
		mergeModel(m);
	writeXm(f, m);
	fclose(f);
	return 0;
}

static int
convLevel(FILE *wad, int fileno, const char *outdir, const char *name)
{
	u8 *lvl;
	u32 lvlsize;
	Blob container, page, maps, blocks[NMAPBLOCK];
	int nblocks, nsub, rc = 0;
	XModel world, sky;
	char path[1024];
	int nskipped = 0, ntex = 0, n16 = 0;
	u32 bgcolor = 0;

	lvl = readWadEntry(wad, fileno, &lvlsize);
	if(lvl == nil) {
		fprintf(stderr, "spyroconv: file %d: cannot read\n", fileno);
		return -1;
	}
	container.d = lvl;
	container.len = lvlsize;
	nsub = tocCount(container);
	if(verbose)
		printf("  file %d: %u bytes, %d sub files\n", fileno, lvlsize, nsub);
	if(nsub < 2) {
		fprintf(stderr, "spyroconv: file %d: not a level container (%d sub files)\n",
			fileno, nsub);
		free(lvl);
		return -1;
	}
	if(!tocEntry(container, 0, &page) || !tocEntry(container, 1, &maps)) {
		fprintf(stderr, "spyroconv: file %d: bad sub file table\n", fileno);
		free(lvl);
		return -1;
	}
	nblocks = mapBlocks(maps, blocks);
	if(verbose) {
		int i;
		printf("    page %u bytes, maps %u bytes, %d blocks:", page.len, maps.len, nblocks);
		for(i = 0; i < nblocks; i++)
			printf(" %u", blocks[i].len);
		printf("\n");
	}
	if(nblocks < NMAPBLOCK) {
		fprintf(stderr, "spyroconv: file %d: only %d map blocks\n", fileno, nblocks);
		free(lvl);
		return -1;
	}

	memset(&world, 0, sizeof(world));
	memset(&sky, 0, sizeof(sky));

	if(convWorld(&world, blocks[1], &nskipped) < 0) {
		fprintf(stderr, "spyroconv: file %d: world does not parse\n", fileno);
		rc = -1;
	}
	if(convSky(&sky, blocks[5], &bgcolor) < 0) {
		fprintf(stderr, "spyroconv: file %d: sky does not parse\n", fileno);
		rc = -1;
	}

	if(world.nmeshes) {
		if(name)
			snprintf(path, sizeof(path), "%s/%s.xm", outdir, name);
		else
			snprintf(path, sizeof(path), "%s/world.xm", outdir);
		if(writeModel(path, &world) < 0)
			rc = -1;
	}
	/* what the level knows beyond its two models: the colour the game
	 * clears to, which the sky leaves showing straight up */
	{
		FILE *f;
		snprintf(path, sizeof(path), "%s/level.txt", outdir);
		f = fopen(path, "wb");
		if(f) {
			fprintf(f, "bgcolor %d %d %d\n", bgcolor & 0xFF, (bgcolor>>8) & 0xFF, (bgcolor>>16) & 0xFF);
			fclose(f);
		}
	}
	if(sky.nmeshes) {
		if(name)
			snprintf(path, sizeof(path), "%s/%s_sky.xm", outdir, name);
		else
			snprintf(path, sizeof(path), "%s/sky.xm", outdir);
		if(writeModel(path, &sky) < 0)
			rc = -1;
	}

	switch(convTextures(outdir, blocks[0], page, &ntex, &n16)) {
	case -1:
		fprintf(stderr, "spyroconv: file %d: texture list does not parse\n", fileno);
		rc = -1;
		break;
	case -2:
		fprintf(stderr, "spyroconv: file %d: some textures did not convert\n", fileno);
		rc = -1;
		break;
	}

	if(!quiet)
		printf("file %2d: world %d nodes %d meshes (%d empty sectors), "
			"sky %d nodes %d meshes, %d textures%s\n",
			fileno, world.nnodes, world.nmeshes, nskipped,
			sky.nnodes, sky.nmeshes, ntex,
			n16 ? " (some 16 bpp)" : "");

	freeModel(&world);
	freeModel(&sky);
	free(lvl);
	return rc;
}

static void
usage(void)
{
	fprintf(stderr,
"usage: spyroconv [-l N] [-f N] [-n NAME] [-o DIR] [-a] [-m] [-v] [-q] WAD\n"
"	-l N	level index, 0..34 (default 0)\n"
"	-f N	pick by raw WAD file index (10, 12, ... 78) instead\n"
"	-n NAME	write NAME.xm and NAME_sky.xm, not world.xm and sky.xm\n"
"	-o DIR	output directory (default .); with -a, DIR/levelNN/\n"
"	-a	convert every level\n"
"	-m	merge: one mesh per texture for the whole level, no sectors\n"
"	-v	print the layout\n"
"	-q	no summary\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	const char *outdir = ".";
	const char *name = nil;
	const char *wadpath = nil;
	int level = 0, fileno = -1, all = 0;
	int i, rc = 0;
	FILE *wad;
	int nentries;

	for(i = 1; i < argc; i++) {
		if(strcmp(argv[i], "-l") == 0 && i+1 < argc)
			level = atoi(argv[++i]);
		else if(strcmp(argv[i], "-f") == 0 && i+1 < argc)
			fileno = atoi(argv[++i]);
		else if(strcmp(argv[i], "-n") == 0 && i+1 < argc)
			name = argv[++i];
		else if(strcmp(argv[i], "-o") == 0 && i+1 < argc)
			outdir = argv[++i];
		else if(strcmp(argv[i], "-m") == 0)
			merge = 1;
		else if(strcmp(argv[i], "-a") == 0)
			all = 1;
		else if(strcmp(argv[i], "-v") == 0)
			verbose = 1;
		else if(strcmp(argv[i], "-q") == 0)
			quiet = 1;
		else if(argv[i][0] == '-')
			usage();
		else if(wadpath == nil)
			wadpath = argv[i];
		else
			usage();
	}
	if(wadpath == nil)
		usage();

	wad = fopen(wadpath, "rb");
	if(wad == nil) {
		fprintf(stderr, "spyroconv: %s: %s\n", wadpath, strerror(errno));
		return 1;
	}
	nentries = countWadEntries(wad);
	if(nentries <= FIRSTLEVEL) {
		fprintf(stderr, "spyroconv: %s: %d entries, not a Spyro WAD\n",
			wadpath, nentries);
		fclose(wad);
		return 1;
	}
	if(verbose)
		printf("%s: %d files\n", wadpath, nentries);

	if(makedir(outdir) < 0) {
		fclose(wad);
		return 1;
	}

	if(all) {
		int n = 0, ok = 0;
		for(i = FIRSTLEVEL; i < nentries; i += LEVELSTEP) {
			char dir[1024];
			snprintf(dir, sizeof(dir), "%s/level%02d", outdir, n);
			if(makedir(dir) < 0) { rc = 1; n++; continue; }
			if(convLevel(wad, i, dir, name) == 0)
				ok++;
			else
				rc = 1;
			n++;
		}
		if(!quiet)
			printf("%d of %d levels converted cleanly\n", ok, n);
	} else {
		if(fileno < 0)
			fileno = FIRSTLEVEL + level*LEVELSTEP;
		if(fileno >= nentries) {
			fprintf(stderr, "spyroconv: no file %d in %s\n", fileno, wadpath);
			fclose(wad);
			return 1;
		}
		if(convLevel(wad, fileno, outdir, name) < 0)
			rc = 1;
	}

	fclose(wad);
	return rc;
}
