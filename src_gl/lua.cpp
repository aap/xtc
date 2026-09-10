#include "xtci.h"
#include "xmodel.h"
#include "app.h"
#include "glad/glad.h"
#include <imgui.h>

#include "conv.h"
#include "assimp_rw.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
}


struct LuaPtr {
	void *p;
	bool luaowned;
};

LuaPtr*
mklptr(lua_State *L, void *p, int owned)
{
	LuaPtr *lp = (LuaPtr*)lua_newuserdata(L, sizeof(LuaPtr));
	lp->p = p;
	lp->luaowned = owned;
	return lp;
}

void*
checklptr(lua_State *L, int n, const char *name)
{
	LuaPtr *lp = (LuaPtr*)luaL_checkudata(L, n, name);
	return lp->p;
}

void
freelptr(lua_State *L, int n, const char *name)
{
	LuaPtr *lp = (LuaPtr*)luaL_checkudata(L, n, name);
	if(lp->luaowned)
		free(lp->p);
}



int
mkptr(lua_State *L, const char *name, void *ptr)
{
	void **p = (void**)lua_newuserdata(L, sizeof(void*));
	*p = ptr;
	luaL_getmetatable(L, name);
	lua_setmetatable(L, -2);
	return 1;
}

void*
checkptr(lua_State *L, int n, const char *name)
{
	if(lua_isnil(L, n))
		return nil;
	return *(void**)luaL_checkudata(L, n, name);
}

void*
checkobj(lua_State *L, int n, char const *name)
{
	return (void*)luaL_checkudata(L, n, name);
}


static Vec2 checkvec2(lua_State *L, int n) { return *(Vec2*)luaL_checkudata(L, n, "Vec2"); }
static int
L_pushvec2(lua_State *L, Vec2 vec)
{
	Vec2 *v = (Vec2*)lua_newuserdata(L, sizeof(Vec2));
	*v = vec;
	luaL_getmetatable(L, "Vec2");
	lua_setmetatable(L, -2);
	return 1;
}

static Vec3 checkvec3(lua_State *L, int n) { return *(Vec3*)luaL_checkudata(L, n, "Vec3"); }
static int
L_pushvec3(lua_State *L, Vec3 vec)
{
	Vec3 *v = (Vec3*)lua_newuserdata(L, sizeof(Vec3));
	*v = vec;
	luaL_getmetatable(L, "Vec3");
	lua_setmetatable(L, -2);
	return 1;
}


static Vec4 checkvec4(lua_State *L, int n) { return *(Vec4*)luaL_checkudata(L, n, "Vec4"); }
static int
L_pushvec4(lua_State *L, Vec4 vec)
{
	Vec4 *v = (Vec4*)lua_newuserdata(L, sizeof(Vec4));
	*v = vec;
	luaL_getmetatable(L, "Vec4");
	lua_setmetatable(L, -2);
	return 1;
}

static Mat4 *checkmat4(lua_State *L, int n) { return (Mat4*)luaL_checkudata(L, n, "Mat4"); }
static int
L_pushmat4(lua_State *L, const Mat4 &mat)
{
	Mat4 *m = (Mat4*)lua_newuserdata(L, sizeof(Mat4));
	*m = mat;
	luaL_getmetatable(L, "Mat4");
	lua_setmetatable(L, -2);
	return 1;
}

static int
L_pushquat(lua_State *L, Quat q)
{
	Quat *p = (Quat*)lua_newuserdata(L, sizeof(Quat));
	*p = q;
	luaL_getmetatable(L, "Quat");
	lua_setmetatable(L, -2);
	return 1;
}

static int
L_pushlptr(lua_State *L, void *p, const char *name)
{
	mklptr(L, p, 0);
	luaL_getmetatable(L, name);
	lua_setmetatable(L, -2);
	return 1;
}



struct StructDesc {
	const char *name;
	u32 offset;
	int type;
};

static int
findslot(lua_State *L, void *v, const char *name, StructDesc *desc)
{
	const char *key;
	if(lua_isnumber(L, 2)) {
		int i = luaL_checkinteger(L, 2)-1;
		if(i < 0) return -3;	// too low
		for(int j = 0; j < i; j++)
			if(desc[j].name == nil)
				return -2;	// too high
		return i;
	}
	key = luaL_checkstring(L, 2);
	for(int i = 0; desc[i].name; i++)
		if(strcmp(key, desc[i].name) == 0)
			return i;
	return -1;	// name not found
}

static int
struct_index(lua_State *L, void *v, const char *name, StructDesc *desc)
{
	int i = findslot(L, v, name, desc);
	if(i >= 0) {
		void *p = (void*)((char*)v + desc[i].offset);
		switch(desc[i].type) {
		case 'i':
			lua_pushinteger(L, *(int*)p);
			break;
		case 'f':
			lua_pushnumber(L, *(float*)p);
			break;
		case 'v2':
			L_pushvec2(L, *(Vec2*)p);
			break;
		case 'v3':
			L_pushvec3(L, *(Vec3*)p);
			break;
		case 'v4':
			L_pushvec4(L, *(Vec4*)p);
			break;
		case 'v4p':
			L_pushlptr(L, p, "Vec4p");
			break;
		}
		return 1;
	} else if(i == -1) {
		lua_getmetatable(L, 1);
		lua_getfield(L, -1, luaL_checkstring(L, 2));
		lua_remove(L, -2);
/*
		lua_getmetatable(L, 1);
		const char *key = luaL_checkstring(L, 2);
		lua_pushstring(L, key);
		lua_rawget(L, -2);
		if(!lua_isnil(L, -1))
			return 1;
		lua_pop(L, 1);
		lua_pushstring(L, "__methods");
		lua_rawget(L, -2);
		if(!lua_isnil(L, -1)) {
			lua_pushstring(L, key);
			lua_rawget(L, -2);
		}
*/
		return 1;
	}
	return 0;
}

