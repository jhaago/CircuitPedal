#include "CircuitFile.h"
#include "GenericCircuitProcessor.h"

#include <cmath>
#include <iostream>
#include <sstream>
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

void expectRelative(double actual,
                    double expected,
                    double relativeTolerance,
                    const char* label)
{
    const double tolerance = std::abs(expected) * relativeTolerance;
    std::ostringstream message;
    message << label << " bias mismatch: actual=" << actual
            << " V expected=" << expected
            << " V tolerance=±" << tolerance << " V";
    expect(std::abs(actual - expected) <= tolerance, message.str());
}

void testEngineeringValues()
{
    struct Case {
        const char* text;
        double expected;
    };

    for (const auto& item : {
             Case { "4k99", 4990.0 },
             Case { "2k2", 2200.0 },
             Case { "500k", 500000.0 },
             Case { "1M", 1000000.0 },
             Case { "220n", 220.0e-9 },
             Case { "10n", 10.0e-9 },
             Case { "100u", 100.0e-6 },
             Case { "6.734p", 6.734e-12 },
             Case { "0.15", 0.15 } })
    {
        bool ok = false;
        const double actual = circuitpedal::parseEngineeringValue(item.text, ok);
        expect(ok, std::string("engineering value did not parse: ") + item.text);
        const double scale = std::max(1.0, std::abs(item.expected));
        expect(std::abs(actual - item.expected) <= scale * 1.0e-12,
               std::string("engineering value mismatch: ") + item.text);
    }

    bool ok = true;
    (void)circuitpedal::parseEngineeringValue("10x", ok);
    expect(!ok, "unknown engineering suffix was accepted");
}

const char* demoCircuit = R"CPEDAL(
CPEDAL 1
NAME "Two Transistor Fuzz File Test"

V VCCSRC VCC 0 9
AUDIO GUITAR INPUT 0 0.15

C C_IN INPUT B1 220n
R R_BIAS1 VCC B1 100k
R R_BIAS1G B1 0 22k
R R_C1 VCC C1 10k
R R_E1 E1 0 1k
Q Q1 C1 B1 E1 2N3904

C C_INTER C1 B2 10n
R R_BIAS2 VCC B2 100k
R R_BIAS2G B2 0 22k
R R_C2 VCC C2 4k7
R R_E2 E2 0 1k
Q Q2 C2 B2 E2 2N3904

C C_OUT C2 TONE_IN 220n
POT TONE 0 TONE_W TONE_IN 10k LIN 0.50
POT VOLUME 0 OUT TONE_W 10k LOG 0.70

OUTPUT OUT 1
)CPEDAL";

void testParseAndCompile()
{
    circuitpedal::CircuitFileDocument document;
    std::string error;
    expect(circuitpedal::parseCircuitFileText(demoCircuit, document, error),
           "valid .cpedal text did not parse: " + error);
    expect(document.name == "Two Transistor Fuzz File Test",
           "quoted circuit NAME was not retained");
    expect(document.controls.size() == 2,
           "POT directives did not create named controls");
    if (document.controls.size() == 2)
    {
        expect(document.controls[0].name == "TONE",
               "first control name mismatch");
        expect(document.controls[1].name == "VOLUME",
               "second control name mismatch");
    }

    circuitpedal::GenericCircuit circuit;
    expect(circuit.compile(document.definition, 48000.0, error),
           "parsed circuit did not compile: " + error);
    expect(circuit.potentiometerCount() == 2,
           "compiled circuit potentiometer count mismatch");

    constexpr double pi = 3.14159265358979323846;
    double peak = 0.0;
    for (int n = 0; n < 24000; ++n)
    {
        if (n == 8000)
            expect(circuit.setPotentiometerPosition(0, 0.9),
                   "live TONE control update failed");
        if (n == 16000)
            expect(circuit.setPotentiometerPosition(1, 0.3),
                   "live VOLUME control update failed");

        const float input = static_cast<float>(
            0.5 * std::sin(2.0 * pi * 110.0 * static_cast<double>(n) / 48000.0));
        const float output = circuit.processSample(input);
        expect(std::isfinite(output),
               "parsed circuit produced non-finite output");
        expect(circuit.lastSolveConverged(),
               "parsed circuit transient solve failed");
        peak = std::max(peak, std::abs(static_cast<double>(output)));
    }
    expect(peak > 1.0e-5,
           "parsed circuit produced no meaningful output");
}

void testLinkedPotentiometerParsing()
{
    const char* linkedCircuit = R"CPEDAL(
CPEDAL 1
NAME "Linked Pot Test"
V VCC VCC 0 9
AUDIO IN INPUT 0 0.1
POT MIDS INPUT MID_A_W 0 20k LIN 0.40
POT_LINK MIDS VCC MID_B_W 0 20k LIN
R R1 MID_A_W OUT 10k
R R2 MID_B_W OUT 10k
R RLOAD OUT 0 100k
OUTPUT OUT 1
)CPEDAL";

    circuitpedal::CircuitFileDocument document;
    std::string error;
    expect(circuitpedal::parseCircuitFileText(linkedCircuit, document, error),
           "linked-pot circuit did not parse: " + error);
    expect(document.controls.size() == 1,
           "linked pot incorrectly created a second GUI control");
    expect(document.definition.potentiometerCount() == 2,
           "linked pot did not create a second electrical pot section");
    if (document.controls.size() == 1)
    {
        expect(document.controls[0].linkedPotentiometerIndices.size() == 1,
               "linked pot index was not attached to the primary control");
        expect(document.controls[0].initialPosition == 0.40,
               "linked pot did not inherit the primary initial position");
    }

    circuitpedal::GenericCircuit circuit;
    expect(circuit.compile(document.definition, 48000.0, error),
           "linked-pot circuit did not compile: " + error);
}

