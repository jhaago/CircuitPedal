#include "DistortionPlusModel.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace circuitpedal {
namespace {

static_assert(std::atomic<float>::is_always_lock_free,
              "Real-time parameter atomics must be lock-free on this target");
static_assert(std::atomic<bool>::is_always_lock_free,
              "Real-time bypass atomic must be lock-free on this target");
static_assert(std::atomic<std::uint32_t>::is_always_lock_free,
              "Real-time preset atomic must be lock-free on this target");

constexpr double pi = 3.14159265358979323846;
constexpr double minimumResistance = 1.0;
constexpr double minimumCapacitance = 1.0e-15;
constexpr double maximumDiodeArgument = 18.0;

struct DiodeEvaluation {
    double currentAmps;
    double conductanceSiemens;
};

DiodeEvaluation evaluateDiodes(double voltage, const ClipNetworkParameters& p) noexcept
{
    const double nvT = std::max(p.diodeIdealityFactor * p.thermalVoltageVolts, 1.0e-9);
    const double rawArgument = voltage / nvT;
    const double limitedArgument = std::clamp(rawArgument,
                                               -maximumDiodeArgument,
                                               maximumDiodeArgument);
    const double limitedSinh = std::sinh(limitedArgument);
    const double limitedCosh = std::cosh(limitedArgument);

    // Continue the exponential linearly outside the safe evaluation region.
    // Unlike clamping sinh() alone, this keeps current and derivative consistent.
    const double argumentDelta = rawArgument - limitedArgument;
    const double scaledCurrent = limitedSinh + limitedCosh * argumentDelta;
    const double current = 2.0 * p.diodeSaturationCurrentAmps * scaledCurrent;
    const double conductance = (2.0 * p.diodeSaturationCurrentAmps / nvT) * limitedCosh;
    return { current, conductance };
}

struct ClipEquationEvaluation {
    double residual;
    double derivative;
    double seriesNodeVoltage;
};

ClipEquationEvaluation evaluateClipEquation(double clipVoltage,
                                             double sourceVoltage,
                                             double timestep,
                                             const ClipNetworkParameters& p,
                                             const ClipNetworkState& state) noexcept
{
    const double seriesG = 1.0 / std::max(p.seriesResistanceOhms, minimumResistance);
    const double couplingG = std::max(p.couplingCapacitanceFarads,
                                      minimumCapacitance) / timestep;
    const double shuntG = std::max(p.shuntCapacitanceFarads,
                                   minimumCapacitance) / timestep;
    const double loadG = 1.0 / std::max(p.loadResistanceOhms, minimumResistance);

    // Eliminate the node between the series coupling capacitor and R_clip.
    const double seriesNode =
        (seriesG * clipVoltage
         + couplingG * (sourceVoltage - state.couplingCapVoltage))
        / (couplingG + seriesG);
    const double seriesNodeDerivative = seriesG / (couplingG + seriesG);
    const auto diode = evaluateDiodes(clipVoltage, p);

    const double residual =
        seriesG * (clipVoltage - seriesNode)
        + shuntG * (clipVoltage - state.clipNodeVoltage)
        + loadG * clipVoltage
        + diode.currentAmps;
    const double derivative =
        seriesG * (1.0 - seriesNodeDerivative)
        + shuntG
        + loadG
        + diode.conductanceSiemens;
    return { residual, derivative, seriesNode };
}

double softRailLimit(double input, double limit, double knee) noexcept
{
    const double magnitude = std::abs(input);
    const double kneeStart = std::max(0.0, limit - knee);
    if (magnitude <= kneeStart)
        return input;

    const double limitedMagnitude =
        kneeStart + knee * std::tanh((magnitude - kneeStart) / knee);
    return std::copysign(std::min(limitedMagnitude, limit), input);
}

double parallelResistance(double a, double b) noexcept
{
    if (a <= minimumResistance)
        return a;
    if (b <= minimumResistance)
        return b;
    return 1.0 / (1.0 / a + 1.0 / b);
}

bool finiteAtLeast(double value, double minimum) noexcept
{
    return std::isfinite(value) && value >= minimum;
}

bool finiteGreaterThan(double value, double minimum) noexcept
{
    return std::isfinite(value) && value > minimum;
}

} // namespace

