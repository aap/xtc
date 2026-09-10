#include "xtci.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <libgraph.h>
#include <libdma.h>
#include <sifrpc.h>

struct xtcState xtcState;

static Mat4 identity = {
	{ 1.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, 1.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 1.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f, 1.0f }
};

void
xtcSetPipeline(xtcPipeline *pipe)
{
	xtcState.pipe = pipe;
}

static void
updateZ(void)
{
	float *p;

	float n = xtcState.near;
	float f = xtcState.far;
	float N = xtcState.nearScreen;
	float F = xtcState.farScreen;

	// done by RW, some safe region?
	N += (F - N)/10000.0f;
	F -= (F - N)/10000.0f;

	float zscale = (N - F)*n*f/(f - n);
	float zoffset = (F*f - N*n)/(f - n);

#if 1
	// perspective
	p = (float*)&xtcState.xyzwScale;
	p[2] = zscale;
	p = (float*)&xtcState.xyzwOffset;
	p[2] = zoffset;
#else
//2d
	p = (float*)&xtcState.xyzwScale;
	p[2] = 0.5f*(F - N);

	p = (float*)&xtcState.xyzwOffset;
	p[2] = 0.5f*(F + N);
#endif
}

void
xtcSetProjectionMatrix(const Mat4 *mat)
{
	float *p;

	xtcState.proj = *mat;

	// GL convention: a = (n+f)/(n-f), b = 2nf/(n-f)
	float a = xtcState.proj.z.z;
	float b = xtcState.proj.w.z;

	// if perspective
	xtcState.near = b/(a-1.0f);
	xtcState.far = b/(a+1.0f);

	p = (float*)&xtcState.clipConsts;
	p[2] = xtcState.near;
	p[3] = xtcState.far;

	updateZ();
	xtcState.xformGen++;
}

void
xtcSetViewMatrix(const Mat4 *mat)
{
	xtcState.view = *mat;
/*
	// TODO: maybe rethink choice of coord system?
	xtcState.view[0] = -xtcState.view[0];
	xtcState.view[4] = -xtcState.view[4];
	xtcState.view[8] = -xtcState.view[8];
	xtcState.view[12] = -xtcState.view[12];
*/
	xtcState.xformGen++;
}

void
xtcSetWorldMatrix(const Mat4 *mat)
{
	// a model's meshes set the same matrix one after the other; the
	// pipelines re-upload on the generation, so only a change counts
	if(memcmp(&xtcState.world, mat, sizeof(Mat4)) == 0)
		return;
	xtcState.world = *mat;
	xtcState.xformGen++;
}

Mat4
xtcGetWorldMatrix(void)
{
	return xtcState.world;
}

// points are drawn as points until a VU1 sprite pipeline exists
void
xtcPointSize(float size)
{
	(void)size;
}

void
xtcSetBoneMatrices(const Mat4 *matrices, int n)
{
	if(n > (int)nelem(xtcState.boneMatrices))
		n = nelem(xtcState.boneMatrices);
	memcpy(xtcState.boneMatrices, matrices, n*sizeof(Mat4));
	xtcState.numBoneMatrices = n;
}

void
xtcViewport(int x, int y, int width, int height)
{
	float *p;

	p = (float*)&xtcState.xyzwScale;
	p[0] = width/2;
	p[1] = -height/2;

	p = (float*)&xtcState.xyzwOffset;
	p[0] = 2048 - xtcState.width/2 + x + width/2;
	p[1] = 2048 + xtcState.height/2 - y - height/2;
	xtcState.xformGen++;
}

void
xtcDepthRange(int near, int far)
{
	xtcState.nearScreen = near;
	xtcState.farScreen = far;
	updateZ();
}

void
xtcScissor(int x, int y, int width, int height)
{
	y = xtcState.height - (y+height);
	xtcgRegs.c1.scissor = SCE_GS_SET_SCISSOR(x, x+width-1, y, y+height-1);
	xtcgRegs.c2.scissor = xtcgRegs.c1.scissor;
}

void
xtcClearColor(int r, int g, int b, int a)
{
	// forget about Q
	xtcState.clearcol = SCE_GS_SET_RGBAQ(r, g, b, a, 0);
}

