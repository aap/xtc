#include "xtci.h"
#include "lodepng.h"
void lodepng_free(void* ptr);

#include <libgraph.h>

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

static void
copy32(uint8 *dst, uint32 dststride, uint8 *src, uint32 srcstride, int32 w, int32 h)
{
	for(int32 y = 0; y < h; y++) {
		uint8 *d = &dst[y*dststride];
		uint8 *s = &src[y*srcstride];
		for(int32 x = 0; x < w; x++) {
			*d++ = *s++;
			*d++ = *s++;
			*d++ = *s++;
			*d++ = *s++ * 128.0f/255.0f;
		}
	}
}

// unused
static void
copy24to32(uint8 *dst, uint32 dststride, uint8 *src, uint32 srcstride, int32 w, int32 h)
{
	for(int32 y = 0; y < h; y++) {
		uint8 *d = &dst[y*dststride];
		uint8 *s = &src[y*srcstride];
		for(int32 x = 0; x < w; x++) {
			*d++ = *s++;
			*d++ = *s++;
			*d++ = *s++;
			*d++ = 128;
		}
	}
}

static void
copy8to4(uint8 *dst, uint32 dststride, uint8 *src, uint32 srcstride, int32 w, int32 h)
{
	uint8 c1, c2;
	for(int32 y = 0; y < h; y++)
		for(int32 x = 0; x < w/2; x++) {
			c1 = src[y*srcstride + 2*x + 0];
			c2 = src[y*srcstride + 2*x + 1];
			dst[y*dststride + x] = (c2<<4) | c1;
		}
}

static void
copy4to4_swap(uint8 *dst, uint32 dststride, uint8 *src, uint32 srcstride, int32 w, int32 h)
{
	uint8 c;
	for(int32 y = 0; y < h; y++)
		for(int32 x = 0; x < w/2; x++) {
			c = src[y*srcstride + x];
			c = (c>>4) | (c<<4);
			dst[y*dststride + x] = c;
		}
}

void static
makeCSM1(uint32 *clut)
{
	for(uint32 i = 0; i < 256; i++) {
		if((i & 0x18) != 0x08) continue;
		uint32 j = i ^ 0x18;
		uint32 t = clut[i];
		clut[i] = clut[j];
		clut[j] = t;
	}
}

// TODO:
// account for minimum size
// check for power of 2?
xtcTexture*
xtcTextureReadPNG(const uint8 *data, uint32 len)
{
	xtcTexture *tex;

	LodePNGState state;
	lodepng_state_init(&state);

	state.decoder.color_convert = 0;
	uint8 *raw = nil;
	uint32 w, h;
	uint32 error = lodepng_decode(&raw, &w, &h, &state, data, len);
	if(error) {
		printf("lodepng error %s\n", lodepng_error_text(error));
		return nil;
	}

	tex = (xtcTexture*)mdmaMalloc(sizeof(*tex));
	memset(tex, 0, sizeof(*tex));
	tex->width = w;
	tex->height = h;

	switch(state.info_raw.colortype) {
	case LCT_PALETTE:
		tex->depth = state.info_raw.palettesize <= 16 ? 4 : 8;
		tex->psm = tex->depth == 4 ? SCE_GS_PSMT4 : SCE_GS_PSMT8;
		tex->clutSize = (1<<tex->depth)*4;
		tex->clut = (uint8*)mdmaMalloc(tex->clutSize);
		copy32(tex->clut, tex->clutSize, state.info_raw.palette, tex->clutSize, 1<<tex->depth, 1);
		tex->pixelSize = w*h*tex->depth/8;
		tex->pixels = (uint8*)mdmaMalloc(tex->pixelSize);
		if(state.info_raw.bitdepth == 4)
			copy4to4_swap(tex->pixels, w/2, raw, w/2, w, h);
		else if(tex->depth == 4)
			copy8to4(tex->pixels, w/2, raw, w, w, h);
		else {
			makeCSM1((uint32*)tex->clut);
			memcpy(tex->pixels, raw, w*h);
		}
		break;
	case LCT_RGB:
		if(state.info_raw.bitdepth != 8)
			goto def;
		tex->depth = 24;
		tex->psm = SCE_GS_PSMCT24;
		tex->pixelSize = w*h*3;
		tex->pixels = (uint8*)mdmaMalloc(tex->pixelSize);
		memcpy(tex->pixels, raw, w*h*3);
//		copy24to32(tex->pixels, w*4, raw, w*3, w, h);
		break;
	default:
	def:
		// can't handle format, load as 32
		lodepng_free(raw);
		lodepng_state_init(&state);
		error = lodepng_decode(&raw, &w, &h, &state, data, len);
		if(error){
			mdmaFree(tex);
			printf("lodepng error %s\n", lodepng_error_text(error));
			return nil;
		}
		assert(state.info_raw.bitdepth == 8);
		assert(state.info_raw.colortype == LCT_RGBA);
		// fall through
	case LCT_RGBA:
		if(state.info_raw.bitdepth != 8)
			goto def;
		tex->depth = 32;
		tex->psm = SCE_GS_PSMCT32;
		tex->pixelSize = w*h*4;
		tex->pixels = (uint8*)mdmaMalloc(tex->pixelSize);
		copy32(tex->pixels, w*4, raw, w*4, w, h);
		break;
	}

	lodepng_free(raw);

	xtctTexBuildChains(tex);

	return tex;
}