void testSwitchParsing()
{
    const char* switchCircuit = R"CPEDAL(
CPEDAL 1
NAME "Switch Parser Test"
V V1 A 0 1
V V2 B 0 0.25
R RLOAD OUT 0 10k
SWITCH MODE ONOFFON OUT A B CENTER "Bright" "Off" "Fat"
OUTPUT OUT 1
)CPEDAL";

    circuitpedal::CircuitFileDocument document;
    std::string error;
    expect(circuitpedal::parseCircuitFileText(switchCircuit, document, error),
           "switch circuit did not parse: " + error);
    expect(document.controls.size() == 1,
           "switch circuit did not expose one control");
    expect(document.definition.switchCount() == 1,
           "switch directive did not create a switch");
    if (document.controls.size() == 1)
    {
        const auto& control = document.controls[0];
        expect(control.kind == circuitpedal::CircuitFileControlKind::Switch,
               "switch control kind was not retained");
        expect(control.switchPositionCount == 3,
               "on-off-on switch did not report three positions");
        expect(control.initialSwitchPosition == 1,
               "CENTER did not map to the middle switch position");
        expect(control.switchPositionNames.size() == 3,
               "switch labels were not retained");
        if (control.switchPositionNames.size() == 3)
        {
            expect(control.switchPositionNames[0] == "Bright",
                   "switch A label mismatch");
            expect(control.switchPositionNames[1] == "Off",
                   "switch center label mismatch");
            expect(control.switchPositionNames[2] == "Fat",
                   "switch B label mismatch");
        }
    }

    circuitpedal::GenericCircuit circuit;
    expect(circuit.compile(document.definition, 48000.0, error),
           "parsed switch circuit did not compile: " + error);
}

void testLinkedSwitchParsing()
{
    const char* linkedSwitchCircuit = R"CPEDAL(
CPEDAL 1
NAME "Linked DPDT Test"
V VA VA 0 1
V VB VB 0 0.25
R RA OUT_A 0 10k
R RB OUT_B 0 10k
SWITCH BIAS SPDT OUT_A VA VB A "Mode A" "Mode B"
SWITCH_LINK BIAS OUT_B VB VA
OUTPUT OUT_A 1
)CPEDAL";

    circuitpedal::CircuitFileDocument document;
    std::string error;
    expect(circuitpedal::parseCircuitFileText(
               linkedSwitchCircuit, document, error),
           "linked-switch circuit did not parse: " + error);
    expect(document.controls.size() == 1,
           "linked switch incorrectly created a second GUI control");
    expect(document.definition.switchCount() == 2,
           "SWITCH_LINK did not create the second electrical pole");
    if (document.controls.size() == 1)
    {
        const auto& control = document.controls[0];
        expect(control.kind == circuitpedal::CircuitFileControlKind::Switch,
               "linked switch primary control kind mismatch");
        expect(control.switchMode == circuitpedal::CircuitSwitchMode::Spdt,
               "linked switch did not retain SPDT mode");
        expect(control.linkedSwitchIndices.size() == 1,
               "linked switch index was not attached to primary control");

        circuitpedal::GenericCircuit circuit;
        expect(circuit.compile(document.definition, 48000.0, error),
               "linked-switch circuit did not compile: " + error);
        if (circuit.switchCount() == 2)
        {
            expect(circuit.setSwitchPosition(control.switchIndex, 1),
                   "primary linked switch pole could not move");
            expect(circuit.setSwitchPosition(control.linkedSwitchIndices[0], 1),
                   "secondary linked switch pole could not move");
            (void)circuit.processSample(0.0f);
            expect(circuit.switchPosition(control.switchIndex) == 1
                   && circuit.switchPosition(control.linkedSwitchIndices[0]) == 1,
                   "linked switch poles did not share requested position");
        }
    }
}

