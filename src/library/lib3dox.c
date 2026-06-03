/**
 ******************************************************************************
 * @file    lib3dox.c
 * @author  Typheye
 * @brief   Lib3Dox implementation.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2021-2026 Typheye. All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */


#include "include/lib3dox.h"


#define CCMRAM __attribute__((section(".ccmram")))


typedef struct {
  float x, y, z;
} vec3_t;
typedef struct {
  float m[4][4];
} mat4_t;
typedef struct {
  vec3_t bbmin, bbmax;
  vec3_t v0, v1, v2;
  vec3_t E1, E2, normal;
} triangle_t;


static CCMRAM mat4_t T;
static CCMRAM mat4_t view;
static CCMRAM triangle_t triangles[32];
static int triangles_ok = 0;

static CCMRAM vec3_t bbmin, bbmax;
static CCMRAM vec3_t eye, center, up;
static CCMRAM vec3_t view_x, view_y, view_z;
static CCMRAM vec3_t lightColor;

static int current_x = 0, current_y = 0, frame_count = 0;

static CCMRAM uint16_t pixel_history[RENDER_WIDTH * RENDER_HEIGHT];

static CCMRAM vec3_t t0, t1, temp;
static float tt;
static float u0, u1, u2;
static int8_t lightIdx;
static CCMRAM vec3_t light_pos;
static CCMRAM vec3_t radiance;

static CCMRAM vec3_t linear_r, linear_x, linear_y, linear_z, linear_t;

static CCMRAM struct {
  int8_t i;
  float t;
} intersection;
static CCMRAM struct {
  vec3_t start, direction, inv_direction;
} ray;
static CCMRAM struct {
  int8_t i;
  float t;
  triangle_t *surface;
  vec3_t position, normal;
} interaction;
static CCMRAM vec3_t reflectance, bsdf_absIdotN;
static float bsdf_pdf;

volatile int render_progress = 0;


#define SPP 1
#define MAX_DEPTH 2
#define INV_PI 0.318310f
#define TWO_PI 6.283185f
#define EPSILON 0.000001f
#define LIGHT_AREA 0.0893f


#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))
#define abs(x) ((x) > 0 ? x : -(x))
#define radians(x) ((x) * 0.017453f)
#define rand01() ((float)rand() / (float)RAND_MAX)


#define V3_ASSIGN(r, v)                                                        \
  do {                                                                         \
    (r).x = (v).x;                                                             \
    (r).y = (v).y;                                                             \
    (r).z = (v).z;                                                             \
  } while (0)
#define V3_ASSIGN_S(v, a)                                                      \
  do {                                                                         \
    (v).x = (a);                                                               \
    (v).y = (a);                                                               \
    (v).z = (a);                                                               \
  } while (0)
#define V3_ASSIGN_S3(v, a, b, c)                                               \
  do {                                                                         \
    (v).x = (a);                                                               \
    (v).y = (b);                                                               \
    (v).z = (c);                                                               \
  } while (0)
#define V3_SUB(r, v1, v2)                                                      \
  do {                                                                         \
    (r).x = (v1).x - (v2).x;                                                   \
    (r).y = (v1).y - (v2).y;                                                   \
    (r).z = (v1).z - (v2).z;                                                   \
  } while (0)
#define V3_ADD(r, v1, v2)                                                      \
  do {                                                                         \
    (r).x = (v1).x + (v2).x;                                                   \
    (r).y = (v1).y + (v2).y;                                                   \
    (r).z = (v1).z + (v2).z;                                                   \
  } while (0)
#define V3_MUL_S(r, v, a)                                                      \
  do {                                                                         \
    (r).x = (v).x * (a);                                                       \
    (r).y = (v).y * (a);                                                       \
    (r).z = (v).z * (a);                                                       \
  } while (0)
#define V3_MUL(r, v1, v2)                                                      \
  do {                                                                         \
    (r).x = (v1).x * (v2).x;                                                   \
    (r).y = (v1).y * (v2).y;                                                   \
    (r).z = (v1).z * (v2).z;                                                   \
  } while (0)
#define V3_MAD_S(r, v1, v2, a)                                                 \
  do {                                                                         \
    (r).x = (v1).x + (v2).x * (a);                                             \
    (r).y = (v1).y + (v2).y * (a);                                             \
    (r).z = (v1).z + (v2).z * (a);                                             \
  } while (0)
#define V3_MAD(r, v1, v2, v3)                                                  \
  do {                                                                         \
    (r).x = (v1).x * (v2).x + (v3).x;                                          \
    (r).y = (v1).y * (v2).y + (v3).y;                                          \
    (r).z = (v1).z * (v2).z + (v3).z;                                          \
  } while (0)
#define V3_MAD_ASSIGN(v1, v2, v3)                                              \
  do {                                                                         \
    (v1).x += (v2).x * (v3).x;                                                 \
    (v1).y += (v2).y * (v3).y;                                                 \
    (v1).z += (v2).z * (v3).z;                                                 \
  } while (0)
