# ADR-004: Processor Latency Must Be Reportable

**Status:** Accepted

## Context

Parallel signal paths can produce severe tonal changes if branches with different processing latency are summed without alignment.

Many current circuit models may have negligible or known fixed host latency, but future DSP effects, oversampling stages, convolution, look-ahead dynamics, pitch processing and other algorithms may introduce non-zero delay.

If latency is hidden inside processors, adding correct parallel routing later becomes substantially harder.

## Decision

The common processor contract will provide a way for each processor instance to report deterministic processing latency in samples (or an equivalent unambiguous representation).

The initial routing engine does not need to implement full latency compensation immediately, but its design must allow a graph layer to insert compensation delay where branches are merged.

Latency compensation is a routing responsibility, not something each individual pedal should solve independently.

## Alternatives considered

### Ignore latency until parallel routing is implemented

Rejected as an architectural assumption because it encourages APIs that cannot expose latency later without disruption.

### Make each pedal compensate itself relative to other pedals

Rejected because an individual processor does not know the topology or latency of sibling branches.

## Consequences

- Many processors may initially report zero latency.
- Oversampled/native DSP processors must report any intentional deterministic delay.
- The graph can later compute cumulative path latency and align branches at merge points.
- Latency metadata becomes part of processor validation.
