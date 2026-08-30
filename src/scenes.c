#include "xtc.h"
#include "m.h"
#include "joy.h"
#include "scenes.h"
#include <stdio.h>
#include <math.h>

#include <libgraph.h>

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

xtcRaster *raster32;
xtcRaster *raster24;
xtcRaster *raster8;
xtcRaster *raster4;

void
scenesInit(void)
{
	raster32 = xtcReadPNG(SIZED(tex32));
	raster24 = xtcReadPNG(SIZED(tex24));
	raster8  = xtcReadPNG(SIZED(tex8));
	raster4  = xtcReadPNG(SIZED(tex4));
}

/*
 * Helpers
 */

static void
setWorld(float x, float y, float z, float s)
{
	float world[16] = {
		s, 0.0f, 0.0f, 0.0f,
		0.0f, s, 0.0f, 0.0f,
		0.0f, 0.0f, s, 0.0f,
		x, y, z, 1.0f
	};
	xtcSetWorldMatrix(world);
}

void
rotateWorld(void)
{
	static float t = 0.0f;
	float speed = 1.0f;
	float world[16];
	world[0] = cosf(t*speed);
	world[1] = sinf(t*speed);
	world[2] = 0.0f;
	world[3] = 0.0f;

	world[4] = -sinf(t*speed);
	world[5] = cosf(t*speed);
	world[6] = 0.0f;
	world[7] = 0.0f;

	world[8] = 0.0f;
	world[9] = 0.0f;
	world[10] = 1.0f;
	world[11] = 0.0f;

	world[12] = 0.0f;
	world[13] = 0.0f;
	world[14] = 0.0f;
	world[15] = 1.0f;
	xtcSetWorldMatrix(world);

	t += 0.01f;
}

void
moveInCircle(float r)
{
	static float t = 0.0f;
	float speed = 1.0f;
	float world[16];
	world[0] = 1.0f;
	world[1] = 0.0f;
	world[2] = 0.0f;
	world[3] = 0.0f;

	world[4] = 0.0f;
	world[5] = 1.0f;
	world[6] = 0.0f;
	world[7] = 0.0f;

	world[8] = 0.0f;
	world[9] = 0.0f;
	world[10] = 1.0f;
	world[11] = 0.0f;

	world[12] = r*cosf(t*speed);
	world[13] = r*sinf(t*speed);
	world[14] = 0.0f;
	world[15] = 1.0f;
	xtcSetWorldMatrix(world);

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
		xtcTexCoord(0.0f, 1.0f, 1.0f);
		xtcVertex(x0, y0, 0.0f);
		xtcTexCoord(1.0f, 1.0f, 1.0f);
		xtcVertex(x1, y0, 0.0f);
		xtcTexCoord(0.0f, 0.0f, 1.0f);
		xtcVertex(x0, y1, 0.0f);
		xtcTexCoord(1.0f, 0.0f, 1.0f);
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
			xtcTexCoord(st[i%6].s, st[i%6].t, 1.0f);
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

static void
scenePrimList(void)
{
	xtcEnable(XTC_CLIPPING);
	drawAxes();
	rotateWorld();
	drawSphere();
}

/*
 * Scene: texture — one quad per raster format: 32, 24, 8 and 4 bit.
 */

static void
texQuad(xtcRaster *r, float x, float z)
{
	xtcBindTexture(r);
	xtcBegin(XTC_TRISTRIP);
		xtcColor(255, 255, 255, 255);
		xtcTexCoord(0.0f, 1.0f, 1.0f);
		xtcVertex(x-0.9f, 0.0f, z-0.9f);
		xtcTexCoord(1.0f, 1.0f, 1.0f);
		xtcVertex(x+0.9f, 0.0f, z-0.9f);
		xtcTexCoord(0.0f, 0.0f, 1.0f);
		xtcVertex(x-0.9f, 0.0f, z+0.9f);
		xtcTexCoord(1.0f, 0.0f, 1.0f);
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

void
drawObj(float (*verts)[3], float (*tex)[2], float (*normals)[3], int (*faces)[3][3], int nfaces)
{
	float s = 0.1f;
	xtcColor(128, 128, 128, 255);

	xtcBegin(XTC_TRILIST);
		for(int i = 0; i < nfaces; i++) {
			xtcColor(0, 0, 0, 255);

			for(int v = 0; v < 3; v++) {
				int vx = faces[i][v][0]-1;
				int vt = faces[i][v][1]-1;
				int vn = faces[i][v][2]-1;
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

	drawObj(teapot_verts, teapot_tex, teapot_normals, teapot_faces, nelem(teapot_faces));
}

static void
setLights(void)
{
	static float t = 0.0f;
	xtcLight l;

	xtcSetAmbient(32, 32, 32);

	l.enabled = 1;
	l.type = XTC_LIGHT_DIRECT;
	l.color.r = 140.0f;
	l.color.g = 140.0f;
	l.color.b = 130.0f;
	l.color.a = 255.0f;
	l.direction.x = -cosf(t);
	l.direction.y = -sinf(t);
	l.direction.z = -0.7f;
	normalize((float*)&l.direction, (float*)&l.direction);
	xtcSetLight(0, &l);

	l.color.r = 0.0f;
	l.color.g = 40.0f;
	l.color.b = 120.0f;
	l.direction.x = 1.0f;
	l.direction.y = -1.0f;
	l.direction.z = 1.0f;
	normalize((float*)&l.direction, (float*)&l.direction);
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
			xtcTexCoord(s, theta1/PI, 1.0f);
			xtcVertex(x1, y1, z1);

			float x2 = sinf(theta2)*x;
			float y2 = sinf(theta2)*y;
			float z2 = cosf(theta2);
			xtcColor(0, 0, 0, 255);
			xtcNormal(x2, y2, z2);
			xtcTexCoord(s, theta2/PI, 1.0f);
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
	xtcBindTexture(raster32);
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
		xtcTexCoord(0.0f, 0.0f, 1.0f);
		xtcVertex(0.0f, 0.0f, 0.0f);
		xtcTexCoord(0.0f, 1.0f, 1.0f);
		xtcVertex(0.0f, 0.9f, 0.0f);
		xtcTexCoord(1.0f, 1.0f, 1.0f);
		xtcVertex(0.9f, 0.9f, 0.0f);

		xtcColor(255, 128, 0, 255);
		xtcTexCoord(0.0f, 0.0f, 1.0f);
		xtcVertex(0.0f, 0.0f, 0.0f);
		xtcTexCoord(1.0f, 0.0f, 1.0f);
		xtcVertex(0.9f, 0.0f, 0.0f);
		xtcTexCoord(1.0f, 1.0f, 1.0f);
		xtcVertex(0.9f, 0.9f, 0.0f);
	xtcEnd();
}

static void
sceneIm2d(void)
{
	const float scl = 128.0f/255.0f;

	xtcDisable(XTC_DEPTH_TEST);
	xtcEnable(XTC_TEXTURE);
	xtcBindTexture(raster8);
	xtcColorScaleTex(1.0f, 1.0f, 1.0f, scl);
	drawIm2D();
	xtcColorScaleTex(scl, scl, scl, scl);
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
	xtcBindTexture(raster32);
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
	{ "texture", sceneTexture },
	{ "lit", sceneLit },
	{ "littex", sceneLitTex },
	{ "im2d", sceneIm2d },
	{ "blend", sceneBlend },
	{ "fog", sceneFog },
	{ "direct", sceneDirect },
	{ "pad", scenePad },
};
int numScenes = nelem(scenes);
