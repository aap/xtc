#include <vector>

#include <assimp/DefaultIOSystem.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <rw.h>
#include <src/rwgta.h>
#include "assimp_rw.h"

static constexpr aiImporterDesc desc = {
	"Renderware DFF Importer",
	"author",
	"maintainer",
	"comments",
	aiImporterFlags_SupportBinaryFlavour,
	0,
	0,
	0,
	0,
	"dff"
};

// We only care about texture names. The null driver can't create
// rasters, not even the empty ones librw's dummy textures want.
static rw::Texture*
readTextureName(const char *name, const char *mask)
{
	rw::Texture *tex = rw::Texture::create(nil);
	if(tex == nil)
		return nil;
	strncpy(tex->name, name, sizeof(tex->name));
	if(mask)
		strncpy(tex->mask, mask, sizeof(tex->mask));
	return tex;
}

static bool rwinitialized;
static void
rwinit(void)
{
	if(rwinitialized) return;
	rw::Engine::init();
	gta::attachPlugins();
	rw::Engine::open(nil);
	rw::Engine::start();
	rw::Texture::readCB = readTextureName;
	rwinitialized = true;
}

namespace Assimp {

DFFImporter::DFFImporter() {}
DFFImporter::~DFFImporter() {}

const aiImporterDesc*
DFFImporter::GetInfo() const {
	return &desc;
}

struct ImpTemp {
	std::vector<aiMesh*> meshes;
	std::vector<aiMaterial*> materials;
};

static aiMaterial*
rwMaterialToAssimpMaterial(ImpTemp &t, rw::Material *m)
{
	aiMaterial *am = new aiMaterial();

	aiColor4D c;
	c.r = m->color.red   * m->surfaceProps.ambient / 255.0f;
	c.g = m->color.green * m->surfaceProps.ambient / 255.0f;
	c.b = m->color.blue  * m->surfaceProps.ambient / 255.0f;
	c.a = m->color.alpha * m->surfaceProps.ambient / 255.0f;
	am->AddProperty(&c, 1, AI_MATKEY_COLOR_AMBIENT);
	c.r = m->color.red   * m->surfaceProps.diffuse / 255.0f;
	c.g = m->color.green * m->surfaceProps.diffuse / 255.0f;
	c.b = m->color.blue  * m->surfaceProps.diffuse / 255.0f;
	c.a = m->color.alpha * m->surfaceProps.diffuse / 255.0f;
	am->AddProperty(&c, 1, AI_MATKEY_COLOR_DIFFUSE);
	c.r = m->color.red   * m->surfaceProps.specular / 255.0f;
	c.g = m->color.green * m->surfaceProps.specular / 255.0f;
	c.b = m->color.blue  * m->surfaceProps.specular / 255.0f;
	c.a = m->color.alpha * m->surfaceProps.specular / 255.0f;
	am->AddProperty(&c, 1, AI_MATKEY_COLOR_SPECULAR);

	if(m->texture) {
		aiString s(m->texture->name);
		am->AddProperty(&s, AI_MATKEY_TEXTURE_DIFFUSE(0));
	}

	return am;
}

static void
rwGeometryToAssimpMeshes(ImpTemp &t, rw::Geometry *g)
{
	using namespace rw;
	int i, j;

	int first = t.materials.size();
	for(i = 0; i < g->matList.numMaterials; i++)
		t.materials.push_back(rwMaterialToAssimpMaterial(t, g->matList.materials[i]));

	MorphTarget *mt = &g->morphTargets[0];
	int *imap = new int[g->numVertices];

	for(i = 0; i < g->matList.numMaterials; i++) {
		int numVerts = 0;
		int numFaces = 0;
		/* build index map for this material/mesh */
		for(j = 0; j < g->numVertices; j++) imap[j] = -1;
		for(j = 0; j < g->numTriangles; j++) {
			if(g->triangles[j].matId != i) continue;
			numFaces++;
			for(int k = 0; k < 3; k++)
				if(imap[g->triangles[j].v[k]] < 0)
					imap[g->triangles[j].v[k]] = numVerts++;
		}

		aiMesh *m = new aiMesh();
		m->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;
		m->mNumVertices = numVerts;
		m->mNumFaces = numFaces;
		m->mMaterialIndex = first + i;

		m->mFaces = new aiFace[m->mNumFaces];
		aiFace *f = m->mFaces;
		for(j = 0; j < g->numTriangles; j++) {
			if(g->triangles[j].matId != i) continue;
			f->mNumIndices = 3;
			f->mIndices = new unsigned int[f->mNumIndices];
			f->mIndices[0] = imap[g->triangles[j].v[0]];
			f->mIndices[1] = imap[g->triangles[j].v[1]];
			f->mIndices[2] = imap[g->triangles[j].v[2]];
			f++;
		}

		m->mVertices = new aiVector3D[m->mNumVertices];
		for(j = 0; j < g->numVertices; j++) {
			int ix = imap[j];
			if(ix < 0) continue;
			m->mVertices[ix].x = mt->vertices[j].x;
			m->mVertices[ix].y = mt->vertices[j].y;
			m->mVertices[ix].z = mt->vertices[j].z;
		}

		if(g->flags & Geometry::NORMALS) {
			m->mNormals = new aiVector3D[m->mNumVertices];
			for(j = 0; j < g->numVertices; j++) {
				int ix = imap[j];
				if(ix < 0) continue;
				m->mNormals[ix].x = mt->normals[j].x;
				m->mNormals[ix].y = mt->normals[j].y;
				m->mNormals[ix].z = mt->normals[j].z;
			}
		}

		if(g->flags & Geometry::PRELIT) {
			m->mColors[0] = new aiColor4D[m->mNumVertices];
			for(j = 0; j < g->numVertices; j++) {
				int ix = imap[j];
				if(ix < 0) continue;
				m->mColors[0][ix].r = g->colors[j].red/255.0f;
				m->mColors[0][ix].g = g->colors[j].green/255.0f;
				m->mColors[0][ix].b = g->colors[j].blue/255.0f;
				m->mColors[0][ix].a = g->colors[j].alpha/255.0f;
			}
		}

		for(int x = 0; x < g->numTexCoordSets; x++) {
			m->mNumUVComponents[x] = 2;
			m->mTextureCoords[x] = new aiVector3D[m->mNumVertices];
			for(j = 0; j < g->numVertices; j++) {
				int ix = imap[j];
				if(ix < 0) continue;
				m->mTextureCoords[x][ix].x = g->texCoords[x][j].u;
				m->mTextureCoords[x][ix].y = 1.0f-g->texCoords[x][j].v;
			}
		}

		t.meshes.push_back(m);
	}

	delete[] imap;
}

static aiNode*
rwFrameToAssimpNode(ImpTemp &t, rw::Frame *f)
{
	using namespace rw;
	aiNode *n;

	n = new aiNode();
	n->mName = gta::getNodeName(f);

	aiMatrix4x4 &mtx = n->mTransformation;
	Matrix *m = &f->matrix;

	mtx.a1 = m->right.x;
	mtx.b1 = m->right.y;
	mtx.c1 = m->right.z;
	mtx.d1 = 0.0f;
	mtx.a2 = m->up.x;
	mtx.b2 = m->up.y;
	mtx.c2 = m->up.z;
	mtx.d2 = 0.0f;
	mtx.a3 = m->at.x;
	mtx.b3 = m->at.y;
	mtx.c3 = m->at.z;
	mtx.d3 = 0.0f;
	mtx.a4 = m->pos.x;
	mtx.b4 = m->pos.y;
	mtx.c4 = m->pos.z;
	mtx.d4 = 1.0f;

	int first = t.meshes.size();
	FORLIST(lnk, f->objectList) {
		Object *obj = (Object*)ObjectWithFrame::fromFrame(lnk);
		if(obj->type != Atomic::ID) continue;
		rwGeometryToAssimpMeshes(t, ((Atomic*)obj)->geometry);
	}
	n->mNumMeshes = t.meshes.size() - first;
	if(n->mNumMeshes) {
		n->mMeshes = new unsigned int[n->mNumMeshes];
		for(unsigned int i = 0; i < n->mNumMeshes; i++)
			n->mMeshes[i] = first + i;
	}

	n->mChildren = new aiNode*[f->count()];
	n->mNumChildren = 0;
        for(Frame *c = f->child; c; c = c->next) {
		aiNode *cn = rwFrameToAssimpNode(t, c);
		cn->mParent = n;
		n->mChildren[n->mNumChildren++] = cn;
	}

	return n;
}

static void
rwClumpToAssimpScene(aiScene *scn, rw::Clump *c)
{
	using namespace rw;
	ImpTemp t;
	scn->mRootNode = rwFrameToAssimpNode(t, c->getFrame());
	scn->mNumMeshes = t.meshes.size();
	scn->mNumMaterials = t.materials.size();
	if(scn->mNumMeshes) {
		scn->mMeshes = new aiMesh*[scn->mNumMeshes];
		memcpy(scn->mMeshes, &t.meshes[0], scn->mNumMeshes*sizeof(aiMesh*));
	}
	if(scn->mNumMaterials) {
		scn->mMaterials = new aiMaterial*[scn->mNumMaterials];
		memcpy(scn->mMaterials, &t.materials[0], scn->mNumMaterials*sizeof(aiMaterial*));
	}
}

void
DFFImporter::InternReadFile(const std::string &pFile, aiScene *pScene, IOSystem *pIOHandler)
{
	using namespace rw;

	rwinit();

	IOStream *stream = pIOHandler->Open(pFile, "rb");
	if(stream == nil)
		throw DeadlyImportError("DFF: Could not open ", pFile);

	uint32 size = stream->FileSize();
	uint8 *data = rwNewT(uint8, size, 0);
	stream->Read(data, 1, size);
	pIOHandler->Close(stream);

        StreamMemory mstr;
        mstr.open(data, size);

	ChunkHeaderInfo header;
	readChunkHeaderInfo(&mstr, &header);
	if(header.type == ID_UVANIMDICT){
		UVAnimDictionary *dict = UVAnimDictionary::streamRead(&mstr);
		currentUVAnimDictionary = dict;
		readChunkHeaderInfo(&mstr, &header);
	}
	if(header.type != ID_CLUMP) {
		mstr.close();
		rwFree(data);
		throw DeadlyImportError("DFF: Could not find clump");
	}
	Clump *c = Clump::streamRead(&mstr);
	mstr.close();
	rwFree(data);
	if(c == nil)
		throw DeadlyImportError("DFF: Could not read clump");

	rwClumpToAssimpScene(pScene, c);

	c->destroy();
}


rw::Clump *assimpSceneToRwClump(const aiScene *scene);

struct HierEntry
{
	rw::Frame *frm;
	aiNode *an;
	aiBone *ab;
	int boneIdx;
	int tag;
};

int
countNodes(aiNode *node)
{
	int n = 1;
	for(unsigned i = 0; i < node->mNumChildren; i++)
		n += countNodes(node->mChildren[i]);
	return n;
}


void
assimpMatrixToRwMatrix(rw::Matrix *m, const aiMatrix4x4 &mtx)
{
	m->right.x = mtx.a1;
	m->right.y = mtx.b1;
	m->right.z = mtx.c1;

	m->up.x = mtx.a2;
	m->up.y = mtx.b2;
	m->up.z = mtx.c2;

	m->at.x = mtx.a3;
	m->at.y = mtx.b3;
	m->at.z = mtx.c3;

	m->pos.x = mtx.a4;
	m->pos.y = mtx.b4;
	m->pos.z = mtx.c4;

	m->optimize();
}

rw::Frame*
buildRwFrameHierarchy(aiNode *root, HierEntry **hp)
{
	rw::Frame *frm = rw::Frame::create();
	// the node name plugin has room for 24 characters
	strncpy(gta::getNodeName(frm), root->mName.C_Str(), 24);

	(*hp)->frm = frm;
	(*hp)->an = root;
	(*hp)->ab = nil;
	(*hp)->boneIdx = -1;
	(*hp)->tag = -1;
	(*hp)++;

	for(unsigned i = 0; i < root->mNumChildren; i++) {
		rw::Frame *child = buildRwFrameHierarchy(root->mChildren[i], hp);
		frm->addChild(child);
	}
	assimpMatrixToRwMatrix(&frm->matrix, root->mTransformation);

	return frm;
}

char*
getTextureName(const char *path)
{
	static char name[24];
	const char *p = strrchr(path, '\\');
	if(p == nil) p = strrchr(path, '/');
	if(p) p++;
	else p = path;

	const char *e = strrchr(p, '.');
	int n = strlen(p);
	if(e && strlen(e+1) <= 4) n = e-p;
	if(n > 24) n = 24;
	strncpy(name, p, n);

	return name;
}

rw::Material*
assimpMaterialToRwMaterial(const aiMaterial *am)
{
	rw::Material *mat = rw::Material::create();

	aiString path;
	for(unsigned j = 0; j < am->GetTextureCount(aiTextureType_DIFFUSE); j++) {
		am->GetTexture(aiTextureType_DIFFUSE, j, &path);
		char *name = getTextureName((char*)path.C_Str());
		mat->texture = rw::Texture::read(name, nil);
	}

	return mat;
}

rw::Atomic*
assimpMeshesToAtomic(const aiScene *scn, aiNode *node, rw::Frame *frm)
{
	int numVerts = 0;
	int numTris = 0;
	int numTexCoordSets = 0;
	int flags = rw::Geometry::POSITIONS | rw::Geometry::LIGHT | rw::Geometry::MODULATE;

	for(unsigned i = 0; i < node->mNumMeshes; i++) {
		aiMesh *m = scn->mMeshes[node->mMeshes[i]];
		numVerts += m->mNumVertices;
		numTris += m->mNumFaces;

		if(m->mPrimitiveTypes != aiPrimitiveType_TRIANGLE) {
			fprintf(stderr, "warning: not triangle mesh, skipping atomic\n");
			return nil;
		}

		if(m->HasNormals()) flags |= rw::Geometry::NORMALS;
		if(m->HasVertexColors(0)) flags |= rw::Geometry::PRELIT;
		int t = m->GetNumUVChannels();
		if(t > numTexCoordSets)
			numTexCoordSets = t;
	}
	if(numTexCoordSets > 8) numTexCoordSets = 8;
	if(numTexCoordSets > 0) flags |= rw::Geometry::TEXTURED;
	if(numTexCoordSets > 1) flags |= rw::Geometry::TEXTURED2;
	flags |= numTexCoordSets << 16;

	if(numVerts == 0 || numTris == 0)
		return nil;

	rw::Geometry *geo = rw::Geometry::create(numVerts, numTris, flags);
	int voff = 0;
	int foff = 0;
	for(unsigned i = 0; i < node->mNumMeshes; i++) {
		aiMesh *m = scn->mMeshes[node->mMeshes[i]];

		rw::Material *mat = assimpMaterialToRwMaterial(scn->mMaterials[m->mMaterialIndex]);
		geo->matList.appendMaterial(mat);
		mat->destroy();

		rw::Triangle *tris = geo->triangles;
		for(unsigned j = 0; j < m->mNumFaces; j++) {
			aiFace *f = &m->mFaces[j];
			tris[foff+j].v[0] = f->mIndices[0] + voff;
			tris[foff+j].v[1] = f->mIndices[1] + voff;
			tris[foff+j].v[2] = f->mIndices[2] + voff;
			tris[foff+j].matId = i;
		}

		rw::V3d *verts = geo->morphTargets[0].vertices;
		for(unsigned j = 0; j < m->mNumVertices; j++) {
			verts[voff+j].x = m->mVertices[j].x;
			verts[voff+j].y = m->mVertices[j].y;
			verts[voff+j].z = m->mVertices[j].z;
		}

		rw::V3d *normals = geo->morphTargets[0].normals;
		if(normals) for(unsigned j = 0; j < m->mNumVertices; j++) {
			normals[voff+j].x = m->mNormals[j].x;
			normals[voff+j].y = m->mNormals[j].y;
			normals[voff+j].z = m->mNormals[j].z;
		}

		rw::RGBA *colors = geo->colors;
		if(colors) for(unsigned j = 0; j < m->mNumVertices; j++) {
			colors[voff+j].red   = m->mColors[0][j].r*255.0f;
			colors[voff+j].green = m->mColors[0][j].g*255.0f;
			colors[voff+j].blue  = m->mColors[0][j].b*255.0f;
			colors[voff+j].alpha = m->mColors[0][j].a*255.0f;
		}

		for(int t = 0; t < numTexCoordSets; t++) {
			rw::TexCoords *tex = geo->texCoords[t];
			for(unsigned j = 0; j < m->mNumVertices; j++) {
				tex[voff+j].u = m->mTextureCoords[t][j].x;
				tex[voff+j].v = 1.0f-m->mTextureCoords[t][j].y;
			}
		}

		voff += m->mNumVertices;
		foff += m->mNumFaces;
	}
	geo->calculateBoundingSphere();
	geo->unlock();

	rw::Atomic *atm = rw::Atomic::create();
	atm->setGeometry(geo, 0);
	atm->setFrame(frm);

	return atm;
}

int
writeAssimpSceneAsDFF(const aiScene *scene, const char *path)
{
	rw::Clump *clp = assimpSceneToRwClump(scene);
	rw::StreamFile out;
	if(!out.open(path, "wb")) {
		clp->destroy();
		return 0;
	}
	clp->streamWrite(&out);
	out.close();
	clp->destroy();
	return 1;
}

rw::Clump*
assimpSceneToRwClump(const aiScene *scene)
{
	rw::Clump *c;

	rwinit();

	c = rw::Clump::create();

	int numNodes = countNodes(scene->mRootNode);
	HierEntry *hier = (HierEntry*)malloc(sizeof(HierEntry) * (numNodes+1));
	HierEntry *hp = hier;
	c->setFrame(buildRwFrameHierarchy(scene->mRootNode, &hp));
	hier[numNodes].frm = nil;
	hier[numNodes].an = nil;

	for(int i = 0; i < numNodes; i++) {
		rw::Frame *frm = hier[i].frm;
		aiNode *an = hier[i].an;
		rw::Atomic *atm = assimpMeshesToAtomic(scene, an, frm);
		if(atm)
			c->addAtomic(atm);
	}

	free(hier);

	return c;
}

}
