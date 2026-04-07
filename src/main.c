#include "gs.h"
#include "kernel_shim.h"
#include "m.h"
#include "mdma.h"
#include "mem.h"
#include "xtc.h"

#include <assert.h>
#include <math.h>

#include <sifrpc.h>

#define VIDEOMODE GS_MODE_NTSC
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 448

const uint32 xoffset = 2048-SCREEN_WIDTH/2;
const uint32 yoffset = 2048-SCREEN_HEIGHT/2;


void dumpDma(uint *packet, int data);

mdmaBuffers buffers;
mdmaList viflist;
uint128	    vifBuffer[100 * 1024] __attribute__((aligned(128)));

void
drawThing(void)
{
	static int x = 0;
	static int y = 0;
	static int vx = 1;
	static int vy = 1;
	static int sz = 32;

	mdmaCntDirect(&viflist, 4);
	mdmaAddGIFtag(&viflist, 3, 1, 1, GS_PRIM_SPRITE, GS_GIF_PACKED, 1, 0xe);
	mdmaAddAD(&viflist, GS_REG_RGBAQ, GS_SET_RGBAQ(255, 255, 255, 255, 255));
	mdmaAddAD(&viflist, GS_REG_XYZ2, GS_SET_XYZ((xoffset + x) << 4, (yoffset + y) << 4, 0));
	mdmaAddAD(&viflist, GS_REG_XYZ2,
	    GS_SET_XYZ((xoffset + x + sz) << 4, (yoffset + y + sz) << 4, 0));

	x += vx;
	y += vy;
	if(x < 0 || x+sz >= SCREEN_WIDTH) vx = -vx;
	if(y < 0 || y+sz >= SCREEN_HEIGHT) vy = -vy;
}




float identity[16] = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
};





void
drawIm2D(void)
{
	xtcSetPipeline(twodPipeline);

//	xtcEnable(XTC_BLEND);
	xtcDisable(XTC_BLEND);
	xtcBlendFuncSrcDst(XTC_BLEND_SRCALPHA, XTC_BLEND_INVSRCALPHA);

	xtcBegin(XTC_TRILIST);
/*
		xtcColor(0, 128, 255, 255);
		xtcVertex(10.0f, 10.0f, 0.0f);
		xtcVertex(100.0f, 10.0f, 0.0f);
		xtcVertex(100.0f, 100.0f, 0.0f);

		xtcColor(255, 128, 0, 255);
		xtcVertex(10.0f, 10.0f, 0.0f);
		xtcVertex(10.0f, 100.0f, 0.0f);
		xtcVertex(100.0f, 100.0f, 0.0f);
*/

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

/*
		xtcColor(255, 255, 255, 128);
		xtcVertex(-0.2f, -0.1f, -0.1f);
		xtcVertex(0.7f, -0.1f, 0.1f);
		xtcColor(255, 0, 255, 128);
		xtcVertex(0.7f, 0.8f, 0.1f);
*/
	xtcEnd();
}

void
setCam(void)
{
	float proj[16], cam[16], view[16];
	makePerspective(proj, 70.0f, 4.0f/3.0f, 0.1f, 100.0f);
	xtcSetProjectionMatrix(proj);
//	float pos[3] = { 5.0f, 5.0f, 3.0f };
	float pos[3] = { 3.0f, 3.0f, 2.0f };
//	float pos[3] = { 3.0f, 3.0f, 0.0f };
	float targ[3] = { 0.0f, 0.0f, 0.0f };
	float up[3] = { 0.0f, 0.0f, 1.0f };
	float fwd[3] = { targ[0]-pos[0], targ[1]-pos[1], targ[2]-pos[2] };
	makeLookAt(cam, fwd, up, pos);
	invertOrthonormal(view, cam);
	xtcSetViewMatrix(view);
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
drawCube(void)
{
	xtcSetPipeline(nolightPipeline);

//	xtcEnable(XTC_BLEND);
//	xtcBlendFuncSrcDst(XTC_BLEND_SRCALPHA, XTC_BLEND_INVSRCALPHA);

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

//	float s = 0.3f;
	float s = 1.0f;
	xtcBegin(XTC_TRILIST);
		for(uint32 i = 0; i < nelem(indices); i++) {
			int idx = indices[i];
			xtcTexCoord(st[i%6].s, st[i%6].t, 1.0f);
			xtcColor(verts[idx].r, verts[idx].g, verts[idx].b, verts[idx].a);
			xtcVertex(verts[idx].x, verts[idx].y, verts[idx].z);
//			xtcVertex(1.0f+s*verts[idx].x, s*verts[idx].y, s*verts[idx].z);
		}
	xtcEnd();
}

xtcPrimList *sphere;

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
//if(i&1) continue;

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
/*
	for(int i = 0; i < 3; i++) {
		for(int j = 0; j < 3; j++) {
			int x1 = i*4 + j;
			int x2 = (i+1)*4 + j;
			int x3 = i*4 + (j+1);
			int x4 = (i+1)*4 + (j+1);

			xtcVertex(cvs[x1][0], cvs[x1][1], cvs[x1][2]);
			xtcVertex(cvs[x2][0], cvs[x2][1], cvs[x2][2]);
			xtcVertex(cvs[x4][0], cvs[x4][1], cvs[x4][2]);

			xtcVertex(cvs[x1][0], cvs[x1][1], cvs[x1][2]);
			xtcVertex(cvs[x3][0], cvs[x3][1], cvs[x3][2]);
			xtcVertex(cvs[x4][0], cvs[x4][1], cvs[x4][2]);
		}
	}
*/

	xtcEnd();
}

