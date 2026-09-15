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
- NTE102/NTE103 complementary germanium transistor datasheet:
  https://www.mouser.com/datasheet/2/300/nte102-179916.pdf
- onsemi 1N914 switching-diode datasheet:
  https://www.onsemi.com/pdf/datasheet/1n914-d.pdf

### Reasonable engineering inferences

- CircuitPedal maps normalized pot position 0 to terminal 1 and position 1 to
  terminal 3. The previous Animato draft had the BOOST signal and AC-ground outer
  terminals reversed, so increasing the exposed control moved the wiper toward
  AC ground. This was a real implementation fault and is now corrected.
- The dual audio-taper DISTORTION topology predicts a steep low-end progression
  because two attenuating gangs move together. CircuitPedal's 10%-at-midpoint
  power law is plausible, but the trace does not establish the original pot's
  exact resistance-versus-rotation curve.
- Bass and low guitar registers should show more attenuation than a full-frequency
  bass distortion because of the Rangemaster input network. This is expected
  circuit behaviour, not automatically a modelling defect.

### Unknown / unverified

- Original-unit NTE102/NTE103 leakage and hFE at operating temperature.
- Exact 2SC2240 gain rank/device spread in a reference Human Gear unit.
- Factory-set BOOST trimmer position and unit-to-unit trimmer variation.
- Exact manufacturer, taper curve and gang tolerance of the original dual 100kA
  DISTORTION potentiometer.
- Production revisions or component-tolerance distributions across Human Gear
  units.
- Lab-grade original-unit frequency-response, transfer-curve and spectrum data.

## Confirmed production changes

Only one model-DSP change was justified by the review:

```text
Before: POT BOOST PAIR_C BOOST_W VA 10k LIN 0.65
After:  POT BOOST VA BOOST_W PAIR_C 10k LIN 0.65
```

This reverses the trimmer's outer terminals while preserving the traced 10k
value, linear taper and default position. No transistor model, clipping diode,
filter, tone-stack, Distortion taper, output calibration or shared DSP component
was changed by the original pass.

The independent review also found a shared live-routing defect. The macOS live
path deliberately runs generic circuits at 1x, with zero FIR latency, but it
still delayed dry/bypass audio and reported latency using the experimental 4x
wrapper's fixed 47-sample delay. Runtime latency now follows the compiled mode:
0 samples at 1x and 47 at 4x. This aligns dry and wet during the bypass crossfade
without changing any pedal's active DSP.

## Automated results

The acceptance runner uses controlled 1x renders and writes the complete report
and CSV data into `build/validation/animato-acceptance`. Steady sine cases now
run for four seconds and analyze the final coherent second. The earlier 180 ms
captures were not long enough for the final 10 uF coupling capacitor and 100k
load to settle after signal onset; their RMS, asymmetry and especially low-level
THD figures are superseded by this campaign.

### BOOST orientation

At 440 Hz with matched settings:

- BOOST 0.0: `BOOST_W` AC RMS = 0.000000025 V
- BOOST 1.0: `BOOST_W` AC RMS = 0.231398661 V

The electrical direction now matches the user-facing direction.

### Output-domain calibration and headroom

With a 50 mV-peak / 35.355 mV-RMS analogue input at medium Distortion:

- analogue `OUT` RMS = 0.410884 V
- host output RMS = 0.205442 FS
- measured conversion = exactly 0.500000 FS/V
- analogue RMS gain = 11.62x / +21.31 dB

The existing `OUTPUT OUT 0.5` conversion is internally consistent and retains
useful host headroom. It was not changed. A larger multiplier copied from another
pedal would create digital hard clipping instead of correcting an Animato fault.

A deliberately strong 150 mV-peak analogue 110 Hz sine at high drive and 75%
Volume reached 0.561539 FS peak and 0.314890 FS AC RMS with 0% host clipping and
zero solver failures. With BOOST, DISTORTION and VOLUME all at maximum, the same
level at 55 Hz reaches full scale and clips 18.49% of samples. This documents the
maximum-output boundary; it does not justify changing all output levels without
an original-unit voltage measurement.