static int
struct_newindex(lua_State *L, void *v, const char *name, StructDesc *desc)
{
	int i = findslot(L, v, name, desc);
	if(i < 0)
		return 0;
	void *p = (void*)((char*)v + desc[i].offset);
	switch(desc[i].type) {
	case 'i':
		*(int*)p = luaL_checkinteger(L, 3);
		break;
	case 'f':
		*(float*)p = luaL_checknumber(L, 3);
		break;
	case 'v2':
		*(Vec2*)p = checkvec2(L, 3);
		break;
	case 'v3':
		*(Vec3*)p = checkvec3(L, 3);
		break;
	case 'v4':
	case 'v4p':
		*(Vec4*)p = checkvec4(L, 3);
		break;
	}
	return 1;
}

#define USERTYPE(type, str) \
static int type##__index(lua_State *L) { return struct_index(L, luaL_checkudata(L, 1, str), str, type##_desc); } \
static int type##__newindex(lua_State *L) { return struct_newindex(L, luaL_checkudata(L, 1, str), str, type##_desc); }

#define USERTYPEPTR(type, str) \
static int type##__index(lua_State *L) { return struct_index(L, checklptr(L, 1, str), str, type##_desc); } \
static int type##__newindex(lua_State *L) { return struct_newindex(L, checklptr(L, 1, str), str, type##_desc); } \
static int type##__gc(lua_State *L) { freelptr(L, 1, str); return 0; }

#define USERPTR(type, str) \
static int type##p__index(lua_State *L) { return struct_index(L, checklptr(L, 1, str "p"), str, type##_desc); } \
static int type##p__newindex(lua_State *L) { return struct_newindex(L, checklptr(L, 1, str "p"), str, type##_desc); } \
static int type##p__gc(lua_State *L) { freelptr(L, 1, str "p"); return 0; }

static StructDesc vec2_desc[] = {
	{ "x", offsetof(Vec3, x), 'f' },
	{ "y", offsetof(Vec3, y), 'f' },
	{ nil, 0, 0 }
};
USERTYPE(vec2, "Vec2")
static const luaL_Reg vec2_meta[] = {
	{ "__index", vec2__index },
	{ "__newindex", vec2__newindex },
	{ nil, nil }
};

static int
L_vec2(lua_State *L)
{
	return L_pushvec2(L,
		vec2(luaL_optnumber(L, 1, 0.0f),
			luaL_optnumber(L, 2, 0.0f)));
}



static StructDesc vec3_desc[] = {
	{ "x", offsetof(Vec3, x), 'f' },
	{ "y", offsetof(Vec3, y), 'f' },
	{ "z", offsetof(Vec3, z), 'f' },
	{ nil, 0, 0 }
};
USERTYPE(vec3, "Vec3")
static const luaL_Reg vec3_meta[] = {
	{ "__index", vec3__index },
	{ "__newindex", vec3__newindex },
	{ nil, nil }
};

static int
L_vec3(lua_State *L)
{
	return L_pushvec3(L,
		vec3(luaL_optnumber(L, 1, 0.0f),
			luaL_optnumber(L, 2, 0.0f),
			luaL_optnumber(L, 3, 0.0f)));
}



static StructDesc vec4_desc[] = {
	{ "x", offsetof(Vec4, x), 'f' },
	{ "y", offsetof(Vec4, y), 'f' },
	{ "z", offsetof(Vec4, z), 'f' },
	{ "w", offsetof(Vec4, w), 'f' },
	{ nil, 0, 0 }
};
USERTYPE(vec4, "Vec4")
static const luaL_Reg vec4_meta[] = {
	{ "__index", vec4__index },
	{ "__newindex", vec4__newindex },
	{ nil, nil }
};

USERPTR(vec4, "Vec4")
static const luaL_Reg vec4p_meta[] = {
	{ "__index", vec4p__index },
	{ "__newindex", vec4p__newindex },
	{ "__gc", vec4p__gc },
	{ nil, nil }
};

static int
L_vec4(lua_State *L)
{
	return L_pushvec4(L,
		vec4(luaL_optnumber(L, 1, 0.0f),
			luaL_optnumber(L, 2, 0.0f),
			luaL_optnumber(L, 3, 0.0f),
			luaL_optnumber(L, 4, 0.0f)));
}



static StructDesc mat4_desc[] = {
	{ "x", 0*sizeof(Vec4), 'v4p' },
	{ "y", 1*sizeof(Vec4), 'v4p' },
	{ "z", 2*sizeof(Vec4), 'v4p' },
	{ "w", 3*sizeof(Vec4), 'v4p' },
	{ nil, 0, 0 }
};
USERTYPE(mat4, "Mat4")
static const luaL_Reg mat4_meta[] = {
	{ "__index", mat4__index },
	{ "__newindex", mat4__newindex },
	{ nil, nil }
};

static int
L_mat4(lua_State *L)
{
	return L_pushmat4(L, m4diag(luaL_optnumber(L, 1, 1.0f)));
	return 0;
}


static StructDesc quat_desc[] = {
	{ "x", offsetof(Quat, x), 'f' },
	{ "y", offsetof(Quat, y), 'f' },
	{ "z", offsetof(Quat, z), 'f' },
	{ "w", offsetof(Quat, w), 'f' },
	{ nil, 0, 0 }
};
USERTYPE(quat, "Quat")
static const luaL_Reg quat_meta[] = {
	{ "__index", quat__index },
	{ "__newindex", quat__newindex },
	{ nil, nil }
};

