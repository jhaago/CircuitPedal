# CircuitPedal Architecture Principles

This document defines the long-term architectural principles for CircuitPedal.

It is intentionally broader than any single pedal model, DSP algorithm, UI implementation, or temporary validation branch. These principles should guide future engineering decisions so new features do not create unnecessary rewrites or constrain later routing, DSP, platform, or pedal-model work.

Specific accepted engineering choices should still be recorded in `docs/processing_decisions.md` or in an Architecture Decision Record (ADR) under `docs/decisions/`.

These principles are normative design guidance for new CircuitPedal work. They describe intended architectural boundaries, not a claim that every capability described here is already implemented.

## 1. A pedal is a processor, not a routing topology

A pedal should receive audio, process it according to its own state, and return audio and any relevant telemetry.

A pedal must not assume that it is:

- the only processor in the signal path;
- the first or last processor;
- permanently connected in series;
- permanently connected to the application input or output;
- globally unique.

The same pedal implementation should be usable in a simple serial chain, in either side of a parallel split, inside a more complex routing graph, or in multiple independent instances.

## 2. Circuit modelling is one processor family, not the definition of a pedal

CircuitPedal began with electrical circuit models, but a user-facing pedal may be implemented by different processing technologies.

Supported processor families may include:

- circuit-model processors backed by `.cpedal` electrical models;
- native DSP processors such as tuners, delays, gates, modulation, pitch and utility effects;
- analysis or utility processors that publish telemetry in addition to, or instead of, materially altering the audio signal;
- future processor technologies that can satisfy the same runtime contract.

The pedal library, package system, signal chain and UI should not require fake schematics for pedals whose correct implementation is digital DSP.

## 3. Circuit and native-DSP pedals should share a common external runtime contract

The routing engine should not need to understand the internal implementation of an effect.

At the routing boundary, processors should expose a consistent lifecycle and processing contract covering concepts such as:

- preparation for sample rate and block size;
- processing input buffers into output buffers;
- bypass state;
- reset/state clearing;
- parameter/state updates;
- latency reporting where applicable;
- optional processor-specific telemetry.

Circuit-specific details stay inside the circuit processor. Tuner-specific pitch detection stays inside the tuner processor. Delay-line implementation stays inside the delay processor.

## 4. Routing belongs to a dedicated signal-graph layer

Series and parallel routing must be represented outside individual pedal implementations.

The long-term routing layer should be capable of representing structures such as:

```text
Input -> Compressor -> Split -> [ Fuzz -> Delay ] -> Merge -> Output
                           \-> [ Clean EQ      ] -/
```

Routing responsibilities include:

- processor ordering;
- branch creation;
- fan-out/splitting;
- merging/summing;
- wet/dry paths;
- gain management around splits and merges;
- latency alignment between parallel branches;
- future stereo/multi-channel routing.

A pedal should not contain special logic merely because it is placed in a parallel branch.

## 5. Pedal packages describe pedals; sessions/boards describe placement

A pedal package should own information intrinsic to the pedal, such as:

- identity and display name;
- processor type and implementation reference;
- artwork and presentation metadata;
- controls and parameter metadata;
- presets;
- pedal-specific capabilities or telemetry presentation.

A package should not encode where that pedal appears in a user's current board or whether it is in series or parallel.

Board/session state should own:

- which pedal instances are present;
- per-instance parameter values;
- routing topology;
- branch and merge configuration;
- instance bypass/engage state;
- any per-board automation or switching state.

## 6. Processor instances must be independent

CircuitPedal must not assume one global instance of a pedal or processor type.

It should eventually be valid to use:

- two delays with different times;
- two Tube Screamer-type pedals with different settings;
- two instances of the same `.cpedal` model;
- the same processor in different branches.

Mutable DSP state belongs to an instance, not to global shared state.

## 7. Latency must be explicit and reportable

Any processor that introduces deterministic latency must be able to report it to the routing engine.