bool distortionPlusCircuitParametersValid(
    const DistortionPlusCircuitParameters& p) noexcept
{
    return finiteAtLeast(p.inputBiasResistanceOhms, minimumResistance)
        && finiteAtLeast(p.opAmpInputResistanceOhms, minimumResistance)
        && finiteAtLeast(p.inputCouplingCapacitanceFarads, minimumCapacitance)
        && finiteAtLeast(p.inputRfCapacitanceFarads, minimumCapacitance)
        && finiteAtLeast(p.feedbackResistanceOhms, minimumResistance)
        && finiteAtLeast(p.gainMinimumResistanceOhms, minimumResistance)
        && finiteAtLeast(p.gainPotentiometerResistanceOhms, minimumResistance)
        && finiteAtLeast(p.gainCapacitanceFarads, minimumCapacitance)
        && finiteGreaterThan(p.gainPotCurveExponent, 0.0)
        && finiteAtLeast(p.clipSeriesResistanceOhms, minimumResistance)
        && finiteAtLeast(p.postOpAmpCouplingCapacitanceFarads, minimumCapacitance)
        && finiteAtLeast(p.clipShuntCapacitanceFarads, minimumCapacitance)
        && finiteAtLeast(p.outputPotentiometerResistanceOhms, minimumResistance)
        && finiteGreaterThan(p.outputTaperExponent, 0.0)
        && finiteGreaterThan(p.opAmpGainBandwidthHz, 0.0)
        && finiteGreaterThan(p.opAmpSlewRateVoltsPerSecond, 0.0)
        && finiteGreaterThan(p.opAmpSwingVolts, 0.0)
        && finiteAtLeast(p.opAmpSoftKneeVolts, 0.0)
        && p.opAmpSoftKneeVolts <= p.opAmpSwingVolts;
}

const char* clippingDiodePresetName(ClippingDiodePreset preset) noexcept
{
    switch (preset)
    {
        case ClippingDiodePreset::ReferenceGermanium:
            return "Reference germanium (V0.2)";
        case ClippingDiodePreset::SiliconLike:
            return "Silicon-like (experimental)";
        case ClippingDiodePreset::LedLike:
            return "LED-like (experimental)";
        case ClippingDiodePreset::NoDiodes:
            return "No clipping diodes";
    }
    return "Reference germanium (V0.2)";
}

DiodeModelParameters diodeModelParameters(ClippingDiodePreset preset) noexcept
{
    switch (preset)
    {
        case ClippingDiodePreset::ReferenceGermanium:
            return { 1.0e-6, 1.6, 0.02585 };
        case ClippingDiodePreset::SiliconLike:
            // Approximate silicon behaviour only; this is not yet fitted to a
            // measured 1N4148/1N914 device.
            return { 2.5e-9, 1.75, 0.02585 };
        case ClippingDiodePreset::LedLike:
            // A deliberately high-threshold diode curve suitable for listening
            // tests. It is not a claim to represent a particular LED part.
            return { 1.0e-10, 3.6, 0.02585 };
        case ClippingDiodePreset::NoDiodes:
            return { 0.0, 1.0, 0.02585 };
    }
    return { 1.0e-6, 1.6, 0.02585 };
}

