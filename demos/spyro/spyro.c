/*
 * spyro -- a level viewer for Spyro the Dragon (PS1) on the PS2.
 * All 35 levels of the game; START and SELECT walk through them.
 *
 * The assets are the game's, so they are not in the repo.  They live in
 * local/spyro, which is gitignored, and are made by demos/spyro/import.sh
 * straight out of the game's archive:
 *
 *	WAD=/path/to/WAD.WAD demos/spyro/import.sh
 *
 * which is demos/spyro/host/spyroconv.c (every level -> levels/levelNN/{world.xm,
 * sky.xm} and its 32x32 palettised tex_NNN.png, which is what the
 * materials name) followed by `make chunks`, which turns each of the 70
 * .xm files into build/chk/spyro/levelNN_{world,sky}.chk.
 *
 * Everything is loaded over host:, so this wants to run from the repo
 * root -- pcsx2run.sh passes an absolute ELF path and host: resolves
 * next to it.  Without the chunks the .xm text files are read instead,
 * which works but takes a few seconds to build the prim lists.
 *
 * The level is drawn the way RenderWare would draw a world: unlit, the
 * vertex colours as emissive (the materials say colormaterial 1) and
 * the texture modulated onto them.  The sky is a dome with no textures
 * that rides with the camera and neither tests nor writes depth.
 *
 * Pad:
 *	left stick	orbit around the target
 *	right stick	turn (look around)
 *	dpad up/down	dolly forward and back
 *	dpad left/right	pan sideways
 *	square / cross	zoom in / out
 *	START / SELECT	next / previous level
 *	L1		sky on/off
 *	R1		world on/off
 *	L2		textures on/off
 *	triangle	print the camera
 * (the camera is the one from the librw spyro demo, spyrodemo/src/camera.cpp)
 *
 * Boot arguments, for a run with no pad -- pcsx2run.sh -s WORD:
 *	level=N		start at level N instead of 0
 *	tour		go to the next level every 3 seconds
 *	tour=N		... every N seconds
 *	notex nosky noworld	leave that out, for measuring
 */

#include "xtci.h"
#include "skel.h"
#include "joy.h"
#include "mem.h"
#include "xmodel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 448

/*
 * The levels, in the order spyroconv writes them: WAD files 10, 12,
 * ... 78 become level00 .. level34, which is the game's own level
 * order.  The names are only for the printout, the index is what
 * everything else goes by.
 */
static const char *levelNames[] = {
	"Artisans",		"Stone Hill",		"Dark Hollow",
	"Town Square",		"Toasty",		"Sunny Flight",
	"Peace Keepers",	"Dry Canyon",		"Cliff Town",
	"Ice Cavern",		"Doctor Shemp",		"Night Flight",
	"Magic Crafters",	"Alpine Ridge",		"High Caves",
	"Wizard Peak",		"Blowhard",		"Crystal Flight",
	"Beast Makers",		"Terrace Village",	"Misty Bog",
	"Tree Tops",		"Metalhead",		"Wild Flight",
	"Dream Weavers",	"Dark Passage",		"Lofty Castle",
	"Haunted Towers",	"Jacques",		"Icy Flight",
	"Gnorc Gnexus",		"Gnorc Cove",		"Twilight Harbor",
	"Gnasty Gnorc",		"Gnasty's Loot"
};
#define NUMLEVELS ((int)nelem(levelNames))

static int level;		/* the one that is loaded */
static xModel *world;
static xModel *sky;

/*
 * The camera: a position and a target, moved the way the librw spyro
 * demo does it.  The start is the view the GL sketch renders for
 * comparison: the island from the south west, a bit above the towers.
 */
static Vec3 camPos = { -30.0388f, -60.0564f, 26.0144f };
static Vec3 camTarget = { 0.0f, -6.0f, 6.0f };
static Vec3 camUp = { 0.0f, 0.0f, 1.0f };
static Vec3 camLocalUp = { 0.0f, 0.0f, 1.0f };
/* that same view as a direction, normalised: from the south west and a
 * little above.  Levels are all sizes and in all places, so camReset
 * takes the distance and the speeds from the bounding sphere */
