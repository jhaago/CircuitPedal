#pragma once

#include "DistortionPlusModel.h"
#include "GenericCircuit.h"

#include <array>
#include <cstddef>
#include <string>

namespace circuitpedal {

// Four-times oversampled wrapper for arbitrary nonlinear circuits. The circuit
// itself is solved at 4x the host rate so capacitor companions and nonlinear
// junctions see the oversampled timestep rather than a post-effect resampler.
class OversampledGenericCircuit {
public:
    static constexpr int factor = Oversampler4x::factor;
    static constexpr std::size_t delayHostSamples =
        Oversampler4x::wetDelayHostSamples;

    bool compile(const CircuitDefinition& definition,
                 double hostSampleRate,
                 std::string& error);
    void reset() noexcept;

    float processSample(float input) noexcept;

    bool setPotentiometerPosition(std::size_t index, double normalized) noexcept
    {
        return circuit_.setPotentiometerPosition(index, normalized);
    }
    double potentiometerPosition(std::size_t index) const noexcept
    {
        return circuit_.potentiometerPosition(index);
    }
    std::size_t potentiometerCount() const noexcept
    {
        return circuit_.potentiometerCount();
    }

    double nodeVoltage(CircuitNode node) const noexcept
    {
        return circuit_.nodeVoltage(node);
    }
    bool lastSolveConverged() const noexcept
    {
        return lastSolveConverged_;
    }

    double hostSampleRate() const noexcept { return hostSampleRate_; }
    double circuitSampleRate() const noexcept { return circuit_.sampleRate(); }

private:
    GenericCircuit circuit_;
    Oversampler4x oversampler_;
    double hostSampleRate_ = 48000.0;
    bool compiled_ = false;
    bool lastSolveConverged_ = false;
};

} // namespace circuitpedal
