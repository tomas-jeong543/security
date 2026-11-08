#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <stdint.h>
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")

/* EASILE HK-01 - x64, Windows, Visual Studio 2022.
   Only run against your own test GameClient process. */

static const wchar_t* base_name(const wchar_t* path)
{
    const wchar_t* a = wcsrchr(path, L'\\');
    const wchar_t* b = wcsrchr(path, L'/');
    const wchar_t* p = (a && (!b || a > b)) ? a : b;
    return p ? p + 1 : path;
}

static HMODULE find_remote_module(DWORD pid, const wchar_t* name)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    MODULEENTRY32W entry = { 0 };
    HMODULE result = NULL;
    if (snapshot == INVALID_HANDLE_VALUE) return NULL;
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szModule, name) == 0) {
                result = entry.hModule;
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
    return result;
}

/* Resolve LoadLibraryW in the remote process using the matching module's
   remote base + locally obtained export RVA. This avoids assuming that the
   remote and local system-DLL base addresses are identical. */
static LPTHREAD_START_ROUTINE remote_load_library(DWORD pid)
{
    HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");
    FARPROC local_proc = kernel32 ? GetProcAddress(kernel32, "LoadLibraryW") : NULL;
    HMODULE owner = NULL, remote_owner = NULL;
    wchar_t owner_path[MAX_PATH];
    if (!local_proc) return NULL;

    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCWSTR)(uintptr_t)local_proc, &owner)) return NULL;
    if (!GetModuleFileNameW(owner, owner_path, MAX_PATH)) return NULL;
    remote_owner = find_remote_module(pid, base_name(owner_path));
    if (!remote_owner) return NULL;

    return (LPTHREAD_START_ROUTINE)((BYTE*)remote_owner +
        ((BYTE*)(uintptr_t)local_proc - (BYTE*)owner));
}

/* Resolve a DLL path relative to injector.exe's directory, not the shell's
   current working directory. Remote LoadLibraryW still receives an absolute
   path, which is more reliable than a relative one in the target process. */
static BOOL resolve_dll_path(const wchar_t* dll_input, wchar_t* full_path, DWORD capacity)
{
    wchar_t candidate[32768];
    wchar_t injector_path[32768];
    wchar_t* separator;
    const wchar_t* source = dll_input;
    DWORD length;

    if (!dll_input || !dll_input[0]) return FALSE;

    if (PathIsRelativeW(dll_input)) {
        length = GetModuleFileNameW(NULL, injector_path,
            (DWORD)(sizeof(injector_path) / sizeof(injector_path[0])));
        if (!length || length >= (DWORD)(sizeof(injector_path) / sizeof(injector_path[0])))
            return FALSE;

        separator = wcsrchr(injector_path, L'\\');
        if (!separator) return FALSE;
        *separator = L'\0';
        if (swprintf_s(candidate, sizeof(candidate) / sizeof(candidate[0]),
            L"%ls\\%ls", injector_path, dll_input) < 0)
            return FALSE;
        source = candidate;
    }

    length = GetFullPathNameW(source, capacity, full_path, NULL);
    return length != 0 && length < capacity;
}

static int inject(DWORD pid, const wchar_t* dll_input)
{
    wchar_t full_path[32768];
    SIZE_T bytes, written = 0;
    HANDLE process = NULL, thread = NULL;
    LPVOID remote_path = NULL;
    LPTHREAD_START_ROUTINE load_library;
    DWORD wait_status, thread_result = 0;
    BOOL finished = FALSE;
    int result = 1;

    if (!resolve_dll_path(dll_input, full_path,
        (DWORD)(sizeof(full_path) / sizeof(full_path[0])))) {
        fwprintf(stderr, L"[-] DLL path resolution failed. Error=%lu\n", GetLastError());
        return 1;
    }
    if (GetFileAttributesW(full_path) == INVALID_FILE_ATTRIBUTES) {
        fwprintf(stderr, L"[-] DLL file not found: %ls\n", full_path);
        return 1;
    }

    wprintf(L"[*] Target PID: %lu\n[*] DLL: %ls\n", pid, full_path);
    process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
        PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
        FALSE, pid);
    if (!process) {
        fwprintf(stderr, L"[-] OpenProcess failed. Error=%lu\n", GetLastError());
        goto cleanup;
    }

    /* Only supports matching 64-bit injector + 64-bit target. */
    {
        BOOL wow64 = FALSE;
        if (!IsWow64Process(process, &wow64)) {
            fwprintf(stderr, L"[-] IsWow64Process failed. Error=%lu\n", GetLastError());
            goto cleanup;
        }
        if (wow64) {
            fwprintf(stderr, L"[-] This injector requires a 64-bit target.\n");
            goto cleanup;
        }
    }

    if (find_remote_module(pid, base_name(full_path))) {
        fwprintf(stderr, L"[-] A module with that filename is already loaded. Restart the test game first.\n");
        goto cleanup;
    }

    load_library = remote_load_library(pid);
    if (!load_library) {
        fwprintf(stderr, L"[-] Could not resolve remote LoadLibraryW. Check process architecture/access.\n");
        goto cleanup;
    }
    bytes = ((SIZE_T)wcslen(full_path) + 1u) * sizeof(wchar_t);
    remote_path = VirtualAllocEx(process, NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote_path) {
        fwprintf(stderr, L"[-] VirtualAllocEx failed. Error=%lu\n", GetLastError());
        goto cleanup;
    }
    if (!WriteProcessMemory(process, remote_path, full_path, bytes, &written) || written != bytes) {
        fwprintf(stderr, L"[-] WriteProcessMemory failed. Error=%lu\n", GetLastError());
        goto cleanup;
    }

    thread = CreateRemoteThread(process, NULL, 0, load_library, remote_path, 0, NULL);
    if (!thread) {
        fwprintf(stderr, L"[-] CreateRemoteThread failed. Error=%lu\n", GetLastError());
        goto cleanup;
    }
    wprintf(L"[*] Remote DLL loading started; waiting for completion...\n");
    wait_status = WaitForSingleObject(thread, 15000);
    if (wait_status != WAIT_OBJECT_0) {
        fwprintf(stderr, L"[-] Remote thread did not finish normally (wait=%lu).\n", wait_status);
        /* Do NOT free the remote string while the thread might still use it. */
        goto cleanup;
    }
    finished = TRUE;
    /* GetExitCodeThread returns DWORD even for x64 targets, so its value
       is NOT a reliable complete HMODULE. Verify by enumerating modules. */
    if (!GetExitCodeThread(thread, &thread_result)) {
        fwprintf(stderr, L"[-] GetExitCodeThread failed. Error=%lu\n", GetLastError());
        goto cleanup;
    }
    (void)thread_result;
    if (!find_remote_module(pid, base_name(full_path))) {
        fwprintf(stderr, L"[-] DLL was not observed in the remote module list.\n");
        goto cleanup;
    }
    wprintf(L"[+] DLL loaded in target process. Hook installation is a separate step.\n");
    result = 0;

