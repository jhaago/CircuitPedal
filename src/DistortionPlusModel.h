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

// V0.5 moves the fixed Distortion+ component values into an explicit circuit
// definition object. This is still a dedicated Distortion+ solver, but it is an
// important boundary on the road to a compiled general netlist/MNA engine.
struct DistortionPlusCircuitParameters {
    double inputBiasResistanceOhms = 1.0e6;
    double opAmpInputResistanceOhms = 2.0e6;
    double inputCouplingCapacitanceFarads = 10.0e-9;
    double inputRfCapacitanceFarads = 1.0e-9;

    double feedbackResistanceOhms = 1.0e6;
    double gainMinimumResistanceOhms = 4.7e3;
    double gainPotentiometerResistanceOhms = 500.0e3;
    double gainCapacitanceFarads = 47.0e-9;
    double gainPotCurveExponent = 2.2;

    double clipSeriesResistanceOhms = 10.0e3;
    double postOpAmpCouplingCapacitanceFarads = 1.0e-6;
    double clipShuntCapacitanceFarads = 1.0e-9;
    double outputPotentiometerResistanceOhms = 50.0e3;
    double outputTaperExponent = 3.321928094887362;

    double opAmpGainBandwidthHz = 1.0e6;
    double opAmpSlewRateVoltsPerSecond = 0.5e6;
    double opAmpSwingVolts = 3.2;
    double opAmpSoftKneeVolts = 0.2;
};

bool distortionPlusCircuitParametersValid(
    const DistortionPlusCircuitParameters& parameters) noexcept;

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
    // Circuit parameters and calibration are configuration, not live controls.
    // Call only while audio processing is stopped, then prepare/reset before
    // restarting the real-time engine.
    bool setCircuitParameters(const DistortionPlusCircuitParameters& parameters) noexcept;
    void setCalibration(const CircuitCalibration& calibration) noexcept;

    float getDistortion() const noexcept;
    float getOutput() const noexcept;
    bool getBypass() const noexcept;
    ClippingDiodePreset getClippingDiodePreset() const noexcept;
    DistortionPlusCircuitParameters getCircuitParameters() const noexcept;
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
    double outputWiperFraction(double normalized) const noexcept;
    static double smoothToward(double current, double target, double coefficient) noexcept;
    void recoverFromNonFinite() noexcept;

    DistortionPlusCircuitParameters circuitParameters_ {};

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
