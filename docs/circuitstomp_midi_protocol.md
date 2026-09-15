# CircuitStomp MIDI Protocol

## Status

This document defines **Prototype 1**, a provisional and intentionally small CircuitStomp protocol. It is not frozen. Later milestones may extend or revise it after physical controller testing.

CircuitStomp is a controller only. Guitar audio never passes through it. CircuitPedal remains authoritative for the active pedal, parameter values, bypass, master output, pedalboard/session state, scenes/presets, and all metadata sent back to the controller.

BLE MIDI and USB MIDI use the same logical MIDI mapping. On Apple hosts both transports appear as CoreMIDI sources and enter the same decoding and routing path.

## Prototype 1 receive mapping

Prototype 1 implements one contextual control:

| Control | MIDI message | Meaning |
| --- | --- | --- |
| P1 | Channel 1, Control Change 20 | Relative movement of the active pedal's first exposed parameter |

P1 is a slot, not a pedal-specific control. For example, it selects Drive when that is a pedal's first exposed parameter and Wool when Wool is first. If the active pedal has no first parameter, the message is a safe no-op.

MASTER is not P1-P5. A pedal parameter named Output, Volume, or Level remains a contextual pedal parameter and is never treated as the global CircuitPedal master output.

## Relative encoder encoding

Encoder values use a relative signed convention around two no-op values:

| MIDI CC value | Meaning |
| --- | --- |
| 0 | No movement |
| 1-63 | `+1` through `+63` encoder steps |
| 64 | No movement |
| 65-127 | `-63` through `-1` encoder steps |

The negative step count is `value - 128`. Thus 65 is -63 and 127 is -1.

One normal potentiometer step is currently 0.01 of its normalized 0-1 range. A discrete switch uses the normalized distance between adjacent positions, ensuring one encoder detent reaches the next position.

Exact Prototype 1 examples:

| Bytes (hex) | Result |
| --- | --- |
| `B0 14 01` | P1 increment by one step |
| `B0 14 7F` | P1 decrement by one step |
| `B0 14 00` | No action |
| `B0 14 40` | No action |

Messages on another channel, with another CC number, or of another MIDI message type are not part of Prototype 1 and are ignored.

## Software boundary

Raw MIDI channel, CC, and relative-value interpretation live in `CircuitStompProtocol`. The decoder emits a platform-neutral `ParameterDelta` action. `ControllerRouter` applies that action to an abstract `ControllerTarget`, and the current macOS target adapter changes the authoritative `MacAudioEngine` value.

The CoreMIDI callback does not call AppKit, the audio engine, or DSP code. It copies decoded actions to the main queue before routing. This keeps MIDI activity away from the real-time audio callback and gives the native UI one safe place to refresh from authoritative engine getters.

The portable decoder and router have no CoreMIDI or AppKit dependency and are intended to be reused by a future iPadOS host.

## Reserved future protocol areas

The following concepts deliberately have no frozen wire assignments in Prototype 1:

| Direction | Capability | Prototype 1 status |
| --- | --- | --- |
| Controller to host | P2-P5 relative or absolute parameter control | Unassigned |
| Controller to host | MASTER relative or absolute control | Unassigned |
| Controller to host | SW1-SW3 press/release actions | Unassigned |
| Controller to host | Bypass control | Unassigned |
| Controller to host | Scene/preset selection | Unassigned |
| Controller to host | EXP expression input | Unassigned |
| Host to controller | Selected pedal name | Unassigned |
| Host to controller | P1-P5 labels and normalized values | Unassigned |
| Host to controller | Bypass and MASTER state | Unassigned |
| Host to controller | Scene/preset name | Unassigned |
| Host to controller | Tuner note, frequency, and cents | Unassigned |
| Either direction | Battery and connection metadata where applicable | Unassigned |

Code may define logical action or host-state types for these future capabilities, but that does not make them part of the Prototype 1 wire protocol.

## Feedback authority

CircuitStomp must treat received host feedback as a view of CircuitPedal state, not as an independent rig database. A future outbound transport will request a fresh `ControllerHostState` snapshot after relevant changes and encode it for the connected controller. Prototype 1 establishes that portable snapshot shape but does not transmit it.

## Development without physical hardware

The protocol decoder and router unit tests inject byte messages and logical actions directly. On macOS, a virtual or external MIDI source can send the exact Prototype 1 messages through CoreMIDI to exercise the same application path used by future BLE and USB CircuitStomp hardware.
