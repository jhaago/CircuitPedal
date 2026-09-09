#include "GenericCircuit.h"
#include "GenericCircuitProcessor.h"

#include <algorithm>
#include <cmath>
#include <iostream>
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

void testResistorDivider()
{
    circuitpedal::CircuitDefinition definition;
    const auto vcc = definition.addNode("VCC");
    const auto out = definition.addNode("OUT");
    definition.addVoltageSource(vcc, circuitpedal::circuitGround, 9.0);
    definition.addResistor(vcc, out, 10000.0);
    definition.addResistor(out, circuitpedal::circuitGround, 10000.0);
    definition.setOutputNode(out);

    circuitpedal::GenericCircuit circuit;
    std::string error;
    expect(circuit.compile(definition, 48000.0, error),
           "resistor divider failed to compile: " + error);
    if (!error.empty())
        std::cerr << error << '\n';
    expect(std::abs(circuit.nodeVoltage(out) - 4.5) < 1.0e-8,
           "resistor divider DC operating point was not 4.5 V");
}

void testPotentiometer()
{
    circuitpedal::CircuitDefinition definition;
    const auto vcc = definition.addNode("VCC");
    const auto wiper = definition.addNode("WIPER");
    definition.addVoltageSource(vcc, circuitpedal::circuitGround, 1.0);
    definition.addPotentiometer(circuitpedal::circuitGround,
                                wiper,
                                vcc,
                                10000.0,
                                0.25,
                                1.0);
    definition.setOutputNode(wiper);

    circuitpedal::GenericCircuit circuit;
    std::string error;
    expect(circuit.compile(definition, 48000.0, error),
           "potentiometer circuit failed to compile: " + error);
    expect(std::abs(circuit.nodeVoltage(wiper) - 0.25) < 1.0e-5,
           "linear potentiometer did not create expected divider voltage");

    expect(circuit.setPotentiometerPosition(0, 0.75),
           "potentiometer position setter failed");
    // A sample triggers a fresh nonlinear/linear solve with the new pot value.
    (void)circuit.processSample(0.0f);
    expect(std::abs(circuit.nodeVoltage(wiper) - 0.75) < 1.0e-5,
           "updated potentiometer position did not affect circuit");
}

void testRcTransient()
{
    circuitpedal::CircuitDefinition definition;
    const auto input = definition.addNode("INPUT");
    const auto out = definition.addNode("OUT");
    definition.addVoltageSource(input,
                                circuitpedal::circuitGround,
                                0.0,
                                1.0);
    definition.addResistor(input, out, 1000.0);
    definition.addCapacitor(out, circuitpedal::circuitGround, 1.0e-6);
    definition.setOutputNode(out);

    circuitpedal::GenericCircuit circuit;
    std::string error;
    expect(circuit.compile(definition, 48000.0, error),
           "RC transient circuit failed to compile: " + error);

    const float first = circuit.processSample(0.5f);
    expect(first > 0.0f && first < 0.1f,
           "RC low-pass first sample was outside expected range");

    float settled = 0.0f;
    for (int i = 0; i < 1000; ++i)
        settled = circuit.processSample(0.5f);
    expect(std::abs(static_cast<double>(settled) - 0.5) < 1.0e-3,
           "RC low-pass did not settle to the input voltage");

    float decayed = settled;
    for (int i = 0; i < 1000; ++i)
        decayed = circuit.processSample(0.0f);
    expect(std::abs(static_cast<double>(decayed)) < 1.0e-3,
           "RC low-pass did not decay after input removal");
}

