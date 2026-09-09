#include "xtc.h"
#include "xmodel.h"
#include "chunk.h"

void*
emalloc(size_t sz)
{
	void *p;
	p = malloc(sz);
	assert(p);
	memset(p, 0, sz);
	return p;
}

int
readfile(const char *path, uint8 **data, uint32 *size)
{
	FILE *f;
	f = fopen(path, "rb");
	if(f == nil)
		return 0;
	fseek(f, 0, SEEK_END);
	*size = ftell(f);
	*data = (uint8*)malloc(*size);
	fseek(f, 0, SEEK_SET);
	fread(*data, 1, *size, f);
	fclose(f);
	return 1;
}

static char *ind(int level) {
	static char spaces[1000];
	memset(spaces, ' ', sizeof(spaces));
	spaces[level*2] = '\0';
	return spaces;
}
#define OUT(file, fmt, ...) fprintf(file, "%s" fmt, ind(level) , ##__VA_ARGS__)


xNode*
allocXNode(void)
{
	return (xNode*)emalloc(sizeof(xNode));
}

xSkin*
allocXSkin(int nbones, int nvertices)
{
	uint32 sz = sizeof(xSkin) +
		nvertices*(sizeof(uint8[4]) + sizeof(float[4]) +
		nbones*sizeof(Mat4));
	xSkin *s = (xSkin*)emalloc(sz);
	s->numBones = nbones;
	s->invMatrices = (Mat4*)(s+1);
	s->weights = (float*)(s->invMatrices + s->numBones);
	s->indices = (uint8*)(s->weights + nvertices*4);
	return s;
}

xSkeleton*
allocXSkeleton(int nbones)
{
	xSkeleton *s = (xSkeleton*)emalloc(sizeof(xSkeleton)
		+ nbones*(sizeof(xBone) + sizeof(Mat4)));
	s->numBones = nbones;
	s->bones = (xBone*)(s+1);
	s->matrices = (Mat4*)(s->bones+s->numBones);
	return s;
}

xAnimList*
allocXAnimList(int nanims)
{
	xAnimList *al;
	al = (xAnimList*)emalloc(sizeof(xAnimList));
	al->numAnims = nanims;
	al->anims = (xAnimation*)emalloc(al->numAnims * sizeof(xAnimation));
	return al;
}

#if 0
static void
dumpAssimpMaterial(FILE *file, aiMaterial *mat, int n)
{
	aiColor4D color;
	float f;
	aiString path;
	char *p;

	fprintf(file, "material %d\n", n);

	mat->Get(AI_MATKEY_COLOR_AMBIENT, color);
	fprintf(file, "\tambient %g %g %g %g\n", color.x, color.y, color.z, color.w);
	mat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
	fprintf(file, "\tdiffuse %g %g %g %g\n", color.x, color.y, color.z, color.w);
	mat->Get(AI_MATKEY_COLOR_SPECULAR, color);
	fprintf(file, "\tspecular %g %g %g %g\n", color.x, color.y, color.z, color.w);
	mat->Get(AI_MATKEY_COLOR_EMISSIVE, color);
	fprintf(file, "\temissive %g %g %g %g\n", color.x, color.y, color.z, color.w);
	mat->Get(AI_MATKEY_SHININESS, f);
	fprintf(file, "\tshininess %g\n", f);

	mat->GetTexture(aiTextureType_DIFFUSE, 0, &path);
	p = strrchr((char*)path.C_Str(), '\\');
	if(p)
		p++;
	else
		p = (char*)path.C_Str();
	fprintf(file, "\tdiffusetex \"%s\"\n", p);
	fprintf(file, "endmaterial\n");
}

static void
dumpAssimpMesh(FILE *file, aiMesh *mesh, int n)
{
	fprintf(file, "mesh %d\n", n);

	aiVector3D *v = mesh->mVertices;
	aiVector3D *nm = mesh->mNormals;
	aiVector3D *uv = mesh->mTextureCoords[0];

	fprintf(file, "\tmaterial %d\n", mesh->mMaterialIndex);
	fprintf(file, "\tnumVerts %d\n", mesh->mNumVertices);
	fprintf(file, "\tnumFaces %d\n", mesh->mNumFaces);

	for(uint32 i = 0; i < mesh->mNumVertices; i++) {
		fprintf(file, "\tv %f %f %f\n", v[i].x, v[i].y, v[i].z);
		fprintf(file, "\tn %f %f %f\n", nm[i].x, nm[i].y, nm[i].z);
		fprintf(file, "\tt %f %f\n", uv[i].x, uv[i].y);
	}

	for(uint32 i = 0; i < mesh->mNumFaces; i++) {
		aiFace *f = &mesh->mFaces[i];
		fprintf(file, "\tf %d %d %d\n", f->mIndices[0], f->mIndices[1], f->mIndices[2]);
	}

	fprintf(file, "endmesh\n");
}

