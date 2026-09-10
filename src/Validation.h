#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace circuitpedal::validation {

enum class SignalKind {
    Sine,
    Step,
    Impulse,
    DualTone,
    LogSweep
};

struct SignalConfiguration {
    SignalKind kind = SignalKind::Sine;
    double sampleRate = 192000.0;
    double amplitude = 0.25;
    double frequencyHz = 440.0;
    double secondFrequencyHz = 1000.0;
    double sweepEndFrequencyHz = 12000.0;
    double durationSeconds = 1.0;
};

double signalSample(const SignalConfiguration& configuration,
                    std::size_t sampleIndex) noexcept;

struct RenderedSample {
    std::size_t sampleIndex = 0;
    double timeSeconds = 0.0;
    double inputFullScale = 0.0;
    double outputVolts = 0.0;
    double outputFullScale = 0.0;
    bool converged = false;
};

bool writeRenderCsv(const std::string& path,
                    const std::vector<RenderedSample>& samples,
                    std::string& error);

struct Waveform {
    std::vector<double> timeSeconds;
    std::vector<double> values;
    double sampleRate = 0.0;
};

// Loads a comparison waveform from CSV. The default overload looks for
// output_v first, followed by output_volts, output, value and v(out). The named
// overload selects an exact column case-insensitively, which allows internal
// node traces exported by CircuitPedal or SPICE to be compared directly.
bool loadWaveformCsv(const std::string& path,
                     Waveform& waveform,
                     std::string& error);

bool loadWaveformCsv(const std::string& path,
                     const std::string& valueColumnName,
                     Waveform& waveform,
                     std::string& error);

// Linearly interpolates a uniformly or non-uniformly sampled waveform onto a
// uniform target rate while preserving its original time span.
bool resampleWaveform(const Waveform& source,
                      double targetSampleRate,
                      Waveform& result,
                      std::string& error);

struct DcReferencePoint {
    std::string nodeName;
    double expectedVolts = 0.0;
    double relativeTolerancePercent = 0.0;
    double absoluteToleranceVolts = 0.0;
};

// Loads a machine-readable operating-point reference table. Required columns
// are node and expected_v. At least one non-zero tolerance must be supplied by
// relative_tolerance_percent/tolerance_percent or absolute_tolerance_v.
bool loadDcReferenceCsv(const std::string& path,
                        std::vector<DcReferencePoint>& points,
                        std::string& error);

struct ComparisonOptions {
    int maxLagSamples = 0;
    double fundamentalHz = 0.0;
    std::size_t harmonicCount = 0;
    double startTimeSeconds = 0.0;
    double durationSeconds = 0.0; // Zero compares the remaining waveform.
    bool allowResampling = true;
};

struct HarmonicComparison {
    std::size_t harmonic = 0;
    double frequencyHz = 0.0;
    double referenceAmplitude = 0.0;
    double actualAmplitude = 0.0;
    double amplitudeErrorDb = 0.0;
    double referencePhaseDegrees = 0.0;
    double actualPhaseDegrees = 0.0;
    double phaseErrorDegrees = 0.0;
};

struct ComparisonMetrics {
    int lagSamples = 0; // Positive means the actual waveform is delayed.
    std::size_t comparedSamples = 0;
    double referenceRms = 0.0;
    double actualRms = 0.0;
    double rmsError = 0.0;
    double normalizedRmsErrorPercent = 0.0;
    double peakAbsoluteError = 0.0;
    double dcError = 0.0;
    double gainErrorDb = 0.0;
    double correlation = 0.0;
    double referenceThdPercent = 0.0;
    double actualThdPercent = 0.0;
    double thdErrorDb = 0.0;
    std::vector<HarmonicComparison> harmonics;
};

bool compareWaveforms(const Waveform& reference,
                      const Waveform& actual,
                      const ComparisonOptions& options,
                      ComparisonMetrics& metrics,
                      std::string& error);

} // namespace circuitpedal::validation