#define V3_ADD_ASSIGN(v1, v2)                                                  \
  do {                                                                         \
    (v1).x += (v2).x;                                                          \
    (v1).y += (v2).y;                                                          \
    (v1).z += (v2).z;                                                          \
  } while (0)
#define V3_MUL_ASSIGN(v1, v2)                                                  \
  do {                                                                         \
    (v1).x *= (v2).x;                                                          \
    (v1).y *= (v2).y;                                                          \
    (v1).z *= (v2).z;                                                          \
  } while (0)
#define V3_MUL_ASSIGN_S(v, a)                                                  \
  do {                                                                         \
    (v).x *= (a);                                                              \
    (v).y *= (a);                                                              \
    (v).z *= (a);                                                              \
  } while (0)
#define V3_DIV_ASSIGN_S(v, a)                                                  \
  do {                                                                         \
    (v).x /= (a);                                                              \
    (v).y /= (a);                                                              \
    (v).z /= (a);                                                              \
  } while (0)
#define V3_DIV_ASSIGN(v1, v2)                                                  \
  do {                                                                         \
    (v1).x /= (v2).x;                                                          \
    (v1).y /= (v2).y;                                                          \
    (v1).z /= (v2).z;                                                          \
  } while (0)
#define V3_RCP(r, v)                                                           \
  do {                                                                         \
    (r).x = 1.0f / (v).x;                                                      \
    (r).y = 1.0f / (v).y;                                                      \
    (r).z = 1.0f / (v).z;                                                      \
  } while (0)
#define V3_MIN(r, v1, v2)                                                      \
  do {                                                                         \
    (r).x = min((v1).x, (v2).x);                                               \
    (r).y = min((v1).y, (v2).y);                                               \
    (r).z = min((v1).z, (v2).z);                                               \
  } while (0)
#define V3_MAX(r, v1, v2)                                                      \
  do {                                                                         \
    (r).x = max((v1).x, (v2).x);                                               \
    (r).y = max((v1).y, (v2).y);                                               \
    (r).z = max((v1).z, (v2).z);                                               \
  } while (0)
#define V3_MINUS(r, v)                                                         \
  do {                                                                         \
    (r).x = -(v).x;                                                            \
    (r).y = -(v).y;                                                            \
    (r).z = -(v).z;                                                            \
  } while (0)

#define V3_COMPMAX(v) (max((v).x, max((v).y, (v).z)))
#define V3_COMPMIN(v) (min((v).x, min((v).y, (v).z)))
#define DOT(v1, v2) ((v1).x * (v2).x + (v1).y * (v2).y + (v1).z * (v2).z)
#define DOTM(v1, v2) (-(v1).x * (v2).x - (v1).y * (v2).y - (v1).z * (v2).z)

#define CROSS(r, v1, v2)                                                       \
  do {                                                                         \
    (r).x = (v1).y * (v2).z - (v1).z * (v2).y;                                 \
    (r).y = (v1).z * (v2).x - (v1).x * (v2).z;                                 \
    (r).z = (v1).x * (v2).y - (v1).y * (v2).x;                                 \
  } while (0)
#define NORMALIZE(v)                                                           \
  do {                                                                         \
    float _r = sqrtf(DOT(v, v));                                               \
    if (_r > 0) {                                                              \
      (v).x /= _r;                                                             \
      (v).y /= _r;                                                             \
      (v).z /= _r;                                                             \
    }                                                                          \
  } while (0)

#define LOOKAT(view, eye, center, up)                                          \
  do {                                                                         \
    vec3_t za;                                                                 \
    V3_SUB(za, eye, center);                                                   \
    NORMALIZE(za);                                                             \
    vec3_t xa;                                                                 \
    CROSS(xa, up, za);                                                         \
    NORMALIZE(xa);                                                             \
    vec3_t ya;                                                                 \
    CROSS(ya, za, xa);                                                         \
    (view).m[0][0] = xa.x;                                                     \
    (view).m[0][1] = ya.x;                                                     \
    (view).m[0][2] = za.x;                                                     \
    (view).m[0][3] = 0;                                                        \
    (view).m[1][0] = xa.y;                                                     \
    (view).m[1][1] = ya.y;                                                     \
    (view).m[1][2] = za.y;                                                     \
    (view).m[1][3] = 0;                                                        \
    (view).m[2][0] = xa.z;                                                     \
    (view).m[2][1] = ya.z;                                                     \
    (view).m[2][2] = za.z;                                                     \
    (view).m[2][3] = 0;                                                        \
    (view).m[3][0] = -DOT(xa, eye);                                            \
    (view).m[3][1] = -DOT(ya, eye);                                            \
    (view).m[3][2] = -DOT(za, eye);                                            \
    (view).m[3][3] = 1;                                                        \
  } while (0)

