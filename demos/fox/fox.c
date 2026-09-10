/*
 * demos/fox -- the fox, idling.
 *
 * A small state machine over the clips of samples/fox/fox.xan.  Four
 * poses -- standing, sitting, lying, and a low alert crouch -- each
 * with a handful of idle loops, and the Trans_* clips of the set to
 * get from one pose to another.  The fox plays an idle or two, moves
 * to another pose, plays idles there, and so on; which pose it starts
 * in, which idle, and how far into it are all random, so a PCSX2
 * window that comes up shows it doing something different each time.
 *
 * Only clips that keep the fox where it is are in the set.  Walk, Run,
 * Trot, Sneak and friends animate the RigRoot *node* (not the pelvis
 * bone: RigRoot is a plain node above the skeleton, channel id -1),
 * and that node's translation carries the whole fox forward -- Run
 * moves it 236 units in 0.43 s, well out of frame.  Those clips belong
 * to the locomotion states the pad will drive later; the machine below
 * is shaped for them (states with entry and exit transitions and a
 * chooser that says where to go next) but the chooser is random.
 *
 * The animation comes from build/chk/fox_anim.chk: 19 clips written by
 * tools/xan2dsm.lua as assembler and linked into a memory image, which
 * loads in one read and a pass over the fixup table.  The same clips
 * as text sit next to it as build/chk/fox_anim.xan and are the
 * fallback; they parse a key at a time and take three hundred times
 * as long (9 ms against 2.8 s under PCSX2).  `make chunks' builds both.
 *
 * Pad: dpad left/right steps through the clips by hand (and stops the
 * machine), dpad up/down hands it back, cross pauses, square spins.
 */

#include "xtci.h"
#include "joy.h"
#include "xmodel.h"
#include "fox.h"
#include "skel.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

/* an orbit camera around the origin, the fox is kept there by
 * foxRecenter; left stick orbits, right stick Y zooms.  Loading the fox
 * frames it */
static float camTheta = 0.785f;
static float camPhi = 0.44f;
static float camDist = 4.7f;

static void
foxCamera(void)
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

#define FOX_DT (1.0f/60.0f)
/* how long the pose at a cut takes to fade into the new clip */
#define FOX_FADE 0.15f

/*
 * The clip set, by name.  Names and not indices because the chunk and
 * the text file need not agree on an order, and because a set that is
 * missing a clip then simply loses it: an unresolved idle drops out of
 * its pose, an unresolved transition drops out of the graph.
 */

enum { FOX_STAND, FOX_SIT, FOX_LIE, FOX_ALERT, FOX_NUMPOSES };
enum { FOX_MAXIDLE = 4 };

STRUCT(FoxPose) {
	const char *name;
	int dwell;	/* at most this many idles before moving on */
	const char *idles[FOX_MAXIDLE];
};

STRUCT(FoxEdge) {
	int from, to;
	const char *clip;
};

static const FoxPose foxPoses[FOX_NUMPOSES] = {
	/* standing: quiet breathing, a scratch at the belly, a yawn,
	 * and a long sniff along the ground */
	{ "standing", 3, { "A4_Stand_Breathing_01", "A1_Stand_Idle_02",
	                   "A3_Stand_Idle_01", "A2_Stand_Eating_01" } },
	/* sitting: quiet, a yawn, and one that looks left and right */
	{ "sitting",  3, { "Sitting_Breathing_01", "Sitting_Idle_01",
	                   "Sitting_Idle_02", nil } },
	/* lying: flat out, and one that raises the head to look around */
	{ "lying",    3, { "Lying_Breathing_01", "Lying_Idle_01",
	                   "Lying_Idle_02", nil } },
	/* the tense low crouch, ears back.  One idle and out again: it is
	 * a beat, not somewhere to settle */
	{ "alert",    1, { "A5_StandAngry_Breathing_01", nil, nil, nil } },
};