ClipNetworkResult processClipNetwork(double sourceVolts,
                                     double timestepSeconds,
                                     const ClipNetworkParameters& parameters,
                                     ClipNetworkState& state) noexcept
{
    ClipNetworkResult result;
    if (!std::isfinite(sourceVolts)
        || !std::isfinite(timestepSeconds)
        || timestepSeconds <= 0.0
        || !std::isfinite(state.couplingCapVoltage)
        || !std::isfinite(state.clipNodeVoltage))
    {
        state = {};
        return result;
    }

    constexpr double voltageBound = 4.0;
    constexpr double maximumNewtonStep = 0.10;
    constexpr double currentTolerance = 1.0e-11;
    constexpr double voltageTolerance = 1.0e-10;
    constexpr std::uint32_t maximumNewtonIterations = 20;
    constexpr std::uint32_t maximumBisectionIterations = 48;

    double lower = -voltageBound;
    double upper = voltageBound;
    auto lowerEval = evaluateClipEquation(lower,
                                          sourceVolts,
                                          timestepSeconds,
                                          parameters,
                                          state);
    auto upperEval = evaluateClipEquation(upper,
                                          sourceVolts,
                                          timestepSeconds,
                                          parameters,
                                          state);
    if (!std::isfinite(lowerEval.residual)
        || !std::isfinite(upperEval.residual)
        || lowerEval.residual > 0.0
        || upperEval.residual < 0.0)
    {
        return result;
    }

    double voltage = std::clamp(state.clipNodeVoltage, -0.35, 0.35);
    double precedingVoltage = voltage;
    ClipEquationEvaluation evaluation {};

    for (std::uint32_t iteration = 1; iteration <= maximumNewtonIterations; ++iteration)
    {
        evaluation = evaluateClipEquation(voltage,
                                          sourceVolts,
                                          timestepSeconds,
                                          parameters,
                                          state);
        result.iterations = iteration;
        if (!std::isfinite(evaluation.residual)
            || !std::isfinite(evaluation.derivative)
            || evaluation.derivative <= 0.0)
        {
            break;
        }

        if (evaluation.residual < 0.0)
            lower = voltage;
        else
            upper = voltage;

        if (std::abs(evaluation.residual) <= currentTolerance
            && std::abs(voltage - precedingVoltage) <= voltageTolerance)
        {
            result.converged = true;
            break;
        }

        double step = -evaluation.residual / evaluation.derivative;
        step = std::clamp(step, -maximumNewtonStep, maximumNewtonStep);
        double candidate = voltage + step;
        if (!std::isfinite(candidate) || candidate <= lower || candidate >= upper)
            candidate = 0.5 * (lower + upper);

        precedingVoltage = voltage;
        voltage = candidate;
    }

    if (!result.converged)
    {
        for (std::uint32_t iteration = 1;
             iteration <= maximumBisectionIterations;
             ++iteration)
        {
            voltage = 0.5 * (lower + upper);
            evaluation = evaluateClipEquation(voltage,
                                              sourceVolts,
                                              timestepSeconds,
                                              parameters,
                                              state);
            result.iterations = maximumNewtonIterations + iteration;
            if (!std::isfinite(evaluation.residual))
                return {};

            if (evaluation.residual < 0.0)
                lower = voltage;
            else
                upper = voltage;

            if (std::abs(upper - lower) <= voltageTolerance)
            {
                result.converged = true;
                break;
            }
        }
    }

    evaluation = evaluateClipEquation(voltage,
                                      sourceVolts,
                                      timestepSeconds,
                                      parameters,
                                      state);
    if (!result.converged
        || !std::isfinite(evaluation.residual)
        || std::abs(evaluation.residual) > 1.0e-8)
    {
        return {};
    }

    state.couplingCapVoltage = sourceVolts - evaluation.seriesNodeVoltage;
    state.clipNodeVoltage = voltage;

    result.clipNodeVoltage = voltage;
    result.seriesNodeVoltage = evaluation.seriesNodeVoltage;
    result.kclResidualAmps = evaluation.residual;
    return result;
}

