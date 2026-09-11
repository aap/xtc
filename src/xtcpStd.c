#include "xtci.h"
#include "xtcpipe.h"
#include "std_layout.h"

/*
 * STD_LAYOUT2: the upload writes the layout of std_layout.h, which the
 * std microcode reads through its equates; 0 is the RenderWare style
 * layout of xtcpipe.h, the way it was before 2026-09-11.
 */
#ifndef STD_LAYOUT2
#define STD_LAYOUT2 1
#endif
/*
 * STD_PIPELINE_CHAIN: the pipeline qwords hold the chain of stage
 * addresses the microcode walks (std_layout.h), from the xtcStdStages
 * tables; 0 keeps the code switch triple in the first qword, which is
 * what the microcode reads until it walks the chain.
 */
#ifndef STD_PIPELINE_CHAIN
#define STD_PIPELINE_CHAIN 1
#endif

extern uint32 xtcStdStages[STD_NUMSTAGES];

#include <stdio.h>
#include <string.h>
#include <libgraph.h>

extern uint32 xtcStdLightProcs[5];

int
getLightBlock(uint128 *lightDirs, Vec4 *lightCols)
{
	int ndir;
	Vec3 dir;
	float (*d)[4] = (float (*)[4])lightDirs;

	memset(lightDirs, 0, 4*sizeof(uint128));
	memset(lightCols, 0, 4*sizeof(uint128));

	ndir = 0;
	for(uint32 i = 0; i < nelem(xtcState.lights); i++) {
		xtcLight *l = &xtcState.lights[i];
		if(l->enabled && l->type == XTC_LIGHT_DIRECT && ndir < 4) {
			dir = m4invXformVecO(&xtcState.world, l->direction);
			d[0][ndir] = -dir.x;
			d[1][ndir] = -dir.y;
			d[2][ndir] = -dir.z;
			lightCols[ndir] = v4scale(255.0f, l->color);	// the VU works in 0..255
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
printf("%g %g %g %g\n", c[0].x, c[0].y, c[0].z, c[0].w);
printf("%g %g %g %g\n", c[1].x, c[1].y, c[1].z, c[1].w);
printf("%g %g %g %g\n", c[2].x, c[2].y, c[2].z, c[2].w);
printf("%g %g %g %g\n", c[3].x, c[3].y, c[3].z, c[3].w);
printf("\n\n");
*/
	return ndir;
}

int
getLightBlock8(uint128 *lightDirs, Vec4 *lightCols)
{
	int ndir;
	Vec3 dir;
	float (*d)[4] = (float (*)[4])lightDirs;

	memset(lightDirs, 0, 8*sizeof(uint128));
	memset(lightCols, 0, 8*sizeof(uint128));

	ndir = 0;
	for(uint32 i = 0; i < nelem(xtcState.lights); i++) {
		xtcLight *l = &xtcState.lights[i];
		if(l->enabled && l->type == XTC_LIGHT_DIRECT && ndir < 8) {
			dir = m4invXformVecO(&xtcState.world, l->direction);
			d[(ndir&4)+0][ndir&3] = -dir.x;
			d[(ndir&4)+1][ndir&3] = -dir.y;
			d[(ndir&4)+2][ndir&3] = -dir.z;
			lightCols[ndir] = v4scale(255.0f, l->color);	// the VU works in 0..255
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
printf("%g %g %g %g\n", c[0].x, c[0].y, c[0].z, c[0].w);
printf("%g %g %g %g\n", c[1].x, c[1].y, c[1].z, c[1].w);
printf("%g %g %g %g\n", c[2].x, c[2].y, c[2].z, c[2].w);
printf("%g %g %g %g\n", c[3].x, c[3].y, c[3].z, c[3].w);
printf("%g %g %g %g\n", c[4].x, c[4].y, c[4].z, c[4].w);
printf("%g %g %g %g\n", c[5].x, c[5].y, c[5].z, c[5].w);
printf("%g %g %g %g\n", c[6].x, c[6].y, c[6].z, c[6].w);
printf("%g %g %g %g\n", c[7].x, c[7].y, c[7].z, c[7].w);
printf("\n\n");
*/
	return ndir;
}

static int lightStage;	// the routine the last light upload picked, as xtcStdLightProcs' index

void
xtcpUploadStdLights8(xtcStdMaterial *m, uint32 colsel, uint32 *procs)
{
	Vec4 *c, *ambLt;
	uint128 lightDirs[8];
	Vec4 D[8];
	Vec4 A = { 0.0f, 0.0f, 0.0f, 0.0f };
	Vec4 B = { 0.0f, 0.0f, 0.0f, 0.0f };

	ambLt = &xtcState.ambient;
	int ndir = getLightBlock8(lightDirs, D);
	if(colsel & XTC_EMISSIVE)
		B.x = B.y = B.z = B.w = 1.0f;
	else
		A = v4scale(255.0f, m->emissive);
	if(colsel & XTC_AMBIENT) {
		B.x += ambLt->x;
		B.y += ambLt->y;
		B.z += ambLt->z;
		B.w += ambLt->w;
	} else {
		A.x += 255.0f*ambLt->x*m->ambient.x;
		A.y += 255.0f*ambLt->y*m->ambient.y;
		A.z += 255.0f*ambLt->z*m->ambient.z;
		A.w += 255.0f*ambLt->w*m->ambient.w;
	}
	if(!(colsel & XTC_DIFFUSE))
		for(int i = 0; i < 8; i++) {
			D[i].x *= m->diffuse.x;
			D[i].y *= m->diffuse.y;
			D[i].z *= m->diffuse.z;
			D[i].w *= m->diffuse.w;
		}

	uint32 *sel = (uint32*)&A.w;
	int l8 = ndir>4;
	if(ndir == 0)
		lightStage = 1;		// no directionals
	else if(colsel & XTC_DIFFUSE)
		lightStage = 3;		// no 8-light code yet
	else
		lightStage = l8 ? 4 : 2;
	*sel = procs[lightStage]>>3;

	mdmaList *list = xtcState.list;

#if STD_LAYOUT2
	// the base and the vertex factor lead, the clamp is in the
	// transform block; then the lights, 4 or 8 of them
	int lsz = STD_LIGHTS4_SIZE;
	if(l8) lsz += 7;
	mdmaCnt(list, lsz);
		mdmaVifStCycl(list, 4,4, 0);
		mdmaBeginUnpack(list, STD_LIGHTS, lsz, UNPACK_V4_32, 0);
		mdmaAddF(list, A.x, A.y, A.z, A.w);			// lightBase
		mdmaAddF(list, B.x, B.y, B.z, B.w);			// vertFactor
		mdmaAdd(list, lightDirs[0]);
		mdmaAdd(list, lightDirs[1]);
		mdmaAdd(list, lightDirs[2]);
		mdmaAddF(list, D[0].x, D[0].y, D[0].z, D[0].w);
		mdmaAddF(list, D[1].x, D[1].y, D[1].z, D[1].w);
		mdmaAddF(list, D[2].x, D[2].y, D[2].z, D[2].w);
		mdmaAddF(list, D[3].x, D[3].y, D[3].z, D[3].w);
		if(l8) {
			mdmaAdd(list, lightDirs[4]);
			mdmaAdd(list, lightDirs[5]);
			mdmaAdd(list, lightDirs[6]);
			mdmaAddF(list, D[4].x, D[4].y, D[4].z, D[4].w);
			mdmaAddF(list, D[5].x, D[5].y, D[5].z, D[5].w);
			mdmaAddF(list, D[6].x, D[6].y, D[6].z, D[6].w);
			mdmaAddF(list, D[7].x, D[7].y, D[7].z, D[7].w);
		}
		mdmaEndUnpack(list);
	mdmaCloseTag(list);
	return;
#else
	mdmaCnt(list, 2+16);
		mdmaVifStCycl(list, 4,4, 0);
		mdmaBeginUnpack(list, vuLight, 2+16, UNPACK_V4_32, 0);

		mdmaAddF(list, xtcState.colorMod.clamp.x, xtcState.colorMod.clamp.y,
			xtcState.colorMod.clamp.z, xtcState.colorMod.clamp.w);	// clamp
		mdmaAddF(list, B.x, B.y, B.z, B.w);			// vertFactor

		mdmaAdd(list, lightDirs[0]);
		mdmaAdd(list, lightDirs[1]);
		mdmaAdd(list, lightDirs[2]);
		mdmaAddF(list, A.x, A.y, A.z, A.w);
		mdmaAddF(list, D[0].x, D[0].y, D[0].z, D[0].w);
		mdmaAddF(list, D[1].x, D[1].y, D[1].z, D[1].w);
		mdmaAddF(list, D[2].x, D[2].y, D[2].z, D[2].w);
		mdmaAddF(list, D[3].x, D[3].y, D[3].z, D[3].w);
		mdmaAdd(list, lightDirs[4]);
		mdmaAdd(list, lightDirs[5]);
		mdmaAdd(list, lightDirs[6]);
		mdmaAddF(list, 0.0f, 0.0f, 0.0f, 0.0f);
		mdmaAddF(list, D[4].x, D[4].y, D[4].z, D[4].w);
		mdmaAddF(list, D[5].x, D[5].y, D[5].z, D[5].w);
		mdmaAddF(list, D[6].x, D[6].y, D[6].z, D[6].w);
		mdmaAddF(list, D[7].x, D[7].y, D[7].z, D[7].w);
#endif

		mdmaEndUnpack(list);
	mdmaCloseTag(list);
}

extern xtcMicrocode xtcCodeStd, xtcCodeStdSkin;

/*
 * What the VU holds from the last upload, so a draw that changes
 * nothing sends nothing: a model with ten materials costs one matrix
 * and one light block, not ten.  The generations are xtcState's; vuGen
 * covers the microcode switch, after which nothing up there is ours.
 */
static struct {
	uint32 xformGen, lightGen, matGen, vuGen;
	xtcMicrocode *code;
	int primtype, clipping;
	float scl[4];
} last = { 0, 0, 0, 0, nil, -1, -1, { 0, 0, 0, 0 } };
static uint32 combinedGen = ~0u;

static mdmaTag *
upload(xtcPipeline *pipe, xtcPrimType primtype, uint32 stages)
{
	mdmaTag *tag;
	mdmaList *l = xtcState.list;
	xtcStdMaterial *m = &xtcState.stdMaterial;
	int fresh = last.vuGen != xtcState.vuGen || last.code != pipe->code;
	int xform = fresh || last.xformGen != xtcState.xformGen;

	if(combinedGen != xtcState.xformGen) {
		xtcpCombineMatrix();
		combinedGen = xtcState.xformGen;
	}
	// the light block is in object space, so it follows the world
	// matrix as well as the lights and the material.  the light
	// routines live at different addresses in each program
	if(xform || last.lightGen != xtcState.lightGen || last.matGen != xtcState.matGen)
		xtcpUploadStdLights8(m, xtcState.stdColSel, xtcStdLightProcs);

	// the skin pipe's bone matrices, ref'd straight from the state:
	// once per draw, the batches never touch that part of VU memory
	if(pipe->code == &xtcCodeStdSkin && xtcState.numBoneMatrices > 0) {
		int n = 4*xtcState.numBoneMatrices;
		mdmaRef(l, xtcState.boneMatrices, n);
			mdmaVifStCycl(l, 4,4, 0);
#if STD_LAYOUT2
			mdmaVifUnpack(l, STD_BONEMATRICES, n, UNPACK_V4_32, 0);
#else
			mdmaVifUnpack(l, vuBoneMatrices, n, UNPACK_V4_32, 0);
#endif
	}

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

	// the rest is the matrices, the GIF tag for the prim type, the
	// colour scale and the code switch: unchanged, unsent
	if(!xform && last.primtype == (int)primtype && last.clipping == xtcState.clipping &&
	   last.scl[0] == scl[0] && last.scl[1] == scl[1] && last.scl[2] == scl[2] && last.scl[3] == scl[3]) {
		tag = mdmaNext(l, nil, 0);
		mdmaCloseTag(l);
		return tag;
	}
	last.xformGen = xtcState.xformGen;
	last.lightGen = xtcState.lightGen;
	last.matGen = xtcState.matGen;
	last.vuGen = xtcState.vuGen;
	last.code = pipe->code;
	last.primtype = primtype;
	last.clipping = xtcState.clipping;
	last.scl[0] = scl[0]; last.scl[1] = scl[1]; last.scl[2] = scl[2]; last.scl[3] = scl[3];

#if STD_LAYOUT2
	// one block: the matrices and their constants, the GIF tag, and
	// what the GS gets; then the pipeline.  clipConstI is not ours,
	// the microcode's own chain brings it with the mpg
	tag = mdmaNext(l, nil, MDMA_AUTO);
		mdmaVifFlush(l, 0);
		mdmaVifFlush(l, 0);
		mdmaVifBase(l, 0, 0);
		mdmaVifOffset(l, pipe->code->offset, 0);
		if(fresh) {
			// the layout's clip constants, with its buffers below
			mdmaVifStCycl(l, 4,4, 0);
			mdmaBeginUnpack(l, STD_CLIPCONSTI, 1, UNPACK_V4_32, 0);
				mdmaAddW(l, pipe->code->clipConsts[0], pipe->code->clipConsts[1],
					pipe->code->clipConsts[2], pipe->code->clipConsts[3]);
			mdmaEndUnpack(l);
			mdmaVifNop(l, 0);
			mdmaVifNop(l, 0);
		}
		mdmaVifStCycl(l, 4,4, 0);
		mdmaBeginUnpack(l, STD_XFORM, STD_XFORM_SIZE, UNPACK_V4_32, 0);
			mdmaAdd(l, xtcState.matrix0);
			mdmaAdd(l, xtcState.matrix1);
			mdmaAdd(l, xtcState.matrix2);
			mdmaAdd(l, xtcState.matrix3);
			mdmaAdd(l, xtcState.xyzwScale);
			mdmaAdd(l, xtcState.xyzwOffset);
			mdmaAdd(l, xtcState.clipConsts);			// clipConstF
			mdmaGifTag(l, 0, 1, 1,primtype, GIF_PACKED, 3, xtcpVertRegs);
			mdmaAddF(l, xtcState.colorMod.clamp.x, xtcState.colorMod.clamp.y,
				xtcState.colorMod.clamp.z, xtcState.colorMod.clamp.w);
			mdmaAddF(l, scl[0], scl[1], scl[2], scl[3]);
		mdmaEndUnpack(l);

#if STD_PIPELINE_CHAIN
		// the pipeline: what this draw runs, in order.  the data says
		// how it arrives (compressed, skinned), the state what happens
		// to it (the light routine, clipping), the prim type how it
		// leaves
		{
			int skin = (stages & XTCP_ST_SKIN) || pipe->code == &xtcCodeStdSkin;
			uint32 *st = xtcStdStages;
			uint32 chain[STD_PIPELINE_SIZE*4];
			int n = 0, i;
			// the rest is End: a jalr there ends the batch, a jalr to
			// 0 would restart the program
			for(i = 0; i < STD_PIPELINE_SIZE*4; i++)
				chain[i] = st[STD_STAGE_END];
			chain[n++] = st[skin ? STD_STAGE_PREP_SKIN_V32T32C8N8 :
				(stages & XTCP_ST_DECOMP16) ? STD_STAGE_PREP_V16T16C8N8 :
				STD_STAGE_PREP_V32T32C8N8];
			chain[n++] = st[STD_STAGE_LT_WHITEV + lightStage];
			if(!xtcState.clipping) chain[n++] = st[STD_STAGE_PROCESS];
			else chain[n++] = st[primtype == XTC_TRILIST ? STD_STAGE_TLCLIP : STD_STAGE_TSCLIP];
			for(i = 0; i < STD_PIPELINE_SIZE*4; i++)
				chain[i] >>= 3;
			// the chain, and the output buffers the submit stage flips
			// between: the pair of the mode, from the footer's switch
			// table, right after the chain so it is one unpack
			mdmaVifNop(l, 0);
			mdmaVifNop(l, 0);
			mdmaVifStCycl(l, 4,4, 0);
			mdmaBeginUnpack(l, STD_PIPELINE, STD_PIPELINE_SIZE + 1, UNPACK_V4_32, 0);
				for(i = 0; i < STD_PIPELINE_SIZE; i++)
					mdmaAddW(l, chain[4*i], chain[4*i+1], chain[4*i+2], chain[4*i+3]);
				mdmaAddW(l, swtch->buf1, swtch->buf2, 0, 0);		// STD_OUTBUFS
			mdmaEndUnpack(l);
		}
#else
		// the pipeline: the code switch as it is, in the first qword;
		// the stage chain replaces it when the microcode walks one
		mdmaVifNop(l, 0);
		mdmaVifNop(l, 0);
		mdmaVifStCycl(l, 4,4, 0);
		mdmaBeginUnpack(l, STD_PIPELINE, 1, UNPACK_V4_32, 0);
			mdmaAddW(l, swtch->process>>3, swtch->buf1, swtch->buf2, 0);
		mdmaEndUnpack(l);
#endif
	mdmaCloseTag(l);

	return tag;
#else
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
#endif
}

static xtcPipeline pipe = {
	upload,
	&xtcCodeStd,
};
xtcPipeline *stdPipeline = &pipe;

// the same upload with the skin microcode; the bone matrices are not
// uploaded yet, the code doesn't read them yet either
static xtcPipeline skinPipe = {
	upload,
	&xtcCodeStdSkin,
};
xtcPipeline *skinPipeline = &skinPipe;
