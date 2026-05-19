#pragma once

#include "Types.hpp"

namespace qutilite {

class LocalSimulationEngine {
public:
    SimulationResult run(const SimulationConfig& config) const;

private:
    MarketProfile resolveMarketProfile(const SimulationConfig& config) const;
};

}  // namespace qutilite