static const FoxEdge foxEdges[] = {
	{ FOX_STAND, FOX_SIT,   "Trans_Stand_to_Sitting" },
	{ FOX_SIT,   FOX_STAND, "Trans_Sitting_to_Stand" },
	{ FOX_STAND, FOX_LIE,   "Trans_Stand_to_Lying" },
	{ FOX_LIE,   FOX_STAND, "Trans_Lying_to_Stand" },
	{ FOX_SIT,   FOX_LIE,   "Trans_Sitting_to_Lying" },
	{ FOX_LIE,   FOX_SIT,   "Trans_Lying_to_Sitting" },
	{ FOX_STAND, FOX_ALERT, "Trans_Stand_to_StandAngry" },
	{ FOX_ALERT, FOX_STAND, "Trans_StandAngry_to_Stand" },
};
#define FOX_NUMEDGES ((int)nelem(foxEdges))

/* the clips those names found */
static xAnimation *foxIdles[FOX_NUMPOSES][FOX_MAXIDLE];
static int foxNumIdles[FOX_NUMPOSES];
static xAnimation *foxEdgeClips[FOX_NUMEDGES];

static xModel *fox;
static xAnimList *foxAnims;
static xAnimPlayer *foxPlayer;
/* what the camera aims at: the bind pose's bounding sphere centre, and
 * where the skeleton's middle sat in the bind pose, so a pose that
 * moves the fox can be followed by the difference (see foxRecenter) */
static Vec3 foxCenter, foxBindCenter, foxBindMiddle;
static float foxAngle;
static int foxLoaded;
static int foxSpin;
static int foxPlaying = 1;

/* the machine: the pose we are in, the transition we are playing (or
 * -1), and how many more idles to play before choosing again */
static int foxPose;
static int foxEdge = -1;
static int foxIdlesLeft;
static int foxManual;		/* the dpad took over */
static int foxClip;		/* which clip, for the dpad */

/* the cross fade: the model space pose at the moment of a cut */
static Mat4 *foxFadePose;
static float foxFadeLeft;

/*
 * Random.  xorshift32, so the sequence does not depend on which libc
 * the build got, and cheap enough to not think about.
 */
static uint32 foxSeed = 2463534242u;

static uint32
foxRand(void)
{
	foxSeed ^= foxSeed << 13;
	foxSeed ^= foxSeed >> 17;
	foxSeed ^= foxSeed << 5;
	return foxSeed;
}

/* 0 .. n-1 */
static int
foxRandN(int n)
{
	return n > 1 ? (int)((foxRand() >> 8) % (uint32)n) : 0;
}

/* 0 .. 1 */
static float
foxRandF(void)
{
	return (foxRand() >> 8)*(1.0f/16777216.0f);
}

/*
 * Something that differs from run to run.  Three sources, xor'd:
 *
 * The console's clock, which is the only one that really varies under
 * an emulator -- PCSX2 sets its RTC from the host's, and its cycle
 * counts are otherwise reproducible to the tick, so two runs of the
 * same ELF get the same everything else.  BCD fields, used as they
 * come; it is a seed, not a date.
 *
 * COP0 register 9, the EE's free running cycle counter (CPU/2), which
 * on real hardware picks up every bit of jitter between power on and
 * here -- disc spin-up, the time a hand took to reach a button.
 *
 * And the pad, so that holding something down changes it as well.
 */

/* libcdvd's RTC.  SDK 3.0 declares neither of these in any EE header
 * (they are in libcdvd.a all the same) and freesce's libcdvd.h
 * declares both, so rather than diverge on the include, here are the
 * prototypes; both libraries satisfy them.  This file is compiled as
 * C++ by the freesce build, hence the linkage. */
STRUCT(foxClock) { uint8 stat, second, minute, hour, pad, day, month, year; };
#ifdef __cplusplus
extern "C" {
#endif
int sceCdInit(int mode);
int sceCdReadClock(foxClock *rtc);
#ifdef __cplusplus
}
#endif
#define FOX_CDINIT_NODISC 1	/* SCECdINoD: bring cdvd up, don't look for a disc */

