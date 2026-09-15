# ADR-005: Tuner as a Native-DSP Utility Pedal

**Status:** Accepted

## Context

CircuitPedal needs a tuner pedal, but a tuner is not appropriately represented as an analogue effect schematic. It analyses the incoming instrument signal and reports pitch information.

The desired behaviour is that of a conventional stomp tuner: when engaged, it analyses the instrument while muting the audible output; when bypassed, normal signal flow resumes.

The tuner is also the first concrete use case for the broader native-DSP pedal architecture.

## Decision

The tuner will be implemented as a native DSP / analysis utility processor within the unified pedal ecosystem.

Initial functional intent:

- chromatic automatic note detection;
- usable for guitar and bass, including extended low bass range;
- normal stomp behaviour: engaged = analyse + mute output;
- bypassed = normal signal flow;
- click-free mute/unmute transition;
- pitch result exposed as telemetry including detected frequency, note, cents error and confidence/validity;
- reference pitch defaults to A4 = 440 Hz;
- expensive pitch analysis should be separated from the real-time callback where practical.

The pitch-detection algorithm is an implementation detail and may evolve through validation; the architecture must not bind routing to a specific detector.

## Alternatives considered

### Model the tuner as a `.cpedal` circuit

Rejected because pitch detection and display behaviour are digital signal-analysis functions, not an analogue transfer characteristic.

### Implement tuner mode outside the pedal system

Rejected because it would create a special application mode and would not establish the intended native-DSP pedal architecture.

## Consequences

- The pedal package schema must support non-circuit processor references.
- The UI needs a way to render processor-specific telemetry without requiring fake knobs.
- Engage behaviour must support utility mute semantics cleanly.
- Tuner validation can use generated reference tones and real instruments in addition to ordinary listening tests.
