#pragma once

#include <cstddef>
#include <variant>

namespace circuitpedal {

struct ParameterDelta {
    std::size_t index = 0;
    int steps = 0;
};

struct ParameterSet {
    std::size_t index = 0;
    float normalizedValue = 0.0f;
};

struct MasterOutputDelta {
    int steps = 0;
};

struct MasterOutputSet {
    float normalizedValue = 0.0f;
};

struct BypassToggle {};

struct BypassSet {
    bool bypassed = false;
};

struct StompAction {
    std::size_t index = 0;
    bool pressed = false;
};

struct ExpressionValue {
    std::size_t index = 0;
    float normalizedValue = 0.0f;
};

using ControllerAction = std::variant<ParameterDelta,
                                      ParameterSet,
                                      MasterOutputDelta,
                                      MasterOutputSet,
                                      BypassToggle,
                                      BypassSet,
                                      StompAction,
                                      ExpressionValue>;

} // namespace circuitpedal
