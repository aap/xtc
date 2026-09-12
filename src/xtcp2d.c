#include "xtci.h"
#include "xtcpipe.h"

#include <libgraph.h>

static mdmaTag *
upload(xtcPipeline *pipe, xtcPrimType primtype, uint32 stages)
{
	(void)stages;
	mdmaTag *tag;
	mdmaList *l = xtcState.list;
	xtcMicrocodeSwitch *swtch = &pipe->code->swtch[0];

	xtcgFlushRegs(l);

	tag = mdmaNext(l, nil, 8);
		mdmaVifFlush(l, 0);
		mdmaVifFlush(l, 0);

		// some uploads and double buffer
		mdmaVifBase(l, 0, 0);
		mdmaVifOffset(l, pipe->code->offset, 0);
		mdmaVifStCycl(l, 4,4, 0);
		mdmaBeginUnpack(l, vuXyzwScale, 2, UNPACK_V4_32, 0);
			mdmaAdd(l, xtcState.xyzwScale);
			mdmaAdd(l, xtcState.xyzwOffset);
		mdmaEndUnpack(l);

		mdmaVifNop(l, 0);
		mdmaVifNop(l, 0);
		mdmaVifStCycl(l, 4,4, 0);
		mdmaBeginUnpack(l, vuGifTag, 2, UNPACK_V4_32, 0);
			mdmaGifTag(l, 0, 1, 1,primtype, GIF_PACKED, 3, xtcpVertRegs);
			mdmaAdd(l, *(uint128*)((xtcgRegs.prmode>>4)&1 ? &xtcState.colorMod.scaleTex : &xtcState.colorMod.scale));
		mdmaEndUnpack(l);

		mdmaVifNop(l, 0);
		mdmaVifNop(l, 0);
		mdmaVifStCycl(l, 4,4, 0);
		mdmaBeginUnpack(l, vuCodeSwitch, 1, UNPACK_V4_32, 0);
			mdmaAddW(l, swtch->process>>3, 0, 0, 0);
		mdmaEndUnpack(l);
	mdmaCloseTag(l);

	return tag;
}

extern xtcMicrocode xtcCode2D;

xtcPipeline xtcTwodPipeline = {
	upload,
	&xtcCode2D,
};
xtcPipeline *twodPipeline = &xtcTwodPipeline;
