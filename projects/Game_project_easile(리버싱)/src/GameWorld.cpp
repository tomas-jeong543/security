#include"IntegrityGuard.h"
#include "GameWorld.h"
#include "BotController.h"
#include "CombatSystem.h"
#include "Logger.h"
#include "PlayerController.h"
#include "VisibilitySystem.h"
#include "MapPaths.h"
#include "Camera.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr const float* laneY = MapPaths::laneY;
const char* TeamName(Team team) { return team == Team::Blue ? "Blue" : "Red"; }
float SegmentCircleHit(Vec2 start, Vec2 end, Vec2 center, float radius)
{
    const Vec2 offset = start - center, delta = end - start;
    const float c = offset.x * offset.x + offset.y * offset.y - radius * radius;
    if (c <= 0.0F) { return 0.0F; }
    const float a = delta.x * delta.x + delta.y * delta.y;
    const float b = offset.x * delta.x + offset.y * delta.y;
    const float discriminant = b * b - a * c;
    if (a <= 0.000001F || discriminant < 0.0F) { return std::numeric_limits<float>::infinity(); }
    const float t = (-b - std::sqrt(discriminant)) / a;
    return t >= 0.0F && t <= 1.0F ? t : std::numeric_limits<float>::infinity();
}
}

GameWorld::GameWorld() : player_({205,720}), bot_({1715,720}),
    allyBot_({205,800},Team::Blue), enemyBot_({1715,640}) {}
std::array<Hero*,4> GameWorld::Heroes() { return {{&player_,&bot_,&allyBot_,&enemyBot_}}; }
std::array<const Hero*,4> GameWorld::Heroes() const { return {{&player_,&bot_,&allyBot_,&enemyBot_}}; }
std::uint64_t GameWorld::HeroId(const Hero& hero) const
{
    const auto heroes=Heroes();
    for (size_t i=0;i<heroes.size();++i) { if (heroes[i]==&hero) { return i; } }
    return UINT64_MAX;
}

void GameWorld::Log(const char* category, const char* event, const char* entity,
                    const std::string& detail) const
{
    Logger{}.Write(simulationTime_, category, event, entity, detail);
}

void GameWorld::Initialize()
{
    player_ = PlayerHero({205, 720});
    bot_ = BotHero({1715, 720});
    allyBot_ = BotHero({205,800},Team::Blue);
    enemyBot_ = BotHero({1715,640});
    botAi_ = {}; allyAi_ = {}; enemyAi_ = {};
    allyAi_.botLane_ = 2; enemyAi_.botLane_ = 0;
    for (int lane = 0; lane < 3; ++lane)
    {
        blueTowers_[lane] = {Team::Blue, {MapPaths::blueTowerX, laneY[lane]}, 300, 0.0F, -1, lane};
        redTowers_[lane] = {Team::Red, {MapPaths::redTowerX, laneY[lane]}, 300, 0.0F, -1, lane};
    }
    blueBase_ = {Team::Blue, MapPaths::blueBase};
    redBase_ = {Team::Red, MapPaths::redBase};
    minions_.clear();
    projectiles_.clear();
    towerShots_.clear();
    simulationTime_ = 0.0;
    firstBaseDestroyedAt_ = -1.0;
    waveTimer_ = 0.0;
    lastPlayerAim_ = {1.0F, 0.0F};
    autoAim_ = true;

    nextMinionId_ = 100;
    pickupActive_ = false;
    pickupTimer_ = 20.0;
    //런타임 cooldown검증을 위해 추가된 변수
    simulationTime_ = 0.0;
    lastUltimateUseTime_ = -10.0;

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
        return minion.hp <= 0 || minion.position.x < -20.0F || minion.position.x > worldWidth + 20.0F;
    }), minions_.end());
    for (int lane = 0; lane < 3; ++lane)
    {
        blueTowers_[lane].targetIndex = redTowers_[lane].targetIndex = -1;
        for (int index = 0; index < 2; ++index)
        {
            const float offset = (lane - 1) * 40.0F + (index == 0 ? -10.0F : 10.0F);
            const auto blueRoute = MapPaths::Route(Team::Blue, lane);
            const auto redRoute = MapPaths::Route(Team::Red, lane);
            minions_.push_back({Team::Blue, blueRoute[0] + Vec2{0.0F, offset}, 40, 0.0F, lane, nextMinionId_++, 1});
            minions_.push_back({Team::Red, redRoute[0] + Vec2{0.0F, offset}, 40, 0.0F, lane, nextMinionId_++, 1});
        }
    }
    Log("MATCH", "MINION_WAVE", "GameWorld", "count_per_team=6;count_per_lane=2");
}

Tower& GameWorld::LaneTower(Team team, int lane)
{
    return team == Team::Blue ? blueTowers_[lane] : redTowers_[lane];
}

const Tower& GameWorld::LaneTower(Team team, int lane) const
{
    return team == Team::Blue ? blueTowers_[lane] : redTowers_[lane];
}

int GameWorld::NearestLane(float y) const
{
    int lane = 0;
    for (int index = 1; index < 3; ++index)
    {
        if (std::abs(y - laneY[index]) < std::abs(y - laneY[lane])) { lane = index; }
    }
    return lane;
}

void GameWorld::SpawnProjectile(Team team, Vec2 position, Vec2 direction,
                                ProjectileKind kind, int damage, std::uint64_t sourceId)
{
    if (Length(direction) == 0.0F) { return; }
    projectiles_.push_back(std::make_unique<Projectile>(
        CombatSystem::CreateProjectile(team, position, direction, kind, damage)));
    projectiles_.back()->sourceId=sourceId==UINT64_MAX ? (team==Team::Blue?0:1) : sourceId;
    projectiles_.back()->origin=position;
    Log("COMBAT", "PROJECTILE_SPAWN", TeamName(team),
        std::string(kind == ProjectileKind::Basic ? "kind=Basic" : "kind=Ultimate") +
        ";damage=" + std::to_string(damage) + ";direction=" +
        std::to_string(direction.x) + "," + std::to_string(direction.y));
}