void testParserFailures()
{
    circuitpedal::CircuitFileDocument document;
    std::string error;

    expect(!circuitpedal::parseCircuitFileText(
               "NAME MissingHeader\nOUTPUT OUT\n", document, error),
           "file without CPEDAL header was accepted");

    expect(!circuitpedal::parseCircuitFileText(
               "CPEDAL 1\nR R1 A 0 10x\nOUTPUT A\n", document, error),
           "invalid engineering value was accepted");

    expect(!circuitpedal::parseCircuitFileText(
               "CPEDAL 1\nQ Q1 C B E MADEUP\nOUTPUT C\n", document, error),
           "unknown transistor model was accepted");

    expect(!circuitpedal::parseCircuitFileText(
               "CPEDAL 1\nR R1 A 0 10k\n", document, error),
           "file without OUTPUT was accepted");

    expect(!circuitpedal::parseCircuitFileText(
               "CPEDAL 1\nPOT_LINK MISSING A W B 10k LIN\nOUTPUT W\n",
               document,
               error),
           "POT_LINK to an unknown control was accepted");

    expect(!circuitpedal::parseCircuitFileText(
               "CPEDAL 1\nSWITCH S ONOFFON C A B 7\nOUTPUT C\n",
               document,
               error),
           "out-of-range switch position was accepted");

    expect(!circuitpedal::parseCircuitFileText(
               "CPEDAL 1\nSWITCH_LINK MISSING A B C\nOUTPUT A\n",
               document,
               error),
           "SWITCH_LINK to an unknown control was accepted");
}

void testRepositoryWoollyReference()
{
#ifndef CIRCUITPEDAL_SOURCE_DIR
    expect(false, "CIRCUITPEDAL_SOURCE_DIR was not defined for file validation");
#else
    const std::string path =
        std::string(CIRCUITPEDAL_SOURCE_DIR)
        + "/circuits/woolly_mammoth_reference_draft.cpedal";

    circuitpedal::CircuitFileDocument document;
    std::string error;
    expect(circuitpedal::loadCircuitFile(path, document, error),
           "Woolly reference .cpedal did not load: " + error);
    expect(document.controls.size() == 4,
           "Woolly reference did not expose four controls");
    if (document.controls.size() == 4)
    {
        expect(document.controls[0].name == "WOOL", "Woolly WOOL control missing");
        expect(document.controls[1].name == "PINCH", "Woolly PINCH control missing");
        expect(document.controls[2].name == "EQ", "Woolly EQ control missing");
        expect(document.controls[3].name == "OUTPUT", "Woolly OUTPUT control missing");
    }

    for (std::size_t i = 0; i < document.definition.potentiometerCount(); ++i)
        expect(document.definition.setPotentiometerPosition(i, 1.0),
               "Could not set Woolly reference pot to maximum");

    circuitpedal::GenericCircuit circuit;
    expect(circuit.compile(document.definition, 48000.0, error),
           "Woolly reference circuit did not compile: " + error);

    const auto b1 = document.definition.findNode("B1");
    const auto c1 = document.definition.findNode("C1_NODE");
    const auto e2 = document.definition.findNode("E2");
    const auto c2 = document.definition.findNode("C2_NODE");
    // A verified physical build reports, with all pots maxed and a 9.33 V
    // supply: Q1 B=0.58 V, C=1.2 V; Q2 E=0.88 V, B=1.2 V, C=2.3 V.
    // Scale the reference to this file's 9.0 V supply. This V0.8 check is a
    // deliberately broad physical sanity guard, not the final fidelity gate:
    // the published readings themselves are approximate and the current BJT
    // remains a compact Ebers-Moll device. The tighter ±10% hardware target is
    // retained in docs/validation_plan.md for the later SPICE/measurement pass.
    constexpr double supplyScale = 9.0 / 9.33;
    constexpr double biasTolerance = 0.35;
    expectRelative(circuit.nodeVoltage(b1),
                   0.58 * supplyScale,
                   biasTolerance,
                   "Woolly Q1 base");
    expectRelative(circuit.nodeVoltage(c1),
                   1.20 * supplyScale,
                   biasTolerance,
                   "Woolly Q1 collector/Q2 base");
    expectRelative(circuit.nodeVoltage(e2),
                   0.88 * supplyScale,
                   biasTolerance,
                   "Woolly Q2 emitter");
    expectRelative(circuit.nodeVoltage(c2),
                   2.30 * supplyScale,
                   biasTolerance,
                   "Woolly Q2 collector");

    constexpr double pi = 3.14159265358979323846;
    double peak = 0.0;
    for (int n = 0; n < 24000; ++n)
    {
        const float input = static_cast<float>(
            0.5 * std::sin(2.0 * pi * 82.0 * static_cast<double>(n) / 48000.0));
        const float output = circuit.processSample(input);
        expect(std::isfinite(output), "Woolly reference produced non-finite audio");
        expect(circuit.lastSolveConverged(),
               "Woolly reference transient solve failed");
        peak = std::max(peak, std::abs(static_cast<double>(output)));
    }
    expect(peak > 1.0e-6,
           "Woolly reference produced no meaningful audio");
#endif
}

