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

bool parseSwitchPosition(CircuitSwitchMode mode,
                         const std::string& text,
                         std::uint32_t& position)
{
    const std::string normalized = upper(text);
    if (mode == CircuitSwitchMode::Spst)
    {
        if (normalized == "OFF") { position = 0; return true; }
        if (normalized == "ON") { position = 1; return true; }
    }
    else if (mode == CircuitSwitchMode::Spdt)
    {
        if (normalized == "A") { position = 0; return true; }
        if (normalized == "B") { position = 1; return true; }
    }
    else
    {
        if (normalized == "A") { position = 0; return true; }
        if (normalized == "OFF" || normalized == "CENTER") { position = 1; return true; }
        if (normalized == "B") { position = 2; return true; }
    }

    try
    {
        std::size_t consumed = 0;
        const unsigned long value = std::stoul(text, &consumed);
        const std::uint32_t count =
            mode == CircuitSwitchMode::OnOffOn ? 3U : 2U;
        if (consumed == text.size() && value < count)
        {
            position = static_cast<std::uint32_t>(value);
            return true;
        }
    }
    catch (...) {}
    return false;
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
    if (normalized == "NTE103")
    {
        GenericNpnBjtModel model;
        // NTE103 is the NPN half of the complementary NTE102/103 germanium
        // pair used by the Animato input booster. Datasheet hFE is typically
        // around 80-90 at tens of milliamps. Leakage/temperature dependence is
        // not yet represented by this compact Ebers-Moll model.
        model.saturationCurrentAmps = 5.0e-8;
        model.forwardBeta = 85.0;
        model.reverseBeta = 2.0;
        model.emissionCoefficient = 1.5;
        ok = true;
        return model;
    }
    if (normalized == "2N3904")
    {
        GenericNpnBjtModel model;
        // Compact parameters derived from the broad behaviour of common
        // 2N3904 SPICE models. These named models remain compact Ebers-Moll
        // approximations rather than manufacturer-specific Gummel-Poon data.
        model.saturationCurrentAmps = 6.734e-15;
        model.forwardBeta = 416.4;
        model.reverseBeta = 0.7371;
        model.emissionCoefficient = 1.0;
        model.thermalVoltageVolts = 0.02585;
        ok = true;
        return model;
    }
    if (normalized == "2N2222" || normalized == "2N2222A")
    {
        GenericNpnBjtModel model;
        model.saturationCurrentAmps = 1.0e-14;
        model.forwardBeta = 220.0;
        model.reverseBeta = 3.0;
        ok = true;
        return model;
    }
    if (normalized == "2SC1815" || normalized == "2SC1815BL")
    {
        GenericNpnBjtModel model;
        // BL is the high-gain rank commonly found in Tube Screamer buffers.
        model.saturationCurrentAmps = 2.0e-14;
        model.forwardBeta = normalized == "2SC1815BL" ? 500.0 : 300.0;
        model.reverseBeta = 4.0;
        ok = true;
        return model;
    }
    if (normalized == "2SC2240" || normalized == "C2240")
    {
        GenericNpnBjtModel model;
        // Toshiba 2SC2240 is a low-noise high-gain silicon NPN. The datasheet
        // spans hFE 200-700 (GR/BL ranks); use a mid-high compact value until
        // a particular physical transistor rank is nominated.
        model.saturationCurrentAmps = 3.0e-14;
        model.forwardBeta = 420.0;
        model.reverseBeta = 4.0;
        ok = true;
        return model;
    }
    if (normalized == "2N5088" || normalized == "2N5089")
    {
        GenericNpnBjtModel model;
        // The 2N5088/89 are the same high-gain, low-noise silicon family.
        // Keep one deliberately compact approximation until a nominated
        // manufacturer SPICE model is introduced; physical device spread is
        // not represented by this Ebers-Moll alias.
        model.saturationCurrentAmps = 3.0e-14;
        model.forwardBeta = normalized == "2N5089" ? 650.0 : 520.0;
        model.reverseBeta = 4.0;
        ok = true;
        return model;
    }
    if (normalized == "2N5133")
    {
        GenericNpnBjtModel model;
        model.saturationCurrentAmps = 2.0e-14;
        model.forwardBeta = 430.0;
        model.reverseBeta = 4.0;
        ok = true;
        return model;
    }
    if (normalized == "BC239" || normalized == "BC239C")
    {
        GenericNpnBjtModel model;
        model.saturationCurrentAmps = 1.5e-14;
        model.forwardBeta = 350.0;
        model.reverseBeta = 4.0;
        ok = true;
        return model;
    }
    if (normalized == "KT3102" || normalized == "KT3102E")
    {
        GenericNpnBjtModel model;
        model.saturationCurrentAmps = 2.0e-14;
        model.forwardBeta = 300.0;
        model.reverseBeta = 4.0;
        ok = true;
        return model;
    }
    if (normalized == "BC550" || normalized == "BC550C")
    {
        GenericNpnBjtModel model;
        model.saturationCurrentAmps = 2.0e-14;
        model.forwardBeta = 500.0;
        model.reverseBeta = 4.0;
        ok = true;
        return model;
    }
    ok = false;
    return {};
}

