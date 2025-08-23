#ifndef VEC3_HH
#define VEC3_HH

#include <SDL2\SDL.h>
#include <cmath>
#include <ostream>


// IN THIS HOUSE, WE USE 1 TRUE SYSTEM OF AXIES:
//  X IS TO THE RIGHT
//  Y IS DOWN
//  Z IS FORWARD
template <typename T>
struct vec3
{
    T x, y, z;

    constexpr vec3<T>& operator+=(const vec3<T>& vec)
    {
        x += vec.x;
        y += vec.y;
        z += vec.z;
        return *this;
    }
    constexpr vec3<T>& operator-=(const vec3<T>& vec)
    {
        x -= vec.x;
        y -= vec.y;
        z -= vec.z;
        return *this;
    }
    template <typename U>
    constexpr vec3<T>& operator/=(U d)
    {
        x /= d;
        y /= d;
        z /= d;
        return *this;
    }
    template <typename U>
    constexpr vec3<T>& operator*=(U m)
    {
        x *= m;
        y *= m;
        z *= m;
        return *this;
    }
    constexpr vec3<T> operator-() const
    {
        vec3<T> vec{ 0, 0, 0 };
        return vec -= *this;
    }

    constexpr operator SDL_FPoint() const { return { x, y }; }
};

template<typename T> std::ostream& operator<<(std::ostream& os, const vec3<T>& vec) { return os << vec.x << ", " << vec.y << ", " << vec.z; }

template<typename T> constexpr vec3<T> operator+(vec3<T> vec1, const vec3<T>& vec2) { return vec1 += vec2; }

template<typename T> constexpr vec3<T> operator-(vec3<T> vec1, const vec3<T>& vec2) { return vec1 -= vec2; }

/** @return the scalar quotient of vec and div */
template<typename T, typename U> constexpr vec3<T> operator/(vec3<T> vec, U div) { return vec /= div; }

/** @return the scalar product of vec and mul */
template<typename T, typename U> constexpr vec3<T> operator*(vec3<T> vec, U mul) { return vec *= mul; }

/** @return the dot product of v1 and v2 */
template<typename T> constexpr T dot(const vec3<T>& v1, const vec3<T>& v2) { return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z; }

/** @return the cross product of v1 and v2 */
template<typename T> constexpr vec3<T> cross(const vec3<T>& a, const vec3<T>& b) { return { a.y * b.z - b.y * a.z, a.z * b.x - b.z * a.x, a.x * b.y - b.x * a.y }; }

/** @return the projection of v1 onto v2 */
template<typename T> constexpr vec3<T> proj(const vec3<T>& v1, const vec3<T>& v2) { return v2 * dot(v1, v2) / dot(v2, v2); }

/** @return the magnitude of vec squared */
template<typename T> constexpr T mag_squared(const vec3<T>& vec) { return dot(vec, vec); }

/** @return the magnitude of vec */
template<typename T> constexpr T mag(const vec3<T>& vec) { return sqrtf(mag_squared(vec)); }

/** @return vec with its magnitude clamped between min and max */
template<typename T> constexpr vec3<T> clamp_mag(const vec3<T>& vec, T min, T max) { return mag(vec) > max ? normal(vec) * max : mag(vec) < min ? normal(vec) * min : vec; }

/** @return vec normalized to a unit vector. if the magnitude of vec is zero, return the zero vector */
template<typename T> constexpr vec3<T> normal(const vec3<T>& vec, const vec3<T>& zero = { 0, 0, 0 }) { return mag(vec) != 0 ? vec / mag(vec) : zero; }

/** @return the distance squared between p1 and p2 */
template<typename T> constexpr T dist_squared(const vec3<T>& p1, const vec3<T>& p2) { return mag_squared(p2 - p1); }

/** @return the distance between p1 and p2 */
template<typename T> constexpr T dist(const vec3<T>& p1, const vec3<T>& p2) { return std::sqrt(dist_squared(p1, p2)); }

typedef vec3<float> vec3f;
typedef vec3<int> vec3i;

#endif