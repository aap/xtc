#include "xtci.h"
#include "joy.h"
#include "fio.h"
#include "scenes.h"
#include "xmodel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <libgraph.h>
#include <sifdev.h>

/*
 * Textures
 */

uint8 tex32[] = {
#include "texture_test/tex32.inc"
};

uint8 tex24[] = {
#include "texture_test/tex24.inc"
};

uint8 tex8[] = {
#include "texture_test/tex8.inc"
};

uint8 tex4[] = {
#include "texture_test/tex4.inc"
};

#define SIZED(array) array, sizeof(array)

xtcTexture *raster32;
xtcTexture *raster24;
xtcTexture *raster8;
xtcTexture *raster4;

/* the file scene's texture, loaded over hostfs at init */
static xtcTexture *fileRaster;

/* the skeleton's orbit camera; the town scene file may preset it */
extern float camDist, camTheta, camPhi;

static void
loadFileTexture(void)
{
	int fd, size, n;
	uint8 *data;

	fd = fioOpen("host:./test.png", SCE_RDONLY);
	if(fd < 0) {
		printf("fio: can't open host:./test.png -> %d\n", fd);
		return;
	}
	size = fioLseek(fd, 0, SCE_SEEK_END);
	fioLseek(fd, 0, SCE_SEEK_SET);
	data = (uint8*)mdmaMalloc(size);
	n = fioRead(fd, data, size);
	fioClose(fd);
	printf("fio: read %d/%d bytes of host:./test.png\n", n, size);
	if(n == size)
		fileRaster = xtcTextureReadPNG(data, size);
}

void
scenesInit(void)
{
	raster32 = xtcTextureReadPNG(SIZED(tex32));
	raster24 = xtcTextureReadPNG(SIZED(tex24));
	raster8  = xtcTextureReadPNG(SIZED(tex8));
	raster4  = xtcTextureReadPNG(SIZED(tex4));

	loadFileTexture();
}

/*
 * Helpers
 */

static void
setWorld(float x, float y, float z, float s)
{
	Mat4 world = m4mul(m4translate(x, y, z), m4scale(s, s, s));
	xtcSetWorldMatrix(&world);
}

void
rotateWorld(void)
{
	static float t = 0.0f;
	float speed = 1.0f;
	Mat4 world = m4rotZ(t*speed);
	xtcSetWorldMatrix(&world);

	t += 0.01f;
}

void
moveInCircle(float r)
{
	static float t = 0.0f;
	float speed = 1.0f;
	Mat4 world = m4translate(r*cosf(t*speed), r*sinf(t*speed), 0.0f);
	xtcSetWorldMatrix(&world);

	t += 0.01f;
}

void
drawAxes(void)
{
	xtcSetPipeline(nolightPipeline);

	xtcBegin(XTC_LINELIST);
		xtcColor(255, 0, 0, 255);
		xtcVertex(0.0f, 0.0f, 0.0f);
		xtcVertex(3.0f, 0.0f, 0.0f);

		xtcColor(0, 255, 0, 255);
		xtcVertex(0.0f, 0.0f, 0.0f);
		xtcVertex(0.0f, 3.0f, 0.0f);

		xtcColor(0, 0, 255, 255);
		xtcVertex(0.0f, 0.0f, 0.0f);
		xtcVertex(0.0f, 0.0f, 3.0f);
	xtcEnd();
}

/* 2d coordinates: y in [-1,1] up, x in the same units (square aspect),
 * so the screen runs from -width/height to +width/height in x */

static float
aspect2d(void)
{
	return (float)xtcState.height/(float)xtcState.width;
}

static void
rect2d(float x, float y, float w, float h)
{
	float a = aspect2d();
	float x0 = (x - 0.5f*w)*a;
	float x1 = (x + 0.5f*w)*a;
	float y0 = y - 0.5f*h;
	float y1 = y + 0.5f*h;

	xtcBegin(XTC_TRISTRIP);
		xtcVertex(x0, y0, 0.0f);
		xtcVertex(x1, y0, 0.0f);
		xtcVertex(x0, y1, 0.0f);
		xtcVertex(x1, y1, 0.0f);
	xtcEnd();
}

static void
texRect2d(float x, float y, float w, float h)
{
	float a = aspect2d();
	float x0 = (x - 0.5f*w)*a;
	float x1 = (x + 0.5f*w)*a;
	float y0 = y - 0.5f*h;
	float y1 = y + 0.5f*h;

	xtcBegin(XTC_TRISTRIP);
		xtcTexCoord3(0.0f, 1.0f, 1.0f);
		xtcVertex(x0, y0, 0.0f);
		xtcTexCoord3(1.0f, 1.0f, 1.0f);
		xtcVertex(x1, y0, 0.0f);
		xtcTexCoord3(0.0f, 0.0f, 1.0f);
		xtcVertex(x0, y1, 0.0f);
		xtcTexCoord3(1.0f, 0.0f, 1.0f);
		xtcVertex(x1, y1, 0.0f);
	xtcEnd();
}

/*
 * Scene: prims — every prim type through the nolight pipeline,
 * one station per type along the x axis, all with the clip microcode.
 */

void
drawCube(void)
{
	xtcSetPipeline(nolightPipeline);

	static struct {
		float x, y, z;
		unsigned char r, g, b, a;
	} verts[] = {
		{ -1.0f, -1.0f, -1.0f,            0,   0,   0, 255 },
		{ -1.0f, -1.0f,  1.0f,            0,   0, 255, 255 },
		{ -1.0f,  1.0f, -1.0f,            0, 255,   0, 255 },
		{ -1.0f,  1.0f,  1.0f,            0, 255, 255, 255 },
		{  1.0f, -1.0f, -1.0f,          255,   0,   0, 255 },
		{  1.0f, -1.0f,  1.0f,          255,   0, 255, 255 },
		{  1.0f,  1.0f, -1.0f,          255, 255,   0, 255 },
		{  1.0f,  1.0f,  1.0f,          255, 255, 255, 255 },
	};
	static int indices[] = {
		0, 1, 2,
		2, 1, 3,

		4, 6, 5,
		5, 6, 7,

		5, 7, 1,
		1, 7, 3,

		0, 2, 4,
		4, 2, 6,

		7, 6, 3,
		3, 6, 2,

		1, 0, 5,
		5, 0, 4
	};
	static struct {
		float s, t;
	} st[] = {
		{ 0.0f, 0.0f },
		{ 1.0f, 0.0f },
		{ 0.0f, 1.0f },

		{ 0.0f, 1.0f },
		{ 1.0f, 0.0f },
		{ 1.0f, 1.0f }
	};

	xtcBegin(XTC_TRILIST);
		for(uint32 i = 0; i < nelem(indices); i++) {
			int idx = indices[i];
			xtcTexCoord3(st[i%6].s, st[i%6].t, 1.0f);
			xtcColor(verts[idx].r, verts[idx].g, verts[idx].b, verts[idx].a);
			xtcVertex(verts[idx].x, verts[idx].y, verts[idx].z);
		}
	xtcEnd();
}

