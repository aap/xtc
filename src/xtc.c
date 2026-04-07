#include "mdma.h"
#include "xtc.h"

#include "gs.h"
#include "mdma.h"

#include <sifrpc.h>

struct xtcState xtcState;

static float identity[16] = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
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
xtcSetProjectionMatrix(const float *mat)
{
	float *p;

	memcpy(xtcState.proj, mat, sizeof(xtcState.proj));

	float a = xtcState.proj[10];
	float b = xtcState.proj[14];

	// if perspective
	xtcState.near = b/(-1.0f-a);
	xtcState.far = b/(1.0f-a);

	p = (float*)&xtcState.clipConsts;
	p[2] = xtcState.near;
	p[3] = xtcState.far;

	updateZ();
}

void
xtcSetViewMatrix(const float *mat)
{
	memcpy(xtcState.view, mat, sizeof(xtcState.view));
/*
	// TODO: maybe rethink choice of coord system?
	xtcState.view[0] = -xtcState.view[0];
	xtcState.view[4] = -xtcState.view[4];
	xtcState.view[8] = -xtcState.view[8];
	xtcState.view[12] = -xtcState.view[12];
*/
}

void
xtcSetWorldMatrix(const float *mat)
{
	memcpy(xtcState.world, mat, sizeof(xtcState.world));
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
	mdmaGSregs.c1.scissor = GS_SET_SCISSOR(x, x + width - 1, y, y + height - 1);
	mdmaGSregs.c2.scissor = mdmaGSregs.c1.scissor;
}

void
xtcClearColor(int r, int g, int b, int a)
{
	// forget about Q
	xtcState.clearcol = GS_SET_RGBAQ(r, g, b, a, 0);
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

	if(mask == 0)
		return;

	mdmaFlushGsRegs(xtcState.list);

	mdmaCntDirect(xtcState.list, 7 + nstrips*2);
	mdmaAddGIFtag(xtcState.list, 6 + nstrips * 2, 1, 1, GS_PRIM_SPRITE, GS_GIF_PACKED, 1, 0xe);
	mdmaCurGSregs.c1.test = GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1);
	mdmaAddAD(xtcState.list, GS_REG_TEST_1, mdmaCurGSregs.c1.test);
	mdmaCurGSregs.c1.scissor = GS_SET_SCISSOR(0, w - 1, 0, h - 1);
	mdmaAddAD(xtcState.list, GS_REG_SCISSOR_1, mdmaCurGSregs.c1.scissor);
	mdmaCurGSregs.prmode = GS_SET_PRMODE(0, 0, 0, 0, 0, 0, 0, 0);
	mdmaAddAD(xtcState.list, GS_REG_PRMODE, mdmaCurGSregs.prmode);
	if(mask & XTC_COLORBUF)
		mdmaCurGSregs.c1.frame = (uint32)mdmaGSregs.c1.frame;
	else
		mdmaCurGSregs.c1.frame = 0xFFFFFFFF00000000 | mdmaGSregs.c1.frame;
	if(mask & XTC_DEPTHBUF)
		mdmaCurGSregs.c1.zbuf = (uint32)mdmaGSregs.c1.zbuf;
	else
		mdmaCurGSregs.c1.zbuf = 0x100000000 | mdmaGSregs.c1.zbuf;
	mdmaAddAD(xtcState.list, GS_REG_FRAME_1, mdmaCurGSregs.c1.frame);
	mdmaAddAD(xtcState.list, GS_REG_ZBUF_1, mdmaCurGSregs.c1.zbuf);
	mdmaAddAD(xtcState.list, GS_REG_RGBAQ, xtcState.clearcol);

	for(int i = 0; i < nstrips; i++){
		int x = 2048 - w/2;
		int y = 2048 - h/2;
		mdmaAddAD(xtcState.list, GS_REG_XYZ2,
		    GS_SET_XYZ((x + i * 32) << 4, y << 4, xtcState.cleardepth));
		mdmaAddAD(xtcState.list, GS_REG_XYZ2,
		    GS_SET_XYZ((x + (i + 1) * 32) << 4, (y + xtcState.height) << 4,
			xtcState.cleardepth));
	}
}

void
xtcSetDraw(mdmaDrawBuffer *draw)
{
	mdmaGSregs.c1.frame &= 0xFFFFFFFF00000000;
	mdmaGSregs.c2.frame &= 0xFFFFFFFF00000000;
	mdmaGSregs.c1.zbuf &= 0xFFFFFFFF00000000;
	mdmaGSregs.c2.zbuf &= 0xFFFFFFFF00000000;

	mdmaGSregs.c1.frame |= (uint32)draw->frame1;
	mdmaGSregs.c2.frame |= (uint32)draw->frame2;
	mdmaGSregs.c1.zbuf |= (uint32)draw->zbuf1;
	mdmaGSregs.c2.zbuf |= (uint32)draw->zbuf2;

	mdmaGSregs.c1.xyoffset = draw->xyoffset1;
	mdmaGSregs.c2.xyoffset = draw->xyoffset2;
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
		mdmaGSregs.c1.test = (mdmaGSregs.c1.test & ~(3UL<<17)) | xtcState.ztst;
		break;
	case XTC_ALPHA_TEST:
		mdmaGSregs.c1.test |= 1;
		break;
	case XTC_BLEND:
		mdmaGSregs.prmode |= 1<<6;
		break;
	case XTC_FOG:
		mdmaGSregs.prmode |= 1<<5;
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
		mdmaGSregs.c1.test = (mdmaGSregs.c1.test & ~(3UL<<17)) | (1<<17);
		break;
	case XTC_ALPHA_TEST:
		mdmaGSregs.c1.test &= ~1UL;
		break;
	case XTC_BLEND:
		mdmaGSregs.prmode &= ~(1UL<<6);
		break;
	case XTC_FOG:
		mdmaGSregs.prmode &= ~(1UL<<5);
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
		mdmaGSregs.c1.test = (mdmaGSregs.c1.test & ~(3UL<<17)) | xtcState.ztst;
}

