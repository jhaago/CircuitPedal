# Woolly Mammoth acceptance phase

This phase treats CircuitPedal's current **48 kHz / 1x live path** as the production path. The deferred 4x oversampling experiment is not part of Woolly Mammoth acceptance.

## Automated acceptance report

Build the validator, then run:

```bash
python3 validation/woolly_mammoth/run_acceptance.py \
  build/circuitpedal_validate \
  build/validation/woolly-acceptance
```

The runner creates `acceptance_report.md` plus the live-path render CSVs.

It deliberately separates:

1. **Numerical/live-path safety** — hard gate at 48 kHz / 1x across multiple WOOL/PINCH edge states. Any nonlinear convergence failure or non-finite output fails the run.
2. **Published working-board DC agreement** — reports the exact error for B1, C1_NODE, E2 and C2_NODE against the 9.33 V working-board reference.
3. **Fidelity promotion** — the historical 35% DC sanity gate remains enforced for now, while the published approximately ±10% working-board range is shown explicitly as a tighter target rather than silently relaxed.

The report does not promote the model automatically. The physical reference quality and transistor-model provenance still matter.

## Manual physical acceptance

Complete `physical_acceptance_checklist.md` using the 48 kHz / 1x path. Exercise every control through its full range and record the actual hardware/test setup.

A positive listening impression is useful evidence, but it is not enough by itself to claim a component-accurate model.

## Independent transient comparison

The existing ngspice campaign remains complementary engineering evidence:

```bash
validation/woolly_mammoth/run_candidate_comparison.sh \
  build/circuitpedal_validate \
  build/validation/woolly-campaign
```

That comparison remains non-gating until the Q2N3904 SPICE model has stronger provenance or the project has measurements from a nominated physical reference pedal/transistor lot.

## Promotion decision

A future promotion from `Reference Draft` should require all of the following:

- the 48 kHz / 1x automated safety gate passes;
- the physical control-range checklist passes;
- DC disagreement is resolved or justified against an appropriate reference;
- transient/spectral behaviour is supported by trustworthy reference data;
- no parameter has been fitted purely to hit one reference point at the expense of physically plausible behaviour;
- the evidence and reference setup are committed alongside the model.