static void
scenePrims(void)
{
	int i, j, k;

	xtcEnable(XTC_CLIPPING);
	drawAxes();
	xtcSetPipeline(nolightPipeline);

	/* points: a little color cube */
	setWorld(-4.0f, 0.0f, 0.0f, 1.0f);
	xtcBegin(XTC_POINTS);
		for(i = 0; i < 5; i++)
		for(j = 0; j < 5; j++)
		for(k = 0; k < 5; k++) {
			xtcColor(i*63, j*63, k*63, 255);
			xtcVertex((i-2)*0.3f, (j-2)*0.3f, (k-2)*0.3f);
		}
	xtcEnd();

	/* line list: spokes */
	setWorld(-2.0f, 0.0f, 0.0f, 1.0f);
	xtcBegin(XTC_LINELIST);
		for(i = 0; i < 12; i++) {
			float phi = TAU*(float)i/12;
			xtcColor(255, i*21, 0, 255);
			xtcVertex(0.0f, 0.0f, 0.0f);
			xtcVertex(0.8f*cosf(phi), 0.0f, 0.8f*sinf(phi));
		}
	xtcEnd();

	/* line strip: a helix */
	setWorld(0.0f, 0.0f, 0.0f, 1.0f);
	xtcBegin(XTC_LINESTRIP);
		for(i = 0; i <= 48; i++) {
			float t = (float)i/48;
			float phi = 3.0f*TAU*t;
			xtcColor(255*t, 128, 255*(1.0f-t), 255);
			xtcVertex(0.5f*cosf(phi), 0.5f*sinf(phi), 1.6f*t - 0.8f);
		}
	xtcEnd();

	/* tri list: the cube */
	setWorld(2.0f, 0.0f, 0.0f, 0.6f);
	drawCube();

	/* tri strip: a twisting ribbon */
	setWorld(4.0f, 0.0f, 0.0f, 1.0f);
	xtcBegin(XTC_TRISTRIP);
		for(i = 0; i <= 16; i++) {
			float t = (float)i/16;
			float phi = PI*t;
			float c = 0.4f*cosf(phi);
			float s = 0.4f*sinf(phi);
			xtcColor(255, 160, 0, 255);
			xtcVertex(1.6f*t - 0.8f, c, s);
			xtcColor(0, 160, 255, 255);
			xtcVertex(1.6f*t - 0.8f, -c, -s);
		}
	xtcEnd();
}

/*
 * Scene: primlist — the recorded tristrip sphere with strip restarts.
 */

static xtcPrimList *sphere;

void
drawSphere(void)
{
	if(sphere) {
		xtcPrimListDraw(sphere);
		return;
	}

	sphere = xtcCreatePrimList();
	xtcStartList(sphere);

	xtcSetPipeline(nolightPipeline);

	xtcBegin(XTC_TRISTRIP);

	const int nh = 40;
	const int nv = 20;
	for(int i = 0; i < nv; i++) {
		float theta1 = PI*(float)i/nv;
		float cth1 = cosf(theta1);
		float sth1 = sinf(theta1);
		float theta2 = PI*(float)(i+1)/nv;
		float cth2 = cosf(theta2);
		float sth2 = sinf(theta2);

		if(i != 0) xtcRestartStrip();

		for(int j = 0; j < nh; j++) {
			float phi = TAU*(float)j/(nh-1);

			float x = cosf(phi);
			float y = sinf(phi);

			float x1 = sth1*x;
			float y1 = sth1*y;
			float z1 = cth1;
			xtcColor(255*0.5f*(1.0f+x1), 255*0.5f*(1.0f+y1), 255*0.5f*(1.0f+z1), 255);
			xtcVertex(x1, y1, z1);

			float x2 = sth2*x;
			float y2 = sth2*y;
			float z2 = cth2;
			xtcColor(255*0.5f*(1.0f+x2), 255*0.5f*(1.0f+y2), 255*0.5f*(1.0f+z2), 255);
			xtcVertex(x2, y2, z2);
		}
	}

	xtcEnd();

	xtcEndList();
}

/*
 * PrimList files.  The chain xtcpBuildList records is position-
 * independent -- cnt tags with the data inline and a ret at the end,
 * no addresses anywhere -- so a file is a small header plus the raw
 * chain bytes.  This is the seed of the offline format: what the cross
 * button saves here is exactly what a PC tool would generate.
 */

#define XPL_IDENT 0x304C5058	/* "XPL0" */

STRUCT(PrimListHeader) {
	uint32 ident;
	uint32 pipe;		/* index into plPipes */
	uint32 primtype;
	uint32 size;		/* chain bytes following */
};

static xtcPipeline **plPipes[] = { &twodPipeline, &nolightPipeline, &defaultPipeline };

static void
savePrimList(xtcPrimList *pl, const char *path)
{
	PrimListHeader h;
	int fd, n;
	uint32 i;

	h.ident = XPL_IDENT;
	h.pipe = nelem(plPipes);
	for(i = 0; i < nelem(plPipes); i++)
		if(pl->pipe == *plPipes[i])
			h.pipe = i;
	if(h.pipe == nelem(plPipes)) {
		printf("xpl: unknown pipeline, not saving\n");
		return;
	}
	h.primtype = pl->primtype;
	h.size = pl->size;

	fd = fioOpen(path, SCE_WRONLY|SCE_CREAT|SCE_TRUNC);
	if(fd < 0) {
		printf("xpl: can't write %s -> %d\n", path, fd);
		return;
	}
	n = fioWrite(fd, &h, sizeof(h));
	n += fioWrite(fd, pl->list, pl->size);
	fioClose(fd);
	printf("xpl: wrote %d/%d bytes to %s\n", n, (int)(sizeof(h)+pl->size), path);
}

static xtcPrimList*
loadPrimList(const char *path)
{
	PrimListHeader h;
	xtcPrimList *pl;
	int fd, n;

	fd = fioOpen(path, SCE_RDONLY);
	if(fd < 0) {
		printf("xpl: can't open %s -> %d\n", path, fd);
		return nil;
	}
	n = fioRead(fd, &h, sizeof(h));
	if(n != sizeof(h) || h.ident != XPL_IDENT || h.pipe >= nelem(plPipes)) {
		printf("xpl: %s is not a primlist file\n", path);
		fioClose(fd);
		return nil;
	}
	pl = xtcCreatePrimList();
	pl->pipe = *plPipes[h.pipe];
	pl->primtype = (xtcPrimType)h.primtype;
	pl->size = h.size;
	pl->list = mdmaMalloc(h.size);
	n = fioRead(fd, pl->list, h.size);
	fioClose(fd);
	printf("xpl: read %d/%d chain bytes from %s\n", n, h.size, path);
	if(n != (int)h.size)
		return nil;
	return pl;
}

static void
scenePrimList(void)
{
	xtcEnable(XTC_CLIPPING);
	drawAxes();
	rotateWorld();
	drawSphere();

	/* record the sphere to a file for the plfile scene */
	if(joy.press & JOY_CROSS)
		savePrimList(sphere, "host:./sphere.xpl");
}

/*
 * Scene: plfile -- prim lists back from files, possibly made offline
 * (tools/xpl.py).  assets.txt next to the ELF names them, one path per
 * line; without it the scene falls back to sphere.xpl, the file the
 * primlist scene's cross button records.  Dpad left/right cycles,
 * cross drops the cache and loads the current one again.
 */

#define MAXASSETS 64

static char assetName[MAXASSETS][64];
static xtcPrimList *assetPl[MAXASSETS];
static int numAssets;
static int curAsset;
static int assetsInited;

static void
loadAssetList(void)
{
	char buf[2048], *p, *e;
	int fd, n, len;

	numAssets = 0;
	fd = fioOpen("host:./assets.txt", SCE_RDONLY);
	if(fd < 0) {
		strcpy(assetName[numAssets++], "sphere.xpl");
		return;
	}
	n = fioRead(fd, buf, sizeof(buf)-1);
	fioClose(fd);
	if(n < 0) n = 0;
	buf[n] = 0;
	for(p = buf; *p && numAssets < MAXASSETS; p = e) {
		e = p;
		while(*e && *e != '\n') e++;
		len = e-p;
		if(*e) e++;
		if(len > 0 && len < (int)sizeof(assetName[0]) && *p != '#') {
			memcpy(assetName[numAssets], p, len);
			assetName[numAssets][len] = 0;
			numAssets++;
		}
	}
	printf("xpl: %d assets listed\n", numAssets);
}

