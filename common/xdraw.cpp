/*
 * Drawing xModels, plus some debug views.
 */

#include "xtc.h"
#include "xmodel.h"
#ifdef XTC_GL
#include "glad/glad.h"
#endif


xtcStdMaterial
DefaultMaterial(void)
{
	xtcStdMaterial mat;
	mat.emissive = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	mat.ambient = vec4(0.2f, 0.2f, 0.2f, 1.0f);
	mat.diffuse = vec4(0.8f, 0.8f, 0.8f, 1.0f);
	mat.specular = vec4(0.0f, 0.0f, 0.0f, 0.0f);	// w is the power
	return mat;
}

// vertex colours only
static xtcStdMaterial*
debugMaterial(void)
{
	static xtcStdMaterial mat;
	static bool init;
	if(!init) {
		mat = DefaultMaterial();
		mat.ambient = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		mat.diffuse = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		init = true;
	}
	return &mat;
}

// solid white
static xtcStdMaterial*
wireMaterial(void)
{
	static xtcStdMaterial mat;
	static bool init;
	if(!init) {
		mat = DefaultMaterial();
		mat.ambient = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		mat.diffuse = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		mat.specular = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		mat.emissive = vec4(1.0f, 1.0f, 1.0f, 1.0f);
		init = true;
	}
	return &mat;
}

// a small axes gizmo at the current world matrix, for the node views
static void
xDrawAxes(float scale)
{
	xtcSetPipeline(defaultPipeline);
	xtcSetTexture(nil);
	xtcSetStdMaterial(debugMaterial());
	xtcSetColorMaterial(XTC_EMISSIVE);
	xtcBegin(XTC_LINELIST);
		xtcColor(255, 0, 0, 255);
		xtcVertex(0.0f, 0.0f, 0.0f);
		xtcVertex(scale, 0.0f, 0.0f);
		xtcColor(0, 255, 0, 255);
		xtcVertex(0.0f, 0.0f, 0.0f);
		xtcVertex(0.0f, scale, 0.0f);
		xtcColor(0, 0, 255, 255);
		xtcVertex(0.0f, 0.0f, 0.0f);
		xtcVertex(0.0f, 0.0f, scale);
	xtcEnd();
}

// the world matrix of the model being drawn, and the world matrix skinned
// meshes are placed with: modelMatrix times the skeleton's base transform.
// The skeleton is rigid and relative to that base, so the model's placement
// and scale live in skinMatrix, not in the skin (design B).
static Mat4 modelMatrix;
static Mat4 skinMatrix;

// a stopgap until the material names its pipeline: when set, unskinned
// meshes are drawn with this pipeline instead of the default one
xtcPipeline *xDrawPipeline;

static void
xMeshSelectShader(xMesh *xm, xSkeleton *skel)
{
	if(xm->skin && skel) {
		xtcSetPipeline(skinPipeline);

		static Mat4 matrices[64];
		xSkin *skin = xm->skin;
		assert(skin->numBones <= 64);
		for(int i = 0; i < skin->numBones; i++)
			matrices[i] = skel->composed[i].m * skin->invbind[i].m;
		xtcSetBoneMatrices(&matrices[0], skin->numBones);
	} else
		xtcSetPipeline(xDrawPipeline ? xDrawPipeline : defaultPipeline);
}

// unskinned vertex positions as points
static void
xVerticesDraw(xMesh *xm, xSkeleton *skel)
{
	xGeometry *geo = xm->geo;
	if(geo == nil)
		return;

	xMeshSelectShader(xm, skel);
	xtcSetTexture(nil);
	xtcSetStdMaterial(debugMaterial());
	xtcSetColorMaterial(XTC_EMISSIVE);
	xtcPointSize(4.0f);
	xtcBegin(XTC_POINTS);
	for(int i = 0; i < geo->numVertices; i++) {
		xVertex *v = &geo->vertices[i];
		if(xm->skin) {
			uint8 *is = &xm->skin->indices[4*i];
			float *ws = &xm->skin->weights[4*i];
			xtcIndices(is[0], is[1], is[2], is[3]);
			xtcWeights(ws[0], ws[1], ws[2], ws[3]);
		}
		if(i & 8)
			xtcColor(155, 0, 40, 255);
		else
			xtcColor(255, 255, 0, 255);
		xtcVertex(v->vtx[0], v->vtx[1], v->vtx[2]);
	}
	xtcEnd();
}

#ifdef XTC_GL
// draw the prim list again as lines on top
static void
xWireDraw(xMesh *xm, xSkeleton *skel)
{
	xMeshSelectShader(xm, skel);
	xtcSetTexture(nil);
	xtcSetStdMaterial(wireMaterial());
	xtcSetColorMaterial(0);
	glEnable(GL_POLYGON_OFFSET_LINE);
	glPolygonOffset(1.5f, 0.0f);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	xtcPrimListDraw(xm->prims);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDisable(GL_POLYGON_OFFSET_LINE);
}
#endif

