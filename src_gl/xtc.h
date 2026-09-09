#pragma once

#include <vector>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>


#define PI 3.1415926535897932384626433832795f
#define TAU (2.0f*PI)

#define nil nullptr
#define nelem(array) (sizeof(array)/sizeof(array[0]))
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int32_t i32;
typedef int16_t i16;
typedef int8_t i8;
// the names the ps2 side and common/ use
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;
typedef int32_t int32;
typedef int16_t int16;
typedef int8_t int8;

#include "xmath.h"

struct xtcRGBA
{
	u8 r, g, b, a;
};

/*
   GS render states

	depth test
		enable
		cmp
	alpha test
		enable
		cmp
		ref
		afail
	dst alpha test?
		enable
		setting
	alpha blend
		enable
		A,B,C,D,FIX
	fog
		enable
		start
		end
		color
	shading
		flat/gouraud
	dithering
		enable
		matrix

  drawing:
	draw fb
		masking
	draw zb
		masking
	scissor
	offset

  per texture:
	texture wrap
	filtering
	mipmap
	modulation


  render states VU1:
	clipping/culling
	backface culling
	modulation/color scale
*/

enum xtceState {
	XTC_DEPTH_TEST,
	XTC_ALPHA_TEST,
	XTC_BLEND,
	XTC_FOG,
	XTC_TEXTURE,
	XTC_CLIPPING		// may want to be more fine grained eventually
};

void xtcEnable(xtceState state);
void xtcDisable(xtceState state);

enum xtceDepthFunc {
	XTC_DEPTH_NEVER,
	XTC_DEPTH_ALWAYS,
	XTC_DEPTH_GEQUAL,
	XTC_DEPTH_GREATER
};

void xtcDepthFunc(xtceDepthFunc func);

enum xtceAlphaFunc {
	XTC_AFUNC_NEVER,
	XTC_AFUNC_ALWAYS,
	XTC_AFUNC_LESS,
	XTC_AFUNC_LEQUAL,
	XTC_AFUNC_EQUAL,
	XTC_AFUNC_GEQUAL,
	XTC_AFUNC_GREATER,
	XTC_AFUNC_NOTEQUAL
};

enum xtceAlphaFail {
	XTC_AFAIL_KEEP,
	XTC_AFAIL_FB_ONLY,
	XTC_AFAIL_ZB_ONLY,
	XTC_AFAIL_RGB_ONLY
};

void xtcAlphaFunc(xtceAlphaFunc func, int ref, xtceAlphaFail fail);

enum xtceAlpha {
	XTC_ALPHA_SRC,
	XTC_ALPHA_DST,
	XTC_ALPHA_ZERO,
	XTC_ALPHA_FIX = XTC_ALPHA_ZERO
};

void xtcBlendFunc(xtceAlpha a, xtceAlpha b, xtceAlpha c, xtceAlpha d, int fix);

enum xtcBlendFactor {
	XTC_BLEND_ZERO,
	XTC_BLEND_ONE,
	XTC_BLEND_SRCALPHA,
	XTC_BLEND_INVSRCALPHA,
	XTC_BLEND_DSTALPHA,
	XTC_BLEND_INVDSTALPHA,
};

void xtcBlendFuncSrcDst(xtcBlendFactor src, xtcBlendFactor dst);
void xtcFog(float start, float end, u32 col);

enum xtceShadeModel {
	XTC_FLAT,
	XTC_SMOOTH,
};

void xtcShadeModel(xtceShadeModel model);

/*
void xtcPixelMask(u32 mask);
void xtcDepthMask(int mask);
*/

/*
 * A pipeline is the programmable part of the drawing path: on the PS2 a
 * piece of VU1 microcode plus the function that uploads its constants,
 * here a GL program.  Every pipeline reads the current material and
 * light set in its own way.
 */
struct xtcPipeline
{
	void (*upload)(void);
	// on PS2 we'd have some info on VU layout here
};
void xtcSetPipeline(xtcPipeline *pipe);

// the original sketch: one directional light, blinn specular with a
// finite viewer.  the skin one is the same with bone matrices
extern xtcPipeline *defaultPipeline;
extern xtcPipeline *skinPipeline;
// the VU1-shaped lighting model, see xtcLight: global ambient,
// up to 4 or 8 directional diffuse lights, one directional specular
extern xtcPipeline *lit4Pipeline;
extern xtcPipeline *lit8Pipeline;








void xtcSetProjectionMatrix(const Mat4 *proj);
void xtcSetViewMatrix(const Mat4 *view);
void xtcSetWorldMatrix(const Mat4 *world);
void xtcSetWorldMatrix(const Mat4 &world);
Mat4 xtcGetWorldMatrix(void);
void xtcSetBoneMatrices(const Mat4 *matrices, int n);

void xtcViewport(int x, int y, int width, int height);
// TODO
//void xtcDepthRange(int near, int far);
void xtcScissor(int x, int y, int width, int height);
void xtcClearColor(int r, int g, int b, int a);
void xtcClearDepth(u32 z);

enum {
	XTC_COLORBUF = 1,
	XTC_DEPTHBUF = 2,
};

