/*
 * std_layout.h -- the std pipe's VU1 data memory above the vertex
 * buffers.  One file for both sides: the microcode (every .dsm goes
 * through cpp) and the upload code in src/xtcpStd.c.  The RenderWare
 * style pipes keep their own layout in defines.inc and xtcpipe.h.
 *
 * Grouped by what changes together, so an upload is one unpack per
 * group: the lights (8 or 16 qwords), the transform and colour block
 * (11 qwords), the pipeline (up to 3).  The last four qwords are not
 * the upload's: a prim list unpacks its own dequantization constants,
 * and the microcode's own chain unpacks clipConstI with the mpg, which
 * is what makes the code independent of the buffer layout.
 * From std_layout.txt.
 */

#ifndef STD_LAYOUT_H
#define STD_LAYOUT_H

/* skin: 64 bones, 4 qwords each, only when skinning */
#define STD_BONEMATRICES	0x2d0

/* lighting.  the base colour (emissive and ambient, the light routine
 * in w) and what of the vertex colour the lighting keeps, then lights
 * 0..3: the direction rows (x of all four in one qword, then y, then
 * z) and four colours; lights 4..7 the same again.  colours 0..255 */
#define STD_LIGHTS		0x3d0	/* the block */
#define STD_LIGHTBASE		0x3d0
#define STD_VERTFACTOR		0x3d1
#define STD_LIGHTDIRX		0x3d2
#define STD_LIGHTDIRY		0x3d3
#define STD_LIGHTDIRZ		0x3d4
#define STD_LIGHTCOL0		0x3d5
#define STD_LIGHTCOL1		0x3d6
#define STD_LIGHTCOL2		0x3d7
#define STD_LIGHTCOL3		0x3d8
#define STD_LIGHTDIRXB		0x3d9
#define STD_LIGHTDIRYB		0x3da
#define STD_LIGHTDIRZB		0x3db
#define STD_LIGHTCOL4		0x3dc
#define STD_LIGHTCOL5		0x3dd
#define STD_LIGHTCOL6		0x3de
#define STD_LIGHTCOL7		0x3df
#define STD_LIGHTS4_SIZE	9
#define STD_LIGHTS8_SIZE	16

/* 0x3e0 .. 0x3eb spare */

/* transform and clipping, then what the GS gets: one block */
#define STD_XFORM		0x3ec	/* the block */
#define STD_MATRIX0		0x3ec
#define STD_MATRIX1		0x3ed
#define STD_MATRIX2		0x3ee
#define STD_MATRIX3		0x3ef
#define STD_XYZWSCALE		0x3f0
#define STD_XYZWOFFSET		0x3f1
#define STD_CLIPCONSTF		0x3f2
#define STD_GIFTAG		0x3f3
#define STD_COLORCLAMP		0x3f4	/* 0..255, see xtcColorMod */
#define STD_COLORSCALE		0x3f5
#define STD_XFORM_SIZE		10

/* the pipeline: the stages a draw runs, as VU instruction addresses
 * (byte offsets >> 3), x y z w of each qword in order, up to 12; the
 * microcode calls each in turn and the last one, a submit stage, ends
 * the batch.  Zero pads the rest.  Right after the block so one unpack
 * can carry both */
#define STD_PIPELINE		0x3f6	/* the block */
#define STD_PIPELINE0		0x3f6
#define STD_PIPELINE1		0x3f7
#define STD_PIPELINE2		0x3f8
#define STD_PIPELINE_SIZE	3
#define STD_PIPELINE_MAX	12

/* the submit stage's two output buffers, which pair depends on whether
 * it clips: x buffer 1, y buffer 2, per draw with the pipeline, so the
 * two are one unpack */
#define STD_OUTBUFS		0x3f9
/* 0x3fa spare */

/* the stages the microcode exports, the order of its xtcStdStages
 * table (a .word per entry, 0 where a program has no such stage) */
#define STD_STAGE_END			0
#define STD_STAGE_PREP_V32T32C8N8	1	/* the input as it is */
#define STD_STAGE_PREP_V16T16C8N8	2	/* dequantize positions and texcoords */
#define STD_STAGE_PREP_SKIN_V32T32C8N8	3	/* skin, then repack */
#define STD_STAGE_TEXGEN		4	/* not yet */
#define STD_STAGE_LT_WHITEV		5	/* the light routines, in xtcStdLightProcs' order */
#define STD_STAGE_LT_BASEV		6
#define STD_STAGE_LT_BASEVDIR		7
#define STD_STAGE_LT_BASEVDIRV		8
#define STD_STAGE_LT_BASEVDIR8		9
#define STD_STAGE_PROCESS		10	/* transform and submit */
#define STD_STAGE_TLCLIP		11	/* the same, clipping tri lists */
#define STD_STAGE_TSCLIP		12	/* and tri strips */
#define STD_NUMSTAGES			13
/* 0x3fb spare */

/* not the upload's */
#define STD_UNXYZSCALE		0x3fc	/* the prim list's: pos = q*scale + off */
#define STD_UNXYZOFF		0x3fd
#define STD_UNUVSCALE		0x3fe	/* uv = q*scale */
#define STD_CLIPCONSTI		0x3ff	/* clipBuf, clipVertLimitTL, clipVertLimitTS: with the mpg */

#endif
