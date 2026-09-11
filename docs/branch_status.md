# CircuitPedal Branch Status

This file records the canonical active branches and the legacy branches that are safe to retire after reconciliation.

## Canonical active branches

| Branch | Purpose | Origin |
| --- | --- | --- |
| `main` | Stable/physically accepted baseline | Authoritative project baseline |
| `feature/pedal-packages` | Pedal package metadata, presets and related UI architecture | Canonical package/UI workstream |
| `feature/reverb-engine` | Boing/BTDR-2 reverb implementation | Canonical reverb workstream; detailed Boing reference material preserved from the old branch |
| `fix/oversampling-regression` | Focused generic-circuit oversampling regression work | Canonical DSP-fix workstream |
| `validation/woolly-mammoth` | Focused Woolly Mammoth validation based from current `main` | Canonical model-validation workstream |
| `validation/live-runtime` | Realtime solve/timing instrumentation and 1x/4x A/B diagnostics | Extracted cleanly from `diagnostic/generic-runtime` onto current `main` |

## Legacy branches now safe to retire

### `feature/boing-reverb`

The useful reference material has been preserved on `feature/reverb-engine`. The old branch also contains diagnostics and unrelated circuit/runtime changes, so its implementation should not be merged wholesale.

### `diagnostic/generic-runtime`

The reusable runtime counters, timing diagnostics, macOS diagnostic panel and signal-summary utility have been extracted to `validation/live-runtime` without importing the unrelated pedal-model and CI changes.

### `integration/stable-1x`

Its final repository state is already represented by the stable `main` history. It is no longer a separate release or integration line.

### `v0.18-validation-framework`

The validation framework represented by this historical version branch is already preserved in later project history. Version milestones should live in tags/releases rather than active branches.

### `v0.19-woolly-validation`

The Woolly validation work represented by this historical branch is already preserved in later project history. New Woolly validation belongs on `validation/woolly-mammoth`.

## Rule for future cleanup

A merged, extracted or superseded working branch should be deleted promptly. Long-term history belongs in commits, tags/releases and documentation rather than a growing set of permanent branch names.
