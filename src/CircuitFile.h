#pragma once

#include "GenericCircuit.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace circuitpedal {

enum class CircuitFileControlKind : std::uint8_t {
    Potentiometer,
    Switch
};

struct CircuitFileControl {
    std::string name;
    std::size_t potentiometerIndex = 0;
    double initialPosition = 0.5;
    // Additional pot sections mechanically/electrically linked to this one
    // GUI control (for example a dual-gang tone or mids control).
    std::vector<std::size_t> linkedPotentiometerIndices;

    CircuitFileControlKind kind = CircuitFileControlKind::Potentiometer;
    std::size_t switchIndex = 0;
    std::uint32_t switchPositionCount = 0;
    std::uint32_t initialSwitchPosition = 0;
    std::vector<std::string> switchPositionNames;
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
GenericOpAmpModel builtInOpAmpModel(const std::string& name, bool& ok) noexcept;
GenericNmosModel builtInNmosModel(const std::string& name, bool& ok) noexcept;
GenericDiodeModel builtInDiodeModel(const std::string& name, bool& ok) noexcept;

} // namespace circuitpedal
