#include "mdma.h"
#include "xtc.h"
#include "xtcpipe.h"
#include "m.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int primSize[] = {
 [XTC_POINTS]	=	1,
 [XTC_LINELIST]	=	2,
 [XTC_LINESTRIP]	=	2,
 [XTC_TRILIST]	=	3,
 [XTC_TRISTRIP]	=	3,
};

int primRepeat[] = {
 [XTC_POINTS]	=	0,
 [XTC_LINELIST]	=	0,
 [XTC_LINESTRIP]	=	1,
 [XTC_TRILIST]	=	0,
 [XTC_TRISTRIP]	=	2,
};

xtcImState imstate;

int lightTypeAmbient = 1;
int lightTypeDirect = 3;

void
xtcpCombineMatrix(void)
{
	float t[16];
	matmul(t, xtcState.view, xtcState.world);
	matmul(xtcState.matrix_f, xtcState.proj, t);
}

void
xtcpUploadLights(void)
{
	int ndir;
	xtcRGBA *c;
	float dir[3];

	ndir = 0;
	for(uint32 i = 0; i < nelem(xtcState.lights); i++) {
		xtcLight *l = &xtcState.lights[i];
		if(l->enabled && l->type == XTC_LIGHT_DIRECT)
			ndir++;
	}

	mdmaCnt(xtcState.list, 2 + 2 * ndir, VIF_STCYCL(4, 4),
	    VIF_UNPACK(V4_32, 2 + 2 * ndir, vuLight));

	c = &xtcState.ambient;
	mdmaAddF(xtcState.list, c->r, c->g, c->b, *(float*)&lightTypeAmbient);

	for(uint32 i = 0; i < nelem(xtcState.lights); i++) {
		xtcLight *l = &xtcState.lights[i];
		if(l->enabled && l->type == XTC_LIGHT_DIRECT) {
			c = &l->color;
			mdmaAddF(xtcState.list, c->r, c->g, c->b, *(float*)&lightTypeDirect);
// TODO: this assumes xtcState.world is orthogonal!!!
			invXformVecO(dir, xtcState.world, (float*)&l->direction);
			mdmaAddF(xtcState.list, dir[0], dir[1], dir[2], 0.0f);
		}
	}

	mdmaAddW(xtcState.list, 0, 0, 0, 0);
}

int
xtcpGetBatchInfo(xtcMicrocode *code, xtcBatchInfo *bi, xtcPrimType type, int32 numVerts)
{
	int numPrims, numPrimsBatch;
	int primsz = primSize[type];
	bi->repeat = primRepeat[type];
// TODO: for strips we can usually use numVerts[POINTS]
//  only when using separate ref'ed uploads do we need this
//  for packet size/alignment purposes
	bi->batchSize = code->numVerts[type];
	// nothing to draw
	if(bi->batchSize < primSize[type])
		return 1;

	if(type & 1) {
		// list
		numPrims = numVerts/primsz;
		numPrimsBatch = bi->batchSize/primsz;
	} else {
		// strip
		numPrims = numVerts - bi->repeat;
		numPrimsBatch = bi->batchSize - bi->repeat;
	}
	bi->numBatches = (numPrims + numPrimsBatch-1) / numPrimsBatch;
	bi->lastBatchSize = numVerts-(bi->numBatches-1)*(bi->batchSize-bi->repeat);
	return 0;
}


