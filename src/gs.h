#ifndef GS_H_
#define GS_H_

/* Constants and macros from PS2SDK */

#define GS_NONINTERLACED 0x00
#define GS_INTERLACED 0x01

/** Read every other line from the beginning with the start of FIELD. */
#define GS_FFMD_FIELD 0x00
/** Read every line from the beginning with the start of FRAME. */
#define GS_FFMD_FRAME 0x01

enum GsVideoModes {
	GS_MODE_NTSC = 0x02,
	GS_MODE_PAL,

	GS_MODE_DTV_480P = 0x50,
};

#define GS_DISABLE 0
#define GS_ENABLE 1

/** types of primitives */
enum GsPrimitiveTypes {
	GS_PRIM_POINT = 0,
	GS_PRIM_LINE,
	GS_PRIM_LINE_STRIP,
	GS_PRIM_TRI,
	GS_PRIM_TRI_STRIP,
	GS_PRIM_TRI_FAN,
	GS_PRIM_SPRITE
};

/** regular Pixel Storage Modes (PSM) */
#define GS_PIXMODE_32 0
#define GS_PIXMODE_24 1
#define GS_PIXMODE_16 2
#define GS_PIXMODE_16S 10

/** clut Pixel Storage Modes (PSM) */
#define GS_CLUT_32 0
#define GS_CLUT_16 2
#define GS_CLUT_16S 10

/** texture/image Pixel Storage Modes (PSM) */
#define GS_TEX_32 0
#define GS_TEX_24 1
#define GS_TEX_16 2
#define GS_TEX_16S 10
#define GS_TEX_8 19
#define GS_TEX_4 20
#define GS_TEX_8H 27
#define GS_TEX_4HL 36
#define GS_TEX_4HH 44

/** Z-Buffer Pixel Storage Modes (PSM) */
#define GS_ZBUFF_32 48
#define GS_ZBUFF_24 49
#define GS_ZBUFF_16 50
#define GS_ZBUFF_16S 58

/** Alpha test Methods */
enum GsATestMethods {
	GS_ALPHA_NEVER = 0,
	GS_ALPHA_ALWAYS,
	GS_ALPHA_LESS,
	GS_ALPHA_LEQUAL,
	GS_ALPHA_EQUAL,
	GS_ALPHA_GEQUAL,
	GS_ALPHA_GREATER,
	GS_ALPHA_NOTEQUAL,
};

/** Alpha test failed update Methods */
enum GsATestFailedUpdateMethods {
	/** standard */
	GS_ALPHA_NO_UPDATE = 0,
	GS_ALPHA_FB_ONLY,
	GS_ALPHA_ZB_ONLY,
	GS_ALPHA_RGB_ONLY
};

/** Zbuffer test Methods */
enum GsZTestMethodTypes { GS_ZBUFF_NEVER = 0, GS_ZBUFF_ALWAYS, GS_ZBUFF_GEQUAL, GS_ZBUFF_GREATER };

// Texture Details
/** use near/far formula */
#define GS_TEX_CALC 0
/** fixed value (use K value) */
#define GS_TEX_FIXED 1

enum GsTexMipmaps {
	/** no mipmap */
	GS_TEX_MIPMAP0 = 0,
	/** 1 mipmap */
	GS_TEX_MIPMAP1,
	/** 2 mipmaps */
	GS_TEX_MIPMAP2,
	/** 3 mipmaps */
	GS_TEX_MIPMAP3,
	/** 4 mipmaps */
	GS_TEX_MIPMAP4,
	/** 5 mipmaps */
	GS_TEX_MIPMAP5,
	/** 6 mipmaps */
	GS_TEX_MIPMAP6
};

enum GsTexFilterMethods {
	/** UnFiltered */
	GS_TEX_NEAREST = 0,
	/** Filtered */
	GS_TEX_LINEAR,
	GS_TEX_NEAREST_MIPMAP_NEAREST,
	GS_TEX_NEAREST_MIPMAP_LINEAR,
	GS_TEX_LINEAR_MIPMAP_NEAREST,
	GS_TEX_LINEAR_MIPMAP_LINEAR
};

/** use values in MIPTBP1 */
#define GS_TEX_MIPMAP_DEFINE 0
/** auto calculate mipmap address */
#define GS_TEX_MIPMAP_AUTO 1

/** Texture Function (used in TEX0->tex_cc) */
enum GsTexFunctions {
	GS_TEX_MODULATE = 0, /** brighten texture based on Pixel's Alpha */
	GS_TEX_DECAL,	     /** keep texture as is */
	GS_TEX_HIGHLIHGT1,   /** used when highlighting translucent polygons */
	GS_TEX_HIGHLIHGT2    /** used when highlighting opaque polygons */
};

