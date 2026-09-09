#include "xtc.h"
#include "glad/glad.h"
#include "lodepng/lodepng.h"

// the backend does its uniform maths with glm, the API speaks xmath
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
using glm::value_ptr;
#define GLM_MAT4(m) (*(const glm::mat4*)(m))

#include <GL/gl.h>
#include <stdio.h>

GLenum primMap[] = {
	[XTC_POINTS] =  GL_POINTS,
	[XTC_LINESTRIP] =  GL_LINE_STRIP,
	[XTC_LINELIST] = GL_LINES,
	[XTC_TRISTRIP] =  GL_TRIANGLE_STRIP,
	[XTC_TRILIST] = GL_TRIANGLES
};

const xtcRGBA white = { 255, 255, 255, 255 };



xtcTexture *textures[8];
struct {
	// global
	glm::mat4 proj;
	glm::mat4 view;
	glm::vec3 eyePos;

	// object
	glm::mat4 worldMat;
	glm::mat4 normalMat;

	// light
	glm::vec4 globalAmbient;
	xtcLight lights[8];

	// mesh
	xtcMaterial material;

	// skin
	glm::mat4 boneMatrices[64];
} uniformState;




#define UNIFORMS \
	X(u_world) \
	X(u_normal) \
	X(u_view) \
	X(u_proj) \
	X(u_eyePos) \
	X(u_matColorSelector) \
	X(u_matAmbient) \
	X(u_matDiffuse) \
	X(u_matSpecular) \
	X(u_matEmissive) \
	X(u_matShininess) \
	X(u_ambient) \
	X(u_lightDiffuse) \
	X(u_lightSpecular) \
	X(u_lightDirection) \
	X(u_lightMat) \
	X(u_lightColMat) \
	X(u_specDir) \
	X(u_specCol) \
	X(u_boneMatrices)

struct Program
{
	i32 program;
#define X(uniform) i32 uniform;
UNIFORMS
#undef X
	void Use(void);
};
static Program *curProg;

void
Program::Use(void)
{
	curProg = this;
	glUseProgram(curProg->program);
}


void
printlog(GLuint object)
{
        GLint log_length;
        char *log;

        if (glIsShader(object))
                glGetShaderiv(object, GL_INFO_LOG_LENGTH, &log_length);
        else if (glIsProgram(object))
                glGetProgramiv(object, GL_INFO_LOG_LENGTH, &log_length);
        else{
                fprintf(stderr, "printlog: Not a shader or a program\n");
                return;
        }

        log = (char*) malloc(log_length);
        if(glIsShader(object))
                glGetShaderInfoLog(object, log_length, NULL, log);
        else if(glIsProgram(object))
                glGetProgramInfoLog(object, log_length, NULL, log);
        fprintf(stderr, "%s", log);
        free(log);
}

// prefix, if given, goes in front of the source: the #version line
// and the pipeline's #defines
GLint
compileshader(GLenum type, const char *prefix, const char *src)
{
	GLint shader, success;
	const char *srcs[2] = { prefix, src };

	shader = glCreateShader(type);
	if(prefix)
		glShaderSource(shader, 2, srcs, NULL);
	else
		glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if(!success){
		fprintf(stderr, "Error in shader\n");
		printlog(shader);
		return -1;
	}
	return shader;
}

GLint
linkprogram(GLint vs, GLint fs)
{
	GLint program, success;

	program = glCreateProgram();

	glAttachShader(program, vs);
	glAttachShader(program, fs);
	glLinkProgram(program);
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	if(!success){
		fprintf(stderr, "glLinkProgram:");
		printlog(program);
		return -1;
	}
	return program;
}

#include "inc/shader.vert.inc"
#include "inc/skin.vert.inc"
#include "inc/lit.vert.inc"
#include "inc/shader.frag.inc"
#include "inc/tex.frag.inc"


static xtcPipeline *curPipe;

void
xtcSetPipeline(xtcPipeline *pipe)
{
	curPipe = pipe;
}

