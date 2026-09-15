# CircuitStomp Control Layer Design

## Purpose

Add the first complete CircuitStomp control path without tying the controller to AppKit, CoreMIDI, or `MacAudioEngine`. The milestone accepts provisional relative P1 MIDI messages on macOS, routes them through portable C++, updates the first exposed control of the authoritative active pedal, and refreshes the native UI from that same state.

This work changes control architecture only. It does not alter pedal DSP, add controller-side rig state, or implement the complete CircuitStomp protocol.

## Baseline and scope

The feature branch is `feature/circuitstomp-control-layer`, based on commit `129ab2d` from `main`. This follows `docs/branch_status.md`, which identifies `main` as the authoritative stable baseline.

Milestone 1 includes:

- a platform-neutral logical action vocabulary;
- a narrow, platform-neutral controller-target interface;
- a portable router with normalized clamping and contextual P1-P5 slots;
- a portable decoder for the provisional CircuitStomp MIDI 1.0 messages;
- host-state snapshot types for future controller feedback;
- a macOS target adapter backed by `MacAudioEngine`;
- a CoreMIDI input adapter for all visible MIDI sources;
- main-thread delivery and UI refresh after external control changes;
- portable tests that need neither CoreMIDI nor AppKit; and
- a provisional protocol document.

Milestone 1 does not include outbound MIDI, source-selection UI, expression input, stomp actions, presets/scenes, tuner transport, controller battery metadata, or a virtual MIDI source.

## Architecture

The implementation uses four focused layers:

1. `CircuitStompProtocol` converts transport-level MIDI 1.0 channel messages into logical `ControllerAction` values. It contains all provisional CC/channel constants. It has no CoreMIDI dependency.
2. `ControllerRouter` applies logical actions to a `ControllerTarget`. It understands contextual parameter indices, normalized values, encoder deltas, clamping, global master output, and bypass. It has no platform or audio-engine dependency.
3. `MacControllerTarget` adapts the narrow target interface to the existing `MacAudioEngine`. It maps generic circuit controls in declared order and maps the built-in Distortion+ controls as Distortion then Output. MASTER maps only to the engine's global master-output range.
4. `MacMidiInput` owns CoreMIDI discovery, source connections, packet receipt, and lifecycle. It forwards decoded logical actions to a supplied callback on the main queue. It does not know about `MacAudioEngine` or AppKit.

`main_mac_gui.mm` composes those pieces. It does not parse MIDI bytes. After the router reports a state change, the app delegate refreshes the relevant controls from `MacAudioEngine`; it never treats an `NSSlider` as authoritative state.

## Portable logical model

`ControllerAction` is a C++17 variant containing these logical concepts:

- `ParameterDelta { index, steps }`
- `ParameterSet { index, normalizedValue }`
- `MasterOutputDelta { steps }`
- `MasterOutputSet { normalizedValue }`
- `BypassToggle`
- `BypassSet { bypassed }`
- `StompAction { index, pressed }`
- `ExpressionValue { index, normalizedValue }`

P1-P5 are represented by parameter indices 0-4. They are contextual positions in the active pedal's exposed-control list and never contain pedal-specific names.

The router implements parameter set/delta, master set/delta, and bypass set/toggle. Stomp and expression types establish vocabulary for later milestones but safely return an unhandled/no-change result until their semantics are specified.

## Controller target boundary

`ControllerTarget` exposes only the state required by the portable router:

- active pedal name;
- contextual parameter count;
- label, normalized value, and preferred normalized encoder step for each parameter;
- normalized parameter writes;
- normalized global master-output read/write; and
- bypass read/write.

The target reports parameter metadata rather than forcing the router to understand circuit switches. Potentiometers use a 0.01 normalized step. A discrete switch uses `1 / (positionCount - 1)`, so one encoder detent selects the next switch position instead of being rounded away.

All router writes clamp to `[0, 1]` before reaching the target. An unavailable P slot is a safe no-op.

`MacControllerTarget` converts normalized MASTER values to the existing `-18 dB` to `+6 dB` `MacAudioEngine` range. It never maps MASTER to a pedal's Output or Level control.

## Provisional MIDI protocol

Prototype 1 uses MIDI 1.0 Control Change on channel 1. P1 is CC 20.

The relative encoding is signed magnitude around the two no-op values:

- values 1-63 mean `+1` through `+63` steps;
- values 65-127 mean `-63` through `-1` steps;
- values 0 and 64 mean no movement.

Therefore the required milestone messages are:

- `B0 14 01`: P1 increment by one step;
- `B0 14 7F`: P1 decrement by one step.

Only P1 is assigned and decoded in Prototype 1. Reserved future ranges are documented without being accepted by the decoder. Raw channel and CC constants live only in `CircuitStompProtocol`.

The decoder accepts a complete MIDI 1.0 channel message and returns an optional logical action. The CoreMIDI adapter is responsible for extracting messages from packet lists. Unsupported channels, controllers, message types, and no-op relative values are ignored.

## macOS MIDI lifecycle and threading

`MacMidiInput` creates one CoreMIDI client and input port, enumerates all current MIDI sources, and connects the port to each source. CoreMIDI presents BLE MIDI and USB MIDI through this same source mechanism.

CoreMIDI notifications schedule a source reconciliation on the main queue. Reconciliation connects newly appeared sources and disconnects endpoints that disappeared. Failure to connect one source does not tear down working sources; the adapter returns diagnostics suitable for display or logging.

The CoreMIDI read callback performs bounded byte parsing and portable protocol decoding only. It never calls AppKit, `MacAudioEngine`, or DSP code. Decoded actions are copied into main-queue work items. A shared callback state with an active flag prevents already-queued work from invoking a destroyed integration.

The main queue then invokes `ControllerRouter`. This matches the existing GUI control thread, avoids AppKit work on CoreMIDI callback threads, and keeps MIDI work off the real-time audio thread. Existing engine atomics remain the audio-thread handoff; no MIDI callback enters audio rendering.

## GUI synchronization

The app delegate owns the engine, macOS target adapter, router, and MIDI input in that lifetime order. MIDI is stopped before the router, target, or engine is destroyed.

When a routed action changes state, the delegate refreshes controller-visible UI from engine getters:

- the affected generic slider or switch control;
- the built-in Distortion or Output control when active;
- the global master-output control; and
- bypass appearance.

For Milestone 1 only P1 MIDI can arrive, but the refresh routine is general enough for later logical actions. The existing meter timer remains dedicated to meters; it is not repurposed as controller-state polling.

## Future host feedback

Portable `ControllerHostState` and parameter-slot state structures represent:

- selected pedal name;
- bypass state;
- up to five available parameter labels and normalized values;
- normalized MASTER output;
- optional scene/preset name; and
- optional tuner state.

The router can build a snapshot from its target. Milestone 1 has no outbound transport and does not cache this snapshot as a second source of truth. A future feedback publisher will request a fresh snapshot after state changes and encode it for BLE/USB MIDI.

## Error handling

- Invalid or non-finite normalized values are rejected or safely clamped before a target write.
- Missing parameter slots return an unchanged result.
- Unsupported MIDI is ignored.
- CoreMIDI startup failure leaves audio and GUI operation available.
- Individual source connection failures do not invalidate successful connections.
- Queued callbacks become inert during shutdown.

MIDI availability is an optional controller capability, not a prerequisite for starting the audio engine.

## Testing

Portable tests use a fake `ControllerTarget` and cover:

1. P1 `+1` reaches contextual parameter index 0.
2. P1 `-1` applies the negative step.
3. parameter values clamp at 0 and 1.
4. P2-P5 route to indices 1-4.
5. unavailable parameter indices do nothing.
6. MASTER changes only global master output, including when a pedal parameter is labelled Output or Level.
7. routing works entirely through the fake target.
8. Prototype 1 P1 increment/decrement MIDI decodes to the expected logical actions without CoreMIDI.
9. unsupported channel, CC, message type, and no-op values do not produce actions.
10. host-state snapshots reflect target values without owning an independent state model.

Existing CMake tests remain unchanged except for registering the new portable test executables or one combined controller test target. The complete test suite runs on Linux and macOS. GitHub's macOS CI must additionally build and validate the native app bundle because the current execution workspace is Linux.

## Acceptance

The milestone is accepted when a simulated or physical Prototype 1 P1 CoreMIDI message changes the first exposed live pedal control, the GUI reads back and shows that authoritative engine value, all portable and existing tests pass, the macOS app builds, and neither the portable protocol/router nor its tests include CoreMIDI, AppKit, or `MacAudioEngine`.