/*
 *         +----------------------------+
 *  +----  | DMA next                   |
 *  |      |                            |
 *  |      |      .....                 |
 *  |      |                            |
 *  |      +----------------------------+
 *  |      |    vertices batch 0        |  <---+            vertices interleaved
 *  |      |                            |      |            in UNPACK V4_32 format
 *  |      |                            |      |
 *  |      |                            |      |
 *  |      |                            |      |
 *  |      |  ------------------------  |      |
 *  |      |    vertices batch 1        |  <-------+
 *  |      |                            |      |   |
 *  |      |                            |      |   |
 *  |      |                            |      |   |
 *  |      |                            |      |   |
 *  |      |  ------------------------  |      |   |
 *  |      |    vertices batch 2        |  <-----------+
 *  |      |                            |      |   |   |
 *  |      |                            |      |   |   |
 *  |      +----------------------------+      |   |   |
 *  +--->  | DMA ref  UNPACK            |  ----+   |   |
 *         +----------------------------+          |   |
 *         | DMA cnt  ITOP, CALL        |          |   |
 *         +----------------------------+          |   |
 *         | DMA ref  UNPACK            |  --------+   |
 *         +----------------------------+              |
 *         | DMA cnt  ITOP, CNT         |              |
 *         +----------------------------+              |
 *         | DMA ref  UNPACK            |  ------------+
 *         +----------------------------+
 *         | DMA cnt  ITOP, CNT         |
 *         +----------------------------+
 *         | DMA cnt  FLUSH             |
 *         +----------------------------+
 *         | .....                      |
 */

void
xtcpRefVertices(uint128 *verts, int32 numVerts, xtcBatchInfo *bi, uint32 stride)
{
	mdmaList *l = xtcState.list;
	uint32 call = VIF_MSCALF(0);

	int vertCount = bi->batchSize;
	for(int i = 0; i < bi->numBatches-1; i++) {
		mdmaRef(l, verts, vertCount * stride, VIF_STCYCL(4, 4),
		    VIF_UNPACK(V4_32, vertCount * stride, 0x8000 + 0));
		mdmaCnt(l, 0, VIF_ITOP(vertCount), call);
		call = VIF_MSCNT(0);
		verts += (vertCount - bi->repeat)*stride;
	}

	vertCount = bi->lastBatchSize;
	mdmaRef(l, verts, vertCount * stride, VIF_STCYCL(4, 4),
	    VIF_UNPACK(V4_32, vertCount * stride, 0x8000 + 0));
	mdmaCnt(l, 1, VIF_ITOP(vertCount), call);
	mdmaAddW(l, VIF_NOP(), VIF_NOP(), VIF_FLUSH(), VIF_FLUSH());
}

/*
 *         | .....                      |
 *         +----------------------------+
 *         | DMA cnt  STCYCL            |
 *         | UNPACK   position          |	UNPACKs not necessarily aligned
 *         |   ....                     |
 *         | UNPACK   tex coords        |
 *         |   ....                     |
 *         | UNPACK   colors            |
 *         |   ....                     |
 *         | UNPACK   normals           |
 *         |   ....                     |
 *         | ITOP CALL                  |
 *         +----------------------------+
 *         |                            |
 *         | .....                      |
 *         |                            |
 *         +----------------------------+
 *         | DMA ret                    |
 *         |   ....                     |
 *         | ITOP CNT FLUSH             |
 *         +----------------------------+
 *         | .....                      |
 */

static uint32
unpackSize(uint32 unpack, uint32 num)
{
	static uint32 size[] = { 32, 16, 8, 16 };
	uint32 data = ((unpack>>26 & 3)+1)*size[unpack>>24 & 3]/8 * num;
	return (data+3)&~3;
}

static uint32
calcBatchQWC(xtcpBatchDesc *desc, uint32 numVerts)
{
	// we'll make one cnt/ret packet per batch for simplicity
	uint32 size = 8		// DMA tag
		+ 4		// NOP
		+ 4;		// STCYCL
	for(int i = 0; i < desc->numAttribs; i++) {
		size += 4;	// UNPACK
		size += unpackSize(desc->attribs[i].unpack, numVerts);
	}
	size += 4 + 4 + 4;	// ITOP, CALL/CNT, NOP/FLUSH
	return (size+15)/16;
}

