#include "xtc.h"
#include "xmodel.h"
#include "app.h"
#include "glad/glad.h"
#include <imgui.h>

#include "camera.h"

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


static vec2 checkvec2(lua_State *L, int n) { return *(vec2*)luaL_checkudata(L, n, "Vec2"); }
static int
L_pushvec2(lua_State *L, vec2 vec)
{
	vec2 *v = (vec2*)lua_newuserdata(L, sizeof(vec2));
	*v = vec;
	luaL_getmetatable(L, "Vec2");
	lua_setmetatable(L, -2);
	return 1;
}

static vec3 checkvec3(lua_State *L, int n) { return *(vec3*)luaL_checkudata(L, n, "Vec3"); }
static int
L_pushvec3(lua_State *L, vec3 vec)
{
	vec3 *v = (vec3*)lua_newuserdata(L, sizeof(vec3));
	*v = vec;
	luaL_getmetatable(L, "Vec3");
	lua_setmetatable(L, -2);
	return 1;
}


static vec4 checkvec4(lua_State *L, int n) { return *(vec4*)luaL_checkudata(L, n, "Vec4"); }
static int
L_pushvec4(lua_State *L, vec4 vec)
{
	vec4 *v = (vec4*)lua_newuserdata(L, sizeof(vec4));
	*v = vec;
	luaL_getmetatable(L, "Vec4");
	lua_setmetatable(L, -2);
	return 1;
}

static mat4 *checkmat4(lua_State *L, int n) { return (mat4*)luaL_checkudata(L, n, "Mat4"); }
static int
L_pushmat4(lua_State *L, const mat4 &mat)
{
	mat4 *m = (mat4*)lua_newuserdata(L, sizeof(mat4));
	*m = mat;
	luaL_getmetatable(L, "Mat4");
	lua_setmetatable(L, -2);
	return 1;
}

static vec4 checkquat(lua_State *L, int n) { return *(vec4*)luaL_checkudata(L, n, "Quat"); }
static int
L_pushquat(lua_State *L, quat q)
{
	quat *p = (quat*)lua_newuserdata(L, sizeof(quat));
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
			L_pushvec2(L, *(vec2*)p);
			break;
		case 'v3':
			L_pushvec3(L, *(vec3*)p);
			break;
		case 'v4':
			L_pushvec4(L, *(vec4*)p);
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
		*(vec2*)p = checkvec2(L, 3);
		break;
	case 'v3':
		*(vec3*)p = checkvec3(L, 3);
		break;
	case 'v4':
	case 'v4p':
		*(vec4*)p = checkvec4(L, 3);
		break;
	}
	return 1;
}


static StructDesc vec2desc[] = {
	{ "x", offsetof(vec3, x), 'f' },
	{ "y", offsetof(vec3, y), 'f' },
	{ nil, 0, 0 }
};
static int vec2_index(lua_State *L) { return struct_index(L, luaL_checkudata(L, 1, "Vec2"), "Vec2", vec2desc); }
static int vec2_newindex(lua_State *L) { return struct_newindex(L, luaL_checkudata(L, 1, "Vec2"), "Vec2", vec2desc); }
static const luaL_Reg vec2_meta[] = {
	{ "__index", vec2_index },
	{ "__newindex", vec2_newindex },
	{ nil, nil }
};

static int
L_vec2(lua_State *L)
{
	return L_pushvec2(L,
		vec2(luaL_optnumber(L, 1, 0.0f),
			luaL_optnumber(L, 2, 0.0f)));
}



