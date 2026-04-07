#include "xtc.h"
#include "xmodel.h"
#include "app.h"
#include "glad/glad.h"

#include "imgui.h"

#include "camera.h"

#include <GL/gl.h>
#include <assimp/Importer.hpp>
#include <assimp/importerdesc.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Exporter.hpp>
#include <glm/ext/vector_float3.hpp>

#include "assimp_rw.h"

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}


xModel *convertAssimpScene(const aiScene *scene);
xAnimList *convertAssimpAnimations(const aiScene *scene, xModel *mdl);

CCamera camera;


void
InitGL(void *loadproc)
{
	gladLoadGLLoader((GLADloadproc)loadproc);
}

lua_State *initLua(void);
lua_State *lua;

void
InitApp(void)
{
	xtcInit();
	lua = initLua();
}

xtcMaterial material, material2;


void
DrawAxes(float scale = 1.0f)
{
	xtcSetShader(xtcGetDefaultShader());
	xtcSetTexture(0, nil);
	xtcSetMaterial(&material);

	xtcBegin(XTC_LINELIST);
		xtcColor(255, 0, 0, 255);
		xtcVertex3(0.0f, 0.0f, 0.0f);
		xtcVertex3(scale, 0.0f, 0.0f);

		xtcColor(0, 255, 0, 255);
		xtcVertex3(0.0f, 0.0f, 0.0f);
		xtcVertex3(0.0f, scale, 0.0f);

		xtcColor(0, 0, 255, 255);
		xtcVertex3(0.0f, 0.0f, 0.0f);
		xtcVertex3(0.0f, 0.0f, scale);
	xtcEnd();
}

void
DrawCube(void)
{
	struct Vertex {
		float pos[3];
		unsigned char color[4];
		float normal[3];
		float uv[2];
	};
	static Vertex vertices[] = {
		{ { -1.0f, -1.0f, -1.0f }, {   0,   0,   0, 255 }, { 0.0f, 0.0f, 0.0f } },
		{ { -1.0f, -1.0f,  1.0f }, {   0,   0, 255, 255 }, { 0.0f, 0.0f, 0.0f } },
		{ { -1.0f,  1.0f, -1.0f }, {   0, 255,   0, 255 }, { 0.0f, 0.0f, 0.0f } },
		{ { -1.0f,  1.0f,  1.0f }, {   0, 255, 255, 255 }, { 0.0f, 0.0f, 0.0f } },
		{ {  1.0f, -1.0f, -1.0f }, { 255,   0,   0, 255 }, { 0.0f, 0.0f, 0.0f } },
		{ {  1.0f, -1.0f,  1.0f }, { 255,   0, 255, 255 }, { 0.0f, 0.0f, 0.0f } },
		{ {  1.0f,  1.0f, -1.0f }, { 255, 255,   0, 255 }, { 0.0f, 0.0f, 0.0f } },
		{ {  1.0f,  1.0f,  1.0f }, { 255, 255, 255, 255 }, { 0.0f, 0.0f, 0.0f } },
	};
	static u16 indices[] = {
		0, 1, 2,
		2, 1, 3,

		4, 6, 5,
		5, 6, 7,

		5, 7, 1,
		1, 7, 3,

		0, 2, 4,
		4, 2, 6,

		7, 6, 3,
		3, 6, 2,

		1, 0, 5,
		5, 0, 4
	};
	static int initialized = 0;

	if(!initialized) {
		initialized = 1;
		for(u32 i = 0; i < nelem(vertices); i++) {
			float n = 1.0f/sqrt(sq(vertices[i].pos[0]) + sq(vertices[i].pos[1]) + sq(vertices[i].pos[2]));
			vertices[i].normal[0] = vertices[i].pos[0]*n;
			vertices[i].normal[1] = vertices[i].pos[1]*n;
			vertices[i].normal[2] = vertices[i].pos[2]*n;
		}
	}

	xtcSetShader(xtcGetDefaultShader());
	xtcSetTexture(0, nil);
	xtcSetMaterial(&material);

	xtcBegin(XTC_TRILIST);
	for(u32 i = 0; i < nelem(indices); i++) {
		int ix = indices[i];
		xtcColor(vertices[ix].color[0], vertices[ix].color[1], vertices[ix].color[2], vertices[ix].color[3]);
		xtcVertex3(vertices[ix].pos[0], vertices[ix].pos[1], vertices[ix].pos[2]);
	}
	xtcEnd();
}

xtcPrimList*
CreateCube(void)
{
	xtcPrimList *pl = xtcCreatePrimList();
	xtcStartList(pl);
	DrawCube();
	xtcEndList();

	return pl;
}


