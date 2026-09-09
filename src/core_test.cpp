#include "DistortionPlusModel.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

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

double referenceClipRoot(double source,
                         double timestep,
                         const circuitpedal::ClipNetworkParameters& p,
                         const circuitpedal::ClipNetworkState& state)
{
    const double seriesG = 1.0 / p.seriesResistanceOhms;
    const double couplingG = p.couplingCapacitanceFarads / timestep;
    const double shuntG = p.shuntCapacitanceFarads / timestep;
    const double loadG = 1.0 / p.loadResistanceOhms;
    const double nvT = p.diodeIdealityFactor * p.thermalVoltageVolts;
    const auto residual = [&](double v) {
        const double seriesNode =
            (seriesG * v + couplingG * (source - state.couplingCapVoltage))
            / (couplingG + seriesG);
        return seriesG * (v - seriesNode)
             + shuntG * (v - state.clipNodeVoltage)
             + loadG * v
             + 2.0 * p.diodeSaturationCurrentAmps * std::sinh(v / nvT);
    };

    double lower = -1.0;
    double upper = 1.0;
    for (int i = 0; i < 100; ++i)
    {
        const double middle = 0.5 * (lower + upper);
        if (residual(middle) < 0.0)
            lower = middle;
        else
            upper = middle;
    }
    return 0.5 * (lower + upper);
}