void
xtcAlphaFunc(xtceAlphaFunc func, int ref, xtceAlphaFail fail)
{
	ref &= 0xFF;
	mdmaGSregs.c1.test = (mdmaGSregs.c1.test & ~(0x1FFFUL<<1)) |
		func<<1 | ref<<4 | fail<<12;
}

void
xtcBlendFunc(xtceAlpha a, xtceAlpha b, xtceAlpha c, xtceAlpha d, int fix)
{
	fix &= 0xFF;
	mdmaGSregs.c1.alpha = GS_SET_ALPHA(a, b, c, d, fix);
}

int64 blendTable[6][6] = {
	// [src][dst]
	{ 0x000000008A, 0x000000004A, 0x0000000089, 0x0000000046, 0x0000000099, 0x0000000056 },
	{ 0x000000000A, 0x8000000029, 0x0000000009,           -1, 0x0000000019,           -1 },
	{ 0x0000000088, 0x0000000048,           -1, 0x0000000044,           -1,           -1 },
	{ 0x0000000002,           -1, 0x0000000001,           -1,           -1,           -1 },
	{ 0x0000000098, 0x0000000058,           -1,           -1,           -1, 0x0000000054 },
	{ 0x0000000012,           -1,           -1,           -1, 0x0000000011,           -1 },
};

void
xtcBlendFuncSrcDst(xtcBlendFactor src, xtcBlendFactor dst)
{
	int64 alpha;
	alpha = blendTable[src][dst];
	if(alpha < 0)
		return;
	mdmaGSregs.c1.alpha = alpha;
}

// color is BBGGRR
void
xtcFog(float start, float end, uint32 col)
{
	float *p;

	xtcState.fogstart = start;
	xtcState.fogend = end;
	mdmaGSregs.fogcol = col;

	float scale = -255.0f/(end - start);
	float shift = -end*scale;

	p = (float*)&xtcState.xyzwScale;
	p[3] = scale;

	p = (float*)&xtcState.xyzwOffset;
	p[3] = shift;

	p = (float*)&xtcState.clipConsts;
	p[0] = start;
	p[1] = end;
}

void
xtcShadeModel(xtceShadeModel model)
{
	switch(model) {
	case XTC_FLAT:
		mdmaGSregs.prmode &= ~(1UL<<3);
		break;
	case XTC_SMOOTH:
		mdmaGSregs.prmode |= 1UL<<3;
		break;
	}
}

// AABBGGRR
void
xtcPixelMask(uint32 mask)
{
	mdmaGSregs.c1.frame &= 0xFFFFFFFF;
	mdmaGSregs.c1.frame |= (uint64)mask << 32;
}

void
xtcDepthMask(int mask)
{
	if(mask)
		mdmaGSregs.c1.frame |= (uint64)1 << 32;
	else
		mdmaGSregs.c1.frame &= ~((uint64)1 << 32);
}

void
xtcColorScale(float r, float g, float b, float a)
{
	xtcState.pColorScale[0] = r;
	xtcState.pColorScale[1] = g;
	xtcState.pColorScale[2] = b;
	xtcState.pColorScale[3] = a;
}

void
xtcColorScaleTex(float r, float g, float b, float a)
{
	xtcState.pColorScaleTex[0] = r;
	xtcState.pColorScaleTex[1] = g;
	xtcState.pColorScaleTex[2] = b;
	xtcState.pColorScaleTex[3] = a;
}

void
xtcSetAmbient(int r, int g, int b)
{
	xtcState.ambient.r = r;
	xtcState.ambient.g = g;
	xtcState.ambient.b = b;
}

void xtcSetLight(int n, xtcLight *light)
{
	if(n < 0 || (uint32)n >= nelem(xtcState.lights))
		return;
	xtcState.lights[n] = *light;
}

void
xtcSetList(mdmaList *list)
{
	xtcState.list = list;
}

void
xtcInit(int width, int height, int depth)
{
	xtcState.pColorScale = (float*)&xtcState.colorScale;
	xtcState.pColorScaleTex = (float*)&xtcState.colorScaleTex;

	xtcState.width = width;
	xtcState.height = height;
	xtcState.clearcol = 0;

	memset(&mdmaGSregs, 0, sizeof(mdmaGSregs));
	xtcShadeModel(XTC_SMOOTH);
	// everything disabled, always enable ztst in always mode
	mdmaGSregs.c1.test = GS_SET_TEST(0, 1, 0, 0, 0, 0, 1, 1);
	mdmaGSregs.c2.test = mdmaGSregs.c1.test;
	mdmaGSregs.texa = GS_SET_TEXA(0, 0, 0x80);
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

	xtcSetProjectionMatrix(identity);
	xtcSetViewMatrix(identity);
	xtcSetWorldMatrix(identity);

	const float scl = 128.0f/255.0f;
	xtcColorScale(1.0f, 1.0f, 1.0f, scl);
	xtcColorScaleTex(scl, scl, scl, scl);

	xtcMaterial *m = &xtcState.material;
	m->color.r = 1.0f;
	m->color.g = 1.0f;
	m->color.b = 1.0f;
	m->color.a = 1.0f;
	m->ambient = 1.0f;
	m->diffuse = 1.0f;
	m->specular = 1.0f;
	m->shininess = 1.0f;

	xtcSetAmbient(51, 51, 51);

	mdmaSetGsRegs(xtcState.list);
}
