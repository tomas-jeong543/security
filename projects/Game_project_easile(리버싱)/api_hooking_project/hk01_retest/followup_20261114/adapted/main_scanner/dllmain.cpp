/*
 * EASILE HK-01: external, observation-only DLLGuard function hook (Windows x64).
 * Target: user's GameClient.exe SHA-256
 * 32a1d0d4d8fbf15e2a1e45467475a6057f26eee54b1af5746945de24c768a926
 *
 * The DetectNewModules implementation RVA 0xEF90 was recovered from this
 * exact binary's .pdata function boundaries and disassembly. It is NOT an
 * export, and this RVA must NOT be reused blindly for another build.
 *
 *
 * Build with Visual Studio x64 and MinHook's x64 static library.
 */



#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <MinHook.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "target_pin.h"

#define GUARD_SCAN_RVA 0x1B050u

 /* MSVC x64 uses a uniform calling convention for this no-argument function. */
typedef void(__cdecl* DetectNewModulesFn)(void);

static DetectNewModulesFn g_original_scan = NULL;
static volatile LONG g_scan_count = 0;

/* Write messages to DebugView / the debugger AND a local file next to EXE.
   One write per event, opened with FILE_APPEND_DATA to avoid overwrites. */
static void log_event(const char* message)
{
    wchar_t path[MAX_PATH];
    wchar_t* last_slash;
    size_t chars_remaining;
    HANDLE file;
    DWORD length;
    DWORD written;

    if (message == NULL) return;
    OutputDebugStringA(message);

    length = GetModuleFileNameW(NULL, path, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) return;

    last_slash = wcsrchr(path, L'\\');
    if (last_slash == NULL) return;
    chars_remaining = MAX_PATH - (size_t)(last_slash + 1 - path);
    if (wcscpy_s(last_slash + 1, chars_remaining,
        L"guard_observer.log") != 0) return;

    file = CreateFileW(path, FILE_APPEND_DATA,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    length = (DWORD)strlen(message);
    WriteFile(file, message, length, &written, NULL);
    CloseHandle(file);
}

static void log_mh_status(const char* stage, MH_STATUS status)
{
    char message[256];
    sprintf_s(message, sizeof(message),
        "[GUARD-OBS] %s: %s (%d)\n",
        stage, MH_StatusToString(status), (int)status);
    log_event(message);
}

/* Detour: bypass the original DLLGuard module scan. */
static void __cdecl observed_detect_new_modules(void)
{
    char message[256];

    const LONG sequence = InterlockedIncrement(&g_scan_count);

    sprintf_s(
        message,
        sizeof(message),
        "[GUARD-BYPASS] SKIPPED #%ld pid=%lu tid=%lu\n",
        sequence,
        GetCurrentProcessId(),
        GetCurrentThreadId()
    );

    log_event(message);

    // Original DetectNewModules() is intentionally not called.
    return;
}

/* Check PE format and original function entry bytes to avoid patching some
   other function when the build does not match the analyzed EXE. */
static BOOL verify_game_build(HMODULE game_module)
{
    if (!verify_pinned_executable()) return FALSE;
    static const BYTE entry_bytes[] = {
        0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x08,
        0x48, 0x89, 0x70, 0x10, 0x48, 0x89, 0x78, 0x18
    };
    const BYTE* base = (const BYTE*)game_module;
    const IMAGE_DOS_HEADER* dos;
    const IMAGE_NT_HEADERS64* nt;
    const BYTE* target;
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T offset;

    if (base == NULL) return FALSE;
    if (!VirtualQuery(base, &mbi, sizeof(mbi)) ||
        mbi.State != MEM_COMMIT || mbi.RegionSize < sizeof(IMAGE_DOS_HEADER))
        return FALSE;

    dos = (const IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE ||
        dos->e_lfanew <= 0 || dos->e_lfanew > 0x1000)
        return FALSE;

    nt = (const IMAGE_NT_HEADERS64*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt->OptionalHeader.SizeOfImage < GUARD_SCAN_RVA + sizeof(entry_bytes))
        return FALSE;

    target = base + GUARD_SCAN_RVA;
    if (!VirtualQuery(target, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT ||
        (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)))
        return FALSE;

    offset = (SIZE_T)((uintptr_t)target - (uintptr_t)mbi.BaseAddress);
    if (offset > mbi.RegionSize ||
        sizeof(entry_bytes) > mbi.RegionSize - offset)
        return FALSE;

    return memcmp(target, entry_bytes, sizeof(entry_bytes)) == 0;
}

/* Initialization is intentionally on a worker, not under the loader lock. */
static DWORD WINAPI observer_worker(LPVOID ignored)
{
    HMODULE game;
    LPVOID target;
    MH_STATUS status;
    BOOL initialized_here = FALSE;

    (void)ignored;
    game = GetModuleHandleW(NULL);
    if (!verify_game_build(game)) {
        log_event("[GUARD-OBS] Wrong/unrecognized GameClient build; no hook installed.\n");
        return 1;
    }

    target = (LPVOID)((BYTE*)game + GUARD_SCAN_RVA);
    status = MH_Initialize();
    if (status == MH_OK) initialized_here = TRUE;
    else if (status != MH_ERROR_ALREADY_INITIALIZED) {
        log_mh_status("MH_Initialize", status);
        return 2;
    }

    status = MH_CreateHook(target, (LPVOID)observed_detect_new_modules,
        (LPVOID*)&g_original_scan);
    if (status != MH_OK) {
        log_mh_status("MH_CreateHook", status);
        if (initialized_here) MH_Uninitialize();
        return 3;
    }

    status = MH_EnableHook(target);
    if (status != MH_OK) {
        log_mh_status("MH_EnableHook", status);
        MH_RemoveHook(target);
        if (initialized_here) MH_Uninitialize();
        return 4;
    }

    log_event("[GUARD-OBS] Hook installed at GameClient + 0x1B050; "
        "original scanner bypass installed (retest).\n");
    /* Keep the DLL loaded for the lifetime of this test process. Do not
       FreeLibrary it while the detour may be executing. */
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    HANDLE worker;
    (void)reserved;

    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
        worker = CreateThread(NULL, 0, observer_worker, NULL, 0, NULL);
        if (worker == NULL) return FALSE;
        CloseHandle(worker);
    }
    /* Never call MinHook cleanup from DLL_PROCESS_DETACH (loader lock). */
    return TRUE;
}
