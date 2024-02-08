#include "mdma.h"
#include "xtc.h"
#include "lodepng.h"

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
xtcRaster*
xtcReadPNG(uint8 *data, uint32 len)
{
	xtcRaster *r;

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

	r = malloc(sizeof(*r));
	memset(r, 0, sizeof(*r));
	r->width = w;
	r->height = h;

	switch(state.info_raw.colortype) {
	case LCT_PALETTE:
		r->depth = state.info_raw.palettesize <= 16 ? 4 : 8;
		r->psm = r->depth == 4 ? SCE_GS_PSMT4 : SCE_GS_PSMT8;
		r->clutSize = (2<<r->depth)*4;
		r->clut = malloc(r->clutSize);
		copy32(r->clut, r->clutSize, state.info_raw.palette, r->clutSize, 256, 1);
		r->pixelSize = w*h*r->depth/8;
		r->pixels = malloc(r->pixelSize);
		if(state.info_raw.bitdepth == 4)
			copy4to4_swap(r->pixels, w/2, raw, w/2, w, h);
		else if(r->depth == 4)
			copy8to4(r->pixels, w/2, raw, w, w, h);
		else {
			makeCSM1((uint32*)r->clut);
			memcpy(r->pixels, raw, w*h);
		}
		break;
	case LCT_RGB:
		if(state.info_raw.bitdepth != 8)
			goto def;
		r->depth = 24;
		r->psm = SCE_GS_PSMCT24;
		r->pixelSize = w*h*3;
		r->pixels = malloc(r->pixelSize);
		memcpy(r->pixels, raw, w*h*3);
//		copy24to32(r->pixels, w*4, raw, w*3, w, h);
		break;
	default:
	def:
		// can't handle format, load as 32
		free(raw);
		lodepng_state_init(&state);
		error = lodepng_decode(&raw, &w, &h, &state, data, len);
		if(error){
			free(r);
			printf("lodepng error %s\n", lodepng_error_text(error));
			return nil;
		}
		assert(state.info_raw.bitdepth == 8);
		assert(state.info_raw.colortype == LCT_RGBA);
		// fall through
	case LCT_RGBA:
		if(state.info_raw.bitdepth != 8)
			goto def;
		r->depth = 32;
		r->psm = SCE_GS_PSMCT32;
		r->pixelSize = w*h*4;
		r->pixels = malloc(r->pixelSize);
		copy32(r->pixels, w*4, raw, w*4, w, h);
		break;
	}

	free(raw);

	xtcrRasterBuildChains(r);

	return r;
}

