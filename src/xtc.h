#ifndef XTC_H_
#define XTC_H_

#include "mdma.h"
#include "types.h"

#define ENUM(name)              \
	typedef enum name name; \
	enum name

#define STRUCT(name) \
typedef struct name name; \
struct name


STRUCT(xtcrBuffer) {
	uint16 bp;	// block address, relative to raster base
	uint16 bw;	// pixel width/64
};

// or maybe xtcTexture?
// TODO: use smaller types
STRUCT(xtcRaster) {
	int32 width;
	int32 height;
	int32 depth;
	uint32 clutSize;
	uint8 *clut;
	uint32 pixelSize;
	uint8 *pixels;

	uint32 psm;
	uint32 numPages;
	xtcrBuffer clutBuf;
	xtcrBuffer texBuf;
	uint32 maxlod;
	uint32 hasAlpha;

	uint64 tex0;
	uint32 base;
	uint128 *pkts;
};

xtcRaster *xtcReadPNG(uint8 *data, uint32 len);
void xtcrRasterBuildChains(xtcRaster *r);
void xtcrUpload(xtcRaster *r);
void xtcBindTexture(xtcRaster *r);

ENUM(xtcTCC) {
	XTC_RGB,
	XTC_RGBA
};

ENUM(xtcTFX) {
	XTC_MODULATE,
	XTC_DECAL,
	XTC_HIGHLIGHT,
	XTC_HIGHLIGHT2
};

ENUM(xtcFilter) {
	XTC_NEAREST,
	XTC_LINEAR,
	XTC_NEAREST_MIP_NEAREST,
	XTC_NEAREST_MIP_LINEAR,
	XTC_LINEAR_MIP_NEAREST,
	XTC_LINEAR_MIP_LINEAR
};

ENUM(xtcWrap) {
	XTC_REPEAT,
	XTC_CLAMP
};

void xtcTexFunc(xtcTCC tcc, xtcTFX tfx);
void xtcTexFilter(xtcFilter min, xtcFilter mag);
void xtcTexWrap(xtcWrap u, xtcWrap v);
void xtcTexLodMode(int lcm, int k, int l);

void xtcColorScale(float r, float g, float b, float a);
void xtcColorScaleTex(float r, float g, float b, float a);


ENUM(xtcPrimType) {
	XTC_POINTS,
	XTC_LINELIST,
	XTC_LINESTRIP,
	XTC_TRILIST,
	XTC_TRISTRIP,
	XTC_NUM_PRIMTYPES
};

/*
	position
		3 float
		3 i16
	   ADC/fog(?):
		4 float
		4 i16
	color		[n]?
		4 u8
		1 u16
	texcoord	[n]?
		2 float
		2 i16
		4 float
		4 i16
	normal
		3 float
		3 u8	-> 4 u8 when alignment needed
	weights&indices
		4 floats+offset (like RW)
*/

/* TODO: this is awfully temporary, just a sketch
 * maybe we should get rid of it altogether in favour of batch descs.
 * there is also a conceptual difference of what we put in the VIF packet
 * and what microcode expects to me in memory */
enum {
	POS_3F		= 0x00000001,
	POS_3S		= 0x00000002,
	POS_4F		= 0x00000004,
	POS_4S		= 0x00000008,

	TEX_2F		= 0x00000010,
	TEX_2S		= 0x00000020,
	TEX_4F		= 0x00000040,
	TEX_4S		= 0x00000080,

	COL_4B		= 0x00001000,
	COL_5551	= 0x00002000,	// not with ref

	NORMAL_3F	= 0x00100000,
	NORMAL_3B	= 0x00200000,	// not with ref, -> pad to 4b?

	SKINDATA_4F	= 0x01000000
};



enum xtcpUsage {
	XTCP_UNUSED,
	XTCP_POSITION,
	XTCP_TEXCOORD,
	XTCP_COLOR,
	XTCP_NORMAL,
	XTCP_SKINDATA,
};

STRUCT(xtcpVertAttrib) {
	uint32 usage;
	uint32 offset;
	uint32 unpack;
};

STRUCT(xtcpBatchDesc) {
	uint32 stride;
	int numAttribs;
	xtcpVertAttrib attribs[10];
};

void xtcpMakeBatchDesc(uint32 vertFmt, xtcpBatchDesc *desc);



