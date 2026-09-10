# CircuitPedal offline validation tool

`circuitpedal_validate` is the executable layer of the CircuitPedal
SPICE/physical-reference validation plan. Its purpose is to turn circuit-model
accuracy into a measurable engineering problem rather than a listening-only
judgement.

## Scope

V0.18 introduced deterministic electrical rendering and waveform comparison.
V0.19 adds arbitrary internal-node capture, named CSV-column comparison, DC
operating-point reporting, machine-readable DC reference gates and validation-
only overrides for named fixed voltage sources.

V0.20 adds silent render pre-roll, automatic linear interpolation across
different sample rates, steady-state comparison windows, harmonic phase and THD
metrics, stronger acceptance thresholds, sanitizer CI and the first reproducible
independent ngspice reference candidate.

The electrical validator works before the real-time host FIR/downsampling path.
That separation is intentional: circuit-equation error can be measured without
mixing it with host resampling error.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Then use:

```bash
./build/circuitpedal_validate --help
```

## Reproducible circuit state

The render, `dc` and `dc-check` commands can set a circuit state before the DC
operating-point solve:

- `--control NAME=0..1` sets a named potentiometer; repeatable.
- `--switch NAME=POSITION` sets a named switch; repeatable.
- `--source NAME=VOLTS` temporarily changes a named fixed `V` source; repeatable.
- `--sample-rate HZ` selects the electrical solver rate from 8 kHz to 384 kHz.

Linked potentiometer sections and linked switch poles move together as they do in
the GUI/live circuit.

`--source` is deliberately validation-only. The validator rewrites the named
fixed `V` directive in memory before parsing. It does not edit the `.cpedal`
file, does not alter the bundled circuit library and does not affect live audio.
This lets a measurement or SPICE run be reproduced at its actual supply voltage
without making that reference condition the normal pedal setting.

For example, the Woolly working-board data was measured at 9.33 V while its
normal `.cpedal` file uses 9.0 V:

```bash
--source VCCSRC=9.33
```

## Render a circuit

Example: render the Woolly Mammoth for one second at 192 kHz:

```bash
./build/circuitpedal_validate render \
  circuits/woolly_mammoth_reference_draft.cpedal \
  build/woolly_110hz.csv \
  --sample-rate 192000 \
  --seconds 1 \
  --signal sine \
  --frequency 110 \
  --amplitude 0.25 \
  --warmup 0.05
```

Available deterministic stimuli are `sine`, `step`, `impulse`, `dualtone` and
`logsweep`.

The transient renderer streams rows directly to disk rather than retaining the
whole run in memory, making high-rate and multi-node captures practical.

`--warmup` processes silence before sample zero without writing those samples.
This lets capacitor and dynamic-device state settle while keeping the captured
stimulus phase and CSV time anchored at zero.

## Internal-node capture

Use repeatable `--node` options to capture named `.cpedal` nodes alongside the
declared output:

```bash
./build/circuitpedal_validate render \
  circuits/woolly_mammoth_reference_draft.cpedal \
  build/woolly_nodes.csv \
  --source VCCSRC=9.33 \
  --sample-rate 192000 \
  --seconds 1 \
  --signal sine \
  --frequency 82 \
  --amplitude 0.25 \
  --node B1 \
  --node C1_NODE \
  --node E2 \
  --node C2_NODE \
  --node TONE_SRC \
  --node OUT
```

Node names are matched case-insensitively. Each requested node becomes a
sanitized CSV column such as `node_B1_v` or `node_C1_NODE_v`.

## Render CSV

Without extra node probes the columns are:

```text
sample,time_s,input_fs,output_v,output_fs,converged
```

Requested nodes are appended as `node_<NODE>_v` columns.

- `sample` is the zero-based electrical-solver sample index.
- `time_s` is time in seconds.
- `input_fs` is the normalized validation stimulus.
- `output_v` is raw voltage at the circuit's declared output node.
- `output_fs` is calibrated/clamped CircuitPedal full-scale output.
- `converged` is 1 when the nonlinear solve converged.
- `node_*_v` values are raw internal-node voltages.

A render exits nonzero if any transient nonlinear solve fails.