static void
dumpAssimpHierarchy(FILE *file, aiNode *node, int level)
{
	OUT(file, "node \"%s\"\n", node->mName.C_Str());
	level++;
	aiMatrix4x4 &mtx = node->mTransformation;
	OUT(file, "xform %g %g %g  %g %g %g  %g %g %g  %g %g %g\n",
		mtx.a1, mtx.b1, mtx.c1,
		mtx.a2, mtx.b2, mtx.c2,
		mtx.a3, mtx.b3, mtx.c3,
		mtx.a4, mtx.b4, mtx.c4);
	if(node->mNumMeshes > 0)
		OUT(file, "numMeshes %d\n", node->mNumMeshes);
	for(uint32 i = 0; i < node->mNumMeshes; i++)
		OUT(file, "mesh %d\n", node->mMeshes[i]);
	for(uint32 i = 0; i < node->mNumChildren; i++)
		dumpAssimpHierarchy(file, node->mChildren[i], level);
	level--;
	OUT(file, "endnode\n");
}

void
dumpAssimpScene(FILE *file, const aiScene *scene)
{
	fprintf(file, "numMaterials %d\n", scene->mNumMaterials);
	fprintf(file, "numMeshes %d\n", scene->mNumMeshes);
	for(uint32 i = 0; i < scene->mNumMaterials; i++)
		dumpAssimpMaterial(file, scene->mMaterials[i], i);
	for(uint32 i = 0; i < scene->mNumMeshes; i++)
		dumpAssimpMesh(file, scene->mMeshes[i], i);
	dumpAssimpHierarchy(file, scene->mRootNode, 0);
}
#endif





void
writeXMatrix(FILE *f, const char *name, Mat4 &m, int level)
{
	OUT(f, "%s %g %g %g  %g %g %g  %g %g %g  %g %g %g\n",
		name,
		m.x.x, m.x.y, m.x.z,
		m.y.x, m.y.y, m.y.z,
		m.z.x, m.z.y, m.z.z,
		m.w.x, m.w.y, m.w.z);
}

int
findPtr(void *p, void **arr, int n)
{
	for(int i = 0; i < n; i++)
		if(arr[i] == p)
			return i;
	return -1;
}


void
writeXMaterial(FILE *f, xMaterial *mat, int n)
{
	fprintf(f, "material %d\n", n);

	Vec4 color = mat->material.ambient;
	fprintf(f, "\tambient %g %g %g %g\n", color.x, color.y, color.z, color.w);
	color = mat->material.diffuse;
	fprintf(f, "\tdiffuse %g %g %g %g\n", color.x, color.y, color.z, color.w);
	color = mat->material.specular;
	fprintf(f, "\tspecular %g %g %g %g\n", color.x, color.y, color.z, color.w);
	color = mat->material.emissive;
	fprintf(f, "\temissive %g %g %g %g\n", color.x, color.y, color.z, color.w);
	fprintf(f, "\tshininess %g\n", mat->material.shininess);

	if(mat->tex)
		fprintf(f, "\tdiffusetex \"%s\"\n", mat->tex->name);

	fprintf(f, "endmaterial\n");
}

static void
writeXMesh(FILE *f, xModel *mdl, xMesh *mesh, int n)
{
	xGeometry *g;
	fprintf(f, "mesh %d\n", n);

	fprintf(f, "\tmaterial %d\n", findPtr(mesh->material, (void**)mdl->materials, mdl->numMaterials));
	g = mesh->geo;
	assert(g);

	fprintf(f, "\tnumVerts %d\n", g->numVertices);
	fprintf(f, "\tnumFaces %d\n", g->numIndices/3);

	xVertex *vx = g->vertices;
	for(int i = 0; i < g->numVertices; i++) {
		fprintf(f, "\tv %f %f %f\n", vx->vtx[0], vx->vtx[1], vx->vtx[2]);
		fprintf(f, "\tn %f %f %f\n", vx->nrm[0], vx->nrm[1], vx->nrm[2]);
		fprintf(f, "\tt %f %f\n", vx->tex[0], vx->tex[1]);
		fprintf(f, "\tc %d %d %d %d\n", vx->col[0], vx->col[1], vx->col[2], vx->col[3]);
		vx++;
	}

	for(int i = 0; i < g->numIndices; i += 3) {
		fprintf(f, "\tf %d %d %d\n", g->indices[i+0], g->indices[i+1], g->indices[i+2]);
	}

	if(mesh->skin) {
		xSkin *s = mesh->skin;

		fprintf(f, "\tskin %d\n", s->numBones);
		float *w = s->weights;
		uint8 *x = s->indices;
		for(int i = 0; i < g->numVertices; i++) {
			fprintf(f, "\tw %f %f %f %f\n", w[i*4+0],  w[i*4+1],  w[i*4+2],  w[i*4+3]);
			fprintf(f, "\ti %d %d %d %d\n", x[i*4+0],  x[i*4+1],  x[i*4+2],  x[i*4+3]);
		}
		for(int i = 0; i < s->numBones; i++) {
			fprintf(f, "\t");
			writeXMatrix(f, "invmat", s->invMatrices[i], 0);
		}
	}

	fprintf(f, "endmesh\n");
}

void
writeXSkeleton(FILE *f, xSkeleton *skel, int level)
{
	OUT(f, "skeleton %d\n", skel->numBones);
	for(int i = 0; i < skel->numBones; i++) {
		xBone *b = &skel->bones[i];
		const char *name = b->node ? b->node->name : "(null)";
		OUT(f, "bone \"%s\" %d %d ", name, b->flag, b->tag);
		writeXMatrix(f, "", skel->matrices[i], 0);
	}
}