static int
L_quat(lua_State *L)
{
	// Lua side is quat(w, x, y, z)
	return L_pushquat(L,
		quat(luaL_optnumber(L, 2, 0.0f),
			luaL_optnumber(L, 3, 0.0f),
			luaL_optnumber(L, 4, 0.0f),
			luaL_optnumber(L, 1, 0.0f)));
}



void
registerXmath(lua_State *L)
{
	luaL_newmetatable(L, "Vec2");
	luaL_setfuncs(L, vec2_meta, 0);
	lua_pop(L, 1);
	lua_register(L, "vec2", L_vec2);

	luaL_newmetatable(L, "Vec3");
	luaL_setfuncs(L, vec3_meta, 0);
	lua_pop(L, 1);
	lua_register(L, "vec3", L_vec3);

	luaL_newmetatable(L, "Vec4");
	luaL_setfuncs(L, vec4_meta, 0);
	lua_pop(L, 1);
	lua_register(L, "vec4", L_vec4);

	luaL_newmetatable(L, "Vec4p");
	luaL_setfuncs(L, vec4p_meta, 0);
	lua_pop(L, 1);
//	lua_register(L, "vec4p", L_vec4p);

	luaL_newmetatable(L, "Mat4");
	luaL_setfuncs(L, mat4_meta, 0);
	lua_pop(L, 1);
	lua_register(L, "mat4", L_mat4);

	luaL_newmetatable(L, "Quat");
	luaL_setfuncs(L, quat_meta, 0);
	lua_pop(L, 1);
	lua_register(L, "quat", L_quat);
}



static int
L_setTexPath(lua_State *L)
{
	texpath = luaL_checkstring(L, 1);
	return 0;
}

static int
L_xtcEnable(lua_State *L)
{
	xtcEnable((xtceState)luaL_checkinteger(L, 1));
	return 0;
}

static int
L_xtcDisable(lua_State *L)
{
	xtcDisable((xtceState)luaL_checkinteger(L, 1));
	return 0;
}

static int
L_xtcBlendFuncSrcDst(lua_State *L)
{
	xtcBlendFuncSrcDst((xtcBlendFactor)luaL_checkinteger(L, 1),
		(xtcBlendFactor)luaL_checkinteger(L, 2));
	return 0;
}

static int
L_xtcSetProjectionMatrix(lua_State *L)
{
	xtcSetProjectionMatrix(checkmat4(L, 1));
	return 0;
}

static int
L_xtcSetViewMatrix(lua_State *L)
{
	xtcSetViewMatrix(checkmat4(L, 1));
	return 0;
}

static int
L_xtcSetWorldMatrix(lua_State *L)
{
	xtcSetWorldMatrix(checkmat4(L, 1));
	return 0;
}

// xtcSetBoneMatrices({mat4, ...})
static int
L_xtcSetBoneMatrices(lua_State *L)
{
	static Mat4 mats[64];
	luaL_checktype(L, 1, LUA_TTABLE);
	int n = lua_rawlen(L, 1);
	if(n > 64) n = 64;
	for(int i = 0; i < n; i++) {
		lua_rawgeti(L, 1, i+1);
		mats[i] = *checkmat4(L, -1);
		lua_pop(L, 1);
	}
	xtcSetBoneMatrices(mats, n);
	return 0;
}

static int
L_xtcGetWorldMatrix(lua_State *L)
{
	L_pushmat4(L, xtcGetWorldMatrix());
	return 1;
}

static int
L_xtcBegin(lua_State *L)
{
	xtcBegin((xtcPrimType)luaL_checkinteger(L, 1));
	return 0;
}

static int
L_xtcEnd(lua_State *L)
{
	xtcEnd();
	return 0;
}

static int
L_xtcVertex(lua_State *L)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);
	float z = luaL_checknumber(L, 3);
	xtcVertex(x, y, z);
	return 0;
}

static int
L_xtcColor(lua_State *L)
{
	int r = luaL_checkinteger(L, 1);
	int g = luaL_checkinteger(L, 2);
	int b = luaL_checkinteger(L, 3);
	int a = luaL_checkinteger(L, 4);
	xtcColor(r, g, b, a);
	return 0;
}

static int
L_xtcNormal(lua_State *L)
{
	float x = luaL_checknumber(L, 1);
	float y = luaL_checknumber(L, 2);
	float z = luaL_checknumber(L, 3);
	xtcNormal(x, y, z);
	return 0;
}

static int
L_xtcTexCoord(lua_State *L)
{
	float u = luaL_checknumber(L, 1);
	float v = luaL_checknumber(L, 2);
	float q = luaL_optnumber(L, 3, 1.0);
	xtcTexCoord3(u, v, q);
	return 0;
}

static int
L_xtcIndices(lua_State *L)
{
	int i1 = luaL_checkinteger(L, 1);
	int i2 = luaL_checkinteger(L, 2);
	int i3 = luaL_checkinteger(L, 3);
	int i4 = luaL_checkinteger(L, 4);
	xtcIndices(i1, i2, i3, i4);
	return 0;
}

static int
L_xtcWeights(lua_State *L)
{
	float w1 = luaL_checknumber(L, 1);
	float w2 = luaL_checknumber(L, 2);
	float w3 = luaL_checknumber(L, 3);
	float w4 = luaL_checknumber(L, 4);
	xtcWeights(w1, w2, w3, w4);
	return 0;
}

void*
checkvalidptr(lua_State *L, int n, const char *name)
{
	return *(void**)luaL_checkudata(L, n, name);
}

