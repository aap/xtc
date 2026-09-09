/*
 * Vectors, matrices, quaternions. Plain C structs and functions so the
 * same header serves the PS2 and PC builds; matrices are column-major
 * (four column vectors, x y z w), the memory layout of float[16] as the
 * VU and GL expect it and of glm::mat4.
 *
 * Conventions:
 *   m4mul(a, b)      a*b, i.e. b is applied first
 *   m4xform(m, v)    m*v
 *   Quat             xi + yj + zk + w, stored xyzw like Vec4, unit quaternions rotate
 *
 * C++ gets operators for the obvious cases so code reads like it did
 * with glm: v+w, s*v, m*n, m*v, q*p.
 */

#ifndef XMATH_H
#define XMATH_H

#include <math.h>

typedef struct { float x, y; } Vec2;
typedef struct { float x, y, z; } Vec3;
typedef struct { float x, y, z, w; } Vec4;
typedef struct { Vec4 x, y, z, w; } Mat4;
typedef struct { float x, y, z, w; } Quat;	// same layout as Vec4, one qword

#ifndef XM_PI
#define XM_PI 3.14159265358979323846f
#endif

/*
 * Vec2
 */

static inline Vec2 vec2(float x, float y) { Vec2 v; v.x = x; v.y = y; return v; }
static inline Vec2 v2neg(Vec2 v) { return vec2(-v.x, -v.y); }
static inline Vec2 v2add(Vec2 u, Vec2 v) { return vec2(u.x+v.x, u.y+v.y); }
static inline Vec2 v2sub(Vec2 u, Vec2 v) { return vec2(u.x-v.x, u.y-v.y); }
static inline Vec2 v2scale(float s, Vec2 v) { return vec2(s*v.x, s*v.y); }
static inline float v2dot(Vec2 a, Vec2 b) { return a.x*b.x + a.y*b.y; }
static inline float v2normsq(Vec2 v) { return v2dot(v, v); }
static inline float v2norm(Vec2 v) { return sqrtf(v2normsq(v)); }
static inline Vec2 v2normalized(Vec2 v) { return v2scale(1.0f/v2norm(v), v); }

/*
 * Vec3
 */

static inline Vec3 vec3(float x, float y, float z) { Vec3 v; v.x = x; v.y = y; v.z = z; return v; }
static inline Vec3 v3neg(Vec3 v) { return vec3(-v.x, -v.y, -v.z); }
static inline Vec3 v3add(Vec3 u, Vec3 v) { return vec3(u.x+v.x, u.y+v.y, u.z+v.z); }
static inline Vec3 v3sub(Vec3 u, Vec3 v) { return vec3(u.x-v.x, u.y-v.y, u.z-v.z); }
static inline Vec3 v3scale(float s, Vec3 v) { return vec3(s*v.x, s*v.y, s*v.z); }
static inline Vec3 v3mul(Vec3 u, Vec3 v) { return vec3(u.x*v.x, u.y*v.y, u.z*v.z); }
static inline float v3dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline Vec3 v3cross(Vec3 a, Vec3 b) {
	return vec3(a.y*b.z - a.z*b.y,
	            a.z*b.x - a.x*b.z,
	            a.x*b.y - a.y*b.x);
}
static inline float v3normsq(Vec3 v) { return v3dot(v, v); }
static inline float v3norm(Vec3 v) { return sqrtf(v3normsq(v)); }
static inline Vec3 v3normalized(Vec3 v) { return v3scale(1.0f/v3norm(v), v); }
static inline Vec3 v3lerp(Vec3 a, Vec3 b, float t) { return v3add(a, v3scale(t, v3sub(b, a))); }
static inline Vec3 v3min(Vec3 a, Vec3 b) { return vec3(a.x < b.x ? a.x : b.x, a.y < b.y ? a.y : b.y, a.z < b.z ? a.z : b.z); }
static inline Vec3 v3max(Vec3 a, Vec3 b) { return vec3(a.x > b.x ? a.x : b.x, a.y > b.y ? a.y : b.y, a.z > b.z ? a.z : b.z); }

/*
 * Vec4
 */

