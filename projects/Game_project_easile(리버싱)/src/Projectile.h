#pragma once
#include "Hero.h"
enum class ProjectileKind{Basic,Ultimate};struct Projectile{Team owner;Vec2 position,direction;ProjectileKind kind;int damage;float speed,maxDistance,traveled=0;};