enum GsGifDataFormat {
	GS_GIF_PACKED = 0,
	GS_GIF_REGLIST,
	GS_GIF_IMAGE,
	/** Same operation with the IMAGE mode */
	GS_GIF_DISABLE
};

// General Purpose Registers
/** Drawing primitive setting. */
#define GS_REG_PRIM 0x00
/** Vertex color setting. */
#define GS_REG_RGBAQ 0x01
/** Specification of vertex texture coordinates. */
#define GS_REG_ST 0x02
/** Specification of vertex texture coordinates. */
#define GS_REG_UV 0x03
/** Setting for vertex coordinate values. */
#define GS_REG_XYZF2 0x04
/** Setting for vertex coordinate values. */
#define GS_REG_XYZ2 0x05
/** Texture information setting. */
#define GS_REG_TEX0 0x06
/** Texture information setting. (Context 1) */
#define GS_REG_TEX0_1 0x06
/** Texture information setting. (Context 2) */
#define GS_REG_TEX0_2 0x07
/** Texture wrap mode. */
#define GS_REG_CLAMP 0x08
/** Texture wrap mode. (Context 1) */
#define GS_REG_CLAMP_1 0x08
/** Texture wrap mode. (Context 2) */
#define GS_REG_CLAMP_2 0x09
/** Vertex fog value setting. */
#define GS_REG_FOG 0x0A
/** Setting for vertex coordinate values. (Without Drawing Kick) */
#define GS_REG_XYZF3 0x0C
/** Setting for vertex coordinate values. (Without Drawing Kick) */
#define GS_REG_XYZ3 0x0D
/** Texture information setting. */
#define GS_REG_TEX1 0x14
/** Texture information setting. (Context 1) */
#define GS_REG_TEX1_1 0x14
/** Texture information setting. (Context 2) */
#define GS_REG_TEX1_2 0x15
/** Texture information setting. */
#define GS_REG_TEX2 0x16
/** Texture information setting. (Context 1) */
#define GS_REG_TEX2_1 0x16
/** Texture information setting. (Context 2) */
#define GS_REG_TEX2_2 0x17
/** Offset value setting. */
#define GS_REG_XYOFFSET 0x18
/** Offset value setting. (Context 1) */
#define GS_REG_XYOFFSET_1 0x18
/** Offset value setting. (Context 2) */
#define GS_REG_XYOFFSET_2 0x19
/** Specification of primitive attribute setting method. */
#define GS_REG_PRMODECONT 0x1A
/** Setting for attributes of drawing primitives. */
#define GS_REG_PRMODE 0x1B
/** Clut position specification. */
#define GS_REG_TEXCLUT 0x1C
/** Raster address mask setting. */
#define GS_REG_SCANMSK 0x22
/** Mipmap information setting for levels 1 - 3. */
#define GS_REG_MIPTBP1 0x34
/** Mipmap information setting for levels 1 - 3. (Context 1) */
#define GS_REG_MIPTBP1_1 0x34
/** Mipmap information setting for levels 1 - 3. (Context 2) */
#define GS_REG_MIPTBP1_2 0x35
/** Mipmap information setting for levels 4 - 6. */
#define GS_REG_MIPTBP2 0x36
/** Mipmap information setting for levels 4 - 6. (Context 1) */
#define GS_REG_MIPTBP2_1 0x36
/** Mipmap information setting for levels 4 - 6. (Context 2) */
#define GS_REG_MIPTBP2_2 0x37
/** Texture alpha value setting. */
#define GS_REG_TEXA 0x3B
/** Distant fog color setting. */
#define GS_REG_FOGCOL 0x3D
/** Texture page buffer disabling. */
#define GS_REG_TEXFLUSH 0x3F
/** Setting for scissoring area. */
#define GS_REG_SCISSOR 0x40
/** Setting for scissoring area. (Context 1) */
#define GS_REG_SCISSOR_1 0x40
/** Setting for scissoring area. (Context 2) */
#define GS_REG_SCISSOR_2 0x41
/** Alpha blending setting. */
#define GS_REG_ALPHA 0x42
/** Alpha blending setting. (Context 1) */
#define GS_REG_ALPHA_1 0x42
/** Alpha blending setting. (Context 2) */
#define GS_REG_ALPHA_2 0x43
/** Dither matrix setting. */
#define GS_REG_DIMX 0x44
/** Dither control. */
#define GS_REG_DTHE 0x45
/** Color clamp control. */
#define GS_REG_COLCLAMP 0x46
/** Pixel test control. */
#define GS_REG_TEST 0x47
/** Pixel test control. (Context 1) */
#define GS_REG_TEST_1 0x47
/** Pixel test control. (Context 2) */
#define GS_REG_TEST_2 0x48
/** Alpha blending control in units of pixels. */
#define GS_REG_PABE 0x49
/** Alpha correction value. */
#define GS_REG_FBA 0x4A
/** Alpha correction value. (Context 1) */
#define GS_REG_FBA_1 0x4A
/** Alpha correction value. (Context 2) */
#define GS_REG_FBA_2 0x4B
/** Frame buffer setting. */
#define GS_REG_FRAME 0x4C
/** Frame buffer setting. (Context 1) */
#define GS_REG_FRAME_1 0x4C
/** Frame buffer setting. (Context 2) */
#define GS_REG_FRAME_2 0x4D
/** Z-Buffer setting. */
#define GS_REG_ZBUF 0x4E
/** Z-Buffer setting. (Context 1) */
#define GS_REG_ZBUF_1 0x4E
/** Z-Buffer setting. (Context 2) */
#define GS_REG_ZBUF_2 0x4F
/** Setting for transmissions between buffers. */
#define GS_REG_BITBLTBUF 0x50
/** Specification of transmission area in buffers. */
#define GS_REG_TRXPOS 0x51
/** Specification of transmission area in buffers. */
#define GS_REG_TRXREG 0x52
/** Activation of transmission area in buffers. */
#define GS_REG_TRXDIR 0x53
/** Data port for transmission between buffers. */
#define GS_REG_HWREG 0x54
/** Signal event occurence request. */
#define GS_REG_SIGNAL 0x60
/** Finish event occurence request. */
#define GS_REG_FINISH 0x61
/** Label event occurence request. */
#define GS_REG_LABEL 0x62
/** GS No Operation */
#define GS_REG_NOP 0x7F

