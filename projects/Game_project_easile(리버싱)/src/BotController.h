#pragma once
#include "GameWorld.h"
class BotController final
{
public:
    BotState Decide(const BotHero& bot, const PlayerHero& player, Vec2 basePosition) const
    {
        if (!bot.Alive()) { return BotState::Dead; }
        if (bot.MaxHealth() > 0 && bot.Health() * 100 <= bot.MaxHealth() * 30) { return BotState::Retreat; }
        if (player.Alive() && Distance(player.Position(), basePosition) <= 450.0F) { return BotState::Defend; }
        if (player.Alive() && Distance(bot.Position(), player.Position()) <= 500.0F) { return BotState::Engage; }
        return BotState::Push;
    }
    Vec2 MoveDirection(BotState state, const BotHero& bot, const PlayerHero& player, Vec2 retreatTarget) const
    {
        if (state == BotState::Retreat) { return Normalize(retreatTarget - bot.Position()); }
        if (state == BotState::Defend || state == BotState::Engage) { return Normalize(player.Position() - bot.Position()); }
        if (state == BotState::Push) { return {-1.0F, 0.0F}; }
        return {};
    }
};
