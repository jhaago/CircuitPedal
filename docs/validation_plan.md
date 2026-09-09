# V0.2 circuit validation plan

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
