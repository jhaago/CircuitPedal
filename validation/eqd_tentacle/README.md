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

## Physical validation findings — 2026-09-11

The first live test required the audio-interface input to be driven nearly to
maximum before the octave path became audible. Review against the PedalPCB
Squidward BOM found that the model had incorrectly used `4k7` for the final
output load. The reference value is `R13 = 47k`; the separate `R100 = 4k7`
belongs to the utility/supply section rather than this audio-path load.

After correcting the load to 47k, the repeat physical test produced no meaningful
level improvement. This showed that the output-load transcription was real but
was not the cause of the severe live level loss.

## Signal-level campaign

`run_signal_campaign.py` renders the model at 48 kHz with a 440 Hz sine at
approximately 20 mVpk, 50 mVpk and 100 mVpk circuit-input levels and records the
major internal nodes. The campaign shows:

- zero nonlinear-solver failures at all three levels;
- healthy gain through Q1 and complementary phase signals around Q2;
- full-wave rectification into Q3 with the 880 Hz second harmonic strongly
  dominating the original 440 Hz fundamental;
- approximately 36 mV RMS analogue output for a 50 mV peak input, so the
  analogue circuit is not suffering the roughly fivefold level loss heard in
  the live app.

The draft declares `AUDIO ... 0.20`, meaning one digital full-scale input maps
to 0.20 V at the circuit. With the old `OUTPUT OUT 1`, one output volt mapped to
only one digital full-scale unit. A roughly unity-voltage-gain analogue effect
was therefore made about five times quieter purely by mismatched I/O conversion.

The validation candidate uses `OUTPUT OUT 5`, the reciprocal of 0.20 V/FS. At
50 mV input the resulting digital output is approximately 0.181 RMS versus about
0.177 RMS for the input sine. At 100 mV input the output remains below full-scale
peak in the automated campaign. This is the candidate for the next physical
listening test; it is not yet an accepted production calibration.

## Validation status

The file parses, compiles and completes automated transient tests without
nonlinear-solver failure. The signal campaign confirms that the intended octave
path is active and that the severe live level loss is primarily an I/O-scaling
problem rather than a dead analogue topology.

This is still a **Reference Draft**. The 2N5089 and 2N3906 aliases are compact
Ebers-Moll approximations; no manufacturer-quality SPICE comparison or accepted
physical Tentacle match has yet been completed. The `OUTPUT OUT 5` calibration
must pass physical listening before promotion to `main`.