void GameWorld::DamageHero(Hero& victim, Team attacker, int damage, bool creditHero, std::uint64_t sourceId)
{
    if (CombatSystem::Damage(victim, damage))
    {
        const char* victimName = victim.TeamId() == Team::Blue ? "PlayerHero" : "BotHero";
        Log("COMBAT", "HERO_DEATH", victimName, "attacker=" + std::string(TeamName(attacker)));
        if (creditHero && victim.TeamId() != attacker)
        {
            if (sourceId==UINT64_MAX) { sourceId=attacker==Team::Blue?0:1; }
            for (Hero* hero : Heroes()) { if (HeroId(*hero)==sourceId && hero->TeamId()==attacker) { hero->AddXp(100); } }
        }
    }
}

void GameWorld::DamageMinion(Minion& victim, Team attacker, int damage, std::uint64_t sourceId, bool creditHero)
{
    const int before = victim.hp;
    const bool died = CombatSystem::Damage(victim, damage);
    if (victim.hp < before)
    {
        Log("COMBAT", "MINION_HIT", TeamName(victim.team), "id=" + std::to_string(victim.id) +
            ";attacker=" + TeamName(attacker) + ";hp=" + std::to_string(before) + "->" + std::to_string(victim.hp));
    }
    if (died)
    {
        Log("COMBAT", "MINION_DEATH", TeamName(victim.team), "attacker=" + std::string(TeamName(attacker)));
        if (victim.team != attacker && creditHero)
        {
            if (sourceId==UINT64_MAX) { sourceId=attacker==Team::Blue?0:1; }
            for (Hero* hero : Heroes()) { if (HeroId(*hero)==sourceId && hero->TeamId()==attacker) { hero->AddXp(20); } }
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
        const Vec2 previous = shot.position;
        const float travel = std::min(shot.speed * deltaSeconds, shot.maxDistance - shot.traveled);
        const Vec2 next = previous + shot.direction * travel;
        float firstHit = MapPaths::FirstWallHit(previous, next);
        int victimType = 0; // A wall wins ties; choose the earliest swept entity hit.
        Minion* victimMinion = nullptr;
        Tower* victimTower = nullptr;
        Hero* victimHero = nullptr;
        for (Hero* enemy : Heroes())
        {
            if (!enemy->Alive() || enemy->TeamId() == shot.owner) { continue; }
            const float hit = SegmentCircleHit(previous, next, enemy->Position(), 23.0F);
            if (hit < firstHit) { firstHit = hit; victimType = 1; victimHero=enemy; }
        }
        for (Minion& minion : minions_)
        {
            if (minion.team != shot.owner && minion.hp > 0)
            {
                const float hit = SegmentCircleHit(previous, next, minion.position, 15.0F);
                if (hit < firstHit) { firstHit = hit; victimType = 2; victimMinion = &minion; }
            }
        }
        Base& base = shot.owner == Team::Blue ? redBase_ : blueBase_;
        for (int lane = 0; lane < 3; ++lane)
        {
            Tower& tower = LaneTower(shot.owner == Team::Blue ? Team::Red : Team::Blue, lane);
            if (tower.hp > 0)
            {
                const float hit = MapPaths::SegmentBoxHit(previous, next,
                    tower.position - Vec2{26,26}, tower.position + Vec2{26,26});
                if (hit < firstHit) { firstHit = hit; victimType = 3; victimTower = &tower; }
            }
        }
        if (base.hp > 0)
        {
            const float hit = MapPaths::SegmentBoxHit(previous, next,
                base.position - Vec2{33,33}, base.position + Vec2{33,33});
            if (hit < firstHit) { firstHit = hit; victimType = 4; }
        }
        
        const bool collided = firstHit <= 1.0F;
        shot.position = collided ? previous + (next - previous) * firstHit : next;
        shot.traveled += travel * (collided ? firstHit : 1.0F);
        if (collided)
        {
            if (victimType == 1 && !MapPaths::AttackBlocked(shot.origin,victimHero->Position()))
            { DamageHero(*victimHero, shot.owner, shot.damage,true,shot.sourceId); }
            else if (victimType == 2 && !MapPaths::AttackBlocked(shot.origin,victimMinion->position))
            { DamageMinion(*victimMinion, shot.owner, shot.damage,shot.sourceId); }
            else if (victimType == 3 && !MapPaths::AttackBlocked(shot.origin,victimTower->position))
            { DamageTower(*victimTower, shot.damage); }
            else if (victimType == 4) { DamageBase(base, shot.damage); }
            else { Log("COMBAT", "PROJECTILE_BLOCKED", TeamName(shot.owner)); }
        }
        const bool expired = collided || shot.traveled >= shot.maxDistance ||
            shot.position.x < 0.0F || shot.position.x > worldWidth || shot.position.y < 0.0F || shot.position.y > worldHeight;
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
        Tower& target = LaneTower(minion.team == Team::Blue ? Team::Red : Team::Blue, minion.lane);
        if (target.hp > 0 && Distance(minion.position, target.position) <= 70.0F &&
            !MapPaths::AttackBlocked(minion.position, target.position))
        {
            if (minion.attackCd <= 0.0F) { DamageTower(target, 8); minion.attackCd = 1.0F; }
            continue;
        }
        Minion* enemyMinion = nullptr;
        float nearest = 50.0F;
        for (Minion& candidate : minions_)
        {
            if (candidate.team == minion.team || candidate.hp <= 0 || candidate.lane != minion.lane ||
                MapPaths::AttackBlocked(minion.position, candidate.position)) { continue; }
            const float distance = Distance(minion.position, candidate.position);
            if (distance < nearest - 0.0001F ||
                (distance <= nearest + 0.0001F && enemyMinion && candidate.id < enemyMinion->id))
            {
                nearest = distance;
                enemyMinion = &candidate;
            }
        }
        if (enemyMinion)
        {
            if (minion.attackCd <= 0.0F) { DamageMinion(*enemyMinion, minion.team, 8,UINT64_MAX,false); minion.attackCd = 1.0F; }
            continue;
        }
        Base& base = minion.team == Team::Blue ? redBase_ : blueBase_;
        if (target.hp <= 0 && base.hp > 0 && Distance(minion.position, base.position) <= 60.0F &&
            !MapPaths::AttackBlocked(minion.position, base.position))
        {
            if (minion.attackCd <= 0.0F) { DamageBase(base, 8); minion.attackCd = 1.0F; }
            continue;
        }
        const auto route = MapPaths::Route(minion.team, minion.lane);
        float remaining = 130.0F * deltaSeconds;
        while (remaining > 0.0F && minion.waypoint < static_cast<int>(route.size()))
        {
            const Vec2 toNext = route[minion.waypoint] - minion.position;
            const float distance = Length(toNext);
            if (distance <= remaining + 0.001F)
            {
                minion.position = route[minion.waypoint++];
                remaining -= distance;
            }
            else
            {
                minion.position += toNext * (remaining / distance);
                remaining = 0.0F;
            }
            if (target.hp > 0 && Distance(minion.position, target.position) <= 70.0F) { break; }
            if (target.hp <= 0 && Distance(minion.position, base.position) <= 60.0F) { break; }
            if (std::any_of(minions_.begin(), minions_.end(), [&](const Minion& other)
                { return other.hp > 0 && other.team != minion.team && other.lane == minion.lane &&
                         Distance(minion.position, other.position) <= 50.0F; })) { break; }
        }
        minion.position = MapPaths::Project(minion.position, MapPaths::minionRadius);
    }
}

void GameWorld::UpdateTowers(float deltaSeconds)
{
    for (TowerShotEffect& shot : towerShots_) { shot.remaining -= deltaSeconds; }
    towerShots_.erase(std::remove_if(towerShots_.begin(), towerShots_.end(),
        [](const TowerShotEffect& shot) { return shot.remaining <= 0.0F; }), towerShots_.end());
    for (int lane = 0; lane < 3; ++lane)
    for (Tower* tower : {&blueTowers_[lane], &redTowers_[lane]})
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
            if (minion.hp > 0 && minion.team != tower->team && minion.lane == lane &&
                distance <= closestDistance && !MapPaths::AttackBlocked(tower->position, minion.position))
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
            towerShots_.push_back({tower->team, tower->position, minions_[closestIndex].position});
            CombatSystem::Damage(minions_[closestIndex], 15);
            tower->attackCd = 1.0F;
            continue;
        }
        tower->targetIndex = -1;
        Hero* target=nullptr;
        float distance=260.0F;
        for (Hero* enemy : Heroes())
        {
            const float candidate=Distance(tower->position,enemy->Position());
            if (enemy->Alive() && enemy->TeamId()!=tower->team && candidate<=distance &&
                !MapPaths::AttackBlocked(tower->position,enemy->Position()) &&
                (!target || candidate<distance || HeroId(*enemy)<HeroId(*target)))
            { target=enemy; distance=candidate; }
        }
        if (target)
        {
            towerShots_.push_back({tower->team, tower->position, target->Position()});
            DamageHero(*target, tower->team, 15, false);
            tower->attackCd = 1.0F;
        }
    }
}

