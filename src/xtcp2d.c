#include "mdma.h"
#include "xtc.h"
#include "xtcpipe.h"
#include "gs.h"


static void**
upload(xtcPipeline *pipe, xtcPrimType primtype)
{
	void **nextptr;
	mdmaList *l = xtcState.list;

	mdmaFlushGsRegs(xtcState.list);

	nextptr = mdmaNext(l, nil, 8, VIFflush, VIFflush);

	// some uploads and double buffer
	mdmaAddW(l, VIFbase + 0, VIFoffset + pipe->code->offset,
		STCYCL(4,4), UNPACK(V4_32, 2, vuXyzwScale));
	mdmaAdd(l, xtcState.xyzwScale);
	mdmaAdd(l, xtcState.xyzwOffset);

	mdmaAddW(l, VIFnop, VIFnop, STCYCL(4,4), UNPACK(V4_32, 2, vuGifTag));
	mdmaAddGIFtag(l, 0, 1, 1,primtype, GS_GIF_PACKED, 3, 0x412);
	if(mdmaGSregs.prmode & 1<<4)
		mdmaAdd(l, xtcState.colorScaleTex);
	else
		mdmaAdd(l, xtcState.colorScale);

	mdmaAddW(l, VIFnop, VIFnop, STCYCL(4,4), UNPACK(V4_32, 1, vuCodeSwitch));
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