void
xtcClearDepth(uint32 z)
{
	xtcState.cleardepth = z;
}

void
xtcClear(int mask)
{
	const uint32 w = xtcState.width;
	const uint32 h = xtcState.height;
	const uint32 nstrips = xtcState.width/32;
	mdmaList *l = xtcState.list;

	if(mask == 0)
		return;

	xtcgFlushRegs(l);

	mdmaCnt(l, 7 + nstrips*2);
		mdmaBeginDirect(l, 7 + nstrips*2, 0);
			mdmaBeginGifTag(l, 6 + nstrips*2, 1, 1,SCE_GS_PRIM_SPRITE,
				GIF_PACKED, 1, GIF_AD);
			xtcgCurRegs.c1.test = SCE_GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1);
			mdmaAddAD(l, SCE_GS_TEST_1, xtcgCurRegs.c1.test);
			xtcgCurRegs.c1.scissor = SCE_GS_SET_SCISSOR(0, w-1, 0, h-1);
			mdmaAddAD(l, SCE_GS_SCISSOR_1, xtcgCurRegs.c1.scissor);
			xtcgCurRegs.prmode = SCE_GS_SET_PRMODE(0, 0, 0, 0, 0, 0, 0, 0);
			mdmaAddAD(l, SCE_GS_PRMODE, xtcgCurRegs.prmode);
			if(mask & XTC_COLORBUF)
				xtcgCurRegs.c1.frame = (uint32)xtcgRegs.c1.frame;
			else
				xtcgCurRegs.c1.frame = 0xFFFFFFFF00000000 | xtcgRegs.c1.frame;
			if(mask & XTC_DEPTHBUF)
				xtcgCurRegs.c1.zbuf = (uint32)xtcgRegs.c1.zbuf;
			else
				xtcgCurRegs.c1.zbuf = 0x100000000 | xtcgRegs.c1.zbuf;
			mdmaAddAD(l, SCE_GS_FRAME_1, xtcgCurRegs.c1.frame);
			mdmaAddAD(l, SCE_GS_ZBUF_1, xtcgCurRegs.c1.zbuf);
			mdmaAddAD(l, SCE_GS_RGBAQ, xtcState.clearcol);

			for(int i = 0; i < nstrips; i++){
				int x = 2048 - w/2;
				int y = 2048 - h/2;
				mdmaAddAD(l, SCE_GS_XYZ2,
					SCE_GS_SET_XYZ((x+i*32)<<4, y<<4, xtcState.cleardepth));
				mdmaAddAD(l, SCE_GS_XYZ2,
					SCE_GS_SET_XYZ((x+(i+1)*32)<<4, (y+xtcState.height)<<4, xtcState.cleardepth));
			}
			mdmaEndGifTag(l);
		mdmaEndDirect(l);
	mdmaCloseTag(l);
}

void
xtcSetDraw(xtcgDrawBuffer *draw)
{
	xtcgRegs.c1.frame &= 0xFFFFFFFF00000000;
	xtcgRegs.c2.frame &= 0xFFFFFFFF00000000;
	xtcgRegs.c1.zbuf &= 0xFFFFFFFF00000000;
	xtcgRegs.c2.zbuf &= 0xFFFFFFFF00000000;

	xtcgRegs.c1.frame |= (uint32)draw->frame1;
	xtcgRegs.c2.frame |= (uint32)draw->frame2;
	xtcgRegs.c1.zbuf |= (uint32)draw->zbuf1;
	xtcgRegs.c2.zbuf |= (uint32)draw->zbuf2;

	xtcgRegs.c1.xyoffset = draw->xyoffset1;
	xtcgRegs.c2.xyoffset = draw->xyoffset2;
}

// TODO:
//	dst alpha
//	dither
void
xtcEnable(xtceState state)
{
	switch(state) {
	case XTC_DEPTH_TEST:
		xtcState.zte = 1;
		xtcgRegs.c1.test = (xtcgRegs.c1.test & ~(3UL<<17)) | xtcState.ztst;
		break;
	case XTC_ALPHA_TEST:
		xtcgRegs.c1.test |= 1;
		break;
	case XTC_BLEND:
		xtcgRegs.prmode |= 1<<6;
		break;
	case XTC_FOG:
		xtcgRegs.prmode |= 1<<5;
		break;
	case XTC_TEXTURE:
		xtcState.tme = 1;
		break;
	case XTC_CLIPPING:
		xtcState.clipping = 1;
		break;
	}
}