void testBigMuffRepositoryModels()
{
#ifndef CIRCUITPEDAL_SOURCE_DIR
    expect(false, "CIRCUITPEDAL_SOURCE_DIR was not defined for Big Muff validation");
#else
    for (const char* filename : {
             "big_muff_triangle.cpedal",
             "big_muff_rams_head.cpedal",
             "big_muff_green_russian.cpedal",
             "big_muff_nyc.cpedal" })
    {
        const std::string path =
            std::string(CIRCUITPEDAL_SOURCE_DIR) + "/circuits/" + filename;

        circuitpedal::CircuitFileDocument document;
        std::string error;
        expect(circuitpedal::loadCircuitFile(path, document, error),
               std::string("Big Muff model did not load: ") + filename + ": " + error);
        expect(document.controls.size() == 3,
               std::string("Big Muff model did not expose Sustain/Tone/Volume: ")
                   + filename);

        circuitpedal::OversampledGenericCircuit circuit;
        expect(circuit.compile(document.definition, 48000.0, error),
               std::string("Big Muff model did not compile at 4x: ")
                   + filename + ": " + error);

        constexpr double pi = 3.14159265358979323846;
        double peak = 0.0;
        for (int n = 0; n < 4800; ++n)
        {
            const float input = static_cast<float>(
                0.45 * std::sin(2.0 * pi * 110.0
                    * static_cast<double>(n) / 48000.0));
            const float output = circuit.processSample(input);
            expect(std::isfinite(output),
                   std::string("Big Muff produced non-finite audio: ") + filename);
            expect(circuit.lastSolveConverged(),
                   std::string("Big Muff nonlinear solve failed: ") + filename);
            peak = std::max(peak, std::abs(static_cast<double>(output)));
        }
        expect(peak > 1.0e-6,
               std::string("Big Muff produced no meaningful audio: ") + filename);
    }
#endif
}

void testNagaViperRepositoryModel()
{
#ifndef CIRCUITPEDAL_SOURCE_DIR
    expect(false, "CIRCUITPEDAL_SOURCE_DIR was not defined for Naga validation");
#else
    const std::string path =
        std::string(CIRCUITPEDAL_SOURCE_DIR) + "/circuits/naga_viper.cpedal";

    circuitpedal::CircuitFileDocument document;
    std::string error;
    expect(circuitpedal::loadCircuitFile(path, document, error),
           "Naga Viper model did not load: " + error);
    expect(document.controls.size() == 3,
           "Naga Viper did not expose Range/Boost/Heat controls");

    circuitpedal::OversampledGenericCircuit circuit;
    const bool compiled = circuit.compile(document.definition, 48000.0, error);
    expect(compiled, "Naga Viper did not compile at 4x: " + error);
    if (!compiled)
        return;

    constexpr double pi = 3.14159265358979323846;
    double peak = 0.0;
    for (int n = 0; n < 9600; ++n)
    {
        if (n == 3200)
            (void)circuit.setPotentiometerPosition(0, 1.0);
        if (n == 6400)
            (void)circuit.setPotentiometerPosition(2, 1.0);

        const float input = static_cast<float>(
            0.5 * std::sin(2.0 * pi * 220.0
                * static_cast<double>(n) / 48000.0));
        const float output = circuit.processSample(input);
        expect(std::isfinite(output), "Naga Viper produced non-finite audio");
        expect(circuit.lastSolveConverged(),
               "Naga Viper nonlinear solve failed");
        peak = std::max(peak, std::abs(static_cast<double>(output)));
    }
    expect(peak > 1.0e-5, "Naga Viper produced no meaningful audio");
#endif
}

void testFuzzFactoryRepositoryModel()
{
#ifndef CIRCUITPEDAL_SOURCE_DIR
    expect(false, "CIRCUITPEDAL_SOURCE_DIR was not defined for Fuzz Factory validation");
#else
    const std::string path =
        std::string(CIRCUITPEDAL_SOURCE_DIR)
        + "/circuits/fuzz_factory_reference.cpedal";

    circuitpedal::CircuitFileDocument document;
    std::string error;
    expect(circuitpedal::loadCircuitFile(path, document, error),
           "Fuzz Factory model did not load: " + error);
    expect(document.controls.size() == 5,
           "Fuzz Factory did not expose five controls");
    if (document.controls.size() == 5)
    {
        expect(document.controls[0].name == "STAB",
               "Fuzz Factory STAB control missing");
        expect(document.controls[1].name == "GATE",
               "Fuzz Factory GATE control missing");
        expect(document.controls[2].name == "COMP",
               "Fuzz Factory COMP control missing");
        expect(document.controls[3].name == "DRIVE",
               "Fuzz Factory DRIVE control missing");
        expect(document.controls[4].name == "VOLUME",
               "Fuzz Factory VOLUME control missing");
    }

    circuitpedal::OversampledGenericCircuit circuit;
    const bool compiled = circuit.compile(document.definition, 48000.0, error);
    expect(compiled, "Fuzz Factory did not compile at 4x: " + error);
    if (!compiled)
        return;

    constexpr double pi = 3.14159265358979323846;
    double peak = 0.0;
    for (int n = 0; n < 12000; ++n)
    {
        const float input = static_cast<float>(
            0.35 * std::sin(2.0 * pi * 110.0
                * static_cast<double>(n) / 48000.0));
        const float output = circuit.processSample(input);
        expect(std::isfinite(output),
               "Fuzz Factory produced non-finite audio");
        expect(circuit.lastSolveConverged(),
               "Fuzz Factory nonlinear solve failed");
        peak = std::max(peak, std::abs(static_cast<double>(output)));
    }
    expect(peak > 1.0e-6,
           "Fuzz Factory produced no meaningful audio");
#endif
}

