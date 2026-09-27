/*
 * skinview -- a skinned model and its clips from build/chk/models/,
 * where Makefile.assets puts whatever local/models/ holds (.xm and
 * .xan, from the GL sketch's converter: ./xtc model.glb -save
 * ../local/models/NAME).  Textures from local/models/textures_ps2.
 *
 *	tools/pcsx2run.sh -s cow skinview_freesce.elf     (the boot argument is the model)
 *	tools/toolrun.sh skinview.elf q:cow               (on the TOOL, the quantized chunk)
 *
 * Prefixes on the name: q: loads NAME_Q.chk, the quantized model (the
 * clips are the same file); pause: starts paused at t=0, so a frame is
 * deterministic and two builds can be compared pixel by pixel
 * (tools/shotdiff.py).  They combine: pause:q:cow.
 *
 * Sticks orbit and zoom, dpad left/right steps the clips, cross pauses,
 * square spins.  The fox demo's camera and lights.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "skel.h"
#include "joy.h"
#include "xmodel.h"

static char modelName[64] = "cow";
static xModel *model;
static xAnimList *anims;
static xAnimPlayer *player;
static int clip, playing = 1, spin, quant, dqmode;
static float angle = 1.3f;
static Vec3 center;
static float camTheta = 0.7f, camPhi = 0.25f, camDist = 3.0f;

#define DT (1.0f/60.0f)

static void
camera(void)
{
	Mat4 proj, view;
	Vec3 pos;

	camTheta += 0.05f*joy.lx;
	camPhi -= 0.05f*joy.ly;
	if(camPhi > 1.5f) camPhi = 1.5f;
	if(camPhi < -1.5f) camPhi = -1.5f;
	camDist += 0.05f*camDist*joy.ry;
	if(camDist < 0.05f) camDist = 0.05f;

	proj = m4persp(70.0f, 4.0f/3.0f, 0.02f*camDist, 50.0f*camDist);
	xtcSetProjectionMatrix(&proj);
	pos = vec3(camDist*cosf(camPhi)*cosf(camTheta),
	           camDist*cosf(camPhi)*sinf(camTheta),
	           camDist*sinf(camPhi));
	view = m4invOrtho(m4lookat(pos, vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 0.0f, 1.0f)));
	xtcSetViewMatrix(&view);
}

static void
lights(void)
{
	xtcLight l;
	int i;

	xtcSetAmbient(0.22f, 0.22f, 0.25f);
	memset(&l, 0, sizeof(l));
	l.type = XTC_LIGHT_DIRECT;
	l.enabled = 1;
	l.color = vec4(0.95f, 0.88f, 0.75f, 1.0f);
	l.direction = v3normalized(vec3(-1.0f, -1.2f, -1.4f));
	xtcSetLight(0, &l);
	l.color = vec4(0.30f, 0.36f, 0.50f, 1.0f);
	l.direction = v3normalized(vec3(1.0f, 0.6f, -0.3f));
	xtcSetLight(1, &l);
	l.enabled = 0;
	for(i = 2; i < 8; i++)
		xtcSetLight(i, &l);
}

static void
selectClip(int n)
{
	if(anims == nil || anims->numAnims == 0)
		return;
	clip = (n + anims->numAnims) % anims->numAnims;
	xAnimPlayerSetAnim(player, &anims->anims[clip]);
	player->time = 0.0f;
	printf("skinview: clip %d/%d %s (%g s)\n", clip+1, anims->numAnims,
		anims->anims[clip].name, anims->anims[clip].duration);
}

static void
load(void)
{
	char path[128];
	float radius;

	texpath = "host:./local/models/textures_ps2";
	sprintf(path, "host:./build/chk/models/%s%s.chk", modelName, dqmode ? "_dq" : quant ? "_Q" : "");
	model = loadXModel(path);
	if(model == nil) {
		printf("skinview: no model %s\n", path);
		return;
	}
	buildXModel(model);
	xModelBoundingSphere(model, &center, &radius);
	camDist = 2.1f*radius;
	printf("skinview: %s: %d meshes, %d bones, radius %g\n", modelName,
		model->numMeshes, model->skel ? model->skel->numBones : 0, radius);
	if(model->skel) {
		xSkeletonResetMatrices(model->skel);
		xSkeletonUpdateMatrices(model->skel);
	}
	sprintf(path, "host:./build/chk/models/%s_anim.chk", modelName);
	anims = loadXAnimList(path);
	if(anims == nil) {
		printf("skinview: no animation %s\n", path);
		return;
	}
	player = xAnimPlayerCreate(model);
	selectClip(0);
}

static void
frame(void)
{
	Mat4 world;

	camera();
	if(model == nil)
		return;
	if(joy.press & JOY_RIGHT) selectClip(clip+1);
	if(joy.press & JOY_LEFT) selectClip(clip-1);
	if(joy.press & JOY_CROSS) playing = !playing;
	if(joy.press & JOY_SQUARE) spin = !spin;

	xtcEnable(XTC_CLIPPING);
	xtcEnable(XTC_TEXTURE);
	xtcTexFunc(XTC_RGBA, XTC_MODULATE);
	/* the textures keep their alpha (the cow's eye shine is a
	 * transparent layer over the eye); opaque ones are unaffected, and
	 * the alpha test keeps fully transparent texels off the depth buffer */
	xtcEnable(XTC_BLEND);
	xtcBlendFuncSrcDst(XTC_BLEND_SRCALPHA, XTC_BLEND_INVSRCALPHA);
	lights();

	if(player) {
		if(playing)
			xAnimPlayerAddTime(player, DT);
		xAnimPlayerApply(player);
	}
	if(spin)
		angle += 0.3f*DT;
	world = m4mul(m4rotZ(angle), m4translate(-center.x, -center.y, -center.z));
	xtcSetWorldMatrix(&world);
	xModelDraw(model, 0);
}

int
main(int argc, char *argv[])
{
	/* the model: any argument that is not a switch (pcsx2run.sh -s NAME
	 * arrives as argv[0]) */
	for(int i = 0; i < argc; i++) {
		const char *a = argv[i];
		for(;;) {
			if(strncmp(a, "pause:", 6) == 0) { playing = 0; a += 6; }
			else if(strncmp(a, "q:", 2) == 0) { quant = 1; a += 2; }
			else if(strncmp(a, "dq:", 3) == 0) { dqmode = 1; a += 3; }
			else break;
		}
		if(a[0] != '-' && a[0] != '/' && strlen(a) < sizeof(modelName))
			strcpy(modelName, a);
	}
	skelInit(640, 448);
	load();
	skelRun(frame);
	return 0;
}
