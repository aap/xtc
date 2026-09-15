/*
 * quattest -- the quaternion routines of quat.vu, written for VU1, run
 * as VU0 macro code next to the same math on the EE core, on random
 * inputs.  quatvu0.h is generated from quat.vu by host/genvu0.lua, so
 * what runs is the text of the routines, not a translation by hand.
 *
 *	tools/pcsx2run.sh -t 6 -g 'quat' quattest_freesce.elf
 *
 * Prints the first few trials side by side, then the largest difference
 * over many, and the EE ticks per call of each side.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "skel.h"
#include "quatvu0.h"

typedef struct { float v[4]; } __attribute__((aligned(16))) V;

/* ---- the EE side: q = (x, y, z, w), w the real part ---- */

static void ee_qmul(V *r, const V *a, const V *b)
{
	const float *p = a->v, *q = b->v;
	r->v[0] = p[3]*q[0] + q[3]*p[0] + (p[1]*q[2] - p[2]*q[1]);
	r->v[1] = p[3]*q[1] + q[3]*p[1] + (p[2]*q[0] - p[0]*q[2]);
	r->v[2] = p[3]*q[2] + q[3]*p[2] + (p[0]*q[1] - p[1]*q[0]);
	r->v[3] = p[3]*q[3] - (p[0]*q[0] + p[1]*q[1] + p[2]*q[2]);
}
static void ee_qconj(V *r, const V *a) { r->v[0] = -a->v[0]; r->v[1] = -a->v[1]; r->v[2] = -a->v[2]; r->v[3] = a->v[3]; }
/* q u q*, u a vector */
static void ee_qrot(V *r, const V *q, const V *u)
{
	V uq, c, t;
	uq.v[0] = u->v[0]; uq.v[1] = u->v[1]; uq.v[2] = u->v[2]; uq.v[3] = 0.0f;
	ee_qconj(&c, q);
	ee_qmul(&t, q, &uq);
	ee_qmul(r, &t, &c);
}
/* rows are the images of the axes: what the VU routine writes into MX MY MZ */
static void ee_qmat(V *mx, V *my, V *mz, const V *q)
{
	float x = q->v[0], y = q->v[1], z = q->v[2], w = q->v[3];
	mx->v[0] = 1.0f - 2.0f*(y*y + z*z); mx->v[1] = 2.0f*(x*y + z*w);       mx->v[2] = 2.0f*(x*z - y*w);       mx->v[3] = 0.0f;
	my->v[0] = 2.0f*(x*y - z*w);       my->v[1] = 1.0f - 2.0f*(x*x + z*z); my->v[2] = 2.0f*(y*z + x*w);       my->v[3] = 0.0f;
	mz->v[0] = 2.0f*(x*z + y*w);       mz->v[1] = 2.0f*(y*z - x*w);       mz->v[2] = 1.0f - 2.0f*(x*x + y*y); mz->v[3] = 0.0f;
}
/* the translation of the dual quaternion A + eps B: 2 vec(B A*) */
static void dqtrans(V *t, const V *a, const V *b)
{
	V c, p;
	ee_qconj(&c, a);
	ee_qmul(&p, b, &c);
	t->v[0] = 2.0f*p.v[0]; t->v[1] = 2.0f*p.v[1]; t->v[2] = 2.0f*p.v[2]; t->v[3] = 1.0f;
}
/* (A0 + eps A1)(B0 + eps B1) = A0 B0 + eps (A0 B1 + A1 B0) */
static void dqmul(V *r0, V *r1, const V *a0, const V *a1, const V *b0, const V *b1)
{
	V p, q;
	ee_qmul(r0, a0, b0);
	ee_qmul(&p, a0, b1);
	ee_qmul(&q, a1, b0);
	for(int i = 0; i < 4; i++) r1->v[i] = p.v[i] + q.v[i];
}
/* rotate, then translate scaled by u.w */
static void dqapply(V *r, const V *a, const V *b, const V *u)
{
	V t;
	ee_qrot(r, a, u);
	dqtrans(&t, a, b);
	for(int i = 0; i < 3; i++) r->v[i] += u->v[3]*t.v[i];
	r->v[3] = u->v[3];
}

/* ---- random inputs ---- */

static unsigned int seed = 12345;
static float frand(void) { seed = seed*1664525u + 1013904223u; return (float)(seed>>8)/(float)(1<<24)*2.0f - 1.0f; }
static void rvec(V *v, float w) { v->v[0] = frand(); v->v[1] = frand(); v->v[2] = frand(); v->v[3] = w; }
static void rndquat(V *q)
{
	float n;
	rvec(q, frand());
	n = sqrtf(q->v[0]*q->v[0] + q->v[1]*q->v[1] + q->v[2]*q->v[2] + q->v[3]*q->v[3]);
	for(int i = 0; i < 4; i++) q->v[i] /= n;
}
/* the dual quaternion of rotation q then translation t: B = (t/2) q */
static void rdual(V *a, V *b)
{
	V t;
	rndquat(a);
	rvec(&t, 0.0f);
	ee_qmul(b, &t, a);
	for(int i = 0; i < 4; i++) b->v[i] *= 0.5f;
}

/* ---- the comparison ---- */

static float maxerr[8];
static const char *names[8];
static int nnames;

