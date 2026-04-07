#include "mdma.h"

#include "gs.h"
#include "kernel_shim.h"
#include "util.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define GS_REG_PMODE (0x12000000)
#define GS_REG_SMODE2 (0x12000020)
#define GS_REG_DISPFB1 (0x12000070)
#define GS_REG_DISPLAY1 (0x12000080)
#define GS_REG_DISPFB2 (0x12000090)
#define GS_REG_DISPLAY2 (0x120000a0)
#define GS_REG_EXTBUF (0x120000b0)
#define GS_REG_EXTDATA (0x120000c0)
#define GS_REG_EXTWRITE (0x120000d0)
#define GS_REG_BGCOLOR (0x120000e0)
#define GS_REG_CSR (0x12001000)
#define GS_REG_CSR_RESET BIT(9)
#define GS_REG_IMR (0x12001010)
#define GS_REG_BUSDIR (0x12001040)
#define GS_REG_SIGLBLID (0x12001080)

#define GIF_REG_CTRL (0x10003000)
#define GIF_REG_CTRL_RST BIT(0)
#define GIF_REG_CTRL_PSE BIT(3)

#define GIF_REG_MODE (0x10003010)
#define GIF_REG_MODE_M3R BIT(0)
#define GIF_REG_MODE_IMT BIT(2)
#define GIF_REG_STAT (0x10003020)
#define GIF_REG_STAT_APATH GENMASK(11, 10)

#define VIF1_STAT 0x10003c00
#define VIF1_STAT_VPS GENMASK(1, 0)
#define VIF1_STAT_VEW BIT(2)
#define VIF1_STAT_VGW BIT(3)
#define VIF1_STAT_MRK BIT(6)
#define VIF1_STAT_DBF BIT(7)
#define VIF1_STAT_VSS BIT(8)
#define VIF1_STAT_VFS BIT(9)
#define VIF1_STAT_VIS BIT(10)
#define VIF1_STAT_INT BIT(11)
#define VIF1_STAT_ER0 BIT(12)
#define VIF1_STAT_ER1 BIT(13)
#define VIF1_STAT_FDR BIT(23)
#define VIF1_STAT_FQC GENMASK(28, 24)

#define VIF1_FBRST (0x10003c10)
#define VIF1_FBRST_RST BIT(0)
#define VIF1_FBRST_FBK BIT(1)
#define VIF1_FBRST_STP BIT(2)
#define VIF1_FBRST_STC BIT(3)

#define VIF1_ERR 0x10003c20
#define VIF1_ERR_MII BIT(0)
#define VIF1_ERR_ME0 BIT(1)
#define VIF1_ERR_ME1 BIT(2)

#define VIF1_FIFO 0x10005000

#define DMA_REG_VIF0(r) (0x10008000 + (r))
#define DMA_REG_VIF1(r) (0x10009000 + (r))
#define DMA_REG_GIF(r) (0x1000A000 + (r))

#define DMA_CHCR 0x0
#define DMA_MADR 0x10
#define DMA_QWC 0x20
#define DMA_TADR 0x30
#define DMA_ASR0 0x40
#define DMA_ASR1 0x50
#define DMA_SADR 0x80

#define DMA_REG_CHCR_DIR BIT(0)
#define DMA_REG_CHCR_DIR_TO_MEM 0
#define DMA_REG_CHCR_DIR_FROM_MEM 1
#define DMA_REG_CHCR_MOD GENMASK(3, 2)
#define DMA_REG_CHCR_MOD_NORM 0
#define DMA_REG_CHCR_MOD_CHAIN 1
#define DMA_REG_CHCR_MOD_INTRL 2
#define DMA_REG_CHCR_ASP GENMASK(5, 4)
#define DMA_REG_CHCR_TTE BIT(6)
#define DMA_REG_CHCR_TIE BIT(7)
#define DMA_REG_CHCR_STR BIT(8)
#define DMA_REG_CHCR_TAG GENMASK(31, 16)

#define DMA_REG_CTRL (0x1000E000)
#define DMA_REG_CTRL_DMAE BIT(0)
#define DMA_REG_CTRL_RELE BIT(1)
#define DMA_REG_CTRL_MFD GENMASK(3, 2)
#define DMA_REG_CTRL_STS GENMASK(5, 4)
#define DMA_REG_CTRL_STD GENMASK(7, 6)
#define DMA_REG_CTRL_RCYC GENMASK(10, 8)