/** GIFtag Address+Data */
#define GIF_REG_AD 0x0E
/** GIFtag No Operation */
#define GIF_REG_NOP 0x0F

#define GIF_SET_TAG(NLOOP, EOP, PRE, PRIM, FLG, NREG)                                  \
	(uint64)((NLOOP) & 0x00007FFF) << 0 | (uint64)((EOP) & 0x00000001) << 15 |     \
	    (uint64)((PRE) & 0x00000001) << 46 | (uint64)((PRIM) & 0x000007FF) << 47 | \
	    (uint64)((FLG) & 0x00000003) << 58 | (uint64)((NREG) & 0x0000000F) << 60

#define GS_SET_ALPHA(A, B, C, D, ALPHA)                                         \
	(uint64)((A) & 0x00000003) << 0 | (uint64)((B) & 0x00000003) << 2 |     \
	    (uint64)((C) & 0x00000003) << 4 | (uint64)((D) & 0x00000003) << 6 | \
	    (uint64)((ALPHA) & 0x000000FF) << 32

#define GS_SET_BITBLTBUF(SBA, SBW, SPSM, DBA, DBW, DPSM)                               \
	(uint64)((SBA) & 0x00003FFF) << 0 | (uint64)((SBW) & 0x0000003F) << 16 |       \
	    (uint64)((SPSM) & 0x0000003F) << 24 | (uint64)((DBA) & 0x00003FFF) << 32 | \
	    (uint64)((DBW) & 0x0000003F) << 48 | (uint64)((DPSM) & 0x0000003F) << 56

#define GS_SET_CLAMP(WMS, WMT, MINU, MAXU, MINV, MAXV)                                 \
	(uint64)((WMS) & 0x00000003) << 0 | (uint64)((WMT) & 0x00000003) << 2 |        \
	    (uint64)((MINU) & 0x000003FF) << 4 | (uint64)((MAXU) & 0x000003FF) << 14 | \
	    (uint64)((MINV) & 0x000003FF) << 24 | (uint64)((MAXV) & 0x000003FF) << 34

#define GS_SET_COLCLAMP(CLAMP) (uint64)((CLAMP) & 0x00000001)