static float fmax2(float a, float b) { return a > b ? a : b; }
static float diff(const V *a, const V *b, int n)
{
	float m = 0.0f;
	for(int i = 0; i < n; i++) { float d = fabsf(a->v[i] - b->v[i]); if(d > m) m = d; }
	return m;
}
static void show(const char *what, const V *ee, const V *vu, int n)
{
	printf("quat   %-14s ee  %9.5f %9.5f %9.5f %9.5f\n", what, ee->v[0], ee->v[1], ee->v[2], ee->v[3]);
	printf("quat   %-14s vu0 %9.5f %9.5f %9.5f %9.5f   diff %.2e\n", "", vu->v[0], vu->v[1], vu->v[2], vu->v[3], diff(ee, vu, n));
}
static void note(int k, const char *name, float e)
{
	if(k >= nnames) { names[nnames++] = name; }
	if(e > maxerr[k]) maxerr[k] = e;
}

static void trial(int print)
{
	V a, b, u, r, s, two, a0, a1, b0, b1, r0, r1, s0, s1, mx, my, mz, mw, nx, ny, nz, nw;
	two.v[0] = two.v[1] = two.v[2] = 2.0f; two.v[3] = 0.0f;
	rndquat(&a); rndquat(&b); rvec(&u, 1.0f);

	ee_qmul(&r, &a, &b); vu0_QuatMult(a.v, b.v, s.v);
	if(print) show("QuatMult", &r, &s, 4);
	note(0, "QuatMult", diff(&r, &s, 4));

	ee_qrot(&r, &a, &u); vu0_QuatSandwich(a.v, u.v, s.v);
	if(print) show("QuatSandwich", &r, &s, 3);
	note(1, "QuatSandwich", diff(&r, &s, 3));

	ee_qmat(&mx, &my, &mz, &a); vu0_QuatMatrix(a.v, two.v, nx.v, ny.v, nz.v);
	if(print) { show("QuatMatrix x", &mx, &nx, 4); show("QuatMatrix y", &my, &ny, 4); show("QuatMatrix z", &mz, &nz, 4); }
	note(2, "QuatMatrix", fmax2(diff(&mx, &nx, 4), fmax2(diff(&my, &ny, 4), diff(&mz, &nz, 4))));

	rdual(&a0, &a1); rdual(&b0, &b1);
	dqmul(&r0, &r1, &a0, &a1, &b0, &b1); vu0_DQuatMult(a0.v, a1.v, b0.v, b1.v, s0.v, s1.v);
	if(print) { show("DQuatMult real", &r0, &s0, 4); show("DQuatMult dual", &r1, &s1, 4); }
	note(3, "DQuatMult", fmax2(diff(&r0, &s0, 4), diff(&r1, &s1, 4)));

	dqapply(&r, &a0, &a1, &u); vu0_DQuatSandwich(a0.v, a1.v, u.v, s.v);
	if(print) show("DQuatSandwich", &r, &s, 4);
	note(4, "DQuatSandwich", diff(&r, &s, 4));

	ee_qmat(&mx, &my, &mz, &a0); dqtrans(&mw, &a0, &a1);
	vu0_DQuatMatrix(a0.v, a1.v, two.v, nx.v, ny.v, nz.v, nw.v);
	if(print) { show("DQuatMatrix x", &mx, &nx, 4); show("DQuatMatrix w", &mw, &nw, 4); }
	note(5, "DQuatMatrix", fmax2(fmax2(diff(&mx, &nx, 4), diff(&my, &ny, 4)), fmax2(diff(&mz, &nz, 4), diff(&mw, &nw, 4))));
}

static void timing(void)
{
	V a, b, r;
	unsigned int t0, t1, t2;
	int i;
	rndquat(&a); rndquat(&b);
	t0 = skelCount();
	for(i = 0; i < 10000; i++) ee_qmul(&r, &a, &b);
	t1 = skelCount();
	for(i = 0; i < 10000; i++) vu0_QuatMult(a.v, b.v, r.v);
	t2 = skelCount();
	printf("quat   QuatMult ticks per call: ee %u, vu0 macro %u (loads and stores included)\n",
		(t1-t0)/10000, (t2-t1)/10000);
	t0 = skelCount();
	for(i = 0; i < 10000; i++) ee_qrot(&r, &a, &b);
	t1 = skelCount();
	for(i = 0; i < 10000; i++) vu0_QuatSandwich(a.v, b.v, r.v);
	t2 = skelCount();
	printf("quat   QuatSandwich ticks per call: ee %u, vu0 macro %u\n", (t1-t0)/10000, (t2-t1)/10000);
}

static void frame(void) { }

int
main(int argc, char *argv[])
{
	int i;
	(void)argc; (void)argv;
	printf("quat   VU0 macro mode against the EE, quat.vu\n");
	for(i = 0; i < 3; i++) { printf("quat   -- trial %d\n", i); trial(1); }
	for(; i < 1000; i++) trial(0);
	printf("quat   -- largest difference over %d trials\n", i);
	for(i = 0; i < nnames; i++)
		printf("quat   %-14s %.3e %s\n", names[i], maxerr[i], maxerr[i] < 1e-5f ? "ok" : "WRONG");
	timing();
	printf("quat   done\n");
	skelInit(640, 448);
	skelRun(frame);
	return 0;
}
