#pragma once

#include "Snapshot.h"

#include <windows.h>
#include <wrl/client.h>
#include <d3d11.h>

class Renderer
{
public:
    bool Initialize(HWND window);
    void Resize(UINT width, UINT height);
    void Render(const GameWorldSnapshot& snapshot, const HUDViewModel& hud, bool capture = false) const;
    static bool ShouldDraw(const EntitySnapshot& entity) { return entity.visible && entity.alive; }

private:
    struct Vertex { float x; float y; float r; float g; float b; float a; };
    struct Color { float r, g, b; };
    void DrawQuad(Vec2 center, Vec2 size, Color color) const;
    void DrawDiamond(Vec2 center, Vec2 size, Color color) const;
    void DrawEntity(const EntitySnapshot& entity) const;
    void DrawHealth(const EntitySnapshot& entity, float width, float top) const;
    void CaptureFrame() const;

    Microsoft::WRL::ComPtr<ID3D11Device> device_;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
    Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> renderTargetView_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer_;
    UINT width_ = 1;
    UINT height_ = 1;
};
