# EQD Tentacle reference draft

This directory records the evidence and modelling decisions behind
`circuits/eqd_tentacle_reference_draft.cpedal`.

## References

- EarthQuaker Devices describes the Tentacle as a control-free analog octave-up
  effect: https://www.earthquakerdevices.com/tentacle
- The user supplied a component-side/copper-side PCB artwork labelled
  `EQD TENTACLE` and `layout by storyboardist`.
- The PedalPCB Squidward build guide, revision 04.04.20, provides an independent
  schematic and complete BOM for its Tentacle-derived implementation:
  https://docs.pedalpcb.com/project/Squidward.pdf
- The verified Guitar FX Layouts build record corroborates the transistor
  selection/orientation and key corrected values:
  https://tagboardeffects.blogspot.com/2016/11/earthquaker-devices-tentacle.html

## Trace decisions

The audio topology is Q1 common-emitter gain, Q2 PNP phase splitting, a balanced
two-diode full-wave rectifier and Q3 emitter-follower buffering. There are no
live parameter controls.

The supplied PCB artwork marks its series supply-protection diode as `1N4001`,
while the PedalPCB derivative uses `1N5817`; the CircuitPedal draft follows the
supplied artwork. Indicator LED and bypass-switch wiring are excluded because
they are not part of the engaged audio path.

The cross-checked audio-path values are:

| Section | Values/devices |
| --- | --- |
| Input | 2.2M pulldown, 100p shunt, 47n coupling |
| Q1 amplifier | 2N5089, 560k/160k bias, 18k collector, 6.2k emitter |
| Q2 phase splitter | 2N3906, 10k emitter, 10k collector, two 47n couplers |
| Rectifier | two 1N4148, two 68k feeds, 22k/22k half-supply reference |
| Q3/output | 2N5089, 10k emitter, 100n coupling, 4.7k load |
| Supply | 1N4001 series protection, 100u filtering |

## Validation status

The file parses, compiles at 4x oversampling and completes a 0.5-second
transient test without nonlinear-solver failure. With a 110 Hz sine input, the
current compact model produces a dominant 220 Hz component, which confirms the
intended octave-generating signal path is active.

This is still a **Reference Draft**. The 2N5089 and 2N3906 aliases are compact
Ebers-Moll approximations; no manufacturer-quality SPICE comparison or
physical Tentacle measurement has yet been performed.
