# CircuitPedal offline validation tool

`circuitpedal_validate` is the first implementation layer of the CircuitPedal
SPICE/physical-reference validation plan.

Its purpose is to turn circuit-model accuracy into a measurable engineering
problem rather than a listening-only judgement.

## Scope of V0.18

V0.18 validates the **generic electrical circuit solver** before the real-time
host oversampling FIR/downsampling path.

A `.cpedal` file is loaded, its requested control state is applied before the DC
operating-point solve, and the circuit is then solved directly at a chosen
validation sample rate. The CSV contains both the raw electrical output-node
voltage and CircuitPedal's calibrated/clamped full-scale output.

For SPICE comparison, use `output_v`. This avoids mixing electrical-model error
with host resampling/filter error. A later validation layer can separately test
the complete real-time audio path.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

The validation executable is then available as:

```bash
./build/circuitpedal_validate --help
```

## Render a circuit

Example: render the Woolly Mammoth at a 192 kHz electrical solver rate for one
second using a 110 Hz sine stimulus:

```bash
./build/circuitpedal_validate render \
  circuits/woolly_mammoth_reference_draft.cpedal \
  build/woolly_110hz.csv \
  --sample-rate 192000 \
  --seconds 1 \
  --signal sine \
  --frequency 110 \
  --amplitude 0.25
```

Available deterministic stimuli are:

- `sine`
- `step`
- `impulse`
- `dualtone`
- `logsweep`

Potentiometers can be fixed before the operating-point solve with repeatable
`--control NAME=0..1` arguments. Switches use their discrete zero-based position:

```bash
./build/circuitpedal_validate render \
  circuits/animato_reference_draft.cpedal \
  build/animato_test.csv \
  --control DISTORTION=0.75 \
  --control TONE=0.50 \
  --switch BIAS=1
```

Linked pot sections and linked switch poles follow the named control together,
matching the GUI/live-circuit behaviour.

## Render CSV format

CircuitPedal writes:

```text
sample,time_s,input_fs,output_v,output_fs,converged
```

- `sample`: zero-based electrical-solver sample index
- `time_s`: time in seconds
- `input_fs`: normalized digital stimulus supplied to the `.cpedal` AUDIO source
- `output_v`: raw voltage at the circuit's declared `OUTPUT` node
- `output_fs`: calibrated/clamped CircuitPedal full-scale output
- `converged`: `1` when the nonlinear solve converged for that sample

A render exits nonzero if any transient nonlinear solve fails.

## Compare against a reference waveform

The comparison loader accepts a CSV containing `time_s` or `time`, plus one of:

1. `output_v`
2. `output_volts`
3. `output`
4. `value`
5. `v(out)`

The names are searched in that priority order. This allows a SPICE export to be
normalized to a small two-column file such as:

```text
time_s,output_v
0.000000000,0.0123
0.000005208,0.0129
...
```

Then run:

```bash
./build/circuitpedal_validate compare \
  reference/spice_woolly_110hz.csv \
  build/woolly_110hz.csv \
  --max-lag 32 \
  --fundamental 110 \
  --harmonics 8
```

The report includes:

- best integer-sample alignment
- correlation
- reference and actual RMS amplitude
- RMS error
- normalized RMS error
- peak absolute error
- DC error
- RMS gain error in dB
- optional per-harmonic amplitude error in dB

A positive reported lag means the CircuitPedal/actual waveform is delayed
relative to the reference waveform.

## CI/golden-reference thresholds

The comparator can act as a pass/fail gate:

```bash
./build/circuitpedal_validate compare \
  reference.csv actual.csv \
  --max-lag 16 \
  --max-nrms 5 \
  --max-peak 0.10
```

It exits nonzero when either requested threshold is exceeded. This is intended
for future committed golden SPICE/measurement datasets so tolerances live beside
the reference data rather than being judged visually.

## Important current limitations

V0.18 is infrastructure, not yet proof that any commercial pedal is
component-accurate.

- The comparator currently expects uniformly sampled reference data at the same
  nominal sample rate as the CircuitPedal render. SPICE interpolation/resampling
  support is a later step.
- Alignment is integer-sample only.
- Harmonic analysis reports amplitude agreement at requested harmonic bins; it
  is not yet a complete FFT/spectral-error report.
- The renderer records the declared circuit output node. Arbitrary internal-node
  capture will be added before detailed multi-node SPICE acceptance work.
- Compact transistor, JFET, diode and op-amp device models still require fitting
  and validation against trusted SPICE models and physical measurements.

The next intended use of this framework is a Woolly Mammoth reference campaign:
DC-node agreement first, then transient/spectral comparison at several control
settings, followed by a physical reference build when suitable measurements are
available.
