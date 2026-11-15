#include "Application.h"
#include <string_view>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR commandLine, int)
{
    Application application;
    return application.Run(instance, std::wstring_view(commandLine).find(L"--smoke-gui") != std::wstring_view::npos);
}
