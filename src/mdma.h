#ifndef MDMA_H_
#define MDMA_H_

#include "types.h"

#include <string.h>
#include <sys/types.h>

#define nil NULL
#ifndef nelem
#define nelem(arr) (sizeof(arr)/sizeof(arr[0]))
#endif

typedef enum DMA_CHAN DMA_CHAN;
enum DMA_CHAN {
	DMA_CHAN_VIF0 = 0x10008000,
	DMA_CHAN_VIF1 = 0x10009000,
	DMA_CHAN_GIF = 0x1000A000,
};

enum {
	IntFlg  = 0x80000000,

	DMArefe = 0x00000000,
	DMAcnt  = 0x10000000,
	DMAnext = 0x20000000,
	DMAref  = 0x30000000,
	DMArefs = 0x40000000,
	DMAcall = 0x50000000,
	DMAret  = 0x60000000,
	DMAend  = 0x70000000,

	VIFnop  = 0,
	VIFstcycl       = 0x01,
	VIFoffset       = 0x02,
	VIFbase         = 0x03,
	VIFitop         = 0x04,
	VIFstmod        = 0x05,
	VIFmskpath3     = 0x06,
	VIFmark         = 0x07,
	VIFflushe       = 0x10,
	VIFflush        = 0x11,
	VIFflusha       = 0x13,
	VIFmscal        = 0x14,
	VIFmscalf       = 0x15,
	VIFmscnt        = 0x17,
	VIFstmask       = 0x20,
	VIFstrow        = 0x30,
	VIFstcol        = 0x31,
	VIFmpg          = 0x4A,
	VIFdirect       = 0x50,
	VIFdirecthl     = 0x51,

	V4_32 = 0x6C
};

enum {
	UNPACK_V1_32 = 0x60000000,
	UNPACK_V1_16 = 0x61000000,
	UNPACK_V1_8  = 0x62000000,

	UNPACK_V2_32 = 0x64000000,
	UNPACK_V2_16 = 0x65000000,
	UNPACK_V2_8  = 0x66000000,

	UNPACK_V3_32 = 0x68000000,
	UNPACK_V3_16 = 0x69000000,
	UNPACK_V3_8  = 0x6A000000,

	UNPACK_V4_32 = 0x6C000000,
	UNPACK_V4_16 = 0x6D000000,
	UNPACK_V4_8  = 0x6E000000,

	UNPACK_V4_5  = 0x6F000000,

	UNPACK_USN   = 0x00004000,
	UNPACK_MSK   = 0x10000000,
	UNPACK_INTR  = 0x80000000
};

#define MAKE_VIF_CODE(_cmd, _num, _immediate) \
	(((uint32)(_cmd) << 24) | ((uint32)(_num) << 16) | ((uint32)(_immediate)))

#define VIF_NOP() (MAKE_VIF_CODE(VIFnop, 0, 0))
#define VIF_STCYCL(WL, CL) (MAKE_VIF_CODE(VIFstcycl, 0, (WL) << 8 | (CL)))
#define VIF_OFFSET(off) (MAKE_VIF_CODE(VIFoffset, 0, off))
#define VIF_BASE(base) (MAKE_VIF_CODE(VIFbase, 0, base))
#define VIF_ITOP(addr) (MAKE_VIF_CODE(VIFitop, 0, addr))
#define VIF_STMASK() (MAKE_VIF_CODE(VIFstmask, 0, 0))
#define VIF_STMOD(mode) (MAKE_VIF_CODE(VIFstmod, 0, mode))
#define VIF_MSKPATH3(mask) (MAKE_VIF_CODE(VIFmskpath3, 0, ((mask) & 1) << 15))
#define VIF_FLUSH() (MAKE_VIF_CODE(VIFflush, 0, 0))
#define VIF_MSCAL(addr) (MAKE_VIF_CODE(VIFmscal, 0, addr))
#define VIF_MSCALF(addr) (MAKE_VIF_CODE(VIFmscalf, 0, addr))
#define VIF_MSCNT(addr) (MAKE_VIF_CODE(VIFmscnt, 0, addr))
#define VIF_DIRECT(size) (MAKE_VIF_CODE(VIFdirect, 0, size))
#define VIF_UNPACK(type, nq, offset) (MAKE_VIF_CODE(type, nq, offset))

void mdmaInit(void);

extern void *(*mdmaMalloc)(size_t sz);
extern void *(*mdmaRealloc)(void *p, size_t sz);
extern void (*mdmaFree)(void *p);

