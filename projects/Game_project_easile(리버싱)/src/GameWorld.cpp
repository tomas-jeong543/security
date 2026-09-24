#include "GameWorld.h"
#include "BotController.h"
#include "CombatSystem.h"
#include "Logger.h"
#include "PlayerController.h"
#include "VisibilitySystem.h"
#include <algorithm>
#include <cmath>

namespace
{
const char* TeamName(Team team) { return team == Team::Blue ? "Blue" : "Red"; }
bool HitsBox(Vec2 point, Vec2 center, float halfSize)
{
    return std::abs(point.x - center.x) <= halfSize && std::abs(point.y - center.y) <= halfSize;
}
}

GameWorld::GameWorld() : player_({140, 360}), bot_({1140, 360}) {}

void GameWorld::Log(const char* category, const char* event, const char* entity,
                    const std::string& detail) const
{
    Logger{}.Write(simulationTime_, category, event, entity, detail);
}

void GameWorld::Initialize()
{
    player_ = PlayerHero({140, 360});
    bot_ = BotHero({1140, 360});
    blueTower_ = {Team::Blue, {260, 360}};
    redTower_ = {Team::Red, {1020, 360}};
    blueBase_ = {Team::Blue, {70, 360}};
    redBase_ = {Team::Red, {1210, 360}};
    minions_.clear();
    projectiles_.clear();
    simulationTime_ = 0.0;
    firstBaseDestroyedAt_ = -1.0;
    waveTimer_ = 0.0;
    botTimer_ = 0.0;
    botRecoveryRemainder_ = 0.0F;
    botWasAlive_ = true;
    retreatTarget_ = {};
    lastPlayerAim_ = {1.0F, 0.0F};
    botState_ = BotState::Patrol;
    result_.clear();
    SpawnWave();
}

void GameWorld::Restart()
{
    state_ = GameState::Initializing;
    Initialize();
    state_ = GameState::Playing;
    Log("MATCH", "START", "GameWorld", "seed=0xE4511E");
}

void GameWorld::SpawnWave()
{
    minions_.erase(std::remove_if(minions_.begin(), minions_.end(), [](const Minion& minion)
    {
        return minion.hp <= 0 || minion.position.x < -20.0F || minion.position.x > 1300.0F;
    }), minions_.end());
    blueTower_.targetIndex = redTower_.targetIndex = -1;
    for (int index = 0; index < 3; ++index)
    {
        minions_.push_back({Team::Blue, {180.0F, 330.0F + index * 30.0F}});
        minions_.push_back({Team::Red, {1100.0F, 330.0F + index * 30.0F}});
    }
    Log("MATCH", "MINION_WAVE", "GameWorld", "count_per_team=3");
}

void GameWorld::SpawnProjectile(Team team, Vec2 position, Vec2 direction,
                                ProjectileKind kind, int damage)
{
    if (Length(direction) == 0.0F) { return; }
    projectiles_.push_back(std::make_unique<Projectile>(
        CombatSystem::CreateProjectile(team, position, direction, kind, damage)));
    Log("COMBAT", "PROJECTILE_SPAWN", TeamName(team),
        kind == ProjectileKind::Basic ? "kind=Basic" : "kind=Ultimate");
}

void GameWorld::DamageHero(Hero& victim, Team attacker, int damage, bool creditHero)
{
    if (CombatSystem::Damage(victim, damage))
    {
        const char* victimName = victim.TeamId() == Team::Blue ? "PlayerHero" : "BotHero";
        Log("COMBAT", "HERO_DEATH", victimName, "attacker=" + std::string(TeamName(attacker)));
        if (creditHero && victim.TeamId() != attacker)
        {
            (attacker == Team::Blue ? static_cast<Hero&>(player_) : static_cast<Hero&>(bot_)).AddXp(100);
        }
    }
}

void GameWorld::DamageMinion(Minion& victim, Team attacker, int damage)
{
    if (CombatSystem::Damage(victim, damage))
    {
        Log("COMBAT", "MINION_DEATH", TeamName(victim.team), "attacker=" + std::string(TeamName(attacker)));
        if (victim.team != attacker)
        {
            (attacker == Team::Blue ? static_cast<Hero&>(player_) : static_cast<Hero&>(bot_)).AddXp(20);
        }
    }
}

