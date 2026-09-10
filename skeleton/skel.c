/* The shared program layer: graphics context, VIF list, pad, frame loop.
 * Factored out of what used to be src/main.c; see skel.h. */

#include "skel.h"
#include "joy.h"
#include <string.h>

#include <libgraph.h>
#include <libdma.h>
#include <sifrpc.h>

#define VIDEOMODE SCE_GS_NTSC

int skelWidth = 640;
int skelHeight = 448;
int skelFrameCount;
unsigned int skelTimeCpu, skelTimeGs, skelTimeVsync;
static unsigned int frameStart;

unsigned int
skelCount(void)
{
	unsigned int c;
	__asm__ __volatile__("mfc0 %0, $9" : "=r"(c));
	return c;
}

xtcgBuffers skelBuffers;

static mdmaArena vifArena;
static mdmaList viflist;
static uint128 vifBuffer[100*1024];
static int field;

static Mat4 identity = {
	{ 1.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, 1.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 1.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f, 1.0f }
};

void
skelClearColor(int r, int g, int b)
{
	xtcClearColor(r, g, b, 255);
}

void
skelDefaultState(void)
{
	xtcSetWorldMatrix(&identity);
	xtcEnable(XTC_DEPTH_TEST);
	xtcDisable(XTC_BLEND);
	xtcDisable(XTC_FOG);
	xtcDisable(XTC_TEXTURE);
	xtcDisable(XTC_CLIPPING);
	xtcSetTexture(nil);
	xtcTexFilter(XTC_LINEAR, XTC_LINEAR);
	xtcTexFunc(XTC_RGB, XTC_MODULATE);
}

void
skelInit(int width, int height)
{
	skelWidth = width;
	skelHeight = height;

	sceSifInitRpc(0);
	joyInit();

	mdmaInit();
	xtcgResetGraph(SCE_GS_INTERLACE, VIDEOMODE, SCE_GS_FIELD);
	xtcgInitBuffers(&skelBuffers, width, height,
		SCE_GS_PSMCT32, SCE_GS_PSMZ24);

	mdmaArenaInit(&vifArena, vifBuffer, nelem(vifBuffer), 1, MDMA_MEM_CACHED);
	mdmaListInit(&viflist, &vifArena);
	xtcSetList(&viflist);
	xtcInit(width, height, 24);

	xtcScissor(0, 0, width, height);
	xtcClearDepth(0);
	xtcClearColor(64, 64, 64, 255);

	/* the list still holds the state xtcInit put there; send it */
	mdmaEnd(&viflist, 0);
	mdmaCloseTag(&viflist);
	mdmaKick(mdmaVIF1, &viflist);
	sceGsSyncPath(0, 0);
}

void
skelBeginFrame(void)
{
	joyUpdate();
	mdmaListReset(&viflist);
	xtcSetDraw(&skelBuffers.draw[field]);
	xtcClear(XTC_COLORBUF | XTC_DEPTHBUF);
	skelDefaultState();
	frameStart = skelCount();
}

void
skelEndFrame(void)
{
	unsigned int t;

	mdmaEnd(&viflist, 0);
	mdmaCloseTag(&viflist);
	t = skelCount();
	skelTimeCpu = t - frameStart;
	mdmaKick(mdmaVIF1, &viflist);
	sceGsSyncPath(0, 0);
	skelTimeGs = skelCount() - t;
	t = skelCount();

	xtcgWaitVSynch();
	skelTimeVsync = skelCount() - t;
	xtcgSetDisp(&skelBuffers.disp[field]);
	field ^= 1;
	skelFrameCount++;
}

void
skelRun(void (*frame)(void))
{
	for(;;) {
		skelBeginFrame();
		frame();
		skelEndFrame();
	}
}