static int
L_xtcSetTexture(lua_State *L)
{
	xtcTexture *tex = (xtcTexture*)checkptr(L, 1, "xtcTexture");
	xtcSetTexture(tex);
	return 0;
}

static int
L_xtcSetTextureN(lua_State *L)
{
	xtcTexture *tex = (xtcTexture*)checkptr(L, 2, "xtcTexture");
	xtcSetTextureN(luaL_checkinteger(L, 1), tex);
	return 0;
}

static int
L_xtcSetPipeline(lua_State *L)
{
	xtcPipeline *pipe = (xtcPipeline*)checkvalidptr(L, 1, "xtcPipeline");
	xtcSetPipeline(pipe);
	return 0;
}

/*
 * Prim lists, same names as the C API.  xtcCreatePrimList() gives a
 * list, xtcStartList/xtcEndList record the begin/end pairs in between
 * into it, xtcPrimListDraw draws it with the current pipeline.
 */

static int
L_xtcCreatePrimList(lua_State *L)
{
	mkptr(L, "xtcPrimList", xtcCreatePrimList());
	return 1;
}

static int
L_xtcStartList(lua_State *L)
{
	xtcPrimList *pl = (xtcPrimList*)checkvalidptr(L, 1, "xtcPrimList");
	xtcStartList(pl);
	return 0;
}

static int
L_xtcEndList(lua_State *L)
{
	xtcEndList();
	return 0;
}

static int
L_xtcPrimListDraw(lua_State *L)
{
	xtcPrimList *pl = (xtcPrimList*)checkvalidptr(L, 1, "xtcPrimList");
	xtcPrimListDraw(pl);
	return 0;
}


static StructDesc xtcLight_desc[] = {
	{ "enabled", offsetof(xtcLight, enabled), 'i' },
	{ "type", offsetof(xtcLight, type), 'i' },
	{ "color", offsetof(xtcLight, color), 'v4' },
	{ "specColor", offsetof(xtcLight, specColor), 'v4' },
	{ "direction", offsetof(xtcLight, direction), 'v3' },
	{ "position", offsetof(xtcLight, position), 'v3' },
	{ nil, 0, 0 }
};
USERTYPEPTR(xtcLight, "xtcLight")
static const luaL_Reg xtcLight__meta[] = {
	{ "__index", xtcLight__index },
	{ "__newindex", xtcLight__newindex },
	{ "__gc", xtcLight__gc },
	{ nil, nil }
};

static int
L_xtcLight(lua_State *L)
{
	mklptr(L, emalloc(sizeof(xtcLight)), 1);
	luaL_getmetatable(L, "xtcLight");
	lua_setmetatable(L, -2);
	return 1;
}

static int
L_xtcSetAmbient(lua_State *L)
{
	xtcSetAmbient(luaL_checknumber(L, 1),
		luaL_checknumber(L, 2),
		luaL_checknumber(L, 3));
	return 0;
}

static int
L_xtcSetLight(lua_State *L)
{
	int n = luaL_checkinteger(L, 1);
	xtcLight *l = (xtcLight*)checklptr(L, 2, "xtcLight");
	xtcSetLight(n, l);
	return 0;
}



static StructDesc xtcStdMaterial_desc[] = {
	{ "ambient", offsetof(xtcStdMaterial, ambient), 'v4' },
	{ "diffuse", offsetof(xtcStdMaterial, diffuse), 'v4' },
	{ "specular", offsetof(xtcStdMaterial, specular), 'v4' },
	{ "emissive", offsetof(xtcStdMaterial, emissive), 'v4' },
	{ nil, 0, 0 }
};
USERTYPEPTR(xtcStdMaterial, "xtcStdMaterial")
static const luaL_Reg xtcStdMaterial__meta[] = {
	{ "__index", xtcStdMaterial__index },
	{ "__newindex", xtcStdMaterial__newindex },
	{ "__gc", xtcStdMaterial__gc },
	{ nil, nil }
};

static int
L_xtcStdMaterial(lua_State *L)
{
	LuaPtr *p = mklptr(L, emalloc(sizeof(xtcStdMaterial)), 1);
	xtcStdMaterial *m = (xtcStdMaterial*)p->p;
	m->ambient = vec4(0.2f, 0.2f, 0.2f, 1.0f);
	m->diffuse = vec4(0.8f, 0.8f, 0.8f, 1.0f);
	m->specular = vec4(0.0f, 0.0f, 0.0f, 0.0f);	// w is the power
	m->emissive = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	luaL_getmetatable(L, "xtcStdMaterial");
	lua_setmetatable(L, -2);
	return 1;
}

static int
L_xtcSetStdMaterial(lua_State *L)
{
	xtcStdMaterial *m = (xtcStdMaterial*)checklptr(L, 1, "xtcStdMaterial");
	xtcSetStdMaterial(m);
	return 0;
}

static int
L_xtcSetColorMaterial(lua_State *L)
{
	xtcSetColorMaterial(luaL_checkinteger(L, 1));
	return 0;
}

// a table of four numbers at idx into v; nothing if it is nil
static void
readVec4(lua_State *L, int idx, Vec4 *v)
{
	float *f = &v->x;
	if(lua_isnoneornil(L, idx))
		return;
	luaL_checktype(L, idx, LUA_TTABLE);
	for(int i = 0; i < 4; i++) {
		lua_rawgeti(L, idx, i+1);
		f[i] = luaL_checknumber(L, -1);
		lua_pop(L, 1);
	}
}