void GameWorld::UpdatePickup(float deltaSeconds)
{
    if (!pickupActive_)
    {
        pickupTimer_ -= deltaSeconds;
        if (pickupTimer_ <= 0.00001)
        {
            pickupActive_ = true;
            for (BotRuntime* ai : {&botAi_,&allyAi_,&enemyAi_}) { ai->pickupChaseUsed_=false; }
            Log("PICKUP", "SPAWN", "JungleHeal", "position=960,540;heal=20");
        }
    }
    if (!pickupActive_) { return; }
    constexpr Vec2 center = MapPaths::healPosition;
    // Stable IDs: Player=0, Enemy=1, Ally=2, Enemy=3. Full HP never consumes it.
    for (Hero* hero : Heroes())
    {
        if (!hero->Alive() || hero->Health() >= hero->MaxHealth() || Distance(hero->Position(), center) > 28.0F) { continue; }
        hero->Heal(20);
        pickupActive_ = false;
        pickupTimer_ = 30.0;
        for (BotRuntime* ai : {&botAi_,&allyAi_,&enemyAi_}) { ai->pickupChaseTime_=0; }
        Log("PICKUP", "TAKEN", hero->TeamId() == Team::Blue ? "PlayerHero" : "BotHero", "heal=20");
        break;
    }
}

Vec2 GameWorld::DefensivePoint(const BotHero& bot, const BotRuntime& ai, bool* usingFallback) const
{
    const Team ownTeam = bot.TeamId();
    const auto& ownTowers = ownTeam == Team::Red ? this->redTowers_ : this->blueTowers_;
    const Base& ownBase = ownTeam == Team::Red ? this->redBase_ : this->blueBase_;
    const float backSign = ownTeam == Team::Red ? 1.0F : -1.0F;

    if (usingFallback) { *usingFallback = false; }
    const Tower& tower = ownTowers[ai.botLane_];
    if (tower.hp > 0 && ownBase.hp > 0)
    {
        return {tower.position.x + backSign * 30.0F, tower.position.y};
    }
    if (usingFallback) { *usingFallback = true; }
    return ownBase.position;
}

