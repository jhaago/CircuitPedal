#pragma once

#include "Btdr2Model.h"

#include <string>

namespace circuitpedal {

// Electrical-port adapter around the time-domain BTDR-2 behavioural model.
//
// This class deliberately separates the black-box reverb memory from the MNA
// solver. GenericCircuit can later stamp the published input resistance and
// two finite-output-impedance Thevenin/Norton ports while this object advances
// the hidden digital reverb state once per host sample.
class Btdr2CircuitElement {
public:
    static constexpr double inputResistanceOhms = 10000.0;
    static constexpr double outputResistanceOhms = 220.0;

    bool prepare(double sampleRate,
                 Btdr2Decay decay,
                 std::string& error);
    void reset() noexcept;

    // Advance the hidden reverb state from the voltage present at the module's
    // audio input pin. Returns the open-circuit source voltages for OUT1/OUT2.
    Btdr2StereoSample advance(float inputVolts) noexcept;

    // Current leaving the input node through the module's nominal 10 kOhm
    // input resistance. This form drops directly into a nodal KCL residual.
    static double inputCurrentAmps(double inputNodeVolts) noexcept;

    // Current leaving an output node toward the module's internal Thevenin
    // source through the nominal 220 ohm output impedance. A negative value
    // means the module is sourcing current into the external circuit node.
    static double outputPortCurrentAmps(double nodeVolts,
                                        double sourceVolts) noexcept;

    bool prepared() const noexcept { return prepared_; }
    Btdr2Decay decayType() const noexcept { return decayType_; }

private:
    Btdr2Model model_;
    Btdr2Decay decayType_ = Btdr2Decay::Medium;
    bool prepared_ = false;
};

} // namespace circuitpedal
