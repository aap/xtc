#include "xtci.h"

#include <stdio.h>
#include <libgraph.h>

/*
 * Display & drawing buffers
 */

STRUCT(xtcgCrtState) {
	int16 inter, mode, ff;
};
static xtcgCrtState crtState;

uint32 xtcgMemStart;
const uint32 xtcgMemEnd = (4*1024*1024)/4/64;

// the SDK's takes a magnification instead of the raw fields
#define SCE_GS_SET_DISPLAY_RAW(dx, dy, magh, magv, dw, dh)\
    ((u_long)((dx)) | \
    ((u_long)((dy)) << 12) | \
    ((u_long)(magh) << 23)  | ((u_long)(magv) << 27) | \
    ((u_long)(dw) << 32)    | ((u_long)(dh) << 44))

static void
initDisp(xtcgDispBuffer *disp, int width, int height, int psm)
{
	int magh, magv;
	int dx, dy;
	int dw, dh;

	dx = crtState.mode == SCE_GS_NTSC ? 636 : 656;
	dy = crtState.mode == SCE_GS_NTSC ? 25 : 36;
	magh = 2560/width - 1;
	magv = 0;
	dw = 2560-1;
	dh = height-1;

	if(crtState.inter == SCE_GS_INTERLACE){
		dy *= 2;
		if(crtState.ff == SCE_GS_FRAME)
			dh = (dh+1)*2-1;
	}

	disp->pmode = SCE_GS_SET_PMODE(0, 1, 1, 1, 1, 0, 0x00);
	disp->bgcolor = 0x000000;
	disp->dispfb1 = 0;
	disp->dispfb2 = SCE_GS_SET_DISPFB(0, width/64, psm, 0, 0);
	disp->display1 = 0;
	disp->display2 = SCE_GS_SET_DISPLAY_RAW(dx, dy, magh, magv, dw, dh);
}

static void
initDraw(xtcgDrawBuffer *draw, int width, int height, int psm, int zpsm)
{
	MAKE128(draw->gifTag, GIF_AD,
		SCE_GIF_SET_TAG(8, 1, 0, 0, GIF_PACKED, 1));
	draw->frame1 = SCE_GS_SET_FRAME(0, width/64, psm, 0);
	draw->ad_frame1 = SCE_GS_FRAME_1;
	draw->frame2 = draw->frame1;
	draw->ad_frame2 = SCE_GS_FRAME_2;
	draw->zbuf1 = SCE_GS_SET_ZBUF(0, zpsm, 0);
	draw->ad_zbuf1 = SCE_GS_ZBUF_1;
	draw->zbuf2 = draw->zbuf1;
	draw->ad_zbuf2 = SCE_GS_ZBUF_2;
	draw->xyoffset1 = SCE_GS_SET_XYOFFSET((2048-width/2)<<4, (2048-height/2)<<4);
	draw->ad_xyoffset1 = SCE_GS_XYOFFSET_1;
	draw->xyoffset2 = draw->xyoffset1;
	draw->ad_xyoffset2 = SCE_GS_XYOFFSET_2;
	draw->scissor1 = SCE_GS_SET_SCISSOR(0, width-1, 0, height-1);
	draw->ad_scissor1 = SCE_GS_SCISSOR_1;
	draw->scissor2 = draw->scissor1;
	draw->ad_scissor2 = SCE_GS_SCISSOR_2;
}

static int psmsizemap[64] = {
	4,	// PSMCT32
	4,	// PSMCT24
	2,	// PSMCT16
	0, 0, 0, 0, 0, 0, 0,
	2,	// PSMCT16S
	0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	4,	// PSMZ32
	4,	// PSMZ24
	2,	// PSMZ16
	2,	// PSMZ16S
	0, 0, 0, 0, 0
};

void
xtcgInitBuffers(xtcgBuffers *buffers, int width, int height, int psm, int zpsm)
{
	uint32 fbsz, zbsz;
	uint32 fbp, zbp;

	fbsz = (width*height*psmsizemap[psm]/4 + 2047)/2048;
	zbsz = (width*height*psmsizemap[0x30|zpsm]/4 + 2047)/2048;
	fbp = fbsz;
	zbp = fbsz*2;

	// display buffers
	initDisp(&buffers->disp[0], width, height, psm);
	buffers->disp[1] = buffers->disp[0];
	buffers->disp[1].dispfb2 |= fbp;

	// draw buffers
	initDraw(&buffers->draw[0], width, height, psm, zpsm);
	buffers->draw[0].zbuf1 |= zbp;
	buffers->draw[0].zbuf2 |= zbp;
	buffers->draw[1] = buffers->draw[0];
	buffers->draw[1].frame1 |= fbp;
	buffers->draw[1].frame2 |= fbp;

	// everything above the buffers is free for textures
	xtcgMemStart = (2*fbsz + zbsz)*2048/64;
}