void testDiodeClamp()
{
    circuitpedal::CircuitDefinition definition;
    const auto input = definition.addNode("INPUT");
    const auto out = definition.addNode("OUT");
    definition.addVoltageSource(input,
                                circuitpedal::circuitGround,
                                0.0,
                                1.0);
    definition.addResistor(input, out, 1000.0);

    circuitpedal::GenericDiodeModel diode;
    diode.saturationCurrentAmps = 1.0e-12;
    diode.idealityFactor = 1.8;
    definition.addDiode(out, circuitpedal::circuitGround, diode);
    definition.setOutputNode(out);

    circuitpedal::GenericCircuit circuit;
    std::string error;
    expect(circuit.compile(definition, 48000.0, error),
           "diode clamp failed to compile: " + error);

    double positive = 0.0;
    for (int i = 0; i < 32; ++i)
        positive = circuit.processSample(1.0f);
    expect(circuit.lastSolveConverged(),
           "diode clamp did not converge");
    expect(positive > 0.35 && positive < 0.9,
           "diode clamp voltage was implausible");

    double negative = 0.0;
    for (int i = 0; i < 32; ++i)
        negative = circuit.processSample(-0.5f);
    expect(negative < -0.45 && negative > -0.55,
           "forward diode incorrectly clamped negative input");
}

void testNpnOperatingPoint()
{
    circuitpedal::CircuitDefinition definition;
    const auto vcc = definition.addNode("VCC");
    const auto collector = definition.addNode("C");
    const auto base = definition.addNode("B");
    const auto emitter = definition.addNode("E");

    definition.addVoltageSource(vcc, circuitpedal::circuitGround, 9.0);
    definition.addResistor(vcc, collector, 4700.0);
    definition.addResistor(vcc, base, 100000.0);
    definition.addResistor(base, circuitpedal::circuitGround, 22000.0);
    definition.addResistor(emitter, circuitpedal::circuitGround, 1000.0);
    definition.addNpnBjt(collector, base, emitter);
    definition.setOutputNode(collector);

    circuitpedal::GenericCircuit circuit;
    std::string error;
    expect(circuit.compile(definition, 48000.0, error),
           "NPN bias circuit failed to compile: " + error);
    if (!error.empty())
        std::cerr << error << '\n';

    const double vc = circuit.nodeVoltage(collector);
    const double vb = circuit.nodeVoltage(base);
    const double ve = circuit.nodeVoltage(emitter);
    expect(vb > 0.5 && vb < 3.0,
           "NPN base DC voltage was implausible");
    expect(ve >= 0.0 && ve < vb,
           "NPN emitter DC voltage was implausible");
    expect(vc > ve && vc < 9.01,
           "NPN collector DC voltage was implausible");
    expect((vb - ve) > 0.45 && (vb - ve) < 0.9,
           "NPN base-emitter voltage was implausible");
}

void testPnpOperatingPoint()
{
    circuitpedal::CircuitDefinition definition;
    const auto vcc = definition.addNode("VCC");
    const auto base = definition.addNode("B");
    const auto collector = definition.addNode("C");

    definition.addVoltageSource(vcc, circuitpedal::circuitGround, 9.0);
    definition.addResistor(vcc, base, 10000.0);
    definition.addResistor(base, circuitpedal::circuitGround, 100000.0);
    definition.addResistor(collector, circuitpedal::circuitGround, 10000.0);

    circuitpedal::GenericPnpBjtModel pnp;
    pnp.saturationCurrentAmps = 2.0e-14;
    pnp.forwardBeta = 150.0;
    definition.addPnpBjt(collector, base, vcc, pnp);
    definition.setOutputNode(collector);

    circuitpedal::GenericCircuit circuit;
    std::string error;
    expect(circuit.compile(definition, 48000.0, error),
           "PNP bias circuit failed to compile: " + error);

    const double vb = circuit.nodeVoltage(base);
    const double vc = circuit.nodeVoltage(collector);
    expect(vb > 7.5 && vb < 8.8,
           "PNP base DC voltage was implausible");
    expect(vc > 0.1 && vc < 9.0,
           "PNP collector DC voltage was implausible");
    expect((9.0 - vb) > 0.45 && (9.0 - vb) < 1.0,
           "PNP emitter-base voltage was implausible");
}