#define DMA_REG_STAT (0x1000E010)
#define DMA_REG_STAT_CIS GENMASK(9, 0)
#define DMA_REG_STAT_SIS BIT(13)
#define DMA_REG_STAT_MEIS BIT(14)
#define DMA_REG_STAT_BEIS BIT(15)
#define DMA_REG_STAT_CIM GENMASK(25, 16)
#define DMA_REG_STAT_SIM BIT(29)
#define DMA_REG_STAT_MEIM BIT(30)

#define DMA_REG_PCR (0x1000e020)
#define DMA_REG_PCR_CPC GENMASK(9, 0)
#define DMA_REG_PCR_CDE GENMASK(25, 16)
#define DMA_REG_PCR_PCE BIT(31)

#define DMA_REG_SQWC (0x1000e030)
#define DMA_REG_SQWC_SQWC GENMASK(7, 0)
#define DMA_REG_SQWC_TQWC GENMASK(23, 16)

#define DMA_REG_RBOR (0x1000e050)
#define DMA_REG_RBOR_ADDR GENMASK(30, 0)

#define DMA_REG_RBSR (0x1000e040)
#define DMA_REG_RBSR_RMSK GENMASK(30, 4)

#define DMA_REG_STADR (0x1000e060)
#define DMA_REG_STADR_ADDR GENMASK(30, 0)

#define DMA_REG_ENABLEW (0x1000f590)
#define DMA_REG_ENABLEW_CPND BIT(16)

#define DMA_REG_ENABLER (0x1000f520)
#define DMA_REG_ENABLER_CPND BIT(16)

#define INTC_REG_I_STAT (0x1000f000)
#define INTC_REG_I_STAT_GS BIT(0)
#define INTC_REG_I_STAT_SBUS BIT(1)
#define INTC_REG_I_STAT_VBON BIT(2)
#define INTC_REG_I_STAT_VBOFF BIT(3)
#define INTC_REG_I_STAT_VIF0 BIT(4)
#define INTC_REG_I_STAT_VIF1 BIT(5)
#define INTC_REG_I_STAT_VU0 BIT(6)
#define INTC_REG_I_STAT_VU1 BIT(7)
#define INTC_REG_I_STAT_IPU BIT(8)
#define INTC_REG_I_STAT_TIM0 BIT(9)
#define INTC_REG_I_STAT_TIM1 BIT(10)
#define INTC_REG_I_STAT_TIM2 BIT(11)
#define INTC_REG_I_STAT_TIM3 BIT(12)
#define INTC_REG_I_STAT_SFIFO BIT(13)
#define INTC_REG_I_STAT_VU0WD BIT(13)

#define INTC_REG_I_MASK (0x1000f010)

// VPU Control register
#define CCR_VPU_STAT 29
#define CCR_VPU_STAT_VBS1 BIT(8)

static void mdmaResetPath();
static void mdmaResetDma();

void
mdmaInit(void)
{
	mdmaResetPath();
	mdmaResetDma();

	clear32(DMA_REG_GIF(DMA_CHCR), DMA_REG_CHCR_TTE);
	write32(GIF_REG_MODE, GIF_REG_MODE_IMT); // IMT intermittet mode
}

static void
mdmaResetPath()
{
	// VIFcode reset routine
	static const uint32 init_vif_regs[] __attribute__((aligned(128))) = {
		VIF_STCYCL(4, 4),  // 0x01000404
		VIF_STMASK(),      // 0x20000000
		0x00000000,        // stmask arg
		VIF_STMOD(0),      // 0x05000000
		VIF_MSKPATH3(0),   // 0x06000000
		VIF_BASE(0),       // 0x03000000
		VIF_OFFSET(0),     // 0x02000000
		VIF_ITOP(0),       // 0x04000000
	};

	write32(VIF1_FBRST, VIF1_FBRST_RST); // reset vif1
	write32(VIF1_ERR, VIF1_ERR_ME0);     // mask tag mismatch

	// Reset VU1 by setting RS1 in FBRST
	__asm__ volatile("sync          \n"
			 "cfc2 $2, $28  \n"
			 "ori $2, 0x200 \n"
			 "ctc2 $2, $28  \n"
			 "sync.p        \n"
			 :
			 :
			 : "2");

	// Put init_vif_regs into the VIF1 FIFO
	// If only TImode worked so we could do this from C...
	// idk if it has to be lq/sq though, maybe ld/sd works
	__asm__ volatile("lq $2, 0(%0)    \n"
			 "sq $2, 0(%1)    \n"
			 "lq $3, 0x10(%0) \n"
			 "sq $3, 0(%1)    \n"
			 :
			 : "d"(init_vif_regs), "d"(VIF1_FIFO)
			 : "2", "3");

	write32(GIF_REG_CTRL, GIF_REG_CTRL_RST); // reset gif
}