void
xtcgSetDisp(xtcgDispBuffer *disp)
{
	*GS_PMODE = disp->pmode;
	*GS_DISPFB1 = disp->dispfb1;
	*GS_DISPLAY1 = disp->display1;
	*GS_DISPFB2 = disp->dispfb2;
	*GS_DISPLAY2 = disp->display2;
	*GS_BGCOLOR = disp->bgcolor;
}

// the buffer carries its own GIF tag, so it goes straight to the GS.
// xtcSetDraw takes the same thing through the register cache instead.
void
xtcgSetDraw(mdmaList *list, xtcgDrawBuffer *draw)
{
	mdmaRef(list, draw, 9);
	mdmaVifDirect(list, 9, 0);
}

void
xtcgResetGraph(int inter, int mode, int ff)
{
	crtState.inter = inter;
	crtState.mode = mode;
	crtState.ff = ff;
	sceGsResetGraph(0, crtState.inter, crtState.mode, crtState.ff);
}

void
xtcgWaitVSynch(void)
{
	sceGsSyncV(0);
}


/*
 * GS register cache
 */

struct xtcgRegs xtcgRegs, xtcgCurRegs;

// every register the cache tracks, in the order both passes walk them
#define REGS \
	ONE(SCE_GS_FRAME_1, c1.frame) \
	ONE(SCE_GS_ZBUF_1, c1.zbuf) \
	ONE(SCE_GS_XYOFFSET_1, c1.xyoffset) \
	ONE(SCE_GS_SCISSOR_1, c1.scissor) \
	ONE(SCE_GS_TEST_1, c1.test) \
	ONE(SCE_GS_ALPHA_1, c1.alpha) \
	ONE(SCE_GS_TEX0_1, c1.tex0) \
	ONE(SCE_GS_TEX1_1, c1.tex1) \
	ONE(SCE_GS_CLAMP_1, c1.clamp) \
	ONE(SCE_GS_FRAME_2, c2.frame) \
	ONE(SCE_GS_ZBUF_2, c2.zbuf) \
	ONE(SCE_GS_XYOFFSET_2, c2.xyoffset) \
	ONE(SCE_GS_SCISSOR_2, c2.scissor) \
	ONE(SCE_GS_TEST_2, c2.test) \
	ONE(SCE_GS_ALPHA_2, c2.alpha) \
	ONE(SCE_GS_TEX0_2, c2.tex0) \
	ONE(SCE_GS_TEX1_2, c2.tex1) \
	ONE(SCE_GS_CLAMP_2, c2.clamp) \
	ONE(SCE_GS_PRMODE, prmode) \
	ONE(SCE_GS_FOGCOL, fogcol) \
	ONE(SCE_GS_TEXA, texa)

enum {
#define ONE(ad, var) +1
	xtcgNumRegs = 0 REGS
#undef ONE
};

void
xtcgSetRegs(mdmaList *list)
{
	mdmaCnt(list, 1 + xtcgNumRegs);
		mdmaBeginDirect(list, 1 + xtcgNumRegs, 0);
			mdmaBeginGifTagAD(list, xtcgNumRegs);
#define ONE(ad, var) mdmaAddAD(list, ad, xtcgRegs.var);
				REGS
#undef ONE
			mdmaEndGifTag(list);
		mdmaEndDirect(list);
	mdmaCloseTag(list);

	xtcgCurRegs = xtcgRegs;
}

// Only what actually changed. Counted first so the counts can be explicit:
// the alternative is reserving the tag and patching it once n is known, which
// is one out-of-order store into a chain that is usually uncached.
void
xtcgFlushRegs(mdmaList *list)
{
	struct { uint32 reg; uint64 val; } dirty[xtcgNumRegs];
	int n, i;

	n = 0;
#define ONE(ad, var) \
	if(xtcgCurRegs.var != xtcgRegs.var) { \
		xtcgCurRegs.var = xtcgRegs.var; \
		dirty[n].reg = ad; \
		dirty[n].val = xtcgRegs.var; \
		n++; \
	}
	REGS
#undef ONE

	if(n == 0)
		return;

	mdmaCnt(list, 1 + n);
		mdmaBeginDirect(list, 1 + n, 0);
			mdmaBeginGifTagAD(list, n);
				for(i = 0; i < n; i++)
					mdmaAddAD(list, dirty[i].reg, dirty[i].val);
			mdmaEndGifTag(list);
		mdmaEndDirect(list);
	mdmaCloseTag(list);
}

#undef REGS
