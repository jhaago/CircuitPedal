#pragma once

#include "ControllerTarget.h"

namespace circuitpedal {

class MacAudioEngine;

class MacControllerTarget final : public ControllerTarget {
public:
    explicit MacControllerTarget(MacAudioEngine& engine) noexcept;

    std::string activePedalName() const override;
    std::size_t parameterCount() const override;
    ControllerParameterState parameterState(std::size_t index) const override;
    bool setParameterNormalized(std::size_t index, float value) noexcept override;

    float masterOutputNormalized() const noexcept override;
    void setMasterOutputNormalized(float value) noexcept override;

    bool bypassed() const noexcept override;
    void setBypassed(bool value) noexcept override;

private:
    MacAudioEngine& engine_;
};

} // namespace circuitpedal