static void
mdmaResetDma()
{
	uint32 stat;

	write32(DMA_REG_VIF0(DMA_CHCR), 0);
	write32(DMA_REG_VIF0(DMA_MADR), 0);
	write32(DMA_REG_VIF0(DMA_TADR), 0);
	write32(DMA_REG_VIF0(DMA_SADR), 0);
	write32(DMA_REG_VIF0(DMA_ASR0), 0);
	write32(DMA_REG_VIF0(DMA_ASR1), 0);

	write32(DMA_REG_VIF1(DMA_CHCR), 0);
	write32(DMA_REG_VIF1(DMA_MADR), 0);
	write32(DMA_REG_VIF1(DMA_TADR), 0);
	write32(DMA_REG_VIF1(DMA_SADR), 0);
	write32(DMA_REG_VIF1(DMA_ASR0), 0);
	write32(DMA_REG_VIF1(DMA_ASR1), 0);

	write32(DMA_REG_GIF(DMA_CHCR), 0);
	write32(DMA_REG_GIF(DMA_MADR), 0);
	write32(DMA_REG_GIF(DMA_TADR), 0);
	write32(DMA_REG_GIF(DMA_SADR), 0);
	write32(DMA_REG_GIF(DMA_ASR0), 0);
	write32(DMA_REG_GIF(DMA_ASR1), 0);

	write32(DMA_REG_STAT, 0xff1f);
	stat = read32(DMA_REG_STAT);
	write32(DMA_REG_STAT, stat & 0xff1f0000);

	write32(DMA_REG_CTRL, 0);
	write32(DMA_REG_PCR, 0);
	write32(DMA_REG_SQWC, 0);
	write32(DMA_REG_RBOR, 0);
	write32(DMA_REG_RBSR, 0);

	set32(DMA_REG_CTRL, DMA_REG_CTRL_DMAE);
}

static void
mdmaPause(DMA_CHAN chan)
{
	uint32 oldintr = DIntr();
	uint32 enabler;

	enabler = read32(DMA_REG_ENABLER);
	if ((enabler & DMA_REG_ENABLER_CPND) == 0) {
		set32(DMA_REG_ENABLER, DMA_REG_ENABLER_CPND);
	}

	clear32(chan + DMA_CHCR, DMA_REG_CHCR_STR);

	write32(DMA_REG_ENABLER, enabler);

	if (oldintr)
		EIntr();
}

static void
mdmaWait(DMA_CHAN chan)
{
	int32 timeout = 0xffffff;

	while (read32(chan + DMA_CHCR) & DMA_REG_CHCR_STR) {
		if (timeout < 0) {
			printf("dma sync timeout\n");
			mdmaPause(chan);
		}
		timeout--;
	}
}

static uint32
mdmaCheckAddr(uint32 data)
{
	// Test if addr is on scratchpad
	if ((data >> 28) == 7) {
		// if so, set SPR bit
		data = (data & 0xfffffff) | 0x80000000;
	}

	return data;
}

static int
mdmaSyncPath()
{
	uint32 vifmask;

	while (read32(DMA_REG_VIF1(DMA_CHCR)) & DMA_REG_CHCR_STR)
		;

	while (read32(DMA_REG_GIF(DMA_CHCR)) & DMA_REG_CHCR_STR)
		;

	vifmask = VIF1_STAT_FQC | VIF1_STAT_VPS;
	while (read32(VIF1_STAT) & vifmask)
		;

	// check vbs1
	while (cfc2(CCR_VPU_STAT) & CCR_VPU_STAT_VBS1)
		;

	while (read32(GIF_REG_STAT) & GIF_REG_STAT_APATH)
		;

	return 0;
}

