#include "xtc.h"
#include "xmodel.h"

#include <assimp/scene.h>

xtcMaterial DefaultMaterial(void);

void
convertMatrix(mat4 &m, const aiMatrix4x4 &mtx)
{
	m[0][0] = mtx.a1;
	m[0][1] = mtx.b1;
	m[0][2] = mtx.c1;
	m[0][3] = 0.0f;
	m[1][0] = mtx.a2;
	m[1][1] = mtx.b2;
	m[1][2] = mtx.c2;
	m[1][3] = 0.0f;
	m[2][0] = mtx.a3;
	m[2][1] = mtx.b3;
	m[2][2] = mtx.c3;
	m[2][3] = 0.0f;
	m[3][0] = mtx.a4;
	m[3][1] = mtx.b4;
	m[3][2] = mtx.c4;
	m[3][3] = 1.0f;
}

int gentag = 1000;

struct HierEntry
{
	xNode *xn;
	aiNode *an;
	aiBone *ab;
	int boneIdx;
	int tag;
};

int
nodeTreeSize(xNode *node)
{
	int n = 1;
	for(xNode *c = node->child; c; c = c->next)
		n += nodeTreeSize(c);
	return n;
}


int
nodeIdx(HierEntry *hier, const char *str)
{
	for(int i = 0; hier[i].xn; i++)
		if(strcmp(hier[i].xn->name, str) == 0)
			return i;
	return -1;
}

void
matchBones(aiMesh *m, HierEntry *hier)
{
	for(u32 i = 0; i < m->mNumBones; i++) {
		aiBone *ab = m->mBones[i];
		int idx = nodeIdx(hier, ab->mName.C_Str());
		assert(idx >= 0);
		hier[idx].ab = ab;
	}
}

void
pushweight(float w, u8 i, float *ws, u8 *is)
{
	for(int j = 0; j < 4; j++) {
		if(w > ws[j]) {
			memmove(&ws[j+1], &ws[j], (3-j)*sizeof(float));
			memmove(&is[j+1], &is[j], (3-j)*sizeof(u8));
			ws[j] = w;
			is[j] = i;
			return;
		}
	}
}

// debug
void
pdepth(xNode *n)
{
	if(n == nil) return;
	if(n->parent) {
		printf("  ");
		pdepth(n->parent);
	}
}
void indent(int n) { while(n--) printf("  "); }


void
dumpHierarchy(HierEntry *hier)
{
	for(int i = 0; hier[i].xn; i++) {
		pdepth(hier[i].xn);
		if(hier[i].ab)
			printf("bone %d %d %s\n", hier[i].boneIdx, hier[i].tag, hier[i].xn->name);
		else
			printf("---- %d %s\n", hier[i].tag, hier[i].xn->name);
	}
}

void
dumpXSkeleton(xSkeleton *s)
{
	int stack[64];
	int sp = 0;
	int parent = -1;
	int depth = 0;
	for(int i = 0; i < s->numBones; i++) {
		xBone *b = &s->bones[i];

		indent(depth);
		printf("%d %d %d %d %s\n", i, parent, b->tag, b->flag, b->node->name);

		if(b->flag & 1) {
			stack[sp++] = parent;
			stack[sp++] = depth;
		}
		parent = i;
		depth++;
		if(b->flag & 2) {
			depth = stack[--sp];
			parent = stack[--sp];
		}
	}
}

void
convertAssimpSkin(xMesh *xm, aiMesh *m, HierEntry *hier)
{
	matchBones(m, hier);

	// TODO: this is bad
	// bones are purely referenced by name, however
	// some child nodes may not be part of the hierarchy
	// so initially we create the skin based on the node hierarchy
	// once we've seen all skins we create the skeletons and adjust
	int nbones = nodeTreeSize(hier[0].xn);
	xm->skin = allocXSkin(nbones, xm->geo->numVertices);
	xSkin *s = xm->skin;

	for(u32 i = 0; i < m->mNumBones; i++) {
		aiBone *b = m->mBones[i];
		// NB: at this point indices are still into the node table
		// this is fixed by fixSkin below once we have a proper skeleton
		int idx = nodeIdx(hier, b->mName.C_Str());
		assert(idx < s->numBones);
		for(u32 j = 0; j < b->mNumWeights; j++) {
			float w = b->mWeights[j].mWeight;
			int vx = b->mWeights[j].mVertexId;
			pushweight(w, idx, s->weights + vx*4, s->indices + vx*4);
		}
		convertMatrix(s->invMatrices[idx], b->mOffsetMatrix);
	}
}

