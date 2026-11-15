#pragma once
#include "Hero.h"
#include <string>
#include <cstdint>
#include <utility>
#include <vector>

enum class EntityType { PlayerHero, BotHero, Minion, Tower, Base, Brush, Projectile, TowerAttack, HealPickup };

struct EntitySnapshot
{
    EntitySnapshot(EntityType entityType, Vec2 entityPosition, Vec2 entitySize,
                   Team entityTeam, bool isVisible, bool isAlive, float healthRatio,
                   Vec2 facingDirection = {}, bool isUltimate = false, bool isRevealed = false,
                   std::uint64_t entityId = 0)
        : type(entityType), position(entityPosition), size(entitySize), team(entityTeam),
          visible(isVisible), alive(isAlive), health(healthRatio), direction(facingDirection),
          ultimate(isUltimate), revealed(isRevealed), id(entityId) {}
    const EntityType type;
    const Vec2 position, size;
    const Team team;
    const bool visible, alive;
    const float health;
    const Vec2 direction;
    const bool ultimate, revealed;
    const std::uint64_t id;
};

struct GameWorldSnapshot
{
    explicit GameWorldSnapshot(std::vector<EntitySnapshot> entitySnapshots, Vec2 offset = {})
        : entities(std::move(entitySnapshots)), cameraOffset(offset) {}
    const std::vector<EntitySnapshot> entities;
    const Vec2 cameraOffset;
};

struct HUDViewModel
{
    int hp = 0, maxHp = 0, level = 1, xp = 0, requiredXp = 100;
    int basicDamage = 5, ultimateDamage = 30;
    float ultimateCd = 0.0F;
    bool upgrade = false;
    bool respawning = false;
    std::wstring upgradePreference = L"Attack", aimMode = L"Auto";
    std::wstring basicAutoTarget = L"None", ultimateAutoTarget = L"None";
    Vec2 cursorWorldPosition{};
    bool cursorVisible = false;
    std::wstring state, result;
};