/* default memory management */
void *(*mdmaMalloc)(size_t sz) = malloc;
void *(*mdmaRealloc)(void *p, size_t sz) = realloc;
void (*mdmaFree)(void *p) = free;

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
	t = MAKE128(0x0, DMAend);
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
		printf("chain limit: %d %d\n", list->size, list->limit);
	}
	assert(list->size < list->limit);
	list->p[list->size++] = q;
}

void
mdmaAddD(mdmaList *list, uint64 d0, uint64 d1)
{
	uint128 t;
	t = MAKE128(d1, d0);
	mdmaAdd(list, t);
}

void
mdmaAddW(mdmaList *list, uint32 w0, uint32 w1, uint32 w2, uint32 w3)
{
	uint128 t;
	t = MAKEQ(w3, w2, w1, w0);
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
	t = MAKE128(regs, GIF_SET_TAG(nloop, eop, pre,prim, flg, nreg));
	mdmaAdd(list, t);
}

void
mdmaAddAD(mdmaList *list, uint64 a, uint64 d)
{
	uint128 t;
	t = MAKE128(a, d);
	mdmaAdd(list, t);
}

void**
mdmaRef(mdmaList *list, void *data, uint16 qwc, uint32 w0, uint32 w1)
{
	uint128 t;
	t = MAKEQ(w1, w0, data, DMAref+qwc);
	mdmaAdd(list, t);
	return (void*)(((uint32*)&list->p[list->size-1])+1);
}

void**
mdmaRefDirect(mdmaList *list, void *data, uint16 qwc)
{
	return mdmaRef(list, data, qwc, VIF_NOP(), VIF_DIRECT(qwc));
}

void
mdmaCnt(mdmaList *list, uint16 qwc, uint32 w0, uint32 w1)
{
	uint128 t;
	t = MAKEQ(w1, w0, 0, DMAcnt+qwc);
	mdmaAdd(list, t);
}

void
mdmaCntDirect(mdmaList *list, uint16 qwc)
{
	mdmaCnt(list, qwc, VIF_NOP(), VIF_DIRECT(qwc));
}

void
mdmaRet(mdmaList *list, uint16 qwc, uint32 w0, uint32 w1)
{
	uint128 t;
	t = MAKEQ(w1, w0, 0, DMAret+qwc);
	mdmaAdd(list, t);
}

void
mdmaRetDirect(mdmaList *list, uint16 qwc)
{
	mdmaRet(list, qwc, VIFnop, VIF_DIRECT(qwc));
}

void**
mdmaNext(mdmaList *list, void *next, uint16 qwc, uint32 w0, uint32 w1)
{
	uint128 t;
	t = MAKEQ(w1, w0, next, DMAnext+qwc);
	mdmaAdd(list, t);
	return (void*)(((uint32*)&list->p[list->size-1])+1);
}

void**
mdmaCall(mdmaList *list, uint16 qwc, void *addr, uint32 w0, uint32 w1)
{
	uint128 t;
	t = MAKEQ(w1, w0, addr, DMAcall+qwc);
	mdmaAdd(list, t);
	return (void*)(((uint32*)&list->p[list->size-1])+1);
}

void
mdmaSend(DMA_CHAN chan, mdmaList *list)
{
	uint32 chcr = 0, addr = 0;

	FlushCache(0);

	addr = mdmaCheckAddr((uint32)list->p);
	mdmaWait(chan);

	write32(chan + DMA_TADR, (uint32)addr);
	write32(chan + DMA_QWC, 0);

	chcr |= FIELD_PREP(DMA_REG_CHCR_DIR, DMA_REG_CHCR_DIR_FROM_MEM);
	chcr |= FIELD_PREP(DMA_REG_CHCR_MOD, DMA_REG_CHCR_MOD_CHAIN);
	chcr |= FIELD_PREP(DMA_REG_CHCR_TTE, 1);
	chcr |= FIELD_PREP(DMA_REG_CHCR_TIE, 1);
	chcr |= FIELD_PREP(DMA_REG_CHCR_STR, 1);
	write32(chan + DMA_CHCR, chcr);
}

void
mdmaSendSynch(DMA_CHAN chan, mdmaList *list)
{
	mdmaSend(chan, list);
	mdmaSyncPath();
}