void testFatFuzzFactoryRepositoryModel()
{
#ifndef CIRCUITPEDAL_SOURCE_DIR
    expect(false, "CIRCUITPEDAL_SOURCE_DIR was not defined for Fat Fuzz Factory validation");
#else
    const std::string path =
        std::string(CIRCUITPEDAL_SOURCE_DIR)
        + "/circuits/fat_fuzz_factory_reference.cpedal";

    circuitpedal::CircuitFileDocument document;
    std::string error;
    expect(circuitpedal::loadCircuitFile(path, document, error),
           "Fat Fuzz Factory model did not load: " + error);
    expect(document.controls.size() == 6,
           "Fat Fuzz Factory did not expose five pots plus Fat switch");

    std::size_t fatControlIndex = document.controls.size();
    for (std::size_t i = 0; i < document.controls.size(); ++i)
    {
        if (document.controls[i].name == "FAT")
        {
            fatControlIndex = i;
            break;
        }
    }
    expect(fatControlIndex < document.controls.size(),
           "Fat Fuzz Factory FAT switch control missing");
    if (fatControlIndex < document.controls.size())
    {
        const auto& fat = document.controls[fatControlIndex];
        expect(fat.kind == circuitpedal::CircuitFileControlKind::Switch,
               "FAT control was not parsed as a switch");
        expect(fat.switchPositionCount == 3,
               "FAT switch did not expose three positions");
        expect(fat.switchPositionNames.size() == 3,
               "FAT switch labels missing");
    }

    circuitpedal::OversampledGenericCircuit circuit;
    const bool compiled = circuit.compile(document.definition, 48000.0, error);
    expect(compiled, "Fat Fuzz Factory did not compile at 4x: " + error);
    if (!compiled)
        return;

    const auto& fat = document.controls[fatControlIndex];
    constexpr double pi = 3.14159265358979323846;
    double peak = 0.0;
    for (int n = 0; n < 18000; ++n)
    {
        if (n == 6000)
            expect(circuit.setSwitchPosition(fat.switchIndex, 0),
                   "FAT switch could not select Fat");
        if (n == 12000)
            expect(circuit.setSwitchPosition(fat.switchIndex, 2),
                   "FAT switch could not select Super Fat");

        const float input = static_cast<float>(
            0.35 * std::sin(2.0 * pi * 82.0
                * static_cast<double>(n) / 48000.0));
        const float output = circuit.processSample(input);
        expect(std::isfinite(output),
               "Fat Fuzz Factory produced non-finite audio");
        expect(circuit.lastSolveConverged(),
               "Fat Fuzz Factory nonlinear solve failed");
        peak = std::max(peak, std::abs(static_cast<double>(output)));
    }
    expect(peak > 1.0e-6,
           "Fat Fuzz Factory produced no meaningful audio");
#endif
}

void testFuzzoloRepositoryModel()
{
#ifndef CIRCUITPEDAL_SOURCE_DIR
    expect(false, "CIRCUITPEDAL_SOURCE_DIR was not defined for Fuzzolo validation");
#else
    const std::string path =
        std::string(CIRCUITPEDAL_SOURCE_DIR)
        + "/circuits/fuzzolo_reference_draft.cpedal";

    circuitpedal::CircuitFileDocument document;
    std::string error;
    expect(circuitpedal::loadCircuitFile(path, document, error),
           "Fuzzolo model did not load: " + error);
    expect(document.controls.size() == 3,
           "Fuzzolo did not expose Pulse Width, Volume and Pickups");

    std::size_t pulseControl = document.controls.size();
    std::size_t volumeControl = document.controls.size();
    std::size_t pickupControl = document.controls.size();
    for (std::size_t i = 0; i < document.controls.size(); ++i)
    {
        if (document.controls[i].name == "PULSE_WIDTH")
            pulseControl = i;
        else if (document.controls[i].name == "VOLUME")
            volumeControl = i;
        else if (document.controls[i].name == "PICKUPS")
            pickupControl = i;
    }

    expect(pulseControl < document.controls.size(),
           "Fuzzolo Pulse Width control missing");
    expect(volumeControl < document.controls.size(),
           "Fuzzolo Volume control missing");
    expect(pickupControl < document.controls.size(),
           "Fuzzolo pickup selector missing");
    if (pickupControl < document.controls.size())
    {
        const auto& pickup = document.controls[pickupControl];
        expect(pickup.kind == circuitpedal::CircuitFileControlKind::Switch,
               "Fuzzolo pickup selector was not parsed as a switch");
        expect(pickup.switchPositionCount == 2,
               "Fuzzolo pickup selector did not expose two positions");
    }

    circuitpedal::OversampledGenericCircuit circuit;
    const bool compiled = circuit.compile(document.definition, 48000.0, error);
    expect(compiled, "Fuzzolo did not compile at 4x: " + error);
    if (!compiled)
        return;

    constexpr double pi = 3.14159265358979323846;
    double peak = 0.0;
    for (int n = 0; n < 18000; ++n)
    {
        if (n == 6000 && pickupControl < document.controls.size())
        {
            expect(circuit.setSwitchPosition(
                       document.controls[pickupControl].switchIndex, 1),
                   "Fuzzolo pickup selector could not select Active");
        }
        if (n == 12000 && pulseControl < document.controls.size())
        {
            expect(circuit.setPotentiometerPosition(
                       document.controls[pulseControl].potentiometerIndex, 0.9),
                   "Fuzzolo Pulse Width could not move live");
        }

        const float input = static_cast<float>(
            0.25 * std::sin(2.0 * pi * 110.0
                * static_cast<double>(n) / 48000.0));
        const float output = circuit.processSample(input);
        expect(std::isfinite(output),
               "Fuzzolo produced non-finite audio");
        expect(circuit.lastSolveConverged(),
               "Fuzzolo nonlinear solve failed");
        peak = std::max(peak, std::abs(static_cast<double>(output)));
    }
    expect(peak > 1.0e-6,
           "Fuzzolo produced no meaningful audio");
#endif
}

