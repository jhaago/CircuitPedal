# Woolly Mammoth 1x acceptance baseline — 2026-09-11

Branch: `validation/woolly-mammoth`
Production path under test: **48 kHz / 1x**
Deferred path: 4x oversampling

This baseline records the first repeatable acceptance run after CircuitPedal standardised the stable live path on 1x processing.

## Published working-board DC comparison

Reference condition: 9.33 V supply, all controls at maximum.

| Node | Published board (V) | CircuitPedal (V) | Error | Published ~±10% target | Historical 35% sanity gate |
| --- | ---: | ---: | ---: | :---: | :---: |
| B1 | 0.5800 | 0.6165 | 6.29% | PASS | PASS |
| C1_NODE | 1.2000 | 1.4771 | 23.09% | FAIL | PASS |
| E2 | 0.8800 | 0.8371 | 4.88% | PASS | PASS |
| C2_NODE | 2.3000 | 1.7312 | 24.73% | FAIL | PASS |

Result: **the old 35% operating-region sanity gate passes; the published approximately ±10% working-board target is not met.**

The external working-board source is the verified Guitar FX Layouts / Tagboard Effects Woolly Mammoth page already documented in this validation directory. These voltages are useful evidence but are not treated as a golden component-calibration target.

## 48 kHz / 1x live-path solver sweep

Input: 110 Hz sine at 0.25 FS after 0.05 s warm-up.

| State | Peak (FS) | RMS (FS) | Clipped samples | Convergence failures |
| --- | ---: | ---: | ---: | ---: |
| nominal | 0.820171 | 0.428126 | 0.000% | 0 |
| all maximum | 1.000000 | 0.302106 | 0.250% | 0 |
| restrained | 0.990819 | 0.368667 | 0.000% | 0 |
| WOOL high / PINCH low | 0.828958 | 0.568972 | 0.000% | 0 |
| WOOL low / PINCH high | 0.934780 | 0.269857 | 0.000% | 0 |

Result: **PASS — zero nonlinear convergence failures across all five automated control states.**

The small amount of full-scale clipping in the all-maximum test is recorded as signal behaviour, not a numerical solver failure.

## Reference-bias sanity check

Using the published voltage set with the known 51k Q1 collector feed, 20k Q2 collector resistor and 2.2k Q2 emitter resistor gives an unusual inferred Q2 DC current gain.

| PINCH end-state assumption | Implied Q1 β | Implied Q2 β |
| --- | ---: | ---: |
| 100k fixed + 500k rheostat | 220.8 | 7.2 |
| 100k fixed + approximately 0 ohm rheostat | 36.0 | 6.8 |

The Q2 result remains around 7 either way. This makes the published voltage set unsuitable as the sole target for transistor parameter fitting, even though it remains valuable as a broad working-region check.

## Status after this baseline

- **48 kHz / 1x numerical live-path safety:** accepted for continued physical testing.
- **Published working-board DC sanity:** passes the broad historical gate.
- **Component-level fidelity:** not yet proven.
- **Model label:** remains `Reference Draft`.

Do not alter Woolly component values merely to force these four external voltage points into ±10%. Stronger physical-reference and device-model provenance should come first.
