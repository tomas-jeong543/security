#pragma once
#include "MathTypes.h"
#include <algorithm>
namespace Camera
{
constexpr float width=1280.0F, height=720.0F;
inline Vec2 Offset(Vec2 player)
{
    return {std::clamp(player.x-width/2,0.0F,worldWidth-width),
            std::clamp(player.y-height/2,0.0F,worldHeight-height)};
}
inline Vec2 WorldToScreen(Vec2 world, Vec2 offset) { return world-offset; }
inline Vec2 ScreenToWorld(Vec2 screen, Vec2 offset) { return screen+offset; }
}