typedef struct PSMdesc PSMdesc;
struct PSMdesc {
	uint32 pageWidth;
	uint32 pageHeight;
	uint32 minXferWidth;
	uint32 hasAlpha;
} psmDescs[] = {
 [SCE_GS_PSMCT32]	= { 64,  32,  2, 1 },
 [SCE_GS_PSMZ32]	= { 64,  32,  2, 0 },
 [SCE_GS_PSMCT24]	= { 64,  32,  8, 0 },
 [SCE_GS_PSMZ24]	= { 64,  32,  8, 0 },
 [SCE_GS_PSMT8H]	= { 64,  32,  8, 1 },
 [SCE_GS_PSMT4HH]	= { 64,  32,  8, 1 },
 [SCE_GS_PSMT4HL]	= { 64,  32,  8, 1 },
 [SCE_GS_PSMCT16]	= { 64,  64,  4, 1 },
 [SCE_GS_PSMCT16S]	= { 64,  64,  4, 1 },
 [SCE_GS_PSMZ16]	= { 64,  64,  4, 1 },
 [SCE_GS_PSMZ16S]	= { 64,  64,  4, 1 },
 [SCE_GS_PSMT8]		= { 128, 64,  8, 1 },
 [SCE_GS_PSMT4]		= { 128, 128, 8, 1 }
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
xtcrRasterBuildChains(xtcRaster *r)
{
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
	r->maxlod = 0;

	r->texBuf.bp = 0;
	r->texBuf.bw = 1;
	r->clutBuf.bp = 0;
	r->clutBuf.bw = 1;

	PSMdesc *p = &psmDescs[r->psm];
	r->hasAlpha = p->hasAlpha;
	uint32 npgW = (r->width+p->pageWidth-1)/p->pageWidth;
	uint32 npgH = (r->height+p->pageHeight-1)/p->pageHeight;
	r->numPages = npgW*npgH;
	uint32 nblocks = r->numPages*BLK2PG;

	/*
	 * If a texture is smaller than a page either in width or height
	 * this will leave the lower right quarter of the page unused.
	 * A palette is never bigger than a quarter page so we can put it
	 * into the last 4 blocks of that page.
	 * Otherwise get a whole page.
	 * TODO: alternatively we could put multiple palettes into one page.
	 *	and have a special palette allocator.
	 */
	if(r->clut) {
		if(r->width < p->pageWidth ||
		   r->height < p->pageHeight) {
			r->clutBuf.bp = nblocks-4;
		} else {
			r->clutBuf.bp = nblocks;
			r->numPages++;
//			nblocks = r->numPages*BLK2PG;
		}
	}

	r->texBuf.bp = 0;
	r->texBuf.bw = npgW*p->pageWidth / 64;

	uint32 tw = logi(r->width);
	uint32 th = logi(r->height);
	uint32 cld = r->clut ? 1 : 0;	// load always
	r->tex0 = SCE_GS_SET_TEX0(r->texBuf.bp, r->texBuf.bw, r->psm,
		tw, th, 0, 0,
		r->clutBuf.bp, SCE_GS_PSMCT32, 0, 0, cld);

	// TODO(mipmap)
	uint32 numPkts = r->clut ? 2 : 1;

	r->pkts = malloc(numPkts*8*16);
	mdmaStart(&l, r->pkts, numPkts*8);
	uint32 w, h, sz;

	// TODO(mipmap): loop
	{
		w = r->width;
		h = r->height;
		sz = (r->pixelSize+15) / 16;

		mdmaCntDirect(&l, 5);
		mdmaAddGIFtag(&l, 3, 0, 0,0, SCE_GIF_PACKED, 1, 0xe);
		// TODO(mipmap)
		mdmaAddAD(&l, SCE_GS_TRXPOS, SCE_GS_SET_TRXPOS(0, 0, 0, 0, 0));
		// TODO(swizzle)
		mdmaAddAD(&l, SCE_GS_TRXREG, SCE_GS_SET_TRXREG(w, h));
		mdmaAddAD(&l, SCE_GS_TRXDIR, SCE_GS_SET_TRXDIR(0));
		mdmaAddGIFtag(&l, sz, 0, 0,0, SCE_GIF_IMAGE, 0, 0);

		mdmaRefDirect(&l, r->pixels, sz);

		mdmaRet(&l, 0, 0, 0);
	}

	if(r->clut) {
		if(r->depth == 4) {
			// 1 column
			w = 8;
			h = 2;
		} else {
			// 8 columns, 2 blocks
			w = 16;
			h = 16;
		}
		sz = r->clutSize / 16;

		mdmaCntDirect(&l, 5);
		mdmaAddGIFtag(&l, 3, 0, 0,0, SCE_GIF_PACKED, 1, 0xe);
		mdmaAddAD(&l, SCE_GS_TRXPOS, SCE_GS_SET_TRXPOS(0, 0, 0, 0, 0));
		mdmaAddAD(&l, SCE_GS_TRXREG, SCE_GS_SET_TRXREG(w, h));
		mdmaAddAD(&l, SCE_GS_TRXDIR, SCE_GS_SET_TRXDIR(0));
		mdmaAddGIFtag(&l, sz, 0, 0,0, SCE_GIF_IMAGE, 0, 0);

		mdmaRefDirect(&l, r->clut, sz);

		mdmaRet(&l, 0, 0, 0);
	}
}

void
xtcrUpload(xtcRaster *r)
{
	mdmaList *l = xtcState.list;

	/* poor man's texture cache */
// TODO: we don't even need this yet because
// we're only doing stupid PATH2 transfers so far
	static int pingpong;
	uint32 sz = (gsEnd - gsStart)/2;
	sz = (sz+31)&~31;
	uint32 base = gsStart + pingpong*sz;
	pingpong = !pingpong;


	r->base = base;

	uint128 *pkt = r->pkts;

	mdmaCntDirect(l, 2);
	mdmaAddGIFtag(l, 1, 0, 0,0, SCE_GIF_PACKED, 1, 0xe);
	mdmaAddAD(l, SCE_GS_BITBLTBUF, SCE_GS_SET_BITBLTBUF(0,0,0,
		r->base+r->texBuf.bp, r->texBuf.bw, r->psm));
	mdmaCall(l, 0, pkt, VIFnop, VIFnop);
	pkt += 8;

	if(r->clut) {
		mdmaCntDirect(l, 2);
		mdmaAddGIFtag(l, 1, 0, 0,0, SCE_GIF_PACKED, 1, 0xe);
		mdmaAddAD(l, SCE_GS_BITBLTBUF, SCE_GS_SET_BITBLTBUF(0,0,0,
			r->base+r->clutBuf.bp, r->clutBuf.bw, SCE_GS_PSMCT32));
		mdmaCall(l, 0, pkt, VIFnop, VIFnop);
	}

	mdmaCntDirect(l, 2);
	mdmaAddGIFtag(l, 1, 1, 0,0, SCE_GIF_PACKED, 1, 0xe);
	mdmaAddAD(l, SCE_GS_TEXFLUSH, 0);
}

void
xtcBindTexture(xtcRaster *r)
{
	if(xtcState.tex != r) {
		xtcState.tex = r;
		if(r)
			xtcrUpload(r);
	}
}

void
xtcTexFunc(xtcTCC tcc, xtcTFX tfx)
{
	tcc &= 1;
	tfx &= 3;
	xtcState.tex0 = SCE_GS_SET_TEX0(0, 0, 0, 0, 0, tcc, tfx, 0, 0, 0, 0, 0);
}

void
xtcTexFilter(xtcFilter min, xtcFilter mag)
{
	min &= 7;
	mag &= 1;
	xtcState.tex1 &= ~(0xFUL<<5);
	xtcState.tex1 |= SCE_GS_SET_TEX1(0, 0, mag, min, 0, 0, 0);
}

void
xtcTexWrap(xtcWrap u, xtcWrap v)
{
	// TODO: support REGION_ modes?
	u &= 3;
	v &= 3;
	mdmaGSregs.c1.clamp = SCE_GS_SET_CLAMP(u, v, 0, 0, 0, 0);
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
