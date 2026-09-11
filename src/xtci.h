/*
 * PS2 backend internals: the GS layer, textures, microcode descriptors,
 * the pipelines and the state block.  The public API is common/xtc.h.
 */

#ifndef XTCI_H
#define XTCI_H

#include "xtc.h"


/*
 * xtcg -- the GS itself: video mode, display and drawing buffers, and the
 * shadow of the register state that xtc flushes into a list.
 */

STRUCT(xtcgDispBuffer) {
	// two circuits
	uint64 pmode;
	uint64 dispfb1;
	uint64 dispfb2;
	uint64 display1;
	uint64 display2;
	uint64 bgcolor;
};

STRUCT(xtcgDrawBuffer) {
	// two contexts, prefixed by their own GIF tag so the whole
	// struct can go to the GS as one DIRECT transfer
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

STRUCT(xtcgBuffers) {
	xtcgDispBuffer disp[2];
	xtcgDrawBuffer draw[2];
};

void xtcgResetGraph(int inter, int mode, int ff);
void xtcgInitBuffers(xtcgBuffers *buffers, int width, int height, int psm, int zpsm);
void xtcgSetDisp(xtcgDispBuffer *disp);
void xtcgSetDraw(mdmaList *list, xtcgDrawBuffer *draw);
void xtcgWaitVSynch(void);

// GS memory above the frame and depth buffers, in 64-word blocks.
// TODO: figure out a better way to deal with GS memory
extern uint32 xtcgMemStart;
extern const uint32 xtcgMemEnd;

struct xtcgRegs {
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
extern struct xtcgRegs xtcgRegs, xtcgCurRegs;

void xtcgSetRegs(mdmaList *list);
void xtcgFlushRegs(mdmaList *list);


/*
 * Textures
 */

STRUCT(xtctBuffer) {
	uint16 bp;	// block address, relative to raster base
	uint16 bw;	// pixel width/64
};

// TODO: use smaller types
struct xtcTexture {
	int32 width;
	int32 height;
	int32 depth;
	uint32 clutSize;
	uint8 *clut;
	uint32 pixelSize;
	uint8 *pixels;

	uint32 psm;
	uint32 numPages;
	xtctBuffer clutBuf;
	xtctBuffer texBuf;
	uint32 maxlod;
	uint32 hasAlpha;

	uint64 tex0;
	uint32 base;
	uint32 epoch;	// of the GS memory it sits in; see xtctUpload
	uint128 *pkts;
};

void xtctTexBuildChains(xtcTexture *tex);
void xtctUpload(xtcTexture *tex);


/*
 * Microcode and batches
 */

// NB: keep in synch with vu1/defines.inc
enum xtcpUsage {
	XTCP_UNUSED,
	XTCP_POSITION,
	XTCP_TEXCOORD,
	XTCP_COLOR,
	XTCP_NORMAL,
	XTCP_SKINDATA,
};

STRUCT(xtcpVertAttrib) {
	uint32 usage;
	uint32 offset;
	// TODO: the code should give us the format it expects,
	// the unpack is not really its business as long as the
	// data arrives correctly
	uint32 unpack;
};

STRUCT(xtcpBatchDesc) {
	uint32 stride;
	int numAttribs;
	xtcpVertAttrib attribs[10];
};

/*
 * Batch vertex count is calculated in a somewhat complicated way.
 * We want the max vertex count to be a multiple of 4 for code optimization purposes.
 * vertCount is the maximum possible vertex count.
 * We may want to ref-UNPACK some data in which case the UNPACK
 * data has to be qword-sized and the starting address qword-aligned.
 * We assume that each vertex is at least 4b so that a multiple of 4
 * is always a valid UNPACK count.
 *
 * For lists the vertex count is a multiple of 4 and the prim count.
 * The following calculation ensures this:
 * numVerts = ((vertCount/primsz) & ~3)*primsz
 *
 * For ref'ed strips we have to continue the strip with each
 * batch by repeating the last vertices,
 * and the beginning of each batch has to be qword-aligned.
 * This means the vertex count *excluding the repeated vertices*
 * has to be a multiple of 4.
 * Because we may need a multiple of 4 for the UNPACK count we may
 * have to round up, for which we need space too.
 * In addition the code expects vertices to come in a multiple of 4
 * so need space for the actual vertex count rounded up to a multiple of 4 as well.
 * The following calculation ensures this:
 * numVerts = (((vertCount & ~3)-repeat) & ~3)+repeat
 *
 * For non-ref'ed strips (the only kind we have right now)
 * we only need the vertex count to be a multiple of 4:
 * numVerts = vertCount & ~3
 */

STRUCT(xtcMicrocodeSwitch) {
        uint32 process, buf1, buf2, buf3;
};

STRUCT(xtcMicrocode) {
	void *code;
	uint32 vertexTop;
	uint32 vertCount;
	uint32 numAttribs;
	uint32 offset;
	xtcpBatchDesc *desc;
	uint32 numVerts[XTC_NUM_PRIMTYPES];
	// the clip scratch and the two flush limits: STD_CLIPCONSTI, sent
	// when the layout changes (the std pipes; the RW ones assemble them in)
	uint32 clipConsts[4];
	// pipeline code will know what to do with this (for now)
	xtcMicrocodeSwitch swtch[0];
};

STRUCT(xtcBatchInfo) {
	uint32 numBatches;
	uint32 batchSize;
	uint32 lastBatchSize;
	int32 repeat;
};

int xtcpGetBatchInfo(xtcMicrocode *code, xtcBatchInfo *bi, xtcPrimType type, int32 numVerts);
void xtcpRefVertices(uint128 *verts, int32 numVerts, xtcBatchInfo *bi, uint32 stride);

void xtcpCombineMatrix(void);
void xtcpUploadLights(void);


/*
 * Immediate mode
 */

STRUCT(xtcImState) {
	float xyzw[4];
	float stq[4];
	uint32 rgba[4];
	int32 normal[4];
	float weights[4];
	uint8 indices[4];

	xtcMicrocode *code;
	uint128 *vertstash;
	void *vertptr;
	int numVerts;
	// the pipe's next-tag: in xtcEnd it is pointed past the vertices
	mdmaTag *skiptag;
	xtcPrimType primtype;
	int restartstrip;
};
extern xtcImState imstate;


/*
 * Pipelines and prim lists
 */

// what a prim list's data asks of the pipeline, beyond the state
enum {
	XTCP_ST_DECOMP16 = 1,	// 16 bit positions and texcoords, dequantize
	XTCP_ST_SKIN = 2	// skin data in the record
};

struct xtcPipeline {
	// returns the `next' tag it opened: the caller writes the vertices,
	// then targets the tag past them.  stages: the list's XTCP_ST_ bits
	mdmaTag *(*upload)(xtcPipeline *pipe, xtcPrimType primtype, uint32 stages);
	xtcMicrocode *code;
};

struct xtcPrimList {
	xtcPipeline *pipe;
	xtcPrimType primtype;
	uint32 size;
	void *list;
	uint32 stages;		// XTCP_ST_ bits
};


/*
 * Setup
 */

void xtcSetDraw(xtcgDrawBuffer *draw);
void xtcSetList(mdmaList *list);
void xtcInit(int width, int height, int depth);


/*
 * The state
 */

struct xtcState
{
	uint32 width, height;

	uint32 clearcol;
	uint32 cleardepth;

	int zte;
	int ztst;

	uint32 nearScreen, farScreen;
	float near, far;
	float fogstart, fogend;

	Mat4 proj;
	Mat4 view;
	Mat4 world;

	int clipping;

	mdmaList *list;
	xtcPipeline *pipe;

	int tme;
	xtcTexture *tex;
	// these have only the stuff that's raster-independent
	uint64 tex0;
	uint64 tex1;

	uint128 matrix0;
	uint128 matrix1;
	uint128 matrix2;
	uint128 matrix3;
	uint128 xyzwScale;
	uint128 xyzwOffset;
	uint128 clipConsts;
	// the scales get added to VIF packets as qwords, so aligned
	xtcColorMod colorMod __attribute__((aligned(16)));

	xtcRwMaterial rwMaterial;
	xtcStdMaterial stdMaterial;
	uint32 stdColSel;

	Vec4 ambient;
	xtcLight lights[8];

	// for the skin pipeline, aligned so a ref can pick them up
	Mat4 boneMatrices[64] __attribute__((aligned(16)));
	int numBoneMatrices;

	// bumped by the setters, so a pipeline can tell what changed since
	// it last uploaded: the matrices and their constants, the lights
	// and ambient, the material and colour selection.  vuGen bumps
	// when the microcode changes, nothing in VU memory is trusted then
	uint32 xformGen, lightGen, matGen, vuGen;
};
extern struct xtcState xtcState;

#endif