#define ORTHONORMALBASIS(N)                                                    \
  do {                                                                         \
    float sgn = (N).z > 0 ? 1.0f : -1.0f;                                      \
    float a = -1.0f / (sgn + (N).z);                                           \
    float b = (N).x * (N).y * a;                                               \
    T.m[0][0] = 1 + sgn * (N).x * (N).x * a;                                   \
    T.m[0][1] = sgn * b;                                                       \
    T.m[0][2] = -sgn * (N).x;                                                  \
    T.m[0][3] = 0;                                                             \
    T.m[1][0] = b;                                                             \
    T.m[1][1] = sgn + (N).y * (N).y * a;                                       \
    T.m[1][2] = -(N).y;                                                        \
    T.m[1][3] = 0;                                                             \
    T.m[2][0] = (N).x;                                                         \
    T.m[2][1] = (N).y;                                                         \
    T.m[2][2] = (N).z;                                                         \
    T.m[2][3] = 0;                                                             \
  } while (0)

#define BALANCEHEURISTIC(a, b) ((a) / ((a) + (b)))


static const float raw_triangles[] = {
    -0.240000f, 1.980000f,  -0.220000f, 0.230000f,  1.980000f,  0.160000f,
    -0.240000f, 1.980000f,  -0.220000f, 0.230000f,  1.980000f,  0.160000f,
    -0.240000f, 1.980000f,  0.160000f,  0.470000f,  0.000000f,  0.380000f,
    0.000000f,  0.000000f,  0.380000f,  0.000000f,  -1.000000f, 0.000000f,
    -0.240000f, 1.980000f,  -0.220000f, 0.230000f,  1.980000f,  0.160000f,
    -0.240000f, 1.980000f,  -0.220000f, 0.230000f,  1.980000f,  -0.220000f,
    0.230000f,  1.980000f,  0.160000f,  0.470000f,  0.000000f,  0.000000f,
    0.470000f,  0.000000f,  0.380000f,  0.000000f,  -1.000000f, 0.000000f,
    -1.010000f, 0.000000f,  -1.040000f, 1.000000f,  0.000000f,  0.990000f,
    1.000000f,  0.000000f,  0.990000f,  -0.990000f, -0.000000f, -1.040000f,
    -1.010000f, 0.000000f,  0.990000f,  -1.990000f, -0.000000f, -2.030000f,
    -2.010000f, 0.000000f,  0.000000f,  0.000000f,  1.000000f,  -0.000000f,
    -1.020000f, 1.990000f,  -1.040000f, 1.000000f,  1.990000f,  0.990000f,
    -1.020000f, 1.990000f,  -1.040000f, 1.000000f,  1.990000f,  0.990000f,
    -1.020000f, 1.990000f,  0.990000f,  2.020000f,  0.000000f,  2.030000f,
    0.000000f,  0.000000f,  2.030000f,  0.000000f,  -1.000000f, 0.000000f,
    -1.020000f, -0.000000f, -1.040000f, 1.000000f,  1.990000f,  -1.040000f,
    -0.990000f, -0.000000f, -1.040000f, 1.000000f,  1.990000f,  -1.040000f,
    -1.020000f, 1.990000f,  -1.040000f, 1.990000f,  1.990000f,  0.000000f,
    -0.030000f, 1.990000f,  0.000000f,  0.000000f,  -0.000000f, 1.000000f,
    -0.990000f, 0.000000f,  -1.040000f, 1.000000f,  0.000000f,  0.990000f,
    1.000000f,  0.000000f,  0.990000f,  1.000000f,  -0.000000f, -1.040000f,
    -0.990000f, -0.000000f, -1.040000f, 0.000000f,  -0.000000f, -2.030000f,
    -1.990000f, -0.000000f, -2.030000f, 0.000000f,  1.000000f,  -0.000000f,
    -1.020000f, 1.990000f,  -1.040000f, 1.000000f,  1.990000f,  0.990000f,
    -1.020000f, 1.990000f,  -1.040000f, 1.000000f,  1.990000f,  -1.040000f,
    1.000000f,  1.990000f,  0.990000f,  2.020000f,  0.000000f,  0.000000f,
    2.020000f,  0.000000f,  2.030000f,  0.000000f,  -1.000000f, 0.000000f,
    -0.990000f, -0.000000f, -1.040000f, 1.000000f,  1.990000f,  -1.040000f,
    -0.990000f, -0.000000f, -1.040000f, 1.000000f,  -0.000000f, -1.040000f,
    1.000000f,  1.990000f,  -1.040000f, 1.990000f,  0.000000f,  0.000000f,
    1.990000f,  1.990000f,  0.000000f,  0.000000f,  0.000000f,  1.000000f,
    -1.020000f, 0.000000f,  -1.040000f, -1.010000f, 1.990000f,  0.990000f,
    -1.010000f, 0.000000f,  0.990000f,  -1.020000f, 1.990000f,  -1.040000f,
    -1.020000f, 1.990000f,  0.990000f,  -0.010000f, 1.990000f,  -2.030000f,
    -0.010000f, 1.990000f,  0.000000f,  0.999987f,  0.005025f,  0.000000f,
    -1.020000f, 0.000000f,  -1.040000f, -0.990000f, 1.990000f,  0.990000f,
    -1.010000f, 0.000000f,  0.990000f,  -0.990000f, -0.000000f, -1.040000f,
    -1.020000f, 1.990000f,  -1.040000f, 0.020000f,  -0.000000f, -2.030000f,
    -0.010000f, 1.990000f,  -2.030000f, 0.999838f,  0.015073f,  0.009851f,
    1.000000f,  0.000000f,  -1.040000f, 1.000000f,  1.990000f,  0.990000f,
    1.000000f,  0.000000f,  0.990000f,  1.000000f,  1.990000f,  -1.040000f,
    1.000000f,  -0.000000f, -1.040000f, 0.000000f,  1.990000f,  -2.030000f,
    0.000000f,  -0.000000f, -2.030000f, -1.000000f, 0.000000f,  -0.000000f,
    1.000000f,  0.000000f,  -1.040000f, 1.000000f,  1.990000f,  0.990000f,
    1.000000f,  0.000000f,  0.990000f,  1.000000f,  1.990000f,  0.990000f,
    1.000000f,  1.990000f,  -1.040000f, 0.000000f,  1.990000f,  0.000000f,
    0.000000f,  1.990000f,  -2.030000f, -1.000000f, 0.000000f,  0.000000f,
    -0.710000f, 1.200000f,  -0.490000f, 0.040000f,  1.200000f,  0.090000f,
    0.040000f,  1.200000f,  -0.090000f, -0.710000f, 1.200000f,  -0.490000f,
    -0.530000f, 1.200000f,  0.090000f,  -0.750000f, 0.000000f,  -0.400000f,
    -0.570000f, 0.000000f,  0.180000f,  0.000000f,  1.000000f,  0.000000f,
    -0.710000f, -0.000000f, -0.490000f, -0.530000f, 1.200000f,  0.090000f,
    -0.530000f, 1.200000f,  0.090000f,  -0.710000f, -0.000000f, -0.490000f,
    -0.530000f, 0.000000f,  0.090000f,  -0.180000f, -1.200000f, -0.580000f,
    0.000000f,  -1.200000f, 0.000000f,  -0.955064f, 0.000000f,  0.296399f,
    -0.710000f, -0.000000f, -0.670000f, -0.140000f, 1.200000f,  -0.490000f,
    -0.710000f, 1.200000f,  -0.490000f, -0.140000f, -0.000000f, -0.670000f,
    -0.710000f, -0.000000f, -0.490000f, 0.570000f,  -1.200000f, -0.180000f,
    0.000000f,  -1.200000f, 0.000000f,  -0.301131f, -0.000000f, -0.953583f,
    -0.140000f, -0.000000f, -0.670000f, 0.040000f,  1.200000f,  -0.090000f,
    -0.140000f, 1.200000f,  -0.670000f, 0.040000f,  -0.000000f, -0.090000f,
    -0.140000f, -0.000000f, -0.670000f, 0.180000f,  -1.200000f, 0.580000f,
    0.000000f,  -1.200000f, 0.000000f,  0.955064f,  0.000000f,  -0.296399f,
    -0.530000f, 0.000000f,  -0.090000f, 0.040000f,  1.200000f,  0.090000f,
    0.040000f,  1.200000f,  -0.090000f, -0.530000f, 0.000000f,  0.090000f,
    0.040000f,  -0.000000f, -0.090000f, -0.570000f, -1.200000f, 0.180000f,
    0.000000f,  -1.200000f, 0.000000f,  0.301131f,  0.000000f,  0.953583f,
    -0.710000f, 1.200000f,  -0.670000f, 0.040000f,  1.200000f,  -0.090000f,
    0.040000f,  1.200000f,  -0.090000f, -0.140000f, 1.200000f,  -0.670000f,
    -0.710000f, 1.200000f,  -0.490000f, -0.180000f, 0.000000f,  -0.580000f,
    -0.750000f, 0.000000f,  -0.400000f, 0.000000f,  1.000000f,  0.000000f,
    -0.710000f, -0.000000f, -0.490000f, -0.530000f, 1.200000f,  0.090000f,
    -0.530000f, 1.200000f,  0.090000f,  -0.710000f, 1.200000f,  -0.490000f,
    -0.710000f, -0.000000f, -0.490000f, -0.180000f, 0.000000f,  -0.580000f,
    -0.180000f, -1.200000f, -0.580000f, -0.955064f, 0.000000f,  0.296399f,
    -0.710000f, -0.000000f, -0.670000f, -0.140000f, 1.200000f,  -0.490000f,
    -0.710000f, 1.200000f,  -0.490000f, -0.140000f, 1.200000f,  -0.670000f,
    -0.140000f, -0.000000f, -0.670000f, 0.570000f,  0.000000f,  -0.180000f,
    0.570000f,  -1.200000f, -0.180000f, -0.301131f, 0.000000f,  -0.953583f,
    -0.140000f, -0.000000f, -0.670000f, 0.040000f,  1.200000f,  -0.090000f,
    -0.140000f, 1.200000f,  -0.670000f, 0.040000f,  1.200000f,  -0.090000f,
    0.040000f,  -0.000000f, -0.090000f, 0.180000f,  0.000000f,  0.580000f,
    0.180000f,  -1.200000f, 0.580000f,  0.955064f,  0.000000f,  -0.296399f,
    -0.530000f, 0.000000f,  -0.090000f, 0.040000f,  1.200000f,  0.090000f,
    0.040000f,  1.200000f,  -0.090000f, -0.530000f, 1.200000f,  0.090000f,
    -0.530000f, 0.000000f,  0.090000f,  -0.570000f, 0.000000f,  0.180000f,
    -0.570000f, -1.200000f, 0.180000f,  0.301131f,  0.000000f,  0.953583f,
    -0.050000f, 0.600000f,  -0.000000f, 0.530000f,  0.600000f,  0.750000f,
    0.530000f,  0.600000f,  0.750000f,  0.130000f,  0.600000f,  -0.000000f,
    -0.050000f, 0.600000f,  0.570000f,  -0.400000f, 0.000000f,  -0.750000f,
    -0.580000f, 0.000000f,  -0.180000f, 0.000000f,  1.000000f,  0.000000f,
    -0.050000f, 0.000000f,  0.000000f,  0.130000f,  0.600000f,  0.570000f,
    -0.050000f, 0.600000f,  0.570000f,  0.130000f,  0.000000f,  0.000000f,
    -0.050000f, 0.000000f,  0.570000f,  0.180000f,  -0.600000f, -0.570000f,
    0.000000f,  -0.600000f, 0.000000f,  -0.953583f, -0.000000f, -0.301131f,
    -0.050000f, 0.000000f,  0.570000f,  0.530000f,  0.600000f,  0.750000f,
    0.530000f,  0.600000f,  0.750000f,  -0.050000f, 0.000000f,  0.570000f,
    0.530000f,  0.000000f,  0.750000f,  -0.580000f, -0.600000f, -0.180000f,
    0.000000f,  -0.600000f, 0.000000f,  -0.296399f, 0.000000f,  0.955064f,
    0.530000f,  0.000000f,  0.170000f,  0.700000f,  0.600000f,  0.750000f,
    0.700000f,  0.600000f,  0.170000f,  0.530000f,  0.000000f,  0.750000f,
    0.700000f,  0.000000f,  0.170000f,  -0.170000f, -0.600000f, 0.580000f,
    0.000000f,  -0.600000f, 0.000000f,  0.959629f,  0.000000f,  0.281270f,
    0.130000f,  0.000000f,  -0.000000f, 0.700000f,  0.600000f,  0.170000f,
    0.130000f,  0.600000f,  -0.000000f, 0.700000f,  0.000000f,  0.170000f,
    0.130000f,  0.000000f,  0.000000f,  0.570000f,  -0.600000f, 0.170000f,
    0.000000f,  -0.600000f, 0.000000f,  0.285805f,  0.000000f,  -0.958288f,
    0.130000f,  0.600000f,  -0.000000f, 0.700000f,  0.600000f,  0.750000f,
    0.530000f,  0.600000f,  0.750000f,  0.700000f,  0.600000f,  0.170000f,
    0.130000f,  0.600000f,  -0.000000f, 0.170000f,  0.000000f,  -0.580000f,
    -0.400000f, 0.000000f,  -0.750000f, 0.000000f,  1.000000f,  0.000000f,
    -0.050000f, 0.000000f,  0.000000f,  0.130000f,  0.600000f,  0.570000f,
    -0.050000f, 0.600000f,  0.570000f,  0.130000f,  0.600000f,  -0.000000f,
    0.130000f,  0.000000f,  0.000000f,  0.180000f,  0.000000f,  -0.570000f,
    0.180000f,  -0.600000f, -0.570000f, -0.953583f, 0.000000f,  -0.301131f,
    -0.050000f, 0.000000f,  0.570000f,  0.530000f,  0.600000f,  0.750000f,
    0.530000f,  0.600000f,  0.750000f,  -0.050000f, 0.600000f,  0.570000f,
    -0.050000f, 0.000000f,  0.570000f,  -0.580000f, 0.000000f,  -0.180000f,
    -0.580000f, -0.600000f, -0.180000f, -0.296399f, 0.000000f,  0.955064f,
    0.530000f,  0.000000f,  0.170000f,  0.700000f,  0.600000f,  0.750000f,
    0.700000f,  0.600000f,  0.170000f,  0.530000f,  0.600000f,  0.750000f,
    0.530000f,  0.000000f,  0.750000f,  -0.170000f, 0.000000f,  0.580000f,
    -0.170000f, -0.600000f, 0.580000f,  0.959629f,  0.000000f,  0.281270f,
    0.130000f,  0.000000f,  -0.000000f, 0.700000f,  0.600000f,  0.170000f,
    0.130000f,  0.600000f,  -0.000000f, 0.700000f,  0.600000f,  0.170000f,
    0.700000f,  0.000000f,  0.170000f,  0.570000f,  0.000000f,  0.170000f,
    0.570000f,  -0.600000f, 0.170000f,  0.285805f,  0.000000f,  -0.958288f};

