#pragma once
#include <cmath>

struct Vec2 { float x = 0.0F, y = 0.0F; };

inline Vec2 operator+(Vec2 left, Vec2 right) { return {left.x + right.x, left.y + right.y}; }
inline Vec2 operator-(Vec2 left, Vec2 right) { return {left.x - right.x, left.y - right.y}; }
inline Vec2 operator*(Vec2 vector, float scale) { return {vector.x * scale, vector.y * scale}; }
inline Vec2& operator+=(Vec2& left, Vec2 right) { left = left + right; return left; }
inline float Length(Vec2 vector) { return std::sqrt(vector.x * vector.x + vector.y * vector.y); }
inline float Distance(Vec2 left, Vec2 right) { return Length(left - right); }
inline Vec2 Normalize(Vec2 vector)
{
    const float length = Length(vector);
    return length > 0.0F ? vector * (1.0F / length) : Vec2{};
}
inline Vec2 ClampToMap(Vec2 position)
{
    position.x = std::fmax(18.0F, std::fmin(1262.0F, position.x));
    position.y = std::fmax(18.0F, std::fmin(702.0F, position.y));
    return position;
}
