#include "GenericCircuit.h"
#include "PedalDeviceModels.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

namespace {

bool expect(bool condition, const char* message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool testTl072VoltageFollower()
{
    circuitpedal::CircuitDefinition definition;
    const auto vcc = definition.addNode("VCC");
    const auto input = definition.addNode("INPUT");
    const auto bias = definition.addNode("BIAS");
    const auto nonInverting = definition.addNode("PLUS");
    const auto output = definition.addNode("OP_OUT");
    const auto acOut = definition.addNode("AC_OUT");

    definition.addVoltageSource(vcc, circuitpedal::circuitGround, 9.0);
    definition.addVoltageSource(input,
                                circuitpedal::circuitGround,
                                0.0,
                                0.20);

    // 4.5 V virtual ground and AC-coupled guitar input.
    definition.addResistor(vcc, bias, 100000.0);
    definition.addResistor(bias, circuitpedal::circuitGround, 100000.0);
    definition.addCapacitor(input, nonInverting, 100.0e-9);
    definition.addResistor(nonInverting, bias, 1000000.0);

    // Unity-gain TL072 follower.
    definition.addOpAmp(nonInverting,
                        output,
                        output,
                        vcc,
                        circuitpedal::circuitGround,
                        circuitpedal::tl072Model());

    // Remove the bias before measuring the audio output.
    definition.addCapacitor(output, acOut, 1.0e-6);
    definition.addResistor(acOut, circuitpedal::circuitGround, 100000.0);
    definition.setOutputNode(acOut);
    definition.setOutputFullScalePerVolt(5.0);

    circuitpedal::GenericCircuit circuit;
    std::string error;
    if (!expect(circuit.compile(definition, 48000.0, error),
                "TL072 follower circuit should compile"))
    {
        std::cerr << error << '\n';
        return false;
    }

    constexpr double pi = 3.14159265358979323846;
    constexpr double sampleRate = 48000.0;
    constexpr double frequency = 1000.0;
    constexpr int totalSamples = 24000;
    constexpr int measureStart = 12000;

    double positivePeak = 0.0;
    double negativePeak = 0.0;
    bool finite = true;
    bool converged = true;

    for (int n = 0; n < totalSamples; ++n)
    {
        const float sample = static_cast<float>(
            0.25 * std::sin(2.0 * pi * frequency
                            * static_cast<double>(n) / sampleRate));
        const float processed = circuit.processSample(sample);
        finite = finite && std::isfinite(processed);
        converged = converged && circuit.lastSolveConverged();
        if (n >= measureStart)
        {
            positivePeak = std::max(positivePeak,
                                    static_cast<double>(processed));
            negativePeak = std::min(negativePeak,
                                    static_cast<double>(processed));
        }
    }

    bool ok = true;
    ok &= expect(finite, "TL072 follower output should remain finite");
    ok &= expect(converged, "TL072 follower solves should converge");

    // 0.25 full-scale in is 50 mV peak at the AUDIO source. A unity follower
    // and 5 FS/V output calibration should therefore return approximately
    // 0.25 full-scale after the coupling capacitors settle.
    const double measuredPeak = std::max(positivePeak, -negativePeak);
    ok &= expect(measuredPeak > 0.20 && measuredPeak < 0.30,
                 "TL072 follower should stay close to unity audio gain");

    const double dcOutput = circuit.nodeVoltage(output);
    ok &= expect(dcOutput > 4.0 && dcOutput < 5.0,
                 "TL072 follower output should bias near the 4.5 V virtual ground");
    return ok;
}

} // namespace

int main()
{
    if (!testTl072VoltageFollower())
        return 1;

    std::cout << "Pedal device model validation passed.\n";
    return 0;
}
