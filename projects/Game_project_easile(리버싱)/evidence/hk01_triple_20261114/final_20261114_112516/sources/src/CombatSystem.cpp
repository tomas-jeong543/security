#include "CombatSystem.h"
#include "GameWorld.h"
#include <algorithm>

bool CombatSystem::Damage(Hero& hero, int amount)
{
    if (!hero.Alive()) { return false; }
    hero.Damage(amount);
    if (hero.Health() > 0) { return false; }
    hero.Die();
    return true;
}

bool CombatSystem::Damage(Minion& minion, int amount)
{
    if (minion.hp <= 0) { return false; }
    minion.hp = std::max(0, minion.hp - amount);
    return minion.hp == 0;
}

bool CombatSystem::Damage(Tower& tower, int amount)
{
    if (tower.hp <= 0) { return false; }
    tower.hp = std::max(0, tower.hp - amount);
    return tower.hp == 0;
}

bool CombatSystem::Damage(Base& base, int amount)
{
    if (base.hp <= 0) { return false; }
    base.hp = std::max(0, base.hp - amount);
    return base.hp == 0;
}
