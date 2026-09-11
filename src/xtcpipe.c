#include "xtci.h"
#include "xtcpipe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// in xtcPrimType order: POINTS, LINELIST, LINESTRIP, TRILIST, TRISTRIP
int primSize[] = { 1, 2, 2, 3, 3 };
int primRepeat[] = { 0, 0, 1, 0, 2 };

xtcImState imstate;

int lightTypeAmbient = 1;
int lightTypeDirect = 3;

void
xtcpCombineMatrix(void)
{
	Mat4 t = m4mul(xtcState.view, xtcState.world);
	*(Mat4*)&xtcState.matrix0 = m4mul(xtcState.proj, t);
}

void
xtcpUploadLights(void)
{
	int ndir;
	Vec4 *c;
	Vec3 dir;

	ndir = 0;
	for(uint32 i = 0; i < nelem(xtcState.lights); i++) {
		xtcLight *l = &xtcState.lights[i];
		if(l->enabled && l->type == XTC_LIGHT_DIRECT)
			ndir++;
	}


	mdmaList *list = xtcState.list;

	mdmaCnt(list, 2+2*ndir);
		mdmaVifStCycl(list, 4,4, 0);
		mdmaBeginUnpack(list, vuLight, 2+2*ndir, UNPACK_V4_32, 0);

		c = &xtcState.ambient;
		mdmaAddF(list, 255.0f*c->x, 255.0f*c->y, 255.0f*c->z, *(float*)&lightTypeAmbient);

		for(uint32 i = 0; i < nelem(xtcState.lights); i++) {
			xtcLight *l = &xtcState.lights[i];
			if(l->enabled && l->type == XTC_LIGHT_DIRECT) {
				c = &l->color;
				mdmaAddF(list, 255.0f*c->x, 255.0f*c->y, 255.0f*c->z, *(float*)&lightTypeDirect);
// TODO: this assumes xtcState.world is orthogonal!!!
				dir = m4invXformVecO(&xtcState.world, l->direction);
				mdmaAddF(list, dir.x, dir.y, dir.z, 0.0f);
			}
		}

		// terminator
		mdmaAddW(list, 0, 0, 0, 0);
		mdmaEndUnpack(list);
	mdmaCloseTag(list);
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
	// only the first batch starts the microprogram, the rest continue it
	int first = 1;

	int vertCount = bi->batchSize;
	for(int i = 0; i < bi->numBatches-1; i++) {
		mdmaRef(l, verts, vertCount*stride);
			mdmaVifStCycl(l, 4,4, 0);
			mdmaVifUnpack(l, UNPACK_DBLBUF + 0, vertCount*stride,
				UNPACK_V4_32, 0);
		mdmaCnt(l, 0);
			mdmaVifItop(l, vertCount, 0);
			if(first) mdmaVifMsCalF(l, 0, 0);
			else mdmaVifMsCnt(l, 0);
		mdmaCloseTag(l);
		first = 0;
		verts += (vertCount - bi->repeat)*stride;
	}

	vertCount = bi->lastBatchSize;
	mdmaRef(l, verts, vertCount*stride);
		mdmaVifStCycl(l, 4,4, 0);
		mdmaVifUnpack(l, UNPACK_DBLBUF + 0, vertCount*stride,
			UNPACK_V4_32, 0);
	mdmaCnt(l, 1);
		mdmaVifItop(l, vertCount, 0);
		if(first) mdmaVifMsCalF(l, 0, 0);
		else mdmaVifMsCnt(l, 0);
		mdmaVifNop(l, 0);
		mdmaVifNop(l, 0);
		mdmaVifFlush(l, 0);
		mdmaVifFlush(l, 0);
	mdmaCloseTag(l);
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
	// CMD is 0110 vvll: vv = components-1, ll = 0:32 1:16 2:8 3:V4-5-5-5-1
	static uint32 size[] = { 32, 16, 8, 16 };
	uint32 data = ((unpack>>2 & 3)+1)*size[unpack & 3]/8 * num;
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

	// USN lives in the address immediate, the format in the CMD byte
	*data++ = SCE_VIF1_SET_UNPACK(
		UNPACK_DBLBUF | (desc->unpack & UNPACK_USN) | desc->offset,
		vertCount & 0xff, desc->unpack & 0xff, 0);
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
		data = (uint32*)i8p;
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
	xtcpBatchDesc *desc = imstate.code->desc;
	uint32 batchQWC = calcBatchQWC(desc, bi->batchSize);
	uint32 lastBatchQWC = calcBatchQWC(desc, bi->lastBatchSize);
	list->size = 16*(batchQWC*(bi->numBatches-1) + lastBatchQWC);
	uint32 *data = (uint32*)mdmaMalloc(list->size);
	assert(((uint32)data & 0xF) == 0);

	list->list = data;
	list->pipe = xtcState.pipe;
	list->primtype = imstate.primtype;

	// hand-built: the qwc of every tag is known up front from calcBatchQWC,
	// and the vertex data goes straight out through packVertices
	uint32 call = SCE_VIF1_SET_MSCALF(0, 0);
	uint32 wait = SCE_VIF1_SET_NOP(0);
	uint32 vertCount;
	uint128 *verts = imstate.vertstash;
	for(int i = 0; i < bi->numBatches; i++) {
		if(i == bi->numBatches-1) {
			vertCount = bi->lastBatchSize;
			wait = SCE_VIF1_SET_FLUSH(0);
			*data++ = DMAret + lastBatchQWC-1;
		} else {
			vertCount = bi->batchSize;
			*data++ = DMAcnt + batchQWC-1;
		}
		*data++ = 0;
		*data++ = SCE_VIF1_SET_NOP(0);
		*data++ = SCE_VIF1_SET_STCYCL(1, desc->stride, 0);

		for(int j = 0; j < desc->numAttribs; j++)
			data = packVertices(data, &desc->attribs[j], verts, vertCount, desc->stride);
		verts += (vertCount - bi->repeat)*desc->stride;

		*data++ = SCE_VIF1_SET_ITOP(vertCount, 0);
		*data++ = call;
		*data++ = wait;
		call = SCE_VIF1_SET_MSCNT(0);

		while((uint32)data & 0xF) *data++ = SCE_VIF1_SET_NOP(0);
	}

//	dumpshit(list->list, list->size);
}