void
xtcDisable(xtceState state)
{
	switch(state) {
	case XTC_DEPTH_TEST:
		xtcState.zte = 0;
		xtcgRegs.c1.test = (xtcgRegs.c1.test & ~(3UL<<17)) | (1<<17);
		break;
	case XTC_ALPHA_TEST:
		xtcgRegs.c1.test &= ~1UL;
		break;
	case XTC_BLEND:
		xtcgRegs.prmode &= ~(1UL<<6);
		break;
	case XTC_FOG:
		xtcgRegs.prmode &= ~(1UL<<5);
		break;
	case XTC_TEXTURE:
		xtcState.tme = 0;
		break;
	case XTC_CLIPPING:
		xtcState.clipping = 0;
		break;
	}
}

void
xtcDepthFunc(xtceDepthFunc func)
{
	xtcState.ztst = func<<17;
	if(xtcState.zte)
		xtcgRegs.c1.test = (xtcgRegs.c1.test & ~(3UL<<17)) | xtcState.ztst;
}

void
xtcAlphaFunc(xtceAlphaFunc func, int ref, xtceAlphaFail fail)
{
	ref &= 0xFF;
	xtcgRegs.c1.test = (xtcgRegs.c1.test & ~(0x1FFFUL<<1)) |
		func<<1 | ref<<4 | fail<<12;
}

void
xtcBlendFunc(xtceAlpha a, xtceAlpha b, xtceAlpha c, xtceAlpha d, int fix)
{
	fix &= 0xFF;
	xtcgRegs.c1.alpha = SCE_GS_SET_ALPHA(a, b, c, d, fix);
}

int64 blendTable[6][6] = {      // [src][dst]
	0x000000008A,  0x000000004A,  0x0000000089,  0x0000000046,  0x0000000099,  0x0000000056,
	0x000000000A,  0x8000000029,  0x0000000009,            -1,  0x0000000019,            -1,
	0x0000000088,  0x0000000048,            -1,  0x0000000044,            -1,            -1,
	0x0000000002,            -1,  0x0000000001,            -1,            -1,            -1,
	0x0000000098,  0x0000000058,            -1,            -1,            -1,  0x0000000054,
	0x0000000012,            -1,            -1,            -1,  0x0000000011,            -1,
};

void
xtcBlendFuncSrcDst(xtcBlendFactor src, xtcBlendFactor dst)
{
	int64 alpha;
	alpha = blendTable[src][dst];
	if(alpha < 0)
		return;
	xtcgRegs.c1.alpha = alpha;
}

// color is BBGGRR
void
xtcFog(float start, float end, uint32 col)
{
	float *p;

	xtcState.fogstart = start;
	xtcState.fogend = end;
	xtcgRegs.fogcol = col;

	float scale = -255.0f/(end - start);
	float shift = -end*scale;

	p = (float*)&xtcState.xyzwScale;
	p[3] = scale;

	p = (float*)&xtcState.xyzwOffset;
	p[3] = shift;

	p = (float*)&xtcState.clipConsts;
	p[0] = start;
	p[1] = end;
	xtcState.xformGen++;
}

void
xtcShadeModel(xtceShadeModel model)
{
	switch(model) {
	case XTC_FLAT:
		xtcgRegs.prmode &= ~(1UL<<3);
		break;
	case XTC_SMOOTH:
		xtcgRegs.prmode |= 1UL<<3;
		break;
	}
}

// AABBGGRR
void
xtcPixelMask(uint32 mask)
{
	xtcgRegs.c1.frame &= 0xFFFFFFFF;
	xtcgRegs.c1.frame |= (uint64)mask << 32;
}

// 1 masks the z buffer, like the GS ZMSK -- which is where this goes.
// (It used to poke FRAME, i.e. FBMSK, which is xtcPixelMask's register.)
void
xtcDepthMask(int mask)
{
	if(mask)
		xtcgRegs.c1.zbuf |= (uint64)1 << 32;
	else
		xtcgRegs.c1.zbuf &= ~((uint64)1 << 32);
}

