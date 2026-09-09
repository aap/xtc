/*
 * Platform independent model, skeleton and animation data.
 * Needs the platform's xtc.h on the include path for
 * xtcMaterial, xtcTexture, xtcPrimList, xtcTextureReadPNG
 * and the integer types.
 */

#pragma once

#include "xtc.h"
#include <stdio.h>

// zeroed, asserts on failure
void *emalloc(size_t sz);
// whole file into malloc'd memory, 0 on failure
int readfile(const char *path, uint8 **data, uint32 *size);

extern const char *texpath;

// this sort of mirrors assimp structs
struct xTexture
{
	char *name;
	xtcTexture *tex;
};

struct xMaterial
{
	xtcMaterial material;
	xTexture *tex;
};

struct xVertex
{
	float vtx[3];
	float nrm[3];
	float tex[2];
	uint8 col[4];
};

// platform-independent data
struct xGeometry
{
	int numVertices;
	int numIndices;
	xVertex *vertices;
	int *indices;
};

struct xSkin
{
	int numBones;
	Mat4 *invMatrices;	// [numBones]
	uint8 *indices;		// [4*numVertices]
	float *weights;		// [4*numVertices]
};
xSkin *allocXSkin(int nbones, int nvertices);

struct xMesh
{
	xtcPrimList *prims;
	xMaterial *material;
	xGeometry *geo;
	xSkin *skin;
};

struct xNode;

struct xBone {
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
struct xSkeleton
{
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

struct xModel
{
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
xModel *loadXModel(FILE *file);
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

struct xRotKey
{
	float time;
	Quat rot;
};

struct xVecKey
{
	float time;
	Vec3 v;
};

struct xAnimChannel
{
	char *name;
	int id;

	int numRotKeys;
	int numTransKeys;
	int numScaleKeys;
	xRotKey *rotKeys;
	xVecKey *transKeys;
	xVecKey *scaleKeys;
};

struct xAnimation
{
	char *name;
	float duration;
	int numChannels;
	xAnimChannel *channels;
};

struct xAnimList
{
	int numAnims;
	xAnimation *anims;
};
xAnimList *allocXAnimList(int nanims);
void writeXAnimList(FILE *f, xAnimList *anims);
void writeXAnimListChunk(FILE *f, xAnimList *alist);
xAnimList *loadXAnimList(FILE *file);
xAnimList *loadXAnimListChunk(FILE *f);

struct xAnimPlayer
{
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
// if set, draws unskinned meshes instead of defaultPipeline (stopgap)
extern xtcPipeline *xDrawPipeline;