### DISTORTION control law

The traced dual 100kA control explains the physically observed sensitivity:

| Rotation | Electrical fraction per gang | Output RMS | Relative to maximum |
| ---: | ---: | ---: | ---: |
| 10% | 0.000477 | 0.000078726 FS | -70.72 dB |
| 25% | 0.010000 | 0.031927636 FS | -18.56 dB |
| 50% | 0.100000 | 0.205441846 FS | -2.39 dB |
| 75% | 0.384559 | 0.254774826 FS | -0.52 dB |
| 100% | 1.000000 | 0.270601416 FS | 0.00 dB |

This is a steep control, and the topology/value/taper designation match the
trace. The exact physical A-taper curve remains unverified, so no arbitrary
linearization or alternate law was added.

### Low-level frequency response

Using a 0.5 mV-peak analogue sine, BOOST at maximum, center Tone and medium
Distortion, the measured model output fundamental gain was:

| Frequency | Output gain |
| ---: | ---: |
| 40 Hz | -9.13 dB |
| 55 Hz | -1.31 dB |
| 82 Hz | +8.08 dB |
| 110 Hz | +14.42 dB |
| 196 Hz | +24.44 dB |
| 440 Hz | +30.56 dB |
| 1 kHz | +29.02 dB |
| 2 kHz | +20.99 dB |
| 5 kHz | +2.26 dB |
| 10 kHz | -13.61 dB |

The Rangemaster stage itself rises strongly from the bass into the upper mids,
while the following Muff stages/tone network roll off the top end. This is
consistent with the documented complaint that the real topology removes bass;
it is not evidence for adding an internal clean blend.

### Tone, clipping and Bias

The passive Tone control moves in the expected direction: at 5 kHz the measured
low-level gain changes from -8.44 dB at Tone 0 to +10.03 dB at Tone 1, while at
110 Hz it moves from +18.55 dB to +10.86 dB. Quarter-step measurements progress
smoothly; 440 Hz, near the two branches' crossover, has a shallow mid-sweep
minimum rather than a simple shelf response.

At 50 mV-peak input, medium/high Distortion produces strong predominantly odd
harmonic content. Medium drive measured 27.15% THD with H2 at -24.13 dB and
H3 at -12.85 dB relative to the fundamental; high drive measured 32.56% THD
with H2 at -25.85 dB and H3 at -11.34 dB. Peak asymmetry remains small, consistent
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
| 44.1 kHz | 0.364171 | 0.240798 | 30.702% | 0 |
| 48 kHz | 0.364258 | 0.240777 | 30.698% | 0 |
| 96 kHz | 0.364756 | 0.240662 | 30.672% | 0 |

## Component-model and oversampling audit

The NTE102/NTE103 compact model uses beta 85 and an Ebers-Moll-style transfer,
which is close to the NTE datasheet's typical DC gain of 80, but it omits the
datasheet's temperature-dependent germanium leakage and junction capacitance.
The original unit's selected devices, leakage and temperature are unknown, so a
shared or invented "vintage" correction is not defensible. The 2SC2240 model's
beta 420 is within the published broad gain range, while Early effect and device
capacitance remain simplified. The 1N914 model omits its few-picofarad junction
capacitance/recovery; that is small beside the explicit 470 pF and 100 nF feedback
components. None of these approximations was proved wrong enough to change.

A high-drive 5 kHz / 140 mV-peak probe found measurable 1x foldback products:
energy outside the legitimate in-band harmonics was -11.3 dB relative to total,
versus -35.3 dB through the current 4x wrapper. A four-tone probe showed a more
modest 2.9 dB reduction in the 16-23.5 kHz band. This does not override the
recorded physical test in which the current 4x path sounded materially wrong.
That path also converts/clamps each circuit sub-sample before FIR decimation,
which the existing processing decision identifies as the next architecture issue
to investigate. Animato therefore remains on the physically accepted 1x path.

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
