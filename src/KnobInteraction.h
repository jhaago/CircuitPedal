#pragma once

namespace circuitpedal {

// Applies a physical vertical drag to a normalized knob value. Positive
// verticalPoints always means upward movement, independent of view coordinates.
double normalizedValueAfterVerticalDrag(double initialValue,
                                        double verticalPoints,
                                        bool fineAdjustment);

// Standard audio-knob sweep: lower-left at minimum, top at midpoint, and
// lower-right at maximum. The input is clamped to the normalized range.
double knobAngleDegrees(double normalizedValue);

} // namespace circuitpedal
