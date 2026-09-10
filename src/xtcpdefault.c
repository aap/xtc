#include "xtci.h"
#include "xtcpipe.h"

#include <stdio.h>
#include <libgraph.h>

static mdmaTag *
upload(xtcPipeline *pipe, xtcPrimType primtype)
{
	mdmaTag *tag;
	mdmaList *l = xtcState.list;

	xtcpCombineMatrix();
	xtcpUploadLights();

	xtcRwMaterial *m = &xtcState.rwMaterial;
	// TODO: want TME bit more elegantly
	float *scl = (float*)((xtcgRegs.prmode>>4)&1 ? &xtcState.colorMod.scaleTex : &xtcState.colorMod.scale);
	xtcMicrocodeSwitch *swtch;
	if(!xtcState.clipping) swtch = &pipe->code->swtch[0];
	else switch(primtype) {
	case XTC_TRILIST: swtch = &pipe->code->swtch[1]; break;
	case XTC_TRISTRIP: swtch = &pipe->code->swtch[2]; break;
	default:
		printf("warning: no clipping for primtype %d\n", primtype);
		swtch = &pipe->code->swtch[0];
	}

	xtcgFlushRegs(l);

	tag = mdmaNext(l, nil, 14);
		mdmaVifFlush(l, 0);
		mdmaVifFlush(l, 0);

		// some uploads and double buffer
		// TODO: only uploads this if necessary
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

		// GIF tag and material
		mdmaVifNop(l, 0);
		mdmaVifNop(l, 0);
		mdmaVifStCycl(l, 4,4, 0);
		mdmaBeginUnpack(l, vuGifTag, 3, UNPACK_V4_32, 0);
			mdmaGifTag(l, 0, 1, 1,primtype, GIF_PACKED, 3, xtcpVertRegs);
			mdmaAddF(l, m->color.x*scl[0], m->color.y*scl[1],
				m->color.z*scl[2], m->color.w*scl[3]);
			mdmaAddF(l, m->ambient, m->specular, m->diffuse, m->shininess);
		mdmaEndUnpack(l);

		// code switch
		mdmaVifNop(l, 0);
		mdmaVifNop(l, 0);
		mdmaVifStCycl(l, 4,4, 0);
		mdmaBeginUnpack(l, vuCodeSwitch, 1, UNPACK_V4_32, 0);
			mdmaAddW(l, swtch->process>>3, swtch->buf1, swtch->buf2, 0);
		mdmaEndUnpack(l);
	mdmaCloseTag(l);

	return tag;
}

extern xtcMicrocode xtcCodeDefault;

static xtcPipeline pipe = {
	upload,
	&xtcCodeDefault,
};
xtcPipeline *defaultPipeline = &pipe;