void GameWorld::DamageTower(Tower& victim, int damage)
{
    if (CombatSystem::Damage(victim, damage))
    {
        Log("MATCH", "TOWER_DESTROYED", TeamName(victim.team));
    }
}

void GameWorld::DamageBase(Base& victim, int damage)
{
    if (CombatSystem::Damage(victim, damage))
    {
        victim.destroyedAt = simulationTime_;
        if (firstBaseDestroyedAt_ < 0.0) { firstBaseDestroyedAt_ = simulationTime_; }
        Log("MATCH", "BASE_DESTROYED", TeamName(victim.team));
    }
}

void GameWorld::UpdateProjectiles(float deltaSeconds)
{
    for (auto projectile = projectiles_.begin(); projectile != projectiles_.end();)
    {
        Projectile& shot = **projectile;
        shot.position += shot.direction * (shot.speed * deltaSeconds);
        shot.traveled += shot.speed * deltaSeconds;
        bool expired = shot.position.x < 0.0F || shot.position.x > 1280.0F ||
                       shot.position.y < 0.0F || shot.position.y > 720.0F;
        Hero& enemy = shot.owner == Team::Blue ? static_cast<Hero&>(bot_) : static_cast<Hero&>(player_);
        if (!expired && enemy.Alive() && Distance(shot.position, enemy.Position()) <= 23.0F)
        {
            DamageHero(enemy, shot.owner, shot.damage);
            expired = true;
        }
        for (Minion& minion : minions_)
        {
            if (!expired && minion.team != shot.owner && minion.hp > 0 &&
                Distance(shot.position, minion.position) <= 15.0F)
            {
                DamageMinion(minion, shot.owner, shot.damage);
                expired = true;
            }
        }
        Tower& tower = shot.owner == Team::Blue ? redTower_ : blueTower_;
        Base& base = shot.owner == Team::Blue ? redBase_ : blueBase_;
        if (!expired && tower.hp > 0 && HitsBox(shot.position, tower.position, 26.0F))
        {
            DamageTower(tower, shot.damage);
            expired = true;
        }
        if (!expired && base.hp > 0 && HitsBox(shot.position, base.position, 33.0F))
        {
            DamageBase(base, shot.damage);
            expired = true;
        }
        expired = expired || shot.traveled >= shot.maxDistance;
        if (expired) { projectile = projectiles_.erase(projectile); }
        else { ++projectile; }
    }
}

void GameWorld::UpdateMinions(float deltaSeconds)
{
    for (Minion& minion : minions_)
    {
        if (minion.hp <= 0) { continue; }
        minion.attackCd = std::max(0.0F, minion.attackCd - deltaSeconds);
        Tower& target = minion.team == Team::Blue ? redTower_ : blueTower_;
        if (target.hp > 0 && Distance(minion.position, target.position) <= 70.0F)
        {
            if (minion.attackCd <= 0.0F) { DamageTower(target, 8); minion.attackCd = 1.0F; }
        }
        else
        {
            minion.position.x += (minion.team == Team::Blue ? 110.0F : -110.0F) * deltaSeconds;
        }
    }
}

void GameWorld::UpdateTowers(float deltaSeconds)
{
    for (Tower* tower : {&blueTower_, &redTower_})
    {
        if (tower->hp <= 0) { continue; }
        tower->attackCd = std::max(0.0F, tower->attackCd - deltaSeconds);
        if (tower->attackCd > 0.0F) { continue; }
        int closestIndex = -1;
        float closestDistance = 260.0F;
        for (int index = 0; index < static_cast<int>(minions_.size()); ++index)
        {
            const Minion& minion = minions_[index];
            const float distance = Distance(tower->position, minion.position);
            if (minion.hp > 0 && minion.team != tower->team && distance <= closestDistance)
            {
                if (distance < closestDistance || index == tower->targetIndex)
                {
                    closestDistance = distance;
                    closestIndex = index;
                }
            }
        }
        if (closestIndex >= 0)
        {
            tower->targetIndex = closestIndex;
            CombatSystem::Damage(minions_[closestIndex], 15);
            tower->attackCd = 1.0F;
            continue;
        }
        tower->targetIndex = -1;
        Hero& enemy = tower->team == Team::Blue ? static_cast<Hero&>(bot_) : static_cast<Hero&>(player_);
        if (enemy.Alive() && Distance(tower->position, enemy.Position()) <= 260.0F)
        {
            DamageHero(enemy, tower->team, 15, false);
            tower->attackCd = 1.0F;
        }
    }
}

