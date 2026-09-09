/*
 * Platform independent model, skeleton and animation data.
 * Builds on the shared xtc.h.
 */

#ifndef XMODEL_H
#define XMODEL_H

#include "xtc.h"
#include <stdio.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// zeroed, asserts on failure
void *emalloc(size_t sz);
// whole file into malloc'd memory, 0 on failure.  the backend's: stdio
// on the PC, fio on the PS2 (src/xfile.c)
int readfile(const char *path, uint8 **data, uint32 *size);

extern const char *texpath;

// this sort of mirrors assimp structs
STRUCT(xTexture) {
	char *name;
	xtcTexture *tex;
};

// one pass: the std material, which of its terms the vertex colour
// replaces, and the texture
STRUCT(xMaterial) {
	xtcStdMaterial material;
	uint32 colorMaterial;
	xTexture *tex;
};

STRUCT(xVertex) {
	float vtx[3];
	float nrm[3];
	float tex[2];
	uint8 col[4];
};

// platform-independent data
STRUCT(xGeometry) {
	int numVertices;
	int numIndices;
	xVertex *vertices;
	int *indices;
};

STRUCT(xSkin) {
	int numBones;
	Mat4 *invMatrices;	// [numBones]
	uint8 *indices;		// [4*numVertices]
	float *weights;		// [4*numVertices]
};
xSkin *allocXSkin(int nbones, int nvertices);

STRUCT(xMesh) {
	xtcPrimList *prims;
	xMaterial *material;
	xGeometry *geo;
	xSkin *skin;
};

typedef struct xNode xNode;

STRUCT(xBone) {
	int flag;	// for topology
	int tag;	// for custom use
	xNode *node;
};

/*
 * Bones are stored in depth-first order with RenderWare-style
 * push/pop flags so the tree can be walked with a small stack.
 * matrices[] are in model space (relative to the xModel root);
 * before xSkeletonUpdateMatrices they hold the bones' local matrices.
 */
STRUCT(xSkeleton) {
	int numBones;
	xBone *bones;
	Mat4 *matrices;
};
xSkeleton *allocXSkeleton(int nbones);
void xSkeletonResetMatrices(xSkeleton *s);
void xSkeletonUpdateMatrices(xSkeleton *s);
int findBone(xSkeleton *skel, const char *str);

struct xNode
{
	Mat4 localMatrix;
	char *name;
	xNode *parent;
	xNode *child;
	xNode *next;

	xMesh **meshes;
	int numMeshes;
	bool hidden;

	// set if this node is the root
	// of the skeleton
	xSkeleton *skel;
	// TODO: or ID?
	int tag;	// to connect this node to a bone
};
xNode *allocXNode(void);
xNode *findXNode(xNode *root, const char *name);

STRUCT(xModel) {
	int numMeshes;
	int numMaterials;
	xMesh **meshes;
	xMaterial **materials;
	xNode *root;

	// This holds the animation state for a skeleton.
	// Should be gotten from one of the nodes.
	// Used to render skinned meshes
	xSkeleton *skel;
};


void allocXGeo(xMesh *m, int numVerts, int numIndices);
void writeXModel(FILE *f, xModel *mdl);
void writeXModelChunk(FILE *f, xModel *mdl);
xModel *loadXModel(const char *path);
xModel *loadXModelChunk(FILE *f);
void buildXModel(xModel *mdl);
// bind pose bounding sphere in model space
void xModelBoundingSphere(xModel *mdl, Vec3 *center, float *radius);



/*
 * Animation.
 * Each channel animates one node (by name; id is the bone index in the
 * model's skeleton, or -1 for a plain node) with independent key tracks
 * for rotation, translation and scale. Times are in seconds.
 */

STRUCT(xRotKey) {
	float time;
	Quat rot;
};

STRUCT(xVecKey) {
	float time;
	Vec3 v;
};

STRUCT(xAnimChannel) {
	char *name;
	int id;

	int numRotKeys;
	int numTransKeys;
	int numScaleKeys;
	xRotKey *rotKeys;
	xVecKey *transKeys;
	xVecKey *scaleKeys;
};

STRUCT(xAnimation) {
	char *name;
	float duration;
	int numChannels;
	xAnimChannel *channels;
};

STRUCT(xAnimList) {
	int numAnims;
	xAnimation *anims;
};
xAnimList *allocXAnimList(int nanims);
void writeXAnimList(FILE *f, xAnimList *anims);
void writeXAnimListChunk(FILE *f, xAnimList *alist);
xAnimList *loadXAnimList(const char *path);
xAnimList *loadXAnimListChunk(FILE *f);

STRUCT(xAnimPlayer) {
	float time;
	xAnimation *anim;
	xModel *model;
	// per channel: the matrix the channel writes.
	// bones: the skeleton's local matrix, other nodes: the node's local matrix
	Mat4 **targets;
};
xAnimPlayer *xAnimPlayerCreate(xModel *mdl);
void xAnimPlayerSetAnim(xAnimPlayer *p, xAnimation *a);
void xAnimPlayerAddTime(xAnimPlayer *p, float t);
void xAnimPlayerApply(xAnimPlayer *p);

enum {
	Dbg_DrawNodes = 1,
	Dbg_DrawSkeleton = 2,
	Dbg_DrawWire = 4,
	Dbg_DrawPoints = 8,
};
void xModelDraw(xModel *m, int flags);
// grey, unlit by vertex colours: what meshes get when the file says nothing
xtcStdMaterial DefaultMaterial(void);
// if set, draws unskinned meshes instead of defaultPipeline (stopgap)
extern xtcPipeline *xDrawPipeline;

#ifdef __cplusplus
}	/* extern "C" */
#endif

#endif
