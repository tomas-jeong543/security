#pragma once

#include "BotHero.h"
#include "InputManager.h"
#include "PlayerHero.h"
#include "Projectile.h"
#include "Snapshot.h"
#include <memory>
#include <string>
#include <vector>

enum class GameState { Menu, Initializing, Playing, Paused, Result };
enum class BotState { Patrol, Push, Engage, Retreat, Recover, Defend, Dead };

struct Minion { Team team; Vec2 position; int hp = 40; float attackCd = 0.0F; };
struct Tower { Team team; Vec2 position; int hp = 300; float attackCd = 0.0F; int targetIndex = -1; };
struct Base { Team team; Vec2 position; int hp = 500; double destroyedAt = -1.0; };
struct Brush
{
    Vec2 center, size;
    bool Contains(Vec2 position) const
    {
        return position.x >= center.x - size.x / 2 && position.x <= center.x + size.x / 2 &&
               position.y >= center.y - size.y / 2 && position.y <= center.y + size.y / 2;
    }
};

class GameWorld
{
public:
    GameWorld();
    void Update(const PlayerCommand& input, float deltaSeconds);
    void Restart();
    GameState State() const { return state_; }
    BotState CurrentBotState() const { return botState_; }
    GameWorldSnapshot CreateSnapshot() const;
    HUDViewModel CreateHud() const;
    int ProjectileCount() const { return static_cast<int>(projectiles_.size()); }

private:
    friend struct GameWorldTests;
    void Initialize();
    void SpawnWave();
    void SpawnProjectile(Team team, Vec2 position, Vec2 direction, ProjectileKind kind, int damage);
    void UpdateProjectiles(float deltaSeconds);
    void UpdateMinions(float deltaSeconds);
    void UpdateTowers(float deltaSeconds);
    void UpdateBot(float deltaSeconds);
    Vec2 DefensivePoint(bool* usingFallback = nullptr) const;
    bool BotInSafeArea() const;
    void SetBotState(BotState state, const char* event, const std::string& detail = {});
    void SeparateHeroes();
    void CheckResult();
    void DamageHero(Hero& victim, Team attacker, int damage, bool creditHero = true);
    void DamageMinion(Minion& victim, Team attacker, int damage);
    void DamageTower(Tower& victim, int damage);
    void DamageBase(Base& victim, int damage);
    void Log(const char* category, const char* event, const char* entity, const std::string& detail = {}) const;

    PlayerHero player_;
    BotHero bot_;
    GameState state_ = GameState::Menu;
    BotState botState_ = BotState::Patrol;
    std::vector<Minion> minions_;
    std::vector<std::unique_ptr<Projectile>> projectiles_;
    Tower blueTower_{Team::Blue, {260, 360}}, redTower_{Team::Red, {1020, 360}};
    Base blueBase_{Team::Blue, {70, 360}}, redBase_{Team::Red, {1210, 360}};
    Brush brushes_[2] = {{{480, 360}, {240, 160}}, {{800, 360}, {240, 160}}};
    double simulationTime_ = 0.0;
    double firstBaseDestroyedAt_ = -1.0;
    double waveTimer_ = 0.0;
    double botTimer_ = 0.0;
    float botRecoveryRemainder_ = 0.0F;
    bool botWasAlive_ = true;
    Vec2 retreatTarget_{};
    Vec2 lastPlayerAim_{1.0F, 0.0F};
    std::wstring result_;
};