GenericPnpBjtModel builtInPnpModel(const std::string& name, bool& ok) noexcept
{
    const std::string normalized = upper(name);
    GenericPnpBjtModel model;

    if (normalized == "GENERIC_PNP" || normalized == "PNP")
    {
        ok = true;
        return model;
    }
    if (normalized == "NTE102")
    {
        // PNP complement to NTE103. Keep the compact pair intentionally
        // symmetric until leakage/temperature measurements are available.
        model.saturationCurrentAmps = 5.0e-8;
        model.forwardBeta = 85.0;
        model.reverseBeta = 2.0;
        model.emissionCoefficient = 1.5;
        ok = true;
        return model;
    }
    if (normalized == "AC128" || normalized == "GERMANIUM_PNP" || normalized == "GE_PNP")
    {
        // Broad vintage-germanium approximation. Leakage and temperature
        // behaviour will be refined when the SPICE/device-library work lands.
        model.saturationCurrentAmps = 5.0e-8;
        model.forwardBeta = 90.0;
        model.reverseBeta = 2.0;
        model.emissionCoefficient = 1.5;
        ok = true;
        return model;
    }
    if (normalized == "AC128_LOW")
    {
        model.saturationCurrentAmps = 5.0e-8;
        model.forwardBeta = 70.0;
        model.reverseBeta = 2.0;
        model.emissionCoefficient = 1.5;
        ok = true;
        return model;
    }
    if (normalized == "AC128_HIGH")
    {
        model.saturationCurrentAmps = 5.0e-8;
        model.forwardBeta = 110.0;
        model.reverseBeta = 2.0;
        model.emissionCoefficient = 1.5;
        ok = true;
        return model;
    }
    if (normalized == "2N1308")
    {
        model.saturationCurrentAmps = 2.0e-8;
        model.forwardBeta = 80.0;
        model.reverseBeta = 2.0;
        model.emissionCoefficient = 1.4;
        ok = true;
        return model;
    }
    if (normalized == "2N3906")
    {
        // General-purpose silicon PNP approximation. This exposes the named
        // phase-splitter part used by the Tentacle while retaining the same
        // compact-model limitations as the NPN aliases.
        model.saturationCurrentAmps = 1.4e-15;
        model.forwardBeta = 180.0;
        model.reverseBeta = 4.0;
        ok = true;
        return model;
    }
    ok = false;
    return {};
}

GenericNjfetModel builtInNjfetModel(const std::string& name, bool& ok) noexcept
{
    const std::string normalized = upper(name);
    GenericNjfetModel model;

    if (normalized == "GENERIC_NJFET" || normalized == "NJFET" || normalized == "JFET")
    {
        ok = true;
        return model;
    }
    if (normalized == "2N5457")
    {
        model.idssAmps = 3.0e-3;
        model.pinchOffVoltageVolts = -2.5;
        ok = true;
        return model;
    }
    if (normalized == "J201")
    {
        model.idssAmps = 0.8e-3;
        model.pinchOffVoltageVolts = -0.8;
        ok = true;
        return model;
    }
    if (normalized == "J113")
    {
        model.idssAmps = 10.0e-3;
        model.pinchOffVoltageVolts = -3.0;
        ok = true;
        return model;
    }
    if (normalized == "MPF4393")
    {
        model.idssAmps = 15.0e-3;
        model.pinchOffVoltageVolts = -2.5;
        ok = true;
        return model;
    }
    if (normalized == "2N5952")
    {
        model.idssAmps = 6.0e-3;
        model.pinchOffVoltageVolts = -2.0;
        ok = true;
        return model;
    }
    ok = false;
    return {};
}