#define GS_SET_DIMX(D00, D01, D02, D03, D10, D11, D12, D13, D20, D21, D22, D23, D30, D31, D32, \
    D33)                                                                                       \
	(uint64)((D00) & 0x00000003) << 0 | (uint64)((D01) & 0x00000003) << 4 |                \
	    (uint64)((D02) & 0x00000003) << 8 | (uint64)((D03) & 0x00000003) << 12 |           \
	    (uint64)((D10) & 0x00000003) << 16 | (uint64)((D11) & 0x00000003) << 20 |          \
	    (uint64)((D12) & 0x00000003) << 24 | (uint64)((D13) & 0x00000003) << 28 |          \
	    (uint64)((D20) & 0x00000003) << 32 | (uint64)((D21) & 0x00000003) << 36 |          \
	    (uint64)((D22) & 0x00000003) << 40 | (uint64)((D23) & 0x00000003) << 44 |          \
	    (uint64)((D30) & 0x00000003) << 48 | (uint64)((D31) & 0x00000003) << 52 |          \
	    (uint64)((D32) & 0x00000003) << 56 | (uint64)((D33) & 0x00000003) << 60

#define GS_SET_DTHE(ENABLE) (uint64)((ENABLE) & 0x00000001)

#define GS_SET_FBA(ALPHA) (uint64)((ALPHA) & 0x00000001)

#define GS_SET_FINISH(A) (uint64)((A) & 0xFFFFFFFF)

#define GS_SET_FOG(FOG) (uint64)((FOG) & 0x000000FF) << 56

#define GS_SET_FOGCOL(R, G, B)                                              \
	(uint64)((R) & 0x000000FF) << 0 | (uint64)((G) & 0x000000FF) << 8 | \
	    (uint64)((B) & 0x000000FF) << 16

#define GS_SET_FRAME(FBA, FBW, PSM, FMSK)                                        \
	(uint64)((FBA) & 0x000001FF) << 0 | (uint64)((FBW) & 0x0000003F) << 16 | \
	    (uint64)((PSM) & 0x0000003F) << 24 | (uint64)((FMSK) & 0xFFFFFFFF) << 32

// PSMCT16 FMSK for GS_SET_FRAME
#define GS_SET_FMSK16(R, G, B, A)                                      \
	(u32)((R) & 0x0000001F) << 3 | (u32)((G) & 0x0000001F) << 11 | \
	    (u32)((G) & 0x0000001F) << 19 | (u32)((A) & 0x00000001) << 31

#define GS_SET_HWREG(A) (uint64)((A) & 0xFFFFFFFFFFFFFFFF)

#define GS_SET_LABEL(ID, MSK) (uint64)((ID) & 0xFFFFFFFF) << 0 | (uint64)((MSK) & 0xFFFFFFFF) << 32

#define GS_SET_MIPTBP1(TBA1, TBW1, TBA2, TBW2, TBA3, TBW3)                              \
	(uint64)((TBA1) & 0x00003FFF) << 0 | (uint64)((TBW1) & 0x0000003F) << 14 |      \
	    (uint64)((TBA2) & 0x00003FFF) << 20 | (uint64)((TBW2) & 0x0000003F) << 34 | \
	    (uint64)((TBA3) & 0x00003FFF) << 40 | (uint64)((TBW3) & 0x0000003F) << 54

#define GS_SET_MIPTBP2(TBA4, TBW4, TBA5, TBW5, TBA6, TBW6)                              \
	(uint64)((TBA4) & 0x00003FFF) << 0 | (uint64)((TBW4) & 0x0000003F) << 14 |      \
	    (uint64)((TBA5) & 0x00003FFF) << 20 | (uint64)((TBW5) & 0x0000003F) << 34 | \
	    (uint64)((TBA6) & 0x00003FFF) << 40 | (uint64)((TBW6) & 0x0000003F) << 54

#define GS_SET_NOP(A) (uint64)((A) & 0xFFFFFFFF)

#define GS_SET_PABE(ENABLE) (uint64)((ENABLE) & 0x00000001)

#define GS_SET_PRIM(PRIM, IIP, TME, FGE, ABE, AA1, FST, CTXT, FIX)                   \
	(uint64)((PRIM) & 0x00000007) << 0 | (uint64)((IIP) & 0x00000001) << 3 |     \
	    (uint64)((TME) & 0x00000001) << 4 | (uint64)((FGE) & 0x00000001) << 5 |  \
	    (uint64)((ABE) & 0x00000001) << 6 | (uint64)((AA1) & 0x00000001) << 7 |  \
	    (uint64)((FST) & 0x00000001) << 8 | (uint64)((CTXT) & 0x00000001) << 9 | \
	    (uint64)((FIX) & 0x00000001) << 10

#define GS_SET_PRMODE(IIP, TME, FGE, ABE, AA1, FST, CTXT, FIX)                      \
	(uint64)((IIP) & 0x00000001) << 3 | (uint64)((TME) & 0x00000001) << 4 |     \
	    (uint64)((FGE) & 0x00000001) << 5 | (uint64)((ABE) & 0x00000001) << 6 | \
	    (uint64)((AA1) & 0x00000001) << 7 | (uint64)((FST) & 0x00000001) << 8 | \
	    (uint64)((CTXT) & 0x00000001) << 9 | (uint64)((FIX) & 0x00000001) << 10

