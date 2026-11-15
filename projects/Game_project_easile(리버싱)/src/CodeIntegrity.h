#pragma once
#include <Windows.h>
#include <cstdint>
#include <vector>
#include <atomic>
#include <thread>
#include <cstring>

namespace CodeIntegrity
{
// MSVC x64 single-inheritance member pointers contain one code address.
template<class T> std::uintptr_t Address(T pointer)
{
    static_assert(sizeof(pointer) == sizeof(std::uintptr_t), "Unsupported member ABI");
    std::uintptr_t address = 0;
    std::memcpy(&address, &pointer, sizeof(address));
    return address;
}

// A baseline comes from this executable on disk, never from possibly hooked RAM.
// The open handle denies replacement/writes for the lifetime of the snapshot.
class Region final
{
public:
    Region(std::uintptr_t entry, const char* name);
    ~Region();
    Region(const Region&) = delete;
    Region& operator=(const Region&) = delete;
    bool Verify() const;
    bool Ready() const { return !expected_.empty(); }
private:
    HANDLE file_ = INVALID_HANDLE_VALUE;
    std::uintptr_t address_ = 0;
    std::vector<std::uint8_t> expected_;
    const char* name_;
};

[[noreturn]] void Fail(const char* defense, const char* detail, UINT exitCode);

// C has its own thread, baseline, clock and result. It never calls the scanner.
class DllScanWatchdog final
{
public:
    DllScanWatchdog();
    ~DllScanWatchdog();
    DllScanWatchdog(const DllScanWatchdog&) = delete;
    DllScanWatchdog& operator=(const DllScanWatchdog&) = delete;
private:
    Region region_;
    std::atomic<bool> stop_{false};
    std::thread worker_;
};
}
