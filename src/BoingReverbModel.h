#pragma once

#include "Btdr2Model.h"

#include <string>

namespace circuitpedal {

// First vertical-slice processor for the one-knob Boing-style reverb.
//
// The BTDR-2 block is modelled explicitly. The surrounding TL072/resistor/
// capacitor network is represented here as a compact behavioural send/return
// path so the reverb core can be validated before it is exposed as a generic
// .cpedal component. The next integration step moves those analogue sections
// back into GenericCircuit/MNA and leaves this class as a reference harness.
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
