#include "BoingReverbModel.h"

#include <algorithm>
#include <cmath>

namespace circuitpedal {
namespace {

constexpr double pi = 3.14159265358979323846;
constexpr float guitarVoltsPerFullScale = 0.20f;
constexpr float voltsToFullScale = 1.0f / guitarVoltsPerFullScale;

float onePoleCoefficient(double sampleRate, double cutoffHz) noexcept
{
    return static_cast<float>(
        1.0 - std::exp(-2.0 * pi * cutoffHz / sampleRate));
}

} // namespace

bool BoingReverbModel::prepare(double sampleRate, std::string& error)
{
    error.clear();
    prepared_ = false;

    if (!std::isfinite(sampleRate)
        || sampleRate < 8000.0
        || sampleRate > 384000.0)
    {
        error = "Boing sample rate must be from 8 kHz to 384 kHz.";
        return false;
    }

    sampleRate_ = sampleRate;
    if (!brick_.prepare(sampleRate_, Btdr2Decay::Medium, error))
        return false;

    sendLowPassCoefficient_ = onePoleCoefficient(sampleRate_, 7200.0);
    wetLowPassCoefficient_ = onePoleCoefficient(sampleRate_, 6200.0);
    wetHighPassCoefficient_ = onePoleCoefficient(sampleRate_, 85.0);

    reset();
    prepared_ = true;
    return true;
}

void BoingReverbModel::reset() noexcept
{
    brick_.reset();
    sendLowPassState_ = 0.0f;
    wetLowPassState_ = 0.0f;
    wetHighPassState_ = 0.0f;
    wetHighPassPreviousInput_ = 0.0f;
}

void BoingReverbModel::setReverb(float normalized) noexcept
{
    if (!std::isfinite(normalized))
        return;
    reverb_ = std::clamp(normalized, 0.0f, 1.0f);
}

float BoingReverbModel::processSample(float inputFullScale) noexcept
{
    if (!prepared_ || !std::isfinite(inputFullScale))
        return 0.0f;

    const float dry = std::clamp(inputFullScale, -1.0f, 1.0f);
    const float inputVolts = dry * guitarVoltsPerFullScale;

    sendLowPassState_ +=
        sendLowPassCoefficient_ * (inputVolts - sendLowPassState_);

    const float sendVolts = 0.82f * sendLowPassState_;
    const Btdr2StereoSample wetStereo = brick_.processSample(sendVolts);
    const float wetMonoVolts = 0.5f * (wetStereo.left + wetStereo.right);

    wetLowPassState_ +=
        wetLowPassCoefficient_ * (wetMonoVolts - wetLowPassState_);

    const float highPassInput = wetLowPassState_;
    const float highPassAlpha = 1.0f - wetHighPassCoefficient_;
    wetHighPassState_ = highPassAlpha
        * (wetHighPassState_ + highPassInput - wetHighPassPreviousInput_);
    wetHighPassPreviousInput_ = highPassInput;

    const float wetFullScale = wetHighPassState_ * voltsToFullScale;
    const float wetAmount = reverb_ * reverb_;
    const float output = dry + 1.35f * wetAmount * wetFullScale;

    return std::clamp(output, -1.0f, 1.0f);
}

} // namespace circuitpedal
