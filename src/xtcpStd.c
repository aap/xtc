#include "xtc.h"
#include "xtcpipe.h"
#include "m.h"

#include <stdio.h>
#include <string.h>
#include <libgraph.h>

extern uint32 xtcStdLightProcs[5];

int
getLightBlock(uint128 *lightDirs, xtcRGBA *lightCols)
{
	int ndir;
	float dir[3];
	float (*d)[4] = (float (*)[4])lightDirs;

	memset(lightDirs, 0, 4*sizeof(uint128));
	memset(lightCols, 0, 4*sizeof(uint128));

	ndir = 0;
	for(uint32 i = 0; i < nelem(xtcState.lights); i++) {
		xtcLight *l = &xtcState.lights[i];
		if(l->enabled && l->type == XTC_LIGHT_DIRECT && ndir < 4) {
			invXformVecO(dir, xtcState.world, (float*)&l->direction);
			d[0][ndir] = -dir[0];
			d[1][ndir] = -dir[1];
			d[2][ndir] = -dir[2];
			lightCols[ndir] = l->color;
			ndir++;
		}
	}

/*
printf("light dirs\n");
printf("%g %g %g\n", d[0][0], d[1][0], d[2][0]);
printf("%g %g %g\n", d[0][1], d[1][1], d[2][1]);
printf("%g %g %g\n", d[0][2], d[1][2], d[2][2]);
printf("%g %g %g\n", d[0][3], d[1][3], d[2][3]);
printf("light colors\n");
printf("%g %g %g %g\n", c[0].r, c[0].g, c[0].b, c[0].a);
printf("%g %g %g %g\n", c[1].r, c[1].g, c[1].b, c[1].a);
printf("%g %g %g %g\n", c[2].r, c[2].g, c[2].b, c[2].a);
printf("%g %g %g %g\n", c[3].r, c[3].g, c[3].b, c[3].a);
printf("\n\n");
*/
	return ndir;
}

int
getLightBlock8(uint128 *lightDirs, xtcRGBA *lightCols)
{
	int ndir;
	float dir[3];
	float (*d)[4] = (float (*)[4])lightDirs;

	memset(lightDirs, 0, 8*sizeof(uint128));
	memset(lightCols, 0, 8*sizeof(uint128));

	ndir = 0;
	for(uint32 i = 0; i < nelem(xtcState.lights); i++) {
		xtcLight *l = &xtcState.lights[i];
		if(l->enabled && l->type == XTC_LIGHT_DIRECT && ndir < 8) {
			invXformVecO(dir, xtcState.world, (float*)&l->direction);
			d[(ndir&4)+0][ndir&3] = -dir[0];
			d[(ndir&4)+1][ndir&3] = -dir[1];
			d[(ndir&4)+2][ndir&3] = -dir[2];
			lightCols[ndir] = l->color;
			ndir++;
		}
	}

/*
printf("light dirs (%d)\n", ndir);
printf("%g %g %g\n", d[0][0], d[1][0], d[2][0]);
printf("%g %g %g\n", d[0][1], d[1][1], d[2][1]);
printf("%g %g %g\n", d[0][2], d[1][2], d[2][2]);
printf("%g %g %g\n", d[0][3], d[1][3], d[2][3]);
printf("%g %g %g\n", d[4][0], d[5][0], d[6][0]);
printf("%g %g %g\n", d[4][1], d[5][1], d[6][1]);
printf("%g %g %g\n", d[4][2], d[5][2], d[6][2]);
printf("%g %g %g\n", d[4][3], d[5][3], d[6][3]);
printf("light colors\n");
printf("%g %g %g %g\n", c[0].r, c[0].g, c[0].b, c[0].a);
printf("%g %g %g %g\n", c[1].r, c[1].g, c[1].b, c[1].a);
printf("%g %g %g %g\n", c[2].r, c[2].g, c[2].b, c[2].a);
printf("%g %g %g %g\n", c[3].r, c[3].g, c[3].b, c[3].a);
printf("%g %g %g %g\n", c[4].r, c[4].g, c[4].b, c[4].a);
printf("%g %g %g %g\n", c[5].r, c[5].g, c[5].b, c[5].a);
printf("%g %g %g %g\n", c[6].r, c[6].g, c[6].b, c[6].a);
printf("%g %g %g %g\n", c[7].r, c[7].g, c[7].b, c[7].a);
printf("\n\n");
*/
	return ndir;
}

