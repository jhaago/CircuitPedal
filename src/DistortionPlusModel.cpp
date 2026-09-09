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

} // namespace

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

double DistortionPlusModel::outputWiperFraction(double normalized) noexcept
{
    // Nominal 10%-at-midpoint audio taper.
    constexpr double audioTaperExponent = 3.321928094887362;
    return std::pow(std::clamp(normalized, 0.0, 1.0), audioTaperExponent);
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
    const double rfCapG = C_inputRf / timestep_;
    const double couplingCapG = C_inputCoupling / timestep_;
    const double inputResistance = parallelResistance(R_inputBias, R_opAmpInput);
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
    const double idealTarget = inputVolts + R_feedback * feedbackCurrentAmps;
    const double closedLoopBandwidth = std::clamp(opAmpGainBandwidthHz / std::max(noiseGain, 1.0),
                                                  10.0,
                                                  substepRate_ * 0.45);
    const double alpha = 1.0 - std::exp(-2.0 * pi * closedLoopBandwidth * timestep_);
    const double bandwidthLimitedTarget =
        opAmpOutputVoltage_ + alpha * (idealTarget - opAmpOutputVoltage_);
    const double maximumDelta = opAmpSlewRateVoltsPerSecond * timestep_;
    const double delta = std::clamp(bandwidthLimitedTarget - opAmpOutputVoltage_,
                                    -maximumDelta,
                                    maximumDelta);
    opAmpOutputVoltage_ = softRailLimit(opAmpOutputVoltage_ + delta,
                                        opAmpSwingVolts,
                                        opAmpSoftKneeVolts);
    return opAmpOutputVoltage_;
}

double DistortionPlusModel::processCircuitSubstep(double inputVolts,
                                                  double gainControl,
                                                  double outputControl,
                                                  double sourceResistance,
                                                  double outputLoad) noexcept
{
    const double opAmpInput = processInputNetwork(inputVolts, sourceResistance);

    const double potFraction = std::pow(1.0 - std::clamp(gainControl, 0.0, 1.0), 2.2);
    const double gainResistance =
        R_gainMinimum + R_gainPotentiometer * potFraction;
    const double gainCapG = C_gain / timestep_;
    const double gainResistorG = 1.0 / gainResistance;
    const double newGainCapVoltage =
        (gainCapG * gainCapVoltage_ + gainResistorG * opAmpInput)
        / (gainCapG + gainResistorG);
    const double feedbackCurrent =
        (opAmpInput - newGainCapVoltage) / gainResistance;
    gainCapVoltage_ = newGainCapVoltage;

    const double noiseGain = 1.0 + R_feedback / gainResistance;
    const double opAmpOutput = processOpAmp(opAmpInput,
                                           feedbackCurrent,
                                           noiseGain);

    const double wiper = outputWiperFraction(outputControl);
    const double resistanceBelowWiper = R_outputPotentiometer * wiper;
    const double resistanceAboveWiper = R_outputPotentiometer - resistanceBelowWiper;
    const double loadedLowerResistance = resistanceBelowWiper <= minimumResistance
        ? 0.0
        : parallelResistance(resistanceBelowWiper, outputLoad);
    const double clippingNodeLoad = std::max(resistanceAboveWiper + loadedLowerResistance,
                                             minimumResistance);
    const double wiperGain = loadedLowerResistance <= 0.0
        ? 0.0
        : loadedLowerResistance / clippingNodeLoad;

    ClipNetworkParameters clipParameters;
    clipParameters.seriesResistanceOhms = R_clip;
    clipParameters.couplingCapacitanceFarads = C_postOpAmpCoupling;
    clipParameters.shuntCapacitanceFarads = C_clip;
    clipParameters.loadResistanceOhms = clippingNodeLoad;
    clipParameters.diodeSaturationCurrentAmps = diodeIs;
    clipParameters.diodeIdealityFactor = diodeN;
    clipParameters.thermalVoltageVolts = thermalV;

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