static inline Vec4 vec4(float x, float y, float z, float w) { Vec4 v; v.x = x; v.y = y; v.z = z; v.w = w; return v; }
static inline Vec4 v3tov4(Vec3 v, float w) { return vec4(v.x, v.y, v.z, w); }
static inline Vec3 v4tov3(Vec4 v) { return vec3(v.x, v.y, v.z); }
static inline Vec4 v4neg(Vec4 v) { return vec4(-v.x, -v.y, -v.z, -v.w); }
static inline Vec4 v4add(Vec4 u, Vec4 v) { return vec4(u.x+v.x, u.y+v.y, u.z+v.z, u.w+v.w); }
static inline Vec4 v4sub(Vec4 u, Vec4 v) { return vec4(u.x-v.x, u.y-v.y, u.z-v.z, u.w-v.w); }
static inline Vec4 v4scale(float s, Vec4 v) { return vec4(s*v.x, s*v.y, s*v.z, s*v.w); }
static inline Vec4 v4mul(Vec4 u, Vec4 v) { return vec4(u.x*v.x, u.y*v.y, u.z*v.z, u.w*v.w); }
static inline float v4dot(Vec4 a, Vec4 b) { return a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w; }
static inline float v4normsq(Vec4 v) { return v4dot(v, v); }
static inline float v4norm(Vec4 v) { return sqrtf(v4normsq(v)); }
static inline Vec4 v4normalized(Vec4 v) { return v4scale(1.0f/v4norm(v), v); }
static inline Vec4 v4lerp(Vec4 a, Vec4 b, float t) { return v4add(a, v4scale(t, v4sub(b, a))); }

/*
 * Mat4
 */

static inline Mat4 mat4(Vec4 x, Vec4 y, Vec4 z, Vec4 w) { Mat4 m; m.x = x; m.y = y; m.z = z; m.w = w; return m; }
static inline float *m4ptr(Mat4 *m) { return &m->x.x; }
static inline const float *m4cptr(const Mat4 *m) { return &m->x.x; }

static inline Mat4 m4diag(float s) {
	return mat4(vec4(s, 0.0f, 0.0f, 0.0f),
	            vec4(0.0f, s, 0.0f, 0.0f),
	            vec4(0.0f, 0.0f, s, 0.0f),
	            vec4(0.0f, 0.0f, 0.0f, s));
}
static inline Mat4 m4ident(void) { return m4diag(1.0f); }
static inline Mat4 m4zero(void) { return m4diag(0.0f); }

static inline Vec4 m4xform(const Mat4 *m, Vec4 v) {
	return v4add(v4scale(v.x, m->x),
	       v4add(v4scale(v.y, m->y),
	       v4add(v4scale(v.z, m->z),
	             v4scale(v.w, m->w))));
}
// affine helpers: points get translated, vectors don't
static inline Vec3 m4xformPoint(const Mat4 *m, Vec3 v) { return v4tov3(m4xform(m, v3tov4(v, 1.0f))); }
static inline Vec3 m4xformVec(const Mat4 *m, Vec3 v) { return v4tov3(m4xform(m, v3tov4(v, 0.0f))); }
// v through the inverse of an orthonormal m, i.e. its transpose (the
// world matrix taking a light direction into object space)
static inline Vec3 m4invXformVecO(const Mat4 *m, Vec3 v) {
	return vec3(v3dot(v4tov3(m->x), v), v3dot(v4tov3(m->y), v), v3dot(v4tov3(m->z), v));
}

static inline Mat4 m4mulP(const Mat4 *a, const Mat4 *b) {
	return mat4(m4xform(a, b->x), m4xform(a, b->y), m4xform(a, b->z), m4xform(a, b->w));
}
static inline Mat4 m4mul(Mat4 a, Mat4 b) { return m4mulP(&a, &b); }

static inline Mat4 m4transposeP(const Mat4 *m) {
	return mat4(vec4(m->x.x, m->y.x, m->z.x, m->w.x),
	            vec4(m->x.y, m->y.y, m->z.y, m->w.y),
	            vec4(m->x.z, m->y.z, m->z.z, m->w.z),
	            vec4(m->x.w, m->y.w, m->z.w, m->w.w));
}
static inline Mat4 m4transpose(Mat4 m) { return m4transposeP(&m); }

