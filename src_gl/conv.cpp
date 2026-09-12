/*
 * assimp scene -> xModel / xAnimList
 */

#include <string.h>
#include "xtc.h"
#include "xmodel.h"
#include <assert.h>
#include "conv.h"

#include <assimp/scene.h>

xTexture *readTexture(const char *name);
xTexture *createTexturePNG(const char *name, const u8 *data, u32 size);

static void
convertMatrix(Mat4 &m, const aiMatrix4x4 &mtx)
{
	m.x.x = mtx.a1;
	m.x.y = mtx.b1;
	m.x.z = mtx.c1;
	m.x.w = 0.0f;
	m.y.x = mtx.a2;
	m.y.y = mtx.b2;
	m.y.z = mtx.c2;
	m.y.w = 0.0f;
	m.z.x = mtx.a3;
	m.z.y = mtx.b3;
	m.z.z = mtx.c3;
	m.z.w = 0.0f;
	m.w.x = mtx.a4;
	m.w.y = mtx.b4;
	m.w.z = mtx.c4;
	m.w.w = 1.0f;
}

static int gentag = 1000;

/*
 * Traverse-order table of nodes. Its order defines bone IDs.
 */
struct HierEntry
{
	xNode *xn;
	aiNode *an;
	aiBone *ab;
	int boneIdx;
	int tag;
};

static int
nodeTreeSize(xNode *node)
{
	int n = 1;
	for(xNode *c = node->child; c; c = c->next)
		n += nodeTreeSize(c);
	return n;
}

static int
nodeIdx(HierEntry *hier, const char *str)
{
	for(int i = 0; hier[i].xn; i++)
		if(strcmp(hier[i].xn->name, str) == 0)
			return i;
	return -1;
}

static void
matchBones(aiMesh *m, HierEntry *hier)
{
	for(u32 i = 0; i < m->mNumBones; i++) {
		aiBone *ab = m->mBones[i];
		int idx = nodeIdx(hier, ab->mName.C_Str());
		assert(idx >= 0);
		hier[idx].ab = ab;
	}
}

// keep the four largest weights, sorted
static void
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
static void
pdepth(xNode *n)
{
	if(n == nil) return;
	if(n->parent) {
		printf("  ");
		pdepth(n->parent);
	}
}
static void indent(int n) { while(n--) printf("  "); }

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
	stack[sp++] = -1;	// see xSkeletonUpdateMatrices
	stack[sp++] = 0;
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

static void
convertAssimpSkin(xMesh *xm, aiMesh *m, HierEntry *hier)
{
	matchBones(m, hier);

	// Bones are referenced by name and we don't have the skeleton yet,
	// so initially the skin is indexed by node. Once all skins have been
	// seen createSkeleton knows the bones and fixSkin remaps.
	int nbones = nodeTreeSize(hier[0].xn);
	xm->skin = allocXSkin(nbones, xm->geo->numVertices);
	xSkin *s = xm->skin;

	for(u32 i = 0; i < m->mNumBones; i++) {
		aiBone *b = m->mBones[i];
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
static void
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
	u32 matsz = skel->numBones * sizeof(Mat4);
	Mat4 *tmp = (Mat4*)malloc(matsz);
	for(int i = 0; i < s->numBones; i++)
		if(hier[i].boneIdx >= 0)
			tmp[hier[i].boneIdx] = s->invMatrices[i];
	memcpy(s->invMatrices, tmp, matsz);
	free(tmp);
	s->numBones = skel->numBones;
}

static xMesh*
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
			// assimp's t runs bottom up; ours, like the GS's, glTF's
			// and RenderWare's, has row 0 at the top of the image
			vx->tex[1] = 1.0f - uv[i].y;
		}
		// no vertex colours: stay black, materials use them as emissive
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

static xMaterial*
convertAssimpMaterial(const aiScene *scene, aiMaterial *m)
{
	xMaterial *xmat;

	xmat = (xMaterial*)malloc(sizeof(xMaterial));
	xmat->tex = nil;
	xmat->material = DefaultMaterial();

	aiColor4D color(1.0f, 1.0f, 1.0f, 1.0f);
	if(m->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS)
		xmat->material.diffuse = vec4(color.r, color.g, color.b, color.a);
	// no ambient colour (glTF): light it like diffuse
	if(m->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS)
		xmat->material.ambient = vec4(color.r, color.g, color.b, color.a);
	else
		xmat->material.ambient = xmat->material.diffuse;
	if(m->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS)
		xmat->material.specular = vec4(color.r, color.g, color.b, color.a);
	if(m->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS)
		xmat->material.emissive = vec4(color.r, color.g, color.b, color.a);
	float shininess = 0.0f;
	m->Get(AI_MATKEY_SHININESS, shininess);
	xmat->material.specular.w = shininess;
	// use vertex color for emissive
	xmat->colorMaterial = XTC_EMISSIVE;
	// TODO: no specular yet
	xmat->material.specular.w = 0.0f;

	aiString path;
	const char *p;
	for(u32 j = 0; j < m->GetTextureCount(aiTextureType_DIFFUSE); j++) {
		m->GetTexture(aiTextureType_DIFFUSE, j, &path);
		const aiTexture *emb = scene->GetEmbeddedTexture(path.C_Str());
		if(emb) {
			// only compressed PNG data for now
			if(emb->mHeight == 0 && emb->CheckFormat("png")) {
				const char *name = emb->mFilename.length ? emb->mFilename.C_Str() : path.C_Str();
				xmat->tex = createTexturePNG(name, (u8*)emb->pcData, emb->mWidth);
			} else
				fprintf(stderr, "warning: unsupported embedded texture %s\n", path.C_Str());
			continue;
		}
		p = strrchr(path.C_Str(), '\\');
		if(p == nil) p = strrchr(path.C_Str(), '/');
		if(p)
			xmat->tex = readTexture(p+1);
		else
			xmat->tex = readTexture(path.C_Str());
	}

	return xmat;
}