static void
foxSeedRandom(void)
{
	foxClock rtc;
	uint32 count, clock;

	memset(&rtc, 0, sizeof(rtc));
	clock = 0;
	sceCdInit(FOX_CDINIT_NODISC);
	if(sceCdReadClock(&rtc))
		clock = ((uint32)rtc.second << 24) | ((uint32)rtc.minute << 16) |
			((uint32)rtc.hour << 8) | rtc.day;
	__asm__ __volatile__("mfc0 %0, $9" : "=r" (count));

	foxSeed = clock*2654435761u ^ count ^ ((uint32)joy.btns << 13);
	if(foxSeed == 0)
		foxSeed = 1;
	/* xorshift needs a few turns to spread a seed out */
	foxRand();
	foxRand();
	foxRand();
	printf("fox: seed %u (clock %02x:%02x:%02x day %02x, count %u)\n",
		foxSeed, rtc.hour, rtc.minute, rtc.second, rtc.day, count);
}

/*
 * The clips
 */

static xAnimation*
foxFindAnim(const char *name)
{
	int i;

	if(foxAnims == nil)
		return nil;
	for(i = 0; i < foxAnims->numAnims; i++)
		if(strcmp(foxAnims->anims[i].name, name) == 0)
			return &foxAnims->anims[i];
	printf("fox: no clip %s\n", name);
	return nil;
}

static void
foxResolveClips(void)
{
	int i, j;

	for(i = 0; i < FOX_NUMPOSES; i++) {
		foxNumIdles[i] = 0;
		for(j = 0; j < FOX_MAXIDLE; j++) {
			xAnimation *a;
			if(foxPoses[i].idles[j] == nil)
				continue;
			a = foxFindAnim(foxPoses[i].idles[j]);
			if(a)
				foxIdles[i][foxNumIdles[i]++] = a;
		}
	}
	for(i = 0; i < FOX_NUMEDGES; i++)
		foxEdgeClips[i] = foxFindAnim(foxEdges[i].clip);
}

/*
 * Playing
 */

/* keep the pose that is on screen, so the next clip can fade out of it */
static void
foxCapturePose(void)
{
	if(foxFadePose == nil || fox->skel == nil || foxPlayer->anim == nil)
		return;
	memcpy(foxFadePose, fox->skel->matrices, fox->skel->numBones*sizeof(Mat4));
	foxFadeLeft = FOX_FADE;
}

static void
foxPlay(xAnimation *a, float t)
{
	if(a == nil)
		return;
	foxCapturePose();
	xAnimPlayerSetAnim(foxPlayer, a);
	foxPlayer->time = t;
	printf("fox: %s\n", a->name);
}

/* one of the current pose's idles, not the one that just played */
static void
foxPlayIdle(void)
{
	xAnimation *a;
	int n, i;

	n = foxNumIdles[foxPose];
	if(n == 0)
		return;
	i = foxRandN(n);
	if(n > 1 && foxIdles[foxPose][i] == foxPlayer->anim)
		i = (i + 1) % n;
	a = foxIdles[foxPose][i];
	foxPlay(a, 0.0f);
}

static void
foxEnterPose(int pose)
{
	foxPose = pose;
	foxEdge = -1;
	/* an idle runs three to seven seconds, so a couple of them is a
	 * comfortable while to stay put */
	foxIdlesLeft = 1 + foxRandN(foxPoses[pose].dwell);
	foxPlayIdle();
}

static int
foxFindEdge(int from, int to)
{
	int i;

	for(i = 0; i < FOX_NUMEDGES; i++)
		if(foxEdges[i].from == from && foxEdges[i].to == to && foxEdgeClips[i])
			return i;
	return -1;
}

/*
 * Where to go next.  One of the poses a Trans_ clip leads to, or this
 * one again -- a random walk over the graph above.  When the pad drives
 * the fox this is the function that will answer "walking", and the rest
 * of the machine does not have to change: a locomotion state is a pose
 * with its own idles (the walk cycle) and its own edges (the
 * Trans_Stand_to_Walk clips).
 */
