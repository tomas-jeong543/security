#include "GameWorld.h"
#include "BotController.h"
#include "PlayerController.h"
#include "Renderer.h"
#include "HUDRenderer.h"
#include "VisibilitySystem.h"
#include <cmath>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
void Check(bool condition, const char* message)
{
    if (!condition) { throw std::runtime_error(message); }
}
bool Near(float actual, float expected, float tolerance = 0.02F)
{
    return std::abs(actual - expected) <= tolerance;
}
void Start(GameWorld& world)
{
    PlayerCommand command;
    command.start = true;
    world.Update(command, 0.0F);
    Check(world.State() == GameState::Playing, "start did not enter Playing");
}
const EntitySnapshot& Find(const GameWorldSnapshot& snapshot, EntityType type)
{
    for (const EntitySnapshot& entity : snapshot.entities) { if (entity.type == type) { return entity; } }
    throw std::runtime_error("snapshot entity missing");
}
}

struct GameWorldTests
{
    static void G01()
    {
        GameWorld world;
        Check(world.State() == GameState::Menu, "initial state");
        Start(world);
        const auto snapshot = world.CreateSnapshot();
        Check(snapshot.entities.size() == 14, "initial objects not created once");
        Check(world.minions_.size() == 6 && world.projectiles_.empty(), "initial wave/projectiles");
        Check(world.player_.BasicCooldown() == 0 && world.bot_.UltimateCooldown() == 0, "initial cooldowns");
    }
    static void G02()
    {
        Check(Near(Length(InputManager::MoveVector(true, false, true, false)), 1), "diagonal normalization");
        Check(InputManager::MoveVector(true, false, false, false).x == -1, "A movement");
        Check(InputManager::BasicCommand(true, false) == InputManager::BasicCommand(false, true), "LMB/K alias");
        GameWorld world; Start(world);
        const Vec2 initial = world.player_.Position();
        PlayerCommand command;
        command.move = InputManager::MoveVector(false, true, false, false);
        command.mouseWorldPosition = {400, 360};
        command.basic = true;
        command.ultimate = true;
        world.Update(command, 0.01F);
        Check(world.player_.Position().x > initial.x && world.ProjectileCount() == 2, "move/attacks");
        Check(Near(world.player_.BasicCooldown(), 1) && Near(world.player_.UltimateCooldown(), 10), "independent cooldowns");
    }
    static void G03()
    {
        GameWorld world; Start(world);
        world.player_.SetPosition({700, 360});
        world.Update({}, 0.1F);
        Check(world.CurrentBotState() == BotState::Engage, "Engage not selected in world update");
        world.bot_.Damage(75);
        world.Update({}, 0.1F);
        Check(world.CurrentBotState() == BotState::Retreat, "Retreat not selected in world update");
        world.bot_.Respawn();
        world.bot_.SetPosition({600, 360});
        world.player_.SetPosition({1210, 360});
        world.Update({}, 0.1F);
        Check(world.CurrentBotState() == BotState::Defend, "Defend not selected in world update");
        world.player_.SetPosition({18, 360});
        world.Update({}, 0.1F);
        Check(world.CurrentBotState() == BotState::Push, "Push not selected in world update");
        world.bot_.Die();
        world.Update({}, 0.1F);
        Check(world.CurrentBotState() == BotState::Dead, "Dead not selected in world update");
        world.bot_.Respawn();
        world.player_.SetPosition({640, 520});
        world.bot_.SetPosition({640, 520});
        world.Update({}, 0.01F);
        Check(Distance(world.player_.Position(), world.bot_.Position()) >= 39.99F, "heroes overlap after update");
    }
    static void G04()
    {
        GameWorld world; Start(world);
        world.minions_[0].position = {950, 360};
        world.redTower_.hp = 8;
        world.UpdateMinions(0.01F);
        Check(world.redTower_.hp == 0, "minion did not destroy enemy tower");
        const float previous = world.minions_[0].position.x;
        world.UpdateMinions(0.01F);
        Check(world.minions_[0].position.x > previous, "minion did not advance after tower destruction");
        for (int step = 0; step < 1200; ++step) { world.Update({}, 0.01F); }
        int freshBlue = 0, freshRed = 0;
        for (const Minion& minion : world.minions_)
        {
            if (minion.team == Team::Blue && Near(minion.position.x, 180.0F, 2.0F)) { ++freshBlue; }
            if (minion.team == Team::Red && Near(minion.position.x, 1100.0F, 2.0F)) { ++freshRed; }
        }
        if (freshBlue != 3 || freshRed != 3)
        {
            throw std::runtime_error("12-second wave: blue=" + std::to_string(freshBlue) +
                " red=" + std::to_string(freshRed) + " timer=" + std::to_string(world.waveTimer_) +
                " state=" + std::to_string(static_cast<int>(world.State())));
        }
    }
    static void G05()
    {
        GameWorld world; Start(world);
        world.bot_.SetPosition({800, 360});
        auto snapshot = world.CreateSnapshot();
        Check(!Find(snapshot, EntityType::BotHero).visible && !Renderer::ShouldDraw(Find(snapshot, EntityType::BotHero)), "brush concealment/render flag");
        world.player_.SetPosition({800, 360});
        const auto sharedSnapshot = world.CreateSnapshot();
        Check(Find(sharedSnapshot, EntityType::BotHero).visible, "shared brush visibility");
        world.player_.SetPosition({140, 360});
        world.bot_.StartBasic();
        const auto revealedSnapshot = world.CreateSnapshot();
        Check(Find(revealedSnapshot, EntityType::BotHero).visible, "attack reveal");
        world.bot_.Tick(1.5F);
        const auto expiredSnapshot = world.CreateSnapshot();
        Check(!Find(expiredSnapshot, EntityType::BotHero).visible, "reveal did not expire");
    }
    static void G06()
    {
        GameWorld world; Start(world);
        const int requirements[] = {100, 120, 144, 173, 208, 250};
        for (int level = 1; level <= 6; ++level)
        {
            Check(world.player_.RequiredXp() == requirements[level - 1], "XP threshold mismatch");
            while (!world.player_.UpgradePending())
            {
                Minion victim{Team::Red, {600, 360}};
                world.DamageMinion(victim, Team::Blue, 40);
            }
            const int beforeHp = world.player_.MaxHealth();
            const int beforeDamage = world.player_.BasicDamage();
            const float beforeSpeed = world.player_.Speed();
            const UpgradeChoice choice = level % 3 == 1 ? UpgradeChoice::Health : level % 3 == 2 ? UpgradeChoice::Attack : UpgradeChoice::MoveSpeed;
            world.player_.Upgrade(choice);
            Check(world.player_.Level() == level + 1, "level did not advance");
            if (choice == UpgradeChoice::Health) { Check(world.player_.MaxHealth() == beforeHp + 20, "HP upgrade"); }
            if (choice == UpgradeChoice::Attack) { Check(world.player_.BasicDamage() == beforeDamage + 5, "attack upgrade"); }
            if (choice == UpgradeChoice::MoveSpeed) { Check(Near(world.player_.Speed(), beforeSpeed + 15), "speed upgrade"); }
        }
        world.player_.AddXp(1000);
        Check(world.player_.Level() == 7 && !world.player_.UpgradePending(), "level cap");
    }
    static void G07()
    {
        GameWorld world; Start(world);
        PlayerCommand fire; fire.basic = true; fire.mouseWorldPosition = {400, 360};
        world.Update(fire, 0.01F);
        PlayerCommand pause; pause.togglePause = true;
        world.Update(pause, 0.01F);
        Check(world.State() == GameState::Paused, "pause state");
        const float cooldown = world.player_.BasicCooldown();
        world.Update({}, 12.0F);
        Check(Near(world.player_.BasicCooldown(), cooldown), "pause advanced cooldown");
        world.Update(pause, 0.01F);
        Check(world.State() == GameState::Playing, "resume state");
        world.DamageBase(world.redBase_, 500);
        world.Update({}, 0.01F);
        Check(world.State() == GameState::Result && world.CreateHud().result == L"Victory", "base/result");
    }
    static void G08()
    {
        GameWorld world; Start(world);
        PlayerCommand fire; fire.basic = true; fire.mouseWorldPosition = {400, 360};
        world.Update(fire, 0.01F);
        const int count = world.ProjectileCount();
        world.Update(fire, 0.01F);
        Check(world.ProjectileCount() == count, "basic cooldown allowed duplicate");
        world.player_.Tick(0.98F);
        Check(!world.player_.BasicReady(), "basic cooldown ended early");
        world.player_.Tick(0.02F);
        Check(world.player_.BasicReady(), "basic cooldown did not end");
        Hero repeated(Team::Blue, {140, 360});
        repeated.StartBasic();
        repeated.StartUltimate();
        for (int step = 0; step < 999; ++step) { repeated.Tick(0.01F); }
        Check(repeated.BasicReady() && !repeated.UltimateReady(), "independent repeated tick cooldowns");
        repeated.Tick(0.01F);
        Check(repeated.BasicReady() && repeated.UltimateReady(), "repeated tick cooldown ended after 10 seconds");
    }
    static void G09()
    {
        GameWorld world; Start(world);
        world.player_.SetPosition({500, 500});
        PlayerCommand command; command.mouseWorldPosition = {600, 600};
        command.basic = command.ultimate = true;
        world.Update(command, 0.01F);
        int playerShots = 0;
        for (const auto& shot : world.projectiles_)
        {
            if (shot->owner != Team::Blue) { continue; }
            ++playerShots;
            Check(Near(shot->direction.x, 0.7071F, 0.001F) && Near(shot->direction.y, 0.7071F, 0.001F), "mouse aim direction");
        }
        Check(playerShots == 2, "two player projectiles not spawned");
        bool sawBasic = false, sawUltimate = false;
        for (const EntitySnapshot& entity : world.CreateSnapshot().entities)
        {
            if (entity.type != EntityType::Projectile) { continue; }
            sawUltimate |= entity.ultimate;
            sawBasic |= !entity.ultimate;
            Check(Length(entity.direction) > 0.99F, "projectile render direction missing");
        }
        Check(sawBasic && sawUltimate, "projectile render kinds missing");
        world.player_.Tick(10.0F);
        command.ultimate = false;
        command.mouseWorldPosition = world.player_.Position();
        const std::size_t previousCount = world.projectiles_.size();
        world.Update(command, 0.01F);
        Check(world.projectiles_.size() >= previousCount + 1, "same-position aim did not fire");
        bool retainedAim = false;
        for (const auto& shot : world.projectiles_)
        {
            if (shot->owner == Team::Blue && Near(shot->direction.x, 0.7071F, 0.001F) &&
                Near(shot->direction.y, 0.7071F, 0.001F)) { retainedAim = true; }
        }
        Check(retainedAim, "last valid aim not retained");
    }
    static void G10()
    {
        GameWorld world; Start(world);
        world.bot_.Die();
        world.DamageHero(world.player_, Team::Red, 100);
        PlayerCommand command; command.move = {1, 0}; command.basic = true; command.mouseWorldPosition = {400, 360};
        const Vec2 deathPosition = world.player_.Position();
        world.Update(command, 0.01F);
        Check(!world.player_.Alive() && Distance(world.player_.Position(), deathPosition) == 0 && world.ProjectileCount() == 0, "dead input acted");
        for (int step = 0; step < 498; ++step) { world.Update({}, 0.01F); }
        Check(!world.player_.Alive(), "respawn before five seconds");
        world.Update({}, 0.01F);
        Check(world.player_.Alive() && world.player_.Health() == world.player_.MaxHealth(), "respawn health/time");
        Check(Near(world.player_.Position().x, 140), "respawn position");
        Hero levelSeven(Team::Blue, {140, 360});
        for (int level = 1; level < 7; ++level) { levelSeven.AddXp(levelSeven.RequiredXp()); levelSeven.Upgrade(UpgradeChoice::Health); }
        levelSeven.Die(); levelSeven.Tick(10.99F);
        Check(!levelSeven.RespawnReady(), "level seven respawn early");
        levelSeven.Tick(0.01F);
        Check(levelSeven.RespawnReady(), "level seven respawn late");
    }
    static void G11()
    {
        GameWorld draw; Start(draw);
        draw.DamageBase(draw.blueBase_, 500);
        draw.CheckResult();
        Check(draw.State() == GameState::Playing, "first base immediately finalized");
        draw.simulationTime_ += 0.01;
        draw.DamageBase(draw.redBase_, 500);
        draw.CheckResult();
        Check(draw.CreateHud().result == L"Draw", "10ms simultaneous destruction");
        GameWorld defeat; Start(defeat);
        defeat.DamageBase(defeat.blueBase_, 500);
        defeat.simulationTime_ += 0.011;
        defeat.DamageBase(defeat.redBase_, 500);
        defeat.CheckResult();
        Check(defeat.CreateHud().result == L"Defeat", "over 10ms wrong winner");
    }
    static void G12()
    {
        GameWorld world; Start(world);
        world.minions_.clear();
        world.minions_.push_back({Team::Blue, {950, 360}});
        world.minions_.push_back({Team::Red, {955, 360}});
        world.bot_.SetPosition({955, 360});
        const int heroHp = world.bot_.Health();
        const int minionHp = world.minions_[1].hp;
        world.UpdateMinions(0.01F);
        Check(world.redTower_.hp == 292 && world.bot_.Health() == heroHp && world.minions_[1].hp == minionHp, "minion attacked non-tower target");
        world.blueTower_.position = {955, 360};
        world.blueTower_.attackCd = 0.0F;
        world.UpdateTowers(0.01F);
        Check(world.minions_[1].hp == minionHp - 15 && world.bot_.Health() == heroHp,
              "tower did not prioritize minion over hero");
    }
    static void BotRecoveryFsm()
    {
        GameWorld world; Start(world);
        const Vec2 towerCenter = world.redTower_.position;
        const Vec2 defensivePoint = world.DefensivePoint();
        Check(Distance(defensivePoint, towerCenter) > 60.0F && defensivePoint.x > towerCenter.x,
              "retreat target is not the base-side defensive point");
        world.bot_.Damage(69);
        world.Update({}, 0.01F);
        Check(world.CurrentBotState() != BotState::Retreat, "HP 31% entered Retreat");
        world.bot_.Damage(1);
        world.Update({}, 0.01F);
        Check(world.CurrentBotState() == BotState::Retreat, "HP exactly 30% did not enter Retreat");
        world.bot_.Heal(1);
        world.Update({}, 0.01F);
        Check(world.CurrentBotState() == BotState::Retreat, "HP above 30% exited Retreat without recovery");
        world.bot_.SetPosition(defensivePoint);
        world.Update({}, 0.01F);
        Check(world.CurrentBotState() == BotState::Recover, "defensive point arrival did not enter Recover");
        const int beforeRecovery = world.bot_.Health();
        world.Update({}, 0.5F);
        Check(world.bot_.Health() == beforeRecovery + 2, "safe recovery is not 4 HP/s using delta time");
        world.bot_.SetPosition({700, 100});
        const int outsideHealth = world.bot_.Health();
        world.Update({}, 0.1F);
        Check(world.bot_.Health() == outsideHealth && world.CurrentBotState() == BotState::Retreat,
              "recover continued outside safe area");
        world.bot_.SetPosition(world.DefensivePoint());
        world.Update({}, 0.01F);
        Check(world.CurrentBotState() == BotState::Recover, "return to defensive point did not resume Recover");
        world.bot_.Heal(23);
        world.Update({}, 0.01F);
        Check(world.CurrentBotState() == BotState::Push, "HP 55% did not re-engage Advance/Push");
        world.redTower_.hp = 0;
        Check(Near(world.DefensivePoint().x, world.redBase_.position.x), "destroyed tower did not use base fallback");
        world.bot_.Die();
        world.Update({}, 0.01F);
        Check(world.CurrentBotState() == BotState::Dead, "dead bot retained recovery state");
        PlayerCommand restart; restart.restart = true;
        world.DamageBase(world.redBase_, 500);
        world.Update({}, 0.01F);
        world.Update(restart, 0.0F);
        Check(world.State() == GameState::Playing && world.CurrentBotState() == BotState::Patrol,
              "restart retained bot FSM state");
    }
    static void BotRecoveryThreeCycles()
    {
        GameWorld world; Start(world);
        world.player_.SetPosition({18, 700});
        for (int cycle = 0; cycle < 3; ++cycle)
        {
            world.DamageHero(world.bot_, Team::Blue, world.bot_.Health() - 30, false);
            Check(world.bot_.Alive() && world.bot_.Health() == 30, "cycle did not establish Retreat threshold");
            bool reengaged = false;
            for (int step = 0; step < 2000; ++step)
            {
                world.Update({}, 0.01F);
                if (world.CurrentBotState() == BotState::Push && world.bot_.Health() >= 55)
                {
                    reengaged = true;
                    break;
                }
            }
            Check(reengaged, "Retreat/Recover/Push cycle did not complete through GameWorld update");
        }
    }
    static void FlowAndRestart()
    {
        for (int match = 0; match < 10; ++match)
        {
            GameWorld world; Start(world);
            world.bot_.Die();
            world.minions_.clear();
            world.player_.SetPosition({1160, 500});
            world.redBase_.hp = 20;
            PlayerCommand command; command.mouseWorldPosition = world.redBase_.position; command.basic = true;
            world.Update(command, 0.01F);
            for (int step = 0; step < 20 && world.State() == GameState::Playing; ++step) { world.Update({}, 0.01F); }
            Check(world.State() == GameState::Result && world.CreateHud().result == L"Victory", "projectile-to-Result flow");
            PlayerCommand restart; restart.restart = true;
            world.Update(restart, 0.0F);
            Check(world.State() == GameState::Playing && world.redBase_.hp == 500 && world.ProjectileCount() == 0 && world.minions_.size() == 6, "restart retained prior match state");
        }
    }
    static void Focus()
    {
        InputManager input;
        const PlayerCommand command = input.Read(reinterpret_cast<HWND>(1));
        Check(!command.basic && !command.ultimate && Length(command.move) == 0 && !command.start, "unfocused input not ignored");
    }
    static void HudText()
    {
        HWND parent = CreateWindowEx(0, L"STATIC", L"test", WS_OVERLAPPEDWINDOW,
                                     0, 0, 600, 300, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
        Check(parent != nullptr, "HUD test window creation");
        HUDRenderer renderer;
        Check(renderer.Initialize(parent), "HUD initialization");
        HUDViewModel view;
        view.hp = 80; view.maxHp = 100; view.level = 3; view.xp = 45; view.requiredXp = 144;
        view.basicCd = 2.5F; view.ultimateCd = 8.0F; view.state = L"Result"; view.result = L"Victory";
        renderer.Render(view);
        HWND first = FindWindowEx(parent, nullptr, L"EasileHudPanel", nullptr);
        HWND second = FindWindowEx(parent, first, L"EasileHudPanel", nullptr);
        RECT firstBounds{};
        GetClientRect(first, &firstBounds);
        HWND label = firstBounds.right == 374 ? first : second;
        HWND overlay = label == first ? second : first;
        wchar_t text[512]{};
        GetWindowText(label, text, 512);
        const std::wstring contents(text);
        Check(contents.find(L"80/100") != std::wstring::npos &&
              contents.find(L"Level: 3") != std::wstring::npos &&
              contents.find(L"Basic (LMB/K): 2.5") != std::wstring::npos &&
              contents.find(L"Ultimate (J): 8.0") != std::wstring::npos &&
              contents.find(L"Victory") != std::wstring::npos, "HUD content missing");
        HDC screen = GetDC(parent);
        HDC memory = CreateCompatibleDC(screen);
        HBITMAP bitmap = CreateCompatibleBitmap(screen, 390, 166);
        HGDIOBJ previous = SelectObject(memory, bitmap);
        SendMessage(label, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(memory), 0);
        Check(GetPixel(memory, 1, 1) == RGB(66, 168, 212) &&
              GetPixel(memory, 10, 10) == RGB(13, 24, 34), "HUD panel paint missing");
        Check(overlay != nullptr, "result overlay missing");
        SendMessage(overlay, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(memory), 0);
        Check(GetPixel(memory, 1, 1) == RGB(221, 191, 97), "result overlay paint missing");
        view.basicCd = 0.0F;
        view.ultimateCd = 0.0F;
        view.state = L"Playing";
        renderer.Render(view);
        GetWindowText(label, text, 512);
        Check(std::wstring(text).find(L"Basic (LMB/K): READY") != std::wstring::npos &&
              std::wstring(text).find(L"Ultimate (J): READY") != std::wstring::npos,
              "HUD ready state missing");
        SelectObject(memory, previous);
        DeleteObject(bitmap);
        DeleteDC(memory);
        ReleaseDC(parent, screen);
        DestroyWindow(parent);
    }
    static void FullMatchFlow()
    {
        GameWorld world; Start(world);
        const Vec2 playerStart = world.player_.Position();
        const Vec2 botStart = world.bot_.Position();
        PlayerCommand move; move.move = {1, 0}; move.mouseWorldPosition = {500, 360};
        for (int step = 0; step < 11; ++step) { world.Update(move, 0.01F); }
        Check(world.player_.Position().x > playerStart.x && world.bot_.Position().x < botStart.x, "player/bot movement");

        world.minions_.clear();
        world.player_.SetPosition({700, 500});
        world.bot_.SetPosition({750, 500});
        PlayerCommand attack; attack.mouseWorldPosition = world.bot_.Position(); attack.basic = attack.ultimate = true;
        const int botHealth = world.bot_.Health();
        world.Update(attack, 0.01F);
        for (int step = 0; step < 6; ++step) { world.Update({}, 0.01F); }
        Check(world.bot_.Health() < botHealth, "projectile collision/damage");
        world.DamageHero(world.bot_, Team::Blue, 100);
        Check(!world.bot_.Alive(), "bot death");
        world.player_.SetPosition({140, 360});
        for (int step = 0; step < 500; ++step) { world.Update({}, 0.01F); }
        Check(world.bot_.Alive() && world.bot_.Health() == world.bot_.MaxHealth(), "bot respawn");

        world.minions_.clear();
        world.minions_.push_back({Team::Blue, {950, 360}});
        world.redTower_.hp = 8;
        world.Update({}, 0.01F);
        Check(world.redTower_.hp == 0, "minion/tower progression");
        world.minions_.clear();
        world.bot_.Die();
        world.player_.SetPosition({1160, 500});
        world.redBase_.hp = 20;
        PlayerCommand finish; finish.mouseWorldPosition = world.redBase_.position; finish.basic = true;
        world.player_.Tick(10.0F);
        world.Update(finish, 0.01F);
        for (int step = 0; step < 20 && world.State() == GameState::Playing; ++step) { world.Update({}, 0.01F); }
        Check(world.State() == GameState::Result && world.CreateHud().result == L"Victory", "base/result flow");
        PlayerCommand restart; restart.restart = true;
        world.Update(restart, 0.0F);
        Check(world.State() == GameState::Playing && world.redBase_.hp == 500 &&
              world.player_.Level() == 1 && world.ProjectileCount() == 0, "restart after full flow");
    }
};

int main()
{
    const std::pair<const char*, std::function<void()>> tests[] = {
        {"TC G01", GameWorldTests::G01}, {"TC G02", GameWorldTests::G02},
        {"TC G03", GameWorldTests::G03}, {"TC G04", GameWorldTests::G04},
        {"TC G05", GameWorldTests::G05}, {"TC G06", GameWorldTests::G06},
        {"TC G07", GameWorldTests::G07}, {"TC G08", GameWorldTests::G08},
        {"TC G09", GameWorldTests::G09}, {"TC G10", GameWorldTests::G10},
        {"TC G11", GameWorldTests::G11}, {"TC G12", GameWorldTests::G12},
        {"TC G13 Bot FSM recovery", GameWorldTests::BotRecoveryFsm},
        {"Bot FSM 3-cycle scenario", GameWorldTests::BotRecoveryThreeCycles},
        {"Flow/Restart x10", GameWorldTests::FlowAndRestart},
        {"Full match flow", GameWorldTests::FullMatchFlow},
        {"HUD text", GameWorldTests::HudText},
        {"Focus", GameWorldTests::Focus}
    };
    int failures = 0;
    for (const auto& test : tests)
    {
        try { test.second(); std::cout << test.first << " PASS\n"; }
        catch (const std::exception& error) { ++failures; std::cout << test.first << " FAIL: " << error.what() << '\n'; }
    }
    return failures == 0 ? 0 : 1;
}