static xNode*
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

static int
countNodes(aiNode *node)
{
	int n = 1;
	for(u32 i = 0; i < node->mNumChildren; i++)
		n += countNodes(node->mChildren[i]);
	return n;
}

static xSkeleton*
createSkeleton(xModel *mdl, HierEntry *hier)
{
	// find bones and assign indices and tags
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
	skel->bones[0].flag &= ~1;	// root cannot have siblings

	xSkeletonResetMatrices(skel);
	xSkeletonUpdateMatrices(skel);

	return skel;
}

xModel*
convertAssimpScene(const aiScene *scene)
{
	xModel *mdl;

	mdl = (xModel*)malloc(sizeof(xModel));
	memset(mdl, 0, sizeof(xModel));

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
		mdl->materials[i] = convertAssimpMaterial(scene, scene->mMaterials[i]);
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
		for(u32 j = 0; j < an->mNumMeshes; j++)
			xn->meshes[j] = mdl->meshes[an->mMeshes[j]];
	}

	if(createSkeleton(mdl, hier)) {
		for(u32 i = 0; i < scene->mNumMeshes; i++)
			fixSkin(mdl->meshes[i], mdl->skel, hier);
	}

	free(hier);

	return mdl;
}





static xAnimChannel*
convertAssimpChannel(xAnimChannel *xch, aiNodeAnim *ch, xModel *mdl, float tps)
{
	xch->name = strdup(ch->mNodeName.C_Str());
	xch->id = mdl->skel ? findBone(mdl->skel, xch->name) : -1;

	xch->numRotKeys = ch->mNumRotationKeys;
	xch->numTransKeys = ch->mNumPositionKeys;
	xch->numScaleKeys = ch->mNumScalingKeys;
	// empty tracks are nil
	xch->rotKeys = xch->numRotKeys ? (xRotKey*)malloc(xch->numRotKeys * sizeof(xRotKey)) : nil;
	xch->transKeys = xch->numTransKeys ? (xVecKey*)malloc(xch->numTransKeys * sizeof(xVecKey)) : nil;
	xch->scaleKeys = xch->numScaleKeys ? (xVecKey*)malloc(xch->numScaleKeys * sizeof(xVecKey)) : nil;

	for(int k = 0; k < xch->numRotKeys; k++) {
		aiQuatKey *rk = &ch->mRotationKeys[k];
		xRotKey *xk = &xch->rotKeys[k];
		xk->time = rk->mTime / tps;
		xk->rot.w = rk->mValue.w;
		xk->rot.x = rk->mValue.x;
		xk->rot.y = rk->mValue.y;
		xk->rot.z = rk->mValue.z;
	}
	for(int k = 0; k < xch->numTransKeys; k++) {
		aiVectorKey *pk = &ch->mPositionKeys[k];
		xVecKey *xk = &xch->transKeys[k];
		xk->time = pk->mTime / tps;
		xk->v = vec3(pk->mValue.x, pk->mValue.y, pk->mValue.z);
	}
	for(int k = 0; k < xch->numScaleKeys; k++) {
		aiVectorKey *sk = &ch->mScalingKeys[k];
		xVecKey *xk = &xch->scaleKeys[k];
		xk->time = sk->mTime / tps;
		xk->v = vec3(sk->mValue.x, sk->mValue.y, sk->mValue.z);
	}

	return xch;
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
		// key times come in ticks, we want seconds
		float tps = a->mTicksPerSecond > 0.0 ? a->mTicksPerSecond : 1.0f;
		xa->name = strdup(a->mName.C_Str());
		xa->duration = a->mDuration / tps;
		xa->numChannels = a->mNumChannels;
		xa->channels = (xAnimChannel*)malloc(xa->numChannels * sizeof(xAnimChannel));
		for(u32 j = 0; j < a->mNumChannels; j++)
			convertAssimpChannel(&xa->channels[j], a->mChannels[j], mdl, tps);
	}

	return al;
}