static void init_scene(void) {
  if (triangles_ok)
    return;
  const float *p = raw_triangles;
  for (int i = 0; i < 32; i++) {
    
    vec3_t bbmin_t, bbmax_t, v0, v1, v2, normal;
    V3_ASSIGN_S3(bbmin_t, p[0], p[1], p[2]);
    V3_ASSIGN_S3(bbmax_t, p[3], p[4], p[5]);
    V3_ASSIGN_S3(v0, p[6], p[7], p[8]);
    V3_ASSIGN_S3(v1, p[9], p[10], p[11]);
    V3_ASSIGN_S3(v2, p[12], p[13], p[14]);
    V3_ASSIGN_S3(normal, p[21], p[22], p[23]); 

    V3_ASSIGN(triangles[i].bbmin, bbmin_t);
    V3_ASSIGN(triangles[i].bbmax, bbmax_t);
    V3_ASSIGN(triangles[i].v0, v0);
    V3_ASSIGN(triangles[i].v1, v1);
    V3_ASSIGN(triangles[i].v2, v2);
    V3_ASSIGN(triangles[i].normal, normal);

    V3_SUB(triangles[i].E1, v1, v0);
    V3_SUB(triangles[i].E2, v2, v0);

    p += 24;
  }
  triangles_ok = 1;
}

