/* xtcdemo: the example and test scenes.  Everything that is not about
 * the scenes -- graphics context, VIF list, pad, frame loop -- lives in
 * skeleton/skel.c now. */

#include "xtci.h"
#include "skel.h"
#include "mem.h"
#include "joy.h"
#include "scenes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 448

void dumpDma(unsigned int *packet, int data);

/* orbit camera around the origin; left stick orbits, right stick Y zooms */
float camTheta = 0.785f;	/* azimuth */
float camPhi = 0.44f;		/* elevation */
float camDist = 4.7f;

void
updateCam(void)
{
	Mat4 proj, view;
	Vec3 pos;

	camTheta += 0.05f*joy.lx;
	camPhi -= 0.05f*joy.ly;
	if(camPhi > 1.5f) camPhi = 1.5f;
	if(camPhi < -1.5f) camPhi = -1.5f;
	camDist += 0.1f*joy.ry;
	if(camDist < 0.5f) camDist = 0.5f;

	proj = m4persp(70.0f, 4.0f/3.0f, 0.1f, 100.0f);
	xtcSetProjectionMatrix(&proj);
	pos = vec3(camDist*cosf(camPhi)*cosf(camTheta),
	           camDist*cosf(camPhi)*sinf(camTheta),
	           camDist*sinf(camPhi));
	view = m4invOrtho(m4lookat(pos, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f)));
	xtcSetViewMatrix(&view);
}

int curScene;

static void
drawFrame(void)
{
	if(joy.press & JOY_R1)
		curScene = (curScene+1) % numScenes;
	if(joy.press & JOY_L1)
		curScene = (curScene+numScenes-1) % numScenes;
	if(joy.press & (JOY_L1|JOY_R1))
		printf("scene: %s\n", scenes[curScene].name);

	updateCam();
	scenes[curScene].draw();
}

int
main(int argc, char *argv[])
{
	// boot into a named scene: pcsx2run.sh -s <scene>, or dsedb run elf <scene>.
	// any argv entry may hold it: pcsx2's -gameargs replaces the whole
	// vector (the scene arrives as argv[0]), dsedb appends after the path
	for(int i = 0; i < argc; i++)
		for(int j = 0; j < numScenes; j++)
			if(strcmp(scenes[j].name, argv[i]) == 0)
				curScene = j;

	// this will make debugging and finding leaks easier
//	memInitManaged();

	skelInit(SCREEN_WIDTH, SCREEN_HEIGHT);

	scenesInit();

	// normalized coordinates
//	xtcViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
	// raw screen coordinates:
//	xtcViewport(-1, SCREEN_HEIGHT+1, 2, -2);

	// test
//	xtcScissor(160, 112, 320, 224);

	printf("scene: %s\n", scenes[curScene].name);

	skelRun(drawFrame);

	return 0;
}
