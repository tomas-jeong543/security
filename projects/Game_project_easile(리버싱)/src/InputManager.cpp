#include "InputManager.h"

void InputManager::ResetEdges()
{
    previousBasic_ = previousUltimate_ = previousPause_ = previousEnter_ = previousRestart_ = false;
    previous1_ = previous2_ = previous3_ = false;
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
    if (client.right > 0 && client.bottom > 0)
    {
        command.mouseWorldPosition = {1280.0F * cursor.x / client.right, 720.0F * cursor.y / client.bottom};
    }
    command.basic = pressed(BasicCommand(down(VK_LBUTTON), down('K')), previousBasic_);
    command.ultimate = pressed(down('J'), previousUltimate_);
    command.togglePause = pressed(down('P'), previousPause_);
    command.start = pressed(down(VK_RETURN), previousEnter_);
    command.restart = pressed(down('R'), previousRestart_);
    if (pressed(down('1'), previous1_)) { command.upgrade = UpgradeChoice::Health; }
    if (pressed(down('2'), previous2_)) { command.upgrade = UpgradeChoice::Attack; }
    if (pressed(down('3'), previous3_)) { command.upgrade = UpgradeChoice::MoveSpeed; }
    return command;
}