static void
scenePlFile(void)
{
	char path[80];
	int load = 0;

	if(!assetsInited) {
		assetsInited = 1;
		loadAssetList();
		load = 1;
	}
	if(joy.press & JOY_RIGHT) { curAsset = (curAsset+1) % numAssets; load = 1; }
	if(joy.press & JOY_LEFT)  { curAsset = (curAsset+numAssets-1) % numAssets; load = 1; }
	if(joy.press & JOY_CROSS) { assetPl[curAsset] = nil; load = 1; }
	if(load && numAssets) {
		printf("asset %d/%d: %s\n", curAsset+1, numAssets, assetName[curAsset]);
		if(assetPl[curAsset] == nil) {
			strcpy(path, "host:./");
			strcat(path, assetName[curAsset]);
			assetPl[curAsset] = loadPrimList(path);
		}
	}

	xtcEnable(XTC_CLIPPING);
	drawAxes();
	rotateWorld();
	if(numAssets && assetPl[curAsset])
		xtcPrimListDraw(assetPl[curAsset]);
}

/*
 * Scene: town -- prim list instances placed by a textual scene
 * description, host:./town.scene, written by a separate layout program
 * (tools/townplan.py).  This little format is the seed of the
 * serialized scene structure:
 *
 *   # comment
 *   cam <dist>                                  camera distance hint
 *   inst <file> <x> <y> <z> <rotz deg> <scale>
 *
 * Cross reloads the file for layout iteration; loaded prim lists stay
 * cached (a reload leaks the instance list's assets only if their
 * files changed names -- fine for a test scene).
 */

#define MAXTOWNASSETS 32
#define MAXINSTS 256

STRUCT(TownInst) { int asset; float x, y, z, rotz, scale; };

static char townName[MAXTOWNASSETS][64];
static xtcPrimList *townPl[MAXTOWNASSETS];
static int numTownAssets;
static TownInst townInsts[MAXINSTS];
static int numInsts;
static int townLoaded;

static int
townAsset(const char *name)
{
	char path[80];
	int i;

	for(i = 0; i < numTownAssets; i++)
		if(strcmp(townName[i], name) == 0)
			return i;
	if(numTownAssets >= MAXTOWNASSETS || strlen(name) >= sizeof(townName[0]))
		return -1;
	i = numTownAssets++;
	strcpy(townName[i], name);
	strcpy(path, "host:./");
	strcat(path, name);
	townPl[i] = loadPrimList(path);
	return i;
}

static char*
townTok(char **pp, char *out, int n)
{
	char *p = *pp;
	int i = 0;

	while(*p == ' ' || *p == '\t') p++;
	while(*p && *p != ' ' && *p != '\t' && i < n-1) out[i++] = *p++;
	out[i] = 0;
	*pp = p;
	return i ? out : nil;
}

static void
loadTown(void)
{
	static char buf[16384];
	char name[64], cmd[8];
	char *p, *e, *q;
	int fd, n;

	numInsts = 0;
	fd = fioOpen("host:./town.scene", SCE_RDONLY);
	if(fd < 0) {
		printf("town: can't open town.scene -> %d\n", fd);
		return;
	}
	n = fioRead(fd, buf, sizeof(buf)-1);
	fioClose(fd);
	if(n < 0) n = 0;
	buf[n] = 0;

	for(p = buf; *p; p = e) {
		e = p;
		while(*e && *e != '\n') e++;
		if(*e) *e++ = 0;

		q = p;
		if(townTok(&q, cmd, sizeof(cmd)) == nil || cmd[0] == '#')
			continue;
		if(strcmp(cmd, "cam") == 0) {
			camDist = strtod(q, &q);
			if(*q) camTheta = strtod(q, &q);
			if(*q) camPhi = strtod(q, &q);
		} else if(strcmp(cmd, "inst") == 0 && numInsts < MAXINSTS) {
			TownInst *in = &townInsts[numInsts];
			if(townTok(&q, name, sizeof(name)) == nil)
				continue;
			in->asset = townAsset(name);
			in->x = strtod(q, &q);
			in->y = strtod(q, &q);
			in->z = strtod(q, &q);
			in->rotz = strtod(q, &q) * (PI/180.0f);
			in->scale = strtod(q, &q);
			if(in->scale == 0.0f) in->scale = 1.0f;
			if(in->asset >= 0)
				numInsts++;
		}
	}
	printf("town: %d instances of %d assets\n", numInsts, numTownAssets);
}

static void
sceneTown(void)
{
	int i;

	if(joy.press & JOY_CROSS)
		townLoaded = 0;
	if(!townLoaded) {
		townLoaded = 1;
		loadTown();
	}

	xtcEnable(XTC_CLIPPING);
	for(i = 0; i < numInsts; i++) {
		TownInst *in = &townInsts[i];
		xtcPrimList *pl = townPl[in->asset];
		if(pl == nil)
			continue;
		float k = in->scale;
		Mat4 world = m4mul(m4translate(in->x, in->y, in->z),
			m4mul(m4rotZ(in->rotz), m4scale(k, k, k)));
		xtcSetWorldMatrix(&world);
		xtcPrimListDraw(pl);
	}
}

/*
 * Scene: texture — one quad per raster format: 32, 24, 8 and 4 bit.
 */

static void
texQuad(xtcTexture *r, float x, float z)
{
	xtcSetTexture(r);
	xtcBegin(XTC_TRISTRIP);
		xtcColor(255, 255, 255, 255);
		xtcTexCoord3(0.0f, 1.0f, 1.0f);
		xtcVertex(x-0.9f, 0.0f, z-0.9f);
		xtcTexCoord3(1.0f, 1.0f, 1.0f);
		xtcVertex(x+0.9f, 0.0f, z-0.9f);
		xtcTexCoord3(0.0f, 0.0f, 1.0f);
		xtcVertex(x-0.9f, 0.0f, z+0.9f);
		xtcTexCoord3(1.0f, 0.0f, 1.0f);
		xtcVertex(x+0.9f, 0.0f, z+0.9f);
	xtcEnd();
}

static void
sceneTexture(void)
{
	xtcEnable(XTC_CLIPPING);
	drawAxes();
	xtcSetPipeline(nolightPipeline);
	xtcEnable(XTC_TEXTURE);

	texQuad(raster32, -1.0f,  1.0f);
	texQuad(raster24,  1.0f,  1.0f);
	texQuad(raster8,  -1.0f, -1.0f);
	texQuad(raster4,   1.0f, -1.0f);
}

/*
 * Scene: lit — the teapot under two directional lights, one circling.
 */

#include "teapot.inc"

/* teapot data */

int patchdata[][16] = {
	{102,103,104,105,4,5,6,7,8,9,10,11,12,13,14,15},               /* rim */
	{12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27},             /* body */
	{24,25,26,27,29,30,31,32,33,34,35,36,37,38,39,40},             /* body */
	{96,96,96,96,97,98,99,100,101,101,101,101,0,1,2,3,},           /* lid */
	{0,1,2,3,106,107,108,109,110,111,112,113,114,115,116,117},     /* lid */
	{118,118,118,118,124,122,119,121,123,126,125,120,40,39,38,37}, /* bottom */
	{41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56},             /* handle */
	{53,54,55,56,57,58,59,60,61,62,63,64,28,65,66,67},             /* handle */
	{68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83},             /* spout */
	{80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95}              /* spout */
};

