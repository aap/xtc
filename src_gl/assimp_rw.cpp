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

static bool rwinitialized;
static void
rwinit(void)
{
	if(rwinitialized) return;
        rw::Engine::init();
        gta::attachPlugins();
        rw::Engine::open(nil);
        rw::Engine::start();
	rw::Texture::setCreateDummies(1);
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

static void
ConvertMaterial(ImpTemp &t, rw::Material *m)
{
	aiMaterial *am = new aiMaterial();
	// TODO: don't duplicate materials
	//	look up existing material and return it
	t.materials.push_back(am);

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
}

static void
ConvertGeometry(ImpTemp &t, rw::Geometry *g)
{
	using namespace rw;
	int i, j;

	int first = t.materials.size();
	for(i = 0; i < g->matList.numMaterials; i++)
		ConvertMaterial(t, g->matList.materials[i]);

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
ConvertFrame(ImpTemp &t, rw::Frame *f)
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
		ConvertGeometry(t, ((Atomic*)obj)->geometry);
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
		aiNode *cn = ConvertFrame(t, c);
		cn->mParent = n;
		n->mChildren[n->mNumChildren++] = cn;
	}

	return n;
}

static void
ConvertClump(aiScene *scn, rw::Clump *c)
{
	using namespace rw;
	ImpTemp t;
	scn->mRootNode = ConvertFrame(t, c->getFrame());
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

	ConvertClump(pScene, c);

	c->destroy();
}

}
