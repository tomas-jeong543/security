#include <stdio.h>
#include <string.h>
#include <windows.h>

// Verified against GameClient(1).exe, matching GameClient.pdb.
// GameWorld::Initialize() RVA: 0x110D0
// Player speed initialization instruction RVA: 0x11158
// Corresponding raw file offset: 0x10558
#define SPEED_PATCH_RVA 0x11158

static void PatchHeroSpeed(void)
{
    BYTE* base = (BYTE*)GetModuleHandleW(NULL);
    if (base == NULL) return;

    BYTE* target = base + SPEED_PATCH_RVA;
    const BYTE expected[] = { 0xC7, 0x41, 0x3C, 0x00, 0x00, 0x5C, 0x43 }; // 220.0f
    const BYTE patch[] = { 0xC7, 0x41, 0x3C, 0x00, 0x00, 0xDC, 0x43 }; // 440.0f

    // Avoid overwriting a different instruction when the game build changes.
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
    OutputDebugStringA("[Payload] Hero speed initialization patch applied.\n");
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
