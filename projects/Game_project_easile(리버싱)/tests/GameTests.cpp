#include "GameWorld.h"
#include "BotController.h"
#include "PlayerController.h"
#include "Renderer.h"
#include "HUDRenderer.h"
#include "VisibilitySystem.h"
#include "MapPaths.h"
#include "Camera.h"
#include <set>
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
    static void ExpandedSpawn()
    {
        GameWorld world; ::Start(world);
        int blue=0,red=0; std::set<std::uint64_t> ids;
        const auto snapshot=world.CreateSnapshot();
        for (const auto& entity : snapshot.entities)
        {
            Check(ids.insert(entity.id).second,"spawn EntityId collision");
            if (entity.type!=EntityType::PlayerHero && entity.type!=EntityType::BotHero) { continue; }
            Check(entity.alive && MapPaths::Contains(entity.position,MapPaths::heroRadius),"hero spawn outside terrain");
            if (entity.team==Team::Blue) { ++blue; } else { ++red; }
        }
        Check(blue==2 && red==2,"not a 2:2 hero roster");
        const auto heroes=world.Heroes();
        for (size_t i=0;i<heroes.size();++i)
        for (size_t j=i+1;j<heroes.size();++j)
        { Check(Distance(heroes[i]->Position(),heroes[j]->Position())>=40,"overlapping hero spawn"); }
        world.allyBot_.Damage(75);
        world.Update({},0.1F);
        Check(world.allyAi_.botState_==BotState::Retreat && world.botAi_.botState_!=BotState::Retreat &&
              world.enemyAi_.botState_!=BotState::Retreat,"bots share FSM or HP");
        world.Restart();
        Check(world.allyBot_.Health()==100 && world.enemyBot_.Health()==100 &&
              world.allyAi_.targetId==UINT64_MAX && world.enemyAi_.botAttackTimer_==0,"restart leaked extra bot state");
    }
    static void MultiHeroCombat()
    {
        GameWorld world; ::Start(world); world.minions_.clear();
        for (Tower& tower : world.blueTowers_) { tower.hp=0; }
        for (Tower& tower : world.redTowers_) { tower.hp=0; }
        world.player_.SetPosition({700,720}); world.allyBot_.SetPosition({800,720});
        world.bot_.SetPosition({900,720}); world.enemyBot_.SetPosition({1000,720});
        world.SpawnProjectile(Team::Blue,{800,720},{1,0},ProjectileKind::Basic,5,2);
        world.UpdateProjectiles(0.2F);
        Check(world.bot_.Health()==95 && world.enemyBot_.Health()==100 && world.allyBot_.Health()==100,
              "projectile selected wrong hero or friendly fire");
        world.bot_.Damage(90);
        world.SpawnProjectile(Team::Blue,{800,720},{1,0},ProjectileKind::Basic,5,2);
        world.UpdateProjectiles(0.2F);
        Check(!world.bot_.Alive() && world.allyBot_.Level()==2 && world.player_.Level()==1 &&
              world.enemyBot_.Level()==1,"kill XP did not belong to firing HeroId");
        world.bot_.Respawn(); world.bot_.SetPosition({900,720});
        world.enemyBot_.SetPosition({500,720});
        Check(world.AutoAim(700).x>0,"equal distance heroes did not use EntityId");
        world.UpdateBot(0.1F,world.allyBot_,world.allyAi_);
        world.UpdateBot(0.1F,world.enemyBot_,world.enemyAi_);
        Check(world.allyAi_.targetId!=world.enemyAi_.targetId,"opposing bots share attack target");
    }
    static void ExpandedTerrain()
    {
        Check(Near(worldWidth*worldHeight/(1280.0F*720.0F),3),"world area is not 3x");
        Check(Near(MapPaths::roadWidth,280) && Near(MapPaths::laneY[1]-MapPaths::laneY[0],360),
              "lane width or separation is not 2x");
        const Vec2 open=MapPaths::MoveInside({750,360},{0,100},MapPaths::heroRadius);
        Check(Near(open.y,460),"widened lane cannot be used");
        const Vec2 blocked=MapPaths::MoveInside(open,{0,700},MapPaths::heroRadius);
        Check(blocked.y<510 && MapPaths::Contains(blocked,MapPaths::heroRadius),"Hero crossed solid island");
        int oldArea=0,newArea=0;
        for (float y=4;y<worldHeight;y+=8)
        for (float x=4;x<worldWidth;x+=8)
        {
            const Vec2 point{x,y};
            if (MapPaths::Contains(point,0)) { ++newArea; }
            if (x>=1280 || y>=720) { continue; }
            bool legacyRoad=false;
            for (float lane : {180.0F,360.0F,540.0F})
            {
                const Vec2 nodes[]={{70,360},{170,360},{260,lane},{1020,lane},{1110,360},{1210,360}};
                for (int i=0;i<5;++i)
                { legacyRoad |= Distance(point,MapPaths::ClosestOnSegment(point,nodes[i],nodes[i+1]))<=70; }
            }
            if (legacyRoad) { ++oldArea; }
        }
        const float ratio=static_cast<float>(newArea)/oldArea;
        std::cout<<"MAP playable_terrain_ratio="<<ratio<<" (8px raster)\n";
        Check(ratio>2.5F && ratio<3.8F,"actual walkable terrain did not expand about 3x");
    }
    static void JungleEntry()
    {
        Vec2 position{960,360};
        for (int step=0;step<90;++step)
        { position=MapPaths::MoveInside(position,{0,8},MapPaths::heroRadius); }
        Check(Distance(position,{960,1080})<0.02F,"central jungle connector does not link all lanes");
        GameWorld world; ::Start(world);
        world.allyBot_.SetPosition({750,360});
        const Vec2 target{960,540};
        for (int step=0;step<400;++step)
        {
            const Vec2 waypoint=MapPaths::NavigationWaypoint(world.allyBot_.Position(),target,MapPaths::heroRadius);
            const Vec2 offset=waypoint-world.allyBot_.Position();
            world.allyBot_.SetPosition(MapPaths::MoveInside(world.allyBot_.Position(),
                Normalize(offset)*std::min(2.2F,Length(offset)),MapPaths::heroRadius));
        }
        Check(Distance(world.allyBot_.Position(),target)<1,"bot could not enter jungle chamber");
    }
    static void JungleWalls()
    {
        GameWorld world; ::Start(world); world.minions_.clear();
        for (ProjectileKind kind : {ProjectileKind::Basic,ProjectileKind::Ultimate})
        {
            world.bot_.SetPosition({750,720});
            world.SpawnProjectile(Team::Blue,{750,360},{0,1},kind,30);
            world.projectiles_.back()->speed=100000;
            world.UpdateProjectiles(0.01F);
            Check(world.bot_.Health()==100 && world.ProjectileCount()==0,"jungle wall let a swept shot pass");
        }
        const Vec2 blocked=MapPaths::MoveInside({750,360},{0,600},MapPaths::heroRadius);
        Check(blocked.y<510,"jungle wall let a Hero pass");
        // The walkable connector is not permission to shoot directly into another lane.
        world.player_.SetPosition({960,360}); world.bot_.SetPosition({960,720});
        world.enemyBot_.Die();
        for (Tower& tower : world.redTowers_) { tower.hp=0; } world.redBase_.hp=0;
        Check(Length(world.AutoAim(700))==0,"Auto targeted another lane through jungle exit");
        for (ProjectileKind kind : {ProjectileKind::Basic,ProjectileKind::Ultimate})
        {
            world.SpawnProjectile(Team::Blue,{960,360},{0,1},kind,30);
            world.projectiles_.back()->speed=100000;
            world.UpdateProjectiles(0.01F);
            Check(world.bot_.Health()==100,"directional shot damaged another lane through jungle exit");
        }
    }
    static void JungleBrush()
    {
        GameWorld world; ::Start(world); world.minions_.clear();
        for (Tower& tower : world.redTowers_) { tower.hp=0; } world.redBase_.hp=0;
        world.enemyBot_.Die(); world.player_.SetPosition({960,360});
        world.bot_.SetPosition(MapPaths::healPosition);
        Check(!Find(world.CreateSnapshot(),EntityType::BotHero).visible && Length(world.AutoAim(700))==0,
              "jungle brush did not hide Hero from renderer/Auto");
        world.bot_.StartBasic();
        Check(Find(world.CreateSnapshot(),EntityType::BotHero).visible && world.AutoAim(700).y>0,
              "jungle attack reveal did not expose Hero");
        world.bot_.Tick(1.5F); world.player_.SetPosition({960,560});
        Check(Find(world.CreateSnapshot(),EntityType::BotHero).visible,"shared jungle brush not visible");
    }
    static void ExpandedMinionPaths()
    {
        for (Team team : {Team::Blue,Team::Red})
        for (int lane=0;lane<3;++lane)
        {
            GameWorld world; ::Start(world); world.minions_.clear();
            const auto route=MapPaths::Route(team,lane);
            world.minions_.push_back({team,route[0],40,0,lane,100,1});
            Tower& tower=world.LaneTower(team==Team::Blue?Team::Red:Team::Blue,lane);
            Base& base=team==Team::Blue?world.redBase_:world.blueBase_;
            for (int step=0;step<18000 && base.hp>0;++step)
            {
                world.UpdateMinions(0.01F);
                const Minion& minion=world.minions_[0];
                Check(MapPaths::Contains(minion.position,MapPaths::minionRadius),"minion crossed terrain boundary");
                Check(minion.lane==lane,"minion switched lanes");
                if (minion.position.x>MapPaths::blueTowerX && minion.position.x<MapPaths::redTowerX)
                { Check(Near(minion.position.y,MapPaths::laneY[lane]),"minion entered jungle"); }
            }
            Check(tower.hp==0 && base.hp==0,"lane path did not finish tower and Nexus");
        }
    }
    static void CameraCoordinates()
    {
        for (Vec2 player : {Vec2{205,720},Vec2{960,540},Vec2{1815,1080}})
        {
            const Vec2 offset=Camera::Offset(player);
            const Vec2 screen=Camera::WorldToScreen(player,offset);
            Check(Distance(Camera::ScreenToWorld(screen,offset),player)<0.01F,"camera transform not invertible");
            const Vec2 cursor=InputManager::MouseWorldPosition({400,225},{0,0,800,450});
            Check(Distance(Camera::ScreenToWorld(cursor,offset),Vec2{640,360}+offset)<0.01F,
                  "resized mouse not converted with camera offset");
            Check(offset.x>=0 && offset.x<=worldWidth-Camera::width && offset.y>=0 &&
                  offset.y<=worldHeight-Camera::height,"camera escaped map");
        }
    }
    static void ExpandedFullMatch()
    {
        double total=0;
        for (int run=0;run<3;++run)
        {
            GameWorld world; ::Start(world);
            double firstMinion=-1, firstTower=-1, firstHero=-1;
            for (int step=0;step<30000 && world.State()==GameState::Playing;++step)
            {
                PlayerCommand command;
                const Vec2 destination=world.redTowers_[1].hp>0 ? Vec2{1100,720} : Vec2{1590,720};
                const Vec2 waypoint=MapPaths::NavigationWaypoint(world.player_.Position(),destination,MapPaths::heroRadius);
                const Vec2 offset=waypoint-world.player_.Position();
                command.move=Normalize(offset)*std::min(1.0F,Length(offset)/2.2F);
                command.basic=step%10==0; command.ultimate=step%100==0;
                command.mouseWorldPosition=world.redBase_.position;
                world.Update(command,0.01F);
                for (const Minion& minion : world.minions_)
                {
                    if (firstMinion<0 && minion.hp<40) { firstMinion=world.simulationTime_; }
                    for (const Tower& tower : world.redTowers_)
                    { if (firstTower<0 && minion.team==Team::Blue && minion.lane==tower.lane &&
                           Distance(minion.position,tower.position)<=260) { firstTower=world.simulationTime_; } }
                }
                if (firstHero<0 && (world.bot_.Health()<100 || world.enemyBot_.Health()<100 || world.allyBot_.Health()<100))
                { firstHero=world.simulationTime_; }
            }
            std::cout<<"TEMPO run="<<run+1<<" minion_first_damage="<<firstMinion
                     <<" minion_tower_range="<<firstTower<<" hero_first_damage="<<firstHero
                     <<" match_seconds="<<world.simulationTime_<<" result="
                     <<(world.CreateHud().result==L"Victory"?"Victory":world.CreateHud().result==L"Defeat"?"Defeat":"unfinished")<<'\n';
            Check(world.State()==GameState::Result && world.redTowers_[1].hp==0 && world.redBase_.hp==0 &&
                  world.CreateHud().result==L"Victory" && firstHero>=0 && firstMinion>=0,
                  "real 2:2 match did not finish lane/hero/tower/Nexus progression");
            total+=world.simulationTime_;
        }
        std::cout<<"TEMPO mean_match_seconds="<<total/3<<'\n';
        for (int lane=0;lane<3;++lane)
        {
            Vec2 position{205,720}; const Vec2 target{960,MapPaths::laneY[lane]}; int steps=0;
            for (;steps<3000 && Distance(position,target)>1;++steps)
            {
                const Vec2 waypoint=MapPaths::NavigationWaypoint(position,target,MapPaths::heroRadius);
                const Vec2 offset=waypoint-position;
                position=MapPaths::MoveInside(position,Normalize(offset)*std::min(2.2F,Length(offset)),MapPaths::heroRadius);
            }
            Check(Distance(position,target)<=1,"Base to lane route never completed");
            std::cout<<"TEMPO base_to_lane_"<<lane<<"_seconds="<<steps*0.01<<'\n';
        }
        GameWorld waves; ::Start(waves);
        double firstClash=-1;
        for (int step=0;step<2000 && firstClash<0;++step)
        {
            waves.UpdateMinions(0.01F);
            for (const Minion& minion : waves.minions_)
            { if (minion.hp<40) { firstClash=(step+1)*0.01; } }
        }
        Check(firstClash>0 && firstClash<15,"expanded minion first clash is too slow");
        std::cout<<"TEMPO minion_only_first_clash_seconds="<<firstClash<<'\n';
    }
    static void ExpandedVisualScenarios()
    {
        RECT bounds{0,0,1280,720}; AdjustWindowRect(&bounds,WS_OVERLAPPEDWINDOW,FALSE);
        const HWND window=CreateWindowExW(0,L"STATIC",L"EASILE v0.14 visual regression",
            WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,40,40,bounds.right-bounds.left,bounds.bottom-bounds.top,
            nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        Check(window!=nullptr,"expanded visual window failed");
        Renderer renderer; HUDRenderer hudRenderer;
        Check(renderer.Initialize(window) && hudRenderer.Initialize(window),"expanded visual DX11/HUD failed");
        ShowWindow(window,SW_SHOWNOACTIVATE);
        wchar_t executable[MAX_PATH]{}; GetModuleFileNameW(nullptr,executable,MAX_PATH);
        std::wstring directory(executable); directory=directory.substr(0,directory.find_last_of(L"\\/")+1);
        GameWorld world; ::Start(world);
        const auto capture=[&](const wchar_t* name)
        {
            HUDViewModel hud=world.CreateHud(); hud.cursorVisible=true;
            hud.cursorWorldPosition=Camera::ScreenToWorld({840,400},world.CreateSnapshot().cameraOffset);
            renderer.Render(world.CreateSnapshot(),hud,true); hudRenderer.Render(hud);
            MSG message{};
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
            UpdateWindow(window);
            Check(MoveFileExW((directory+L"visual-smoke.bmp").c_str(),(directory+name).c_str(),MOVEFILE_REPLACE_EXISTING)!=0,
                  "expanded visual capture missing");
        };
        capture(L"v014-start.bmp");
        world.player_.SetPosition({960,720}); world.allyBot_.SetPosition({820,720});
        world.bot_.SetPosition({1100,720}); world.enemyBot_.SetPosition({1120,1080});
        for (int step=0;step<30;++step)
        { PlayerCommand command; command.basic=step==0; command.ultimate=step==1; world.Update(command,0.01F); }
        capture(L"v014-center-combat.bmp");
        world.player_.SetPosition(MapPaths::healPosition); world.pickupActive_=true;
        capture(L"v014-jungle.bmp");
        world.player_.SetPosition({1815,1080});
        capture(L"v014-camera-edge.bmp");
        world.player_.SetPosition({1100,720}); world.bot_.SetPosition({1560,720}); world.bot_.Damage(75);
        world.allyBot_.SetPosition({360,1080}); world.allyBot_.Damage(75);
        for (int step=0;step<50;++step) { world.Update({},0.01F); }
        capture(L"v014-retreat.bmp");
        DestroyWindow(window);
        std::cout<<"VISUAL expanded start/combat/jungle/camera/retreat PASS\n";
    }
    static void Start(GameWorld& world)
    {
        ::Start(world);
        world.allyBot_.Die(); world.enemyBot_.Die();
    }
    static void VisualScenarios()
    {
        RECT bounds{0,0,1280,720}; AdjustWindowRect(&bounds,WS_OVERLAPPEDWINDOW,FALSE);
        const HWND window = CreateWindowExW(0,L"STATIC",L"EASILE visual regression",
            WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,40,40,bounds.right-bounds.left,bounds.bottom-bounds.top,
            nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        Check(window != nullptr,"visual window failed");
        Renderer renderer; HUDRenderer hud;
        Check(renderer.Initialize(window) && hud.Initialize(window),"visual DX11/HUD failed");
        ShowWindow(window,SW_SHOWNOACTIVATE);
        wchar_t executable[MAX_PATH]{}; GetModuleFileNameW(nullptr,executable,MAX_PATH);
        std::wstring directory(executable); directory=directory.substr(0,directory.find_last_of(L"\\/")+1);
        const auto capture = [&](GameWorld& world,const wchar_t* name)
        {
            renderer.Render(world.CreateSnapshot(),world.CreateHud(),true); hud.Render(world.CreateHud());
            MSG message{};
            while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
            UpdateWindow(window);
            Check(MoveFileExW((directory+L"visual-smoke.bmp").c_str(),(directory+name).c_str(),MOVEFILE_REPLACE_EXISTING)!=0,
                  "visual capture was not saved");
        };
        GameWorld start; Start(start); capture(start,L"visual-start.bmp");
        for(int step=0;step<385;++step) { start.Update({},0.01F); }
        capture(start,L"visual-combat.bmp");
        GameWorld defense; Start(defense); defense.minions_.clear();
        defense.player_.SetPosition({105.0F,720.0F}); defense.bot_.SetPosition({1575.0F,720.0F}); defense.bot_.Damage(75);
        defense.botAi_.botLane_=1; defense.botAi_.botState_=BotState::Recover; defense.botAi_.botStrafeTargetX_=1060;
        for(Tower& tower: defense.blueTowers_) { tower.hp=0; }
        for(Tower& tower: defense.redTowers_) { tower.attackCd=1000; }
        for(int step=0;step<50;++step) { defense.Update({},0.01F); }
        Check(defense.ProjectileCount()==0,"visual defense fired with no target");
        capture(defense,L"visual-defense-empty.bmp");
        defense.minions_.push_back({Team::Blue,{1380.0F,720.0F},40,0,1,100,3});
        defense.minions_.push_back({Team::Blue,{1410.0F,360.0F},40,0,0,101,3});
        capture(defense,L"visual-defense-before.bmp");
        for(int step=0;step<20;++step) { defense.Update({},0.01F); }
        capture(defense,L"visual-defense-shot.bmp");
        for(int step=0;step<380;++step) { defense.Update({},0.01F); }
        capture(defense,L"visual-defense-hit.bmp");
        Check(defense.minions_[0].hp<=20 && defense.minions_[1].hp==40,"visual defense target damage failed");
        std::cout << "VISUAL defense: Blue same-lane HP=" << defense.minions_[0].hp
                  << "; other-lane HP=" << defense.minions_[1].hp << "; Bot HP=" << defense.bot_.Health() << '\n';
        for(int step=0;step<400 && defense.bot_.Health()<55;++step) { defense.Update({},0.01F); }
        capture(defense,L"visual-defense-push.bmp");
        Check(defense.botAi_.botState_==BotState::Push && defense.bot_.Health()>=55,"visual defense did not resume Push");
        std::cout << "VISUAL recovery returned to Push PASS\n";
        const float recoveredX = defense.bot_.Position().x;
        for(int step=0;step<50;++step) { defense.Update({},0.01F); }
        capture(defense,L"visual-defense-advance.bmp");
        Check(defense.bot_.Position().x < recoveredX-10.0F,"visual recovered Bot did not advance");
        std::cout << "VISUAL recovered Bot advanced PASS\n";
        DestroyWindow(window);
    }
    static void ClearCombatObjects(GameWorld& world)
    {
        world.minions_.clear(); world.projectiles_.clear(); world.allyBot_.Die(); world.enemyBot_.Die();
        for (Tower& tower : world.blueTowers_) { tower.hp = 0; }
        for (Tower& tower : world.redTowers_) { tower.hp = 0; }
        world.blueBase_.hp = world.redBase_.hp = 0;
    }
    static void LaneWallCombat()
    {
        for (Team owner : {Team::Blue, Team::Red})
        for (ProjectileKind kind : {ProjectileKind::Basic, ProjectileKind::Ultimate})
        for (int lane : {0, 1})
        for (int direction : {1, -1})
        {
            GameWorld world; Start(world); ClearCombatObjects(world);
            const float firstY = MapPaths::laneY[direction > 0 ? lane : lane + 1];
            const float lastY = MapPaths::laneY[direction > 0 ? lane + 1 : lane];
            Hero& source = owner == Team::Blue ? static_cast<Hero&>(world.player_) : static_cast<Hero&>(world.bot_);
            Hero& target = owner == Team::Blue ? static_cast<Hero&>(world.bot_) : static_cast<Hero&>(world.player_);
            source.SetPosition({500,firstY}); target.SetPosition({500,lastY});
            const int damage = kind == ProjectileKind::Basic ? 5 : 30;
            world.SpawnProjectile(owner, source.Position(), Normalize(target.Position()-source.Position()), kind, damage);
            world.projectiles_.back()->speed = 80000.0F;
            world.UpdateProjectiles(0.01F);
            Check(target.Health() == 100 && world.ProjectileCount() == 0, "fast projectile crossed lane wall");
            // The same shot works on one lane and around the open wall endpoint.
            target.SetPosition({680,firstY});
            world.SpawnProjectile(owner, source.Position(), {1,0}, kind, damage);
            world.projectiles_.back()->speed = 80000.0F;
            world.UpdateProjectiles(0.01F);
            Check(target.Health() == 100-damage, "wall disabled same-lane damage");
            source.SetPosition({390,firstY}); target.SetPosition({390,lastY});
            world.SpawnProjectile(owner, source.Position(), Normalize(target.Position()-source.Position()), kind, damage);
            world.projectiles_.back()->speed = 80000.0F;
            world.UpdateProjectiles(0.01F);
            Check(target.Health() == 100-2*damage, "wall endpoint blocked open junction attack");
        }
        GameWorld nearest; Start(nearest); ClearCombatObjects(nearest);
        nearest.bot_.SetPosition({750.0F,720.0F});
        nearest.minions_.push_back({Team::Red,{750.0F,430.0F},40,0,0,1,1});
        nearest.SpawnProjectile(Team::Blue,{750.0F,360.0F},{0,1},ProjectileKind::Basic,5);
        nearest.projectiles_.back()->speed = 80000.0F;
        nearest.UpdateProjectiles(0.01F);
        Check(nearest.minions_[0].hp == 35 && nearest.bot_.Health() == 100,
              "wall collision discarded nearer entity hit");
    }
    static void WallAimAndAttackModes()
    {
        for (int lane : {0, 1})
        {
            GameWorld autoWorld; Start(autoWorld); ClearCombatObjects(autoWorld);
            autoWorld.player_.SetPosition({750.0F,MapPaths::laneY[lane]});
            autoWorld.bot_.SetPosition({750.0F,MapPaths::laneY[lane+1]});
            autoWorld.minions_.push_back({Team::Red,{795.0F,MapPaths::laneY[lane+1]},40,0,lane+1,1,1});
            std::wstring name;
            Check(Length(autoWorld.AutoAim(1000,&name)) == 0 && name == L"None" &&
                  autoWorld.CreateHud().basicAutoTarget == L"None" &&
                  autoWorld.CreateHud().ultimateAutoTarget == L"None", "HUD/Auto selected target across wall");
            PlayerCommand automatic; automatic.basic = automatic.ultimate = true;
            autoWorld.Update(automatic,0.01F);
            for (const auto& shot : autoWorld.projectiles_)
            { if (shot->owner == Team::Blue) { Check(shot->direction.x > 0.99F, "Auto aimed through wall"); } }
            for (int mode : {0,1,2})
            {
                GameWorld world; Start(world); ClearCombatObjects(world);
                world.player_.SetPosition({750.0F,MapPaths::laneY[lane]});
                world.bot_.SetPosition({750.0F,MapPaths::laneY[lane+1]});
                world.botAi_.botState_ = BotState::Engage;
                world.autoAim_ = mode == 0;
                PlayerCommand command; command.mouseWorldPosition = world.bot_.Position();
                command.basic = mode != 2; command.clickBasic = mode == 0; command.ultimate = mode == 2;
                world.Update(command,0.01F);
                Check(world.ProjectileCount() == 1 && world.projectiles_[0]->owner == Team::Blue,
                      "Bot fired through wall or directional input failed");
                for (int step=0; step<100; ++step) { world.UpdateProjectiles(0.01F); }
                Check(world.bot_.Health() == 100 && world.ProjectileCount() == 0,
                      "LMB/Mouse Basic/Ultimate damaged target across wall");
            }
        }
    }
    static void WallTowerAndMinionRules()
    {
        for (Team team : {Team::Blue,Team::Red})
        for (int lane : {0,1})
        {
            GameWorld world; Start(world); ClearCombatObjects(world);
            const float otherX = team == Team::Blue ? 560.0F : 1360.0F;
            Tower& tower = world.LaneTower(team,lane); tower.hp = 300;
            Hero& enemy = team == Team::Blue ? static_cast<Hero&>(world.bot_) : static_cast<Hero&>(world.player_);
            enemy.SetPosition({otherX,MapPaths::laneY[lane+1]});
            world.UpdateTowers(0.01F);
            Check(enemy.Health() == 100 && world.towerShots_.empty(), "Tower fired through lane wall");
            const Team other = team == Team::Blue ? Team::Red : Team::Blue;
            world.minions_.push_back({other,{otherX,MapPaths::laneY[lane+1]},40,0,lane,10,1});
            world.UpdateTowers(0.01F);
            Check(world.minions_[0].hp == 40, "Tower selected wall-blocked minion");
            world.minions_.clear(); enemy.SetPosition({otherX,MapPaths::laneY[lane]});
            world.UpdateTowers(0.01F);
            Check(enemy.Health() == 85 && !world.towerShots_.empty(), "Tower lost same-lane attack");
            world.minions_.clear(); tower.hp=0;
            const float edge = lane == 0 ? 540.0F : 900.0F;
            world.minions_.push_back({team,{750,edge-22},40,0,lane,1,1});
            world.minions_.push_back({other,{750,edge+22},40,0,lane,2,1});
            world.UpdateMinions(0.01F);
            Check(world.minions_[0].hp == 40 && world.minions_[1].hp == 40, "Minion attack ignored wall");
        }
    }
    static void BotDefensivePatrol()
    {
        for (int lane : {0,1,2})
        {
            GameWorld world; Start(world); world.minions_.clear();
            world.player_.SetPosition({105.0F,720.0F});
            world.botAi_.botLane_=lane; world.bot_.SetPosition({1575.0F,MapPaths::laneY[lane]});
            world.bot_.Damage(75); world.botAi_.botState_=BotState::Recover;
            world.botAi_.botStrafeTargetX_=1060;
            for(int step=0;step<50;++step) { world.UpdateBot(0.01F); world.UpdateProjectiles(0.01F); }
            Check(world.ProjectileCount()==0,"defensive Bot fired without a target");
            const int differentLane=(lane+1)%3;
            world.minions_.push_back({Team::Blue,{1410.0F,MapPaths::laneY[differentLane]},40,0,differentLane,1,1});
            for(int step=0;step<50;++step) { world.UpdateBot(0.01F); world.UpdateProjectiles(0.01F); }
            Check(world.minions_[0].hp==40 && world.ProjectileCount()==0,"defensive Bot attacked another lane");
            world.minions_.push_back({Team::Blue,{1410.0F,MapPaths::laneY[lane]},40,0,lane,2,1});
            float minX=world.bot_.Position().x,maxX=minX;
            for(int step=0;step<400;++step)
            {
                world.UpdateBot(0.01F); world.UpdateProjectiles(0.01F);
                const Vec2 position=world.bot_.Position(); minX=std::min(minX,position.x); maxX=std::max(maxX,position.x);
                Check(MapPaths::Contains(position,MapPaths::heroRadius) && Near(position.y,MapPaths::laneY[lane]),
                      "defensive patrol left its lane");
            }
            Check(maxX-minX>20 && world.minions_[1].hp<=20,"Bot did not patrol and repeatedly hit minions");
            for(int step=0;step<400 && world.botAi_.botState_==BotState::Recover;++step) { world.UpdateBot(0.01F); }
            Check(world.botAi_.botState_==BotState::Push && world.bot_.Health()>=55,"defensive patrol never returned to Push");
        }
        GameWorld timeout; Start(timeout); timeout.minions_.clear();
        timeout.player_.SetPosition({105.0F,720.0F}); timeout.bot_.SetPosition({1575.0F,720.0F}); timeout.bot_.Damage(75);
        timeout.botAi_.botState_=BotState::Recover; timeout.botAi_.botRecoveryTime_=17.99F;
        timeout.UpdateBot(0.02F); timeout.UpdateBot(0.1F);
        Check(timeout.botAi_.botState_==BotState::Push && timeout.botAi_.botRetreatCooldown_>0,"recovery timeout immediately re-entered Retreat");
    }
    static void BotWallDetour()
    {
        for(int from : {0,1,2})
        for(int to : {0,1,2})
        {
            if(from==to) {continue;}
            GameWorld world; Start(world); ClearCombatObjects(world);
            world.bot_.SetPosition({1050.0F,MapPaths::laneY[from]});
            world.player_.SetPosition({1050.0F,MapPaths::laneY[to]});
            world.botAi_.botState_=BotState::Engage;
            bool arrived=false; float farthest=700;
            for(int step=0;step<1400;++step)
            {
                world.UpdateBot(0.01F); world.projectiles_.clear();
                const Vec2 position=world.bot_.Position();farthest=std::max(farthest,position.x);
                Check(MapPaths::Contains(position,MapPaths::heroRadius),"Bot detour left road");
                if(Distance(position,world.player_.Position())<10){arrived=true;break;}
            }
            Check(arrived && farthest>=1040,("Bot detour "+std::to_string(from)+"->"+std::to_string(to)+
                " stopped at "+std::to_string(world.bot_.Position().x)+","+std::to_string(world.bot_.Position().y)+
                ";farthest="+std::to_string(farthest)).c_str());
        }
    }
    static void RoadBounds()
    {
        Check(MapPaths::Contains({105.0F,720.0F}, MapPaths::heroRadius) &&
              MapPaths::Contains({1815.0F,720.0F}, MapPaths::heroRadius) &&
              MapPaths::Contains({960.0F,720.0F}, MapPaths::heroRadius), "Base/heal outside road");
        for (int lane = 0; lane < 3; ++lane)
        {
            Check(MapPaths::Contains({750.0F,MapPaths::laneY[lane]}, MapPaths::heroRadius), "lane outside road");
        }
        Vec2 position{750.0F,360.0F};
        for (int i = 0; i < 100; ++i)
        {
            position = MapPaths::MoveInside(position, {0,8}, MapPaths::heroRadius);
            Check(MapPaths::Contains(position, MapPaths::heroRadius), "hero crossed road boundary");
        }
        Check(position.y < 510, "hero switched lanes away from fork");
        const Vec2 fork = MapPaths::MoveInside({255.0F,720.0F}, {90,-180}, MapPaths::heroRadius);
        Check(MapPaths::Contains(fork, MapPaths::heroRadius) && fork.y < 720, "fork route inaccessible");
        Check(MapPaths::Contains({750.0F,360.0F}, MapPaths::heroRadius) &&
              MapPaths::Contains({1170.0F,1080.0F}, MapPaths::heroRadius), "brush inaccessible");
    }
    static void RoadJunctionRoundTrip()
    {
        // Player and Bot use the hero radius; Minions use the smaller radius.
        for (float radius : {MapPaths::heroRadius, MapPaths::minionRadius})
        for (Team team : {Team::Blue, Team::Red})
        for (int lane : {0, 2})
        {
            const auto route = MapPaths::Route(team, lane);
            for (int direction : {1, -1})
            {
                Vec2 position = route[direction > 0 ? 0 : 5];
                for (int step = 0; step < 5; ++step)
                {
                    const Vec2 target = route[direction > 0 ? step + 1 : 4 - step];
                    position = MapPaths::MoveInside(position, target - position, radius);
                    Check(Distance(position, target) < 0.02F && MapPaths::Contains(position, radius),
                          "road junction blocked round trip");
                }
            }
        }
    }
    static void MinionPriority()
    {
        for (Team team : {Team::Blue, Team::Red})
        {
            GameWorld world; Start(world);
            world.minions_.clear();
            const int lane = 1;
            const float towerX = team == Team::Blue ? MapPaths::redTowerX : MapPaths::blueTowerX;
            const float approach = team == Team::Blue ? -1.0F : 1.0F;
            Tower& tower = world.LaneTower(team == Team::Blue ? Team::Red : Team::Blue, lane);
            world.minions_.push_back({team,{towerX + approach*60,720},40,0,lane,1,3});
            world.minions_.push_back({team == Team::Blue ? Team::Red : Team::Blue,
                                      {towerX + approach*80,720},40,0,lane,2,3});
            world.UpdateMinions(0.01F);
            Check(tower.hp == 292 && world.minions_[1].hp == 40, "tower did not outrank minion");
            world.minions_[0].attackCd = 0;
            world.minions_[0].position = {towerX + approach*120,720};
            world.minions_[1].position = {towerX + approach*140,720};
            world.UpdateMinions(0.01F);
            Check(world.minions_[1].hp == 32, "distant tower blocked minion combat");
            tower.hp = 0;
            world.minions_[0].position = {team == Team::Blue ? 1760.0F : 160.0F,720};
            world.minions_[1].position = {team == Team::Blue ? 1780.0F : 140.0F,720};
            world.minions_[0].attackCd = 0;
            const int baseHp = (team == Team::Blue ? world.redBase_ : world.blueBase_).hp;
            world.UpdateMinions(0.01F);
            Check((team == Team::Blue ? world.redBase_ : world.blueBase_).hp == baseHp &&
                  world.minions_[1].hp == 24, "enemy minion did not interrupt Nexus siege");
            world.minions_[1].hp = 0;
            world.minions_[0].attackCd = 0;
            world.UpdateMinions(0.01F);
            Check((team == Team::Blue ? world.redBase_ : world.blueBase_).hp == baseHp - 8,
                  "Nexus siege did not resume");
        }
    }
    static void BotCoverFire()
    {
        GameWorld world; Start(world);
        world.minions_.clear();
        world.bot_.SetPosition({1635.0F,720.0F});
        world.player_.SetPosition({1200.0F,720.0F});
        world.bot_.Damage(75);
        world.UpdateBot(0.01F);
        Check(world.CurrentBotState() == BotState::Retreat || world.CurrentBotState() == BotState::Recover,
              "low HP Bot did not retreat");
        Check(MapPaths::Contains(world.DefensivePoint(), MapPaths::heroRadius), "cover point is off road");
        Check(world.ProjectileCount() == 1, "retreating Bot did not fire clear Basic");
        world.projectiles_.clear(); world.botAi_.botAttackTimer_ = 0;
        world.minions_.push_back({Team::Blue,{1455.0F,720.0F},40,0,1,100,3});
        world.UpdateBot(0.01F);
        Check(world.ProjectileCount() == 1, "Bot did not select blocking enemy minion");
        for (int i = 0; i < 50; ++i) { world.UpdateProjectiles(0.01F); }
        Check(world.minions_[0].hp == 35, "defensive Basic did not hit the enemy minion");

        GameWorld crossLane; Start(crossLane);
        crossLane.minions_.clear();
        crossLane.bot_.SetPosition({1200.0F,720.0F});
        crossLane.player_.SetPosition({1050.0F,360.0F});
        crossLane.bot_.Damage(75);
        bool reachedCover = false;
        for (int i = 0; i < 200; ++i)
        {
            crossLane.UpdateBot(0.05F);
            Check(MapPaths::Contains(crossLane.bot_.Position(), MapPaths::heroRadius),
                  "retreat path left road");
            if (crossLane.CurrentBotState() == BotState::Recover) { reachedCover = true; break; }
        }
        Check(reachedCover, "cross-lane retreat never reached safe point");
    }
    static void G01()
    {
        GameWorld world;
        Check(world.State() == GameState::Menu, "initial state");
        Start(world);
        const auto snapshot = world.CreateSnapshot();
        Check(snapshot.entities.size() == 30, "initial objects not created once");
        Check(world.minions_.size() == 12 && world.projectiles_.empty(), "initial wave/projectiles");
        Check(world.player_.UltimateCooldown() == 0 && world.bot_.UltimateCooldown() == 0, "initial cooldowns");
    }
    static void G02()
    {
        Check(Near(Length(InputManager::MoveVector(true, false, true, false)), 1), "diagonal normalization");
        Check(InputManager::MoveVector(true, false, false, false).x == -1, "A movement");
        Check(InputManager::BasicCommand(true, false) == InputManager::BasicCommand(false, true), "LMB/K alias");
        const Vec2 center = InputManager::MouseWorldPosition({632,340}, {0, 0, 1264, 680});
        Check(Near(center.x, 640) && Near(center.y, 360), "client-to-world mouse conversion");
        GameWorld world; Start(world);
        const Vec2 initial = world.player_.Position();
        PlayerCommand command;
        command.move = InputManager::MoveVector(false, true, false, false);
        command.mouseWorldPosition = {600.0F,720.0F};
        command.basic = true;
        command.ultimate = true;
        world.Update(command, 0.01F);
        Check(world.player_.Position().x > initial.x && world.ProjectileCount() == 2, "move/attacks");
        Check(Near(world.player_.UltimateCooldown(), 10), "Ultimate ten-second cooldown");
    }
    static void G03()
    {
        GameWorld world; Start(world);
        world.player_.SetPosition({1240.0F,720.0F});
        world.Update({}, 0.1F);
        Check(world.CurrentBotState() == BotState::Engage, "Engage not selected in world update");
        world.bot_.Damage(75);
        world.Update({}, 0.1F);
        Check(world.CurrentBotState() == BotState::Retreat, "Retreat not selected in world update");
        world.bot_.Respawn();
        world.bot_.SetPosition({900.0F,720.0F});
        world.player_.SetPosition({1815.0F,720.0F});
        world.Update({}, 0.1F);
        Check(world.CurrentBotState() == BotState::Defend, "Defend not selected in world update");
        world.bot_.SetPosition({1530,720});
        world.player_.SetPosition(MapPaths::blueBase);
        world.Update({}, 0.1F);
        Check(world.CurrentBotState() == BotState::Push, "Push not selected in world update");
        world.bot_.Die();
        world.Update({}, 0.1F);
        Check(world.CurrentBotState() == BotState::Dead, "Dead not selected in world update");
        world.bot_.Respawn();
        world.player_.SetPosition({960.0F,1040.0F});
        world.bot_.SetPosition({960.0F,1040.0F});
        world.Update({}, 0.01F);
        Check(Distance(world.player_.Position(), world.bot_.Position()) >= 39.99F, "heroes overlap after update");
    }
    static void G04()
    {
        GameWorld world; Start(world);
        world.minions_[0].position = {1460.0F,360.0F};
        world.minions_[0].waypoint = 3;
        world.redTowers_[0].hp = 8;
        world.UpdateMinions(0.01F);
        Check(world.redTowers_[0].hp == 0, "minion did not destroy enemy tower");
        const float previous = world.minions_[0].position.x;
        world.UpdateMinions(0.01F);
        Check(world.minions_[0].position.x > previous, "minion did not advance after tower destruction");
        for (int step = 0; step < 1200; ++step) { world.Update({}, 0.01F); }
        int freshBlue = 0, freshRed = 0;
        for (const Minion& minion : world.minions_)
        {
            if (minion.team == Team::Blue && Near(minion.position.x, 205.0F, 2.0F)) { ++freshBlue; }
            if (minion.team == Team::Red && Near(minion.position.x, 1715.0F, 2.0F)) { ++freshRed; }
        }
        if (freshBlue != 6 || freshRed != 6)
        {
            throw std::runtime_error("12-second wave: blue=" + std::to_string(freshBlue) +
                " red=" + std::to_string(freshRed) + " timer=" + std::to_string(world.waveTimer_) +
                " state=" + std::to_string(static_cast<int>(world.State())));
        }
    }
    static void G05()
    {
        GameWorld world; Start(world);
        world.bot_.SetPosition({1170.0F,1080.0F});
        auto snapshot = world.CreateSnapshot();
        Check(!Find(snapshot, EntityType::BotHero).visible && !Renderer::ShouldDraw(Find(snapshot, EntityType::BotHero)), "brush concealment/render flag");
        world.player_.SetPosition({1170.0F,1080.0F});
        const auto sharedSnapshot = world.CreateSnapshot();
        Check(Find(sharedSnapshot, EntityType::BotHero).visible, "shared brush visibility");
        world.player_.SetPosition({210.0F,720.0F});
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
            const int beforeHp = world.player_.MaxHealth();
            const int beforeDamage = world.player_.BasicDamage();
            const float beforeSpeed = world.player_.Speed();
            const UpgradeChoice choice = level % 3 == 1 ? UpgradeChoice::Health : level % 3 == 2 ? UpgradeChoice::Attack : UpgradeChoice::MoveSpeed;
            world.player_.SetPreferredUpgrade(choice);
            world.player_.AddXp(requirements[level - 1] - world.player_.Xp());
            Check(world.player_.Level() == level + 1, "level did not advance");
            if (choice == UpgradeChoice::Health) { Check(world.player_.MaxHealth() == beforeHp + 20, "HP upgrade"); }
            if (choice == UpgradeChoice::Attack) { Check(world.player_.BasicDamage() == beforeDamage + 2, "attack upgrade"); }
            if (choice == UpgradeChoice::MoveSpeed) { Check(Near(world.player_.Speed(), beforeSpeed + 15), "speed upgrade"); }
        }
        world.player_.AddXp(1000);
        Check(world.player_.Level() == 7 && !world.player_.UpgradePending(), "level cap");
    }
    static void G07()
    {
        GameWorld world; Start(world);
        PlayerCommand fire; fire.ultimate = true; fire.mouseWorldPosition = {600.0F,720.0F};
        world.Update(fire, 0.01F);
        PlayerCommand pause; pause.togglePause = true;
        world.Update(pause, 0.01F);
        Check(world.State() == GameState::Paused, "pause state");
        const float cooldown = world.player_.UltimateCooldown();
        world.Update({}, 12.0F);
        Check(Near(world.player_.UltimateCooldown(), cooldown), "pause advanced cooldown");
        world.Update(pause, 0.01F);
        Check(world.State() == GameState::Playing, "resume state");
        world.DamageBase(world.redBase_, 500);
        world.Update({}, 0.01F);
        Check(world.State() == GameState::Result && world.CreateHud().result == L"Victory", "base/result");
    }
    static void G08()
    {
        GameWorld world; Start(world);
        world.minions_.clear();
        InputManager input;
        PlayerCommand both = input.ReadAttackEdges(true, true, false, false);
        both.mouseWorldPosition = {600.0F,720.0F};
        world.Update(both, 0.01F);
        Check(world.ProjectileCount() == 1, "simultaneous LMB/K generated two shots");
        PlayerCommand held = input.ReadAttackEdges(true, true, false, false);
        world.Update(held, 0.01F);
        Check(world.ProjectileCount() == 1, "held input generated repeated shot");
        input.ReadAttackEdges(false, false, false, false);
        PlayerCommand key = input.ReadAttackEdges(false, true, false, false);
        world.Update(key, 0.01F);
        Check(world.ProjectileCount() == 2, "new K press did not fire immediately");
        input.ReadAttackEdges(false, false, false, false);
        PlayerCommand click = input.ReadAttackEdges(true, false, false, false);
        click.mouseWorldPosition = {600.0F,720.0F};
        world.Update(click, 0.01F);
        Check(world.ProjectileCount() == 3, "new LMB click did not fire immediately");
        Hero repeated(Team::Blue, {210.0F,720.0F});
        repeated.StartUltimate();
        for (int step = 0; step < 999; ++step) { repeated.Tick(0.01F); }
        Check(!repeated.UltimateReady(), "Ultimate cooldown ended early");
        repeated.Tick(0.01F);
        Check(repeated.UltimateReady(), "Ultimate cooldown did not end after ten seconds");
    }
    static void G09()
    {
        GameWorld world; Start(world);
        world.player_.SetPosition({750.0F,1000.0F});
        PlayerCommand command; command.mouseWorldPosition = {850.0F,1100.0F}; command.toggleAim = true;
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
        world.player_.Tick(5.0F);
        command.ultimate = false;
        command.toggleAim = false;
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
        PlayerCommand command; command.move = {1, 0}; command.basic = true; command.mouseWorldPosition = {600.0F,720.0F};
        const Vec2 deathPosition = world.player_.Position();
        world.Update(command, 0.01F);
        Check(!world.player_.Alive() && Distance(world.player_.Position(), deathPosition) == 0 && world.ProjectileCount() == 0, "dead input acted");
        for (int step = 0; step < 498; ++step) { world.Update({}, 0.01F); }
        Check(!world.player_.Alive(), "respawn before five seconds");
        world.Update({}, 0.01F);
        Check(world.player_.Alive() && world.player_.Health() == world.player_.MaxHealth(), "respawn health/time");
        Check(Near(world.player_.Position().x, 205), "respawn position");
        Hero levelSeven(Team::Blue, {210.0F,720.0F});
        for (int level = 1; level < 7; ++level) { levelSeven.AddXp(levelSeven.RequiredXp()); }
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
        world.minions_.push_back({Team::Blue, {1460.0F,720.0F}});
        world.minions_.push_back({Team::Red, {1465.0F,720.0F}});
        world.bot_.SetPosition({1465.0F,720.0F});
        const int heroHp = world.bot_.Health();
        const int minionHp = world.minions_[1].hp;
        world.UpdateMinions(0.01F);
        Check(world.redTowers_[1].hp == 292 && world.bot_.Health() == heroHp && world.minions_[1].hp == minionHp, "minion attacked non-tower target");
        world.blueTowers_[1].position = {1465.0F,720.0F};
        world.blueTowers_[1].attackCd = 0.0F;
        world.UpdateTowers(0.01F);
        Check(world.minions_[1].hp == minionHp - 15 && world.bot_.Health() == heroHp,
              "tower did not prioritize minion over hero");
    }
    static void BotRecoveryFsm()
    {
        GameWorld world; Start(world);
        const Vec2 towerCenter = world.redTowers_[1].position;
        const Vec2 defensivePoint = world.DefensivePoint();
        Check(Near(Distance(defensivePoint, towerCenter), 30.0F) && defensivePoint.x > towerCenter.x &&
              Near(defensivePoint.y, towerCenter.y),
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
        world.redTowers_[world.botAi_.botLane_].hp = 0;
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
    static void G14()
    {
        Hero hero(Team::Blue, {210.0F,720.0F});
        hero.Damage(20);
        hero.TickRegeneration(4.9F, true);
        Check(hero.Health() == 80, "regen began before five seconds");
        hero.TickRegeneration(0.6F, true);
        Check(hero.Health() == 81, "regen did not use fractional delta time");
        hero.Damage(1);
        hero.TickRegeneration(4.9F, true);
        Check(hero.Health() == 80, "second hit did not reset delay");
        hero.TickRegeneration(1.1F, true);
        Check(hero.Health() == 82, "regen after repeated hit");
        hero.TickRegeneration(100.0F, true);
        Check(hero.Health() == hero.MaxHealth(), "regen exceeded maximum HP");
        hero.Die(); hero.TickRegeneration(20.0F, true);
        Check(hero.Health() == 0, "dead hero regenerated");
        GameWorld world; Start(world);
        world.player_.Damage(20);
        PlayerCommand pause; pause.togglePause = true;
        world.Update(pause, 0.01F);
        world.Update({}, 20.0F);
        Check(world.player_.Health() == 80, "paused world regenerated");
        world.bot_.Damage(70);
        world.bot_.SetPosition(world.DefensivePoint());
        world.botAi_.botState_ = BotState::Recover;
        world.Update(pause, 0.01F);
        world.Update({}, 0.5F);
        Check(world.bot_.Health() == 32, "Bot Recover combined with natural regen");
    }
    static void G15()
    {
        GameWorld world; Start(world);
        world.player_.AddXp(99);
        Check(world.player_.Level() == 1, "early level up");
        Minion victim{Team::Red, {900.0F,720.0F}};
        world.DamageMinion(victim, Team::Blue, 40);
        world.DamageMinion(victim, Team::Blue, 40);
        Check(world.player_.Level() == 2 && world.player_.Xp() == 19 && world.player_.BasicDamage() == 7,
              "kill XP boundary or duplicate award");
        PlayerCommand prefer; prefer.upgrade = UpgradeChoice::Health;
        world.Update(prefer, 0.01F);
        world.player_.AddXp(120 - world.player_.Xp());
        Check(world.player_.Level() == 3 && world.player_.MaxHealth() == 120 &&
              world.CreateHud().upgradePreference == L"HP", "preferred upgrade not applied immediately");
        world.player_.AddXp(10000);
        Check(world.player_.Level() == 7 && world.player_.Xp() > 0 && world.player_.RequiredXp() == 0,
              "chained level up or XP remainder");
        Hero enemy(Team::Red, {1500.0F,720.0F});
        world.DamageHero(enemy, Team::Blue, 100);
        const int xp = world.player_.Xp();
        world.DamageHero(enemy, Team::Blue, 100);
        Check(world.player_.Xp() == xp, "Hero kill awarded XP twice");
    }
    static void G16()
    {
        GameWorld world; Start(world);
        for (int lane = 0; lane < 3; ++lane)
        {
            Check(Near(world.blueTowers_[lane].position.y, MapPaths::laneY[lane]) &&
                  Near(world.redTowers_[lane].position.y, MapPaths::laneY[lane]), "tower lane placement");
            int blue = 0, red = 0;
            for (const Minion& minion : world.minions_)
            {
                if (minion.lane == lane && minion.team == Team::Blue) { ++blue; }
                if (minion.lane == lane && minion.team == Team::Red) { ++red; }
            }
            Check(blue == 2 && red == 2, "initial lane wave count");
        }
        for (std::size_t left = 0; left < world.minions_.size(); ++left)
        for (std::size_t right = left + 1; right < world.minions_.size(); ++right)
        {
            if (world.minions_[left].team == world.minions_[right].team)
            {
                Check(Distance(world.minions_[left].position, world.minions_[right].position) >= 19.0F,
                      "same-team minions stacked at Base spawn");
            }
        }
        world.minions_.clear();
        world.minions_.push_back({Team::Blue, {1460.0F,360.0F}, 40, 0, 0});
        world.botAi_.botLane_ = 0;
        Check(Near(world.DefensivePoint().y, 360.0F) && Near(world.DefensivePoint().x, 1560.0F),
              "retreat chose wrong lane tower");
        world.redTowers_[0].hp = 8;
        world.UpdateMinions(0.01F);
        Check(world.redTowers_[0].hp == 0 && world.redTowers_[1].hp == 300,
              "minion attacked another lane");
        Check(Near(world.DefensivePoint().x, world.redBase_.position.x), "destroyed lane tower did not fall back to Base");
        world.bot_.SetPosition({1050.0F,1080.0F});
        world.player_.SetPosition({1095.0F,1080.0F});
        world.Update({}, 0.01F);
        Check(world.botAi_.botLane_ == 2, "Bot did not select threatened lower lane");
    }
    static void G17()
    {
        GameWorld world; Start(world);
        world.minions_.clear();
        world.player_.SetPosition({1170.0F,720.0F});
        world.bot_.SetPosition({1170.0F,1080.0F});
        std::wstring target;
        Check(world.AutoAim(1000, &target).x > 0 && target == L"Tower",
              "concealed Bot was selected by auto aim");
        world.redTowers_[0].hp = world.redTowers_[1].hp = world.redTowers_[2].hp = 0;
        world.redBase_.hp = 0;
        Check(Length(world.AutoAim(700, &target)) == 0 && target == L"None", "no-target auto aim");
        world.bot_.StartBasic();
        Check(Length(world.AutoAim(700, &target)) == 0 && target == L"None", "reveal bypassed lane wall");
        world.player_.SetPosition({960.0F,1080.0F});
        Check(world.AutoAim(700, &target).x > 0 && target == L"Bot Hero 1", "same-lane revealed target not selected");
        world.player_.SetPosition({750.0F,720.0F});
        world.bot_.Tick(1.5F);
        PlayerCommand click; click.basic = click.clickBasic = true; click.mouseWorldPosition = {750.0F,1000.0F};
        world.Update(click, 0.01F);
        Check(world.projectiles_[0]->direction.y > 0.99F, "LMB did not use click direction in Auto mode");
        const int beforeKey = world.ProjectileCount();
        PlayerCommand key; key.basic = true;
        world.Update(key, 0.01F);
        Check(world.ProjectileCount() == beforeKey + 1,
              "new K press did not fire immediately after LMB");
        PlayerCommand mouse; mouse.toggleAim = true; mouse.ultimate = true;
        mouse.mouseWorldPosition = {600.0F,720.0F};
        world.Update(mouse, 0.01F);
        Check(world.CreateHud().aimMode == L"Mouse" && world.CreateHud().basicAutoTarget == L"Off" &&
              world.projectiles_.back()->direction.x < -0.99F,
              "Mouse mode J direction");
        GameWorld tied; Start(tied);
        tied.minions_.clear();
        tied.player_.SetPosition({750.0F,720.0F});
        tied.bot_.Die();
        for (Tower& tower : tied.redTowers_) { tower.hp = 0; }
        tied.redBase_.hp = 0;
        tied.minions_.push_back({Team::Red, {900.0F,700.0F}, 40, 0, 1, 5});
        tied.minions_.push_back({Team::Red, {900.0F,740.0F}, 40, 0, 1, 3});
        Check(tied.AutoAim(700).y > 0, "equal-distance target did not use fixed EntityId");
        tied.minions_.clear();
        PlayerCommand noTarget; noTarget.basic = true;
        tied.Update(noTarget, 0.01F);
        Check(!tied.projectiles_.empty() && tied.projectiles_[0]->direction.x > 0.99F,
              "no-target attack did not use initial Base direction");
    }
    static void BotVisibilityRegression()
    {
        GameWorld world; Start(world);
        const auto initial = Find(world.CreateSnapshot(), EntityType::BotHero);
        Check(initial.id == 1 && initial.alive && initial.visible && Renderer::ShouldDraw(initial),
              "alive non-brush Bot vanished at spawn distance");
        world.minions_.clear();
        world.player_.SetPosition({1050.0F,720.0F});
        world.bot_.SetPosition({1170.0F,1080.0F});
        const auto concealed = Find(world.CreateSnapshot(), EntityType::BotHero);
        Check(concealed.alive && !concealed.visible && !Renderer::ShouldDraw(concealed),
              "brush concealment did not match render flag");
        std::wstring target;
        for (Tower& tower : world.redTowers_) { tower.hp = 0; }
        world.redBase_.hp = 0;
        Check(world.AutoAim(700, &target).x == 0 && target == L"None",
              "AutoAim selected a Bot hidden by the renderer");
        world.bot_.SetPosition({1450.0F,720.0F});
        const auto emerged = Find(world.CreateSnapshot(), EntityType::BotHero);
        Check(emerged.visible && Renderer::ShouldDraw(emerged) && world.AutoAim(700, &target).x > 0 &&
              target == L"Bot Hero 1", "Bot did not reappear or match AutoAim after brush exit");
        world.bot_.Die();
        const auto dead = Find(world.CreateSnapshot(), EntityType::BotHero);
        Check(!dead.alive && !Renderer::ShouldDraw(dead) && Length(world.AutoAim(700)) == 0,
              "dead Bot remained rendered or targetable");
        world.bot_.Tick(5.0F);
        Check(world.bot_.RespawnReady(), "Bot respawn timer did not finish");
        world.bot_.Respawn();
        const auto respawned = Find(world.CreateSnapshot(), EntityType::BotHero);
        Check(respawned.alive && respawned.visible && Renderer::ShouldDraw(respawned),
              "respawned Bot invisible at distant spawn");
    }
    static void BasicDamagePath()
    {
        GameWorld world; Start(world);
        world.minions_.clear();
        world.player_.SetPosition({750.0F,720.0F});
        world.bot_.SetPosition({900.0F,720.0F});
        const int botHp = world.bot_.Health();
        PlayerCommand click; click.basic = click.clickBasic = true; click.mouseWorldPosition = world.bot_.Position();
        world.Update(click, 0.01F);
        const auto playerShot = std::find_if(world.projectiles_.begin(), world.projectiles_.end(),
            [](const auto& shot) { return shot->owner == Team::Blue && shot->kind == ProjectileKind::Basic; });
        Check(playerShot != world.projectiles_.end() && (*playerShot)->damage == 5,
              "Player Basic spawn damage is not 5");
        world.UpdateProjectiles(0.18F);
        Check(world.bot_.Health() == botHp - 5 &&
              std::none_of(world.projectiles_.begin(), world.projectiles_.end(),
                  [](const auto& shot) { return shot->owner == Team::Blue; }),
              "one Basic hit did not deal exactly 5 to Bot and consume projectile");
        world.UpdateProjectiles(0.1F);
        Check(world.bot_.Health() == botHp - 5, "Basic projectile damaged Bot twice");

        GameWorld towerWorld; Start(towerWorld);
        towerWorld.minions_.clear();
        towerWorld.bot_.Die();
        towerWorld.player_.SetPosition({1380.0F,720.0F});
        towerWorld.SpawnProjectile(Team::Blue, towerWorld.player_.Position(), {1, 0},
                                   ProjectileKind::Basic, towerWorld.player_.BasicDamage());
        towerWorld.UpdateProjectiles(0.2F);
        Check(towerWorld.redTowers_[1].hp == 295 && towerWorld.ProjectileCount() == 0,
              "one Basic hit did not deal exactly 5 to Tower");
        towerWorld.player_.AddXp(100);
        Check(towerWorld.player_.BasicDamage() == 7 && towerWorld.player_.UltimateDamage() == 38,
              "Attack upgrade damage values changed");
        const Vec2 mapped = InputManager::MouseWorldPosition({400,225}, {0, 0, 800, 450});
        Check(Near(mapped.x, 640) && Near(mapped.y, 360), "resized client cursor mapping");
    }
    static void ConnectedLaneSieges()
    {
        for (Team team : {Team::Blue, Team::Red})
        for (int lane = 0; lane < 3; ++lane)
        {
            const auto route = MapPaths::Route(team, lane);
            Check(Near(route[0].y, 720) && Near(route[1].y, 720) &&
                  Near(route[2].y, MapPaths::laneY[lane]) &&
                  Near(route[3].y, MapPaths::laneY[lane]) && Near(route[4].y, 720) &&
                  Near(route[5].y, 720), "route does not connect Base-fork-lane-merge-Base");
            GameWorld world; Start(world);
            world.minions_.clear();
            world.minions_.push_back({team, route[0], 40, 0, lane, 1, 1});
            Tower& targetTower = world.LaneTower(team == Team::Blue ? Team::Red : Team::Blue, lane);
            Base& targetBase = team == Team::Blue ? world.redBase_ : world.blueBase_;
            bool sawTowerStop = false, sawTowerRepeatedDamage = false, sawBaseStop = false;
            int lastTowerHp = targetTower.hp, lastBaseHp = targetBase.hp;
            for (int tick = 0; tick < 2500 && world.State() == GameState::Playing; ++tick)
            {
                world.simulationTime_ += 0.05;
                const Vec2 before = world.minions_[0].position;
                world.UpdateMinions(0.05F);
                world.CheckResult();
                if (targetTower.hp < lastTowerHp)
                {
                    Check(Distance(before, targetTower.position) <= 70.01F,
                          "minion struck tower outside range");
                    sawTowerRepeatedDamage |= targetTower.hp <= 284;
                    sawTowerStop |= Distance(before, world.minions_[0].position) < 0.01F;
                }
                if (targetBase.hp < lastBaseHp)
                {
                    Check(targetTower.hp == 0 && Distance(before, targetBase.position) <= 60.01F,
                          "minion bypassed live tower or struck Base outside range");
                    sawBaseStop |= Distance(before, world.minions_[0].position) < 0.01F;
                }
                lastTowerHp = targetTower.hp;
                lastBaseHp = targetBase.hp;
            }
            Check(sawTowerStop && sawTowerRepeatedDamage && targetTower.hp == 0 && sawBaseStop &&
                  targetBase.hp == 0 && world.State() == GameState::Result &&
                  world.CreateHud().result == (team == Team::Blue ? L"Victory" : L"Defeat"),
                  "minion siege did not finish tower then Nexus in a lane");
        }
    }
    static void TowerAttackVisuals()
    {
        GameWorld world; Start(world);
        world.minions_.clear();
        world.player_.SetPosition({750.0F,1400.0F});
        world.bot_.SetPosition({1170.0F,1400.0F});
        world.UpdateTowers(0.01F);
        Check(world.towerShots_.empty(), "tower fired without target");
        world.minions_.push_back({Team::Red, {580.0F,360.0F}, 40, 0, 0, 1});
        world.minions_.push_back({Team::Blue, {1340.0F,1080.0F}, 40, 0, 2, 2});
        world.UpdateTowers(0.01F);
        Check(world.towerShots_.size() == 2 && world.minions_[0].hp == 25 && world.minions_[1].hp == 25,
              "both teams' towers did not apply 15 damage and create effects");
        int blueEffects = 0, redEffects = 0;
        for (const auto& entity : world.CreateSnapshot().entities)
        {
            if (entity.type != EntityType::TowerAttack) { continue; }
            if (entity.team == Team::Blue) { ++blueEffects; } else { ++redEffects; }
            Check(Renderer::ShouldDraw(entity) && entity.size.x > 0,
                  "tower attack effect missing from renderer snapshot");
        }
        Check(blueEffects == 1 && redEffects == 1, "tower effect faction missing");
        world.minions_.clear();
        world.UpdateTowers(0.19F);
        Check(world.towerShots_.empty(), "tower effect did not expire after target disappeared");
    }
    static void CenterPickup()
    {
        GameWorld world; Start(world);
        world.player_.SetPosition(MapPaths::healPosition);
        world.bot_.SetPosition(MapPaths::healPosition);
        world.UpdatePickup(19.99F);
        Check(!world.pickupActive_, "pickup spawned before 20 seconds");
        world.UpdatePickup(0.01F);
        Check(world.pickupActive_ && Find(world.CreateSnapshot(), EntityType::HealPickup).visible,
              "pickup did not spawn visibly at 20 seconds");
        world.UpdatePickup(0.01F);
        Check(world.pickupActive_, "full-health hero consumed pickup");
        world.player_.Damage(30);
        world.bot_.Damage(30);
        world.UpdatePickup(0.01F);
        Check(!world.pickupActive_ && world.player_.Health() == 90 && world.bot_.Health() == 70,
              "same-tick pickup was not awarded once to stable Player ID");
        world.player_.SetPosition({210.0F,720.0F});
        world.UpdatePickup(29.99F);
        Check(!world.pickupActive_, "pickup respawned before 30 seconds");
        world.UpdatePickup(0.01F);
        Check(!world.pickupActive_ && world.bot_.Health() == 90,
              "Bot did not acquire respawned pickup or exceeded +20 HP");
        world.bot_.SetPosition({1710.0F,720.0F});
        world.UpdatePickup(30.0F);
        Check(world.pickupActive_, "pickup failed to respawn after Bot acquisition");
        world.minions_.push_back({Team::Blue, MapPaths::healPosition});
        world.UpdatePickup(0.01F);
        Check(world.pickupActive_, "minion consumed hero-only pickup");
    }
    static void BotRouteAndPickupLimit()
    {
        GameWorld world; Start(world);
        world.player_.SetPosition({18, 700});
        world.bot_.SetPosition({1710.0F,360.0F});
        const Vec2 before = world.bot_.Position();
        world.UpdateBot(0.1F);
        Check(world.CurrentBotState() == BotState::Push && world.botAi_.botLane_ == 0 &&
              world.bot_.Position().x < before.x && world.bot_.Position().y > before.y,
              "Bot did not enter upper lane via Base merge");
        world.bot_.SetPosition({1200.0F,720.0F});
        world.bot_.Damage(60);
        world.pickupActive_ = true;
        world.botAi_.pickupChaseUsed_ = false;
        world.UpdateBot(0.1F);
        Check(world.botAi_.pickupChaseUsed_ && world.botAi_.pickupChaseTime_ > 2.3F &&
              world.bot_.Position().x < 1200.0F,
              "low-HP Bot did not briefly pursue active center pickup");
        world.UpdateBot(2.5F);
        Check(world.botAi_.pickupChaseTime_ == 0.0F && world.botAi_.pickupChaseUsed_,
              "Bot pickup pursuit did not end within its time limit");
        world.bot_.SetPosition({1200.0F,720.0F});
        world.UpdateBot(0.1F);
        Check(world.botAi_.pickupChaseTime_ == 0.0F, "Bot restarted pickup pursuit in same spawn cycle");
    }
    static void BotAttackCadence()
    {
        GameWorld world; Start(world);
        world.minions_.clear();
        world.player_.SetPosition({1050.0F,720.0F});
        world.bot_.SetPosition({1650.0F,720.0F});
        const auto basicShots = [&world]()
        {
            int count = 0;
            for (const auto& shot : world.projectiles_)
            {
                if (shot->owner == Team::Red && shot->kind == ProjectileKind::Basic) { ++count; }
            }
            return count;
        };
        world.UpdateBot(0.1F);
        Check(basicShots() == 1 && world.bot_.BasicDamage() == 5 &&
              world.bot_.UltimateDamage() == 30, "Bot initial attack values");
        for (int tick = 0; tick < 8; ++tick) { world.UpdateBot(0.1F); }
        Check(basicShots() == 1, "Bot fired Basic faster than AI one-second cadence");
        for (int tick = 0; tick < 2; ++tick) { world.UpdateBot(0.1F); }
        Check(basicShots() == 2, "Bot AI cadence did not allow next Basic");
        PlayerCommand pause; pause.togglePause = true;
        world.Update(pause, 0.01F);
        const float timer = world.botAi_.botAttackTimer_;
        world.Update({}, 5.0F);
        Check(Near(world.botAi_.botAttackTimer_, timer), "Bot AI cadence advanced during Pause");
    }
    static void FlowAndRestart()
    {
        for (int match = 0; match < 10; ++match)
        {
            GameWorld world; Start(world);
            world.bot_.Die();
            world.minions_.clear();
            world.player_.SetPosition({1740.0F,800.0F});
            world.redBase_.hp = 5;
            PlayerCommand command; command.mouseWorldPosition = world.redBase_.position; command.basic = command.clickBasic = true;
            world.Update(command, 0.01F);
            for (int step = 0; step < 20 && world.State() == GameState::Playing; ++step) { world.Update({}, 0.01F); }
            Check(world.State() == GameState::Result && world.CreateHud().result == L"Victory", "projectile-to-Result flow");
            PlayerCommand restart; restart.restart = true;
            world.Update(restart, 0.0F);
            Check(world.State() == GameState::Playing && world.redBase_.hp == 500 && world.ProjectileCount() == 0 && world.minions_.size() == 12, "restart retained prior match state");
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
        view.ultimateCd = 4.0F; view.state = L"Result"; view.result = L"Victory";
        renderer.Render(view);
        HWND first = FindWindowEx(parent, nullptr, L"EasileHudPanel", nullptr);
        HWND second = FindWindowEx(parent, first, L"EasileHudPanel", nullptr);
        RECT firstBounds{};
        GetClientRect(first, &firstBounds);
        HWND label = firstBounds.bottom == 160 ? first : second;
        HWND overlay = label == first ? second : first;
        wchar_t text[512]{};
        GetWindowText(label, text, 512);
        const std::wstring contents(text);
        Check(contents.find(L"80/100") != std::wstring::npos &&
              contents.find(L"Level: 3") != std::wstring::npos &&
              contents.find(L"Basic (LMB/K): PRESS") != std::wstring::npos &&
              contents.find(L"BasicDamage: 5") != std::wstring::npos &&
              contents.find(L"Ultimate (J): 4.0") != std::wstring::npos &&
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
        view.ultimateCd = 0.0F;
        view.state = L"Playing";
        renderer.Render(view);
        GetWindowText(label, text, 512);
        Check(std::wstring(text).find(L"Basic (LMB/K): PRESS") != std::wstring::npos &&
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
        PlayerCommand move; move.move = {1, 0}; move.mouseWorldPosition = {750.0F,720.0F};
        for (int step = 0; step < 11; ++step) { world.Update(move, 0.01F); }
        Check(world.player_.Position().x > playerStart.x && world.bot_.Position().x < botStart.x, "player/bot movement");

        world.minions_.clear();
        world.player_.SetPosition({1050.0F,1000.0F});
        world.bot_.SetPosition({1125.0F,1000.0F});
        PlayerCommand attack; attack.mouseWorldPosition = world.bot_.Position(); attack.basic = attack.ultimate = true;
        const int botHealth = world.bot_.Health();
        world.Update(attack, 0.01F);
        for (int step = 0; step < 6; ++step) { world.Update({}, 0.01F); }
        Check(world.bot_.Health() < botHealth, "projectile collision/damage");
        world.DamageHero(world.bot_, Team::Blue, 100);
        Check(!world.bot_.Alive(), "bot death");
        world.player_.SetPosition({210.0F,720.0F});
        for (int step = 0; step < 500; ++step) { world.Update({}, 0.01F); }
        Check(world.bot_.Alive() && world.bot_.Health() == world.bot_.MaxHealth(), "bot respawn");

        world.minions_.clear();
        world.minions_.push_back({Team::Blue, {1460.0F,720.0F}});
        world.redTowers_[1].hp = 8;
        world.Update({}, 0.01F);
        Check(world.redTowers_[1].hp == 0, "minion/tower progression");
        world.minions_.clear();
        world.bot_.Die();
        world.player_.SetPosition({1740.0F,800.0F});
        world.redBase_.hp = 5;
        PlayerCommand finish; finish.mouseWorldPosition = world.redBase_.position; finish.basic = finish.clickBasic = true;
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

int main(int argc, char** argv)
{
    if(argc>1 && std::string(argv[1])=="--visual-expanded")
    {
        try { GameWorldTests::ExpandedVisualScenarios(); return 0; }
        catch(const std::exception& error) { std::cout<<"VISUAL FAIL: "<<error.what()<<'\n'; return 1; }
    }
    if(argc>1 && std::string(argv[1])=="--visual-scenarios")
    {
        try { GameWorldTests::VisualScenarios(); return 0; }
        catch(const std::exception& error) { std::cout<<"VISUAL FAIL: "<<error.what()<<'\n'; return 1; }
    }
    const std::pair<const char*, std::function<void()>> tests[] = {
        {"TC G18 Ally Bot spawn",GameWorldTests::ExpandedSpawn},
        {"TC G19 second Enemy Bot spawn",GameWorldTests::ExpandedSpawn},
        {"TC G20 multi-Hero combat/XP",GameWorldTests::MultiHeroCombat},
        {"TC G21 expanded lane collision",GameWorldTests::ExpandedTerrain},
        {"TC G22 cross-lane attacks",GameWorldTests::LaneWallCombat},
        {"TC G23 jungle entry",GameWorldTests::JungleEntry},
        {"TC G24 jungle wall collision",GameWorldTests::JungleWalls},
        {"TC G25 jungle brush",GameWorldTests::JungleBrush},
        {"TC G26 expanded minion paths",GameWorldTests::ExpandedMinionPaths},
        {"TC G27 camera/mouse",GameWorldTests::CameraCoordinates},
        {"TC G28 full match/tempo",GameWorldTests::ExpandedFullMatch},
        {"Road bounds", GameWorldTests::RoadBounds},
        {"Lane walls: swept Basic/Ultimate x16", GameWorldTests::LaneWallCombat},
        {"Wall Auto/HUD/LMB/Mouse", GameWorldTests::WallAimAndAttackModes},
        {"Wall Tower/Minion rules", GameWorldTests::WallTowerAndMinionRules},
        {"Bot defensive patrol x3", GameWorldTests::BotDefensivePatrol},
        {"Bot wall detour x6", GameWorldTests::BotWallDetour},
        {"Road junction round trip", GameWorldTests::RoadJunctionRoundTrip},
        {"Minion target priority", GameWorldTests::MinionPriority},
        {"Bot cover fire", GameWorldTests::BotCoverFire},
        {"TC G01", GameWorldTests::G01}, {"TC G02", GameWorldTests::G02},
        {"TC G03", GameWorldTests::G03}, {"TC G04", GameWorldTests::G04},
        {"TC G05", GameWorldTests::G05}, {"TC G06", GameWorldTests::G06},
        {"TC G07", GameWorldTests::G07}, {"TC G08", GameWorldTests::G08},
        {"TC G09", GameWorldTests::G09}, {"TC G10", GameWorldTests::G10},
        {"TC G11", GameWorldTests::G11}, {"TC G12", GameWorldTests::G12},
        {"TC G13 Bot FSM recovery", GameWorldTests::BotRecoveryFsm},
        {"TC G14", GameWorldTests::G14}, {"TC G15", GameWorldTests::G15},
        {"TC G16", GameWorldTests::G16}, {"TC G17", GameWorldTests::G17},
        {"Bot visibility regression", GameWorldTests::BotVisibilityRegression},
        {"Basic damage path", GameWorldTests::BasicDamagePath},
        {"Connected lane sieges x6", GameWorldTests::ConnectedLaneSieges},
        {"Tower attack visuals", GameWorldTests::TowerAttackVisuals},
        {"Center pickup", GameWorldTests::CenterPickup},
        {"Bot route/pickup limit", GameWorldTests::BotRouteAndPickupLimit},
        {"Bot attack cadence", GameWorldTests::BotAttackCadence},
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