bool GameWorld::BotHasClearShot(const BotHero& bot, Vec2 target, std::uint64_t targetMinion) const
{
    const Team ownTeam = bot.TeamId(), enemyTeam=ownTeam==Team::Blue?Team::Red:Team::Blue;
    const auto& enemyTowers = ownTeam == Team::Red ? this->blueTowers_ : this->redTowers_;
    const Base& enemyBase = ownTeam == Team::Red ? this->blueBase_ : this->redBase_;

    const Vec2 origin = bot.Position();
    const float distance = Distance(origin, target);
    if (distance <= 0.0F || distance > 650.0F || MapPaths::AttackBlocked(origin, target)) { return false; }
    const float targetHit = std::max(0.0F, 1.0F - (targetMinion ? 15.0F : 23.0F) / distance);
    for (const Minion& minion : minions_)
    {
        if (minion.team == enemyTeam && minion.hp > 0 && (!targetMinion || minion.id != targetMinion) &&
            SegmentCircleHit(origin, target, minion.position, 15.0F) < targetHit) { return false; }
    }
    for (const Tower& tower : enemyTowers)
    {
        if (tower.hp > 0 && MapPaths::SegmentBoxHit(origin, target,
            tower.position - Vec2{26,26}, tower.position + Vec2{26,26}) < targetHit) { return false; }
    }
    if (enemyBase.hp > 0 && MapPaths::SegmentBoxHit(origin, target,
        enemyBase.position - Vec2{33,33}, enemyBase.position + Vec2{33,33}) < targetHit) { return false; }
    return true;
}

bool GameWorld::BotInSafeArea(const BotHero& bot, const BotRuntime& ai) const
{
    const Team ownTeam = bot.TeamId();
    const auto& ownTowers = ownTeam == Team::Red ? this->redTowers_ : this->blueTowers_;
    const Base& ownBase = ownTeam == Team::Red ? this->redBase_ : this->blueBase_;

    constexpr float towerSafetyRadius = 95.0F;
    constexpr float baseSafetyRadius = 90.0F;
    const Tower& tower = ownTowers[ai.botLane_];
    return (tower.hp > 0 && Distance(bot.Position(), tower.position) <= towerSafetyRadius) ||
           (ownBase.hp > 0 && Distance(bot.Position(), ownBase.position) <= baseSafetyRadius);
}

void GameWorld::SetBotState(BotRuntime& ai, BotState state, const char* event, const std::string& detail)
{
    if (ai.botState_ == state) { return; }
    ai.botState_ = state;
    Log("BOT_FSM", event, "BotHero", detail);
}