void
xtcpUploadStdLights8(xtcStdMaterial *m, uint32 colsel)
{
	xtcRGBA *c, *ambLt;
	uint128 lightDirs[8];
	xtcRGBA D[8];
	xtcRGBA A = { 0.0f, 0.0f, 0.0f, 0.0f };
	xtcRGBA B = { 0.0f, 0.0f, 0.0f, 0.0f };

	ambLt = &xtcState.ambient;
	int ndir = getLightBlock8(lightDirs, D);
	if(colsel & XTC_EMISSIVE)
		B.r = B.g = B.b = B.a = 1.0f;
	else
		A = m->emissive;
	if(colsel & XTC_AMBIENT) {
		B.r += ambLt->r/255.0f;
		B.g += ambLt->g/255.0f;
		B.b += ambLt->b/255.0f;
		B.a += ambLt->a/255.0f;
	} else {
		A.r += ambLt->r * m->ambient.r;
		A.g += ambLt->g * m->ambient.g;
		A.b += ambLt->b * m->ambient.b;
		A.a += ambLt->a * m->ambient.a;
	}
	if(!(colsel & XTC_DIFFUSE))
		for(int i = 0; i < 8; i++) {
			D[i].r *= m->diffuse.r;
			D[i].g *= m->diffuse.g;
			D[i].b *= m->diffuse.b;
			D[i].a *= m->diffuse.a;
		}

	uint32 *sel = (uint32*)&A.a;
	if(ndir == 0)
		*sel = xtcStdLightProcs[1]>>3;	// no directionals
	else if(colsel & XTC_DIFFUSE)
		*sel = xtcStdLightProcs[3]>>3;	// no 8-light code yet
	else
		*sel = xtcStdLightProcs[ndir>4 ? 4 : 2]>>3;

	mdmaList *list = xtcState.list;

	mdmaCnt(list, 2+16);
		mdmaVifStCycl(list, 4,4, 0);
		mdmaBeginUnpack(list, vuLight, 2+16, UNPACK_V4_32, 0);

		mdmaAddF(list, 255.0f, 255.0f, 255.0f, 255.0f);		// clamp
//		mdmaAddF(list, 1.0f, 1.0f, 1.0f, 1.0f);			// vertFactor
//		mdmaAddF(list, 0.3f, 0.3f, 0.3f, 1.0f);			// vertFactor
//		mdmaAddF(list, 0.0f, 0.0f, 0.0f, 1.0f);			// vertFactor
		mdmaAddF(list, B.r, B.g, B.b, B.a);			// vertFactor

		mdmaAdd(list, lightDirs[0]);
		mdmaAdd(list, lightDirs[1]);
		mdmaAdd(list, lightDirs[2]);
		mdmaAddF(list, A.r, A.g, A.b, A.a);
		mdmaAddF(list, D[0].r, D[0].g, D[0].b, D[0].a);
		mdmaAddF(list, D[1].r, D[1].g, D[1].b, D[1].a);
		mdmaAddF(list, D[2].r, D[2].g, D[2].b, D[2].a);
		mdmaAddF(list, D[3].r, D[3].g, D[3].b, D[3].a);
		mdmaAdd(list, lightDirs[4]);
		mdmaAdd(list, lightDirs[5]);
		mdmaAdd(list, lightDirs[6]);
		mdmaAddF(list, 0.0f, 0.0f, 0.0f, 0.0f);
		mdmaAddF(list, D[4].r, D[4].g, D[4].b, D[4].a);
		mdmaAddF(list, D[5].r, D[5].g, D[5].b, D[5].a);
		mdmaAddF(list, D[6].r, D[6].g, D[6].b, D[6].a);
		mdmaAddF(list, D[7].r, D[7].g, D[7].b, D[7].a);

		mdmaEndUnpack(list);
	mdmaCloseTag(list);
}

static mdmaTag *
upload(xtcPipeline *pipe, xtcPrimType primtype)
{
	mdmaTag *tag;
	mdmaList *l = xtcState.list;
	xtcStdMaterial *m = &xtcState.stdMaterial;

	xtcpCombineMatrix();
	xtcpUploadStdLights8(m, xtcState.stdColSel);

	// TODO: want TME bit more elegantly
	float *scl = (float*)&xtcState.colorScale[(xtcgRegs.prmode>>4)&1];
//scl = (float*)&xtcState.colorScale[1];
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

	tag = mdmaNext(l, nil, 13);
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

		// GIF tag and colorScale
		mdmaVifNop(l, 0);
		mdmaVifNop(l, 0);
		mdmaVifStCycl(l, 4,4, 0);
		mdmaBeginUnpack(l, vuGifTag, 2, UNPACK_V4_32, 0);
			mdmaGifTag(l, 0, 1, 1,primtype, GIF_PACKED, 3, xtcpVertRegs);
			mdmaAddF(l, scl[0], scl[1], scl[2], scl[3]);		// colorScale
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

extern xtcMicrocode xtcCodeStd;

static xtcPipeline pipe = {
	upload,
	&xtcCodeStd,
};
xtcPipeline *stdPipeline = &pipe;
