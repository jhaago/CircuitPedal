#include "GenericCircuitProcessor.h"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace circuitpedal {
namespace {

std::atomic<std::uint32_t> requestedModeRaw {
    static_cast<std::uint32_t>(GenericProcessingMode::FourX)
};

GenericProcessingMode decodeMode(std::uint32_t raw) noexcept
{
    return raw == static_cast<std::uint32_t>(GenericProcessingMode::OneX)
        ? GenericProcessingMode::OneX
        : GenericProcessingMode::FourX;
}

} // namespace

void setGenericProcessingMode(GenericProcessingMode mode) noexcept
{
    requestedModeRaw.store(static_cast<std::uint32_t>(mode),
                           std::memory_order_relaxed);
}

GenericProcessingMode genericProcessingMode() noexcept
{
    return decodeMode(requestedModeRaw.load(std::memory_order_relaxed));
}

bool OversampledGenericCircuit::compile(const CircuitDefinition& definition,
                                        double hostSampleRate,
                                        std::string& error)
{
    compiled_ = false;
    lastSolveConverged_ = false;
    error.clear();

    if (!std::isfinite(hostSampleRate)
        || hostSampleRate < 8000.0
        || hostSampleRate > 96000.0)
    {
        error = "Generic processing supports host sample rates from 8 kHz to 96 kHz.";
        return false;
    }

    hostSampleRate_ = hostSampleRate;
    activeMode_ = genericProcessingMode();

    const double circuitRate = activeMode_ == GenericProcessingMode::FourX
        ? hostSampleRate_ * static_cast<double>(factor)
        : hostSampleRate_;
    if (!circuit_.compile(definition, circuitRate, error))
        return false;

    oversampler_.prepare();
    oversampler_.reset();
    compiled_ = true;
    lastSolveConverged_ = true;
    return true;
}

void OversampledGenericCircuit::reset() noexcept
{
    if (!compiled_)
        return;
    circuit_.reset();
    oversampler_.reset();
    lastSolveConverged_ = true;
}

float OversampledGenericCircuit::processSample(float input) noexcept
{
    if (!compiled_ || !std::isfinite(input))
        return 0.0f;

    if (activeMode_ == GenericProcessingMode::OneX)
    {
        const float output = circuit_.processSample(input);
        lastSolveConverged_ = circuit_.lastSolveConverged();
        return output;
    }

    std::array<double, factor> oversampledInput {};
    oversampler_.upsample(static_cast<double>(input), oversampledInput);

    double wet = 0.0;
    bool allConverged = true;
    for (int phase = 0; phase < factor; ++phase)
    {
        const float circuitOutput =
            circuit_.processSample(
                static_cast<float>(
                    oversampledInput[static_cast<std::size_t>(phase)]));
        allConverged = allConverged && circuit_.lastSolveConverged();
        (void)oversampler_.pushDownsample(
            static_cast<double>(circuitOutput),
            phase,
            wet);
    }

    lastSolveConverged_ = allConverged;
    if (!allConverged || !std::isfinite(wet))
        return 0.0f;

    return static_cast<float>(std::clamp(wet, -1.0, 1.0));
}

} // namespace circuitpedal
