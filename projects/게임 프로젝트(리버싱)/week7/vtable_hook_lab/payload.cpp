// clang++ -std=c++17 -shared game/payload.cpp -o game/payload.dll -ld3d11 -ldxgi -luser32
#include <windows.h>
#include <stdio.h>
#include <d3d11.h>
#include <winnt.h>

void (*originalDrawIndexed)(ID3D11DeviceContext*, UINT, UINT, INT) = nullptr;

ID3D11DepthStencilState* g_depthOff = nullptr;
void initalizeDepthState(ID3D11DeviceContext* ctx) {
    if (g_depthOff) return;

    ID3D11Device* dev = nullptr;
    ctx->GetDevice(&dev);

    D3D11_DEPTH_STENCIL_DESC d = {};
    d.DepthFunc      = D3D11_COMPARISON_LESS;
    d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    d.DepthEnable    = FALSE;

    dev->CreateDepthStencilState(&d, &g_depthOff);
    dev->Release();
}

void myDrawIndexed(ID3D11DeviceContext* ctx, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation) {
    initalizeDepthState(ctx);

    //그리는 대상이 벽인 경우나  g_depthOff가 아직 생성 안 됐거나 실패해서 NULL인 경우에 대한 처리
    if (IndexCount != 36 || !g_depthOff)
        return originalDrawIndexed(ctx, IndexCount, StartIndexLocation, BaseVertexLocation);

    UINT ref, old;
    ID3D11DepthStencilState* oldState;
    // 1. 기존 depth 상태 저장
    ctx->OMGetDepthStencilState(&oldState, &old);
    // 2. depth test OFF
    ctx->OMSetDepthStencilState(g_depthOff, ref);
    // 3. 적 그리기
    originalDrawIndexed(ctx, IndexCount, StartIndexLocation, BaseVertexLocation);
    // 4. 기존 depth 상태로 복구
    ctx->OMSetDepthStencilState(oldState, old);
    //해제
    oldState->Release();
}

void installVTableHook() {
    // 1. ID3D11ContextDevice 의 vtable 주소 구하기
    void* base = GetModuleHandle(NULL);

    //IDA의 .data:14002FC78에서 ppImmediateContext가 image base 140000000을 뺀 RVA가 2FC78이라서 base + 0x2FC78을 사용한 것.
    ID3D11DeviceContext* ctx = *(ID3D11DeviceContext**)((char*)base + 0x2FC78);
    // ID3D11DeviceContext 내부의 struct ID3D11DeviceContext1Vtbl *lpVtbl;로 테이블을 접근하기 때문에 vtable을 이렇게 선언을 해야 한다.
    void** vtable = *(void***)ctx;

    // 2. vtable 에 있는 DrawIndex 함수 바꿔치기
    
    //VirtualProtect()를 호출할 때 기존 메모리 보호 속성을 저장할 변수
    DWORD old;
    // vtable의 12번 째 저장된 함수 주소를 type casting을 해서 originalDrawIndexed에 백업한다
    originalDrawIndexed = (void (*)(ID3D11DeviceContext*, UINT, UINT, INT)) vtable[12];
    //vtable의 13번째 주소에 대해서 쓰기권한을 임시로 부여함
    VirtualProtect(&vtable[12], sizeof(void*), PAGE_READWRITE, &old);
    //주소를 내가 호출할 함수로 변경함
    vtable[12] = (void*) &myDrawIndexed;
    //다시 원래 읽기모드로 돌아감
    VirtualProtect(&vtable[12], sizeof(void*), old, &old);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    HANDLE th;

    switch (reason) {
        case DLL_PROCESS_ATTACH:
            printf("[*] DLL Injection 성공");
            installVTableHook();
            break;

        case DLL_PROCESS_DETACH:
            break;
    }
    return TRUE;
}
