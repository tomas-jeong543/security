#include "Renderer.h"

#include <d3dcompiler.h>
#include <cstring>
#include <algorithm>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

bool Renderer::Initialize(HWND window)
{
    RECT clientRect{};
    GetClientRect(window, &clientRect);
    width_ = static_cast<UINT>(clientRect.right - clientRect.left);
    height_ = static_cast<UINT>(clientRect.bottom - clientRect.top);

    DXGI_SWAP_CHAIN_DESC swapChainDescription{};
    swapChainDescription.BufferCount = 1;
    swapChainDescription.BufferDesc.Width = width_;
    swapChainDescription.BufferDesc.Height = height_;
    swapChainDescription.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDescription.OutputWindow = window;
    swapChainDescription.SampleDesc.Count = 1;
    swapChainDescription.Windowed = TRUE;

    if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &swapChainDescription, &swapChain_, &device_, nullptr, &context_))) { return false; }

    static constexpr char shaderSource[] =
        "struct VSIn { float2 position : POSITION; float4 color : COLOR; };"
        "struct VSOut { float4 position : SV_POSITION; float4 color : COLOR; };"
        "VSOut VSMain(VSIn input) { VSOut output; output.position=float4(input.position,0,1); output.color=input.color; return output; }"
        "float4 PSMain(VSOut input) : SV_TARGET { return input.color; }";
    ComPtr<ID3DBlob> vertexBlob;
    ComPtr<ID3DBlob> pixelBlob;
    if (FAILED(D3DCompile(shaderSource, sizeof(shaderSource), nullptr, nullptr, nullptr, "VSMain", "vs_4_0", 0, 0, &vertexBlob, nullptr)) ||
        FAILED(D3DCompile(shaderSource, sizeof(shaderSource), nullptr, nullptr, nullptr, "PSMain", "ps_4_0", 0, 0, &pixelBlob, nullptr))) { return false; }
    if (FAILED(device_->CreateVertexShader(vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), nullptr, &vertexShader_)) ||
        FAILED(device_->CreatePixelShader(pixelBlob->GetBufferPointer(), pixelBlob->GetBufferSize(), nullptr, &pixelShader_))) { return false; }

    const D3D11_INPUT_ELEMENT_DESC layout[] = {{"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0}, {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    if (FAILED(device_->CreateInputLayout(layout, ARRAYSIZE(layout), vertexBlob->GetBufferPointer(), vertexBlob->GetBufferSize(), &inputLayout_))) { return false; }

    D3D11_BUFFER_DESC bufferDescription{};
    bufferDescription.ByteWidth = sizeof(Vertex) * 6;
    bufferDescription.Usage = D3D11_USAGE_DYNAMIC;
    bufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bufferDescription.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device_->CreateBuffer(&bufferDescription, nullptr, &vertexBuffer_))) { return false; }
    Resize(width_, height_);
    return renderTargetView_ != nullptr;
}

void Renderer::Resize(UINT width, UINT height)
{
    if (width == 0 || height == 0 || !swapChain_) { return; }
    width_ = width;
    height_ = height;
    renderTargetView_.Reset();
    context_->OMSetRenderTargets(0, nullptr, nullptr);
    if (FAILED(swapChain_->ResizeBuffers(0, width_, height_, DXGI_FORMAT_UNKNOWN, 0))) { return; }
    ComPtr<ID3D11Texture2D> backBuffer;
    if (SUCCEEDED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) { device_->CreateRenderTargetView(backBuffer.Get(), nullptr, &renderTargetView_); }
}