#define GS_SET_PRMODECONT(CTRL) (uint64)((CTRL) & 0x00000001)

#define GS_SET_RGBAQ(R, G, B, A, Q)                                               \
	(uint64)((R) & 0x000000FF) << 0 | (uint64)((G) & 0x000000FF) << 8 |       \
	    (uint64)((B) & 0x000000FF) << 16 | (uint64)((A) & 0x000000FF) << 24 | \
	    (uint64)((Q) & 0xFFFFFFFF) << 32

#define GS_SET_SCANMSK(MSK) (uint64)((MSK) & 0x00000003)

#define GS_SET_SCISSOR(X0, X1, Y0, Y1)                                         \
	(uint64)((X0) & 0x000007FF) << 0 | (uint64)((X1) & 0x000007FF) << 16 | \
	    (uint64)((Y0) & 0x000007FF) << 32 | (uint64)((Y1) & 0x000007FF) << 48

#define GS_SET_SIGNAL(ID, MSK) (uint64)((ID) & 0xFFFFFFFF) << 0 | (uint64)((MSK) & 0xFFFFFFFF) << 32

#define GS_SET_ST(S, T) (uint64)((S) & 0xFFFFFFFF) << 0 | (uint64)((T) & 0xFFFFFFFF) << 32

#define GS_SET_TEST(ATEN, ATMETH, ATREF, ATFAIL, DATEN, DATMD, ZTEN, ZTMETH)              \
	(uint64)((ATEN) & 0x00000001) << 0 | (uint64)((ATMETH) & 0x00000007) << 1 |       \
	    (uint64)((ATREF) & 0x000000FF) << 4 | (uint64)((ATFAIL) & 0x00000003) << 12 | \
	    (uint64)((DATEN) & 0x00000001) << 14 | (uint64)((DATMD) & 0x00000001) << 15 | \
	    (uint64)((ZTEN) & 0x00000001) << 16 | (uint64)((ZTMETH) & 0x00000003) << 17

#define GS_SET_TEX0_SMALL(TBA, TBW, PSM, TW, TH, TCC, TFNCT)                         \
	(uint64)((TBA) & 0x00003FFF) << 0 | (uint64)((TBW) & 0x0000003F) << 14 |     \
	    (uint64)((PSM) & 0x0000003F) << 20 | (uint64)((TW) & 0x0000000F) << 26 | \
	    (uint64)((TH) & 0x0000000F) << 30 | (uint64)((TCC) & 0x00000001) << 34 | \
	    (uint64)((TFNCT) & 0x00000003) << 35

#define GS_SET_TEX0(TBA, TBW, PSM, TW, TH, TCC, TFNCT, CBA, CPSM, CSM, CSA, CLD)        \
	(uint64)((TBA) & 0x00003FFF) << 0 | (uint64)((TBW) & 0x0000003F) << 14 |        \
	    (uint64)((PSM) & 0x0000003F) << 20 | (uint64)((TW) & 0x0000000F) << 26 |    \
	    (uint64)((TH) & 0x0000000F) << 30 | (uint64)((TCC) & 0x00000001) << 34 |    \
	    (uint64)((TFNCT) & 0x00000003) << 35 | (uint64)((CBA) & 0x00003FFF) << 37 | \
	    (uint64)((CPSM) & 0x0000000F) << 51 | (uint64)((CSM) & 0x00000001) << 55 |  \
	    (uint64)((CSA) & 0x0000001F) << 56 | (uint64)((CLD) & 0x00000007) << 61

#define GS_SET_TEX1(LCM, MXL, MMAG, MMIN, MTBA, L, K)                                 \
	(uint64)((LCM) & 0x00000001) << 0 | (uint64)((MXL) & 0x00000007) << 2 |       \
	    (uint64)((MMAG) & 0x00000001) << 5 | (uint64)((MMIN) & 0x00000007) << 6 | \
	    (uint64)((MTBA) & 0x00000001) << 9 | (uint64)((L) & 0x00000003) << 19 |   \
	    (uint64)((K) & 0x00000FFF) << 32

#define GS_SET_TEX2(PSM, CBA, CPSM, CSM, CSA, CLD)                                     \
	(uint64)((PSM) & 0x0000003F) << 20 | (uint64)((CBA) & 0x00003FFF) << 37 |      \
	    (uint64)((CPSM) & 0x0000000F) << 51 | (uint64)((CSM) & 0x00000001) << 55 | \
	    (uint64)((CSA) & 0x0000001F) << 56 | (uint64)((CLD) & 0x00000007) << 61

