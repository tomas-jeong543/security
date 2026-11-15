#include "GameWorld.h"
#include "IntegrityGuard.h"
#include "CodeIntegrity.h"
#include "Logger.h"
#include <iostream>
#include <string>
#include <TlHelp32.h>

namespace
{
[[noreturn]] void UnexpectedDetour() { ExitProcess(90); }
void InstallEntryDetour(std::uintptr_t target)
{
    // Same x64 absolute jump shape as the original SpawnProjectile payload.
    std::uint8_t jump[14] = {0xff, 0x25, 0, 0, 0, 0};
    const auto destination = CodeIntegrity::Address(&UnexpectedDetour);
    std::memcpy(jump + 6, &destination, sizeof(destination));
    DWORD oldProtection = 0;
    if (!VirtualProtect(reinterpret_cast<void*>(target), sizeof(jump), PAGE_EXECUTE_READWRITE, &oldProtection))
        ExitProcess(91);
    std::memcpy(reinterpret_cast<void*>(target), jump, sizeof(jump));
    DWORD unused;
    if (!VirtualProtect(reinterpret_cast<void*>(target), sizeof(jump), oldProtection, &unused) ||
        !FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(target), sizeof(jump)) ||
        std::memcmp(reinterpret_cast<const void*>(target), jump, sizeof(jump)) != 0) ExitProcess(91);
    Logger{}.Write(0, "TEST", "PATCH_INSTALLED", "HK01", "entry_ff25_14_bytes;readback_verified=1");
}
}

struct GameWorldTests
{
    static std::uintptr_t Spawn() { return CodeIntegrity::Address(&GameWorld::SpawnProjectile); }
    static void Fire(GameWorld& world)
    {
        world.CheckedSpawnProjectile(Team::Blue, {800,720}, {1,0}, ProjectileKind::Basic, 5);
    }
};

int main(int argc, char** argv)
{
    if (argc != 2) return 99;
    const std::string mode = argv[1];
    if (mode == "B-preinit") InstallEntryDetour(GameWorldTests::Spawn());
    GameWorld world;
    if (mode == "normal")
    {
        CodeIntegrity::DllScanWatchdog watchdog;
        world.Restart();
        GameWorldTests::Fire(world);
        if (!IntegrityGuard::VerifyUpdateIntegrity() || !GameWorld::VerifyInitializeCode() ||
            world.ProjectileCount() != 1) return 10;
        CodeIntegrity::Region invalid(1, "InvalidAddress");
        if (invalid.Verify()) return 11;
        std::cout << "normal baseline, CRC, projectile, watchdog and invalid-address PASS\n";
        return 0;
    }
    if (mode == "B")
    {
        InstallEntryDetour(GameWorldTests::Spawn());
        GameWorldTests::Fire(world);
        return 92;
    }
    if (mode == "C-preinit") InstallEntryDetour(CodeIntegrity::Address(&IntegrityGuard::DetectNewModules));
    if (mode == "C" || mode == "C-preinit")
    {
        CodeIntegrity::DllScanWatchdog watchdog;
        InstallEntryDetour(CodeIntegrity::Address(&IntegrityGuard::DetectNewModules));
        Sleep(2000);
        return 93;
    }
    if (mode == "A" || mode == "A-C-blocked")
    {
        CodeIntegrity::DllScanWatchdog watchdog;
        wchar_t executable[32768];
        GetModuleFileNameW(nullptr, executable, _countof(executable));
        std::wstring dll(executable);
        dll = dll.substr(0, dll.find_last_of(L"\\/") + 1) + L"Hk01Probe.dll";
        // Warm normal loader/Toolhelp dependencies, then unload the benign
        // probe and assert its absence before capturing the actual baseline.
        // Keep dependency references so the second load adds only the probe.
        const HMODULE warmProbe = LoadLibraryW(dll.c_str());
        if (!warmProbe) return 95;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
        if (snapshot == INVALID_HANDLE_VALUE) return 95;
        MODULEENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        if (Module32FirstW(snapshot, &entry)) do {
            if (entry.hModule != warmProbe) {
                HMODULE retained = nullptr;
                if (!GetModuleHandleExW(0, entry.szExePath, &retained)) return 95;
            }
        } while (Module32NextW(snapshot, &entry));
        CloseHandle(snapshot);
        if (!FreeLibrary(warmProbe) || GetModuleHandleW(dll.c_str())) return 95;
        Logger{}.Write(0, "TEST", "PROBE_ABSENT_BEFORE_BASELINE", "HK01_A", "verified=1");
        if (!IntegrityGuard::InitializeDllBaseline()) return 95;
        if (!LoadLibraryW(dll.c_str())) return 96;
        if (mode == "A-C-blocked")
        {
            std::thread([] {
                Sleep(500);
                InstallEntryDetour(CodeIntegrity::Address(&IntegrityGuard::DetectNewModules));
            }).detach();
        }
        IntegrityGuard::DetectNewModules();
        return 97;
    }
    if (mode == "CRC")
    {
        InstallEntryDetour(CodeIntegrity::Address(&GameWorld::Update));
        if (IntegrityGuard::VerifyUpdateIntegrity()) return 94;
        std::cout << "modified Update CRC rejected PASS\n";
        return 0;
    }
    return 98;
}
