#include "CircuitFile.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace circuitpedal {
namespace {

std::string upper(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

std::string trim(const std::string& text)
{
    const auto first = std::find_if_not(text.begin(), text.end(),
        [](unsigned char c) { return std::isspace(c) != 0; });
    if (first == text.end())
        return {};
    const auto last = std::find_if_not(text.rbegin(), text.rend(),
        [](unsigned char c) { return std::isspace(c) != 0; }).base();
    return std::string(first, last);
}

std::vector<std::string> tokenizeLine(const std::string& raw, std::string& error)
{
    std::vector<std::string> tokens;
    std::string current;
    bool quoted = false;
    bool escaped = false;

    for (std::size_t i = 0; i < raw.size(); ++i)
    {
        const char c = raw[i];
        if (!quoted && c == '#')
            break;
        if (!quoted && c == '/' && i + 1 < raw.size() && raw[i + 1] == '/')
            break;

        if (escaped)
        {
            current.push_back(c);
            escaped = false;
            continue;
        }
        if (quoted && c == '\\')
        {
            escaped = true;
            continue;
        }
        if (c == '"')
        {
            quoted = !quoted;
            continue;
        }
        if (!quoted && std::isspace(static_cast<unsigned char>(c)) != 0)
        {
            if (!current.empty())
            {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }
        current.push_back(c);
    }

    if (quoted)
    {
        error = "Unterminated quoted string.";
        return {};
    }
    if (!current.empty())
        tokens.push_back(current);
    return tokens;
}

CircuitNode nodeFor(CircuitDefinition& definition, const std::string& token)
{
    const std::string normalized = upper(token);
    if (normalized == "0" || normalized == "GND" || normalized == "GROUND")
        return circuitGround;
    return definition.addNode(token);
}

bool parseFinite(const std::string& text, double& value)
{
    bool ok = false;
    value = parseEngineeringValue(text, ok);
    return ok && std::isfinite(value);
}

double taperExponent(const std::string& token, bool& ok)
{
    const std::string normalized = upper(token);
    if (normalized == "LIN" || normalized == "LINEAR")
    {
        ok = true;
        return 1.0;
    }
    if (normalized == "LOG" || normalized == "AUDIO")
    {
        ok = true;
        return 3.321928094887362;
    }
    constexpr const char* prefix = "EXP:";
    if (normalized.rfind(prefix, 0) == 0)
    {
        double exponent = 0.0;
        ok = parseFinite(token.substr(4), exponent) && exponent > 0.0;
        return exponent;
    }
    ok = false;
    return 0.0;
}

std::string lineError(std::size_t lineNumber, const std::string& message)
{
    return "Line " + std::to_string(lineNumber) + ": " + message;
}

} // namespace

double parseEngineeringValue(const std::string& raw, bool& ok) noexcept
{
    ok = false;
    const std::string text = trim(raw);
    if (text.empty())
        return 0.0;

    try
    {
        std::size_t suffixIndex = std::string::npos;
        for (std::size_t i = 0; i < text.size(); ++i)
        {
            if (std::isalpha(static_cast<unsigned char>(text[i])) != 0)
            {
                suffixIndex = i;
                break;
            }
        }

        if (suffixIndex == std::string::npos)
        {
            std::size_t consumed = 0;
            const double value = std::stod(text, &consumed);
            ok = consumed == text.size() && std::isfinite(value);
            return ok ? value : 0.0;
        }

        const char suffix = text[suffixIndex];
        double multiplier = 0.0;
        switch (suffix)
        {
            case 'K':
            case 'k': multiplier = 1.0e3; break;
            case 'M': multiplier = 1.0e6; break;
            case 'm': multiplier = 1.0e-3; break;
            case 'U':
            case 'u': multiplier = 1.0e-6; break;
            case 'N':
            case 'n': multiplier = 1.0e-9; break;
            case 'P':
            case 'p': multiplier = 1.0e-12; break;
            default: return 0.0;
        }

        const std::string before = text.substr(0, suffixIndex);
        const std::string after = text.substr(suffixIndex + 1);
        if (before.empty())
            return 0.0;

        std::string numeric = before;
        if (!after.empty())
        {
            if (before.find('.') != std::string::npos)
                return 0.0;
            numeric += ".";
            numeric += after;
        }

        std::size_t consumed = 0;
        const double base = std::stod(numeric, &consumed);
        if (consumed != numeric.size() || !std::isfinite(base))
            return 0.0;

        const double value = base * multiplier;
        ok = std::isfinite(value);
        return ok ? value : 0.0;
    }
    catch (...)
    {
        return 0.0;
    }
}

GenericNpnBjtModel builtInNpnModel(const std::string& name, bool& ok) noexcept
{
    const std::string normalized = upper(name);
    if (normalized == "GENERIC_NPN" || normalized == "GENERIC")
    {
        ok = true;
        return {};
    }
    if (normalized == "2N3904")
    {
        GenericNpnBjtModel model;
        // Compact parameters derived from the broad behaviour of common
        // 2N3904 SPICE models. V0.7 intentionally does not claim a full
        // Gummel-Poon or manufacturer-specific device reproduction.
        model.saturationCurrentAmps = 6.734e-15;
        model.forwardBeta = 416.4;
        model.reverseBeta = 0.7371;
        model.emissionCoefficient = 1.0;
        model.thermalVoltageVolts = 0.02585;
        ok = true;
        return model;
    }
    ok = false;
    return {};
}

GenericDiodeModel builtInDiodeModel(const std::string& name, bool& ok) noexcept
{
    const std::string normalized = upper(name);
    GenericDiodeModel model;
    if (normalized == "GENERIC" || normalized == "GENERIC_DIODE")
    {
        ok = true;
        return model;
    }
    if (normalized == "1N4148" || normalized == "SILICON")
    {
        model.saturationCurrentAmps = 2.5e-9;
        model.idealityFactor = 1.75;
        ok = true;
        return model;
    }
    if (normalized == "1N34A" || normalized == "GERMANIUM")
    {
        model.saturationCurrentAmps = 1.0e-6;
        model.idealityFactor = 1.6;
        ok = true;
        return model;
    }
    ok = false;
    return {};
}

bool parseCircuitFileText(const std::string& text,
                          CircuitFileDocument& document,
                          std::string& error)
{
    error.clear();
    CircuitFileDocument parsed;
    bool sawHeader = false;
    bool sawOutput = false;

    std::istringstream stream(text);
    std::string rawLine;
    std::size_t lineNumber = 0;
    while (std::getline(stream, rawLine))
    {
        ++lineNumber;
        std::string tokenError;
        auto tokens = tokenizeLine(rawLine, tokenError);
        if (!tokenError.empty())
        {
            error = lineError(lineNumber, tokenError);
            return false;
        }
        if (tokens.empty())
            continue;

        const std::string command = upper(tokens[0]);
        if (command == "CPEDAL")
        {
            if (tokens.size() != 2 || tokens[1] != "1")
            {
                error = lineError(lineNumber,
                    "Expected 'CPEDAL 1'.");
                return false;
            }
            if (sawHeader)
            {
                error = lineError(lineNumber, "Duplicate CPEDAL header.");
                return false;
            }
            sawHeader = true;
            continue;
        }

        if (!sawHeader)
        {
            error = lineError(lineNumber,
                "The first directive must be 'CPEDAL 1'.");
            return false;
        }

        if (command == "NAME")
        {
            if (tokens.size() != 2)
            {
                error = lineError(lineNumber, "NAME requires one quoted/name token.");
                return false;
            }
            parsed.name = tokens[1];
        }
        else if (command == "NODE")
        {
            if (tokens.size() != 2)
            {
                error = lineError(lineNumber, "NODE requires a node name.");
                return false;
            }
            (void)nodeFor(parsed.definition, tokens[1]);
        }
        else if (command == "V")
        {
            if (tokens.size() != 5)
            {
                error = lineError(lineNumber,
                    "V syntax: V <id> <positive> <negative> <dc-volts>.");
                return false;
            }
            double dc = 0.0;
            if (!parseFinite(tokens[4], dc))
            {
                error = lineError(lineNumber, "Invalid voltage-source value.");
                return false;
            }
            parsed.definition.addVoltageSource(
                nodeFor(parsed.definition, tokens[2]),
                nodeFor(parsed.definition, tokens[3]),
                dc);
        }
        else if (command == "AUDIO")
        {
            if (tokens.size() != 5)
            {
                error = lineError(lineNumber,
                    "AUDIO syntax: AUDIO <id> <positive> <negative> <volts-per-full-scale>.");
                return false;
            }
            double scale = 0.0;
            if (!parseFinite(tokens[4], scale))
            {
                error = lineError(lineNumber, "Invalid AUDIO scale.");
                return false;
            }
            parsed.definition.addVoltageSource(
                nodeFor(parsed.definition, tokens[2]),
                nodeFor(parsed.definition, tokens[3]),
                0.0,
                scale);
        }
        else if (command == "R")
        {
            if (tokens.size() != 5)
            {
                error = lineError(lineNumber,
                    "R syntax: R <id> <node-a> <node-b> <resistance>.");
                return false;
            }
            double resistance = 0.0;
            if (!parseFinite(tokens[4], resistance))
            {
                error = lineError(lineNumber, "Invalid resistor value.");
                return false;
            }
            parsed.definition.addResistor(
                nodeFor(parsed.definition, tokens[2]),
                nodeFor(parsed.definition, tokens[3]),
                resistance);
        }
        else if (command == "C")
        {
            if (tokens.size() != 5)
            {
                error = lineError(lineNumber,
                    "C syntax: C <id> <node-a> <node-b> <capacitance>.");
                return false;
            }
            double capacitance = 0.0;
            if (!parseFinite(tokens[4], capacitance))
            {
                error = lineError(lineNumber, "Invalid capacitor value.");
                return false;
            }
            parsed.definition.addCapacitor(
                nodeFor(parsed.definition, tokens[2]),
                nodeFor(parsed.definition, tokens[3]),
                capacitance);
        }
        else if (command == "D")
        {
            if (tokens.size() != 5)
            {
                error = lineError(lineNumber,
                    "D syntax: D <id> <anode> <cathode> <model>.");
                return false;
            }
            bool modelOk = false;
            const auto model = builtInDiodeModel(tokens[4], modelOk);
            if (!modelOk)
            {
                error = lineError(lineNumber,
                    "Unknown diode model '" + tokens[4] + "'.");
                return false;
            }
            parsed.definition.addDiode(
                nodeFor(parsed.definition, tokens[2]),
                nodeFor(parsed.definition, tokens[3]),
                model);
        }
        else if (command == "Q" || command == "NPN")
        {
            if (tokens.size() != 6)
            {
                error = lineError(lineNumber,
                    "Q syntax: Q <id> <collector> <base> <emitter> <model>.");
                return false;
            }
            bool modelOk = false;
            const auto model = builtInNpnModel(tokens[5], modelOk);
            if (!modelOk)
            {
                error = lineError(lineNumber,
                    "Unknown NPN model '" + tokens[5] + "'.");
                return false;
            }
            parsed.definition.addNpnBjt(
                nodeFor(parsed.definition, tokens[2]),
                nodeFor(parsed.definition, tokens[3]),
                nodeFor(parsed.definition, tokens[4]),
                model);
        }
        else if (command == "POT")
        {
            if (tokens.size() != 8)
            {
                error = lineError(lineNumber,
                    "POT syntax: POT <name> <terminal1> <wiper> <terminal3> "
                    "<resistance> <LIN|LOG|EXP:x> <initial-0..1>.");
                return false;
            }
            double resistance = 0.0;
            double initial = 0.0;
            bool taperOk = false;
            const double exponent = taperExponent(tokens[6], taperOk);
            if (!parseFinite(tokens[5], resistance)
                || !parseFinite(tokens[7], initial)
                || !taperOk
                || initial < 0.0
                || initial > 1.0)
            {
                error = lineError(lineNumber, "Invalid potentiometer parameters.");
                return false;
            }
            const auto index = parsed.definition.addPotentiometer(
                nodeFor(parsed.definition, tokens[2]),
                nodeFor(parsed.definition, tokens[3]),
                nodeFor(parsed.definition, tokens[4]),
                resistance,
                initial,
                exponent);
            parsed.controls.push_back({ tokens[1], index, initial });
        }
        else if (command == "OUTPUT")
        {
            if (tokens.size() != 2 && tokens.size() != 3)
            {
                error = lineError(lineNumber,
                    "OUTPUT syntax: OUTPUT <node> [full-scale-per-volt].");
                return false;
            }
            parsed.definition.setOutputNode(nodeFor(parsed.definition, tokens[1]));
            if (tokens.size() == 3)
            {
                double scale = 0.0;
                if (!parseFinite(tokens[2], scale))
                {
                    error = lineError(lineNumber, "Invalid OUTPUT scale.");
                    return false;
                }
                parsed.definition.setOutputFullScalePerVolt(scale);
            }
            sawOutput = true;
        }
        else
        {
            error = lineError(lineNumber,
                "Unknown directive '" + tokens[0] + "'.");
            return false;
        }
    }

    if (!sawHeader)
    {
        error = "Circuit file is missing 'CPEDAL 1'.";
        return false;
    }
    if (!sawOutput)
    {
        error = "Circuit file is missing an OUTPUT directive.";
        return false;
    }
    if (parsed.name.empty())
        parsed.name = "Unnamed Circuit";

    std::string validationError;
    if (!parsed.definition.validate(validationError))
    {
        error = "Circuit definition is invalid: " + validationError;
        return false;
    }

    document = std::move(parsed);
    return true;
}

bool loadCircuitFile(const std::string& path,
                     CircuitFileDocument& document,
                     std::string& error)
{
    error.clear();
    std::ifstream file(path);
    if (!file)
    {
        error = "Could not open circuit file: " + path;
        return false;
    }

    std::ostringstream contents;
    contents << file.rdbuf();
    if (!file.good() && !file.eof())
    {
        error = "Could not read circuit file: " + path;
        return false;
    }
    return parseCircuitFileText(contents.str(), document, error);
}

} // namespace circuitpedal
