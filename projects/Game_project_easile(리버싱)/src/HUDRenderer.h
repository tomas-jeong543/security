#pragma once
#include "Snapshot.h"
#include <windows.h>

class HUDRenderer final
{
public:
    bool Initialize(HWND parent);
    void Render(const HUDViewModel& viewModel);

private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    void Paint(HWND window, HDC canvas, bool overlay) const;
    HWND parent_ = nullptr;
    HWND panel_ = nullptr;
    HWND overlay_ = nullptr;
    HUDViewModel view_;
};