GenericOpAmpModel builtInOpAmpModel(const std::string& name, bool& ok) noexcept
{
    const std::string normalized = upper(name);
    GenericOpAmpModel model;

    if (normalized == "GENERIC_OPAMP" || normalized == "OPAMP")
    {
        ok = true;
        return model;
    }
    if (normalized == "4558"
        || normalized == "4558D"
        || normalized == "JRC4558"
        || normalized == "JRC4558D")
    {
        model.openLoopGain = 100000.0;
        model.gainBandwidthHz = 3.0e6;
        model.slewRateVoltsPerSecond = 1.0e6;
        model.outputHeadroomVolts = 1.4;
        ok = true;
        return model;
    }
    if (normalized == "CA3130"
        || normalized == "CA3130E"
        || normalized == "CA3130EZ")
    {
        model.openLoopGain = 100000.0;
        model.gainBandwidthHz = 15.0e6;
        model.slewRateVoltsPerSecond = 30.0e6;
        model.outputHeadroomVolts = 0.25;
        ok = true;
        return model;
    }
    ok = false;
    return {};
}

GenericNmosModel builtInNmosModel(const std::string& name, bool& ok) noexcept
{
    const std::string normalized = upper(name);
    GenericNmosModel model;

    if (normalized == "GENERIC_NMOS" || normalized == "NMOS" || normalized == "MOSFET")
    {
        ok = true;
        return model;
    }
    if (normalized == "BS170")
    {
        model.thresholdVoltageVolts = 2.1;
        model.betaAmpsPerVoltSquared = 0.01;
        model.bodyDiodeSaturationCurrentAmps = 1.0e-12;
        model.bodyDiodeIdealityFactor = 1.8;
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
    if (normalized == "1N4148"
        || normalized == "1N914"
        || normalized == "1S1588"
        || normalized == "KD521"
        || normalized == "SILICON")
    {
        model.saturationCurrentAmps = 2.5e-9;
        model.idealityFactor = 1.75;
        ok = true;
        return model;
    }
    if (normalized == "1N4001"
        || normalized == "1N4007"
        || normalized == "RECTIFIER")
    {
        model.saturationCurrentAmps = 5.0e-9;
        model.idealityFactor = 1.9;
        ok = true;
        return model;
    }
    if (normalized == "LED_RED"
        || normalized == "RED_LED"
        || normalized == "LED")
    {
        // Compact low-current red-LED junction. Chosen to give roughly
        // 1.6-1.8 V forward drop over pedal-scale clipping currents.
        model.saturationCurrentAmps = 1.0e-18;
        model.idealityFactor = 2.0;
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
    if (normalized == "1N6263" || normalized == "SCHOTTKY")
    {
        model.saturationCurrentAmps = 1.0e-8;
        model.idealityFactor = 1.05;
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
        else if (command == "PNP")
        {
            if (tokens.size() != 6)
            {
                error = lineError(lineNumber,
                    "PNP syntax: PNP <id> <collector> <base> <emitter> <model>.");
                return false;
            }
            bool modelOk = false;
            const auto model = builtInPnpModel(tokens[5], modelOk);
            if (!modelOk)
            {
                error = lineError(lineNumber,
                    "Unknown PNP model '" + tokens[5] + "'.");
                return false;
            }
            parsed.definition.addPnpBjt(
                nodeFor(parsed.definition, tokens[2]),
                nodeFor(parsed.definition, tokens[3]),
                nodeFor(parsed.definition, tokens[4]),
                model);
        }
        else if (command == "JFET" || command == "NJFET")
        {
            if (tokens.size() != 6)
            {
                error = lineError(lineNumber,
                    "JFET syntax: JFET <id> <drain> <gate> <source> <model>.");
                return false;
            }
            bool modelOk = false;
            const auto model = builtInNjfetModel(tokens[5], modelOk);
            if (!modelOk)
            {
                error = lineError(lineNumber,
                    "Unknown N-JFET model '" + tokens[5] + "'.");
                return false;
            }
            parsed.definition.addNjfet(
                nodeFor(parsed.definition, tokens[2]),
                nodeFor(parsed.definition, tokens[3]),
                nodeFor(parsed.definition, tokens[4]),
                model);
        }
        else if (command == "NMOS" || command == "MOSFET")
        {
            if (tokens.size() != 6)
            {
                error = lineError(lineNumber,
                    "NMOS syntax: NMOS <id> <drain> <gate> <source> <model>.");
                return false;
            }
            bool modelOk = false;
            const auto model = builtInNmosModel(tokens[5], modelOk);
            if (!modelOk)
            {
                error = lineError(lineNumber,
                    "Unknown N-MOSFET model '" + tokens[5] + "'.");
                return false;
            }
            parsed.definition.addNmos(
                nodeFor(parsed.definition, tokens[2]),
                nodeFor(parsed.definition, tokens[3]),
                nodeFor(parsed.definition, tokens[4]),
                model);
        }
        else if (command == "OPAMP")
        {
            if (tokens.size() != 8)
            {
                error = lineError(lineNumber,
                    "OPAMP syntax: OPAMP <id> <plus> <minus> <out> "
                    "<positive-rail> <negative-rail> <model>.");
                return false;
            }
            bool modelOk = false;
            const auto model = builtInOpAmpModel(tokens[7], modelOk);
            if (!modelOk)
            {
                error = lineError(lineNumber,
                    "Unknown op-amp model '" + tokens[7] + "'.");
                return false;
            }
            parsed.definition.addOpAmp(
                nodeFor(parsed.definition, tokens[2]),
                nodeFor(parsed.definition, tokens[3]),
                nodeFor(parsed.definition, tokens[4]),
                nodeFor(parsed.definition, tokens[5]),
                nodeFor(parsed.definition, tokens[6]),
                model);
        }
        else if (command == "SWITCH")
        {
            if (tokens.size() < 6)
            {
                error = lineError(lineNumber,
                    "SWITCH syntax is SWITCH <name> <SPST|SPDT|ONOFFON> ...");
                return false;
            }

            const std::string modeToken = upper(tokens[2]);
            CircuitSwitchMode mode = CircuitSwitchMode::Spst;
            std::uint32_t position = 0;
            CircuitNode common = circuitGround;
            CircuitNode throwA = circuitGround;
            CircuitNode throwB = circuitGround;
            std::vector<std::string> labels;

            if (modeToken == "SPST")
            {
                if (tokens.size() != 6 && tokens.size() != 8)
                {
                    error = lineError(lineNumber,
                        "SPST syntax: SWITCH <name> SPST <a> <b> <initial> [<off-label> <on-label>].");
                    return false;
                }
                mode = CircuitSwitchMode::Spst;
                common = nodeFor(parsed.definition, tokens[3]);
                throwA = nodeFor(parsed.definition, tokens[4]);
                if (!parseSwitchPosition(mode, tokens[5], position))
                {
                    error = lineError(lineNumber, "Invalid SPST initial position.");
                    return false;
                }
                labels = tokens.size() == 8
                    ? std::vector<std::string>{tokens[6], tokens[7]}
                    : std::vector<std::string>{"Off", "On"};
            }
            else if (modeToken == "SPDT" || modeToken == "ONOFFON")
            {
                const bool centerOff = modeToken == "ONOFFON";
                const std::size_t basicSize = 7;
                const std::size_t labelledSize = centerOff ? 10 : 9;
                if (tokens.size() != basicSize && tokens.size() != labelledSize)
                {
                    error = lineError(lineNumber,
                        centerOff
                            ? "ONOFFON syntax: SWITCH <name> ONOFFON <common> <a> <b> <initial> [<a-label> <off-label> <b-label>]."
                            : "SPDT syntax: SWITCH <name> SPDT <common> <a> <b> <initial> [<a-label> <b-label>].");
                    return false;
                }
                mode = centerOff ? CircuitSwitchMode::OnOffOn : CircuitSwitchMode::Spdt;
                common = nodeFor(parsed.definition, tokens[3]);
                throwA = nodeFor(parsed.definition, tokens[4]);
                throwB = nodeFor(parsed.definition, tokens[5]);
                if (!parseSwitchPosition(mode, tokens[6], position))
                {
                    error = lineError(lineNumber, "Invalid switch initial position.");
                    return false;
                }
                if (tokens.size() == labelledSize)
                {
                    if (centerOff)
                        labels = {tokens[7], tokens[8], tokens[9]};
                    else
                        labels = {tokens[7], tokens[8]};
                }
                else
                {
                    labels = centerOff
                        ? std::vector<std::string>{"A", "Off", "B"}
                        : std::vector<std::string>{"A", "B"};
                }
            }
            else
            {
                error = lineError(lineNumber,
                    "Unknown switch mode '" + tokens[2] + "'.");
                return false;
            }

            const auto switchIndex =
                parsed.definition.addSwitch(mode, common, throwA, throwB, position);

            CircuitFileControl control;
            control.name = tokens[1];
            control.kind = CircuitFileControlKind::Switch;
            control.switchMode = mode;
            control.switchIndex = switchIndex;
            control.switchPositionCount =
                mode == CircuitSwitchMode::OnOffOn ? 3U : 2U;
            control.initialSwitchPosition = position;
            control.switchPositionNames = std::move(labels);
            parsed.controls.push_back(std::move(control));
        }
        else if (command == "SWITCH_LINK")
        {
            if (tokens.size() != 4 && tokens.size() != 5)
            {
                error = lineError(lineNumber,
                    "SWITCH_LINK syntax: SWITCH_LINK <existing-switch-name> "
                    "<a> <b> for SPST, or <common> <throw-a> <throw-b> "
                    "for SPDT/ONOFFON.");
                return false;
            }

            const auto control = std::find_if(
                parsed.controls.begin(),
                parsed.controls.end(),
                [&tokens](const CircuitFileControl& candidate) {
                    return candidate.name == tokens[1];
                });
            if (control == parsed.controls.end()
                || control->kind != CircuitFileControlKind::Switch)
            {
                error = lineError(lineNumber,
                    "SWITCH_LINK references unknown/non-switch control '"
                    + tokens[1] + "'.");
                return false;
            }

            const bool spst = control->switchMode == CircuitSwitchMode::Spst;
            if ((spst && tokens.size() != 4)
                || (!spst && tokens.size() != 5))
            {
                error = lineError(lineNumber,
                    spst
                        ? "Linked SPST needs two electrical nodes."
                        : "Linked SPDT/ONOFFON needs common and two throw nodes.");
                return false;
            }

            const CircuitNode common = nodeFor(parsed.definition, tokens[2]);
            const CircuitNode throwA = nodeFor(parsed.definition, tokens[3]);
            const CircuitNode throwB = spst
                ? circuitGround
                : nodeFor(parsed.definition, tokens[4]);
            const auto index = parsed.definition.addSwitch(
                control->switchMode,
                common,
                throwA,
                throwB,
                control->initialSwitchPosition);
            control->linkedSwitchIndices.push_back(index);
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
            CircuitFileControl control;
            control.name = tokens[1];
            control.potentiometerIndex = index;
            control.initialPosition = initial;
            parsed.controls.push_back(std::move(control));
        }
        else if (command == "POT_LINK")
        {
            if (tokens.size() != 7)
            {
                error = lineError(lineNumber,
                    "POT_LINK syntax: POT_LINK <existing-control-name> "
                    "<terminal1> <wiper> <terminal3> <resistance> "
                    "<LIN|LOG|EXP:x>.");
                return false;
            }

            const auto control = std::find_if(
                parsed.controls.begin(),
                parsed.controls.end(),
                [&tokens](const CircuitFileControl& candidate) {
                    return candidate.name == tokens[1];
                });
            if (control == parsed.controls.end()
                || control->kind != CircuitFileControlKind::Potentiometer)
            {
                error = lineError(lineNumber,
                    "POT_LINK references unknown/non-pot control '"
                    + tokens[1] + "'.");
                return false;
            }

            double resistance = 0.0;
            bool taperOk = false;
            const double exponent = taperExponent(tokens[6], taperOk);
            if (!parseFinite(tokens[5], resistance) || !taperOk)
            {
                error = lineError(lineNumber,
                    "Invalid linked potentiometer parameters.");
                return false;
            }

            const auto index = parsed.definition.addPotentiometer(
                nodeFor(parsed.definition, tokens[2]),
                nodeFor(parsed.definition, tokens[3]),
                nodeFor(parsed.definition, tokens[4]),
                resistance,
                control->initialPosition,
                exponent);
            control->linkedPotentiometerIndices.push_back(index);
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
