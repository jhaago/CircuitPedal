# CircuitPedal Branch Status

This file records the canonical active branches and the branches that are safe to retire.

## Canonical active branches

| Branch | Purpose | Status |
| --- | --- | --- |
| `main` | Stable/physically accepted 48 kHz / 1x baseline plus reusable validation infrastructure | Authoritative project baseline |
| `feature/pedal-packages` | Pedal package metadata, presets and related UI architecture | Active feature workstream |
| `feature/reverb-engine` | Boing/BTDR-2 reverb implementation | Active feature workstream |
| `validation/fuzz-factory` | Focused Fuzz Factory validation using the Woolly acceptance process as the template | Current validation workstream |

## Processing decision

The accepted live generic-circuit path is currently **48 kHz / 1x**. Physical A/B testing found the 4x path still sounded materially wrong even though the observed test produced no host solve failures or failed oversampled sub-solves. Oversampling is therefore deferred rather than merged. See `docs/processing_decisions.md`.

## Branches safe to retire

### `validation/woolly-mammoth`

The acceptance harness, physical checklist, dated baselines, DC-model triage and related CI have been promoted into `main`. The Woolly campaign remains documented in the repository, so this working branch no longer needs to remain active.

### `validation/live-runtime`

The branch completed its purpose: it established that the physical 4x problem was not explained by observed solver/sub-solve failures during the test session. The reusable `validation/model_signal_summary.py` utility has been preserved in `main`, and the engineering conclusion is recorded in `docs/processing_decisions.md`. The diagnostic overlay/runtime mode-switching implementation is intentionally not being retained as production code.

### `fix/oversampling-regression`

The experimental 4x DSP correction has not passed physical listening acceptance. Because oversampling is deferred, this branch should be retired rather than kept as an apparently active fix. The key engineering lead from that work—decimating raw circuit-domain output voltage before digital full-scale conversion/clamping—is recorded in `docs/processing_decisions.md` so the branch does not need to remain as an archive.

## Rule for future cleanup

A merged, extracted, superseded or deliberately deferred working branch should be deleted promptly. Long-term history belongs in accepted code, tags/releases and documentation rather than a growing set of permanent branch names.