void Oversampler4x::prepare() noexcept
{
    constexpr double normalizedCutoff = 0.1125;
    constexpr double centre = static_cast<double>(tapCount - 1) * 0.5;
    double sum = 0.0;

    for (std::size_t i = 0; i < tapCount; ++i)
    {
        const double offset = static_cast<double>(i) - centre;
        const double sinc = std::abs(offset) < 1.0e-12
            ? 2.0 * normalizedCutoff
            : std::sin(2.0 * pi * normalizedCutoff * offset) / (pi * offset);
        const double phase = 2.0 * pi * static_cast<double>(i)
                           / static_cast<double>(tapCount - 1);
        const double blackman = 0.42 - 0.5 * std::cos(phase) + 0.08 * std::cos(2.0 * phase);
        coefficients_[i] = sinc * blackman;
        sum += coefficients_[i];
    }

    if (std::abs(sum) > 1.0e-12)
    {
        for (double& coefficient : coefficients_)
            coefficient /= sum;
    }
    reset();
}

void Oversampler4x::reset() noexcept
{
    inputHistory_.fill(0.0);
    outputHistory_.fill(0.0);
    inputWriteIndex_ = 0;
    outputWriteIndex_ = 0;
}

void Oversampler4x::upsample(double input,
                            std::array<double, factor>& output) noexcept
{
    inputHistory_[inputWriteIndex_] = input;
    for (int phase = 0; phase < factor; ++phase)
    {
        double value = 0.0;
        std::size_t historyOffset = 0;
        for (std::size_t tap = static_cast<std::size_t>(phase);
             tap < tapCount;
             tap += factor, ++historyOffset)
        {
            const std::size_t historyIndex =
                (inputWriteIndex_ + inputHistorySize - historyOffset) % inputHistorySize;
            value += coefficients_[tap] * inputHistory_[historyIndex];
        }
        output[static_cast<std::size_t>(phase)] = value * static_cast<double>(factor);
    }
    inputWriteIndex_ = (inputWriteIndex_ + 1) % inputHistorySize;
}

bool Oversampler4x::pushDownsample(double input, int phase, double& output) noexcept
{
    outputHistory_[outputWriteIndex_] = input;
    const bool outputReady = phase == downsamplePhase;
    if (outputReady)
    {
        output = 0.0;
        for (std::size_t tap = 0; tap < tapCount; ++tap)
        {
            const std::size_t historyIndex =
                (outputWriteIndex_ + tapCount - tap) % tapCount;
            output += coefficients_[tap] * outputHistory_[historyIndex];
        }
    }
    outputWriteIndex_ = (outputWriteIndex_ + 1) % tapCount;
    return outputReady;
}

void DistortionPlusModel::prepare(double sampleRate)
{
    sampleRate_ = std::isfinite(sampleRate) ? std::max(8000.0, sampleRate) : 48000.0;
    substepRate_ = sampleRate_ * static_cast<double>(Oversampler4x::factor);
    timestep_ = 1.0 / substepRate_;
    constexpr double smoothingTimeSeconds = 0.005;
    smoothingCoefficient_ = 1.0 - std::exp(-1.0 / (smoothingTimeSeconds * sampleRate_));
    oversampler_.prepare();
    reset();
}

void DistortionPlusModel::reset() noexcept
{
    inputRfCapVoltage_ = 0.0;
    inputCouplingCapVoltage_ = 0.0;
    gainCapVoltage_ = 0.0;
    opAmpOutputVoltage_ = 0.0;
    clipState_ = {};
    oversampler_.reset();
    dryDelay_.fill(0.0);
    dryDelayWriteIndex_ = 0;
    currentDistortion_ = getDistortion();
    currentOutput_ = getOutput();
    currentWetMix_ = getBypass() ? 0.0 : 1.0;
}

void DistortionPlusModel::setDistortion(float normalized) noexcept
{
    if (std::isfinite(normalized))
        distortionTarget_.store(std::clamp(normalized, 0.0f, 1.0f), std::memory_order_relaxed);
}

