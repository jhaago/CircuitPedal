#include "Btdr2CircuitElement.h"

#include <cmath>

namespace circuitpedal {

bool Btdr2CircuitElement::prepare(double sampleRate,
                                  Btdr2Decay decay,
                                  std::string& error)
{
    prepared_ = false;
    decayType_ = decay;
    if (!model_.prepare(sampleRate, decay, error))
        return false;

    prepared_ = true;
    return true;
}

void Btdr2CircuitElement::reset() noexcept
{
    model_.reset();
}

Btdr2StereoSample Btdr2CircuitElement::advance(float inputVolts) noexcept
{
    if (!prepared_ || !std::isfinite(inputVolts))
        return {};
    return model_.processSample(inputVolts);
}

double Btdr2CircuitElement::inputCurrentAmps(double inputNodeVolts) noexcept
{
    if (!std::isfinite(inputNodeVolts))
        return 0.0;
    return inputNodeVolts / inputResistanceOhms;
}

double Btdr2CircuitElement::outputPortCurrentAmps(double nodeVolts,
                                                   double sourceVolts) noexcept
{
    if (!std::isfinite(nodeVolts) || !std::isfinite(sourceVolts))
        return 0.0;
    return (nodeVolts - sourceVolts) / outputResistanceOhms;
}

} // namespace circuitpedal
