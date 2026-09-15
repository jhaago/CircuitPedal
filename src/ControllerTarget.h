#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>

namespace circuitpedal {

constexpr std::size_t circuitStompParameterSlotCount = 5U;

struct ControllerParameterState {
    bool available = false;
    std::string label;
    float normalizedValue = 0.0f;
    float encoderStepNormalized = 0.01f;
};

struct TunerState {
    std::string noteName;
    double frequencyHz = 0.0;
    double centsOffset = 0.0;
    bool active = false;
};

struct ControllerHostState {
    std::string selectedPedalName;
    bool bypassed = false;
    std::array<ControllerParameterState, circuitStompParameterSlotCount> parameters {};
    float masterOutputNormalized = 0.0f;
    std::string presetName;
    std::optional<TunerState> tuner;
};

class ControllerTarget {
public:
    virtual ~ControllerTarget() = default;

    virtual std::string activePedalName() const = 0;
    virtual std::size_t parameterCount() const = 0;
    virtual ControllerParameterState parameterState(std::size_t index) const = 0;
    virtual bool setParameterNormalized(std::size_t index, float value) noexcept = 0;

    virtual float masterOutputNormalized() const noexcept = 0;
    virtual void setMasterOutputNormalized(float value) noexcept = 0;

    virtual bool bypassed() const noexcept = 0;
    virtual void setBypassed(bool value) noexcept = 0;
};

} // namespace circuitpedal