xtcMaterial
DefaultMaterial(void)
{
	xtcMaterial mat;
	mat.colorSelector = vec4(0.0f);
	mat.ambient = vec4(0.2f, 0.2f, 0.2f, 1.0f);
	mat.diffuse = vec4(0.8f, 0.8f, 0.8f, 1.0f);
	mat.specular = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	mat.emissive = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	mat.shininess = 0.0f;
	return mat;
}


int
readfile(const char *path, u8 **data, u32 *size)
{
	FILE *f;
	f = fopen(path, "rb");
	if(f == nil)
		return 0;
	fseek(f, 0, 2);
	*size = ftell(f);
	*data = (u8*)malloc(*size);
	fseek(f, 0, 0);
	fread(*data, 1, *size, f);
	fclose(f);
	return 1;
}

xtcPrimList *cube;



const char *texpath = ".";
int findBone(xSkeleton *skel, const char *str);

void
initXAnimPlayer(xAnimPlayer *p, xAnimation *a, xSkeleton *s)
{
	p->time = 0.0f;
	p->anim = a;
	p->skel = s;
	p->matrices = (mat4**)malloc(a->numChannels * sizeof(mat4*));
	memset(p->matrices, 0, a->numChannels * sizeof(mat4*));
	for(int i = 0; i < a->numChannels; i++) {
		xAnimChannel *ch = &a->channels[i];
		// TODO: could also do lookup on id here
		int bi = findBone(s, ch->name);
		assert(bi >= 0 && bi < s->numBones);
		p->matrices[i] = &s->matrices[bi];
	}
}

void
xAnimPlayerAddTime(xAnimPlayer *p, float t)
{
	p->time += t;
	while(p->time > p->anim->duration) p->time -= p->anim->duration;
}

void
applyKeyFrame(mat4 *mp, int type, xAnimChannel::Key *k)
{
	mat4 m = mat4(1.0f);
	// TODO: think about order, but roughtly correct
	if(type & 2) m = glm::translate(m, k->trans);
	if(type & 1) m *= mat4(k->rot);
	if(type & 4) m = glm::scale(m, k->scale);
	*mp = m;
}

void
xAnimPlayerApply(xAnimPlayer *p)
{
	xAnimation *a = p->anim;
//	for(int i = 0; i < p->skel->numBones; i++)
//		p->skel->matrices[i] = mat4(1.0f);
	for(int i = 0; i < a->numChannels; i++) {
		xAnimChannel *c = &a->channels[i];
		for(int j = 0; j < c->numKeys-1; j++)
			if(p->time < c->keys[j+1].time) {
				applyKeyFrame(p->matrices[i], c->typemask, &c->keys[j]);
				break;
			}
	}
}

void
xSkeletonUpdateMatrices(xSkeleton *s)
{
	int stack[64];
	int sp = 0;
	int parent = 0;
	for(int i = 1; i < s->numBones; i++) {
		xBone *b = &s->bones[i];
		s->matrices[i] = s->matrices[parent] * s->matrices[i];
		if(b->flag & 1) stack[sp++] = parent;
		parent = i;
		if(b->flag & 2) parent = stack[--sp];
	}
}

xModel *model;
xAnimList *anims;
xAnimPlayer *animPlayer;
mat4 worldMat;
xtcLight light;

extern "C" int tokenize(char *s, char **args, int maxargs);

FILE*
efopen(const char *path, const char *mode)
{
	FILE *f = fopen(path, mode);
	if(f == nil) {
		fprintf(stderr, "couldn't open file <%s>\n", path);
		exit(1);
	}
	return f;
}

void*
emalloc(size_t sz)
{
	void *p;
	p = malloc(sz);
	assert(p);
	memset(p, 0, sz);
	return p;
}

