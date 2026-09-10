#include "GenericCircuitProcessor.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>

namespace circuitpedal {
namespace {

constexpr std::uint64_t timingStride = 64;

// Preserve normal/non-GUI behaviour at 4x so existing validation continues to
// exercise the production oversampled path. The diagnostic macOS overlay
// explicitly requests 1x before live audio is started.
std::atomic<std::uint32_t> requestedModeRaw {
    static_cast<std::uint32_t>(GenericProcessingMode::FourX)
};
std::atomic<std::uint32_t> activeModeRaw {
    static_cast<std::uint32_t>(GenericProcessingMode::FourX)
};
std::atomic<std::uint64_t> hostSamplesProcessed { 0 };
std::atomic<std::uint64_t> hostSolveFailures { 0 };
std::atomic<std::uint64_t> subSolveFailures { 0 };
std::atomic<std::uint64_t> timedSamples { 0 };
std::atomic<std::uint64_t> sampleBudgetMisses { 0 };
std::atomic<std::uint64_t> maximumProcessNanoseconds { 0 };
std::atomic<std::uint64_t> sampleBudgetNanoseconds { 0 };

GenericProcessingMode decodeMode(std::uint32_t raw) noexcept
{
    return raw == static_cast<std::uint32_t>(GenericProcessingMode::FourX)
        ? GenericProcessingMode::FourX
        : GenericProcessingMode::OneX;
}

void updateMaximumProcessTime(std::uint64_t nanoseconds) noexcept
{
    std::uint64_t previous = maximumProcessNanoseconds.load(std::memory_order_relaxed);
    while (nanoseconds > previous
           && !maximumProcessNanoseconds.compare_exchange_weak(
               previous,
               nanoseconds,
               std::memory_order_relaxed,
               std::memory_order_relaxed))
    {
    }
}

void recordMeasuredProcessingTime(
    const std::chrono::steady_clock::time_point& started) noexcept
{
    const auto elapsed = std::chrono::steady_clock::now() - started;
    const auto elapsedNanoseconds = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
    timedSamples.fetch_add(1, std::memory_order_relaxed);
    updateMaximumProcessTime(elapsedNanoseconds);

    const std::uint64_t budget =
        sampleBudgetNanoseconds.load(std::memory_order_relaxed);
    if (budget > 0 && elapsedNanoseconds > budget)
        sampleBudgetMisses.fetch_add(1, std::memory_order_relaxed);
}

} // namespace

void setGenericProcessingModeForDiagnostics(GenericProcessingMode mode) noexcept
{
    requestedModeRaw.store(static_cast<std::uint32_t>(mode),
                           std::memory_order_relaxed);
}

GenericProcessingMode genericProcessingModeForDiagnostics() noexcept
{
    return decodeMode(requestedModeRaw.load(std::memory_order_relaxed));
}

void resetGenericProcessorDiagnostics() noexcept
{
    hostSamplesProcessed.store(0, std::memory_order_relaxed);
    hostSolveFailures.store(0, std::memory_order_relaxed);
    subSolveFailures.store(0, std::memory_order_relaxed);
    timedSamples.store(0, std::memory_order_relaxed);
    sampleBudgetMisses.store(0, std::memory_order_relaxed);
    maximumProcessNanoseconds.store(0, std::memory_order_relaxed);
}

GenericProcessorDiagnostics genericProcessorDiagnostics() noexcept
{
    GenericProcessorDiagnostics snapshot;
    snapshot.requestedMode = decodeMode(
        requestedModeRaw.load(std::memory_order_relaxed));
    snapshot.activeMode = decodeMode(
        activeModeRaw.load(std::memory_order_relaxed));
    snapshot.hostSamplesProcessed =
        hostSamplesProcessed.load(std::memory_order_relaxed);
    snapshot.hostSolveFailures =
        hostSolveFailures.load(std::memory_order_relaxed);
    snapshot.subSolveFailures =
        subSolveFailures.load(std::memory_order_relaxed);
    snapshot.timedSamples = timedSamples.load(std::memory_order_relaxed);
    snapshot.sampleBudgetMisses =
        sampleBudgetMisses.load(std::memory_order_relaxed);
    snapshot.maximumProcessMicroseconds =
        static_cast<double>(maximumProcessNanoseconds.load(std::memory_order_relaxed))
        / 1000.0;
    snapshot.sampleBudgetMicroseconds =
        static_cast<double>(sampleBudgetNanoseconds.load(std::memory_order_relaxed))
        / 1000.0;
    return snapshot;
}

bool OversampledGenericCircuit::compile(const CircuitDefinition& definition,
                                        double hostSampleRate,
                                        std::string& error)
{
    compiled_ = false;
    lastSolveConverged_ = false;
    error.clear();

    if (!std::isfinite(hostSampleRate)
        || hostSampleRate < 8000.0
        || hostSampleRate > 96000.0)
    {
        error = "Generic diagnostic processing supports host sample rates from 8 kHz to 96 kHz.";
        return false;
    }

    hostSampleRate_ = hostSampleRate;
    outputNode_ = definition.outputNode();
    outputFullScalePerVolt_ = definition.outputFullScalePerVolt();
    activeMode_ = genericProcessingModeForDiagnostics();
    activeModeRaw.store(static_cast<std::uint32_t>(activeMode_),
                        std::memory_order_relaxed);
    sampleBudgetNanoseconds.store(
        static_cast<std::uint64_t>(1.0e9 / hostSampleRate_),
        std::memory_order_relaxed);
    resetGenericProcessorDiagnostics();

    const double circuitRate = activeMode_ == GenericProcessingMode::FourX
        ? hostSampleRate_ * static_cast<double>(factor)
        : hostSampleRate_;
    if (!circuit_.compile(definition, circuitRate, error))
        return false;

    oversampler_.prepare();
    oversampler_.reset();
    compiled_ = true;
    lastSolveConverged_ = true;
    return true;
}

void OversampledGenericCircuit::reset() noexcept
{
    if (!compiled_)
        return;
    circuit_.reset();
    oversampler_.reset();
    lastSolveConverged_ = true;
    resetGenericProcessorDiagnostics();
}

float OversampledGenericCircuit::processSample(float input) noexcept
{
    if (!compiled_ || !std::isfinite(input))
        return 0.0f;

    const std::uint64_t sampleIndex =
        hostSamplesProcessed.fetch_add(1, std::memory_order_relaxed);
    const bool measureTiming = (sampleIndex % timingStride) == 0;
    const auto started = measureTiming
        ? std::chrono::steady_clock::now()
        : std::chrono::steady_clock::time_point {};

    if (activeMode_ == GenericProcessingMode::OneX)
    {
        const float output = circuit_.processSample(input);
        lastSolveConverged_ = circuit_.lastSolveConverged();
        if (!lastSolveConverged_)
        {
            hostSolveFailures.fetch_add(1, std::memory_order_relaxed);
            subSolveFailures.fetch_add(1, std::memory_order_relaxed);
        }
        if (measureTiming)
            recordMeasuredProcessingTime(started);
        return output;
    }

    std::array<double, factor> oversampledInput {};
    oversampler_.upsample(static_cast<double>(input), oversampledInput);

    double wetVolts = 0.0;
    bool allConverged = true;
    std::uint64_t failedSubsteps = 0;
    for (int phase = 0; phase < factor; ++phase)
    {
        // Advance circuit state at the oversampled rate. The anti-alias filter
        // operates on the raw circuit-domain output so digital full-scale
        // clipping is applied only after decimation.
        (void)circuit_.processSample(
            static_cast<float>(
                oversampledInput[static_cast<std::size_t>(phase)]));
        const bool converged = circuit_.lastSolveConverged();
        allConverged = allConverged && converged;
        if (!converged)
            ++failedSubsteps;

        const double circuitOutputVolts = circuit_.nodeVoltage(outputNode_);
        (void)oversampler_.pushDownsample(
            circuitOutputVolts,
            phase,
            wetVolts);
    }

    lastSolveConverged_ = allConverged;
    if (failedSubsteps > 0)
    {
        hostSolveFailures.fetch_add(1, std::memory_order_relaxed);
        subSolveFailures.fetch_add(failedSubsteps, std::memory_order_relaxed);
    }

    float result = 0.0f;
    if (allConverged && std::isfinite(wetVolts))
    {
        const double wetDigital = wetVolts * outputFullScalePerVolt_;
        if (std::isfinite(wetDigital))
            result = static_cast<float>(std::clamp(wetDigital, -1.0, 1.0));
    }

    if (measureTiming)
        recordMeasuredProcessingTime(started);
    return result;
}

} // namespace circuitpedal
