#pragma once

#include "DistortionPlusModel.h"
#include "GenericCircuit.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace circuitpedal {

enum class GenericProcessingMode : std::uint8_t {
    OneX = 1,
    FourX = 4
};

struct GenericProcessorDiagnostics {
    GenericProcessingMode requestedMode = GenericProcessingMode::OneX;
    GenericProcessingMode activeMode = GenericProcessingMode::OneX;
    std::uint64_t hostSamplesProcessed = 0;
    std::uint64_t hostSolveFailures = 0;
    std::uint64_t subSolveFailures = 0;
    std::uint64_t timedSamples = 0;
    std::uint64_t sampleBudgetMisses = 0;
    double maximumProcessMicroseconds = 0.0;
    double sampleBudgetMicroseconds = 0.0;
};

// Diagnostic mode is intentionally selected before audio starts. Changing the
// requested mode while audio is running takes effect on the next compile/start.
void setGenericProcessingModeForDiagnostics(GenericProcessingMode mode) noexcept;
GenericProcessingMode genericProcessingModeForDiagnostics() noexcept;
void resetGenericProcessorDiagnostics() noexcept;
GenericProcessorDiagnostics genericProcessorDiagnostics() noexcept;

// Generic nonlinear circuit wrapper. In normal 4x mode, the circuit itself is
// solved at 4x the host rate so capacitor companions and nonlinear junctions see
// the oversampled timestep. The temporary 1x diagnostic mode reproduces the
// pre-V0.8 live GenericCircuit processing path for physical A/B testing.
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

    bool setSwitchPosition(std::size_t index, std::uint32_t position) noexcept
    {
        return circuit_.setSwitchPosition(index, position);
    }
    std::uint32_t switchPosition(std::size_t index) const noexcept
    {
        return circuit_.switchPosition(index);
    }
    std::size_t switchCount() const noexcept
    {
        return circuit_.switchCount();
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
    GenericProcessingMode activeProcessingMode() const noexcept { return activeMode_; }

private:
    GenericCircuit circuit_;
    Oversampler4x oversampler_;
    CircuitNode outputNode_ = circuitGround;
    double outputFullScalePerVolt_ = 1.0;
    double hostSampleRate_ = 48000.0;
    GenericProcessingMode activeMode_ = GenericProcessingMode::OneX;
    bool compiled_ = false;
    bool lastSolveConverged_ = false;
};

} // namespace circuitpedal
