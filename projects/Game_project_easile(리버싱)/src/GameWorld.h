#pragma once

#include "BotHero.h"
#include "InputManager.h"
#include "PlayerHero.h"
#include "Projectile.h"
#include "Snapshot.h"
#include <memory>
#include <array>
#include <cstdint>
#include <string>
#include <vector>


enum class GameState { Menu, Initializing, Playing, Paused, Result };
enum class BotState { Patrol, Push, Engage, Retreat, Recover, Defend, Dead };

struct Minion { Team team; Vec2 position; int hp = 40; float attackCd = 0.0F; int lane = 1; std::uint64_t id = 0; int waypoint = 1; };
struct Tower { Team team; Vec2 position; int hp = 300; float attackCd = 0.0F; int targetIndex = -1; int lane = 1; };
struct Base { Team team; Vec2 position; int hp = 500; double destroyedAt = -1.0; };
struct TowerShotEffect { Team team; Vec2 origin, target; float remaining = 0.18F; };
struct Brush
{
    Vec2 center, size;
    bool Contains(Vec2 position) const
    {
        return position.x >= center.x - size.x / 2 && position.x <= center.x + size.x / 2 &&
               position.y >= center.y - size.y / 2 && position.y <= center.y + size.y / 2;
    }
};

struct BotRuntime
{
    BotState botState_ = BotState::Patrol;
    double botTimer_ = 0.0;
    float botAttackTimer_ = 0.0F;
    float botRecoveryRemainder_ = 0.0F;
    float botRecoveryTime_ = 0.0F;
    float botRetreatCooldown_ = 0.0F;
    float botStrafeHold_ = 0.0F;
    float botStrafeTargetX_ = 0.0F;
    bool botWasAlive_ = true;
    Vec2 retreatTarget_{};
    int botLane_ = 1;
    float pickupChaseTime_ = 0.0F;
    bool pickupChaseUsed_ = false;
    std::uint64_t targetId = UINT64_MAX;
};

class GameWorld
{
public:
    GameWorld();
    void Update(const PlayerCommand& input, float deltaSeconds);
    void Restart();
    GameState State() const { return state_; }
    BotState CurrentBotState() const { return botAi_.botState_; }
    GameWorldSnapshot CreateSnapshot() const;
    HUDViewModel CreateHud() const;
    int ProjectileCount() const { return static_cast<int>(projectiles_.size()); }

private:
    friend struct GameWorldTests;
    void Initialize();
    void SpawnWave();
    void SpawnProjectile(Team team, Vec2 position, Vec2 direction, ProjectileKind kind, int damage, std::uint64_t sourceId = UINT64_MAX);
    void UpdateProjectiles(float deltaSeconds);
    void UpdateMinions(float deltaSeconds);
    void UpdateTowers(float deltaSeconds);
    void UpdatePickup(float deltaSeconds);
    void UpdateBot(float deltaSeconds) { UpdateBot(deltaSeconds,bot_,botAi_); }
    void UpdateBot(float deltaSeconds, BotHero& bot_, BotRuntime& ai);
    Vec2 DefensivePoint(bool* fallback = nullptr) const { return DefensivePoint(bot_,botAi_,fallback); }
    Vec2 DefensivePoint(const BotHero& bot_, const BotRuntime& ai, bool* usingFallback = nullptr) const;
    bool BotInSafeArea() const { return BotInSafeArea(bot_,botAi_); }
    bool BotInSafeArea(const BotHero& bot_, const BotRuntime& ai) const;
    bool BotHasClearShot(Vec2 target, std::uint64_t targetMinion = 0) const { return BotHasClearShot(bot_,target,targetMinion); }
    bool BotHasClearShot(const BotHero& bot_, Vec2 target, std::uint64_t targetMinion = 0) const;
    void SetBotState(BotRuntime& ai, BotState state, const char* event, const std::string& detail = {});
    void SeparateHeroes();
    void CheckResult();
    void DamageHero(Hero& victim, Team attacker, int damage, bool creditHero = true, std::uint64_t sourceId = UINT64_MAX);
    void DamageMinion(Minion& victim, Team attacker, int damage, std::uint64_t sourceId = UINT64_MAX, bool creditHero = true);
    void DamageTower(Tower& victim, int damage);
    void DamageBase(Base& victim, int damage);
    Tower& LaneTower(Team team, int lane);
    const Tower& LaneTower(Team team, int lane) const;
    int NearestLane(float y) const;
    Vec2 AutoAim(float range, std::wstring* target = nullptr) const;
    void Log(const char* category, const char* event, const char* entity, const std::string& detail = {}) const;

    PlayerHero player_;
    BotHero bot_;
    BotHero allyBot_, enemyBot_;
    BotRuntime botAi_, allyAi_, enemyAi_;
    std::array<Hero*,4> Heroes();
    std::array<const Hero*,4> Heroes() const;
    std::uint64_t HeroId(const Hero& hero) const;
    GameState state_ = GameState::Menu;

    std::vector<Minion> minions_;
    std::vector<std::unique_ptr<Projectile>> projectiles_;
    std::vector<TowerShotEffect> towerShots_;
    std::array<Tower, 3> blueTowers_{}, redTowers_{};
    Base blueBase_{Team::Blue, {105, 720}}, redBase_{Team::Red, {1815, 720}};
    Brush brushes_[6] = {{{750, 360}, {180, 80}}, {{1170, 1080}, {180, 80}}, {{960, 540}, {140, 80}}, {{960, 900}, {140, 80}}, {{600,720}, {140,80}}, {{1320,720}, {140,80}}};
    double simulationTime_ = 0.0;
    double firstBaseDestroyedAt_ = -1.0;
    double waveTimer_ = 0.0;
    Vec2 lastPlayerAim_{1.0F, 0.0F};
    bool autoAim_ = true;

    std::uint64_t nextMinionId_ = 100;
    bool pickupActive_ = false;
    double pickupTimer_ = 20.0;
    double lastUltimateUseTime_ = -10.0;
    std::wstring result_;
};
