#pragma once

#include <chrono>

class GameTime
{
public:
    void Reset();
    void Tick();
    float DeltaSeconds() const { return deltaSeconds_; }

private:
    std::chrono::steady_clock::time_point previousTime_{};
    float deltaSeconds_ = 0.0F;
};