cleanup:
    if (remote_path && (!thread || finished)) VirtualFreeEx(process, remote_path, 0, MEM_RELEASE);
    if (thread) CloseHandle(thread);
    if (process) CloseHandle(process);
    return result;
}

/* Usage:
   injector.exe <PID> [hook-DLL-path] [payload-DLL-path]
   Default paths: .\Dll2.dll and .\payload.dll, relative to injector.exe.
   For only the first DLL, pass - as the third path argument.

   IMPORTANT: Loading a DLL is not proof that its asynchronous worker thread
   has finished installing the hook. With two DLLs, ask the operator to
   verify the hook-ready log before continuing. */
int wmain(int argc, wchar_t** argv)
{
    wchar_t pid_text[32];
    wchar_t confirmation[16];
    wchar_t* end = NULL;
    unsigned long value;
    const wchar_t* first_dll;
    const wchar_t* second_dll;

    if (argc > 4) {
        fwprintf(stderr, L"Usage: injector.exe <PID> [hook-DLL-path] [second-DLL-path]\n");
        return 1;
    }

    if (argc >= 2) {
        value = wcstoul(argv[1], &end, 10);
        if (!argv[1][0] || *end || value == 0) {
            fwprintf(stderr, L"Usage: injector.exe <PID> [hook-DLL-path] [second-DLL-path]\n");
            return 1;
        }
    }
    else {
        wprintf(L"GameClient PID: ");
        if (!fgetws(pid_text, (int)(sizeof(pid_text) / sizeof(pid_text[0])), stdin)) return 1;
        value = wcstoul(pid_text, &end, 10);
        if (value == 0 || (end && *end != L'\n' && *end != L'\0')) return 1;
    }

    first_dll = argc >= 3 ? argv[2] : L".\\Dll2.dll";
    second_dll = argc >= 4 ? argv[3] : L".\\hook_payload.dll";
    if (wcscmp(second_dll, L"-") == 0) second_dll = NULL;

    wprintf(L"[*] Relative DLL paths are based on injector.exe's directory.\n");
    wprintf(L"\n[1/2] Loading first DLL (hook) ...\n");
    if (inject((DWORD)value, first_dll) != 0) {
        fwprintf(stderr, L"[-] First DLL failed. Second DLL will NOT be loaded.\n");
        return 1;
    }

    if (second_dll == NULL) {
        wprintf(L"[+] Single-DLL injection complete.\n");
        return 0;
    }

    /* The hook DLL's DllMain starts a worker asynchronously. Waiting for
       LoadLibraryW alone does not establish that MH_EnableHook succeeded. */
    wprintf(L"\n[*] Verify the first DLL's hook-installed log (guard_observer.log).\n");
    wprintf(L"[*] Press Enter to load the second DLL, or type q + Enter to abort: ");
    fflush(stdout);
    if (!fgetws(confirmation, (int)(sizeof(confirmation) / sizeof(confirmation[0])), stdin))
        return 1;
    if (confirmation[0] == L'q' || confirmation[0] == L'Q') {
        wprintf(L"[*] Second DLL canceled. First DLL remains loaded.\n");
        return 1;
    }

    wprintf(L"\n[2/2] Loading second DLL (payload) ...\n");
    if (inject((DWORD)value, second_dll) != 0) {
        fwprintf(stderr, L"[-] Second DLL failed. First DLL may still be loaded.\n");
        return 1;
    }

    wprintf(L"\n[+] Both DLLs are present in the target module list.\n");
    return 0;
}