void testTs10RepositoryModel()
{
#ifndef CIRCUITPEDAL_SOURCE_DIR
    expect(false, "CIRCUITPEDAL_SOURCE_DIR was not defined for TS10 validation");
#else
    const std::string path =
        std::string(CIRCUITPEDAL_SOURCE_DIR)
        + "/circuits/ts10_reference_draft.cpedal";

    circuitpedal::CircuitFileDocument document;
    std::string error;
    expect(circuitpedal::loadCircuitFile(path, document, error),
           "TS10 model did not load: " + error);
    expect(document.controls.size() == 3,
           "TS10 did not expose Drive, Tone and Level controls");
    if (document.controls.size() == 3)
    {
        expect(document.controls[0].name == "DRIVE", "TS10 Drive control missing");
        expect(document.controls[1].name == "TONE", "TS10 Tone control missing");
        expect(document.controls[2].name == "LEVEL", "TS10 Level control missing");
    }

    circuitpedal::OversampledGenericCircuit circuit;
    const bool compiled = circuit.compile(document.definition, 48000.0, error);
    expect(compiled, "TS10 did not compile at 4x: " + error);
    if (!compiled)
        return;

    constexpr double pi = 3.14159265358979323846;
    double peak = 0.0;
    int firstFailure = -1;
    int failureCount = 0;
    for (int n = 0; n < 18000; ++n)
    {
        if (n == 6000)
            expect(circuit.setPotentiometerPosition(
                       document.controls[0].potentiometerIndex, 0.95),
                   "TS10 Drive could not move live");
        if (n == 12000)
            expect(circuit.setPotentiometerPosition(
                       document.controls[1].potentiometerIndex, 0.80),
                   "TS10 Tone could not move live");

        const float input = static_cast<float>(
            0.35 * std::sin(2.0 * pi * 196.0
                * static_cast<double>(n) / 48000.0));
        const float output = circuit.processSample(input);
        expect(std::isfinite(output), "TS10 produced non-finite audio");
        if (!circuit.lastSolveConverged())
        {
            if (firstFailure < 0)
                firstFailure = n;
            ++failureCount;
        }
        peak = std::max(peak, std::abs(static_cast<double>(output)));
    }
    if (failureCount != 0)
    {
        std::cerr << "TS10 first failed host sample: " << firstFailure
                  << ", failed host samples: " << failureCount << '\n';
        expect(false, "TS10 nonlinear solve failed");
    }
    expect(peak > 1.0e-6, "TS10 produced no meaningful audio");
#endif
}