// inverse of a rotation+translation matrix
static inline Mat4 m4invOrthoP(const Mat4 *m) {
	return mat4(vec4(m->x.x, m->y.x, m->z.x, 0.0f),
	            vec4(m->x.y, m->y.y, m->z.y, 0.0f),
	            vec4(m->x.z, m->y.z, m->z.z, 0.0f),
	            vec4(-v4dot(m->x, m->w), -v4dot(m->y, m->w), -v4dot(m->z, m->w), 1.0f));
}
static inline Mat4 m4invOrtho(Mat4 m) { return m4invOrthoP(&m); }

static inline Mat4 m4translate(float x, float y, float z) {
	Mat4 m = m4ident();
	m.w = vec4(x, y, z, 1.0f);
	return m;
}
static inline Mat4 m4translatev(Vec3 v) { return m4translate(v.x, v.y, v.z); }

static inline Mat4 m4scale(float x, float y, float z) {
	return mat4(vec4(x, 0.0f, 0.0f, 0.0f),
	            vec4(0.0f, y, 0.0f, 0.0f),
	            vec4(0.0f, 0.0f, z, 0.0f),
	            vec4(0.0f, 0.0f, 0.0f, 1.0f));
}
static inline Mat4 m4scalev(Vec3 v) { return m4scale(v.x, v.y, v.z); }

static inline Mat4 m4rotX(float a) {
	float c = cosf(a), s = sinf(a);
	return mat4(vec4(1.0f, 0.0f, 0.0f, 0.0f),
	            vec4(0.0f, c, s, 0.0f),
	            vec4(0.0f, -s, c, 0.0f),
	            vec4(0.0f, 0.0f, 0.0f, 1.0f));
}
static inline Mat4 m4rotY(float a) {
	float c = cosf(a), s = sinf(a);
	return mat4(vec4(c, 0.0f, -s, 0.0f),
	            vec4(0.0f, 1.0f, 0.0f, 0.0f),
	            vec4(s, 0.0f, c, 0.0f),
	            vec4(0.0f, 0.0f, 0.0f, 1.0f));
}
static inline Mat4 m4rotZ(float a) {
	float c = cosf(a), s = sinf(a);
	return mat4(vec4(c, s, 0.0f, 0.0f),
	            vec4(-s, c, 0.0f, 0.0f),
	            vec4(0.0f, 0.0f, 1.0f, 0.0f),
	            vec4(0.0f, 0.0f, 0.0f, 1.0f));
}

// fov in degrees, maps [-n,-f] to [-1,1] like GL
static inline Mat4 m4persp(float fov, float aspect, float n, float f) {
	float w = tanf(fov/360.0f*2.0f*XM_PI/2.0f);
	float h = w/aspect;
	float a = (n+f)/(n-f);
	float b = 2.0f*n*f/(n-f);
	w = 1.0f/w;
	h = 1.0f/h;
	return mat4(vec4(w, 0.0f, 0.0f, 0.0f),
	            vec4(0.0f, h, 0.0f, 0.0f),
	            vec4(0.0f, 0.0f, a, -1.0f),
	            vec4(0.0f, 0.0f, b, 0.0f));
}

static inline Mat4 m4ortho(float w, float h, float n, float f) {
	float a = 2.0f/(n-f);
	float b = (f+n)/(n-f);
	return mat4(vec4(2.0f/w, 0.0f, 0.0f, 0.0f),
	            vec4(0.0f, 2.0f/h, 0.0f, 0.0f),
	            vec4(0.0f, 0.0f, a, 0.0f),
	            vec4(0.0f, 0.0f, b, 1.0f));
}

// camera matrix (not the view matrix, invert it for that)
static inline Mat4 m4lookat(Vec3 pos, Vec3 target, Vec3 up) {
	Vec3 z = v3normalized(v3sub(pos, target));
	Vec3 x = v3normalized(v3cross(up, z));
	Vec3 y = v3normalized(v3cross(z, x));
	return mat4(v3tov4(x, 0.0f), v3tov4(y, 0.0f), v3tov4(z, 0.0f), v3tov4(pos, 1.0f));
}

/*
 * Quat
 */