void
hideCarParts(xNode *node)
{
	if(strstr(node->name, "_dam") ||
	   strstr(node->name, "_vlo"))
		node->hidden = true;
	for(xNode *child = node->child; child; child = child->next)
		hideCarParts(child);
}
void
LoadAssets(void)
{
	mat4 rotYup = glm::rotate(mat4(1.0f), PI/2.0f, vec3(1.0f, 0.0f, 0.0f));
	texpath = "/u/aap/3dmodels/gta3_textures";
//	texpath = "/u/aap";
	FILE *f;

#if 0
	Assimp::Importer importer;
	importer.RegisterLoader(new Assimp::DFFImporter);
#define HOME "/u/aap/"
#define GTA3_MODELS HOME "n/indra/platte/spiele/rockstargames/pc/gta3/models/gta3_archive/"

//	const char *file = HOME "3dmodels/vampire/dancing_vampire.dae";
//	const char *file = GTA3_MODELS "briefcase.dff";
//	const char *file = GTA3_MODELS "playerx.DFF";
	const char *file = GTA3_MODELS "kuruma.dff";
//	const char *file = GTA3_MODELS "building_fucked.dff";
	const aiScene *scene = importer.ReadFile(file, aiProcess_Triangulate | aiProcess_PopulateArmatureData);
	if(scene == nil ||
	   scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
	   scene->mRootNode == nil) {
		printf("can't load file\n");
		exit(1);
	}
//	Assimp::Exporter exporter;
//	exporter.Export(scene, "assjson", "xxx.json");


	model = convertAssimpScene(scene);
	//anims = convertAssimpAnimations(scene, model);
	hideCarParts(model->root);
	//model->root->localMatrix *= rotYup;
#endif

	if(1) {
		f = efopen("model.xm", "r");
		model = loadXModel(f);
		fclose(f);
		f = fopen("anim.xan", "r");
		if(f) {
			anims = loadXAnimList(f);
			fclose(f);
		}
	}

	if(0) {
		f = efopen("model.xm.chk", "rb");
		model = loadXModelChunk(f);
		fclose(f);
		f = fopen("anim.xan.chk", "rb");
		if(f) {
			anims = loadXAnimListChunk(f);
			fclose(f);
		}
	}

	if(1) {
		f = efopen("model_out.xm", "w");
		writeXModel(f, model);
		fclose(f);
		if(anims) {
			f = efopen("anim_out.xan", "w");
			writeXAnimList(f, anims);
			fclose(f);
		}
	}

	if(1) {
		f = efopen("model_out.xm.chk", "wb");
		writeXModelChunk(f, model);
		fclose(f);
		if(anims) {
			f = efopen("anim_out.xan.chk", "wb");
			writeXAnimListChunk(f, anims);
			fclose(f);
		}
	}

	if(anims) {
		animPlayer = (xAnimPlayer*)malloc(sizeof(xAnimPlayer));
		initXAnimPlayer(animPlayer, &anims->anims[0], model->skel);
	}
//exit(0);
}

void
InitScene(void)
{
	xtcSetAmbient(100, 100, 100);
	light.enabled = 1;
	light.type = XTC_LIGHT_DIRECT;
	light.color = vec4(0.8f, 0.8f, 0.8f, 1.0f);
	light.specColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);
	light.direction = normalize(vec3(-1.0f, 1.0f, -1.0f));
	xtcSetLight(0, &light);

	cube = CreateCube();
	material = DefaultMaterial();
	material.colorSelector = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	material.ambient = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	material.diffuse = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	material2 = DefaultMaterial();

//	camera.m_position = vec3(4.0f, 8.0f, 4.0f)*5.0f;
	camera.m_position = vec3(4.0f, -6.0f, 4.0f)*0.7f;
//	camera.m_position = vec3(4.0f, 8.0f, 4.0f)*0.3f;
	camera.m_target = vec3(0.0f, 0.0f, 0.0f);

	mat4 rotYup = glm::rotate(mat4(1.0f), PI/2.0f, vec3(1.0f, 0.0f, 0.0f));
//	worldMat = mat4(1.0f);
//	worldMat = glm::rotate(worldMat, PI/2.0f, vec3(1.0f, 0.0f, 0.0f));
//	texpath = "/u/aap/3dmodels/jap/textures";
	texpath = "/u/aap/3dmodels/gta3_textures";
//	texpath = "/u/aap";
	FILE *f;

	if(model == nil) {
		LoadAssets();
		buildXModel(model);
	}

/*
	const char *code =
		"setTexPath('/u/aap/3dmodels/vampire/')\n"
		"loadXModel('vampire.xm')\n";
//		"print('result: ' .. myadd(10,35))\n";
	if(luaL_dostring(lua, code) != LUA_OK) {
		fprintf(stderr, "error: %s\n", lua_tostring(lua, -1));
		lua_close(lua);
		lua = nil;
	}
*/
	if(luaL_dofile(lua, "init.lua") != LUA_OK) {
		fprintf(stderr, "error: %s\n", lua_tostring(lua, -1));
		lua_pop(lua, 1);
		exit(1);
	}
	if(luaL_dostring(lua, "init()\n") != LUA_OK) {
		fprintf(stderr, "error: %s\n", lua_tostring(lua, -1));
		lua_pop(lua, 1);
		exit(1);
	}
}


