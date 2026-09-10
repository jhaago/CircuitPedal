#include "Validation.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int failures = 0;
constexpr double testPi = 3.14159265358979323846;

void expect(bool condition, const std::string& message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool near(double actual, double expected, double tolerance)
{
    return std::abs(actual - expected) <= tolerance;
}

void testSignalGeneration()
{
    circuitpedal::validation::SignalConfiguration signal;
    signal.kind = circuitpedal::validation::SignalKind::Sine;
    signal.sampleRate = 1000.0;
    signal.frequencyHz = 250.0;
    signal.amplitude = 1.0;

    expect(near(circuitpedal::validation::signalSample(signal, 0U), 0.0, 1.0e-12),
           "sine sample 0 was not zero");
    expect(near(circuitpedal::validation::signalSample(signal, 1U), 1.0, 1.0e-12),
           "sine quarter-cycle sample was not one");

    signal.kind = circuitpedal::validation::SignalKind::Impulse;
    signal.amplitude = 0.4;
    expect(near(circuitpedal::validation::signalSample(signal, 0U), 0.4, 1.0e-12),
           "impulse first sample was incorrect");
    expect(near(circuitpedal::validation::signalSample(signal, 1U), 0.0, 1.0e-12),
           "impulse did not return to zero");
}

circuitpedal::validation::Waveform makeSine(std::size_t count,
                                            double sampleRate,
                                            double frequency,
                                            double gain = 1.0,
                                            double phaseRadians = 0.0)
{
    circuitpedal::validation::Waveform waveform;
    waveform.sampleRate = sampleRate;
    waveform.timeSeconds.reserve(count);
    waveform.values.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        waveform.timeSeconds.push_back(static_cast<double>(i) / sampleRate);
        waveform.values.push_back(
            gain * std::sin(2.0 * testPi * frequency * static_cast<double>(i) / sampleRate
                            + phaseRadians));
    }
    return waveform;
}

void testIdenticalComparison()
{
    const auto reference = makeSine(2048U, 48000.0, 750.0);
    const auto actual = reference;
    circuitpedal::validation::ComparisonOptions options;
    options.maxLagSamples = 12;
    options.fundamentalHz = 750.0;
    options.harmonicCount = 3U;

    circuitpedal::validation::ComparisonMetrics metrics;
    std::string error;
    expect(circuitpedal::validation::compareWaveforms(
               reference, actual, options, metrics, error),
           "identical comparison failed: " + error);
    expect(metrics.lagSamples == 0, "identical waveform alignment was not zero");
    expect(metrics.rmsError < 1.0e-12, "identical waveform RMS error was nonzero");
    expect(metrics.peakAbsoluteError < 1.0e-12, "identical waveform peak error was nonzero");
    expect(metrics.normalizedRmsErrorPercent < 1.0e-9,
           "identical waveform normalized RMS error was nonzero");
    expect(metrics.correlation > 0.999999999,
           "identical waveform correlation was not one");
    expect(metrics.harmonics.size() == 3U,
           "harmonic comparison did not return requested harmonics");
    if (!metrics.harmonics.empty())
        expect(std::abs(metrics.harmonics.front().amplitudeErrorDb) < 1.0e-9,
               "identical fundamental amplitude error was nonzero");
}

void testDelayAlignment()
{
    const auto reference = makeSine(4096U, 48000.0, 937.5);
    auto actual = reference;
    constexpr std::size_t delay = 7U;
    for (std::size_t i = actual.values.size(); i-- > delay;)
        actual.values[i] = reference.values[i - delay];
    for (std::size_t i = 0; i < delay; ++i)
        actual.values[i] = 0.0;

    circuitpedal::validation::ComparisonOptions options;
    options.maxLagSamples = 12;
    circuitpedal::validation::ComparisonMetrics metrics;
    std::string error;
    expect(circuitpedal::validation::compareWaveforms(
               reference, actual, options, metrics, error),
           "delayed comparison failed: " + error);
    expect(metrics.lagSamples == static_cast<int>(delay),
           "integer delay alignment did not recover the known delay");
    expect(metrics.rmsError < 1.0e-12,
           "aligned delayed waveform still had RMS error");
}

void testGainError()
{
    const auto reference = makeSine(4096U, 48000.0, 750.0);
    const auto actual = makeSine(4096U, 48000.0, 750.0, 1.1);
    circuitpedal::validation::ComparisonOptions options;
    options.fundamentalHz = 750.0;
    options.harmonicCount = 1U;

    circuitpedal::validation::ComparisonMetrics metrics;
    std::string error;
    expect(circuitpedal::validation::compareWaveforms(
               reference, actual, options, metrics, error),
           "gain comparison failed: " + error);
    expect(near(metrics.normalizedRmsErrorPercent, 10.0, 0.05),
           "10 percent gain change did not produce about 10 percent NRMS error");
    expect(near(metrics.gainErrorDb, 20.0 * std::log10(1.1), 1.0e-6),
           "gain error dB was incorrect");
    if (!metrics.harmonics.empty())
        expect(near(metrics.harmonics.front().amplitudeErrorDb,
                    20.0 * std::log10(1.1), 1.0e-6),
               "fundamental amplitude error dB was incorrect");
}