typedef struct PSMdesc PSMdesc;
// indexed by PSM. Ordered rather than designated: no C99 designated
// initialisers in C++, and the holes are codes the GS does not define.
struct PSMdesc {
	uint32 pageWidth;
	uint32 pageHeight;
	uint32 minXferWidth;
	uint32 hasAlpha;
} psmDescs[] = {
	{ 64,  32,  2, 1 },			// 0  PSMCT32
	{ 64,  32,  8, 0 },			// 1  PSMCT24
	{ 64,  64,  4, 1 },			// 2  PSMCT16
	{0},{0},{0},{0},{0},{0},{0},		// 3-9
	{ 64,  64,  4, 1 },			// 10 PSMCT16S
	{0},{0},{0},{0},{0},{0},{0},{0},	// 11-18
	{ 128, 64,  8, 1 },			// 19 PSMT8
	{ 128, 128, 8, 1 },			// 20 PSMT4
	{0},{0},{0},{0},{0},{0},		// 21-26
	{ 64,  32,  8, 1 },			// 27 PSMT8H
	{0},{0},{0},{0},{0},{0},{0},{0},	// 28-35
	{ 64,  32,  8, 1 },			// 36 PSMT4HL
	{0},{0},{0},{0},{0},{0},{0},		// 37-43
	{ 64,  32,  8, 1 },			// 44 PSMT4HH
	{0},{0},{0},				// 45-47
	{ 64,  32,  2, 0 },			// 48 PSMZ32
	{ 64,  32,  8, 0 },			// 49 PSMZ24
	{ 64,  64,  4, 1 },			// 50 PSMZ16
	{0},{0},{0},{0},{0},{0},{0},		// 51-57
	{ 64,  64,  4, 1 }			// 58 PSMZ16S
};
enum {
	BLK2PG = 32,
	WD2BLK = 64,
	WD2PG = 2048
};

uint32
logi(uint32 sz)
{
	uint32 l = 0;
	while(sz >>= 1) l++;
	return l;
}

