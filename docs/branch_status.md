# CircuitPedal Branch Status

This file records which branches are canonical and which older branches are retained only for history/reconciliation.

## Canonical active branches

| Branch | Purpose | Origin |
| --- | --- | --- |
| `main` | Stable/physically accepted baseline | Authoritative project baseline |
| `feature/pedal-packages` | Pedal package metadata, presets and related UI architecture | Replaces `feature/pedal-packages-v0.1` |
| `feature/reverb-engine` | Boing/BTDR-2 reverb implementation | Replaces `feature/boing-reverb-clean` as the canonical clean reverb line |
| `fix/oversampling-regression` | Focused generic-circuit oversampling regression work | Replaces `fix/generic-oversampling-regression` |
| `validation/woolly-mammoth` | Fresh Woolly Mammoth validation work based from current `main` | New canonical validation branch |

## Safe legacy deletions

These branches are superseded by another branch that contains the same or later work and should not be used for new development:

- `feature/pedal-packages-v0.1`
- `feature/pedal-packages-v0.1-ci`
- `feature/ui-aesthetic-pass`
- `feature/boing-reverb-clean`
- `fix/generic-oversampling-regression`
- `fix/restore-1x-live-generic`

They may be deleted once convenient.

## Legacy branches requiring reconciliation before deletion

These branches still contain unique or mixed-purpose commits and should be retained until their useful work has been either merged, deliberately discarded, or preserved another way:

### `feature/boing-reverb`

Mixed-purpose branch. Contains reverb work plus diagnostics, generic runtime/oversampling changes and pedal-model changes. Do not continue development here. Extract only the pieces still wanted after comparison with `feature/reverb-engine`, `fix/oversampling-regression` and `main`.

### `diagnostic/generic-runtime`

Diagnostic branch with substantial mixed changes. Keep temporarily for reference while determining which diagnostics or runtime fixes deserve focused branches or integration into `main`.

### `integration/stable-1x`

Historical integration branch that still has unique commits, but `main` is now the authoritative stable branch. Reconcile any remaining useful changes into focused branches or `main`, then retire it.

### `v0.18-validation-framework`

Historical version-named validation branch. Do not continue work here. Preserve any missing validation functionality in `main`/`validation/*`, then replace the historical milestone with a tag/release if desired.

### `v0.19-woolly-validation`

Historical Woolly validation branch. Do not continue work here. New Woolly work belongs on `validation/woolly-mammoth` from current `main`.

## Rule for future branch cleanup

A merged or superseded working branch should be deleted promptly. Long-term history should live in Git commits, tags/releases and documentation, not in a growing set of permanent branch names.
