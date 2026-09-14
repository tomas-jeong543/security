// clang++ -std=c++17 game_fixed.cpp -o game.exe -ld3d11 -ld3dcompiler -ldxgi -luser32 -lgdi32
//
// [후킹 스터디용 D3D11 데모]
//  - 벽(Wall)   : DrawIndexed(6,  ...)  -> 회색 사각형, z=0
//  - 적(Enemy)  : DrawIndexed(36, ...)  -> 빨간 큐브, z=3(벽 뒤)
//  깊이 테스트(LESS) 때문에 적은 벽에 가려 안 보인다.
//  실습 목표: DrawIndexed 후킹 -> IndexCount==36 이면 깊이 끄고 그리기 = 월핵
//
//  [조작]
//   마우스       : 시점 회전 (FPS 방식, 커서는 창에 잠김)
//   W/A/S/D     : 앞/왼/뒤/오른 이동
//   Q / E       : 아래 / 위 이동
//   방향키       : 시점 회전 (마우스 대체)
//   Shift       : 빠르게 이동
//   TAB         : 마우스 잠금 해제 / 다시 잠금 (창 밖으로 나갈 때)
//   F1          : 깊이 테스트 토글 (후킹 결과 미리보기)
//   R           : 카메라 초기 위치로 리셋
//   ESC         : 종료

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <cstring>
#include <cstdio>
#include <cmath>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;

// ---------------- 전역 ----------------
static const int W = 800, H = 600;

static ID3D11Device*            g_dev      = nullptr;
static ID3D11DeviceContext*     g_ctx      = nullptr;
static IDXGISwapChain*          g_swap     = nullptr;
static ID3D11RenderTargetView*  g_rtv      = nullptr;
static ID3D11DepthStencilView*  g_dsv      = nullptr;
static ID3D11VertexShader*      g_vs       = nullptr;
static ID3D11PixelShader*       g_ps       = nullptr;
static ID3D11InputLayout*       g_layout   = nullptr;
static ID3D11Buffer*            g_cbuf     = nullptr;
static ID3D11Buffer*            g_wallVB   = nullptr;
static ID3D11Buffer*            g_wallIB   = nullptr;
static ID3D11Buffer*            g_cubeVB   = nullptr;
static ID3D11Buffer*            g_cubeIB   = nullptr;
static ID3D11RasterizerState*   g_rs       = nullptr;
static ID3D11DepthStencilState* g_dssOn    = nullptr;
static ID3D11DepthStencilState* g_dssOff   = nullptr;

static bool  g_depthOn = true;
static HWND  g_hwnd    = nullptr;

// ---- 카메라 상태 ----
static XMFLOAT3 g_camPos   = { 0.0f, 0.0f, -5.0f };
static float    g_camYaw   = 0.0f;   // 좌우 (라디안)
static float    g_camPitch = 0.0f;   // 상하 (라디안)

// ---- 마우스 룩 (FPS 방식: 커서를 창 중앙에 고정하고 delta만 사용) ----
static bool  g_mouseCaptured = false;
static bool  g_cursorHidden  = false;
static float g_mouseSens     = 0.0022f;   // 라디안 / 픽셀

struct Vertex { float x, y, z; };
struct CB { XMFLOAT4X4 mvp; XMFLOAT4 color; };

// ---------------- HLSL ----------------
static const char* g_shader = R"(
cbuffer CB : register(b0) { matrix mvp; float4 color; };
float4 VSMain(float3 pos : POSITION) : SV_POSITION { return mul(float4(pos,1), mvp); }
float4 PSMain(float4 p : SV_POSITION) : SV_TARGET { return color; }
)";

// ---------------- 지오메트리 ----------------
static Vertex g_wallV[] = { {-1,-1,0}, {1,-1,0}, {1,1,0}, {-1,1,0} };
static UINT   g_wallI[] = { 0,1,2, 0,2,3 };                          // 6개

static Vertex g_cubeV[] = {
    {-.5f,-.5f,-.5f}, {.5f,-.5f,-.5f}, {.5f,.5f,-.5f}, {-.5f,.5f,-.5f},
    {-.5f,-.5f, .5f}, {.5f,-.5f, .5f}, {.5f,.5f, .5f}, {-.5f,.5f, .5f},
};
static UINT g_cubeI[] = {                                            // 36개
    0,1,2, 0,2,3,  4,6,5, 4,7,6,  4,5,1, 4,1,0,
    3,2,6, 3,6,7,  1,5,6, 1,6,2,  4,0,3, 4,3,7,
};

