#include "CircuitFile.h"
#include "GenericCircuit.h"
#include "Validation.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using circuitpedal::CircuitFileControlKind;
using circuitpedal::CircuitFileDocument;
using circuitpedal::GenericCircuit;
using circuitpedal::validation::ComparisonMetrics;
using circuitpedal::validation::ComparisonOptions;
using circuitpedal::validation::RenderedSample;
using circuitpedal::validation::SignalConfiguration;
using circuitpedal::validation::SignalKind;
using circuitpedal::validation::Waveform;

std::string upper(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
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

bool parseUnsigned(const std::string& text, std::size_t& value)
{
    try
    {
        std::size_t used = 0;
        const unsigned long long parsed = std::stoull(text, &used);
        if (used != text.size())
            return false;
        value = static_cast<std::size_t>(parsed);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool splitAssignment(const std::string& text,
                     std::string& name,
                     std::string& value)
{
    const auto equals = text.find('=');
    if (equals == std::string::npos || equals == 0U || equals + 1U >= text.size())
        return false;
    name = text.substr(0U, equals);
    value = text.substr(equals + 1U);
    return true;
}

void printUsage()
{
    std::cout
        << "CircuitPedal validation tool\n\n"
        << "Render electrical solver output:\n"
        << "  circuitpedal_validate render <circuit.cpedal> <output.csv> [options]\n\n"
        << "Render options:\n"
        << "  --signal sine|step|impulse|dualtone|logsweep\n"
        << "  --sample-rate <Hz>       default 192000\n"
        << "  --seconds <s>            default 1.0\n"
        << "  --amplitude <FS>         default 0.25\n"
        << "  --frequency <Hz>         sine/dualtone/sweep start, default 440\n"
        << "  --frequency2 <Hz>        dualtone second tone, default 1000\n"
        << "  --sweep-end <Hz>         logsweep end, default 12000\n"
        << "  --control NAME=0..1      set a pot before the DC solve; repeatable\n"
        << "  --switch NAME=POSITION   set a switch before the DC solve; repeatable\n\n"
        << "Compare two uniformly sampled waveform CSV files:\n"
        << "  circuitpedal_validate compare <reference.csv> <actual.csv> [options]\n\n"
        << "Compare options:\n"
        << "  --max-lag <samples>      integer alignment search range\n"
        << "  --fundamental <Hz>       report harmonic amplitude errors\n"
        << "  --harmonics <count>      default 5 when --fundamental is used\n"
        << "  --max-nrms <percent>     fail when normalized RMS error is higher\n"
        << "  --max-peak <volts>       fail when peak absolute error is higher\n";
}

bool parseSignalKind(const std::string& text, SignalKind& kind)
{
    const std::string value = upper(text);
    if (value == "SINE")
        kind = SignalKind::Sine;
    else if (value == "STEP")
        kind = SignalKind::Step;
    else if (value == "IMPULSE")
        kind = SignalKind::Impulse;
    else if (value == "DUALTONE" || value == "DUAL_TONE")
        kind = SignalKind::DualTone;
    else if (value == "LOGSWEEP" || value == "LOG_SWEEP")
        kind = SignalKind::LogSweep;
    else
        return false;
    return true;
}

bool applyPotOverride(CircuitFileDocument& document,
                      const std::string& requestedName,
                      double position,
                      std::string& error)
{
    if (!std::isfinite(position) || position < 0.0 || position > 1.0)
    {
        error = "Pot override must be between 0 and 1: " + requestedName;
        return false;
    }
    const std::string target = upper(requestedName);
    for (const auto& control : document.controls)
    {
        if (control.kind != CircuitFileControlKind::Potentiometer
            || upper(control.name) != target)
        {
            continue;
        }
        if (!document.definition.setPotentiometerPosition(
                control.potentiometerIndex, position))
        {
            error = "Could not set pot control: " + requestedName;
            return false;
        }
        for (const auto linked : control.linkedPotentiometerIndices)
        {
            if (!document.definition.setPotentiometerPosition(linked, position))
            {
                error = "Could not set linked pot section: " + requestedName;
                return false;
            }
        }
        return true;
    }
    error = "Unknown pot control: " + requestedName;
    return false;
}

bool applySwitchOverride(CircuitFileDocument& document,
                         const std::string& requestedName,
                         std::size_t position,
                         std::string& error)
{
    const std::string target = upper(requestedName);
    for (const auto& control : document.controls)
    {
        if (control.kind != CircuitFileControlKind::Switch
            || upper(control.name) != target)
        {
            continue;
        }
        if (position >= static_cast<std::size_t>(control.switchPositionCount))
        {
            error = "Switch position out of range for " + requestedName;
            return false;
        }
        const auto switchPosition = static_cast<std::uint32_t>(position);
        if (!document.definition.setSwitchPosition(control.switchIndex, switchPosition))
        {
            error = "Could not set switch control: " + requestedName;
            return false;
        }
        for (const auto linked : control.linkedSwitchIndices)
        {
            if (!document.definition.setSwitchPosition(linked, switchPosition))
            {
                error = "Could not set linked switch pole: " + requestedName;
                return false;
            }
        }
        return true;
    }
    error = "Unknown switch control: " + requestedName;
    return false;
}

int renderCommand(int argc, const char* argv[])
{
    if (argc < 4)
    {
        printUsage();
        return 2;
    }

    const std::string circuitPath = argv[2];
    const std::string outputPath = argv[3];
    SignalConfiguration signal;
    std::vector<std::pair<std::string, double>> potOverrides;
    std::vector<std::pair<std::string, std::size_t>> switchOverrides;

    for (int i = 4; i < argc; ++i)
    {
        const std::string option = argv[i];
        if (i + 1 >= argc)
        {
            std::cerr << "Missing value for option: " << option << '\n';
            return 2;
        }
        const std::string value = argv[++i];
        if (option == "--signal")
        {
            if (!parseSignalKind(value, signal.kind))
            {
                std::cerr << "Unknown signal kind: " << value << '\n';
                return 2;
            }
        }
        else if (option == "--sample-rate")
        {
            if (!parseDouble(value, signal.sampleRate))
                return 2;
        }
        else if (option == "--seconds")
        {
            if (!parseDouble(value, signal.durationSeconds))
                return 2;
        }
        else if (option == "--amplitude")
        {
            if (!parseDouble(value, signal.amplitude))
                return 2;
        }
        else if (option == "--frequency")
        {
            if (!parseDouble(value, signal.frequencyHz))
                return 2;
        }
        else if (option == "--frequency2")
        {
            if (!parseDouble(value, signal.secondFrequencyHz))
                return 2;
        }
        else if (option == "--sweep-end")
        {
            if (!parseDouble(value, signal.sweepEndFrequencyHz))
                return 2;
        }
        else if (option == "--control")
        {
            std::string name;
            std::string positionText;
            double position = 0.0;
            if (!splitAssignment(value, name, positionText)
                || !parseDouble(positionText, position))
            {
                std::cerr << "Expected --control NAME=0..1\n";
                return 2;
            }
            potOverrides.emplace_back(name, position);
        }
        else if (option == "--switch")
        {
            std::string name;
            std::string positionText;
            std::size_t position = 0U;
            if (!splitAssignment(value, name, positionText)
                || !parseUnsigned(positionText, position))
            {
                std::cerr << "Expected --switch NAME=POSITION\n";
                return 2;
            }
            switchOverrides.emplace_back(name, position);
        }
        else
        {
            std::cerr << "Unknown render option: " << option << '\n';
            return 2;
        }
    }

    if (!std::isfinite(signal.sampleRate)
        || signal.sampleRate < 8000.0 || signal.sampleRate > 384000.0
        || !std::isfinite(signal.durationSeconds) || signal.durationSeconds <= 0.0
        || signal.durationSeconds > 60.0)
    {
        std::cerr << "Use a sample rate from 8 kHz to 384 kHz and duration from 0 to 60 seconds.\n";
        return 2;
    }

    CircuitFileDocument document;
    std::string error;
    if (!circuitpedal::loadCircuitFile(circuitPath, document, error))
    {
        std::cerr << "Could not load circuit: " << error << '\n';
        return 2;
    }

    for (const auto& overrideValue : potOverrides)
    {
        if (!applyPotOverride(document, overrideValue.first, overrideValue.second, error))
        {
            std::cerr << error << '\n';
            return 2;
        }
    }
    for (const auto& overrideValue : switchOverrides)
    {
        if (!applySwitchOverride(document, overrideValue.first, overrideValue.second, error))
        {
            std::cerr << error << '\n';
            return 2;
        }
    }

    GenericCircuit circuit;
    if (!circuit.compile(document.definition, signal.sampleRate, error))
    {
        std::cerr << "Could not compile circuit: " << error << '\n';
        return 2;
    }

    const double requestedSamples = signal.sampleRate * signal.durationSeconds;
    const std::size_t sampleCount = static_cast<std::size_t>(std::llround(requestedSamples));
    std::vector<RenderedSample> samples;
    samples.reserve(sampleCount);
    std::size_t convergenceFailures = 0U;

    const auto outputNode = document.definition.outputNode();
    for (std::size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        const double input = circuitpedal::validation::signalSample(signal, sampleIndex);
        const float outputFullScale = circuit.processSample(static_cast<float>(input));
        const bool converged = circuit.lastSolveConverged();
        if (!converged)
            ++convergenceFailures;
        samples.push_back({
            sampleIndex,
            static_cast<double>(sampleIndex) / signal.sampleRate,
            input,
            circuit.nodeVoltage(outputNode),
            static_cast<double>(outputFullScale),
            converged
        });
    }

    if (!circuitpedal::validation::writeRenderCsv(outputPath, samples, error))
    {
        std::cerr << error << '\n';
        return 2;
    }

    std::cout << "Rendered: " << document.name << '\n'
              << "Samples: " << sampleCount << " @ " << signal.sampleRate << " Hz\n"
              << "Output: " << outputPath << '\n'
              << "Nonlinear solve failures: " << convergenceFailures << '\n';
    return convergenceFailures == 0U ? 0 : 3;
}

int compareCommand(int argc, const char* argv[])
{
    if (argc < 4)
    {
        printUsage();
        return 2;
    }

    const std::string referencePath = argv[2];
    const std::string actualPath = argv[3];
    ComparisonOptions options;
    double maxNrms = -1.0;
    double maxPeak = -1.0;
    bool harmonicCountSpecified = false;

    for (int i = 4; i < argc; ++i)
    {
        const std::string option = argv[i];
        if (i + 1 >= argc)
        {
            std::cerr << "Missing value for option: " << option << '\n';
            return 2;
        }
        const std::string value = argv[++i];
        if (option == "--max-lag")
        {
            std::size_t parsed = 0U;
            if (!parseUnsigned(value, parsed) || parsed > 100000U)
                return 2;
            options.maxLagSamples = static_cast<int>(parsed);
        }
        else if (option == "--fundamental")
        {
            if (!parseDouble(value, options.fundamentalHz) || options.fundamentalHz <= 0.0)
                return 2;
        }
        else if (option == "--harmonics")
        {
            if (!parseUnsigned(value, options.harmonicCount) || options.harmonicCount > 64U)
                return 2;
            harmonicCountSpecified = true;
        }
        else if (option == "--max-nrms")
        {
            if (!parseDouble(value, maxNrms) || maxNrms < 0.0)
                return 2;
        }
        else if (option == "--max-peak")
        {
            if (!parseDouble(value, maxPeak) || maxPeak < 0.0)
                return 2;
        }
        else
        {
            std::cerr << "Unknown compare option: " << option << '\n';
            return 2;
        }
    }

    if (options.fundamentalHz > 0.0 && !harmonicCountSpecified)
        options.harmonicCount = 5U;

    Waveform reference;
    Waveform actual;
    std::string error;
    if (!circuitpedal::validation::loadWaveformCsv(referencePath, reference, error))
    {
        std::cerr << error << '\n';
        return 2;
    }
    if (!circuitpedal::validation::loadWaveformCsv(actualPath, actual, error))
    {
        std::cerr << error << '\n';
        return 2;
    }

    ComparisonMetrics metrics;
    if (!circuitpedal::validation::compareWaveforms(
            reference, actual, options, metrics, error))
    {
        std::cerr << error << '\n';
        return 2;
    }

    std::cout << std::setprecision(8)
              << "Aligned lag: " << metrics.lagSamples
              << " samples (positive = actual delayed)\n"
              << "Compared samples: " << metrics.comparedSamples << '\n'
              << "Correlation: " << metrics.correlation << '\n'
              << "Reference RMS: " << metrics.referenceRms << " V\n"
              << "Actual RMS: " << metrics.actualRms << " V\n"
              << "RMS error: " << metrics.rmsError << " V\n"
              << "Normalized RMS error: " << metrics.normalizedRmsErrorPercent << "%\n"
              << "Peak absolute error: " << metrics.peakAbsoluteError << " V\n"
              << "DC error: " << metrics.dcError << " V\n"
              << "RMS gain error: " << metrics.gainErrorDb << " dB\n";

    if (!metrics.harmonics.empty())
    {
        std::cout << "Harmonic amplitude comparison:\n";
        for (const auto& harmonic : metrics.harmonics)
        {
            std::cout << "  H" << harmonic.harmonic
                      << " " << harmonic.frequencyHz << " Hz"
                      << " ref=" << harmonic.referenceAmplitude
                      << " V actual=" << harmonic.actualAmplitude
                      << " V error=" << harmonic.amplitudeErrorDb << " dB\n";
        }
    }

    bool passed = true;
    if (maxNrms >= 0.0 && metrics.normalizedRmsErrorPercent > maxNrms)
    {
        std::cerr << "FAIL: normalized RMS error exceeds " << maxNrms << "%\n";
        passed = false;
    }
    if (maxPeak >= 0.0 && metrics.peakAbsoluteError > maxPeak)
    {
        std::cerr << "FAIL: peak absolute error exceeds " << maxPeak << " V\n";
        passed = false;
    }
    return passed ? 0 : 3;
}

} // namespace

int main(int argc, const char* argv[])
{
    if (argc < 2)
    {
        printUsage();
        return 2;
    }
    const std::string command = argv[1];
    if (command == "render")
        return renderCommand(argc, argv);
    if (command == "compare")
        return compareCommand(argc, argv);
    if (command == "--help" || command == "-h" || command == "help")
    {
        printUsage();
        return 0;
    }

    std::cerr << "Unknown command: " << command << '\n';
    printUsage();
    return 2;
}
