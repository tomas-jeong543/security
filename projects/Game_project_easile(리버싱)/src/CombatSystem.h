#pragma once
#include "Projectile.h"
struct Minion;
struct Tower;
struct Base;
class CombatSystem final
{
public:
    static Projectile CreateProjectile(Team owner, Vec2 position, Vec2 direction, ProjectileKind kind, int damage)
    {
        return {owner, position, Normalize(direction), kind, damage,
            kind == ProjectileKind::Basic ? 1000.0F : 1250.0F,
            kind == ProjectileKind::Basic ? 700.0F : 1000.0F};
    }
    static bool Damage(Hero& hero, int amount);
    static bool Damage(Minion& minion, int amount);
    static bool Damage(Tower& tower, int amount);
    static bool Damage(Base& base, int amount);
};