static Vec3 camStartDir = { -0.4622f, -0.8318f, 0.3079f };
static float camSpeed = 0.2f;	/* per frame, dolly/pan/zoom */
static float camFar = 600.0f;	/* the far plane, big enough for the sky */

static void
camReset(Vec3 center, float radius)
{
	/* 2.15r would put the whole sphere inside the 70 degree field of
	 * view, but a level is a flat thing seen from above and the
	 * sphere is mostly its width, so that leaves it tiny.  1.2r is
	 * where the view demos/spyro started at sat -- the island fills
	 * the frame and the corners of the box fall outside it. */
	camTarget = center;
	camPos = v3add(center, v3scale(1.2f*radius, camStartDir));
	camUp = vec3(0.0f, 0.0f, 1.0f);
	camLocalUp = camUp;
	camSpeed = radius*0.004f;
	camFar = 3.5f*radius;
}

/* rotate the view direction: around z, then around the camera's right */
static Vec3
camRotateDir(Vec3 dir, float yaw, float pitch)
{
	Quat r = qaxisangle(vec3(0.0f, 0.0f, 1.0f), yaw);
	Vec3 right;

	dir = qrotatev3(r, dir);
	camLocalUp = qrotatev3(r, camLocalUp);
	right = v3normalized(v3cross(dir, camLocalUp));
	r = qaxisangle(right, pitch);
	dir = qrotatev3(r, dir);
	camLocalUp = v3normalized(v3cross(right, dir));
	camUp.z = camLocalUp.z >= 0.0f ? 1.0f : -1.0f;
	return dir;
}

/* look around: the target moves */
static void
camTurn(float yaw, float pitch)
{
	Vec3 dir = camRotateDir(v3sub(camTarget, camPos), yaw, pitch);
	camTarget = v3add(camPos, dir);
}

/* circle the target: the position moves */
static void
camOrbit(float yaw, float pitch)
{
	Vec3 dir = camRotateDir(v3sub(camTarget, camPos), yaw, -pitch);
	camPos = v3sub(camTarget, dir);
}

/* along the view, both together */
static void
camDolly(float dist)
{
	Vec3 dir = v3scale(dist, v3normalized(v3sub(camTarget, camPos)));
	camPos = v3add(camPos, dir);
	camTarget = v3add(camTarget, dir);
}

/* towards the target, never through it */
static void
camZoom(float dist)
{
	Vec3 dir = v3sub(camTarget, camPos);
	float cur = v3norm(dir);
	if(dist >= cur)
		dist = cur - 0.3f;
	camPos = v3add(camPos, v3scale(dist, v3normalized(dir)));
}

/* sideways and up, both together */
static void
camPan(float x, float y)
{
	Vec3 dir = v3normalized(v3sub(camTarget, camPos));
	Vec3 right = v3normalized(v3cross(dir, camUp));
	Vec3 localup = v3normalized(v3cross(right, dir));
	Vec3 d = v3add(v3scale(x, right), v3scale(y, localup));
	camPos = v3add(camPos, d);
	camTarget = v3add(camTarget, d);
}

static int drawSky = 1;
static int drawWorld = 1;

/* `trace': print one frame's texture binds, for demos/spyro/host/texsim.lua */
static int traceBinds;
static void
bindHook(xtcTexture *tex, int pages)
{
	printf("bind %p %d\n", (void*)tex, pages);
}
static int textured = 1;
/* the tour: seconds a level is up before the next one, 0 for never.
 * The clock is the EE's, so a level that draws slowly stays up as long
 * as one that does not */
static unsigned int tour;

static Mat4 identity = {
	{ 1.0f, 0.0f, 0.0f, 0.0f },
	{ 0.0f, 1.0f, 0.0f, 0.0f },
	{ 0.0f, 0.0f, 1.0f, 0.0f },
	{ 0.0f, 0.0f, 0.0f, 1.0f }
};

