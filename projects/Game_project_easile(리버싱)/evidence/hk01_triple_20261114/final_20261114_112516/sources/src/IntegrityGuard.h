#pragma once

#include <cstdint>
#include <cstddef>

namespace IntegrityGuard
{
    // PATCH-01: 기존 Update 함수 무결성 검증
    __declspec(noinline) bool VerifyUpdateIntegrity();

    uint32_t CalculateCRC32(
        const uint8_t* data,
        size_t size
    );

    // PATCH-02: DLL Baseline 검사
    bool InitializeDllBaseline();
    void DetectNewModules();
}
