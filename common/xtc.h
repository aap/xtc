/*
 * xtc -- the public API, shared by the PS2 backend (src/) and the OpenGL
 * one (src_gl/).  Each backend provides xtcplat.h (integer types, the
 * odd platform include) ahead of this file and keeps its internals in
 * its own xtci.h.  Everything in here is plain C; C++ only gets a few
 * inline conveniences at the bottom.
 *
 * Conventions:
 *   matrices     column-major Mat4 (xmath.h), GL style: the camera looks
 *                down -z, the projection maps [-n,-f] to [-1,1]
 *   colours      float colours (lights, materials) are Vec4 rgba in 0..1,
 *                vertex colours are xtcRGBA bytes
 *   lights       8 slots, world space, direction = where the light shines;
 *                what a pipeline does with them is its own business
 */

#ifndef XTC_H
#define XTC_H

#include "xtcplat.h"
#include "xmath.h"

#ifndef nil
#define nil ((void*)0)
#endif
#ifndef nelem
#define nelem(arr) (sizeof(arr)/sizeof(arr[0]))
#endif
#ifndef PI
#define PI 3.14159265358979323846f
#define TAU (2.0f*PI)
#endif

/* C++ has no forward-declared enums (not in every vintage anyway), and
 * does not need the typedef: the tag is already a type name. */
#ifndef STRUCT
#ifdef __cplusplus
#define ENUM(name) enum name
#else
#define ENUM(name) \
typedef enum name name; \
enum name
#endif
#define STRUCT(name) \
typedef struct name name; \
struct name
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* a vertex colour */
STRUCT(xtcRGBA) { uint8 r, g, b, a; };


/*
 * Textures.  The struct is the backend's.
 */

typedef struct xtcTexture xtcTexture;

xtcTexture *xtcTextureReadPNG(const uint8 *data, uint32 len);
void xtcTextureFree(xtcTexture *tex);
/* called on every bind that changes the texture, with its size in GS
 * pages, when set: for tracing what a texture cache would have to do */
extern void (*xtcTextureBindHook)(xtcTexture *tex, int pages);
void xtcSetTexture(xtcTexture *tex);

ENUM(xtcTCC) {
	XTC_RGB,
	XTC_RGBA
};

ENUM(xtcTFX) {
	XTC_MODULATE,
	XTC_DECAL,
	XTC_HIGHLIGHT,
	XTC_HIGHLIGHT2
};

ENUM(xtcFilter) {
	XTC_NEAREST,
	XTC_LINEAR,
	XTC_NEAREST_MIP_NEAREST,
	XTC_NEAREST_MIP_LINEAR,
	XTC_LINEAR_MIP_NEAREST,
	XTC_LINEAR_MIP_LINEAR
};

ENUM(xtcWrap) {
	XTC_REPEAT,
	XTC_CLAMP
};

void xtcTexFunc(xtcTCC tcc, xtcTFX tfx);
void xtcTexFilter(xtcFilter min, xtcFilter mag);
void xtcTexWrap(xtcWrap u, xtcWrap v);
void xtcTexLodMode(int lcm, int k, int l);

/* What the GS gets.  The lit vertex colour, in 0..255, is clamped per
 * component and then scaled; the scale has one set for untextured
 * drawing, where 255 is white, and one for textured, where the GS's
 * modulate takes 128 as 1.0 and 255 as twice that.  Alpha is in the GS
 * convention throughout, 128 is opaque.  So the defaults are a clamp of
 * 255, a scale of (1, 1, 1, 128/255) and a textured scale of 128/255;
 * colours already in the 128 = 1.0 convention (PS1 data, say) want a
 * textured scale of (1, 1, 1, 128/255) and the clamp as it is, which
 * keeps their overbright range.  Not part of the material: the material
 * is what the lights see, this is what the GS makes of the result. */
STRUCT(xtcColorMod) {
	Vec4 clamp;
	Vec4 scale;
	Vec4 scaleTex;
};
void xtcSetColorMod(const xtcColorMod *mod);
void xtcGetColorMod(xtcColorMod *mod);


/*
 * Pipelines: the programmable part of the drawing path, VU1 microcode
 * plus its upload on the PS2, a GL program here.  The struct is the
 * backend's; these are the ones every backend is expected to have.
 */