void Renderer::Render(const GameWorldSnapshot& snapshot, const HUDViewModel& hud, bool capture) const
{
    (void)hud;
    if (!renderTargetView_) { return; }
    const float clearColor[] = {0.08F, 0.10F, 0.14F, 1.0F};
    context_->OMSetRenderTargets(1, renderTargetView_.GetAddressOf(), nullptr);
    context_->ClearRenderTargetView(renderTargetView_.Get(), clearColor);
    const D3D11_VIEWPORT viewport{0.0F, 0.0F, static_cast<float>(width_), static_cast<float>(height_), 0.0F, 1.0F};
    context_->RSSetViewports(1, &viewport);
    context_->IASetInputLayout(inputLayout_.Get());
    context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const UINT stride = sizeof(Vertex); const UINT offset = 0;
    context_->IASetVertexBuffers(0, 1, vertexBuffer_.GetAddressOf(), &stride, &offset);
    context_->VSSetShader(vertexShader_.Get(), nullptr, 0);
    context_->PSSetShader(pixelShader_.Get(), nullptr, 0);
    DrawQuad({320, 360}, {640, 720}, {0.075F, 0.15F, 0.22F});
    DrawQuad({960, 360}, {640, 720}, {0.22F, 0.10F, 0.15F});
    DrawQuad({640, 360}, {1280, 190}, {0.22F, 0.25F, 0.28F});
    DrawQuad({640, 360}, {1280, 152}, {0.16F, 0.19F, 0.23F});
    DrawQuad({640, 360}, {1280, 4}, {0.37F, 0.40F, 0.42F});
    for (int index = 0; index < 14; ++index)
    {
        DrawQuad({55.0F + index * 90.0F, 281}, {42, 3}, {0.37F, 0.40F, 0.42F});
        DrawQuad({55.0F + index * 90.0F, 439}, {42, 3}, {0.37F, 0.40F, 0.42F});
    }
    for (const EntitySnapshot& entity : snapshot.entities)
    {
        if (entity.type == EntityType::Brush && ShouldDraw(entity)) { DrawEntity(entity); }
    }
    for (const EntitySnapshot& entity : snapshot.entities)
    {
        if (entity.type != EntityType::Brush && ShouldDraw(entity)) { DrawEntity(entity); }
    }
    if (capture) { CaptureFrame(); }
    swapChain_->Present(1, 0);
}

void Renderer::CaptureFrame() const
{
    ComPtr<ID3D11Texture2D> backBuffer;
    if (FAILED(swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer)))) { return; }
    D3D11_TEXTURE2D_DESC description{};
    backBuffer->GetDesc(&description);
    description.Usage = D3D11_USAGE_STAGING;
    description.BindFlags = 0;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    description.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> staging;
    if (FAILED(device_->CreateTexture2D(&description, nullptr, &staging))) { return; }
    context_->CopyResource(staging.Get(), backBuffer.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) { return; }
    const UINT rowBytes = width_ * 4;
    std::vector<unsigned char> pixels(static_cast<size_t>(rowBytes) * height_);
    for (UINT row = 0; row < height_; ++row)
    {
        const auto* source = static_cast<const unsigned char*>(mapped.pData) + row * mapped.RowPitch;
        auto* destination = pixels.data() + (height_ - row - 1) * rowBytes;
        for (UINT column = 0; column < width_; ++column)
        {
            destination[column * 4] = source[column * 4 + 2];
            destination[column * 4 + 1] = source[column * 4 + 1];
            destination[column * 4 + 2] = source[column * 4];
            destination[column * 4 + 3] = 0;
        }
    }
    context_->Unmap(staging.Get(), 0);
    BITMAPFILEHEADER fileHeader{};
    fileHeader.bfType = 0x4D42;
    fileHeader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    fileHeader.bfSize = fileHeader.bfOffBits + static_cast<DWORD>(pixels.size());
    BITMAPINFOHEADER imageHeader{};
    imageHeader.biSize = sizeof(BITMAPINFOHEADER);
    imageHeader.biWidth = static_cast<LONG>(width_);
    imageHeader.biHeight = static_cast<LONG>(height_);
    imageHeader.biPlanes = 1;
    imageHeader.biBitCount = 32;
    imageHeader.biSizeImage = static_cast<DWORD>(pixels.size());
    wchar_t executable[MAX_PATH]{};
    GetModuleFileNameW(nullptr, executable, MAX_PATH);
    std::wstring path(executable);
    path = path.substr(0, path.find_last_of(L"\\/" ) + 1) + L"visual-smoke.bmp";
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { return; }
    DWORD written = 0;
    WriteFile(file, &fileHeader, sizeof(fileHeader), &written, nullptr);
    WriteFile(file, &imageHeader, sizeof(imageHeader), &written, nullptr);
    WriteFile(file, pixels.data(), static_cast<DWORD>(pixels.size()), &written, nullptr);
    CloseHandle(file);
}