/*
 * The textures, so they can be switched off.  xMeshDraw uploads one per
 * mesh whatever the render state says, and with 838 meshes that is worth
 * being able to measure; clearing the xtcTexture in the shared xTexture
 * is what actually stops the upload.
 *
 * A material holds an xTexture each, so there are as many of those as
 * materials; how many distinct xtcTextures they point at is what the GS
 * sees, and readTexturePNG's cache keeps that down to one per image.
 */
enum {
	MAXTEX = 2560,	/* materials: the biggest level has 1888 + 191 */
	MAXGSTEX = 256	/* images: no level names more than 95 */
};
static xTexture *texList[MAXTEX];	/* one per material */
static xtcTexture *texSaved[MAXTEX];
static int numTex;
static xtcTexture *gsTexList[MAXGSTEX];	/* the distinct images behind them */
static int numGsTex;

static void
addTexture(xTexture *t)
{
	int i;

	if(t == nil)
		return;
	for(i = 0; i < numTex; i++)
		if(texList[i] == t)
			return;
	if(numTex == MAXTEX)
		return;
	texList[numTex] = t;
	texSaved[numTex] = t->tex;
	numTex++;

	if(t->tex == nil)
		return;
	for(i = 0; i < numGsTex; i++)
		if(gsTexList[i] == t->tex)
			return;
	if(numGsTex < MAXGSTEX)
		gsTexList[numGsTex++] = t->tex;
}

static void
collectTextures(xModel *m)
{
	int i;

	if(m == nil)
		return;
	for(i = 0; i < m->numMaterials; i++)
		addTexture(m->materials[i]->tex);
}

static void
applyTextured(void)
{
	int i;

	for(i = 0; i < numTex; i++)
		texList[i]->tex = textured ? texSaved[i] : nil;
}

/* the chunk the assembler linked (make chunks), else the text */
static xModel*
loadPart(int n, const char *part)
{
	char path[128];
	xModel *m;

	/* no snprintf in the PS2's libc; the paths are known and short */
	sprintf(path, "host:./build/chk/spyro/level%02d_%s.chk", n, part);
	m = loadXModel(path);
	if(m == nil) {
		sprintf(path, "host:./local/spyro/levels/level%02d/%s.xm", n, part);
		m = loadXModel(path);
	}
	if(m)
		buildXModel(m);
	return m;
}

/*
 * Everything a level owns goes back: the two chunks, and then the
 * images, which the chunks' globals pointed at and the model cache
 * owns.  In that order -- the models must be gone before the textures
 * they name are.  texList holds pointers into the chunk, so it has to
 * be forgotten first of all.
 */
static void
freeLevel(void)
{
	numTex = 0;
	numGsTex = 0;
	freeXModel(world);
	freeXModel(sky);
	world = nil;
	sky = nil;
	xTextureCacheFlush();
}

/* level.txt, written by the converter next to the models: the colour
 * the game clears to, which shows where the sky dome is open */
static void
loadLevelInfo(int n)
{
	char path[80];
	uint8 *data;
	uint32 size;
	int r, g, b;

	skelClearColor(160, 160, 160);
	sprintf(path, "host:./local/spyro/levels/level%02d/level.txt", n);
	if(!readfile(path, &data, &size))
		return;
	if(size < 32 && sscanf((char*)data, "bgcolor %d %d %d", &r, &g, &b) == 3)
		skelClearColor(r, g, b);
	free(data);
}

static char texpathBuf[64];
static unsigned int levelTime;	/* when the level came up, for the tour */