static uint32_t rgb565_to_rgb888(uint16_t c) {
  uint32_t r = (uint32_t)((c >> 11) & 0x1F);
  uint32_t g = (uint32_t)((c >> 5) & 0x3F);
  uint32_t b = (uint32_t)(c & 0x1F);
  r = (r * 255U + 15U) / 31U;
  g = (g * 255U + 31U) / 63U;
  b = (b * 255U + 15U) / 31U;
  return (r << 16) | (g << 8) | b;
}

static uint16_t tone_to_history565(vec3_t sample, uint16_t prev, uint32_t prev_count) {
  vec3_t one, mapped;
  V3_ASSIGN_S(one, 1.0f);
  V3_ADD(mapped, sample, one);
  V3_DIV_ASSIGN(sample, mapped);

  int r = (int)(sample.x * 31.0f);
  int g = (int)(sample.y * 63.0f);
  int b = (int)(sample.z * 31.0f);
  if (r < 0) r = 0; else if (r > 31) r = 31;
  if (g < 0) g = 0; else if (g > 63) g = 63;
  if (b < 0) b = 0; else if (b > 31) b = 31;

  if (prev_count > 0U) {
    uint32_t pr = (prev >> 11) & 0x1F;
    uint32_t pg = (prev >> 5) & 0x3F;
    uint32_t pb = prev & 0x1F;
    if (prev_count > 1024U) prev_count = 1024U;
    r = (int)((pr * prev_count + (uint32_t)r) / (prev_count + 1U));
    g = (int)((pg * prev_count + (uint32_t)g) / (prev_count + 1U));
    b = (int)((pb * prev_count + (uint32_t)b) / (prev_count + 1U));
  }

  return (uint16_t)(((uint16_t)r << 11) | ((uint16_t)g << 5) | (uint16_t)b);
}


