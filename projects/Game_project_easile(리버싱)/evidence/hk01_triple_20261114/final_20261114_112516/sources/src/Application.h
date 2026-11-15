#pragma once

#include "GameTime.h"
#include "GameWorld.h"
#include "InputManager.h"
#include "HUDRenderer.h"
#include "Renderer.h"

#include <windows.h>

class Application
{
public:
    int Run(HINSTANCE instance, bool smokeGui = false);

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    bool CreateMainWindow(HINSTANCE instance);

    HWND window_ = nullptr;
    GameTime gameTime_;
    GameWorld gameWorld_;
    InputManager inputManager_;
    Renderer renderer_;
    HUDRenderer hudRenderer_;
    PlayerCommand pendingCommand_;
};