#define GS_SET_TEXA(A0, AM, A1)                                                \
	(uint64)((A0) & 0x000000FF) << 0 | (uint64)((AM) & 0x00000001) << 15 | \
	    (uint64)((A1) & 0x000000FF) << 32

#define GS_SET_TEXCLUT(CBW, CU, CV)                                            \
	(uint64)((CBW) & 0x0000003F) << 0 | (uint64)((CU) & 0x0000003F) << 6 | \
	    (uint64)((CV) & 0x00000FFF) << 12

#define GS_SET_TRXDIR(DIR) (uint64)((DIR) & 0x00000003)

#define GS_SET_TEXFLUSH(A) (uint64)((A) & 0xFFFFFFFF)

#define GS_SET_TRXPOS(SX, SY, DX, DY, DIR)                                          \
	(uint64)((SX) & 0x000007FF) << 0 | (uint64)((SY) & 0x000007FF) << 16 |      \
	    (uint64)((DX) & 0x000007FF) << 32 | (uint64)((DY) & 0x000007FF) << 48 | \
	    (uint64)((DIR) & 0x00000003) << 59

#define GS_SET_TRXREG(W, H) (uint64)((W) & 0x00000FFF) << 0 | (uint64)((H) & 0x00000FFF) << 32

#define GS_SET_UV(U, V) (uint64)((U) & 0x00003FFF) << 0 | (uint64)((V) & 0x00003FFF) << 16

#define GS_SET_XYOFFSET(X, Y) (uint64)((X) & 0x0000FFFF) << 0 | (uint64)((Y) & 0x0000FFFF) << 32

#define GS_SET_XYZ(X, Y, Z)                                                  \
	(uint64)((X) & 0x0000FFFF) << 0 | (uint64)((Y) & 0x0000FFFF) << 16 | \
	    (uint64)((Z) & 0xFFFFFFFF) << 32

#define GS_SET_XYZF(X, Y, Z, F)                                              \
	(uint64)((X) & 0x0000FFFF) << 0 | (uint64)((Y) & 0x0000FFFF) << 16 | \
	    (uint64)((Z) & 0x00FFFFFF) << 32 | (uint64)((F) & 0x000000FF) << 56

#define GS_SET_ZBUF(ZBA, ZSM, ZMSK)                                              \
	(uint64)((ZBA) & 0x000001FF) << 0 | (uint64)((ZSM) & 0x0000000F) << 24 | \
	    (uint64)((ZMSK) & 0x00000001) << 32

#define GS_SET_BGCOLOR(R, G, B)                                              \
	((uint64)((R) & 0x000000FF) << 0 | (uint64)((G) & 0x000000FF) << 8 | \
	    (uint64)((B) & 0x000000FF) << 16)

#define GS_SET_BUSDIR(DIR) ((uint64)((DIR) & 0x00000001))

// pcsx2's source defines two more regs as ZERO1 and ZERO2
// probably need to be set 0
#define GS_SET_CSR(SIGNAL, FINISH, HSINT, VSINT, EDWINT, FLUSH, RESET, NFIELD, FIELD, FIFO, REV, \
    ID)                                                                                          \
	((uint64)((SIGNAL) & 0x00000001) << 0 | (uint64)((FINISH) & 0x00000001) << 1 |           \
	    (uint64)((HSINT) & 0x00000001) << 2 | (uint64)((VSINT) & 0x00000001) << 3 |          \
	    (uint64)((EDWINT) & 0x00000001) << 4 | (uint64)((0) & 0x00000001) << 5 |             \
	    (uint64)((0) & 0x00000001) << 6 | (uint64)((FLUSH) & 0x00000001) << 8 |              \
	    (uint64)((RESET) & 0x00000001) << 9 | (uint64)((NFIELD) & 0x00000001) << 12 |        \
	    (uint64)((FIELD) & 0x00000001) << 13 | (uint64)((FIFO) & 0x00000003) << 14 |         \
	    (uint64)((REV) & 0x000000FF) << 16 | (uint64)((ID) & 0x000000FF) << 24)

#define GS_SET_DISPFB(FBP, FBW, PSM, DBX, DBY)                                        \
	((uint64)((FBP) & 0x000001FF) << 0 | (uint64)((FBW) & 0x0000003F) << 9 |      \
	    (uint64)((PSM) & 0x0000001F) << 15 | (uint64)((DBX) & 0x000007FF) << 32 | \
	    (uint64)((DBY) & 0x000007FF) << 43)