static void
loadLevel(int n)
{
	Vec3 center;
	float radius, skyRadius;
	unsigned int t0 = skelCount();

	freeLevel();
	level = n;
	/* the heap with nothing loaded: the same number every time, or
	 * the last level did not all come back */
	printf("spyro: level %d %s (empty heap %u of %u)\n", n, levelNames[n],
		memHeapUsed(), memHeapSize());

	sprintf(texpathBuf, "host:./local/spyro/levels/level%02d", n);
	texpath = texpathBuf;
	loadLevelInfo(n);

	world = loadPart(n, "world");
	if(world == nil) {
		printf("spyro: no world\n");
		center = vec3(0.0f, 0.0f, 0.0f);
		radius = 100.0f;
	} else {
		xModelBoundingSphere(world, &center, &radius);
		printf("spyro: world %d meshes, %d materials, radius %g, "
			"centre %g %g %g\n",
			world->numMeshes, world->numMaterials, radius,
			center.x, center.y, center.z);
	}
	camReset(center, radius);

	sky = loadPart(n, "sky");
	if(sky == nil)
		printf("spyro: no sky\n");
	else {
		xModelBoundingSphere(sky, &center, &skyRadius);
		printf("spyro: sky %d meshes, radius %g\n",
			sky->numMeshes, skyRadius);
		/* the dome rides with the camera, so it is the far plane
		 * that has to hold it */
		if(camFar < 1.2f*skyRadius)
			camFar = 1.2f*skyRadius;
	}

	collectTextures(world);
	collectTextures(sky);
	applyTextured();

	levelTime = skelCount();
	printf("spyro: %d texture references, %d images on the GS, "
		"%u ms, heap %u of %u\n",
		numTex, numGsTex,
		(levelTime - t0) / (SKEL_TICKS_PER_SEC/1000),
		memHeapUsed(), memHeapSize());
}

/* no lights at all: every material term but the emissive one has
 * nothing to work with, and the emissive one is the vertex colour */
static void
setUnlit(void)
{
	xtcLight l;
	int i;

	xtcSetAmbient(0.0f, 0.0f, 0.0f);
	memset(&l, 0, sizeof(l));
	l.type = XTC_LIGHT_DIRECT;
	l.enabled = 0;
	for(i = 0; i < 8; i++)
		xtcSetLight(i, &l);
}

static void
updateCam(void)
{
	Mat4 proj, view;
	/* the level's own size: Gnasty's Loot is ten times Sunny Flight */
	const float zoomspeed = camSpeed;
	const float orbitspeed = 0.04f;

	if(joy.btns & JOY_SQUARE) camZoom(zoomspeed);
	if(joy.btns & JOY_CROSS) camZoom(-zoomspeed);
	camOrbit(joy.lx*orbitspeed, -joy.ly*orbitspeed);
	camTurn(-joy.rx*orbitspeed, -joy.ry*orbitspeed);
	if(joy.btns & JOY_UP) camDolly(zoomspeed);
	if(joy.btns & JOY_DOWN) camDolly(-zoomspeed);
	if(joy.btns & JOY_RIGHT) camPan(zoomspeed, 0.0f);
	if(joy.btns & JOY_LEFT) camPan(-zoomspeed, 0.0f);

	/* a level is big: the far plane has to hold the sky dome too */
	proj = m4persp(70.0f, 4.0f/3.0f, 1.0f, camFar);
	xtcSetProjectionMatrix(&proj);
	view = m4invOrtho(m4lookat(camPos, camTarget, camUp));
	xtcSetViewMatrix(&view);
}