void GameWorld::UpdateBot(float deltaSeconds, BotHero& bot, BotRuntime& ai)
{
    const Team ownTeam = bot.TeamId(), enemyTeam = ownTeam == Team::Blue ? Team::Red : Team::Blue;
    const auto& ownTowers = ownTeam == Team::Red ? this->redTowers_ : this->blueTowers_;
    const auto& enemyTowers = ownTeam == Team::Red ? this->blueTowers_ : this->redTowers_;
    const Base& ownBase = ownTeam == Team::Red ? this->redBase_ : this->blueBase_;
    const Base& enemyBase = ownTeam == Team::Red ? this->blueBase_ : this->redBase_;
    const float backSign = ownTeam == Team::Red ? 1.0F : -1.0F;

    const Hero* selected = nullptr;
    float nearest = std::numeric_limits<float>::infinity();
    for (const Hero* hero : Heroes())
    {
        if (hero == &bot || hero->TeamId() == ownTeam || !hero->Alive() ||
            !VisibilitySystem{}.IsVisible(bot,*hero,brushes_,6)) { continue; }
        const float distance = Distance(bot.Position(),hero->Position());
        if (!selected || distance < nearest-0.0001F ||
            (std::abs(distance-nearest)<0.0001F && (hero->Health()<selected->Health() ||
             (hero->Health()==selected->Health() && HeroId(*hero)<HeroId(*selected)))))
        { selected=hero; nearest=distance; }
    }
    const Hero& opponent = selected ? *selected : static_cast<const Hero&>(this->player_);
    ai.targetId = selected ? HeroId(*selected) : UINT64_MAX;
    if (!bot.Alive())
    {
        ai.targetId=UINT64_MAX;
        ai.botWasAlive_ = false;
        SetBotState(ai,BotState::Dead, "STATE", "Dead");
        ai.retreatTarget_ = {};
        ai.botRecoveryRemainder_ = 0.0F;
        ai.botRecoveryTime_ = ai.botRetreatCooldown_ = ai.botStrafeHold_ = 0.0F;
        ai.botStrafeTargetX_ = 0.0F;
        ai.botAttackTimer_ = 0.0F;
        return;
    }
    if (!ai.botWasAlive_ || bot.ConsumeRespawned())
    {
        ai.botWasAlive_ = true;
        ai.retreatTarget_ = {};
        ai.botRecoveryRemainder_ = 0.0F;
        ai.botRecoveryTime_ = ai.botRetreatCooldown_ = ai.botStrafeHold_ = 0.0F;
        ai.botStrafeTargetX_ = 0.0F;
        ai.botAttackTimer_ = 0.0F;
        SetBotState(ai,selected ? BotController{}.Decide(bot, opponent, ownBase.position) : BotState::Push, "RESPAWN_RESET", "fsm_state_cleared");
    }
    ai.botAttackTimer_ = std::max(0.0F, ai.botAttackTimer_ - deltaSeconds);
    ai.botRetreatCooldown_ = std::max(0.0F, ai.botRetreatCooldown_ - deltaSeconds);
    if (ai.botState_ != BotState::Retreat && ai.botState_ != BotState::Recover)
    {
        if (selected && opponent.Alive() && (Distance(opponent.Position(), bot.Position()) <= 650.0F ||
                                Distance(opponent.Position(), ownBase.position) <= 450.0F))
        {
            ai.botLane_ = NearestLane(opponent.Position().y);
        }
        else
        {
            const Vec2 ownFork=ownTeam==Team::Blue?MapPaths::blueFork:MapPaths::redFork;
            int chosen = Distance(bot.Position(),ownFork)<300.0F ? ai.botLane_ : NearestLane(bot.Position().y);
            for (int lane = 0; lane < 3; ++lane)
            {
                if (ownTowers[lane].hp > 0 && enemyTowers[lane].hp > 0 &&
                    (ownTowers[chosen].hp <= 0 || enemyTowers[chosen].hp <= 0)) { chosen = lane; }
            }
            ai.botLane_ = chosen;
        }
    }
    constexpr float arrivalTolerance = 10.0F;
    constexpr float recoveryPerSecond = 4.0F;
    int threats=0, support=0;
    for (const Hero* hero : Heroes())
    {
        if (hero==&bot || !hero->Alive()) { continue; }
        if (hero->TeamId()==ownTeam && Distance(bot.Position(),hero->Position())<450) { ++support; }
        if (hero->TeamId()!=ownTeam && Distance(bot.Position(),hero->Position())<500 &&
            !MapPaths::AttackBlocked(bot.Position(),hero->Position()) &&
            VisibilitySystem{}.IsVisible(bot,*hero,brushes_,6)) { ++threats; }
    }
    for (const Minion& minion : minions_)
    {
        if (minion.hp<=0 || minion.lane!=ai.botLane_ || Distance(bot.Position(),minion.position)>200) { continue; }
        if (minion.team==ownTeam) { ++support; } else { ++threats; }
    }
    const bool lowHealth = bot.MaxHealth() > 0 && bot.Health() * 100 <=
        bot.MaxHealth() * (threats>support+1 ? 40 : 30);
    bool fallback = false;
    const Vec2 safeTarget = DefensivePoint(bot,ai,&fallback);
    if (ai.botState_ == BotState::Retreat)
    {
        if (Distance(bot.Position(), safeTarget) <= arrivalTolerance)
        {
            ai.retreatTarget_ = safeTarget;
            SetBotState(ai,BotState::Recover, "RETREAT_TO_RECOVER", "defensive_point_reached");
            ai.botStrafeTargetX_ = fallback ? ownBase.position.x - backSign * 30.0F : ownTowers[ai.botLane_].position.x + backSign * 40.0F;
            ai.botStrafeHold_ = 0.25F;
        }
        else if (Distance(ai.retreatTarget_, safeTarget) > 0.01F)
        {
            ai.retreatTarget_ = safeTarget;
            Log("BOT_FSM", "RETREAT_TARGET", "BotHero", fallback ? "base_fallback" : "tower_to_base_defensive_point");
        }
    }
    else if (ai.botState_ == BotState::Recover)
    {
        ai.botRecoveryTime_ += deltaSeconds;
        if (BotInSafeArea(bot,ai))
        {
            ai.botRecoveryRemainder_ += recoveryPerSecond * deltaSeconds;
            const int wholeHealth = static_cast<int>(ai.botRecoveryRemainder_);
            if (wholeHealth > 0)
            {
                bot.Heal(wholeHealth);
                ai.botRecoveryRemainder_ -= static_cast<float>(wholeHealth);
            }
        }
        if ((bot.MaxHealth() > 0 && bot.Health() * 100 >= bot.MaxHealth() * 55) ||
            ai.botRecoveryTime_ >= 18.0F)
        {
            ai.retreatTarget_ = {};
            if (ai.botRecoveryTime_ >= 18.0F) { ai.botRetreatCooldown_ = 3.0F; }
            SetBotState(ai,BotState::Push, "RECOVER_TO_ADVANCE",
                        ai.botRecoveryTime_ >= 18.0F ? "recovery_timeout" : "hp_ratio>=55%");
            ai.botRecoveryTime_ = 0.0F;
        }
        else if (!BotInSafeArea(bot,ai))
        {
            ai.retreatTarget_ = safeTarget;
            SetBotState(ai,BotState::Retreat, "RECOVER_TARGET_LOST", fallback ? "base_fallback" : "return_to_defensive_point");
        }
    }
    else if (lowHealth && ai.botRetreatCooldown_ <= 0.0F)
    {
        ai.botRecoveryTime_ = 0.0F;
        ai.retreatTarget_ = safeTarget;
        SetBotState(ai,BotState::Retreat, "ADVANCE_TO_RETREAT", fallback ? "hp_ratio<=30%;base_fallback" : "hp_ratio<=30%;tower_to_base_defensive_point");
    }
    else
    {
        if (lowHealth) { SetBotState(ai,BotState::Push, "RETREAT_COOLDOWN", "temporary_advance"); }
        else
        {
            ai.botTimer_ += deltaSeconds;
            while (ai.botTimer_ >= 0.1 - 0.000001)
            {
                ai.botTimer_ -= 0.1;
                SetBotState(ai,selected ? BotController{}.Decide(bot, opponent, ownBase.position) : BotState::Push, "STATE", "normal_decision");
            }
        }
    }

    const auto connectedDirection = [&](Vec2 target)
    {
        const Vec2 waypoint=MapPaths::NavigationWaypoint(bot.Position(),target,MapPaths::heroRadius);
        const Vec2 offset=waypoint-bot.Position();
        return Normalize(offset)*std::min(1.0F,Length(offset)/std::max(0.001F,bot.Speed()*deltaSeconds));
    };
    Vec2 direction = BotController{}.MoveDirection(ai.botState_, bot, opponent, ai.retreatTarget_);
    if (ai.botState_ == BotState::Push)
    {
        const auto route = MapPaths::Route(ownTeam, ai.botLane_);
        int next = 5;
        if (backSign*(bot.Position().x-route[1].x) > 8.0F) { next = 1; }
        else if (backSign*(bot.Position().x-route[2].x) > 8.0F) { next = 2; }
        else if (backSign*(bot.Position().x-route[3].x) > 8.0F) { next = 3; }
        else if (backSign*(bot.Position().x-route[4].x) > 8.0F) { next = 4; }
        direction = connectedDirection(route[next]);
    }
    if ((ai.botState_ == BotState::Engage || ai.botState_ == BotState::Defend) && selected && opponent.Alive())
    {
        direction = connectedDirection(opponent.Position());
    }
    if (pickupActive_ && !ai.pickupChaseUsed_ && ai.pickupChaseTime_ <= 0.0F &&
        (ai.botState_ == BotState::Push || ai.botState_ == BotState::Engage) &&
        bot.Health() * 100 > bot.MaxHealth() * 30 &&
        bot.Health() * 100 <= bot.MaxHealth() * 50 &&
        Distance(bot.Position(), MapPaths::healPosition) <= 300.0F)
    {
        ai.pickupChaseTime_ = 2.5F;
        ai.pickupChaseUsed_ = true;
    }
    if (ai.pickupChaseTime_ > 0.0F)
    {
        ai.pickupChaseTime_ = std::max(0.0F, ai.pickupChaseTime_ - deltaSeconds);
        if (pickupActive_ && ai.botState_ != BotState::Retreat && ai.botState_ != BotState::Recover)
        {
            direction = connectedDirection(MapPaths::healPosition);
        }
    }
    if (ai.botState_ == BotState::Retreat)
    {
        direction = connectedDirection(safeTarget);
    }
    else if (ai.botState_ == BotState::Recover)
    {
        const float front = fallback ? ownBase.position.x - backSign * 30.0F : ownTowers[ai.botLane_].position.x + backSign * 12.0F;
        const float back = fallback ? ownBase.position.x + backSign * 20.0F : ownTowers[ai.botLane_].position.x + backSign * 40.0F;
        if (ai.botStrafeTargetX_ < std::min(front,back) || ai.botStrafeTargetX_ > std::max(front,back)) { ai.botStrafeTargetX_ = back; }
        if (std::abs(bot.Position().x - ai.botStrafeTargetX_) <= 3.0F)
        {
            ai.botStrafeHold_ -= deltaSeconds;
            direction = {};
            if (ai.botStrafeHold_ <= 0.0F)
            {
                ai.botStrafeTargetX_ = ai.botStrafeTargetX_ == back ? front : back;
                ai.botStrafeHold_ = 0.30F;
                Log("BOT_FSM", "DEFENSE_TURN", "BotHero", "target_x=" + std::to_string(ai.botStrafeTargetX_));
            }
        }
        else
        {
            const Vec2 offset = Vec2{ai.botStrafeTargetX_, safeTarget.y} - bot.Position();
            direction = Normalize(offset) * std::min(1.0F, Length(offset) / std::max(0.001F, bot.Speed() * deltaSeconds));
        }
    }
    const Vec2 botBeforeMove = bot.Position();
    bot.Move(direction, deltaSeconds);
    bot.SetPosition(MapPaths::MoveInside(botBeforeMove, bot.Position() - botBeforeMove, MapPaths::heroRadius));
    const bool defending = ai.botState_ == BotState::Retreat || ai.botState_ == BotState::Recover;
    const VisibilitySystem visibility;
    const Hero* attackTarget=nullptr;
    float attackDistance=1000.0F;
    for (const Hero* hero : Heroes())
    {
        const float distance=Distance(bot.Position(),hero->Position());
        if (hero->TeamId()==ownTeam || !hero->Alive() || distance>1000 ||
            MapPaths::AttackBlocked(bot.Position(),hero->Position()) ||
            !visibility.IsVisible(bot,*hero,brushes_,6)) { continue; }
        if (!attackTarget || distance<attackDistance-0.0001F ||
            (std::abs(distance-attackDistance)<0.0001F && (hero->Health()<attackTarget->Health() ||
             (hero->Health()==attackTarget->Health() && HeroId(*hero)<HeroId(*attackTarget)))))
        { attackTarget=hero; attackDistance=distance; }
    }
    const Hero& attackOpponent=attackTarget?*attackTarget:opponent;
    const bool playerVisible = attackTarget!=nullptr;
    ai.targetId=attackTarget?HeroId(*attackTarget):UINT64_MAX;
    if (ai.botState_ != BotState::Dead)
    {
        const Minion* defendTarget = nullptr;
        float defendDistance = 650.0F;
        if (defending)
        for (const Minion& minion : minions_)
        {
            const float distance = Distance(bot.Position(), minion.position);
            if (minion.hp > 0 && minion.team == enemyTeam && minion.lane == ai.botLane_ &&
                (distance < defendDistance-0.0001F ||
                 (defendTarget && std::abs(distance-defendDistance)<0.0001F && minion.id<defendTarget->id)) &&
                BotHasClearShot(bot,minion.position,minion.id))
            { defendTarget = &minion; defendDistance = distance; }
        }
        const Vec2 basicTarget = defendTarget ? defendTarget->position : attackOpponent.Position();
        if (ai.botAttackTimer_ <= 0.0F &&
            (defendTarget || (playerVisible && BotHasClearShot(bot,attackOpponent.Position()))))
        {
            ai.targetId=defendTarget?1+defendTarget->id:HeroId(attackOpponent);
            SpawnProjectile(ownTeam, bot.Position(), Normalize(basicTarget - bot.Position()),
                            ProjectileKind::Basic, bot.BasicDamage(),HeroId(bot));
            bot.StartBasic();
            ai.botAttackTimer_ = 1.0F;
            if (defending)
            { Log("BOT_FSM", "DEFENSE_SHOT", "BotHero", defendTarget ? "target=BlueMinion" : "target=Player"); }
        }
        if (!defending && playerVisible && Distance(bot.Position(), attackOpponent.Position()) <= 850.0F &&
            !MapPaths::AttackBlocked(bot.Position(), attackOpponent.Position()) && bot.UltimateReady())
        {
            SpawnProjectile(ownTeam, bot.Position(), Normalize(attackOpponent.Position() - bot.Position()),
                            ProjectileKind::Ultimate, bot.UltimateDamage(),HeroId(bot));
            bot.StartUltimate();
        }
    }
    if (ai.botState_ == BotState::Push && Distance(bot.Position(), enemyBase.position) <= 700.0F &&
        !MapPaths::AttackBlocked(bot.Position(), enemyBase.position) && ai.botAttackTimer_ <= 0.0F)
    {
        SpawnProjectile(ownTeam, bot.Position(), Normalize(enemyBase.position - bot.Position()),
                        ProjectileKind::Basic, bot.BasicDamage(),HeroId(bot));
        bot.StartBasic();
        ai.botAttackTimer_ = 1.0F;
    }
}

