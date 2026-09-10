#include "CircuitFile.h"
#include "GenericCircuit.h"
#include "Validation.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace {

using circuitpedal::CircuitDefinition;
using circuitpedal::CircuitFileControlKind;
using circuitpedal::CircuitFileDocument;
using circuitpedal::CircuitNode;
using circuitpedal::GenericCircuit;
using circuitpedal::validation::ComparisonMetrics;
using circuitpedal::validation::ComparisonOptions;
using circuitpedal::validation::DcReferencePoint;
using circuitpedal::validation::SignalConfiguration;
using circuitpedal::validation::SignalKind;
using circuitpedal::validation::Waveform;

struct CircuitStateOptions {
    double sampleRate = 192000.0;
    std::vector<std::pair<std::string, double>> potOverrides;
    std::vector<std::pair<std::string, std::size_t>> switchOverrides;
};

struct NodeProbe {
    std::string name;
    std::string columnName;
    CircuitNode node = circuitpedal::circuitGround;
};

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

bool validSampleRate(double sampleRate) noexcept
{
    return std::isfinite(sampleRate)
        && sampleRate >= 8000.0
        && sampleRate <= 384000.0;
}

std::string probeColumnName(const std::string& nodeName)
{
    std::string result = "node_";
    result.reserve(nodeName.size() + 7U);
    for (unsigned char c : nodeName)
    {
        if (std::isalnum(c) != 0 || c == '_')
            result.push_back(static_cast<char>(c));
        else
            result.push_back('_');
    }
    result += "_v";
    return result;
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
        << "  --switch NAME=POSITION   set a switch before the DC solve; repeatable\n"
        << "  --node NAME              export an internal node-voltage column; repeatable\n\n"
        << "Print a circuit DC operating point:\n"
        << "  circuitpedal_validate dc <circuit.cpedal> [options]\n"
        << "  Uses --sample-rate/--control/--switch and optional repeatable --node NAME.\n"
        << "  With no --node arguments, every circuit node is printed.\n\n"
        << "Check a DC operating point against a reference table:\n"
        << "  circuitpedal_validate dc-check <circuit.cpedal> <reference.csv> [options]\n"
        << "  Uses --sample-rate/--control/--switch.\n\n"
        << "Compare two uniformly sampled waveform CSV files:\n"
        << "  circuitpedal_validate compare <reference.csv> <actual.csv> [options]\n\n"
        << "Compare options:\n"
        << "  --column <name>          use this value column in both CSV files\n"
        << "  --reference-column <n>  select the reference CSV value column\n"
        << "  --actual-column <name>   select the actual CSV value column\n"
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

bool parseStateOption(const std::string& option,
                      const std::string& value,
                      CircuitStateOptions& state,
                      std::string& error)
{
    if (option == "--sample-rate")
    {
        if (!parseDouble(value, state.sampleRate) || !validSampleRate(state.sampleRate))
        {
            error = "Sample rate must be from 8 kHz to 384 kHz.";
            return false;
        }
        return true;
    }
    if (option == "--control")
    {
        std::string name;
        std::string positionText;
        double position = 0.0;
        if (!splitAssignment(value, name, positionText)
            || !parseDouble(positionText, position)
            || position < 0.0 || position > 1.0)
        {
            error = "Expected --control NAME=0..1";
            return false;
        }
        state.potOverrides.emplace_back(name, position);
        return true;
    }
    if (option == "--switch")
    {
        std::string name;
        std::string positionText;
        std::size_t position = 0U;
        if (!splitAssignment(value, name, positionText)
            || !parseUnsigned(positionText, position))
        {
            error = "Expected --switch NAME=POSITION";
            return false;
        }
        state.switchOverrides.emplace_back(name, position);
        return true;
    }
    error = "Unknown circuit-state option: " + option;
    return false;
}

bool isStateOption(const std::string& option) noexcept
{
    return option == "--sample-rate"
        || option == "--control"
        || option == "--switch";
}

bool prepareDocument(const std::string& circuitPath,
                     const CircuitStateOptions& state,
                     CircuitFileDocument& document,
                     std::string& error)
{
    if (!circuitpedal::loadCircuitFile(circuitPath, document, error))
    {
        error = "Could not load circuit: " + error;
        return false;
    }
    for (const auto& overrideValue : state.potOverrides)
    {
        if (!applyPotOverride(document, overrideValue.first, overrideValue.second, error))
            return false;
    }
    for (const auto& overrideValue : state.switchOverrides)
    {
        if (!applySwitchOverride(document, overrideValue.first, overrideValue.second, error))
            return false;
    }
    return true;
}

bool resolveNode(const CircuitDefinition& definition,
                 const std::string& requestedName,
                 NodeProbe& probe,
                 std::string& error)
{
    const std::string requestedUpper = upper(requestedName);
    for (std::size_t i = 0; i < definition.nodeCount(); ++i)
    {
        const auto node = static_cast<CircuitNode>(i);
        const std::string& candidate = definition.nodeName(node);
        if (upper(candidate) == requestedUpper)
        {
            probe.name = candidate;
            probe.columnName = probeColumnName(candidate);
            probe.node = node;
            return true;
        }
    }
    error = "Unknown circuit node: " + requestedName;
    return false;
}

bool resolveNodes(const CircuitDefinition& definition,
                  const std::vector<std::string>& requestedNames,
                  std::vector<NodeProbe>& probes,
                  std::string& error)
{
    probes.clear();
    if (requestedNames.size() > 64U)
    {
        error = "A render may export at most 64 internal nodes.";
        return false;
    }
    for (const auto& requestedName : requestedNames)
    {
        NodeProbe probe;
        if (!resolveNode(definition, requestedName, probe, error))
            return false;
        const auto duplicate = std::find_if(
            probes.begin(), probes.end(),
            [&probe](const NodeProbe& existing) { return existing.node == probe.node; });
        if (duplicate != probes.end())
        {
            error = "Duplicate circuit node probe: " + probe.name;
            return false;
        }
        const auto duplicateColumn = std::find_if(
            probes.begin(), probes.end(),
            [&probe](const NodeProbe& existing) {
                return upper(existing.columnName) == upper(probe.columnName);
            });
        if (duplicateColumn != probes.end())
        {
            error = "Node names collide after CSV column normalization: " + probe.name;
            return false;
        }
        probes.push_back(probe);
    }
    return true;
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
    CircuitStateOptions state;
    state.sampleRate = signal.sampleRate;
    std::vector<std::string> requestedNodes;
    std::string error;

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
        else if (option == "--node")
        {
            requestedNodes.push_back(value);
        }
        else if (isStateOption(option))
        {
            if (!parseStateOption(option, value, state, error))
            {
                std::cerr << error << '\n';
                return 2;
            }
            signal.sampleRate = state.sampleRate;
        }
        else
        {
            std::cerr << "Unknown render option: " << option << '\n';
            return 2;
        }
    }

    if (!validSampleRate(signal.sampleRate)
        || !std::isfinite(signal.durationSeconds) || signal.durationSeconds <= 0.0
        || signal.durationSeconds > 60.0)
    {
        std::cerr << "Use a sample rate from 8 kHz to 384 kHz and duration from 0 to 60 seconds.\n";
        return 2;
    }

    CircuitFileDocument document;
    if (!prepareDocument(circuitPath, state, document, error))
    {
        std::cerr << error << '\n';
        return 2;
    }

    std::vector<NodeProbe> probes;
    if (!resolveNodes(document.definition, requestedNodes, probes, error))
    {
        std::cerr << error << '\n';
        return 2;
    }

    GenericCircuit circuit;
    if (!circuit.compile(document.definition, signal.sampleRate, error))
    {
        std::cerr << "Could not compile circuit: " << error << '\n';
        return 2;
    }

    std::ofstream output(outputPath);
    if (!output)
    {
        std::cerr << "Could not open render CSV for writing: " << outputPath << '\n';
        return 2;
    }
    output << "sample,time_s,input_fs,output_v,output_fs,converged";
    for (const auto& probe : probes)
        output << ',' << probe.columnName;
    output << '\n' << std::setprecision(17);

    const double requestedSamples = signal.sampleRate * signal.durationSeconds;
    const std::size_t sampleCount = static_cast<std::size_t>(std::llround(requestedSamples));
    std::size_t convergenceFailures = 0U;
    const auto outputNode = document.definition.outputNode();

    for (std::size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        const double input = circuitpedal::validation::signalSample(signal, sampleIndex);
        const float outputFullScale = circuit.processSample(static_cast<float>(input));
        const bool converged = circuit.lastSolveConverged();
        if (!converged)
            ++convergenceFailures;

        output << sampleIndex << ','
               << static_cast<double>(sampleIndex) / signal.sampleRate << ','
               << input << ','
               << circuit.nodeVoltage(outputNode) << ','
               << static_cast<double>(outputFullScale) << ','
               << (converged ? 1 : 0);
        for (const auto& probe : probes)
            output << ',' << circuit.nodeVoltage(probe.node);
        output << '\n';
    }

    if (!output)
    {
        std::cerr << "Failed while writing render CSV: " << outputPath << '\n';
        return 2;
    }

    std::cout << "Rendered: " << document.name << '\n'
              << "Samples: " << sampleCount << " @ " << signal.sampleRate << " Hz\n"
              << "Output: " << outputPath << '\n'
              << "Internal node probes: " << probes.size() << '\n'
              << "Nonlinear solve failures: " << convergenceFailures << '\n';
    return convergenceFailures == 0U ? 0 : 3;
}