#define GS_SET_DISPLAY(DX, DY, MAGH, MAGV, DW, DH)                                      \
	((uint64)((DX) & 0x00000FFF) << 0 | (uint64)((DY) & 0x000007FF) << 12 |         \
	    (uint64)((MAGH) & 0x0000000F) << 23 | (uint64)((MAGV) & 0x00000003) << 27 | \
	    (uint64)((DW) & 0x00000FFF) << 32 | (uint64)((DH) & 0x000007FF) << 44)

#define GS_SET_EXTBUF(EXBP, EXBW, FBIN, WFFMD, EMODA, EMODC, WDX, WDY)                    \
	((uint64)((EXBP) & 0x00003FFF) << 0 | (uint64)((EXBW) & 0x0000003F) << 14 |       \
	    (uint64)((FBIN) & 0x00000003) << 20 | (uint64)((WFFMD) & 0x00000001) << 22 |  \
	    (uint64)((EMODA) & 0x00000003) << 23 | (uint64)((EMODC) & 0x00000003) << 25 | \
	    (uint64)((WDX) & 0x000007FF) << 32 | (uint64)((WDY) & 0x000007FF) << 43)

#define GS_SET_EXTDATA(SX, SY, SMPH, SMPV, WW, WH)                                      \
	((uint64)((SX) & 0x00000FFF) << 0 | (uint64)((SY) & 0x000007FF) << 12 |         \
	    (uint64)((SMPH) & 0x0000000F) << 23 | (uint64)((SMPV) & 0x00000003) << 27 | \
	    (uint64)((WW) & 0x00000FFF) << 32 | (uint64)((WH) & 0x000007FF) << 44)

#define GS_SET_EXTWRITE(WRITE) ((uint64)((WRITE) & 0x00000001))

#define GS_SET_IMR(SIGMSK, FINMSK, HSMSK, VSMSK, EDWMSK)                                  \
	((uint64)((SIGMSK) & 0x00000001) << 8 | (uint64)((FINMSK) & 0x00000001) << 9 |    \
	    (uint64)((HSMSK) & 0x00000001) << 10 | (uint64)((VSMSK) & 0x00000001) << 11 | \
	    (uint64)((EDWMSK) & 0x00000001) << 12 | (uint64)((1) & 0x00000001) << 13 |    \
	    (uint64)((1) & 0x00000001) << 14)

// I guess CRTMD is always set 1
#define GS_SET_PMODE(EN1, EN2, MMOD, AMOD, SLBG, ALP)                                 \
	((uint64)((EN1) & 0x00000001) << 0 | (uint64)((EN2) & 0x00000001) << 1 |      \
	    (uint64)((1) & 0x00000007) << 2 | (uint64)((MMOD) & 0x00000001) << 5 |    \
	    (uint64)((AMOD) & 0x00000001) << 6 | (uint64)((SLBG) & 0x00000001) << 7 | \
	    (uint64)((ALP) & 0x000000FF) << 8 | (uint64)((0) & 0x00000001) << 16)

#define GS_SET_PMODE_EXT(EN1, EN2, MMOD, AMOD, SLBG, ALP, NFLD, EXVWINS, EXVWINE, EXSYNCMD)   \
	((uint64)((EN1) & 0x00000001) << 0 | (uint64)((EN2) & 0x00000001) << 1 |              \
	    (uint64)((1) & 0x00000007) << 2 | (uint64)((MMOD) & 0x00000001) << 5 |            \
	    (uint64)((AMOD) & 0x00000001) << 6 | (uint64)((SLBG) & 0x00000001) << 7 |         \
	    (uint64)((ALP) & 0x000000FF) << 8 | (uint64)((NFLD) & 0x00000001) << 16 |         \
	    (uint64)((EXVWINS) & 0x000003FF) << 32 | (uint64)((EXVWINE) & 0x000003FF) << 42 | \
	    (uint64)((EVSYNCMD) & 0x00001FFF) << 52)

#define GS_SET_SIGLBLID(SIGID, LBLID) \
	((uint64)((SIGID) & 0xFFFFFFFF) << 0 | (uint64)((LBLID) & 0xFFFFFFFF) << 32)

