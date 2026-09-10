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

`working_board_dc.csv` keeps the original measured values in
`measured_v_at_9_33` and provides an `expected_v` scaled by `9.0 / 9.33` because
the current `.cpedal` reference file has a fixed 9.0 V supply.

That linear voltage scaling is only a pragmatic continuity step for the existing
V0.8 sanity check. A transistor circuit does not generally scale linearly with
supply voltage, so these scaled values must not be described as a substitute for
a measurement or SPICE run at the same supply voltage.

The current CI tolerance remains 35%, matching the intentionally broad legacy
sanity gate. This checks that the compact Ebers-Moll model stays in the same
working region; it is **not** the final fidelity acceptance limit.

Run the current gate with:

```bash
./build/circuitpedal_validate dc-check \
  circuits/woolly_mammoth_reference_draft.cpedal \
  validation/woolly_mammoth/working_board_dc.csv \
  --control WOOL=1 \
  --control PINCH=1 \
  --control EQ=1 \
  --control OUTPUT=1
```

## V0.19 node-trace campaign

The V0.19 validator can export arbitrary circuit nodes alongside the declared
output node. A first transient capture can therefore include the transistor
bias/clipping nodes directly:

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
  --node TONE_SRC \
  --node OUT
```

Each requested node is written as a column such as `node_B1_v` or
`node_C2_NODE_v`. The comparator can then target that column explicitly with
`--column`, or use different source/destination names with `--reference-column`
and `--actual-column`.

## Promotion criteria

The Woolly Mammoth must remain a **Reference Draft** until stronger evidence is
available. The next confidence steps are:

1. compare at the same 9.33 V supply rather than applying linear scaling;
2. achieve approximately the published ±10% working-board DC range without
   obviously unphysical parameter fitting;
3. cross-check the two 2N3904 devices against an established SPICE model;
4. compare transient node waveforms and spectra at several control settings;
5. preferably repeat the measurements on a nominated physical reference pedal.

This directory is intended to accumulate those reference datasets and metadata
rather than hiding fidelity assumptions inside the circuit implementation.
