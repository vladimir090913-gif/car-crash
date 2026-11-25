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
    V3 operator-(const V3& o) const { return {x-o.x, y-o.y, z+ -o.z}; }
    V3 operator*(float s)     const { return {x*s, y*s, z*s}; }
    V3 operator-()            const { return {-x, -y, -z}; }
    V3& operator+=(const V3& o) { x+=o.x; y+=o.y; z+=o.z; return *this; }
    V3& operator-=(const V3& o) { x-=o.x; y-=o.y; z-=o.z; return *this; }
    V3& operator*=(float s)     { x*=s; y*=s; z*=s; return *this; }
};