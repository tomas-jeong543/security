#include "InputManager.h"

void InputManager::ResetEdges()
{
    previousClick_ = previousK_ = previousUltimate_ = previousTab_ = previousPause_ = previousEnter_ = previousRestart_ = false;
    previous1_ = previous2_ = previous3_ = false;
}

PlayerCommand InputManager::ReadAttackEdges(bool click, bool keyK, bool keyJ, bool tab)
{
    const auto pressed = [](bool current, bool& previous)
    {
        const bool result = current && !previous;
        previous = current;
        return result;
    };
    PlayerCommand command;
    command.clickBasic = pressed(click, previousClick_);
    const bool keyBasic = pressed(keyK, previousK_);
    command.basic = command.clickBasic || keyBasic;
    command.ultimate = pressed(keyJ, previousUltimate_);
    command.toggleAim = pressed(tab, previousTab_);
    return command;
}

PlayerCommand InputManager::Read(HWND window)
{
    PlayerCommand command;
    if (GetForegroundWindow() != window) { ResetEdges(); return command; }

    const auto down = [](int key) { return (GetAsyncKeyState(key) & 0x8000) != 0; };
    const auto pressed = [](bool current, bool& previous) { const bool result = current && !previous; previous = current; return result; };
    command.move = MoveVector(down('A') || down(VK_LEFT), down('D') || down(VK_RIGHT),
                              down('W') || down(VK_UP), down('S') || down(VK_DOWN));

    POINT cursor{};
    GetCursorPos(&cursor);
    ScreenToClient(window, &cursor);
    RECT client{};
    GetClientRect(window, &client);
    command.mouseWorldPosition = MouseWorldPosition(cursor, client);
    const PlayerCommand attacks = ReadAttackEdges(down(VK_LBUTTON), down('K'), down('J'), down(VK_TAB));
    command.clickBasic = attacks.clickBasic;
    command.basic = attacks.basic;
    command.ultimate = attacks.ultimate;
    command.toggleAim = attacks.toggleAim;
    command.togglePause = pressed(down('P'), previousPause_);
    command.start = pressed(down(VK_RETURN), previousEnter_);
    command.restart = pressed(down('R'), previousRestart_);
    if (pressed(down('1'), previous1_)) { command.upgrade = UpgradeChoice::Health; }
    if (pressed(down('2'), previous2_)) { command.upgrade = UpgradeChoice::Attack; }
    if (pressed(down('3'), previous3_)) { command.upgrade = UpgradeChoice::MoveSpeed; }
    return command;
}