void
writeXNode(FILE *f, xModel *mdl, xNode *node, int level)
{
	OUT(f, "node \"%s\"\n", node->name);
	level++;
	if(node->tag >= 0)
		OUT(f, "tag %d\n", node->tag);
	writeXMatrix(f, "xform", node->localMatrix, level);
	if(node->numMeshes > 0)
		OUT(f, "numMeshes %d\n", node->numMeshes);
	for(int i = 0; i < node->numMeshes; i++)
		OUT(f, "mesh %d\n", findPtr(node->meshes[i], (void**)mdl->meshes, mdl->numMeshes));
	for(xNode *child = node->child; child; child = child->next)
		writeXNode(f, mdl, child, level);
	// need skeleton at end because we're referencing node names
	if(node->skel)
		writeXSkeleton(f, node->skel, level);
	level--;
	OUT(f, "endnode\n");
}

void
writeXModel(FILE *f, xModel *mdl)
{
	fprintf(f, "numMaterials %d\n", mdl->numMaterials);
	fprintf(f, "numMeshes %d\n", mdl->numMeshes);
	for(int i = 0; i < mdl->numMaterials; i++)
		writeXMaterial(f, mdl->materials[i], i);
	for(int i = 0; i < mdl->numMeshes; i++)
		writeXMesh(f, mdl, mdl->meshes[i], i);
	writeXNode(f, mdl, mdl->root, 0);
}


static void
writeXAnimChannel(FILE *f, xAnimChannel *c)
{
	fprintf(f, "channel \"%s\" %d %d %d %d\n",
		c->name, c->id, c->numRotKeys, c->numTransKeys, c->numScaleKeys);
	for(int i = 0; i < c->numRotKeys; i++) {
		xRotKey *k = &c->rotKeys[i];
		fprintf(f, "\trot %f %f %f %f %f\n", k->time, k->rot.x, k->rot.y, k->rot.z, k->rot.w);
	}
	for(int i = 0; i < c->numTransKeys; i++) {
		xVecKey *k = &c->transKeys[i];
		fprintf(f, "\ttrans %f %f %f %f\n", k->time, k->v.x, k->v.y, k->v.z);
	}
	for(int i = 0; i < c->numScaleKeys; i++) {
		xVecKey *k = &c->scaleKeys[i];
		fprintf(f, "\tscale %f %f %f %f\n", k->time, k->v.x, k->v.y, k->v.z);
	}
}

static void
writeXAnimation(FILE *f, xAnimation *a)
{
	fprintf(f, "anim \"%s\" %f %d\n",
		a->name, a->duration, a->numChannels);
	for(int i = 0; i < a->numChannels; i++)
		writeXAnimChannel(f, &a->channels[i]);
}

void
writeXAnimList(FILE *f, xAnimList *al)
{
	fprintf(f, "numAnimations %d\n", al->numAnims);
	for(int i = 0; i < al->numAnims; i++)
		writeXAnimation(f, &al->anims[i]);
}


void
addChild(xNode *parent, xNode *child)
{
	xNode **n;
	child->parent = parent;
	child->next = nil;
	for(n = &parent->child; *n; n = &(*n)->next);
	*n = child;
}


static const char *extensions[] = {
	"",
	".PNG",
	".png",
	nil
};

static xTexture*
allocXTexture(const char *name)
{
	xTexture *tex;
	tex = (xTexture*)emalloc(sizeof(xTexture)+strlen(name)+1);
	tex->name = (char*)(tex+1);
	strcpy(tex->name, name);
	return tex;
}

xTexture*
createTexturePNG(const char *name, const uint8 *data, uint32 size)
{
	xTexture *tex = allocXTexture(name);
	tex->tex = xtcTextureReadPNG(data, size);
	return tex;
}

// looks for a PNG file in texpath
xTexture*
readTexture(const char *name)
{
	xTexture *tex;
	char abspath[1024];
	uint8 *data;
	uint32 size;

	tex = allocXTexture(name);
	for(int i = 0; extensions[i]; i++) {
		snprintf(abspath, sizeof(abspath), "%s/%s%s", texpath, name, extensions[i]);
		if(readfile(abspath, &data, &size)) {
			tex->tex = xtcTextureReadPNG(data, size);
			free(data);
			return tex;
		}
	}
	fprintf(stderr, "warning: texture %s not found in %s\n", name, texpath);
	return tex;
}

const char *texpath = ".";

extern "C" int tokenize(char *s, char **args, int maxargs);

struct Cmd {
	const char *str;
	int cmd;
};

int
lookup(struct Cmd *tab, char *str)
{
	for(int i = 0; tab[i].str; i++)
		if(strcmp(tab[i].str, str) == 0)
			return tab[i].cmd;
	return -1;
}

#define CMDS \
	X("numMeshes", CMD_NUM_MESHES) \
	X("numMaterials", CMD_NUM_MATERIALS) \
	X("material", CMD_MATERIAL) \
	X("ambient", CMD_AMBIENT) \
	X("diffuse", CMD_DIFFUSE) \
	X("specular", CMD_SPECULAR) \
	X("emissive", CMD_EMISSIVE) \
	X("shininess", CMD_SHININESS) \
	X("diffusetex", CMD_DIFFUSETEX) \
	X("endmaterial", CMD_ENDMATERIAL) \
	X("mesh", CMD_MESH) \
	X("numVerts", CMD_NUM_VERTS) \
	X("v", CMD_V) \
	X("n", CMD_N) \
	X("t", CMD_T) \
	X("c", CMD_C) \
	X("numFaces", CMD_NUM_FACES) \
	X("f", CMD_F) \
	X("skin", CMD_SKIN) \
	X("w", CMD_W) \
	X("i", CMD_I) \
	X("invmat", CMD_INVMAT) \
	X("endmesh", CMD_ENDMESH) \
	X("node", CMD_NODE) \
	X("tag", CMD_TAG) \
	X("xform", CMD_XFORM) \
	X("skeleton", CMD_SKELETON) \
	X("bone", CMD_BONE) \
	X("endnode", CMD_ENDNODE) \

