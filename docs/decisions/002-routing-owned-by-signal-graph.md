# ADR-002: Routing Is Owned by a Signal Graph

**Status:** Accepted

## Context

CircuitPedal currently has a simple live path, but the intended product direction includes multiple pedals in series and parallel.

If individual pedal processors are written with assumptions about their fixed position in the chain, later support for splits, merges, wet/dry paths and parallel processing will require extensive rewrites.

## Decision

Signal topology will belong to a dedicated routing/graph layer, not to individual pedal implementations.

A processor receives audio from the graph and returns processed audio to the graph. It must not assume that it is first, last, serial-only, or unique.

The future graph may represent:

- serial processing;
- split/fan-out nodes;
- parallel branches;
- merge/sum nodes;
- wet/dry paths;
- instance-level bypass;
- future stereo or multi-channel paths.

Pedal package metadata describes the pedal itself. Board/session data describes pedal instances and their placement within the graph.

## Alternatives considered

### Store next/previous pedal relationships inside processors

Rejected because processors would become coupled to topology and difficult to reuse in parallel branches.

### Implement only a mutable ordered list and retrofit parallel routing later

Acceptable as an initial UI/runtime stage only if the underlying processor contract does not encode serial-only assumptions. The long-term architecture remains graph-based.

## Consequences

- A simple serial chain can be implemented as a trivial graph/list first.
- Processor APIs must remain location-independent.
- Board/session state will eventually need persistent graph representation.
- Routing validation becomes independently testable from pedal DSP fidelity.
