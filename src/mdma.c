#include "mdma.h"
#include <stdio.h>
#include <assert.h>
#include <libgraph.h>

sceDmaChan *mdmaGIF;
sceDmaChan *mdmaVIF;

void
mdmaInit(void)
{
	sceGsResetPath();
	sceDmaReset(1);

	mdmaGIF = sceDmaGetChan(SCE_DMA_GIF);
	mdmaGIF->chcr.TTE = 0;
	*GIF_MODE = 4;  // IMT intermittent mode

	mdmaVIF = sceDmaGetChan(SCE_DMA_VIF1);
	mdmaVIF->chcr.TTE = 1;
	mdmaVIF->chcr.TIE = 1;
}



/*
 * DMA lists and packets
 */


void
mdmaStart(mdmaList *list, uint128 *buf, uint32 size)
{
	list->p = buf;
	list->size = 0;
	list->limit = size;
}

void
mdmaFinish(mdmaList *list)
{
	uint128 t;
	MAKE128(t, 0x0, DMAend);
	mdmaAdd(list, t);
}

uint128*
mdmaSkip(mdmaList *list, uint32 n)
{
	uint128 *p;

	p = &list->p[list->size];
	list->size += n;
	assert(list->size <= list->limit);
	return p;
}

void
mdmaAdd(mdmaList *list, uint128 q)
{
	if(!(list->size < list->limit)) {
		printf("chain limit: %d %d\n", list->size < list->limit);
	}
	assert(list->size < list->limit);
	list->p[list->size++] = q;
}

void
mdmaAddD(mdmaList *list, uint64 d0, uint64 d1)
{
	uint128 t;
	MAKE128(t, d1, d0);
	mdmaAdd(list, t);
}

void
mdmaAddW(mdmaList *list, uint32 w0, uint32 w1, uint32 w2, uint32 w3)
{
	uint128 t;
	MAKEQ(t, w3, w2, w1, w0);
	mdmaAdd(list, t);
}

void
mdmaAddF(mdmaList *list, float f0, float f1, float f2, float f3)
{
	uint32 w0 = *(uint32*)&f0;
	uint32 w1 = *(uint32*)&f1;
	uint32 w2 = *(uint32*)&f2;
	uint32 w3 = *(uint32*)&f3;
	mdmaAddW(list, w0, w1, w2, w3);
}

void
mdmaAddGIFtag(mdmaList *list, int nloop, int eop, int pre, int prim, int flg, int nreg, uint64 regs)
{
	uint128 t;
	MAKE128(t, regs, SCE_GIF_SET_TAG(nloop, eop, pre,prim, flg, nreg));
	mdmaAdd(list, t);
}

void
mdmaAddAD(mdmaList *list, uint64 a, uint64 d)
{
	uint128 t;
	MAKE128(t, a, d);
	mdmaAdd(list, t);
}

void**
mdmaRef(mdmaList *list, void *data, uint16 qwc, uint32 w0, uint32 w1)
{
	uint128 t;
	MAKEQ(t, w1, w0, data, DMAref+qwc);
	mdmaAdd(list, t);
	return (void*)(((uint32*)&list->p[list->size-1])+1);
}

void**
mdmaRefDirect(mdmaList *list, void *data, uint16 qwc)
{
	return mdmaRef(list, data, qwc, VIFnop, VIFdirect+qwc);
}

void
mdmaCnt(mdmaList *list, uint16 qwc, uint32 w0, uint32 w1)
{
	uint128 t;
	MAKEQ(t, w1, w0, 0, DMAcnt+qwc);
	mdmaAdd(list, t);
}

void
mdmaCntDirect(mdmaList *list, uint16 qwc)
{
	mdmaCnt(list, qwc, VIFnop, VIFdirect+qwc);
}

void
mdmaRet(mdmaList *list, uint16 qwc, uint32 w0, uint32 w1)
{
	uint128 t;
	MAKEQ(t, w1, w0, 0, DMAret+qwc);
	mdmaAdd(list, t);
}

void
mdmaRetDirect(mdmaList *list, uint16 qwc)
{
	mdmaRet(list, qwc, VIFnop, VIFdirect+qwc);
}

void**
mdmaNext(mdmaList *list, void *next, uint16 qwc, uint32 w0, uint32 w1)
{
	uint128 t;
	MAKEQ(t, w1, w0, next, DMAnext+qwc);
	mdmaAdd(list, t);
	return (void*)(((uint32*)&list->p[list->size-1])+1);
}

void**
mdmaCall(mdmaList *list, uint16 qwc, void *addr, uint32 w0, uint32 w1)
{
	uint128 t;
	MAKEQ(t, w1, w0, addr, DMAcall+qwc);
	mdmaAdd(list, t);
	return (void*)(((uint32*)&list->p[list->size-1])+1);
}

void
mdmaSend(sceDmaChan *chan, mdmaList *list)
{
	FlushCache(0);
	sceDmaSend(chan, (void*)((uint32)list->p & ~0xF0000000));
}