static int
foxChooseNextPose(void)
{
	int outs[FOX_NUMEDGES];
	int n, i;

	/* staying put in a pose with a single idle would only play the
	 * same clip again, so only linger where there is a choice */
	if(foxNumIdles[foxPose] > 1 && foxRandN(100) < 30)
		return foxPose;
	n = 0;
	for(i = 0; i < FOX_NUMEDGES; i++)
		if(foxEdges[i].from == foxPose && foxEdgeClips[i])
			outs[n++] = foxEdges[i].to;
	return n ? outs[foxRandN(n)] : foxPose;
}

/* the clip that was playing has run out */
static void
foxAdvance(void)
{
	int next, e;

	if(foxEdge >= 0) {
		/* a transition finished: we are in the new pose */
		foxEnterPose(foxEdges[foxEdge].to);
		return;
	}
	if(--foxIdlesLeft > 0) {
		foxPlayIdle();
		return;
	}
	next = foxChooseNextPose();
	e = next == foxPose ? -1 : foxFindEdge(foxPose, next);
	if(e < 0) {
		foxEnterPose(foxPose);
		return;
	}
	foxEdge = e;
	foxPlay(foxEdgeClips[e], 0.0f);
}

/* the dpad, stepping through the whole list by hand */
static void
foxSelectClip(int n)
{
	if(foxAnims == nil || foxAnims->numAnims == 0)
		return;
	foxManual = 1;
	foxClip = (n + foxAnims->numAnims) % foxAnims->numAnims;
	foxPlay(&foxAnims->anims[foxClip], 0.0f);
}

/*
 * Framing.  The bounding sphere is the bind pose's, and the bind pose
 * is a standing fox: lying down drops the body and pushes it a body
 * length forward, which walks it out of the frame the sphere chose.
 * So the point the camera aims at follows the middle of the skeleton,
 * by the amount that middle has moved since the bind pose -- and
 * slowly, over about a second, so that sitting down still reads as
 * sitting down instead of the world sliding to keep up.  It is also
 * what will keep a walking fox on screen later.
 */
static Vec3
foxSkelMiddle(void)
{
	xSkeleton *s;
	Vec3 c;
	int i;

	s = fox->skel;
	if(s == nil || s->numBones == 0)
		return foxBindMiddle;
	c = vec3(0.0f, 0.0f, 0.0f);
	for(i = 0; i < s->numBones; i++)
		c = v3add(c, v4tov3(s->matrices[i].w));
	return v3scale(1.0f/s->numBones, c);
}

static void
foxRecenter(float rate)
{
	Vec3 want;

	want = v3add(foxBindCenter, v3sub(foxSkelMiddle(), foxBindMiddle));
	foxCenter = v3add(foxCenter, v3scale(rate, v3sub(want, foxCenter)));
}

/*
 * Lights: a portrait rig, a warm key from the front left and above and
 * a cool fill from behind on the right, the other slots off.
 */
static void
foxLights(void)
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

/*
 * Loading
 */

