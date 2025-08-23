#ifndef QUAT_HH
#define QUAT_HH

#include "vec3.hh"
#include "util.hh"

struct quaternion {
    float w, x, y, z;

    constexpr quaternion(float w, float x, float y, float z)
        : w{ w }, x{ x }, y{ y }, z{ z } {
    };

    constexpr quaternion(float w, const vec3f& vec)
        : w{ w }, x{ vec.x }, y{ vec.y }, z{ vec.z } {
    }

    static constexpr quaternion axis_angle(const vec3f& axis, float angle) {
        return { std::cos(angle), normal(axis) * std::sin(angle) };
    }

    constexpr quaternion& operator+=(const quaternion& quat)
    {
        w += quat.w;
        x += quat.x;
        y += quat.y;
        z += quat.z;
        return *this;
    }
    constexpr quaternion& operator-=(const quaternion& quat)
    {
        w -= quat.w;
        x -= quat.x;
        y -= quat.y;
        z -= quat.z;
        return *this;
    }
    constexpr quaternion& operator/=(float div)
    {
        w /= div;
        x /= div;
        y /= div;
        z /= div;
        return *this;
    }
    constexpr quaternion& operator*=(float mul)
    {
        w *= mul;
        x *= mul;
        y *= mul;
        z *= mul;
        return *this;
    }
    constexpr quaternion operator-() const
    {
        quaternion quat{ 0, 0, 0, 0 };
        return quat -= *this;
    }

};


constexpr quaternion operator+(quaternion q1, const quaternion& q2) { return q1 += q2; }
constexpr quaternion operator-(quaternion q1, const quaternion& q2) { return q1 -= q2; }
constexpr quaternion operator/(quaternion quat, float div) { return quat /= div; }
constexpr quaternion operator*(quaternion quat, float mul) { return quat *= mul; }

/** @return the dot product of q1 and q2 */
constexpr float dot(const quaternion& q1, const quaternion& q2) {
    return q1.w * q2.w + q1.x * q2.x + q1.y * q2.y + q1.z * q2.z;
}

/** @return the conjugate quat, q* */
constexpr quaternion conj(const quaternion& quat) {
    return { quat.w, -quat.x, -quat.y, -quat.z };
}

/** @return the norm squared of quat, ||q||^2 */
constexpr float norm_squared(const quaternion& quat) {
    return dot(quat, quat);
}

/** @return the norm of quat, ||q|| */
constexpr float norm(const quaternion& quat) {
    return std::sqrt(norm_squared(quat));
}

/** @return the reciprocal (multiplicative inverse) of quat, q^-1 */
constexpr quaternion recip(const quaternion& quat) {
    return conj(quat) / norm_squared(quat);
}

/** @return the unit (versor) of quat, uq */
constexpr quaternion unit(const quaternion& quat) {
    return quat / norm(quat);
}

/** @return the composition of q1 and q2 */
constexpr quaternion comp(const quaternion& q1, const quaternion& q2) {
    return {
        (q1.w * q2.w) - (q1.x * q2.x) - (q1.y * q2.y) - (q1.z * q2.z),
        (q1.w * q2.x) + (q1.x * q2.w) + (q1.y * q2.z) - (q1.z * q2.y),
        (q1.w * q2.y) - (q1.x * q2.z) + (q1.y * q2.w) + (q1.z * q2.x),
        (q1.w * q2.z) + (q1.x * q2.y) - (q1.y * q2.x) + (q1.z * q2.w),
    };
}

/** @return quat vec quat^-1 */
constexpr vec3f apply(const vec3f& vec, const quaternion& quat) {
    // apply the quaternion sandwich
    quaternion ret = comp(comp(quat, { 0, vec }), recip(quat));
    return { ret.x, ret.y, ret.z };
}

/** spherical linear interpolation */
constexpr quaternion slerp(const quaternion& q1, const quaternion& q2, float t) {
    float a = std::acos(dot(q1, q2));
    if (a == 0) { return lerp(q1, q2, t); }
    float s = std::sin(a);
    return q1 * (std::sin((1 - t) * a) / s) + q2 * (std::sin(t * a) / s);
}

/** normalized linear interpolation */
constexpr quaternion nlerp(const quaternion& q1, const quaternion& q2, float t) {
    return unit(q1 * (1 - t) + q2 * std::copysign(t, dot(q1, q2)));
}

/**
 * @brief fast slerp approximation
 * from https://zeux.io/2016/05/05/optimizing-slerp/
 */
constexpr quaternion onlerp(const quaternion& q1, const quaternion& q2, float t) {
    float ca = dot(q1, q2);
    float d = std::abs(ca);
    float A = 1.0904f + d * (-3.2452f + d * (3.55645f - d * 1.43519f));
    float B = 0.848013f + d * (-1.06021f + d * 0.215638f);
    float k = A * (t - 0.5f) * (t - 0.5f) + B;
    float ot = t + t * (t - 0.5f) * (t - 1) * k;
    return unit(q1 * (1 - ot) + q2 * std::copysign(ot, ca));
}

#endif