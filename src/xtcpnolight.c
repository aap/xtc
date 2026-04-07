#include "gs.h"
#include "mdma.h"
#include "xtc.h"
#include "xtcpipe.h"

static void**
upload(xtcPipeline *pipe, xtcPrimType primtype)
{
	void **nextptr;
	mdmaList *l = xtcState.list;

	xtcpCombineMatrix();

	mdmaFlushGsRegs(xtcState.list);

	nextptr = mdmaNext(l, nil, 13, VIF_FLUSH(), VIF_FLUSH());

	// some uploads and double buffer
	mdmaAddW(l, VIF_BASE(0), VIF_OFFSET(pipe->code->offset), VIF_STCYCL(4, 4),
	    VIF_UNPACK(V4_32, 7, vuMatrix));
	mdmaAdd(l, xtcState.matrix0);
	mdmaAdd(l, xtcState.matrix1);
	mdmaAdd(l, xtcState.matrix2);
	mdmaAdd(l, xtcState.matrix3);
	mdmaAdd(l, xtcState.xyzwScale);
	mdmaAdd(l, xtcState.xyzwOffset);
	mdmaAdd(l, xtcState.clipConsts);

	mdmaAddW(l, VIF_NOP(), VIF_NOP(), VIF_STCYCL(4, 4), VIF_UNPACK(V4_32, 2, vuGifTag));
	mdmaAddGIFtag(l, 0, 1, 1, primtype, GS_GIF_PACKED, 3, 0x412);
	if(mdmaGSregs.prmode & 1<<4)
		mdmaAdd(l, xtcState.colorScaleTex);
	else
		mdmaAdd(l, xtcState.colorScale);

	mdmaAddW(l, VIF_NOP(), VIF_NOP(), VIF_STCYCL(4, 4), VIF_UNPACK(V4_32, 1, vuCodeSwitch));
	xtcMicrocodeSwitch *swtch;
	if(xtcState.clipping)
		swtch = &pipe->code->swtch[1 + primtype];
	else
		swtch = &pipe->code->swtch[0];
	mdmaAddW(l, swtch->process>>3, swtch->buf1, swtch->buf2, 0);

	return nextptr;
}

extern xtcMicrocode xtcCodeNolight;

static xtcPipeline pipe = {
	upload,
	&xtcCodeNolight,
};
xtcPipeline *nolightPipeline = &pipe;