void DistortionPlusModel::setOutput(float normalized) noexcept
{
    if (std::isfinite(normalized))
        outputTarget_.store(std::clamp(normalized, 0.0f, 1.0f), std::memory_order_relaxed);
}

void DistortionPlusModel::setBypass(bool shouldBypass) noexcept
{
    bypassTarget_.store(shouldBypass, std::memory_order_relaxed);
}

void DistortionPlusModel::setClippingDiodePreset(ClippingDiodePreset preset) noexcept
{
    const auto raw = static_cast<std::uint32_t>(preset);
    const auto maximum = static_cast<std::uint32_t>(ClippingDiodePreset::NoDiodes);
    clippingDiodePresetTarget_.store(raw <= maximum ? raw : 0U, std::memory_order_relaxed);
}

bool DistortionPlusModel::setCircuitParameters(
    const DistortionPlusCircuitParameters& parameters) noexcept
{
    if (!distortionPlusCircuitParametersValid(parameters))
        return false;
    circuitParameters_ = parameters;
    return true;
}

void DistortionPlusModel::setCalibration(const CircuitCalibration& calibration) noexcept
{
    if (std::isfinite(calibration.inputVoltsPerFullScale)
        && calibration.inputVoltsPerFullScale > 0.0)
    {
        calibration_.inputVoltsPerFullScale = calibration.inputVoltsPerFullScale;
    }
    if (std::isfinite(calibration.outputFullScalePerVolt)
        && calibration.outputFullScalePerVolt > 0.0)
    {
        calibration_.outputFullScalePerVolt = calibration.outputFullScalePerVolt;
    }
    if (std::isfinite(calibration.sourceResistanceOhms)
        && calibration.sourceResistanceOhms >= minimumResistance)
    {
        calibration_.sourceResistanceOhms = calibration.sourceResistanceOhms;
    }
    if (std::isfinite(calibration.outputLoadOhms)
        && calibration.outputLoadOhms >= minimumResistance)
    {
        calibration_.outputLoadOhms = calibration.outputLoadOhms;
    }
}

float DistortionPlusModel::getDistortion() const noexcept
{
    return distortionTarget_.load(std::memory_order_relaxed);
}

float DistortionPlusModel::getOutput() const noexcept
{
    return outputTarget_.load(std::memory_order_relaxed);
}

bool DistortionPlusModel::getBypass() const noexcept
{
    return bypassTarget_.load(std::memory_order_relaxed);
}

ClippingDiodePreset DistortionPlusModel::getClippingDiodePreset() const noexcept
{
    const auto raw = clippingDiodePresetTarget_.load(std::memory_order_relaxed);
    const auto maximum = static_cast<std::uint32_t>(ClippingDiodePreset::NoDiodes);
    return static_cast<ClippingDiodePreset>(raw <= maximum ? raw : 0U);
}

DistortionPlusCircuitParameters DistortionPlusModel::getCircuitParameters() const noexcept
{
    return circuitParameters_;
}

CircuitCalibration DistortionPlusModel::getCalibration() const noexcept
{
    return calibration_;
}

double DistortionPlusModel::smoothToward(double current,
                                         double target,
                                         double coefficient) noexcept
{
    return current + coefficient * (target - current);
}

double DistortionPlusModel::outputWiperFraction(double normalized) const noexcept
{
    return std::pow(std::clamp(normalized, 0.0, 1.0),
                    circuitParameters_.outputTaperExponent);
}

