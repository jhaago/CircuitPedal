# CircuitPedal Distortion+ V0.2 reference circuit

This document is the normative circuit definition for the V0.2 model. It prevents code, tests and later schematic work from silently targeting different pedal variants.

V0.2 is an AC-equivalent model centred around the pedal's nominal 4.5 V bias point. It is a defined engineering reference, not yet a claim that every historical or reissue MXR Distortion+ uses these exact values.

## External calibration and loading

| Item | V0.2 value |
|---|---:|
| Digital input calibration | 1.0 V peak per digital full-scale |
| Source resistance | 10 kOhm |
| Digital output calibration | 1.0 digital full-scale per volt |
| Following equipment input load | 1 MOhm |

These are configurable through `CircuitCalibration`. Calibration must be set while audio processing is stopped.

## Input network

The input is modelled as:

1. calibrated source voltage;
2. 10 kOhm source resistance;
3. 1 nF capacitor from the RF node to AC ground;
4. 10 nF coupling capacitor from the RF node to the op-amp input;
5. 1 MOhm bias resistance in parallel with a nominal 2 MOhm op-amp input resistance to AC ground.

Both capacitors are solved together using backward-Euler companion conductances. The source resistance is external to the pedal and is configurable because a real pickup is not a fixed resistor.

## Gain and op-amp stage

| Component/model | V0.2 value |
|---|---:|
| Feedback resistor | 1 MOhm |
| Minimum gain-branch resistance | 4.7 kOhm |
| Reverse-log gain potentiometer | 500 kOhm |
| Gain-branch capacitor | 47 nF |
| Nominal op-amp gain-bandwidth | 1 MHz |
| Nominal slew rate | 0.5 V/us |
| AC output swing | +/-3.2 V |
| Soft rail knee | 0.2 V |

The gain capacitor state and variable resistor are solved as connected components. The compact op-amp model then applies closed-loop bandwidth, slew-rate and rail constraints. It is intended to be fitted against an LM741 SPICE macromodel and measured hardware; it is not a transistor-level model.

For V0.2 the gain-pot resistance fraction is `(1 - control)^2.2`. This curve is pinned for repeatability but remains provisional until a target potentiometer is measured.

## Coupling, clipping and output network

The connected topology is:

```text
op-amp source -- 1 uF -- 10 kOhm -- clipping node
                                         |
                                         +-- anti-parallel germanium diodes -- AC ground
                                         +-- 1 nF capacitor ----------------- AC ground
                                         +-- complete 50 kOhm output pot ---- AC ground
```

The output-pot wiper drives the configurable external load. Loading reflected through both sections of the potentiometer is included in the clipping-node equation.

| Component/model | V0.2 value |
|---|---:|
| Coupling capacitor | 1 uF |
| Clipping resistor | 10 kOhm |
| Shunt capacitor | 1 nF |
| Output potentiometer | 50 kOhm, nominal 10%-at-midpoint audio taper |
| Diode pair | matched anti-parallel pair |
| Diode saturation current | 1 uA |
| Diode ideality factor | 1.6 |
| Thermal voltage | 25.85 mV |

The 1 uF capacitor, 10 kOhm resistor, diode pair, 1 nF capacitor and output-pot load are solved as one coupled network.

## Known validation limits

- Diode parameters are provisional until actual devices are measured across current and temperature.
- The compact op-amp model requires comparison with a LM741 SPICE and a physical pedal.
- A resistive guitar-source approximation does not reproduce pickup inductance, cable capacitance or guitar-volume position.
- Component tolerances, noise, bias error and unequal diode behaviour are not enabled in V0.2.