static xtcMicrocode *currentCode;

void
xtcpSetMicrocode(xtcMicrocode *code)
{
	// two layouts of one program share the image: no upload between
	// them, and the constants up there stay valid
	if(code == currentCode)
		return;
	if(currentCode == nil || code->code != currentCode->code) {
		mdmaCall(xtcState.list, code->code, 0);
		mdmaCloseTag(xtcState.list);
		xtcState.vuGen++;
	}
	currentCode = code;
}

// TODO: not quite happy with this
void
xtcpUseTexture(xtcTexture *r)
{
	if(xtcState.tme && r) {
		xtcgRegs.prmode |= 1<<4;
		xtcgRegs.c1.tex0 = xtcState.tex0 | r->tex0;
		xtcgRegs.c1.tex0 += r->base;
		if(r->clut)
			xtcgRegs.c1.tex0 += (uint64)r->base<<37;
		xtcgRegs.c1.tex1 = xtcState.tex1 | r->maxlod<<2;
	} else
		xtcgRegs.prmode &= ~(1UL<<4);
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

	xtcpSetMicrocode(pipe->code);
	xtcpUseTexture(xtcState.tex);

	mdmaTag *skiptag = pl->pipe->upload(pl->pipe, pl->primtype, pl->stages);
	// nothing inline to skip here — the vertices are in the prim list's own
	// buffer — so the chain continues immediately and it is really a cnt
	mdmaSetTarget(xtcState.list, skiptag, mdmaHere(xtcState.list));

	mdmaCall(xtcState.list, pl->list, 0);
	mdmaCloseTag(xtcState.list);
}


void
xtcBegin(xtcPrimType prim)
{
	xtcPipeline *pipe = xtcState.pipe;
	imstate.code = pipe->code;
	imstate.primtype = prim;
	imstate.restartstrip = 0;
	imstate.numVerts = 0;

	if(curList == nil) {
		xtcpSetMicrocode(pipe->code);
		xtcpUseTexture(xtcState.tex);
		imstate.skiptag = xtcState.pipe->upload(xtcState.pipe, prim, 0);
	}

	// the vertices go straight into the chain here; the upload tag will be
	// made to jump over them once we know how many there were
	imstate.vertstash = mdmaHere(xtcState.list);
	imstate.vertptr = imstate.vertstash;
}

