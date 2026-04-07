#ifndef M_H_
#define M_H_

#define HALF_PI 1.57079632679489661923f
#define PI 3.14159265358979323846f
#define TAU (2.0f*PI)

void makePerspective(float proj[16], float fov, float aspect, float near, float far);
float dot(float a[3], float b[3]);
void normalize(float out[3], float in[3]);
void cross(float out[3], float a[3], float b[3]);
void makeLookAt(float mat[16], float fwd[3], float up[3], float pos[3]);
void invertOrthonormal(float out[16], float in[16]);
void matmul(float out[16], float a[16], float b[16]);
void invXformVecO(float out[3], float m[16], float v[3]);

#endif // M_H_
