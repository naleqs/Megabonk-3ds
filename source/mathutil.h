#pragma once
#include <math.h>
#include <stdlib.h>
#include "game_types.h"

static inline vec3 v3(float x, float y, float z) { return (vec3){ x, y, z }; }
static inline vec3 v3_add(vec3 a, vec3 b) { return v3(a.x + b.x, a.y + b.y, a.z + b.z); }
static inline vec3 v3_sub(vec3 a, vec3 b) { return v3(a.x - b.x, a.y - b.y, a.z - b.z); }
static inline vec3 v3_scale(vec3 a, float s) { return v3(a.x * s, a.y * s, a.z * s); }
static inline float v3_dot(vec3 a, vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
static inline float v3_len(vec3 a) { return sqrtf(v3_dot(a, a)); }
static inline vec3 v3_norm(vec3 a)
{
	float l = v3_len(a);
	return l > 1e-6f ? v3_scale(a, 1.0f / l) : v3(0, 0, 0);
}
// distance on the ground plane
static inline float dist2_xz(vec3 a, vec3 b)
{
	float dx = a.x - b.x, dz = a.z - b.z;
	return dx * dx + dz * dz;
}
// squared ground-plane distance from p to the segment a-b
static inline float seg_dist2_xz(vec3 a, vec3 b, vec3 p)
{
	float dx = b.x - a.x, dz = b.z - a.z;
	float len2 = dx * dx + dz * dz;
	float t = len2 > 1e-6f ? ((p.x - a.x) * dx + (p.z - a.z) * dz) / len2 : 0;
	t = t < 0 ? 0 : t > 1 ? 1 : t;
	float cx = a.x + dx * t - p.x, cz = a.z + dz * t - p.z;
	return cx * cx + cz * cz;
}
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline float randf(void) { return (float)rand() / (float)RAND_MAX; }
static inline float randrange(float a, float b) { return a + (b - a) * randf(); }
static inline float yaw_of(vec3 dir) { return atan2f(dir.x, dir.z); }
static inline vec3 dir_of_yaw(float yaw) { return v3(sinf(yaw), 0, cosf(yaw)); }
static inline float angle_diff(float a, float b)
{
	float d = fmodf(b - a + (float)M_PI, 2.0f * (float)M_PI);
	if (d < 0) d += 2.0f * (float)M_PI;
	return d - (float)M_PI;
}