void
xtctTexBuildChains(xtcTexture *tex)
{
	mdmaArena arena;
	mdmaList l;
	/*
	   pkt:
		DMAcnt
		 GIFtag A+D
		  TRXPOS
		  TRXREG
		  TRXDIR
		 GIFtag IMAGE
		DMAref pixels/palette
		DMAret

	   upload:
		DMAcnt
		 GIFtag A+D
		  BITBLTBUF
		DMAcall pkt
		--------
		DMAcnt
		 GIFtag A+D EOP
		  TEXFLUSH
	 */

	// TODO(mipmap)
	tex->maxlod = 0;

	tex->texBuf.bp = 0;
	tex->texBuf.bw = 1;
	tex->clutBuf.bp = 0;
	tex->clutBuf.bw = 1;

	PSMdesc *p = &psmDescs[tex->psm];
	tex->hasAlpha = p->hasAlpha;
	uint32 npgW = (tex->width+p->pageWidth-1)/p->pageWidth;
	uint32 npgH = (tex->height+p->pageHeight-1)/p->pageHeight;
	tex->numPages = npgW*npgH;
	uint32 nblocks = tex->numPages*BLK2PG;

	/*
	 * If a texture is smaller than a page either in width or height
	 * this will leave the lower right quarter of the page unused.
	 * A palette is never bigger than a quarter page so we can put it
	 * into the last 4 blocks of that page.
	 * Otherwise get a whole page.
	 * TODO: alternatively we could put multiple palettes into one page.
	 *	and have a special palette allocator.
	 */
	if(tex->clut) {
		if(tex->width < p->pageWidth ||
		   tex->height < p->pageHeight) {
			tex->clutBuf.bp = nblocks-4;
		} else {
			tex->clutBuf.bp = nblocks;
			tex->numPages++;
//			nblocks = tex->numPages*BLK2PG;
		}
	}

	tex->texBuf.bp = 0;
	tex->texBuf.bw = npgW*p->pageWidth / 64;

	uint32 tw = logi(tex->width);
	uint32 th = logi(tex->height);
	uint32 cld = tex->clut ? 1 : 0;	// load always
	tex->tex0 = SCE_GS_SET_TEX0(tex->texBuf.bp, tex->texBuf.bw, tex->psm,
		tw, th, 0, 0,
		tex->clutBuf.bp, SCE_GS_PSMCT32, 0, 0, cld);

	// TODO(mipmap)
	uint32 numPkts = tex->clut ? 2 : 1;

	tex->pkts = (uint128*)mdmaMalloc(numPkts*8*16);
	mdmaArenaInit(&arena, tex->pkts, numPkts*8, 1, MDMA_MEM_CACHED);
	mdmaListInit(&l, &arena);
	uint32 w, h, sz;

	// TODO(mipmap): loop
	{
		w = tex->width;
		h = tex->height;
		sz = (tex->pixelSize+15) / 16;

		mdmaCnt(&l, 5);
			mdmaBeginDirect(&l, 5, 0);
				mdmaBeginGifTag(&l, 3, 0, 0,0, GIF_PACKED, 1, GIF_AD);
				// TODO(mipmap)
				mdmaAddAD(&l, SCE_GS_TRXPOS, SCE_GS_SET_TRXPOS(0, 0, 0, 0, 0));
				// TODO(swizzle)
				mdmaAddAD(&l, SCE_GS_TRXREG, SCE_GS_SET_TRXREG(w, h));
				mdmaAddAD(&l, SCE_GS_TRXDIR, SCE_GS_SET_TRXDIR(0));
				mdmaEndGifTag(&l);
				// the image itself arrives through the ref below
				mdmaGifTag(&l, sz, 0, 0,0, GIF_IMAGE, 0, 0);
			mdmaEndDirect(&l);
		mdmaCloseTag(&l);

		mdmaRef(&l, tex->pixels, sz);
			mdmaVifDirect(&l, sz, 0);

		mdmaRet(&l, 0);
		mdmaCloseTag(&l);
	}

	if(tex->clut) {
		if(tex->depth == 4) {
			// 1 column
			w = 8;
			h = 2;
		} else {
			// 8 columns, 2 blocks
			w = 16;
			h = 16;
		}
		sz = tex->clutSize / 16;

		mdmaCnt(&l, 5);
			mdmaBeginDirect(&l, 5, 0);
				mdmaBeginGifTag(&l, 3, 0, 0,0, GIF_PACKED, 1, GIF_AD);
				mdmaAddAD(&l, SCE_GS_TRXPOS, SCE_GS_SET_TRXPOS(0, 0, 0, 0, 0));
				mdmaAddAD(&l, SCE_GS_TRXREG, SCE_GS_SET_TRXREG(w, h));
				mdmaAddAD(&l, SCE_GS_TRXDIR, SCE_GS_SET_TRXDIR(0));
				mdmaEndGifTag(&l);
				mdmaGifTag(&l, sz, 0, 0,0, GIF_IMAGE, 0, 0);
			mdmaEndDirect(&l);
		mdmaCloseTag(&l);

		mdmaRef(&l, tex->clut, sz);
			mdmaVifDirect(&l, sz, 0);

		mdmaRet(&l, 0);
		mdmaCloseTag(&l);
	}
}

