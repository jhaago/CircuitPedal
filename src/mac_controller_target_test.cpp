#include "CircuitStompProtocol.h"
#include "ControllerRouter.h"
#include "MacAudioEngine.h"
#include "MacControllerTarget.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string>

#ifndef CIRCUITPEDAL_SOURCE_DIR
#define CIRCUITPEDAL_SOURCE_DIR "."
#endif

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
    expect(std::abs(actual - expected) < 1.0e-5f, message);
}

void testBuiltInControlsAndMaster()
{
    circuitpedal::MacAudioEngine engine;
    circuitpedal::MacControllerTarget target(engine);

    expect(target.activePedalName() == "Built-in Distortion+", "built-in pedal name");
    expect(target.parameterCount() == 2U, "built-in pedal exposes two contextual controls");
    expect(target.parameterState(0U).label == "Distortion", "built-in P1 label");
    expect(target.parameterState(1U).label == "Output", "built-in P2 label");
    expect(!target.parameterState(2U).available, "unavailable built-in P3 is safe");

    expect(target.setParameterNormalized(0U, 0.72f), "built-in P1 write succeeds");
    expectNear(engine.distortion(), 0.72f, "built-in P1 writes engine distortion");
    expect(target.setParameterNormalized(1U, 0.34f), "built-in P2 write succeeds");
    expectNear(engine.output(), 0.34f, "built-in P2 writes engine output");
    expect(!target.setParameterNormalized(2U, 0.5f), "unavailable built-in write fails safely");

    target.setMasterOutputNormalized(1.0f);
    expectNear(engine.masterOutputDb(), 6.0f, "normalized master maps to +6 dB");
    target.setMasterOutputNormalized(0.0f);
    expectNear(engine.masterOutputDb(), -18.0f, "normalized master maps to -18 dB");
    engine.setMasterOutputDb(-6.0f);
    expectNear(target.masterOutputNormalized(), 0.5f, "master dB reads back normalized");

    target.setBypassed(true);
    expect(engine.bypassed() && target.bypassed(), "bypass delegates to engine");
}

void testLoadedCircuitControls()
{
    circuitpedal::MacAudioEngine engine;
    const std::filesystem::path circuitPath =
        std::filesystem::path(CIRCUITPEDAL_SOURCE_DIR)
        / "circuits" / "woolly_mammoth_reference_draft.cpedal";
    std::string error;
    expect(engine.loadCircuitFile(circuitPath.string(), error),
           "Woolly circuit loads for controller adapter test: " + error);

    circuitpedal::MacControllerTarget target(engine);
    expect(target.parameterCount() == 4U, "Woolly exposes four contextual controls");
    expect(target.parameterState(0U).label == "WOOL", "Woolly P1 is first declared control");
    expect(target.parameterState(1U).label == "PINCH", "Woolly P2 is second declared control");
    expect(target.parameterState(2U).label == "EQ", "Woolly P3 is third declared control");
    expect(target.parameterState(3U).label == "OUTPUT", "Woolly P4 is fourth declared control");
    expect(!target.parameterState(4U).available, "unused Woolly P5 is safe");

    expect(target.setParameterNormalized(0U, 0.83f), "Woolly P1 write succeeds");
    expectNear(engine.circuitControl(0U), 0.83f,
               "loaded-circuit P1 reaches authoritative engine control");

    circuitpedal::ControllerRouter router(target);
    const auto midiAction = circuitpedal::circuitstomp::decodeMidi1Message(0xB0, 20, 1);
    expect(midiAction.has_value(), "Prototype 1 P1 increment decodes for macOS integration");
    if (midiAction.has_value())
    {
        const auto result = router.route(*midiAction);
        expect(result.handled && result.stateChanged,
               "decoded P1 increment routes through the macOS target");
        expectNear(engine.circuitControl(0U), 0.84f,
                   "simulated CircuitStomp MIDI changes the live engine P1 value");
    }
    expect(!target.setParameterNormalized(4U, 0.5f),
           "unavailable loaded-circuit parameter fails safely");
}

} // namespace

int main()
{
    testBuiltInControlsAndMaster();
    testLoadedCircuitControls();

    if (failures != 0)
    {
        std::cerr << failures << " Mac controller target test(s) failed\n";
        return 1;
    }
    std::cout << "Mac controller target tests passed\n";
    return 0;
}
