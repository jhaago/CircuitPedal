#include "KnobInteraction.h"

#include <algorithm>

namespace circuitpedal {

double normalizedValueAfterVerticalDrag(double initialValue,
                                        double verticalPoints,
                                        bool fineAdjustment)
{
    constexpr double pointsForFullRange = 180.0;
    constexpr double fineAdjustmentScale = 0.1;
    const double scale = fineAdjustment ? fineAdjustmentScale : 1.0;
    return std::clamp(initialValue + verticalPoints * scale / pointsForFullRange,
                      0.0,
                      1.0);
}

double knobAngleDegrees(double normalizedValue)
{
    constexpr double minimumAngle = 225.0;
    constexpr double sweepDegrees = 270.0;
    return minimumAngle - sweepDegrees * std::clamp(normalizedValue, 0.0, 1.0);
}

} // namespace circuitpedal
