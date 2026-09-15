#include "MacControllerTarget.h"

#include "MacAudioEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace circuitpedal {
namespace {

constexpr float defaultEncoderStep = 0.01f;
constexpr float minimumMasterDb = -18.0f;
constexpr float maximumMasterDb = 6.0f;
constexpr std::size_t builtInControlCount = 2U;

float clampNormalized(float value) noexcept
{
    return std::clamp(value, 0.0f, 1.0f);
}

} // namespace

MacControllerTarget::MacControllerTarget(MacAudioEngine& engine) noexcept
    : engine_(engine)
{
}

std::string MacControllerTarget::activePedalName() const
{
    return engine_.activeModelName();
}

std::size_t MacControllerTarget::parameterCount() const
{
    return engine_.usingCircuitFile()
        ? engine_.circuitControls().size()
        : builtInControlCount;
}

ControllerParameterState MacControllerTarget::parameterState(std::size_t index) const
{
    ControllerParameterState state;
    if (!engine_.usingCircuitFile())
    {
        if (index >= builtInControlCount)
            return state;
        state.available = true;
        state.label = index == 0U ? "Distortion" : "Output";
        state.normalizedValue = index == 0U ? engine_.distortion() : engine_.output();
        state.encoderStepNormalized = defaultEncoderStep;
        return state;
    }

    const auto controls = engine_.circuitControls();
    if (index >= controls.size())
        return state;

    const auto& control = controls[index];
    state.available = true;
    state.label = control.name;
    state.normalizedValue = engine_.circuitControl(index);
    if (control.kind == CircuitFileControlKind::Switch)
    {
        const std::uint32_t positions =
            std::max<std::uint32_t>(2U, control.switchPositionCount);
        state.encoderStepNormalized = 1.0f / static_cast<float>(positions - 1U);
    }
    else
    {
        state.encoderStepNormalized = defaultEncoderStep;
    }
    return state;
}

bool MacControllerTarget::setParameterNormalized(std::size_t index, float value) noexcept
{
    if (!std::isfinite(value))
        return false;
    const float normalized = clampNormalized(value);
    if (engine_.usingCircuitFile())
        return engine_.setCircuitControl(index, normalized);

    if (index == 0U)
    {
        engine_.setDistortion(normalized);
        return true;
    }
    if (index == 1U)
    {
        engine_.setOutput(normalized);
        return true;
    }
    return false;
}

float MacControllerTarget::masterOutputNormalized() const noexcept
{
    const float normalized = (engine_.masterOutputDb() - minimumMasterDb)
        / (maximumMasterDb - minimumMasterDb);
    return clampNormalized(normalized);
}

void MacControllerTarget::setMasterOutputNormalized(float value) noexcept
{
    if (!std::isfinite(value))
        return;
    const float normalized = clampNormalized(value);
    engine_.setMasterOutputDb(
        minimumMasterDb + normalized * (maximumMasterDb - minimumMasterDb));
}

bool MacControllerTarget::bypassed() const noexcept
{
    return engine_.bypassed();
}

void MacControllerTarget::setBypassed(bool value) noexcept
{
    engine_.setBypass(value);
}

} // namespace circuitpedal
