/*
 * Skeleton and animation runtime.
 */

#include "xtc.h"
#include "xmodel.h"


int
findBone(xSkeleton *skel, const char *str)
{
	for(int i = 0; i < skel->numBones; i++) {
		xBone *b = &skel->bones[i];
		if(strcmp(b->node->name, str) == 0)
			return i;
	}
	return -1;
}

/*
 * Skeleton
 */

// bind pose: every bone's local matrix from its node
void
xSkeletonResetMatrices(xSkeleton *s)
{
	for(int i = 0; i < s->numBones; i++)
		s->matrices[i] = s->bones[i].node->localMatrix;
}

// the transform of everything above the root bone,
// so skeleton matrices end up in model space
static Mat4
ancestorMatrix(xNode *n)
{
	Mat4 m = m4ident();
	for(n = n->parent; n; n = n->parent)
		m = n->localMatrix * m;
	return m;
}

// compose local matrices into model space matrices
void
xSkeletonUpdateMatrices(xSkeleton *s)
{
	// RenderWare convention: the last leaf pops once more than was
	// pushed, so seed the stack with the root
	int stack[64];
	int sp = 0;
	stack[sp++] = 0;

	s->matrices[0] = ancestorMatrix(s->bones[0].node) * s->matrices[0];
	int parent = 0;
	for(int i = 1; i < s->numBones; i++) {
		xBone *b = &s->bones[i];
		s->matrices[i] = s->matrices[parent] * s->matrices[i];
		if(b->flag & 1) stack[sp++] = parent;
		parent = i;
		if(b->flag & 2) parent = stack[--sp];
	}
}

/*
 * Animation player
 */

xAnimPlayer*
xAnimPlayerCreate(xModel *mdl)
{
	xAnimPlayer *p = (xAnimPlayer*)emalloc(sizeof(xAnimPlayer));
	p->model = mdl;
	return p;
}

void
xAnimPlayerSetAnim(xAnimPlayer *p, xAnimation *a)
{
	xModel *mdl = p->model;

	free(p->targets);
	p->anim = a;
	p->time = 0.0f;
	p->targets = nil;
	if(a == nil)
		return;

	p->targets = (Mat4**)emalloc(a->numChannels * sizeof(Mat4*));
	for(int i = 0; i < a->numChannels; i++) {
		xAnimChannel *ch = &a->channels[i];
		int bi = ch->id;
		if(bi < 0 && mdl->skel)
			bi = findBone(mdl->skel, ch->name);
		if(bi >= 0 && mdl->skel && bi < mdl->skel->numBones)
			p->targets[i] = &mdl->skel->matrices[bi];
		else {
			xNode *n = findXNode(mdl->root, ch->name);
			if(n == nil)
				fprintf(stderr, "warning: no node for channel %s\n", ch->name);
			p->targets[i] = n ? &n->localMatrix : nil;
		}
	}
}

void
xAnimPlayerAddTime(xAnimPlayer *p, float t)
{
	if(p->anim == nil)
		return;
	p->time += t;
	float d = p->anim->duration;
	if(d <= 0.0f) {
		p->time = 0.0f;
		return;
	}
	while(p->time >= d) p->time -= d;
	while(p->time < 0.0f) p->time += d;
}

// index of the key at or before t, and the fraction to the next
template <typename Key>
static int
findKey(Key *keys, int n, float t, float *frac)
{
	*frac = 0.0f;
	if(t <= keys[0].time)
		return 0;
	if(t >= keys[n-1].time)
		return n-1;
	int i = 0;
	while(t >= keys[i+1].time) i++;
	float dt = keys[i+1].time - keys[i].time;
	if(dt > 0.0f)
		*frac = (t - keys[i].time)/dt;
	return i;
}

static Quat
sampleRot(xRotKey *keys, int n, float t)
{
	float f;
	int i = findKey(keys, n, t, &f);
	if(f == 0.0f)
		return keys[i].rot;
	return qslerp(keys[i].rot, keys[i+1].rot, f);
}

static Vec3
sampleVec(xVecKey *keys, int n, float t)
{
	float f;
	int i = findKey(keys, n, t, &f);
	if(f == 0.0f)
		return keys[i].v;
	return v3lerp(keys[i].v, keys[i+1].v, f);
}

// evaluate a channel. tracks the animation doesn't have
// are taken from the bind pose matrix
static Mat4
sampleChannel(xAnimChannel *c, float t, const Mat4 &bind)
{
	Mat4 m;
	if(c->numTransKeys)
		m = m4translatev(sampleVec(c->transKeys, c->numTransKeys, t));
	else
		m = m4translatev(v4tov3(bind.w));
	if(c->numRotKeys)
		m *= qtomat4(sampleRot(c->rotKeys, c->numRotKeys, t));
	else {
		// rotation and scale together
		Mat4 rs = bind;
		rs.w = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		return m * rs;
	}
	if(c->numScaleKeys)
		m *= m4scalev(sampleVec(c->scaleKeys, c->numScaleKeys, t));
	return m;
}

void
xAnimPlayerApply(xAnimPlayer *p)
{
	xModel *mdl = p->model;
	xAnimation *a = p->anim;
	xSkeleton *skel = mdl->skel;

	if(skel)
		xSkeletonResetMatrices(skel);
	if(a) {
		for(int i = 0; i < a->numChannels; i++) {
			xAnimChannel *c = &a->channels[i];
			Mat4 *target = p->targets[i];
			if(target == nil)
				continue;
			// the bind pose is what's in the target right now:
			// bones were just reset, plain nodes keep their matrix
			*target = sampleChannel(c, p->time, *target);
		}
	}
	if(skel)
		xSkeletonUpdateMatrices(skel);
}