static StructDesc vec3desc[] = {
	{ "x", offsetof(vec3, x), 'f' },
	{ "y", offsetof(vec3, y), 'f' },
	{ "z", offsetof(vec3, z), 'f' },
	{ nil, 0, 0 }
};
static int vec3_index(lua_State *L) { return struct_index(L, luaL_checkudata(L, 1, "Vec3"), "Vec3", vec3desc); }
static int vec3_newindex(lua_State *L) { return struct_newindex(L, luaL_checkudata(L, 1, "Vec3"), "Vec3", vec3desc); }
static const luaL_Reg vec3_meta[] = {
	{ "__index", vec3_index },
	{ "__newindex", vec3_newindex },
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



static StructDesc vec4desc[] = {
	{ "x", offsetof(vec4, x), 'f' },
	{ "y", offsetof(vec4, y), 'f' },
	{ "z", offsetof(vec4, z), 'f' },
	{ "w", offsetof(vec4, w), 'f' },
	{ nil, 0, 0 }
};
static int vec4_index(lua_State *L) { return struct_index(L, luaL_checkudata(L, 1, "Vec4"), "Vec4", vec4desc); }
static int vec4_newindex(lua_State *L) { return struct_newindex(L, luaL_checkudata(L, 1, "Vec4"), "Vec4", vec4desc); }
static const luaL_Reg vec4_meta[] = {
	{ "__index", vec4_index },
	{ "__newindex", vec4_newindex },
	{ nil, nil }
};

static int vec4p_index(lua_State *L) { return struct_index(L, checklptr(L, 1, "Vec4p"), "Vec4", vec4desc); }
static int vec4p_newindex(lua_State *L) { return struct_newindex(L, checklptr(L, 1, "Vec4p"), "Vec4", vec4desc); }
static int vec4p_gc(lua_State *L) { freelptr(L, 1, "Vec4p"); return 0; }
static const luaL_Reg vec4p_meta[] = {
	{ "__index", vec4p_index },
	{ "__newindex", vec4p_newindex },
	{ "__gc", vec4p_gc },
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



static StructDesc mat4desc[] = {
	{ "x", 0*sizeof(vec4), 'v4p' },
	{ "y", 1*sizeof(vec4), 'v4p' },
	{ "z", 2*sizeof(vec4), 'v4p' },
	{ "w", 3*sizeof(vec4), 'v4p' },
	{ nil, 0, 0 }
};
static int mat4_index(lua_State *L) { return struct_index(L, luaL_checkudata(L, 1, "Mat4"), "Mat4", mat4desc); }
static int mat4_newindex(lua_State *L) { return struct_newindex(L, luaL_checkudata(L, 1, "Mat4"), "Mat4", mat4desc); }
static const luaL_Reg mat4_meta[] = {
	{ "__index", mat4_index },
	{ "__newindex", mat4_newindex },
	{ nil, nil }
};

static int
L_mat4(lua_State *L)
{
	return L_pushmat4(L, mat4(luaL_optnumber(L, 1, 1.0f)));
	return 0;
}


static StructDesc quatdesc[] = {
	{ "x", offsetof(glm::quat, x), 'f' },
	{ "y", offsetof(glm::quat, y), 'f' },
	{ "z", offsetof(glm::quat, z), 'f' },
	{ "w", offsetof(glm::quat, w), 'f' },
	{ nil, 0, 0 }
};
static int quat_index(lua_State *L) { return struct_index(L, luaL_checkudata(L, 1, "Quat"), "Quat", quatdesc); }
static int quat_newindex(lua_State *L) { return struct_newindex(L, luaL_checkudata(L, 1, "Quat"), "Quat", quatdesc); }
static const luaL_Reg quat_meta[] = {
	{ "__index", quat_index },
	{ "__newindex", quat_newindex },
	{ nil, nil }
};

static int
L_quat(lua_State *L)
{
	return L_pushquat(L,
		quat(luaL_optnumber(L, 1, 0.0f),
			luaL_optnumber(L, 2, 0.0f),
			luaL_optnumber(L, 3, 0.0f),
			luaL_optnumber(L, 4, 0.0f)));
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
	xtcTexCoord(u, v);
	return 0;
}

static int
L_xtcIndices(lua_State *L)
{
	int i1 = luaL_checkinteger(L, 1);
	int i2 = luaL_checkinteger(L, 2);
	int i3 = luaL_checkinteger(L, 2);
	int i4 = luaL_checkinteger(L, 2);
	xtcIndices(i1, i2, i3, i4);
	return 0;
}

static int
L_xtcWeights(lua_State *L)
{
	float w1 = luaL_checknumber(L, 1);
	float w2 = luaL_checknumber(L, 2);
	float w3 = luaL_checknumber(L, 2);
	float w4 = luaL_checknumber(L, 2);
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
	xtcTexture *tex = (xtcTexture*)checkptr(L, 2, "xtcTexture");
	xtcSetTexture(luaL_checkinteger(L, 1), tex);
	return 0;
}

static int
L_xtcSetShader(lua_State *L)
{
	xtcShader *sh = (xtcShader*)checkvalidptr(L, 1, "xtcShader");
	xtcSetShader(sh);
	return 0;
}

static int
L_xtcGetDefaultShader(lua_State *L)
{
	mkptr(L, "xtcShader", xtcGetDefaultShader());
	return 1;
}


static StructDesc lightdesc[] = {
	{ "enabled", offsetof(xtcLight, enabled), 'i' },
	{ "type", offsetof(xtcLight, type), 'i' },
	{ "color", offsetof(xtcLight, color), 'v4' },
	{ "specColor", offsetof(xtcLight, specColor), 'v4' },
	{ "direction", offsetof(xtcLight, direction), 'v3' },
	{ "position", offsetof(xtcLight, position), 'v3' },
	{ nil, 0, 0 }
};
static int xtcLight__index(lua_State *L) { return struct_index(L, checklptr(L, 1, "xtcLight"), "xtcLight", lightdesc); }
static int xtcLight__newindex(lua_State *L) { return struct_newindex(L, checklptr(L, 1, "xtcLight"), "xtcLight", lightdesc); }
static int xtcLight__gc(lua_State *L) { freelptr(L, 1, "xtcLight"); return 0; }
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
	xtcSetAmbient(luaL_checkinteger(L, 1),
		luaL_checkinteger(L, 2),
		luaL_checkinteger(L, 3));
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



static StructDesc materialdesc[] = {
	{ "colorSelector", offsetof(xtcMaterial, colorSelector), 'v4' },
	{ "ambient", offsetof(xtcMaterial, ambient), 'v4' },
	{ "diffuse", offsetof(xtcMaterial, diffuse), 'v4' },
	{ "specular", offsetof(xtcMaterial, specular), 'v4' },
	{ "emissive", offsetof(xtcMaterial, emissive), 'v4' },
	{ "shininess", offsetof(xtcMaterial, shininess), 'f' },
	{ nil, 0, 0 }
};
static int xtcMaterial__index(lua_State *L) { return struct_index(L, checklptr(L, 1, "xtcMaterial"), "xtcMaterial", materialdesc); }
static int xtcMaterial__newindex(lua_State *L) { return struct_newindex(L, checklptr(L, 1, "xtcMaterial"), "xtcMaterial", materialdesc); }
static int xtcMaterial__gc(lua_State *L) { freelptr(L, 1, "xtcMaterial"); return 0; }
static const luaL_Reg xtcMaterial__meta[] = {
	{ "__index", xtcMaterial__index },
	{ "__newindex", xtcMaterial__newindex },
	{ "__gc", xtcMaterial__gc },
	{ nil, nil }
};

static int
L_xtcMaterial(lua_State *L)
{
	LuaPtr *p = mklptr(L, emalloc(sizeof(xtcMaterial)), 1);
	xtcMaterial *m = (xtcMaterial*)p->p;
	m->colorSelector = vec4(0.0f);
	m->ambient = vec4(0.2f, 0.2f, 0.2f, 1.0f);
	m->diffuse = vec4(0.8f, 0.8f, 0.8f, 1.0f);
	m->specular = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	m->emissive = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	m->shininess = 0.0f;
	luaL_getmetatable(L, "xtcMaterial");
	lua_setmetatable(L, -2);
	return 1;
}

static int
L_xtcSetMaterial(lua_State *L)
{
	xtcMaterial *m = (xtcMaterial*)checklptr(L, 1, "xtcMaterial");
	xtcSetMaterial(m);
	return 0;
}


void
registerXtc(lua_State *L)
{
	luaL_newmetatable(L, "xtcShader");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);
	lua_register(L, "xtcSetShader", L_xtcSetShader);
	lua_register(L, "xtcGetDefaultShader", L_xtcGetDefaultShader);

	luaL_newmetatable(L, "xtcTexture");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);
	lua_register(L, "xtcSetTexture", L_xtcSetTexture);

	luaL_newmetatable(L, "xtcMaterial");
	luaL_setfuncs(L, xtcMaterial__meta, 0);
	lua_pop(L, 1);
	lua_register(L, "xtcMaterial", L_xtcMaterial);
	lua_register(L, "xtcSetMaterial", L_xtcSetMaterial);

	luaL_newmetatable(L, "xtcLight");
	luaL_setfuncs(L, xtcLight__meta, 0);
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
	lua_register(L, "xtcBegin", L_xtcBegin);
	lua_register(L, "xtcEnd", L_xtcEnd);
	lua_register(L, "xtcVertex", L_xtcVertex);
	lua_register(L, "xtcColor", L_xtcColor);
	lua_register(L, "xtcNormal", L_xtcNormal);
	lua_register(L, "xtcTexCoord", L_xtcTexCoord);
	lua_register(L, "xtcIndices", L_xtcIndices);
	lua_register(L, "xtcWeights", L_xtcWeights);
}