void
mdmaSendSynch(sceDmaChan *chan, mdmaList *list)
{
	mdmaSend(chan, list);
	sceGsSyncPath(0, 0);
}




/*
 * Drawing & Display
 */

typedef struct GsCrtState GsCrtState;
struct GsCrtState
{
	int16 inter, mode, ff;
};

GsCrtState gsCrtState;

uint32 gsAllocPtr;
uint32 gsStart;
const uint32 gsEnd = (4*1024*1024)/4/64;

#define SCE_GS_SET_DISPLAY_RAW(dx, dy, magh, magv, dw, dh)\
    ((u_long)((dx)) | \
    ((u_long)((dy)) << 12) | \
    ((u_long)(magh) << 23)  | ((u_long)(magv) << 27) | \
    ((u_long)(dw) << 32)    | ((u_long)(dh) << 44))

void
mdmaInitDisp(mdmaDispBuffer *disp, int width, int height, int psm)
{
	int magh, magv;
	int dx, dy;
	int dw, dh;

	dx = gsCrtState.mode == SCE_GS_NTSC ? 636 : 656;
	dy = gsCrtState.mode == SCE_GS_NTSC ? 25 : 36;
	magh = 2560/width - 1;
	magv = 0;
	dw = 2560-1;
	dh = height-1;

	if(gsCrtState.inter == SCE_GS_INTERLACE){
		dy *= 2;
		if(gsCrtState.ff == SCE_GS_FRAME)
			dh = (dh+1)*2-1;
	}

	disp->pmode = SCE_GS_SET_PMODE(0, 1, 1, 1, 1, 0, 0x00);
	disp->bgcolor = 0x000000;
	disp->dispfb1 = 0;
	disp->dispfb2 = SCE_GS_SET_DISPFB(0, width/64, psm, 0, 0);
	disp->display1 = 0;
	disp->display2 = SCE_GS_SET_DISPLAY_RAW(dx, dy, magh, magv, dw, dh);
}