static inline Quat quat(float x, float y, float z, float w) { Quat q; q.x = x; q.y = y; q.z = z; q.w = w; return q; }
static inline Quat rquat(float r) { return quat(0.0f, 0.0f, 0.0f, r); }
static inline Quat qident(void) { return rquat(1.0f); }
static inline Quat qvec(Quat q) { return quat(q.x, q.y, q.z, 0.0f); }
static inline Quat qneg(Quat q) { return quat(-q.x, -q.y, -q.z, -q.w); }
static inline Quat qadd(Quat p, Quat q) { return quat(p.x+q.x, p.y+q.y, p.z+q.z, p.w+q.w); }
static inline Quat qsub(Quat p, Quat q) { return quat(p.x-q.x, p.y-q.y, p.z-q.z, p.w-q.w); }
static inline Quat qscale(float s, Quat q) { return quat(s*q.x, s*q.y, s*q.z, s*q.w); }
static inline Quat qconj(Quat q) { return quat(-q.x, -q.y, -q.z, q.w); }
static inline Quat qmul(Quat p, Quat q) {
	return quat(p.w*q.x + p.x*q.w + p.y*q.z - p.z*q.y,
	            p.w*q.y + p.y*q.w + p.z*q.x - p.x*q.z,
	            p.w*q.z + p.z*q.w + p.x*q.y - p.y*q.x,
	            p.w*q.w - p.x*q.x - p.y*q.y - p.z*q.z);
}
static inline float qdot(Quat p, Quat q) { return p.w*q.w + p.x*q.x + p.y*q.y + p.z*q.z; }
static inline float qnormsq(Quat q) { return qdot(q, q); }
static inline float qnorm(Quat q) { return sqrtf(qnormsq(q)); }
static inline Quat qinv(Quat q) { return qscale(1.0f/qnormsq(q), qconj(q)); }
static inline Quat qnormalized(Quat q) { return qscale(1.0f/qnorm(q), q); }
// q normalized before and after
static inline Quat qsqrtnorm(Quat q) { return qnormalized(qadd(q, rquat(1.0f))); }
static inline Quat qfromto(Quat u, Quat v) { return qsqrtnorm(qmul(quat(v.x, v.y, v.z, 0.0f), quat(-u.x, -u.y, -u.z, 0.0f))); }
static inline Quat qsandwich(Quat q, Quat v) { return qmul(q, qmul(v, qconj(q))); }
static inline Quat qrsandwich(Quat q, Quat v) { return qmul(qconj(q), qmul(v, q)); }
static inline Quat qrotate(Quat q, Quat v) { return qmul(q, qmul(qvec(v), qinv(q))); }
static inline Quat qrrotate(Quat q, Quat v) { return qmul(qinv(q), qmul(qvec(v), q)); }

// rotation by angle around a unit axis
static inline Quat qaxisangle(Vec3 axis, float a) {
	float s = sinf(a*0.5f);
	return quat(s*axis.x, s*axis.y, s*axis.z, cosf(a*0.5f));
}
static inline Vec3 qrotatev3(Quat q, Vec3 v) {
	Quat r = qmul(q, qmul(quat(v.x, v.y, v.z, 0.0f), qconj(q)));
	return vec3(r.x, r.y, r.z);
}

// unit quaternion to rotation matrix
static inline Mat4 qtomat4(Quat q) {
	float xx = q.x*q.x, yy = q.y*q.y, zz = q.z*q.z;
	float xy = q.x*q.y, xz = q.x*q.z, yz = q.y*q.z;
	float wx = q.w*q.x, wy = q.w*q.y, wz = q.w*q.z;
	return mat4(vec4(1.0f - 2.0f*(yy + zz), 2.0f*(xy + wz), 2.0f*(xz - wy), 0.0f),
	            vec4(2.0f*(xy - wz), 1.0f - 2.0f*(xx + zz), 2.0f*(yz + wx), 0.0f),
	            vec4(2.0f*(xz + wy), 2.0f*(yz - wx), 1.0f - 2.0f*(xx + yy), 0.0f),
	            vec4(0.0f, 0.0f, 0.0f, 1.0f));
}

// shortest-path interpolation between unit quaternions
static inline Quat qslerp(Quat a, Quat b, float t) {
	float th, s;
	float c = qdot(a, b);
	if(c < 0.0f) {
		b = qneg(b);
		c = -c;
	}
	// nearly parallel: lerp, avoiding the division
	if(c > 0.9995f)
		return qnormalized(qadd(a, qscale(t, qsub(b, a))));
	th = acosf(c);
	s = sinf(th);
	return qadd(qscale(sinf((1.0f-t)*th)/s, a), qscale(sinf(t*th)/s, b));
}

