#include "Hero.h"
#include <algorithm>

void Hero::Tick(float deltaSeconds)
{
    basicCd_ = std::max(0.0, basicCd_ - deltaSeconds);
    ultimateCd_ = std::max(0.0, ultimateCd_ - deltaSeconds);
    reveal_ = std::max(0.0, reveal_ - deltaSeconds);
    if (!Alive()) { respawn_ = std::max(0.0, respawn_ - deltaSeconds); }
}

void Hero::Die()
{
    state_ = HeroState::Dead;
    hp_ = 0;
    respawn_ = 5.0 + (level_ - 1);
}

void Hero::Respawn()
{
    state_ = HeroState::Alive;
    position_ = spawn_;
    hp_ = maxHp_;
    respawned_ = true;
}

void Hero::Upgrade(UpgradeChoice choice)
{
    if (!upgrade_ || choice == UpgradeChoice::None || level_ >= 7) { return; }
    xp_ -= requiredXp_;
    if (choice == UpgradeChoice::Health)
    {
        maxHp_ += 20;
        hp_ = std::min(maxHp_, hp_ + 20);
    }
    if (choice == UpgradeChoice::Attack)
    {
        basicDamage_ += 5;
        ultimateDamage_ += 8;
    }
    if (choice == UpgradeChoice::MoveSpeed) { speed_ += 15.0F; }
    ++level_;
    static constexpr int requirements[] = {100, 120, 144, 173, 208, 250};
    requiredXp_ = level_ < 7 ? requirements[level_ - 1] : 0;
    upgrade_ = level_ < 7 && xp_ >= requiredXp_;
}
