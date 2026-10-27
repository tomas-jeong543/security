#include "IntegrityGuard.h"

#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <Windows.h>

// PATCH-02에 필요한 표준 라이브러리 / Windows 모듈 열거 API
#include <TlHelp32.h>
#include <algorithm>
#include <cwctype>
#include <string>
#include <unordered_set>

namespace IntegrityGuard
{

    // PATCH-01: GameWorld::Update()
    constexpr uintptr_t UPDATE_RVA = 0x144C0;
    constexpr size_t UPDATE_SIZE = 0x13BB;
    constexpr uint32_t EXPECTED_UPDATE_CRC = 0xB46010D3;

    // PATCH-02: GameWorld::Initialize()
    constexpr uintptr_t HERO_RVA = 0x10D70;
    constexpr size_t HERO_SIZE = 0x685;
    constexpr uint32_t EXPECTED_HERO_CRC = 0x17743DC9;


    uint32_t CalculateCRC32(const uint8_t* data, size_t length)
    {
        uint32_t crc = 0xFFFFFFFF;

        for (size_t i = 0; i < length; ++i)
        {
            crc ^= data[i];

            for (int j = 0; j < 8; ++j)
            {
                if (crc & 1)
                    crc = (crc >> 1) ^ 0xEDB88320;
                else
                    crc >>= 1;
            }
        }

        return crc ^ 0xFFFFFFFF;
    }

    bool VerifyRegionIntegrity(
        uintptr_t rva,
        size_t size,
        uint32_t expectedCRC,
        const char* regionName
    )
    {
        HMODULE module = GetModuleHandleW(nullptr);
        if (module == nullptr)
            return false;

        uintptr_t moduleBase = reinterpret_cast<uintptr_t>(module);
        const uint8_t* address =
            reinterpret_cast<const uint8_t*>(moduleBase + rva);

        uint32_t currentCRC = CalculateCRC32(address, size);

        char buffer[128];
        sprintf_s(
            buffer,
            "[Integrity] %s CRC32 = %08X\n",
            regionName,
            static_cast<unsigned int>(currentCRC)
        );
        OutputDebugStringA(buffer);

        return currentCRC == expectedCRC;
    }

    __declspec(noinline)
        bool VerifyUpdateIntegrity()
    {
        return VerifyRegionIntegrity(
            UPDATE_RVA,
            UPDATE_SIZE,
            EXPECTED_UPDATE_CRC,
            "Update"
        );
    }


    __declspec(noinline)
        bool VerifyHeroIntegrity()
    {
        return VerifyRegionIntegrity(
            HERO_RVA,
            HERO_SIZE,
            EXPECTED_HERO_CRC,
            "Hero"
        );
    }


    // ===== PATCH-02: DLL Baseline 탐지 =====
    namespace
    {
        // 한 프로세스 안에서 유지되는 최초 정상 목록과 탐지 이력
        std::unordered_set<std::wstring> baselineDlls;
        std::unordered_set<std::wstring> reportedDlls;
        bool baselineInitialized = false;

        // 경로 대소문자 차이로 생기는 비교 오류를 줄이기 위한 간단한 정규화
        std::wstring NormalizePath(std::wstring path)
        {
            std::transform(
                path.begin(), path.end(), path.begin(),
                [](wchar_t ch)
                {
                    return static_cast<wchar_t>(std::towlower(ch));
                }
            );
            return path;
        }

        // 현재 프로세스에 로드된 .dll 모듈들의 전체 경로를 수집
        bool CaptureLoadedDlls(std::unordered_set<std::wstring>& result)
        {
            result.clear();

            HANDLE snapshot = CreateToolhelp32Snapshot(
                TH32CS_SNAPMODULE,
                GetCurrentProcessId()
            );

            if (snapshot == INVALID_HANDLE_VALUE)
                return false;

            MODULEENTRY32W module{};
            module.dwSize = sizeof(module);

            if (!Module32FirstW(snapshot, &module))
            {
                CloseHandle(snapshot);
                return false;
            }

            do
            {
                std::wstring name = NormalizePath(module.szModule);

                // EXE는 제외하고 DLL 파일만 Baseline에 넣는다.
                if (name.size() >= 4 &&
                    name.compare(name.size() - 4, 4, L".dll") == 0)
                {
                    result.insert(NormalizePath(module.szExePath));
                }
            } while (Module32NextW(snapshot, &module));

            // 마지막 Module32NextW 실패가 정상 종료인지 확인
            const DWORD error = GetLastError();
            CloseHandle(snapshot);

            return error == ERROR_NO_MORE_FILES;
        }
    } // anonymous namespace

    // 정상 실행 상태를 최초 Baseline으로 고정
    bool InitializeDllBaseline()
    {
        std::unordered_set<std::wstring> currentDlls;

        if (!CaptureLoadedDlls(currentDlls))
        {
            OutputDebugStringW(L"[DLLGuard] Baseline capture failed\n");
            baselineInitialized = false;
            return false;
        }

        baselineDlls.swap(currentDlls);
        reportedDlls.clear();
        baselineInitialized = true;

        wchar_t message[128];
        swprintf_s(
            message,
            L"[DLLGuard] Baseline initialized (%zu DLLs)\n",
            baselineDlls.size()
        );
        OutputDebugStringW(message);
        return true;
    }

    // Baseline에 없던 DLL을 발견하면 해당 경로를 한 번만 출력
    void DetectNewModules()
    {
        if (!baselineInitialized)
            return;

        std::unordered_set<std::wstring> currentDlls;
        if (!CaptureLoadedDlls(currentDlls))
        {
            OutputDebugStringW(L"[DLLGuard] Module scan failed\n");
            return;
        }

        for (const auto& dllPath : currentDlls)
        {
            
            if (baselineDlls.find(dllPath) != baselineDlls.end())
                continue;

            // 이미 경고한 DLL이면 중복 출력 방지
            if (!reportedDlls.insert(dllPath).second)
                continue;

            const std::wstring message =
                L"[DLLGuard] New DLL detected: " + dllPath + L"\n";
            OutputDebugStringW(message.c_str());

            MessageBoxW(
                nullptr,
                L"게임 파일이 이상합니다.",
                L"EASILE 보안 경고",
                MB_OK | MB_ICONWARNING
            );

            // 새로운 DLL 탐지 시 게임 종료
            ExitProcess(1);
        }
    }
}
