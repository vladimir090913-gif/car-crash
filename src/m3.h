#pragma once
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include "raylib.h"

struct V3 {
    float x=0, y=0, z=0;
    V3() {}
    V3(float a, float b, float c) : x(a), y(b), z(c) {}
    V3 operator+(const V3& o) const { return {x+o.x, y+o.y, z+o.z}; }
    V3 operator-(const V3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    V3 operator*(float s)     const { return {x*s, y*s, z*s}; }
    V3 operator-()            const { return {-x, -y, -z}; }
    V3& operator+=(const V3& o) { x+=o.x; y+=o.y; z+=o.z; return *this; }
    V3& operator-=(const V3& o) { x-=o.x; y-=o.y; z-=o.z; return *this; }
    V3& operator*=(float s)     { x*=s; y*=s; z*=s; return *this; }
};

inline float dot(V3 a, V3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline V3 cross(V3 a, V3 b) {
    return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}
inline float len2(V3 a) { return dot(a, a); }
inline float len(V3 a)  { return std::sqrt(dot(a, a)); }
inline V3 norm(V3 a) { float l = len(a); return l > 1e-9f ? a*(1.f/l) : V3(0,1,0); }
inline V3 lerp(V3 a, V3 b, float t) { return a + (b-a)*t; }
inline float clampf(float v, float a, float b) { return v<a ? a : (v>b ? b : v); }
inline float signf(float v) { return v < 0.f ? -1.f : 1.f; }
inline float frand(float a, float b) { return a + (b-a)*(rand()/(float)RAND_MAX); }
inline V3 randDir() { return norm(V3(frand(-1,1), frand(-1,1), frand(-1,1))); }
inline Vector3 rv(V3 v) { return Vector3{v.x, v.y, v.z}; }
inline float smoothTo(float cur, float tgt, float rate, float dt) {
    return cur + (tgt-cur)*(1.f - std::exp(-rate*dt));
}