void
drawObj(float (*verts)[3], float (*tex)[2], float (*normals)[3], int (*faces)[3][3], int nfaces)
{
	float s = 0.1f;
	xtcColor(128, 128, 128, 255);
//	srand(0);

	xtcBegin(XTC_TRILIST);
		for(int i = 0; i < nfaces; i++) {
//			xtcColor(rand()&0xFF, rand()&0xFF, rand()&0xFF, 255);
//			xtcColor(255, 0, 0, 255);
			xtcColor(0, 0, 0, 255);

			for(int v = 0; v < 3; v++) {
				int vx = faces[i][v][0]-1;
				int vt = faces[i][v][1]-1;
				int vn = faces[i][v][2]-1;
				xtcNormal(normals[vn][0], -normals[vn][2], normals[vn][1]);
//				xtcColor(255*(1.0f+normals[vn][0]), 255*(1.0f+normals[vn][1]), 255*(1.0f+normals[vn][2]), 255);
//				xtcVertex(s*verts[vx][0], s*verts[vx][1], s*verts[vx][2]);
				xtcVertex(s*verts[vx][0], -s*verts[vx][2], s*verts[vx][1]);
			}
		}
	xtcEnd();
}

void
drawTeapot(void)
{
	float patch[16][3];
	int *indices;

	xtcSetPipeline(defaultPipeline);

	drawObj(teapot_verts, teapot_tex, teapot_normals, teapot_faces, nelem(teapot_faces));

/*
	indices = &patchdata[1][0];
	for(int i = 0; i < 16; i++) {
		patch[i][0] = cpdata[indices[i]][0];
		patch[i][1] = cpdata[indices[i]][1];
		patch[i][2] = cpdata[indices[i]][2];
	}
	drawPatch(patch);
*/
}

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

int
main()
{
	ShimInitRpc(0);

	// this will make debugging and finding leaks easier
//	memInitManaged();

	mdmaInit();
	mdmaResetGraph(GS_INTERLACED, VIDEOMODE, GS_FFMD_FIELD);
	mdmaInitBuffers(&buffers, SCREEN_WIDTH, SCREEN_HEIGHT, GS_TEX_32, GS_ZBUFF_24);

	raster32 = xtcReadPNG(SIZED(tex32));
	raster24 = xtcReadPNG(SIZED(tex24));
	raster8 = xtcReadPNG(SIZED(tex8));
	raster4 = xtcReadPNG(SIZED(tex4));

	mdmaStart(&viflist, vifBuffer, nelem(vifBuffer));
	xtcSetList(&viflist);
	xtcInit(SCREEN_WIDTH, SCREEN_HEIGHT, 24);

	// normalized coordinates
//	xtcViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	// raw screen coordinates:
//	xtcViewport(-1, SCREEN_HEIGHT+1, 2, -2);

	xtcScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	// test
//	xtcScissor(160, 112, 320, 224);

	xtcFog(50.0f, 100.0f, 0);

//xtcrUpload(raster32);
//xtcrUpload(raster8);
//xtcrUpload(raster4);

	xtcClearDepth(0);
	xtcClearColor(64, 64, 64, 255);
	mdmaFinish(&viflist);
	mdmaSendSynch(DMA_CHAN_VIF1, &viflist);

	xtcSetAmbient(32, 32, 32);
	xtcLight l;
	l.enabled = 1;
	l.type = XTC_LIGHT_DIRECT;
	l.color = (xtcRGBA){ 128.0f, 128.0f, 128.0f, 255.0f };
	l.direction = (xtcVec3){ -1.0f, 1.0f, -1.0f };
	normalize((float*)&l.direction, (float*)&l.direction);
	xtcSetLight(0, &l);

	int f = 0;
	for(;;) {
		mdmaStart(&viflist, vifBuffer, nelem(vifBuffer));
		xtcSetDraw(&buffers.draw[f]);
		xtcClear(XTC_COLORBUF | XTC_DEPTHBUF);
//		drawThing();

		setCam();
		xtcSetWorldMatrix(identity);

		rotateWorld();

		drawAxes();

		xtcEnable(XTC_DEPTH_TEST);
		xtcTexFilter(XTC_LINEAR, XTC_LINEAR);
		xtcTexFunc(XTC_RGB, XTC_MODULATE);
//		moveInCircle(10.0f);
		drawTeapot();
//		drawSphere();

		xtcEnable(XTC_TEXTURE);
		xtcBindTexture(raster24);
//		drawCube();


/*
	const float scl = 128.0f/255.0f;
	xtcColorScaleTex(1.0f, 1.0f, 1.0f, scl);
//		xtcTexFunc(XTC_RGB, XTC_DECAL);
		xtcDisable(XTC_DEPTH_TEST);
		drawIm2D();
	xtcColorScaleTex(scl, scl, scl, scl);
*/

		xtcDisable(XTC_TEXTURE);
		xtcBindTexture(nil);


		mdmaFinish(&viflist);
//dumpDma((uint*)viflist.p, 1);
		mdmaSendSynch(DMA_CHAN_VIF1, &viflist);

		mdmaWaitVSynch();
		mdmaSetDisp(&buffers.disp[f]);
		f ^= 1;
	}

	return 0;
}