float DistortionPlusModel::processSample(float input) noexcept
{
    if (!std::isfinite(input))
    {
        recoverFromNonFinite();
        input = 0.0f;
    }

    const double distortionTarget = distortionTarget_.load(std::memory_order_relaxed);
    const double outputTarget = outputTarget_.load(std::memory_order_relaxed);
    const double wetTarget = bypassTarget_.load(std::memory_order_relaxed) ? 0.0 : 1.0;
    currentDistortion_ = smoothToward(currentDistortion_, distortionTarget, smoothingCoefficient_);
    currentOutput_ = smoothToward(currentOutput_, outputTarget, smoothingCoefficient_);
    currentWetMix_ = smoothToward(currentWetMix_, wetTarget, smoothingCoefficient_);

    const double inputScale = calibration_.inputVoltsPerFullScale;
    const double outputScale = calibration_.outputFullScalePerVolt;
    const double sourceResistance = calibration_.sourceResistanceOhms;
    const double outputLoad = calibration_.outputLoadOhms;

    std::array<double, Oversampler4x::factor> oversampledInput {};
    oversampler_.upsample(static_cast<double>(input) * inputScale, oversampledInput);

    double wetVolts = 0.0;
    for (int phase = 0; phase < Oversampler4x::factor; ++phase)
    {
        const double circuitOutput = processCircuitSubstep(
            oversampledInput[static_cast<std::size_t>(phase)],
            currentDistortion_,
            currentOutput_,
            sourceResistance,
            outputLoad);
        oversampler_.pushDownsample(circuitOutput, phase, wetVolts);
    }

    const std::size_t dryReadIndex =
        (dryDelayWriteIndex_ + dryDelay_.size() - Oversampler4x::wetDelayHostSamples)
        % dryDelay_.size();
    const double delayedDry = dryDelay_[dryReadIndex];
    dryDelay_[dryDelayWriteIndex_] = input;
    dryDelayWriteIndex_ = (dryDelayWriteIndex_ + 1) % dryDelay_.size();

    const double wetDigital = wetVolts * outputScale;
    const double mixed = delayedDry * (1.0 - currentWetMix_) + wetDigital * currentWetMix_;
    if (!std::isfinite(mixed))
    {
        recoverFromNonFinite();
        return 0.0f;
    }
    return static_cast<float>(std::clamp(mixed, -1.0, 1.0));
}

double DistortionPlusModel::processInputNetwork(double sourceVolts,
                                                double sourceResistance) noexcept
{
    const double sourceG = 1.0 / std::max(sourceResistance, minimumResistance);
    const double rfCapG = circuitParameters_.inputRfCapacitanceFarads / timestep_;
    const double couplingCapG = circuitParameters_.inputCouplingCapacitanceFarads / timestep_;
    const double inputResistance = parallelResistance(circuitParameters_.inputBiasResistanceOhms,
                                                       circuitParameters_.opAmpInputResistanceOhms);
    const double inputG = 1.0 / inputResistance;

    const double a11 = sourceG + rfCapG + couplingCapG;
    const double a12 = -couplingCapG;
    const double a21 = -couplingCapG;
    const double a22 = inputG + couplingCapG;
    const double rhs1 = sourceG * sourceVolts
                      + rfCapG * inputRfCapVoltage_
                      + couplingCapG * inputCouplingCapVoltage_;
    const double rhs2 = -couplingCapG * inputCouplingCapVoltage_;
    const double determinant = a11 * a22 - a12 * a21;
    if (!std::isfinite(determinant) || std::abs(determinant) < 1.0e-24)
        return 0.0;

    const double rfNode = (rhs1 * a22 - a12 * rhs2) / determinant;
    const double opAmpInput = (a11 * rhs2 - rhs1 * a21) / determinant;
    inputRfCapVoltage_ = rfNode;
    inputCouplingCapVoltage_ = rfNode - opAmpInput;
    return opAmpInput;
}