/*
 * Drawing & Display
 */

typedef struct GsCrtState GsCrtState;
struct GsCrtState {
	int16 inter, mode, ff;
};

GsCrtState gsCrtState;

uint32 gsAllocPtr;
uint32 gsStart;
const uint32 gsEnd = (4*1024*1024)/4/64;

void
mdmaInitDisp(mdmaDispBuffer *disp, int width, int height, int psm)
{
	int magh, magv;
	int dx, dy;
	int dw, dh;

	dx = gsCrtState.mode == GS_MODE_NTSC ? 636 : 656;
	dy = gsCrtState.mode == GS_MODE_NTSC ? 25 : 36;
	magh = 2560/width - 1;
	magv = 0;
	dw = 2560-1;
	dh = height-1;

	if (gsCrtState.inter == GS_INTERLACED) {
		dy *= 2;
		if (gsCrtState.ff == GS_FFMD_FRAME)
			dh = (dh+1)*2-1;
	}

	// disp->pmode = GS_SET_PMODE(0, 1, 1, 1, 1, 0, 0x00);
	disp->pmode = GS_SET_PMODE(0, 1, 1, 1, 0, 0x00);
	disp->bgcolor = 0x000000;
	disp->dispfb1 = 0;
	disp->dispfb2 = GS_SET_DISPFB2(0, width / 64, psm, 0, 0);
	disp->display1 = 0;
	disp->display2 = GS_SET_DISPLAY(dx, dy, magh, magv, dw, dh);
}

void
mdmaInitDraw(mdmaDrawBuffer *draw, int width, int height, int psm, int zpsm)
{
	draw->gifTag = MAKE128(0xe, GIF_SET_TAG(8, 1, 0, 0, GS_GIF_PACKED, 1));
	draw->frame1 = GS_SET_FRAME(0, width / 64, psm, 0);
	draw->ad_frame1 = GS_REG_FRAME_1;
	draw->frame2 = draw->frame1;
	draw->ad_frame2 = GS_REG_FRAME_2;
	draw->zbuf1 = GS_SET_ZBUF(0, zpsm, 0);
	draw->ad_zbuf1 = GS_REG_ZBUF_1;
	draw->zbuf2 = draw->zbuf1;
	draw->ad_zbuf2 = GS_REG_ZBUF_2;
	draw->xyoffset1 = GS_SET_XYOFFSET((2048 - width / 2) << 4, (2048 - height / 2) << 4);
	draw->ad_xyoffset1 = GS_REG_XYOFFSET_1;
	draw->xyoffset2 = draw->xyoffset1;
	draw->ad_xyoffset2 = GS_REG_XYOFFSET_2;
	draw->scissor1 = GS_SET_SCISSOR(0, width - 1, 0, height - 1);
	draw->ad_scissor1 = GS_REG_SCISSOR_1;
	draw->scissor2 = draw->scissor1;
	draw->ad_scissor2 = GS_REG_SCISSOR_2;
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
	write64(GS_REG_PMODE, disp->pmode);
	write64(GS_REG_DISPFB1, disp->dispfb1);
	write64(GS_REG_DISPLAY1, disp->display1);
	write64(GS_REG_DISPFB2, disp->dispfb2);
	write64(GS_REG_DISPLAY2, disp->display2);
	write64(GS_REG_BGCOLOR, disp->bgcolor);
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

	write64(GS_REG_CSR, GS_REG_CSR_RESET);
	GsPutIMR(0xff00);
	SetGsCrt(inter & 1, mode & 0xff, ff & 1);
}

void
mdmaWaitVSynch(void)
{
	uint32 oldintr = DIntr();
	write32(INTC_REG_I_STAT, INTC_REG_I_STAT_VBON);
	__asm__ volatile("sync");
	if (oldintr)
		EIntr();

	// wait for vbon flag to come back on
	while ((read32(INTC_REG_I_STAT) & INTC_REG_I_STAT_VBON) == 0)
		;

	oldintr = DIntr();
	write32(INTC_REG_I_STAT, INTC_REG_I_STAT_VBON);
	__asm__ volatile("sync");
	if (oldintr)
		EIntr();
}




struct mdmaGSregs mdmaGSregs, mdmaCurGSregs;