This is particularly important for parallel routing. If one branch introduces latency and another does not, summing them without alignment can create comb filtering, phase cancellation, or other unintended behaviour.

The routing architecture should therefore be designed so latency compensation can be added without changing individual pedal DSP implementations.

Latency reporting may initially be zero for many processors, but the interface should not make future latency compensation impossible.

## 8. Channel behaviour must be explicit

CircuitPedal may operate primarily as a mono guitar/bass processor today, but the architecture should not permanently prevent future stereo or multi-channel effects.

Processors should have an explicit channel contract rather than relying on hidden assumptions.

Future effects such as stereo delay, chorus, reverb or dual-path processing should be possible without replacing the entire processor interface.

This does not require implementing stereo routing immediately.

## 9. Real-time safety is non-negotiable

Code executed on the real-time audio path must avoid operations that can block unpredictably or allocate unexpectedly.

In particular, the audio callback should avoid:

- locks and blocking synchronization;
- file or network I/O;
- UI operations;
- heap allocation during normal processing;
- logging that can block;
- expensive work that can safely be moved off the audio thread.

Cross-thread state should use appropriate lock-free or real-time-safe communication patterns.

Analysis-heavy processors such as tuners should separate sample capture from heavyweight analysis where practical.

## 10. Bypass and engage behaviour should be standardised

CircuitPedal should distinguish between:

- ordinary effect bypass, where signal passes around an effect;
- processor-specific engage behaviour, where activation may intentionally mute or otherwise alter routing behaviour.

The tuner is an example: when engaged as a normal stomp tuner it analyses the instrument input while muting the audible output. That should be represented deliberately rather than by pretending the tuner is an ordinary audio transform that happens to output zero.

The routing/runtime architecture should provide a consistent place for these behaviours.

## 11. Validated pedal behaviour should survive architecture changes

Refactoring the surrounding application architecture must not silently change the sound of already validated pedals.

Changes to:

- routing;
- package loading;
- processor abstraction;
- UI;
- pedal-instance management;
- native-DSP support;

must preserve accepted circuit-model behaviour unless a sound change is intentional, independently justified, tested and documented.

Physical listening validation remains important for analogue-model fidelity.

## 12. The architecture should enable progressive complexity

CircuitPedal should support a useful simple case without requiring the complete future routing engine to exist immediately.

A reasonable progression is:

1. one processor in the live path;
2. multiple processors in serial;
3. a general processor chain;
4. split/merge routing;
5. latency-compensated parallel paths;
6. richer stereo/multi-channel graphs where useful.

New work should avoid shortcuts that make later stages unnecessarily difficult.

## 13. Prefer explicit capability contracts over special cases

If a processor needs behaviour beyond basic audio processing, expose that through a defined capability rather than application-wide type checks and scattered special cases.

Examples may include:

- pitch-analysis telemetry;
- latency reporting;
- tail/release behaviour for delay or reverb;
- sidechain input capability;
- tempo synchronisation;
- stereo input/output capability;
- utility mute behaviour.

Capabilities should be added only when needed and should remain narrow.

## 14. Keep DSP, routing, package metadata and presentation separate

CircuitPedal should maintain clear boundaries between:

- DSP/model implementation;
- routing/graph state;
- package/library metadata;
- user-interface presentation;
- validation/test infrastructure.

A visual redesign should not change DSP topology. Loading artwork should not alter electrical behaviour. Routing changes should not require changes to every pedal model. Validation tools should not become hidden production dependencies.

## 15. Decisions that constrain future architecture must be documented

When a change creates a long-lived architectural constraint, record it as an ADR under `docs/decisions/`.

An ADR should capture:

- the problem/context;
- the decision;
- important alternatives considered;
- consequences and trade-offs;
- whether the decision is accepted, provisional, superseded or deprecated.

This makes future engineering work depend on repository evidence rather than chat history or temporary branches.