Vec2 GameWorld::DefensivePoint(bool* usingFallback) const
{
    constexpr float retreatOffset = 70.0F;
    if (usingFallback) { *usingFallback = false; }
    if (redTower_.hp > 0 && redBase_.hp > 0)
    {
        return ClampToMap(redTower_.position + Normalize(redBase_.position - redTower_.position) * retreatOffset);
    }
    if (usingFallback) { *usingFallback = true; }
    return ClampToMap(redBase_.position);
}

bool GameWorld::BotInSafeArea() const
{
    constexpr float towerSafetyRadius = 95.0F;
    constexpr float baseSafetyRadius = 90.0F;
    return (redTower_.hp > 0 && Distance(bot_.Position(), redTower_.position) <= towerSafetyRadius) ||
           (redBase_.hp > 0 && Distance(bot_.Position(), redBase_.position) <= baseSafetyRadius);
}

void GameWorld::SetBotState(BotState state, const char* event, const std::string& detail)
{
    if (botState_ == state) { return; }
    botState_ = state;
    Log("BOT_FSM", event, "BotHero", detail);
}

void GameWorld::UpdateBot(float deltaSeconds)
{
    if (!bot_.Alive())
    {
        botWasAlive_ = false;
        SetBotState(BotState::Dead, "STATE", "Dead");
        retreatTarget_ = {};
        botRecoveryRemainder_ = 0.0F;
        return;
    }
    if (!botWasAlive_ || bot_.ConsumeRespawned())
    {
        botWasAlive_ = true;
        retreatTarget_ = {};
        botRecoveryRemainder_ = 0.0F;
        SetBotState(BotController{}.Decide(bot_, player_, redBase_.position), "RESPAWN_RESET", "fsm_state_cleared");
    }
    constexpr float arrivalTolerance = 10.0F;
    constexpr float recoveryPerSecond = 4.0F;
    constexpr float counterAttackRange = 160.0F;
    const bool lowHealth = bot_.MaxHealth() > 0 && bot_.Health() * 100 <= bot_.MaxHealth() * 30;
    bool fallback = false;
    const Vec2 safeTarget = DefensivePoint(&fallback);
    if (botState_ == BotState::Retreat)
    {
        if (Distance(bot_.Position(), safeTarget) <= arrivalTolerance)
        {
            retreatTarget_ = safeTarget;
            SetBotState(BotState::Recover, "RETREAT_TO_RECOVER", "defensive_point_reached");
        }
        else if (Distance(retreatTarget_, safeTarget) > 0.01F)
        {
            retreatTarget_ = safeTarget;
            Log("BOT_FSM", "RETREAT_TARGET", "BotHero", fallback ? "base_fallback" : "tower_to_base_defensive_point");
        }
    }
    else if (botState_ == BotState::Recover)
    {
        if (BotInSafeArea())
        {
            botRecoveryRemainder_ += recoveryPerSecond * deltaSeconds;
            const int wholeHealth = static_cast<int>(botRecoveryRemainder_);
            if (wholeHealth > 0)
            {
                bot_.Heal(wholeHealth);
                botRecoveryRemainder_ -= static_cast<float>(wholeHealth);
            }
        }
        if (bot_.MaxHealth() > 0 && bot_.Health() * 100 >= bot_.MaxHealth() * 55)
        {
            retreatTarget_ = {};
            SetBotState(BotState::Push, "RECOVER_TO_ADVANCE", "hp_ratio>=55%");
        }
        else if (!BotInSafeArea())
        {
            retreatTarget_ = safeTarget;
            SetBotState(BotState::Retreat, "RECOVER_TARGET_LOST", fallback ? "base_fallback" : "return_to_defensive_point");
        }
    }
    else if (lowHealth)
    {
        retreatTarget_ = safeTarget;
        SetBotState(BotState::Retreat, "ADVANCE_TO_RETREAT", fallback ? "hp_ratio<=30%;base_fallback" : "hp_ratio<=30%;tower_to_base_defensive_point");
    }
    else
    {
        botTimer_ += deltaSeconds;
        while (botTimer_ >= 0.1 - 0.000001)
        {
            botTimer_ -= 0.1;
            SetBotState(BotController{}.Decide(bot_, player_, redBase_.position), "STATE", "normal_decision");
        }
    }

    const Vec2 direction = BotController{}.MoveDirection(botState_, bot_, player_, retreatTarget_);
    bot_.Move(direction, deltaSeconds);
    if (bot_.UpgradePending()) { bot_.Upgrade(UpgradeChoice::Attack); }
    const bool canCounterAttack = botState_ == BotState::Recover &&
                                  Distance(bot_.Position(), player_.Position()) <= counterAttackRange;
    if ((botState_ != BotState::Retreat && botState_ != BotState::Recover && botState_ != BotState::Dead || canCounterAttack) && player_.Alive())
    {
        const float distance = Distance(bot_.Position(), player_.Position());
        const Vec2 aim = Normalize(player_.Position() - bot_.Position());
        if (player_.Alive() && distance <= (canCounterAttack ? counterAttackRange : 650.0F) && bot_.BasicReady())
        {
            SpawnProjectile(Team::Red, bot_.Position(), aim, ProjectileKind::Basic, bot_.BasicDamage());
            bot_.StartBasic();
        }
        if (!canCounterAttack && player_.Alive() && distance <= 850.0F && bot_.UltimateReady())
        {
            SpawnProjectile(Team::Red, bot_.Position(), aim, ProjectileKind::Ultimate, bot_.UltimateDamage());
            bot_.StartUltimate();
        }
    }
    if (botState_ == BotState::Push && Distance(bot_.Position(), blueBase_.position) <= 700.0F && bot_.BasicReady())
    {
        SpawnProjectile(Team::Red, bot_.Position(), Normalize(blueBase_.position - bot_.Position()),
                        ProjectileKind::Basic, bot_.BasicDamage());
        bot_.StartBasic();
    }
}

