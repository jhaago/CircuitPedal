#pragma once

#include "GenericCircuit.h"

#include <cstddef>
#include <string>
#include <vector>

namespace circuitpedal {

struct CircuitFileControl {
    std::string name;
    std::size_t potentiometerIndex = 0;
    double initialPosition = 0.5;
};

struct CircuitFileDocument {
    std::string name;
    CircuitDefinition definition;
    std::vector<CircuitFileControl> controls;
};

double parseEngineeringValue(const std::string& text, bool& ok) noexcept;

bool parseCircuitFileText(const std::string& text,
                          CircuitFileDocument& document,
                          std::string& error);

bool loadCircuitFile(const std::string& path,
                     CircuitFileDocument& document,
                     std::string& error);

GenericNpnBjtModel builtInNpnModel(const std::string& name, bool& ok) noexcept;
GenericPnpBjtModel builtInPnpModel(const std::string& name, bool& ok) noexcept;
GenericNjfetModel builtInNjfetModel(const std::string& name, bool& ok) noexcept;
GenericDiodeModel builtInDiodeModel(const std::string& name, bool& ok) noexcept;

} // namespace circuitpedal