/*
 * Programs.  Every pipeline has an untextured and a textured program
 * with the same vertex shader, compiled on first use.
 */

struct Programs
{
	Program plain, tex;
	int ready;
};

static void
buildPrograms(Programs *p, const char *prefix, const char *vsrc)
{
	GLint vs = compileshader(GL_VERTEX_SHADER, prefix, vsrc);
	GLint fs = compileshader(GL_FRAGMENT_SHADER, nil, shader_frag_src);
	GLint fs_tex = compileshader(GL_FRAGMENT_SHADER, nil, tex_frag_src);
	p->plain.program = linkprogram(vs, fs);
	p->tex.program = linkprogram(vs, fs_tex);

#define X(uniform) p->plain.uniform = glGetUniformLocation(p->plain.program, #uniform); \
		p->tex.uniform = glGetUniformLocation(p->tex.program, #uniform);
	UNIFORMS
#undef X
	p->ready = 1;
}

static void
usePrograms(Programs *p, const char *prefix, const char *vsrc)
{
	if(!p->ready)
		buildPrograms(p, prefix, vsrc);
	if(textures[0])
		p->tex.Use();
	else
		p->plain.Use();
}

/*
 * Uniforms
 */

static void
uploadCameraUniforms(void)
{
	glUniformMatrix4fv(curProg->u_view, 1, GL_FALSE, value_ptr(uniformState.view));
	glUniformMatrix4fv(curProg->u_proj, 1, GL_FALSE, value_ptr(uniformState.proj));
	const glm::mat4 &v = uniformState.view;
	uniformState.eyePos.x = -(v[3][0]*v[0][0] + v[3][1]*v[0][1] + v[3][2]*v[0][2]);
	uniformState.eyePos.y = -(v[3][0]*v[1][0] + v[3][1]*v[1][1] + v[3][2]*v[1][2]);
	uniformState.eyePos.z = -(v[3][0]*v[2][0] + v[3][1]*v[2][1] + v[3][2]*v[2][2]);
	glUniform3fv(curProg->u_eyePos, 1, value_ptr(uniformState.eyePos));
}

static void
uploadObjectUniforms(void)
{
	glUniformMatrix4fv(curProg->u_world, 1, GL_FALSE, value_ptr(uniformState.worldMat));
	glUniformMatrix4fv(curProg->u_normal, 1, GL_FALSE, value_ptr(uniformState.normalMat));
}

static void
uploadMaterialUniforms(void)
{
	const xtcMaterial &mat = uniformState.material;
	glUniform4fv(curProg->u_matColorSelector, 1, &mat.colorSelector.x);
	glUniform4fv(curProg->u_matAmbient, 1, &mat.ambient.x);
	glUniform4fv(curProg->u_matDiffuse, 1, &mat.diffuse.x);
	glUniform4fv(curProg->u_matSpecular, 1, &mat.specular.x);
	glUniform4fv(curProg->u_matEmissive, 1, &mat.emissive.x);
	glUniform1f(curProg->u_matShininess, mat.shininess);
}

// the one hardcoded light of the default and skin pipelines
static void
uploadOneLightUniforms(void)
{
	glUniform4fv(curProg->u_ambient, 1, value_ptr(uniformState.globalAmbient));
	xtcLight &l = uniformState.lights[0];
	if(l.enabled) {
		glUniform4fv(curProg->u_lightDiffuse, 1, &l.color.x);
		glUniform4fv(curProg->u_lightSpecular, 1, &l.specColor.x);
		glUniform3fv(curProg->u_lightDirection, 1, &l.direction.x);
	} else {
		glm::vec4 zero(0.0f);
		glUniform4fv(curProg->u_lightDiffuse, 1, value_ptr(zero));
		glUniform4fv(curProg->u_lightSpecular, 1, value_ptr(zero));
		glUniform3fv(curProg->u_lightDirection, 1, value_ptr(zero));
	}
}

// the lit pipelines: the enabled directional lights in slot order,
// transformed into object space and packed into matrices, see xtc.h.
// this is what the PS2 upload would do on the EE.
static void
uploadLitLightUniforms(int nlights)
{
	glm::mat4 lightMat[2], colMat[2];
	glm::vec3 specDir(0.0f);
	glm::vec4 specCol(0.0f);
	int n;

	// TODO: like the PS2 upload this assumes an orthogonal world matrix
	glm::mat4 invWorld = glm::inverse(uniformState.worldMat);
	// towards the (infinite) viewer, in object space
	const glm::mat4 &v = uniformState.view;
	glm::vec3 eye = glm::normalize(glm::vec3(invWorld * glm::vec4(v[0][2], v[1][2], v[2][2], 0.0f)));

	lightMat[0] = lightMat[1] = glm::mat4(0.0f);
	colMat[0] = colMat[1] = glm::mat4(0.0f);
	n = 0;
	for(u32 i = 0; i < nelem(uniformState.lights) && n < nlights; i++) {
		xtcLight *l = &uniformState.lights[i];
		if(!l->enabled || l->type != XTC_LIGHT_DIRECT)
			continue;
		// towards the light
		glm::vec3 d = glm::normalize(glm::vec3(invWorld *
			glm::vec4(-l->direction.x, -l->direction.y, -l->direction.z, 0.0f)));
		int m = n/4, r = n%4;
		// row r of the light matrix, column r of the colour matrix
		lightMat[m][0][r] = d.x;
		lightMat[m][1][r] = d.y;
		lightMat[m][2][r] = d.z;
		colMat[m][r] = glm::vec4(l->color.x, l->color.y, l->color.z, 0.0f);
		if(n == 0) {
			specDir = glm::normalize(d + eye);
			specCol = glm::vec4(l->specColor.x, l->specColor.y, l->specColor.z, 0.0f);
		}
		n++;
	}

	glUniform4fv(curProg->u_ambient, 1, value_ptr(uniformState.globalAmbient));
	glUniformMatrix4fv(curProg->u_lightMat, nlights/4, GL_FALSE, value_ptr(lightMat[0]));
	glUniformMatrix4fv(curProg->u_lightColMat, nlights/4, GL_FALSE, value_ptr(colMat[0]));
	glUniform3fv(curProg->u_specDir, 1, value_ptr(specDir));
	glUniform4fv(curProg->u_specCol, 1, value_ptr(specCol));
}

/*
 * Pipelines
 */

static Programs defProgs, skinProgs, lit4Progs, lit8Progs;

static void
uploadDefault(void)
{
	usePrograms(&defProgs, nil, shader_vert_src);
	uploadCameraUniforms();
	uploadObjectUniforms();
	uploadMaterialUniforms();
	uploadOneLightUniforms();
}

static void
uploadSkin(void)
{
	usePrograms(&skinProgs, nil, skin_vert_src);
	uploadCameraUniforms();
	uploadObjectUniforms();
	uploadMaterialUniforms();
	uploadOneLightUniforms();
	glUniformMatrix4fv(curProg->u_boneMatrices, 64, GL_FALSE, value_ptr(uniformState.boneMatrices[0]));
}

static void
uploadLit4(void)
{
	usePrograms(&lit4Progs, "#version 460\n#define NLIGHTS 4\n", lit_vert_src);
	uploadCameraUniforms();
	uploadObjectUniforms();
	uploadMaterialUniforms();
	uploadLitLightUniforms(4);
}

static void
uploadLit8(void)
{
	usePrograms(&lit8Progs, "#version 460\n#define NLIGHTS 8\n", lit_vert_src);
	uploadCameraUniforms();
	uploadObjectUniforms();
	uploadMaterialUniforms();
	uploadLitLightUniforms(8);
}

static xtcPipeline defaultPipe = { uploadDefault };
static xtcPipeline skinPipe = { uploadSkin };
static xtcPipeline lit4Pipe = { uploadLit4 };
static xtcPipeline lit8Pipe = { uploadLit8 };
xtcPipeline *defaultPipeline = &defaultPipe;
xtcPipeline *skinPipeline = &skinPipe;
xtcPipeline *lit4Pipeline = &lit4Pipe;
xtcPipeline *lit8Pipeline = &lit8Pipe;


void
xtcSetProjectionMatrix(const Mat4 *proj)
{
	uniformState.proj = GLM_MAT4(proj);
}

void
xtcSetViewMatrix(const Mat4 *view)
{
	uniformState.view = GLM_MAT4(view);
}

void
xtcSetWorldMatrix(const Mat4 *world)
{
	uniformState.worldMat = GLM_MAT4(world);
	uniformState.normalMat = glm::inverse(glm::transpose(uniformState.worldMat));
}
void
xtcSetWorldMatrix(const Mat4 &world)
{
	xtcSetWorldMatrix(&world);
}

Mat4
xtcGetWorldMatrix(void)
{
	return *(const Mat4*)&uniformState.worldMat;
}

void
xtcSetAmbient(int r, int g, int b)
{
	uniformState.globalAmbient = glm::vec4(r, g, b, 255)/255.0f;
}

void
xtcSetLight(int n, const xtcLight *light)
{
	if(n < 0 || (u32)n >= nelem(uniformState.lights))
		return;
	uniformState.lights[n] = *light;
}

void
xtcSetMaterial(const xtcMaterial *mat)
{
	uniformState.material = *mat;
}


void
xtcSetTextureN(int n, xtcTexture *tex)
{
	if(n < 0 || (u32)n >= nelem(textures))
		return;
	if(tex)
		glBindTextureUnit(n, tex->tex);
	else
		glBindTextureUnit(n, 0);
	textures[n] = tex;
}

void
xtcSetBoneMatrices(const Mat4 *matrices, int n)
{
	if(n > 64) n = 64;
	for(int i = 0; i < n; i++)
		uniformState.boneMatrices[i] = GLM_MAT4(&matrices[i]);
}


void
xtcViewport(int x, int y, int width, int height)
{
	glViewport(x, y, width, height);
}

//void xtcDepthRange(int near, int far);
void
xtcScissor(int x, int y, int width, int height)
{
	glScissor(x, y, width, height);
}

void
xtcClearColor(int r, int g, int b, int a)
{
	const float s = 1/255.0f;
	glClearColor(r*s, g*s, b*s, a*s);
}

void
xtcClearDepth(u32 z)
{
	// oh well
	glClearDepth((float)z/0xFFFFFF);
}

void
xtcClear(int mask)
{
	int bits = 0;
	if(mask & XTC_COLORBUF) bits |= GL_COLOR_BUFFER_BIT;
	if(mask & XTC_DEPTHBUF) bits |= GL_DEPTH_BUFFER_BIT;
	glClear(bits);
}




int stateMap[] = {
	GL_DEPTH_TEST,	// XTC_DEPTH_TEST
	-1,	// XTC_ALPHA_TEST,
	GL_BLEND,	// XTC_BLEND,
	-1,	//XTC_FOG,
	-1,	//XTC_TEXTURE,
	-1,	//XTC_CLIPPING
};

void
xtcEnable(xtceState state)
{
	glEnable(stateMap[state]);
}

void
xtcDisable(xtceState state)
{
	glDisable(stateMap[state]);
}

/*
enum xtceDepthFunc {
	XTC_DEPTH_NEVER,
	XTC_DEPTH_ALWAYS,
	XTC_DEPTH_GEQUAL,
	XTC_DEPTH_GREATER
};
*/

void xtcDepthFunc(xtceDepthFunc func);

/*
enum xtceAlphaFunc {
	XTC_AFUNC_NEVER,
	XTC_AFUNC_ALWAYS,
	XTC_AFUNC_LESS,
	XTC_AFUNC_LEQUAL,
	XTC_AFUNC_EQUAL,
	XTC_AFUNC_GEQUAL,
	XTC_AFUNC_GREATER,
	XTC_AFUNC_NOTEQUAL
};

enum xtceAlphaFail {
	XTC_AFAIL_KEEP,
	XTC_AFAIL_FB_ONLY,
	XTC_AFAIL_ZB_ONLY,
	XTC_AFAIL_RGB_ONLY
};
*/

void xtcAlphaFunc(xtceAlphaFunc func, int ref, xtceAlphaFail fail);

/*
enum xtceAlpha {
	XTC_ALPHA_SRC,
	XTC_ALPHA_DST,
	XTC_ALPHA_ZERO,
	XTC_ALPHA_FIX = XTC_ALPHA_ZERO
};
*/

void xtcBlendFunc(xtceAlpha a, xtceAlpha b, xtceAlpha c, xtceAlpha d, int fix);

static int blendMap[] = {
	GL_ZERO, // XTC_BLEND_ZERO
	GL_ONE,	// XTC_BLEND_ONE
	GL_SRC_ALPHA, // XTC_BLEND_SRCALPHA
	GL_ONE_MINUS_SRC_ALPHA,	// XTC_BLEND_INVSRCALPHA
	GL_DST_ALPHA,	// XTC_BLEND_DSTALPHA
	GL_ONE_MINUS_DST_ALPHA,	// XTC_BLEND_INVDSTALPHA
};

void
xtcBlendFuncSrcDst(xtcBlendFactor src, xtcBlendFactor dst)
{
	glBlendFunc(blendMap[src], blendMap[dst]);
}

void xtcFog(float start, float end, u32 col);

/*
enum xtceShadeModel {
	XTC_FLAT,
	XTC_SMOOTH,
};
*/

void xtcShadeModel(xtceShadeModel model);



static void
flip(u8 *texdata, u32 width, u32 height)
{
	u8 *newdata = (u8*)malloc(width*height*4);
	for(u32 i = 0; i < height; i++)
		memcpy(newdata + i*width*4, texdata + (height-1-i)*width*4, width*4);
	memcpy(texdata, newdata, width*height*4);
	free(newdata);
}

xtcTexture*
xtcTextureReadPNG(const u8 *data, u32 size)
{
	u8 *texdata;
	u32 width, height;
	u32 error = lodepng_decode32(&texdata, &width, &height, data, size);
	if(error) {
		fprintf(stderr, "error: decode png\n");
		return nil;
	}

	flip(texdata, width, height);

	xtcTexture *tex = (xtcTexture*)malloc(sizeof(xtcTexture));
	tex->width = width;
	tex->height = height;
	glCreateTextures(GL_TEXTURE_2D, 1, &tex->tex);
	glTextureStorage2D(tex->tex, 1, GL_RGBA8, width, height);
	glTextureSubImage2D(tex->tex, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, texdata);

//memset(texdata, 0, width*height*4);
//	glGetTextureImage(tex->tex, 0, GL_RGBA, GL_UNSIGNED_BYTE, width*height*4, texdata);
	free(texdata);

	return tex;
}




xtcPrimList*
xtcCreatePrimList(void)
{
	xtcPrimList *pl;

	pl = (xtcPrimList*)malloc(sizeof(xtcPrimList));
	memset(pl, 0, sizeof(*pl));

	return pl;
}

// this probably shouldn't be a PrimList but a more general DisplayList
// containing PrimList elements for every begin/end pair

static xtcPrimList *curList;

void
xtcStartList(xtcPrimList *pl)
{
	curList = pl;
}

void
xtcEndList(void)
{
	curList = nil;
}

// not optimal, so something more sensible
// depending on format
void
xtcPrimListSetData(xtcPrimList *pl, xtcPrimType primType, u32 numVertices, xtcImmVertex3D *vertices)
{
	if(pl->vbo || pl->vao) {
		fprintf(stderr, "error: xtcPrimListSetData already called\n");
		return;
	}

	pl->primType = primType;
	pl->numVertices = numVertices;
	u32 stride = sizeof(xtcImmVertex3D);

	glCreateBuffers(1, &pl->vbo);
// TODO: may want to have other usage here
	glNamedBufferStorage(pl->vbo, numVertices*stride, vertices, GL_DYNAMIC_STORAGE_BIT);

	glCreateVertexArrays(1, &pl->vao);
	glVertexArrayVertexBuffer(pl->vao, 0, pl->vbo, 0, stride);

	glEnableVertexArrayAttrib(pl->vao, 0);
	glEnableVertexArrayAttrib(pl->vao, 1);
	glEnableVertexArrayAttrib(pl->vao, 2);
	glEnableVertexArrayAttrib(pl->vao, 3);
	glEnableVertexArrayAttrib(pl->vao, 4);
	glEnableVertexArrayAttrib(pl->vao, 5);
	glVertexArrayAttribFormat(pl->vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(xtcImmVertex3D, pos));
	glVertexArrayAttribFormat(pl->vao, 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(xtcImmVertex3D, color));
	glVertexArrayAttribFormat(pl->vao, 2, 3, GL_FLOAT, GL_FALSE, offsetof(xtcImmVertex3D, normal));
	glVertexArrayAttribFormat(pl->vao, 3, 3, GL_FLOAT, GL_FALSE, offsetof(xtcImmVertex3D, texcoord));
	glVertexArrayAttribIFormat(pl->vao, 4, 4, GL_UNSIGNED_BYTE, offsetof(xtcImmVertex3D, indices));
	glVertexArrayAttribFormat(pl->vao, 5, 4, GL_FLOAT, GL_FALSE, offsetof(xtcImmVertex3D, weights));
	glVertexArrayAttribBinding(pl->vao, 0, 0);
	glVertexArrayAttribBinding(pl->vao, 1, 0);
	glVertexArrayAttribBinding(pl->vao, 2, 0);
	glVertexArrayAttribBinding(pl->vao, 3, 0);
	glVertexArrayAttribBinding(pl->vao, 4, 0);
	glVertexArrayAttribBinding(pl->vao, 5, 0);
}

/*
// TODO: make format more flexible
xtcPrimList*
xtcCreatePrimList(xtcPrimType primType, u32 numVertices, xtcImmVertex3D *vertices)
{
	xtcPrimList *pl;

	pl = xtcCreatePrimList();
	xtcPrimListSetData(pl, primType, numVertices, vertices);
	return pl;
}
*/

void
xtcPrimListDraw(xtcPrimList *pl)
{
	curPipe->upload();

	glBindVertexArray(pl->vao);
	glDrawArrays(primMap[pl->primType], 0, pl->numVertices);
	glBindVertexArray(0);
}





ImmState immstate;

void
xtcInit(void)
{
	xtcSetPipeline(defaultPipeline);

	glCreateVertexArrays(1, &immstate.vao);
}

void
xtcBegin(xtcPrimType prim)
{
	immstate.primType = prim;
	immstate.vertstore.clear();
	immstate.vert.pos = vec3(0.0f, 0.0f, 0.0f);
	immstate.vert.color = white;
	immstate.vert.normal = vec3(0.0f, 0.0f, 0.0f);
	immstate.vert.texcoord = vec3(0.0f, 0.0f, 1.0f);

	if(curList == nil)
		curPipe->upload();
}

void
xtcFlush(void)
{
	u32 stride = sizeof(xtcImmVertex3D);
	u32 numVertices = immstate.vertstore.size();
	xtcImmVertex3D *vertices = &immstate.vertstore[0];

	glCreateBuffers(1, &immstate.vbo);
	glNamedBufferStorage(immstate.vbo, numVertices*stride, vertices, GL_DYNAMIC_STORAGE_BIT);
	glVertexArrayVertexBuffer(immstate.vao, 0, immstate.vbo, 0, stride);

	glEnableVertexArrayAttrib(immstate.vao, 0);
	glEnableVertexArrayAttrib(immstate.vao, 1);
	glEnableVertexArrayAttrib(immstate.vao, 2);
	glEnableVertexArrayAttrib(immstate.vao, 3);
	glEnableVertexArrayAttrib(immstate.vao, 4);
	glEnableVertexArrayAttrib(immstate.vao, 5);
	glVertexArrayAttribFormat(immstate.vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(xtcImmVertex3D, pos));
	glVertexArrayAttribFormat(immstate.vao, 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, offsetof(xtcImmVertex3D, color));
	glVertexArrayAttribFormat(immstate.vao, 2, 3, GL_FLOAT, GL_FALSE, offsetof(xtcImmVertex3D, normal));
	glVertexArrayAttribFormat(immstate.vao, 3, 3, GL_FLOAT, GL_FALSE, offsetof(xtcImmVertex3D, texcoord));
	glVertexArrayAttribFormat(immstate.vao, 4, 4, GL_UNSIGNED_BYTE, GL_FALSE, offsetof(xtcImmVertex3D, indices));
	glVertexArrayAttribFormat(immstate.vao, 5, 4, GL_FLOAT, GL_FALSE, offsetof(xtcImmVertex3D, weights));
	glVertexArrayAttribBinding(immstate.vao, 0, 0);
	glVertexArrayAttribBinding(immstate.vao, 1, 0);
	glVertexArrayAttribBinding(immstate.vao, 2, 0);
	glVertexArrayAttribBinding(immstate.vao, 3, 0);
	glVertexArrayAttribBinding(immstate.vao, 4, 0);
	glVertexArrayAttribBinding(immstate.vao, 5, 0);

	glBindVertexArray(immstate.vao);
	glDrawArrays(primMap[immstate.primType], 0, numVertices);

	glBindVertexArray(0);
	glVertexArrayVertexBuffer(immstate.vao, 0, 0, 0, 0);
	glDeleteBuffers(1, &immstate.vbo);
}

void
xtcEnd(void)
{
	if(curList == nil)
		xtcFlush();
	else {
		// TODO: this might perhaps be made more general with multiple
		// begin/end pairs
		xtcPrimListSetData(curList, immstate.primType, immstate.vertstore.size(), &immstate.vertstore[0]);
	}
	immstate.vertstore.clear();
}

void
xtcPointSize(float size)
{
	glPointSize(size);
}

void
xtcVertex3(float x, float y, float z)
{
	immstate.vert.pos.x = x;
	immstate.vert.pos.y = y;
	immstate.vert.pos.z = z;
	immstate.vertstore.push_back(immstate.vert);
}
void
xtcVertex3v(const Vec3 &xyz)
{
	immstate.vert.pos = xyz;
	immstate.vertstore.push_back(immstate.vert);
}

void
xtcColor(u8 r, u8 g, u8 b, u8 a)
{
	immstate.vert.color.r = r;
	immstate.vert.color.g = g;
	immstate.vert.color.b = b;
	immstate.vert.color.a = a;
}
void
xtcColor(const xtcRGBA &rgba)
{
	immstate.vert.color = rgba;
}

void
xtcNormal(float x, float y, float z)
{
	immstate.vert.normal.x = x;
	immstate.vert.normal.y = y;
	immstate.vert.normal.z = z;
}
void
xtcNormalv(const Vec3 &xyz)
{
	immstate.vert.normal = xyz;
}

void
xtcTexCoord2(float s, float t)
{
	immstate.vert.texcoord = vec3(s, t, 1.0f);
}
void
xtcTexCoord3(float s, float t, float q)
{
	immstate.vert.texcoord = vec3(s, t, q);
}
void
xtcTexCoordv(const Vec2 &st)
{
	immstate.vert.texcoord = vec3(st.x, st.y, 1.0f);
}

void
xtcIndices(u8 i0, u8 i1, u8 i2, u8 i3)
{
	union {
		u32 integ;
		u8 byte[4];
	} x;
	x.byte[0] = i0;
	x.byte[1] = i1;
	x.byte[2] = i2;
	x.byte[3] = i3;
	immstate.vert.indices = x.integ;
}

void
xtcWeights(float w0, float w1, float w2, float w3)
{
	immstate.vert.weights.x = w0;
	immstate.vert.weights.y = w1;
	immstate.vert.weights.z = w2;
	immstate.vert.weights.w = w3;
}
