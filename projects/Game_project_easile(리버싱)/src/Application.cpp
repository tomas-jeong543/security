#include "Application.h"
#include "Logger.h"
#include "Camera.h"
#include "IntegrityGuard.h" // PATCH-02 추가
#include <algorithm>

bool Application::CreateMainWindow(HINSTANCE instance)
{
    constexpr wchar_t className[] = L"EasileGameClientWindow";
    WNDCLASS windowClass{};
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    if (!RegisterClass(&windowClass)) { return false; }

    window_ = CreateWindowEx(0, className, L"EASILE GameClient", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720, nullptr, nullptr, instance, this);
    return window_ != nullptr;
}

int Application::Run(HINSTANCE instance, bool smokeGui)
{
    if (!CreateMainWindow(instance)) { Logger{}.Write(0.0, "APP", "WINDOW_FAILED", "Application", ""); return 1; }
    Logger{}.Write(0.0, "APP", "WINDOW_CREATED", "Application", "");
    if (!renderer_.Initialize(window_)) { Logger{}.Write(0.0, "APP", "DX11_FAILED", "Renderer", ""); return 1; }
    Logger{}.Write(0.0, "APP", "DX11_READY", "Renderer", "");
    if (!hudRenderer_.Initialize(window_)) { Logger{}.Write(0.0, "APP", "HUD_FAILED", "HUDRenderer", ""); return 1; }
    Logger{}.Write(0.0, "APP", "HUD_READY", "HUDRenderer", smokeGui ? "smoke=1" : "smoke=0");
    ShowWindow(window_, SW_SHOWDEFAULT);
    gameTime_.Reset();

    bool dllBaselineReady = false;
    ULONGLONG lastDllCheck = GetTickCount64();

    if (smokeGui)
    {
        gameWorld_.Restart();
        for (int step = 0; step < 385 && gameWorld_.State() == GameState::Playing; ++step)
        {
            PlayerCommand sample;
            sample.mouseWorldPosition = { 550.0F, 360.0F };
            sample.basic = sample.ultimate = step == 380;
            gameWorld_.Update(sample, 0.01F);
        }
    }

    MSG message{};
    float accumulator = 0.0F;
    int renderedFrames = 0;
    while (message.message != WM_QUIT)
    {
        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
        if (message.message == WM_QUIT) { break; }

        // PATCH-02: 약 1초 간격으로 새로 로드된 DLL 확인 + 이미지 랜더링까지 된 후에 한다.
        const ULONGLONG now = GetTickCount64();
        if (dllBaselineReady  &&  now - lastDllCheck >= 1000)
        {
            IntegrityGuard::DetectNewModules();
            lastDllCheck = now;
        }

        gameTime_.Tick();
        PlayerCommand command = inputManager_.Read(window_);
        const Vec2 screenCursor = command.mouseWorldPosition;
        command.mouseWorldPosition = Camera::ScreenToWorld(screenCursor, gameWorld_.CreateSnapshot().cameraOffset);
        if (GetForegroundWindow() != window_) { pendingCommand_ = {}; }
        pendingCommand_.basic |= command.basic;
        pendingCommand_.clickBasic |= command.clickBasic;
        pendingCommand_.toggleAim |= command.toggleAim;
        if (command.clickBasic) { pendingCommand_.mouseWorldPosition = command.mouseWorldPosition; }
        pendingCommand_.ultimate |= command.ultimate;
        if (command.upgrade != UpgradeChoice::None) { pendingCommand_.upgrade = command.upgrade; }
        if (gameWorld_.State() == GameState::Menu || gameWorld_.State() == GameState::Paused ||
            gameWorld_.State() == GameState::Result || command.togglePause)
        {
            gameWorld_.Update(command, 0.0F);
            accumulator = 0.0F;
            pendingCommand_ = {};
        }
        else
        {
            accumulator += std::min(gameTime_.DeltaSeconds(), 0.25F);
            bool firstStep = true;
            while (accumulator >= 0.01F)
            {
                PlayerCommand step = command;
                step.basic = pendingCommand_.basic;
                step.clickBasic = pendingCommand_.clickBasic;
                step.toggleAim = pendingCommand_.toggleAim;
                if (step.clickBasic) { step.mouseWorldPosition = pendingCommand_.mouseWorldPosition; }
                step.ultimate = pendingCommand_.ultimate;
                step.upgrade = pendingCommand_.upgrade;
                if (!firstStep)
                {
                    step.basic = step.clickBasic = step.ultimate = step.toggleAim = false;
                    step.upgrade = UpgradeChoice::None;
                }
                gameWorld_.Update(step, 0.01F);
                accumulator -= 0.01F;
                firstStep = false;
                pendingCommand_ = {};
            }
        }
        HUDViewModel hud = gameWorld_.CreateHud();
        hud.cursorWorldPosition = Camera::ScreenToWorld(screenCursor, gameWorld_.CreateSnapshot().cameraOffset);
        hud.cursorVisible = GetForegroundWindow() == window_ &&
            screenCursor.x >= 0.0F && screenCursor.x < Camera::width &&
            screenCursor.y >= 0.0F && screenCursor.y < Camera::height;
        
        renderer_.Render(gameWorld_.CreateSnapshot(), hud, smokeGui && renderedFrames == 4);
        hudRenderer_.Render(hud);
        if (smokeGui && ++renderedFrames == 5)
        {
            Logger{}.Write(0.0, "APP", "FRAMES_RENDERED", "Renderer", "count=5");
            PostMessage(window_, WM_CLOSE, 0, 0);
        }

        // 최초 렌더링 완료 후 DLL Baseline 생성
        if (!dllBaselineReady)
        {
            if (IntegrityGuard::InitializeDllBaseline())
            {
                dllBaselineReady = true;
                lastDllCheck = GetTickCount64();

                OutputDebugStringA(
                    "[DLLGuard] Baseline created after first render\n"
                );
            }
            else
            {
                OutputDebugStringA(
                    "[DLLGuard] Baseline initialization failed\n"
                );
            }
        }

    }
    Logger{}.Write(0.0, "APP", "EXIT", "Application", "");
    return static_cast<int>(message.wParam);
}

LRESULT CALLBACK Application::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE)
    {
        const auto create = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* application = reinterpret_cast<Application*>(GetWindowLongPtr(window, GWLP_USERDATA));

    switch (message)
    {
    case WM_SIZE:
        if (application != nullptr) { application->renderer_.Resize(LOWORD(lParam), HIWORD(lParam)); }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(window, message, wParam, lParam);
    }
}