typedef struct xtcPipeline xtcPipeline;

extern xtcPipeline *twodPipeline;	// screen space, no lighting
extern xtcPipeline *nolightPipeline;	// vertex colours as they are
extern xtcPipeline *defaultPipeline;	// RenderWare style lighting, xtcRwMaterial
extern xtcPipeline *stdPipeline;	// GL/PSP style lighting, xtcStdMaterial
extern xtcPipeline *skinPipeline;	// std plus skinning
// the objects behind them, for data linked into the ELF (xm2dsm -link
// writes .int xtcStdPipeline into a prim list); PS2 only
extern xtcPipeline xtcTwodPipeline, xtcNolightPipeline, xtcDefaultPipeline,
	xtcStdPipeline, xtcSkinPipeline;

void xtcSetPipeline(xtcPipeline *pipe);


/*
 * Immediate mode.  Between xtcBegin and xtcEnd the sticky attributes
 * (colour, normal, texcoord, skin data) apply to every xtcVertex.
 * Inside xtcStartList/xtcEndList the primitives are recorded into a
 * prim list instead of drawn.
 */

ENUM(xtcPrimType) {
	XTC_POINTS,
	XTC_LINELIST,
	XTC_LINESTRIP,
	XTC_TRILIST,
	XTC_TRISTRIP,
	XTC_NUM_PRIMTYPES
};

void xtcBegin(xtcPrimType prim);
void xtcEnd(void);
void xtcRestartStrip(void);
void xtcVertex(float x, float y, float z);
void xtcColor(uint32 r, uint32 g, uint32 b, uint32 a);
void xtcNormal(float x, float y, float z);
/* q is only meaningful to a 2D pipeline that hands it to the GS as is,
 * the 3D pipelines compute their own; it defaults to 1 */
void xtcTexCoord2(float s, float t);
void xtcTexCoord3(float s, float t, float q);
#define xtcTexCoord xtcTexCoord2
/* skinning: four bone indices and weights per vertex */
void xtcIndices(uint8 i0, uint8 i1, uint8 i2, uint8 i3);
void xtcWeights(float w0, float w1, float w2, float w3);
void xtcPointSize(float size);

typedef struct xtcPrimList xtcPrimList;

xtcPrimList *xtcCreatePrimList(void);
void xtcStartList(xtcPrimList *pl);
void xtcEndList(void);
void xtcPrimListDraw(xtcPrimList *pl);


/*
 * Materials: the constants of the programmable part.  Which one a
 * pipeline reads is up to the pipeline.
 */

/* RenderWare: one colour and four coefficients */
STRUCT(xtcRwMaterial) {
	Vec4 color;
	float ambient;
	float diffuse;
	float specular;
	float shininess;
};

/* GL/PSP: a colour per term, four qwords; specular.w is the power */
STRUCT(xtcStdMaterial) {
	Vec4 emissive;
	Vec4 ambient;
	Vec4 diffuse;
	Vec4 specular;
};

void xtcSetRwMaterial(const xtcRwMaterial *mat);
void xtcSetStdMaterial(const xtcStdMaterial *mat);

/* which of the std material's terms take the vertex colour instead */
ENUM(xtcColorBit) {
	XTC_EMISSIVE = 1,
	XTC_AMBIENT  = 2,
	XTC_DIFFUSE  = 4,
	XTC_SPECULAR = 8
};
void xtcSetColorMaterial(uint32 bits);


/*
 * Lights
 */

ENUM(xtcLightType) {
	XTC_LIGHT_DIRECT,
	XTC_LIGHT_POINT,	// not yet
	XTC_LIGHT_SPOT		// not yet
};

STRUCT(xtcLight) {
	int enabled;
	xtcLightType type;
	Vec4 color;
	Vec4 specColor;
	Vec3 direction;
	Vec3 position;
};

void xtcSetAmbient(float r, float g, float b);
void xtcSetLight(int n, const xtcLight *light);


/*
 * Transforms
 */

