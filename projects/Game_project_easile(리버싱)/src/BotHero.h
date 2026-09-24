#pragma once
#include "Hero.h"
class BotHero final:public Hero{public:explicit BotHero(Vec2 s):Hero(Team::Red,s){}};