void GameWorld::SeparateHeroes()
{
    if (!player_.Alive() || !bot_.Alive()) { return; }
    const Vec2 difference = bot_.Position() - player_.Position();
    const float distance = Length(difference);
    constexpr float minimumDistance = 40.0F;
    if (distance >= minimumDistance) { return; }
    const Vec2 direction = distance > 0.0001F ? difference * (1.0F / distance) : Vec2{1.0F, 0.0F};
    const Vec2 adjustment = direction * ((minimumDistance - distance) * 0.5F);
    player_.SetPosition(player_.Position() - adjustment);
    bot_.SetPosition(bot_.Position() + adjustment);
}

void GameWorld::CheckResult()
{
    if (firstBaseDestroyedAt_ < 0.0) { return; }
    if (blueBase_.destroyedAt >= 0.0 && redBase_.destroyedAt >= 0.0)
    {
        result_ = std::abs(blueBase_.destroyedAt - redBase_.destroyedAt) <= 0.0100001
                    ? L"Draw" : (blueBase_.destroyedAt < redBase_.destroyedAt ? L"Defeat" : L"Victory");
    }
    else if (simulationTime_ - firstBaseDestroyedAt_ >= 0.010 - 0.000001)
    {
        result_ = blueBase_.destroyedAt >= 0.0 ? L"Defeat" : L"Victory";
    }
    if (!result_.empty())
    {
        state_ = GameState::Result;
        Log("MATCH", "END", "GameWorld", result_ == L"Draw" ? "Draw" : result_ == L"Victory" ? "Victory" : "Defeat");
    }
}

