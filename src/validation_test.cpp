#include "Validation.h"

#include <cmath>
#include <cstdio>
#include <fstream>
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
                                            double gain = 1.0)
{
    circuitpedal::validation::Waveform waveform;
    waveform.sampleRate = sampleRate;
    waveform.timeSeconds.reserve(count);
    waveform.values.reserve(count);
    constexpr double pi = 3.14159265358979323846;
    for (std::size_t i = 0; i < count; ++i)
    {
        waveform.timeSeconds.push_back(static_cast<double>(i) / sampleRate);
        waveform.values.push_back(
            gain * std::sin(2.0 * pi * frequency * static_cast<double>(i) / sampleRate));
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
        output << "sample,time_s,input_fs,output_v,output_fs,converged\n";
        output << "0,0.0,0.0,1.25,0.1,1\n";
        output << "1,0.001,0.0,1.50,0.2,1\n";
        output << "2,0.002,0.0,1.75,0.3,1\n";
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
    (void)std::remove(path.c_str());
}

} // namespace

int main()
{
    testSignalGeneration();
    testIdenticalComparison();
    testDelayAlignment();
    testGainError();
    testCsvLoadingPrefersOutputVolts();

    if (failures != 0)
    {
        std::cerr << failures << " validation framework test(s) failed.\n";
        return 1;
    }
    std::cout << "Validation framework tests passed.\n";
    return 0;
}