static void
xMeshDraw(xMesh *xm, xSkeleton *skel, int flags)
{
	bool skinned = xm->skin && skel;
	Mat4 nodeMatrix = xtcGetWorldMatrix();
	if(skinned)
		xtcSetWorldMatrix(skinMatrix);

	xMeshSelectShader(xm, skel);
	if(xm->material->tex)
		xtcSetTexture(xm->material->tex->tex);
	else
		xtcSetTexture(nil);
	xtcSetStdMaterial(&xm->material->material);
	xtcSetColorMaterial(xm->material->colorMaterial);
#ifdef XTC_GL
	if(flags & Dbg_DrawWire) {
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(2.0f, 0.0f);
	}
	xtcPrimListDraw(xm->prims);
	glDisable(GL_POLYGON_OFFSET_FILL);
	if(flags & Dbg_DrawWire)
		xWireDraw(xm, skel);
#else
	xtcPrimListDraw(xm->prims);
#endif

	if(flags & Dbg_DrawPoints)
		xVerticesDraw(xm, skel);

	if(skinned)
		xtcSetWorldMatrix(nodeMatrix);
}

// in model space
static void
xSkeletonDraw(xSkeleton *s)
{
	int stack[64];
	int sp = 0;
	stack[sp++] = 0;	// see xSkeletonUpdateMatrices

	xtcSetPipeline(defaultPipeline);
	xtcSetTexture(nil);
	xtcSetStdMaterial(debugMaterial());
	xtcSetColorMaterial(XTC_EMISSIVE);

	int parent = 0;
	xtcBegin(XTC_LINELIST);
	for(int i = 1; i < s->numBones; i++) {
		xBone *b = &s->bones[i];
		Vec4 p1 = s->composed[parent].m * vec4(0.0f, 0.0f, 0.0f, 1.0f);
		Vec4 p2 = s->composed[i].m * vec4(0.0f, 0.0f, 0.0f, 1.0f);
		xtcColor(255, 255, 255, 255);
		xtcVertex(p1.x, p1.y, p1.z);
		xtcColor(255, 128, 0, 255);
		xtcVertex(p2.x, p2.y, p2.z);

		if(b->flag & 1) stack[sp++] = parent;
		parent = i;
		if(b->flag & 2) parent = stack[--sp];
	}
	xtcEnd();
}

// PROVISIONAL: a node that is a bone takes the skeleton's animated
// matrix (model space after xAnimPlayerApply), so a mesh parented under
// a bone -- the cow's eyes under its head -- follows the animation
// instead of the bind pose.  Whether nodes should be driven this way at
// all is still open.
static int
boneOfNode(xSkeleton *skel, xNode *n)
{
	if(skel)
		for(int i = 0; i < skel->numBones; i++)
			if(skel->bones[i].node == n)
				return i;
	return -1;
}

static void
xNodeDraw(xNode *n, xSkeleton *skel, int flags)
{
	Mat4 oldmat = xtcGetWorldMatrix();
	int b = boneOfNode(skel, n);
	static int dbg = 0;
	if(0) { dbg++; Mat4 m = b >= 0 ? skel->composed[b].m : n->localMatrix; printf("xdraw: node %s bone %d meshes %d  w %g %g %g  x %g %g %g\n", n->name ? n->name : "?", b, n->numMeshes, m.w.x, m.w.y, m.w.z, m.x.x, m.x.y, m.x.z); }
	if(b >= 0)
		xtcSetWorldMatrix(skinMatrix * skel->composed[b].m);
	else
		xtcSetWorldMatrix(oldmat * n->localMatrix);
	if(!n->hidden)
		for(int i = 0; i < n->numMeshes; i++)
			xMeshDraw(n->meshes[i], skel, flags);
	for(xNode *child = n->child; child; child = child->next)
		xNodeDraw(child, skel, flags);
	xtcSetWorldMatrix(oldmat);
}

static void
xNodeDebugDraw(xNode *n, int flags)
{
	Mat4 oldmat = xtcGetWorldMatrix();
	xtcSetWorldMatrix(oldmat * n->localMatrix);
	if(flags & Dbg_DrawNodes)
		xDrawAxes(0.1f);
	for(xNode *child = n->child; child; child = child->next)
		xNodeDebugDraw(child, flags);
	xtcSetWorldMatrix(oldmat);
}

void
xModelDraw(xModel *m, int flags)
{
	modelMatrix = xtcGetWorldMatrix();
	skinMatrix = m->skel ? modelMatrix * xSkeletonBaseMatrix(m->skel) : modelMatrix;
	xNodeDraw(m->root, m->skel, flags);
	if(flags & (Dbg_DrawNodes | Dbg_DrawSkeleton)) {
		xtcDisable(XTC_DEPTH_TEST);
		if(flags & Dbg_DrawSkeleton && m->skel)
			xSkeletonDraw(m->skel);
		xNodeDebugDraw(m->root, flags);
		xtcEnable(XTC_DEPTH_TEST);
	}
}
