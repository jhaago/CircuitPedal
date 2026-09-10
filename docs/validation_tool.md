# CircuitPedal offline validation tool

`circuitpedal_validate` is the executable layer of the CircuitPedal
SPICE/physical-reference validation plan.

Its purpose is to turn circuit-model accuracy into a measurable engineering
problem rather than a listening-only judgement.

## Scope

V0.18 introduced deterministic electrical rendering and waveform comparison.
The V0.19 validation stage adds arbitrary internal-node capture, named CSV-column
comparison, DC operating-point reporting and machine-readable DC reference gates.

A `.cpedal` file is loaded, its requested control state is applied before the DC
operating-point solve, and the circuit is then solved directly at a chosen
validation sample rate. The transient CSV always contains the raw declared
output-node voltage and may now also contain any requested internal nodes.

For SPICE comparison, use raw voltage columns (`output_v` or `node_*_v`). This
keeps electrical-model error separate from host FIR/downsampling error. A later
validation layer can separately test the complete real-time audio path.

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

## Internal node capture

Use repeatable `--node` options to capture any named `.cpedal` node without
changing the circuit's declared output:

```bash
./build/circuitpedal_validate render \
  circuits/woolly_mammoth_reference_draft.cpedal \
  build/woolly_nodes.csv \
  --sample-rate 192000 \
  --seconds 1 \
  --signal sine \
  --frequency 82 \
  --amplitude 0.25 \
  --node B1 \
  --node C1_NODE \
  --node E2 \
  --node C2_NODE \
  --node OUT
```

Node names are matched case-insensitively. Each requested node becomes a
sanitized CSV column named `node_<NODE>_v`; for example `B1` becomes
`node_B1_v` and `C1_NODE` becomes `node_C1_NODE_v`.

The render path now streams rows directly to the CSV instead of retaining the
entire render in memory. This matters when high electrical sample rates and
multi-second reference runs are used.

## Render CSV format

Without additional probes CircuitPedal writes:

```text
sample,time_s,input_fs,output_v,output_fs,converged
```

With node probes it appends columns:

```text
sample,time_s,input_fs,output_v,output_fs,converged,node_B1_v,node_C1_NODE_v,...
```

- `sample`: zero-based electrical-solver sample index
- `time_s`: time in seconds
- `input_fs`: normalized digital stimulus supplied to the `.cpedal` AUDIO source
- `output_v`: raw voltage at the circuit's declared `OUTPUT` node
- `output_fs`: calibrated/clamped CircuitPedal full-scale output
- `converged`: `1` when the nonlinear solve converged for that sample
- `node_*_v`: raw voltage at each explicitly requested internal node

A render exits nonzero if any transient nonlinear solve fails.

## Inspect a DC operating point

The `dc` command compiles the circuit and reports the solved operating point
before any transient sample is processed:

```bash
./build/circuitpedal_validate dc \
  circuits/woolly_mammoth_reference_draft.cpedal \
  --control WOOL=1 \
  --control PINCH=1 \
  --node B1 \
  --node C1_NODE \
  --node E2 \
  --node C2_NODE
```

If no `--node` option is supplied, every named node in the circuit is printed.
The same `--sample-rate`, `--control` and `--switch` options used by the render
command are supported so the circuit state is reproducible.

## Machine-readable DC reference checks

The `dc-check` command compares the solved operating point against a CSV table.
A table requires:

```text
node,expected_v,relative_tolerance_percent,absolute_tolerance_v
```

`node` and `expected_v` are required. A row must specify at least one positive
relative or absolute tolerance. The checker uses the larger permitted absolute
error when both are present, which allows near-zero nodes to use a sensible
voltage tolerance.

Example:

```bash
./build/circuitpedal_validate dc-check \
  circuits/woolly_mammoth_reference_draft.cpedal \
  validation/woolly_mammoth/working_board_dc.csv \
  --control WOOL=1 \
  --control PINCH=1 \
  --control EQ=1 \
  --control OUTPUT=1
```

Each node is reported with expected voltage, actual voltage, signed error and the
allowed error. The command exits nonzero when any reference point fails.

The first committed external reference table is documented under
`validation/woolly_mammoth/`. Its present 35% tolerance is intentionally a
sanity gate, not a component-accuracy claim. The physical source reports a 9.33 V
working board while the `.cpedal` model is currently fixed at 9.0 V; that supply
mismatch must be removed before the tighter fidelity target is claimed.

## Compare against a reference waveform

The default comparison loader accepts a CSV containing `time_s` or `time`, plus
one of these value columns in priority order:

1. `output_v`
2. `output_volts`
3. `output`
4. `value`
5. `v(out)`

A normalized SPICE export can therefore be as small as:

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

For internal nodes, select an exact value column with `--column`:

```bash
./build/circuitpedal_validate compare \
  reference/woolly_spice_nodes.csv \
  build/woolly_nodes.csv \
  --column node_C2_NODE_v \
  --max-lag 32
```

If the reference simulator uses a different header name, select the two sides
independently:

```bash
./build/circuitpedal_validate compare \
  reference/woolly_spice_nodes.csv \
  build/woolly_nodes.csv \
  --reference-column "v(c2_node)" \
  --actual-column node_C2_NODE_v \
  --max-lag 32
```

Named column matching is case-insensitive.

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

The transient comparator can act as a pass/fail gate:

```bash
./build/circuitpedal_validate compare \
  reference.csv actual.csv \
  --max-lag 16 \
  --max-nrms 5 \
  --max-peak 0.10
```

It exits nonzero when either requested threshold is exceeded. This is intended
for committed golden SPICE/measurement datasets so tolerances live beside the
reference data rather than being judged visually.

DC reference tables already carry their tolerances per node, so `dc-check` can
be placed directly in CI. The Woolly Mammoth working-board sanity table is the
first use of that mechanism.

## Important current limitations

The validation tooling is infrastructure and evidence handling; it does not by
itself prove that any commercial pedal is component-accurate.

- The transient comparator currently expects uniformly sampled reference data at
  the same nominal sample rate as the CircuitPedal render. SPICE
  interpolation/resampling support is still needed.
- Alignment is integer-sample only.
- Harmonic analysis reports amplitude agreement at requested harmonic bins; it
  is not yet a complete FFT/spectral-error report.
- Internal node capture is now available, but the generic `.cpedal` format still
  has no validation-only supply-voltage override. The first Woolly physical
  readings were taken at 9.33 V while the current reference file uses 9.0 V.
- Compact transistor, JFET, diode and op-amp device models still require fitting
  and validation against trusted SPICE models and physical measurements.

The active Woolly Mammoth campaign therefore proceeds in this order: machine-
readable DC provenance and sanity gate, same-supply DC comparison, transient
multi-node SPICE comparison at several control settings, spectral comparison,
and finally physical reference-pedal measurements where available.
