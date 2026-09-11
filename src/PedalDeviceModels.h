#pragma once

#include "GenericCircuit.h"

namespace circuitpedal {

// Compact analogue-device helpers shared by circuit models that are easier to
// express programmatically than as one-off parameter literals. These remain
// deliberately lightweight pedal-scale approximations rather than complete
// manufacturer SPICE macromodels.
GenericOpAmpModel tl072Model() noexcept;

} // namespace circuitpedal
