#include "Btdr2CircuitElement.h"

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

bool testElectricalPorts()
{
    bool ok = true;

    const double inputCurrent =
        circuitpedal::Btdr2CircuitElement::inputCurrentAmps(1.0);
    ok &= expect(std::abs(inputCurrent - 100.0e-6) < 1.0e-12,
                 "BTDR-2 10k input port should draw 100 uA at 1 V");

    const double sourcedCurrent =
        circuitpedal::Btdr2CircuitElement::outputPortCurrentAmps(0.0, 0.5);
    ok &= expect(std::abs(sourcedCurrent + (0.5 / 220.0)) < 1.0e-12,
                 "BTDR-2 output port should behave like a 220 ohm Thevenin source");

    const double balancedCurrent =
        circuitpedal::Btdr2CircuitElement::outputPortCurrentAmps(0.5, 0.5);
    ok &= expect(std::abs(balancedCurrent) < 1.0e-15,
                 "BTDR-2 output port current should be zero at its source voltage");
    return ok;
}

bool testRealtimeAdvance()
{
    circuitpedal::Btdr2CircuitElement element;
    std::string error;
    if (!expect(element.prepare(48000.0,
                                circuitpedal::Btdr2Decay::Medium,
                                error),
                "BTDR-2 circuit element should prepare"))
    {
        std::cerr << error << '\n';
        return false;
    }

    bool finite = true;
    float tailPeak = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const float input = i == 0 ? 0.5f : 0.0f;
        const auto output = element.advance(input);
        finite = finite
            && std::isfinite(output.left)
            && std::isfinite(output.right);
        if (i > 3000)
        {
            tailPeak = std::max(tailPeak,
                                std::max(std::abs(output.left),
                                         std::abs(output.right)));
        }
    }

    bool ok = true;
    ok &= expect(finite,
                 "BTDR-2 circuit element should remain finite");
    ok &= expect(tailPeak > 0.001f,
                 "BTDR-2 circuit element should expose a delayed wet tail");

    element.reset();
    float resetPeak = 0.0f;
    for (int i = 0; i < 8000; ++i)
    {
        const auto output = element.advance(0.0f);
        resetPeak = std::max(resetPeak,
                             std::max(std::abs(output.left),
                                      std::abs(output.right)));
    }
    ok &= expect(resetPeak < 1.0e-8f,
                 "BTDR-2 circuit element reset should clear hidden reverb state");
    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok &= testElectricalPorts();
    ok &= testRealtimeAdvance();

    if (!ok)
        return 1;

    std::cout << "BTDR-2 electrical port validation passed.\n";
    return 0;
}
