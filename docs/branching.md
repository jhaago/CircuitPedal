# CircuitPedal Branching Policy

CircuitPedal uses a deliberately small branch model. The goal is to keep `main` authoritative, make the purpose of every active branch obvious, and avoid long-lived parallel versions of the project.

## Core rule

**One branch = one purpose. Always branch from the current `main` unless there is a documented reason not to.**

Do not continue unrelated work on an existing feature branch simply because it is convenient.

## Branch types

### `main`

`main` is the authoritative stable development baseline.

It should contain only work that is considered safe enough to become the basis of the next physical test or release candidate. Experimental pedal models, diagnostics and incomplete architecture changes should not live permanently on `main`.

For CircuitPedal specifically, `main` is also the reference point for the currently accepted live-audio behaviour.

### `feature/<purpose>`

Use for one new capability or subsystem.

Examples:

- `feature/pedal-packages`
- `feature/reverb-engine`
- `feature/preset-library`

A feature branch should not accumulate unrelated bug fixes, validation campaigns or other pedal experiments.

### `fix/<purpose>`

Use for one defined defect or regression.

Examples:

- `fix/oversampling-regression`
- `fix/device-selection`

Once the fix is accepted into `main`, the branch should be retired.

### `validation/<subject>`

Use for focused validation work that may produce measurements, comparison tooling or model-specific acceptance changes.

Examples:

- `validation/woolly-mammoth`
- `validation/live-runtime`
- `validation/fuzz-factory`

Validation branches should be short-lived. Important validation tooling that is useful project-wide should be merged into `main`; historical milestones should be preserved with tags/releases rather than permanent version-number branches.

### `experiment/<purpose>`

Use only for genuinely uncertain R&D that may never be merged.

Examples:

- `experiment/alternate-solver`
- `experiment/neural-tone-matching`

An experiment must not be treated as production-ready merely because it builds successfully.

## Naming rules

Do not create new branches with suffixes such as:

- `-clean`
- `-new`
- `-final`
- `-final2`
- `-ci`
- version numbers such as `v0.19-*`

If a feature needs a clean restart, create a newly named branch whose purpose is explicit, and document which older branch it supersedes.

Software versions belong in tags/releases, not long-lived branches.

## Current canonical workstreams

As of 2026-09-11, use these workstreams as the canonical directions for new CircuitPedal work:

- `main` — physically accepted/stable baseline
- `feature/pedal-packages` — pedal package format, metadata/presets and related UI architecture
- `feature/reverb-engine` — Boing/BTDR-2 reverb implementation work
- `fix/oversampling-regression` — focused generic-circuit oversampling regression work
- `validation/woolly-mammoth` — focused Woolly Mammoth validation work based from current `main`
- `validation/live-runtime` — realtime solve/timing instrumentation and engineering A/B diagnostics based from current `main`

## Legacy branches

The following older branches have been superseded and should not be used for new development:

- `diagnostic/generic-runtime` — useful runtime diagnostics were extracted to `validation/live-runtime`
- `feature/boing-reverb` — detailed reference material was preserved on `feature/reverb-engine`; the old mixed implementation is superseded
- `integration/stable-1x` — its final state is already preserved by the stable `main` history
- `v0.18-validation-framework` — its validation framework is already preserved in later project history
- `v0.19-woolly-validation` — its Woolly validation work is already preserved in later project history; new work belongs on `validation/woolly-mammoth`

These legacy branches are safe to retire once no local work depends on them.

## Merge discipline

Before merging into `main`:

1. Confirm the branch still has one clear purpose.
2. Rebase/merge current `main` as appropriate and resolve conflicts deliberately.
3. Run the relevant automated tests.
4. For live-audio or pedal-model changes, perform the required physical validation before describing the model as accepted/stable.
5. Merge only the work intended for that branch.
6. Retire the branch after merge rather than keeping it as a permanent alternate version.

## AI-assisted development rule

When using an AI coding agent, every implementation prompt should explicitly state:

- the repository
- the exact branch to work on
- the purpose of that branch
- that unrelated changes must not be added
- whether the work is implementation, validation, diagnostics or experiment
- whether changes are permitted to reach `main`

If the requested work does not match the current branch purpose, create a new branch from current `main` instead of extending the existing branch.