static uint8_t bb_intersect(void) {
  V3_SUB(t0, bbmin, ray.start);
  V3_MUL_ASSIGN(t0, ray.inv_direction);
  V3_SUB(t1, bbmax, ray.start);
  V3_MUL_ASSIGN(t1, ray.inv_direction);
  V3_MIN(temp, t0, t1);
  tt = max(V3_COMPMAX(temp), 0);
  V3_MAX(temp, t0, t1);
  return V3_COMPMIN(temp) >= tt ? 1 : 0;
}
static uint8_t tb_intersect(triangle_t *s) {
  V3_SUB(t0, s->bbmin, ray.start);
  V3_MUL_ASSIGN(t0, ray.inv_direction);
  V3_SUB(t1, s->bbmax, ray.start);
  V3_MUL_ASSIGN(t1, ray.inv_direction);
  V3_MIN(temp, t0, t1);
  tt = max(V3_COMPMAX(temp), 0);
  V3_MAX(temp, t0, t1);
  return V3_COMPMIN(temp) >= tt ? 1 : 0;
}
static uint8_t tt_intersect(triangle_t *s) {
  vec3_t P;
  CROSS(P, ray.direction, s->E2);
  float det = DOT(P, s->E1);
  if (det < EPSILON && det > -EPSILON)
    return 0;
  float inv = 1.0f / det;
  vec3_t T_;
  V3_SUB(T_, ray.start, s->v0);
  float u = DOT(P, T_) * inv;
  if (u > 1 || u < 0)
    return 0;
  vec3_t Q;
  CROSS(Q, T_, s->E1);
  float v = DOT(Q, ray.direction) * inv;
  if (v > 1 || v < 0 || u + v > 1)
    return 0;
  tt = DOT(Q, s->E2) * inv;
  if (tt <= 0)
    return 0;
  return 1;
}
static void intersect(void) {
  intersection.t = FLT_MAX;
  intersection.i = -1;
  if (bb_intersect()) {
    for (int k = 0; k < 32; k++) {
      triangle_t *s = &triangles[k];
      if (!tb_intersect(s))
        continue;
      if (tt_intersect(s) && tt < intersection.t) {
        intersection.t = tt;
        intersection.i = k;
      }
    }
  }
}