void
mdmaInitDraw(mdmaDrawBuffer *draw, int width, int height, int psm, int zpsm)
{
	MAKE128(draw->gifTag, 0xe,
		SCE_GIF_SET_TAG(8, 1, 0, 0, SCE_GIF_PACKED, 1));
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

int psmsizemap[64] = {
	4,      // PSMCT32
	4,      // PSMCT24
	2,      // PSMCT16
	0, 0, 0, 0, 0, 0, 0,
	2,      // PSMCT16S
	0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	4,      // PSMZ32
	4,      // PSMZ24
	2,      // PSMZ16
	2,      // PSMZ16S
	0, 0, 0, 0, 0
};

void
mdmaInitBuffers(mdmaBuffers *buffers, int width, int height, int psm, int zpsm)
{
	uint fbsz, zbsz;
	uint fbp, zbp;
	fbsz = (width*height*psmsizemap[psm]/4 + 2047)/2048;
	zbsz = (width*height*psmsizemap[0x30|zpsm]/4 + 2047)/2048;
	gsAllocPtr = (2*fbsz + zbsz)*4*2048;
	fbp = fbsz;
	zbp = fbsz*2;

	// display buffers
	mdmaInitDisp(&buffers->disp[0], width, height, psm);
	buffers->disp[1] = buffers->disp[0];
	buffers->disp[1].dispfb2 |= fbp;

	// draw buffers
	mdmaInitDraw(&buffers->draw[0], width, height, psm, zpsm);
	buffers->draw[0].zbuf1 |= zbp;
	buffers->draw[0].zbuf2 |= zbp;
	buffers->draw[1] = buffers->draw[0];
	buffers->draw[1].frame1 |= fbp;
	buffers->draw[1].frame2 |= fbp;

	gsStart = gsAllocPtr/4/64;
}

void
mdmaSetDisp(mdmaDispBuffer *disp)
{
	*GS_PMODE = disp->pmode;
	*GS_DISPFB1 = disp->dispfb1;
	*GS_DISPLAY1 = disp->display1;
	*GS_DISPFB2 = disp->dispfb2;
	*GS_DISPLAY2 = disp->display2;
	*GS_BGCOLOR = disp->bgcolor;
}

void
mdmaSetDraw(mdmaList *list, mdmaDrawBuffer *draw)
{
	mdmaRefDirect(list, draw, 9);
}

void
mdmaResetGraph(int inter, int mode, int ff)
{
	gsCrtState.inter = inter;
	gsCrtState.mode = mode;
	gsCrtState.ff = ff;
	sceGsResetGraph(0, gsCrtState.inter, gsCrtState.mode, gsCrtState.ff);
}

void
mdmaWaitVSynch(void)
{
		sceGsSyncV(0);
}




struct mdmaGSregs mdmaGSregs, mdmaCurGSregs;

void
mdmaSetGsRegs(mdmaList *list)
{
	mdmaCntDirect(list, 1 + 11);
	mdmaAddGIFtag(list, 11, 1, 0,0, SCE_GIF_PACKED, 1, 0xe);

	mdmaAddAD(list, SCE_GS_FRAME_1, mdmaGSregs.c1.frame);
	mdmaAddAD(list, SCE_GS_ZBUF_1, mdmaGSregs.c1.zbuf);
	mdmaAddAD(list, SCE_GS_XYOFFSET_1, mdmaGSregs.c1.xyoffset);
	mdmaAddAD(list, SCE_GS_SCISSOR_1, mdmaGSregs.c1.scissor);
	mdmaAddAD(list, SCE_GS_TEST_1, mdmaGSregs.c1.test);
	mdmaAddAD(list, SCE_GS_ALPHA_1, mdmaGSregs.c1.alpha);
	mdmaAddAD(list, SCE_GS_TEX0_1, mdmaGSregs.c1.tex0);
	mdmaAddAD(list, SCE_GS_TEX1_1, mdmaGSregs.c1.tex1);
	mdmaAddAD(list, SCE_GS_CLAMP_1, mdmaGSregs.c1.clamp);

	mdmaAddAD(list, SCE_GS_FRAME_2, mdmaGSregs.c2.frame);
	mdmaAddAD(list, SCE_GS_ZBUF_2, mdmaGSregs.c2.zbuf);
	mdmaAddAD(list, SCE_GS_XYOFFSET_2, mdmaGSregs.c2.xyoffset);
	mdmaAddAD(list, SCE_GS_SCISSOR_2, mdmaGSregs.c2.scissor);
	mdmaAddAD(list, SCE_GS_TEST_2, mdmaGSregs.c2.test);
	mdmaAddAD(list, SCE_GS_ALPHA_2, mdmaGSregs.c2.alpha);
	mdmaAddAD(list, SCE_GS_TEX0_2, mdmaGSregs.c2.tex0);
	mdmaAddAD(list, SCE_GS_TEX1_2, mdmaGSregs.c2.tex1);
	mdmaAddAD(list, SCE_GS_CLAMP_2, mdmaGSregs.c2.clamp);

	mdmaAddAD(list, SCE_GS_PRMODE, mdmaGSregs.prmode);
	mdmaAddAD(list, SCE_GS_FOGCOL, mdmaGSregs.fogcol);
	mdmaAddAD(list, SCE_GS_TEXA, mdmaGSregs.texa);

	mdmaCurGSregs = mdmaGSregs;
}

void
mdmaFlushGsRegs(mdmaList *list)
{
	mdmaList tag;
	int n;

	mdmaStart(&tag, mdmaSkip(list, 2), 2);

	n = 0;

#define FLUSHONE(ad, var) \
	if(mdmaCurGSregs.var != mdmaGSregs.var) { \
		mdmaCurGSregs.var = mdmaGSregs.var; \
		mdmaAddAD(list, ad, mdmaGSregs.var); \
		n++; \
	}

	FLUSHONE(SCE_GS_FRAME_1, c1.frame)
	FLUSHONE(SCE_GS_ZBUF_1, c1.zbuf)
	FLUSHONE(SCE_GS_XYOFFSET_1, c1.xyoffset)
	FLUSHONE(SCE_GS_SCISSOR_1, c1.scissor)
	FLUSHONE(SCE_GS_TEST_1, c1.test)
	FLUSHONE(SCE_GS_ALPHA_1, c1.alpha)
	FLUSHONE(SCE_GS_TEX0_1, c1.tex0)
	FLUSHONE(SCE_GS_TEX1_1, c1.tex1)
	FLUSHONE(SCE_GS_CLAMP_1, c1.clamp)

	FLUSHONE(SCE_GS_FRAME_2, c2.frame)
	FLUSHONE(SCE_GS_ZBUF_2, c2.zbuf)
	FLUSHONE(SCE_GS_XYOFFSET_2, c2.xyoffset)
	FLUSHONE(SCE_GS_SCISSOR_2, c2.scissor)
	FLUSHONE(SCE_GS_TEST_2, c2.test)
	FLUSHONE(SCE_GS_ALPHA_2, c2.alpha)
	FLUSHONE(SCE_GS_TEX0_2, c2.tex0)
	FLUSHONE(SCE_GS_TEX1_2, c2.tex1)
	FLUSHONE(SCE_GS_CLAMP_2, c2.clamp)

	FLUSHONE(SCE_GS_PRMODE, prmode)
	FLUSHONE(SCE_GS_FOGCOL, fogcol)
	FLUSHONE(SCE_GS_TEXA, texa)

#undef FLUSHONE

	if(n) {
		mdmaCntDirect(&tag, 1 + n);
		mdmaAddGIFtag(&tag, n, 1, 0,0, SCE_GIF_PACKED, 1, 0xe);
	} else {
		list->size -= 2;
	}
}