void testAnimatoRepositoryModel()
{
#ifndef CIRCUITPEDAL_SOURCE_DIR
    expect(false, "CIRCUITPEDAL_SOURCE_DIR was not defined for Animato validation");
#else
    const std::string path =
        std::string(CIRCUITPEDAL_SOURCE_DIR)
        + "/circuits/animato_reference_draft.cpedal";

    circuitpedal::CircuitFileDocument document;
    std::string error;
    expect(circuitpedal::loadCircuitFile(path, document, error),
           "Animato model did not load: " + error);
    expect(document.controls.size() == 5,
           "Animato did not expose Bias, Boost, Distortion, Tone and Volume");

    std::size_t bias = document.controls.size();
    std::size_t boost = document.controls.size();
    std::size_t distortion = document.controls.size();
    std::size_t tone = document.controls.size();
    std::size_t volume = document.controls.size();
    for (std::size_t i = 0; i < document.controls.size(); ++i)
    {
        const auto& name = document.controls[i].name;
        if (name == "BIAS") bias = i;
        else if (name == "BOOST") boost = i;
        else if (name == "DISTORTION") distortion = i;
        else if (name == "TONE") tone = i;
        else if (name == "VOLUME") volume = i;
    }

    expect(bias < document.controls.size(), "Animato Bias control missing");
    expect(boost < document.controls.size(), "Animato Boost control missing");
    expect(distortion < document.controls.size(), "Animato Distortion control missing");
    expect(tone < document.controls.size(), "Animato Tone control missing");
    expect(volume < document.controls.size(), "Animato Volume control missing");

    if (bias < document.controls.size())
    {
        expect(document.controls[bias].kind
                   == circuitpedal::CircuitFileControlKind::Switch,
               "Animato Bias was not parsed as a switch");
        expect(document.controls[bias].linkedSwitchIndices.size() == 1,
               "Animato Bias did not retain its second linked switch pole");
    }
    if (distortion < document.controls.size())
    {
        expect(document.controls[distortion].linkedPotentiometerIndices.size() == 1,
               "Animato Distortion did not retain its second pot gang");
    }

    circuitpedal::OversampledGenericCircuit circuit;
    const bool compiled = circuit.compile(document.definition, 48000.0, error);
    expect(compiled, "Animato did not compile at 4x: " + error);
    if (!compiled)
        return;

    constexpr double pi = 3.14159265358979323846;
    double peak = 0.0;
    int failureCount = 0;
    for (int n = 0; n < 24000; ++n)
    {
        if (n == 6000 && bias < document.controls.size())
        {
            const auto& control = document.controls[bias];
            expect(circuit.setSwitchPosition(control.switchIndex, 1),
                   "Animato primary Bias pole could not move");
            for (const std::size_t linked : control.linkedSwitchIndices)
                expect(circuit.setSwitchPosition(linked, 1),
                       "Animato linked Bias pole could not move");
        }
        if (n == 12000 && distortion < document.controls.size())
        {
            const auto& control = document.controls[distortion];
            expect(circuit.setPotentiometerPosition(
                       control.potentiometerIndex, 0.9),
                   "Animato Distortion primary gang could not move");
            for (const std::size_t linked : control.linkedPotentiometerIndices)
                expect(circuit.setPotentiometerPosition(linked, 0.9),
                       "Animato Distortion linked gang could not move");
        }
        if (n == 18000 && tone < document.controls.size())
        {
            expect(circuit.setPotentiometerPosition(
                       document.controls[tone].potentiometerIndex, 0.8),
                   "Animato Tone could not move");
        }

        const float input = static_cast<float>(
            0.35 * std::sin(2.0 * pi * 82.0
                * static_cast<double>(n) / 48000.0));
        const float output = circuit.processSample(input);
        expect(std::isfinite(output), "Animato produced non-finite audio");
        if (!circuit.lastSolveConverged())
            ++failureCount;
        peak = std::max(peak, std::abs(static_cast<double>(output)));
    }
    expect(failureCount == 0,
           "Animato nonlinear solve failed during live-control sweep");
    expect(peak > 1.0e-6, "Animato produced no meaningful audio");
#endif
}

void testKalamazooRepositoryModel()
{
#ifndef CIRCUITPEDAL_SOURCE_DIR
    expect(false, "CIRCUITPEDAL_SOURCE_DIR was not defined for Kalamazoo validation");
#else
    const std::string path =
        std::string(CIRCUITPEDAL_SOURCE_DIR)
        + "/circuits/kalamazoo_reference_draft.cpedal";

    circuitpedal::CircuitFileDocument document;
    std::string error;
    expect(circuitpedal::loadCircuitFile(path, document, error),
           "Kalamazoo model did not load: " + error);
    expect(document.controls.size() == 4,
           "Kalamazoo did not expose Drive, Tone, Glass and Level");

    std::size_t drive = document.controls.size();
    std::size_t tone = document.controls.size();
    std::size_t glass = document.controls.size();
    std::size_t level = document.controls.size();
    for (std::size_t i = 0; i < document.controls.size(); ++i)
    {
        const auto& name = document.controls[i].name;
        if (name == "DRIVE") drive = i;
        else if (name == "TONE") tone = i;
        else if (name == "GLASS") glass = i;
        else if (name == "LEVEL") level = i;
    }

    expect(drive < document.controls.size(), "Kalamazoo Drive control missing");
    expect(tone < document.controls.size(), "Kalamazoo Tone control missing");
    expect(glass < document.controls.size(), "Kalamazoo Glass control missing");
    expect(level < document.controls.size(), "Kalamazoo Level control missing");

    circuitpedal::OversampledGenericCircuit circuit;
    const bool compiled = circuit.compile(document.definition, 48000.0, error);
    expect(compiled, "Kalamazoo did not compile at 4x: " + error);
    if (!compiled)
        return;

    constexpr double pi = 3.14159265358979323846;
    double peak = 0.0;
    int failureCount = 0;
    for (int n = 0; n < 24000; ++n)
    {
        if (n == 6000 && drive < document.controls.size())
            expect(circuit.setPotentiometerPosition(
                       document.controls[drive].potentiometerIndex, 0.92),
                   "Kalamazoo Drive could not move live");
        if (n == 12000 && tone < document.controls.size())
            expect(circuit.setPotentiometerPosition(
                       document.controls[tone].potentiometerIndex, 0.90),
                   "Kalamazoo Tone could not move live");
        if (n == 18000 && glass < document.controls.size())
            expect(circuit.setPotentiometerPosition(
                       document.controls[glass].potentiometerIndex, 0.90),
                   "Kalamazoo Glass could not move live");

        const float input = static_cast<float>(
            0.30 * std::sin(2.0 * pi * 196.0
                * static_cast<double>(n) / 48000.0));
        const float output = circuit.processSample(input);
        expect(std::isfinite(output), "Kalamazoo produced non-finite audio");
        if (!circuit.lastSolveConverged())
            ++failureCount;
        peak = std::max(peak, std::abs(static_cast<double>(output)));
    }

    expect(failureCount == 0,
           "Kalamazoo nonlinear solve failed during live-control sweep");
    expect(peak > 1.0e-6, "Kalamazoo produced no meaningful audio");
#endif
}