static void
pushVec4(lua_State *L, const Vec4 *v)
{
	const float *f = &v->x;
	lua_createtable(L, 4, 0);
	for(int i = 0; i < 4; i++) {
		lua_pushnumber(L, f[i]);
		lua_rawseti(L, -2, i+1);
	}
}

// xtcSetColorMod(scale, scaleTex, clamp): tables of four, nil keeps
static int
L_xtcSetColorMod(lua_State *L)
{
	xtcColorMod cm;
	xtcGetColorMod(&cm);
	readVec4(L, 1, &cm.scale);
	readVec4(L, 2, &cm.scaleTex);
	readVec4(L, 3, &cm.clamp);
	xtcSetColorMod(&cm);
	return 0;
}

// xtcGetColorMod() -> scale, scaleTex, clamp
static int
L_xtcGetColorMod(lua_State *L)
{
	xtcColorMod cm;
	xtcGetColorMod(&cm);
	pushVec4(L, &cm.scale);
	pushVec4(L, &cm.scaleTex);
	pushVec4(L, &cm.clamp);
	return 3;
}

void
newMetatable(lua_State *L, const char *name, const luaL_Reg *meta)
{
	luaL_newmetatable(L, name);
	if(meta)
		luaL_setfuncs(L, meta, 0);
	else {
		lua_pushvalue(L, -1);
		lua_setfield(L, -2, "__index");
	}
}

void
registerXtc(lua_State *L)
{
	newMetatable(L, "xtcPipeline", nil);
	lua_pop(L, 1);
	lua_register(L, "xtcSetPipeline", L_xtcSetPipeline);
	// the pipelines are globals, like on the PS2
	mkptr(L, "xtcPipeline", defaultPipeline); lua_setglobal(L, "defaultPipeline");
	mkptr(L, "xtcPipeline", skinPipeline); lua_setglobal(L, "skinPipeline");
	mkptr(L, "xtcPipeline", stdPipeline); lua_setglobal(L, "stdPipeline");

	newMetatable(L, "xtcPrimList", nil);
	lua_pop(L, 1);
	lua_register(L, "xtcCreatePrimList", L_xtcCreatePrimList);
	lua_register(L, "xtcStartList", L_xtcStartList);
	lua_register(L, "xtcEndList", L_xtcEndList);
	lua_register(L, "xtcPrimListDraw", L_xtcPrimListDraw);

	newMetatable(L, "xtcTexture", nil);
	lua_pop(L, 1);
	lua_register(L, "xtcSetTexture", L_xtcSetTexture);
	lua_register(L, "xtcSetTextureN", L_xtcSetTextureN);

	newMetatable(L, "xtcStdMaterial", xtcStdMaterial__meta);
	lua_pop(L, 1);
	lua_register(L, "xtcStdMaterial", L_xtcStdMaterial);
	lua_register(L, "xtcSetStdMaterial", L_xtcSetStdMaterial);
	lua_register(L, "xtcSetColorMaterial", L_xtcSetColorMaterial);
	lua_register(L, "xtcSetColorMod", L_xtcSetColorMod);
	lua_register(L, "xtcGetColorMod", L_xtcGetColorMod);

	newMetatable(L, "xtcLight", xtcLight__meta);
	lua_pop(L, 1);
	lua_register(L, "xtcLight", L_xtcLight);
	lua_register(L, "xtcSetAmbient", L_xtcSetAmbient);
	lua_register(L, "xtcSetLight", L_xtcSetLight);

	lua_register(L, "setTexPath", L_setTexPath);
	lua_register(L, "xtcEnable", L_xtcEnable);
	lua_register(L, "xtcDisable", L_xtcDisable);
	lua_register(L, "xtcBlendFuncSrcDst", L_xtcBlendFuncSrcDst);
	lua_register(L, "xtcSetProjectionMatrix", L_xtcSetProjectionMatrix);
	lua_register(L, "xtcSetViewMatrix", L_xtcSetViewMatrix);
	lua_register(L, "xtcSetWorldMatrix", L_xtcSetWorldMatrix);
	lua_register(L, "xtcGetWorldMatrix", L_xtcGetWorldMatrix);
	lua_register(L, "xtcSetBoneMatrices", L_xtcSetBoneMatrices);
	lua_register(L, "xtcBegin", L_xtcBegin);
	lua_register(L, "xtcEnd", L_xtcEnd);
	lua_register(L, "xtcVertex", L_xtcVertex);
	lua_register(L, "xtcColor", L_xtcColor);
	lua_register(L, "xtcNormal", L_xtcNormal);
	lua_register(L, "xtcTexCoord", L_xtcTexCoord);
	lua_register(L, "xtcIndices", L_xtcIndices);
	lua_register(L, "xtcWeights", L_xtcWeights);
}

/*
 * xModel
 */

static xModel*
checkxmodel(lua_State *L, int n)
{
	return (xModel*)checkvalidptr(L, n, "xModel");
}

static xAnimList*
checkxanimlist(lua_State *L, int n)
{
	return (xAnimList*)checkvalidptr(L, n, "xAnimList");
}

static xAnimation*
checkxanim(lua_State *L, int n)
{
	return (xAnimation*)checkvalidptr(L, n, "xAnimation");
}

static xAnimPlayer*
checkxanimplayer(lua_State *L, int n)
{
	return (xAnimPlayer*)checkvalidptr(L, n, "xAnimPlayer");
}

static int
L_loadXModel(lua_State *L)
{
	xModel *mdl = loadXModel(luaL_checkstring(L, 1));
	if(mdl == nil)
		return 0;
	buildXModel(mdl);
	return mkptr(L, "xModel", mdl);
}

