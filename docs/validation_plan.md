# CircuitPedal circuit validation plan

The acceptance target is measured agreement with the defined circuit, not merely a similar sound.

## Validation infrastructure

V0.18 introduced the first executable layer of this plan through
`circuitpedal_validate` and `docs/validation_tool.md`.

The V0.19 validation stage extends it with:

- arbitrary internal-node capture during deterministic transient renders;
- explicit named-column comparison for CircuitPedal and simulator exports;
- DC operating-point inspection;
- machine-readable per-node DC reference tables and pass/fail tolerances;
- the first committed external-reference dataset for the Woolly Mammoth;
- visible Woolly DC reference reporting in CI.

The current implementation can therefore:

- generate deterministic sine, step, impulse, dual-tone and logarithmic-sweep stimuli;
- render a `.cpedal` model directly at an electrical solver rate up to 384 kHz;
- apply named potentiometer and switch states before the DC operating-point solve;
- export the raw declared output-node voltage, calibrated CircuitPedal output and requested internal-node voltages to CSV;
- load normalized CircuitPedal/SPICE waveform CSV files and select arbitrary value columns;
- search a bounded integer-sample lag for alignment;
- report correlation, RMS error, normalized RMS error, peak error, DC error and gain error;
- report per-harmonic amplitude error for a requested fundamental;
- fail with threshold-based exit status for transient golden-reference CI gates;
- fail a DC reference check when any node leaves its documented tolerance.

The electrical renderer intentionally runs the generic circuit directly instead
of comparing after the host 4x FIR/downsampling wrapper. This separates circuit-
equation error from real-time resampling-path error. End-to-end host-path
validation remains a separate future layer.

No existing pedal is promoted from **Reference Draft** by the existence of this
tooling alone. A promotion still requires trusted external reference data and
passing tolerances.

## Reference models

1. Create an ngspice/LTspice netlist matching the circuit being validated.
2. Record simulator version, device-model version/source, supply voltage, source impedance and load with every reference dataset.
3. Use documented manufacturer or otherwise established semiconductor models before fitting CircuitPedal device aliases to a specific physical pedal.
4. Measure actual I-V/device behaviour where unusual or highly variable components materially affect the result.
5. Build or nominate a physical reference pedal and record component values and measurement conditions when a physical-accuracy claim is desired.

## Automated comparisons

Export high-rate SPICE waveforms at meaningful internal nodes and the output. Compare them with offline CircuitPedal renders using:

- small-signal frequency and phase sweeps;
- stepped input amplitudes and frequencies;
- positive and negative impulses and steps;
- gain/tone/output parameter sweeps;
- harmonic and intermodulation spectra;
- re-amped DI guitar tracks.

Every comparison must report time alignment, amplitude calibration, RMS error,
peak error and harmonic-amplitude error. Store tolerances beside each trusted
golden dataset rather than relying on visual inspection.

V0.19 covers arbitrary-node capture and named-column comparison. The main
remaining framework work before broad automated SPICE acceptance is
reference-data resampling/interpolation, richer spectral metrics and committed
reference metadata describing simulator/model versions and circuit state.

## Physical measurements

Use calibrated input/output levels and a defined load. Capture the same internal
nodes used by SPICE with a probe whose loading is included in the reference.
Repeat at multiple supply voltages and temperatures where semiconductor behaviour
changes materially.

## Real-time acceptance

- No heap allocation, locks or console I/O in the audio callback.
- No non-finite output for any finite or non-finite input test.
- All nonlinear solves meet the KCL residual limit or fail safely without committing state.
- Callback execution remains below 50% of the device deadline at the supported minimum buffer size.
- Physical loopback round-trip latency is measured for each supported interface configuration.

## Generic-circuit / Woolly Mammoth reference

The Woolly Mammoth is the first named external-reference campaign.

### Working-board source

The current physical sanity source is the verified Guitar FX Layouts / Tagboard
Effects Woolly Mammoth build page:

https://tagboardeffects.blogspot.com/2011/02/zvex-woolly-mammoth-vero.html

It reports, with all controls at maximum and a 9.33 V supply:

- Q1 emitter: 0 V
- Q1 base: 0.58 V
- Q1 collector / Q2 base: 1.2 V
- Q2 emitter: 0.88 V
- Q2 collector: 2.3 V

The source suggests working builds should be around 10% from those readings.

### Current automated sanity gate

The machine-readable data and provenance notes live under
`validation/woolly_mammoth/`.

The current `.cpedal` reference file uses a fixed 9.0 V supply. To maintain
continuity with the existing V0.8 sanity test, the machine-readable gate carries
9.0/9.33-scaled comparison values while preserving the original 9.33 V measured
readings in the same dataset.

This simple linear scaling is **not** claimed to model the real supply-voltage
response of the transistor circuit. The present 35% tolerance is deliberately
broad because the CircuitPedal 2N3904 remains a compact Ebers-Moll approximation,
the supply does not yet match the measurement, and the published readings come
from one physical build.

CI now runs the generic `dc-check` path against that table, so regression output
shows the expected voltage, actual voltage, signed error and allowed error for
each Woolly transistor node.

### Fidelity target

For a stronger Woolly Mammoth claim:

1. Run the circuit at the same 9.33 V reference supply and all-controls-maxed state used by the published measurements.
2. Bring the DC node voltages to approximately ±10% of the working-board reference without tuning the model to an obviously unphysical device.
3. Cross-check the result against at least one established 2N3904 SPICE model.
4. Compare transient waveforms at `B1`, `C1_NODE`, `E2`, `C2_NODE`, tone-network nodes and `OUT` at multiple control settings using V0.19 internal-node capture.
5. Compare spectra and intermodulation behaviour at representative input levels.
6. Prefer measurements from a nominated physical reference build over continued fitting to a single web-published voltage set.

The generic nonlinear circuit must continue to run at 4x the host sample rate
through FIR interpolation/decimation and keep bypass delay-aligned with the wet
path. That real-time path remains independently tested from the raw electrical
comparison renderer.
