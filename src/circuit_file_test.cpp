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
    expect(circuit.compile(document.definition, 48000.0, error),
           "Naga Viper did not compile at 4x: " + error);

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

void testBuiltInModels()
{
    bool ok = false;
    const auto transistor = circuitpedal::builtInNpnModel("2N3904", ok);
    expect(ok && transistor.forwardBeta > 100.0,
           "2N3904 built-in model was unavailable");

    const auto germanium = circuitpedal::builtInDiodeModel("1N34A", ok);
    expect(ok && germanium.saturationCurrentAmps > 0.0,
           "1N34A built-in diode model was unavailable");
}

} // namespace

int main()
{
    testEngineeringValues();
    testParseAndCompile();
    testParserFailures();
    testRepositoryWoollyReference();
    testBigMuffRepositoryModels();
    testNagaViperRepositoryModel();
    testBuiltInModels();

    if (failures != 0)
    {
        std::cerr << failures << " circuit-file assertion(s) failed\n";
        return 1;
    }

    std::cout << "PASS: CircuitPedal V0.9 circuit-file validation suite\n";
    return 0;
}
