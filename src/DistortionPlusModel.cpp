#include "DistortionPlusModel.h"

namespace circuitpedal {

static constexpr double pi = 3.14159265358979323846;

void OnePoleHP::prepare(double sampleRate, double cutoffHz)
{
    cutoffHz = std::clamp(cutoffHz, 0.1, sampleRate * 0.45);
    const double K = 2.0 * sampleRate / (2.0 * pi * cutoffHz); // 2RC/T
    b0_ = K / (1.0 + K);
    b1_ = -b0_;
    a1_ = (1.0 - K) / (1.0 + K);
    reset();
}

void OnePoleHP::reset() noexcept
{
    x1_ = y1_ = 0.0;
}

double OnePoleHP::process(double x) noexcept
{
    const double y = b0_ * x + b1_ * x1_ - a1_ * y1_;
    x1_ = x;
    y1_ = y;
    return y;
}

void OnePoleLP::prepare(double sampleRate, double cutoffHz)
{
    cutoffHz = std::clamp(cutoffHz, 0.1, sampleRate * 0.45);
    alpha_ = 1.0 - std::exp(-2.0 * pi * cutoffHz / sampleRate);
    reset();
}

void OnePoleLP::reset() noexcept
{
    y_ = 0.0;
}

double OnePoleLP::process(double x) noexcept
{
    y_ += alpha_ * (x - y_);
    return y_;
}

void DistortionPlusModel::prepare(double sampleRate)
{
    sampleRate_ = std::max(8000.0, sampleRate);
    substepRate_ = sampleRate_ * static_cast<double>(substeps);

    // C2=10 nF with effective input impedance ~676 kOhm => ~23.5 Hz.
    inputCoupling_.prepare(substepRate_, 23.5);

    // C1=1 nF shunt cap has source-impedance-dependent behaviour in the real
    // pedal. For V0.1 we retain its RF/very-high-audio attenuation as a pole.
    inputRfFilter_.prepare(substepRate_, 18000.0);

    // C4=1 uF feeding the 10 kOhm clipping resistor => ~15.9 Hz.
    postOpAmpCoupling_.prepare(substepRate_, 15.9);
    reset();
}

void DistortionPlusModel::reset() noexcept
{
    previousInputVolts_ = 0.0;
    clipNodeV_ = 0.0;
    feedbackHpX1_ = 0.0;
    feedbackHpY1_ = 0.0;
    inputCoupling_.reset();
    inputRfFilter_.reset();
    postOpAmpCoupling_.reset();
}

void DistortionPlusModel::setDistortion(float normalized) noexcept
{
    distortion_.store(std::clamp(normalized, 0.0f, 1.0f));
}

void DistortionPlusModel::setOutput(float normalized) noexcept
{
    output_.store(std::clamp(normalized, 0.0f, 1.0f));
}

void DistortionPlusModel::setBypass(bool shouldBypass) noexcept
{
    bypass_.store(shouldBypass);
}

float DistortionPlusModel::processSample(float input) noexcept
{
    if (bypass_.load(std::memory_order_relaxed))
        return input;

    const double targetV = static_cast<double>(input) * inputVoltsPerFS_;
    double outputV = 0.0;

    // Linear interpolation across four nonlinear solver substeps gives the
    // circuit states a smaller timestep. Proper anti-alias oversampling will
    // replace this simple substepping in a later version.
    for (int s = 0; s < substeps; ++s)
    {
        const double t = static_cast<double>(s + 1) / static_cast<double>(substeps);
        const double v = previousInputVolts_ + (targetV - previousInputVolts_) * t;
        outputV += processCircuitSubstep(v);
    }
    previousInputVolts_ = targetV;
    outputV /= static_cast<double>(substeps);

    const double digitalOut = outputV * outputFSPerVolt_;
    return static_cast<float>(std::clamp(digitalOut, -1.0, 1.0));
}

double DistortionPlusModel::processCircuitSubstep(double inputVolts) noexcept
{
    double x = inputCoupling_.process(inputVolts);
    x = inputRfFilter_.process(x);

    // Reverse-log gain control: clockwise distortion reduces the resistance
    // in the inverting branch. This mapping is an approximation of the pot's
    // mechanical taper, while the resistance itself remains the circuit variable.
    const double knob = static_cast<double>(distortion_.load(std::memory_order_relaxed));
    const double potFraction = std::pow(1.0 - knob, 2.2);
    const double Rg = R_gain_min + R_gain_pot * potFraction;

    // The C_gain + Rg branch is a series high-pass from the ideal op-amp's
    // inverting node to AC ground. Calculate the voltage across Rg using a
    // bilinear one-pole high-pass, then use i = V/R and Vout = Vin + Rf*i.
    const double fc = 1.0 / (2.0 * pi * Rg * C_gain);
    const double K = 2.0 * substepRate_ / (2.0 * pi * std::max(fc, 0.01));
    const double b0 = K / (1.0 + K);
    const double b1 = -b0;
    const double a1 = (1.0 - K) / (1.0 + K);
    const double vAcrossRg = b0 * x + b1 * feedbackHpX1_ - a1 * feedbackHpY1_;
    feedbackHpX1_ = x;
    feedbackHpY1_ = vAcrossRg;

    double opAmpV = x + R_feedback * (vAcrossRg / Rg);

    // A 741 on a 9 V single supply cannot reach the rails. Around the 4.5 V
    // bias point, model an approximate +/-3.2 V available AC swing with a
    // smooth transition rather than an impossible ideal infinite swing.
    constexpr double opAmpSwing = 3.2;
    opAmpV = opAmpSwing * std::tanh(opAmpV / opAmpSwing);

    opAmpV = postOpAmpCoupling_.process(opAmpV);

    // Actual clipping-node topology: source -> 10k -> node, with anti-parallel
    // germanium diodes and 1 nF capacitor from node to ground.
    double clipped = solveGermaniumClipNode(opAmpV, 1.0 / substepRate_);

    // Approximate 50 kOhm audio-taper output potentiometer. The control changes
    // the virtual wiper position; it is not an arbitrary post-effect gain knob.
    const double outKnob = static_cast<double>(output_.load(std::memory_order_relaxed));
    const double wiper = outKnob * outKnob;
    return clipped * wiper;
}

double DistortionPlusModel::solveGermaniumClipNode(double sourceVolts, double dt) noexcept
{
    const double oldV = clipNodeV_;
    const double capG = C_clip / dt; // backward-Euler companion conductance
    const double invR = 1.0 / R_clip;
    const double nvT = diodeN * thermalV;

    double v = oldV;
    for (int iter = 0; iter < 7; ++iter)
    {
        const double arg = std::clamp(v / nvT, -20.0, 20.0);
        const double iD = 2.0 * diodeIs * std::sinh(arg);
        const double gD = (2.0 * diodeIs / nvT) * std::cosh(arg);

        // KCL: (v-source)/R + C(v-old)/dt + I_diode(v) = 0
        const double f = (v - sourceVolts) * invR + capG * (v - oldV) + iD;
        const double df = invR + capG + gD;
        v -= f / std::max(df, 1.0e-12);
    }

    clipNodeV_ = std::clamp(v, -2.0, 2.0);
    return clipNodeV_;
}

} // namespace circuitpedal