// change node-based to bone-based
void
fixSkin(xMesh *m, xSkeleton *skel, HierEntry *hier)
{
	xSkin *s = m->skin;
	if(s == nil)
		return;

	// remap indices
	for(int i = 0; i < 4*m->geo->numVertices; i++)
		if(s->weights[i] > 0.0f)
			s->indices[i] = hier[s->indices[i]].boneIdx;
	// reshuffle matrices
	u32 matsz = skel->numBones * sizeof(mat4);
	mat4 *tmp = (mat4*)malloc(matsz);
	for(int i = 0; i < s->numBones; i++)
		if(hier[i].boneIdx >= 0)
			tmp[hier[i].boneIdx] = s->invMatrices[i];
	memcpy(s->invMatrices, tmp, matsz);
	free(tmp);
	s->numBones = skel->numBones;
}

xMesh*
convertAssimpMesh(aiMesh *m, HierEntry *hier)
{
	xMesh *xm;
	xGeometry *g;
	xVertex *vx;
	int j;

	xm = (xMesh*)malloc(sizeof(xMesh));
	xm->prims = nil;
	xm->geo = nil;
	xm->skin = nil;

	allocXGeo(xm, m->mNumVertices, m->mNumFaces*3);
	g = xm->geo;

	aiVector3D *v = m->mVertices;
	aiVector3D *nm = m->mNormals;
	aiVector3D *uv = m->mTextureCoords[0];
	aiColor4D *col = m->mColors[0];

	vx = g->vertices;
	for(u32 i = 0; i < m->mNumVertices; i++) {
		vx->vtx[0] = v[i].x;
		vx->vtx[1] = v[i].y;
		vx->vtx[2] = v[i].z;
		if(nm) {
			vx->nrm[0] = nm[i].x;
			vx->nrm[1] = nm[i].y;
			vx->nrm[2] = nm[i].z;
		}
		if(uv) {
			vx->tex[0] = uv[i].x;
			vx->tex[1] = uv[i].y;
		}
		if(col) {
			vx->col[0] = col[i].r*255.0f;
			vx->col[1] = col[i].g*255.0f;
			vx->col[2] = col[i].b*255.0f;
			vx->col[3] = col[i].a*255.0f;
		}
		vx++;
	}

	j = 0;
	for(u32 i = 0; i < m->mNumFaces; i++) {
		aiFace *f = &m->mFaces[i];
		assert(f->mNumIndices == 3);
		g->indices[j++] = f->mIndices[0];
		g->indices[j++] = f->mIndices[1];
		g->indices[j++] = f->mIndices[2];
	}

	if(m->mBones)
		convertAssimpSkin(xm, m, hier);

	return xm;
}

xTexture *readTexture(char *name);

xMaterial*
convertAssimpMaterial(aiMaterial *m)
{
	xMaterial *xmat;

	xmat = (xMaterial*)malloc(sizeof(xMaterial));
	xmat->tex = nil;
	xmat->material = DefaultMaterial();

	aiColor4D color;
	m->Get(AI_MATKEY_COLOR_AMBIENT, color);
	xmat->material.ambient = vec4(color.r, color.g, color.b, color.a);
	m->Get(AI_MATKEY_COLOR_DIFFUSE, color);
	xmat->material.diffuse = vec4(color.r, color.g, color.b, color.a);
	m->Get(AI_MATKEY_COLOR_SPECULAR, color);
	xmat->material.specular = vec4(color.r, color.g, color.b, color.a);
	m->Get(AI_MATKEY_COLOR_EMISSIVE, color);
	xmat->material.emissive = vec4(color.r, color.g, color.b, color.a);
	m->Get(AI_MATKEY_SHININESS, xmat->material.shininess);
	// use vertex color for emissive
	xmat->material.colorSelector = vec4(0.0f, 0.0f, 0.0f, 1.0f);
//xmat->material.ambient = vec4(1.0f, 1.0f, 1.0f, 1.0f);
xmat->material.shininess = 0;

	aiString path;
	char *p;
	for(u32 j = 0; j < m->GetTextureCount(aiTextureType_DIFFUSE); j++) {
		m->GetTexture(aiTextureType_DIFFUSE, j, &path);
//		printf("tex %s\n", path.C_Str());
		p = strrchr((char*)path.C_Str(), '\\');
		if(p == nil) p = strrchr((char*)path.C_Str(), '/');
		if(p)
			xmat->tex = readTexture(p+1);
		else
			xmat->tex = readTexture((char*)path.C_Str());
	}

	return xmat;
}

