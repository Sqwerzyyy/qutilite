#pragma once

#include <cstdint>
#include <string>

namespace qutilite {

enum class CommandMode {
    Run,
    Risk
};

struct SimulationConfig {
    std::string ticker = "AAPL";
    std::uint64_t simulations = 1'000'000;
    std::uint32_t days = 30;
    double confidence = 0.95;
    bool aiCalibration = false;
    CommandMode mode = CommandMode::Run;
};

struct MarketProfile {
    std::string ticker;
    double initialPrice = 100.0;
    double annualDrift = 0.08;
    double annualVolatility = 0.25;
};

struct SimulationResult {
    std::string ticker;
    double initialPrice = 0.0;
    double expectedFinalPrice = 0.0;
    double valueAtRisk = 0.0;
    double standardDeviation = 0.0;
    double throughputMillionPathsPerSecond = 0.0;
    double elapsedSeconds = 0.0;
    std::uint64_t simulations = 0;
    std::uint32_t days = 0;
    double confidence = 0.95;
    bool aiCalibration = false;
};

}  // namespace qutilite