void
xtctUpload(xtcTexture *tex)
{
	mdmaList *l = xtcState.list;

	/* poor man's texture cache */
// TODO: we don't even need this yet because
// we're only doing stupid PATH2 transfers so far
	static int pingpong;
	uint32 sz = (xtcgMemEnd - xtcgMemStart)/2;
	sz = (sz+31)&~31;
	uint32 base = xtcgMemStart + pingpong*sz;
	pingpong = !pingpong;


	tex->base = base;

	uint128 *pkt = tex->pkts;

	mdmaCall(l, pkt, 2);
		mdmaBeginDirect(l, 2, 0);
			mdmaBeginGifTag(l, 1, 0, 0,0, GIF_PACKED, 1, GIF_AD);
			mdmaAddAD(l, SCE_GS_BITBLTBUF, SCE_GS_SET_BITBLTBUF(0,0,0,
				tex->base+tex->texBuf.bp, tex->texBuf.bw, tex->psm));
			mdmaEndGifTag(l);
		mdmaEndDirect(l);
	mdmaCloseTag(l);
	pkt += 8;

	if(tex->clut) {
		mdmaCall(l, pkt, 2);
			mdmaBeginDirect(l, 2, 0);
				mdmaBeginGifTag(l, 1, 0, 0,0, GIF_PACKED, 1, GIF_AD);
				mdmaAddAD(l, SCE_GS_BITBLTBUF, SCE_GS_SET_BITBLTBUF(0,0,0,
					tex->base+tex->clutBuf.bp, tex->clutBuf.bw, SCE_GS_PSMCT32));
				mdmaEndGifTag(l);
			mdmaEndDirect(l);
		mdmaCloseTag(l);
	}

	mdmaCnt(l, 2);
		mdmaBeginDirect(l, 2, 0);
			mdmaBeginGifTagAD(l, 1);
				mdmaAddAD(l, SCE_GS_TEXFLUSH, 0);
			mdmaEndGifTag(l);
		mdmaEndDirect(l);
	mdmaCloseTag(l);
}

void
xtcSetTexture(xtcTexture *tex)
{
	if(xtcState.tex != tex) {
		xtcState.tex = tex;
		if(tex)
			xtctUpload(tex);
	}
}

void
xtcTexFunc(xtcTCC tcc, xtcTFX tfx)
{
	uint32 c = tcc & 1;
	uint32 f = tfx & 3;
	xtcState.tex0 = SCE_GS_SET_TEX0(0, 0, 0, 0, 0, c, f, 0, 0, 0, 0, 0);
}

void
xtcTexFilter(xtcFilter min, xtcFilter mag)
{
	uint32 mn = min & 7;
	uint32 mg = mag & 1;
	xtcState.tex1 &= ~(0xFUL<<5);
	xtcState.tex1 |= SCE_GS_SET_TEX1(0, 0, mg, mn, 0, 0, 0);
}

void
xtcTexWrap(xtcWrap u, xtcWrap v)
{
	// TODO: support REGION_ modes?
	uint32 uu = u & 3;
	uint32 vv = v & 3;
	xtcgRegs.c1.clamp = SCE_GS_SET_CLAMP(uu, vv, 0, 0, 0, 0);
}

void
xtcTexLodMode(int lcm, int k, int l)
{
	lcm &= 1;
	k &= 0xFFF;
	l &= 3;
	xtcState.tex1 &= 0xF<<5;
	xtcState.tex1 |= SCE_GS_SET_TEX1(lcm, 0, 0, 0, 0, l, k);
}
