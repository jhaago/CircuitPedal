# ADR-003: Real-Time Audio Contract

**Status:** Accepted

## Context

CircuitPedal runs live instrument audio and must remain stable at low buffer sizes. Native DSP features such as tuning introduce analysis work that could be too expensive or unpredictable if performed carelessly in the audio callback.

Future serial/parallel processing will increase the amount of work performed per audio block, making clear real-time rules more important.

## Decision

All processor and routing work executed on the real-time audio path must follow a strict real-time-safe contract.

During normal audio processing, the real-time path should avoid:

- blocking locks;
- filesystem or network access;
- UI calls;
- unbounded work;
- avoidable heap allocation;
- blocking logging or diagnostics.

Where substantial analysis is required, the audio thread should capture or enqueue preallocated sample/state data using an appropriate real-time-safe mechanism and a worker/non-real-time component should perform the expensive analysis.

State exposed to the UI should be transferred using lock-free or otherwise real-time-safe mechanisms appropriate to the data involved.

## Alternatives considered

### Perform all processor work synchronously in the callback regardless of cost

Rejected because analysis-heavy or future complex DSP can create dropouts and poor low-latency reliability.

### Use ordinary mutex-protected shared state

Rejected on the real-time path because lock contention and priority inversion can cause unpredictable callback timing.

## Consequences

- Processor APIs must make preparation/allocation phases distinct from live processing.
- Some processors may have worker components in addition to the audio-facing processor.
- Automated tests should include allocation/real-time-safety checks where practical.
- The tuner should not perform heavyweight pitch estimation directly inside the callback if it can be separated safely.