void
xtcEnd(void)
{
	mdmaList *l = xtcState.list;
	xtcBatchInfo bi;
	int numVerts = imstate.numVerts;

	if(xtcpGetBatchInfo(imstate.code, &bi, imstate.primtype, numVerts)) {
		// nothing to draw: drop the vertices by resuming where they started
		if(curList == nil)
			mdmaSetTarget(l, imstate.skiptag, mdmaHere(l));
	} else {
		if(curList) {
			xtcpBuildList(curList, &bi);
		} else {
			mdmaSkip(l, numVerts*imstate.code->numAttribs);
			mdmaSetTarget(l, imstate.skiptag, mdmaHere(l));

			xtcpRefVertices(imstate.vertstash, numVerts, &bi, imstate.code->numAttribs);
		}
	}

	imstate.vertstash = nil;
}

// TODO: make this pipeline driven?
void
xtcpKickVertex(xtcMicrocode *code)
{
	xtcpBatchDesc *d = code->desc;
	uint8 *v = (uint8*)imstate.vertptr;
	uint8 *sk;
	// TODO: this really isn't ideal, but ok for now
	for(int i = 0; i < d->numAttribs; i++) {
		xtcpVertAttrib *a = &d->attribs[i];
		switch(a->usage) {
		case XTCP_POSITION: memcpy(v+a->offset*16, imstate.xyzw, 16); break;
		case XTCP_TEXCOORD: memcpy(v+a->offset*16, imstate.stq, 16); break;
		case XTCP_COLOR:    memcpy(v+a->offset*16, imstate.rgba, 16); break;
		case XTCP_NORMAL:   memcpy(v+a->offset*16, imstate.normal, 16); break;
		case XTCP_SKINDATA:
			memcpy(v+a->offset*16, imstate.weights, 16);
			sk = (uint8*)(v+a->offset*16);
			// use lower 10 bits for matrix offset
			sk[0+ 0] = imstate.indices[0]<<2; sk[1+ 0] &= ~3;
			sk[0+ 4] = imstate.indices[1]<<2; sk[1+ 4] &= ~3;
			sk[0+ 8] = imstate.indices[2]<<2; sk[1+ 8] &= ~3;
			sk[0+12] = imstate.indices[3]<<2; sk[1+12] &= ~3;
			break;
		}
	}
	imstate.vertptr = v + d->stride*16;
	imstate.numVerts++;
}

void
xtcVertex(float x, float y, float z)
{
	imstate.xyzw[0] = x;
	imstate.xyzw[1] = y;
	imstate.xyzw[2] = z;
	imstate.xyzw[3] = 0.0f;

	xtcpKickVertex(imstate.code);
	if(imstate.restartstrip) {
		xtcpKickVertex(imstate.code);
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
		xtcpKickVertex(imstate.code);
		imstate.restartstrip = 1;
		break;
	}
}

void
xtcTexCoord3(float s, float t, float q)
{
	imstate.stq[0] = s;
	imstate.stq[1] = t;
	imstate.stq[2] = q;
	imstate.stq[3] = 0.0f;
}

void
xtcTexCoord2(float s, float t)
{
	xtcTexCoord3(s, t, 1.0f);
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
	imstate.normal[0] = (int)(x*127.0f);
	imstate.normal[1] = (int)(y*127.0f);
	imstate.normal[2] = (int)(z*127.0f);
	imstate.normal[3] = 0;
}


void
xtcIndices(uint8 i0, uint8 i1, uint8 i2, uint8 i3)
{
	imstate.indices[0] = i0;
	imstate.indices[1] = i1;
	imstate.indices[2] = i2;
	imstate.indices[3] = i3;
}

void
xtcWeights(float w0, float w1, float w2, float w3)
{
	imstate.weights[0] = w0;
	imstate.weights[1] = w1;
	imstate.weights[2] = w2;
	imstate.weights[3] = w3;
}
