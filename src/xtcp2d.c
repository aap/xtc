#include "gs.h"
#include "mdma.h"
#include "xtc.h"
#include "xtcpipe.h"

static void**
upload(xtcPipeline *pipe, xtcPrimType primtype)
{
	void **nextptr;
	mdmaList *l = xtcState.list;

	mdmaFlushGsRegs(xtcState.list);

	nextptr = mdmaNext(l, nil, 8, VIF_FLUSH(), VIF_FLUSH());

	// some uploads and double buffer
	mdmaAddW(l, VIF_BASE(0), VIF_OFFSET(pipe->code->offset), VIF_STCYCL(4, 4),
	    VIF_UNPACK(V4_32, 2, vuXyzwScale));
	mdmaAdd(l, xtcState.xyzwScale);
	mdmaAdd(l, xtcState.xyzwOffset);

	mdmaAddW(l, VIF_NOP(), VIF_NOP(), VIF_STCYCL(4, 4), VIF_UNPACK(V4_32, 2, vuGifTag));
	mdmaAddGIFtag(l, 0, 1, 1, primtype, GS_GIF_PACKED, 3, 0x412);
	if(mdmaGSregs.prmode & 1<<4)
		mdmaAdd(l, xtcState.colorScaleTex);
	else
		mdmaAdd(l, xtcState.colorScale);

	mdmaAddW(l, VIF_NOP(), VIF_NOP(), VIF_STCYCL(4, 4), VIF_UNPACK(V4_32, 1, vuCodeSwitch));
	xtcMicrocodeSwitch *swtch = &pipe->code->swtch[0];
	mdmaAddW(l, swtch->process>>3, 0, 0, 0);

	return nextptr;
}

extern xtcMicrocode xtcCode2D;

static xtcPipeline pipe = {
	upload,
	&xtcCode2D,
};
xtcPipeline *twodPipeline = &pipe;