float cpdata[][3] = {
	{0.2,0,2.7},{0.2,-0.112,2.7},{0.112,-0.2,2.7},{0,-0.2,2.7},{1.3375,0,2.53125},
	{1.3375,-0.749,2.53125},{0.749,-1.3375,2.53125},{0,-1.3375,2.53125},
	{1.4375,0,2.53125},{1.4375,-0.805,2.53125},{0.805,-1.4375,2.53125},
	{0,-1.4375,2.53125},{1.5,0,2.4},{1.5,-0.84,2.4},{0.84,-1.5,2.4},{0,-1.5,2.4},
	{1.75,0,1.875},{1.75,-0.98,1.875},{0.98,-1.75,1.875},{0,-1.75,1.875},{2,0,1.35},
	{2,-1.12,1.35},{1.12,-2,1.35},{0,-2,1.35},{2,0,0.9},{2,-1.12,0.9},{1.12,-2,0.9},
	{0,-2,0.9},{-2,0,0.9},{2,0,0.45},{2,-1.12,0.45},{1.12,-2,0.45},{0,-2,0.45},
	{1.5,0,0.225},{1.5,-0.84,0.225},{0.84,-1.5,0.225},{0,-1.5,0.225},{1.5,0,0.15},
	{1.5,-0.84,0.15},{0.84,-1.5,0.15},{0,-1.5,0.15},{-1.6,0,2.025},{-1.6,-0.3,2.025},
	{-1.5,-0.3,2.25},{-1.5,0,2.25},{-2.3,0,2.025},{-2.3,-0.3,2.025},{-2.5,-0.3,2.25},
	{-2.5,0,2.25},{-2.7,0,2.025},{-2.7,-0.3,2.025},{-3,-0.3,2.25},{-3,0,2.25},
	{-2.7,0,1.8},{-2.7,-0.3,1.8},{-3,-0.3,1.8},{-3,0,1.8},{-2.7,0,1.575},
	{-2.7,-0.3,1.575},{-3,-0.3,1.35},{-3,0,1.35},{-2.5,0,1.125},{-2.5,-0.3,1.125},
	{-2.65,-0.3,0.9375},{-2.65,0,0.9375},{-2,-0.3,0.9},{-1.9,-0.3,0.6},{-1.9,0,0.6},
	{1.7,0,1.425},{1.7,-0.66,1.425},{1.7,-0.66,0.6},{1.7,0,0.6},{2.6,0,1.425},
	{2.6,-0.66,1.425},{3.1,-0.66,0.825},{3.1,0,0.825},{2.3,0,2.1},{2.3,-0.25,2.1},
	{2.4,-0.25,2.025},{2.4,0,2.025},{2.7,0,2.4},{2.7,-0.25,2.4},{3.3,-0.25,2.4},
	{3.3,0,2.4},{2.8,0,2.475},{2.8,-0.25,2.475},{3.525,-0.25,2.49375},
	{3.525,0,2.49375},{2.9,0,2.475},{2.9,-0.15,2.475},{3.45,-0.15,2.5125},
	{3.45,0,2.5125},{2.8,0,2.4},{2.8,-0.15,2.4},{3.2,-0.15,2.4},{3.2,0,2.4},
	{0,0,3.15},{0.8,0,3.15},{0.8,-0.45,3.15},{0.45,-0.8,3.15},{0,-0.8,3.15},
	{0,0,2.85},{1.4,0,2.4},{1.4,-0.784,2.4},{0.784,-1.4,2.4},{0,-1.4,2.4},
	{0.4,0,2.55},{0.4,-0.224,2.55},{0.224,-0.4,2.55},{0,-0.4,2.55},{1.3,0,2.55},
	{1.3,-0.728,2.55},{0.728,-1.3,2.55},{0,-1.3,2.55},{1.3,0,2.4},{1.3,-0.728,2.4},
	{0.728,-1.3,2.4},{0,-1.3,2.4},{0,0,0},{1.425,-0.798,0},{1.5,0,0.075},{1.425,0,0},
	{0.798,-1.425,0},{0,-1.5,0.075},{0,-1.425,0},{1.5,-0.84,0.075},{0.84,-1.5,0.075}
};

void
eval(float *out, float (*cvs)[3], float u, float v)
{
	out[0] = 0.0f;
	out[1] = 0.0f;
	out[2] = 0.0f;

	float us[4], vs[4];
	float iu = 1.0f-u;
	float iv = 1.0f-v;
	us[0] = iu*iu*iu;
	us[1] = 3.0f*u*iu*iu;
	us[2] = 3.0f*u*u*iu;
	us[3] = u*u*u;
	vs[0] = iv*iv*iv;
	vs[1] = 3.0f*v*iv*iv;
	vs[2] = 3.0f*v*v*iv;
	vs[3] = v*v*v;

	for(int i = 0; i < 4; i++)
		for(int j = 0; j < 4; j++) {
			int ix = i*4+j;
			float f = us[j]*vs[i];
			out[0] += cvs[ix][0]*f;
			out[1] += cvs[ix][1]*f;
			out[2] += cvs[ix][2]*f;
		}
}

void
drawPatch(float (*cvs)[3])
{
	float p1[3];
	float p2[3];
	float p3[3];
	float p4[3];

	xtcColor(128, 128, 128, 255);
	xtcBegin(XTC_TRILIST);

	for(int i = 0; i < 13; i++) {
		float v1 = (float)i/13;
		float v2 = (float)(i+1)/13;
		for(int j = 0; j < 13; j++) {
			float u1 = (float)j/13;
			float u2 = (float)(j+1)/13;

			eval(p1, cvs, u1, v1);
			eval(p2, cvs, u2, v1);
			eval(p3, cvs, u1, v2);
			eval(p4, cvs, u2, v2);

			xtcVertex(p1[0], p1[1], p1[2]);
			xtcVertex(p2[0], p2[1], p2[2]);
			xtcVertex(p4[0], p4[1], p4[2]);

			xtcVertex(p1[0], p1[1], p1[2]);
			xtcVertex(p3[0], p3[1], p3[2]);
			xtcVertex(p4[0], p4[1], p4[2]);
		}
	}

	xtcEnd();
}

/* y-up OBJ data (teapot.inc, monkey.inc) into our z-up world, scaled by s.
 * colors may be nil, then the vertices are black (unlit for RW) */
void
drawObj(float (*verts)[3], float (*tex)[2], float (*normals)[3], uint8 (*colors)[4],
	int (*faces)[3][3], int nfaces, float s)
{
	xtcColor(0, 0, 0, 255);

	xtcBegin(XTC_TRILIST);
		for(int i = 0; i < nfaces; i++) {
			for(int v = 0; v < 3; v++) {
				int vx = faces[i][v][0]-1;
				int vt = faces[i][v][1]-1;
				int vn = faces[i][v][2]-1;
				if(colors)
					xtcColor(colors[vx][0], colors[vx][1], colors[vx][2], colors[vx][3]);
				xtcNormal(normals[vn][0], -normals[vn][2], normals[vn][1]);
				xtcVertex(s*verts[vx][0], -s*verts[vx][2], s*verts[vx][1]);
			}
		}
	xtcEnd();
}

void
drawTeapot(void)
{
	xtcSetPipeline(defaultPipeline);

	drawObj(teapot_verts, teapot_tex, teapot_normals, nil, teapot_faces, nelem(teapot_faces), 0.1f);
}

