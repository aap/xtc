#include "xtc.h"
#include "xmodel.h"

void DrawAxes(float scale = 1.0f);
extern xtcMaterial material;

void
xMeshDraw(xMesh *xm, xSkeleton *skel)
{
	if(xm->skin && skel) {
		xtcSetShader(xtcGetSkinShader());

		static mat4 matrices[64];
		xSkin *skin = xm->skin;
		assert(skin->numBones < 64);
		for(int i = 0; i < skin->numBones; i++)
			matrices[i] = skel->matrices[i] * skin->invMatrices[i];
		xtcSetBoneMatrices(&matrices[0], skin->numBones);
	} else
		xtcSetShader(xtcGetDefaultShader());

	if(xm->material->tex)
		xtcSetTexture(0, xm->material->tex->tex);
	else
		xtcSetTexture(0, nil);
	xtcSetMaterial(&xm->material->material);
	xtcPrimListDraw(xm->prims);
}

void
xSkeletonDraw(xSkeleton *s)
{
	int stack[64];
	int sp = 0;

	xtcSetShader(xtcGetDefaultShader());
	xtcSetTexture(0, nil);
	xtcSetMaterial(&material);

	int parent = 0;
	for(int i = 1; i < s->numBones; i++) {
		xBone *b = &s->bones[i];
		vec4 p1 = s->matrices[parent] * vec4(0.0f, 0.0f, 0.0f, 1.0f);
		vec4 p2 = s->matrices[i] * vec4(0.0f, 0.0f, 0.0f, 1.0f);
		xtcBegin(XTC_LINELIST);
			xtcColor(255, 255, 255, 255);
			xtcVertex3(p1.x, p1.y, p1.z);
			xtcVertex3(p2.x, p2.y, p2.z);
		xtcEnd();

		if(b->flag & 1) stack[sp++] = parent;
		parent = i;
		if(b->flag & 2) parent = stack[--sp];
	}
}

void
xNodeDraw(xNode *n, xSkeleton *skel)
{
	mat4 oldmat = xtcGetWorldMatrix();
	xtcSetWorldMatrix(oldmat * n->localMatrix);
	if(!n->hidden)
		for(int i = 0; i < n->numMeshes; i++)
			xMeshDraw(n->meshes[i], skel);
	for(xNode *child = n->child; child; child = child->next)
		xNodeDraw(child, skel);
	xtcSetWorldMatrix(oldmat);
}

void
xNodeDebugDraw(xNode *n, int flags)
{
	// important to draw before setting worldMat
	// because skeleton matrix and node matrix can add up
	// TODO: figure out what we actually want to do here
	if(flags & Dbg_DrawSkeleton && n->skel)
		xSkeletonDraw(n->skel);

	mat4 oldmat = xtcGetWorldMatrix();
	xtcSetWorldMatrix(oldmat * n->localMatrix);
	if(flags & Dbg_DrawNodes)
		DrawAxes(10.0f);
	for(xNode *child = n->child; child; child = child->next)
		xNodeDebugDraw(child, flags);
	xtcSetWorldMatrix(oldmat);
}

void
xModelDraw(xModel *m, int flags)
{
	xNodeDraw(m->root, m->skel);
	if(flags) {
		xtcDisable(XTC_DEPTH_TEST);
		xNodeDebugDraw(m->root, flags);
		xtcEnable(XTC_DEPTH_TEST);
	}
}