static uint32*
packVertices(uint32 *data, xtcpVertAttrib *desc, uint128 *verts, uint32 vertCount, uint32 stride)
{
	uint8 *u8p;
	int8 *i8p;
	uint32 *u32p;
	int32 *i32p;

	*data++ = desc->unpack | vertCount<<16 | 0x8000 | desc->offset;
	verts += desc->offset;

	switch(desc->unpack) {
	case UNPACK_V4_32:
		while(vertCount--) {
			memcpy(data, verts, 16);
			data += 4;
			verts += stride;
		}
		break;
	case UNPACK_V3_32:
		while(vertCount--) {
			memcpy(data, verts, 12);
			data += 3;
			verts += stride;
		}
		break;
	case UNPACK_V2_32:
		while(vertCount--) {
			memcpy(data, verts, 8);
			data += 2;
			verts += stride;
		}
		break;

	case UNPACK_V4_8 | UNPACK_USN:
		u8p = (uint8*)data;
		while(vertCount--) {
			u32p = (uint32*)verts;
			*u8p++ = *u32p++;
			*u8p++ = *u32p++;
			*u8p++ = *u32p++;
			*u8p++ = *u32p++;
			verts += stride;
		}
		data = (uint32*)u8p;
		break;

	case UNPACK_V3_8:
		i8p = (int8*)data;
		while(vertCount--) {
			i32p = (int32*)verts;
			*i8p++ = *i32p++;
			*i8p++ = *i32p++;
			*i8p++ = *i32p++;
			verts += stride;
		}
		while((uint32)i8p & 3) *i8p++ = 0;
		data = (uint32 *)i8p;
		break;

	default:
		printf("unsupported unpack format %08X", desc->unpack);
		assert(0 && "unsupported unpack format");
	}

	return data;
}

static void
dumpshit(uint32 *data, uint32 size)
{
	size /= 16;

	while(size--) {
		printf("%p: %08X %08X %08X %08X\n", data,
			data[0], data[1], data[2], data[3]);
		data += 4;
	}
}

void
xtcpBuildList(xtcPrimList *list, xtcBatchInfo *bi)
{
	xtcpBatchDesc *desc = imstate.desc;
	uint32 batchQWC = calcBatchQWC(desc, bi->batchSize);
	uint32 lastBatchQWC = calcBatchQWC(desc, bi->lastBatchSize);
	list->size = 16*(batchQWC*(bi->numBatches-1) + lastBatchQWC);
	uint32 *data = mdmaMalloc(list->size);
	assert(((uint32)data & 0xF) == 0);

	list->list = data;
	list->pipe = xtcState.pipe;
	list->primtype = imstate.primtype;

	uint32 call = VIF_MSCALF(0);
	uint32 wait = VIF_NOP();
	uint32 vertCount;
	uint128 *verts = imstate.vertstash;
	for(int i = 0; i < bi->numBatches; i++) {
		if(i == bi->numBatches-1) {
			vertCount = bi->lastBatchSize;
			wait = VIF_FLUSH();
			*data++ = DMAret + lastBatchQWC-1;
		} else {
			vertCount = bi->batchSize;
			*data++ = DMAcnt + batchQWC-1;
		}
		*data++ = 0;
		*data++ = VIF_NOP();
		*data++ = VIF_STCYCL(1, desc->stride);

		for(int j = 0; j < desc->numAttribs; j++)
			data = packVertices(data, &desc->attribs[j], verts, vertCount, desc->stride);
		verts += (vertCount - bi->repeat)*desc->stride;

		*data++ = VIF_ITOP(vertCount);
		*data++ = call;
		*data++ = wait;
		call = VIF_MSCNT(0);

		while ((uint32)data & 0xF)
			*data++ = VIF_NOP();
	}

//	dumpshit(list->list, list->size);
}

static xtcMicrocode *currentCode;

void
xtcpSetMicrocode(xtcMicrocode *code)
{
	if(code == currentCode)
		return;
	mdmaCall(xtcState.list, 0, code->code, VIF_NOP(), VIF_NOP());
	currentCode = code;
}

// TODO: not quite happy with this
void
xtcpUseTexture(xtcRaster *r)
{
	if(xtcState.tme && r) {
		mdmaGSregs.prmode |= 1<<4;
		mdmaGSregs.c1.tex0 = xtcState.tex0 | r->tex0;
		mdmaGSregs.c1.tex0 += r->base;
		if(r->clut)
			mdmaGSregs.c1.tex0 += (uint64)r->base<<37;
		mdmaGSregs.c1.tex1 = xtcState.tex1 | r->maxlod<<2;
	} else
		mdmaGSregs.prmode &= ~(1UL<<4);
}



