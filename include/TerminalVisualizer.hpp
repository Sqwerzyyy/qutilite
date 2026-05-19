#pragma once

#include "Types.hpp"

#include <ostream>

namespace qutilite {

class TerminalVisualizer {
public:
    void print(const SimulationResult& result, std::ostream& out) const;
};

}  // namespace qutilite
