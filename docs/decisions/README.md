# CircuitPedal Architecture Decision Records

This directory contains Architecture Decision Records (ADRs) for long-lived CircuitPedal architectural decisions.

Use an ADR when a decision materially affects how future subsystems fit together, constrains public/internal interfaces, or would be expensive/confusing to rediscover later.

Do not use ADRs for every implementation detail. Short-lived experiments and validation findings belong in their relevant workstream documentation, while accepted processing-path findings may belong in `docs/processing_decisions.md`.

## Format

Each ADR should contain:

- Status
- Context
- Decision
- Alternatives considered
- Consequences
- Follow-up / validation notes where useful

Suggested statuses:

- Proposed
- Accepted
- Superseded
- Deprecated

File names use a numeric prefix and short kebab-case title, for example:

```text
001-unified-pedal-processor-abstraction.md
002-routing-owned-by-signal-graph.md
```

When a decision is replaced, keep the old ADR and mark it `Superseded` with a link to the newer record rather than deleting historical context.