// ---------------- 유틸 ----------------
static bool Fail(HRESULT hr, const char* what) {
    if (SUCCEEDED(hr)) return false;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s/nHRESULT = 0x%08lX", what, (unsigned long)hr);
    MessageBoxA(nullptr, buf, "D3D11 Error", MB_OK | MB_ICONERROR);
    return true;
}

static ID3D11Buffer* MakeBuffer(const void* data, UINT size, UINT bind, const char* name) {
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = size;
    bd.Usage     = D3D11_USAGE_DEFAULT;
    bd.BindFlags = bind;
    D3D11_SUBRESOURCE_DATA sd = {};
    sd.pSysMem = data;
    ID3D11Buffer* b = nullptr;
    if (Fail(g_dev->CreateBuffer(&bd, &sd, &b), name)) return nullptr;
    return b;
}

static bool CompileShader(const char* entry, const char* target, ID3DBlob** out) {
    ID3DBlob* err = nullptr;
    HRESULT hr = D3DCompile(g_shader, strlen(g_shader), nullptr, nullptr, nullptr,
                            entry, target, D3DCOMPILE_ENABLE_STRICTNESS, 0, out, &err);
    if (FAILED(hr)) {
        MessageBoxA(nullptr, err ? (const char*)err->GetBufferPointer() : "D3DCompile failed",
                    "Shader Compile Error", MB_OK | MB_ICONERROR);
        if (err) err->Release();
        return false;
    }
    if (err) err->Release();
    return true;
}

// ---------------- 초기화 ----------------
static bool InitD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount        = 1;
    scd.BufferDesc.Width   = W;
    scd.BufferDesc.Height  = H;
    scd.BufferDesc.Format  = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferUsage        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow       = hwnd;
    scd.SampleDesc.Count   = 1;
    scd.Windowed           = TRUE;

    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };

    UINT flags = D3D11_CREATE_DEVICE_DEBUG;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
        &scd, &g_swap, &g_dev, nullptr, &g_ctx);
    if (FAILED(hr)) {
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            levels, ARRAYSIZE(levels), D3D11_SDK_VERSION,
            &scd, &g_swap, &g_dev, nullptr, &g_ctx);
    }
    if (Fail(hr, "D3D11CreateDeviceAndSwapChain")) return false;

    ID3D11Texture2D* back = nullptr;
    if (Fail(g_swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back), "GetBuffer")) return false;
    hr = g_dev->CreateRenderTargetView(back, nullptr, &g_rtv);
    back->Release();
    if (Fail(hr, "CreateRenderTargetView")) return false;

    D3D11_TEXTURE2D_DESC dd = {};
    dd.Width            = W;
    dd.Height           = H;
    dd.MipLevels        = 1;
    dd.ArraySize        = 1;
    dd.Format           = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.SampleDesc.Count = 1;
    dd.Usage            = D3D11_USAGE_DEFAULT;
    dd.BindFlags        = D3D11_BIND_DEPTH_STENCIL;
    ID3D11Texture2D* depth = nullptr;
    if (Fail(g_dev->CreateTexture2D(&dd, nullptr, &depth), "CreateTexture2D(depth)")) return false;
    hr = g_dev->CreateDepthStencilView(depth, nullptr, &g_dsv);
    depth->Release();
    if (Fail(hr, "CreateDepthStencilView")) return false;

    g_ctx->OMSetRenderTargets(1, &g_rtv, g_dsv);

    D3D11_DEPTH_STENCIL_DESC ds = {};
    ds.DepthEnable    = TRUE;
    ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    ds.DepthFunc      = D3D11_COMPARISON_LESS;
    if (Fail(g_dev->CreateDepthStencilState(&ds, &g_dssOn), "DepthStencilState(on)")) return false;

    ds.DepthEnable    = FALSE;
    ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    if (Fail(g_dev->CreateDepthStencilState(&ds, &g_dssOff), "DepthStencilState(off)")) return false;

    g_ctx->OMSetDepthStencilState(g_dssOn, 0);

    D3D11_VIEWPORT vp = { 0.0f, 0.0f, (float)W, (float)H, 0.0f, 1.0f };
    g_ctx->RSSetViewports(1, &vp);

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode        = D3D11_FILL_SOLID;
    rd.CullMode        = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    if (Fail(g_dev->CreateRasterizerState(&rd, &g_rs), "CreateRasterizerState")) return false;
    g_ctx->RSSetState(g_rs);

    ID3DBlob* vsb = nullptr;
    ID3DBlob* psb = nullptr;
    if (!CompileShader("VSMain", "vs_5_0", &vsb)) return false;
    if (!CompileShader("PSMain", "ps_5_0", &psb)) { vsb->Release(); return false; }

    hr = g_dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &g_vs);
    if (Fail(hr, "CreateVertexShader")) { vsb->Release(); psb->Release(); return false; }
    hr = g_dev->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &g_ps);
    if (Fail(hr, "CreatePixelShader")) { vsb->Release(); psb->Release(); return false; }

    D3D11_INPUT_ELEMENT_DESC il[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    hr = g_dev->CreateInputLayout(il, 1, vsb->GetBufferPointer(), vsb->GetBufferSize(), &g_layout);
    vsb->Release();
    psb->Release();
    if (Fail(hr, "CreateInputLayout")) return false;

    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth      = sizeof(CB);
    cbd.Usage          = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (Fail(g_dev->CreateBuffer(&cbd, nullptr, &g_cbuf), "CreateBuffer(cb)")) return false;

    g_wallVB = MakeBuffer(g_wallV, sizeof(g_wallV), D3D11_BIND_VERTEX_BUFFER, "wall VB");
    g_wallIB = MakeBuffer(g_wallI, sizeof(g_wallI), D3D11_BIND_INDEX_BUFFER,  "wall IB");
    g_cubeVB = MakeBuffer(g_cubeV, sizeof(g_cubeV), D3D11_BIND_VERTEX_BUFFER, "cube VB");
    g_cubeIB = MakeBuffer(g_cubeI, sizeof(g_cubeI), D3D11_BIND_INDEX_BUFFER,  "cube IB");
    if (!g_wallVB || !g_wallIB || !g_cubeVB || !g_cubeIB) return false;

    return true;
}