void xtcClear(int mask);

enum xtcLightType
{
	XTC_LIGHT_DIRECT,
	XTC_LIGHT_POINT
	// TODO: spot?
};

/*
 * Lights live in world space, colours are 0..1.  direction is the
 * direction the light shines in.  There are 8 slots; how many of them a
 * pipeline uses and how is up to the pipeline:
 *
 * lit4/lit8: the enabled directional lights in slot order, up to 4 or 8,
 * are packed into a light matrix in object space, so per vertex the
 * diffuse term is one matrix multiply, a clamp and a second matrix
 * multiply with the colours (what the PSP does and what VU1 likes).
 * The first of them is also the specular light: blinn with an infinite
 * viewer, so its half vector is a constant too.  specColor of the other
 * lights is ignored.  Global ambient is added on top.
 */
struct xtcLight
{
	int enabled;
	xtcLightType type;
	Vec4 color;
	Vec4 specColor;
	Vec3 direction;
	Vec3 position;
};
void xtcSetAmbient(int r, int g, int b);
void xtcSetLight(int n, const xtcLight *light);

struct xtcMaterial
{
	Vec4 colorSelector;	// amb, diff, spec, emiss, 0 - material, 1 - vertex
	Vec4 ambient;
	Vec4 diffuse;
	Vec4 specular;
	Vec4 emissive;
	float shininess;
};
void xtcSetMaterial(const xtcMaterial *mat);

struct xtcTexture
{
	u32 tex;
	u32 width, height;
};
void xtcSetTextureN(int n, xtcTexture *tex);
inline void xtcSetTexture(xtcTexture *tex) { xtcSetTextureN(0, tex); }
xtcTexture *xtcTextureReadPNG(const u8 *data, u32 size);


enum xtcTCC {
	XTC_RGB,
	XTC_RGBA
};

enum xtcTFX {
	XTC_MODULATE,
	XTC_DECAL,
	XTC_HIGHLIGHT,
	XTC_HIGHLIGHT2
};

enum xtcFilter {
	XTC_NEAREST,
	XTC_LINEAR,
	XTC_NEAREST_MIP_NEAREST,
	XTC_NEAREST_MIP_LINEAR,
	XTC_LINEAR_MIP_NEAREST,
	XTC_LINEAR_MIP_LINEAR
};

enum xtcWrap {
	XTC_REPEAT,
	XTC_CLAMP
};
// TODO
/*
void xtcTexFunc(xtcTCC tcc, xtcTFX tfx);
void xtcTexFilter(xtcFilter min, xtcFilter mag);
void xtcTexWrap(xtcWrap u, xtcWrap v);
void xtcTexLodMode(int lcm, int k, int l);

void xtcColorScale(float r, float g, float b, float a);
void xtcColorScaleTex(float r, float g, float b, float a);
*/

enum xtcPrimType
{
	XTC_POINTS,
	XTC_LINESTRIP,
	XTC_LINELIST,
	XTC_TRISTRIP,
	XTC_TRILIST
};

/*
 * Immediate mode
 */

struct xtcImmVertex3D
{
	Vec3 pos;
	xtcRGBA color;
	Vec3 normal;
	Vec3 texcoord;	// s, t, q
	// for skinning - maybe skip this for most cases somehow?
	u32 indices;
	Vec4 weights;
};

struct ImmState
{
	xtcPrimType primType;
	xtcImmVertex3D vert;
	std::vector<xtcImmVertex3D> vertstore;
	u32 vao;
	u32 vbo;
};

void xtcBegin(xtcPrimType prim);
void xtcFlush(void);
void xtcEnd(void);

void xtcPointSize(float size);
void xtcVertex3(float x, float y, float z);
void xtcVertex3v(const Vec3 &xyz);
#define xtcVertex xtcVertex3
void xtcColor(u8 r, u8 g, u8 b, u8 a);
void xtcColor(const xtcRGBA &rgba);
void xtcNormal(float x, float y, float z);
void xtcNormalv(const Vec3 &xyz);
// q is only meaningful to a 2D pipeline that hands it to the GS as is,
// the 3D pipelines compute their own.  it defaults to 1
void xtcTexCoord2(float s, float t);
void xtcTexCoord3(float s, float t, float q);
#define xtcTexCoord xtcTexCoord2
void xtcTexCoordv(const Vec2 &st);
void xtcIndices(u8 i0, u8 i1, u8 i2, u8 i3);
void xtcWeights(float w0, float w1, float w2, float w3);


void xtcInit(void);

/*
 * Retained mode
 */

struct xtcPrimList
{
// TODO: shader
	xtcPrimType primType;
// TODO: vertex desc
	u32 numVertices;
	u32 vao;
	u32 vbo;
};

// TODO: vertex desc
xtcPrimList *xtcCreatePrimList(void);
void xtcStartList(xtcPrimList *pl);
void xtcEndList(void);
void xtcPrimListSetData(xtcPrimList *pl, xtcPrimType primType, u32 numVertices, xtcImmVertex3D *vertices);
void xtcPrimListDraw(xtcPrimList *pl);
