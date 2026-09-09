#include "GenericCircuitProcessor.h"

#include <algorithm>
#include <cmath>

namespace circuitpedal {

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
        error = "4x generic oversampling supports host sample rates from 8 kHz to 96 kHz.";
        return false;
    }

    hostSampleRate_ = hostSampleRate;
    const double oversampledRate =
        hostSampleRate_ * static_cast<double>(factor);
    if (!circuit_.compile(definition, oversampledRate, error))
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
