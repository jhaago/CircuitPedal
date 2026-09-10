#include "Validation.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <string>
#include <vector>

namespace circuitpedal::validation {
namespace {

constexpr double pi = 3.1415926535897932384626433832795;
constexpr double tiny = 1.0e-15;

std::string trim(std::string text)
{
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const auto last = text.find_last_not_of(" \t\r\n");
    return text.substr(first, last - first + 1U);
}

std::string lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::vector<std::string> splitCsvLine(const std::string& line)
{
    std::vector<std::string> fields;
    std::string current;
    bool quoted = false;
    for (char c : line)
    {
        if (c == '"')
        {
            quoted = !quoted;
            continue;
        }
        if (c == ',' && !quoted)
        {
            fields.push_back(trim(current));
            current.clear();
        }
        else
        {
            current.push_back(c);
        }
    }
    fields.push_back(trim(current));
    return fields;
}

bool parseDouble(const std::string& text, double& value)
{
    try
    {
        std::size_t used = 0;
        const double parsed = std::stod(text, &used);
        if (used != text.size() || !std::isfinite(parsed))
            return false;
        value = parsed;
        return true;
    }
    catch (...)
    {
        return false;
    }
}

std::vector<std::string> normalizedHeaders(const std::vector<std::string>& headers)
{
    std::vector<std::string> normalized;
    normalized.reserve(headers.size());
    for (const auto& header : headers)
        normalized.push_back(lower(trim(header)));
    return normalized;
}

std::size_t findHeader(const std::vector<std::string>& headers,
                       const std::vector<std::string>& candidates) noexcept
{
    for (const auto& candidate : candidates)
    {
        const auto found = std::find(headers.begin(), headers.end(), candidate);
        if (found != headers.end())
            return static_cast<std::size_t>(std::distance(headers.begin(), found));
    }
    return headers.size();
}

struct AlignedRange {
    std::size_t referenceStart = 0;
    std::size_t actualStart = 0;
    std::size_t count = 0;
};

AlignedRange alignedRange(std::size_t referenceSize,
                          std::size_t actualSize,
                          int lag) noexcept
{
    AlignedRange range;
    if (lag >= 0)
    {
        const std::size_t delayed = static_cast<std::size_t>(lag);
        if (delayed >= actualSize)
            return range;
        range.actualStart = delayed;
        range.count = std::min(referenceSize, actualSize - delayed);
    }
    else
    {
        const std::size_t advanced = static_cast<std::size_t>(-lag);
        if (advanced >= referenceSize)
            return range;
        range.referenceStart = advanced;
        range.count = std::min(referenceSize - advanced, actualSize);
    }
    return range;
}

double correlationForLag(const Waveform& reference,
                         const Waveform& actual,
                         int lag) noexcept
{
    const AlignedRange range = alignedRange(reference.values.size(), actual.values.size(), lag);
    if (range.count < 8U)
        return -2.0;

    double referenceMean = 0.0;
    double actualMean = 0.0;
    for (std::size_t i = 0; i < range.count; ++i)
    {
        referenceMean += reference.values[range.referenceStart + i];
        actualMean += actual.values[range.actualStart + i];
    }
    referenceMean /= static_cast<double>(range.count);
    actualMean /= static_cast<double>(range.count);

    double numerator = 0.0;
    double referenceEnergy = 0.0;
    double actualEnergy = 0.0;
    for (std::size_t i = 0; i < range.count; ++i)
    {
        const double r = reference.values[range.referenceStart + i] - referenceMean;
        const double a = actual.values[range.actualStart + i] - actualMean;
        numerator += r * a;
        referenceEnergy += r * r;
        actualEnergy += a * a;
    }

    const double denominator = std::sqrt(referenceEnergy * actualEnergy);
    if (denominator <= tiny)
        return 0.0;
    return numerator / denominator;
}

double rmsRange(const std::vector<double>& values,
                std::size_t start,
                std::size_t count) noexcept
{
    if (count == 0U)
        return 0.0;
    double sumSquares = 0.0;
    for (std::size_t i = 0; i < count; ++i)
    {
        const double value = values[start + i];
        sumSquares += value * value;
    }
    return std::sqrt(sumSquares / static_cast<double>(count));
}

double harmonicAmplitude(const std::vector<double>& values,
                         std::size_t start,
                         std::size_t count,
                         double sampleRate,
                         double frequencyHz) noexcept
{
    if (count < 4U || sampleRate <= 0.0 || frequencyHz <= 0.0)
        return 0.0;

    double sineProjection = 0.0;
    double cosineProjection = 0.0;
    double weightSum = 0.0;
    for (std::size_t i = 0; i < count; ++i)
    {
        const double phasePosition = static_cast<double>(i) / static_cast<double>(count - 1U);
        const double weight = 0.5 - 0.5 * std::cos(2.0 * pi * phasePosition);
        const double phase = 2.0 * pi * frequencyHz * static_cast<double>(i) / sampleRate;
        const double value = values[start + i];
        sineProjection += weight * value * std::sin(phase);
        cosineProjection += weight * value * std::cos(phase);
        weightSum += weight;
    }

    if (weightSum <= tiny)
        return 0.0;
    return 2.0 * std::sqrt(sineProjection * sineProjection
                           + cosineProjection * cosineProjection)
        / weightSum;
}

double amplitudeErrorDb(double referenceAmplitude,
                        double actualAmplitude) noexcept
{
    const double referenceSafe = std::max(referenceAmplitude, tiny);
    const double actualSafe = std::max(actualAmplitude, tiny);
    return 20.0 * std::log10(actualSafe / referenceSafe);
}

} // namespace

double signalSample(const SignalConfiguration& configuration,
                    std::size_t sampleIndex) noexcept
{
    if (!std::isfinite(configuration.sampleRate)
        || configuration.sampleRate <= 0.0
        || !std::isfinite(configuration.amplitude))
    {
        return 0.0;
    }

    const double time = static_cast<double>(sampleIndex) / configuration.sampleRate;
    switch (configuration.kind)
    {
    case SignalKind::Sine:
        return configuration.amplitude
            * std::sin(2.0 * pi * configuration.frequencyHz * time);
    case SignalKind::Step:
        return sampleIndex == 0U ? 0.0 : configuration.amplitude;
    case SignalKind::Impulse:
        return sampleIndex == 0U ? configuration.amplitude : 0.0;
    case SignalKind::DualTone:
        return 0.5 * configuration.amplitude
            * (std::sin(2.0 * pi * configuration.frequencyHz * time)
               + std::sin(2.0 * pi * configuration.secondFrequencyHz * time));
    case SignalKind::LogSweep:
    {
        if (configuration.durationSeconds <= 0.0
            || configuration.frequencyHz <= 0.0
            || configuration.sweepEndFrequencyHz <= configuration.frequencyHz)
        {
            return 0.0;
        }
        const double ratio = configuration.sweepEndFrequencyHz / configuration.frequencyHz;
        const double k = std::log(ratio) / configuration.durationSeconds;
        const double phase = 2.0 * pi * configuration.frequencyHz
            * (std::exp(k * time) - 1.0) / k;
        return configuration.amplitude * std::sin(phase);
    }
    }
    return 0.0;
}

bool writeRenderCsv(const std::string& path,
                    const std::vector<RenderedSample>& samples,
                    std::string& error)
{
    error.clear();
    std::ofstream output(path);
    if (!output)
    {
        error = "Could not open render CSV for writing: " + path;
        return false;
    }

    output << "sample,time_s,input_fs,output_v,output_fs,converged\n";
    output << std::setprecision(17);
    for (const auto& sample : samples)
    {
        output << sample.sampleIndex << ','
               << sample.timeSeconds << ','
               << sample.inputFullScale << ','
               << sample.outputVolts << ','
               << sample.outputFullScale << ','
               << (sample.converged ? 1 : 0) << '\n';
    }

    if (!output)
    {
        error = "Failed while writing render CSV: " + path;
        return false;
    }
    return true;
}

bool loadWaveformCsv(const std::string& path,
                     Waveform& waveform,
                     std::string& error)
{
    return loadWaveformCsv(path, std::string {}, waveform, error);
}

bool loadWaveformCsv(const std::string& path,
                     const std::string& valueColumnName,
                     Waveform& waveform,
                     std::string& error)
{
    error.clear();
    waveform = {};

    std::ifstream input(path);
    if (!input)
    {
        error = "Could not open waveform CSV: " + path;
        return false;
    }

    std::string line;
    if (!std::getline(input, line))
    {
        error = "Waveform CSV is empty: " + path;
        return false;
    }

    const auto headers = splitCsvLine(line);
    const auto normalized = normalizedHeaders(headers);
    const std::size_t timeColumn = findHeader(normalized, { "time_s", "time" });

    std::size_t valueColumn = headers.size();
    if (!trim(valueColumnName).empty())
    {
        const std::string requested = lower(trim(valueColumnName));
        const auto found = std::find(normalized.begin(), normalized.end(), requested);
        if (found != normalized.end())
            valueColumn = static_cast<std::size_t>(std::distance(normalized.begin(), found));
    }
    else
    {
        valueColumn = findHeader(
            normalized, { "output_v", "output_volts", "output", "value", "v(out)" });
    }

    if (timeColumn == headers.size())
    {
        error = "Waveform CSV needs a time_s or time column.";
        return false;
    }
    if (valueColumn == headers.size())
    {
        if (!trim(valueColumnName).empty())
            error = "Waveform CSV does not contain requested value column: " + valueColumnName;
        else
            error = "Waveform CSV needs an output_v/output/value column.";
        return false;
    }

    std::size_t lineNumber = 1U;
    while (std::getline(input, line))
    {
        ++lineNumber;
        if (trim(line).empty())
            continue;
        const auto fields = splitCsvLine(line);
        if (timeColumn >= fields.size() || valueColumn >= fields.size())
        {
            error = "Malformed waveform CSV row at line " + std::to_string(lineNumber) + '.';
            return false;
        }

        double time = 0.0;
        double value = 0.0;
        if (!parseDouble(fields[timeColumn], time)
            || !parseDouble(fields[valueColumn], value))
        {
            error = "Non-numeric waveform CSV value at line " + std::to_string(lineNumber) + '.';
            return false;
        }
        waveform.timeSeconds.push_back(time);
        waveform.values.push_back(value);
    }

    if (waveform.values.size() < 2U)
    {
        error = "Waveform CSV needs at least two samples.";
        return false;
    }

    double deltaSum = 0.0;
    std::size_t deltaCount = 0U;
    const std::size_t checkCount = std::min<std::size_t>(
        waveform.timeSeconds.size() - 1U, 1024U);
    for (std::size_t i = 0; i < checkCount; ++i)
    {
        const double delta = waveform.timeSeconds[i + 1U] - waveform.timeSeconds[i];
        if (!std::isfinite(delta) || delta <= 0.0)
        {
            error = "Waveform CSV time values must increase strictly.";
            return false;
        }
        deltaSum += delta;
        ++deltaCount;
    }
    const double averageDelta = deltaSum / static_cast<double>(deltaCount);
    waveform.sampleRate = 1.0 / averageDelta;
    return true;
}

bool loadDcReferenceCsv(const std::string& path,
                        std::vector<DcReferencePoint>& points,
                        std::string& error)
{
    error.clear();
    points.clear();

    std::ifstream input(path);
    if (!input)
    {
        error = "Could not open DC reference CSV: " + path;
        return false;
    }

    std::string line;
    if (!std::getline(input, line))
    {
        error = "DC reference CSV is empty: " + path;
        return false;
    }

    const auto headers = splitCsvLine(line);
    const auto normalized = normalizedHeaders(headers);
    const std::size_t nodeColumn = findHeader(normalized, { "node", "node_name" });
    const std::size_t expectedColumn = findHeader(
        normalized, { "expected_v", "expected_volts" });
    const std::size_t relativeColumn = findHeader(
        normalized, { "relative_tolerance_percent", "tolerance_percent" });
    const std::size_t absoluteColumn = findHeader(
        normalized, { "absolute_tolerance_v", "absolute_tolerance_volts" });

    if (nodeColumn == headers.size() || expectedColumn == headers.size())
    {
        error = "DC reference CSV needs node and expected_v columns.";
        return false;
    }
    if (relativeColumn == headers.size() && absoluteColumn == headers.size())
    {
        error = "DC reference CSV needs a relative or absolute tolerance column.";
        return false;
    }

    std::size_t lineNumber = 1U;
    while (std::getline(input, line))
    {
        ++lineNumber;
        if (trim(line).empty())
            continue;
        const auto fields = splitCsvLine(line);
        const std::size_t requiredMax = std::max(nodeColumn, expectedColumn);
        if (requiredMax >= fields.size())
        {
            error = "Malformed DC reference row at line " + std::to_string(lineNumber) + '.';
            return false;
        }

        DcReferencePoint point;
        point.nodeName = trim(fields[nodeColumn]);
        if (point.nodeName.empty()
            || !parseDouble(fields[expectedColumn], point.expectedVolts))
        {
            error = "Invalid DC reference node/value at line " + std::to_string(lineNumber) + '.';
            return false;
        }

        if (relativeColumn < fields.size() && !trim(fields[relativeColumn]).empty())
        {
            if (!parseDouble(fields[relativeColumn], point.relativeTolerancePercent)
                || point.relativeTolerancePercent < 0.0)
            {
                error = "Invalid relative DC tolerance at line " + std::to_string(lineNumber) + '.';
                return false;
            }
        }
        if (absoluteColumn < fields.size() && !trim(fields[absoluteColumn]).empty())
        {
            if (!parseDouble(fields[absoluteColumn], point.absoluteToleranceVolts)
                || point.absoluteToleranceVolts < 0.0)
            {
                error = "Invalid absolute DC tolerance at line " + std::to_string(lineNumber) + '.';
                return false;
            }
        }
        if (point.relativeTolerancePercent <= 0.0
            && point.absoluteToleranceVolts <= 0.0)
        {
            error = "DC reference row has no positive tolerance at line "
                + std::to_string(lineNumber) + '.';
            return false;
        }

        const std::string normalizedName = lower(point.nodeName);
        for (const auto& existing : points)
        {
            if (lower(existing.nodeName) == normalizedName)
            {
                error = "Duplicate DC reference node: " + point.nodeName;
                return false;
            }
        }
        points.push_back(point);
    }

    if (points.empty())
    {
        error = "DC reference CSV contains no reference points.";
        return false;
    }
    return true;
}

bool compareWaveforms(const Waveform& reference,
                      const Waveform& actual,
                      const ComparisonOptions& options,
                      ComparisonMetrics& metrics,
                      std::string& error)
{
    error.clear();
    metrics = {};

    if (reference.values.size() < 8U || actual.values.size() < 8U)
    {
        error = "Waveform comparison needs at least eight samples in each file.";
        return false;
    }
    if (!std::isfinite(reference.sampleRate) || !std::isfinite(actual.sampleRate)
        || reference.sampleRate <= 0.0 || actual.sampleRate <= 0.0)
    {
        error = "Waveform comparison requires valid sample rates.";
        return false;
    }

    const double relativeRateDifference =
        std::abs(reference.sampleRate - actual.sampleRate) / reference.sampleRate;
    if (relativeRateDifference > 1.0e-3)
    {
        error = "Reference and actual CSV sample rates differ by more than 0.1%.";
        return false;
    }

    const std::size_t shortest = std::min(reference.values.size(), actual.values.size());
    const int maximumAllowedLag = shortest > 16U
        ? static_cast<int>(std::min<std::size_t>(shortest / 2U, 100000U))
        : 0;
    const int requestedLag = std::max(0, options.maxLagSamples);
    const int maxLag = std::min(requestedLag, maximumAllowedLag);

    int bestLag = 0;
    double bestCorrelation = correlationForLag(reference, actual, 0);
    for (int lag = -maxLag; lag <= maxLag; ++lag)
    {
        const double candidate = correlationForLag(reference, actual, lag);
        if (candidate > bestCorrelation + 1.0e-12
            || (std::abs(candidate - bestCorrelation) <= 1.0e-12
                && std::abs(lag) < std::abs(bestLag)))
        {
            bestCorrelation = candidate;
            bestLag = lag;
        }
    }

    const AlignedRange range = alignedRange(reference.values.size(), actual.values.size(), bestLag);
    if (range.count < 8U)
    {
        error = "Alignment left too few samples to compare.";
        return false;
    }

    double errorSquares = 0.0;
    double peakError = 0.0;
    double referenceMean = 0.0;
    double actualMean = 0.0;
    for (std::size_t i = 0; i < range.count; ++i)
    {
        const double r = reference.values[range.referenceStart + i];
        const double a = actual.values[range.actualStart + i];
        const double difference = a - r;
        errorSquares += difference * difference;
        peakError = std::max(peakError, std::abs(difference));
        referenceMean += r;
        actualMean += a;
    }
    referenceMean /= static_cast<double>(range.count);
    actualMean /= static_cast<double>(range.count);

    metrics.lagSamples = bestLag;
    metrics.comparedSamples = range.count;
    metrics.referenceRms = rmsRange(reference.values, range.referenceStart, range.count);
    metrics.actualRms = rmsRange(actual.values, range.actualStart, range.count);
    metrics.rmsError = std::sqrt(errorSquares / static_cast<double>(range.count));
    metrics.normalizedRmsErrorPercent = metrics.referenceRms > tiny
        ? 100.0 * metrics.rmsError / metrics.referenceRms
        : (metrics.rmsError <= tiny ? 0.0 : std::numeric_limits<double>::infinity());
    metrics.peakAbsoluteError = peakError;
    metrics.dcError = actualMean - referenceMean;
    metrics.gainErrorDb = amplitudeErrorDb(metrics.referenceRms, metrics.actualRms);
    metrics.correlation = bestCorrelation;

    if (options.fundamentalHz > 0.0 && options.harmonicCount > 0U)
    {
        metrics.harmonics.reserve(options.harmonicCount);
        for (std::size_t harmonic = 1U; harmonic <= options.harmonicCount; ++harmonic)
        {
            const double frequency = options.fundamentalHz * static_cast<double>(harmonic);
            if (frequency >= 0.5 * reference.sampleRate)
                break;
            const double referenceAmplitude = harmonicAmplitude(
                reference.values, range.referenceStart, range.count,
                reference.sampleRate, frequency);
            const double actualAmplitude = harmonicAmplitude(
                actual.values, range.actualStart, range.count,
                actual.sampleRate, frequency);
            metrics.harmonics.push_back({
                harmonic,
                frequency,
                referenceAmplitude,
                actualAmplitude,
                amplitudeErrorDb(referenceAmplitude, actualAmplitude)
            });
        }
    }

    return true;
}

} // namespace circuitpedal::validation