static int
L_loadXModelChunk(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	FILE *f = efopen(name, "rb");
	xModel *mdl = loadXModelChunk(f);
	fclose(f);
	if(mdl == nil)
		return 0;
	buildXModel(mdl);
	return mkptr(L, "xModel", mdl);
}

static const aiScene*
importAssimpScene(Assimp::Importer &importer, const char *file)
{
	importer.RegisterLoader(new Assimp::DFFImporter);
	const aiScene *scene = importer.ReadFile(file, aiProcess_Triangulate | aiProcess_PopulateArmatureData);
	if(scene == nil ||
	   scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE ||
	   scene->mRootNode == nil) {
		fprintf(stderr, "can't load %s: %s\n", file, importer.GetErrorString());
		return nil;
	}
	return scene;
}

// importScene(file [, rootMatrix])
// anything assimp can read. returns model and (if there are any) animations.
// rootMatrix is applied on top of the root node, e.g. to convert y-up to z-up
static int
L_importScene(lua_State *L)
{
	const char *file = luaL_checkstring(L, 1);
	Assimp::Importer importer;
	const aiScene *scene = importAssimpScene(importer, file);
	if(scene == nil)
		return 0;
	xModel *mdl = convertAssimpScene(scene);
	if(!lua_isnoneornil(L, 2)) {
		mdl->root->localMatrix = *checkmat4(L, 2) * mdl->root->localMatrix;
		if(mdl->skel) {
			xSkeletonResetMatrices(mdl->skel);
			xSkeletonUpdateMatrices(mdl->skel);
		}
	}
	xAnimList *anims = convertAssimpAnimations(scene, mdl);
	buildXModel(mdl);
	mkptr(L, "xModel", mdl);
	if(anims)
		mkptr(L, "xAnimList", anims);
	else
		lua_pushnil(L);
	return 2;
}

// anything assimp can read -> DFF
static int
L_exportDFF(lua_State *L)
{
	const char *file = luaL_checkstring(L, 1);
	const char *out = luaL_checkstring(L, 2);
	Assimp::Importer importer;
	const aiScene *scene = importAssimpScene(importer, file);
	if(scene == nil)
		return 0;
	lua_pushboolean(L, Assimp::writeAssimpSceneAsDFF(scene, out));
	return 1;
}

static int
L_saveXModel(lua_State *L)
{
	xModel *mdl = checkxmodel(L, 1);
	FILE *f = efopen(luaL_checkstring(L, 2), "w");
	writeXModel(f, mdl);
	fclose(f);
	return 0;
}

static int
L_saveXModelChunk(lua_State *L)
{
	xModel *mdl = checkxmodel(L, 1);
	FILE *f = efopen(luaL_checkstring(L, 2), "wb");
	writeXModelChunk(f, mdl);
	fclose(f);
	return 0;
}

// mdl:draw([flags [, pipeline]])
static int
L_xModelDraw(lua_State *L)
{
	xModel *mdl = checkxmodel(L, 1);
	if(!lua_isnoneornil(L, 3))
		xDrawPipeline = (xtcPipeline*)checkvalidptr(L, 3, "xtcPipeline");
	xModelDraw(mdl, luaL_optinteger(L, 2, 0));
	xDrawPipeline = nil;
	return 0;
}

// mdl:setMaterial(xtcStdMaterial): every mesh gets this material
static int
L_xModelSetMaterial(lua_State *L)
{
	xModel *mdl = checkxmodel(L, 1);
	xtcStdMaterial *m = (xtcStdMaterial*)checklptr(L, 2, "xtcStdMaterial");
	for(int i = 0; i < mdl->numMeshes; i++)
		if(mdl->meshes[i]->material)
			mdl->meshes[i]->material->material = *m;
	return 0;
}

static int
L_xModelHideCarParts(lua_State *L)
{
	xModel *mdl = checkxmodel(L, 1);
	void hideCarParts(xNode *node);
	hideCarParts(mdl->root);
	return 0;
}

static int
L_xModelInfo(lua_State *L)
{
	xModel *mdl = checkxmodel(L, 1);
	int nverts = 0, ntris = 0;
	for(int i = 0; i < mdl->numMeshes; i++) {
		xGeometry *g = mdl->meshes[i]->geo;
		if(g) {
			nverts += g->numVertices;
			ntris += g->numIndices/3;
		}
	}
	lua_newtable(L);
	lua_pushinteger(L, mdl->numMeshes); lua_setfield(L, -2, "numMeshes");
	lua_pushinteger(L, mdl->numMaterials); lua_setfield(L, -2, "numMaterials");
	lua_pushinteger(L, mdl->skel ? mdl->skel->numBones : 0); lua_setfield(L, -2, "numBones");
	lua_pushinteger(L, nverts); lua_setfield(L, -2, "numVertices");
	lua_pushinteger(L, ntris); lua_setfield(L, -2, "numTriangles");
	return 1;
}

// center, radius
static int
L_xModelBounds(lua_State *L)
{
	xModel *mdl = checkxmodel(L, 1);
	Vec3 center;
	float radius;
	xModelBoundingSphere(mdl, &center, &radius);
	L_pushvec3(L, center);
	lua_pushnumber(L, radius);
	return 2;
}

static const luaL_Reg xModel_methods[] = {
	{ "draw", L_xModelDraw },
	{ "setMaterial", L_xModelSetMaterial },
	{ "bounds", L_xModelBounds },
	{ "hideCarParts", L_xModelHideCarParts },
	{ "info", L_xModelInfo },
	{ nil, nil }
};

/*
 * Animations
 */

static int
L_loadXAnimList(lua_State *L)
{
	xAnimList *al = loadXAnimList(luaL_checkstring(L, 1));
	if(al == nil)
		return 0;
	return mkptr(L, "xAnimList", al);
}

