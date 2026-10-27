#include <stdio.h>
#include <string.h>
#include <windows.h>

// GameClient.exe (uploaded build): Hero initialization speed assignment
// RVA 0x110A8 / file offset 0x104A8
#define SPEED_PATCH_RVA 0x110A8

static void PatchHeroSpeed(void)
{
    BYTE* base = (BYTE*)GetModuleHandleW(NULL);
    if (base == NULL) return;

    BYTE* target = base + SPEED_PATCH_RVA;
    const BYTE expected[] = { 0xC7, 0x41, 0x3C, 0x00, 0x00, 0x5C, 0x43 }; // 220.0f
    const BYTE patch[] = { 0xC7, 0x41, 0x3C, 0x00, 0x00, 0xDC, 0x43 }; // 440.0f

    // Do not overwrite unknown code after a rebuild.
    if (memcmp(target, expected, sizeof(expected)) != 0) {
        OutputDebugStringA("[Payload] Unexpected original bytes; patch skipped.\n");
        return;
    }

    DWORD oldProtect = 0;
    if (!VirtualProtect(target, sizeof(patch), PAGE_EXECUTE_READWRITE, &oldProtect)) {
        OutputDebugStringA("[Payload] VirtualProtect failed.\n");
        return;
    }

    memcpy(target, patch, sizeof(patch));
    FlushInstructionCache(GetCurrentProcess(), target, sizeof(patch));

    DWORD unusedProtect = 0;
    VirtualProtect(target, sizeof(patch), oldProtect, &unusedProtect);
    OutputDebugStringA("[Payload] Hero speed patch applied.\n");
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    (void)instance;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        PatchHeroSpeed();
    }
    return TRUE;
}