double DistortionPlusModel::processOpAmp(double inputVolts,
                                         double feedbackCurrentAmps,
                                         double noiseGain) noexcept
{
    const double idealTarget = inputVolts
        + circuitParameters_.feedbackResistanceOhms * feedbackCurrentAmps;
    const double closedLoopBandwidth = std::clamp(
        circuitParameters_.opAmpGainBandwidthHz / std::max(noiseGain, 1.0),
                                                  10.0,
                                                  substepRate_ * 0.45);
    const double alpha = 1.0 - std::exp(-2.0 * pi * closedLoopBandwidth * timestep_);
    const double bandwidthLimitedTarget =
        opAmpOutputVoltage_ + alpha * (idealTarget - opAmpOutputVoltage_);
    const double maximumDelta = circuitParameters_.opAmpSlewRateVoltsPerSecond * timestep_;
    const double delta = std::clamp(bandwidthLimitedTarget - opAmpOutputVoltage_,
                                    -maximumDelta,
                                    maximumDelta);
    opAmpOutputVoltage_ = softRailLimit(opAmpOutputVoltage_ + delta,
                                        circuitParameters_.opAmpSwingVolts,
                                        circuitParameters_.opAmpSoftKneeVolts);
    return opAmpOutputVoltage_;
}

double DistortionPlusModel::processCircuitSubstep(double inputVolts,
                                                  double gainControl,
                                                  double outputControl,
                                                  double sourceResistance,
                                                  double outputLoad) noexcept
{
    const double opAmpInput = processInputNetwork(inputVolts, sourceResistance);

    const double potFraction = std::pow(
        1.0 - std::clamp(gainControl, 0.0, 1.0),
        circuitParameters_.gainPotCurveExponent);
    const double gainResistance =
        circuitParameters_.gainMinimumResistanceOhms
        + circuitParameters_.gainPotentiometerResistanceOhms * potFraction;
    const double gainCapG = circuitParameters_.gainCapacitanceFarads / timestep_;
    const double gainResistorG = 1.0 / gainResistance;
    const double newGainCapVoltage =
        (gainCapG * gainCapVoltage_ + gainResistorG * opAmpInput)
        / (gainCapG + gainResistorG);
    const double feedbackCurrent =
        (opAmpInput - newGainCapVoltage) / gainResistance;
    gainCapVoltage_ = newGainCapVoltage;

    const double noiseGain = 1.0
        + circuitParameters_.feedbackResistanceOhms / gainResistance;
    const double opAmpOutput = processOpAmp(opAmpInput,
                                           feedbackCurrent,
                                           noiseGain);

    const double wiper = outputWiperFraction(outputControl);
    const double resistanceBelowWiper =
        circuitParameters_.outputPotentiometerResistanceOhms * wiper;
    const double resistanceAboveWiper =
        circuitParameters_.outputPotentiometerResistanceOhms - resistanceBelowWiper;
    const double loadedLowerResistance = resistanceBelowWiper <= minimumResistance
        ? 0.0
        : parallelResistance(resistanceBelowWiper, outputLoad);
    const double clippingNodeLoad = std::max(resistanceAboveWiper + loadedLowerResistance,
                                             minimumResistance);
    const double wiperGain = loadedLowerResistance <= 0.0
        ? 0.0
        : loadedLowerResistance / clippingNodeLoad;

    ClipNetworkParameters clipParameters;
    clipParameters.seriesResistanceOhms = circuitParameters_.clipSeriesResistanceOhms;
    clipParameters.couplingCapacitanceFarads =
        circuitParameters_.postOpAmpCouplingCapacitanceFarads;
    clipParameters.shuntCapacitanceFarads =
        circuitParameters_.clipShuntCapacitanceFarads;
    clipParameters.loadResistanceOhms = clippingNodeLoad;
    const auto diode = diodeModelParameters(getClippingDiodePreset());
    clipParameters.diodeSaturationCurrentAmps = diode.saturationCurrentAmps;
    clipParameters.diodeIdealityFactor = diode.idealityFactor;
    clipParameters.thermalVoltageVolts = diode.thermalVoltageVolts;

    const auto clipResult = processClipNetwork(opAmpOutput,
                                               timestep_,
                                               clipParameters,
                                               clipState_);
    if (!clipResult.converged)
        return 0.0;
    return clipResult.clipNodeVoltage * wiperGain;
}

void DistortionPlusModel::recoverFromNonFinite() noexcept
{
    reset();
}

} // namespace circuitpedal