## DC operating point

The `dc` command reports the solved operating point before transient processing:

```bash
./build/circuitpedal_validate dc \
  circuits/woolly_mammoth_reference_draft.cpedal \
  --source VCCSRC=9.33 \
  --control WOOL=1 \
  --control PINCH=1 \
  --node B1 \
  --node C1_NODE \
  --node E2 \
  --node C2_NODE
```

With no `--node` arguments, every named circuit node is printed.

## Machine-readable DC reference checks

`dc-check` compares the solved operating point with a CSV table. A reference
table requires `node` and `expected_v`, plus at least one positive tolerance via
`relative_tolerance_percent` or `absolute_tolerance_v`.

Example:

```bash
./build/circuitpedal_validate dc-check \
  circuits/woolly_mammoth_reference_draft.cpedal \
  validation/woolly_mammoth/working_board_dc.csv \
  --source VCCSRC=9.33 \
  --control WOOL=1 \
  --control PINCH=1 \
  --control EQ=1 \
  --control OUTPUT=1
```

For each point the checker reports expected voltage, actual voltage, signed
error, allowed error and pass/fail. It exits nonzero if any point fails.

The first committed external-reference table is under
`validation/woolly_mammoth/`. Its present 35% tolerance preserves the old broad
sanity guard; it is not a component-accuracy claim. V0.19 now removes the supply
mismatch by evaluating the circuit at the source measurement's actual 9.33 V.

## Waveform comparison

The default CSV loader expects `time_s` or `time` and then searches for one of
these value columns, in order: `output_v`, `output_volts`, `output`, `value`,
`v(out)`.

A normalized simulator export can therefore be as simple as:

```text
time_s,output_v
0.000000000,0.0123
0.000005208,0.0129
...
```

Run a comparison with:

```bash
./build/circuitpedal_validate compare \
  reference/spice_woolly_110hz.csv \
  build/woolly_110hz.csv \
  --max-lag 32 \
  --fundamental 110 \
  --harmonics 8
```

For an internal node, select a column explicitly:

```bash
./build/circuitpedal_validate compare \
  reference/woolly_spice_nodes.csv \
  build/woolly_nodes.csv \
  --column node_C2_NODE_v \
  --max-lag 32
```

When simulator and CircuitPedal column names differ, use
`--reference-column` and `--actual-column` separately. Named column matching is
case-insensitive.

The comparator automatically linearly interpolates the actual waveform onto the
reference sample grid when effective rates differ. Use `--no-resample` when an
exact rate match is itself an acceptance requirement. Select matching steady-
state regions with `--start SECONDS` and `--duration SECONDS`.

The report includes best integer-sample alignment, correlation, reference and
actual RMS, RMS error, normalized RMS error, peak absolute error, DC error, RMS
gain error, per-harmonic amplitude/phase error and aggregate THD error. Select a
steady-state window containing an integer number of fundamental cycles for
meaningful harmonic results.

## CI thresholds

Transient comparisons can fail CI with `--max-nrms`, `--max-peak`,
`--min-correlation`, `--max-gain-db` and `--max-thd-db`. DC reference tables
carry their tolerances per node and can be used directly as `dc-check` gates.

The Woolly working-board check is the first such external-reference gate and its
actual node-by-node report is printed as a dedicated CI step.

## Current limitations

The validator is evidence infrastructure; it does not by itself prove that a
commercial pedal is component-accurate.

- Alignment is integer-sample only.
- Linear interpolation handles ordinary simulator and measurement exports but
  is not a band-limited sample-rate converter.
- Harmonic analysis checks requested harmonic amplitude/phase and THD but is
  not yet a full broadband spectral-error analysis.
- Fixed-source override matching uses the `.cpedal` `V` directive ID and is
  intentionally confined to offline validation.
- Compact transistor, JFET, diode and op-amp aliases still require comparison
  with trusted simulator models and physical measurements.

The active Woolly campaign is therefore: exact-supply DC comparison, investigate
collector-node error, established 2N3904 SPICE comparison, transient multi-node
comparison at several control settings, spectral/intermodulation comparison and
finally physical reference-pedal measurements where available.