enum {
#define X(str, cmd) cmd,
	CMDS
#undef X
};

static Cmd cmds[] = {
#define X(str, cmd) { str, cmd },
	CMDS
#undef X
	{ nil, -1 }
};

void
allocXGeo(xMesh *m, int numVerts, int numIndices)
{
	xGeometry *g;
	if(m->geo) return;
	g = (xGeometry*)emalloc(sizeof(xGeometry) + numVerts*sizeof(xVertex) + numIndices*sizeof(int));
	g->numVertices = numVerts;
	g->numIndices = numIndices;
	g->vertices = (xVertex*)(g+1);
	g->indices = (int*)(g->vertices+numVerts);
	m->geo = g;
}

Mat4
readXMatrix(char **tokens)
{
	Mat4 m;
	m.x.x = atof(tokens[0]);
	m.x.y = atof(tokens[1]);
	m.x.z = atof(tokens[2]);
	m.x.w = 0.0f;
	m.y.x = atof(tokens[3]);
	m.y.y = atof(tokens[4]);
	m.y.z = atof(tokens[5]);
	m.y.w = 0.0f;
	m.z.x = atof(tokens[6]);
	m.z.y = atof(tokens[7]);
	m.z.z = atof(tokens[8]);
	m.z.w = 0.0f;
	m.w.x = atof(tokens[9]);
	m.w.y = atof(tokens[10]);
	m.w.z = atof(tokens[11]);
	m.w.w = 1.0f;
	return m;
}

xNode*
findXNode(xNode *root, const char *name)
{
	if(root == nil) return nil;
	if(strcmp(root->name, name) == 0) return root;
	for(xNode *child = root->child; child; child = child->next) {
		xNode *n = findXNode(child, name);
		if(n) return n;
	}
	return nil;
}

