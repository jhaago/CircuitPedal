#include "Btdr2Model.h"

#include <algorithm>
#include <cmath>

namespace circuitpedal {
namespace {

constexpr double pi = 3.14159265358979323846;
constexpr float btdrPeakInputVolts = 1.5f;
constexpr float outputGain = 0.70710678f; // approximately -3 dB per output

float softLimit(float value, float limit) noexcept
{
    if (!(limit > 0.0f))
        return 0.0f;
    return limit * std::tanh(value / limit);
}

} // namespace

double Btdr2Model::t60ForDecay(Btdr2Decay decay) noexcept
{
    switch (decay)
    {
    case Btdr2Decay::Short:
        return 2.0;
    case Btdr2Decay::Medium:
        return 2.5;
    case Btdr2Decay::Long:
        return 2.85;
    }
    return 2.5;
}

std::size_t Btdr2Model::delaySamples(double sampleRate,
                                     double milliseconds) noexcept
{
    const double requested = sampleRate * milliseconds * 0.001;
    return static_cast<std::size_t>(
        std::max(1.0, std::round(requested)));
}

float Btdr2Model::feedbackForT60(double delaySeconds,
                                 double t60Seconds) noexcept
{
    if (!(delaySeconds > 0.0) || !(t60Seconds > 0.0))
        return 0.0f;

    // Repeating the delay N times over T60 should reduce low-frequency
    // amplitude to 0.001 (-60 dB).
    const double gain = std::pow(0.001, delaySeconds / t60Seconds);
    return static_cast<float>(std::clamp(gain, 0.0, 0.995));
}

void Btdr2Model::prepareLine(DelayLine& line,
                             double delayMilliseconds,
                             double t60Seconds)
{
    const std::size_t samples = delaySamples(sampleRate_, delayMilliseconds);
    line.buffer.assign(samples, 0.0f);
    line.writeIndex = 0;
    line.dampingState = 0.0f;
    line.feedback = feedbackForT60(
        static_cast<double>(samples) / sampleRate_,
        t60Seconds);
}

bool Btdr2Model::prepare(double sampleRate,
                         Btdr2Decay decay,
                         std::string& error)
{
    error.clear();
    prepared_ = false;

    if (!std::isfinite(sampleRate)
        || sampleRate < 8000.0
        || sampleRate > 384000.0)
    {
        error = "BTDR-2 sample rate must be from 8 kHz to 384 kHz.";
        return false;
    }

    sampleRate_ = sampleRate;
    decayType_ = decay;
    nominalT60Seconds_ = t60ForDecay(decay);

    // Three decorrelated recirculating delays deliberately evoke the clustered
    // PT2399-family topology associated with Belton bricks without pretending
    // that the potted internal schematic is fully known. The unequal times
    // suppress obvious metallic periodicity while retaining the slightly
    // grainy/spring-like quality expected from this module family.
    prepareLine(earlyA_, 43.7, nominalT60Seconds_);
    prepareLine(earlyB_, 61.3, nominalT60Seconds_);
    prepareLine(late_, 79.9, nominalT60Seconds_);

    // Internal bandwidth is intentionally limited rather than studio-clean.
    // A one-pole 4.2 kHz damping stage is applied inside each recirculation.
    const double dampingHz = 4200.0;
    dampingCoefficient_ = static_cast<float>(
        1.0 - std::exp(-2.0 * pi * dampingHz / sampleRate_));

    reset();
    prepared_ = true;
    return true;
}

void Btdr2Model::reset() noexcept
{
    const auto clearLine = [](DelayLine& line) noexcept {
        std::fill(line.buffer.begin(), line.buffer.end(), 0.0f);
        line.writeIndex = 0;
        line.dampingState = 0.0f;
    };

    clearLine(earlyA_);
    clearLine(earlyB_);
    clearLine(late_);
    inputDcState_ = 0.0f;
    previousInput_ = 0.0f;
}

float Btdr2Model::processLine(DelayLine& line,
                              float input,
                              float dampingCoefficient) noexcept
{
    if (line.buffer.empty())
        return 0.0f;

    const float delayed = line.buffer[line.writeIndex];
    line.dampingState += dampingCoefficient * (delayed - line.dampingState);

    // Mild saturation keeps large recirculating peaks bounded and gives the
    // model the non-pristine quality of the real low-cost digital brick.
    const float writeValue = softLimit(
        input + line.feedback * line.dampingState,
        btdrPeakInputVolts * 1.35f);

    line.buffer[line.writeIndex] = writeValue;
    ++line.writeIndex;
    if (line.writeIndex >= line.buffer.size())
        line.writeIndex = 0;

    return line.dampingState;
}

Btdr2StereoSample Btdr2Model::processSample(float inputVolts) noexcept
{
    if (!prepared_ || !std::isfinite(inputVolts))
        return {};

    // The published BTDR-2 interface is AC-coupled. Remove very slow DC before
    // the delay network, then softly respect the approximate 1.5 V peak input
    // envelope instead of introducing an abrupt digital hard clip.
    const float dcCoefficient = static_cast<float>(
        1.0 - std::exp(-2.0 * pi * 8.0 / sampleRate_));
    inputDcState_ += dcCoefficient * (inputVolts - inputDcState_);
    const float acInput = softLimit(
        inputVolts - inputDcState_,
        btdrPeakInputVolts);

    // Two early paths feed a third later path. A small cross-feed from the late
    // field makes the tail denser without collapsing the stereo decorrelation.
    const float latePrevious = late_.dampingState;
    const float earlyA = processLine(
        earlyA_, acInput + 0.08f * latePrevious, dampingCoefficient_);
    const float earlyB = processLine(
        earlyB_, acInput - 0.07f * latePrevious, dampingCoefficient_);
    const float lateInput = 0.47f * earlyA + 0.53f * earlyB;
    const float late = processLine(late_, lateInput, dampingCoefficient_);

    previousInput_ = acInput;

    // Each real BTDR-2 output is specified around -3 dB. Keep the stereo taps
    // slightly different so summing them to mono remains dense rather than
    // simply doubling one identical signal.
    Btdr2StereoSample result;
    result.left = outputGain * (0.58f * earlyA + 0.42f * late);
    result.right = outputGain * (0.56f * earlyB + 0.44f * late);
    return result;
}

} // namespace circuitpedal