Vec2 GameWorld::AutoAim(float range, std::wstring* target) const
{
    float best = range;
    std::uint64_t bestId = UINT64_MAX;
    Vec2 bestDirection{};
    std::wstring bestName = L"None";
    const auto consider = [&](Vec2 position, std::uint64_t id, const std::wstring& name)
    {
        const float distance = Distance(player_.Position(), position);
        if (distance <= range && !MapPaths::AttackBlocked(player_.Position(), position) &&
            (distance < best - 0.0001F ||
                                  (std::abs(distance - best) <= 0.0001F && id < bestId)))
        {
            best = distance;
            bestId = id;
            bestDirection = Normalize(position - player_.Position());
            bestName = name;
        }
    };
    for (const Hero* hero : Heroes())
    {
        if (hero->TeamId()!=player_.TeamId() && hero->Alive() && VisibilitySystem{}.IsVisible(player_, *hero, brushes_, 6))
        { consider(hero->Position(), HeroId(*hero), L"Bot Hero " + std::to_wstring(HeroId(*hero))); }
    }
    for (int index = 0; index < static_cast<int>(minions_.size()); ++index)
    {
        if (minions_[index].team == Team::Red && minions_[index].hp > 0)
        {
            consider(minions_[index].position, 1 + minions_[index].id, L"Minion");
        }
    }
    for (int lane = 0; lane < 3; ++lane)
    {
        if (redTowers_[lane].hp > 0) { consider(redTowers_[lane].position, 1000000001ULL + lane * 2, L"Tower"); }
    }
    if (redBase_.hp > 0) { consider(redBase_.position, 2000000001ULL, L"Base"); }
    if (target) { *target = bestName; }
    return bestDirection;
}

