#pragma once
#include "MathTypes.h"
#include <algorithm>
#include <cstdint>

enum class Team { Blue, Red };
enum class HeroState { Alive, Dead };
enum class UpgradeChoice { None, Health, Attack, MoveSpeed };

class Hero
{
public:
    Hero(Team team, Vec2 spawn) : team_(team), spawn_(spawn), position_(spawn) {}
    virtual ~Hero() = default;
    Team TeamId() const { return team_; }
    Vec2 Position() const { return position_; }
    void SetPosition(Vec2 position) { position_ = ClampToMap(position); }
    bool Alive() const { return state_ == HeroState::Alive; }
    HeroState State() const { return state_; }
    int Health() const { return hp_; }
    int MaxHealth() const { return maxHp_; }
    int Level() const { return level_; }
    int Xp() const { return xp_; }
    int RequiredXp() const { return requiredXp_; }
    float Speed() const { return speed_; }
    int BasicDamage() const { return basicDamage_; }
    int UltimateDamage() const { return ultimateDamage_; }
    float UltimateCooldown() const { return static_cast<float>(ultimateCd_ <= 0.000001 ? 0.0 : ultimateCd_); }
    float Reveal() const { return static_cast<float>(reveal_ <= 0.000001 ? 0.0 : reveal_); }
    bool UpgradePending() const { return upgrade_; }
    UpgradeChoice PreferredUpgrade() const { return preferredUpgrade_; }
    void SetPreferredUpgrade(UpgradeChoice choice) { if (choice != UpgradeChoice::None) { preferredUpgrade_ = choice; } }
    bool UltimateReady() const { return ultimateCd_ <= 0.000001; }
    bool RespawnReady() const { return !Alive() && respawn_ <= 0.000001; }
    bool ConsumeRespawned() { const bool respawned = respawned_; respawned_ = false; return respawned; }
    void Move(Vec2 direction, float deltaSeconds)
    {
        if (Alive()) { SetPosition(position_ + direction * (speed_ * deltaSeconds)); }
    }
    void Tick(float deltaSeconds);
    void Damage(int amount) { if (Alive() && amount > 0) { hp_ -= amount; sinceDamage_ = 0.0F; regenRemainder_ = 0.0F; } }
    void Heal(int amount) { if (Alive() && amount > 0) { hp_ = std::min(maxHp_, hp_ + amount); } }
    void TickRegeneration(float deltaSeconds, bool enabled);
    void Die();
    void Respawn();
    void AddXp(int amount);
    void Upgrade(UpgradeChoice choice);
    void StartBasic() { reveal_ = 1.5F; }
    void StartUltimate() { ultimateCd_ = 10.0F; reveal_ = 1.5F; }

private:
    Team team_;
    Vec2 spawn_, position_;
    HeroState state_ = HeroState::Alive;
    int hp_ = 100, maxHp_ = 100, level_ = 1, xp_ = 0, requiredXp_ = 100;
    int basicDamage_ = 5, ultimateDamage_ = 30;
    float speed_ = 220.0F;
    double ultimateCd_ = 0.0, reveal_ = 0.0, respawn_ = 0.0;
    bool respawned_ = false;
    bool upgrade_ = false;
    UpgradeChoice preferredUpgrade_ = UpgradeChoice::Attack;
    float sinceDamage_ = 0.0F, regenRemainder_ = 0.0F;
};
