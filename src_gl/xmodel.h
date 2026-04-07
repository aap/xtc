#pragma once

#include "xtc.h"
#include <stdio.h>

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
	u8 col[4];
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
	mat4 *invMatrices;	// [numBones]
	u8 *indices;		// [4*numVertices]
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

struct xSkeleton
{
	int numBones;
	xBone *bones;
	mat4 *matrices;
};
xSkeleton *allocXSkeleton(int nbones);

struct xNode
{
	mat4 localMatrix;
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


typedef struct aiScene aiScene;
void dumpAssimpScene(FILE *file, const aiScene *scene);

void allocXGeo(xMesh *m, int numVerts, int numIndices);
void writeXModel(FILE *f, xModel *mdl);
void writeXModelChunk(FILE *f, xModel *mdl);
xModel *loadXModel(FILE *file);
xModel *loadXModelChunk(FILE *f);
void buildXModel(xModel *mdl);




struct xAnimChannel
{
	// bone
	char *name;
	int id;

	int typemask;
	int numKeys;
	struct Key {
		glm::quat rot;
		vec3 trans;
		vec3 scale;
		float time;
	};
	Key *keys;
};

struct xAnimation
{
	char *name;
	float duration;
	float timescale;
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
	xSkeleton *skel;
	// map from channel index to matrix
	mat4 **matrices;
};

enum {
	Dbg_DrawNodes = 1,
	Dbg_DrawSkeleton = 2,
};
void xModelDraw(xModel *m, int flags);