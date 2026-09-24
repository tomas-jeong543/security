#pragma once
#include "Hero.h"
class PlayerHero final:public Hero{public:explicit PlayerHero(Vec2 s):Hero(Team::Blue,s){}};