void Renderer::DrawQuad(Vec2 center, Vec2 size, Color color) const
{
    const float left = center.x - size.x * 0.5F;
    const float right = center.x + size.x * 0.5F;
    const float top = center.y - size.y * 0.5F;
    const float bottom = center.y + size.y * 0.5F;
    const auto x = [](float pixel) { return pixel / 1280.0F * 2.0F - 1.0F; };
    const auto y = [](float pixel) { return 1.0F - pixel / 720.0F * 2.0F; };
    const Vertex vertices[] = {{x(left), y(top), color.r, color.g, color.b, 1}, {x(right), y(top), color.r, color.g, color.b, 1}, {x(right), y(bottom), color.r, color.g, color.b, 1}, {x(left), y(top), color.r, color.g, color.b, 1}, {x(right), y(bottom), color.r, color.g, color.b, 1}, {x(left), y(bottom), color.r, color.g, color.b, 1}};
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(context_->Map(vertexBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) { std::memcpy(mapped.pData, vertices, sizeof(vertices)); context_->Unmap(vertexBuffer_.Get(), 0); context_->Draw(6, 0); }
}

void Renderer::DrawDiamond(Vec2 center, Vec2 size, Color color) const
{
    const auto x = [](float pixel) { return pixel / 1280.0F * 2.0F - 1.0F; };
    const auto y = [](float pixel) { return 1.0F - pixel / 720.0F * 2.0F; };
    const float left = center.x - size.x * 0.5F, right = center.x + size.x * 0.5F;
    const float top = center.y - size.y * 0.5F, bottom = center.y + size.y * 0.5F;
    const Vertex vertices[] = {
        {x(center.x), y(top), color.r, color.g, color.b, 1}, {x(right), y(center.y), color.r, color.g, color.b, 1}, {x(center.x), y(bottom), color.r, color.g, color.b, 1},
        {x(center.x), y(top), color.r, color.g, color.b, 1}, {x(center.x), y(bottom), color.r, color.g, color.b, 1}, {x(left), y(center.y), color.r, color.g, color.b, 1}};
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(context_->Map(vertexBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) { std::memcpy(mapped.pData, vertices, sizeof(vertices)); context_->Unmap(vertexBuffer_.Get(), 0); context_->Draw(6, 0); }
}

void Renderer::DrawHealth(const EntitySnapshot& entity, float width, float top) const
{
    const float ratio = std::clamp(entity.health, 0.0F, 1.0F);
    DrawQuad({entity.position.x, top}, {width + 4, 8}, {0.03F, 0.05F, 0.08F});
    if (ratio > 0.0F)
    {
        DrawQuad({entity.position.x - width * (1.0F - ratio) * 0.5F, top}, {width * ratio, 4},
                 entity.team == Team::Blue ? Color{0.19F, 0.80F, 0.91F} : Color{1.0F, 0.38F, 0.36F});
    }
}

void Renderer::DrawEntity(const EntitySnapshot& entity) const
{
    const Vec2 position = entity.position;
    const Color outline{0.02F, 0.04F, 0.07F};
    const Color faction = entity.team == Team::Blue ? Color{0.12F, 0.58F, 0.91F} : Color{0.83F, 0.20F, 0.25F};
    const Color light = entity.team == Team::Blue ? Color{0.61F, 0.92F, 1.0F} : Color{1.0F, 0.70F, 0.60F};
    switch (entity.type)
    {
    case EntityType::Brush:
        DrawQuad(position, entity.size, {0.05F, 0.22F, 0.18F});
        DrawQuad(position, {entity.size.x - 7, entity.size.y - 7}, {0.09F, 0.32F, 0.23F});
        for (int row = -1; row <= 1; ++row)
        {
            for (int column = -3; column <= 3; ++column)
            {
                DrawDiamond({position.x + column * 30.0F + (row % 2) * 12.0F, position.y + row * 45.0F},
                            {17, 24}, {0.14F, 0.43F, 0.27F});
            }
        }
        break;
    case EntityType::PlayerHero:
        DrawDiamond(position, {52, 52}, outline);
        DrawDiamond(position, {44, 44}, faction);
        DrawDiamond(position, {20, 20}, light);
        DrawDiamond(position + entity.direction * 31.0F, {12, 12}, light);
        if (entity.revealed) { DrawDiamond(position, {10, 10}, {1.0F, 0.90F, 0.35F}); }
        DrawHealth(entity, 48, position.y - 37);
        break;
    case EntityType::BotHero:
        DrawQuad(position, {48, 48}, outline);
        DrawQuad(position, {40, 40}, faction);
        DrawQuad(position, {27, 27}, {0.42F, 0.08F, 0.13F});
        DrawQuad({position.x - 8, position.y - 4}, {7, 7}, light);
        DrawQuad({position.x + 8, position.y - 4}, {7, 7}, light);
        DrawQuad({position.x, position.y + 10}, {20, 4}, light);
        if (entity.revealed) { DrawDiamond({position.x, position.y - 23}, {10, 10}, {1.0F, 0.89F, 0.35F}); }
        DrawHealth(entity, 48, position.y - 37);
        break;
    case EntityType::Minion:
        DrawDiamond(position, {23, 23}, outline);
        DrawDiamond(position, {17, 17}, faction);
        DrawDiamond(position, {7, 7}, light);
        DrawHealth(entity, 20, position.y - 18);
        break;
    case EntityType::Tower:
        DrawQuad({position.x, position.y + 19}, {50, 12}, outline);
        DrawQuad(position, {34, 62}, outline);
        DrawQuad(position, {26, 54}, faction);
        DrawQuad({position.x, position.y - 26}, {48, 14}, light);
        DrawDiamond({position.x, position.y - 4}, {16, 20}, light);
        DrawHealth(entity, 52, position.y - 46);
        break;
    case EntityType::Base:
        DrawDiamond(position, {90, 90}, outline);
        DrawDiamond(position, {78, 78}, faction);
        DrawDiamond(position, {55, 55}, outline);
        DrawDiamond(position, {42, 42}, light);
        DrawDiamond(position, {21, 21}, faction);
        DrawHealth(entity, 80, position.y - 58);
        break;
    case EntityType::Projectile:
    {
        const Vec2 direction = Length(entity.direction) > 0.0F ? entity.direction : Vec2{1, 0};
        const bool horizontal = std::abs(direction.x) >= std::abs(direction.y);
        const Vec2 trail = position - direction * (entity.ultimate ? 18.0F : 13.0F);
        DrawQuad(trail, horizontal ? Vec2{entity.ultimate ? 32.0F : 23.0F, entity.ultimate ? 15.0F : 7.0F}
                                   : Vec2{entity.ultimate ? 15.0F : 7.0F, entity.ultimate ? 32.0F : 23.0F},
                 entity.ultimate ? faction : Color{0.86F, 0.62F, 0.24F});
        if (entity.ultimate)
        {
            DrawDiamond(position, {36, 36}, outline);
            DrawDiamond(position, {29, 29}, faction);
            DrawDiamond(position, {14, 14}, {1.0F, 0.94F, 0.59F});
        }
        else
        {
            DrawDiamond(position, {18, 18}, {1.0F, 0.89F, 0.43F});
            DrawDiamond(position, {8, 8}, {1.0F, 1.0F, 0.87F});
        }
        break;
    }
    }
}
