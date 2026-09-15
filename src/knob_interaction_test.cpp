#include "KnobInteraction.h"

#include <cmath>
#include <iostream>

namespace {

bool expectNear(double actual, double expected, const char* message)
{
    if (std::abs(actual - expected) <= 1.0e-9)
        return true;

    std::cerr << "FAIL: " << message << " (expected " << expected
              << ", got " << actual << ")\n";
    return false;
}

} // namespace

int main()
{
    bool ok = true;

    ok &= expectNear(circuitpedal::normalizedValueAfterVerticalDrag(0.5, 18.0, false),
                     0.6,
                     "dragging upward increases the normalized value");
    ok &= expectNear(circuitpedal::normalizedValueAfterVerticalDrag(0.5, -18.0, false),
                     0.4,
                     "dragging downward decreases the normalized value");
    ok &= expectNear(circuitpedal::normalizedValueAfterVerticalDrag(0.5, 18.0, true),
                     0.51,
                     "fine adjustment has one tenth normal sensitivity");
    ok &= expectNear(circuitpedal::normalizedValueAfterVerticalDrag(0.95, 180.0, false),
                     1.0,
                     "vertical dragging clamps at the maximum");
    ok &= expectNear(circuitpedal::normalizedValueAfterVerticalDrag(0.05, -180.0, false),
                     0.0,
                     "vertical dragging clamps at the minimum");

    ok &= expectNear(circuitpedal::knobAngleDegrees(0.0),
                     225.0,
                     "minimum points toward the lower left");
    ok &= expectNear(circuitpedal::knobAngleDegrees(0.5),
                     90.0,
                     "midpoint points straight up");
    ok &= expectNear(circuitpedal::knobAngleDegrees(1.0),
                     -45.0,
                     "maximum points toward the lower right");
    ok &= expectNear(circuitpedal::knobAngleDegrees(-1.0),
                     225.0,
                     "angle calculation clamps below the minimum");
    ok &= expectNear(circuitpedal::knobAngleDegrees(2.0),
                     -45.0,
                     "angle calculation clamps above the maximum");

    if (ok)
        std::cout << "Knob interaction validation passed.\n";
    return ok ? 0 : 1;
}
