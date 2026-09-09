#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace circuitpedal {

struct CircuitCalibration {
    double inputVoltsPerFullScale = 1.0;
    double outputFullScalePerVolt = 1.0;
    double sourceResistanceOhms = 10.0e3;
    double outputLoadOhms = 1.0e6;
};

// V0.4 starts exposing physical circuit substitutions without replacing the
// circuit solver with generic DSP blocks. The reference germanium setting is
// exactly the V0.2 diode model; the other presets are deliberately labelled
// experimental until they are fitted to measured devices.
enum class ClippingDiodePreset : std::uint32_t {
    ReferenceGermanium = 0,
    SiliconLike = 1,
    LedLike = 2,
    NoDiodes = 3
};

struct DiodeModelParameters {
    double saturationCurrentAmps = 1.0e-6;
    double idealityFactor = 1.6;
    double thermalVoltageVolts = 0.02585;
};

const char* clippingDiodePresetName(ClippingDiodePreset preset) noexcept;
DiodeModelParameters diodeModelParameters(ClippingDiodePreset preset) noexcept;

struct ClipNetworkParameters {
    double seriesResistanceOhms = 10.0e3;
    double couplingCapacitanceFarads = 1.0e-6;
    double shuntCapacitanceFarads = 1.0e-9;
    double loadResistanceOhms = 50.0e3;
    double diodeSaturationCurrentAmps = 1.0e-6;
    double diodeIdealityFactor = 1.6;
    double thermalVoltageVolts = 0.02585;
};

struct ClipNetworkState {
    double couplingCapVoltage = 0.0;
    double clipNodeVoltage = 0.0;
};

struct ClipNetworkResult {
    double clipNodeVoltage = 0.0;
    double seriesNodeVoltage = 0.0;
    double kclResidualAmps = 0.0;
    std::uint32_t iterations = 0;
    bool converged = false;
};

ClipNetworkResult processClipNetwork(double sourceVolts,
                                     double timestepSeconds,
                                     const ClipNetworkParameters& parameters,
                                     ClipNetworkState& state) noexcept;

class Oversampler4x {
public:
    static constexpr int factor = 4;
    static constexpr std::size_t tapCount = 191;
    static constexpr std::size_t inputHistorySize = (tapCount + factor - 1) / factor;
    static constexpr int downsamplePhase = 2;
    static constexpr std::size_t wetDelayHostSamples =
        (tapCount - 1 - static_cast<std::size_t>(downsamplePhase)) / factor;

    void prepare() noexcept;
    void reset() noexcept;
    void upsample(double input, std::array<double, factor>& output) noexcept;
    bool pushDownsample(double input, int phase, double& output) noexcept;

private:
    std::array<double, tapCount> coefficients_ {};
    std::array<double, inputHistorySize> inputHistory_ {};
    std::array<double, tapCount> outputHistory_ {};
    std::size_t inputWriteIndex_ = 0;
    std::size_t outputWriteIndex_ = 0;
};

class DistortionPlusModel {
public:
    void prepare(double sampleRate);
    void reset() noexcept;

    void setDistortion(float normalized) noexcept;
    void setOutput(float normalized) noexcept;
    void setBypass(bool shouldBypass) noexcept;
    void setClippingDiodePreset(ClippingDiodePreset preset) noexcept;
    // Calibration is configuration, not a live control. Call only while audio
    // processing is stopped, then call reset() before restarting.
    void setCalibration(const CircuitCalibration& calibration) noexcept;

    float getDistortion() const noexcept;
    float getOutput() const noexcept;
    bool getBypass() const noexcept;
    ClippingDiodePreset getClippingDiodePreset() const noexcept;
    CircuitCalibration getCalibration() const noexcept;

    float processSample(float input) noexcept;

private:
    double processCircuitSubstep(double inputVolts,
                                 double gainControl,
                                 double outputControl,
                                 double sourceResistance,
                                 double outputLoad) noexcept;
    double processInputNetwork(double sourceVolts, double sourceResistance) noexcept;
    double processOpAmp(double inputVolts, double feedbackCurrentAmps, double noiseGain) noexcept;
    static double outputWiperFraction(double normalized) noexcept;
    static double smoothToward(double current, double target, double coefficient) noexcept;
    void recoverFromNonFinite() noexcept;

    // Pinned V0.2 reference values. See docs/reference_circuit.md.
    static constexpr double R_inputBias = 1.0e6;
    static constexpr double R_opAmpInput = 2.0e6;
    static constexpr double C_inputCoupling = 10.0e-9;
    static constexpr double C_inputRf = 1.0e-9;
    static constexpr double R_feedback = 1.0e6;
    static constexpr double R_gainMinimum = 4.7e3;
    static constexpr double R_gainPotentiometer = 500.0e3;
    static constexpr double C_gain = 47.0e-9;
    static constexpr double R_clip = 10.0e3;
    static constexpr double C_postOpAmpCoupling = 1.0e-6;
    static constexpr double C_clip = 1.0e-9;
    static constexpr double R_outputPotentiometer = 50.0e3;

    static constexpr double opAmpGainBandwidthHz = 1.0e6;
    static constexpr double opAmpSlewRateVoltsPerSecond = 0.5e6;
    static constexpr double opAmpSwingVolts = 3.2;
    static constexpr double opAmpSoftKneeVolts = 0.2;

    double sampleRate_ = 48000.0;
    double substepRate_ = 192000.0;
    double timestep_ = 1.0 / 192000.0;
    double smoothingCoefficient_ = 0.0;

    double inputRfCapVoltage_ = 0.0;
    double inputCouplingCapVoltage_ = 0.0;
    double gainCapVoltage_ = 0.0;
    double opAmpOutputVoltage_ = 0.0;

    ClipNetworkState clipState_;
    Oversampler4x oversampler_;

    std::array<double, 64> dryDelay_ {};
    std::size_t dryDelayWriteIndex_ = 0;

    double currentDistortion_ = 0.65;
    double currentOutput_ = 0.70;
    double currentWetMix_ = 1.0;

    std::atomic<float> distortionTarget_ { 0.65f };
    std::atomic<float> outputTarget_ { 0.70f };
    std::atomic<bool> bypassTarget_ { false };
    std::atomic<std::uint32_t> clippingDiodePresetTarget_ {
        static_cast<std::uint32_t>(ClippingDiodePreset::ReferenceGermanium)
    };
    CircuitCalibration calibration_;
};

} // namespace circuitpedal
