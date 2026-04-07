#include "gs.h"
#include "mdma.h"
#include "xtc.h"
#include "xtcpipe.h"

#include <stdio.h>

static void**
upload(xtcPipeline *pipe, xtcPrimType primtype)
{
	void **nextptr;
	mdmaList *l = xtcState.list;

	xtcpCombineMatrix();
	xtcpUploadLights();

	mdmaFlushGsRegs(xtcState.list);

	nextptr = mdmaNext(l, nil, 14, VIF_FLUSH(), VIF_FLUSH());

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

	mdmaAddW(l, VIF_NOP(), VIF_NOP(), VIF_STCYCL(4, 4), VIF_UNPACK(V4_32, 3, vuGifTag));
	mdmaAddGIFtag(l, 0, 1, 1, primtype, GS_GIF_PACKED, 3, 0x412);
	xtcMaterial *m = &xtcState.material;
	float *scl;
	if(mdmaGSregs.prmode & 1<<4)
		scl = xtcState.pColorScaleTex;
	else
		scl = xtcState.pColorScale;
	mdmaAddF(l, m->color.r*scl[0], m->color.g*scl[1],
		m->color.b*scl[2], m->color.a*scl[3]);
	mdmaAddF(l, m->ambient, m->specular, m->diffuse, m->shininess);

	mdmaAddW(l, VIF_NOP(), VIF_NOP(), VIF_STCYCL(4, 4), VIF_UNPACK(V4_32, 1, vuCodeSwitch));
	xtcMicrocodeSwitch *swtch;
	if(xtcState.clipping) {
		switch(primtype) {
		case XTC_TRILIST:
			swtch = &pipe->code->swtch[1];
			break;
		case XTC_TRISTRIP:
			swtch = &pipe->code->swtch[2];
			break;
		default:
			printf("warning: no clipping for primtype %d\n", primtype);
			swtch = &pipe->code->swtch[0];
		}
	} else
		swtch = &pipe->code->swtch[0];
	mdmaAddW(l, swtch->process>>3, swtch->buf1, swtch->buf2, 0);

	return nextptr;
}

extern xtcMicrocode xtcCodeDefault;

static xtcPipeline pipe = {
	upload,
	&xtcCodeDefault,
};
xtcPipeline *defaultPipeline = &pipe;