static void Reflectance(int8_t i) {
  if (i == 8 || i == 9)
    V3_ASSIGN_S3(reflectance, 0.05f, 0.65f, 0.05f); 
  else if (i == 10 || i == 11)
    V3_ASSIGN_S3(reflectance, 0.65f, 0.05f, 0.05f); 
  else
    V3_ASSIGN_S(reflectance, 0.65f); 
}

static uint8_t sampleBSDF(void) {
  vec3_t wi, z;
  V3_ASSIGN_S3(z, T.m[2][0], T.m[2][1], T.m[2][2]);
  wi.z = DOT(z, ray.direction);
  if (wi.z <= 0)
    return 0;
  bsdf_pdf = wi.z * INV_PI;
  float tmp = INV_PI * abs(wi.z);
  Reflectance(interaction.i);
  V3_MUL_S(bsdf_absIdotN, reflectance, tmp);
  return bsdf_pdf > 0 ? 1 : 0;
}


static void linearCombination(void) {
  linear_r.x = DOT(linear_x, linear_t);
  linear_r.y = DOT(linear_y, linear_t);
  linear_r.z = DOT(linear_z, linear_t);
}
static void lightPoint(void) {
  float su = sqrtf(u0);
  float x = 1 - su, y = (1 - u1) * su, z = u1 * su;
  triangle_t *lt = &triangles[lightIdx];
  V3_ASSIGN_S3(linear_t, x, y, z);
  V3_ASSIGN_S3(linear_x, lt->v0.x, lt->v1.x, lt->v2.x);
  V3_ASSIGN_S3(linear_y, lt->v0.y, lt->v1.y, lt->v2.y);
  V3_ASSIGN_S3(linear_z, lt->v0.z, lt->v1.z, lt->v2.z);
  linearCombination();
  V3_ASSIGN(light_pos, linear_r);
}
static void makeInteraction(void) {
  interaction.i = intersection.i;
  interaction.t = intersection.t;
  interaction.surface = &triangles[intersection.i];
  V3_MAD_S(interaction.position, ray.start, ray.direction, intersection.t);
  V3_ASSIGN(interaction.normal, interaction.surface->normal);
  if (DOTM(ray.direction, interaction.normal) < 0)
    V3_MINUS(interaction.normal, interaction.normal);
  ORTHONORMALBASIS(interaction.normal);
}
static void cosWeightedHemi(void) {
  u0 = rand01();
  u1 = rand01();
  float r = sqrtf(u0);
  float az = u1 * TWO_PI;
  vec3_t v;
  V3_ASSIGN_S3(v, r * cosf(az), r * sinf(az), sqrtf(1 - u0));
  V3_MAD_S(ray.start, interaction.position, interaction.normal, EPSILON);
  vec3_t xx, yy, zz;
  V3_ASSIGN_S3(xx, T.m[0][0], T.m[1][0], T.m[2][0]);
  V3_ASSIGN_S3(yy, T.m[0][1], T.m[1][1], T.m[2][1]);
  V3_ASSIGN_S3(zz, T.m[0][2], T.m[1][2], T.m[2][2]);
  ray.direction.x = DOT(xx, v);
  ray.direction.y = DOT(yy, v);
  ray.direction.z = DOT(zz, v);
  V3_RCP(ray.inv_direction, ray.direction);
}
static void sampleLight(void) {
  V3_ASSIGN_S(radiance, 0);
  u0 = rand01();
  u1 = rand01();
  u2 = rand01();
  lightIdx = u0 < 0.5f ? 0 : 1;
  lightPoint();
  V3_MAD_S(ray.start, interaction.position, interaction.normal, EPSILON);
  V3_SUB(ray.direction, light_pos, ray.start);
  NORMALIZE(ray.direction);
  V3_RCP(ray.inv_direction, ray.direction);
  triangle_t *lt = &triangles[lightIdx];
  float clt = DOTM(ray.direction, lt->normal);
  if (clt <= 0)
    return;
  float cth = DOT(ray.direction, interaction.normal);
  if (cth <= 0)
    return;
  intersect();
  if (intersection.i == -1 || intersection.i != lightIdx)
    return;
  if (!sampleBSDF())
    return;
  float lpdf = (intersection.t * intersection.t) / (LIGHT_AREA * clt);
  float mw = BALANCEHEURISTIC(lpdf, bsdf_pdf);
  V3_MUL(radiance, bsdf_absIdotN, lightColor);
  V3_MUL_ASSIGN_S(radiance, mw);
  V3_DIV_ASSIGN_S(radiance, lpdf);
  V3_DIV_ASSIGN_S(radiance, 0.5f);
}
static vec3_t sampleRay(void) {
  vec3_t sample;
  V3_ASSIGN_S(sample, 0);
  vec3_t throughput;
  V3_ASSIGN_S(throughput, 1);
  int depth = 0;
  while (1) {
    intersect();
    if (intersection.i == -1) {
      V3_ADD_ASSIGN(sample, throughput);
      return sample;
    }
    if (intersection.i < 2) {
      if (depth == 0)
        V3_ADD_ASSIGN(sample, lightColor);
      return sample;
    }
    makeInteraction();
    sampleLight();
    V3_MAD_ASSIGN(sample, radiance, throughput);
    depth++;
    cosWeightedHemi();
    if (!sampleBSDF())
      return sample;
    V3_MUL_ASSIGN(throughput, bsdf_absIdotN);
    V3_DIV_ASSIGN_S(throughput, bsdf_pdf);
    if (depth >= MAX_DEPTH)
      return sample;
  }
}


