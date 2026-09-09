# CircuitPedal circuit validation plan

The acceptance target is measured agreement with the defined circuit, not merely a similar sound.

## Reference models

1. Create an ngspice/LTspice netlist matching `reference_circuit.md`.
2. Use a documented LM741 macromodel and record its version and source.
3. Measure the actual I-V curves of the selected germanium diodes and fit the digital/SPICE parameters.
4. Build or nominate one physical reference pedal and record component values, supply voltage, source impedance and load.

## Automated comparisons

Export high-rate SPICE waveforms at the op-amp output, clipping node and output wiper. Compare them with offline CircuitPedal renders using:

- small-signal frequency and phase sweeps;
- stepped input amplitudes and frequencies;
- positive and negative impulses and steps;
- gain/output parameter sweeps;
- harmonic and intermodulation spectra;
- re-amped DI guitar tracks.

Every comparison must report time alignment, amplitude calibration, RMS error, peak error and harmonic-amplitude error. Store tolerances beside each golden dataset test rather than relying on visual inspection.

## Physical measurements

Use calibrated input/output levels and a defined load. Capture the same internal nodes used by SPICE with a probe whose loading is included in the reference. Repeat at multiple supply voltages and temperatures where the diode behaviour changes materially.

## Real-time acceptance

- No heap allocation, locks or console I/O in the audio callback.
- No non-finite output for any finite or non-finite input test.
- All nonlinear solves meet the KCL residual limit or fail safely without committing state.
- Callback execution remains below 50% of the device deadline at the supported minimum buffer size.
- Physical loopback round-trip latency is measured for each supported interface configuration.


## Generic-circuit / Woolly Mammoth reference

The V0.8 Woolly Mammoth reference file is now covered by two levels of validation.

### Automated sanity gate

CI loads `circuits/woolly_mammoth_reference_draft.cpedal`, sets all four
potentiometers to maximum, compiles the generic circuit and confirms stable
transient operation. A broad DC sanity gate compares the compact BJT solution
with published readings from a verified working board.

The published reference uses a 9.33 V supply with all controls maxed:

- Q1 emitter: 0 V
- Q1 base: 0.58 V
- Q1 collector / Q2 base: 1.2 V
- Q2 emitter: 0.88 V
- Q2 collector: 2.3 V

The V0.8 automated gate is deliberately wider than the final target because the
current 2N3904 is a compact Ebers-Moll approximation and the published readings
come from one physical build. This sanity gate must not be described as
component-accurate validation.

### Fidelity target

For a stronger Woolly Mammoth claim:

1. Run the circuit at the same 9.33 V reference supply and all-controls-maxed
   state used by the published measurements.
2. Bring the DC node voltages to approximately ±10% of the working-board
   reference without tuning the model to an obviously unphysical device.
3. Cross-check the result against at least one established 2N3904 SPICE model.
4. Compare audio waveforms and spectra at multiple control settings.
5. Prefer measurements from a nominated physical reference build over continued
   fitting to web-published voltages.

V0.8 also requires the generic nonlinear circuit to run at 4x the host sample
rate through FIR interpolation/decimation and to keep bypass delay-aligned with
the wet path.