/*
	return traverse-order table of nodes:
	THIS defines bone IDs
	{ xNode, aiNode, flag? }

	create and assign meshes later
 */

xNode*
buildHierarchy(aiNode *root, HierEntry **hp)
{
	xNode *node = allocXNode();
	node->name = strdup(root->mName.C_Str());
	node->parent = nil;
	node->child = nil;
	node->next = nil;

	node->numMeshes = 0;
	node->meshes = nil;

	node->skel = nil;
	node->tag = -1;

	(*hp)->xn = node;
	(*hp)->an = root;
	(*hp)->ab = nil;
	(*hp)->boneIdx = -1;
	(*hp)->tag = -1;
	(*hp)++;

	xNode **pchild = &node->child;
	for(u32 i = 0; i < root->mNumChildren; i++) {
		*pchild = buildHierarchy(root->mChildren[i], hp);
		(*pchild)->parent = node;
		pchild = &(*pchild)->next;
	}
	convertMatrix(node->localMatrix, root->mTransformation);

	return node;
}

int
countNodes(aiNode *node)
{
	int n = 1;
	for(u32 i = 0; i < node->mNumChildren; i++)
		n += countNodes(node->mChildren[i]);
	return n;
}

xSkeleton*
createSkeleton(xModel *mdl, HierEntry *hier)
{
	// find bone and assign indices and tags
	int numBones = 0;
	int rootIdx = -1;
	for(int i = 0; hier[i].xn; i++) {
		if(hier[i].ab) {
			hier[i].boneIdx = numBones++;
			if(hier[i].tag < 0) hier[i].tag = gentag++;
			hier[i].xn->tag = hier[i].tag;
			if(rootIdx < 0)
				rootIdx = i;
		}
	}
	if(numBones == 0)
		return nil;

	xSkeleton *skel = allocXSkeleton(numBones);

//dumpHierarchy(hier);
//printf("%d bones. root at %d\n", numBones, rootIdx);

	hier += rootIdx;
	hier->xn->skel = skel;
	mdl->skel = skel;

	for(int i = 0; i < skel->numBones; i++) {
		xBone *b = &skel->bones[i];
		b->flag = 0;
		while(hier[0].xn->tag < 0) hier++;
		b->node = (hier++)->xn;
		b->tag = b->node->tag;
		if(b->node->next && b->node->next->tag >= 0)
			b->flag |= 1;	// push
		if(b->node->child==nil || b->node->child->tag < 0)
			b->flag |= 2;	// pop
	}
	skel->bones[0].flag &= ~1;	// root cannot have children
//dumpXSkeleton(skel);

	int stack[64];
	int sp = 0;

	mat4 *m = skel->matrices;
	memset(m, 0, skel->numBones*sizeof(mat4));
// TODO: hmm??
//	m[0] = mat4(1.0f);
	m[0] = skel->bones->node->localMatrix;
	int parent = 0;
	for(int i = 1; i < skel->numBones; i++) {
		xBone *b = &skel->bones[i];
		m[i] = m[parent] * b->node->localMatrix;
		if(b->flag & 1) stack[sp++] = parent;
		parent = i;
		if(b->flag & 2) parent = stack[--sp];
	}

	return skel;
}

xModel*
convertAssimpScene(const aiScene *scene)
{
	xModel *mdl;

	mdl = (xModel*)malloc(sizeof(xModel));

	mdl->numMeshes = scene->mNumMeshes;
	mdl->numMaterials = scene->mNumMaterials;
	mdl->meshes = (xMesh**)malloc(mdl->numMeshes*sizeof(xMesh*));
	mdl->materials = (xMaterial**)malloc(mdl->numMaterials*sizeof(xMaterial*));
	mdl->skel = nil;


	int numNodes = countNodes(scene->mRootNode);
	HierEntry *hier = (HierEntry*)malloc(sizeof(HierEntry) * (numNodes+1));
	HierEntry *hp = hier;
	mdl->root = buildHierarchy(scene->mRootNode, &hp);
	hier[numNodes].xn = nil;
	hier[numNodes].an = nil;


	for(u32 i = 0; i < scene->mNumMaterials; i++)
		mdl->materials[i] = convertAssimpMaterial(scene->mMaterials[i]);
	gentag = 1000;
	for(u32 i = 0; i < scene->mNumMeshes; i++) {
		mdl->meshes[i] = convertAssimpMesh(scene->mMeshes[i], hier);
		mdl->meshes[i]->material = mdl->materials[scene->mMeshes[i]->mMaterialIndex];
	}
	for(int i = 0; i < numNodes; i++) {
		xNode *xn = hier[i].xn;
		aiNode *an = hier[i].an;
		xn->numMeshes = an->mNumMeshes;
		xn->meshes = (xMesh**)malloc(xn->numMeshes*sizeof(xMesh*));
		for(u32 i = 0; i < an->mNumMeshes; i++)
			xn->meshes[i] = mdl->meshes[an->mMeshes[i]];
	}

	if(createSkeleton(mdl, hier)) {
		for(u32 i = 0; i < scene->mNumMeshes; i++)
			fixSkin(mdl->meshes[i], mdl->skel, hier);
	}

	return mdl;
}





