// Local evidence runner: copies are supplied by Validate-Hk01.ps1.
// Debug events only observe code and OutputDebugString; no guard is disabled.
#include <Windows.h>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

static bool Launch(const std::wstring& command, const wchar_t* cwd, DWORD flags, PROCESS_INFORMATION& process)
{
    STARTUPINFOW start{};
    start.cb = sizeof(start);
    start.dwFlags = STARTF_USESHOWWINDOW;
    start.wShowWindow = SW_HIDE;
    std::vector<wchar_t> buffer(command.begin(), command.end());
    buffer.push_back(0);
    return CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE, flags,
        nullptr, cwd, &start, &process) != FALSE;
}

static void Entry(HANDLE process, std::uintptr_t address, const char* label)
{
    BYTE bytes[16]{};
    SIZE_T count = 0;
    std::printf("ENTRY %s address=%llX ", label, static_cast<unsigned long long>(address));
    if (ReadProcessMemory(process, reinterpret_cast<const void*>(address), bytes, sizeof(bytes), &count))
        for (SIZE_T i = 0; i < count; ++i) std::printf("%02X ", bytes[i]);
    else std::printf("unreadable=%lu", GetLastError());
    std::printf("\n");
}

int wmain(int argc, wchar_t** argv)
{
    if (argc != 3) return 99; // cwd and DLL name, single DLL per fresh process
    const std::wstring cwd = argv[1];
    PROCESS_INFORMATION game{}, injector{};
    if (!Launch(L"\"" + cwd + L"\\GameClient.exe\"", cwd.c_str(), DEBUG_ONLY_THIS_PROCESS, game)) return 98;
    const ULONGLONG start = GetTickCount64();
    std::uintptr_t base = 0;
    bool baseline = false, injected = false, detected = false, finished = false;
    DWORD gameExit = STILL_ACTIVE;
    while (GetTickCount64() - start < 10000)
    {
        DEBUG_EVENT event{};
        if (WaitForDebugEvent(&event, 100))
        {
            DWORD continuation = DBG_CONTINUE;
            if (event.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT)
            {
                base = reinterpret_cast<std::uintptr_t>(event.u.CreateProcessInfo.lpBaseOfImage);
                if (event.u.CreateProcessInfo.hFile) CloseHandle(event.u.CreateProcessInfo.hFile);
                // Debug-event process/thread handles are owned by the debug
                // session until exit events. Do not close and reuse them early.
                Entry(game.hProcess, base + 0x137e0, "old_spawn_before");
                Entry(game.hProcess, base + 0xef90, "old_scanner_before");
            }
            else if (event.dwDebugEventCode == LOAD_DLL_DEBUG_EVENT && event.u.LoadDll.hFile)
                CloseHandle(event.u.LoadDll.hFile);
            else if (event.dwDebugEventCode == OUTPUT_DEBUG_STRING_EVENT)
            {
                const auto& info = event.u.DebugString;
                const SIZE_T bytes = static_cast<SIZE_T>(info.nDebugStringLength) * (info.fUnicode ? sizeof(wchar_t) : 1);
                std::vector<char> data(bytes + 2, 0);
                SIZE_T read = 0;
                if (ReadProcessMemory(game.hProcess, info.lpDebugStringData, data.data(), bytes, &read))
                {
                    std::string text;
                    if (info.fUnicode)
                    {
                        const int length = WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<const wchar_t*>(data.data()),
                            static_cast<int>(read / sizeof(wchar_t)), nullptr, 0, nullptr, nullptr);
                        text.resize(length);
                        WideCharToMultiByte(CP_UTF8, 0, reinterpret_cast<const wchar_t*>(data.data()),
                            static_cast<int>(read / sizeof(wchar_t)), text.data(), length, nullptr, nullptr);
                    }
                    else text.assign(data.data(), read);
                    std::printf("DEBUG ms=%llu %s", GetTickCount64()-start, text.c_str());
                    if (text.find("Baseline initialized") != std::string::npos) baseline = true;
                    if (text.find("New DLL detected") != std::string::npos) detected = true;
                }
            }
            else if (event.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
            {
                const auto code = event.u.Exception.ExceptionRecord.ExceptionCode;
                if (code != EXCEPTION_BREAKPOINT && code != 0x40010006 && code != 0x4001000a)
                {
                    std::printf("EXCEPTION code=%08lX firstChance=%lu\n", code, event.u.Exception.dwFirstChance);
                    continuation = DBG_EXCEPTION_NOT_HANDLED;
                }
            }
            else if (event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT)
            {
                gameExit = event.u.ExitProcess.dwExitCode;
                finished = true;
            }
            ContinueDebugEvent(event.dwProcessId, event.dwThreadId, continuation);
            if (finished) break;
        }
        if (baseline && !detected && !injected && GetTickCount64()-start >= 2000)
        {
            const std::wstring command = L"\"" + cwd + L"\\injector.exe\" " + std::to_wstring(game.dwProcessId) +
                L" \"" + cwd + L"\\" + argv[2] + L"\" -";
            injected = Launch(command, cwd.c_str(), 0, injector);
            std::printf("INJECTION_LAUNCHED success=%d target_pid=%lu\n", injected, game.dwProcessId);
            if (!injected) break;
        }
        if (detected && !injected)
        {
            std::printf("BASELINE_FAILED new_DLL_before_injection\n");
            break;
        }
        if (injected && GetTickCount64()-start >= 6000) break;
    }
    if (!finished)
    {
        Entry(game.hProcess, base + 0x137e0, "old_spawn_after");
        Entry(game.hProcess, base + 0xef90, "old_scanner_after");
        // Cleanup of only our fresh child. This is explicitly not a natural game exit.
        TerminateProcess(game.hProcess, 77);
        // Drain debug events so process termination and file writes finish.
        const ULONGLONG cleanupStart = GetTickCount64();
        while (GetTickCount64()-cleanupStart < 3000)
        {
            DEBUG_EVENT event{};
            if (!WaitForDebugEvent(&event, 100)) continue;
            const bool exited = event.dwDebugEventCode == EXIT_PROCESS_DEBUG_EVENT;
            if (exited) gameExit = event.u.ExitProcess.dwExitCode;
            ContinueDebugEvent(event.dwProcessId, event.dwThreadId, DBG_CONTINUE);
            if (exited) break;
        }
        std::printf("CLEANUP external_termination=1 exit=%lu\n", gameExit);
    }
    if (injector.hProcess)
    {
        if (WaitForSingleObject(injector.hProcess, 2000) != WAIT_OBJECT_0) TerminateProcess(injector.hProcess, 77);
        DWORD code = STILL_ACTIVE;
        if (GetExitCodeProcess(injector.hProcess, &code)) std::printf("INJECTOR_EXIT=%lu\n", code);
        else std::printf("INJECTOR_EXIT_UNAVAILABLE error=%lu\n", GetLastError());
        CloseHandle(injector.hThread); CloseHandle(injector.hProcess);
    }
    CloseHandle(game.hThread); CloseHandle(game.hProcess);
    std::printf("RESULT baseline=%d injection_launched=%d module_detected=%d natural_exit=%d\n", baseline, injected, detected, finished);
    return baseline && injected ? 0 : 2;
}
