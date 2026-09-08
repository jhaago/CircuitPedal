#pragma once

#include <atomic>
#include <cmath>
#include <algorithm>

namespace circuitpedal {

class OnePoleHP {
public:
    void prepare(double sampleRate, double cutoffHz);
    void reset() noexcept;
    double process(double x) noexcept;
private:
    double b0_ = 1.0, b1_ = -1.0, a1_ = 0.0;
    double x1_ = 0.0, y1_ = 0.0;
};

class OnePoleLP {
public:
    void prepare(double sampleRate, double cutoffHz);
    void reset() noexcept;
    double process(double x) noexcept;
private:
    double alpha_ = 1.0;
    double y_ = 0.0;
};

/**
 * Component-driven proof-of-concept model of an MXR Distortion+-style circuit.
 *
 * This V0.1 intentionally models the major analogue mechanisms explicitly:
 *  - AC input coupling / input filtering
 *  - frequency-dependent non-inverting op-amp feedback network
 *  - finite op-amp output swing
 *  - 10 kOhm series resistor feeding anti-parallel germanium diodes
 *  - 1 nF shunt capacitor at the clipping node
 *  - output potentiometer
 *
 * It is NOT yet a general arbitrary-netlist solver. The architecture is kept
 * separate from the audio I/O so we can replace this class later with a WDF/
 * modified-nodal-analysis circuit engine without rewriting the app.
 */
class DistortionPlusModel {
public:
    void prepare(double sampleRate);
    void reset() noexcept;

    void setDistortion(float normalized) noexcept;
    void setOutput(float normalized) noexcept;
    void setBypass(bool shouldBypass) noexcept;

    float getDistortion() const noexcept { return distortion_.load(); }
    float getOutput() const noexcept { return output_.load(); }
    bool getBypass() const noexcept { return bypass_.load(); }

    float processSample(float input) noexcept;

private:
    double processCircuitSubstep(double inputVolts) noexcept;
    double solveGermaniumClipNode(double sourceVolts, double dt) noexcept;

    // Nominal analogue component values from common Distortion+ schematics.
    static constexpr double R_feedback = 1.0e6;   // 1 MOhm
    static constexpr double R_gain_min = 4.7e3;   // 4.7 kOhm
    static constexpr double R_gain_pot = 1.0e6;   // 1 MOhm range in reference analysis
    static constexpr double C_gain = 47.0e-9;     // 47 nF
    static constexpr double R_clip = 10.0e3;      // 10 kOhm
    static constexpr double C_clip = 1.0e-9;      // 1 nF

    // Approximate 1N270 germanium diode parameters. These are intentionally
    // exposed here as physical model constants rather than a generic waveshaper.
    static constexpr double diodeIs = 1.0e-6;     // saturation current, A
    static constexpr double diodeN = 1.6;         // ideality factor
    static constexpr double thermalV = 0.02585;   // V at ~25 C

    static constexpr int substeps = 4;

    double sampleRate_ = 48000.0;
    double substepRate_ = 192000.0;
    double previousInputVolts_ = 0.0;
    double clipNodeV_ = 0.0;

    // We treat digital full-scale as roughly 1 V peak for this first bench model.
    // Later this becomes a calibrated parameter tied to the ADC front-end.
    double inputVoltsPerFS_ = 1.0;
    double outputFSPerVolt_ = 1.0;

    OnePoleHP inputCoupling_;
    OnePoleLP inputRfFilter_;
    OnePoleHP postOpAmpCoupling_;

    // State for the frequency-dependent feedback branch (high-pass voltage
    // across R_gain = R_gain_min + pot resistance).
    double feedbackHpX1_ = 0.0;
    double feedbackHpY1_ = 0.0;

    std::atomic<float> distortion_ { 0.65f };
    std::atomic<float> output_ { 0.70f };
    std::atomic<bool> bypass_ { false };
};

} // namespace circuitpedal