// ---------------- 카메라 ----------------
static XMVECTOR CamForward() {
    float cp = cosf(g_camPitch), sp = sinf(g_camPitch);
    float cy = cosf(g_camYaw),   sy = sinf(g_camYaw);
    // 왼손 좌표계: yaw=0 일 때 +Z 방향
    return XMVector3Normalize(XMVectorSet(sy * cp, sp, cy * cp, 0.0f));
}

static XMVECTOR CamRight() {
    // 월드 Up(0,1,0) x Forward  -> 왼손계에서의 오른쪽
    return XMVector3Normalize(XMVector3Cross(XMVectorSet(0, 1, 0, 0), CamForward()));
}

// 창 클라이언트 영역의 중앙(화면 좌표)
static POINT ClientCenter() {
    RECT rc;
    GetClientRect(g_hwnd, &rc);
    POINT c = { (rc.right - rc.left) / 2, (rc.bottom - rc.top) / 2 };
    ClientToScreen(g_hwnd, &c);
    return c;
}

// 마우스 잠금 켜기/끄기
static void SetMouseCapture(bool on) {
    if (on == g_mouseCaptured) return;
    g_mouseCaptured = on;

    if (on) {
        SetCursorPos(ClientCenter().x, ClientCenter().y);  // 첫 delta가 튀지 않도록 중앙으로
        if (!g_cursorHidden) { ShowCursor(FALSE); g_cursorHidden = true; }
        // 커서가 창 밖으로 나가지 않게 가둠
        RECT rc;
        GetClientRect(g_hwnd, &rc);
        POINT tl = { rc.left, rc.top }, br = { rc.right, rc.bottom };
        ClientToScreen(g_hwnd, &tl);
        ClientToScreen(g_hwnd, &br);
        RECT clip = { tl.x, tl.y, br.x, br.y };
        ClipCursor(&clip);
    } else {
        ClipCursor(nullptr);
        if (g_cursorHidden) { ShowCursor(TRUE); g_cursorHidden = false; }
    }
}