static inline Quat qexp(Quat q) {
	Quat qv = qvec(q);
	float l = qnorm(qv);
	if(l == 0.0f)
		return rquat(expf(q.w));
	return qscale(expf(q.w), qadd(rquat(cosf(l)), qscale(sinf(l)/l, qv)));
}
static inline Quat qlog(Quat q) {
	float l = qnorm(q);
	float c, s;
	q = qscale(1.0f/l, q);
	c = q.w; q.w = 0.0f;
	s = qnorm(q);
	if(s == 0.0f)
		return rquat(logf(l));
	return qadd(rquat(logf(l)), qscale(atan2f(s, c)/s, q));
}
static inline Quat qpow(Quat q, float a) { return qexp(qscale(a, qlog(q))); }
static inline Quat qcross(Quat p, Quat q) { return qscale(0.5f, qsub(qmul(p, q), qmul(q, p))); }


#ifdef __cplusplus

static inline Vec2 operator-(Vec2 v) { return v2neg(v); }
static inline Vec2 operator+(Vec2 u, Vec2 v) { return v2add(u, v); }
static inline Vec2 operator-(Vec2 u, Vec2 v) { return v2sub(u, v); }
static inline Vec2 operator*(float s, Vec2 v) { return v2scale(s, v); }
static inline Vec2 operator*(Vec2 v, float s) { return v2scale(s, v); }
static inline Vec2 operator/(Vec2 v, float s) { return v2scale(1.0f/s, v); }
static inline Vec2 &operator+=(Vec2 &u, Vec2 v) { u = v2add(u, v); return u; }
static inline Vec2 &operator-=(Vec2 &u, Vec2 v) { u = v2sub(u, v); return u; }
static inline Vec2 &operator*=(Vec2 &v, float s) { v = v2scale(s, v); return v; }

static inline Vec3 operator-(Vec3 v) { return v3neg(v); }
static inline Vec3 operator+(Vec3 u, Vec3 v) { return v3add(u, v); }
static inline Vec3 operator-(Vec3 u, Vec3 v) { return v3sub(u, v); }
static inline Vec3 operator*(float s, Vec3 v) { return v3scale(s, v); }
static inline Vec3 operator*(Vec3 v, float s) { return v3scale(s, v); }
static inline Vec3 operator/(Vec3 v, float s) { return v3scale(1.0f/s, v); }
static inline Vec3 &operator+=(Vec3 &u, Vec3 v) { u = v3add(u, v); return u; }
static inline Vec3 &operator-=(Vec3 &u, Vec3 v) { u = v3sub(u, v); return u; }
static inline Vec3 &operator*=(Vec3 &v, float s) { v = v3scale(s, v); return v; }

static inline Vec4 operator-(Vec4 v) { return v4neg(v); }
static inline Vec4 operator+(Vec4 u, Vec4 v) { return v4add(u, v); }
static inline Vec4 operator-(Vec4 u, Vec4 v) { return v4sub(u, v); }
static inline Vec4 operator*(float s, Vec4 v) { return v4scale(s, v); }
static inline Vec4 operator*(Vec4 v, float s) { return v4scale(s, v); }
static inline Vec4 operator/(Vec4 v, float s) { return v4scale(1.0f/s, v); }
static inline Vec4 &operator+=(Vec4 &u, Vec4 v) { u = v4add(u, v); return u; }
static inline Vec4 &operator-=(Vec4 &u, Vec4 v) { u = v4sub(u, v); return u; }
static inline Vec4 &operator*=(Vec4 &v, float s) { v = v4scale(s, v); return v; }

static inline Mat4 operator*(const Mat4 &a, const Mat4 &b) { return m4mulP(&a, &b); }
static inline Vec4 operator*(const Mat4 &m, Vec4 v) { return m4xform(&m, v); }
static inline Mat4 &operator*=(Mat4 &a, const Mat4 &b) { a = m4mulP(&a, &b); return a; }

static inline Quat operator-(Quat q) { return qneg(q); }
static inline Quat operator+(Quat p, Quat q) { return qadd(p, q); }
static inline Quat operator-(Quat p, Quat q) { return qsub(p, q); }
static inline Quat operator*(Quat p, Quat q) { return qmul(p, q); }
static inline Quat operator*(float s, Quat q) { return qscale(s, q); }
static inline Quat operator*(Quat q, float s) { return qscale(s, q); }

#endif

#endif
