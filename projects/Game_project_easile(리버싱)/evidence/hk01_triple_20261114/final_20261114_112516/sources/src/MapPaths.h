#pragma once

#include "MathTypes.h"
#include "Hero.h"
#include <array>
#include <algorithm>
#include <limits>

namespace MapPaths
{
constexpr float laneY[3] = {360.0F, 720.0F, 1080.0F};
constexpr Vec2 blueBase{105.0F, 720.0F};
constexpr Vec2 redBase{1815.0F, 720.0F};
constexpr Vec2 blueFork{255.0F, 720.0F};
constexpr Vec2 redFork{1665.0F, 720.0F};
constexpr float blueTowerX = 390.0F, redTowerX = 1530.0F;
constexpr Vec2 healPosition{960.0F, 540.0F};
constexpr float roadWidth = 280.0F;
constexpr float roadInnerWidth = 272.0F;
constexpr float heroRadius = 26.0F;
constexpr float minionRadius = 12.0F;
struct Wall { Vec2 start, end; float halfWidth; };
constexpr std::array<Wall, 4> walls{{
    {{450.0F, 540.0F}, {850.0F, 540.0F}, 30.0F},
    {{1070.0F, 540.0F}, {1470.0F, 540.0F}, 30.0F},
    {{450.0F, 900.0F}, {850.0F, 900.0F}, 30.0F},
    {{1070.0F, 900.0F}, {1470.0F, 900.0F}, 30.0F}
}};
struct Region { Vec2 center, size; };
// Open chambers separated by solid islands, with a narrow central lane connector.
constexpr std::array<Region, 2> jungle{{
    {{960, 540}, {220, 160}}, {{960, 900}, {220, 160}}
}};

inline std::array<Vec2, 6> Route(Team team, int lane)
{
    const float y = laneY[lane];
    if (team == Team::Blue)
    {
        return {{{205.0F, 720.0F}, blueFork, {blueTowerX, y}, {redTowerX, y}, redFork, redBase}};
    }
    return {{{1715.0F, 720.0F}, redFork, {redTowerX, y}, {blueTowerX, y}, blueFork, blueBase}};
}

template <typename Visit>
inline void ForEachJungleSegment(Visit visit)
{
    visit(Vec2{960,360}, Vec2{960,1080});
}

template <typename Visit>
inline void ForEachRoadSegment(Visit visit)
{
    for (int lane = 0; lane < 3; ++lane)
    {
        const auto route = Route(Team::Blue, lane);
        visit(blueBase, route[0]);
        for (int i = 0; i < 5; ++i) { visit(route[i], route[i + 1]); }
    }
}

inline Vec2 ClosestOnSegment(Vec2 point, Vec2 a, Vec2 b)
{
    const Vec2 axis = b - a;
    const float lengthSquared = axis.x * axis.x + axis.y * axis.y;
    if (lengthSquared <= 0.0F) { return a; }
    const float t = std::clamp(((point.x-a.x)*axis.x + (point.y-a.y)*axis.y) / lengthSquared, 0.0F, 1.0F);
    return a + axis * t;
}

inline float SegmentBoxHit(Vec2 start, Vec2 end, Vec2 minimum, Vec2 maximum)
{
    const Vec2 delta = end - start;
    float enter = 0.0F, leave = 1.0F;
    const auto clip = [&](float origin, float travel, float low, float high)
    {
        if (std::abs(travel) < 0.000001F) { return origin >= low && origin <= high; }
        float nearTime = (low - origin) / travel, farTime = (high - origin) / travel;
        if (nearTime > farTime) { std::swap(nearTime, farTime); }
        enter = std::max(enter, nearTime);
        leave = std::min(leave, farTime);
        return enter <= leave;
    };
    return clip(start.x, delta.x, minimum.x, maximum.x) &&
           clip(start.y, delta.y, minimum.y, maximum.y)
        ? enter : std::numeric_limits<float>::infinity();
}

inline float FirstWallHit(Vec2 start, Vec2 end, float radius = 0.0F)
{
    float first = std::numeric_limits<float>::infinity();
    for (const Wall& wall : walls)
    {
        first = std::min(first, SegmentBoxHit(start, end,
            {wall.start.x - radius, wall.start.y - wall.halfWidth - radius},
            {wall.end.x + radius, wall.end.y + wall.halfWidth + radius}));
    }
    return first;
}

inline bool WallBlocks(Vec2 start, Vec2 end, float radius = 0.0F)
{
    return FirstWallHit(start, end, radius) <= 1.0F;
}

inline int CombatLane(Vec2 point)
{
    if (point.x<450 || point.x>1470) { return -1; }
    for (int lane=0;lane<3;++lane)
    { if (std::abs(point.y-laneY[lane])<=roadWidth*0.5F) { return lane; } }
    return -1;
}

inline bool AttackBlocked(Vec2 start, Vec2 target)
{
    const int sourceLane=CombatLane(start), targetLane=CombatLane(target);
    return WallBlocks(start,target) || (sourceLane>=0 && targetLane>=0 && sourceLane!=targetLane);
}

inline Vec2 ClosestRoadCenter(Vec2 point)
{
    Vec2 nearest = blueBase;
    float best = std::numeric_limits<float>::max();
    ForEachRoadSegment([&](Vec2 a, Vec2 b)
    {
        const Vec2 candidate = ClosestOnSegment(point, a, b);
        const float distance = Distance(point, candidate);
        if (distance < best) { best = distance; nearest = candidate; }
    });
    return nearest;
}

inline bool Contains(Vec2 point, float bodyRadius)
{
    if (point.x < bodyRadius || point.x > worldWidth-bodyRadius ||
        point.y < bodyRadius || point.y > worldHeight-bodyRadius) { return false; }
    bool onTerrain = Distance(point, ClosestRoadCenter(point)) <= roadWidth * 0.5F - bodyRadius + 0.001F;
    ForEachJungleSegment([&](Vec2 a, Vec2 b)
    { onTerrain |= Distance(point, ClosestOnSegment(point,a,b)) <= 70.0F-bodyRadius; });
    for (const Region& region : jungle)
    {
        onTerrain |= std::abs(point.x-region.center.x) <= region.size.x/2-bodyRadius &&
                     std::abs(point.y-region.center.y) <= region.size.y/2-bodyRadius;
    }
    if (!onTerrain) { return false; }
    for (const Wall& wall : walls)
    {
        if (point.x > wall.start.x - bodyRadius && point.x < wall.end.x + bodyRadius &&
            point.y > wall.start.y - wall.halfWidth - bodyRadius &&
            point.y < wall.start.y + wall.halfWidth + bodyRadius) { return false; }
    }
    return true;
}

inline Vec2 Project(Vec2 point, float bodyRadius)
{
    if (Contains(point, bodyRadius)) { return point; }
    Vec2 closest = ClosestRoadCenter(point);
    ForEachJungleSegment([&](Vec2 a, Vec2 b)
    {
        const Vec2 candidate = ClosestOnSegment(point,a,b);
        if (Distance(point,candidate) < Distance(point,closest)) { closest = candidate; }
    });
    const Vec2 offset = point - closest;
    const float limit = roadWidth * 0.5F - bodyRadius;
    const float distance = Length(offset);
    const Vec2 candidate = distance <= limit ? point : closest + offset * (limit / distance);
    return Contains(candidate,bodyRadius) ? candidate : closest;
}

inline bool WalkableSegment(Vec2 a, Vec2 b, float radius)
{
    if (WallBlocks(a,b,radius)) { return false; }
    const int steps = std::max(1, static_cast<int>(std::ceil(Distance(a,b)/12.0F)));
    for (int i=0;i<=steps;++i)
    { if (!Contains(a+(b-a)*(static_cast<float>(i)/steps),radius)) { return false; } }
    return true;
}

inline Vec2 NavigationWaypoint(Vec2 start, Vec2 target, float radius)
{
    target = Project(target,radius);
    if (WalkableSegment(start,target,radius)) { return target; }
    std::array<Vec2, 17> nodes{{start,target,blueBase,redBase,blueFork,redFork,
        {blueTowerX,laneY[0]}, {blueTowerX,laneY[1]}, {blueTowerX,laneY[2]},
        {redTowerX,laneY[0]}, {redTowerX,laneY[1]}, {redTowerX,laneY[2]},
        {960,360},{960,540},{960,720},{960,900},{960,1080}}};
    std::array<float,17> cost; cost.fill(std::numeric_limits<float>::infinity());
    std::array<int,17> parent; parent.fill(-1);
    std::array<bool,17> visited{}; cost[0]=0;
    for (size_t iteration=0;iteration<nodes.size();++iteration)
    {
        int next=-1;
        for (int i=0;i<static_cast<int>(nodes.size());++i)
        { if (!visited[i] && (next<0 || cost[i]<cost[next])) { next=i; } }
        if (next<0 || !std::isfinite(cost[next])) { break; }
        if (next==1) { break; }
        visited[next]=true;
        for (int i=0;i<static_cast<int>(nodes.size());++i)
        {
            const float candidate=cost[next]+Distance(nodes[next],nodes[i]);
            if (!visited[i] && candidate<cost[i] && WalkableSegment(nodes[next],nodes[i],radius))
            { cost[i]=candidate; parent[i]=next; }
        }
    }
    if (parent[1]<0) { return start; }
    int next=1;
    while (parent[next]>0) { next=parent[next]; }
    return nodes[next];
}

inline Vec2 MoveInside(Vec2 start, Vec2 displacement, float bodyRadius)
{
    // Small steps stop a fast entity from tunnelling across the gap between lanes.
    const int steps = std::max(1, static_cast<int>(std::ceil(Length(displacement) / 8.0F)));
    const Vec2 step = displacement * (1.0F / steps);
    Vec2 position = Project(start, bodyRadius);
    for (int i = 0; i < steps; ++i)
    {
        const Vec2 next = position + step;
        if (Contains(next, bodyRadius)) { position = next; continue; }
        const Vec2 xOnly{next.x, position.y};
        const Vec2 yOnly{position.x, next.y};
        if (Contains(xOnly, bodyRadius)) { position = xOnly; }
        if (Contains(yOnly, bodyRadius)) { position = yOnly; }
        if (!Contains(position, bodyRadius)) { position = Project(position, bodyRadius); }
    }
    return position;
}
}
