#include "PedalDeviceModels.h"

namespace circuitpedal {

GenericOpAmpModel tl072Model() noexcept
{
    GenericOpAmpModel model;

    // Pedal-scale TL072 approximation. The intent is to capture the properties
    // that materially affect audio-band circuit behaviour in the current MNA
    // solver: high DC gain, roughly 3 MHz gain-bandwidth, fast slew rate and
    // finite output swing on a 9 V single supply. Input-bias/FET input effects
    // and full manufacturer macromodel detail remain future device-library work.
    model.openLoopGain = 200000.0;
    model.gainBandwidthHz = 3.0e6;
    model.slewRateVoltsPerSecond = 13.0e6;
    model.outputHeadroomVolts = 1.5;
    model.inputOffsetVolts = 0.0;
    return model;
}

} // namespace circuitpedal
