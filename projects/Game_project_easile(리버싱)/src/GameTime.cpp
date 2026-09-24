#include "GameTime.h"

void GameTime::Reset()
{
    previousTime_ = std::chrono::steady_clock::now();
    deltaSeconds_ = 0.0F;
}

void GameTime::Tick()
{
    const auto currentTime = std::chrono::steady_clock::now();
    deltaSeconds_ = std::chrono::duration<float>(currentTime - previousTime_).count();
    previousTime_ = currentTime;
}