static void
setLights(void)
{
	static float t = 0.0f;
	xtcLight l;

	xtcSetAmbient(32/255.0f, 32/255.0f, 32/255.0f);

	l.enabled = 1;
	l.type = XTC_LIGHT_DIRECT;
	l.color = vec4(140/255.0f, 140/255.0f, 130/255.0f, 1.0f);
	l.direction = v3normalized(vec3(-cosf(t), -sinf(t), -0.7f));
	xtcSetLight(0, &l);

	l.color = vec4(0.0f, 40/255.0f, 120/255.0f, 1.0f);
	l.direction = v3normalized(vec3(1.0f, -1.0f, 1.0f));
	xtcSetLight(1, &l);

	t += 0.01f;
}

static void
sceneLit(void)
{
	xtcEnable(XTC_CLIPPING);
	drawAxes();
	setLights();
	drawTeapot();
}

/*
 * Scene: lights -- RW lighting on the monkey: global ambient and up to
 * 8 directional lights, the same set as src_gl/lights.fnl so the two
 * backends can be compared.  The monkey is recorded into a prim list on
 * first use; the lights stay in world space while it turns.
 *
 *   dpad left/right  select a light (its stub blinks white)
 *   cross            toggle the selected light
 *   dpad up/down     light intensity
 *   triangle         toggle the ambient
 *   square           toggle the spin
 *   circle           toggle the vertex colours (monkey.inc carries a
 *                    position gradient, see tools/obj2inc.py)
 */

#include "monkey.inc"

#define NLIGHTS 8

static float lightColors[NLIGHTS][3] = {
	{ 1.0f, 1.0f, 1.0f }, { 1.0f, 0.2f, 0.2f }, { 0.2f, 1.0f, 0.2f }, { 0.3f, 0.4f, 1.0f },
	{ 1.0f, 1.0f, 0.2f }, { 1.0f, 0.2f, 1.0f }, { 0.2f, 1.0f, 1.0f }, { 1.0f, 0.6f, 0.2f }
};
static int lightOn[NLIGHTS] = { 1, 1, 1, 1, 1, 1, 1, 1 };
static int curLight;
static int ambientOn = 1;
static int monkeySpin = 1;
static float lightIntensity = 0.6f;
static float monkeyAngle;
static int useVertexColors = 1;
static xtcPrimList *monkey, *monkeyPlain;

/* where light i shines from: round the circle, alternately above and below */
static Vec3
lightFrom(int i)
{
	float a = TAU*i/NLIGHTS;
	return v3normalized(vec3(cosf(a), sinf(a), (i & 1) ? -0.5f : 0.8f));
}

static void
lightsControls(void)
{
	if(joy.press & JOY_RIGHT) curLight = (curLight+1) % NLIGHTS;
	if(joy.press & JOY_LEFT) curLight = (curLight+NLIGHTS-1) % NLIGHTS;
	if(joy.press & JOY_CROSS) lightOn[curLight] = !lightOn[curLight];
	if(joy.press & JOY_UP) lightIntensity += 0.1f;
	if(joy.press & JOY_DOWN) lightIntensity -= 0.1f;
	if(lightIntensity < 0.0f) lightIntensity = 0.0f;
	if(lightIntensity > 1.0f) lightIntensity = 1.0f;
	if(joy.press & JOY_TRIANGLE) ambientOn = !ambientOn;
	if(joy.press & JOY_SQUARE) monkeySpin = !monkeySpin;
	if(joy.press & JOY_CIRCLE) useVertexColors = !useVertexColors;
	if(joy.press & (JOY_RIGHT|JOY_LEFT|JOY_CROSS|JOY_UP|JOY_DOWN|JOY_TRIANGLE|JOY_CIRCLE))
		printf("light %d: %s, intensity %.1f, ambient %s, vertex colors %s\n", curLight,
			lightOn[curLight] ? "on" : "off", lightIntensity,
			ambientOn ? "on" : "off", useVertexColors ? "on" : "off");
}

static void
setDemoLights(void)
{
	xtcLight l;
	Vec3 from;
	int i;

	if(ambientOn)
		xtcSetAmbient(30/255.0f, 30/255.0f, 30/255.0f);
//		xtcSetAmbient(150, 30, 30);
	else
		xtcSetAmbient(0.0f, 0.0f, 0.0f);

	memset(&l, 0, sizeof(l));
	l.type = XTC_LIGHT_DIRECT;
	for(i = 0; i < NLIGHTS; i++) {
		l.enabled = lightOn[i];
		l.color = vec4(lightIntensity*lightColors[i][0],
			lightIntensity*lightColors[i][1],
			lightIntensity*lightColors[i][2], 1.0f);
		from = lightFrom(i);
		l.direction = v3neg(from);
		xtcSetLight(i, &l);
	}
}

/* a stub pointing at each light in its colour, dim when the light is
 * off, the selected one blinking white */
static void
drawLightStubs(void)
{
	static int frame;
	Vec3 from;
	int i, r, g, b;

	frame++;
	xtcSetPipeline(nolightPipeline);
	xtcBegin(XTC_LINELIST);
	for(i = 0; i < NLIGHTS; i++) {
		r = 255*lightColors[i][0];
		g = 255*lightColors[i][1];
		b = 255*lightColors[i][2];
		if(!lightOn[i]) { r /= 4; g /= 4; b /= 4; }
		if(i == curLight && (frame & 16)) r = g = b = 255;
		from = lightFrom(i);
		xtcColor(r, g, b, 255);
		xtcVertex(1.3f*from.x, 1.3f*from.y, 1.3f*from.z);
		xtcVertex(1.9f*from.x, 1.9f*from.y, 1.9f*from.z);
	}
	xtcEnd();
}

static void
drawMonkey(void)
{
	xtcRwMaterial m;

	/* two recordings, with and without the vertex colours */
	if(monkey == nil) {
		monkey = xtcCreatePrimList();
		xtcStartList(monkey);
//		xtcSetPipeline(defaultPipeline);
		xtcSetPipeline(stdPipeline);	// no explicit material for this!
		drawObj(monkey_verts, monkey_tex, monkey_normals, monkey_colors,
			monkey_faces, nelem(monkey_faces), 0.75f);
		xtcEndList();

		monkeyPlain = xtcCreatePrimList();
		xtcStartList(monkeyPlain);
		xtcSetPipeline(stdPipeline);
		drawObj(monkey_verts, monkey_tex, monkey_normals, nil,
			monkey_faces, nelem(monkey_faces), 0.75f);
		xtcEndList();
	}

	/* plain white RW material: the lights are the whole look */
	m.color = vec4(1.0f, 1.0f, 1.0f, 1.0f);
	m.ambient = 1.0f;
	m.diffuse = 1.0f;
	m.specular = 0.0f;
	m.shininess = 0.0f;
	xtcSetRwMaterial(&m);
	xtcPrimListDraw(useVertexColors ? monkey : monkeyPlain);
}

static void
sceneLights(void)
{
	Mat4 world;

	lightsControls();
	xtcEnable(XTC_CLIPPING);
	drawAxes();
	drawLightStubs();
	setDemoLights();

	/* the monkey turns about z and is tipped a little, the lights stay */
	if(monkeySpin)
		monkeyAngle += 0.4f/60.0f;
	world = m4mul(m4rotZ(monkeyAngle), m4rotX(0.4f));
	xtcSetWorldMatrix(&world);
	drawMonkey();
}

