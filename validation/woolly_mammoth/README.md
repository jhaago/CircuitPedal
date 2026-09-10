# Woolly Mammoth reference campaign

This directory starts the first external-reference validation campaign for the
CircuitPedal generic circuit engine.

## Working-board DC source

The initial physical reference is the verified Guitar FX Layouts / Tagboard
Effects Woolly Mammoth build page:

https://tagboardeffects.blogspot.com/2011/02/zvex-woolly-mammoth-vero.html

The page reports a working board with all pots at maximum and a 9.33 V supply:

- Q1 emitter: 0 V
- Q1 base: 0.58 V
- Q1 collector / Q2 base: 1.2 V
- Q2 emitter: 0.88 V
- Q2 collector: 2.3 V

The author suggests working builds should be around 10% from those readings.

## Current machine-readable gate

`working_board_dc.csv` retains the published voltages directly. V0.19 adds the
validation-only `--source NAME=VOLTS` option, so the normal 9.0 V Woolly circuit
file can be evaluated at the source measurement's actual 9.33 V condition
without changing the live model or duplicating its topology.

The current CI tolerance remains 35%, matching the intentionally broad legacy
sanity gate. This checks that the compact Ebers-Moll model stays in the same
working region; it is **not** the final fidelity acceptance limit.

Run the current gate with:

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

`--source` is validation-only. It rewrites the named fixed `V` source in memory
before the circuit file is parsed; it does not edit the `.cpedal` file and it
does not affect normal GUI/live-audio operation.

## V0.19 node-trace campaign

The V0.19 validator can export arbitrary circuit nodes alongside the declared
output node. A first transient capture can therefore include the transistor
bias/clipping nodes directly:

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

Each requested node is written as a column such as `node_B1_v` or
`node_C2_NODE_v`. The comparator can then target that column explicitly with
`--column`, or use different source/destination names with `--reference-column`
and `--actual-column`.

## Promotion criteria

The Woolly Mammoth must remain a **Reference Draft** until stronger evidence is
available. V0.19 removes the supply-voltage mismatch from the existing DC
comparison, but the next confidence steps are still substantial:

1. quantify the exact 9.33 V DC errors and investigate the collector-node mismatch;
2. achieve approximately the published ±10% working-board DC range without
   obviously unphysical parameter fitting;
3. cross-check the two 2N3904 devices against an established SPICE model;
4. compare transient node waveforms and spectra at several control settings;
5. preferably repeat the measurements on a nominated physical reference pedal.

This directory is intended to accumulate those reference datasets and metadata
rather than hiding fidelity assumptions inside the circuit implementation.

## Independent ngspice reference candidate

`woolly_mammoth_ngspice.cir` expresses the same topology independently in
ngspice and uses a fuller Gummel-Poon Q2N3904 model. Run it with:

```bash
validation/woolly_mammoth/run_ngspice.sh build/validation/woolly
```

The script produces a normalized CSV containing the output plus B1, C1_NODE,
E2 and C2_NODE. Render the matching CircuitPedal state and compare a steady-
state window with:

```bash
./build/circuitpedal_validate render \
  circuits/woolly_mammoth_reference_draft.cpedal \
  build/validation/woolly/circuitpedal.csv \
  --sample-rate 192000 --seconds 1 \
  --signal sine --frequency 110 --amplitude 0.25 \
  --control WOOL=0.75 --control PINCH=0.35 \
  --control EQ=0.50 --control OUTPUT=0.70 \
  --node B1 --node C1_NODE --node E2 --node C2_NODE

./build/circuitpedal_validate compare \
  build/validation/woolly/woolly_mammoth_ngspice.csv \
  build/validation/woolly/circuitpedal.csv \
  --start 0.25 --duration 0.50 --max-lag 64 \
  --fundamental 110 --harmonics 8
```

This remains a **candidate**, not golden data. The included Q2N3904 parameter
set is widely distributed, but its original manufacturer/library provenance is
not established here. Before permanent fidelity thresholds are introduced,
replace or corroborate it with a versioned manufacturer model and record its
source URL, retrieval date, licence and SHA-256 checksum, or nominate and
measure a physical transistor lot.

### Multi-state transient campaign

`run_candidate_comparison.sh` renders both simulators at 192 kHz for the
nominal, all-maximum and restrained control states. It reports output plus four
transistor-node comparisons over the steady 0.25-0.75 s window and retains the
effective ngspice netlist, raw/normalized traces and metric report for each
case:

```bash
validation/woolly_mammoth/run_candidate_comparison.sh \
  build/circuitpedal_validate \
  build/validation/woolly-campaign
```

No fidelity threshold is attached to this exploratory campaign yet. CI proves
the two independent render paths execute and makes their disagreement visible;
promotion thresholds will follow only after the candidate transistor model is
given reliable provenance or replaced with physical reference measurements.