void
foxInit(void)
{
	Vec3 center;
	float radius;

	if(foxLoaded)
		return;
	foxLoaded = 1;
	texpath = "host:./samples/fox/textures_ps2";
	/* the chunk the assembler linked (make chunks), else the text */
	fox = loadXModel("host:./build/chk/fox.chk");
	if(fox == nil)
		fox = loadXModel("host:./samples/fox/fox.xm");
	if(fox == nil) {
		printf("fox: no model\n");
		return;
	}
	buildXModel(fox);
	xModelBoundingSphere(fox, &center, &radius);
	/* a three quarter view from a little above, the fox centred */
	foxBindCenter = center;
	foxCenter = center;
	if(fox->skel) {
		/* the bind pose in model space, to measure poses against */
		xSkeletonResetMatrices(fox->skel);
		xSkeletonUpdateMatrices(fox->skel);
		foxBindMiddle = foxSkelMiddle();
	}
	camDist = 2.1f*radius;
	camPhi = 0.25f;
	foxAngle = 1.3f;
	printf("fox: %d meshes, %d bones, radius %g\n", fox->numMeshes,
		fox->skel ? fox->skel->numBones : 0, radius);

	foxAnims = loadXAnimList("host:./build/chk/fox_anim.chk");
	if(foxAnims == nil)
		foxAnims = loadXAnimList("host:./build/chk/fox_anim.xan");
	if(foxAnims == nil) {
		printf("fox: no animation\n");
		return;
	}
	printf("fox: %d animations\n", foxAnims->numAnims);
	foxPlayer = xAnimPlayerCreate(fox);
	foxResolveClips();
	if(fox->skel)
		foxFadePose = (Mat4*)emalloc(fox->skel->numBones*sizeof(Mat4));

	foxSeedRandom();
	foxEnterPose(foxRandN(FOX_NUMPOSES));
	/* and not at the top of the clip either */
	if(foxPlayer->anim)
		foxPlayer->time = foxPlayer->anim->duration*foxRandF();
	/* frame that pose right away rather than sliding into it */
	xAnimPlayerApply(foxPlayer);
	foxRecenter(1.0f);
}

/*
 * The frame
 */

/* m = (1-f)*m + f*old, componentwise.  Blending model space matrices
 * shortens a bone by the cosine of half the angle between the two
 * poses, which over a tenth of a second between poses that nearly
 * match -- the machine cuts at loop points -- is not visible.  It is
 * the dpad stepping to an unrelated clip that this softens. */
static void
foxBlendPose(float f)
{
	Mat4 *m, *o;
	float *a, *b;
	int i, k;

	if(foxFadePose == nil || fox->skel == nil)
		return;
	for(i = 0; i < fox->skel->numBones; i++) {
		m = &fox->skel->matrices[i];
		o = &foxFadePose[i];
		a = (float*)m;
		b = (float*)o;
		for(k = 0; k < 16; k++)
			a[k] += f*(b[k] - a[k]);
	}
}

void
foxFrame(void)
{
	Mat4 world;
	float prev;

	if(!foxLoaded)
		foxInit();
	foxCamera();
	if(fox == nil)
		return;

	if(joy.press & JOY_RIGHT) foxSelectClip(foxClip+1);
	if(joy.press & JOY_LEFT) foxSelectClip(foxClip-1);
	if(joy.press & (JOY_UP|JOY_DOWN)) {
		/* back to the machine, somewhere new */
		foxManual = 0;
		if(foxPlayer)
			foxEnterPose(foxRandN(FOX_NUMPOSES));
	}
	if(joy.press & JOY_CROSS) foxPlaying = !foxPlaying;
	if(joy.press & JOY_SQUARE) foxSpin = !foxSpin;

	xtcEnable(XTC_CLIPPING);
	xtcEnable(XTC_TEXTURE);
	xtcTexFunc(XTC_RGBA, XTC_MODULATE);
	foxLights();

	if(foxPlayer) {
		if(foxPlaying) {
			prev = foxPlayer->time;
			xAnimPlayerAddTime(foxPlayer, FOX_DT);
			/* the player wraps, so time going backwards is the
			 * clip having run out */
			if(foxPlayer->time < prev && !foxManual)
				foxAdvance();
		}
		xAnimPlayerApply(foxPlayer);
		if(foxFadeLeft > 0.0f) {
			foxBlendPose(foxFadeLeft/FOX_FADE);
			if(foxPlaying)
				foxFadeLeft -= FOX_DT;
		}
		foxRecenter(0.02f);
	}
	if(foxSpin)
		foxAngle += 0.3f*FOX_DT;
	world = m4mul(m4rotZ(foxAngle), m4translate(-foxCenter.x, -foxCenter.y, -foxCenter.z));
	xtcSetWorldMatrix(&world);
	xModelDraw(fox, 0);
}


int
main(int argc, char *argv[])
{
	(void)argc; (void)argv;
	skelInit(640, 448);
	foxInit();
	skelRun(foxFrame);
	return 0;
}