xModel*
loadXModel(FILE *file)
{
	char line[4096];
	char *tokens[1000];
	int n, ntok;
	xModel *mdl;
	xMaterial *mat = nil;
	xMesh *mesh = nil;
	xGeometry *g;
	xSkin *s;
	xBone *b;
	xNode *t, *node = nil;
	int numVerts, numFaces, numBones;
	int nv, nn, nt, nc, ni, nw, nx, nb;

	mdl = (xModel*)emalloc(sizeof(xModel));

	while(fgets(line, sizeof(line), file)) {
		ntok = tokenize(line, tokens, 1000);
		if(ntok < 1) continue;
		switch(lookup(cmds, tokens[0])) {
		case CMD_NUM_MESHES:
			if(node) {
				node->numMeshes = atoi(tokens[1]);
				node->meshes = (xMesh**)emalloc(node->numMeshes*sizeof(xMesh*));
				// counting up later
				node->numMeshes = 0;
			} else {
				mdl->numMeshes = atoi(tokens[1]);
				mdl->meshes = (xMesh**)emalloc(mdl->numMeshes*sizeof(xMesh*));
			}
			break;
		case CMD_NUM_MATERIALS:
			mdl->numMaterials = atoi(tokens[1]);
			mdl->materials = (xMaterial**)emalloc(mdl->numMaterials*sizeof(xMaterial*));
			break;

		case CMD_MATERIAL:
			if(mesh) {
				n = atoi(tokens[1]);
				assert(n < mdl->numMaterials);
				mesh->material = mdl->materials[n];
			} else {
				assert(mat == nil);
				mat = (xMaterial*)emalloc(sizeof(xMaterial));
				mat->material.shininess = 0.0f;
				mat->material.ambient = vec4(1.0f, 1.0f, 1.0f, 1.0f);
				mat->material.diffuse = vec4(1.0f, 1.0f, 1.0f, 1.0f);
				mat->material.specular = vec4(1.0f, 1.0f, 1.0f, 1.0f);
				// use vertex color for emissive
				mat->material.colorSelector = vec4(0.0f, 0.0f, 0.0f, 1.0f);
				n = atoi(tokens[1]);
				assert(n < mdl->numMaterials);
				mdl->materials[n] = mat;
			}
			break;
		case CMD_ENDMATERIAL:
			assert(mat);
			mat = nil;
			break;
		case CMD_AMBIENT:
			assert(mat);
			mat->material.ambient.x = atof(tokens[1]);
			mat->material.ambient.y = atof(tokens[2]);
			mat->material.ambient.z = atof(tokens[3]);
			mat->material.ambient.w = atof(tokens[4]);
			break;
		case CMD_DIFFUSE:
			assert(mat);
			mat->material.diffuse.x = atof(tokens[1]);
			mat->material.diffuse.y = atof(tokens[2]);
			mat->material.diffuse.z = atof(tokens[3]);
			mat->material.diffuse.w = atof(tokens[4]);
			break;
		case CMD_SPECULAR:
			assert(mat);
			mat->material.specular.x = atof(tokens[1]);
			mat->material.specular.y = atof(tokens[2]);
			mat->material.specular.z = atof(tokens[3]);
			mat->material.specular.w = atof(tokens[4]);
			break;
		case CMD_EMISSIVE:
			assert(mat);
			mat->material.emissive.x = atof(tokens[1]);
			mat->material.emissive.y = atof(tokens[2]);
			mat->material.emissive.z = atof(tokens[3]);
			mat->material.emissive.w = atof(tokens[4]);
			break;
		case CMD_SHININESS:
			assert(mat);
			mat->material.shininess = atof(tokens[1]);
			break;
		case CMD_DIFFUSETEX:
			assert(mat);
			mat->tex = readTexture(tokens[1]);
			break;

		case CMD_MESH:
			if(node) {
				n = atoi(tokens[1]);
				assert(n < mdl->numMeshes);
				node->meshes[node->numMeshes++] = mdl->meshes[n];
			} else {
				assert(mesh == nil);
				mesh = (xMesh*)emalloc(sizeof(xMesh));
				n = atoi(tokens[1]);
				assert(n < mdl->numMeshes);
				mdl->meshes[n] = mesh;
				numVerts = 0;
				numFaces = 0;
				numBones = 0;
				nv = 0;
				nn = 0;
				nt = 0;
				nc = 0;
				ni = 0;
				nw = 0;
				nx = 0;
				nb = 0;
			}
			break;
		case CMD_ENDMESH:
			assert(mesh);
			mesh = nil;
			break;

		case CMD_NODE:
			t = (xNode*)emalloc(sizeof(xNode));
			t->name = strdup(tokens[1]);
			t->tag = -1;
			if(node) {
				addChild(node, t);
			} else {
				assert(mdl->root == nil);
				mdl->root = t;
			}
			node = t;
			break;
		case CMD_TAG:
			assert(node);
			node->tag = atoi(tokens[1]);
			break;
		case CMD_XFORM:
			assert(node);
			node->localMatrix = readXMatrix(tokens+1);
			break;
		case CMD_ENDNODE:
			assert(node);
			node = node->parent;
			break;

		case CMD_NUM_VERTS:
			assert(mesh);
			assert(numVerts == 0);
			numVerts = atoi(tokens[1]);
			break;
		case CMD_V:
			assert(mesh);
			allocXGeo(mesh, numVerts, numFaces*3);
			g = mesh->geo;
			assert(nv < g->numVertices);
			g->vertices[nv].vtx[0] = atof(tokens[1]);
			g->vertices[nv].vtx[1] = atof(tokens[2]);
			g->vertices[nv].vtx[2] = atof(tokens[3]);
			nv++;
			break;
		case CMD_N:
			assert(mesh);
			allocXGeo(mesh, numVerts, numFaces*3);
			g = mesh->geo;
			assert(nn < g->numVertices);
			g->vertices[nn].nrm[0] = atof(tokens[1]);
			g->vertices[nn].nrm[1] = atof(tokens[2]);
			g->vertices[nn].nrm[2] = atof(tokens[3]);
			nn++;
			break;
		case CMD_T:
			assert(mesh);
			allocXGeo(mesh, numVerts, numFaces*3);
			g = mesh->geo;
			assert(nt < g->numVertices);
			g->vertices[nt].tex[0] = atof(tokens[1]);
			g->vertices[nt].tex[1] = atof(tokens[2]);
			nt++;
			break;
		case CMD_C:
			assert(mesh);
			allocXGeo(mesh, numVerts, numFaces*3);
			g = mesh->geo;
			assert(nc < g->numVertices);
			g->vertices[nc].col[0] = atoi(tokens[1]);
			g->vertices[nc].col[1] = atoi(tokens[2]);
			g->vertices[nc].col[2] = atoi(tokens[3]);
			g->vertices[nc].col[3] = atoi(tokens[4]);
			nc++;
			break;


		case CMD_NUM_FACES:
			assert(mesh);
			assert(numFaces == 0);
			numFaces = atoi(tokens[1]);
			break;
		case CMD_F:
			assert(mesh);
			allocXGeo(mesh, numVerts, numFaces*3);
			g = mesh->geo;
			assert(ni < g->numIndices);
			g->indices[ni++] = atoi(tokens[1]);
			g->indices[ni++] = atoi(tokens[2]);
			g->indices[ni++] = atoi(tokens[3]);
			break;

		case CMD_SKIN:
			assert(mesh);
			assert(mesh->skin == nil);
			numBones = atoi(tokens[1]);
			mesh->skin = allocXSkin(numBones, mesh->geo->numVertices);
			break;
		case CMD_W:
			assert(mesh);
			assert(mesh->skin);
			assert(nw < mesh->geo->numVertices);
			s = mesh->skin;
			s->weights[nw*4+0] = atof(tokens[1]);
			s->weights[nw*4+1] = atof(tokens[2]);
			s->weights[nw*4+2] = atof(tokens[3]);
			s->weights[nw*4+3] = atof(tokens[4]);
			nw++;
			break;
		case CMD_I:
			assert(mesh);
			assert(mesh->skin);
			assert(nx < mesh->geo->numVertices);
			s = mesh->skin;
			s->indices[nx*4+0] = atoi(tokens[1]);
			s->indices[nx*4+1] = atoi(tokens[2]);
			s->indices[nx*4+2] = atoi(tokens[3]);
			s->indices[nx*4+3] = atoi(tokens[4]);
			nx++;
			break;
		case CMD_INVMAT:
			assert(mesh);
			assert(mesh->skin);
			s = mesh->skin;
			assert(nb < s->numBones);
			s->invMatrices[nb++] = readXMatrix(tokens+1);
			break;

		case CMD_SKELETON:
			assert(node);
			assert(node->skel == nil);
			numBones = atoi(tokens[1]);
			node->skel = allocXSkeleton(numBones);
			mdl->skel = node->skel;
			nb = 0;
			break;
		case CMD_BONE:
			assert(node);
			assert(node->skel);
			assert(nb < node->skel->numBones);
			b = &node->skel->bones[nb];
			b->node = findXNode(node, tokens[1]);
			b->flag = atoi(tokens[2]);
			b->tag = atoi(tokens[3]);
			node->skel->matrices[nb] = readXMatrix(tokens+4);
			nb++;
			break;

		case -1:
			printf("unknown cmd %s\n", tokens[0]);
			exit(1);
		}
	}

	return mdl;
}

