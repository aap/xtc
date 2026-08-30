#include "xtc.h"
#include "m.h"
#include "mem.h"
#include "joy.h"
#include "scenes.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#include <libgraph.h>
#include <libdma.h>
#include <sifrpc.h>

#define VIDEOMODE SCE_GS_NTSC
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 448

void dumpDma(unsigned int *packet, int data);

xtcgBuffers buffers;
mdmaArena vifArena;
mdmaList viflist;
uint128 vifBuffer[100*1024];

float identity[16] = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
};

/* orbit camera around the origin; left stick orbits, right stick Y zooms */
float camTheta = 0.785f;	/* azimuth */
float camPhi = 0.44f;		/* elevation */
float camDist = 4.7f;

void
updateCam(void)
{
	float proj[16], cam[16], view[16];

	camTheta += 0.05f*joy.lx;
	camPhi -= 0.05f*joy.ly;
	if(camPhi > 1.5f) camPhi = 1.5f;
	if(camPhi < -1.5f) camPhi = -1.5f;
	camDist += 0.1f*joy.ry;
	if(camDist < 0.5f) camDist = 0.5f;

	makePerspective(proj, 70.0f, 4.0f/3.0f, 0.1f, 100.0f);
	xtcSetProjectionMatrix(proj);
	float pos[3] = {
		camDist*cosf(camPhi)*cosf(camTheta),
		camDist*cosf(camPhi)*sinf(camTheta),
		camDist*sinf(camPhi)
	};
	float targ[3] = { 0.0f, 0.0f, 0.0f };
	float up[3] = { 0.0f, 0.0f, 1.0f };
	float fwd[3] = { targ[0]-pos[0], targ[1]-pos[1], targ[2]-pos[2] };
	makeLookAt(cam, fwd, up, pos);
	invertOrthonormal(view, cam);
	xtcSetViewMatrix(view);
}

int curScene;

int
main()
{
	sceSifInitRpc(0);
	joyInit();

	// this will make debugging and finding leaks easier
//	memInitManaged();

	mdmaInit();
	xtcgResetGraph(SCE_GS_INTERLACE, VIDEOMODE, SCE_GS_FIELD);
	xtcgInitBuffers(&buffers, SCREEN_WIDTH, SCREEN_HEIGHT,
		SCE_GS_PSMCT32, SCE_GS_PSMZ24);

	mdmaArenaInit(&vifArena, vifBuffer, nelem(vifBuffer), 1, MDMA_MEM_CACHED);
	mdmaListInit(&viflist, &vifArena);
	xtcSetList(&viflist);
	xtcInit(SCREEN_WIDTH, SCREEN_HEIGHT, 24);

	scenesInit();

	// normalized coordinates
//	xtcViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	// raw screen coordinates:
//	xtcViewport(-1, SCREEN_HEIGHT+1, 2, -2);

	xtcScissor(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	// test
//	xtcScissor(160, 112, 320, 224);

	xtcClearDepth(0);
	xtcClearColor(64, 64, 64, 255);
	mdmaEnd(&viflist, 0);
	mdmaCloseTag(&viflist);
	mdmaKick(mdmaVIF1, &viflist);
	sceGsSyncPath(0, 0);

	printf("scene: %s\n", scenes[curScene].name);

	int f = 0;
	for(;;) {
		joyUpdate();
		if(joy.press & JOY_R1)
			curScene = (curScene+1) % numScenes;
		if(joy.press & JOY_L1)
			curScene = (curScene+numScenes-1) % numScenes;
		if(joy.press & (JOY_L1|JOY_R1))
			printf("scene: %s\n", scenes[curScene].name);

		mdmaListReset(&viflist);
		xtcSetDraw(&buffers.draw[f]);
		xtcClear(XTC_COLORBUF | XTC_DEPTHBUF);

		updateCam();
		xtcSetWorldMatrix(identity);

		// the baseline the scenes start from
		xtcEnable(XTC_DEPTH_TEST);
		xtcDisable(XTC_BLEND);
		xtcDisable(XTC_FOG);
		xtcDisable(XTC_TEXTURE);
		xtcDisable(XTC_CLIPPING);
		xtcBindTexture(nil);
		xtcTexFilter(XTC_LINEAR, XTC_LINEAR);
		xtcTexFunc(XTC_RGB, XTC_MODULATE);

		scenes[curScene].draw();

		mdmaEnd(&viflist, 0);
		mdmaCloseTag(&viflist);
//dumpDma((unsigned int*)viflist.p, 1);
		mdmaKick(mdmaVIF1, &viflist);
		sceGsSyncPath(0, 0);

		xtcgWaitVSynch();
		xtcgSetDisp(&buffers.disp[f]);
		f ^= 1;
	}

	return 0;
}
