#ifndef VEC_HH
#define VEC_HH

#include <SDL2\SDL.h>
#include <cmath>

struct vec2f
{
    float x, y;

    constexpr vec2f& operator+=(const vec2f& vec)
    {
        x += vec.x;
        y += vec.y;
        return *this;
    }
    constexpr vec2f& operator-=(const vec2f& vec)
    {
        x -= vec.x;
        y -= vec.y;
        return *this;
    }
    constexpr vec2f& operator/=(float d)
    {
        x /= d;
        y /= d;
        return *this;
    }
    constexpr vec2f& operator*=(float m)
    {
        x *= m;
        y *= m;
        return *this;
    }
    constexpr vec2f operator-() const
    {
        vec2f vec{0, 0};
        return vec -= *this;
    }

    constexpr operator SDL_FPoint() const { return { x, y }; }
};


std::ostream& operator<<(std::ostream& os, const vec2f& vec) { return os << vec.x << ", " << vec.y; }

constexpr vec2f operator+(vec2f vec1, const vec2f& vec2) { return vec1 += vec2; }
constexpr vec2f operator-(vec2f vec1, const vec2f& vec2) { return vec1 -= vec2; }
constexpr vec2f operator/(vec2f vec, float d) { return vec /= d; }
constexpr vec2f operator*(vec2f vec, float m) { return vec *= m; }

constexpr float dist_squared(const vec2f& p1, const vec2f& p2) { return (p2.x - p1.x) * (p2.x - p1.x) + (p2.y - p1.y) * (p2.y - p1.y); }
constexpr float dist(const vec2f& p1, const vec2f& p2) { return std::sqrt(dist_squared(p1, p2)); }
constexpr float dot(const vec2f& v1, const vec2f& v2) { return v1.x * v2.x + v1.y * v2.y; }
/** @return the magnitude of the cross product of v1 and v2 */
constexpr float cross_mag(const vec2f& v1, const vec2f& v2) { return v1.x * v2.y - v1.y * v2.x; } 
/** @return the projection of v1 onto v2 */
constexpr vec2f proj(const vec2f& v1, const vec2f& v2) { return v2 * dot(v1, v2) / dot(v2, v2); }
constexpr float mag(const vec2f& vec) { return sqrtf(vec.x * vec.x + vec.y * vec.y); }
/** @return vec normalized to a unit vector. if the magnitude of vec is zero, return the zero vector */
constexpr vec2f normal(const vec2f& vec, const vec2f& zero = { 0, 0 }) { return mag(vec) != 0 ? vec / mag(vec) : zero; }
constexpr vec2f clamp_mag(const vec2f& vec, float min, float max) { return mag(vec) > max ? normal(vec) * max : mag(vec) < min ? normal(vec) * min : vec; }
/** @return vec rotated by angle radians */
constexpr vec2f rotate(const vec2f& vec, float angle) { return { vec.x * std::cos(angle) - vec.y * std::sin(angle), vec.x * std::sin(angle) + vec.y * std::cos(angle) }; }

/** Compute the shortest vector from pos to the interior of rect */
constexpr vec2f vec_to(vec2f pos, SDL_FRect rect)
{
    vec2f result = { 0, 0 };
    if (pos.x < rect.x) { result.x = rect.x - pos.x; }
    if (pos.y < rect.y) { result.y = rect.y - pos.y; }
    if (pos.x > rect.x + rect.w) { result.x = rect.x + rect.w - pos.x; }
    if (pos.y > rect.y + rect.h) { result.y = rect.y + rect.h - pos.y; }
    return result;
}

#endif