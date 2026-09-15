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
| Q3/output | 2N5089, 10k emitter, 100n coupling, 47k output load |
| Supply | 1N4001 series protection, 100u filtering |

## Physical validation finding — 2026-09-11

The first live test required the audio-interface input to be driven nearly to
maximum before the octave path became audible. Review against the PedalPCB
Squidward BOM found that the model had incorrectly used `4k7` for the final
output load. The reference value is `R13 = 47k`; the separate `R100 = 4k7`
belongs to the utility/supply section rather than this audio-path load.

The model was corrected to `R_OUTPUT_LOAD = 47k`, but the repeat physical test
showed no meaningful level improvement. A later review found a separate host
calibration error: `AUDIO ... 0.20` was paired with `OUTPUT ... 1`, so a roughly
unity-voltage-gain analogue path was attenuated by another factor of five when
converted back to digital audio.

The candidate now uses `OUTPUT OUT 5`, the reciprocal of the 0.20 V/full-scale
input calibration. This changes only the model/host conversion; it does not
increase gain inside the analogue circuit or alter the rectifier topology.

## Validation status

The file parses, compiles and completes automated transient tests without
nonlinear-solver failure. `run_signal_campaign.py` checks 82.41-1318.51 Hz at
20-100 mV peak circuit inputs. It requires:

- the doubled-frequency component to exceed the fundamental by at least 10:1;
- the Q2 emitter/collector AC levels to remain balanced within 10% and their
  normalized correlation to remain at or below -0.95, proving phase opposition;
- calibrated output to remain within 1.9 dB of dry level at normal inputs;
- useful low-level output and zero nonlinear-solver failures.

The topology was rechecked against the PedalPCB Squidward revision 04.04.20
schematic. Q2's emitter feeds the VCC-side 10k branch, its collector feeds the
ground-side 10k branch, both 1N4148 anodes receive the opposing AC phases, and
their cathodes combine at Q3's base. The current `.cpedal` matches those
connections and component values; no speculative topology change was made.

This is still a **Reference Draft**. The 2N5089 and 2N3906 aliases are compact
Ebers-Moll approximations; no manufacturer-quality SPICE comparison or accepted
physical Tentacle match has yet been completed. The calibrated candidate still
requires a repeat physical listening test, especially with a neck pickup above
the 12th fret, before the Tentacle is bundled in the stable app.
