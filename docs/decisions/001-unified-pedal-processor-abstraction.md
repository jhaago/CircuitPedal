# ADR-001: Unified Pedal Processor Abstraction

**Status:** Accepted

## Context

CircuitPedal currently focuses on electrically modelled pedals backed by `.cpedal` circuit files. Future pedals include devices whose correct implementation is native DSP rather than analogue-circuit emulation, beginning with a chromatic stomp tuner.

Treating each non-circuit pedal as an application special case would fragment the library, routing and UI architecture. Conversely, forcing every pedal to have a fake schematic would misrepresent the device and complicate implementation.

CircuitPedal also intends to support multiple effects in serial and parallel in the future, which requires routing code to treat processors uniformly regardless of their internal modelling technology.

## Decision

A user-facing pedal and its processing implementation are separate concepts.

CircuitPedal will evolve toward a common pedal-processor abstraction. Processor implementations may include:

- `.cpedal` circuit-model processors;
- native DSP processors;
- analysis/utility processors;
- future processor families that satisfy the common runtime contract.

The routing layer should interact with processors through a common external contract and should not need to know how an individual processor is implemented internally.

The pedal package format should eventually identify the processor implementation explicitly rather than assuming every package references a `.cpedal` file.

## Alternatives considered

### Separate digital-pedal package system

Rejected as the preferred direction because it would duplicate library, artwork, preset, control and routing concepts and create two parallel pedal ecosystems.

### Hard-code each digital utility in the application

Rejected because it scales poorly and would create repeated special cases for tuner, delay, gate, looper, pitch and other DSP features.

### Require every pedal to use a `.cpedal` representation

Rejected because it is technically incorrect for inherently digital/algorithmic utilities and would constrain the product architecture unnecessarily.

## Consequences

- Existing `.cpedal` models remain valid and become one processor family.
- The package schema will need a backwards-compatible evolution path.
- Runtime processor lifecycle and state interfaces will need to be defined carefully.
- Future routing work can operate on processor instances rather than circuit types.
- Processor-specific telemetry/capabilities may be exposed through narrow optional interfaces rather than broad type checks.
