#pragma once
#include "Hero.h"
#include <string>
#include <utility>
#include <vector>

enum class EntityType { PlayerHero, BotHero, Minion, Tower, Base, Brush, Projectile };

struct EntitySnapshot
{
    EntitySnapshot(EntityType entityType, Vec2 entityPosition, Vec2 entitySize,
                   Team entityTeam, bool isVisible, bool isAlive, float healthRatio,
                   Vec2 facingDirection = {}, bool isUltimate = false, bool isRevealed = false)
        : type(entityType), position(entityPosition), size(entitySize), team(entityTeam),
          visible(isVisible), alive(isAlive), health(healthRatio), direction(facingDirection),
          ultimate(isUltimate), revealed(isRevealed) {}
    const EntityType type;
    const Vec2 position, size;
    const Team team;
    const bool visible, alive;
    const float health;
    const Vec2 direction;
    const bool ultimate, revealed;
};

struct GameWorldSnapshot
{
    explicit GameWorldSnapshot(std::vector<EntitySnapshot> entitySnapshots)
        : entities(std::move(entitySnapshots)) {}
    const std::vector<EntitySnapshot> entities;
};

struct HUDViewModel
{
    int hp = 0, maxHp = 0, level = 1, xp = 0, requiredXp = 100;
    float basicCd = 0.0F, ultimateCd = 0.0F;
    bool upgrade = false;
    bool respawning = false;
    std::wstring state, result;
};