void GameWorld::SeparateHeroes()
{
    const auto heroes=Heroes();
    for (size_t i=0;i<heroes.size();++i)
    for (size_t j=i+1;j<heroes.size();++j)
    {
    Hero& first=*heroes[i]; Hero& second=*heroes[j];
    if (!first.Alive() || !second.Alive()) { continue; }
    const Vec2 difference = second.Position() - first.Position();
    const float distance = Length(difference);
    constexpr float minimumDistance = 40.0F;
    if (distance >= minimumDistance) { continue; }
    const Vec2 direction = distance > 0.0001F ? difference * (1.0F / distance) : Vec2{1.0F, 0.0F};
    const Vec2 adjustment = direction * ((minimumDistance - distance) * 0.5F);
    first.SetPosition(MapPaths::MoveInside(first.Position(),adjustment*-1.0F,MapPaths::heroRadius));
    second.SetPosition(MapPaths::MoveInside(second.Position(),adjustment,MapPaths::heroRadius));
    }
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
    if (command.upgrade != UpgradeChoice::None) { player_.SetPreferredUpgrade(command.upgrade); }
    if (command.toggleAim)
    {
        autoAim_ = !autoAim_;
        Log("INPUT", "AIM_MODE", "PlayerHero", autoAim_ ? "Auto" : "Mouse");
    }
    simulationTime_ += deltaSeconds;
    player_.Tick(deltaSeconds);
    for (Hero* hero : Heroes()) { if (hero != &player_) { hero->Tick(deltaSeconds); } }
    if (player_.RespawnReady()) { player_.Respawn(); Log("COMBAT", "HERO_RESPAWN", "PlayerHero"); }
    for (Hero* hero : Heroes()) { if (hero != &player_ && hero->RespawnReady()) { hero->Respawn(); Log("COMBAT","HERO_RESPAWN","BotHero", "id="+std::to_string(HeroId(*hero))); } }
    player_.TickRegeneration(deltaSeconds, true);
    const Vec2 playerBeforeMove = player_.Position();
    player_.Move(command.move, deltaSeconds);
    player_.SetPosition(MapPaths::MoveInside(playerBeforeMove, player_.Position() - playerBeforeMove, MapPaths::heroRadius));
    const Vec2 mouseAim = PlayerController{}.AimDirection(command, player_);
    const auto resolveAim = [&](bool click, float range)
    {
        Vec2 direction = click || !autoAim_ ? mouseAim : AutoAim(range);
        if (Length(direction) > 0.00001F) { lastPlayerAim_ = direction; }
        return lastPlayerAim_;
    };
    if (player_.Alive())
    {
        if (command.basic)
        {
            const Vec2 direction = resolveAim(command.clickBasic, 700.0F);
            Log("INPUT", "BASIC", "PlayerHero",
                std::string(command.clickBasic ? "source=LMB" : "source=K") +
                ";mode=" + (autoAim_ ? "Auto" : "Mouse") +
                ";mouse=" + std::to_string(command.mouseWorldPosition.x) + "," +
                std::to_string(command.mouseWorldPosition.y));
            SpawnProjectile(Team::Blue, player_.Position(), direction, ProjectileKind::Basic, player_.BasicDamage());
            player_.StartBasic();
        }
        //checkpoint for intergrity
        // checkpoint for integrity
        if (command.ultimate)
        {

            // 1. 코드 변조 검사
            if (!IntegrityGuard::VerifyUpdateIntegrity())
            {
                Log("SECURITY", "INTEGRITY_FAIL", "GameWorld",
                    "GameWorld::Update modified");
                return;
            }

            // 2. 실제 경과 시간 검사 - > 런타임 쿨다운을 통해서 만약 x64dbg를 이용해서 패치를 해도 실제로 시간을 다시 재서 검증을 해서 틀리면 통과가 안된다.
            const double elapsed =
                simulationTime_ - lastUltimateUseTime_;

            if (elapsed < 10.0)
            {
                OutputDebugStringA("[ULT] Runtime cooldown failed\n");
                Log("SECURITY", "RUNTIME_COOLDOWN_FAIL",
                    "PlayerHero", "Ultimate used too early");
                return;
            }

            // 3. 기존 쿨다운도 정상인지 확인
            if (player_.UltimateReady())
            {
                SpawnProjectile(
                    Team::Blue,
                    player_.Position(),
                    resolveAim(false, 1000.0F),
                    ProjectileKind::Ultimate,
                    player_.UltimateDamage()
                );

                player_.StartUltimate();

                // 실제 발사 성공 시각 저장
                lastUltimateUseTime_ = simulationTime_;
            }
        }
    }
    waveTimer_ += deltaSeconds;
    while (waveTimer_ >= 12.0 - 0.000001) { waveTimer_ -= 12.0; SpawnWave(); }
    const bool botWasRecovering = botAi_.botState_ == BotState::Recover || botAi_.botState_ == BotState::Retreat;
    UpdateBot(deltaSeconds);
    bot_.TickRegeneration(deltaSeconds, !botWasRecovering && botAi_.botState_ != BotState::Retreat && botAi_.botState_ != BotState::Recover);
    UpdateBot(deltaSeconds,allyBot_,allyAi_);
    UpdateBot(deltaSeconds,enemyBot_,enemyAi_);
    allyBot_.TickRegeneration(deltaSeconds,allyAi_.botState_!=BotState::Recover && allyAi_.botState_!=BotState::Retreat);
    enemyBot_.TickRegeneration(deltaSeconds,enemyAi_.botState_!=BotState::Recover && enemyAi_.botState_!=BotState::Retreat);
    SeparateHeroes();
    UpdateProjectiles(deltaSeconds);
    UpdateMinions(deltaSeconds);
    UpdateTowers(deltaSeconds);
    UpdatePickup(deltaSeconds);
    CheckResult();
}

