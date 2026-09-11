#include "Btdr2Model.h"
#include "BoingReverbModel.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>

namespace {

bool expect(bool condition, const char* message)
{
    if (!condition)
        std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool testBtdr2Impulse()
{
    circuitpedal::Btdr2Model brick;
    std::string error;
    if (!expect(brick.prepare(48000.0,
                              circuitpedal::Btdr2Decay::Medium,
                              error),
                "BTDR-2 medium should prepare"))
    {
        std::cerr << error << '\n';
        return false;
    }

    bool ok = true;
    ok &= expect(std::abs(brick.nominalT60Seconds() - 2.5) < 1.0e-9,
                 "medium BTDR-2 should report 2.5 s nominal T60");

    constexpr double sampleRate = 48000.0;
    constexpr std::size_t samples = static_cast<std::size_t>(sampleRate * 4.0);
    float earlyPeak = 0.0f;
    float midTailPeak = 0.0f;
    float lateTailPeak = 0.0f;
    float maximumPeak = 0.0f;
    bool finite = true;

    for (std::size_t i = 0; i < samples; ++i)
    {
        const float input = i == 0 ? 0.5f : 0.0f;
        const auto output = brick.processSample(input);
        finite = finite && std::isfinite(output.left) && std::isfinite(output.right);
        const float magnitude = std::max(std::abs(output.left), std::abs(output.right));
        maximumPeak = std::max(maximumPeak, magnitude);
        const double time = static_cast<double>(i) / sampleRate;
        if (time >= 0.035 && time < 0.20)
            earlyPeak = std::max(earlyPeak, magnitude);
        if (time >= 0.45 && time < 1.00)
            midTailPeak = std::max(midTailPeak, magnitude);
        if (time >= 2.7 && time < 3.5)
            lateTailPeak = std::max(lateTailPeak, magnitude);
    }

    ok &= expect(finite, "BTDR-2 impulse response should remain finite");
    ok &= expect(earlyPeak > 0.01f,
                 "BTDR-2 should produce a delayed early reverb field");
    ok &= expect(midTailPeak > 0.0002f,
                 "BTDR-2 medium should retain a measurable tail after 450 ms");
    ok &= expect(lateTailPeak < earlyPeak * 0.25f,
                 "BTDR-2 tail should decay substantially by about 3 seconds");
    ok &= expect(maximumPeak < 1.6f,
                 "BTDR-2 behavioural model should remain within a safe voltage envelope");

    brick.reset();
    float resetPeak = 0.0f;
    for (int i = 0; i < 12000; ++i)
    {
        const auto output = brick.processSample(0.0f);
        resetPeak = std::max(resetPeak,
                             std::max(std::abs(output.left), std::abs(output.right)));
    }
    ok &= expect(resetPeak < 1.0e-8f,
                 "BTDR-2 reset should clear the reverb tail");
    return ok;
}

bool testBtdr2Variants()
{
    bool ok = true;
    std::string error;
    circuitpedal::Btdr2Model shortBrick;
    circuitpedal::Btdr2Model mediumBrick;
    circuitpedal::Btdr2Model longBrick;

    ok &= expect(shortBrick.prepare(48000.0,
                                    circuitpedal::Btdr2Decay::Short,
                                    error),
                 "short BTDR-2 should prepare");
    ok &= expect(mediumBrick.prepare(48000.0,
                                     circuitpedal::Btdr2Decay::Medium,
                                     error),
                 "medium BTDR-2 should prepare");
    ok &= expect(longBrick.prepare(48000.0,
                                   circuitpedal::Btdr2Decay::Long,
                                   error),
                 "long BTDR-2 should prepare");
    ok &= expect(shortBrick.nominalT60Seconds() < mediumBrick.nominalT60Seconds(),
                 "short decay should be shorter than medium");
    ok &= expect(mediumBrick.nominalT60Seconds() < longBrick.nominalT60Seconds(),
                 "medium decay should be shorter than long");

    circuitpedal::Btdr2Model invalid;
    ok &= expect(!invalid.prepare(1000.0,
                                  circuitpedal::Btdr2Decay::Medium,
                                  error),
                 "unsupported BTDR-2 sample rate should fail cleanly");
    return ok;
}

bool testBoingDryAndTail()
{
    circuitpedal::BoingReverbModel boing;
    std::string error;
    if (!expect(boing.prepare(48000.0, error),
                "Boing draft should prepare"))
    {
        std::cerr << error << '\n';
        return false;
    }

    bool ok = true;
    boing.setReverb(0.0f);
    for (int i = 0; i < 2000; ++i)
    {
        const float input = 0.2f * std::sin(2.0 * 3.14159265358979323846
                                            * 110.0
                                            * static_cast<double>(i)
                                            / 48000.0);
        const float output = boing.processSample(input);
        ok &= expect(std::abs(output - input) < 1.0e-6f,
                     "Boing at zero reverb should preserve the dry signal");
        if (!ok)
            break;
    }

    boing.reset();
    boing.setReverb(1.0f);
    float tailPeak = 0.0f;
    bool finite = true;
    for (int i = 0; i < 48000; ++i)
    {
        const float input = i == 0 ? 0.5f : 0.0f;
        const float output = boing.processSample(input);
        finite = finite && std::isfinite(output);
        if (i > 3000)
            tailPeak = std::max(tailPeak, std::abs(output));
    }
    ok &= expect(finite, "Boing output should remain finite");
    ok &= expect(tailPeak > 0.001f,
                 "Boing should produce an audible wet tail after the dry impulse");
    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok &= testBtdr2Impulse();
    ok &= testBtdr2Variants();
    ok &= testBoingDryAndTail();

    if (!ok)
        return 1;

    std::cout << "BTDR-2 / Boing behavioural validation passed.\n";
    return 0;
}
