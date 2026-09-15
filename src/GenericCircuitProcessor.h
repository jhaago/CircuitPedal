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

// Selects the mode used by subsequently compiled generic processors. The core
// defaults to FourX so the existing oversampling validation remains available;
// the macOS live-audio target explicitly selects OneX at startup because that
// is the physically validated realtime path. Change mode only while audio is
// stopped, before compiling a circuit.
void setGenericProcessingMode(GenericProcessingMode mode) noexcept;
GenericProcessingMode genericProcessingMode() noexcept;

// Aligns the dry side of a generic-circuit bypass crossfade with the active
// processing mode. OneX is sample-synchronous; FourX uses the FIR delay.
class GenericDryDelay {
public:
    static constexpr std::size_t capacity =
        Oversampler4x::wetDelayHostSamples + 1U;

    void reset() noexcept
    {
        samples_.fill(0.0);
        writeIndex_ = 0U;
    }

    double process(double input, std::size_t delayHostSamples) noexcept
    {
        const std::size_t delay = delayHostSamples < capacity
            ? delayHostSamples
            : capacity - 1U;
        const std::size_t readIndex =
            (writeIndex_ + capacity - delay) % capacity;
        const double output = delay == 0U ? input : samples_[readIndex];
        samples_[writeIndex_] = input;
        writeIndex_ = (writeIndex_ + 1U) % capacity;
        return output;
    }

private:
    std::array<double, capacity> samples_ {};
    std::size_t writeIndex_ = 0U;
};

// Generic nonlinear circuit wrapper. FourX remains available for engineering
// work, but the live macOS app currently selects OneX. The class name is kept
// for source compatibility with the existing audio engine while oversampling is
// being treated as experimental rather than production-ready.
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
    std::size_t processingDelayHostSamples() const noexcept
    {
        return activeMode_ == GenericProcessingMode::FourX
            ? delayHostSamples
            : 0U;
    }

private:
    GenericCircuit circuit_;
    Oversampler4x oversampler_;
    double hostSampleRate_ = 48000.0;
    GenericProcessingMode activeMode_ = GenericProcessingMode::FourX;
    bool compiled_ = false;
    bool lastSolveConverged_ = false;
};

} // namespace circuitpedal