GameWorldSnapshot GameWorld::CreateSnapshot() const
{
    std::vector<EntitySnapshot> entities;
    const VisibilitySystem visibility;
    for (int index = 0; index < 6; ++index)
    {
        const Brush& brush = brushes_[index];
        entities.emplace_back(EntityType::Brush, brush.center, brush.size, Team::Blue, true, true, 1.0F,
                              Vec2{}, false, false, 3000000000ULL + index);
    }
    entities.emplace_back(EntityType::PlayerHero, player_.Position(), Vec2{36, 36}, Team::Blue,
                          true, player_.Alive(), static_cast<float>(player_.Health()) / player_.MaxHealth(), lastPlayerAim_, false, player_.Reveal() > 0, 0);
    for (const Hero* hero : Heroes())
    {
        if (hero == &player_) { continue; }
        entities.emplace_back(EntityType::BotHero,hero->Position(),Vec2{36,36},hero->TeamId(),
            hero->TeamId()==Team::Blue || visibility.IsVisible(player_,*hero,brushes_,6),hero->Alive(),
            static_cast<float>(hero->Health())/hero->MaxHealth(),Normalize(player_.Position()-hero->Position()),
            false,hero->Reveal()>0,HeroId(*hero));
    }
    for (const Minion& minion : minions_)
    {
        entities.emplace_back(EntityType::Minion, minion.position, Vec2{20, 20}, minion.team,
                              true, minion.hp > 0, static_cast<float>(minion.hp) / 40.0F,
                              Vec2{}, false, false, 1 + minion.id);
    }
    for (int lane = 0; lane < 3; ++lane)
    for (const Tower* tower : {&blueTowers_[lane], &redTowers_[lane]})
    {
        entities.emplace_back(EntityType::Tower, tower->position, Vec2{42, 42}, tower->team,
                              true, tower->hp > 0, static_cast<float>(tower->hp) / 300.0F,
                              Vec2{}, false, false, 1000000000ULL + lane * 2 + (tower->team == Team::Red));
    }
    for (const Base* base : {&blueBase_, &redBase_})
    {
        entities.emplace_back(EntityType::Base, base->position, Vec2{54, 54}, base->team,
                              true, base->hp > 0, static_cast<float>(base->hp) / 500.0F,
                              Vec2{}, false, false, 2000000000ULL + (base->team == Team::Red));
    }
    std::uint64_t projectileId = 4000000000ULL;
    for (const auto& projectile : projectiles_)
    {
        entities.emplace_back(EntityType::Projectile, projectile->position, Vec2{10, 10},
                              projectile->owner, true, true, 1.0F, projectile->direction,
                              projectile->kind == ProjectileKind::Ultimate, false, projectileId++);
    }
    for (const TowerShotEffect& shot : towerShots_)
    {
        entities.emplace_back(EntityType::TowerAttack, shot.origin,
                              Vec2{Distance(shot.origin, shot.target), shot.remaining}, shot.team,
                              true, true, 1.0F, Normalize(shot.target - shot.origin));
    }
    if (pickupActive_)
    {
        entities.emplace_back(EntityType::HealPickup, MapPaths::healPosition, Vec2{24.0F, 24.0F},
                              Team::Blue, true, true, 1.0F, Vec2{}, false, false, 5000000000ULL);
    }
    return GameWorldSnapshot(std::move(entities), Camera::Offset(player_.Position()));
}

HUDViewModel GameWorld::CreateHud() const
{
    HUDViewModel view;
    view.hp = player_.Health(); view.maxHp = player_.MaxHealth();
    view.level = player_.Level(); view.xp = player_.Xp(); view.requiredXp = player_.RequiredXp();
    view.basicDamage = player_.BasicDamage(); view.ultimateDamage = player_.UltimateDamage();
    view.ultimateCd = player_.UltimateCooldown();
    view.upgrade = false;
    view.upgradePreference = player_.PreferredUpgrade() == UpgradeChoice::Health ? L"HP" :
                             player_.PreferredUpgrade() == UpgradeChoice::MoveSpeed ? L"MoveSpeed" : L"Attack";
    view.aimMode = autoAim_ ? L"Auto" : L"Mouse";
    if (autoAim_)
    {
        AutoAim(700.0F, &view.basicAutoTarget);
        AutoAim(1000.0F, &view.ultimateAutoTarget);
    }
    else { view.basicAutoTarget = view.ultimateAutoTarget = L"Off"; }
    view.respawning = !player_.Alive();
    view.state = state_ == GameState::Menu ? L"Menu" : state_ == GameState::Initializing ? L"Initializing" :
                 state_ == GameState::Playing ? L"Playing" : state_ == GameState::Paused ? L"Paused" : L"Result";
    view.result = result_;
    return view;
}
