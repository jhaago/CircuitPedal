#include "ControllerRouter.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <limits>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void expectNear(float actual, float expected, const std::string& message)
{
    expect(std::abs(actual - expected) < 1.0e-6f, message);
}

class FakeControllerTarget final : public circuitpedal::ControllerTarget {
public:
    FakeControllerTarget()
    {
        for (std::size_t i = 0; i < parameters.size(); ++i)
        {
            parameters[i].available = true;
            parameters[i].label = "P" + std::to_string(i + 1U);
            parameters[i].normalizedValue = 0.5f;
            parameters[i].encoderStepNormalized = 0.01f;
        }
    }

    std::string activePedalName() const override { return pedalName; }
    std::size_t parameterCount() const override { return availableParameterCount; }

    circuitpedal::ControllerParameterState parameterState(std::size_t index) const override
    {
        if (index >= availableParameterCount || index >= parameters.size())
            return {};
        return parameters[index];
    }

    bool setParameterNormalized(std::size_t index, float value) noexcept override
    {
        if (index >= availableParameterCount || index >= parameters.size())
            return false;
        lastParameterIndex = index;
        ++parameterWriteCount;
        parameters[index].normalizedValue = value;
        return true;
    }

    float masterOutputNormalized() const noexcept override { return masterOutput; }

    void setMasterOutputNormalized(float value) noexcept override
    {
        ++masterWriteCount;
        masterOutput = value;
    }

    bool bypassed() const noexcept override { return bypass; }

    void setBypassed(bool value) noexcept override
    {
        ++bypassWriteCount;
        bypass = value;
    }

    std::string pedalName = "Test Pedal";
    std::array<circuitpedal::ControllerParameterState, 5> parameters {};
    std::size_t availableParameterCount = parameters.size();
    std::size_t lastParameterIndex = parameters.size();
    int parameterWriteCount = 0;
    int masterWriteCount = 0;
    int bypassWriteCount = 0;
    float masterOutput = 0.5f;
    bool bypass = false;
};

void testParameterDeltasAndClamping()
{
    FakeControllerTarget target;
    circuitpedal::ControllerRouter router(target);

    auto result = router.route(circuitpedal::ParameterDelta { 0U, 1 });
    expect(result.handled && result.stateChanged, "P1 increment reports a state change");
    expect(target.lastParameterIndex == 0U, "P1 delta routes to parameter zero");
    expectNear(target.parameters[0].normalizedValue, 0.51f,
               "P1 increments by its declared step");

    result = router.route(circuitpedal::ParameterDelta { 0U, -1 });
    expect(result.handled && result.stateChanged, "P1 decrement reports a state change");
    expectNear(target.parameters[0].normalizedValue, 0.50f, "P1 decrements");

    result = router.route(circuitpedal::ParameterSet { 0U, 5.0f });
    expect(result.handled && result.stateChanged, "high parameter set is handled");
    expectNear(target.parameters[0].normalizedValue, 1.0f, "parameter set clamps high");

    result = router.route(circuitpedal::ParameterDelta { 0U, 1 });
    expect(result.handled && !result.stateChanged, "clamped delta reports no state change");
    expectNear(target.parameters[0].normalizedValue, 1.0f, "parameter delta clamps high");

    result = router.route(circuitpedal::ParameterSet { 0U, -5.0f });
    expect(result.handled && result.stateChanged, "low parameter set is handled");
    expectNear(target.parameters[0].normalizedValue, 0.0f, "parameter set clamps low");

    result = router.route(circuitpedal::ParameterDelta { 0U, -1 });
    expect(result.handled && !result.stateChanged, "low clamped delta reports no change");
    expectNear(target.parameters[0].normalizedValue, 0.0f, "parameter delta clamps low");
}