void
buildXModel(xModel *mdl)
{
	xMesh *m;
	xGeometry *g;
	for(int i = 0; i < mdl->numMeshes; i++) {
		m = mdl->meshes[i];
		g = m->geo;
		if(g == nil) continue;

		m->prims = xtcCreatePrimList();
		xtcStartList(m->prims);

		xtcBegin(XTC_TRILIST);
		for(int i = 0; i < g->numIndices; i++) {
			if(m->skin) {
				uint8 *is = &m->skin->indices[4*g->indices[i]];
				float *ws = &m->skin->weights[4*g->indices[i]];
				xtcIndices(is[0], is[1], is[2], is[3]);
				xtcWeights(ws[0], ws[1], ws[2], ws[3]);
			}

			xVertex *v = &g->vertices[g->indices[i]];
			xtcTexCoord(v->tex[0], v->tex[1]);
			xtcNormal(v->nrm[0], v->nrm[1], v->nrm[2]);
			xtcColor(v->col[0], v->col[1], v->col[2], v->col[3]);
			xtcVertex3(v->vtx[0], v->vtx[1], v->vtx[2]);
		}
		xtcEnd();
		xtcEndList();

//		free(g);
//		m->geo = nil;
	}
}


static void
nodeBounds(xNode *n, const Mat4 &parent, xSkeleton *skel, Vec3 *bmin, Vec3 *bmax)
{
	Mat4 world = parent * n->localMatrix;
	for(int i = 0; i < n->numMeshes; i++) {
		xMesh *m = n->meshes[i];
		xGeometry *g = m->geo;
		if(g == nil)
			continue;
		bool skinned = m->skin && skel;
		for(int j = 0; j < g->numVertices; j++) {
			xVertex *vx = &g->vertices[j];
			Vec4 v = vec4(vx->vtx[0], vx->vtx[1], vx->vtx[2], 1.0f);
			Vec3 p;
			if(skinned) {
				p = vec3(0.0f, 0.0f, 0.0f);
				for(int k = 0; k < 4; k++) {
					float w = m->skin->weights[4*j+k];
					int b = m->skin->indices[4*j+k];
					if(w > 0.0f)
						p += w * v4tov3(skel->matrices[b] * m->skin->invMatrices[b] * v);
				}
			} else
				p = v4tov3(world * v);
			*bmin = v3min(*bmin, p);
			*bmax = v3max(*bmax, p);
		}
	}
	for(xNode *child = n->child; child; child = child->next)
		nodeBounds(child, world, skel, bmin, bmax);
}

// skinned meshes use whatever pose the skeleton is in
void
xModelBoundingSphere(xModel *mdl, Vec3 *center, float *radius)
{
	Vec3 bmin = vec3(1e30f, 1e30f, 1e30f), bmax = vec3(-1e30f, -1e30f, -1e30f);
	nodeBounds(mdl->root, m4ident(), mdl->skel, &bmin, &bmax);
	if(bmin.x > bmax.x) {
		*center = vec3(0.0f, 0.0f, 0.0f);
		*radius = 1.0f;
		return;
	}
	*center = (bmin + bmax) * 0.5f;
	*radius = v3norm(bmax - bmin) * 0.5f;
}


#define ACMDS \
	X("numAnimations", CMD_NUM_ANIMATIONS) \
	X("anim", CMD_ANIM) \
	X("channel", CMD_CHANNEL) \
	X("rot", CMD_ROT) \
	X("trans", CMD_TRANS) \
	X("scale", CMD_SCALE) \


enum {
#define X(str, cmd) cmd,
	ACMDS
#undef X
};

static Cmd acmds[] = {
#define X(str, cmd) { str, cmd },
	ACMDS
#undef X
	{ nil, -1 }
};

static Vec3
readVec3(char **tokens)
{
	return vec3(atof(tokens[0]), atof(tokens[1]), atof(tokens[2]));
}