/*
 * Scene: dsm -- prim lists assembled offline.  src/data/monkey_std.dsm
 * (inline batches, the shape the runtime records) on the left and
 * monkey_std_ref.dsm (a DMAref chain into contiguous attribute arrays)
 * on the right, both written by tools/primdsm.py from monkey_col.obj,
 * assembled by ee-dvp-as and linked into the ELF.  Same lights and
 * controls as the lights scene.
 */

extern uint128 monkey_std[];
extern uint128 monkey_std_ref[];

static void
sceneDsm(void)
{
	static Vec4 black = { 0.0f, 0.0f, 0.0f, 1.0f };
	static Vec4 white = { 1.0f, 1.0f, 1.0f, 1.0f };
	xtcStdMaterial mat = {
		{ 0.0f, 0.0f, 0.0f, 1.0f },	// emissive (the upload scales it to 0..255 now, white would be white)
		{ 1.0f, 1.0f, 1.0f, 1.0f },	// ambient
		{ 1.0f, 1.0f, 1.0f, 1.0f },	// diffuse
		{ 0.0f, 0.0f, 0.0f, 10.0f },	// specular
	};

	static xtcPrimList inl, ref;
	Mat4 world;

	if(inl.list == nil) {
		inl.pipe = stdPipeline;
		inl.primtype = XTC_TRILIST;
		inl.list = monkey_std;
		ref.pipe = stdPipeline;
		ref.primtype = XTC_TRILIST;
		ref.list = monkey_std_ref;
	}

	lightsControls();
	xtcEnable(XTC_CLIPPING);
	drawAxes();
	drawLightStubs();
	setDemoLights();

xtcSetColorMaterial(0);
//mat.ambient = black;
mat.diffuse = black;
xtcSetStdMaterial(&mat);
//xtcSetColorMaterial(XTC_AMBIENT);
xtcSetColorMaterial(XTC_EMISSIVE);
xtcSetColorMaterial(XTC_AMBIENT | XTC_DIFFUSE);
//xtcSetColorMaterial(XTC_DIFFUSE);

	if(monkeySpin)
		monkeyAngle += 0.4f/60.0f;
	world = m4mul(m4rotZ(monkeyAngle), m4rotX(0.4f));

	world.w.x = -1.3f;
	xtcSetWorldMatrix(&world);
//	xtcPrimListDraw(&inl);

	world.w.x = 1.3f;
	xtcSetWorldMatrix(&world);
	xtcPrimListDraw(&ref);
}

/*
 * Scene: skin -- the monkey through the skin pipeline.  Five attributes
 * per vertex; the skin data is a dummy (bone 0, weight 1) and the
 * microcode only compacts it away for now, so this should look exactly
 * like the lights scene with vertex colours on.  Same controls.
 */

static xtcPrimList *skinMonkey;

// left: nothing but the vertex colours (checks positions and colours)
static const xtcStdMaterial colorsOnly = {
	{ 0.0f, 0.0f, 0.0f, 1.0f },	// emissive
	{ 0.0f, 0.0f, 0.0f, 1.0f },	// ambient
	{ 0.0f, 0.0f, 0.0f, 1.0f },	// diffuse
	{ 0.0f, 0.0f, 0.0f, 0.0f },	// specular
};
// right: white, lit (checks the normals)
static const xtcStdMaterial lit = {
	{ 0.0f, 0.0f, 0.0f, 1.0f },	// emissive
	{ 1.0f, 1.0f, 1.0f, 1.0f },	// ambient
	{ 1.0f, 1.0f, 1.0f, 1.0f },	// diffuse
	{ 0.0f, 0.0f, 0.0f, 0.0f },	// specular
};

static void
sceneSkin(void)
{
	Mat4 world;

	if(skinMonkey == nil) {
		skinMonkey = xtcCreatePrimList();
		xtcStartList(skinMonkey);
		xtcSetPipeline(skinPipeline);
		xtcIndices(0, 0, 0, 0);
		xtcWeights(1.0f, 0.0f, 0.0f, 0.0f);
		drawObj(monkey_verts, monkey_tex, monkey_normals, monkey_colors,
			monkey_faces, nelem(monkey_faces), 0.75f);
		xtcEndList();
	}

	lightsControls();
	xtcEnable(XTC_CLIPPING);
	drawAxes();
	drawLightStubs();
	setDemoLights();

	if(monkeySpin)
		monkeyAngle += 0.4f/60.0f;
	world = m4mul(m4rotZ(monkeyAngle), m4rotX(0.4f));

	world.w.x = -1.3f;
	xtcSetWorldMatrix(&world);
	xtcSetStdMaterial(&colorsOnly);
	xtcSetColorMaterial(XTC_EMISSIVE);
	xtcPrimListDraw(skinMonkey);

	world.w.x = 1.3f;
	xtcSetWorldMatrix(&world);
	xtcSetStdMaterial(&lit);
	xtcSetColorMaterial(0);
	xtcPrimListDraw(skinMonkey);
}

/*
 * Scene: tube -- the skinning test subject.  A tube along z with
 * TUBE_BONES bones spaced along it; every vertex is weighted between the
 * two bones it sits between and coloured like its weights.  The joints
 * wave, so a working skin pipe bends the tube into a snake; until then
 * it stays straight.  src_gl/skin.fnl draws the same tube on GL, that
 * is the picture to aim for.  Left: vertex colours only, right: lit.
 * Same controls as the lights scene; square stops the waving.
 */

#define TUBE_BONES 6
#define TUBE_RINGS 5		/* rings per bone segment */
#define TUBE_SIDES 16
#define TUBE_LEN 3.0f
#define TUBE_RADIUS 0.4f
#define TUBE_SPACING (TUBE_LEN/(TUBE_BONES-1))

static xtcPrimList *tube;
static float tubeTime;

static const uint8 tubeColors[TUBE_BONES][3] = {
	{ 255, 60, 60 }, { 255, 200, 40 }, { 60, 220, 60 },
	{ 60, 200, 255 }, { 90, 90, 255 }, { 240, 80, 240 }
};

static void
tubeVertex(int ring, int side)
{
	float t = (float)ring/TUBE_RINGS;	/* position in bone units */
	int b = (int)t;
	if(b > TUBE_BONES-2) b = TUBE_BONES-2;
	t -= b;					/* 0..1 from bone b to b+1 */
	float z = -TUBE_LEN/2 + (b + t)*TUBE_SPACING;
	float phi = TAU*side/TUBE_SIDES;
	float c = cosf(phi), s = sinf(phi);

	xtcColor((uint32)(tubeColors[b][0]*(1.0f-t) + tubeColors[b+1][0]*t),
	         (uint32)(tubeColors[b][1]*(1.0f-t) + tubeColors[b+1][1]*t),
	         (uint32)(tubeColors[b][2]*(1.0f-t) + tubeColors[b+1][2]*t), 255);
	xtcNormal(c, s, 0.0f);
	xtcTexCoord((float)side/TUBE_SIDES, t);
	xtcIndices(b, b+1, 0, 0);
	xtcWeights(1.0f-t, t, 0.0f, 0.0f);
	xtcVertex(TUBE_RADIUS*c, TUBE_RADIUS*s, z);
}

static void
buildTube(void)
{
	int nrings = (TUBE_BONES-1)*TUBE_RINGS;

	tube = xtcCreatePrimList();
	xtcStartList(tube);
	xtcSetPipeline(skinPipeline);
	xtcBegin(XTC_TRILIST);
	for(int r = 0; r < nrings; r++)
		for(int s = 0; s < TUBE_SIDES; s++) {
			int s1 = (s+1) % TUBE_SIDES;
			tubeVertex(r, s); tubeVertex(r+1, s); tubeVertex(r+1, s1);
			tubeVertex(r, s); tubeVertex(r+1, s1); tubeVertex(r, s1);
		}
	xtcEnd();
	xtcEndList();
}