void testNjfetOperatingPointAndAudio()
{
    circuitpedal::CircuitDefinition definition;
    const auto vcc = definition.addNode("VCC");
    const auto input = definition.addNode("INPUT");
    const auto gate = definition.addNode("G");
    const auto drain = definition.addNode("D");
    const auto source = definition.addNode("S");
    const auto out = definition.addNode("OUT");

    definition.addVoltageSource(vcc, circuitpedal::circuitGround, 9.0);
    definition.addVoltageSource(input,
                                circuitpedal::circuitGround,
                                0.0,
                                0.1);
    definition.addCapacitor(input, gate, 100.0e-9);
    definition.addResistor(gate, circuitpedal::circuitGround, 1.0e6);
    definition.addResistor(vcc, drain, 4700.0);
    definition.addResistor(source, circuitpedal::circuitGround, 1000.0);

    circuitpedal::GenericNjfetModel jfet;
    jfet.idssAmps = 3.0e-3;
    jfet.pinchOffVoltageVolts = -2.5;
    definition.addNjfet(drain, gate, source, jfet);

    definition.addCapacitor(drain, out, 100.0e-9);
    definition.addResistor(out, circuitpedal::circuitGround, 100000.0);
    definition.setOutputNode(out);

    circuitpedal::GenericCircuit circuit;
    std::string error;
    expect(circuit.compile(definition, 48000.0, error),
           "N-JFET bias circuit failed to compile: " + error);

    const double vd = circuit.nodeVoltage(drain);
    const double vs = circuit.nodeVoltage(source);
    const double vg = circuit.nodeVoltage(gate);
    expect(vs > 0.1 && vs < 2.5,
           "N-JFET source DC voltage was implausible");
    expect(vd > vs && vd < 9.0,
           "N-JFET drain DC voltage was implausible");
    expect(std::abs(vg) < 0.05,
           "N-JFET gate DC bias was not near ground");

    constexpr double pi = 3.14159265358979323846;
    double peak = 0.0;
    for (int n = 0; n < 12000; ++n)
    {
        const float inputSample = static_cast<float>(
            0.6 * std::sin(2.0 * pi * 220.0 * static_cast<double>(n) / 48000.0));
        const float outputSample = circuit.processSample(inputSample);
        expect(std::isfinite(outputSample),
               "N-JFET audio produced non-finite output");
        expect(circuit.lastSolveConverged(),
               "N-JFET transient solve failed");
        peak = std::max(peak, std::abs(static_cast<double>(outputSample)));
    }
    expect(peak > 1.0e-5,
           "N-JFET audio stage produced no meaningful output");
}

void testOversampledGenericCircuit()
{
    circuitpedal::CircuitDefinition definition;
    const auto input = definition.addNode("INPUT");
    const auto clip = definition.addNode("CLIP");
    const auto out = definition.addNode("OUT");

    definition.addVoltageSource(input,
                                circuitpedal::circuitGround,
                                0.0,
                                0.8);
    definition.addResistor(input, clip, 1000.0);

    circuitpedal::GenericDiodeModel diode;
    diode.saturationCurrentAmps = 2.5e-9;
    diode.idealityFactor = 1.75;
    definition.addDiode(clip, circuitpedal::circuitGround, diode);

    definition.addCapacitor(clip, out, 100.0e-9);
    definition.addResistor(out, circuitpedal::circuitGround, 10000.0);
    definition.setOutputNode(out);

    circuitpedal::OversampledGenericCircuit circuit;
    std::string error;
    expect(circuit.compile(definition, 48000.0, error),
           "oversampled generic circuit failed to compile: " + error);
    expect(std::abs(circuit.circuitSampleRate() - 192000.0) < 1.0e-6,
           "oversampled circuit did not run at 4x host sample rate");

    constexpr double pi = 3.14159265358979323846;
    double peak = 0.0;
    for (int n = 0; n < 24000; ++n)
    {
        const float inputSample = static_cast<float>(
            0.8 * std::sin(2.0 * pi * 997.0 * static_cast<double>(n) / 48000.0));
        const float outputSample = circuit.processSample(inputSample);
        expect(std::isfinite(outputSample),
               "oversampled generic circuit produced non-finite output");
        expect(circuit.lastSolveConverged(),
               "oversampled generic circuit failed a nonlinear substep");
        peak = std::max(peak, std::abs(static_cast<double>(outputSample)));
    }
    expect(peak > 1.0e-5,
           "oversampled generic circuit produced no meaningful audio");

    circuit.reset();
    const float resetOutput = circuit.processSample(0.0f);
    expect(std::isfinite(resetOutput),
           "oversampled generic circuit did not recover after reset");
}