xAnimList*
loadXAnimList(FILE *file)
{
	char line[4096];
	char *tokens[1000];
	int ntok;
	xAnimList *al;
	xAnimation *anim;
	xAnimChannel *chan;
	int na, nc, nr, nt, ns;

	al = nil;
	anim = nil;
	chan = nil;
	na = nc = nr = nt = ns = 0;

	while(fgets(line, sizeof(line), file)) {
		ntok = tokenize(line, tokens, 1000);
		if(ntok < 1) continue;
		switch(lookup(acmds, tokens[0])) {
		case CMD_NUM_ANIMATIONS:
			assert(al == nil);
			al = allocXAnimList(atoi(tokens[1]));
			na = 0;
			break;

		case CMD_ANIM:
			assert(al);
			assert(na < al->numAnims);
			anim = &al->anims[na++];
			anim->name = strdup(tokens[1]);
			anim->duration = atof(tokens[2]);
			anim->numChannels = atoi(tokens[3]);
			anim->channels = (xAnimChannel*)emalloc(anim->numChannels * sizeof(xAnimChannel));
			nc = 0;
			break;

		case CMD_CHANNEL:
			assert(anim);
			assert(nc < anim->numChannels);
			chan = &anim->channels[nc++];
			chan->name = strdup(tokens[1]);
			chan->id = atoi(tokens[2]);
			chan->numRotKeys = atoi(tokens[3]);
			chan->numTransKeys = atoi(tokens[4]);
			chan->numScaleKeys = atoi(tokens[5]);
			chan->rotKeys = chan->numRotKeys ? (xRotKey*)emalloc(chan->numRotKeys * sizeof(xRotKey)) : nil;
			chan->transKeys = chan->numTransKeys ? (xVecKey*)emalloc(chan->numTransKeys * sizeof(xVecKey)) : nil;
			chan->scaleKeys = chan->numScaleKeys ? (xVecKey*)emalloc(chan->numScaleKeys * sizeof(xVecKey)) : nil;
			nr = nt = ns = 0;
			break;

		case CMD_ROT:
			assert(chan);
			assert(nr < chan->numRotKeys);
			chan->rotKeys[nr].time = atof(tokens[1]);
			chan->rotKeys[nr].rot.x = atof(tokens[2]);
			chan->rotKeys[nr].rot.y = atof(tokens[3]);
			chan->rotKeys[nr].rot.z = atof(tokens[4]);
			chan->rotKeys[nr].rot.w = atof(tokens[5]);
			nr++;
			break;

		case CMD_TRANS:
			assert(chan);
			assert(nt < chan->numTransKeys);
			chan->transKeys[nt].time = atof(tokens[1]);
			chan->transKeys[nt].v = readVec3(tokens+2);
			nt++;
			break;

		case CMD_SCALE:
			assert(chan);
			assert(ns < chan->numScaleKeys);
			chan->scaleKeys[ns].time = atof(tokens[1]);
			chan->scaleKeys[ns].v = readVec3(tokens+2);
			ns++;
			break;

		case -1:
			printf("unknown cmd %s\n", tokens[0]);
			exit(1);
		}
	}

	return al;
}






#define PTR sizeof(void*)

static void
saveXMaterialChunk(ChunkData *chk, xMaterial *mat)
{
	if(registerBlock(chk, mat, sizeof(*mat), 16))
		return;
	registerPointer(chk, &mat->tex);
	if(mat->tex) {
		char *name = mat->tex->name;
		// mega hack - pointer will be restored
		mat->tex = (xTexture*)name;
		registerBlock(chk, name, strlen(name)+1, 1);
	}
}

static void
saveXGeometryChunk(ChunkData *chk, xGeometry *geo)
{
	if(registerBlock(chk, geo, sizeof(*geo), 16))
		return;
	registerPointer(chk, &geo->vertices);
	registerPointer(chk, &geo->indices);
	registerBlock(chk, geo->vertices, geo->numVertices*sizeof(*geo->vertices), 16);
	registerBlock(chk, geo->indices, geo->numIndices*sizeof(*geo->indices), 16);
}

static void
saveXSkinChunk(ChunkData *chk, xGeometry *geo, xSkin *skin)
{
	if(registerBlock(chk, skin, sizeof(*skin), 16))
		return;
	registerPointer(chk, &skin->invMatrices);
	registerPointer(chk, &skin->indices);
	registerPointer(chk, &skin->weights);
	registerBlock(chk, skin->invMatrices, skin->numBones*sizeof(Mat4), 16);
	registerBlock(chk, skin->indices, geo->numVertices*4*sizeof(*skin->indices), 4);
	registerBlock(chk, skin->weights, geo->numVertices*4*sizeof(*skin->weights), 16);
}

static void
saveXMeshChunk(ChunkData *chk, xMesh *mesh)
{
	if(registerBlock(chk, mesh, sizeof(*mesh), 16))
		return;
	// TODO: native geometry. for now the platform data is
	// rebuilt after loading, so don't save the pointer
	mesh->prims = nil;
	registerPointer(chk, &mesh->prims);
	registerPointer(chk, &mesh->material);
	assert(mesh->geo);
	registerPointer(chk, &mesh->geo);
	registerPointer(chk, &mesh->skin);
	saveXGeometryChunk(chk, mesh->geo);
	if(mesh->skin)
		saveXSkinChunk(chk, mesh->geo, mesh->skin);
}

static void
saveXSkeletonChunk(ChunkData *chk, xSkeleton *skel)
{
	if(registerBlock(chk, skel, sizeof(*skel), 16))
		return;
	registerPointer(chk, &skel->bones);
	registerPointer(chk, &skel->matrices);
	registerBlock(chk, skel->matrices, skel->numBones*sizeof(*skel->matrices), 16);
	registerBlock(chk, skel->bones, skel->numBones*sizeof(*skel->bones), PTR);
	for(int i = 0; i < skel->numBones; i++)
		registerPointer(chk, &skel->bones[i].node);
}