static int
L_loadXAnimListChunk(lua_State *L)
{
	FILE *f = efopen(luaL_checkstring(L, 1), "rb");
	xAnimList *al = loadXAnimListChunk(f);
	fclose(f);
	if(al == nil)
		return 0;
	return mkptr(L, "xAnimList", al);
}

static int
L_saveXAnimList(lua_State *L)
{
	xAnimList *al = checkxanimlist(L, 1);
	FILE *f = efopen(luaL_checkstring(L, 2), "w");
	writeXAnimList(f, al);
	fclose(f);
	return 0;
}

static int
L_saveXAnimListChunk(lua_State *L)
{
	xAnimList *al = checkxanimlist(L, 1);
	FILE *f = efopen(luaL_checkstring(L, 2), "wb");
	writeXAnimListChunk(f, al);
	fclose(f);
	return 0;
}

static int
L_xAnimListCount(lua_State *L)
{
	lua_pushinteger(L, checkxanimlist(L, 1)->numAnims);
	return 1;
}

// 1-based
static int
L_xAnimListGet(lua_State *L)
{
	xAnimList *al = checkxanimlist(L, 1);
	int i = luaL_checkinteger(L, 2) - 1;
	if(i < 0 || i >= al->numAnims)
		return 0;
	return mkptr(L, "xAnimation", &al->anims[i]);
}

static int
L_xAnimListNames(lua_State *L)
{
	xAnimList *al = checkxanimlist(L, 1);
	lua_newtable(L);
	for(int i = 0; i < al->numAnims; i++) {
		lua_pushstring(L, al->anims[i].name);
		lua_rawseti(L, -2, i+1);
	}
	return 1;
}

static const luaL_Reg xAnimList_methods[] = {
	{ "count", L_xAnimListCount },
	{ "get", L_xAnimListGet },
	{ "names", L_xAnimListNames },
	{ nil, nil }
};

static int
L_xAnimationName(lua_State *L)
{
	lua_pushstring(L, checkxanim(L, 1)->name);
	return 1;
}

static int
L_xAnimationDuration(lua_State *L)
{
	lua_pushnumber(L, checkxanim(L, 1)->duration);
	return 1;
}

static const luaL_Reg xAnimation_methods[] = {
	{ "name", L_xAnimationName },
	{ "duration", L_xAnimationDuration },
	{ nil, nil }
};

static int
L_xAnimPlayer(lua_State *L)
{
	xModel *mdl = checkxmodel(L, 1);
	return mkptr(L, "xAnimPlayer", xAnimPlayerCreate(mdl));
}

static int
L_xAnimPlayerSetAnim(lua_State *L)
{
	xAnimPlayer *p = checkxanimplayer(L, 1);
	xAnimation *a = lua_isnoneornil(L, 2) ? nil : checkxanim(L, 2);
	xAnimPlayerSetAnim(p, a);
	return 0;
}

static int
L_xAnimPlayerAddTime(lua_State *L)
{
	xAnimPlayerAddTime(checkxanimplayer(L, 1), luaL_checknumber(L, 2));
	return 0;
}

static int
L_xAnimPlayerApply(lua_State *L)
{
	xAnimPlayerApply(checkxanimplayer(L, 1));
	return 0;
}

static int
L_xAnimPlayerGetTime(lua_State *L)
{
	lua_pushnumber(L, checkxanimplayer(L, 1)->time);
	return 1;
}

static int
L_xAnimPlayerSetTime(lua_State *L)
{
	xAnimPlayer *p = checkxanimplayer(L, 1);
	p->time = 0.0f;
	xAnimPlayerAddTime(p, luaL_checknumber(L, 2));
	return 0;
}

static const luaL_Reg xAnimPlayer_methods[] = {
	{ "setAnim", L_xAnimPlayerSetAnim },
	{ "addTime", L_xAnimPlayerAddTime },
	{ "apply", L_xAnimPlayerApply },
	{ "getTime", L_xAnimPlayerGetTime },
	{ "setTime", L_xAnimPlayerSetTime },
	{ nil, nil }
};

static void
newMethodTable(lua_State *L, const char *name, const luaL_Reg *methods)
{
	newMetatable(L, name, nil);
	luaL_setfuncs(L, methods, 0);
	lua_pop(L, 1);
}

void
registerXModel(lua_State *L)
{
	newMethodTable(L, "xModel", xModel_methods);
	newMethodTable(L, "xAnimList", xAnimList_methods);
	newMethodTable(L, "xAnimation", xAnimation_methods);
	newMethodTable(L, "xAnimPlayer", xAnimPlayer_methods);

	lua_register(L, "loadXModel", L_loadXModel);
	lua_register(L, "loadXModelChunk", L_loadXModelChunk);
	lua_register(L, "saveXModel", L_saveXModel);
	lua_register(L, "saveXModelChunk", L_saveXModelChunk);
	lua_register(L, "importScene", L_importScene);
	lua_register(L, "exportDFF", L_exportDFF);

	lua_register(L, "loadXAnimList", L_loadXAnimList);
	lua_register(L, "loadXAnimListChunk", L_loadXAnimListChunk);
	lua_register(L, "saveXAnimList", L_saveXAnimList);
	lua_register(L, "saveXAnimListChunk", L_saveXAnimListChunk);
	lua_register(L, "xAnimPlayer", L_xAnimPlayer);
}

/*
 * ImGui. Just enough for debug panels.
 * Widgets return their (new) value, callers keep the state.
 */