/*
 * Batch vertex count is calculated in a somewhat complicated way.
 * We want the max vertex count to be a multiple of 4 for code optimization purposes.
 * vertCount is the maximum possible vertex count.
 * We may want to ref-UNPACK some data in which case the UNPACK
 * data has to be qword-sized and the starting address qword-aligned.
 * We assume that each vertex is at least 4b so that a multiple of 4
 * is always a valid UNPACK count.
 *
 * For lists the vertex count is a multiple of 4 and the prim count.
 * The following calculation ensures this:
 * numVerts = ((vertCount/primsz) & ~3)*primsz
 *
 * For ref'ed strips we have to continue the strip with each
 * batch by repeating the last vertices,
 * and the beginning of each batch has to be qword-aligned.
 * This means the vertex count *excluding the repeated vertices*
 * has to be a multiple of 4.
 * Because we may need a multiple of 4 for the UNPACK count we may
 * have to round up, for which we need space too.
 * In addition the code expects vertices to come in a multiple of 4
 * so need space for the actual vertex count rounded up to a multiple of 4 as well.
 * The following calculation ensures this:
 * numVerts = (((vertCount & ~3)-repeat) & ~3)+repeat
 *
 * For non-ref'ed strips (the only kind we have right now)
 * we only need the vertex count to be a multiple of 4:
 * numVerts = vertCount & ~3
 */

STRUCT(xtcMicrocodeSwitch) {
        uint32 process, buf1, buf2, buf3;
};

STRUCT(xtcMicrocode) {
	void *code;
	uint32 vertexTop;
	uint32 vertCount;
	uint32 numAttribs;
	uint32 offset;
	uint32 vertFmt;
	uint32 numVerts[XTC_NUM_PRIMTYPES];
	// pipeline code will know what to do with this (for now)
	xtcMicrocodeSwitch swtch[0];
};

STRUCT(xtcBatchInfo) {
	uint32 numBatches;
	uint32 batchSize;
	uint32 lastBatchSize;
	int32 repeat;
};

int xtcpGetBatchInfo(xtcMicrocode *code, xtcBatchInfo *bi, xtcPrimType type, int32 numVerts);
void xtcpRefVertices(uint128 *verts, int32 numVerts, xtcBatchInfo *bi, uint32 stride);

void xtcpCombineMatrix(void);
void xtcpUploadLights(void);



STRUCT(xtcImState) {
	float xyzw[4];
	float stq[4];
	uint32 rgba[4];
	int32 normal[4];

	xtcMicrocode *code;
	xtcpBatchDesc *desc;
	uint128 *vertstash;
	void *vertptr;
	int numVerts;
	void **nextptr;
	xtcPrimType primtype;
	int restartstrip;
};
extern xtcImState imstate;

void xtcpKickVertex(uint32 vertFmt);



STRUCT(xtcPipeline) {
	void **(*upload)(xtcPipeline *pipe, xtcPrimType primtype);
	xtcMicrocode *code;

	xtcpBatchDesc desc;
};

extern xtcPipeline *twodPipeline;
extern xtcPipeline *nolightPipeline;
extern xtcPipeline *defaultPipeline;

void xtcSetPipeline(xtcPipeline *pipe);

void xtcBegin(xtcPrimType prim);
void xtcEnd(void);
void xtcRestartStrip(void);
void xtcVertex(float x, float y, float z);
void xtcTexCoord(float s, float t, float q);
void xtcColor(uint32 r, uint32 g, uint32 b, uint32 a);
void xtcNormal(float x, float y, float z);


STRUCT(xtcPrimList) {
	xtcPipeline *pipe;
	xtcPrimType primtype;
	uint32 size;
	void *list;
};
xtcPrimList *xtcCreatePrimList(void);
void xtcStartList(xtcPrimList *pl);
void xtcEndList(void);
void xtcPrimListDraw(xtcPrimList *pl);


STRUCT(xtcVec3) { float x, y, z; };
STRUCT(xtcVec4) { float x, y, z, w; };
STRUCT(xtcRGBA) { float r, g, b, a; };

// TODO: this should probably be pipe-specific
STRUCT(xtcMaterial) {
	xtcRGBA color;
	float ambient;
	float diffuse;
	float specular;
	float shininess;
};
void xtcSetMaterial(xtcMaterial *mat);


ENUM(xtcLightType) {
	XTC_LIGHT_DIRECT,
	XTC_LIGHT_POINT,	// not yet
	XTC_LIGHT_SPOT		// not yet
};