int
findBone(xSkeleton *skel, const char *str)
{
	for(int i = 0; i < skel->numBones; i++) {
		xBone *b = &skel->bones[i];
		if(strcmp(b->node->name, str) == 0)
			return i;
	}
	return -1;
}

xAnimList*
convertAssimpAnimations(const aiScene *scene, xModel *mdl)
{
	if(scene->mNumAnimations == 0)
		return nil;

	xAnimList *al = allocXAnimList(scene->mNumAnimations);
	for(u32 i = 0; i < scene->mNumAnimations; i++) {
		aiAnimation *a = scene->mAnimations[i];
		xAnimation *xa = &al->anims[i];
		xa->name = strdup(a->mName.C_Str());
		xa->duration = a->mDuration;
		xa->timescale = a->mTicksPerSecond;
		xa->numChannels = a->mNumChannels;
		xa->channels = (xAnimChannel*)malloc(xa->numChannels * sizeof(xAnimChannel));
//printf("%s %f %d\n", xa->name, xa->duration, xa->numChannels);
		for(u32 j = 0; j < a->mNumChannels; j++) {
			aiNodeAnim *ch = a->mChannels[j];
			xAnimChannel *xch = &xa->channels[j];
			xch->name = strdup(ch->mNodeName.C_Str());
			xch->id = -1;	// TODO
				//findBone(mdl->skel, xch->name),
			xch->typemask = 0;
			xch->numKeys = ch->mNumPositionKeys;
			if(ch->mNumRotationKeys > (u32)xch->numKeys) xch->numKeys = ch->mNumRotationKeys;
			if(ch->mNumScalingKeys > (u32)xch->numKeys) xch->numKeys = ch->mNumScalingKeys;
			xch->keys = (xAnimChannel::Key*)malloc(xch->numKeys * sizeof(xAnimChannel::Key));
			if(ch->mNumRotationKeys) {
				xch->typemask |= 1;
				assert(ch->mNumRotationKeys == (u32)xch->numKeys);
			}
			if(ch->mNumPositionKeys) {
				xch->typemask |= 2;
				assert(ch->mNumPositionKeys == (u32)xch->numKeys);
			}
			if(ch->mNumScalingKeys) {
				xch->typemask |= 4;
				assert(ch->mNumScalingKeys == (u32)xch->numKeys);
			}
//printf("\t%s %d %d\n", xch->name, xch->id, xch->numKeys);
			for(int k = 0; k < xch->numKeys; k++) {
				xAnimChannel::Key *xkey = &xch->keys[k];
				if(xch->typemask & 1) {
					aiQuatKey *rk = &ch->mRotationKeys[k];
					xkey->time = rk->mTime;
					xkey->rot.w = rk->mValue.w;
					xkey->rot.x = rk->mValue.x;
					xkey->rot.y = rk->mValue.y;
					xkey->rot.z = rk->mValue.z;
				}
				if(xch->typemask & 2) {
					aiVectorKey *pk = &ch->mPositionKeys[k];
					xkey->time = pk->mTime;
					xkey->trans.x = pk->mValue.x;
					xkey->trans.y = pk->mValue.y;
					xkey->trans.z = pk->mValue.z;
				}
				if(xch->typemask & 4) {
					aiVectorKey *sk = &ch->mScalingKeys[k];
					xkey->time = sk->mTime;
					xkey->scale.x = sk->mValue.x;
					xkey->scale.y = sk->mValue.y;
					xkey->scale.z = sk->mValue.z;
				}
			}
		}
	}

	return al;
}
