#include <math.h>
#include "m.h"

/*
 * most of this file is temporary crap
 */

// assume looking into positive z
// negate proj[10,11] for negative z
// NB: z row is only used for clipping!
void
makePerspective(float proj[16], float fov, float aspect, float near, float far)
{
	float w = tanf(fov*PI/360.0f);
	float h = w/aspect;
	float x = 0.0f;
	float y = 0.0f;

	proj[0] = 1.0f/w;
	proj[1] = 0.0f;
	proj[2] = 0.0f;
	proj[3] = 0.0f;

	proj[4] = 0.0f;
	proj[5] = 1.0f/h;
	proj[6] = 0.0f;
	proj[7] = 0.0f;

	proj[8] = x/w;
	proj[9] = y/h;
	proj[10] = (far+near)/(far-near);
	proj[11] = 1.0f;

	proj[12] = -proj[8];
	proj[13] = -proj[9];
	proj[14] = -2.0f*near*far/(far-near);
	proj[15] = 0.0f;
}

float
dot(float a[3], float b[3])
{
	return a[0]*b[0] + a[1]*b[1] + a[2]*b[2];
}

void
normalize(float out[3], float in[3])
{
	float lensq = dot(in, in);
	float invlen = 1.0f/sqrtf(lensq);
	out[0] = in[0]*invlen;
	out[1] = in[1]*invlen;
	out[2] = in[2]*invlen;
}

void
cross(float out[3], float a[3], float b[3])
{
	float x = a[1]*b[2] - a[2]*b[1];
	float y = a[2]*b[0] - a[0]*b[2];
	float z = a[0]*b[1] - a[1]*b[0];
	out[0] = x;
	out[1] = y;
	out[2] = z;
}

void
makeLookAt(float mat[16], float fwd[3], float up[3], float pos[3])
{
	normalize(&mat[8], fwd);
	mat[11] = 0.0f;

	cross(&mat[0], up, &mat[8]);
	normalize(&mat[0], &mat[0]);
	mat[3] = 0.0f;

	cross(&mat[4], &mat[8], &mat[0]);
	mat[7] = 0.0f;

	mat[12] = pos[0];
	mat[13] = pos[1];
	mat[14] = pos[2];
	mat[15] = 1.0f;

//// make left handed - at least for now
	mat[0] = -mat[0];
	mat[1] = -mat[1];
	mat[2] = -mat[2];
}

void
invertOrthonormal(float out[16], float in[16])
{
	out[0] = in[0];
	out[1] = in[4];
	out[2] = in[8];
	out[3] = 0.0f;

	out[4] = in[1];
	out[5] = in[5];
	out[6] = in[9];
	out[7] = 0.0f;

	out[8] = in[2];
	out[9] = in[6];
	out[10] = in[10];
	out[11] = 0.0f;

	out[12] = -dot(&in[12], &in[0]);
	out[13] = -dot(&in[12], &in[4]);
	out[14] = -dot(&in[12], &in[8]);
	out[15] = 1.0;
}

void
matmul(float out[16], float a[16], float b[16])
{
#define A(i,j) a[i*4+j]
#define B(i,j) b[i*4+j]
#define OUT(i,j) out[i*4+j]
	for(int i = 0; i < 4; i++)
		for(int j = 0; j < 4; j++)
			OUT(i,j) = A(0,j)*B(i,0)
				+ A(1,j)*B(i,1)
				+ A(2,j)*B(i,2)
				+ A(3,j)*B(i,3);
#undef A
#undef B
#undef OUT
}

// assumes m is orthogonal
void
invXformVecO(float out[3], float m[16], float v[3])
{
	float x = dot(&m[0], v);
	float y = dot(&m[4], v);
	float z = dot(&m[8], v);
	out[0] = x;
	out[1] = y;
	out[2] = z;
}