void render_init(void) {
  init_scene();
  V3_ASSIGN_S3(eye, 0, 1, 3.5f);
  V3_ASSIGN_S3(center, 0, 1, 0);
  V3_ASSIGN_S3(up, 0, 1, 0);
  V3_ASSIGN_S3(lightColor, 200, 200, 200);
  V3_ASSIGN_S3(bbmin, -1, 0, -1);
  V3_ASSIGN_S3(bbmax, 1, 2, 1);
  LOOKAT(view, eye, center, up);
  V3_ASSIGN_S3(view_x, view.m[0][0], view.m[0][1], view.m[0][2]);
  V3_ASSIGN_S3(view_y, view.m[1][0], view.m[1][1], view.m[1][2]);
  V3_ASSIGN_S3(view_z, view.m[2][0], view.m[2][1], view.m[2][2]);
  current_x = current_y = frame_count = 0;
  memset(pixel_history, 0, sizeof(pixel_history));
  srand(12345);
}


int render_step(pixel_callback_t pixel_cb) {
  int x = current_x, y = current_y;
  int w = RENDER_WIDTH, h = RENDER_HEIGHT;
  float alpha_rad = radians(45);
  float Zc = -(h * 0.5f) / tanf(alpha_rad * 0.5f);

  float Xw = x + rand01(), Yw = (h - 1 - y) + rand01();
  float Xc = Xw - w * 0.5f, Yc = Yw - h * 0.5f;
  V3_ASSIGN_S3(linear_t, Xc, Yc, Zc);
  V3_ASSIGN_S3(linear_x, view_x.x, view_y.x, view_z.x);
  V3_ASSIGN_S3(linear_y, view_x.y, view_y.y, view_z.y);
  V3_ASSIGN_S3(linear_z, view_x.z, view_y.z, view_z.z);
  linearCombination();
  V3_ASSIGN(ray.direction, linear_r);
  V3_ASSIGN(ray.start, eye);
  NORMALIZE(ray.direction);
  V3_RCP(ray.inv_direction, ray.direction);

  vec3_t sp = sampleRay();
  int idx = y * w + x;
  uint16_t avg = tone_to_history565(sp, pixel_history[idx], (uint32_t)frame_count);
  pixel_history[idx] = avg;
  if (pixel_cb) pixel_cb(x, y, rgb565_to_rgb888(avg));

  current_x++;
  if (current_x >= w) {
    current_x = 0;
    current_y++;
  }
  int done = 0;
  if (current_y >= h) {
    current_y = 0;
    frame_count++;
    done = 1;
  }
  render_progress = (y + 1) * 100 / h;
  return done;
}

int get_render_progress(void) { return render_progress; }

uint16_t render_get_pixel565(int x, int y) {
  if (x < 0 || x >= RENDER_WIDTH || y < 0 || y >= RENDER_HEIGHT)
    return 0;
  return pixel_history[y * RENDER_WIDTH + x];
}

int render_get_frame_count(void) { return frame_count; }