void xtcSetProjectionMatrix(const Mat4 *mat);
void xtcSetViewMatrix(const Mat4 *mat);
void xtcSetWorldMatrix(const Mat4 *mat);
Mat4 xtcGetWorldMatrix(void);
void xtcSetBoneMatrices(const Mat4 *matrices, int n);

void xtcViewport(int x, int y, int width, int height);
void xtcDepthRange(int near, int far);
void xtcScissor(int x, int y, int width, int height);


/*
 * Frame buffer and render state, the GS's terms exposed raw
 */

void xtcClearColor(int r, int g, int b, int a);
void xtcClearDepth(uint32 z);

enum {
	XTC_COLORBUF = 1,
	XTC_DEPTHBUF = 2
};

void xtcClear(int mask);

ENUM(xtceState) {
	XTC_DEPTH_TEST,
	XTC_ALPHA_TEST,
	XTC_BLEND,
	XTC_FOG,
	XTC_TEXTURE,
	XTC_CLIPPING		// may want to be more fine grained eventually
};

void xtcEnable(xtceState state);
void xtcDisable(xtceState state);

ENUM(xtceDepthFunc) {
	XTC_DEPTH_NEVER,
	XTC_DEPTH_ALWAYS,
	XTC_DEPTH_GEQUAL,
	XTC_DEPTH_GREATER
};

void xtcDepthFunc(xtceDepthFunc func);

ENUM(xtceAlphaFunc) {
	XTC_AFUNC_NEVER,
	XTC_AFUNC_ALWAYS,
	XTC_AFUNC_LESS,
	XTC_AFUNC_LEQUAL,
	XTC_AFUNC_EQUAL,
	XTC_AFUNC_GEQUAL,
	XTC_AFUNC_GREATER,
	XTC_AFUNC_NOTEQUAL
};

ENUM(xtceAlphaFail) {
	XTC_AFAIL_KEEP,
	XTC_AFAIL_FB_ONLY,
	XTC_AFAIL_ZB_ONLY,
	XTC_AFAIL_RGB_ONLY
};

void xtcAlphaFunc(xtceAlphaFunc func, int ref, xtceAlphaFail fail);

ENUM(xtceAlpha) {
	XTC_ALPHA_SRC,
	XTC_ALPHA_DST,
	XTC_ALPHA_ZERO,
	XTC_ALPHA_FIX = XTC_ALPHA_ZERO
};

/* the GS blend equation: (a - b)*c + d */
void xtcBlendFunc(xtceAlpha a, xtceAlpha b, xtceAlpha c, xtceAlpha d, int fix);

ENUM(xtcBlendFactor) {
	XTC_BLEND_ZERO,
	XTC_BLEND_ONE,
	XTC_BLEND_SRCALPHA,
	XTC_BLEND_INVSRCALPHA,
	XTC_BLEND_DSTALPHA,
	XTC_BLEND_INVDSTALPHA
};

/* the GL way of saying it, for the combinations the GS can do */
void xtcBlendFuncSrcDst(xtcBlendFactor src, xtcBlendFactor dst);

/* colour is BBGGRR */
void xtcFog(float start, float end, uint32 col);

ENUM(xtceShadeModel) {
	XTC_FLAT,
	XTC_SMOOTH
};

void xtcShadeModel(xtceShadeModel model);

/* AABBGGRR */
void xtcPixelMask(uint32 mask);
void xtcDepthMask(int mask);


#ifdef __cplusplus
}	/* extern "C" */
#endif


/*
 * C++ conveniences
 */

#ifdef __cplusplus
static inline void xtcVertex(Vec3 v) { xtcVertex(v.x, v.y, v.z); }
static inline void xtcNormal(Vec3 n) { xtcNormal(n.x, n.y, n.z); }
static inline void xtcTexCoord2(Vec2 st) { xtcTexCoord2(st.x, st.y); }
static inline void xtcColor(xtcRGBA c) { xtcColor(c.r, c.g, c.b, c.a); }
static inline void xtcSetProjectionMatrix(const Mat4 &m) { xtcSetProjectionMatrix(&m); }
static inline void xtcSetViewMatrix(const Mat4 &m) { xtcSetViewMatrix(&m); }
static inline void xtcSetWorldMatrix(const Mat4 &m) { xtcSetWorldMatrix(&m); }
#endif

#endif
