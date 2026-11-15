#pragma once
#include "Hero.h"
class BotHero final:public Hero{public:explicit BotHero(Vec2 s, Team team = Team::Red):Hero(team,s){}};
