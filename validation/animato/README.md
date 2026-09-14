# Human Gear Animato validation

This directory contains the focused engineering campaign for CircuitPedal's
Human Gear Animato model. It evaluates the component-level `GenericCircuit`
solver at 1x, matching the accepted live macOS path, rather than treating the
older experimental 4x wrapper as the production reference.

## Circuit evidence

### Facts supported by the traced circuit / Aion Polaris documentation

- The Animato is a Big Muff-derived distortion/sustainer with a Rangemaster-style
  treble-booster stage in front.
- The booster uses a complementary NTE103/NTE102 germanium Sziklai pair so it can
  operate from a conventional negative-ground supply.
- The original silicon stages are 2SC2240 devices; Aion uses 2N5088 as a practical
  equivalent.
- DISTORTION is a dual 100k audio-taper potentiometer. Its two gangs control the
  level leaving the Rangemaster and the level leaving the first Muff boost stage.
- The two distortion stages use antiparallel 1N914 feedback diodes.
- TONE is a 100k linear passive Big-Muff-style low-pass/high-pass pan network.
- VOLUME is 100k linear.
- BIAS is a linked two-pole switch that inserts parallel bias/emitter resistors.
- BOOST is a 10k internal trimmer in the Rangemaster stage.
- The 10 nF input coupling capacitor is part of the traced circuit. Aion explicitly
  notes that many bass users find the Rangemaster input removes too much low end
  and use an external clean blend. CircuitPedal must therefore not invent a bass
  restoration path inside the Animato model.

Primary references:

- Aion FX, *Polaris Distortion/Sustainer* build documentation, schematic and BOM:
  https://aionfx.com/app/files/docs/polaris_documentation.pdf
- Effects Layouts / guitar-fx-layouts 2014 Animato layout based on Adam (Crowella)'s
  trace, including original semiconductor designations and dual-gang gain control.

### Reasonable engineering inferences

- CircuitPedal maps normalized pot position 0 to terminal 1 and position 1 to
  terminal 3. The previous Animato draft had the BOOST signal and AC-ground outer
  terminals reversed, so increasing the exposed control moved the wiper toward
  AC ground. This was a real implementation fault and is now corrected.
- The dual audio-taper DISTORTION control legitimately has a steep low-end
  progression because two attenuating gangs move together. It should not be
  linearized merely to make the sweep feel easier.
- Bass and low guitar registers should show more attenuation than a full-frequency
  bass distortion because of the Rangemaster input network. This is expected
  circuit behaviour, not automatically a modelling defect.

### Unknown / unverified

- Original-unit NTE102/NTE103 leakage and hFE at operating temperature.
- Exact 2SC2240 gain rank/device spread in a reference Human Gear unit.
- Factory-set BOOST trimmer position and unit-to-unit trimmer variation.
- Production revisions or component-tolerance distributions across Human Gear
  units.
- Lab-grade original-unit frequency-response, transfer-curve and spectrum data.

## Confirmed production change

Only one model-DSP change was justified by the review:

```text
Before: POT BOOST PAIR_C BOOST_W VA 10k LIN 0.65
After:  POT BOOST VA BOOST_W PAIR_C 10k LIN 0.65
```

This reverses the trimmer's outer terminals while preserving the traced 10k
value, linear taper and default position. No transistor model, clipping diode,
filter, tone-stack, Distortion taper, output calibration or shared DSP component
was changed.

## Automated results

The acceptance runner uses controlled 1x renders and writes the complete report
and CSV data into `build/validation/animato-acceptance`.

### BOOST orientation

At 440 Hz with matched settings:

- BOOST 0.0: `BOOST_W` AC RMS = 0.000000025 V
- BOOST 1.0: `BOOST_W` AC RMS = 0.231211295 V

The electrical direction now matches the user-facing direction.

### Output-domain calibration and headroom

With a 50 mV-peak / 35.355 mV-RMS analogue input at medium Distortion:

- analogue `OUT` RMS = 0.410594 V
- host output RMS = 0.205297 FS
- measured conversion = exactly 0.500000 FS/V
- analogue RMS gain = 11.61x / +21.30 dB

The existing `OUTPUT OUT 0.5` conversion is internally consistent and retains
useful host headroom. It was not changed. A larger multiplier copied from another
pedal would create digital hard clipping instead of correcting an Animato fault.

A deliberately strong 150 mV-peak analogue sine at high drive reached only
0.590095 FS peak and 0.315304 FS RMS with 0% host full-scale clipping and zero
solver failures.