static int
L_imguiBegin(lua_State *L)
{
	lua_pushboolean(L, ImGui::Begin(luaL_checkstring(L, 1)));
	return 1;
}

static int
L_imguiEnd(lua_State *L)
{
	ImGui::End();
	return 0;
}

static int
L_imguiText(lua_State *L)
{
	ImGui::TextUnformatted(luaL_checkstring(L, 1));
	return 0;
}

static int
L_imguiButton(lua_State *L)
{
	lua_pushboolean(L, ImGui::Button(luaL_checkstring(L, 1)));
	return 1;
}

static int
L_imguiCheckbox(lua_State *L)
{
	bool v = lua_toboolean(L, 2);
	ImGui::Checkbox(luaL_checkstring(L, 1), &v);
	lua_pushboolean(L, v);
	return 1;
}

static int
L_imguiSliderFloat(lua_State *L)
{
	float v = luaL_checknumber(L, 2);
	ImGui::SliderFloat(luaL_checkstring(L, 1), &v, luaL_checknumber(L, 3), luaL_checknumber(L, 4));
	lua_pushnumber(L, v);
	return 1;
}

static int
L_imguiSliderInt(lua_State *L)
{
	int v = luaL_checkinteger(L, 2);
	ImGui::SliderInt(luaL_checkstring(L, 1), &v, luaL_checkinteger(L, 3), luaL_checkinteger(L, 4));
	lua_pushinteger(L, v);
	return 1;
}

// imguiCombo(label, index, {items}) -> index, 1-based
static int
L_imguiCombo(lua_State *L)
{
	const char *label = luaL_checkstring(L, 1);
	int cur = luaL_checkinteger(L, 2) - 1;
	luaL_checktype(L, 3, LUA_TTABLE);
	int n = luaL_len(L, 3);
	std::vector<const char*> items(n);
	// the strings stay on the stack until we're done, so make room for them
	luaL_checkstack(L, n, "combo items");
	for(int i = 0; i < n; i++) {
		lua_rawgeti(L, 3, i+1);
		items[i] = lua_tostring(L, -1);
	}
	if(cur < 0) cur = 0;
	if(cur >= n) cur = n-1;
	ImGui::Combo(label, &cur, n ? &items[0] : nil, n, 20);
	lua_pop(L, n);
	lua_pushinteger(L, cur+1);
	return 1;
}

static int
L_imguiSameLine(lua_State *L)
{
	ImGui::SameLine();
	return 0;
}

static int
L_imguiSeparator(lua_State *L)
{
	ImGui::Separator();
	return 0;
}

static void
registerImgui(lua_State *L)
{
	lua_register(L, "imguiBegin", L_imguiBegin);
	lua_register(L, "imguiEnd", L_imguiEnd);
	lua_register(L, "imguiText", L_imguiText);
	lua_register(L, "imguiButton", L_imguiButton);
	lua_register(L, "imguiCheckbox", L_imguiCheckbox);
	lua_register(L, "imguiSliderFloat", L_imguiSliderFloat);
	lua_register(L, "imguiSliderInt", L_imguiSliderInt);
	lua_register(L, "imguiCombo", L_imguiCombo);
	lua_register(L, "imguiSameLine", L_imguiSameLine);
	lua_register(L, "imguiSeparator", L_imguiSeparator);
}

static StructDesc imguiIOdesc[] = {
	{ "DisplaySize", offsetof(ImGuiIO, DisplaySize), 'v2' },
	{ "DeltaTime", offsetof(ImGuiIO, DeltaTime), 'f' },
	{ "Framerate", offsetof(ImGuiIO, Framerate), 'f' },
	{ "MouseDelta", offsetof(ImGuiIO, MouseDelta), 'v2' },
	{ "MousePos", offsetof(ImGuiIO, MousePos), 'v2' },
	{ nil, 0, 0 }
};
static int imguiIO__index(lua_State *L) { return struct_index(L, luaL_checkudata(L, 1, "ImguiIO"), "ImguiIO", imguiIOdesc); }
static const luaL_Reg imguiIO__meta[] = {
	{ "__index", imguiIO__index },
	{ nil, nil }
};
static int
L_imguiIO(lua_State *L)
{
	ImGuiIO *io = (ImGuiIO*)lua_newuserdata(L, sizeof(ImGuiIO));
	memcpy((void*)io, &ImGui::GetIO(), sizeof(ImGuiIO));
	luaL_getmetatable(L, "ImguiIO");
	lua_setmetatable(L, -2);
	return 1;
}

static int
L_getDragMode(lua_State *L)
{
	lua_pushinteger(L, dragging);
	return 1;
}

static int
L_screenshot(lua_State *L)
{
	lua_pushboolean(L, Screenshot(luaL_checkstring(L, 1)));
	return 1;
}

static int
L_quit(lua_State *L)
{
	appShouldQuit = true;
	return 0;
}


lua_State*
initLua(int argc, char **argv)
{
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);
	registerXmath(L);
	registerXtc(L);
	registerXModel(L);
	registerImgui(L);

	luaL_newmetatable(L, "ImguiIO");
	luaL_setfuncs(L, imguiIO__meta, 0);
	lua_pop(L, 1);
	lua_register(L, "imguiIO", L_imguiIO);
	lua_register(L, "getDragMode", L_getDragMode);
	lua_register(L, "screenshot", L_screenshot);
	lua_register(L, "quit", L_quit);

	// command line as the usual arg table, arg[1] is the first argument
	lua_newtable(L);
	for(int i = 0; i < argc; i++) {
		lua_pushstring(L, argv[i]);
		lua_rawseti(L, -2, i);
	}
	lua_setglobal(L, "arg");

	return L;
}
