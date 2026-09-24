#pragma once
#include "InputManager.h"
class PlayerController final
{
public:
    PlayerCommand BuildCommand(const PlayerCommand& input, const Hero& player) const
    {
        PlayerCommand command = input;
        if (!player.Alive()) { command.move = {}; command.basic = false; command.ultimate = false; }
        command.move = Normalize(command.move);
        return command;
    }
    Vec2 AimDirection(const PlayerCommand& command, const Hero& player) const
    {
        return Normalize(command.mouseWorldPosition - player.Position());
    }
};