### DISTORTION control law

The traced dual 100kA control explains the physically observed sensitivity:

| Rotation | Electrical fraction per gang | Output RMS | Relative to maximum |
| ---: | ---: | ---: | ---: |
| 10% | 0.000477 | 0.000078300 FS | -70.84 dB |
| 25% | 0.010000 | 0.031755189 FS | -18.68 dB |
| 50% | 0.100000 | 0.205297029 FS | -2.47 dB |
| 75% | 0.384559 | 0.256518683 FS | -0.53 dB |
| 100% | 1.000000 | 0.272787429 FS | 0.00 dB |

This is a steep control, but the model's topology and taper match the trace. No
arbitrary linearization or gain compensation was added.

### Low-level frequency response

Using a 0.5 mV-peak analogue sine, BOOST at maximum, center Tone and medium
Distortion, the measured model output fundamental gain was:

| Frequency | Output gain |
| ---: | ---: |
| 40 Hz | -9.17 dB |
| 55 Hz | -1.44 dB |
| 82 Hz | +8.00 dB |
| 110 Hz | +14.34 dB |
| 196 Hz | +24.35 dB |
| 440 Hz | +30.53 dB |
| 1 kHz | +29.01 dB |
| 2 kHz | +20.98 dB |
| 5 kHz | +2.26 dB |
| 10 kHz | -13.61 dB |

The Rangemaster stage itself rises strongly from the bass into the upper mids,
while the following Muff stages/tone network roll off the top end. This is
consistent with the documented complaint that the real topology removes bass;
it is not evidence for adding an internal clean blend.

### Tone, clipping and Bias

The passive Tone control moves in the expected direction: at 5 kHz the measured
low-level gain changes from -8.44 dB at Tone 0 to +10.02 dB at Tone 1, while at
110 Hz it moves from +18.46 dB to +10.80 dB. Center Tone is intermediate.

At 50 mV-peak input, medium/high Distortion produces strong predominantly odd
harmonic content. Medium drive measured about 26.9% THD with H2 at -25.3 dB and
H3 at -12.8 dB relative to the fundamental; high drive measured about 31.9% THD
with H2 at -27.2 dB and H3 at -11.5 dB. Peak asymmetry remains small, consistent
with the antiparallel feedback clipping plus transistor-stage biasing.

BIAS changes level/harmonic behaviour modestly rather than radically, matching
the traced linked two-pole bias switch and Aion's description of a slight EQ/gain
shift.

### Stability and sample rates

No acceptance case produced a non-finite value or solver failure. Step, impulse,
82+659 Hz dual-tone and 196+1760 Hz dual-tone stress cases all produced 0% host
full-scale clipping.

Matched 220 Hz results were nearly invariant across supported sample rates:

| Sample rate | Peak FS | RMS FS | THD | Solver failures |
| ---: | ---: | ---: | ---: | ---: |
| 44.1 kHz | 0.394691 | 0.242824 | 30.093% | 0 |
| 48 kHz | 0.394931 | 0.242820 | 30.089% | 0 |
| 96 kHz | 0.396239 | 0.242800 | 30.064% | 0 |

## Running the campaign

Build the validation CLI, then run:

```sh
python3 validation/animato/run_acceptance.py \
  build/circuitpedal_validate \
  build/validation/animato-acceptance
```

Hard gates cover BOOST direction, finite/non-silent output, solver convergence,
host digital headroom, output-conversion consistency, transient/multi-frequency
safety and 44.1/48/96 kHz robustness. Gain, clipping and harmonic metrics are
reported for engineering interpretation rather than tuned to arbitrary targets.

## Physical listening still required

Passing the automated campaign does not automatically promote the Animato into
the bundled stable library. Physical Mac testing should compare:

- BOOST 0 / 25 / 65 / 100% for direction and useful trim range;
- DISTORTION 25 / 50 / 75 / 100%, with particular attention to whether the steep
  lower-quarter behaviour resembles the reference pedal;
- TONE 0 / 50 / 100% on chords and sustained single notes;
- both BIAS positions at matched Volume;
- VOLUME across the usable range for unity level and output headroom;
- hard guitar strums and high-output pickups for unwanted host clipping;
- bass fundamentals around E1/A1 and higher-register bass notes to judge the
  expected Rangemaster low-frequency loss rather than a bass-preservation target.

Physical A/B against a nominated Human Gear Animato, or a verified clone with a
known build provenance, remains the most valuable next fidelity step.
