#include "ControllerRouter.h"

#include <algorithm>
#include <cmath>
#include <type_traits>

namespace circuitpedal {
namespace {

constexpr float defaultMasterEncoderStep = 0.01f;

float boundedNormalized(float value) noexcept
{
    return std::clamp(value, 0.0f, 1.0f);
}

bool validParameterState(const ControllerParameterState& state) noexcept
{
    return state.available
        && std::isfinite(state.normalizedValue)
        && std::isfinite(state.encoderStepNormalized)
        && state.encoderStepNormalized > 0.0f;
}

} // namespace

ControllerRouter::ControllerRouter(ControllerTarget& target) noexcept
    : target_(target)
{
}

ControllerRouteResult ControllerRouter::route(const ControllerAction& action) noexcept
{
    try
    {
        return std::visit([this](const auto& command) -> ControllerRouteResult {
            using Command = std::decay_t<decltype(command)>;

            if constexpr (std::is_same_v<Command, ParameterDelta>)
            {
                if (command.index >= circuitStompParameterSlotCount
                    || command.index >= target_.parameterCount())
                {
                    return {};
                }
                const ControllerParameterState state = target_.parameterState(command.index);
                if (!validParameterState(state))
                    return {};
                const float requested = state.normalizedValue
                    + static_cast<float>(command.steps) * state.encoderStepNormalized;
                if (!std::isfinite(requested))
                    return {};
                const float value = boundedNormalized(requested);
                if (value == state.normalizedValue)
                    return { true, false };
                if (!target_.setParameterNormalized(command.index, value))
                    return {};
                return { true, true };
            }
            else if constexpr (std::is_same_v<Command, ParameterSet>)
            {
                if (command.index >= circuitStompParameterSlotCount
                    || command.index >= target_.parameterCount()
                    || !std::isfinite(command.normalizedValue))
                {
                    return {};
                }
                const ControllerParameterState state = target_.parameterState(command.index);
                if (!state.available || !std::isfinite(state.normalizedValue))
                    return {};
                const float value = boundedNormalized(command.normalizedValue);
                if (value == state.normalizedValue)
                    return { true, false };
                if (!target_.setParameterNormalized(command.index, value))
                    return {};
                return { true, true };
            }
            else if constexpr (std::is_same_v<Command, MasterOutputDelta>)
            {
                const float current = target_.masterOutputNormalized();
                if (!std::isfinite(current))
                    return {};
                const float requested = current
                    + static_cast<float>(command.steps) * defaultMasterEncoderStep;
                if (!std::isfinite(requested))
                    return {};
                const float value = boundedNormalized(requested);
                if (value == current)
                    return { true, false };
                target_.setMasterOutputNormalized(value);
                return { true, true };
            }
            else if constexpr (std::is_same_v<Command, MasterOutputSet>)
            {
                if (!std::isfinite(command.normalizedValue))
                    return {};
                const float current = target_.masterOutputNormalized();
                if (!std::isfinite(current))
                    return {};
                const float value = boundedNormalized(command.normalizedValue);
                if (value == current)
                    return { true, false };
                target_.setMasterOutputNormalized(value);
                return { true, true };
            }
            else if constexpr (std::is_same_v<Command, BypassToggle>)
            {
                target_.setBypassed(!target_.bypassed());
                return { true, true };
            }
            else if constexpr (std::is_same_v<Command, BypassSet>)
            {
                if (target_.bypassed() == command.bypassed)
                    return { true, false };
                target_.setBypassed(command.bypassed);
                return { true, true };
            }
            else
            {
                return {};
            }
        }, action);
    }
    catch (...)
    {
        return {};
    }
}

ControllerHostState ControllerRouter::hostState() const
{
    ControllerHostState state;
    state.selectedPedalName = target_.activePedalName();
    state.bypassed = target_.bypassed();
    const std::size_t count = std::min(target_.parameterCount(), state.parameters.size());
    for (std::size_t index = 0; index < count; ++index)
        state.parameters[index] = target_.parameterState(index);
    state.masterOutputNormalized = boundedNormalized(target_.masterOutputNormalized());
    return state;
}

} // namespace circuitpedal