int dcCommand(int argc, const char* argv[])
{
    if (argc < 3)
    {
        printUsage();
        return 2;
    }

    const std::string circuitPath = argv[2];
    CircuitStateOptions state;
    std::vector<std::string> requestedNodes;
    std::string error;

    for (int i = 3; i < argc; ++i)
    {
        const std::string option = argv[i];
        if (i + 1 >= argc)
        {
            std::cerr << "Missing value for option: " << option << '\n';
            return 2;
        }
        const std::string value = argv[++i];
        if (option == "--node")
            requestedNodes.push_back(value);
        else if (isStateOption(option))
        {
            if (!parseStateOption(option, value, state, error))
            {
                std::cerr << error << '\n';
                return 2;
            }
        }
        else
        {
            std::cerr << "Unknown DC option: " << option << '\n';
            return 2;
        }
    }

    CircuitFileDocument document;
    if (!prepareDocument(circuitPath, state, document, error))
    {
        std::cerr << error << '\n';
        return 2;
    }
    GenericCircuit circuit;
    if (!circuit.compile(document.definition, state.sampleRate, error))
    {
        std::cerr << "Could not compile circuit: " << error << '\n';
        return 2;
    }

    std::vector<NodeProbe> probes;
    if (requestedNodes.empty())
    {
        requestedNodes.reserve(document.definition.nodeCount());
        for (std::size_t i = 0; i < document.definition.nodeCount(); ++i)
        {
            requestedNodes.push_back(
                document.definition.nodeName(static_cast<CircuitNode>(i)));
        }
    }
    if (!resolveNodes(document.definition, requestedNodes, probes, error))
    {
        std::cerr << error << '\n';
        return 2;
    }

    std::cout << std::setprecision(10)
              << "DC operating point: " << document.name << '\n'
              << "Solver sample rate: " << state.sampleRate << " Hz\n";
    for (const auto& probe : probes)
        std::cout << "  " << probe.name << " = " << circuit.nodeVoltage(probe.node) << " V\n";
    return 0;
}

