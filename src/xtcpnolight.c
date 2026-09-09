#include "xtci.h"
#include "xtcpipe.h"

#include <libgraph.h>

static mdmaTag *
upload(xtcPipeline *pipe, xtcPrimType primtype)
{
	mdmaTag *tag;
	mdmaList *l = xtcState.list;

	xtcpCombineMatrix();

	xtcMicrocodeSwitch *swtch;
	if(xtcState.clipping)
		swtch = &pipe->code->swtch[1 + primtype];
	else
		swtch = &pipe->code->swtch[0];

	xtcgFlushRegs(l);

	tag = mdmaNext(l, nil, 13);
		mdmaVifFlush(l, 0);
		mdmaVifFlush(l, 0);

		// some uploads and double buffer
		mdmaVifBase(l, 0, 0);
		mdmaVifOffset(l, pipe->code->offset, 0);
		mdmaVifStCycl(l, 4,4, 0);
		mdmaBeginUnpack(l, vuMatrix, 7, UNPACK_V4_32, 0);
			mdmaAdd(l, xtcState.matrix0);
			mdmaAdd(l, xtcState.matrix1);
			mdmaAdd(l, xtcState.matrix2);
			mdmaAdd(l, xtcState.matrix3);
			mdmaAdd(l, xtcState.xyzwScale);
			mdmaAdd(l, xtcState.xyzwOffset);
			mdmaAdd(l, xtcState.clipConsts);
		mdmaEndUnpack(l);

		mdmaVifNop(l, 0);
		mdmaVifNop(l, 0);
		mdmaVifStCycl(l, 4,4, 0);
		mdmaBeginUnpack(l, vuGifTag, 2, UNPACK_V4_32, 0);
			mdmaGifTag(l, 0, 1, 1,primtype, GIF_PACKED, 3, xtcpVertRegs);
			mdmaAdd(l, xtcState.colorScale[(xtcgRegs.prmode>>4)&1]);
		mdmaEndUnpack(l);

		mdmaVifNop(l, 0);
		mdmaVifNop(l, 0);
		mdmaVifStCycl(l, 4,4, 0);
		mdmaBeginUnpack(l, vuCodeSwitch, 1, UNPACK_V4_32, 0);
			mdmaAddW(l, swtch->process>>3, swtch->buf1, swtch->buf2, 0);
		mdmaEndUnpack(l);
	mdmaCloseTag(l);

	return tag;
}

extern xtcMicrocode xtcCodeNolight;

static xtcPipeline pipe = {
	upload,
	&xtcCodeNolight,
};
xtcPipeline *nolightPipeline = &pipe;