static int
L_loadXModel(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	FILE *f = efopen(name, "r");
	xModel *mdl = loadXModel(f);
	if(mdl) {
		buildXModel(mdl);
		mkptr(L, "xModel", mdl);
		fclose(f);
		return 1;
	}
	fclose(f);
	return 0;
}

static int
L_loadXModelChunk(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	FILE *f = efopen(name, "rb");
	xModel *mdl = loadXModelChunk(f);
	if(mdl) {
		buildXModel(mdl);
		mkptr(L, "xModel", mdl);
		fclose(f);
		return 1;
	}
	fclose(f);
	return 0;
}

static int
L_xModelDraw(lua_State *L)
{
	xModel *mdl = *(xModel**)luaL_checkudata(L, 1, "xModel");
	xModelDraw(mdl, 0);
	return 0;
}


void
registerXModel(lua_State *L)
{
	luaL_newmetatable(L, "xModel");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, L_xModelDraw);
	lua_setfield(L, -2, "draw");
	lua_pop(L, 1);

	lua_register(L, "loadXModel", L_loadXModel);
	lua_register(L, "loadXModelChunk", L_loadXModelChunk);
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


lua_State*
initLua(void)
{
	lua_State *L = luaL_newstate();
	luaL_openlibs(L);
	registerXmath(L);
	registerXtc(L);
	registerXModel(L);

	luaL_newmetatable(L, "ImguiIO");
	luaL_setfuncs(L, imguiIO__meta, 0);
	lua_pop(L, 1);
	lua_register(L, "imguiIO", L_imguiIO);
	lua_register(L, "getDragMode", L_getDragMode);

	return L;
}