// 매 프레임: 중앙에서 얼마나 벗어났는지를 delta로 쓰고 다시 중앙으로 되돌림
static void UpdateMouseLook() {
    if (!g_mouseCaptured) return;
    if (GetForegroundWindow() != g_hwnd) { SetMouseCapture(false); return; }

    POINT center = ClientCenter();
    POINT cur;
    if (!GetCursorPos(&cur)) return;

    float dx = (float)(cur.x - center.x);
    float dy = (float)(cur.y - center.y);

    if (dx != 0.0f || dy != 0.0f) {
        g_camYaw   += dx * g_mouseSens;
        g_camPitch -= dy * g_mouseSens;   // 화면 아래로 움직이면 아래를 봄

        const float limit = XMConvertToRadians(89.0f);
        if (g_camPitch >  limit) g_camPitch =  limit;
        if (g_camPitch < -limit) g_camPitch = -limit;

        SetCursorPos(center.x, center.y);
    }
}

static void UpdateCamera(float dt) {
    UpdateMouseLook();

    // 창이 포커스를 잃으면 키 입력 무시
    if (GetForegroundWindow() != g_hwnd) return;

    const float rotSpeed = 1.8f;   // 라디안/초
    bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
    float moveSpeed = shift ? 10.0f : 4.0f;   // 단위/초

    // --- 방향키로 시점 회전 ---
    if (GetAsyncKeyState(VK_LEFT)  & 0x8000) g_camYaw   -= rotSpeed * dt;
    if (GetAsyncKeyState(VK_RIGHT) & 0x8000) g_camYaw   += rotSpeed * dt;
    if (GetAsyncKeyState(VK_UP)    & 0x8000) g_camPitch += rotSpeed * dt;
    if (GetAsyncKeyState(VK_DOWN)  & 0x8000) g_camPitch -= rotSpeed * dt;

    // 짐벌 뒤집힘 방지 (±89도)
    const float limit = XMConvertToRadians(89.0f);
    if (g_camPitch >  limit) g_camPitch =  limit;
    if (g_camPitch < -limit) g_camPitch = -limit;

    // --- WASD 이동 ---
    XMVECTOR fwd   = CamForward();
    XMVECTOR right = CamRight();
    XMVECTOR up    = XMVectorSet(0, 1, 0, 0);
    XMVECTOR move  = XMVectorZero();

    if (GetAsyncKeyState('W') & 0x8000) move = XMVectorAdd(move, fwd);
    if (GetAsyncKeyState('S') & 0x8000) move = XMVectorSubtract(move, fwd);
    if (GetAsyncKeyState('D') & 0x8000) move = XMVectorAdd(move, right);
    if (GetAsyncKeyState('A') & 0x8000) move = XMVectorSubtract(move, right);
    if (GetAsyncKeyState('E') & 0x8000) move = XMVectorAdd(move, up);
    if (GetAsyncKeyState('Q') & 0x8000) move = XMVectorSubtract(move, up);

    // 대각선 이동이 빨라지지 않도록 정규화
    if (XMVectorGetX(XMVector3LengthSq(move)) > 0.0001f) {
        move = XMVectorScale(XMVector3Normalize(move), moveSpeed * dt);
        XMVECTOR pos = XMVectorAdd(XMLoadFloat3(&g_camPos), move);
        XMStoreFloat3(&g_camPos, pos);
    }
}

static XMMATRIX BuildViewMatrix() {
    XMVECTOR eye = XMLoadFloat3(&g_camPos);
    XMVECTOR at  = XMVectorAdd(eye, CamForward());
    return XMMatrixLookAtLH(eye, at, XMVectorSet(0, 1, 0, 0));
}

static void ResetCamera() {
    g_camPos   = { 0.0f, 0.0f, -5.0f };
    g_camYaw   = 0.0f;
    g_camPitch = 0.0f;
}

