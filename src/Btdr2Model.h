#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace circuitpedal {

enum class Btdr2Decay {
    Short,
    Medium,
    Long
};

struct Btdr2StereoSample {
    float left = 0.0f;
    float right = 0.0f;
};

// Compact realtime behavioural model of the Accutronics/Belton BTDR-2 module.
//
// The real BTDR-2 is itself a potted digital reverb component. Treating it as
// a black-box component is therefore closer to the physical pedal than
// replacing an entire reverb pedal with a generic room algorithm. This first
// model intentionally targets the published electrical envelope and broad
// Belton-brick character; it is not yet calibrated from a captured physical
// BTDR-2 impulse response.
//
// prepare() may allocate. processSample() performs no allocation or locking.
class Btdr2Model {
public:
    bool prepare(double sampleRate,
                 Btdr2Decay decay,
                 std::string& error);
    void reset() noexcept;

    // Input and outputs are volts at the BTDR-2 signal pins. The published
    // module limit is approximately 1.5 V peak and each output is nominally
    // about -3 dB relative to the input reference.
    Btdr2StereoSample processSample(float inputVolts) noexcept;

    double sampleRate() const noexcept { return sampleRate_; }
    double nominalT60Seconds() const noexcept { return nominalT60Seconds_; }
    Btdr2Decay decayType() const noexcept { return decayType_; }
    bool prepared() const noexcept { return prepared_; }

private:
    struct DelayLine {
        std::vector<float> buffer;
        std::size_t writeIndex = 0;
        float feedback = 0.0f;
        float dampingState = 0.0f;
    };

    static double t60ForDecay(Btdr2Decay decay) noexcept;
    static std::size_t delaySamples(double sampleRate,
                                    double milliseconds) noexcept;
    static float feedbackForT60(double delaySeconds,
                                double t60Seconds) noexcept;

    float processLine(DelayLine& line,
                      float input,
                      float dampingCoefficient) noexcept;
    void prepareLine(DelayLine& line,
                     double delayMilliseconds,
                     double t60Seconds);

    DelayLine earlyA_;
    DelayLine earlyB_;
    DelayLine late_;

    double sampleRate_ = 48000.0;
    double nominalT60Seconds_ = 2.5;
    Btdr2Decay decayType_ = Btdr2Decay::Medium;

    float dampingCoefficient_ = 0.0f;
    float inputDcState_ = 0.0f;
    float previousInput_ = 0.0f;
    bool prepared_ = false;
};

} // namespace circuitpedal