static void
saveXNodeChunk(ChunkData *chk, xNode *node)
{
	if(registerBlock(chk, node, sizeof(*node), 16))
		return;
	registerPointer(chk, &node->name);
	registerPointer(chk, &node->parent);
	registerPointer(chk, &node->child);
	registerPointer(chk, &node->next);
	registerPointer(chk, &node->meshes);
	registerPointer(chk, &node->skel);

	if(node->name)
		registerBlock(chk, node->name, strlen(node->name)+1, 1);

	if(node->meshes) {
		registerBlock(chk, node->meshes, node->numMeshes*PTR, PTR);
		for(int i = 0; i < node->numMeshes; i++)
			registerPointer(chk, &node->meshes[i]);
	}
	for(xNode *child = node->child; child; child = child->next)
		saveXNodeChunk(chk, child);
	if(node->skel)
		saveXSkeletonChunk(chk, node->skel);
}

static void
saveXModelChunk(ChunkData *chk, xModel *mdl)
{
	int i;

	if(registerBlock(chk, mdl, sizeof(*mdl), 16))
		return;
	registerPointer(chk, &mdl->meshes);
	registerPointer(chk, &mdl->materials);
	registerPointer(chk, &mdl->root);
	// should have been saved elsewhere
	registerPointer(chk, &mdl->skel);

	registerBlock(chk, mdl->meshes, mdl->numMeshes*PTR, PTR);
	for(i = 0; i < mdl->numMeshes; i++)
		registerPointer(chk, &mdl->meshes[i]);

	registerBlock(chk, mdl->materials, mdl->numMaterials*PTR, PTR);
	for(i = 0; i < mdl->numMaterials; i++)
		registerPointer(chk, &mdl->materials[i]);

	for(i = 0; i < mdl->numMeshes; i++)
		saveXMeshChunk(chk, mdl->meshes[i]);
	for(i = 0; i < mdl->numMaterials; i++)
		saveXMaterialChunk(chk, mdl->materials[i]);
	saveXNodeChunk(chk, mdl->root);
}

void
writeXModelChunk(FILE *f, xModel *mdl)
{
	ChunkData *chk;
	xtcPrimList **prims = (xtcPrimList**)malloc(mdl->numMeshes*sizeof(xtcPrimList*));
	for(int i = 0; i < mdl->numMeshes; i++)
		prims[i] = mdl->meshes[i]->prims;
	chk = makeChunkData();
	saveXModelChunk(chk, mdl);
	writeChunk(chk, f);
	freeChunkData(chk);
	for(int i = 0; i < mdl->numMeshes; i++)
		mdl->meshes[i]->prims = prims[i];
	free(prims);
}




xModel*
loadXModelChunk(FILE *f)
{
	xModel *mdl;
	xMaterial *mat;

	mdl = (xModel*)loadChunk(f);
	for(int i = 0; i < mdl->numMaterials; i++) {
		mat = mdl->materials[i];
		if(mat->tex)
			mat->tex = readTexture((char*)mat->tex);
	}

	return mdl;
}



static void
saveXAnimListChunk(ChunkData *chk, xAnimList *alist)
{
	int i, j;

	if(registerBlock(chk, alist, sizeof(*alist), 16))
		return;
	registerPointer(chk, &alist->anims);
	// pointers have to be registered while their block is the latest one
	registerBlock(chk, alist->anims, alist->numAnims*sizeof(*alist->anims), PTR);
	for(i = 0; i < alist->numAnims; i++) {
		xAnimation *a = &alist->anims[i];
		registerPointer(chk, &a->name);
		registerPointer(chk, &a->channels);
	}
	for(i = 0; i < alist->numAnims; i++) {
		xAnimation *a = &alist->anims[i];
		registerBlock(chk, a->name, strlen(a->name)+1, 1);
		registerBlock(chk, a->channels, a->numChannels*sizeof(*a->channels), PTR);
		for(j = 0; j < a->numChannels; j++) {
			xAnimChannel *c = &a->channels[j];
			registerPointer(chk, &c->name);
			registerPointer(chk, &c->rotKeys);
			registerPointer(chk, &c->transKeys);
			registerPointer(chk, &c->scaleKeys);
		}
		for(j = 0; j < a->numChannels; j++) {
			xAnimChannel *c = &a->channels[j];
			registerBlock(chk, c->name, strlen(c->name)+1, 1);
			if(c->rotKeys)
				registerBlock(chk, c->rotKeys, c->numRotKeys*sizeof(*c->rotKeys), 16);
			if(c->transKeys)
				registerBlock(chk, c->transKeys, c->numTransKeys*sizeof(*c->transKeys), 16);
			if(c->scaleKeys)
				registerBlock(chk, c->scaleKeys, c->numScaleKeys*sizeof(*c->scaleKeys), 16);
		}
	}
}

void
writeXAnimListChunk(FILE *f, xAnimList *alist)
{
	ChunkData *chk;
	chk = makeChunkData();
	saveXAnimListChunk(chk, alist);
	writeChunk(chk, f);
	freeChunkData(chk);
}

xAnimList*
loadXAnimListChunk(FILE *f)
{
	return (xAnimList*)loadChunk(f);
}