static void
drawFrame(void)
{
	Mat4 m;

	if(joy.press & JOY_L1) drawSky = !drawSky;
	if(joy.press & JOY_R1) drawWorld = !drawWorld;
	if(joy.press & JOY_L2) {
		textured = !textured;
		applyTextured();
	}
	if(joy.press & JOY_TRIANGLE)
		printf("spyro: cam pos %g %g %g target %g %g %g\n",
			camPos.x, camPos.y, camPos.z,
			camTarget.x, camTarget.y, camTarget.z);

	/* the level: by hand, or by itself on the tour */
	if(joy.press & JOY_START)
		loadLevel((level+1) % NUMLEVELS);
	else if(joy.press & JOY_SELECT)
		loadLevel((level+NUMLEVELS-1) % NUMLEVELS);
	else if(tour && skelCount() - levelTime > tour*SKEL_TICKS_PER_SEC)
		loadLevel((level+1) % NUMLEVELS);

	updateCam();

	xtcEnable(XTC_CLIPPING);
	if(textured) {
		xtcEnable(XTC_TEXTURE);
		xtcTexFunc(XTC_RGB, XTC_MODULATE);
		/* the game's vertex colours are in the PS1's convention, 128
		 * is 1.0 and 255 twice that, which is the GS's own, so they
		 * go to the GS as they are; the default scale maps 255 to
		 * 128 for GL style colours and halved everything.  Alpha
		 * stays in the GS convention */
		xtcColorMod cm;
		xtcGetColorMod(&cm);
		cm.scaleTex = vec4(1.0f, 1.0f, 1.0f, 128.0f/255.0f);
		xtcSetColorMod(&cm);
	}
	setUnlit();

	/* the sky first: it sits around the camera and leaves the depth
	 * buffer alone, so the world can be drawn straight over it */
	if(sky && drawSky) {
		xtcDisable(XTC_DEPTH_TEST);
		xtcDepthMask(1);
		m = m4translate(camPos.x, camPos.y, camPos.z);
		xtcSetWorldMatrix(&m);
		xModelDraw(sky, 0);
		xtcDepthMask(0);
		xtcEnable(XTC_DEPTH_TEST);
	}

	if(world && drawWorld) {
		xtcSetWorldMatrix(&identity);
		xModelDraw(world, 0);
	}

	/* a rough frame rate: count the lines in a run of known length;
	 * and where the previous frame went, in COP0 ticks */
	if(skelFrameCount % 100 == 0)
		printf("spyro: frame %d cpu %u gs %u vsync %u\n", skelFrameCount,
			skelTimeCpu, skelTimeGs, skelTimeVsync);
	if(traceBinds) {
		/* the frame after the first: everything loaded, nothing special */
		if(skelFrameCount == 2) {
			printf("bind frame\n");
			xtcTextureBindHook = bindHook;
		} else if(skelFrameCount == 3) {
			printf("bind end\n");
			xtcTextureBindHook = nil;
		}
	}
}

int
main(int argc, char *argv[])
{
	int startLevel = 0;

	/* boot switches, for measuring: pcsx2run.sh -s notex, or dsedb
	 * run elf notex.  any argv entry may hold one -- pcsx2's -gameargs
	 * replaces the whole vector, so it can arrive as argv[0] */
	for(int i = 0; i < argc; i++) {
		if(strcmp(argv[i], "notex") == 0) textured = 0;
		if(strcmp(argv[i], "trace") == 0) traceBinds = 1;
		if(strcmp(argv[i], "nosky") == 0) drawSky = 0;
		if(strcmp(argv[i], "noworld") == 0) drawWorld = 0;
		if(strcmp(argv[i], "tour") == 0) tour = 3;
		if(strncmp(argv[i], "tour=", 5) == 0) tour = atoi(argv[i]+5);
		if(strncmp(argv[i], "level=", 6) == 0) startLevel = atoi(argv[i]+6);
	}
	/* the EE's counter wraps in 14 seconds, so that is the longest
	 * interval this way of measuring can tell apart */
	if(tour > 14) tour = 14;
	if(startLevel < 0 || startLevel >= NUMLEVELS) {
		printf("spyro: no level %d, there are %d\n", startLevel, NUMLEVELS);
		startLevel = 0;
	}

	skelInit(SCREEN_WIDTH, SCREEN_HEIGHT);
	/* the GL sketch clears to this, so the two pictures compare */
	skelClearColor(160, 160, 160);

	printf("spyro: loading, heap %u of %u\n", memHeapUsed(), memHeapSize());
	loadLevel(startLevel);
	printf("spyro: ready\n");

	skelRun(drawFrame);

	return 0;
}