// ---------------- 상수 버퍼 ----------------
static void SetCB(const XMMATRIX& world, const XMMATRIX& viewProj, XMFLOAT4 color) {
    CB cb;
    XMStoreFloat4x4(&cb.mvp, XMMatrixTranspose(world * viewProj));
    cb.color = color;

    D3D11_MAPPED_SUBRESOURCE m = {};
    if (FAILED(g_ctx->Map(g_cbuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) return;
    memcpy(m.pData, &cb, sizeof(cb));
    g_ctx->Unmap(g_cbuf, 0);

    g_ctx->VSSetConstantBuffers(0, 1, &g_cbuf);
    g_ctx->PSSetConstantBuffers(0, 1, &g_cbuf);
}

// ---------------- 렌더 ----------------
static void Render(float dt) {
    static float t = 0.0f;
    t += dt;

    UpdateCamera(dt);

    //좌우 벽 선언
    XMMATRIX left_wall = XMMatrixScaling(1.5f, 1.5f, 1.0f) * XMMatrixRotationY(XM_PIDIV2) * XMMatrixTranslation(-1.59f, 0.0f, 1.5f);
    XMMATRIX  right_wall = XMMatrixScaling(1.5f, 1.5f, 1.0f) * XMMatrixRotationY(XM_PIDIV2) * XMMatrixTranslation(1.59f, 0.0f, 1.5f);


    const float clear[4] = { 0.10f, 0.10f, 0.15f, 1.0f };
    g_ctx->ClearRenderTargetView(g_rtv, clear);
    g_ctx->ClearDepthStencilView(g_dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    XMMATRIX view = BuildViewMatrix();
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XM_PIDIV4, (float)W / (float)H, 0.1f, 100.0f);
    XMMATRIX viewProj = view * proj;


    g_ctx->IASetInputLayout(g_layout);
    g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_ctx->VSSetShader(g_vs, nullptr, 0);
    g_ctx->PSSetShader(g_ps, nullptr, 0);

    UINT stride = sizeof(Vertex), off = 0;

    // ========================================================================
    // 그리는 순서가 핵심이다: 실제 게임처럼 [배경/벽] -> [캐릭터] 순서로 그린다.
    // 적을 먼저 그리면 벽이 나중에 덮어써서, 깊이를 꺼도 월핵이 성립하지 않는다.
    // ========================================================================

    // --- 1) 벽 (z=0) : DrawIndexed(6) — 항상 깊이 ON, 깊이 버퍼에 기록 ---
    g_ctx->OMSetDepthStencilState(g_dssOn, 0);
    g_ctx->IASetVertexBuffers(0, 1, &g_wallVB, &stride, &off);
    g_ctx->IASetIndexBuffer(g_wallIB, DXGI_FORMAT_R32_UINT, 0);
    
    //앞벽
    SetCB(XMMatrixScaling(1.5f, 1.5f, 1.0f), viewProj, XMFLOAT4(0.5f, 0.5f, 0.5f, 1));  // 회색
    g_ctx->DrawIndexed(6, 0, 0);
    //좌측의 벽
    SetCB( left_wall, viewProj, XMFLOAT4(0.5f, 0.5f, 0.5f, 1));  // 회색
    g_ctx->DrawIndexed(6, 0, 0);

    //우측의 벽
    SetCB(right_wall, viewProj, XMFLOAT4(0.5f, 0.5f, 0.5f, 1));  // 회색
    g_ctx->DrawIndexed(6, 0, 0);


    // --- 2) 적 (벽 뒤 z=3) : DrawIndexed(36) ---
    //  깊이 ON  -> 벽보다 뒤라서 깊이 테스트 실패 -> 안 보임 (정상 동작)
    //  깊이 OFF -> 깊이 테스트를 안 하므로 벽 위에 덮어 그림 -> 보임 (월핵)
    //  F1 토글이 하는 이 동작을, 나중에 DrawIndexed 후킹으로 똑같이 재현하면 된다.
    g_ctx->OMSetDepthStencilState(g_depthOn ? g_dssOn : g_dssOff, 0);

    g_ctx->IASetVertexBuffers(0, 1, &g_cubeVB, &stride, &off);
    g_ctx->IASetIndexBuffer(g_cubeIB, DXGI_FORMAT_R32_UINT, 0);

    const float ex[2] = { -1.0f, 1.0f };
    for (int i = 0; i < 2; ++i) {
        XMMATRIX wld = XMMatrixScaling(0.6f, 0.6f, 0.6f)
                     * XMMatrixRotationY(t)
                     * XMMatrixTranslation(ex[i], 0.0f, 3.0f);
        SetCB(wld, viewProj, XMFLOAT4(1, 0, 0, 1));   // 빨강
        g_ctx->DrawIndexed(36, 0, 0);                 // <== 후킹 판별 지점
    }

    // 상태 복구 (후킹 코드도 원본 상태로 되돌려야 한다)
    g_ctx->OMSetDepthStencilState(g_dssOn, 0);

    g_swap->Present(1, 0);
}

// ---------------- 정리 ----------------
template <class T> static void Rel(T*& p) { if (p) { p->Release(); p = nullptr; } }

static void Cleanup() {
    if (g_ctx) g_ctx->ClearState();
    Rel(g_cubeIB); Rel(g_cubeVB); Rel(g_wallIB); Rel(g_wallVB);
    Rel(g_cbuf);   Rel(g_layout); Rel(g_ps);     Rel(g_vs);
    Rel(g_dssOff); Rel(g_dssOn);  Rel(g_rs);
    Rel(g_dsv);    Rel(g_rtv);    Rel(g_swap);   Rel(g_ctx);  Rel(g_dev);
}

// ---------------- 윈도우 ----------------
static void UpdateTitle(HWND hwnd) {
    char buf[160];
    snprintf(buf, sizeof(buf), "Hook Study | Depth:%s | Mouse:%s | pos(%.1f, %.1f, %.1f) | WASD/QE, TAB, F1, R, ESC",
             g_depthOn ? "ON" : "OFF", g_mouseCaptured ? "LOCK" : "FREE",
             g_camPos.x, g_camPos.y, g_camPos.z);
    SetWindowTextA(hwnd, buf);
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_KEYDOWN:
        // ESC: 마우스가 잠겨 있으면 먼저 풀고, 이미 풀려 있으면 종료
        if (w == VK_ESCAPE) {
            if (g_mouseCaptured) SetMouseCapture(false);
            else PostQuitMessage(0);
            return 0;
        }
        if (w == VK_TAB) { SetMouseCapture(!g_mouseCaptured); return 0; }
        if (w == VK_F1)  { g_depthOn = !g_depthOn; UpdateTitle(h); return 0; }
        if (w == 'R')    { ResetCamera(); UpdateTitle(h); return 0; }
        break;

    // 창을 클릭하면 다시 마우스 잠금
    case WM_LBUTTONDOWN:
        SetMouseCapture(true);
        return 0;

    // 창이 비활성화되면 잠금 해제 (Alt+Tab 등)
    case WM_ACTIVATE:
        if (LOWORD(w) == WA_INACTIVE) SetMouseCapture(false);
        return 0;

    case WM_KILLFOCUS:
        SetMouseCapture(false);
        return 0;

    case WM_DESTROY:
        SetMouseCapture(false);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

int main() {
    HINSTANCE hInst = GetModuleHandleA(nullptr);

    WNDCLASSA wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = "d3dwnd";
    if (!RegisterClassA(&wc)) { MessageBoxA(nullptr, "RegisterClass failed", "Error", MB_OK); return 1; }

    DWORD style = (WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX);
    RECT rc = { 0, 0, W, H };
    AdjustWindowRect(&rc, style, FALSE);

    g_hwnd = CreateWindowA("d3dwnd", "Hook Study", style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInst, nullptr);
    if (!g_hwnd) { MessageBoxA(nullptr, "CreateWindow failed", "Error", MB_OK); return 1; }

    ShowWindow(g_hwnd, SW_SHOW);

    if (!InitD3D(g_hwnd)) { Cleanup(); return 1; }
    UpdateTitle(g_hwnd);

    SetForegroundWindow(g_hwnd);
    SetMouseCapture(true);   // 시작하자마자 마우스로 시점 조작 가능

    // 델타 타임용 고해상도 타이머
    LARGE_INTEGER freq, prev;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);

    int titleTick = 0;

    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        } else {
            LARGE_INTEGER now;
            QueryPerformanceCounter(&now);
            float dt = (float)(now.QuadPart - prev.QuadPart) / (float)freq.QuadPart;
            prev = now;
            if (dt > 0.1f) dt = 0.1f;   // 디버거 정지 등으로 튀는 것 방지

            Render(dt);

            if (++titleTick >= 15) { titleTick = 0; UpdateTitle(g_hwnd); }  // 좌표 표시 갱신
        }
    }

    SetMouseCapture(false);   // 커서 복구 (안 하면 커서가 숨은 채로 남음)
    Cleanup();
    return (int)msg.wParam;
}