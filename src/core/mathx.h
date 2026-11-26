#pragma once
// Small vector and matrix types. Matrices are column-major, as OpenGL reads them.

#include <cmath>

#include "core/types.h"

namespace gt {

struct V2 {
    float x = 0, y = 0;
    constexpr V2() = default;
    constexpr V2(float x_, float y_) : x(x_), y(y_) {}
};
inline V2 operator+(V2 a, V2 b) { return {a.x + b.x, a.y + b.y}; }
inline V2 operator-(V2 a, V2 b) { return {a.x - b.x, a.y - b.y}; }
inline V2 operator*(V2 a, float s) { return {a.x * s, a.y * s}; }

struct V3 {
    float x = 0, y = 0, z = 0;
    constexpr V3() = default;
    constexpr V3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
};
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline V3 operator*(V3 a, V3 b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float length(V3 a) { return std::sqrt(dot(a, a)); }
inline V3 normalize(V3 a) {
    const float l = length(a);
    return l > 0 ? a * (1.0f / l) : a;
}
inline V3 lerp(V3 a, V3 b, float t) { return a + (b - a) * t; }

struct V4 {
    float x = 0, y = 0, z = 0, w = 0;
    constexpr V4() = default;
    constexpr V4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    constexpr V4(V3 v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
};

struct M4 {
    float m[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    float& at(int row, int col) { return m[col * 4 + row]; }
    float at(int row, int col) const { return m[col * 4 + row]; }
};

inline M4 operator*(const M4& a, const M4& b) {
    M4 r;
    for (int c = 0; c < 4; ++c)
        for (int rr = 0; rr < 4; ++rr) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.at(rr, k) * b.at(k, c);
            r.at(rr, c) = s;
        }
    return r;
}

inline V4 mul(const M4& a, V4 v) {
    return {a.at(0, 0) * v.x + a.at(0, 1) * v.y + a.at(0, 2) * v.z + a.at(0, 3) * v.w,
            a.at(1, 0) * v.x + a.at(1, 1) * v.y + a.at(1, 2) * v.z + a.at(1, 3) * v.w,
            a.at(2, 0) * v.x + a.at(2, 1) * v.y + a.at(2, 2) * v.z + a.at(2, 3) * v.w,
            a.at(3, 0) * v.x + a.at(3, 1) * v.y + a.at(3, 2) * v.z + a.at(3, 3) * v.w};
}

inline M4 translation(V3 t) {
    M4 r;
    r.at(0, 3) = t.x;
    r.at(1, 3) = t.y;
    r.at(2, 3) = t.z;
    return r;
}

inline M4 scaling(V3 s) {
    M4 r;
    r.at(0, 0) = s.x;
    r.at(1, 1) = s.y;
    r.at(2, 2) = s.z;
    return r;
}

inline M4 rotationY(float a) {
    M4 r;
    const float c = std::cos(a), s = std::sin(a);
    r.at(0, 0) = c;
    r.at(0, 2) = s;
    r.at(2, 0) = -s;
    r.at(2, 2) = c;
    return r;
}

inline M4 rotationZ(float a) {
    M4 r;
    const float c = std::cos(a), s = std::sin(a);
    r.at(0, 0) = c;
    r.at(0, 1) = -s;
    r.at(1, 0) = s;
    r.at(1, 1) = c;
    return r;
}

inline M4 ortho(float l, float r, float b, float t) {
    M4 o;
    o.at(0, 0) = 2 / (r - l);
    o.at(1, 1) = 2 / (t - b);
    o.at(2, 2) = -1;
    o.at(0, 3) = -(r + l) / (r - l);
    o.at(1, 3) = -(t + b) / (t - b);
    return o;
}

inline float smoothstep(float a, float b, float x) {
    const float t = clampv((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}
inline float mixf(float a, float b, float t) { return a + (b - a) * t; }

constexpr float kPi = 3.14159265358979f;

}  // namespace gt