void testClipSolver()
{
    circuitpedal::ClipNetworkParameters parameters;
    for (double sampleRate : { 8000.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        const double timestep = 1.0 / (sampleRate * 4.0);
        for (double oldClip : { -0.35, 0.0, 0.35 })
        {
            for (double oldCoupling : { -3.0, 0.0, 3.0 })
            {
                for (double source : { -3.2, -1.0, 0.0, 1.0, 3.2 })
                {
                    circuitpedal::ClipNetworkState state { oldCoupling, oldClip };
                    const auto oldState = state;
                    const double reference = referenceClipRoot(source,
                                                               timestep,
                                                               parameters,
                                                               oldState);
                    const auto result = circuitpedal::processClipNetwork(source,
                                                                         timestep,
                                                                         parameters,
                                                                         state);
                    expect(result.converged, "clip solver did not converge");
                    expect(std::isfinite(result.clipNodeVoltage), "clip solver returned non-finite voltage");
                    expect(std::abs(result.kclResidualAmps) <= 1.0e-8,
                           "clip solver KCL residual exceeded tolerance");
                    const double referenceError = std::abs(result.clipNodeVoltage - reference);
                    if (referenceError > 1.0e-7)
                    {
                        std::cerr << "clip mismatch: fs=" << sampleRate
                                  << " oldClip=" << oldClip
                                  << " oldCoupling=" << oldCoupling
                                  << " source=" << source
                                  << " result=" << result.clipNodeVoltage
                                  << " reference=" << reference
                                  << " residual=" << result.kclResidualAmps
                                  << " iterations=" << result.iterations << '\n';
                    }
                    expect(referenceError <= 1.0e-7,
                           "clip solver disagreed with independent bisection reference");
                }
            }
        }
    }
}

void testOversampler()
{
    circuitpedal::Oversampler4x oversampler;
    oversampler.prepare();
    double output = 0.0;
    for (int n = 0; n < 2000; ++n)
    {
        std::array<double, circuitpedal::Oversampler4x::factor> highRate {};
        oversampler.upsample(1.0, highRate);
        for (int phase = 0; phase < circuitpedal::Oversampler4x::factor; ++phase)
            oversampler.pushDownsample(highRate[static_cast<std::size_t>(phase)], phase, output);
    }
    expect(std::abs(output - 1.0) < 1.0e-9, "oversampler does not preserve DC gain");

    for (double frequency : { 1000.0, 10000.0 })
    {
        oversampler.prepare();
        double inputEnergy = 0.0;
        double outputEnergy = 0.0;
        for (int n = 0; n < 48000; ++n)
        {
            const double input = std::sin(2.0 * pi * frequency * n / 48000.0);
            std::array<double, circuitpedal::Oversampler4x::factor> highRate {};
            oversampler.upsample(input, highRate);
            for (int phase = 0; phase < circuitpedal::Oversampler4x::factor; ++phase)
                oversampler.pushDownsample(highRate[static_cast<std::size_t>(phase)], phase, output);
            if (n > 1000)
            {
                inputEnergy += input * input;
                outputEnergy += output * output;
            }
        }
        const double gain = std::sqrt(outputEnergy / inputEnergy);
        expect(gain > 0.99 && gain < 1.01, "oversampler pass-band gain exceeded tolerance");
    }

    oversampler.prepare();
    double stopBandEnergy = 0.0;
    std::size_t stopBandSamples = 0;
    for (int n = 0; n < 192000; ++n)
    {
        const double input = std::sin(2.0 * pi * 30000.0 * n / 192000.0);
        if (oversampler.pushDownsample(input, n % 4, output) && n > 4000)
        {
            stopBandEnergy += output * output;
            ++stopBandSamples;
        }
    }
    const double stopBandGain = std::sqrt(stopBandEnergy / static_cast<double>(stopBandSamples))
                              / std::sqrt(0.5);
    expect(stopBandGain < 1.0e-4, "oversampler stop-band rejection was insufficient");
}

std::vector<float> renderSine(circuitpedal::DistortionPlusModel& model,
                              double sampleRate,
                              double frequency,
                              double amplitude,
                              int sampleCount)
{
    std::vector<float> output(static_cast<std::size_t>(sampleCount));
    for (int n = 0; n < sampleCount; ++n)
    {
        const float input = static_cast<float>(
            amplitude * std::sin(2.0 * pi * frequency * static_cast<double>(n) / sampleRate));
        output[static_cast<std::size_t>(n)] = model.processSample(input);
    }
    return output;
}

double rmsAfterWarmup(const std::vector<float>& samples, std::size_t warmup)
{
    double sumSquares = 0.0;
    for (std::size_t i = warmup; i < samples.size(); ++i)
        sumSquares += static_cast<double>(samples[i]) * samples[i];
    return std::sqrt(sumSquares / static_cast<double>(samples.size() - warmup));
}

void testSilenceDcAndSampleRates()
{
    for (double sampleRate : { 8000.0, 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        circuitpedal::DistortionPlusModel model;
        model.prepare(sampleRate);
        double peak = 0.0;
        for (int n = 0; n < static_cast<int>(sampleRate); ++n)
        {
            const float output = model.processSample(0.0f);
            expect(std::isfinite(output), "silence produced a non-finite sample");
            peak = std::max(peak, std::abs(static_cast<double>(output)));
        }
        expect(peak == 0.0, "silence did not remain exactly silent");
    }

    circuitpedal::DistortionPlusModel dcModel;
    dcModel.prepare(48000.0);
    float finalOutput = 0.0f;
    for (int n = 0; n < 480000; ++n)
        finalOutput = dcModel.processSample(0.5f);
    expect(std::abs(finalOutput) < 1.0e-5, "DC did not decay through coupling capacitors");
}

void testFiniteRecoveryAndExtremes()
{
    circuitpedal::DistortionPlusModel model;
    circuitpedal::CircuitCalibration calibration;
    calibration.inputVoltsPerFullScale = 0.75;
    calibration.outputFullScalePerVolt = 1.25;
    calibration.sourceResistanceOhms = 12000.0;
    calibration.outputLoadOhms = 500000.0;
    model.setCalibration(calibration);
    const auto storedCalibration = model.getCalibration();
    expect(storedCalibration.inputVoltsPerFullScale == calibration.inputVoltsPerFullScale
               && storedCalibration.outputFullScalePerVolt == calibration.outputFullScalePerVolt
               && storedCalibration.sourceResistanceOhms == calibration.sourceResistanceOhms
               && storedCalibration.outputLoadOhms == calibration.outputLoadOhms,
           "valid calibration was not retained");
    model.prepare(48000.0);
    expect(std::isfinite(model.processSample(std::numeric_limits<float>::quiet_NaN())),
           "NaN input escaped the core");
    expect(std::isfinite(model.processSample(0.1f)),
           "core did not recover after NaN input");
    expect(std::isfinite(model.processSample(std::numeric_limits<float>::infinity())),
           "infinite input escaped the core");
    expect(std::isfinite(model.processSample(0.1f)),
           "core did not recover after infinite input");

    model.setDistortion(std::numeric_limits<float>::quiet_NaN());
    model.setOutput(std::numeric_limits<float>::infinity());
    expect(std::isfinite(model.processSample(0.1f)),
           "non-finite parameter poisoned the core");

    model.setDistortion(1.0f);
    model.setOutput(1.0f);
    double peak = 0.0;
    for (int n = 0; n < 4096; ++n)
    {
        const float input = n == 0 ? 1.0f : (n == 1 ? -1.0f : 0.0f);
        const float output = model.processSample(input);
        expect(std::isfinite(output), "extreme transient produced a non-finite sample");
        peak = std::max(peak, std::abs(static_cast<double>(output)));
    }
    expect(peak <= 1.0, "extreme transient escaped output bounds");
}

void testDeterminismAndSymmetry()
{
    circuitpedal::DistortionPlusModel first;
    circuitpedal::DistortionPlusModel second;
    circuitpedal::DistortionPlusModel inverted;
    first.prepare(48000.0);
    second.prepare(48000.0);
    inverted.prepare(48000.0);

    double symmetryError = 0.0;
    for (int n = 0; n < 48000; ++n)
    {
        const float input = static_cast<float>(0.2 * std::sin(2.0 * pi * 997.0 * n / 48000.0));
        const float a = first.processSample(input);
        const float b = second.processSample(input);
        const float c = inverted.processSample(-input);
        expect(a == b, "identical model instances were not deterministic");
        symmetryError = std::max(symmetryError,
                                 std::abs(static_cast<double>(a) + static_cast<double>(c)));
    }
    expect(symmetryError < 1.0e-6, "symmetric circuit produced asymmetric clipping");
}

void testControlSweepsAndBypass()
{
    double precedingRms = -1.0;
    for (float outputControl : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        circuitpedal::DistortionPlusModel model;
        model.setOutput(outputControl);
        model.prepare(48000.0);
        const auto output = renderSine(model, 48000.0, 440.0, 0.12, 48000);
        const double rms = rmsAfterWarmup(output, 4096);
        expect(rms + 1.0e-8 >= precedingRms, "output sweep was not monotonic");
        precedingRms = rms;
    }

    circuitpedal::DistortionPlusModel bypass;
    bypass.prepare(48000.0);
    bypass.setBypass(true);
    std::vector<float> inputHistory(48000, 0.0f);
    double maximumError = 0.0;
    for (int n = 0; n < 48000; ++n)
    {
        const float input = static_cast<float>(0.1 * std::sin(2.0 * pi * 440.0 * n / 48000.0));
        inputHistory[static_cast<std::size_t>(n)] = input;
        const float output = bypass.processSample(input);
        if (n > 4096)
        {
            const float expected = inputHistory[static_cast<std::size_t>(
                n - static_cast<int>(circuitpedal::Oversampler4x::wetDelayHostSamples))];
            maximumError = std::max(maximumError,
                                    std::abs(static_cast<double>(output - expected)));
        }
    }
    expect(maximumError < 1.0e-5, "settled bypass was not delayed-dry transparent");

    double maximumDelta = 0.0;
    float previous = 0.0f;
    for (int n = 0; n < 48000; ++n)
    {
        if (n == 12000 || n == 36000)
            bypass.setBypass(!bypass.getBypass());
        if (n == 24000)
        {
            bypass.setDistortion(1.0f);
            bypass.setOutput(0.1f);
        }
        const float input = static_cast<float>(0.1 * std::sin(2.0 * pi * 440.0 * n / 48000.0));
        const float output = bypass.processSample(input);
        maximumDelta = std::max(maximumDelta,
                                std::abs(static_cast<double>(output - previous)));
        previous = output;
        expect(std::isfinite(output), "control change produced non-finite output");
    }
    expect(maximumDelta < 0.2, "smoothed control/bypass change produced an excessive discontinuity");
}

double thirdHarmonicRatio(float distortionControl)
{
    circuitpedal::DistortionPlusModel model;
    model.setDistortion(distortionControl);
    model.setOutput(1.0f);
    model.prepare(48000.0);

    double fundamentalSine = 0.0;
    double fundamentalCosine = 0.0;
    double thirdSine = 0.0;
    double thirdCosine = 0.0;
    constexpr int measuredSamples = 48000;
    for (int n = 0; n < measuredSamples * 2; ++n)
    {
        const double phase = 2.0 * pi * 1000.0 * n / 48000.0;
        const float output = model.processSample(static_cast<float>(0.2 * std::sin(phase)));
        if (n >= measuredSamples)
        {
            fundamentalSine += output * std::sin(phase);
            fundamentalCosine += output * std::cos(phase);
            thirdSine += output * std::sin(3.0 * phase);
            thirdCosine += output * std::cos(3.0 * phase);
        }
    }

    const double fundamental = std::hypot(fundamentalSine, fundamentalCosine);
    const double third = std::hypot(thirdSine, thirdCosine);
    return third / fundamental;
}

void testHarmonicBehaviour()
{
    const double minimumDriveRatio = thirdHarmonicRatio(0.0f);
    const double maximumDriveRatio = thirdHarmonicRatio(1.0f);
    expect(maximumDriveRatio > minimumDriveRatio * 1.5,
           "distortion control did not materially increase harmonic content");
}

void testDiodePresets()
{
    using circuitpedal::ClippingDiodePreset;

    const auto germanium =
        circuitpedal::diodeModelParameters(ClippingDiodePreset::ReferenceGermanium);
    const auto silicon =
        circuitpedal::diodeModelParameters(ClippingDiodePreset::SiliconLike);
    const auto led =
        circuitpedal::diodeModelParameters(ClippingDiodePreset::LedLike);
    const auto none =
        circuitpedal::diodeModelParameters(ClippingDiodePreset::NoDiodes);

    expect(germanium.saturationCurrentAmps > silicon.saturationCurrentAmps,
           "germanium and silicon presets did not have distinct diode curves");
    expect(led.idealityFactor > silicon.idealityFactor,
           "LED-like preset did not have a higher-threshold curve");
    expect(none.saturationCurrentAmps == 0.0,
           "no-diodes preset still enabled diode current");

    std::vector<double> rmsValues;
    for (const auto preset : {
             ClippingDiodePreset::ReferenceGermanium,
             ClippingDiodePreset::SiliconLike,
             ClippingDiodePreset::LedLike,
             ClippingDiodePreset::NoDiodes })
    {
        circuitpedal::DistortionPlusModel model;
        model.setClippingDiodePreset(preset);
        model.setDistortion(1.0f);
        model.setOutput(1.0f);
        model.prepare(48000.0);
        expect(model.getClippingDiodePreset() == preset,
               "diode preset was not retained");

        const auto output = renderSine(model, 48000.0, 997.0, 0.35, 48000);
        for (float sample : output)
            expect(std::isfinite(sample), "diode preset produced non-finite output");
        rmsValues.push_back(rmsAfterWarmup(output, 4096));
    }

    expect(std::abs(rmsValues[0] - rmsValues[1]) > 1.0e-4,
           "germanium and silicon presets were audibly/numerically indistinguishable");
    expect(std::abs(rmsValues[0] - rmsValues[3]) > 1.0e-4,
           "reference and no-diode presets were numerically indistinguishable");

    circuitpedal::DistortionPlusModel invalid;
    invalid.setClippingDiodePreset(static_cast<ClippingDiodePreset>(999U));
    expect(invalid.getClippingDiodePreset() == ClippingDiodePreset::ReferenceGermanium,
           "invalid diode preset did not fall back safely");
}

void testLongRunStability()
{
    circuitpedal::DistortionPlusModel model;
    model.prepare(48000.0);
    std::uint32_t randomState = 0x12345678U;
    for (int n = 0; n < 480000; ++n)
    {
        randomState = randomState * 1664525U + 1013904223U;
        const double normalized = static_cast<double>(randomState) / 4294967295.0;
        const float input = static_cast<float>((normalized * 2.0 - 1.0) * 0.4);
        const float output = model.processSample(input);
        expect(std::isfinite(output), "long random run produced non-finite output");
        expect(std::abs(output) <= 1.0f, "long random run exceeded output bounds");
    }
}

} // namespace

int main()
{
    testClipSolver();
    testOversampler();
    testSilenceDcAndSampleRates();
    testFiniteRecoveryAndExtremes();
    testDeterminismAndSymmetry();
    testControlSweepsAndBypass();
    testHarmonicBehaviour();
    testDiodePresets();
    testLongRunStability();

    if (failures != 0)
    {
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }

    std::cout << "PASS: CircuitPedal V0.4 validation suite\n";
    return 0;
}