void
mdmaSetGsRegs(mdmaList *list)
{
	mdmaCntDirect(list, 1 + 11);
	mdmaAddGIFtag(list, 11, 1, 0, 0, GS_GIF_PACKED, 1, 0xe);

	mdmaAddAD(list, GS_REG_FRAME_1, mdmaGSregs.c1.frame);
	mdmaAddAD(list, GS_REG_ZBUF_1, mdmaGSregs.c1.zbuf);
	mdmaAddAD(list, GS_REG_XYOFFSET_1, mdmaGSregs.c1.xyoffset);
	mdmaAddAD(list, GS_REG_SCISSOR_1, mdmaGSregs.c1.scissor);
	mdmaAddAD(list, GS_REG_TEST_1, mdmaGSregs.c1.test);
	mdmaAddAD(list, GS_REG_ALPHA_1, mdmaGSregs.c1.alpha);
	mdmaAddAD(list, GS_REG_TEX0_1, mdmaGSregs.c1.tex0);
	mdmaAddAD(list, GS_REG_TEX1_1, mdmaGSregs.c1.tex1);
	mdmaAddAD(list, GS_REG_CLAMP_1, mdmaGSregs.c1.clamp);

	mdmaAddAD(list, GS_REG_FRAME_2, mdmaGSregs.c2.frame);
	mdmaAddAD(list, GS_REG_ZBUF_2, mdmaGSregs.c2.zbuf);
	mdmaAddAD(list, GS_REG_XYOFFSET_2, mdmaGSregs.c2.xyoffset);
	mdmaAddAD(list, GS_REG_SCISSOR_2, mdmaGSregs.c2.scissor);
	mdmaAddAD(list, GS_REG_TEST_2, mdmaGSregs.c2.test);
	mdmaAddAD(list, GS_REG_ALPHA_2, mdmaGSregs.c2.alpha);
	mdmaAddAD(list, GS_REG_TEX0_2, mdmaGSregs.c2.tex0);
	mdmaAddAD(list, GS_REG_TEX1_2, mdmaGSregs.c2.tex1);
	mdmaAddAD(list, GS_REG_CLAMP_2, mdmaGSregs.c2.clamp);

	mdmaAddAD(list, GS_REG_PRMODE, mdmaGSregs.prmode);
	mdmaAddAD(list, GS_REG_FOGCOL, mdmaGSregs.fogcol);
	mdmaAddAD(list, GS_REG_TEXA, mdmaGSregs.texa);

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

	FLUSHONE(GS_REG_FRAME_1, c1.frame)
	FLUSHONE(GS_REG_ZBUF_1, c1.zbuf)
	FLUSHONE(GS_REG_XYOFFSET_1, c1.xyoffset)
	FLUSHONE(GS_REG_SCISSOR_1, c1.scissor)
	FLUSHONE(GS_REG_TEST_1, c1.test)
	FLUSHONE(GS_REG_ALPHA_1, c1.alpha)
	FLUSHONE(GS_REG_TEX0_1, c1.tex0)
	FLUSHONE(GS_REG_TEX1_1, c1.tex1)
	FLUSHONE(GS_REG_CLAMP_1, c1.clamp)

	FLUSHONE(GS_REG_FRAME_2, c2.frame)
	FLUSHONE(GS_REG_ZBUF_2, c2.zbuf)
	FLUSHONE(GS_REG_XYOFFSET_2, c2.xyoffset)
	FLUSHONE(GS_REG_SCISSOR_2, c2.scissor)
	FLUSHONE(GS_REG_TEST_2, c2.test)
	FLUSHONE(GS_REG_ALPHA_2, c2.alpha)
	FLUSHONE(GS_REG_TEX0_2, c2.tex0)
	FLUSHONE(GS_REG_TEX1_2, c2.tex1)
	FLUSHONE(GS_REG_CLAMP_2, c2.clamp)

	FLUSHONE(GS_REG_PRMODE, prmode)
	FLUSHONE(GS_REG_FOGCOL, fogcol)
	FLUSHONE(GS_REG_TEXA, texa)

#undef FLUSHONE

	if(n) {
		mdmaCntDirect(&tag, 1 + n);
		mdmaAddGIFtag(&tag, n, 1, 0, 0, GS_GIF_PACKED, 1, 0xe);
	} else {
		list->size -= 2;
	}
}