void testTwoTransistorFuzzLikeNetwork()
{
    circuitpedal::CircuitDefinition definition;
    const auto vcc = definition.addNode("VCC");
    const auto input = definition.addNode("INPUT");
    const auto b1 = definition.addNode("B1");
    const auto c1 = definition.addNode("C1");
    const auto e1 = definition.addNode("E1");
    const auto b2 = definition.addNode("B2");
    const auto c2 = definition.addNode("C2");
    const auto e2 = definition.addNode("E2");
    const auto out = definition.addNode("OUT");

    definition.addVoltageSource(vcc, circuitpedal::circuitGround, 9.0);
    definition.addVoltageSource(input,
                                circuitpedal::circuitGround,
                                0.0,
                                0.15);
    definition.addCapacitor(input, b1, 220.0e-9);
    definition.addResistor(vcc, b1, 100000.0);
    definition.addResistor(b1, circuitpedal::circuitGround, 22000.0);
    definition.addResistor(vcc, c1, 10000.0);
    definition.addResistor(e1, circuitpedal::circuitGround, 1000.0);
    definition.addNpnBjt(c1, b1, e1);

    definition.addCapacitor(c1, b2, 10.0e-9);
    definition.addResistor(vcc, b2, 100000.0);
    definition.addResistor(b2, circuitpedal::circuitGround, 22000.0);
    definition.addResistor(vcc, c2, 4700.0);
    definition.addResistor(e2, circuitpedal::circuitGround, 1000.0);
    definition.addNpnBjt(c2, b2, e2);

    definition.addCapacitor(c2, out, 220.0e-9);
    definition.addResistor(out, circuitpedal::circuitGround, 10000.0);
    definition.setOutputNode(out);
    definition.setOutputFullScalePerVolt(1.0);

    circuitpedal::GenericCircuit circuit;
    std::string error;
    expect(circuit.compile(definition, 48000.0, error),
           "two-transistor fuzz-like circuit failed to compile: " + error);

    constexpr double pi = 3.14159265358979323846;
    double peak = 0.0;
    for (int n = 0; n < 48000; ++n)
    {
        const float inputSample = static_cast<float>(
            0.5 * std::sin(2.0 * pi * 110.0 * static_cast<double>(n) / 48000.0));
        const float outputSample = circuit.processSample(inputSample);
        expect(std::isfinite(outputSample),
               "two-transistor network produced non-finite output");
        expect(circuit.lastSolveConverged(),
               "two-transistor network failed a transient solve");
        peak = std::max(peak, std::abs(static_cast<double>(outputSample)));
    }
    expect(peak > 1.0e-4,
           "two-transistor network produced no meaningful output");
}

} // namespace

int main()
{
    testResistorDivider();
    testPotentiometer();
    testRcTransient();
    testDiodeClamp();
    testNpnOperatingPoint();
    testPnpOperatingPoint();
    testNjfetOperatingPointAndAudio();
    testOversampledGenericCircuit();
    testTwoTransistorFuzzLikeNetwork();

    if (failures != 0)
    {
        std::cerr << failures << " generic-circuit assertion(s) failed\n";
        return 1;
    }

    std::cout << "PASS: CircuitPedal V0.9 generic circuit validation suite\n";
    return 0;
}
