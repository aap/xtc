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

// bind pose: every bone's local transform from its node
void
xSkeletonResetMatrices(xSkeleton *s)
{
	for(int i = 0; i < s->numBones; i++) {
		Mat4 m = s->bones[i].node->localMatrix;
		if(s->rep == XFORM_DQ)
			s->local[i].dq = dfrommat(m);	// offline-style extraction; a
							// DQ pose player will build these
							// straight from rotor + place
		else
			s->local[i].m = m;
	}
}

// the transform of the model above the skeleton's root bone: where the
// skeleton is anchored, and the world matrix a skinned mesh is placed
// with.  The skeleton composes only from the root bone down, so its
// matrices stay rigid -- the model's placement (with its scale) lives
// here, in the world matrix, not in the skin.
Mat4
xSkeletonBaseMatrix(xSkeleton *s)
{
	Mat4 m = m4ident();
	for(xNode *n = s->bones[0].node->parent; n; n = n->parent)
		m = n->localMatrix * m;
	return m;
}

// compose local matrices into matrices relative to the skeleton base
void
xSkeletonUpdateMatrices(xSkeleton *s)
{
	// RenderWare convention: the last leaf pops once more than was
	// pushed, so seed the stack with the root
	int stack[64];
	int sp = 0;
	stack[sp++] = 0;

	int parent = 0;
	s->composed[0] = s->local[0];		// root: composed local
	for(int i = 1; i < s->numBones; i++) {
		xBone *b = &s->bones[i];
		if(s->rep == XFORM_DQ)
			s->composed[i].dq = dmul(s->composed[parent].dq, s->local[i].dq);
		else
			s->composed[i].m = s->composed[parent].m * s->local[i].m;
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
	free(p->bone);
	p->anim = a;
	p->time = 0.0f;
	p->targets = nil;
	p->bone = nil;
	if(a == nil)
		return;

	p->bone = (int*)emalloc(a->numChannels * sizeof(int));
	p->targets = (Mat4**)emalloc(a->numChannels * sizeof(Mat4*));
	for(int i = 0; i < a->numChannels; i++) {
		xAnimChannel *ch = &a->channels[i];
		int bi = ch->id;
		if(bi < 0 && mdl->skel)
			bi = findBone(mdl->skel, ch->name);
		if(bi >= 0 && mdl->skel && bi < mdl->skel->numBones) {
			p->bone[i] = bi;
			p->targets[i] = nil;
			continue;
		}
		{
			p->bone[i] = -1;
			xNode *n = findXNode(mdl->root, ch->name);
			if(n == nil)
				printf("warning: no node for channel %s\n", ch->name);
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
			if(p->bone[i] >= 0) {
				// a skeleton bone: sample the pose (a matrix; the
				// bind for missing tracks is the bone's node local)
				// and store it into local in the skeleton's rep
				int bi = p->bone[i];
				Mat4 posed = sampleChannel(c, p->time, skel->bones[bi].node->localMatrix);
				if(skel->rep == XFORM_DQ)
					skel->local[bi].dq = dfrommat(posed);
				else
					skel->local[bi].m = posed;
			} else if(p->targets[i]) {
				// a plain node keeps its matrix as the bind
				Mat4 *target = p->targets[i];
				*target = sampleChannel(c, p->time, *target);
			}
		}
	}
	if(skel)
		xSkeletonUpdateMatrices(skel);
}