#define GS_SET_SMODE1(RC, LC, T1248, SLCK, CMOD, EX, PRST, SINT, XPCK, PCK2, SPML, GCONT, PHS, \
    PVS, PEHS, PEVS, CLKSEL, NVCK, SLCK2, VCKSEL, VHP)                                         \
	((uint64)((RC) & 0x00000007) << 0 | (uint64)((LC) & 0x0000007F) << 3 |                 \
	    (uint64)((T1248) & 0x00000003) << 10 | (uint64)((SLCK) & 0x00000001) << 12 |       \
	    (uint64)((CMOD) & 0x00000003) << 13 | (uint64)((EX) & 0x00000001) << 15 |          \
	    (uint64)((PRST) & 0x00000001) << 16 | (uint64)((SINT) & 0x00000001) << 17 |        \
	    (uint64)((XPCK) & 0x00000001) << 18 | (uint64)((PCK2) & 0x00000003) << 19 |        \
	    (uint64)((SPML) & 0x0000000F) << 21 | (uint64)((GCONT) & 0x00000001) << 25 |       \
	    (uint64)((PHS) & 0x00000001) << 26 | (uint64)((PVS) & 0x00000001) << 27 |          \
	    (uint64)((PEHS) & 0x00000001) << 28 | (uint64)((PEVS) & 0x00000001) << 29 |        \
	    (uint64)((CLKSEL) & 0x00000003) << 30 | (uint64)((NVCK) & 0x00000001) << 32 |      \
	    (uint64)((SLCK2) & 0x00000001) << 33 | (uint64)((VCKSEL) & 0x00000003) << 34 |     \
	    (uint64)((VHP) & 0x00000003) << 36)

#define GS_SET_SMODE2(INT, FFMD, DPMS)                                            \
	((uint64)((INT) & 0x00000001) << 0 | (uint64)((FFMD) & 0x00000001) << 1 | \
	    (uint64)((DPMS) & 0x00000003) << 2)

#define GS_SET_SRFSH(A) ((uint64)((A) & 0x00000000))

#define GS_SET_SYNCH1(HFP, HBP, HSEQ, HSVS, HS)                                         \
	((uint64)((HFP) & 0x000007FF) << 0 | (uint64)((HBP) & 0x000007FF) << 11 |       \
	    (uint64)((HSEQ) & 0x000003FF) << 22 | (uint64)((HSVS) & 0x000007FF) << 32 | \
	    (uint64)((HS) & 0x0000FFFF) << 43)

#define GS_SET_SYNCH2(HF, HB) ((uint64)((HF) & 0x000007FF) << 0 | (uint64)((HB) & 0x0000FFFF) << 11)

#define GS_SET_SYNCHV(VFP, VFPE, VBP, VBPE, VDP, VS)                                   \
	((uint64)((VFP) & 0x000003FF) << 0 | (uint64)((VFPE) & 0x000003FF) << 10 |     \
	    (uint64)((VBP) & 0x00000FFF) << 20 | (uint64)((VBPE) & 0x00000FFF) << 32 | \
	    (uint64)((VDP) & 0x000007FF) << 42 | (uint64)((VS) & 0x00000FFF) << 53)

#define GS_SET_DISPFB1(address, width, psm, x, y)                                     \
	(uint64)((address) & 0x000001FF) << 0 | (uint64)((width) & 0x0000003F) << 9 | \
	    (uint64)((psm) & 0x0000001F) << 15 | (uint64)((x) & 0x000007FF) << 32 |   \
	    (uint64)((y) & 0x000007FF) << 43

#define GS_SET_DISPFB2(address, width, psm, x, y)                                     \
	(uint64)((address) & 0x000001FF) << 0 | (uint64)((width) & 0x0000003F) << 9 | \
	    (uint64)((psm) & 0x0000001F) << 15 | (uint64)((x) & 0x000007FF) << 32 |   \
	    (uint64)((y) & 0x000007FF) << 43

#define GS_SET_DISPLAY1(display_x, display_y, magnify_h, magnify_v, display_w, display_h)         \
	(uint64)((display_x) & 0x00000FFF) << 0 | (uint64)((display_y) & 0x000007FF) << 12 |      \
	    (uint64)((magnify_h) & 0x0000000F) << 23 | (uint64)((magnify_v) & 0x00000003) << 27 | \
	    (uint64)((display_w) & 0x00000FFF) << 32 | (uint64)((display_h) & 0x000007FF) << 44

#define GS_SET_DISPLAY2(display_x, display_y, magnify_h, magnify_v, display_w, display_h)         \
	(uint64)((display_x) & 0x00000FFF) << 0 | (uint64)((display_y) & 0x000007FF) << 12 |      \
	    (uint64)((magnify_h) & 0x0000000F) << 23 | (uint64)((magnify_v) & 0x00000003) << 27 | \
	    (uint64)((display_w) & 0x00000FFF) << 32 | (uint64)((display_h) & 0x000007FF) << 44

#endif // GS_H_