/* the skinning matrices: every joint bends a little, in alternating
 * planes, and the bones chain up from the bottom.  the bind pose is
 * bone b standing at its pivot, so the skinning matrix is the animated
 * frame times the translation back from the pivot */
static void
tubeBones(float time, Mat4 *bones)
{
	Mat4 m = m4translate(0.0f, 0.0f, -TUBE_LEN/2);
	for(int b = 0; b < TUBE_BONES; b++) {
		float pivot = -TUBE_LEN/2 + b*TUBE_SPACING;
		float a = 0.3f*sinf(2.0f*time + 0.9f*b);
		if(b > 0)
			m = m4mul(m, m4translate(0.0f, 0.0f, TUBE_SPACING));
		m = m4mul(m, (b & 1) ? m4rotY(a) : m4rotX(a));
		bones[b] = m4mul(m, m4translate(0.0f, 0.0f, -pivot));
	}
}

static void
sceneTube(void)
{
	Mat4 bones[TUBE_BONES], world;

	if(tube == nil)
		buildTube();

	lightsControls();
	xtcEnable(XTC_CLIPPING);
	drawAxes();
	drawLightStubs();
	setDemoLights();

	if(monkeySpin)
		tubeTime += 1.0f/60.0f;
	tubeBones(tubeTime, bones);
	xtcSetBoneMatrices(bones, TUBE_BONES);

	world = m4translate(-1.3f, 0.0f, 0.0f);
	xtcSetWorldMatrix(&world);
	xtcSetStdMaterial(&colorsOnly);
	xtcSetColorMaterial(XTC_EMISSIVE);
	xtcPrimListDraw(tube);

	world = m4translate(1.3f, 0.0f, 0.0f);
	xtcSetWorldMatrix(&world);
	xtcSetStdMaterial(&lit);
	xtcSetColorMaterial(0);
	xtcPrimListDraw(tube);
}

/*
 * Scene: littex — lit and textured: a recorded sphere with normals
 * and texcoords through the default pipeline, texture modulated.
 */

static xtcPrimList *litSphere;

static void
drawLitSphere(void)
{
	if(litSphere) {
		xtcPrimListDraw(litSphere);
		return;
	}

	litSphere = xtcCreatePrimList();
	xtcStartList(litSphere);

	xtcSetPipeline(defaultPipeline);

	xtcBegin(XTC_TRISTRIP);

	const int nh = 32;
	const int nv = 16;
	for(int i = 0; i < nv; i++) {
		float theta1 = PI*(float)i/nv;
		float theta2 = PI*(float)(i+1)/nv;

		if(i != 0) xtcRestartStrip();

		for(int j = 0; j < nh; j++) {
			float s = (float)j/(nh-1);
			float phi = TAU*s;
			float x = cosf(phi);
			float y = sinf(phi);

			float x1 = sinf(theta1)*x;
			float y1 = sinf(theta1)*y;
			float z1 = cosf(theta1);
			xtcColor(0, 0, 0, 255);
			xtcNormal(x1, y1, z1);
			xtcTexCoord3(s, theta1/PI, 1.0f);
			xtcVertex(x1, y1, z1);

			float x2 = sinf(theta2)*x;
			float y2 = sinf(theta2)*y;
			float z2 = cosf(theta2);
			xtcColor(0, 0, 0, 255);
			xtcNormal(x2, y2, z2);
			xtcTexCoord3(s, theta2/PI, 1.0f);
			xtcVertex(x2, y2, z2);
		}
	}

	xtcEnd();

	xtcEndList();
}

static void
sceneLitTex(void)
{
	xtcEnable(XTC_CLIPPING);
	drawAxes();
	setLights();
	xtcEnable(XTC_TEXTURE);
	xtcSetTexture(raster32);
	rotateWorld();
	drawLitSphere();
}

/*
 * Scene: im2d — the original immediate 2d test, two textured tris.
 */

void
drawIm2D(void)
{
	xtcSetPipeline(twodPipeline);

	xtcDisable(XTC_BLEND);
	xtcBlendFuncSrcDst(XTC_BLEND_SRCALPHA, XTC_BLEND_INVSRCALPHA);

	xtcBegin(XTC_TRILIST);
		xtcColor(0, 128, 255, 255);
		xtcTexCoord3(0.0f, 0.0f, 1.0f);
		xtcVertex(0.0f, 0.0f, 0.0f);
		xtcTexCoord3(0.0f, 1.0f, 1.0f);
		xtcVertex(0.0f, 0.9f, 0.0f);
		xtcTexCoord3(1.0f, 1.0f, 1.0f);
		xtcVertex(0.9f, 0.9f, 0.0f);

		xtcColor(255, 128, 0, 255);
		xtcTexCoord3(0.0f, 0.0f, 1.0f);
		xtcVertex(0.0f, 0.0f, 0.0f);
		xtcTexCoord3(1.0f, 0.0f, 1.0f);
		xtcVertex(0.9f, 0.0f, 0.0f);
		xtcTexCoord3(1.0f, 1.0f, 1.0f);
		xtcVertex(0.9f, 0.9f, 0.0f);
	xtcEnd();
}

static void
sceneIm2d(void)
{
	xtcColorMod cm, saved;

	xtcDisable(XTC_DEPTH_TEST);
	xtcEnable(XTC_TEXTURE);
	xtcSetTexture(raster8);
	/* the 2d colours are in the GS's own convention */
	xtcGetColorMod(&saved);
	cm = saved;
	cm.scaleTex = vec4(1.0f, 1.0f, 1.0f, 128.0f/255.0f);
	xtcSetColorMod(&cm);
	drawIm2D();
	xtcSetColorMod(&saved);
}

/*
 * Scene: blend — alpha and additive blending in the 2d pipeline,
 * plus a textured quad blended by its own alpha channel.
 */

static void
sceneBlend(void)
{
	xtcSetPipeline(twodPipeline);
	xtcDisable(XTC_DEPTH_TEST);

	/* opaque reference bar */
	xtcColor(200, 200, 200, 255);
	rect2d(0.0f, 0.0f, 2.6f, 0.12f);

	/* alpha blending: three half-transparent quads overlapping */
	xtcEnable(XTC_BLEND);
	xtcBlendFuncSrcDst(XTC_BLEND_SRCALPHA, XTC_BLEND_INVSRCALPHA);
	xtcColor(255, 0, 0, 128);
	rect2d(-0.85f, 0.45f, 0.8f, 0.8f);
	xtcColor(0, 255, 0, 128);
	rect2d(-0.55f, 0.45f, 0.8f, 0.8f);
	xtcColor(0, 0, 255, 128);
	rect2d(-0.70f, 0.15f, 0.8f, 0.8f);

	/* additive: two dim quads overlapping */
	xtcBlendFuncSrcDst(XTC_BLEND_ONE, XTC_BLEND_ONE);
	xtcColor(90, 40, 0, 255);
	rect2d(-0.85f, -0.5f, 0.8f, 0.8f);
	xtcColor(0, 40, 90, 255);
	rect2d(-0.55f, -0.5f, 0.8f, 0.8f);

	/* the texture's own alpha */
	xtcBlendFuncSrcDst(XTC_BLEND_SRCALPHA, XTC_BLEND_INVSRCALPHA);
	xtcEnable(XTC_TEXTURE);
	xtcTexFunc(XTC_RGBA, XTC_MODULATE);
	xtcSetTexture(raster32);
	xtcColor(255, 255, 255, 255);
	texRect2d(0.7f, 0.0f, 1.1f, 1.1f);
}