int dcCheckCommand(int argc, const char* argv[])
{
    if (argc < 4)
    {
        printUsage();
        return 2;
    }

    const std::string circuitPath = argv[2];
    const std::string referencePath = argv[3];
    CircuitStateOptions state;
    std::string error;

    for (int i = 4; i < argc; ++i)
    {
        const std::string option = argv[i];
        if (i + 1 >= argc)
        {
            std::cerr << "Missing value for option: " << option << '\n';
            return 2;
        }
        const std::string value = argv[++i];
        if (!isStateOption(option)
            || !parseStateOption(option, value, state, error))
        {
            if (error.empty())
                error = "Unknown DC-check option: " + option;
            std::cerr << error << '\n';
            return 2;
        }
    }

    CircuitFileDocument document;
    if (!prepareDocument(circuitPath, state, document, error))
    {
        std::cerr << error << '\n';
        return 2;
    }
    GenericCircuit circuit;
    if (!circuit.compile(document.definition, state.sampleRate, error))
    {
        std::cerr << "Could not compile circuit: " << error << '\n';
        return 2;
    }

    std::vector<DcReferencePoint> referencePoints;
    if (!circuitpedal::validation::loadDcReferenceCsv(
            referencePath, referencePoints, error))
    {
        std::cerr << error << '\n';
        return 2;
    }

    bool passed = true;
    std::cout << std::setprecision(8)
              << "DC reference check: " << document.name << '\n'
              << "Reference: " << referencePath << '\n';
    for (const auto& reference : referencePoints)
    {
        NodeProbe probe;
        if (!resolveNode(document.definition, reference.nodeName, probe, error))
        {
            std::cerr << error << '\n';
            return 2;
        }
        const double actual = circuit.nodeVoltage(probe.node);
        const double difference = actual - reference.expectedVolts;
        const double absoluteError = std::abs(difference);
        const double relativeAllowance = std::abs(reference.expectedVolts)
            * reference.relativeTolerancePercent / 100.0;
        const double allowance = std::max(
            relativeAllowance, reference.absoluteToleranceVolts);
        const bool pointPassed = absoluteError <= allowance;
        passed = passed && pointPassed;

        std::cout << "  " << probe.name
                  << " expected=" << reference.expectedVolts << " V"
                  << " actual=" << actual << " V"
                  << " error=" << difference << " V"
                  << " allowed=±" << allowance << " V"
                  << " [" << (pointPassed ? "PASS" : "FAIL") << "]\n";
    }
    std::cout << (passed ? "DC reference check PASSED\n" : "DC reference check FAILED\n");
    return passed ? 0 : 3;
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
    std::string referenceColumn;
    std::string actualColumn;

    for (int i = 4; i < argc; ++i)
    {
        const std::string option = argv[i];
        if (i + 1 >= argc)
        {
            std::cerr << "Missing value for option: " << option << '\n';
            return 2;
        }
        const std::string value = argv[++i];
        if (option == "--column")
        {
            referenceColumn = value;
            actualColumn = value;
        }
        else if (option == "--reference-column")
        {
            referenceColumn = value;
        }
        else if (option == "--actual-column")
        {
            actualColumn = value;
        }
        else if (option == "--max-lag")
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
    const bool referenceLoaded = referenceColumn.empty()
        ? circuitpedal::validation::loadWaveformCsv(referencePath, reference, error)
        : circuitpedal::validation::loadWaveformCsv(
              referencePath, referenceColumn, reference, error);
    if (!referenceLoaded)
    {
        std::cerr << error << '\n';
        return 2;
    }

    const bool actualLoaded = actualColumn.empty()
        ? circuitpedal::validation::loadWaveformCsv(actualPath, actual, error)
        : circuitpedal::validation::loadWaveformCsv(
              actualPath, actualColumn, actual, error);
    if (!actualLoaded)
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
    if (command == "dc")
        return dcCommand(argc, argv);
    if (command == "dc-check")
        return dcCheckCommand(argc, argv);
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