void
drawObj(float (*verts)[3], float (*tex)[2], float (*normals)[3], int (*faces)[3][3], int nfaces)
{
	float s = 0.1f;
	xtcBegin(XTC_TRILIST);
		for(int i = 0; i < nfaces; i++) {
			for(int v = 0; v < 3; v++) {
				int vx = faces[i][v][0]-1;
				int vt = faces[i][v][1]-1;
				int vn = faces[i][v][2]-1;
	xtcColor(255, 255, 255, 255);
//                              xtcVertex3(s*verts[vx][0], s*verts[vx][1], s*verts[vx][2]);
				xtcNormal(normals[vx][0], -normals[vx][2], normals[vx][1]);
				xtcVertex3(s*verts[vx][0], -s*verts[vx][2], s*verts[vx][1]);
			}
		}
	xtcEnd();
}

#include "teapot.inc"

float timeNow, timeStep;

void
RenderScene(void)
{
	if(luaL_dostring(lua, "draw()\n") != LUA_OK) {
		fprintf(stderr, "error: %s\n", lua_tostring(lua, -1));
		lua_pop(lua, 1);
		exit(1);
	}

return;
	camera.m_aspectRatio = (float)display_w/display_h;
	camera.m_fov = 70.0f;
	camera.Process();
	camera.update();
/*
float *m = (float*)&camera.m_projMat;
printf("mat:\n");
printf("%f %f %f %f\n"
       "%f %f %f %f\n"
       "%f %f %f %f\n"
       "%f %f %f %f\n",
	m[0], m[4], m[8], m[12],
	m[1], m[5], m[9], m[13],
	m[2], m[6], m[10], m[14],
	m[3], m[7], m[11], m[15]);
*/

	if(animPlayer) {
		xAnimPlayerAddTime(animPlayer, 20.0f);
		xAnimPlayerApply(animPlayer);
		xSkeletonUpdateMatrices(animPlayer->skel);
	}

	xtcSetProjectionMatrix(&camera.m_projMat);
	xtcSetViewMatrix(&camera.m_viewMat);
	xtcSetWorldMatrix(mat4(1.0f));

	xtcEnable(XTC_DEPTH_TEST);

	xtcSetTexture(0, nil);
	xtcSetMaterial(&material);

//	xtcPrimListDraw(cube);
//	DrawCube();

	DrawAxes();

	xtcBegin(XTC_LINELIST);
		xtcColor(255, 255, 255, 255);
		xtcVertex3(0.0f, 0.0f, 0.0f);
		xtcVertex3(-light.direction.x*3, -light.direction.y*3, -light.direction.z*3);
	xtcEnd();
/*
	xtcBegin(XTC_LINESTRIP);
		xtcColor(255, 255, 255, 255);
		float x, y, z;
		for(int i = 0; i < 100; i++) {
			x = 0.05f*i;
			y = cosf(i*0.1f + timeNow);
			z = sinf(i*0.1f + timeNow);
			xtcVertex3(x, y, z);
		}
	xtcEnd();
*/
//	drawObj(teapot_verts, teapot_tex, teapot_normals, teapot_faces, nelem(teapot_faces));

	xtcEnable(XTC_BLEND);
	xtcBlendFuncSrcDst(XTC_BLEND_SRCALPHA, XTC_BLEND_INVSRCALPHA);
	//xModelDraw(model, Dbg_DrawSkeleton | Dbg_DrawNodes);
	xModelDraw(model, 0);
}


int dragging;
bool startDragging;
bool stopDragging;
bool dragCtrl;          
bool dragShift;         
bool dragAlt;
vec2 dragStart, dragEnd, dragDelta;

void
DragStuff(void)
{
	ImGuiIO &io = ImGui::GetIO();
	if(stopDragging)
		dragging = 0;
	startDragging = false;
	stopDragging = false;
	if(!dragging) {
//              if(!io.WantCaptureMouse && !io.WantCaptureKeyboard && ModNone()) {
		if(!io.WantCaptureMouse && !io.WantCaptureKeyboard) {
			if(ImGui::IsMouseDragging(0, 0.0f))
				dragging = 1;
			else if(ImGui::IsMouseDragging(1, 0.0f))
				dragging = 2;
			else if(ImGui::IsMouseDragging(2, 0.0f))
				dragging = 3;
		}               
		if(dragging) {
			dragCtrl = io.KeyCtrl;
			dragShift = io.KeyShift;
			dragAlt = io.KeyAlt;
			dragStart = ImGui::GetMousePos();
			dragEnd = ImGui::GetMousePos(); 
			startDragging = true;
//printf("start dragging %d   %d %d %d\n", dragging, dragCtrl, dragShift, dragAlt);
		}                       
	} else {                
		dragDelta = vec2(ImGui::GetMousePos()) - dragEnd;
		dragEnd = ImGui::GetMousePos();
		if(!ImGui::IsMouseDragging(dragging-1, 0.0f)) {
			stopDragging = true;
//printf("stop dragging\n");
		}
	}       
}

void
GUI(void)
{
	DragStuff();
}
