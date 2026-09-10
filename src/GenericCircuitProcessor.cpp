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
    outputNode_ = definition.outputNode();
    outputFullScalePerVolt_ = definition.outputFullScalePerVolt();

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

    double wetVolts = 0.0;
    bool allConverged = true;
    for (int phase = 0; phase < factor; ++phase)
    {
        // Advance the circuit state at the oversampled rate, but do not feed
        // GenericCircuit::processSample()'s already scaled/clamped digital
        // return value into the decimation filter. The anti-alias filter must
        // operate on the raw circuit-domain output, just as the dedicated
        // Distortion+ oversampled path does.
        (void)circuit_.processSample(
            static_cast<float>(
                oversampledInput[static_cast<std::size_t>(phase)]));
        allConverged = allConverged && circuit_.lastSolveConverged();

        const double circuitOutputVolts = circuit_.nodeVoltage(outputNode_);
        (void)oversampler_.pushDownsample(
            circuitOutputVolts,
            phase,
            wetVolts);
    }

    lastSolveConverged_ = allConverged;
    if (!allConverged || !std::isfinite(wetVolts))
        return 0.0f;

    const double wetDigital = wetVolts * outputFullScalePerVolt_;
    if (!std::isfinite(wetDigital))
        return 0.0f;

    return static_cast<float>(std::clamp(wetDigital, -1.0, 1.0));
}

} // namespace circuitpedal
