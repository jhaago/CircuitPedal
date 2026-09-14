# Human Gear Animato validation

This directory contains the focused engineering campaign for the source-only
Human Gear Animato model. It evaluates the component-level `GenericCircuit`
solver at 1x rather than the deferred experimental 4x live path.

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

- With CircuitPedal's potentiometer convention, normalized position 0 places the
  wiper at terminal 1 and position 1 places it at terminal 3. The current draft's
  BOOST terminal 3 is the ideal 9 V rail (AC ground), so the exposed control is
  electrically reversed: increasing its normalized value reduces booster signal.
- The dual audio-taper DISTORTION control can legitimately have a steep perceptual
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

## Running the campaign

Build the validation CLI, then run:

```sh
python3 validation/animato/run_acceptance.py \
  build/circuitpedal_validate \
  build/validation/animato-acceptance
```

The runner creates an `acceptance_report.md` and CSVs for each controlled render.
Hard gates cover BOOST direction, finite/non-silent output, solver convergence and
44.1/48/96 kHz numerical robustness. Gain, clipping and harmonic metrics are
reported for engineering interpretation rather than tuned to arbitrary targets.

## Physical listening still required

Passing the automated campaign does not automatically promote the Animato into
the bundled stable library. Physical Mac testing should compare low/mid/high
DISTORTION, both BIAS states, BOOST direction/range, Tone extremes and Volume
headroom with both guitar- and bass-range material before the source-only bundle
exclusion is removed.