/*
 * DMA lists and packets
 */

typedef struct mdmaList mdmaList;
struct mdmaList
{
	uint128 *p;
	uint32 size;
	uint32 limit;
};
void mdmaStart(mdmaList *list, uint128 *buf, uint32 size);
void mdmaFinish(mdmaList *list);

uint128 *mdmaSkip(mdmaList *list, uint32 n);
void mdmaAdd(mdmaList *list, uint128 q);
void mdmaAddD(mdmaList *list, uint64 d0, uint64 d1);
void mdmaAddW(mdmaList *list, uint32 w0, uint32 w1, uint32 w2, uint32 w3);
void mdmaAddF(mdmaList *list, float f0, float f1, float f2, float f3);
void mdmaAddGIFtag(mdmaList *list, int nloop, int eop, int pre, int prim, int flg, int nreg, uint64 regs);
void mdmaAddAD(mdmaList *list, uint64 a, uint64 d);
void **mdmaRef(mdmaList *list, void *data, uint16 qwc, uint32 w0, uint32 w1);
void **mdmaRefDirect(mdmaList *list, void *data, uint16 qwc);
void mdmaCnt(mdmaList *list, uint16 qwc, uint32 w0, uint32 w1);
void mdmaCntDirect(mdmaList *list, uint16 qwc);
void mdmaRet(mdmaList *list, uint16 qwc, uint32 w0, uint32 w1);
void mdmaRetDirect(mdmaList *list, uint16 qwc);
void **mdmaNext(mdmaList *list, void *next, uint16 qwc, uint32 w0, uint32 w1);
void **mdmaCall(mdmaList *list, uint16 qwc, void *addr, uint32 w0, uint32 w1);

void mdmaSend(DMA_CHAN chan, mdmaList *list);
void mdmaSendSynch(DMA_CHAN chan, mdmaList *list);


/*
 * Drawing & Display
 */


typedef struct mdmaDispBuffer mdmaDispBuffer;
struct mdmaDispBuffer
{
	// two circuits
	uint64 pmode;
	uint64 dispfb1;
	uint64 dispfb2;
	uint64 display1;
	uint64 display2;
	uint64 bgcolor;
};

typedef struct mdmaDrawBuffer mdmaDrawBuffer;
struct mdmaDrawBuffer
{
	//two contexts
	uint128 gifTag;
	uint64 frame1;
	uint64 ad_frame1;
	uint64 frame2;
	uint64 ad_frame2;
	uint64 zbuf1;
	uint64 ad_zbuf1;
	uint64 zbuf2;
	uint64 ad_zbuf2;
	uint64 xyoffset1;
	uint64 ad_xyoffset1;
	uint64 xyoffset2;
	uint64 ad_xyoffset2;
	uint64 scissor1;
	uint64 ad_scissor1;
	uint64 scissor2;
	uint64 ad_scissor2;
};

typedef struct mdmaBuffers mdmaBuffers;
struct mdmaBuffers
{
	mdmaDispBuffer disp[2];
	mdmaDrawBuffer draw[2];
};

void mdmaInitBuffers(mdmaBuffers *ctx, int width, int height, int psm, int zpsm);
void mdmaSetDisp(mdmaDispBuffer *disp);
void mdmaSetDraw(mdmaList *list, mdmaDrawBuffer *draw);
void mdmaResetGraph(int inter, int mode, int ff);
void mdmaWaitVSynch(void);


// TODO: figure out a better way to deal with GS memory
// in blocks
extern uint32 gsStart;
extern const uint32 gsEnd;

/*
 * GS register cache
 */

struct mdmaGSregs
{
	struct {
		uint64 frame;
		uint64 zbuf;
		uint64 xyoffset;
		uint64 scissor;
		uint64 test;
		uint64 alpha;

		uint64 tex0;
		uint64 tex1;
		uint64 clamp;
	} c1, c2;
	uint64 prmode;
	uint64 fogcol;
	uint64 texa;

/*
	miptbp1_1/2
	miptbp2_1/2
	fba_1/2

	prmodecont

	? texclut
	? scanmsk
	? dimx
	? dthe
	? colclamp
	? pabe
*/

};
extern struct mdmaGSregs mdmaGSregs, mdmaCurGSregs;

void mdmaSetGsRegs(mdmaList *list);
void mdmaFlushGsRegs(mdmaList *list);

#endif // MDMA_H_