static xtcPrimList *curList;

xtcPrimList*
xtcCreatePrimList(void)
{
	xtcPrimList *pl;

	pl = (xtcPrimList*)mdmaMalloc(sizeof(xtcPrimList));
	memset(pl, 0, sizeof(*pl));

	return pl;
}

void
xtcStartList(xtcPrimList *pl)
{
	curList = pl;
}

void
xtcEndList(void)
{
	curList = nil;
}

void
xtcPrimListDraw(xtcPrimList *pl)
{
	xtcPipeline *pipe = pl->pipe;
	if(pipe->desc.numAttribs == 0)
		xtcpMakeBatchDesc(pipe->code->vertFmt, &pipe->desc);

	xtcpSetMicrocode(pipe->code);
	xtcpUseTexture(xtcState.tex);
	void **next = pl->pipe->upload(pl->pipe, pl->primtype);
	*next = mdmaSkip(xtcState.list, 0);

	mdmaCall(xtcState.list, 0, pl->list, VIF_NOP(), VIF_NOP());
}


void
xtcBegin(xtcPrimType prim)
{
	xtcPipeline *pipe = xtcState.pipe;
	if(pipe->desc.numAttribs == 0)
		xtcpMakeBatchDesc(pipe->code->vertFmt, &pipe->desc);
	imstate.code = pipe->code;
	imstate.desc = &pipe->desc;
	imstate.primtype = prim;
	imstate.restartstrip = 0;
	imstate.numVerts = 0;

	if(curList == nil) {
		xtcpSetMicrocode(pipe->code);
		xtcpUseTexture(xtcState.tex);
		imstate.nextptr = xtcState.pipe->upload(xtcState.pipe, prim);
	}

	imstate.vertstash = mdmaSkip(xtcState.list, 0);
	imstate.vertptr = imstate.vertstash;
}

void
xtcEnd(void)
{
	mdmaList *l = xtcState.list;
	xtcBatchInfo bi;
	int numVerts = imstate.numVerts;

	if(xtcpGetBatchInfo(imstate.code, &bi, imstate.primtype, numVerts)) {
		// nothing to draw, discard vertices if they ever existed
		if(curList == nil)
			*imstate.nextptr = mdmaSkip(l, 0);
	} else {
		if(curList) {
			xtcpBuildList(curList, &bi);
		} else {
			mdmaSkip(l, numVerts*imstate.code->numAttribs);
			*imstate.nextptr = mdmaSkip(l, 0);

			xtcpRefVertices(imstate.vertstash, numVerts, &bi, imstate.code->numAttribs);
		}
	}

	imstate.vertstash = nil;
	imstate.nextptr = nil;
}

void
xtcpKickVertex(uint32 vertFmt)
{
	if(vertFmt & (POS_3F | POS_4F)) {
		float *v = (float*)imstate.vertptr;
		v[0] = imstate.xyzw[0];
		v[1] = imstate.xyzw[1];
		v[2] = imstate.xyzw[2];
		v[3] = imstate.xyzw[3];
		imstate.vertptr = v+4;
	}

	if(vertFmt & TEX_2F) {
		float *v = (float*)imstate.vertptr;
		v[0] = imstate.stq[0];
		v[1] = imstate.stq[1];
		v[2] = imstate.stq[2];
		v[3] = imstate.stq[3];
		imstate.vertptr = v+4;
	}

	if(vertFmt & COL_4B) {
		uint32 *v = (uint32*)imstate.vertptr;
		v[0] = imstate.rgba[0];
		v[1] = imstate.rgba[1];
		v[2] = imstate.rgba[2];
		v[3] = imstate.rgba[3];
		imstate.vertptr = v+4;
	}

	if(vertFmt & NORMAL_3B) {
		int32 *v = (int32*)imstate.vertptr;
		v[0] = imstate.normal[0];
		v[1] = imstate.normal[1];
		v[2] = imstate.normal[2];
		v[3] = imstate.normal[3];
		imstate.vertptr = v+4;
	}

	imstate.numVerts++;
}

