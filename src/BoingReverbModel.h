#pragma once

#include "Btdr2Model.h"

#include <string>

namespace circuitpedal {

class BoingReverbModel {
public:
    bool prepare(double sampleRate, std::string& error);
    void reset() noexcept;

    void setReverb(float normalized) noexcept;
    float reverb() const noexcept { return reverb_; }

    float processSample(float inputFullScale) noexcept;

private:
    Btdr2Model brick_;
    double sampleRate_ = 48000.0;
    float reverb_ = 0.45f;

    float sendLowPassState_ = 0.0f;
    float wetLowPassState_ = 0.0f;
    float wetHighPassState_ = 0.0f;
    float wetHighPassPreviousInput_ = 0.0f;
    float sendLowPassCoefficient_ = 0.0f;
    float wetLowPassCoefficient_ = 0.0f;
    float wetHighPassCoefficient_ = 0.0f;
    bool prepared_ = false;
};

} // namespace circuitpedal
