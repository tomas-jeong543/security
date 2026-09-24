#pragma once
#include <chrono>
#include <fstream>
#include <iomanip>
#include <string>

class Logger final
{
public:
    void Write(double simulationTime, const char* category, const char* event,
               const char* entity, const std::string& detail) const
    {
        const auto wallTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::ofstream output("GameClient.log", std::ios::app);
        output << "[wall=" << wallTime << "][simulation=" << std::fixed << std::setprecision(3)
               << simulationTime << "][" << category << "][" << event << "][" << entity
               << "][" << detail << "]\n";
    }
};
