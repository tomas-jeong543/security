#include "CodeIntegrity.h"
#include "IntegrityGuard.h"
#include "Logger.h"
#include <algorithm>
#include <chrono>
#include <cstdio>

namespace CodeIntegrity
{
namespace
{
struct View
{
    HANDLE mapping = nullptr;
    const std::uint8_t* bytes = nullptr;
    ~View() { if (bytes) UnmapViewOfFile(bytes); if (mapping) CloseHandle(mapping); }
};
}

Region::Region(std::uintptr_t entry, const char* name) : name_(name)
{
    wchar_t path[32768]{};
    const DWORD length = GetModuleFileNameW(nullptr, path, _countof(path));
    if (!length || length >= _countof(path)) return;
    file_ = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file_ == INVALID_HANDLE_VALUE) return;
    View view;
    view.mapping = CreateFileMappingW(file_, nullptr, PAGE_READONLY | SEC_IMAGE_NO_EXECUTE, 0, 0, nullptr);
    if (!view.mapping) return;
    view.bytes = static_cast<const std::uint8_t*>(MapViewOfFile(view.mapping, FILE_MAP_READ, 0, 0, 0));
    if (!view.bytes) return;
    const auto diskDos = reinterpret_cast<const IMAGE_DOS_HEADER*>(view.bytes);
    if (diskDos->e_magic != IMAGE_DOS_SIGNATURE || diskDos->e_lfanew <= 0) return;
    const auto disk = reinterpret_cast<const IMAGE_NT_HEADERS64*>(view.bytes + diskDos->e_lfanew);
    if (disk->Signature != IMAGE_NT_SIGNATURE || disk->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        disk->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return;
    const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
    const auto memoryDos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto memory = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + memoryDos->e_lfanew);
    const auto size = disk->OptionalHeader.SizeOfImage;
    if (memory->FileHeader.TimeDateStamp != disk->FileHeader.TimeDateStamp ||
        memory->OptionalHeader.SizeOfImage != size || entry < base || entry - base >= size) return;
    DWORD rva = static_cast<DWORD>(entry - base);
    // /INCREMENTAL:NO is required: do not follow a mutable runtime jump to choose a baseline.
    const auto& table = disk->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    if (table.VirtualAddress >= size || table.Size > size - table.VirtualAddress) return;
    const auto functions = reinterpret_cast<const RUNTIME_FUNCTION*>(view.bytes + table.VirtualAddress);
    DWORD end = 0;
    for (DWORD i = 0; i < table.Size / sizeof(RUNTIME_FUNCTION); ++i)
        if (functions[i].BeginAddress == rva) { end = functions[i].EndAddress; break; }
    if (end <= rva || end > size) return;
    bool executable = false;
    const auto sections = IMAGE_FIRST_SECTION(disk);
    for (WORD i = 0; i < disk->FileHeader.NumberOfSections; ++i)
        if ((sections[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) && rva >= sections[i].VirtualAddress &&
            end <= static_cast<std::uint64_t>(sections[i].VirtualAddress) + sections[i].Misc.VirtualSize)
            executable = true;
    if (!executable) return;
    std::vector<std::uint8_t> expected(view.bytes + rva, view.bytes + end);
    // SEC_IMAGE_NO_EXECUTE does not apply ASLR relocations. Rebase expected DIR64 operands.
    const auto delta = base - disk->OptionalHeader.ImageBase;
    const auto& reloc = disk->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (reloc.VirtualAddress >= size || reloc.Size > size - reloc.VirtualAddress) return;
    DWORD offset = 0;
    while (offset < reloc.Size)
    {
        if (reloc.Size - offset < sizeof(IMAGE_BASE_RELOCATION)) return;
        const auto block = reinterpret_cast<const IMAGE_BASE_RELOCATION*>(view.bytes + reloc.VirtualAddress + offset);
        if (block->SizeOfBlock < sizeof(*block) || block->SizeOfBlock > reloc.Size - offset) return;
        const auto entries = reinterpret_cast<const WORD*>(block + 1);
        for (DWORD i = 0; i < (block->SizeOfBlock - sizeof(*block)) / sizeof(WORD); ++i)
        {
            const WORD type = entries[i] >> 12;
            if (type == IMAGE_REL_BASED_ABSOLUTE) continue;
            const std::uint64_t target = static_cast<std::uint64_t>(block->VirtualAddress) + (entries[i] & 0xfff);
            if (target >= end || target + sizeof(std::uint64_t) <= rva) continue;
            if (type != IMAGE_REL_BASED_DIR64 || target < rva || target + sizeof(std::uint64_t) > end) return;
            std::uint64_t value;
            std::memcpy(&value, expected.data() + target - rva, sizeof(value));
            value += delta;
            std::memcpy(expected.data() + target - rva, &value, sizeof(value));
        }
        offset += block->SizeOfBlock;
    }
    address_ = base + rva;
    expected_ = std::move(expected);
    char detail[192];
    sprintf_s(detail, "rva=0x%lX;size=0x%lX;expected_crc32=%08X;source=exe_disk", rva, end - rva,
        IntegrityGuard::CalculateCRC32(expected_.data(), expected_.size()));
    Logger{}.Write(0, "SECURITY", "BASELINE", name_, detail);
}

Region::~Region() { if (file_ != INVALID_HANDLE_VALUE) CloseHandle(file_); }

bool Region::Verify() const
{
    if (!Ready()) return false;
    std::vector<std::uint8_t> actual(expected_.size());
    SIZE_T read = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address_),
        actual.data(), actual.size(), &read) || read != actual.size()) return false;
    // Keep the existing CRC32 algorithm, and compare bytes as well to reject CRC collisions.
    return IntegrityGuard::CalculateCRC32(actual.data(), actual.size()) ==
        IntegrityGuard::CalculateCRC32(expected_.data(), expected_.size()) && actual == expected_;
}

[[noreturn]] void Fail(const char* defense, const char* detail, UINT exitCode)
{
    Logger{}.Write(0, "SECURITY", "DETECTED", defense, detail);
    OutputDebugStringA(defense);
    OutputDebugStringA(": code integrity failure\n");
    // Terminate without a modal wait, including when A is blocked in MessageBox.
    ExitProcess(exitCode);
}

DllScanWatchdog::DllScanWatchdog()
    : region_(Address(&IntegrityGuard::DetectNewModules), "HK01_C_DetectNewModules")
{
    if (!region_.Verify()) Fail("HK01_C", "baseline_or_initial_code_invalid", 3);
    worker_ = std::thread([this]
    {
        Logger{}.Write(0, "SECURITY", "WATCHDOG_STARTED", "HK01_C", "interval_ms=100");
        while (!stop_.load())
        {
            if (!region_.Verify()) Fail("HK01_C", "DetectNewModules_modified_or_unreadable", 3);
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
}
DllScanWatchdog::~DllScanWatchdog()
{
    stop_.store(true);
    if (worker_.joinable()) worker_.join();
}
}