void GameWorld::Update(const PlayerCommand& input, float deltaSeconds)
{
    if (state_ == GameState::Menu) { if (input.start) { Restart(); } return; }
    if (state_ == GameState::Result) { if (input.restart) { Restart(); } return; }
    if (input.togglePause)
    {
        state_ = state_ == GameState::Playing ? GameState::Paused : GameState::Playing;
        Log("MATCH", state_ == GameState::Paused ? "PAUSE" : "RESUME", "GameWorld");
        return;
    }
    if (state_ != GameState::Playing || deltaSeconds <= 0.0F) { return; }

    const PlayerCommand command = PlayerController{}.BuildCommand(input, player_);
    simulationTime_ += deltaSeconds;
    player_.Tick(deltaSeconds);
    bot_.Tick(deltaSeconds);
    if (player_.RespawnReady()) { player_.Respawn(); Log("COMBAT", "HERO_RESPAWN", "PlayerHero"); }
    if (bot_.RespawnReady()) { bot_.Respawn(); Log("COMBAT", "HERO_RESPAWN", "BotHero"); }
    player_.Move(command.move, deltaSeconds);
    if (player_.UpgradePending()) { player_.Upgrade(command.upgrade); }
    const Vec2 currentAim = PlayerController{}.AimDirection(command, player_);
    if (Length(currentAim) > 0.00001F) { lastPlayerAim_ = currentAim; }
    const Vec2 aim = lastPlayerAim_;
    if (player_.Alive() && Length(aim) > 0.0F)
    {
        if (command.basic && player_.BasicReady())
        {
            SpawnProjectile(Team::Blue, player_.Position(), aim, ProjectileKind::Basic, player_.BasicDamage());
            player_.StartBasic();
        }
        if (command.ultimate && player_.UltimateReady())
        {
            SpawnProjectile(Team::Blue, player_.Position(), aim, ProjectileKind::Ultimate, player_.UltimateDamage());
            player_.StartUltimate();
        }
    }
    waveTimer_ += deltaSeconds;
    while (waveTimer_ >= 12.0 - 0.000001) { waveTimer_ -= 12.0; SpawnWave(); }
    UpdateBot(deltaSeconds);
    SeparateHeroes();
    UpdateProjectiles(deltaSeconds);
    UpdateMinions(deltaSeconds);
    UpdateTowers(deltaSeconds);
    CheckResult();
}

GameWorldSnapshot GameWorld::CreateSnapshot() const
{
    std::vector<EntitySnapshot> entities;
    const VisibilitySystem visibility;
    for (const Brush& brush : brushes_)
    {
        entities.emplace_back(EntityType::Brush, brush.center, brush.size, Team::Blue, true, true, 1.0F);
    }
    entities.emplace_back(EntityType::PlayerHero, player_.Position(), Vec2{36, 36}, Team::Blue,
                          true, player_.Alive(), static_cast<float>(player_.Health()) / player_.MaxHealth(), lastPlayerAim_, false, player_.Reveal() > 0);
    entities.emplace_back(EntityType::BotHero, bot_.Position(), Vec2{36, 36}, Team::Red,
                          visibility.IsVisible(player_, bot_, brushes_, 2), bot_.Alive(),
                          static_cast<float>(bot_.Health()) / bot_.MaxHealth(), Normalize(player_.Position() - bot_.Position()), false, bot_.Reveal() > 0);
    for (const Minion& minion : minions_)
    {
        entities.emplace_back(EntityType::Minion, minion.position, Vec2{20, 20}, minion.team,
                              true, minion.hp > 0, static_cast<float>(minion.hp) / 40.0F);
    }
    for (const Tower* tower : {&blueTower_, &redTower_})
    {
        entities.emplace_back(EntityType::Tower, tower->position, Vec2{42, 42}, tower->team,
                              true, tower->hp > 0, static_cast<float>(tower->hp) / 300.0F);
    }
    for (const Base* base : {&blueBase_, &redBase_})
    {
        entities.emplace_back(EntityType::Base, base->position, Vec2{54, 54}, base->team,
                              true, base->hp > 0, static_cast<float>(base->hp) / 500.0F);
    }
    for (const auto& projectile : projectiles_)
    {
        entities.emplace_back(EntityType::Projectile, projectile->position, Vec2{10, 10},
                              projectile->owner, true, true, 1.0F, projectile->direction,
                              projectile->kind == ProjectileKind::Ultimate);
    }
    return GameWorldSnapshot(std::move(entities));
}

HUDViewModel GameWorld::CreateHud() const
{
    HUDViewModel view;
    view.hp = player_.Health(); view.maxHp = player_.MaxHealth();
    view.level = player_.Level(); view.xp = player_.Xp(); view.requiredXp = player_.RequiredXp();
    view.basicCd = player_.BasicCooldown(); view.ultimateCd = player_.UltimateCooldown();
    view.upgrade = player_.UpgradePending();
    view.respawning = !player_.Alive();
    view.state = state_ == GameState::Menu ? L"Menu" : state_ == GameState::Initializing ? L"Initializing" :
                 state_ == GameState::Playing ? L"Playing" : state_ == GameState::Paused ? L"Paused" : L"Result";
    view.result = result_;
    return view;
}