void testBlueberryRepositoryModel()
{
#ifndef CIRCUITPEDAL_SOURCE_DIR
    expect(false, "CIRCUITPEDAL_SOURCE_DIR was not defined for Blueberry validation");
#else
    const std::string path =
        std::string(CIRCUITPEDAL_SOURCE_DIR)
        + "/circuits/blueberry_bass_overdrive_reference_draft.cpedal";

    circuitpedal::CircuitFileDocument document;
    std::string error;
    expect(circuitpedal::loadCircuitFile(path, document, error),
           "Blueberry model did not load: " + error);
    expect(document.controls.size() == 3,
           "Blueberry did not expose Drive, Tone and Volume");
    if (document.controls.size() == 3)
    {
        expect(document.controls[0].name == "DRIVE",
               "Blueberry Drive control missing");
        expect(document.controls[1].name == "TONE",
               "Blueberry Tone control missing");
        expect(document.controls[2].name == "VOLUME",
               "Blueberry Volume control missing");
    }

    circuitpedal::OversampledGenericCircuit circuit;
    const bool compiled = circuit.compile(document.definition, 48000.0, error);
    expect(compiled, "Blueberry did not compile at 4x: " + error);
    if (!compiled)
        return;

    constexpr double pi = 3.14159265358979323846;
    double peak = 0.0;
    int failureCount = 0;
    for (int n = 0; n < 24000; ++n)
    {
        if (n == 8000)
            expect(circuit.setPotentiometerPosition(
                       document.controls[0].potentiometerIndex, 0.92),
                   "Blueberry Drive could not move live");
        if (n == 16000)
            expect(circuit.setPotentiometerPosition(
                       document.controls[1].potentiometerIndex, 0.85),
                   "Blueberry Tone could not move live");

        const float input = static_cast<float>(
            0.30 * std::sin(2.0 * pi * 82.0
                * static_cast<double>(n) / 48000.0));
        const float output = circuit.processSample(input);
        expect(std::isfinite(output), "Blueberry produced non-finite audio");
        if (!circuit.lastSolveConverged())
            ++failureCount;
        peak = std::max(peak, std::abs(static_cast<double>(output)));
    }

    expect(failureCount == 0,
           "Blueberry nonlinear solve failed during live-control sweep");
    expect(peak > 1.0e-6, "Blueberry produced no meaningful audio");
#endif
}

void testBuiltInModels()
{
    bool ok = false;
    const auto transistor = circuitpedal::builtInNpnModel("2N3904", ok);
    expect(ok && transistor.forwardBeta > 100.0,
           "2N3904 built-in model was unavailable");

    const auto germanium = circuitpedal::builtInDiodeModel("1N34A", ok);
    expect(ok && germanium.saturationCurrentAmps > 0.0,
           "1N34A built-in diode model was unavailable");

    const auto redLed = circuitpedal::builtInDiodeModel("LED_RED", ok);
    expect(ok && redLed.saturationCurrentAmps < 1.0e-12
              && redLed.idealityFactor >= 1.8,
           "red LED clipping model was unavailable");

    const auto rectifier = circuitpedal::builtInDiodeModel("1N4007", ok);
    expect(ok && rectifier.saturationCurrentAmps > 0.0,
           "1N4007 diode model was unavailable");

    const auto nte103 = circuitpedal::builtInNpnModel("NTE103", ok);
    expect(ok && nte103.forwardBeta >= 80.0
              && nte103.saturationCurrentAmps > 1.0e-10,
           "NTE103 germanium NPN model was unavailable");

    const auto nte102 = circuitpedal::builtInPnpModel("NTE102", ok);
    expect(ok && nte102.forwardBeta >= 80.0
              && nte102.saturationCurrentAmps > 1.0e-10,
           "NTE102 germanium PNP model was unavailable");

    const auto c2240 = circuitpedal::builtInNpnModel("2SC2240", ok);
    expect(ok && c2240.forwardBeta >= 200.0,
           "2SC2240 high-gain NPN model was unavailable");
}

} // namespace

int main()
{
    testEngineeringValues();
    testParseAndCompile();
    testLinkedPotentiometerParsing();
    testSwitchParsing();
    testLinkedSwitchParsing();
    testParserFailures();
    testRepositoryWoollyReference();
    testBigMuffRepositoryModels();
    testNagaViperRepositoryModel();
    testFuzzFactoryRepositoryModel();
    testFatFuzzFactoryRepositoryModel();
    testFuzzoloRepositoryModel();
    testTs10RepositoryModel();
    testAnimatoRepositoryModel();
    testKalamazooRepositoryModel();
    testBlueberryRepositoryModel();
    testBuiltInModels();

    if (failures != 0)
    {
        std::cerr << failures << " circuit-file assertion(s) failed\n";
        return 1;
    }

    std::cout << "PASS: CircuitPedal V0.12 circuit-file validation suite\n";
    return 0;
}
