#include "HUDRenderer.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace
{
constexpr wchar_t className[] = L"EasileHudPanel";
void Fill(HDC canvas, RECT area, COLORREF color)
{
    HBRUSH brush = CreateSolidBrush(color);
    FillRect(canvas, &area, brush);
    DeleteObject(brush);
}
void Bar(HDC canvas, int left, int top, int width, float ratio, COLORREF color)
{
    Fill(canvas, {left, top, left + width, top + 9}, RGB(49, 60, 69));
    const int filled = static_cast<int>(width * std::clamp(ratio, 0.0F, 1.0F));
    if (filled > 0) { Fill(canvas, {left, top, left + filled, top + 9}, color); }
}
void Label(HDC canvas, const std::wstring& value, RECT area, COLORREF color, HFONT font, UINT format = DT_LEFT)
{
    SelectObject(canvas, font);
    SetTextColor(canvas, color);
    DrawTextW(canvas, value.c_str(), -1, &area, DT_SINGLELINE | DT_VCENTER | format);
}
std::wstring Cooldown(float seconds)
{
    if (seconds <= 0.0001F) { return L"READY"; }
    std::wostringstream result;
    result << std::fixed << std::setprecision(1) << seconds << L"s";
    return result.str();
}
}

bool HUDRenderer::Initialize(HWND parent)
{
    parent_ = parent;
    WNDCLASS windowClass{};
    windowClass.hInstance = GetModuleHandle(nullptr);
    windowClass.lpszClassName = className;
    windowClass.lpfnWndProc = WindowProc;
    if (!RegisterClass(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) { return false; }
    panel_ = CreateWindowEx(0, className, L"", WS_CHILD | WS_VISIBLE,
                            16, 16, 374, 157, parent, nullptr, windowClass.hInstance, this);
    overlay_ = CreateWindowEx(0, className, L"", WS_CHILD,
                              0, 0, 390, 166, parent, nullptr, windowClass.hInstance, this);
    return panel_ != nullptr && overlay_ != nullptr;
}

void HUDRenderer::Render(const HUDViewModel& viewModel)
{
    if (!panel_ || !overlay_) { return; }
    view_ = viewModel;
    std::wostringstream text;
    text << L"State: " << viewModel.state << L"    HP: " << viewModel.hp << L"/" << viewModel.maxHp
         << L"    Level: " << viewModel.level << L"    XP: " << viewModel.xp << L"/" << viewModel.requiredXp
         << L"    Basic (LMB/K): " << Cooldown(viewModel.basicCd)
         << L"    Ultimate (J): " << Cooldown(viewModel.ultimateCd)
         << L"    " << viewModel.result;
    const std::wstring description = text.str();
    SetWindowTextW(panel_, description.c_str());
    SetWindowTextW(overlay_, description.c_str());
    InvalidateRect(panel_, nullptr, FALSE);
    const bool showOverlay = viewModel.state == L"Menu" || viewModel.state == L"Paused" ||
                             viewModel.state == L"Result" || viewModel.respawning;
    if (showOverlay)
    {
        RECT client{};
        GetClientRect(parent_, &client);
        SetWindowPos(overlay_, HWND_TOP, (client.right - 390) / 2, (client.bottom - 166) / 2,
                     390, 166, SWP_SHOWWINDOW | SWP_NOACTIVATE);
        InvalidateRect(overlay_, nullptr, FALSE);
    }
    else { ShowWindow(overlay_, SW_HIDE); }
}

void HUDRenderer::Paint(HWND window, HDC canvas, bool overlay) const
{
    RECT bounds{};
    GetClientRect(window, &bounds);
    Fill(canvas, bounds, RGB(13, 24, 34));
    Fill(canvas, {0, 0, bounds.right, 3}, overlay ? RGB(221, 191, 97) : RGB(66, 168, 212));
    HFONT title = CreateFontW(24, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT normal = CreateFontW(17, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HGDIOBJ previousFont = GetCurrentObject(canvas, OBJ_FONT);
    SetBkMode(canvas, TRANSPARENT);
    if (overlay)
    {
        const std::wstring heading = view_.state == L"Result" ? view_.result :
            view_.respawning && view_.state == L"Playing" ? L"RESPAWNING" :
            view_.state == L"Paused" ? L"PAUSED" : L"EASILE";
        const std::wstring action = view_.state == L"Result" ? L"Press R to restart" :
            view_.state == L"Paused" ? L"Press P to resume" :
            view_.respawning ? L"Return to base soon" : L"Press Enter to start";
        Label(canvas, heading, {18, 29, 372, 84}, RGB(242, 232, 195), title, DT_CENTER);
        Label(canvas, action, {18, 93, 372, 137}, RGB(185, 204, 214), normal, DT_CENTER);
    }
    else
    {
        Label(canvas, L"PLAYER  /  " + view_.state, {16, 8, 360, 38}, RGB(229, 239, 246), title);
        std::wostringstream health, experience;
        health << L"HP  " << std::max(0, view_.hp) << L" / " << view_.maxHp;
        experience << L"LV " << view_.level << L"  |  XP  " << view_.xp << L" / " << view_.requiredXp;
        Label(canvas, health.str(), {17, 40, 350, 62}, RGB(221, 231, 234), normal);
        Bar(canvas, 18, 65, 336, view_.maxHp > 0 ? static_cast<float>(view_.hp) / view_.maxHp : 0.0F, RGB(69, 198, 205));
        Label(canvas, experience.str(), {17, 80, 350, 102}, RGB(221, 231, 234), normal);
        Bar(canvas, 18, 105, 336, view_.requiredXp > 0 ? static_cast<float>(view_.xp) / view_.requiredXp : 0.0F, RGB(227, 175, 83));
        Label(canvas, L"BASIC  " + Cooldown(view_.basicCd), {18, 120, 184, 150},
              view_.basicCd <= 0.0001F ? RGB(114, 227, 153) : RGB(236, 196, 116), normal);
        Label(canvas, L"ULT  " + Cooldown(view_.ultimateCd), {191, 120, 355, 150},
              view_.ultimateCd <= 0.0001F ? RGB(114, 227, 153) : RGB(236, 196, 116), normal);
    }
    SelectObject(canvas, previousFont);
    DeleteObject(title);
    DeleteObject(normal);
}

LRESULT CALLBACK HUDRenderer::WindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_NCCREATE)
    {
        auto* create = reinterpret_cast<CREATESTRUCT*>(lParam);
        SetWindowLongPtr(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }
    auto* renderer = reinterpret_cast<HUDRenderer*>(GetWindowLongPtr(window, GWLP_USERDATA));
    if (message == WM_PRINTCLIENT && renderer)
    {
        renderer->Paint(window, reinterpret_cast<HDC>(wParam), window == renderer->overlay_);
        return 0;
    }
    if (message == WM_PAINT && renderer)
    {
        PAINTSTRUCT paint{};
        HDC canvas = BeginPaint(window, &paint);
        renderer->Paint(window, canvas, window == renderer->overlay_);
        EndPaint(window, &paint);
        return 0;
    }
    if (message == WM_ERASEBKGND) { return 1; }
    return DefWindowProc(window, message, wParam, lParam);
}