STRUCT(xtcLight) {
	int enabled;
	xtcLightType type;
	xtcRGBA color;
//	xtcRGBA specColor;	// maybe later?
	xtcVec3 direction;
	xtcVec3 position;
};
void xtcSetAmbient(int r, int g, int b);
void xtcSetLight(int n, xtcLight *light);
        

void xtcSetProjectionMatrix(const float *mat);
void xtcSetViewMatrix(const float *mat);
void xtcSetWorldMatrix(const float *mat);
        
        
void xtcViewport(int x, int y, int width, int height);
void xtcDepthRange(int near, int far);
void xtcScissor(int x, int y, int width, int height);
void xtcClearColor(int r, int g, int b, int a);
void xtcClearDepth(uint32 z);

enum {
	XTC_COLORBUF = 1,
	XTC_DEPTHBUF = 2,
};

void xtcClear(int mask);
void xtcSetDraw(mdmaDrawBuffer *draw);

ENUM(xtceState) {
	XTC_DEPTH_TEST,
	XTC_ALPHA_TEST,
	XTC_BLEND,
	XTC_FOG,
	XTC_TEXTURE,
	XTC_CLIPPING		// may want to be more fine grained eventually
};

void xtcEnable(xtceState state);
void xtcDisable(xtceState state);

ENUM(xtceDepthFunc) {
	XTC_DEPTH_NEVER,
	XTC_DEPTH_ALWAYS,
	XTC_DEPTH_GEQUAL,
	XTC_DEPTH_GREATER
};

void xtcDepthFunc(xtceDepthFunc func);

ENUM(xtceAlphaFunc) {
	XTC_AFUNC_NEVER,
	XTC_AFUNC_ALWAYS,
	XTC_AFUNC_LESS,
	XTC_AFUNC_LEQUAL,
	XTC_AFUNC_EQUAL,
	XTC_AFUNC_GEQUAL,
	XTC_AFUNC_GREATER,
	XTC_AFUNC_NOTEQUAL
};

ENUM(xtceAlphaFail) {
	XTC_AFAIL_KEEP,
	XTC_AFAIL_FB_ONLY,
	XTC_AFAIL_ZB_ONLY,
	XTC_AFAIL_RGB_ONLY
};

void xtcAlphaFunc(xtceAlphaFunc func, int ref, xtceAlphaFail fail);

ENUM(xtceAlpha) {
	XTC_ALPHA_SRC,
	XTC_ALPHA_DST,
	XTC_ALPHA_ZERO,
	XTC_ALPHA_FIX = XTC_ALPHA_ZERO
};

void xtcBlendFunc(xtceAlpha a, xtceAlpha b, xtceAlpha c, xtceAlpha d, int fix);

ENUM(xtcBlendFactor) {
	XTC_BLEND_ZERO,
	XTC_BLEND_ONE,
	XTC_BLEND_SRCALPHA,
	XTC_BLEND_INVSRCALPHA,
	XTC_BLEND_DSTALPHA,
	XTC_BLEND_INVDSTALPHA,
};

void xtcBlendFuncSrcDst(xtcBlendFactor src, xtcBlendFactor dst);
void xtcFog(float start, float end, uint32 col);

ENUM(xtceShadeModel) {
	XTC_FLAT,
	XTC_SMOOTH,
};

void xtcShadeModel(xtceShadeModel model);

void xtcPixelMask(uint32 mask);
void xtcDepthMask(int mask);
void xtcSetList(mdmaList *list);
void xtcInit(int width, int height, int depth);




/*
 * sort of internal
 */

struct xtcState
{
	uint32 width, height;

	uint32 clearcol;
	uint32 cleardepth;

	int zte;
	int ztst;

	uint32 nearScreen, farScreen;
	float near, far;
	float fogstart, fogend;

	float proj[16];
	float view[16];
	float world[16];

	int clipping;

	mdmaList *list;
	xtcPipeline *pipe;

	int tme;
	xtcRaster *tex;
	// these have only the stuff that's raster-independent
	uint64 tex0;
	uint64 tex1;

	union {
		float matrix_f[16];
		struct {
			uint128 matrix0;
			uint128 matrix1;
			uint128 matrix2;
			uint128 matrix3;
		};
	};
	uint128 xyzwScale;
	uint128 xyzwOffset;
	uint128 clipConsts;
	uint128 colorScale;
	uint128 colorScaleTex;

	float *pColorScale;
	float *pColorScaleTex;

	xtcMaterial material;

	xtcRGBA ambient;
	xtcLight lights[8];
};
extern struct xtcState xtcState;

#endif // XTC_H_