void testCsvLoadingPrefersOutputVolts()
{
    const std::string path = "validation_test_waveform.csv";
    {
        std::ofstream output(path);
        output << "sample,time_s,input_fs,output_v,output_fs,converged,node_B1_v\n";
        output << "0,0.0,0.0,1.25,0.1,1,0.55\n";
        output << "1,0.001,0.0,1.50,0.2,1,0.56\n";
        output << "2,0.002,0.0,1.75,0.3,1,0.57\n";
    }

    circuitpedal::validation::Waveform waveform;
    std::string error;
    expect(circuitpedal::validation::loadWaveformCsv(path, waveform, error),
           "render CSV could not be reloaded: " + error);
    expect(waveform.values.size() == 3U, "CSV loader sample count was incorrect");
    if (waveform.values.size() == 3U)
    {
        expect(near(waveform.values[0], 1.25, 1.0e-12),
               "CSV loader did not prefer output_v");
        expect(near(waveform.sampleRate, 1000.0, 1.0e-9),
               "CSV sample-rate inference was incorrect");
    }

    circuitpedal::validation::Waveform nodeWaveform;
    expect(circuitpedal::validation::loadWaveformCsv(
               path, "NODE_b1_V", nodeWaveform, error),
           "named internal-node CSV column could not be loaded: " + error);
    if (nodeWaveform.values.size() == 3U)
    {
        expect(near(nodeWaveform.values[0], 0.55, 1.0e-12),
               "named CSV column did not select the internal-node trace");
        expect(near(nodeWaveform.values[2], 0.57, 1.0e-12),
               "named CSV column returned the wrong final sample");
    }

    circuitpedal::validation::Waveform missing;
    expect(!circuitpedal::validation::loadWaveformCsv(
               path, "node_missing_v", missing, error),
           "missing named CSV column was accepted");
    (void)std::remove(path.c_str());
}

void testDcReferenceLoading()
{
    const std::string path = "validation_test_dc_reference.csv";
    {
        std::ofstream output(path);
        output << "node,expected_v,relative_tolerance_percent,absolute_tolerance_v,notes\n";
        output << "B1,0.58,10,0.01,base\n";
        output << "C1_NODE,1.20,15,0,collector\n";
    }

    std::vector<circuitpedal::validation::DcReferencePoint> points;
    std::string error;
    expect(circuitpedal::validation::loadDcReferenceCsv(path, points, error),
           "DC reference CSV could not be loaded: " + error);
    expect(points.size() == 2U, "DC reference point count was incorrect");
    if (points.size() == 2U)
    {
        expect(points[0].nodeName == "B1", "DC reference node name mismatch");
        expect(near(points[0].expectedVolts, 0.58, 1.0e-12),
               "DC reference expected voltage mismatch");
        expect(near(points[0].relativeTolerancePercent, 10.0, 1.0e-12),
               "DC reference relative tolerance mismatch");
        expect(near(points[0].absoluteToleranceVolts, 0.01, 1.0e-12),
               "DC reference absolute tolerance mismatch");
    }
    (void)std::remove(path.c_str());
}

void testResamplingAndWindow()
{
    auto reference = makeSine(4801U, 48000.0, 1000.0);
    auto actual = makeSine(4411U, 44100.0, 1000.0);
    for (std::size_t i = 0; i < 500U; ++i)
        reference.values[i] += 0.5;
    for (std::size_t i = 0; i < 460U; ++i)
        actual.values[i] += 0.5;

    circuitpedal::validation::ComparisonOptions options;
    options.startTimeSeconds = 0.02;
    options.durationSeconds = 0.06;
    circuitpedal::validation::ComparisonMetrics metrics;
    std::string error;
    expect(circuitpedal::validation::compareWaveforms(
               reference, actual, options, metrics, error),
           "mismatched-rate windowed comparison failed: " + error);
    expect(metrics.normalizedRmsErrorPercent < 0.25,
           "linear resampling introduced excessive sine-wave error");

    options.allowResampling = false;
    expect(!circuitpedal::validation::compareWaveforms(
               reference, actual, options, metrics, error),
           "mismatched sample rates were accepted with resampling disabled");
}

void testPhaseAndThdMetrics()
{
    constexpr double sampleRate = 48000.0;
    constexpr double frequency = 1000.0;
    constexpr std::size_t count = 4800U;
    auto reference = makeSine(count, sampleRate, frequency);
    auto actual = makeSine(count, sampleRate, frequency, 1.0, testPi / 6.0);
    for (std::size_t i = 0; i < count; ++i)
    {
        const double time = static_cast<double>(i) / sampleRate;
        reference.values[i] += 1.0
            + 0.1 * std::sin(4.0 * testPi * frequency * time);
        actual.values[i] += 2.0
            + 0.2 * std::sin(4.0 * testPi * frequency * time + testPi / 6.0);
    }

    circuitpedal::validation::ComparisonOptions options;
    options.fundamentalHz = frequency;
    options.harmonicCount = 5U;
    circuitpedal::validation::ComparisonMetrics metrics;
    std::string error;
    expect(circuitpedal::validation::compareWaveforms(
               reference, actual, options, metrics, error),
           "phase/THD comparison failed: " + error);
    expect(!metrics.harmonics.empty()
               && near(metrics.harmonics.front().phaseErrorDegrees, 30.0, 0.2),
           "fundamental phase error was not recovered");
    expect(near(metrics.referenceThdPercent, 10.0, 0.2),
           "reference THD was not recovered");
    expect(near(metrics.actualThdPercent, 20.0, 0.3),
           "actual THD was not recovered");
    expect(near(metrics.thdErrorDb, 20.0 * std::log10(2.0), 0.1),
           "THD error in dB was incorrect");
}

} // namespace

int main()
{
    testSignalGeneration();
    testIdenticalComparison();
    testDelayAlignment();
    testGainError();
    testCsvLoadingPrefersOutputVolts();
    testDcReferenceLoading();
    testResamplingAndWindow();
    testPhaseAndThdMetrics();

    if (failures != 0)
    {
        std::cerr << failures << " validation framework test(s) failed.\n";
        return 1;
    }
    std::cout << "Validation framework tests passed.\n";
    return 0;
}