void
xtcVertex(float x, float y, float z)
{
	imstate.xyzw[0] = x;
	imstate.xyzw[1] = y;
	imstate.xyzw[2] = z;
	imstate.xyzw[3] = 0.0f;

	xtcpKickVertex(imstate.code->vertFmt);
	if(imstate.restartstrip) {
		xtcpKickVertex(imstate.code->vertFmt);
		imstate.restartstrip = 0;
	}
}

// TODO: just use ADC bits for this, it's really stupid right now
void
xtcRestartStrip(void)
{
	switch(imstate.primtype) {
	case XTC_LINESTRIP:
// not possible right now, with ADC it will be
		break;
	case XTC_TRISTRIP:
		xtcpKickVertex(imstate.code->vertFmt);
		imstate.restartstrip = 1;
		break;
	default:
		printf("%s: Unsupported primtype %04x\n", __FUNCTION__, imstate.primtype);
		break;
	}
}

void
xtcTexCoord(float s, float t, float q)
{
	imstate.stq[0] = s;
	imstate.stq[1] = t;
	imstate.stq[2] = q;
	imstate.stq[3] = 0.0f;
}

void
xtcColor(uint32 r, uint32 g, uint32 b, uint32 a)
{
	imstate.rgba[0] = r;
	imstate.rgba[1] = g;
	imstate.rgba[2] = b;
	imstate.rgba[3] = a;
}

void
xtcNormal(float x, float y, float z)
{
	imstate.normal[0] = x*127.0f;
	imstate.normal[1] = y*127.0f;
	imstate.normal[2] = z*127.0f;
	imstate.normal[3] = 0.0f;
}



void
xtcpMakeBatchDesc(uint32 vertFmt, xtcpBatchDesc *desc)
{
	uint32 offset;
	xtcpVertAttrib *a;

	memset(desc, 0, sizeof(*desc));
	offset = 0;

	a = &desc->attribs[desc->numAttribs];
	if(vertFmt & POS_3F) {
		a->usage = XTCP_POSITION;
		a->offset = offset++;
// TODO: might want to mask for ADC here
		a->unpack = UNPACK_V3_32;
		desc->numAttribs++;
	} else if(vertFmt & POS_4F) {
		a->usage = XTCP_POSITION;
		a->offset = offset++;
		a->unpack = UNPACK_V4_32;
		desc->numAttribs++;
	}

	a = &desc->attribs[desc->numAttribs];
	if(vertFmt & TEX_2F) {
		a->usage = XTCP_TEXCOORD;
		a->offset = offset++;
		a->unpack = UNPACK_V2_32;
		desc->numAttribs++;
	}

	a = &desc->attribs[desc->numAttribs];
	if(vertFmt & COL_4B) {
		a->usage = XTCP_COLOR;
		a->offset = offset++;
		a->unpack = UNPACK_V4_8 | UNPACK_USN;
		desc->numAttribs++;
	}

	a = &desc->attribs[desc->numAttribs];
	if(vertFmt & NORMAL_3B) {
		a->usage = XTCP_NORMAL;
		a->offset = offset++;
		a->unpack = UNPACK_V3_8;
		desc->numAttribs++;
	}

	a = &desc->attribs[desc->numAttribs];
	if(vertFmt & SKINDATA_4F) {
		a->usage = XTCP_SKINDATA;
		a->offset = offset++;
		a->unpack = UNPACK_V4_32;
		desc->numAttribs++;
	}

	// TODO: might have some non-uploaded data in the stride
	// probably get that from microcode numInAttribs?
	desc->stride = offset;

/*
printf("stride: %d\n", desc->stride);
for(int i = 0; i < desc->numAttribs; i++) {
	printf("%d: %X %X %X\n", i,
		desc->attribs[i].usage,
		desc->attribs[i].offset,
		desc->attribs[i].unpack
	);
}
*/

}