void
xtcSetColorMod(const xtcColorMod *mod)
{
	if(memcmp(&xtcState.colorMod, mod, sizeof(xtcColorMod)) == 0)
		return;
	xtcState.colorMod = *mod;
	// the clamp rides with the light block, the scale with the switch
	xtcState.matGen++;
}

void
xtcGetColorMod(xtcColorMod *mod)
{
	*mod = xtcState.colorMod;
}

void
xtcSetAmbient(float r, float g, float b)
{
	xtcState.ambient = vec4(r, g, b, 0.0f);
	xtcState.lightGen++;
}

void xtcSetLight(int n, const xtcLight *light)
{
	if(n < 0 || (uint32)n >= nelem(xtcState.lights))
		return;
	xtcState.lights[n] = *light;
	xtcState.lightGen++;
}

void xtcSetRwMaterial(const xtcRwMaterial *mat)
{
	xtcState.rwMaterial = *mat;
}

void xtcSetStdMaterial(const xtcStdMaterial *mat)
{
	if(memcmp(&xtcState.stdMaterial, mat, sizeof(xtcStdMaterial)) == 0)
		return;
	xtcState.stdMaterial = *mat;
	xtcState.matGen++;
}

void xtcSetColorMaterial(uint32 bits)
{
	if(xtcState.stdColSel == bits)
		return;
	xtcState.stdColSel = bits;
	xtcState.matGen++;
}

void
xtcSetList(mdmaList *list)
{
	xtcState.list = list;
}

void
xtcInit(int width, int height, int depth)
{

	xtcState.width = width;
	xtcState.height = height;
	xtcState.clearcol = 0;

	memset(&xtcgRegs, 0, sizeof(xtcgRegs));
	xtcShadeModel(XTC_SMOOTH);
	// everything disabled, always enable ztst in always mode
	xtcgRegs.c1.test = SCE_GS_SET_TEST(0, 1, 0, 0, 0, 0, 1, 1);
	xtcgRegs.c2.test = xtcgRegs.c1.test;
	xtcgRegs.texa = SCE_GS_SET_TEXA(0, 0, 0x80);
	xtcState.zte = 0;
	xtcState.ztst = 2<<17;	// GEQUAL

	xtcEnable(XTC_CLIPPING);

	xtcTexFunc(XTC_RGB, XTC_MODULATE);
	xtcTexFilter(XTC_NEAREST, XTC_NEAREST);
	xtcTexWrap(XTC_REPEAT, XTC_REPEAT);
	xtcTexLodMode(0, 0xFC0, 0);

	xtcViewport(0, 0, width, height);
	xtcDepthRange((1<<depth)-1, 0);
	xtcScissor(0, 0, width, height);
	xtcFog(0.0f, 1.0f, 0);

	xtcSetProjectionMatrix(&identity);
	xtcSetViewMatrix(&identity);
	xtcSetWorldMatrix(&identity);

	{
		const float scl = 128.0f/255.0f;
		xtcColorMod cm;
		cm.clamp = vec4(255.0f, 255.0f, 255.0f, 255.0f);
		cm.scale = vec4(1.0f, 1.0f, 1.0f, scl);
		cm.scaleTex = vec4(scl, scl, scl, scl);
		xtcSetColorMod(&cm);
	}

	xtcRwMaterial *m = &xtcState.rwMaterial;
	m->color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
	m->ambient = 1.0f;
	m->diffuse = 1.0f;
	m->specular = 1.0f;
	m->shininess = 1.0f;

	Vec4 black = { 0.0f, 0.0f, 0.0f, 0.0f };
	Vec4 white = { 1.0f, 1.0f, 1.0f, 0.0f };
	xtcStdMaterial *n = &xtcState.stdMaterial;
	n->emissive = black;
	n->ambient = white;
	n->diffuse = white;
	n->specular = white;
	xtcState.stdColSel = 0;

	xtcSetAmbient(0.2f, 0.2f, 0.2f);

	xtcgSetRegs(xtcState.list);
}
