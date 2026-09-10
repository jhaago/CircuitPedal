#include "DistortionPlusModel.h"
#include "GenericCircuit.h"
#include "GenericCircuitProcessor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <string>

namespace {

constexpr double pi = 3.14159265358979323846;

int failures = 0;

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void testRawCircuitOutputIsDecimatedBeforeDigitalClamp()
{
    constexpr double hostSampleRate = 48000.0;
    constexpr double outputScale = 0.25;

    // Deliberately create an otherwise-linear circuit whose oversampled raw
    // output exceeds digital full scale around waveform peaks. Correct
    // oversampling filters the raw circuit-domain voltage first and clamps only
    // the final host-rate sample. The V0.8 regression instead clipped every
    // oversampled substep before the decimation filter.
    circuitpedal::CircuitDefinition definition;
    const auto out = definition.addNode("OUT");
    definition.addVoltageSource(out,
                                circuitpedal::circuitGround,
                                0.0,
                                12.0);
    definition.setOutputNode(out);
    definition.setOutputFullScalePerVolt(outputScale);

    circuitpedal::OversampledGenericCircuit underTest;
    std::string error;
    expect(underTest.compile(definition, hostSampleRate, error),
           "oversampled circuit failed to compile: " + error);
    if (!error.empty())
        std::cerr << error << '\n';

    circuitpedal::GenericCircuit referenceCircuit;
    expect(referenceCircuit.compile(
               definition,
               hostSampleRate * circuitpedal::OversampledGenericCircuit::factor,
               error),
           "reference circuit failed to compile: " + error);

    circuitpedal::Oversampler4x referenceOversampler;
    referenceOversampler.prepare();

    double maximumDifference = 0.0;
    std::size_t clippedSubstepsExercised = 0;

    for (int n = 0; n < 4096; ++n)
    {
        const float input = static_cast<float>(
            0.45 * std::sin(2.0 * pi * 3000.0 * static_cast<double>(n)
                            / hostSampleRate));

        const float actual = underTest.processSample(input);
        expect(underTest.lastSolveConverged(),
               "oversampled circuit solve did not converge");

        std::array<double, circuitpedal::OversampledGenericCircuit::factor>
            oversampledInput {};
        referenceOversampler.upsample(static_cast<double>(input),
                                      oversampledInput);

        double filteredRawVolts = 0.0;
        for (int phase = 0;
             phase < circuitpedal::OversampledGenericCircuit::factor;
             ++phase)
        {
            (void)referenceCircuit.processSample(static_cast<float>(
                oversampledInput[static_cast<std::size_t>(phase)]));
            expect(referenceCircuit.lastSolveConverged(),
                   "reference circuit solve did not converge");

            const double rawVolts = referenceCircuit.nodeVoltage(out);
            if (std::abs(rawVolts * outputScale) > 1.0)
                ++clippedSubstepsExercised;

            (void)referenceOversampler.pushDownsample(
                rawVolts,
                phase,
                filteredRawVolts);
        }

        const double expected = std::clamp(filteredRawVolts * outputScale,
                                           -1.0,
                                           1.0);
        maximumDifference = std::max(
            maximumDifference,
            std::abs(static_cast<double>(actual) - expected));
    }

    expect(clippedSubstepsExercised > 0,
           "regression fixture never exceeded substep digital full scale");
    expect(maximumDifference < 1.0e-6,
           "oversampled generic path did not decimate raw circuit output before clamp");
}

} // namespace

int main()
{
    testRawCircuitOutputIsDecimatedBeforeDigitalClamp();

    if (failures != 0)
    {
        std::cerr << failures
                  << " generic-oversampling regression assertion(s) failed\n";
        return 1;
    }

    std::cout << "Generic oversampling regression checks passed.\n";
    return 0;
}