/*
 * Scene: fog — a row of cubes marching off into it.
 */

static void
sceneFog(void)
{
	int i;

	xtcEnable(XTC_CLIPPING);
	xtcEnable(XTC_FOG);
	xtcFog(2.0f, 14.0f, 64 | 64<<8 | 64<<16);

	drawAxes();

	for(i = 0; i < 8; i++) {
		setWorld(i*1.8f, 0.0f, 0.0f, 0.5f);
		drawCube();
	}
}

/*
 * Scene: direct — a bouncing sprite straight through a DIRECT tag,
 * no pipeline involved.
 */

void
drawThing(void)
{
	static int x = 0;
	static int y = 0;
	static int vx = 1;
	static int vy = 1;
	static int sz = 32;

	mdmaList *l = xtcState.list;
	uint32 xoff = 2048 - xtcState.width/2;
	uint32 yoff = 2048 - xtcState.height/2;

	mdmaCnt(l, 4);
		mdmaBeginDirect(l, 4, 0);
			mdmaBeginGifTag(l, 3, 1, 1,SCE_GS_PRIM_SPRITE,
				GIF_PACKED, 1, GIF_AD);
			mdmaAddAD(l, SCE_GS_RGBAQ,
				SCE_GS_SET_RGBAQ(255, 255, 255, 255, 255));
			mdmaAddAD(l, SCE_GS_XYZ2,
				SCE_GS_SET_XYZ((xoff+x)<<4, (yoff+y)<<4, 0));
			mdmaAddAD(l, SCE_GS_XYZ2,
				SCE_GS_SET_XYZ((xoff+x+sz)<<4, (yoff+y+sz)<<4, 0));
			mdmaEndGifTag(l);
		mdmaEndDirect(l);
	mdmaCloseTag(l);

	x += vx;
	y += vy;
	if(x < 0 || x+sz >= (int)xtcState.width) vx = -vx;
	if(y < 0 || y+sz >= (int)xtcState.height) vy = -vy;
}

static void
sceneDirect(void)
{
	drawThing();
}

/*
 * Scene: file — a texture loaded from the host filesystem through
 * fio.c; a red quad means the file didn't make it.
 */

static void
sceneFile(void)
{
	xtcEnable(XTC_CLIPPING);
	drawAxes();
	xtcSetPipeline(nolightPipeline);

	if(fileRaster) {
		xtcEnable(XTC_TEXTURE);
		texQuad(fileRaster, 0.0f, 1.0f);
	} else {
		xtcBegin(XTC_TRISTRIP);
			xtcColor(255, 0, 0, 255);
			xtcVertex(-0.9f, 0.0f, 0.1f);
			xtcVertex( 0.9f, 0.0f, 0.1f);
			xtcVertex(-0.9f, 0.0f, 1.9f);
			xtcVertex( 0.9f, 0.0f, 1.9f);
		xtcEnd();
	}
}

/*
 * Scene: pad — the pad, schematically; buttons light up as pressed,
 * the stick dots follow the sticks (red when clicked in).
 */

static void
btnRect(uint16 mask, float x, float y, float w, float h, int r, int g, int b)
{
	if(joy.btns & mask)
		xtcColor(r, g, b, 255);
	else
		xtcColor(r/5, g/5, b/5, 255);
	rect2d(x, y, w, h);
}

static void
stickGate(float x, float y, float sz)
{
	float a = aspect2d();
	float h = 0.5f*sz;

	xtcColor(120, 120, 120, 255);
	xtcBegin(XTC_LINESTRIP);
		xtcVertex((x-h)*a, y-h, 0.0f);
		xtcVertex((x+h)*a, y-h, 0.0f);
		xtcVertex((x+h)*a, y+h, 0.0f);
		xtcVertex((x-h)*a, y+h, 0.0f);
		xtcVertex((x-h)*a, y-h, 0.0f);
	xtcEnd();
}

static void
scenePad(void)
{
	float dx, dy, fx, fy;

	xtcSetPipeline(twodPipeline);
	xtcDisable(XTC_DEPTH_TEST);

	/* body */
	xtcColor(40, 40, 48, 255);
	rect2d(0.0f, -0.05f, 1.9f, 0.75f);

	/* shoulders */
	btnRect(JOY_L2, -0.7f, 0.56f, 0.30f, 0.10f, 180, 180, 180);
	btnRect(JOY_L1, -0.7f, 0.44f, 0.30f, 0.10f, 230, 230, 230);
	btnRect(JOY_R2,  0.7f, 0.56f, 0.30f, 0.10f, 180, 180, 180);
	btnRect(JOY_R1,  0.7f, 0.44f, 0.30f, 0.10f, 230, 230, 230);

	/* dpad */
	dx = -0.62f;
	dy = 0.05f;
	xtcColor(25, 25, 30, 255);
	rect2d(dx, dy, 0.44f, 0.14f);
	rect2d(dx, dy, 0.14f, 0.44f);
	btnRect(JOY_UP,    dx, dy+0.16f, 0.13f, 0.13f, 220, 220, 220);
	btnRect(JOY_DOWN,  dx, dy-0.16f, 0.13f, 0.13f, 220, 220, 220);
	btnRect(JOY_LEFT,  dx-0.16f, dy, 0.13f, 0.13f, 220, 220, 220);
	btnRect(JOY_RIGHT, dx+0.16f, dy, 0.13f, 0.13f, 220, 220, 220);

	/* face buttons, in their canonical colors */
	fx = 0.62f;
	fy = 0.05f;
	btnRect(JOY_TRIANGLE, fx, fy+0.16f, 0.14f, 0.14f,  60, 220, 140);
	btnRect(JOY_CIRCLE,   fx+0.16f, fy, 0.14f, 0.14f, 235,  90,  80);
	btnRect(JOY_CROSS,    fx, fy-0.16f, 0.14f, 0.14f, 130, 160, 235);
	btnRect(JOY_SQUARE,   fx-0.16f, fy, 0.14f, 0.14f, 225, 130, 200);

	/* select and start */
	btnRect(JOY_SELECT, -0.16f, -0.16f, 0.14f, 0.07f, 200, 200, 200);
	btnRect(JOY_START,   0.16f, -0.16f, 0.14f, 0.07f, 200, 200, 200);

	/* sticks: the dot tracks the stick, clicking it in turns it red */
	stickGate(-0.30f, -0.55f, 0.34f);
	stickGate( 0.30f, -0.55f, 0.34f);
	btnRect(JOY_L3, -0.30f + 0.12f*joy.lx, -0.55f - 0.12f*joy.ly,
		0.10f, 0.10f, 255, 60, 60);
	btnRect(JOY_R3,  0.30f + 0.12f*joy.rx, -0.55f - 0.12f*joy.ry,
		0.10f, 0.10f, 255, 60, 60);
}

/*
 * The table
 */

Scene scenes[] = {
	{ "prims", scenePrims },
	{ "primlist", scenePrimList },
	{ "plfile", scenePlFile },
	{ "town", sceneTown },
	{ "texture", sceneTexture },
	{ "lit", sceneLit },
	{ "lights", sceneLights },
	{ "dsm", sceneDsm },
	{ "skin", sceneSkin },
	{ "tube", sceneTube },
	{ "littex", sceneLitTex },
	{ "im2d", sceneIm2d },
	{ "blend", sceneBlend },
	{ "fog", sceneFog },
	{ "direct", sceneDirect },
	{ "file", sceneFile },
	{ "pad", scenePad },
};
int numScenes = nelem(scenes);