void testContextualSlotsAndUnavailableParameters()
{
    FakeControllerTarget target;
    circuitpedal::ControllerRouter router(target);

    for (std::size_t index = 1U; index < 5U; ++index)
    {
        const auto result = router.route(circuitpedal::ParameterDelta { index, 1 });
        expect(result.handled && result.stateChanged,
               "available contextual parameter delta is handled");
        expect(target.lastParameterIndex == index,
               "P2-P5 route to their matching contextual index");
    }

    target.availableParameterCount = 2U;
    const int writesBefore = target.parameterWriteCount;
    const auto result = router.route(circuitpedal::ParameterDelta { 4U, 1 });
    expect(!result.handled && !result.stateChanged, "unavailable P slot is a safe no-op");
    expect(target.parameterWriteCount == writesBefore,
           "unavailable P slot does not write the target");
}

void testMasterIsSeparateFromPedalOutput()
{
    FakeControllerTarget target;
    target.parameters[0].label = "Output";
    const float pedalOutputBefore = target.parameters[0].normalizedValue;
    circuitpedal::ControllerRouter router(target);

    auto result = router.route(circuitpedal::MasterOutputDelta { 2 });
    expect(result.handled && result.stateChanged, "MASTER delta is handled");
    expectNear(target.masterOutput, 0.52f, "MASTER delta changes global master output");
    expectNear(target.parameters[0].normalizedValue, pedalOutputBefore,
               "MASTER does not change a pedal Output control");
    expect(target.parameterWriteCount == 0, "MASTER never writes a pedal parameter");

    result = router.route(circuitpedal::MasterOutputSet { -2.0f });
    expectNear(target.masterOutput, 0.0f, "MASTER set clamps low");
}

void testBypassAndUnsupportedActions()
{
    FakeControllerTarget target;
    circuitpedal::ControllerRouter router(target);

    auto result = router.route(circuitpedal::BypassToggle {});
    expect(result.handled && result.stateChanged && target.bypass,
           "bypass toggle changes target state");

    result = router.route(circuitpedal::BypassSet { true });
    expect(result.handled && !result.stateChanged,
           "setting the existing bypass value reports no state change");

    const int parameterWritesBefore = target.parameterWriteCount;
    result = router.route(circuitpedal::ParameterSet {
        0U, std::numeric_limits<float>::quiet_NaN()
    });
    expect(!result.handled && !result.stateChanged, "non-finite parameter set is rejected");
    expect(target.parameterWriteCount == parameterWritesBefore,
           "non-finite parameter set does not write target");

    result = router.route(circuitpedal::StompAction { 0U, true });
    expect(!result.handled && !result.stateChanged,
           "unassigned stomp action is safely unhandled");
    result = router.route(circuitpedal::ExpressionValue { 0U, 0.5f });
    expect(!result.handled && !result.stateChanged,
           "unassigned expression action is safely unhandled");
}

void testHostStateSnapshot()
{
    FakeControllerTarget target;
    target.pedalName = "Snapshot Pedal";
    target.parameters[0].label = "Level";
    target.parameters[0].normalizedValue = 0.75f;
    target.availableParameterCount = 2U;
    target.masterOutput = 0.25f;
    target.bypass = true;
    circuitpedal::ControllerRouter router(target);

    const auto state = router.hostState();
    expect(state.selectedPedalName == "Snapshot Pedal", "snapshot contains pedal name");
    expect(state.bypassed, "snapshot contains bypass state");
    expect(state.parameters[0].available && state.parameters[0].label == "Level",
           "snapshot contains available parameter metadata");
    expectNear(state.parameters[0].normalizedValue, 0.75f,
               "snapshot contains parameter value");
    expect(!state.parameters[2].available, "snapshot leaves unavailable slots empty");
    expectNear(state.masterOutputNormalized, 0.25f,
               "snapshot contains global master value");
    expect(state.presetName.empty() && !state.tuner.has_value(),
           "future feedback fields default to absent");
}

} // namespace

int main()
{
    testParameterDeltasAndClamping();
    testContextualSlotsAndUnavailableParameters();
    testMasterIsSeparateFromPedalOutput();
    testBypassAndUnsupportedActions();
    testHostStateSnapshot();

    if (failures != 0)
    {
        std::cerr << failures << " controller router test(s) failed\n";
        return 1;
    }
    std::cout << "Controller router tests passed\n";
    return 0;
}
